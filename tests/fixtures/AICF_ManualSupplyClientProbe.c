// Только isolated runtime-source: AIConflictArland/Scripts/Game/AIConflict/Tests.
// Требует загрузки после Core для override AICF_SetSupplyStatus.
// Для подготовки рейсов используется вместе с AICF_LogisticsRuntimeProbe.c.
// GUI не открывается. Используется тот же client intent/RPC, что кнопкой формы.
class AICF_ManualSupplyProbeResults
{
	ref array<int> m_aAccepted = {};
	ref array<int> m_aFinished = {};

	bool Observe(int request, bool busy)
	{
		if (request <= 0) return false;
		if (busy)
		{
			if (m_aAccepted.Contains(request)) return false;
			m_aAccepted.Insert(request);
			return true;
		}
		if (!m_aAccepted.Contains(request) || m_aFinished.Contains(request)) return false;
		m_aFinished.Insert(request);
		return true;
	}

	static bool CanClose(bool submissionsDone, bool pending, int accepted, int finished)
	{
		return submissionsDone && !pending && accepted >= 0 && finished == accepted;
	}
}

// Только test-only snapshot: production UI по-прежнему хранит последнюю заявку.
modded class SCR_PlayerController
{
	protected ref AICF_ManualSupplyProbeResults m_AICFManualProbeResults = new AICF_ManualSupplyProbeResults();
	[RplProp(condition: RplCondition.OwnerOnly)]
	protected int m_iAICFManualProbeAcceptedCount;
	[RplProp(condition: RplCondition.OwnerOnly)]
	protected int m_iAICFManualProbeFinishedCount;

	override void AICF_SetSupplyStatus(int request, bool busy, string status)
	{
		super.AICF_SetSupplyStatus(request, busy, status);
		// Старый token может быть скрыт production snapshot, но его результат
		// всё равно нужен probe. Повторные статусы одного token считаются один раз.
		if (!Replication.IsServer() || !m_AICFManualProbeResults.Observe(request, busy)) return;
		m_iAICFManualProbeAcceptedCount = m_AICFManualProbeResults.m_aAccepted.Count();
		m_iAICFManualProbeFinishedCount = m_AICFManualProbeResults.m_aFinished.Count();
		Replication.BumpMe();
	}

	int AICF_ManualProbeAcceptedCount() { return m_iAICFManualProbeAcceptedCount; }
	int AICF_ManualProbeFinishedCount() { return m_iAICFManualProbeFinishedCount; }

	bool AICF_ManualProbeCanClose(bool submissionsDone)
	{
		return AICF_ManualSupplyProbeResults.CanClose(submissionsDone, AICF_IsSupplyRequestPending(),
			m_iAICFManualProbeAcceptedCount, m_iAICFManualProbeFinishedCount);
	}
}

modded class SCR_GameModeCampaign
{
	protected int m_iAICFManualProbeStep;
	protected int m_iAICFManualProbeAttempts;
	protected bool m_bAICFManualProbeAccepted;
	protected bool m_bAICFManualProbeConcurrent;
	protected bool m_bAICFManualProbeSecondSent;
	protected bool m_bAICFManualProbeTimedOut;
	protected int m_iAICFManualProbeConcurrentAtMs;
	protected string m_sAICFManualProbeLastStatus;

	override void OnGameStart()
	{
		super.OnGameStart();
		string enabled;
		if (Replication.IsServer() || !System.GetCLIParam("aicfManualSupplyProbe", enabled) || enabled != "1") return;
		string startStep;
		if (System.GetCLIParam("aicfManualSupplyProbeStartStep", startStep)) m_iAICFManualProbeStep = startStep.ToInt();
		string concurrent;
		m_bAICFManualProbeConcurrent = System.GetCLIParam("aicfManualSupplyProbeConcurrent", concurrent) && concurrent == "1";
		GetGame().GetCallqueue().CallLater(AICF_ManualProbeTick, 20000, true);
		GetGame().GetCallqueue().CallLater(AICF_ManualProbeDeadline, 330000, false);
	}

	protected void AICF_ManualProbeTick()
	{
		SCR_PlayerController player = SCR_PlayerController.Cast(GetGame().GetPlayerController());
		SCR_CampaignFaction faction = SCR_CampaignFaction.Cast(SCR_FactionManager.SGetLocalPlayerFaction());
		if (!player) return;
		if (m_bAICFManualProbeConcurrent)
		{
			m_bAICFManualProbeAccepted = player.AICF_ManualProbeAcceptedCount() > 0;
			if (player.AICF_ManualProbeCanClose(m_bAICFManualProbeSecondSent || m_bAICFManualProbeTimedOut))
			{
				AICF_ManualProbeClose();
				return;
			}
			if (m_bAICFManualProbeTimedOut) return;
		}
		string stable;
		if (System.GetCLIParam("aicfLogisticsClientSpawnFaction", stable))
		{
			Faction desired = GetGame().GetFactionManager().GetFactionByKey(AICF_ContentProfile.GetActive().GetRuntimeFactionKey(stable));
			SCR_PlayerFactionAffiliationComponent affiliation = SCR_PlayerFactionAffiliationComponent.Cast(player.FindComponent(SCR_PlayerFactionAffiliationComponent));
			if (desired && affiliation && affiliation.GetAffiliatedFaction() != desired)
			{
				affiliation.RequestFaction(desired);
				Print("[AICF][MANUAL_PROBE_FACTION] test_only=1 requested=" + stable);
				return;
			}
		}
		if (!faction || !faction.GetMainBase()) return;
		string status = player.AICF_GetSupplyStatus();
		if (status != m_sAICFManualProbeLastStatus)
		{
			m_sAICFManualProbeLastStatus = status;
			Print(string.Format("[AICF][MANUAL_PROBE_RESPONSE] test_only=1 step=%1 busy=%2 status=%3", m_iAICFManualProbeStep, player.AICF_IsSupplyBusy(), status));
		}
		if (m_iAICFManualProbeStep >= 4 && player.AICF_IsSupplyBusy() && !status.Contains("Ожидается ответ")) m_bAICFManualProbeAccepted = true;
		if (!m_bAICFManualProbeConcurrent && m_bAICFManualProbeAccepted && !player.AICF_IsSupplyBusy())
		{
			AICF_ManualProbeClose();
			return;
		}
		if (m_bAICFManualProbeAccepted && !m_iAICFManualProbeConcurrentAtMs)
			m_iAICFManualProbeConcurrentAtMs = System.GetTickCount() + 40000;
		bool sendConcurrent = m_bAICFManualProbeConcurrent && m_bAICFManualProbeAccepted && !m_bAICFManualProbeSecondSent &&
			player.AICF_IsSupplyBusy() && !player.AICF_IsSupplyRequestPending() && System.GetTickCount() >= m_iAICFManualProbeConcurrentAtMs;
		if ((player.AICF_IsSupplyBusy() || m_bAICFManualProbeAccepted) && !sendConcurrent) return;
		array<ref AICF_SupplyMapBase> bases = {};
		AICF_SupplyMapData.Collect(faction, bases);
		AICF_SupplyMapBase source;
		AICF_SupplyMapBase destination;
		float nearest = float.MAX;
		foreach (AICF_SupplyMapBase entry : bases)
		{
			if (entry.m_Base == faction.GetMainBase()) { destination = entry; continue; }
			int supplies;
			if (!AICF_SupplyMapData.ReadSupplies(entry, faction, supplies) || supplies < 100) continue;
			float distance = vector.DistanceXZ(entry.m_Base.GetOwner().GetOrigin(), faction.GetMainBase().GetOwner().GetOrigin());
			if (distance >= nearest) continue;
			nearest = distance;
			source = entry;
		}
		if (!source || !destination || !source.NetworkId().IsValid() || !destination.NetworkId().IsValid()) return;
		if (m_iAICFManualProbeStep == 0)
			player.AICF_RequestSupplyTransport(source.NetworkId(), source.NetworkId(), 100);
		else if (m_iAICFManualProbeStep == 1)
			player.AICF_RequestSupplyTransport(source.NetworkId(), destination.NetworkId(), -1);
		else if (m_iAICFManualProbeStep == 2)
		{
			array<SCR_MilitaryBaseComponent> all = {};
			SCR_MilitaryBaseSystem.GetInstance().GetBases(all);
			foreach (SCR_MilitaryBaseComponent candidate : all)
			{
				SCR_CampaignMilitaryBaseComponent enemy = SCR_CampaignMilitaryBaseComponent.Cast(candidate);
				if (!enemy || !enemy.GetFaction() || enemy.GetFaction() == faction || !enemy.GetOwner()) continue;
				AICF_SupplyMapBase target = new AICF_SupplyMapBase(enemy);
				if (!target.NetworkId().IsValid()) continue;
				player.AICF_RequestSupplyTransport(source.NetworkId(), target.NetworkId(), 100);
				break;
			}
		}
		else
		{
			if (++m_iAICFManualProbeAttempts > 8) return;
			if (sendConcurrent)
			{
				m_bAICFManualProbeSecondSent = true;
				Print("[AICF][MANUAL_PROBE_CONCURRENT_SENT] test_only=1 previous_busy=1 pending_reply=0");
			}
			player.AICF_RequestSupplyTransport(source.NetworkId(), destination.NetworkId(), 100);
			player.AICF_RequestSupplyTransport(source.NetworkId(), destination.NetworkId(), 100);
		}
		Print(string.Format("[AICF][MANUAL_PROBE_SENT] test_only=1 step=%1 source=%2 destination=%3 duplicate=%4", m_iAICFManualProbeStep, source.NetworkId(), destination.NetworkId(), m_iAICFManualProbeStep >= 3));
		m_iAICFManualProbeStep++;
	}

	protected void AICF_ManualProbeDeadline()
	{
		if (!m_bAICFManualProbeConcurrent)
		{
			AICF_ManualProbeClose();
			return;
		}
		m_bAICFManualProbeTimedOut = true;
		Print("[AICF][MANUAL_PROBE_TIMEOUT] test_only=1 verdict=FAIL waiting_for_all_accepted_results=1");
		// Больше не отправляем заявки, но не отменяем чужим disconnect их работу.
		// Server fixture имеет свой deadline/Stop и отдаёт оставшиеся результаты.
		AICF_ManualProbeTick();
	}

	protected void AICF_ManualProbeClose()
	{
		SCR_PlayerController player = SCR_PlayerController.Cast(GetGame().GetPlayerController());
		if (m_bAICFManualProbeConcurrent && (!player ||
			!player.AICF_ManualProbeCanClose(m_bAICFManualProbeSecondSent || m_bAICFManualProbeTimedOut))) return;
		GetGame().GetCallqueue().Remove(AICF_ManualProbeTick);
		GetGame().GetCallqueue().Remove(AICF_ManualProbeDeadline);
		Print(string.Format("[AICF][MANUAL_PROBE_CLIENT_FINISHED] test_only=1 accepted=%1", m_bAICFManualProbeAccepted));
		if (m_bAICFManualProbeConcurrent)
			Print(string.Format("[AICF][MANUAL_PROBE_CONCURRENT_FINISHED] test_only=1 second_sent=%1 server_acceptance_requires_log=1 accepted=%2 finished=%3 timed_out=%4",
				m_bAICFManualProbeSecondSent, player.AICF_ManualProbeAcceptedCount(), player.AICF_ManualProbeFinishedCount(), m_bAICFManualProbeTimedOut));
		GetGame().RequestClose();
	}

	void ~SCR_GameModeCampaign()
	{
		if (!GetGame()) return;
		GetGame().GetCallqueue().Remove(AICF_ManualProbeTick);
		GetGame().GetCallqueue().Remove(AICF_ManualProbeDeadline);
	}
}
