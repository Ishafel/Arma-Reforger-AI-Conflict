enum AICF_EAIQuotaStartup
{
	READY,
	WAIT,
	BLOCKED
}

// Подготовка ДО m_bStarted и любого project SetLimit. Уже оплаченная native
// очередь заканчивается по старым правилам; её request objects не усыновляются
// новым basket и не удаляются ради запуска policy.
class AICF_AIQuotaStartup
{
	static const int RETRY_INTERVAL_MS = 1000;
	static const int MAXIMUM_WAIT_MS = 120000;
	static const int WAIT_LOG_INTERVAL_MS = 10000;
	protected AICF_MatchController m_Controller;
	protected SCR_GameModeCampaign m_Campaign;
	protected SCR_AIWorld m_World;
	protected BaseWorld m_MainWorld;
	protected AICF_AIQuotaWorldState m_WorldState;
	protected SCR_SpawnerAIGroupManagerComponent m_Manager;
	protected ref AICF_Stage1Config m_Config;
	protected ref AICF_ContentProfile m_Content;
	protected string m_sMapKey;
	protected string m_sLastReason = "NOT_CAPTURED";
	protected int m_iStartedAtMs;
	protected int m_iLastWaitLogMs = -WAIT_LOG_INTERVAL_MS;
	protected bool m_bScheduled;
	protected bool m_bCancelled;

	static AICF_AIQuotaStartup Capture(AICF_MatchController controller, SCR_GameModeCampaign campaign,
		AICF_Stage1Config config, AICF_ContentProfile content, string mapKey)
	{
		if (!Replication.IsServer() || !GetGame() || !GetGame().InPlayMode() || !GetGame().GetWorld() ||
			!controller || !campaign || !campaign.IsMaster() || GetGame().GetGameMode() != campaign) return null;
		SCR_AIWorld world = SCR_AIWorld.Cast(GetGame().GetAIWorld());
		if (!world || world.GetWorld() != GetGame().GetWorld()) return null;
		AICF_AIQuotaStartup startup = new AICF_AIQuotaStartup();
		startup.m_Controller = controller;
		startup.m_Campaign = campaign;
		startup.m_World = world;
		startup.m_MainWorld = GetGame().GetWorld();
		startup.m_Config = config;
		startup.m_Content = content;
		startup.m_sMapKey = mapKey;
		startup.m_iStartedAtMs = System.GetTickCount();
		startup.m_Manager = SCR_SpawnerAIGroupManagerComponent.Cast(campaign.FindComponent(SCR_SpawnerAIGroupManagerComponent));
		// World-owned capture is readonly and does not require started controller.
		startup.m_WorldState = world.AICF_GetQuotaWorldState();
		if (!startup.m_Manager || !startup.m_WorldState || !startup.m_WorldState.IsCurrentWorld() ||
			startup.m_WorldState.GetOriginalNativeLimit() <= 0) return null;
		return startup;
	}

	string GetLastReason() { return m_sLastReason; }

	bool IsCurrent()
	{
		if (m_bCancelled || !Replication.IsServer() || !GetGame() || !GetGame().InPlayMode()) return false;
		if (!m_Controller || !m_Controller.IsAIQuotaStartupCurrent(this) || !m_Campaign || !m_Campaign.IsMaster() ||
			GetGame().GetGameMode() != m_Campaign) return false;
		if (!m_MainWorld || GetGame().GetWorld() != m_MainWorld || !m_World ||
			GetGame().GetAIWorld() != m_World || m_World.GetWorld() != m_MainWorld) return false;
		return m_WorldState && m_WorldState.IsCurrentWorld() && m_Manager && m_Manager.GetOwner() == m_Campaign &&
			m_Campaign.FindComponent(SCR_SpawnerAIGroupManagerComponent) == m_Manager;
	}

	static bool IsWithinWait(int elapsedMs)
	{
		return elapsedMs >= 0 && elapsedMs < MAXIMUM_WAIT_MS;
	}

	AICF_EAIQuotaStartup Check()
	{
		if (!IsCurrent()) return Block("STARTUP_CONTEXT_CHANGED");
		// Same-world restart keeps the already active stock policy and its exact
		// bound queue. Only pre-policy native requests need the empty-queue gate.
		if (AICF_AIWorldQuotaService.GetAuthorityForWorld(m_MainWorld))
		{
			if (!IsCurrent()) return Block("STARTUP_CONTEXT_CHANGED");
			m_sLastReason = "EXISTING_WORLD_POLICY";
			return AICF_EAIQuotaStartup.READY;
		}
		int elapsedMs = System.GetTickCount(m_iStartedAtMs);
		if (!IsWithinWait(elapsedMs)) return Block("NATIVE_PAID_CATALOG_QUEUE_DEADLINE");
		bool empty = m_Manager.AICF_IsNativeQuotaQueueEmpty();
		if (!IsCurrent()) return Block("STARTUP_CONTEXT_CHANGED");
		if (empty)
		{
			m_sLastReason = "NATIVE_PAID_CATALOG_QUEUE_DRAINED";
			return AICF_EAIQuotaStartup.READY;
		}
		m_sLastReason = "NATIVE_PAID_CATALOG_QUEUE_PENDING";
		if (elapsedMs - m_iLastWaitLogMs >= WAIT_LOG_INTERVAL_MS)
		{
			m_iLastWaitLogMs = elapsedMs;
			AICF_Stage1Diagnostics.Info("AI_QUOTA_STARTUP_WAIT", string.Format(
				"reason=%1 original_stock_limit=%2 native_limit=%3 elapsed_ms=%4 maximum_ms=%5 queue_untouched=1 policy_published=0",
				m_sLastReason, m_WorldState.GetOriginalNativeLimit(), m_World.GetLimitOfActiveAIs(), elapsedMs, MAXIMUM_WAIT_MS));
		}
		Schedule();
		return AICF_EAIQuotaStartup.WAIT;
	}

	protected AICF_EAIQuotaStartup Block(string reason)
	{
		m_sLastReason = reason;
		Cancel();
		return AICF_EAIQuotaStartup.BLOCKED;
	}

	protected void Schedule()
	{
		if (m_bScheduled || !IsCurrent()) return;
		m_bScheduled = true;
		GetGame().GetCallqueue().CallLater(Retry, RETRY_INTERVAL_MS, false);
	}

	protected void Retry()
	{
		m_bScheduled = false;
		if (!IsCurrent()) { Cancel(); return; }
		m_Controller.Start(m_Campaign, m_Config, m_Content, m_sMapKey);
	}

	void Cancel()
	{
		m_bCancelled = true;
		m_bScheduled = false;
		if (GetGame()) GetGame().GetCallqueue().Remove(Retry);
	}

	void ~AICF_AIQuotaStartup()
	{
		Cancel();
	}
}
