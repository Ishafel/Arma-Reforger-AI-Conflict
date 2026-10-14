// Isolated terminal fixture. Synthetic strategic inputs; actual async rosters,
// commander/planner application, authority, slot fences and waypoint entities.
class AICF_AdaptiveProbeScenario
{
	static int s_Mode;
}

modded class AICF_AdaptiveCommanderPolicy
{
	override protected void BuildCandidates(AICF_GroupSlot slot, int now, array<ref AICF_AdaptiveCandidate> candidates)
	{
		super.BuildCandidates(slot, now, candidates);
		string enabled;
		if (!System.GetCLIParam("aicfAdaptiveProbe", enabled) || enabled != "1")
			return;
		IEntity leader = AICF_GroupRuntime.ResolveAliveLeader(slot.GetGroup());
		foreach (AICF_AdaptiveCandidate candidate : candidates)
		{
			if (!candidate.m_bAttack)
				continue;
			// Pairwise scenarios equalise physical approach and graph value. The
			// live adapter still supplies actual allocation, readiness and cooldown.
			candidate.m_vPosition = leader.GetOrigin() + "50 0 0";
			candidate.m_iOpenDirections = 0;
			candidate.m_iConnectivity = 0;
			candidate.m_bRecentLoss = false;
			candidate.m_fDemand = AICF_AdaptiveCommanderConfig.UNKNOWN_RESISTANCE;
			if (AICF_AdaptiveProbeScenario.s_Mode == 1 && candidate.m_iNodeId == 1)
			{
				candidate.m_fDemand = 20;
				candidate.m_iOpenDirections = 3;
				candidate.m_bRecentLoss = true;
			}
		}
	}
	float AICF_ProbeAllocated(AICF_GroupSlot slot, SCR_CampaignMilitaryBaseComponent target)
	{
		return AllocatedForce(slot, target);
	}
	bool AICF_ProbeRecentLoss(SCR_CampaignMilitaryBaseComponent base)
	{
		int now = System.GetTickCount();
		ObserveOwners(now);
		int node = m_Graph.FindNodeId(base);
		return node >= 0 && m_aLossAt[node] > 0 && now - m_aLossAt[node] <= AICF_AdaptiveCommanderConfig.LOSS_TTL_MS;
	}
	int AICF_ProbeConnectivity(int node) { return CountDependentFriendlyNodes(node); }
	void AICF_ProbeExpireLoss()
	{
		for (int i; i < m_aLossAt.Count(); i++)
			m_aLossAt[i] = System.GetTickCount() - AICF_AdaptiveCommanderConfig.LOSS_TTL_MS - 1;
	}
}

modded class AICF_GroupSlot
{
	void AICF_ProbeAgeGates(int assignmentAge, int candidateAge)
	{
		m_iStrategicAssignmentAtMs = Math.Max(1, System.GetTickCount() - assignmentAge);
		m_iStrategicCandidateFirstSeenAtMs = Math.Max(1, System.GetTickCount() - candidateAge);
	}
	void AICF_ProbeExpireFailure()
	{
		for (int i; i < m_aRejectedRouteUntil.Count(); i++)
			m_aRejectedRouteUntil[i] = System.GetTickCount() - 1;
	}
	void AICF_ProbeGeneration(int delta) { m_iSpawnGeneration += delta; }
	void AICF_ProbeReady(bool ready)
	{
		m_State = AICF_EGroupSlotState.SPAWNING;
		if (ready) m_State = AICF_EGroupSlotState.READY;
	}
}

modded class AICF_MatchController
{
	protected int m_iAdaptivePhase;
	protected int m_iAdaptiveStarted;
	protected int m_iAdaptivePhaseAt;
	protected int m_iAdaptiveChecks;
	protected int m_iAdaptiveFailures;
	protected bool m_bAdaptiveGatesChecked;
	protected ref AICF_FactionState m_AdaptiveUS;
	protected ref AICF_FactionState m_AdaptiveUSSR;
	protected ref AICF_ObjectiveGraph m_AdaptiveGraph;
	protected ref AICF_ObjectiveGraph m_AdaptiveGraphUSSR;
	protected ref AICF_AICommander m_AdaptiveCommander;
	protected ref AICF_AICommander m_AdaptiveCommanderUSSR;
	protected ref AICF_AdaptiveCommanderPolicy m_AdaptivePolicy;
	protected ref array<SCR_AIGroup> m_aAdaptiveGroups = {};
	protected ref array<AICF_GroupSlot> m_aAdaptiveSlots = {};
	protected ref array<SCR_CampaignMilitaryBaseComponent> m_aAdaptiveTargets = {};

	protected void AICF_AdaptiveCheck(string name, bool passed)
	{
		m_iAdaptiveChecks++;
		if (!passed) m_iAdaptiveFailures++;
		Print(string.Format("[AICF][ADAPTIVE_PROBE] case=%1 passed=%2 phase=%3", name, passed, m_iAdaptivePhase));
	}
	protected void AICF_AdaptiveNext()
	{
		m_iAdaptivePhase++;
		m_iAdaptivePhaseAt = System.GetTickCount();
	}
	protected void AICF_AdaptiveReset()
	{
		for (int i; i < 2; i++)
			m_OrderPlanner.ReleasePlayerCommand(m_AdaptiveUS.GetSlot(i));
	}
	protected bool AICF_AdaptiveBound(AICF_GroupSlot slot)
	{
		array<AIWaypoint> waypoints = {};
		slot.GetGroup().GetWaypoints(waypoints);
		return slot.GetWaypoint() && waypoints.Contains(slot.GetWaypoint()) && slot.GetWaypoint().GetID() != EntityID.INVALID;
	}
	protected AICF_AdaptiveCandidate AICF_MakeAdaptiveCandidate(int id)
	{
		AICF_AdaptiveCandidate candidate = new AICF_AdaptiveCandidate();
		candidate.m_iNodeId = id;
		candidate.m_bAttack = true;
		candidate.m_bObjective = true;
		candidate.m_vPosition = "100 0 0";
		return candidate;
	}
	protected int AICF_AdaptiveChoose(AICF_AdaptiveSquad squad, AICF_AdaptiveCandidate first, AICF_AdaptiveCandidate second, int now = 100000)
	{
		array<ref AICF_AdaptiveCandidate> candidates = {second, first};
		AICF_AdaptiveScore score;
		string reason;
		AICF_AdaptiveCandidate chosen = AICF_AdaptiveScore.Choose(squad, candidates, now, score, reason);
		if (!chosen) return -1;
		return chosen.m_iNodeId;
	}
	protected void AICF_AdaptiveTable()
	{
		AICF_AdaptiveSquad squad = new AICF_AdaptiveSquad();
		squad.m_bReady = true;
		squad.m_iAlive = 10;
		squad.m_iDesired = 10;
		squad.m_Role = AICF_EGroupRole.ATTACK;
		AICF_AdaptiveCandidate first = AICF_MakeAdaptiveCandidate(1);
		AICF_AdaptiveCandidate second = AICF_MakeAdaptiveCandidate(2);
		AICF_AdaptiveCheck("stable_tie", AICF_AdaptiveChoose(squad, first, second) == 1);
		AICF_AdaptiveCheck("repeat_snapshot", AICF_AdaptiveChoose(squad, first, second) == 1 && AICF_AdaptiveChoose(squad, first, second) == 1);
		first.m_bObjective = false;
		AICF_AdaptiveCheck("victory_value", AICF_AdaptiveChoose(squad, first, second) == 2);
		first.m_bObjective = true;
		second.m_iOpenDirections = 2;
		AICF_AdaptiveCheck("open_direction", AICF_AdaptiveChoose(squad, first, second) == 2);
		second.m_iOpenDirections = 0;
		second.m_iConnectivity = 1;
		AICF_AdaptiveCheck("connectivity_value", AICF_AdaptiveChoose(squad, first, second) == 2);
		second.m_iConnectivity = 0;
		second.m_vPosition = "1100 0 0";
		AICF_AdaptiveCheck("approach_near", AICF_AdaptiveChoose(squad, first, second) == 1);
		squad.m_vPosition = "1200 0 0";
		AICF_AdaptiveCheck("approach_position_changed", AICF_AdaptiveChoose(squad, first, second) == 2);
		second.m_bAvailable = false;
		AICF_AdaptiveCheck("unavailable_near", AICF_AdaptiveChoose(squad, first, second) == 1);
		second.m_bAvailable = true;
		second.m_bDeferred = true;
		AICF_AdaptiveCheck("deferred_near", AICF_AdaptiveChoose(squad, first, second) == 1);
		second.m_bDeferred = false;
		squad.m_vPosition = vector.Zero;
		second.m_vPosition = first.m_vPosition;
		first.m_bKnownRoute = true;
		first.m_fRouteMeters = 2000;
		AICF_AdaptiveCheck("known_route_cost", AICF_AdaptiveChoose(squad, first, second) == 2);
		first.m_bKnownRoute = false;
		AICF_AdaptiveScore unknown = AICF_AdaptiveScore.Evaluate(squad, first, 100000);
		AICF_AdaptiveCheck("unknown_resistance_nonzero", unknown.m_fAllocation == AICF_AdaptiveCommanderConfig.DEFICIT_VALUE);
		first.m_fAllocated = 10;
		AICF_AdaptiveCheck("spread_same_cycle", AICF_AdaptiveChoose(squad, first, second) == 2);
		first.m_fDemand = 20;
		first.m_iOpenDirections = 3;
		first.m_bRecentLoss = true;
		AICF_AdaptiveCheck("concentrate_deficit", AICF_AdaptiveChoose(squad, first, second) == 1);
		first.m_fDemand = 8;
		first.m_fAllocated = 0;
		first.m_iOpenDirections = 0;
		first.m_bRecentLoss = false;
		second.m_bAttack = false;
		second.m_bHQ = true;
		second.m_bThreat = true;
		second.m_iThreatAtMs = 100000;
		AICF_AdaptiveCheck("hq_threat", AICF_AdaptiveChoose(squad, first, second) == 2);
		second.m_bHQ = false;
		AICF_AdaptiveCheck("friendly_base_threat", AICF_AdaptiveChoose(squad, first, second) == 2);
		AICF_AdaptiveCheck("stale_threat_fallback", AICF_AdaptiveScore.Evaluate(squad, second, 115001).m_iUrgency == 0);
		second.m_bThreat = false;
		AICF_AdaptiveCheck("unknown_threat_fallback", AICF_AdaptiveScore.Evaluate(squad, second, 100000).m_iUrgency == 0);
		AICF_AdaptiveCheck("full_force", AICF_AdaptiveScore.EffectiveForce(true, 10, 10) == 10);
		AICF_AdaptiveCheck("weakened_force", AICF_AdaptiveScore.EffectiveForce(true, 3, 10) == 1.5);
		AICF_AdaptiveCheck("critical_force", AICF_AdaptiveScore.EffectiveForce(true, 2, 10) == 0);
		AICF_AdaptiveCheck("not_ready_force", AICF_AdaptiveScore.EffectiveForce(false, 10, 10) == 0);
		squad.m_iAlive = 2;
		AICF_AdaptiveCheck("critical_no_attack", !AICF_AdaptiveScore.Evaluate(squad, first, 100000).m_bEligible);
		squad.m_iAlive = 10;
		squad.m_bReady = false;
		AICF_AdaptiveCheck("not_ready_no_assignment", AICF_AdaptiveChoose(squad, first, second) == -1);
		squad.m_bReady = true;
		second.m_bAttack = true;
		squad.m_iCurrentNode = 1;
		second.m_vPosition = "0 0 0";
		AICF_AdaptiveCheck("small_oscillation", AICF_AdaptiveChoose(squad, first, second) == 1);
		squad.m_fProgressMeters = 30;
		AICF_AdaptiveCheck("progress_bonus", AICF_AdaptiveScore.Evaluate(squad, first, 100000).m_fContinuation == 30);
		first.m_bAvailable = false;
		second.m_bAvailable = false;
		AICF_AdaptiveCheck("no_candidates", AICF_AdaptiveChoose(squad, first, second) == -1);
	}

	protected void AICF_AdaptiveAgent(AIAgent agent)
	{
		foreach (AICF_GroupSlot slot : m_aAdaptiveSlots)
		{
			if (agent && agent.GetParentGroup() == slot.GetGroup()) slot.ObserveRosterAgent(agent, true);
		}
	}
	protected void AICF_AdaptiveRosterComplete(SCR_AIGroup group)
	{
		foreach (AICF_GroupSlot slot : m_aAdaptiveSlots)
		{
			if (slot.GetGroup() == group) slot.MarkRosterCompletionCallbackObserved();
		}
	}
	protected void AICF_AdaptiveSpawn(AICF_FactionState state, SCR_CampaignFaction faction)
	{
		for (int i; i < 2; i++)
		{
			AICF_GroupSlot slot = state.GetSlot(i);
			slot.SetDesiredSize(10);
			slot.BeginInitialSpawn();
			SCR_AIGroup group = m_GroupSpawner.SpawnGroup(faction, faction.GetMainBase(), i, 10, false);
			if (!group || !slot.BindSpawnedGroup(group))
			{
				AICF_AdaptiveCheck("spawn_bind", false);
				return;
			}
			m_aAdaptiveGroups.Insert(group);
			m_aAdaptiveSlots.Insert(slot);
			group.GetOnAgentAdded().Insert(AICF_AdaptiveAgent);
			group.GetOnAllDelayedEntitySpawned().Insert(AICF_AdaptiveRosterComplete);
			slot.MarkRosterSpawnRequested(10);
			AICF_AdaptiveCheck("async_roster_request_" + faction.GetFactionKey() + i, m_GroupSpawner.BeginRosterSpawn(group, 10));
		}
	}
	protected bool AICF_AdaptiveReady()
	{
		foreach (AICF_GroupSlot slot : m_aAdaptiveSlots)
		{
			SCR_AIGroup group = slot.GetGroup();
			int count, mismatch, dead;
			if (!group || !slot.WasRosterCompletionCallbackObserved() || slot.GetRosterCallbackAgentCount() != 10 ||
				!AICF_GroupRuntime.HasExactFactionRoster(group, group.GetFaction().GetFactionKey(), 10, count, mismatch, dead))
				return false;
			array<AIAgent> agents = {};
			group.GetAgents(agents);
			foreach (AIAgent agent : agents)
			{
				if (!slot.WasRosterAgentObservedFromCallback(agent) || slot.GetRosterAgentObservedAgeMs(agent) < 1000)
					return false;
			}
		}
		foreach (AICF_GroupSlot readySlot : m_aAdaptiveSlots)
		{
			if (!readySlot.IsCombatReady()) readySlot.MarkReady();
			readySlot.CompleteInfantryMusterIfReady();
		}
		return m_aAdaptiveSlots.Count() == 4;
	}

	override protected void Update()
	{
		super.Update();
		string flag;
		if (!System.GetCLIParam("aicfAdaptiveProbe", flag) || flag != "1" || !m_bRosterReady || m_bStopped)
			return;
		int now = System.GetTickCount();
		if (!m_iAdaptiveStarted)
		{
			m_iAdaptiveStarted = now;
			m_iAdaptivePhaseAt = now;
			AICF_AdaptiveTable();
			m_AdaptiveUS = new AICF_FactionState(m_USFaction.GetFactionKey(), m_Config);
			m_AdaptiveUSSR = new AICF_FactionState(m_USSRFaction.GetFactionKey(), m_Config);
			AICF_AdaptiveSpawn(m_AdaptiveUS, m_USFaction);
			AICF_AdaptiveSpawn(m_AdaptiveUSSR, m_USSRFaction);
			for (int node; node < m_ObjectiveGraph.GetNodeCount(); node++)
			{
				AICF_ObjectiveNode objective = m_ObjectiveGraph.GetNode(node);
				if (objective.IsObjective() && !objective.GetBase().IsHQ() && objective.GetBase().GetFaction() != m_USFaction && objective.GetBase().GetFaction() != m_USSRFaction)
					m_aAdaptiveTargets.Insert(objective.GetBase());
				if (m_aAdaptiveTargets.Count() == 2) break;
			}
			AICF_AdaptiveProbeWorld.s_Targets.Copy(m_aAdaptiveTargets);
			AICF_AdaptiveProbeWorld.s_Faction = m_USFaction;
			m_AdaptiveGraph = new AICF_ObjectiveGraph();
			m_AdaptiveGraph.AICF_ProbeGraph(m_USFaction, m_aAdaptiveTargets);
			m_AdaptiveGraphUSSR = new AICF_ObjectiveGraph();
			m_AdaptiveGraphUSSR.AICF_ProbeGraph(m_USSRFaction, m_aAdaptiveTargets);
			m_AdaptiveCommander = new AICF_AICommander(m_AdaptiveUS, m_USFaction, m_CommandAuthorityPolicy, m_OrderPlanner, m_AdaptiveGraph, m_TargetSelector);
			m_AdaptiveCommanderUSSR = new AICF_AICommander(m_AdaptiveUSSR, m_USSRFaction, m_CommandAuthorityPolicy, m_OrderPlanner, m_AdaptiveGraphUSSR, m_TargetSelector);
			m_AdaptivePolicy = new AICF_AdaptiveCommanderPolicy(m_AdaptiveUS, m_USFaction, m_AdaptiveGraph, m_TargetSelector);
			AICF_AdaptiveNext();
		}
		if (now - m_iAdaptiveStarted > 180000)
		{
			AICF_AdaptiveCheck("timeout", false);
			AICF_AdaptiveFinish();
			return;
		}
		if (m_iAdaptivePhase == 1)
		{
			if (!AICF_AdaptiveReady()) return;
			AICF_AdaptiveCheck("exact_async_rosters", true);
			SCR_CampaignFaction originalOwner = SCR_CampaignFaction.Cast(m_aAdaptiveTargets[0].GetFaction());
			m_aAdaptiveTargets[0].SetFaction(m_USFaction);
			m_aAdaptiveTargets[1].SetFaction(m_USFaction);
			m_AdaptiveGraph.AICF_ProbeDependency(false);
			AICF_AdaptiveCheck("live_graph_dependency", m_AdaptivePolicy.AICF_ProbeConnectivity(1) == 1);
			m_AdaptiveGraph.AICF_ProbeDependency(true);
			AICF_AdaptiveCheck("live_graph_bypass", m_AdaptivePolicy.AICF_ProbeConnectivity(1) == 0);
			m_aAdaptiveTargets[0].SetFaction(originalOwner);
			m_aAdaptiveTargets[1].SetFaction(originalOwner);
			m_AdaptiveGraph.AICF_ProbeGraph(m_USFaction, m_aAdaptiveTargets);
			AICF_AdaptiveNext();
			return;
		}
		AICF_GroupSlot first = m_AdaptiveUS.GetSlot(0);
		AICF_GroupSlot second = m_AdaptiveUS.GetSlot(1);
		SCR_CampaignMilitaryBaseComponent target1 = m_aAdaptiveTargets[0];
		SCR_CampaignMilitaryBaseComponent target2 = m_aAdaptiveTargets[1];
		if (m_iAdaptivePhase == 2 || m_iAdaptivePhase == 3)
		{
			bool appliedFirst = m_AdaptiveCommander.AssignOrder(first, "ADAPTIVE_PROBE");
			bool appliedSecond = m_AdaptiveCommander.AssignOrder(second, "ADAPTIVE_PROBE");
			if (!appliedFirst || !appliedSecond || !AICF_AdaptiveBound(first) || !AICF_AdaptiveBound(second)) return;
			if (m_iAdaptivePhase == 2)
			{
				AICF_AdaptiveCheck("live_distribution_waypoints", first.GetTargetBase() != second.GetTargetBase());
				AICF_AdaptiveCheck("live_same_cycle_allocation", m_AdaptivePolicy.AICF_ProbeAllocated(second, first.GetTargetBase()) == 10);
				AICF_AdaptiveReset();
				AICF_AdaptiveProbeScenario.s_Mode = 1;
			}
			else
			{
				AICF_AdaptiveCheck("live_joint_attack_waypoints", first.GetTargetBase() == target1 && second.GetTargetBase() == target1);
				AICF_AdaptiveProbeScenario.s_Mode = 0;
			}
			AICF_AdaptiveNext();
			return;
		}
		if (m_iAdaptivePhase == 4)
		{
			if (!m_bAdaptiveGatesChecked)
			{
				AIWaypoint old = second.GetWaypoint();
				AICF_AdaptiveCheck("live_dwell_held", !m_AdaptiveCommander.ReconcileStrategicOrder(second, "ADAPTIVE_PROBE", 30000, 1000) && second.GetWaypoint() == old);
				second.AICF_ProbeAgeGates(60000, 0);
				AICF_AdaptiveCheck("live_stable_candidate_held", !m_AdaptiveCommander.ReconcileStrategicOrder(second, "ADAPTIVE_PROBE", 30000, 1000) && second.GetWaypoint() == old);
				second.AICF_ProbeAgeGates(60000, 2000);
				m_bAdaptiveGatesChecked = true;
			}
			if (!m_AdaptiveCommander.ReconcileStrategicOrder(second, "ADAPTIVE_PROBE", 30000, 1000)) return;
			AICF_AdaptiveCheck("live_gates_pass_change", second.GetTargetBase() == target2 && AICF_AdaptiveBound(second));
			AICF_AdaptiveNext();
			return;
		}
		if (m_iAdaptivePhase == 5)
		{
			AICF_AdaptiveProbeWorld.s_Threat = m_USFaction.GetMainBase();
			bool firstChanged = m_AdaptiveCommander.ReconcileStrategicOrder(first, "ADAPTIVE_PROBE", 999999, 999999);
			bool secondChanged = m_AdaptiveCommander.ReconcileStrategicOrder(second, "ADAPTIVE_PROBE", 999999, 999999);
			AICF_AdaptiveCheck("live_urgent_hq_bypasses_gates", firstChanged && secondChanged && first.GetTargetBase() == m_USFaction.GetMainBase() && second.GetTargetBase() == first.GetTargetBase());
			AICF_AdaptiveCheck("live_urgent_defend_waypoints", SCR_DefendWaypoint.Cast(first.GetWaypoint()) && SCR_DefendWaypoint.Cast(second.GetWaypoint()));
			AICF_AdaptiveProbeWorld.s_Threat = null;
			AICF_AdaptiveNext();
			return;
		}
		if (m_iAdaptivePhase == 6)
		{
			if (!m_OrderPlanner.AssignPlayerOrder(first, m_USFaction, target1)) return;
			AIWaypoint manual = first.GetWaypoint();
			int revision = first.GetStrategicAssignmentRevision();
			m_AdaptiveCommander.ReconcileStrategicOrder(first, "ADAPTIVE_PROBE", 0, 0);
			AICF_AdaptiveCheck("live_manual_preserved", first.GetWaypoint() == manual && first.GetStrategicAssignmentRevision() == revision && first.HasPlayerStrategicIntent());
			AICF_AdaptiveCheck("live_manual_allocated", m_AdaptivePolicy.AICF_ProbeAllocated(second, target1) == 10);
			m_OrderPlanner.ReleasePlayerCommand(second);
			AICF_AdaptiveNext();
			return;
		}
		if (m_iAdaptivePhase == 7)
		{
			if (!m_AdaptiveCommander.AssignOrder(second, "ADAPTIVE_PROBE")) return;
			AICF_AdaptiveCheck("live_manual_spread", second.GetTargetBase() == target2);
			AICF_AdaptiveDecision fresh = m_AdaptivePolicy.Select(second);
			AIWaypoint oldWaypoint = second.GetWaypoint();
			second.AICF_ProbeGeneration(1);
			AICF_AdaptiveCheck("stale_generation_rejected", !m_OrderPlanner.ReconcileAICommanderOrder(second, m_USFaction, m_AdaptiveGraph, m_TargetSelector, "STALE", 0, 0, false, fresh) && second.GetWaypoint() == oldWaypoint);
			second.AICF_ProbeGeneration(-1);
			m_AdaptiveGraph.AICF_ProbeRevision();
			AICF_AdaptiveCheck("stale_graph_rejected", !m_OrderPlanner.ReconcileAICommanderOrder(second, m_USFaction, m_AdaptiveGraph, m_TargetSelector, "STALE", 0, 0, false, fresh) && second.GetWaypoint() == oldWaypoint);
			fresh = m_AdaptivePolicy.Select(second);
			second.TouchCommanderConfiguration();
			AICF_AdaptiveCheck("stale_assignment_rejected", !fresh.IsCurrent(second, m_USFaction, m_AdaptiveGraph));
			second.AICF_ProbeReady(false);
			AICF_AdaptiveCheck("live_not_ready_rejected", !m_AdaptivePolicy.Select(second) && !m_AdaptiveCommander.AssignOrder(second, "NOT_READY"));
			second.AICF_ProbeReady(true);
			second.DeferFailedRouteTarget(target2);
			AICF_AdaptiveDecision deferred = m_AdaptivePolicy.Select(second);
			AICF_AdaptiveCheck("live_failed_route_excluded", deferred && deferred.m_Target != target2);
			second.AICF_ProbeExpireFailure();
			AICF_AdaptiveDecision expired = m_AdaptivePolicy.Select(second);
			AICF_AdaptiveCheck("live_failure_expired", expired && expired.m_Target == target2);
			second.DeferFailedRouteTarget(target1);
			second.DeferFailedRouteTarget(target2);
			m_AdaptiveCommander.ReconcileStrategicOrder(second, "NO_CANDIDATE", 999999, 999999);
			AICF_AdaptiveCheck("live_no_attack_fallback", second.GetTargetBase() == m_USFaction.GetMainBase() && SCR_DefendWaypoint.Cast(second.GetWaypoint()));
			second.AICF_ProbeExpireFailure();
			m_OrderPlanner.ReleasePlayerCommand(second);
			AICF_AdaptiveNext();
			return;
		}
		if (m_iAdaptivePhase == 8)
		{
			if (!second.GetWaypoint() && !m_AdaptiveCommander.AssignOrder(second, "PRE_OWNERSHIP")) return;
			AICF_AdaptiveDecision before = m_AdaptivePolicy.Select(second);
			vector controllerOrigin = second.GetGroup().GetOrigin();
			second.GetGroup().SetOrigin(controllerOrigin + "10000 0 10000");
			AICF_AdaptiveDecision after = m_AdaptivePolicy.Select(second);
			AICF_AdaptiveCheck("live_controller_origin_ignored", before && after && before.m_Target == after.m_Target);
			second.GetGroup().SetOrigin(controllerOrigin);
			target2.SetFaction(m_USFaction);
			AICF_AdaptiveProbeWorld.s_Threat = target2;
			m_AdaptiveGraph.AICF_ProbeRevision();
			if (!m_AdaptiveCommander.ReconcileStrategicOrder(second, "OWNERSHIP", 999999, 999999)) return;
			AICF_AdaptiveCheck("live_invalid_target_bypasses_gates", second.GetOperationalPosture() == "AREA_SECURITY" && AICF_AdaptiveBound(second));
			AICF_AdaptiveCheck("live_friendly_base_threat", second.GetTargetBase() == target2 && SCR_DefendWaypoint.Cast(second.GetWaypoint()));
			m_AdaptivePolicy.Select(second);
			AICF_AdaptiveProbeWorld.s_Threat = null;
			SCR_CampaignFaction fia = SCR_CampaignFaction.Cast(GetGame().GetFactionManager().GetFactionByKey("FIA"));
			target2.SetFaction(fia);
			m_AdaptiveGraph.AICF_ProbeRevision();
			AICF_AdaptiveCheck("loss_signal_survives_rebuild", m_AdaptivePolicy.AICF_ProbeRecentLoss(target2));
			m_AdaptivePolicy.AICF_ProbeExpireLoss();
			AICF_AdaptiveCheck("loss_signal_expires", !m_AdaptivePolicy.AICF_ProbeRecentLoss(target2));
			m_OrderPlanner.ReleasePlayerCommand(second);
			AICF_AdaptiveNext();
			return;
		}
		if (m_iAdaptivePhase == 9)
		{
			if (!m_AdaptiveCommander.AssignOrder(second, "VEHICLE_SUSPENDED", null, true)) return;
			AICF_AdaptiveCheck("live_vehicle_suspension", !second.GetWaypoint() && second.HasStrategicIntent());
			int usRevision = second.GetStrategicAssignmentRevision();
			AICF_AdaptiveProbeWorld.s_Faction = m_USSRFaction;
			bool ussrAssigned = m_AdaptiveCommanderUSSR.AssignOrder(m_AdaptiveUSSR.GetSlot(0), "INDEPENDENCE");
			AICF_AdaptiveProbeWorld.s_Faction = m_USFaction;
			if (!ussrAssigned) return;
			AICF_AdaptiveCheck("live_faction_independence", second.GetStrategicAssignmentRevision() == usRevision && AICF_AdaptiveBound(m_AdaptiveUSSR.GetSlot(0)));
			AICF_AdaptiveCheck("foreign_slot_rejected", !m_AdaptiveCommander.AssignOrder(m_AdaptiveUSSR.GetSlot(0), "FOREIGN"));
			AICF_AdaptiveFinish();
		}
	}
	protected void AICF_AdaptiveFinish()
	{
		Print(string.Format("[AICF][ADAPTIVE_PROBE_FINISHED] checks=%1 failures=%2 phase=%3", m_iAdaptiveChecks, m_iAdaptiveFailures, m_iAdaptivePhase));
		Stop(true);
		GetGame().RequestClose();
	}
	override void Stop(bool cleanupEntities)
	{
		for (int i; i < m_aAdaptiveGroups.Count(); i++)
		{
			SCR_AIGroup group = m_aAdaptiveGroups[i];
			if (group)
			{
				group.GetOnAgentAdded().Remove(AICF_AdaptiveAgent);
				group.GetOnAllDelayedEntitySpawned().Remove(AICF_AdaptiveRosterComplete);
			}
			m_OrderPlanner.ClearOrder(m_aAdaptiveSlots[i]);
			m_aAdaptiveSlots[i].Reset();
		}
		m_aAdaptiveGroups.Clear();
		m_aAdaptiveSlots.Clear();
		AICF_AdaptiveProbeWorld.s_Threat = null;
		AICF_AdaptiveProbeWorld.s_Targets.Clear();
		super.Stop(cleanupEntities);
	}
}
