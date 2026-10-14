// Faction-scoped read-only planning. Applied assignments are the allocation ledger:
// each next slot observes earlier successful writes (including player intents).
class AICF_AdaptiveCommanderPolicy
{
	protected AICF_FactionState m_State;
	protected SCR_CampaignFaction m_Faction;
	protected AICF_ObjectiveGraph m_Graph;
	protected AICF_TargetSelector m_Selector;
	protected ref array<Faction> m_aOwners = {};
	protected ref array<int> m_aLossAt = {};
	protected ref array<SCR_CampaignMilitaryBaseComponent> m_aObservedBases = {};
	protected int m_iObservedGraph;
	protected ref map<int, float> m_mStartDistance = new map<int, float>();
	protected ref map<int, string> m_mProgressIdentity = new map<int, string>();
	protected ref map<int, string> m_mLastTrace = new map<int, string>();
	protected ref map<int, int> m_mLastTraceAt = new map<int, int>();

	void AICF_AdaptiveCommanderPolicy(AICF_FactionState state, SCR_CampaignFaction faction,
		AICF_ObjectiveGraph graph, AICF_TargetSelector selector)
	{
		m_State = state;
		m_Faction = faction;
		m_Graph = graph;
		m_Selector = selector;
	}

	protected void ObserveOwners(int now)
	{
		if (m_iObservedGraph != m_Graph.GetRevision() || m_aOwners.Count() != m_Graph.GetNodeCount())
		{
			array<SCR_CampaignMilitaryBaseComponent> oldBases = {};
			array<Faction> oldOwners = {};
			array<int> oldLoss = {};
			oldBases.Copy(m_aObservedBases);
			oldOwners.Copy(m_aOwners);
			oldLoss.Copy(m_aLossAt);
			m_aObservedBases.Clear();
			m_aOwners.Clear();
			m_aLossAt.Clear();
			m_iObservedGraph = m_Graph.GetRevision();
			for (int initial; initial < m_Graph.GetNodeCount(); initial++)
			{
				SCR_CampaignMilitaryBaseComponent base = m_Graph.GetNode(initial).GetBase();
				m_aObservedBases.Insert(base);
				int previous = oldBases.Find(base);
				Faction owner = base.GetFaction();
				int loss;
				if (previous >= 0)
				{
					owner = oldOwners[previous];
					loss = oldLoss[previous];
				}
				m_aOwners.Insert(owner);
				m_aLossAt.Insert(loss);
			}
		}
		for (int i; i < m_aOwners.Count(); i++)
		{
			Faction owner = m_Graph.GetNode(i).GetBase().GetFaction();
			if (m_aOwners[i] == m_Faction && owner != m_Faction)
				m_aLossAt[i] = now;
			m_aOwners[i] = owner;
		}
	}

	protected float AllocatedForce(AICF_GroupSlot requesting, SCR_CampaignMilitaryBaseComponent target)
	{
		float force;
		for (int i; i < m_State.GetSlotCount(); i++)
		{
			AICF_GroupSlot other = m_State.GetSlot(i);
			if (!other || other == requesting || !other.IsCombatReady() || other.IsRecruitingInfantry() ||
				other.IsLoneSurvivorRetreat() || other.IsTemporaryRouteReplanHold() ||
				other.IsPersistentStuckFieldHold() || other.HasPendingOrderRecovery() ||
				other.GetRouteRecoveryEpisode().IsBlocked(other))
				continue;
			bool assigned = other.GetTargetBase() == target;
			if (other.HasStrategicIntent())
			{
				assigned = other.IsStrategicIntentRoleCurrent() && other.GetStrategicIntentTargetBase() == target;
				if (other.GetStrategicIntentTargetKind() == AICF_EOrderTargetKind.POSITION)
					assigned = vector.DistanceXZ(other.GetStrategicIntentTargetPosition(), target.GetOwner().GetOrigin()) <= AICF_AdaptiveCommanderConfig.POINT_COMMIT_RADIUS;
			}
			if (assigned && !other.IsRouteTargetDeferred(target) && !other.HasFailedMovement() && !other.NeedsInfantryMuster())
				force += AICF_AdaptiveScore.EffectiveForce(true, AICF_GroupRuntime.CountAliveAgents(other.GetGroup()), other.GetDesiredSize());
		}
		return force;
	}

	void RecordLoss(SCR_CampaignMilitaryBaseComponent lostBase)
	{
		if (!Replication.IsServer() || !m_Graph || !m_Faction || !lostBase || lostBase.GetFaction() == m_Faction)
			return;
		int now = System.GetTickCount();
		ObserveOwners(now);
		int node = m_Graph.FindNodeId(lostBase);
		if (node >= 0)
			m_aLossAt[node] = now;
	}

	protected void BuildCandidates(AICF_GroupSlot slot, int now, array<ref AICF_AdaptiveCandidate> candidates)
	{
		array<int> attackNodes = {};
		m_Selector.CollectAttackNodes(m_Graph, m_Faction, attackNodes);
		bool usableAttack;
		foreach (int attackId : attackNodes)
		{
			if (!slot.IsRouteTargetDeferred(m_Graph.GetNode(attackId).GetBase()))
				usableAttack = true;
		}
		for (int i; i < m_Graph.GetNodeCount(); i++)
		{
			AICF_ObjectiveNode node = m_Graph.GetNode(i);
			SCR_CampaignMilitaryBaseComponent base = node.GetBase();
			if (!base || !base.GetOwner() || !base.IsInitialized())
				continue;
			bool friendly = base.GetFaction() == m_Faction;
			bool threatened = friendly && (base.GetCaptureState() != SCR_EBaseCaptureState.NONE ||
				base.IsBeingCaptured() || base.AreEnemiesPresent());
			if (!friendly && !attackNodes.Contains(i))
				continue;
			if (friendly && (!base.GetSpawnPoint() || base.GetType() == SCR_ECampaignBaseType.RELAY ||
				(!threatened && (!base.GetSpawnPoint().IsSpawnPointEnabled() || !base.GetSpawnPoint().IsSpawnPointActive()))))
				continue;
			AICF_AdaptiveCandidate candidate = new AICF_AdaptiveCandidate();
			candidate.m_iNodeId = i;
			candidate.m_Base = base;
			candidate.m_vPosition = base.GetOwner().GetOrigin();
			candidate.m_bAttack = !friendly;
			candidate.m_bObjective = node.IsObjective() && !base.IsHQ();
			candidate.m_bHQ = base == m_Faction.GetMainBase();
			candidate.m_bThreat = threatened;
			candidate.m_iThreatAtMs = now;
			if (threatened)
			{
				candidate.m_fDemand = AICF_AdaptiveCommanderConfig.CONTESTED_DEMAND;
				if (candidate.m_bHQ)
					candidate.m_fDemand = AICF_AdaptiveCommanderConfig.HQ_DEMAND;
			}
			candidate.m_bDeferred = slot.IsRouteTargetDeferred(base);
			if (slot.HasFailedMovement() && base == slot.GetTargetBase())
				candidate.m_bDeferred = true;
			candidate.m_fAllocated = AllocatedForce(slot, base);
			candidate.m_bRecentLoss = m_aLossAt[i] > 0 && now - m_aLossAt[i] <= AICF_AdaptiveCommanderConfig.LOSS_TTL_MS;
			if (candidate.m_bRecentLoss)
				candidate.m_fDemand = Math.Max(candidate.m_fDemand, AICF_AdaptiveCommanderConfig.CONTESTED_DEMAND);
			foreach (int next : node.GetOutgoingNodeIds())
			{
				AICF_ObjectiveNode adjacent = m_Graph.GetNode(next);
				if (!adjacent || !adjacent.GetBase())
					continue;
				if (adjacent.GetBase().GetFaction() != m_Faction && !adjacent.GetBase().IsHQ())
					candidate.m_iOpenDirections++;
				if (friendly && m_aLossAt[next] > 0 && now - m_aLossAt[next] <= AICF_AdaptiveCommanderConfig.LOSS_TTL_MS)
				candidate.m_bRecentLoss = true;
			}
			candidate.m_iConnectivity = CountDependentFriendlyNodes(i);
			// Без срочности ATTACK сохраняет наступательное назначение; слабый отряд
			// получает только безопасный HQ hold. DEFEND держит передний край.
			if (friendly && !threatened)
			{
				if (slot.GetRole() != AICF_EGroupRole.DEFEND && !candidate.m_bHQ)
					continue;
				candidate.m_fFrontValue = Math.Min(AICF_AdaptiveCommanderConfig.MAX_FRONT_VALUE, candidate.m_iOpenDirections * AICF_AdaptiveCommanderConfig.FRONT_VALUE_PER_EXIT);
				if (slot.GetRole() == AICF_EGroupRole.ATTACK)
				{
					if (usableAttack && AICF_GroupRuntime.CountAliveAgents(slot.GetGroup()) >= AICF_AdaptiveCommanderConfig.MIN_ATTACK_ALIVE)
						candidate.m_fFrontValue = -AICF_AdaptiveCommanderConfig.ATTACK_HOLD_COST;
				}
			}
			if (slot.GetRole() == AICF_EGroupRole.RESERVE && !candidate.m_bHQ && !threatened)
				continue;
			candidates.Insert(candidate);
		}
	}

	// Friendly nodes reachable from HQ that would lose connectivity without this
	// anchor. Pure graph topology; no map names, physical path claims or hidden AI.
	protected int CountDependentFriendlyNodes(int excluded)
	{
		array<int> withAnchor = {};
		array<int> withoutAnchor = {};
		CollectFriendlyReachable(-1, withAnchor);
		CollectFriendlyReachable(excluded, withoutAnchor);
		int lost;
		foreach (int node : withAnchor)
		{
			if (node != excluded && !withoutAnchor.Contains(node))
				lost++;
		}
		return lost;
	}

	protected void CollectFriendlyReachable(int excluded, array<int> reached)
	{
		int start = m_Graph.FindNodeId(m_Faction.GetMainBase());
		if (start < 0 || start == excluded)
			return;
		reached.Insert(start);
		for (int head; head < reached.Count(); head++)
		{
			foreach (int next : m_Graph.GetNode(reached[head]).GetOutgoingNodeIds())
			{
				AICF_ObjectiveNode node = m_Graph.GetNode(next);
				if (next != excluded && node && node.GetBase().GetFaction() == m_Faction && !reached.Contains(next))
					reached.Insert(next);
			}
		}
	}

	AICF_AdaptiveDecision Select(AICF_GroupSlot slot, SCR_CampaignMilitaryBaseComponent excluded = null)
	{
		if (!Replication.IsServer() || !m_State || !m_Faction || !m_Graph || !m_Selector || !slot)
			return null;
		if (m_State.GetSlot(slot.GetSlotId()) != slot || !slot.IsCombatReady() || !slot.GetGroup() || slot.GetGroup().GetFaction() != m_Faction)
			return null;
		if (slot.HasPlayerStrategicIntent() || slot.HasPlayerStrategicOrder() || slot.IsRecruitingInfantry() || slot.IsLoneSurvivorRetreat())
			return null;
		if (slot.IsTemporaryRouteReplanHold() || slot.IsPersistentStuckFieldHold() ||
			slot.HasPendingOrderRecovery() || slot.GetRouteRecoveryEpisode().IsBlocked(slot))
			return null;
		IEntity leader = AICF_GroupRuntime.ResolveAliveLeader(slot.GetGroup());
		if (!leader)
			return null;
		int now = System.GetTickCount();
		ObserveOwners(now);
		AICF_AdaptiveSquad squad = new AICF_AdaptiveSquad();
		squad.m_bReady = slot.IsCombatReady();
		squad.m_iAlive = AICF_GroupRuntime.CountAliveAgents(slot.GetGroup());
		squad.m_iDesired = slot.GetDesiredSize();
		squad.m_Role = slot.GetRole();
		squad.m_vPosition = leader.GetOrigin();
		squad.m_iCurrentNode = m_Graph.FindNodeId(slot.GetTargetBase());
		string identity = string.Format("%1/%2/%3", slot.GetSpawnGeneration(), slot.GetStrategicAssignmentRevision(), squad.m_iCurrentNode);
		if (slot.GetTargetBase() && slot.GetTargetBase().GetOwner())
		{
			float remaining = vector.DistanceXZ(leader.GetOrigin(), slot.GetTargetBase().GetOwner().GetOrigin());
			string observed;
			if (!m_mProgressIdentity.Find(slot.GetSlotId(), observed) || observed != identity)
			{
				m_mProgressIdentity.Set(slot.GetSlotId(), identity);
				m_mStartDistance.Set(slot.GetSlotId(), remaining);
			}
			float started;
			m_mStartDistance.Find(slot.GetSlotId(), started);
			squad.m_fProgressMeters = Math.Max(0, started - remaining);
		}
		array<ref AICF_AdaptiveCandidate> candidates = {};
		BuildCandidates(slot, now, candidates);
		foreach (AICF_AdaptiveCandidate candidate : candidates)
		{
			if (excluded && candidate.m_Base == excluded)
				candidate.m_bAvailable = false;
		}
		AICF_AdaptiveScore bestScore;
		string reason;
		AICF_AdaptiveCandidate best = AICF_AdaptiveScore.Choose(squad, candidates, now, bestScore, reason);
		if (!best)
			return null;
		AICF_AdaptiveScore currentScore;
		foreach (AICF_AdaptiveCandidate currentCandidate : candidates)
		{
			if (currentCandidate.m_iNodeId == squad.m_iCurrentNode)
				currentScore = AICF_AdaptiveScore.Evaluate(squad, currentCandidate, now);
		}
		AICF_AdaptiveDecision decision = new AICF_AdaptiveDecision();
		decision.m_Slot = slot;
		decision.m_Group = slot.GetGroup();
		decision.m_GroupId = decision.m_Group.GetID();
		decision.m_sFaction = m_Faction.GetFactionKey();
		decision.m_iGeneration = slot.GetSpawnGeneration();
		decision.m_iGraphRevision = m_Graph.GetRevision();
		decision.m_iAssignmentRevision = slot.GetStrategicAssignmentRevision();
		decision.m_Target = best.m_Base;
		decision.m_sReason = reason;
		decision.m_bUrgent = bestScore.m_iUrgency > 0 && (!currentScore || bestScore.m_iUrgency > currentScore.m_iUrgency);
		decision.m_sPosture = "AREA_SECURITY";
		if (best.m_bAttack)
		{
			decision.m_sPosture = "ATTACK_PRIMARY";
			if (best.m_fAllocated > 0)
				decision.m_sPosture = "ATTACK_SUPPORT";
		}
		else if (slot.GetRole() == AICF_EGroupRole.DEFEND)
		{
			decision.m_sPosture = "FORWARD_DEFEND";
			if (bestScore.m_iUrgency > 0)
				decision.m_sPosture = "QRF";
		}
		Trace(slot, best, bestScore, decision, now);
		return decision;
	}

	protected void Trace(AICF_GroupSlot slot, AICF_AdaptiveCandidate candidate, AICF_AdaptiveScore score, AICF_AdaptiveDecision decision, int now)
	{
		string key = string.Format("%1/%2/%3/%4/%5", decision.m_iGeneration, decision.m_iGraphRevision,
			decision.m_iAssignmentRevision, candidate.m_iNodeId, decision.m_sReason);
		string previous;
		int at;
		if (m_mLastTrace.Find(slot.GetSlotId(), previous) && previous == key &&
			m_mLastTraceAt.Find(slot.GetSlotId(), at) && now - at < AICF_AdaptiveCommanderConfig.TRACE_INTERVAL_MS)
			return;
		m_mLastTrace.Set(slot.GetSlotId(), key);
		m_mLastTraceAt.Set(slot.GetSlotId(), now);
		string line = string.Format("faction=%1 numeric_slot=%2 generation=%3 graph_revision=%4 assignment_revision=%5 candidate=%6 allocated=%7 demand=%8 urgency=%9",
			decision.m_sFaction, slot.GetSlotId(), decision.m_iGeneration, decision.m_iGraphRevision,
			decision.m_iAssignmentRevision, candidate.m_iNodeId, candidate.m_fAllocated, candidate.m_fDemand, score.m_iUrgency);
		line += string.Format(" strategic=%1 approach=%2 threat=%3 readiness=%4 allocation=%5 continuation=%6 total=%7 reason=%8 posture=%9",
			score.m_fStrategic, score.m_fApproach, score.m_fThreat, score.m_fReadiness, score.m_fAllocation, score.m_fContinuation,
			score.m_fTotal, decision.m_sReason, decision.m_sPosture);
		line += string.Format(" current_target=%1 candidate_position=%2 ready=%3 alive=%4 desired=%5 role=%6 route_known=%7 deferred=%8",
			m_Graph.FindNodeId(slot.GetTargetBase()), candidate.m_vPosition, slot.IsCombatReady(), AICF_GroupRuntime.CountAliveAgents(slot.GetGroup()),
			slot.GetDesiredSize(), slot.GetRole(), candidate.m_bKnownRoute, candidate.m_bDeferred);
		AICF_Stage35Diagnostics.Info("ADAPTIVE_DECISION", line);
	}
}
