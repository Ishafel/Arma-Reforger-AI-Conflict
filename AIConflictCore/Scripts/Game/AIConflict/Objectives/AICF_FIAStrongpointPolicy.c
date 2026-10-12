// Radio edges направлены. Для топологии промежуточного узла учитываем связь
// в любом направлении; запрет соседства с HQ также проверяется в обе стороны.
class AICF_FIAStrongpointPolicy
{
	static bool Adjacent(AICF_ObjectiveGraph graph, int a, int b)
	{
		if (a == b || !graph.GetNode(a) || !graph.GetNode(b)) return false;
		return graph.GetNode(a).GetOutgoingNodeIds().Contains(b) || graph.GetNode(b).GetOutgoingNodeIds().Contains(a);
	}

	static bool Reachable(AICF_ObjectiveGraph graph, int from, int to)
	{
		array<int> visited = {from};
		int cursor;
		while (cursor < visited.Count())
		{
			int current = visited[cursor++];
			if (current == to) return true;
			for (int next; next < graph.GetNodeCount(); next++)
			{
				if (!visited.Contains(next) && Adjacent(graph, current, next)) visited.Insert(next);
			}
		}
		return false;
	}

	// Отдельный pure boundary пригоден для synthetic graph probes без изменения мира.
	static bool Eligible(AICF_ObjectiveGraph graph, int nodeId, array<int> headquarters)
	{
		if (!graph || !graph.GetNode(nodeId) || headquarters.Count() != 2 || headquarters.Contains(nodeId)) return false;
		int degree;
		for (int other; other < graph.GetNodeCount(); other++)
		{
			if (Adjacent(graph, nodeId, other)) degree++;
		}
		if (degree < 2) return false;
		foreach (int hq : headquarters)
		{
			if (!graph.GetNode(hq) || Adjacent(graph, nodeId, hq) || !Reachable(graph, nodeId, hq)) return false;
		}
		return true;
	}

	static void Collect(AICF_ObjectiveGraph graph, array<int> headquarters, array<int> candidates)
	{
		headquarters.Clear();
		candidates.Clear();
		for (int i; i < graph.GetNodeCount(); i++)
		{
			SCR_CampaignMilitaryBaseComponent base = graph.GetNode(i).GetBase();
			if (base && base.IsHQ()) headquarters.Insert(i);
		}
		for (int i; i < graph.GetNodeCount(); i++)
		{
			AICF_ObjectiveNode node = graph.GetNode(i);
			SCR_CampaignMilitaryBaseComponent base = node.GetBase();
			if (!node.IsObjective() || !base || !base.GetOwner() || base.IsHQ() || !base.GetFaction() || base.GetFaction().GetFactionKey() != "FIA") continue;
			if (Eligible(graph, i, headquarters)) candidates.Insert(i);
		}
	}
}
