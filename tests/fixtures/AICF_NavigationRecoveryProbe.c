// Только изолированный stage. Координаты воспроизводят snapshot 2026-09-19.
// Test setup перемещает исходных бойцов; recovery и дальнейшее движение — production.
// Radio eligibility двух тестовых целей задаётся fixture: это gate навигации,
// а не проверка стратегического выбора или воспроизведение всей кампании.
modded class AICF_OrderPlanner
{
	override protected bool IsTargetValidForRole(AICF_GroupSlot slot,
		SCR_CampaignFaction faction, SCR_CampaignMilitaryBaseComponent target)
	{
		string enabled;
		if (System.GetCLIParam("aicfNavigationProbe", enabled) && enabled == "1" &&
			slot && slot.GetSlotId() == 0 && slot.GetRole() == AICF_EGroupRole.ATTACK &&
			target && target.GetOwner() && target.IsInitialized() && !target.IsHQ())
		{
			string name = target.GetOwner().GetName();
			if (name == "TownBaseMeaux" || name == "TownBaseKermovan")
				return true;
		}
		return super.IsTargetValidForRole(slot, faction, target);
	}
}

class AICF_NavigationDeniedWatchdog : AICF_VehicleWatchdog
{
	int m_iPlayerChecks;
	override bool CanApplyHiddenRecovery(vector source, vector destination,
		float playerProtectionRadiusMeters, out float nearestPlayerMeters, out string rejectionReason)
	{
		m_iPlayerChecks++;
		nearestPlayerMeters = 1;
		rejectionReason = "PROBE_PLAYER_NEARBY";
		return false;
	}
}

modded class AICF_MatchController
{
	protected bool m_bAICFNavigationProbeStarted;
	protected int m_iAICFNavigationProbePolls;
	protected ref array<vector> m_aAICFNavigationAnchors = {"5231.34 35.7214 10695.8", "5975.95 0.0492825 9670.6"};
	protected ref array<SCR_AIGroup> m_aAICFNavigationGroups = {};
	protected ref array<AIWaypoint> m_aAICFNavigationWaypoints = {};
	protected ref array<int> m_aAICFNavigationIntents = {};
	protected ref array<int> m_aAICFNavigationGenerations = {};
	protected ref array<float> m_aAICFNavigationDisplacements = {0, 0};
	protected bool m_bAICFNavigationFencePassed;
	protected bool m_bAICFNavigationBudgetPassed;

	override protected void TryLogRosterReady()
	{
		super.TryLogRosterReady();
		string enabled;
		if (!m_bRosterReady || m_bAICFNavigationProbeStarted ||
			!System.GetCLIParam("aicfNavigationProbe", enabled) || enabled != "1")
			return;
		m_bAICFNavigationProbeStarted = true;
		m_USState.GetSlot(0).SetDesiredSize(1);
		m_USSRState.GetSlot(0).SetDesiredSize(1);
		GetGame().GetCallqueue().CallLater(AICF_BeginNavigationProbe, 1000, false);
	}

	protected void AICF_BeginNavigationProbe()
	{
		array<SCR_MilitaryBaseComponent> bases = {};
		SCR_MilitaryBaseSystem.GetInstance().GetBases(bases);
		array<string> names = {"TownBaseMeaux", "TownBaseKermovan"};
		for (int side; side < 2; side++)
		{
			AICF_GroupSlot slot = m_USState.GetSlot(0);
			SCR_CampaignFaction faction = m_USFaction;
			if (side == 1)
			{
				slot = m_USSRState.GetSlot(0);
				faction = m_USSRFaction;
			}
			SCR_CampaignMilitaryBaseComponent target;
			foreach (SCR_MilitaryBaseComponent rawBase : bases)
			{
				if (rawBase.GetOwner() && rawBase.GetOwner().GetName() == names[side])
					target = SCR_CampaignMilitaryBaseComponent.Cast(rawBase);
			}
			SCR_AIGroup group = slot.GetGroup();
			if (!group || !target || slot.IsRecruitingInfantry())
			{
				Print("[AICF][NAVIGATION_PROBE_FAILED] reason=SETUP_NOT_READY");
				GetGame().RequestClose();
				return;
			}
			array<AIAgent> agents = {};
			group.GetAgents(agents);
			foreach (AIAgent agent : agents)
			{
				ChimeraCharacter character = ChimeraCharacter.Cast(agent.GetControlledEntity());
				if (!character) continue;
				vector transform[4];
				character.GetWorldTransform(transform);
				transform[3] = m_aAICFNavigationAnchors[side];
				character.Teleport(transform);
			}
			bool assigned = m_OrderPlanner.AssignPlayerOrder(slot, faction, target);
			AIPathfindingComponent pathfinding = AIPathfindingComponent.Cast(group.FindComponent(AIPathfindingComponent));
			if (pathfinding && pathfinding.GetNavmeshComponent())
				pathfinding.GetNavmeshComponent().LoadTileIn(m_aAICFNavigationAnchors[side]);
			m_aAICFNavigationGroups.Insert(group);
			m_aAICFNavigationIntents.Insert(slot.GetStrategicIntentRevision());
			m_aAICFNavigationGenerations.Insert(slot.GetSpawnGeneration());
			Print(string.Format("[AICF][NAVIGATION_PROBE_SETUP] side=%1 assigned=%2 target=%3 origin=%4", side, assigned, names[side], m_aAICFNavigationAnchors[side]));
		}
		GetGame().GetCallqueue().CallLater(AICF_IssueNavigationRecovery, 5000, false);
	}

	protected void AICF_IssueNavigationRecovery()
	{
		for (int side; side < 2; side++)
		{
			AICF_GroupSlot slot = m_USState.GetSlot(0);
			SCR_CampaignFaction faction = m_USFaction;
			if (side == 1)
			{
				slot = m_USSRState.GetSlot(0);
				faction = m_USSRFaction;
			}
			if (side == 0)
			{
				AICF_NavigationDeniedWatchdog denied = new AICF_NavigationDeniedWatchdog();
				IEntity leader = AICF_GroupRuntime.ResolveAliveLeader(slot.GetGroup());
				vector before = leader.GetOrigin();
				bool moved = AICF_IsolatedNavmeshRecovery.TryRecover(slot, denied, m_Stage3Config);
				m_bAICFNavigationFencePassed = !moved && denied.m_iPlayerChecks > 0 &&
					leader.GetOrigin() == before && !slot.HasUsedIsolatedNavmeshRecovery();
				Print(string.Format("[AICF][NAVIGATION_PROBE_FENCE] passed=%1 checks=%2", m_bAICFNavigationFencePassed, denied.m_iPlayerChecks));
			}
			bool issued = m_OrderPlanner.RebuildCurrentOrder(slot, faction, "STUCK_ROUTE_REBUILD", true);
			m_aAICFNavigationWaypoints.Insert(slot.GetWaypoint());
			Print(string.Format("[AICF][NAVIGATION_PROBE_ISSUED] side=%1 issued=%2 intermediate=%3", side, issued, slot.IsStuckRouteWaypoint()));
		}
		GetGame().GetCallqueue().CallLater(AICF_PollNavigationProbe, 5000, true);
	}

	protected void AICF_PollNavigationProbe()
	{
		m_iAICFNavigationProbePolls++;
		int passed;
		for (int side; side < 2; side++)
		{
			AICF_GroupSlot slot = m_USState.GetSlot(0);
			if (side == 1) slot = m_USSRState.GetSlot(0);
			IEntity leader = AICF_GroupRuntime.ResolveAliveLeader(slot.GetGroup());
			float displacement;
			if (leader) displacement = vector.DistanceXZ(leader.GetOrigin(), m_aAICFNavigationAnchors[side]);
			if (side == 0 && leader && slot.HasUsedIsolatedNavmeshRecovery() && !m_bAICFNavigationBudgetPassed)
			{
				vector before = leader.GetOrigin();
				bool repeated = AICF_IsolatedNavmeshRecovery.TryRecover(slot, m_HiddenRecoveryWatchdog, m_Stage3Config);
				m_bAICFNavigationBudgetPassed = !repeated && leader.GetOrigin() == before;
				Print(string.Format("[AICF][NAVIGATION_PROBE_BUDGET] passed=%1", m_bAICFNavigationBudgetPassed));
			}
			m_aAICFNavigationDisplacements[side] = Math.Max(m_aAICFNavigationDisplacements[side], displacement);
			bool identity = slot.GetGroup() == m_aAICFNavigationGroups[side] &&
				slot.GetSpawnGeneration() == m_aAICFNavigationGenerations[side] &&
				slot.GetStrategicIntentRevision() == m_aAICFNavigationIntents[side];
			bool continued = identity && !slot.IsStuckRouteWaypoint() &&
				slot.GetWaypoint() != m_aAICFNavigationWaypoints[side] && displacement >= 80;
			if (continued) passed++;
			Print(string.Format("[AICF][NAVIGATION_PROBE_PROGRESS] side=%1 displacement=%2 identity=%3 continued=%4 intermediate=%5", side, displacement, identity, continued, slot.IsStuckRouteWaypoint()));
		}
		if (passed == 2 || m_iAICFNavigationProbePolls >= 36)
		{
			Print(string.Format("[AICF][NAVIGATION_PROBE_FINISHED] passed=%1 total=2 us_max=%2 ussr_max=%3 player_fence=%4 single_use=%5", passed, m_aAICFNavigationDisplacements[0], m_aAICFNavigationDisplacements[1], m_bAICFNavigationFencePassed, m_bAICFNavigationBudgetPassed));
			GetGame().GetCallqueue().Remove(AICF_PollNavigationProbe);
			GetGame().RequestClose();
		}
	}

	override protected void Stop(bool cleanupEntities)
	{
		if (GetGame())
		{
			GetGame().GetCallqueue().Remove(AICF_BeginNavigationProbe);
			GetGame().GetCallqueue().Remove(AICF_IssueNavigationRecovery);
			GetGame().GetCallqueue().Remove(AICF_PollNavigationProbe);
		}
		super.Stop(cleanupEntities);
	}
}
