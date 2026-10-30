// CONTROLLED STOCK CHARACTER PRESSURE ONLY. Normal birth, recruitment and
// commander APIs; no fake roster, health, activation, LOD, limit or body moves.
class AICF_QuotaStockPressureProof
{
	static bool Full(int physical, int reserved, int limit)
	{
		return limit > 0 && limit <= 4096 && physical == limit && reserved == 0;
	}
	static bool Veto(bool admitted, int agents, int beforeIntended, int afterIntended, bool full)
	{
		return !admitted && agents == 0 && beforeIntended >= 0 && beforeIntended == afterIntended && full;
	}
	static bool Ready(bool current, int ageMs, bool full, bool original, bool recruited, bool exactRoster)
	{
		return current && ageMs >= 35000 && ageMs <= 600000 && full && original && recruited && exactRoster;
	}
}
// Optional profiler wraps the actual call once; it never refreshes/evaluates
// quotas on its own and does not change the caller's result or cadence.
modded class AICF_AIWorldQuotaService
{
	protected int m_iPressureCensusCalls;
	protected int m_iPressureCensusMs;
	protected int m_iPressureCensusMaxMs;
	protected int m_iPressureCensusReportAt;
	override bool RefreshCensus()
	{
		if (!System.IsCLIParam("aicfQuotaStockPressure")) return super.RefreshCensus();
		int started = System.GetTickCount();
		bool result = super.RefreshCensus();
		int finished = System.GetTickCount();
		int elapsed = finished - started;
		m_iPressureCensusCalls++;
		m_iPressureCensusMs += elapsed;
		m_iPressureCensusMaxMs = Math.Max(m_iPressureCensusMaxMs, elapsed);
		if (finished - m_iPressureCensusReportAt >= 15000 && IsCurrentWorld())
		{
			m_iPressureCensusReportAt = finished;
			string data = string.Format("calls=%1 elapsed_sum_ms=%2 max_ms=%3 last_result=%4 native_active=%5 native_intended=%6",
				m_iPressureCensusCalls, m_iPressureCensusMs, m_iPressureCensusMaxMs, result, m_World.GetCurrentNumOfActiveAIs(), m_World.GetActiveGroupIntendedCount());
			Print("[AICF][QUOTA_CENSUS_PROFILE] " + data + " diagnostic_only=1 added_refresh_calls=0");
			m_iPressureCensusCalls = 0;
			m_iPressureCensusMs = 0;
			m_iPressureCensusMaxMs = 0;
		}
		return result;
	}
}
modded class AICF_MatchController
{
	protected bool m_bQuotaPressureMode;
	protected bool m_bQuotaPressureDone;
	protected bool m_bQuotaPressureBusy;
	protected bool m_bQuotaPressureSelected;
	protected bool m_bQuotaPressurePendingLogged;
	protected bool m_bQuotaPressureRecruitAccepted;
	protected int m_iQuotaPressureNextRecruit;
	protected vector m_vQuotaPressureHome;
	protected int m_iQuotaPressureStarted;
	protected int m_iQuotaPressureFullSince;
	protected int m_iQuotaPressureVetoAt;
	protected int m_iQuotaPressureGeneration;
	protected BaseWorld m_QuotaPressureWorld;
	protected SCR_GameModeCampaign m_QuotaPressureCampaign;
	protected AICF_FactionState m_QuotaPressureState;
	protected AICF_GroupSlot m_QuotaPressureSlot;
	protected SCR_AIGroup m_QuotaPressureOriginal;
	protected EntityID m_QuotaPressureOriginalId;
	protected string m_sQuotaPressureOriginalRpl;
	protected IEntity m_QuotaPressureFirstActor;
	protected EntityID m_QuotaPressureFirstActorId;
	protected string m_sQuotaPressureFirstActorRpl;
	protected SCR_AIGroup m_QuotaPressureExtra;
	protected EntityID m_QuotaPressureExtraId;
	protected string m_sQuotaPressureExtraRpl;
	protected ref array<SCR_AIGroup> m_aQuotaPressureStock = {};

	override void Start(SCR_GameModeCampaign campaign, AICF_Stage1Config immutableConfig = null,
		AICF_ContentProfile contentProfile = null, string mapKey = "UNKNOWN")
	{
		super.Start(campaign, immutableConfig, contentProfile, mapKey);
		string enabled;
		if (m_bQuotaPressureMode || !Replication.IsServer() || !System.GetCLIParam("aicfQuotaStockPressure", enabled) || enabled != "1") return;
		m_bQuotaPressureMode = true;
		m_iQuotaPressureStarted = System.GetTickCount();
		m_QuotaPressureWorld = GetGame().GetWorld();
		m_QuotaPressureCampaign = campaign;
		GetGame().GetCallqueue().CallLater(AICF_QuotaPressureTimeout, 600000, false);
	}

	protected bool AICF_QuotaPressureCurrent()
	{
		return Replication.IsServer() && GetGame() && m_bQuotaPressureMode && !m_bQuotaPressureDone &&
			!m_bStopped && m_bStarted && GetActiveController() == this && GetGame().GetWorld() == m_QuotaPressureWorld &&
			m_Campaign == m_QuotaPressureCampaign && m_Campaign && m_Campaign.IsMaster() && GetGame().GetGameMode() == m_Campaign;
	}
	protected bool AICF_QuotaPressureTargetCurrent()
	{
		if (!AICF_QuotaPressureCurrent() || m_QuotaPressureState != m_USState || !m_USState ||
			m_USState.GetSlot(0) != m_QuotaPressureSlot || !m_QuotaPressureSlot || !m_QuotaPressureOriginal) return false;
		if (m_QuotaPressureSlot.GetGroup() != m_QuotaPressureOriginal ||
			m_QuotaPressureSlot.GetSpawnGeneration() != m_iQuotaPressureGeneration) return false;
		if (m_QuotaPressureOriginal.GetWorld() != m_QuotaPressureWorld || m_QuotaPressureOriginal.GetID() != m_QuotaPressureOriginalId ||
			m_QuotaPressureWorld.FindEntityByID(m_QuotaPressureOriginalId) != m_QuotaPressureOriginal ||
			AICF_VehicleBehaviorBinding.ComponentRpl(m_QuotaPressureOriginal) != m_sQuotaPressureOriginalRpl) return false;
		if (!m_QuotaPressureFirstActor || m_QuotaPressureFirstActor.GetID() != m_QuotaPressureFirstActorId ||
			m_QuotaPressureWorld.FindEntityByID(m_QuotaPressureFirstActorId) != m_QuotaPressureFirstActor ||
			AICF_VehicleBehaviorBinding.ComponentRpl(m_QuotaPressureFirstActor) != m_sQuotaPressureFirstActorRpl) return false;
		return AICF_QuotaPressureCurrent();
	}
	protected void AICF_QuotaPressureLog(string eventName, string text)
	{
		Print(string.Format("[AICF][QUOTA_STOCK_PRESSURE][%1] elapsed_ms=%2 %3", eventName,
			System.GetTickCount(m_iQuotaPressureStarted), text));
	}
	protected void AICF_QuotaPressureFinish(bool passed, string reason)
	{
		if (!AICF_QuotaPressureCurrent()) return;
		m_bQuotaPressureDone = true;
		GetGame().GetCallqueue().Remove(AICF_QuotaPressureTimeout);
		AICF_QuotaPressureLog("DONE", string.Format("passed=%1 reason=%2 scope=STOCK_CHARACTER_MANAGED_RECRUIT_ONLY original_route_pass=0 actor_deletes=0", passed, reason));
		GetGame().RequestClose();
	}
	protected void AICF_QuotaPressureTimeout()
	{
		if (!Replication.IsServer() || !GetGame() || m_bQuotaPressureDone ||
			GetGame().GetWorld() != m_QuotaPressureWorld || !m_QuotaPressureCampaign ||
			GetGame().GetGameMode() != m_QuotaPressureCampaign || !m_QuotaPressureCampaign.IsMaster()) return;
		if (GetActiveController() && GetActiveController() != this) return;
		m_bQuotaPressureDone = true;
		GetGame().GetCallqueue().Remove(AICF_QuotaPressureTimeout);
		AICF_QuotaPressureLog("DONE", "passed=0 reason=BOUNDED_SETUP_OR_PROOF_TIMEOUT scope=STOCK_CHARACTER_MANAGED_RECRUIT_ONLY");
		GetGame().RequestClose();
	}
	protected SCR_AIGroup AICF_QuotaPressureNewStock(int units)
	{
		if (!AICF_QuotaPressureTargetCurrent() || !m_GroupSpawner || !m_USFaction || !m_USFaction.GetMainBase() || units < 1 || units > 10) return null;
		SCR_AIGroup group = m_GroupSpawner.SpawnGroup(m_USFaction, m_USFaction.GetMainBase(), 100 + m_aQuotaPressureStock.Count(), units, false);
		if (!AICF_QuotaPressureTargetCurrent() || !group) return null;
		AICF_AIWorldQuotaService quota = AICF_AIWorldQuotaService.GetAuthorityForWorld(m_QuotaPressureWorld);
		AICF_EAIQuotaBucket bucket;
		if (!quota || !quota.IsOwnerCurrent() || quota.GetQuotaController() != this || quota.GetOwners().Find(group) ||
			!quota.GetOwners().ClassifyGroup(group, bucket) || bucket != AICF_EAIQuotaBucket.STOCK ||
			group.GetAgentsCount() != 0 || group.GetFaction() != m_USFaction) return null;
		group.SetLifecyclePolicy(SCR_EAIGroupLifecyclePolicy.Manual);
		group.SetDeleteWhenEmpty(false);
		m_aQuotaPressureStock.Insert(group);
		EntityID groupId = group.GetID();
		string rpl = AICF_VehicleBehaviorBinding.ComponentRpl(group);
		AICF_QuotaPressureLog("NEW_EMPTY_STOCK", string.Format("group=%1 rpl=%2 units=%3 faction=US typed_managed_owner=0 actual_agents=0", groupId, rpl, units));
		return group;
	}

	override protected void Update()
	{
		super.Update();
		if (!AICF_QuotaPressureCurrent() || m_bQuotaPressureBusy || !m_USState || !m_USFaction) return;
		m_bQuotaPressureBusy = true;
		AICF_QuotaPressureTick();
		m_bQuotaPressureBusy = false;
	}
	protected void AICF_QuotaPressureTick()
	{
		AICF_AIWorldQuotaService quota = AICF_AIWorldQuotaService.GetAuthorityForWorld(m_QuotaPressureWorld);
		if (!quota || !quota.IsConfigured() || !quota.IsOwnerCurrent() || quota.GetQuotaController() != this || !quota.GetLimits() || !quota.GetLedger())
		{
			if (m_QuotaPressureExtra) AICF_QuotaPressureFinish(false, "PRESSURE_CENSUS_OR_OWNER_UNKNOWN");
			return;
		}
		if (!m_QuotaPressureSlot)
		{
			AICF_GroupSlot slot = m_USState.GetSlot(0);
			if (!slot || !slot.GetGroup() || slot.GetGroup().GetAgentsCount() != 1 || !slot.GetRosterMember(0)) return;
			m_QuotaPressureState = m_USState;
			m_QuotaPressureSlot = slot;
			m_QuotaPressureOriginal = slot.GetGroup();
			m_QuotaPressureOriginalId = m_QuotaPressureOriginal.GetID();
			m_sQuotaPressureOriginalRpl = AICF_VehicleBehaviorBinding.ComponentRpl(m_QuotaPressureOriginal);
			m_iQuotaPressureGeneration = slot.GetSpawnGeneration();
			m_QuotaPressureFirstActor = slot.GetRosterMember(0);
			m_QuotaPressureFirstActorId = m_QuotaPressureFirstActor.GetID();
			m_sQuotaPressureFirstActorRpl = AICF_VehicleBehaviorBinding.ComponentRpl(m_QuotaPressureFirstActor);
			m_vQuotaPressureHome = m_QuotaPressureFirstActor.GetOrigin();
			if (!AICF_QuotaPressureTargetCurrent() || !m_USState.ApplyCommanderConfiguration(0, slot.GetRole(), AICF_EGroupUnitType.INFANTRY, 1))
			{ AICF_QuotaPressureFinish(false, "TARGET_CONFIG_CONTEXT"); return; }
			slot.TouchCommanderConfiguration();
			if (!AICF_QuotaPressureTargetCurrent() || !m_OrderPlanner || !m_OrderPlanner.AssignPlayerPointOrder(slot, m_USFaction, m_vQuotaPressureHome, false))
			{ AICF_QuotaPressureFinish(false, "INITIAL_NATIVE_POINT_HOLD_REJECTED"); return; }
			AICF_QuotaPressureLog("TARGET_HOLD_ONE", string.Format("group=%1 group_rpl=%2 generation=%3 original_actor=%4 desired=1 normal_commander_api=1 controlled_point_hold=1", m_QuotaPressureOriginalId, m_sQuotaPressureOriginalRpl, m_iQuotaPressureGeneration, m_QuotaPressureFirstActorId));
		}
		if (!AICF_QuotaPressureTargetCurrent()) { AICF_QuotaPressureFinish(false, "TARGET_IDENTITY_CHANGED"); return; }
		int limit = quota.GetLimits().GetLimit(AICF_EAIQuotaBucket.STOCK);
		int physical = quota.GetLedger().GetPhysical(AICF_EAIQuotaBucket.STOCK);
		int reserved = quota.GetLedger().GetReserved(AICF_EAIQuotaBucket.STOCK);
		if (physical < 0 || reserved < 0 || limit < 1)
		{
			if (m_QuotaPressureExtra) AICF_QuotaPressureFinish(false, "PRESSURE_BUDGET_UNKNOWN");
			return;
		}
		if (!m_QuotaPressureExtra)
		{
			int available = limit - physical - reserved;
			if (available > 0)
			{
				m_iQuotaPressureFullSince = 0;
				if (m_aQuotaPressureStock.Count() >= 32) { AICF_QuotaPressureFinish(false, "FIXTURE_GROUP_BOUND"); return; }
				SCR_AIGroup fill = AICF_QuotaPressureNewStock(Math.Min(10, available));
				if (!fill || !AICF_QuotaPressureTargetCurrent()) { AICF_QuotaPressureFinish(false, "NEW_STOCK_CONTEXT"); return; }
				m_GroupSpawner.BeginRosterSpawn(fill, fill.m_aUnitPrefabSlots.Count());
				return;
			}
			if (!AICF_QuotaStockPressureProof.Full(physical, reserved, limit))
			{
				if (!m_bQuotaPressurePendingLogged && reserved > 0 && physical + reserved == limit)
				{
					SCR_AIWorld pendingWorld = SCR_AIWorld.Cast(GetGame().GetAIWorld());
					AICF_QuotaPressureLog("PENDING_BASKETS", string.Format("stock_physical=%1 stock_reserved=%2 stock_limit=%3 native_active=%4 native_intended=%5 metadata_not_fabricated_bodies=1", physical, reserved, limit, pendingWorld.GetCurrentNumOfActiveAIs(), pendingWorld.GetActiveGroupIntendedCount()));
					m_bQuotaPressurePendingLogged = true;
				}
				return;
			}
			if (!m_iQuotaPressureFullSince) { m_iQuotaPressureFullSince = System.GetTickCount(); return; }
			if (System.GetTickCount(m_iQuotaPressureFullSince) < 3000) return;
			SCR_AIGroup extra = AICF_QuotaPressureNewStock(1);
			if (!extra || !AICF_QuotaPressureTargetCurrent()) { AICF_QuotaPressureFinish(false, "EXTRA_STOCK_CONTEXT"); return; }
			SCR_AIWorld world = SCR_AIWorld.Cast(GetGame().GetAIWorld());
			int beforeIntended = world.GetActiveGroupIntendedCount();
			bool admitted = quota.AdmitGroupRequest(extra, 1);
			m_GroupSpawner.BeginRosterSpawn(extra, 1);
			bool fullAfter = AICF_QuotaStockPressureProof.Full(quota.GetLedger().GetPhysical(AICF_EAIQuotaBucket.STOCK), quota.GetLedger().GetReserved(AICF_EAIQuotaBucket.STOCK), limit);
			if (!AICF_QuotaPressureTargetCurrent() || !AICF_QuotaStockPressureProof.Veto(admitted, extra.GetAgentsCount(), beforeIntended, world.GetActiveGroupIntendedCount(), fullAfter))
			{ AICF_QuotaPressureFinish(false, "EXTRA_STOCK_NOT_ATOMICALLY_VETOED"); return; }
			m_QuotaPressureExtra = extra;
			m_QuotaPressureExtraId = extra.GetID();
			m_sQuotaPressureExtraRpl = AICF_VehicleBehaviorBinding.ComponentRpl(extra);
			m_iQuotaPressureVetoAt = System.GetTickCount();
			AICF_QuotaPressureLog("FULL_STOCK_VETO", string.Format("stock_physical=%1 stock_limit=%2 stock_reserved=0 group=%3 rpl=%4 admitted=0 actual_agents=0 native_intended_before=%5 native_intended_after=%6", physical, limit, m_QuotaPressureExtraId, m_sQuotaPressureExtraRpl, beforeIntended, world.GetActiveGroupIntendedCount()));
			if (!m_USState.ApplyCommanderConfiguration(0, m_QuotaPressureSlot.GetRole(), AICF_EGroupUnitType.INFANTRY, 5))
			{ AICF_QuotaPressureFinish(false, "MANAGED_FIVE_INTENT_REJECTED"); return; }
			m_QuotaPressureSlot.TouchCommanderConfiguration();
			if (!AICF_QuotaPressureTargetCurrent() || !m_OrderPlanner.AssignPlayerPointOrder(m_QuotaPressureSlot, m_USFaction, m_vQuotaPressureHome, false))
			{ AICF_QuotaPressureFinish(false, "MANAGED_FIVE_POINT_HOLD_REJECTED"); return; }
			m_bQuotaPressureSelected = true;
			AICF_QuotaPressureLog("MANAGED_FIVE_INTENT", string.Format("group=%1 generation=%2 actual_agents=%3 desired=5 after_physical_stock_full=1 normal_commander_api=1", m_QuotaPressureOriginalId, m_iQuotaPressureGeneration, m_QuotaPressureOriginal.GetAgentsCount()));
			return;
		}
		if (m_QuotaPressureExtra.GetWorld() != m_QuotaPressureWorld || m_QuotaPressureExtra.GetID() != m_QuotaPressureExtraId ||
			m_QuotaPressureWorld.FindEntityByID(m_QuotaPressureExtraId) != m_QuotaPressureExtra ||
			AICF_VehicleBehaviorBinding.ComponentRpl(m_QuotaPressureExtra) != m_sQuotaPressureExtraRpl || m_QuotaPressureExtra.GetAgentsCount() != 0)
		{ AICF_QuotaPressureFinish(false, "EXTRA_STOCK_IDENTITY_OR_BIRTH"); return; }
		if (!AICF_QuotaStockPressureProof.Full(physical, reserved, limit) || !m_bQuotaPressureSelected)
		{ AICF_QuotaPressureFinish(false, "CONTINUOUS_STOCK_PRESSURE_LOST"); return; }
		if (m_InfantryRecruitment && !m_QuotaPressureSlot.IsRecruitingInfantry() &&
			System.GetTickCount() >= m_iQuotaPressureNextRecruit && AICF_GroupRuntime.CountAliveAgents(m_QuotaPressureOriginal) < 5)
		{
			m_iQuotaPressureNextRecruit = System.GetTickCount() + 5000;
			string reason;
			bool accepted = m_InfantryRecruitment.RequestPlayerRecruitment(m_QuotaPressureSlot, m_USFaction, reason);
			if (accepted) m_bQuotaPressureRecruitAccepted = true;
			AICF_QuotaPressureLog("RECRUIT_REQUEST", string.Format("accepted=%1 reason=%2 normal_recruitment_api=1", accepted, reason));
		}
		int actual, factionWrong, nonAlive;
		if (!m_bQuotaPressureRecruitAccepted || System.GetTickCount(m_iQuotaPressureVetoAt) < 35000) return;
		bool exactRoster = AICF_GroupRuntime.HasExactFactionRoster(m_QuotaPressureOriginal, "US", 5, actual, factionWrong, nonAlive);
		if (!exactRoster) return;
		array<AIAgent> agents = {};
		m_QuotaPressureOriginal.GetAgents(agents);
		bool originalPresent;
		foreach (AIAgent agent : agents)
		{
			if (!agent) { AICF_QuotaPressureFinish(false, "MANAGED_AGENT_UNKNOWN"); return; }
			IEntity actor = agent.GetControlledEntity();
			if (!actor) { AICF_QuotaPressureFinish(false, "MANAGED_ACTOR_UNKNOWN"); return; }
			if (actor == m_QuotaPressureFirstActor) originalPresent = true;
			EntityID actorId = actor.GetID();
			string actorRpl = AICF_VehicleBehaviorBinding.ComponentRpl(actor);
			AICF_QuotaPressureLog("MANAGED_MEMBER", string.Format("group=%1 generation=%2 actor=%3 rpl=%4 faction=US actual_native_member=1", m_QuotaPressureOriginalId, m_iQuotaPressureGeneration, actorId, actorRpl));
		}
		if (!AICF_QuotaStockPressureProof.Ready(AICF_QuotaPressureTargetCurrent(), System.GetTickCount(m_iQuotaPressureVetoAt),
			AICF_QuotaStockPressureProof.Full(physical, reserved, limit), originalPresent, m_bQuotaPressureRecruitAccepted, exactRoster))
		{ AICF_QuotaPressureFinish(false, "ORIGINAL_MEMBER_OR_PROOF_CONTEXT"); return; }
		AICF_QuotaPressureLog("PROOF", string.Format("stock_physical=%1 stock_limit=%2 stock_reserved=0 extra_actual=0 actual_managed=5 veto_hold_ms=%3 group=%4 generation=%5 same_faction_mixed=1", physical, limit, System.GetTickCount(m_iQuotaPressureVetoAt), m_QuotaPressureOriginalId, m_iQuotaPressureGeneration));
		AICF_QuotaPressureFinish(true, "STOCK_FULL_MANAGED_RECRUIT_FIVE");
	}
	override protected void Stop(bool cleanupEntities)
	{
		m_bQuotaPressureDone = true;
		GetGame().GetCallqueue().Remove(AICF_QuotaPressureTimeout);
		super.Stop(cleanupEntities);
	}
}
