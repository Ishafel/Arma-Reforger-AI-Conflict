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

		int bestNodeId = -1;
		int bestDistance = int.MAX;
		for (int head = 0; head < queue.Count(); head++)
		{
			int currentNodeId = queue[head];
			AICF_ObjectiveNode currentNode = graph.GetNode(currentNodeId);
			if (!currentNode)
				continue;

			SCR_CampaignMilitaryBaseComponent currentBase = currentNode.GetBase();
			if (currentNodeId != startNodeId &&
				currentNode.IsObjective() &&
				currentBase != excludedTarget &&
				currentBase.IsValidTarget(faction))
			{
				if (distance[currentNodeId] < bestDistance ||
					(distance[currentNodeId] == bestDistance && currentNodeId < bestNodeId))
				{
					bestNodeId = currentNodeId;
					bestDistance = distance[currentNodeId];
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
