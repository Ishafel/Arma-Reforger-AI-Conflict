// Deterministic live target selection over the Stage 0 radio graph.
class AICF_TargetSelector
{
	SCR_CampaignMilitaryBaseComponent SelectAttackTarget(
		AICF_ObjectiveGraph graph,
		SCR_CampaignFaction faction,
		SCR_CampaignMilitaryBaseComponent excludedTarget = null)
	{
		if (!graph || !faction || !faction.GetMainBase())
			return null;

		int startNodeId = graph.FindNodeId(faction.GetMainBase());
		if (startNodeId < 0)
			return null;

		array<int> queue = {};
		array<int> distance = {};
		distance.Resize(graph.GetNodeCount());
		for (int i = 0; i < distance.Count(); i++)
			distance[i] = -1;

		queue.Insert(startNodeId);
		distance[startNodeId] = 0;

		int bestObjectiveNodeId = -1;
		int bestObjectiveDistance = int.MAX;
		int bestRelayNodeId = -1;
		int bestRelayDistance = int.MAX;
		for (int head = 0; head < queue.Count(); head++)
		{
			int currentNodeId = queue[head];
			AICF_ObjectiveNode currentNode = graph.GetNode(currentNodeId);
			if (!currentNode)
				continue;

			SCR_CampaignMilitaryBaseComponent currentBase = currentNode.GetBase();
			if (currentNodeId != startNodeId &&
				currentBase != excludedTarget &&
				currentBase.IsValidTarget(faction))
			{
				if (currentBase.GetType() == SCR_ECampaignBaseType.RELAY)
				{
					if (distance[currentNodeId] < bestRelayDistance ||
						(distance[currentNodeId] == bestRelayDistance && currentNodeId < bestRelayNodeId))
					{
						bestRelayNodeId = currentNodeId;
						bestRelayDistance = distance[currentNodeId];
					}
				}
				else if (currentNode.IsObjective() && !currentBase.IsHQ())
				{
					if (distance[currentNodeId] < bestObjectiveDistance ||
						(distance[currentNodeId] == bestObjectiveDistance && currentNodeId < bestObjectiveNodeId))
					{
						bestObjectiveNodeId = currentNodeId;
						bestObjectiveDistance = distance[currentNodeId];
					}
				}
			}

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

		int bestNodeId = bestObjectiveNodeId;
		if (bestNodeId < 0)
			bestNodeId = bestRelayNodeId;

		if (bestNodeId < 0 && excludedTarget)
			return SelectAttackTarget(graph, faction, null);

		AICF_ObjectiveNode bestNode = graph.GetNode(bestNodeId);
		if (!bestNode)
			return null;

		return bestNode.GetBase();
	}

	SCR_CampaignMilitaryBaseComponent SelectDefendTarget(SCR_CampaignFaction faction)
	{
		if (!faction)
			return null;

		SCR_CampaignMilitaryBaseComponent mainBase = faction.GetMainBase();
		if (!mainBase || mainBase.GetFaction() != faction)
			return null;

		return mainBase;
	}

	protected bool CanTransit(SCR_CampaignMilitaryBaseComponent base, SCR_CampaignFaction faction)
	{
		return base && faction && base.GetFaction() == faction;
	}
}
