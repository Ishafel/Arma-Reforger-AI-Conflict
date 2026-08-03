// Snapshot of stock Conflict radio reachability. Edges are directed because the stock query is directed.
class AICF_ObjectiveGraph
{
	protected ref array<ref AICF_ObjectiveNode> m_aNodes = {};
	protected int m_iEdgeCount;

	bool Build(
		array<SCR_CampaignMilitaryBaseComponent> graphBases,
		array<SCR_CampaignMilitaryBaseComponent> objectiveBases)
	{
		m_aNodes.Clear();
		m_iEdgeCount = 0;

		foreach (SCR_CampaignMilitaryBaseComponent base : graphBases)
		{
			if (!base || !base.GetOwner() || !base.IsInitialized())
			{
				AICF_Diagnostics.Warning("GRAPH_NODE_SKIPPED", "Null, ownerless, or uninitialized base skipped");
				continue;
			}

			m_aNodes.Insert(new AICF_ObjectiveNode(m_aNodes.Count(), base, objectiveBases.Contains(base)));
		}

		if (m_aNodes.IsEmpty())
		{
			AICF_Diagnostics.Error("GRAPH_EMPTY", "No valid nodes are available for the objective graph");
			return false;
		}

		for (int sourceId = 0; sourceId < m_aNodes.Count(); sourceId++)
		{
			AICF_ObjectiveNode sourceNode = m_aNodes[sourceId];
			SCR_CampaignMilitaryBaseComponent sourceBase = sourceNode.GetBase();
			IEntity sourceOwner = sourceBase.GetOwner();
			SCR_CoverageRadioComponent sourceRadio = SCR_CoverageRadioComponent.Cast(
				sourceOwner.FindComponent(SCR_CoverageRadioComponent));

			// CanReachByRadio() dereferences the source radio internally in 1.7.0.54.
			if (!sourceRadio)
			{
				AICF_Diagnostics.Warning(
					"GRAPH_SOURCE_RADIO_MISSING",
					string.Format("node=%1 %2", sourceId, AICF_Diagnostics.DescribeBase(sourceBase)));
				continue;
			}

			for (int targetId = 0; targetId < m_aNodes.Count(); targetId++)
			{
				if (sourceId == targetId)
					continue;

				AICF_ObjectiveNode targetNode = m_aNodes[targetId];
				SCR_CampaignMilitaryBaseComponent targetBase = targetNode.GetBase();
				IEntity targetOwner = targetBase.GetOwner();
				if (!targetOwner)
					continue;

				if (!sourceBase.CanReachByRadio(targetOwner))
					continue;

				sourceNode.AddOutgoingNodeId(targetId);
				m_iEdgeCount++;
				AICF_Diagnostics.Info(
					"GRAPH_EDGE",
					string.Format("from=%1 to=%2", sourceId, targetId));
			}
		}

		LogNodes();
		AICF_Diagnostics.Info(
			"GRAPH_SUMMARY",
			string.Format("nodes=%1 objectives=%2 directed_edges=%3", m_aNodes.Count(), CountObjectives(), m_iEdgeCount));

		if (m_iEdgeCount == 0)
			AICF_Diagnostics.Warning("GRAPH_DISCONNECTED", "The graph has no radio-reachability edges");

		return true;
	}

	int GetNodeCount()
	{
		return m_aNodes.Count();
	}

	AICF_ObjectiveNode GetNode(int nodeId)
	{
		if (nodeId < 0 || nodeId >= m_aNodes.Count())
			return null;

		return m_aNodes[nodeId];
	}

	int FindNodeId(SCR_CampaignMilitaryBaseComponent base)
	{
		for (int i = 0; i < m_aNodes.Count(); i++)
		{
			if (m_aNodes[i].GetBase() == base)
				return i;
		}

		return -1;
	}

	protected int CountObjectives()
	{
		int count;
		foreach (AICF_ObjectiveNode node : m_aNodes)
		{
			if (node.IsObjective())
				count++;
		}

		return count;
	}

	protected void LogNodes()
	{
		foreach (AICF_ObjectiveNode node : m_aNodes)
		{
			string outgoing = "";
			foreach (int targetId : node.GetOutgoingNodeIds())
			{
				if (!outgoing.IsEmpty())
					outgoing += ",";
				outgoing += targetId.ToString();
			}

			if (outgoing.IsEmpty())
				outgoing = "none";

			AICF_Diagnostics.Info(
				"GRAPH_NODE",
				string.Format("node=%1 objective=%2 outgoing=[%3] %4",
					node.GetId(),
					node.IsObjective(),
					outgoing,
					AICF_Diagnostics.DescribeBase(node.GetBase())));
		}
	}
}
