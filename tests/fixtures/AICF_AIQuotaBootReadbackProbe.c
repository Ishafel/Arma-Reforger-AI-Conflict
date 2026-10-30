// Diagnostic only: no activation, health, order, roster or quota writes.
class AICF_QuotaBootReadback
{
	void Start() { GetGame().GetCallqueue().CallLater(Read, 15000, true); }
	void Stop() { if (GetGame()) GetGame().GetCallqueue().Remove(Read); }
	protected void Read()
	{
		if (!Replication.IsServer() || !GetGame()) return;
		SCR_AIWorld world = SCR_AIWorld.Cast(GetGame().GetAIWorld());
		if (!world || world.GetWorld() != GetGame().GetWorld()) return;
		array<AIAgent> agents = {};
		world.GetAIAgents(agents);
		int examined, invalid, chars, aliveInactiveUncounted, aliveCounted, vehicles, sample;
		bool bounded = agents.Count() <= 4096;
		if (bounded)
		{
			foreach (AIAgent agent : agents)
			{
				examined++;
				if (!agent || agent.GetWorld() != world.GetWorld()) { invalid++; continue; }
				IEntity entity = agent.GetControlledEntity();
				if (Vehicle.Cast(entity)) vehicles++;
				ChimeraCharacter actor = ChimeraCharacter.Cast(entity);
				if (!actor) continue;
				chars++;
				CharacterControllerComponent control = actor.GetCharacterController();
				if (!control || control.IsPlayerControlled() || control.GetLifeState() != ECharacterLifeState.ALIVE) continue;
				bool counted = agent.CountTowardsAIWorldLimit();
				bool active = agent.IsAIActivated();
				if (counted) aliveCounted++;
				if (counted || active) continue;
				aliveInactiveUncounted++;
				if (sample >= 8) continue;
				sample++;
				EntityID actorId = actor.GetID();
				EntityID agentId = agent.GetID();
				AIGroup group = agent.GetParentGroup();
				EntityID groupId;
				if (group) groupId = group.GetID();
				Print(string.Format("[AICF][QUOTA_BOOT_INACTIVE] actor=%1 agent=%2 group=%3 counted=%4 activated=%5 life=ALIVE diagnostic_only=1", actorId, agentId, groupId, counted, active));
			}
		}
		string details = string.Format("registered=%1 examined=%2 invalid=%3 chars=%4 alive_counted=%5 alive_inactive_uncounted=%6 vehicles=%7 bounded=%8", agents.Count(), examined, invalid, chars, aliveCounted, aliveInactiveUncounted, vehicles, bounded);
		Print(string.Format("[AICF][QUOTA_BOOT_CENSUS] %1 active=%2 intended=%3 hard=%4 diagnostic_only=1", details, world.GetCurrentNumOfActiveAIs(), world.GetActiveGroupIntendedCount(), world.GetLimitOfActiveAIs()));
		AICF_AIQuotaCensus snapshot = new AICF_AIQuotaCensus();
		bool known = snapshot.Capture(world);
		Print(string.Format("[AICF][QUOTA_BOOT_NATIVE_CAPTURE] known=%1 reason=%2 diagnostic_only=1", known, snapshot.GetReason()));
		AICF_AIWorldQuotaService quota = AICF_AIWorldQuotaService.GetAuthorityForWorld(GetGame().GetWorld());
		if (!quota) { Print("[AICF][QUOTA_BOOT_POLICY] active=0 diagnostic_only=1"); return; }
		string budget = quota.BudgetDetails(AICF_EAIQuotaBucket.STOCK, 0);
		Print(string.Format("[AICF][QUOTA_BOOT_POLICY] active=1 configured=%1 owner_current=%2 reason=%3 %4 diagnostic_only=1", quota.IsConfigured(), quota.IsOwnerCurrent(), quota.GetLastReason(), budget));
	}
}
modded class ArmaReforgerScripted
{
	protected ref AICF_QuotaBootReadback m_AICFQuotaBootReadback;
	override bool OnGameStart()
	{
		bool result = super.OnGameStart();
		if (result && Replication.IsServer() && System.IsCLIParam("aicfQuotaBootReadback"))
		{
			m_AICFQuotaBootReadback = new AICF_QuotaBootReadback();
			m_AICFQuotaBootReadback.Start();
		}
		return result;
	}
	override void OnGameEnd()
	{
		if (m_AICFQuotaBootReadback) m_AICFQuotaBootReadback.Stop();
		m_AICFQuotaBootReadback = null;
		super.OnGameEnd();
	}
}
