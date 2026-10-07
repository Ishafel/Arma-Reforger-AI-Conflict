// Только изолированный stage с -aicfInfantryAdvanceProbe 1.
// Инъекция угроз проверяет production policy на настоящих готовых бойцах.
modded class SCR_AIObserveThreatSystemBehavior
{
	float AICF_ProbePriority(int sector, float elapsed)
	{
		bool active = m_bBehaviorActive;
		int previousSector = m_iCurrentSector;
		float duration = m_fHighPriorityDuration_s;
		WorldTimestamp started = m_TimestampStartHighPriorityState;
		m_bBehaviorActive = true;
		m_iCurrentSector = sector;
		m_fHighPriorityDuration_s = 6.0;
		m_TimestampStartHighPriorityState = GetGame().GetWorld().GetTimestamp().PlusSeconds(-elapsed);
		float result = CustomEvaluate();
		m_bBehaviorActive = active;
		m_iCurrentSector = previousSector;
		m_fHighPriorityDuration_s = duration;
		m_TimestampStartHighPriorityState = started;
		return result;
	}
}

modded class AICF_GroupSlot
{
	bool AICF_ProbeStaleAdvance(SCR_AIUtilityComponent utility)
	{
		SCR_AIGroup group = m_Group;
		m_Group = null;
		bool denied = !AICF_InfantryAdvancePolicy.IsAdvancing(utility);
		m_Group = group;
		return denied;
	}

	bool AICF_ProbeSuspendedAdvance(SCR_AIUtilityComponent utility)
	{
		AIWaypoint waypoint = m_Waypoint;
		m_Waypoint = null;
		bool denied = !AICF_InfantryAdvancePolicy.IsAdvancing(utility);
		m_Waypoint = waypoint;
		return denied;
	}
}

modded class AICF_MatchController
{
	protected bool m_bAICFAdvanceProbeStarted;
	protected int m_iAICFAdvanceChecks;
	protected int m_iAICFAdvanceFailures;
	protected int m_iAICFAdvancePolls;
	protected ref array<SCR_AIUtilityComponent> m_aAICFAdvanceUtilities = {};
	protected ref array<vector> m_aAICFAdvancePositions = {};
	protected ref array<float> m_aAICFAdvanceDistances = {};

	protected void AICF_AdvanceCheck(string name, bool passed)
	{
		m_iAICFAdvanceChecks++;
		if (!passed)
			m_iAICFAdvanceFailures++;
		Print(string.Format("[AICF][ADVANCE_PROBE_CHECK] name=%1 passed=%2", name, passed));
	}

	override protected void TryLogRosterReady()
	{
		super.TryLogRosterReady();
		string enabled;
		if (!m_bRosterReady || m_bAICFAdvanceProbeStarted ||
			!System.GetCLIParam("aicfInfantryAdvanceProbe", enabled) || enabled != "1")
			return;
		m_bAICFAdvanceProbeStarted = true;
		// Сохраняем одночленный initial roster без отвлечения на казармы.
		m_USState.GetSlot(0).SetDesiredSize(1);
		m_USSRState.GetSlot(0).SetDesiredSize(1);
		GetGame().GetCallqueue().CallLater(AICF_BeginAdvanceProbe, 2000, false);
	}

	protected void AICF_ResetProbeThreats(SCR_AIUtilityComponent utility)
	{
		for (int i; i < SCR_AISectorThreatFilter.SECTOR_COUNT; i++)
			utility.m_SectorThreatFilter.GetSector(i).Reset();
		utility.m_SectorThreatFilter.Update(1.0);
		utility.m_ThreatSystem.SetThreatValues(0, 0, 0, 0);
	}

	protected int AICF_AddProbeThreat(SCR_AIUtilityComponent utility, vector offset, bool direct = false)
	{
		utility.m_SectorThreatFilter.OnShotsFired(utility.m_OwnerEntity.GetOrigin() + offset, 1, direct);
		utility.m_SectorThreatFilter.Update(1.0);
		int major, minor;
		utility.m_SectorThreatFilter.GetActiveSectors(major, minor);
		return major;
	}

	protected void AICF_TestAdvanceFaction(AICF_GroupSlot slot, SCR_CampaignFaction faction)
	{
		string side = faction.GetFactionKey();
		IEntity character = AICF_GroupRuntime.ResolveAliveLeader(slot.GetGroup());
		AIAgent agent = slot.GetGroup().GetLeaderAgent();
		SCR_AIUtilityComponent utility;
		if (agent)
			utility = SCR_AIUtilityComponent.Cast(agent.FindComponent(SCR_AIUtilityComponent));
		bool setup = character && utility && slot.GetWaypoint() && !slot.IsRecruitingInfantry() &&
			m_OrderPlanner.AssignPlayerOrder(slot, faction, slot.GetTargetBase());
		AICF_AdvanceCheck(side + "_SETUP", setup);
		if (!setup)
			return;
		array<ref AIActionBase> actions = {};
		utility.GetActions(actions);
		SCR_AIObserveThreatSystemBehavior observe;
		foreach (AIActionBase action : actions)
		{
			observe = SCR_AIObserveThreatSystemBehavior.Cast(action);
			if (observe)
				break;
		}
		AICF_AdvanceCheck(side + "_NATIVE_OBSERVER", observe != null);
		if (!observe)
			return;
		// Synchronous scope tests используют тот же waypoint; после них endpoint восстановлен.
		AIWaypoint waypoint = slot.GetWaypoint();
		vector originalWaypoint = waypoint.GetOrigin();
		waypoint.SetOrigin(character.GetOrigin() + "300 0 0");
		AICF_ResetProbeThreats(utility);
		int sector = AICF_AddProbeThreat(utility, "250 0 0");
		AICF_AdvanceCheck(side + "_MARCH_SCOPE", AICF_InfantryAdvancePolicy.IsAdvancing(utility));
		AICF_AdvanceCheck(side + "_DISTANT_INITIAL_REACTION", observe.AICF_ProbePriority(sector, 0.5) == 69);
		AICF_AdvanceCheck(side + "_DISTANT_RESUME", observe.AICF_ProbePriority(sector, 2.0) == 4);
		AICF_ResetProbeThreats(utility);
		sector = AICF_AddProbeThreat(utility, "100 0 0");
		AICF_AdvanceCheck(side + "_MEDIUM_INITIAL_REACTION", observe.AICF_ProbePriority(sector, 2.0) == 69);
		AICF_AdvanceCheck(side + "_MEDIUM_RESUME", observe.AICF_ProbePriority(sector, 3.1) == 4);
		AICF_ResetProbeThreats(utility);
		sector = AICF_AddProbeThreat(utility, "50 0 0");
		AICF_AdvanceCheck(side + "_CLOSE_FIRE_NATIVE", observe.AICF_ProbePriority(sector, 4.0) == 69);
		AICF_ResetProbeThreats(utility);
		sector = AICF_AddProbeThreat(utility, "250 0 0");
		AICF_AddProbeThreat(utility, "-250 0 0", true);
		AICF_AdvanceCheck(side + "_SIDE_FLYBY_NATIVE", observe.AICF_ProbePriority(sector, 4.0) == 69);
		AICF_ResetProbeThreats(utility);
		sector = AICF_AddProbeThreat(utility, "250 0 0");
		utility.m_ThreatSystem.ThreatBulletImpact(1);
		AICF_AdvanceCheck(side + "_SUPPRESSION_NATIVE", observe.AICF_ProbePriority(sector, 4.0) == 69);
		utility.m_ThreatSystem.SetThreatValues(0, 0, 0.3, 0);
		AICF_AdvanceCheck(side + "_INJURY_NATIVE", observe.AICF_ProbePriority(sector, 4.0) == 69);
		utility.m_ThreatSystem.SetThreatValues(0, 0, 0, 0);
		utility.m_SectorThreatFilter.OnDamageTaken(character.GetOrigin() + "250 0 0");
		utility.m_SectorThreatFilter.Update(1.0);
		AICF_AdvanceCheck(side + "_DAMAGE_NATIVE", observe.AICF_ProbePriority(sector, 4.0) == 69);
		AICF_ResetProbeThreats(utility);
		sector = AICF_AddProbeThreat(utility, "250 0 0");
		waypoint.SetOrigin(character.GetOrigin());
		AICF_AdvanceCheck(side + "_ARRIVED_NATIVE", observe.AICF_ProbePriority(sector, 4.0) == 69);
		waypoint.SetOrigin(character.GetOrigin() + "300 0 0");
		AICF_AdvanceCheck(side + "_STALE_GROUP_DENIED", slot.AICF_ProbeStaleAdvance(utility));
		AICF_AdvanceCheck(side + "_VEHICLE_SUSPEND_DENIED", slot.AICF_ProbeSuspendedAdvance(utility));
		slot.SetUnitType(AICF_EGroupUnitType.MOTORIZED_ARMED_LIGHT);
		AICF_AdvanceCheck(side + "_CREW_DENIED", !AICF_InfantryAdvancePolicy.IsAdvancing(utility));
		slot.SetUnitType(AICF_EGroupUnitType.INFANTRY);
		waypoint.SetOrigin(originalWaypoint);
		AICF_ResetProbeThreats(utility);
		m_aAICFAdvanceUtilities.Insert(utility);
		m_aAICFAdvancePositions.Insert(character.GetOrigin());
		m_aAICFAdvanceDistances.Insert(vector.DistanceXZ(character.GetOrigin(), waypoint.GetOrigin()));
	}

	protected void AICF_BeginAdvanceProbe()
	{
		AICF_TestAdvanceFaction(m_USState.GetSlot(0), m_USFaction);
		AICF_TestAdvanceFaction(m_USSRState.GetSlot(0), m_USSRFaction);
		AICF_AdvanceCheck("BOTH_FACTIONS_TESTED", m_aAICFAdvanceUtilities.Count() == 2);
		GetGame().GetCallqueue().CallLater(AICF_PollAdvanceProbe, 1000, true);
	}

	protected void AICF_PollAdvanceProbe()
	{
		m_iAICFAdvancePolls++;
		foreach (int index, SCR_AIUtilityComponent utility : m_aAICFAdvanceUtilities)
		{
			if (!utility || !AICF_GroupRuntime.IsAliveCharacter(utility.m_OwnerEntity))
				continue;
			// Умеренная фоновая стрельба: без урона, цели или близкого пролёта.
			utility.m_SectorThreatFilter.OnShotsFired(utility.m_OwnerEntity.GetOrigin() + "250 0 0", 1, false);
			utility.m_ThreatSystem.ThreatShotFired(250, 1);
			if (m_iAICFAdvancePolls < 30)
				continue;
			SCR_AIGroup group = SCR_AIGroup.Cast(utility.GetAIAgent().GetParentGroup());
			AIWaypoint waypoint = group.GetCurrentWaypoint();
			float reduction;
			if (waypoint)
				reduction = m_aAICFAdvanceDistances[index] - vector.DistanceXZ(utility.m_OwnerEntity.GetOrigin(), waypoint.GetOrigin());
			float displacement = vector.DistanceXZ(m_aAICFAdvancePositions[index], utility.m_OwnerEntity.GetOrigin());
			Print(string.Format("[AICF][ADVANCE_PROBE_MOVEMENT] side=%1 elapsed_s=30 displacement_m=%2 route_reduction_m=%3 eligible=%4",
				group.GetFaction().GetFactionKey(), displacement, reduction, AICF_InfantryAdvancePolicy.IsAdvancing(utility)));
		}
		if (m_iAICFAdvancePolls >= 30)
		{
			Print(string.Format("[AICF][ADVANCE_PROBE_FINISHED] checks=%1 failures=%2", m_iAICFAdvanceChecks, m_iAICFAdvanceFailures));
			GetGame().GetCallqueue().Remove(AICF_PollAdvanceProbe);
			GetGame().RequestClose();
		}
	}

	override protected void Stop(bool cleanupEntities)
	{
		if (GetGame())
		{
			GetGame().GetCallqueue().Remove(AICF_BeginAdvanceProbe);
			GetGame().GetCallqueue().Remove(AICF_PollAdvanceProbe);
		}
		super.Stop(cleanupEntities);
	}
}
