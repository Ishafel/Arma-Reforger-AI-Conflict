// Read-only inputs shared by live adapter and VM fixtures. No entity mutations.
class AICF_AdaptiveCandidate
{
	int m_iNodeId;
	SCR_CampaignMilitaryBaseComponent m_Base;
	vector m_vPosition;
	bool m_bAttack;
	bool m_bAvailable = true;
	bool m_bDeferred;
	bool m_bKnownRoute;
	float m_fRouteMeters;
	bool m_bObjective;
	int m_iOpenDirections;
	int m_iConnectivity;
	bool m_bHQ;
	bool m_bThreat;
	int m_iThreatAtMs;
	bool m_bRecentLoss;
	float m_fAllocated;
	float m_fDemand = AICF_AdaptiveCommanderConfig.UNKNOWN_RESISTANCE;
	float m_fFrontValue;
}

class AICF_AdaptiveSquad
{
	bool m_bReady;
	int m_iAlive;
	int m_iDesired;
	AICF_EGroupRole m_Role;
	vector m_vPosition;
	int m_iCurrentNode = -1;
	float m_fProgressMeters;
}

class AICF_AdaptiveScore
{
	bool m_bEligible;
	int m_iUrgency;
	float m_fStrategic;
	float m_fApproach;
	float m_fThreat;
	float m_fReadiness;
	float m_fAllocation;
	float m_fContinuation;
	float m_fTotal;

	static float EffectiveForce(bool ready, int alive, int desired)
	{
		if (!ready || alive < AICF_AdaptiveCommanderConfig.MIN_ATTACK_ALIVE)
			return 0;
		float ratio = Math.Clamp(alive / Math.Max(1.0, desired), 0.0, 1.0);
		return alive * Math.Clamp(ratio, 0.5, 1.0);
	}

	static AICF_AdaptiveScore Evaluate(AICF_AdaptiveSquad squad, AICF_AdaptiveCandidate candidate, int now)
	{
		AICF_AdaptiveScore score = new AICF_AdaptiveScore();
		if (!squad || !candidate || !squad.m_bReady || squad.m_iAlive <= 0 ||
			!candidate.m_bAvailable || candidate.m_bDeferred)
			return score;
		if (candidate.m_bAttack && (squad.m_Role != AICF_EGroupRole.ATTACK ||
			squad.m_iAlive < AICF_AdaptiveCommanderConfig.MIN_ATTACK_ALIVE))
			return score;
		score.m_bEligible = true;
		bool freshThreat = candidate.m_bThreat && now >= candidate.m_iThreatAtMs &&
			now - candidate.m_iThreatAtMs <= AICF_AdaptiveCommanderConfig.THREAT_TTL_MS;
		float demand = Math.Max(AICF_AdaptiveCommanderConfig.UNKNOWN_RESISTANCE, candidate.m_fDemand);
		if (freshThreat)
		{
			demand = Math.Max(demand, AICF_AdaptiveCommanderConfig.CONTESTED_DEMAND);
			if (candidate.m_bHQ)
				demand = Math.Max(demand, AICF_AdaptiveCommanderConfig.HQ_DEMAND);
			if (candidate.m_fAllocated < demand)
			{
				score.m_iUrgency = 2;
				score.m_fThreat = AICF_AdaptiveCommanderConfig.BASE_THREAT_VALUE;
				if (candidate.m_bHQ)
				{
					score.m_iUrgency = 3;
					score.m_fThreat = AICF_AdaptiveCommanderConfig.HQ_THREAT_VALUE;
				}
			}
		}
		if (candidate.m_bObjective)
			score.m_fStrategic = AICF_AdaptiveCommanderConfig.OBJECTIVE_VALUE;
		else if (candidate.m_bAttack)
			score.m_fStrategic = AICF_AdaptiveCommanderConfig.RELAY_VALUE;
		score.m_fStrategic += Math.Min(6, candidate.m_iOpenDirections) * AICF_AdaptiveCommanderConfig.OPEN_DIRECTION_VALUE;
		score.m_fStrategic += Math.Min(3, candidate.m_iConnectivity) * AICF_AdaptiveCommanderConfig.CONNECTIVITY_VALUE;
		score.m_fStrategic += candidate.m_fFrontValue;
		if (candidate.m_bRecentLoss)
			score.m_fThreat += AICF_AdaptiveCommanderConfig.LOSS_VALUE;
		float distance = vector.DistanceXZ(squad.m_vPosition, candidate.m_vPosition);
		if (candidate.m_bKnownRoute)
			distance = Math.Max(distance, candidate.m_fRouteMeters);
		else
			score.m_fApproach -= AICF_AdaptiveCommanderConfig.UNKNOWN_ROUTE_COST;
		score.m_fApproach -= Math.Min(AICF_AdaptiveCommanderConfig.MAX_DISTANCE_COST, distance * AICF_AdaptiveCommanderConfig.DISTANCE_COST_PER_METER);
		float force = EffectiveForce(squad.m_bReady, squad.m_iAlive, squad.m_iDesired);
		score.m_fReadiness = Math.Min(10, force);
		if (candidate.m_bAttack)
			score.m_fReadiness -= Math.Max(0, squad.m_iDesired - squad.m_iAlive) * AICF_AdaptiveCommanderConfig.ATTRITION_COST;
		float coverage = candidate.m_fAllocated / demand;
		score.m_fAllocation = Math.Max(0, 1 - coverage) * AICF_AdaptiveCommanderConfig.DEFICIT_VALUE;
		score.m_fAllocation -= Math.Min(3, coverage) * AICF_AdaptiveCommanderConfig.SATURATION_COST;
		if (candidate.m_iNodeId == squad.m_iCurrentNode)
		{
			score.m_fContinuation = AICF_AdaptiveCommanderConfig.CONTINUATION_VALUE;
			if (squad.m_fProgressMeters >= AICF_AdaptiveCommanderConfig.PROGRESS_METERS || distance <= AICF_AdaptiveCommanderConfig.ARRIVAL_METERS)
				score.m_fContinuation += AICF_AdaptiveCommanderConfig.PROGRESS_VALUE;
		}
		score.m_fTotal = score.m_fStrategic + score.m_fApproach + score.m_fThreat +
			score.m_fReadiness + score.m_fAllocation + score.m_fContinuation;
		return score;
	}

	static bool IsBetter(AICF_AdaptiveScore score, int node, AICF_AdaptiveScore best, int bestNode)
	{
		return score && score.m_bEligible && (!best || !best.m_bEligible ||
			score.m_iUrgency > best.m_iUrgency || (score.m_iUrgency == best.m_iUrgency &&
			(score.m_fTotal > best.m_fTotal || (score.m_fTotal == best.m_fTotal && node < bestNode))));
	}

	static AICF_AdaptiveCandidate Choose(AICF_AdaptiveSquad squad, array<ref AICF_AdaptiveCandidate> candidates,
		int now, out AICF_AdaptiveScore bestScore, out string reason)
	{
		bestScore = null;
		reason = "ADAPTIVE_SCORE";
		AICF_AdaptiveCandidate best;
		AICF_AdaptiveCandidate current;
		AICF_AdaptiveScore currentScore;
		foreach (AICF_AdaptiveCandidate candidate : candidates)
		{
			AICF_AdaptiveScore score = Evaluate(squad, candidate, now);
			if (candidate.m_iNodeId == squad.m_iCurrentNode && score.m_bEligible)
			{
				current = candidate;
				currentScore = score;
			}
			int bestId = int.MAX;
			if (best)
				bestId = best.m_iNodeId;
			if (IsBetter(score, candidate.m_iNodeId, bestScore, bestId))
			{
				best = candidate;
				bestScore = score;
			}
		}
		if (current && best == current)
			reason = "ADAPTIVE_CONTINUE";
		if (current && best != current && bestScore.m_iUrgency <= currentScore.m_iUrgency &&
			bestScore.m_fTotal < currentScore.m_fTotal + AICF_AdaptiveCommanderConfig.SWITCH_MARGIN)
		{
			best = current;
			bestScore = currentScore;
			reason = "ADAPTIVE_MARGIN_HELD";
		}
		return best;
	}
}

// Captured identity is checked again at the planner boundary before any write.
class AICF_AdaptiveDecision
{
	AICF_GroupSlot m_Slot;
	SCR_AIGroup m_Group;
	EntityID m_GroupId;
	FactionKey m_sFaction;
	int m_iGeneration;
	int m_iGraphRevision;
	int m_iAssignmentRevision;
	SCR_CampaignMilitaryBaseComponent m_Target;
	string m_sPosture;
	string m_sReason;
	bool m_bUrgent;

	bool IsCurrent(AICF_GroupSlot slot, SCR_CampaignFaction faction, AICF_ObjectiveGraph graph)
	{
		if (!Replication.IsServer() || !slot || slot != m_Slot || !faction || !graph || !m_Group)
			return false;
		if (faction.GetFactionKey() != m_sFaction || !slot.IsCombatReady() || slot.GetGroup() != m_Group || m_Group.GetID() != m_GroupId)
			return false;
		return m_Group.GetFaction() == faction && slot.GetSpawnGeneration() == m_iGeneration &&
			graph.GetRevision() == m_iGraphRevision && slot.GetStrategicAssignmentRevision() == m_iAssignmentRevision &&
			!slot.HasPlayerStrategicIntent() && !slot.HasPlayerStrategicOrder();
	}
}
