// Deterministic live target selection over the Stage 0 radio graph. Stage 3.5
// ranks every reachable attack objective so A0/A1/A2 do not unconditionally
// converge, and separately selects a threatened QRF base or safe front line.
class AICF_TargetSelector
{
	SCR_CampaignMilitaryBaseComponent SelectAttackTarget(
		AICF_ObjectiveGraph graph,
		SCR_CampaignFaction faction,
		out string selectionMode,
		SCR_CampaignMilitaryBaseComponent excludedTarget = null,
		int preferredIndex = 0)
	{
		selectionMode = "NO_REACHABLE_TARGET";
		if (!graph || !faction || !faction.GetMainBase())
			return null;

		int startNodeId = graph.FindNodeId(faction.GetMainBase());
		if (startNodeId < 0)
			return null;

		array<int> queue = {};
		array<int> distance = {};
		array<int> rankedNodeIds = {};
		distance.Resize(graph.GetNodeCount());
		for (int i = 0; i < distance.Count(); i++)
			distance[i] = -1;

		queue.Insert(startNodeId);
		distance[startNodeId] = 0;
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
				if (currentBase.GetType() == SCR_ECampaignBaseType.RELAY ||
					(currentNode.IsObjective() && !currentBase.IsHQ()))
				{
					InsertRankedAttackNode(graph, rankedNodeIds, currentNodeId, distance);
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

		if (rankedNodeIds.IsEmpty() && excludedTarget)
			return SelectAttackTarget(graph, faction, selectionMode, null, preferredIndex);
		if (rankedNodeIds.IsEmpty())
			return null;

		if (preferredIndex < 0)
			preferredIndex = 0;
		int selectedNodeId = SelectAttackPlanNode(
			graph,
			rankedNodeIds,
			preferredIndex,
			selectionMode);
		AICF_ObjectiveNode selectedNode = graph.GetNode(selectedNodeId);
		if (!selectedNode)
			return null;

		return selectedNode.GetBase();
	}

	SCR_CampaignMilitaryBaseComponent SelectDefendTarget(
		AICF_ObjectiveGraph graph,
		SCR_CampaignFaction faction,
		out string posture,
		out string trigger)
	{
		posture = "FORWARD_DEFEND";
		trigger = "FORWARD_LINE";
		if (!graph || !faction)
			return null;

		SCR_CampaignMilitaryBaseComponent threatenedBase;
		int threatenedNodeId = int.MAX;
		bool threatenedIsHQ;
		for (int nodeId = 0; nodeId < graph.GetNodeCount(); nodeId++)
		{
			AICF_ObjectiveNode node = graph.GetNode(nodeId);
			SCR_CampaignMilitaryBaseComponent base;
			if (node)
				base = node.GetBase();
			if (!IsFriendlyDefendAnchor(base, faction) || !IsThreatened(base))
				continue;

			bool isHQ = base == faction.GetMainBase();
			if (!threatenedBase || (isHQ && !threatenedIsHQ) ||
				(isHQ == threatenedIsHQ && nodeId < threatenedNodeId))
			{
				threatenedBase = base;
				threatenedNodeId = nodeId;
				threatenedIsHQ = isHQ;
			}
		}

		if (threatenedBase)
		{
			posture = "QRF";
			if (threatenedIsHQ)
				trigger = "HQ_THREAT";
			else
				trigger = "CONTESTED";
			return threatenedBase;
		}

		SCR_CampaignMilitaryBaseComponent frontLineBase;
		float frontLineEnemyDistanceSq = float.MAX;
		int frontLineNodeId = int.MAX;
		SCR_CampaignMilitaryBaseComponent hqFallback;
		for (int candidateNodeId = 0; candidateNodeId < graph.GetNodeCount(); candidateNodeId++)
		{
			AICF_ObjectiveNode candidateNode = graph.GetNode(candidateNodeId);
			SCR_CampaignMilitaryBaseComponent candidate;
			if (candidateNode)
				candidate = candidateNode.GetBase();
			if (candidate == faction.GetMainBase() &&
				IsFriendlyDefendAnchor(candidate, faction) && !IsThreatened(candidate))
			{
				hqFallback = candidate;
				continue;
			}
			if (!IsSafeForwardDefendBase(candidate, faction))
				continue;
			if (!candidateNode.IsObjective() || candidate.GetType() == SCR_ECampaignBaseType.RELAY)
				continue;

			float enemyDistanceSq = GetNearestHostileDistanceSq(graph, candidate, faction);
			if (enemyDistanceSq < frontLineEnemyDistanceSq ||
				(enemyDistanceSq == frontLineEnemyDistanceSq && candidateNodeId < frontLineNodeId))
			{
				frontLineBase = candidate;
				frontLineEnemyDistanceSq = enemyDistanceSq;
				frontLineNodeId = candidateNodeId;
			}
		}

		if (frontLineBase)
			return frontLineBase;

		trigger = "HQ_FALLBACK";
		return hqFallback;
	}

	SCR_CampaignMilitaryBaseComponent SelectLossResponseTarget(
		AICF_ObjectiveGraph graph,
		SCR_CampaignFaction faction,
		SCR_CampaignMilitaryBaseComponent lostBase)
	{
		if (!graph || !faction || !lostBase || !lostBase.GetOwner())
			return null;

		SCR_CampaignMilitaryBaseComponent bestBase;
		float bestDistanceSq = float.MAX;
		int bestNodeId = int.MAX;
		SCR_CampaignMilitaryBaseComponent hqFallback;
		for (int nodeId = 0; nodeId < graph.GetNodeCount(); nodeId++)
		{
			AICF_ObjectiveNode node = graph.GetNode(nodeId);
			SCR_CampaignMilitaryBaseComponent candidate;
			if (node)
				candidate = node.GetBase();
			if (candidate == faction.GetMainBase() &&
				IsFriendlyDefendAnchor(candidate, faction) && !IsThreatened(candidate))
			{
				hqFallback = candidate;
				continue;
			}
			if (!IsSafeForwardDefendBase(candidate, faction))
				continue;
			if (!node.IsObjective() || candidate.GetType() == SCR_ECampaignBaseType.RELAY)
				continue;

			float distanceSq = vector.DistanceSqXZ(
				candidate.GetOwner().GetOrigin(),
				lostBase.GetOwner().GetOrigin());
			if (distanceSq < bestDistanceSq ||
				(distanceSq == bestDistanceSq && nodeId < bestNodeId))
			{
				bestBase = candidate;
				bestDistanceSq = distanceSq;
				bestNodeId = nodeId;
			}
		}

		if (bestBase)
			return bestBase;

		return hqFallback;
	}

	protected void InsertRankedAttackNode(
		AICF_ObjectiveGraph graph,
		array<int> rankedNodeIds,
		int nodeId,
		array<int> distance)
	{
		int insertIndex = rankedNodeIds.Count();
		for (int i = 0; i < rankedNodeIds.Count(); i++)
		{
			int rankedNodeId = rankedNodeIds[i];
			bool nodePreferred = IsOrdinaryAttackObjective(graph, nodeId);
			bool rankedPreferred = IsOrdinaryAttackObjective(graph, rankedNodeId);
			if (distance[nodeId] < distance[rankedNodeId] ||
				(distance[nodeId] == distance[rankedNodeId] && nodePreferred && !rankedPreferred) ||
				(distance[nodeId] == distance[rankedNodeId] && nodePreferred == rankedPreferred && nodeId < rankedNodeId))
			{
				insertIndex = i;
				break;
			}
		}

		if (insertIndex >= rankedNodeIds.Count())
			rankedNodeIds.Insert(nodeId);
		else
				rankedNodeIds.InsertAt(nodeId, insertIndex);
	}

	protected int SelectAttackPlanNode(
		AICF_ObjectiveGraph graph,
		array<int> rankedNodeIds,
		int preferredIndex,
		out string selectionMode)
	{
		int primaryNodeId = rankedNodeIds[0];
		if (preferredIndex == 0 || rankedNodeIds.Count() == 1)
		{
			if (preferredIndex == 0)
				selectionMode = "PRIMARY_RANKED_REACHABLE";
			else
				selectionMode = "PRIMARY_ONLY_FALLBACK";
			return primaryNodeId;
		}

		int secondaryNodeId = -1;
		for (int adjacentIndex = 1; adjacentIndex < rankedNodeIds.Count(); adjacentIndex++)
		{
			int candidateNodeId = rankedNodeIds[adjacentIndex];
			if (AreAttackNodesAdjacent(graph, primaryNodeId, candidateNodeId))
			{
				secondaryNodeId = candidateNodeId;
				break;
			}
		}
		if (secondaryNodeId < 0 && rankedNodeIds.Count() > 1)
			secondaryNodeId = rankedNodeIds[1];

		if (preferredIndex == 1)
		{
			if (AreAttackNodesAdjacent(graph, primaryNodeId, secondaryNodeId))
				selectionMode = "ADJACENT_TO_PRIMARY";
			else
				selectionMode = "SECONDARY_RANK_FALLBACK";
			return secondaryNodeId;
		}

		for (int supportIndex = 1; supportIndex < rankedNodeIds.Count(); supportIndex++)
		{
			int supportNodeId = rankedNodeIds[supportIndex];
			if (supportNodeId == secondaryNodeId)
				continue;
			if (!AreAttackNodesAdjacent(graph, primaryNodeId, supportNodeId) &&
				!AreAttackNodesAdjacent(graph, secondaryNodeId, supportNodeId))
			{
				continue;
			}

			selectionMode = "SUPPORT_ADJACENT_DIRECTION";
			return supportNodeId;
		}

		selectionMode = "SUPPORT_SECONDARY_DIRECTION";
		return secondaryNodeId;
	}

	protected bool AreAttackNodesAdjacent(
		AICF_ObjectiveGraph graph,
		int firstNodeId,
		int secondNodeId)
	{
		if (!graph || firstNodeId < 0 || secondNodeId < 0 || firstNodeId == secondNodeId)
			return false;

		AICF_ObjectiveNode firstNode = graph.GetNode(firstNodeId);
		AICF_ObjectiveNode secondNode = graph.GetNode(secondNodeId);
		return (firstNode && firstNode.GetOutgoingNodeIds().Contains(secondNodeId)) ||
			(secondNode && secondNode.GetOutgoingNodeIds().Contains(firstNodeId));
	}

	protected bool IsOrdinaryAttackObjective(AICF_ObjectiveGraph graph, int nodeId)
	{
		AICF_ObjectiveNode node;
		if (graph)
			node = graph.GetNode(nodeId);
		return node && node.IsObjective() && node.GetBase() &&
			node.GetBase().GetType() != SCR_ECampaignBaseType.RELAY;
	}

	protected float GetNearestHostileDistanceSq(
		AICF_ObjectiveGraph graph,
		SCR_CampaignMilitaryBaseComponent friendlyBase,
		SCR_CampaignFaction faction)
	{
		float nearestDistanceSq = float.MAX;
		if (!graph || !friendlyBase || !friendlyBase.GetOwner() || !faction)
			return nearestDistanceSq;

		for (int nodeId = 0; nodeId < graph.GetNodeCount(); nodeId++)
		{
			AICF_ObjectiveNode node = graph.GetNode(nodeId);
			SCR_CampaignMilitaryBaseComponent hostileBase;
			if (node)
				hostileBase = node.GetBase();
			Faction hostileFaction;
			if (hostileBase)
				hostileFaction = hostileBase.GetFaction();
			if (!hostileBase || !hostileBase.GetOwner() || !hostileFaction || hostileFaction == faction)
				continue;

			float distanceSq = vector.DistanceSqXZ(
				friendlyBase.GetOwner().GetOrigin(),
				hostileBase.GetOwner().GetOrigin());
			if (distanceSq < nearestDistanceSq)
				nearestDistanceSq = distanceSq;
		}

		return nearestDistanceSq;
	}

	protected bool IsFriendlyDefendAnchor(
		SCR_CampaignMilitaryBaseComponent base,
		SCR_CampaignFaction faction)
	{
		return base && faction && base.GetOwner() && base.IsInitialized() &&
			base.GetFaction() == faction && base.GetSpawnPoint();
	}

	protected bool IsSafeForwardDefendBase(
		SCR_CampaignMilitaryBaseComponent base,
		SCR_CampaignFaction faction)
	{
		if (!IsFriendlyDefendAnchor(base, faction) || IsThreatened(base))
			return false;

		SCR_SpawnPoint spawnPoint = base.GetSpawnPoint();
		return spawnPoint.IsSpawnPointEnabled() && spawnPoint.IsSpawnPointActive() &&
			spawnPoint.GetFactionKey() == faction.GetFactionKey();
	}

	protected bool IsThreatened(SCR_CampaignMilitaryBaseComponent base)
	{
		return base && (base.GetCaptureState() != SCR_EBaseCaptureState.NONE ||
			base.IsBeingCaptured() || base.AreEnemiesPresent());
	}

	protected bool CanTransit(SCR_CampaignMilitaryBaseComponent base, SCR_CampaignFaction faction)
	{
		return base && faction && base.GetFaction() == faction;
	}
}
