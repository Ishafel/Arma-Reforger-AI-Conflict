class AICF_ReinforcementBaseCandidate
{
	SCR_CampaignMilitaryBaseComponent Base;
	bool Connected;
	int TargetHops;
	int NodeId;
	float RemainingSupplies;
}

// Deterministic Stage 4 spawn-base filtering and ranking.
class AICF_ReinforcementBaseSelector
{
	protected AICF_ObjectiveGraph m_Graph;
	protected AICF_SupplyNetwork m_SupplyNetwork;
	protected AICF_ConflictAdapter m_ConflictAdapter;
	protected bool m_bRejectedUnsafeSite;

	void AICF_ReinforcementBaseSelector(
		AICF_ObjectiveGraph graph,
		AICF_SupplyNetwork supplyNetwork,
		AICF_ConflictAdapter conflictAdapter)
	{
		m_Graph = graph;
		m_SupplyNetwork = supplyNetwork;
		m_ConflictAdapter = conflictAdapter;
	}

	bool HasRejectedUnsafeSite()
	{
		return m_bRejectedUnsafeSite;
	}

	bool Select(
		SCR_CampaignFaction faction,
		AICF_GroupSlot slot,
		SCR_CampaignMilitaryBaseComponent savedTargetBase,
		int supplyCost,
		out SCR_CampaignMilitaryBaseComponent selectedBase)
	{
		selectedBase = null;
		if (!faction || !slot || !m_Graph || !m_SupplyNetwork || !m_ConflictAdapter)
			return false;

		ref AICF_ReinforcementBaseCandidate best;
		for (int nodeId = 0; nodeId < m_Graph.GetNodeCount(); nodeId++)
		{
			AICF_ObjectiveNode node = m_Graph.GetNode(nodeId);
			SCR_CampaignMilitaryBaseComponent base = null;
			if (node)
				base = node.GetBase();
			string rejectionReason = m_ConflictAdapter.GetSpawnRejectionReason(base, faction);
			if (!rejectionReason.IsEmpty())
			{
				if (rejectionReason == "ENEMY_OWNED" || rejectionReason == "CONTESTED")
					m_bRejectedUnsafeSite = true;
				AICF_Stage4Diagnostics.Info(
					"BASE_CANDIDATE_REJECTED",
					string.Format(
						"faction=%1 slot=%2 base=%3 reason=%4",
						faction.GetFactionKey(),
						slot.GetSlotId(),
						AICF_Stage1Diagnostics.BaseKey(base),
						rejectionReason));
				continue;
			}

			float supplies = base.GetSupplies();
			if (supplies < supplyCost)
			{
				AICF_Stage4Diagnostics.Info(
					"BASE_CANDIDATE_REJECTED",
					string.Format(
						"faction=%1 slot=%2 base=%3 reason=INSUFFICIENT_SUPPLIES supplies=%4 required=%5",
						faction.GetFactionKey(),
						slot.GetSlotId(),
						AICF_Stage1Diagnostics.BaseKey(base),
						supplies,
						supplyCost));
				continue;
			}

			ref AICF_ReinforcementBaseCandidate candidate = new AICF_ReinforcementBaseCandidate();
			candidate.Base = base;
			candidate.NodeId = nodeId;
			candidate.RemainingSupplies = supplies - supplyCost;
			candidate.TargetHops = m_Graph.GetHopDistance(base, savedTargetBase);
			if (candidate.TargetHops < 0)
				candidate.TargetHops = 1000000;
			SCR_CampaignMilitaryBaseComponent sourceBase;
			int sourceHops;
			candidate.Connected = m_SupplyNetwork.IsConnectedToSource(base, faction, sourceBase, sourceHops);

			if (!best || IsBetter(candidate, best))
				best = candidate;
		}

		if (!best)
			return false;

		selectedBase = best.Base;
		AICF_Stage4Diagnostics.Info(
			"BASE_SELECTED",
			string.Format(
				"faction=%1 slot=%2 base=%3 connected=%4 target_hops=%5 remaining_supplies=%6 node=%7 graph_revision=%8",
				faction.GetFactionKey(),
				slot.GetSlotId(),
				AICF_Stage1Diagnostics.BaseKey(selectedBase),
				best.Connected,
				best.TargetHops,
				best.RemainingSupplies,
				best.NodeId,
				m_Graph.GetRevision()));
		return true;
	}

	protected bool IsBetter(
		AICF_ReinforcementBaseCandidate candidate,
		AICF_ReinforcementBaseCandidate best)
	{
		if (candidate.Connected != best.Connected)
			return candidate.Connected;
		if (candidate.TargetHops != best.TargetHops)
			return candidate.TargetHops < best.TargetHops;
		if (candidate.RemainingSupplies != best.RemainingSupplies)
			return candidate.RemainingSupplies > best.RemainingSupplies;
		return candidate.NodeId < best.NodeId;
	}
}
