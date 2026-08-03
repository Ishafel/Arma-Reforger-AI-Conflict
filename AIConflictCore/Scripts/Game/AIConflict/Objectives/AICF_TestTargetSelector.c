// Stage 0 only: deterministically choose the nearest valid objective in the radio graph.
class AICF_TestTargetSelector
{
	SCR_CampaignMilitaryBaseComponent SelectTarget(AICF_ObjectiveGraph graph, SCR_CampaignFaction faction)
	{
		if (!graph || !faction || !faction.GetMainBase())
		{
			AICF_Diagnostics.Error("TARGET_INPUT_INVALID", "Graph, faction, or faction HQ is missing");
			return null;
		}

		int startNodeId = graph.FindNodeId(faction.GetMainBase());
		if (startNodeId < 0)
		{
			AICF_Diagnostics.Error(
				"TARGET_HQ_NOT_IN_GRAPH",
				string.Format("Faction %1 HQ is absent from the graph", faction.GetFactionKey()));
			return null;
		}

		array<int> queue = {};
		array<int> distance = {};
		distance.Resize(graph.GetNodeCount());
		for (int i = 0; i < distance.Count(); i++)
			distance[i] = -1;

		queue.Insert(startNodeId);
		distance[startNodeId] = 0;

		int bestNodeId = -1;
		int bestDistance = int.MAX;
		for (int head = 0; head < queue.Count(); head++)
		{
			int currentNodeId = queue[head];
			AICF_ObjectiveNode currentNode = graph.GetNode(currentNodeId);
			if (!currentNode)
				continue;

			SCR_CampaignMilitaryBaseComponent currentBase = currentNode.GetBase();
			if (currentNodeId != startNodeId && currentNode.IsObjective() && currentBase.IsValidTarget(faction))
			{
				if (distance[currentNodeId] < bestDistance ||
					(distance[currentNodeId] == bestDistance && currentNodeId < bestNodeId))
				{
					bestNodeId = currentNodeId;
					bestDistance = distance[currentNodeId];
				}
			}

			// Enemy/neutral objectives are candidate endpoints, never transit territory.
			if (currentNodeId != startNodeId && !CanTransit(currentBase, faction))
				continue;

			foreach (int nextNodeId : currentNode.GetOutgoingNodeIds())
			{
				if (nextNodeId < 0 || nextNodeId >= distance.Count() || distance[nextNodeId] >= 0)
					continue;

				distance[nextNodeId] = distance[currentNodeId] + 1;
				queue.Insert(nextNodeId);
			}
		}

		if (bestNodeId < 0)
		{
			AICF_Diagnostics.Error(
				"TARGET_NOT_FOUND",
				string.Format("Faction %1 has no valid objective reachable from its HQ", faction.GetFactionKey()));
			return null;
		}

		AICF_ObjectiveNode bestNode = graph.GetNode(bestNodeId);
		AICF_Diagnostics.Info(
			"TARGET_SELECTED",
			string.Format("faction=%1 node=%2 hops=%3 target={%4}",
				faction.GetFactionKey(),
				bestNodeId,
				bestDistance,
				AICF_Diagnostics.DescribeBase(bestNode.GetBase())));
		return bestNode.GetBase();
	}

	protected bool CanTransit(SCR_CampaignMilitaryBaseComponent base, SCR_CampaignFaction faction)
	{
		if (!base || !faction)
			return false;

		// RELAY nodes are useful transit nodes only after stock Conflict assigns them to this faction.
		// This deliberately rejects neutral/enemy territory even if a physical radio edge exists.
		return base.GetFaction() == faction;
	}
}
