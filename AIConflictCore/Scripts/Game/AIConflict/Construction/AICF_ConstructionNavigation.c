// Кэш принадлежит одному order и одному navmesh context. Геометрия будущего
// layout проверяется отдельно. Положительные рёбра — только подсказка поиска:
// найденная цепочка целиком повторяет native RayTrace перед выбором площадки.
modded class SCR_SpawnPoint
{
	// Read-only authored positions вместо GetPositionAndRotation, который
	// выбирает случайный marker и запускает скрытые FindEmptyTerrain queries.
	void AICF_ConstructionPositions(array<vector> positions)
	{
		array<IEntity> sources = {};
		if (m_bUseNearbySpawnPositions && !m_bForcedPosition)
		{
			SCR_SpawnPositionComponentManager manager = SCR_SpawnPositionComponentManager.GetInstance(false);
			array<SCR_SpawnPositionComponent> nearby = {};
			if (manager)
				manager.GetSpawnPositionsInRange(GetOrigin(), m_fSpawnPositionUsageRange, nearby);
			foreach (SCR_SpawnPositionComponent point : nearby)
			{
				if (point && point.GetOwner())
					sources.Insert(point.GetOwner());
			}
		}
		if (!m_bForcedPosition)
		{
			foreach (SCR_Position child : m_aChildren)
			{
				if (child)
					sources.Insert(child);
			}
		}
		for (int i = 1; i < sources.Count(); i++)
		{
			IEntity value = sources[i];
			int j = i - 1;
			while (j >= 0 && sources[j].GetID().ToString().Compare(value.GetID().ToString()) > 0)
			{
				sources[j + 1] = sources[j];
				j--;
			}
			sources[j + 1] = value;
		}
		foreach (IEntity source : sources)
		{
			if (positions.Count() >= 16)
				break;
			positions.Insert(source.GetOrigin());
		}
		positions.Insert(GetOrigin());
		for (int direction; direction < 8; direction++)
		{
			float angle = direction * Math.PI / 4;
			positions.Insert(GetOrigin() + Vector(Math.Sin(angle), 0, Math.Cos(angle)) * 6);
		}
	}
}

class AICF_ConstructionRoute
{
	vector m_vPosition;
	vector m_vRoot;
	float m_fCost;
	int m_iDepth;
	ref AICF_ConstructionRoute m_Previous;
}

class AICF_ConstructionNavSample
{
	vector m_vPosition;
	bool m_bValid;
	int m_iAt;
}

class AICF_ConstructionNavEdge
{
	bool m_bClear;
	int m_iAt;
}

class AICF_ConstructionNavigation
{
	ref array<ref AICF_ConstructionRoute> m_aReached = {};
	int m_iRevision;
	protected ref map<string, ref AICF_ConstructionNavSample> m_mSamples = new map<string, ref AICF_ConstructionNavSample>();
	protected ref map<string, ref AICF_ConstructionNavEdge> m_mEdges = new map<string, ref AICF_ConstructionNavEdge>();

	// -1 нет navmesh, 0 ждать квоты, 1 projected.
	int Project(AICF_ConstructionOrder order, AIPathfindingComponent pathfinding, vector point, out vector corrected)
	{
		string key = point.ToString();
		AICF_ConstructionNavSample sample = m_mSamples.Get(key);
		int now = System.GetTickCount();
		if (sample && now - sample.m_iAt < 30000)
		{
			order.m_iNavCacheHits++;
			corrected = sample.m_vPosition;
			if (sample.m_bValid)
				return 1;
			return -1;
		}
		if (!AICF_ConstructionSiteSearch.TakeQueries(order, 2))
			return 0;
		point[1] = GetGame().GetWorld().GetSurfaceY(point[0], point[2]);
		sample = new AICF_ConstructionNavSample();
		sample.m_bValid = pathfinding.GetClosestPositionOnNavmesh(point, "1.5 3 1.5", sample.m_vPosition);
		sample.m_iAt = now;
		if (m_mSamples.Count() >= 2048)
			m_mSamples.Clear();
		m_mSamples.Set(key, sample);
		corrected = sample.m_vPosition;
		if (sample.m_bValid)
			return 1;
		return -1;
	}

	// -1 blocked, 0 ждать квоты, 1 clear. Направления не смешиваются.
	int Edge(AICF_ConstructionOrder order, AIPathfindingComponent pathfinding, vector from, vector to)
	{
		string key = from.ToString() + ":" + to.ToString();
		AICF_ConstructionNavEdge edge = m_mEdges.Get(key);
		int now = System.GetTickCount();
		if (edge && now - edge.m_iAt < 30000)
		{
			order.m_iNavCacheHits++;
			if (edge.m_bClear)
				return 1;
			return -1;
		}
		if (!AICF_ConstructionSiteSearch.TakeQueries(order, 1))
			return 0;
		edge = new AICF_ConstructionNavEdge();
		vector hit;
		edge.m_bClear = pathfinding.RayTrace(from, to, hit);
		edge.m_iAt = now;
		if (m_mEdges.Count() >= 4096)
			m_mEdges.Clear();
		m_mEdges.Set(key, edge);
		if (edge.m_bClear)
			return 1;
		return -1;
	}

	void Invalidate()
	{
		m_iRevision++;
		m_mSamples.Clear();
		m_mEdges.Clear();
		m_aReached.Clear();
	}

	void Remember(AICF_ConstructionRoute route)
	{
		if (!route || route.m_iDepth > 64)
			return;
		foreach (int i, AICF_ConstructionRoute reached : m_aReached)
		{
			if (vector.DistanceSqXZ(reached.m_vPosition, route.m_vPosition) > 0.04)
				continue;
			if (route.m_fCost < reached.m_fCost)
				m_aReached[i] = route;
			return;
		}
		if (m_aReached.Count() < 512)
			m_aReached.Insert(route);
	}
}
