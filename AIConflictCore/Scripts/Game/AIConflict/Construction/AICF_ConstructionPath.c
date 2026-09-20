// Ограниченный A* по navmesh. Каждый переход подтверждён native RayTrace;
// отсутствие пути в пределах квоты не доказывает глобальную недостижимость.
// Состояние принадлежит одному candidate/order, callbacks и entities нет.
class AICF_ConstructionPathNode
{
	int m_iX;
	int m_iZ;
	vector m_vPosition;
	vector m_vRoot;
	float m_fCost;
	bool m_bClosed;
	int m_iParent = -1;
	ref AICF_ConstructionRoute m_Route;
}

class AICF_ConstructionPath
{
	static const float STEP = 2;
	static const int MAX_NODES = 512;
	static const int MAX_QUERIES = 512;
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
	protected int m_iRoots;
	protected int m_iDirectRoot;
	protected int m_iDirectGoal;
	protected ref AICF_ConstructionRoute m_Validate;
	protected vector m_vGoal;
	protected vector m_vChosenRoot;
	protected AICF_ConstructionNavigation m_Seeds;
	protected int m_iSeedCursor;
	protected int m_iSeedCount;
	protected int m_iSeedRevision;
	protected AIPathfindingComponent m_Pathfinding;
	protected bool m_bContextSet;

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
		node.m_vRoot = start;
		node.m_Route = new AICF_ConstructionRoute();
		node.m_Route.m_vPosition = start;
		node.m_Route.m_vRoot = start;
		m_aNodes.Insert(node);
	}

	void AddStarts(array<vector> starts)
	{
		foreach (vector start : starts)
		{
			if (vector.DistanceSqXZ(start, m_vOrigin) < 0.04)
				continue;
			AICF_ConstructionPathNode node = new AICF_ConstructionPathNode();
			node.m_iX = Math.Round((start[0] - m_vOrigin[0]) / STEP);
			node.m_iZ = Math.Round((start[2] - m_vOrigin[2]) / STEP);
			node.m_vPosition = start;
			node.m_vRoot = start;
			node.m_Route = new AICF_ConstructionRoute();
			node.m_Route.m_vPosition = start;
			node.m_Route.m_vRoot = start;
			m_aNodes.Insert(node);
		}
		m_iRoots = m_aNodes.Count();
	}

	void Seed(AICF_ConstructionNavigation navigation)
	{
		m_Seeds = navigation;
		m_iSeedCount = navigation.m_aReached.Count();
		m_iSeedRevision = navigation.m_iRevision;
	}

	void IncludeArea(vector center, float radius)
	{
		m_vMin[0] = Math.Min(m_vMin[0], center[0] - radius);
		m_vMin[2] = Math.Min(m_vMin[2], center[2] - radius);
		m_vMax[0] = Math.Max(m_vMax[0], center[0] + radius);
		m_vMax[2] = Math.Max(m_vMax[2], center[2] + radius);
	}

	protected void SeedNext()
	{
		AICF_ConstructionRoute route = m_Seeds.m_aReached[m_iSeedCursor++];
		if (m_aNodes.Count() >= MAX_NODES)
			return;
		// Не переносим путь сквозь новый unfinished footprint. Это дешёвая
		// проверка сохранённой цепочки; native freshness проверяется в конце.
		AICF_ConstructionRoute current = route;
		bool blocked;
		while (current && current.m_Previous)
		{
			if (AICF_ConstructionPlanner.SegmentIntersects(current.m_Previous.m_vPosition, current.m_vPosition, m_vBlockMin, m_vBlockMax))
			{
				blocked = true;
				break;
			}
			current = current.m_Previous;
		}
		if (blocked)
			return;
		int x = Math.Round((route.m_vPosition[0] - m_vOrigin[0]) / STEP);
		int z = Math.Round((route.m_vPosition[2] - m_vOrigin[2]) / STEP);
		bool duplicate;
		foreach (AICF_ConstructionPathNode existing : m_aNodes)
		{
			if (existing.m_iX == x && existing.m_iZ == z)
				duplicate = true;
		}
		if (duplicate)
			return;
		AICF_ConstructionPathNode node = new AICF_ConstructionPathNode();
		node.m_iX = x;
		node.m_iZ = z;
		node.m_vPosition = route.m_vPosition;
		node.m_vRoot = route.m_vRoot;
		node.m_fCost = route.m_fCost;
		node.m_Route = route;
		m_aNodes.Insert(node);
	}

	protected vector NearestGoal(vector position, int rank)
	{
		int first;
		int second = -1;
		for (int i = 1; i < m_aGoals.Count(); i++)
		{
			if (vector.DistanceSqXZ(position, m_aGoals[i]) < vector.DistanceSqXZ(position, m_aGoals[first]))
			{
				second = first;
				first = i;
			}
			else if (second < 0 || vector.DistanceSqXZ(position, m_aGoals[i]) < vector.DistanceSqXZ(position, m_aGoals[second]))
				second = i;
		}
		if (rank == 1 && second >= 0)
			return m_aGoals[second];
		return m_aGoals[first];
	}

	protected float Heuristic(vector position)
	{
		float best = float.MAX;
		foreach (vector goal : m_aGoals)
			best = Math.Min(best, vector.DistanceXZ(position, goal));
		return best;
	}

	float RemainingDistance()
	{
		float best = float.MAX;
		foreach (AICF_ConstructionPathNode node : m_aNodes)
			best = Math.Min(best, Heuristic(node.m_vPosition));
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
		if (m_Seeds && m_Seeds.m_iRevision != m_iSeedRevision)
		{
			order.m_sReason = "NAVMESH_CACHE_INVALIDATED";
			return -1;
		}
		if (!m_bContextSet)
		{
			m_Pathfinding = pathfinding;
			m_bContextSet = true;
		}
		if (!m_Pathfinding || m_Pathfinding != pathfinding)
		{
			order.m_sReason = "NAVMESH_CONTEXT_CHANGED";
			return -1;
		}
		int started = System.GetTickCount();
		int transitions;
		while (transitions++ < 64 && System.GetTickCount() - started < sliceMs)
		{
			if (m_iSeedCursor < m_iSeedCount)
			{
				SeedNext();
				continue;
			}
			// Одна итерация может выполнить projection (2) и edge (1).
			// Резерв не позволяет этой неделимой работе превысить лимит.
			if (order.m_iQueries - order.m_iPathQueriesAt >= MAX_QUERIES - 2)
			{
				order.m_sReason = "WORKER_PATH_QUERY_LIMIT";
				return -1;
			}
			if (m_Validate)
			{
				if (!m_Validate.m_Previous)
				{
					order.m_vPathStart = m_vChosenRoot;
					order.m_vWork = m_vGoal;
					order.m_sReason = "SITE_VALIDATED";
					order.m_sObstacle = "NONE";
					order.Log("CONSTRUCTION_PATH_FOUND", Describe() + " fresh_chain=1");
					return 1;
				}
				if (!AICF_ConstructionSiteSearch.TakeQueries(order, 1))
					return 0;
				if (!ClearSegment(m_Validate.m_Previous.m_vPosition, m_Validate.m_vPosition, pathfinding))
				{
					order.m_Navigation.Invalidate();
					order.m_sReason = "WORKER_PATH_CHANGED";
					return -1;
				}
				m_Validate = m_Validate.m_Previous;
				continue;
			}
			// Проверяем прямые пути всех стартов до расширения первого. Это
			// не позволяет близкому, но изолированному старту съесть весь бюджет.
			if (m_iDirectRoot < m_iRoots)
			{
				AICF_ConstructionPathNode root = m_aNodes[m_iDirectRoot];
				vector directGoal = NearestGoal(root.m_vPosition, m_iDirectGoal);
				if (!AICF_ConstructionPlanner.SegmentIntersects(root.m_vPosition, directGoal, m_vBlockMin, m_vBlockMax))
				{
					if (!AICF_ConstructionSiteSearch.TakeQueries(order, 1))
						return 0;
					if (ClearSegment(root.m_vPosition, directGoal, pathfinding))
					{
						order.m_vWork = directGoal;
						order.m_vPathStart = root.m_vRoot;
						order.m_sReason = "SITE_VALIDATED";
						order.m_sObstacle = "NONE";
						order.Log("CONSTRUCTION_PATH_FOUND", Describe() + " direct=1 fresh_chain=1");
						return 1;
					}
				}
				if (++m_iDirectGoal >= Math.Min(2, m_aGoals.Count()))
				{
					m_iDirectGoal = 0;
					m_iDirectRoot++;
				}
				continue;
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
			// Два ближайших endpoint каждой вершины; общий поиск обслуживает все стороны.
			int goalCount = Math.Min(2, m_aGoals.Count());
			// Дальние повторные прострелы к тем же endpoints не расширяют
			// достижимую область. Редкие shortcuts плюс полный тест вблизи цели.
			if (Heuristic(node.m_vPosition) > 12 && m_iExpanded % 8 != 1)
				goalCount = 0;
			if (m_iEdge < goalCount)
			{
				vector goal = NearestGoal(node.m_vPosition, m_iEdge);
				// Чистая геометрия не расходует native query. Такие отрезки
				// раньше отнимали квоту у реальных navmesh проверок других баз.
				if (AICF_ConstructionPlanner.SegmentIntersects(node.m_vPosition, goal, m_vBlockMin, m_vBlockMax))
				{
					m_iEdge++;
					order.m_iPathPruned++;
					continue;
				}
				if (!AICF_ConstructionSiteSearch.TakeQueries(order, 1))
					return 0;
				m_iEdge++;
				if (ClearSegment(node.m_vPosition, goal, pathfinding))
				{
					m_vGoal = goal;
					m_vChosenRoot = node.m_vRoot;
					m_Validate = node.m_Route;
				}
				continue;
			}
			int direction = m_iEdge - goalCount;
			if (direction >= 24 || node.m_Route.m_iDepth >= 64)
			{
				node.m_bClosed = true;
				m_iActive = -1;
				continue;
			}
			// Сначала длинные шаги, затем промежуточные точки узких проходов.
			int stride = 4;
			if (direction >= 8)
				stride = 2;
			if (direction >= 16)
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
			AICF_ConstructionPathNode next = null;
			foreach (AICF_ConstructionPathNode existing : m_aNodes)
			{
				if (existing.m_iX == x && existing.m_iZ == z)
				{
					next = existing;
					break;
				}
			}
			// Закрытый узел никогда не переоткрывался и раньше. Для открытого
			// повторно используем его navmesh позицию только в этом candidate.
			if (next && (next.m_bClosed || next.m_fCost <= node.m_fCost + vector.DistanceXZ(node.m_vPosition, next.m_vPosition)))
			{
				m_iEdge++;
				order.m_iPathPruned++;
				continue;
			}
			if (!next && m_aNodes.Count() >= MAX_NODES)
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
			vector corrected;
			if (next)
				corrected = next.m_vPosition;
			else
			{
				int projection = order.m_Navigation.Project(order, pathfinding, point, corrected);
				if (projection == 0)
					return 0;
				if (projection < 0)
				{
					m_iEdge++;
					continue;
				}
			}
			if (vector.DistanceSqXZ(node.m_vPosition, corrected) < 0.04 ||
				AICF_ConstructionPlanner.SegmentIntersects(node.m_vPosition, corrected, m_vBlockMin, m_vBlockMax))
			{
				m_iEdge++;
				order.m_iPathPruned++;
				continue;
			}
			int edge = order.m_Navigation.Edge(order, pathfinding, node.m_vPosition, corrected);
			if (edge == 0)
				return 0;
			m_iEdge++;
			if (edge < 0)
				continue;
			float cost = node.m_fCost + vector.DistanceXZ(node.m_vPosition, corrected);
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
			next.m_vRoot = node.m_vRoot;
			next.m_iParent = m_iActive;
			next.m_Route = new AICF_ConstructionRoute();
			next.m_Route.m_vPosition = corrected;
			next.m_Route.m_vRoot = node.m_vRoot;
			next.m_Route.m_fCost = cost;
			next.m_Route.m_Previous = node.m_Route;
			next.m_Route.m_iDepth = node.m_Route.m_iDepth + 1;
			order.m_Navigation.Remember(next.m_Route);
		}
		order.m_iPathSteps++;
		order.m_sReason = "WORKER_PATH_PENDING";
		return 0;
	}
}
