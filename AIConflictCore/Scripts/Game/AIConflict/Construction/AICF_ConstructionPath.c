// Ограниченный A* по navmesh. Каждый переход подтверждён native RayTrace;
// отсутствие пути в пределах квоты не доказывает глобальную недостижимость.
// Состояние принадлежит одному candidate/order, callbacks и entities нет.
class AICF_ConstructionPathNode
{
	int m_iX;
	int m_iZ;
	vector m_vPosition;
	float m_fCost;
	bool m_bClosed;
}

class AICF_ConstructionPath
{
	static const float STEP = 2;
	static const int MAX_NODES = 512;
	static const int MAX_QUERIES = 4096;
	protected ref array<ref AICF_ConstructionPathNode> m_aNodes = {};
	protected ref array<vector> m_aGoals = {};
	protected vector m_vOrigin;
	protected vector m_vMin;
	protected vector m_vMax;
	protected vector m_vBlockMin;
	protected vector m_vBlockMax;
	protected int m_iActive = -1;
	protected int m_iEdge;
	protected int m_iTileRetries;
	protected int m_iExpanded;
	protected AIPathfindingComponent m_Pathfinding;

	string Describe()
	{
		return string.Format("nodes=%1 expanded=%2 goals=%3 active=%4 edge=%5 block_min=%6 block_max=%7 origin=%8", m_aNodes.Count(), m_iExpanded, m_aGoals.Count(), m_iActive, m_iEdge, m_vBlockMin, m_vBlockMax, m_vOrigin);
	}

	void Begin(vector start, array<vector> goals, vector blockMin, vector blockMax)
	{
		m_vOrigin = start;
		m_vMin = start;
		m_vMax = start;
		m_vBlockMin = blockMin;
		m_vBlockMax = blockMax;
		foreach (vector goal : goals)
		{
			m_aGoals.Insert(goal);
			for (int axis; axis < 3; axis++)
			{
				m_vMin[axis] = Math.Min(m_vMin[axis], goal[axis]);
				m_vMax[axis] = Math.Max(m_vMax[axis], goal[axis]);
			}
		}
		m_vMin -= "32 0 32";
		m_vMax += "32 0 32";
		AICF_ConstructionPathNode node = new AICF_ConstructionPathNode();
		node.m_vPosition = start;
		m_aNodes.Insert(node);
	}

	protected float Heuristic(vector position)
	{
		float best = float.MAX;
		foreach (vector goal : m_aGoals)
			best = Math.Min(best, vector.DistanceXZ(position, goal));
		return best;
	}

	protected bool ClearSegment(vector from, vector to, AIPathfindingComponent pathfinding)
	{
		if (AICF_ConstructionPlanner.SegmentIntersects(from, to, m_vBlockMin, m_vBlockMax))
			return false;
		vector hit;
		return pathfinding.RayTrace(from, to, hit);
	}

	int Step(AICF_ConstructionOrder order, AIPathfindingComponent pathfinding, int sliceMs)
	{
		if (!m_Pathfinding && m_aNodes.Count() == 1 && m_iExpanded == 0)
			m_Pathfinding = pathfinding;
		if (!m_Pathfinding || m_Pathfinding != pathfinding)
		{
			order.m_sReason = "NAVMESH_CONTEXT_CHANGED";
			return -1;
		}
		int started = System.GetTickCount();
		int transitions;
		while (transitions++ < 64 && System.GetTickCount() - started < sliceMs)
		{
			// Все запасные старты одного кандидата делят конечный бюджет.
			// После отказа planner продвигает candidate cursor, а не начинает
			// тот же дорогой обход в следующем order после общего deadline.
			if (order.m_iQueries - order.m_iPathQueriesAt >= MAX_QUERIES)
			{
				order.m_sReason = "WORKER_PATH_QUERY_LIMIT";
				return -1;
			}
			if (m_iActive < 0)
			{
				float best = float.MAX;
				foreach (int index, AICF_ConstructionPathNode candidate : m_aNodes)
				{
					float score = candidate.m_fCost + Heuristic(candidate.m_vPosition) * 1.4;
					if (!candidate.m_bClosed && score < best)
					{
						best = score;
						m_iActive = index;
					}
				}
				if (m_iActive < 0 || m_iExpanded >= MAX_NODES)
				{
					order.m_sReason = "WORKER_PATH_SEARCH_EXHAUSTED";
					return -1;
				}
				m_iExpanded++;
				m_iEdge = 0;
			}
			AICF_ConstructionPathNode node = m_aNodes[m_iActive];
			// Сначала пробуем все endpoints: один поиск обслуживает все стороны.
			if (m_iEdge < m_aGoals.Count())
			{
				if (!AICF_ConstructionSiteSearch.TakeQueries(order, 1))
					return 0;
				vector goal = m_aGoals[m_iEdge++];
				if (ClearSegment(node.m_vPosition, goal, pathfinding))
				{
					order.m_vWork = goal;
					order.m_sReason = "SITE_VALIDATED";
					order.Log("CONSTRUCTION_PATH_FOUND", Describe());
					return 1;
				}
				continue;
			}
			int direction = m_iEdge - m_aGoals.Count();
			if (direction >= 16)
			{
				node.m_bClosed = true;
				m_iActive = -1;
				continue;
			}
			if (!AICF_ConstructionSiteSearch.TakeQueries(order, 3))
				return 0;
			// Сначала длинные шаги, затем промежуточные точки узких проходов.
			int stride = 2;
			if (direction >= 8)
				stride = 1;
			int azimuth = direction % 8;
			int dx = Math.Round(Math.Sin(azimuth * Math.PI / 4)) * stride;
			int dz = Math.Round(Math.Cos(azimuth * Math.PI / 4)) * stride;
			int x = node.m_iX + dx;
			int z = node.m_iZ + dz;
			vector point = m_vOrigin + Vector(x * STEP, 0, z * STEP);
			if (point[0] < m_vMin[0] || point[0] > m_vMax[0] || point[2] < m_vMin[2] || point[2] > m_vMax[2])
			{
				m_iEdge++;
				continue;
			}
			NavmeshWorldComponent navmesh = pathfinding.GetNavmeshComponent();
			if (!navmesh.IsTileLoaded(point))
			{
				if (!navmesh.IsTileRequested(point))
					navmesh.LoadTileIn(point);
				if (++m_iTileRetries < 10)
					return 0;
				m_iTileRetries = 0;
				m_iEdge++;
				continue;
			}
			m_iTileRetries = 0;
			m_iEdge++;
			point[1] = GetGame().GetWorld().GetSurfaceY(point[0], point[2]);
			vector corrected;
			if (!pathfinding.GetClosestPositionOnNavmesh(point, "1.5 3 1.5", corrected) ||
				vector.DistanceSqXZ(node.m_vPosition, corrected) < 0.04 || !ClearSegment(node.m_vPosition, corrected, pathfinding))
				continue;
			float cost = node.m_fCost + vector.DistanceXZ(node.m_vPosition, corrected);
			AICF_ConstructionPathNode next = null;
			foreach (AICF_ConstructionPathNode existing : m_aNodes)
			{
				if (existing.m_iX == x && existing.m_iZ == z)
				{
					next = existing;
					break;
				}
			}
			if (next && (next.m_bClosed || next.m_fCost <= cost))
				continue;
			if (!next)
			{
				if (m_aNodes.Count() >= MAX_NODES)
					continue;
				next = new AICF_ConstructionPathNode();
				next.m_iX = x;
				next.m_iZ = z;
				m_aNodes.Insert(next);
			}
			next.m_fCost = cost;
			next.m_vPosition = corrected;
		}
		order.m_sReason = "WORKER_PATH_PENDING";
		return 0;
	}
}
