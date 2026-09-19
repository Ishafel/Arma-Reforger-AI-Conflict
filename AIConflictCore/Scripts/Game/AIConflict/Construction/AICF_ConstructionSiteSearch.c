// Общая квота geometry queries, включая live commit и completion. Никаких
// gameplay entities для примерки; terrain sampling продолжается со своего cursor.
class AICF_ConstructionSiteSearch
{
	protected static int s_iWindow;
	protected static int s_iQueries;
	protected static int s_iLimit = 96;
	protected ref AICF_ConstructionConfig m_Config;
	protected IEntity m_ExcludedRoot;
	protected vector m_vQueryMin;
	protected vector m_vQueryMax;
	protected bool m_bBlocked;
	protected string m_sObstacle;
	protected AICF_ConstructionOrder m_QueryOrder;
	protected vector m_aQueryTransform[4];
	protected vector m_vLocalQueryMin;
	protected vector m_vLocalQueryMax;

	void AICF_ConstructionSiteSearch(AICF_ConstructionConfig config)
	{
		m_Config = config;
		s_iLimit = config.m_iQueriesPerTick;
	}

	static bool TakeQueries(AICF_ConstructionOrder order, int count, bool charge = true)
	{
		int window = System.GetTickCount() / 1000;
		if (s_iWindow != window)
		{
			s_iWindow = window;
			s_iQueries = 0;
		}
		if (s_iQueries + count > s_iLimit)
		{
			order.m_sReason = "QUERY_BUDGET";
			return false;
		}
		if (charge)
		{
			s_iQueries += count;
			order.m_iQueries += count;
		}
		return true;
	}

	static void Bounds(AICF_ConstructionOrder order, vector transform[4], float margin, out vector mins, out vector maxs)
	{
		TransformBounds(order.m_Metadata.m_vMin, order.m_Metadata.m_vMax, transform, margin, mins, maxs);
	}

	static void TransformBounds(vector localMin, vector localMax, vector transform[4], float margin, out vector mins, out vector maxs)
	{
		mins = Vector(float.MAX, float.MAX, float.MAX);
		maxs = Vector(-float.MAX, -float.MAX, -float.MAX);
		for (int corner; corner < 8; corner++)
		{
			vector point = localMin;
			for (int axis; axis < 3; axis++)
			{
				if (corner & (1 << axis))
					point[axis] = localMax[axis];
			}
			point = transform[3] + transform[0] * point[0] + transform[1] * point[1] + transform[2] * point[2];
			for (int axis; axis < 3; axis++)
			{
				mins[axis] = Math.Min(mins[axis], point[axis]);
				maxs[axis] = Math.Max(maxs[axis], point[axis]);
			}
		}
		mins -= Vector(margin, 0, margin);
		maxs += Vector(margin, 1, margin);
	}

	bool BeginCandidate(AICF_ConstructionOrder order)
	{
		if (!TakeQueries(order, 1))
			return false;
		int attempt = order.m_iSearchOffset + order.m_iAttempts++;
		float extent = vector.DistanceXZ(order.m_Metadata.m_vMin, order.m_Metadata.m_vMax) * 0.5 + m_Config.m_fMargin;
		float outer = Math.Max(0, order.m_Provider.GetBuildingRadius() - extent);
		vector offset = CandidateOffset(attempt, extent, outer, order.m_fYaw);
		Math3D.AnglesToMatrix(Vector(order.m_fYaw, 0, 0), order.m_aTransform);
		// Кандидат задаёт центр footprint, а не произвольный prefab pivot.
		vector center = (order.m_Metadata.m_vMin + order.m_Metadata.m_vMax) * 0.5;
		order.m_aTransform[3] = order.m_vProviderPosition + offset - order.m_aTransform[0] * center[0] - order.m_aTransform[2] * center[2];
		order.m_aTransform[3][1] = GetGame().GetWorld().GetSurfaceY(order.m_aTransform[3][0], order.m_aTransform[3][2]);
		Bounds(order, order.m_aTransform, m_Config.m_fMargin, order.m_vMin, order.m_vMax);
		order.m_fMinHeight = order.m_aTransform[3][1];
		order.m_fMaxHeight = order.m_fMinHeight;
		order.m_iSample = 0;
		order.m_aTerrainHeights.Clear();
		order.m_iNavRetry = 0;
		order.m_iNavPathCursor = 0;
		order.m_Path = null;
		order.m_aWorkCandidates.Clear();
		order.m_bPathStartSampled = false;
		order.m_bPathStartReady = false;
		order.m_iPathStartOption = 0;
		order.m_iPathQueriesAt = -1;
		order.m_iExitOption = 0;
		order.m_iExitSample = 0;
		order.m_aExits.Clear();
		order.m_aExitHeights.Clear();
		order.m_sObstacle = "NONE";
		order.m_sReason = "CANDIDATE";
		if (!InsideBounds(order) || !LiveClear(order, null))
			return false;
		order.m_iStage = 1;
		return true;
	}

	static vector CandidateOffset(int attempt, float extent, float outer, out float yaw)
	{
		// Halton покрывает площадь диска: каждая попытка исследует новый центр.
		// Порядок не зависит от размера order; cursor продолжает ту же выборку.
		yaw = Math.Floor(Fraction((attempt + 1) * 0.61803399) * 8) * 45;
		float radial = RadicalInverse(attempt + 1, 2);
		float angle = RadicalInverse(attempt + 1, 3) * Math.PI2;
		float inner = Math.Min(8, outer);
		float radius = Math.Sqrt(Math.Lerp(inner * inner, outer * outer, radial));
		return Vector(Math.Sin(angle) * radius, 0, Math.Cos(angle) * radius);
	}

	protected static float RadicalInverse(int index, int radix)
	{
		float result;
		float factor = 1.0 / radix;
		while (index > 0)
		{
			int digit = index % radix;
			result += digit * factor;
			index = index / radix;
			factor /= radix;
		}
		return result;
	}

	protected static float Fraction(float value)
	{
		return value - Math.Floor(value);
	}

	protected bool InsideBounds(AICF_ConstructionOrder order)
	{
		vector margin = Vector(m_Config.m_fMargin, 0, m_Config.m_fMargin);
		return InsideVolume(order, order.m_Metadata.m_vMin - margin, order.m_Metadata.m_vMax + margin);
	}

	protected bool InsideVolume(AICF_ConstructionOrder order, vector mins, vector maxs)
	{
		if (!order.m_Provider || !order.m_Provider.GetOwner())
		{
			order.m_sReason = "PROVIDER_UNAVAILABLE";
			return false;
		}
		GenericWorldEntity worldEntity = GetGame().GetWorldEntity();
		GenericTerrainEntity terrain;
		if (worldEntity)
			terrain = worldEntity.GetTerrain(0, 0);
		if (!terrain)
			return false;
		vector worldMin, worldMax;
		terrain.GetTerrainBoundBox(worldMin, worldMax);
		if (VolumeInsideBounds(mins, maxs, order.m_aTransform, worldMin, worldMax, order.m_Provider.GetOwner().GetOrigin(), order.m_Provider.GetBuildingRadius()))
			return true;
		order.m_sReason = "OUTSIDE_BUILDING_OR_WORLD_BOUNDS";
		return false;
	}

	static bool VolumeInsideBounds(vector mins, vector maxs, vector transform[4], vector worldMin, vector worldMax, vector providerPosition, float radius)
	{
		for (int corner; corner < 4; corner++)
		{
			vector point = mins;
			if (corner & 1)
				point[0] = maxs[0];
			if (corner & 2)
				point[2] = maxs[2];
			// Проверяем углы повернутого объёма, а не пустые углы world AABB.
			point = transform[3] + transform[0] * point[0] + transform[2] * point[2];
			if (point[0] < worldMin[0] || point[0] > worldMax[0] || point[2] < worldMin[2] || point[2] > worldMax[2] ||
				vector.DistanceSqXZ(point, providerPosition) > radius * radius)
			{
				return false;
			}
		}
		return true;
	}

	// -1 отказ, 0 pending, 1 полноценная площадка.
	int Step(AICF_ConstructionOrder order, AIPathfindingComponent pathfinding)
	{
		if (order.m_iStage == 4)
			return 1;
		// Terrain grid следует локальному footprint; пустые углы world AABB и
		// внешняя полоса отступа не являются фундаментом.
		vector localMin = order.m_Metadata.m_vMin;
		vector localMax = order.m_Metadata.m_vMax;
		int columns = Math.Ceil((localMax[0] - localMin[0]) / m_Config.m_fTerrainStep) + 1;
		int rows = Math.Ceil((localMax[2] - localMin[2]) / m_Config.m_fTerrainStep) + 1;
		if (columns * rows > 1600)
		{
			order.m_sReason = "FOOTPRINT_SAMPLE_LIMIT";
			return -1;
		}
		BaseWorld world = GetGame().GetWorld();
		int samples;
		int sliceStarted = System.GetTickCount();
		while (order.m_iStage == 1 && order.m_iSample < columns * rows && samples++ < 32 && System.GetTickCount() - sliceStarted < m_Config.m_iSliceMs)
		{
			if (!TakeQueries(order, 2))
				return 0;
			int sample = order.m_iSample++;
			int column = sample % columns;
			int row = sample / columns;
			vector local = Vector(Math.Lerp(localMin[0], localMax[0], column / (columns - 1.0)), 0,
				Math.Lerp(localMin[2], localMax[2], row / (rows - 1.0)));
			vector point = order.m_aTransform[3] + order.m_aTransform[0] * local[0] + order.m_aTransform[2] * local[2];
			point[1] = world.GetSurfaceY(point[0], point[2]);
			order.m_fMinHeight = Math.Min(order.m_fMinHeight, point[1]);
			order.m_fMaxHeight = Math.Max(order.m_fMaxHeight, point[1]);
			if (point[1] <= world.GetOceanBaseHeight() + 0.2 || ChimeraWorldUtils.TryGetWaterSurfaceSimple(world, point))
			{
				order.m_sReason = "WATER";
				return -1;
			}
			// Stock completion применяет TERRAIN к дочерним объектам композиции.
			// Ограничиваем местный уклон, а не перепад между дальними краями:
			// длинный пологий участок не является разрывом фундамента.
			bool supported = true;
			if (column > 0)
				supported = TerrainEdgeSupported(point[1], order.m_aTerrainHeights[sample - 1], (localMax[0] - localMin[0]) / (columns - 1), m_Config.m_fHeightDelta, m_Config.m_fTerrainStep);
			if (row > 0)
				supported = supported && TerrainEdgeSupported(point[1], order.m_aTerrainHeights[sample - columns], (localMax[2] - localMin[2]) / (rows - 1), m_Config.m_fHeightDelta, m_Config.m_fTerrainStep);
			order.m_aTerrainHeights.Insert(point[1]);
			if (!supported)
			{
				order.m_sReason = "SLOPE_OR_FOUNDATION_GAP";
				return -1;
			}
		}
		if (order.m_iSample < columns * rows)
			return 0;
		if (order.m_iStage == 1)
			order.m_iStage = 2;
		if (order.m_iStage == 2)
		{
			int exits = ValidateExits(order);
			if (exits <= 0)
				return exits;
			order.m_iStage = 3;
		}
		// Проверенный endpoint затем без пересчёта использует worker.
		return ValidatePath(order, pathfinding);
	}

	static bool TerrainEdgeSupported(float first, float second, float distance, float heightDelta, float referenceStep)
	{
		if (distance <= 0 || referenceStep <= 0)
			return false;
		return Math.AbsFloat(first - second) <= heightDelta * distance / referenceStep;
	}

	protected int ValidateExits(AICF_ConstructionOrder order)
	{
		int slot = order.m_aExits.Count();
		if (slot >= order.m_Metadata.m_aExitPairs.Count())
			return 1;
		if (order.m_iExitOption >= 2)
		{
			order.m_sReason = "DEPOT_EXIT_BLOCKED";
			return -1;
		}
		AICF_ConstructionExitPair pair = order.m_Metadata.m_aExitPairs[slot];
		AICF_ConstructionVolume volume = pair.m_Forward;
		if (order.m_iExitOption == 1)
			volume = pair.m_Backward;
		if (!order.m_Metadata.ExitAvoidsComposition(volume))
		{
			order.m_iExitOption++;
			return 0;
		}
		int columns = Math.Ceil((volume.m_vMax[0] - volume.m_vMin[0]) / 3) + 1;
		int rows = Math.Ceil((volume.m_vMax[2] - volume.m_vMin[2]) / 3) + 1;
		if (columns * rows > 400)
		{
			order.m_sReason = "DEPOT_EXIT_SAMPLE_LIMIT";
			return -1;
		}
		int started = System.GetTickCount();
		BaseWorld world = GetGame().GetWorld();
		for (int batch; order.m_iExitSample < columns * rows && batch < 32 && System.GetTickCount() - started < m_Config.m_iSliceMs; batch++)
		{
			if (!TakeQueries(order, 2))
				return 0;
			int sample = order.m_iExitSample++;
			int column = sample % columns;
			int row = sample / columns;
			vector local = Vector(Math.Lerp(volume.m_vMin[0], volume.m_vMax[0], column / (columns - 1.0)), 0,
				Math.Lerp(volume.m_vMin[2], volume.m_vMax[2], row / (rows - 1.0)));
			vector point = order.m_aTransform[3] + order.m_aTransform[0] * local[0] + order.m_aTransform[2] * local[2];
			point[1] = world.GetSurfaceY(point[0], point[2]);
			if (sample == 0)
			{
				order.m_fExitMinHeight = point[1];
				order.m_fExitMaxHeight = point[1];
			}
			order.m_fExitMinHeight = Math.Min(order.m_fExitMinHeight, point[1]);
			order.m_fExitMaxHeight = Math.Max(order.m_fExitMaxHeight, point[1]);
			bool steep = false;
			if (column > 0 && Math.AbsFloat(point[1] - order.m_aExitHeights[sample - 1]) >
				(volume.m_vMax[0] - volume.m_vMin[0]) / (columns - 1) * 0.25)
				steep = true;
			if (row > 0 && Math.AbsFloat(point[1] - order.m_aExitHeights[sample - columns]) >
				(volume.m_vMax[2] - volume.m_vMin[2]) / (rows - 1) * 0.25)
				steep = true;
			order.m_aExitHeights.Insert(point[1]);
			if (point[1] <= world.GetOceanBaseHeight() + 0.2 || ChimeraWorldUtils.TryGetWaterSurfaceSimple(world, point) ||
				steep || order.m_fExitMaxHeight - order.m_fExitMinHeight > 2)
			{
				order.m_iExitOption++;
				order.m_iExitSample = 0;
				order.m_aExitHeights.Clear();
				return 0;
			}
		}
		if (order.m_iExitSample < columns * rows)
			return 0;
		AICF_ConstructionVolume selected = new AICF_ConstructionVolume();
		selected.m_vMin = volume.m_vMin;
		selected.m_vMax = volume.m_vMax;
		selected.m_vMin[1] = order.m_fExitMinHeight - order.m_aTransform[3][1] + 0.3;
		selected.m_vMax[1] = order.m_fExitMaxHeight - order.m_aTransform[3][1] + 4.1;
		if (!ClearExit(order, selected, null))
		{
			if (order.m_sReason == "QUERY_BUDGET")
				return 0;
			order.m_iExitOption++;
			order.m_iExitSample = 0;
			order.m_aExitHeights.Clear();
			return 0;
		}
		order.m_aExits.Insert(selected);
		order.m_iExitOption = 0;
		order.m_iExitSample = 0;
		order.m_aExitHeights.Clear();
		return 0;
	}

	int ValidatePath(AICF_ConstructionOrder order, AIPathfindingComponent pathfinding)
	{
		if (order.m_iPathQueriesAt < 0)
			order.m_iPathQueriesAt = order.m_iQueries;
		if (order.m_iQueries - order.m_iPathQueriesAt >= AICF_ConstructionPath.MAX_QUERIES)
		{
			order.m_Path = null;
			order.m_sReason = "WORKER_PATH_QUERY_LIMIT";
			return -1;
		}
		if (!pathfinding || !pathfinding.GetNavmeshComponent() || !order.m_Base.GetSpawnPoint())
		{
			order.m_sReason = "NAVMESH_UNAVAILABLE";
			return -1;
		}
		if (order.m_Path)
		{
			int result = order.m_Path.Step(order, pathfinding, m_Config.m_iSliceMs);
			if (result != 0)
				order.m_Path = null;
			if (result < 0 && order.m_sReason == "WORKER_PATH_SEARCH_EXHAUSTED")
				return NextPathStart(order);
			return result;
		}
		vector mins, maxs;
		WorkBounds(order, mins, maxs);
		if (!TakeQueries(order, 2))
			return 0;
		if (!order.m_bPathStartSampled)
		{
			vector rotation;
			order.m_Base.GetSpawnPoint().GetPositionAndRotation(order.m_vSpawnOrigin, rotation);
			order.m_bPathStartSampled = true;
		}
		vector start = order.m_vSpawnOrigin;
		if (order.m_iPathStartOption > 0)
		{
			float angle = (order.m_iPathStartOption - 1) * Math.PI / 4;
			start += Vector(Math.Sin(angle), 0, Math.Cos(angle)) * 6;
			start[1] = GetGame().GetWorld().GetSurfaceY(start[0], start[2]);
		}
		NavmeshWorldComponent navmesh = pathfinding.GetNavmeshComponent();
		if (!navmesh.IsTileLoaded(start) || !navmesh.IsTileLoaded(order.m_aTransform[3]))
		{
			if (order.m_iNavRetry++ >= 10)
			{
				order.m_sReason = "NAVMESH_TILE_TIMEOUT";
				return -1;
			}
			if (!navmesh.IsTileRequested(start))
				navmesh.LoadTileIn(start);
			if (!navmesh.IsTileRequested(order.m_aTransform[3]))
				navmesh.LoadTileIn(order.m_aTransform[3]);
			order.m_sReason = "NAVMESH_TILE_LOADING";
			return 0;
		}
		if (!order.m_bPathStartReady && !pathfinding.GetClosestPositionOnNavmesh(start, "1 3 1", order.m_vPathStart))
		{
			order.m_sReason = "WORKER_START_OFF_NAVMESH";
			return NextPathStart(order);
		}
		order.m_bPathStartReady = true;
		// Контур unfinished layout, а не общий AABB всех будущих объектов.
		// Три позиции на каждой стороне и четыре внешних fallback endpoints.
		while (order.m_iNavPathCursor < 16)
		{
			if (!TakeQueries(order, 2))
				return 0;
			int side = order.m_iNavPathCursor % 4;
			int option = order.m_iNavPathCursor / 4;
			order.m_iNavPathCursor++;
			if (option == 3)
			{
				mins = order.m_vMin;
				maxs = order.m_vMax;
			}
			float x = Math.Clamp(order.m_vPathStart[0], mins[0], maxs[0]);
			float z = Math.Clamp(order.m_vPathStart[2], mins[2], maxs[2]);
			if (option > 0 && option < 3)
			{
				x = Math.Lerp(mins[0], maxs[0], option * 0.333333);
				z = Math.Lerp(mins[2], maxs[2], option * 0.333333);
			}
			vector work;
			switch (side)
			{
				case 0: work = Vector(mins[0] - 2, 0, z); break;
				case 1: work = Vector(maxs[0] + 2, 0, z); break;
				case 2: work = Vector(x, 0, mins[2] - 2); break;
				case 3: work = Vector(x, 0, maxs[2] + 2); break;
			}
			work[1] = GetGame().GetWorld().GetSurfaceY(work[0], work[2]);
			vector endpoint;
			if (!pathfinding.GetClosestPositionOnNavmesh(work, "0.5 2 0.5", endpoint) || vector.DistanceSqXZ(work, endpoint) > 0.25)
				continue;
			if (!WorkClearOfExits(order, endpoint) || !WorkClearOfSolids(order, endpoint))
				continue;
			order.m_aWorkCandidates.Insert(endpoint);
		}
		if (order.m_aWorkCandidates.IsEmpty())
		{
			order.m_sReason = "WORKER_ENDPOINT_UNREACHABLE";
			return -1;
		}
		order.m_Path = new AICF_ConstructionPath();
		WorkBounds(order, mins, maxs);
		order.m_Path.Begin(order.m_vPathStart, order.m_aWorkCandidates, mins - "0.5 0 0.5", maxs + "0.5 0 0.5");
		return 0;
	}

	protected int NextPathStart(AICF_ConstructionOrder order)
	{
		// Group spawn умеет смещать персонажа от stock marker. Выбираем рядом
		// реальную navmesh позицию; builder затем получает именно этот старт.
		if (++order.m_iPathStartOption >= 9)
			return -1;
		order.m_Path = null;
		order.m_bPathStartReady = false;
		order.m_iNavPathCursor = 0;
		order.m_aWorkCandidates.Clear();
		order.m_sReason = "WORKER_START_RETRY";
		return 0;
	}

	static void WorkBounds(AICF_ConstructionOrder order, out vector mins, out vector maxs)
	{
		if (order.m_Metadata.m_bOutlineBounds)
			TransformBounds(order.m_Metadata.m_vOutlineMin, order.m_Metadata.m_vOutlineMax, order.m_aTransform, 0, mins, maxs);
		else
			Bounds(order, order.m_aTransform, 0, mins, maxs);
	}

	static bool WorkClearOfSolids(AICF_ConstructionOrder order, vector endpoint)
	{
		vector local = LocalPoint(order, endpoint);
		// Та же геометрия, что у completion TracePosition: worker не должен
		// заблокировать завершение своим телом. Включает margin + arrival/body.
		foreach (AICF_ConstructionVolume solid : order.m_Metadata.m_aCollisionVolumes)
		{
			if (local[0] >= solid.m_vMin[0] - 2.5 && local[0] <= solid.m_vMax[0] + 2.5 &&
				local[2] >= solid.m_vMin[2] - 2.5 && local[2] <= solid.m_vMax[2] + 2.5)
				return false;
		}
		return true;
	}
	bool LiveClear(AICF_ConstructionOrder order, IEntity excludedRoot)
	{
		// Проверяем наличие всей квоты до синхронной live-проверки. Списываются
		// только фактические queries; ранний blocker не расходует квоту остальных OBB.
		if (!TakeQueries(order, 2 + order.m_Metadata.m_aCollisionVolumes.Count() + order.m_aExits.Count() * 2, false))
			return false;
		TakeQueries(order, 2);
		m_ExcludedRoot = excludedRoot;
		m_bBlocked = false;
		m_sObstacle = "NONE";
		m_vQueryMin = order.m_vMin;
		m_vQueryMax = order.m_vMax;
		// При TERRAIN привязке части композиции меняют высоту. Консервативно
		// сохраняем и исходный объём, и весь измеренный диапазон сдвига.
		float lowerShift = Math.Min(0, order.m_fMinHeight - order.m_aTransform[3][1]);
		float upperShift = Math.Max(0, order.m_fMaxHeight - order.m_aTransform[3][1]);
		m_vQueryMin[1] = m_vQueryMin[1] + lowerShift;
		m_vQueryMax[1] = m_vQueryMax[1] + upperShift;
		m_QueryOrder = order;
		Math3D.MatrixCopy(order.m_aTransform, m_aQueryTransform);
		m_vLocalQueryMin = order.m_Metadata.m_vMin - Vector(m_Config.m_fMargin, 0, m_Config.m_fMargin);
		m_vLocalQueryMax = order.m_Metadata.m_vMax + Vector(m_Config.m_fMargin, 1, m_Config.m_fMargin);
		BaseWorld world = GetGame().GetWorld();
		vector transform[4];
		Math3D.MatrixIdentity4(transform);
		world.QueryEntitiesByOBB(m_vQueryMin, m_vQueryMax, transform, CheckEntity, null, EQueryEntitiesFlags.ALL);
		if (m_bBlocked || !AICF_ConstructionPlanner.SpatialClear(order, m_vQueryMin, m_vQueryMax))
		{
			order.m_sReason = "OBSTACLE_OR_RESERVED_SITE";
			order.m_sObstacle = m_sObstacle;
			return false;
		}
		ChimeraAIWorld aiWorld = ChimeraAIWorld.Cast(GetGame().GetAIWorld());
		if (!aiWorld || !aiWorld.GetRoadNetworkManager())
			return false;
		array<BaseRoad> roads = {};
		aiWorld.GetRoadNetworkManager().GetRoadsInAABB(m_vQueryMin, m_vQueryMax, roads);
		int segments;
		int roadChecks;
		foreach (BaseRoad road : roads)
		{
			array<vector> points = {};
			road.GetPoints(points);
			float clearance = road.GetWidth() * 0.5 + 2;
			for (int i = 1; i < points.Count(); i++)
			{
				float roadMargin = clearance + m_Config.m_fMargin;
				if (++segments > 2048)
				{
					order.m_sReason = "ROAD_GEOMETRY_LIMIT";
					return false;
				}
				vector start = LocalPoint(order, points[i - 1]);
				vector end = LocalPoint(order, points[i]);
				vector buffer = Vector(roadMargin, 0, roadMargin);
				if (!AICF_ConstructionPlanner.SegmentIntersects(start, end, order.m_Metadata.m_vMin - buffer, order.m_Metadata.m_vMax + buffer))
					continue;
				// Дорогу перекрывает геометрия, а не пустой угол composition AABB
				// или площадь перед vehicle slot. Проверяем полные дочерние meshes.
				foreach (AICF_ConstructionVolume solid : order.m_Metadata.m_aSolids)
				{
					if (++roadChecks > 4096)
					{
						order.m_sReason = "ROAD_GEOMETRY_LIMIT";
						return false;
					}
					if (AICF_ConstructionPlanner.SegmentIntersects(start, end, solid.m_vMin - buffer, solid.m_vMax + buffer))
					{
						order.m_sReason = "ROAD_OR_ACCESS_CORRIDOR";
						return false;
					}
				}
			}
		}
		TraceOBB trace = new TraceOBB();
		for (int axis; axis < 3; axis++)
			trace.Mat[axis] = order.m_aTransform[axis];
		trace.Start = order.m_aTransform[3];
		// WORLD означает terrain (Script Diff): рельеф проверяет отдельная сетка.
		// Здесь остаётся вся blocking entity geometry, включая статические rocks.
		trace.Flags = TraceFlags.ENTS;
		trace.LayerMask = EPhysicsLayerPresets.Projectile;
		foreach (AICF_ConstructionVolume volume : order.m_Metadata.m_aCollisionVolumes)
		{
			TakeQueries(order, 1);
			trace.Mins = volume.m_vMin - Vector(m_Config.m_fMargin, 0, m_Config.m_fMargin);
			trace.Maxs = volume.m_vMax + Vector(m_Config.m_fMargin, 0.1, m_Config.m_fMargin);
			trace.Mins[1] = trace.Mins[1] + lowerShift;
			trace.Maxs[1] = trace.Maxs[1] + upperShift;
			if (world.TracePosition(trace, TraceEntity) < 0)
			{
				order.m_sReason = "PHYSICAL_OBSTRUCTION";
				order.m_sObstacle = Describe(trace.TraceEnt);
				return false;
			}
		}
		foreach (AICF_ConstructionVolume exitVolume : order.m_aExits)
		{
			if (!ClearExit(order, exitVolume, excludedRoot))
				return false;
		}
		return true;
	}

	protected bool ClearExit(AICF_ConstructionOrder order, AICF_ConstructionVolume volume, IEntity excludedRoot)
	{
		if (!TakeQueries(order, 2))
			return false;
		m_ExcludedRoot = excludedRoot;
		m_bBlocked = false;
		vector mins, maxs;
		TransformBounds(volume.m_vMin, volume.m_vMax, order.m_aTransform, 0, mins, maxs);
		if (!InsideVolume(order, volume.m_vMin, volume.m_vMax))
			return false;
		m_QueryOrder = order;
		m_vQueryMin = mins;
		m_vQueryMax = maxs;
		Math3D.MatrixCopy(order.m_aTransform, m_aQueryTransform);
		m_vLocalQueryMin = volume.m_vMin;
		m_vLocalQueryMax = volume.m_vMax;
		BaseWorld world = GetGame().GetWorld();
		vector identity[4];
		Math3D.MatrixIdentity4(identity);
		world.QueryEntitiesByOBB(mins, maxs, identity, CheckEntity, null, EQueryEntitiesFlags.ALL);
		TraceOBB trace = new TraceOBB();
		for (int axis; axis < 3; axis++)
			trace.Mat[axis] = order.m_aTransform[axis];
		trace.Start = order.m_aTransform[3];
		trace.Mins = volume.m_vMin;
		trace.Maxs = volume.m_vMax;
		trace.Flags = TraceFlags.ENTS;
		trace.LayerMask = EPhysicsLayerPresets.Projectile;
		if (m_bBlocked || !AICF_ConstructionPlanner.SpatialClear(order, mins, maxs) || world.TracePosition(trace, TraceEntity) < 0)
		{
			order.m_sReason = "DEPOT_EXIT_BLOCKED";
			order.m_sObstacle = Describe(trace.TraceEnt);
			return false;
		}
		return true;
	}

	protected static string Describe(IEntity entity)
	{
		if (!entity)
			return "NO_ENTITY";
		if (entity.GetPrefabData())
			return entity.GetPrefabData().GetPrefabName();
		return entity.ClassName();
	}

	protected static vector LocalPoint(AICF_ConstructionOrder order, vector point)
	{
		vector relative = point - order.m_aTransform[3];
		return Vector(vector.Dot(relative, order.m_aTransform[0]), vector.Dot(relative, order.m_aTransform[1]), vector.Dot(relative, order.m_aTransform[2]));
	}

	static bool WorkClearOfExits(AICF_ConstructionOrder order, vector position)
	{
		vector local = LocalPoint(order, position);
		foreach (AICF_ConstructionVolume exitVolume : order.m_aExits)
		{
			// Включает arrival tolerance: worker не должен сам блокировать
			// выбранный выезд во время последнего completion increment.
			if (local[0] >= exitVolume.m_vMin[0] - 2 && local[0] <= exitVolume.m_vMax[0] + 2 &&
				local[2] >= exitVolume.m_vMin[2] - 2 && local[2] <= exitVolume.m_vMax[2] + 2)
				return false;
		}
		return true;
	}

	protected bool CheckEntity(IEntity entity)
	{
		if (!entity || entity == m_ExcludedRoot || (m_ExcludedRoot && entity.GetRootParent() == m_ExcludedRoot))
			return true;
		// Query bounds point/controller entities могут включать радиус сервиса.
		// Для них broad phase не доказывает занятую геометрию: сохраняем actual
		// spawn envelope и доступ к точке; модели независимо проверяются trace.
		SCR_CampaignBuildingCompositionComponent composition = SCR_CampaignBuildingCompositionComponent.Cast(entity.FindComponent(SCR_CampaignBuildingCompositionComponent));
		// У готовой composition общий bounds включает пустое пространство между
		// дочерними объектами. Стены/палатки защищает physics trace, а реальные
		// service/spawn zones — проверки ниже. Незавершённый layout сохраняет
		// полный резерв будущей геометрии, которой ещё нет в physics world.
		bool blocked = composition && !composition.IsCompositionSpawned();
		SCR_EntitySpawnerSlotComponent slot = SCR_EntitySpawnerSlotComponent.Cast(entity.FindComponent(SCR_EntitySpawnerSlotComponent));
		if (slot)
		{
			SCR_EntitySpawnerSlotComponentClass data = SCR_EntitySpawnerSlotComponentClass.Cast(slot.GetComponentData(entity));
			if (!data)
				blocked = true;
			else
			{
				vector transform[4];
				entity.GetWorldTransform(transform);
				vector mins, maxs;
				TransformBounds(data.GetMinBoundsVector(), data.GetMaxBoundsVector(), transform, 0, mins, maxs);
				blocked = blocked || (mins[0] <= m_vQueryMax[0] && maxs[0] >= m_vQueryMin[0] && mins[2] <= m_vQueryMax[2] && maxs[2] >= m_vQueryMin[2]);
			}
		}
		SCR_SpawnPoint spawnPoint = SCR_SpawnPoint.Cast(entity);
		if (entity.FindComponent(SCR_ServicePointComponent) || spawnPoint)
		{
			vector position = entity.GetOrigin();
			vector rotation;
			if (spawnPoint)
				spawnPoint.GetPositionAndRotation(position, rotation);
			vector relative = position - m_aQueryTransform[3];
			vector local = Vector(vector.Dot(relative, m_aQueryTransform[0]), 0, vector.Dot(relative, m_aQueryTransform[2]));
			bool inside = local[0] >= m_vLocalQueryMin[0] - 2 && local[0] <= m_vLocalQueryMax[0] + 2 &&
				local[2] >= m_vLocalQueryMin[2] - 2 && local[2] <= m_vLocalQueryMax[2] + 2;
			blocked = blocked || inside;
			if (!blocked && m_QueryOrder)
			{
				m_QueryOrder.m_iBroadphaseIgnored++;
				if (m_QueryOrder.m_iBroadphaseIgnored == 1)
					m_QueryOrder.Log("CONSTRUCTION_BROADPHASE_IGNORED", string.Format("obstacle=%1 obstacle_position=%2 point_clearance=2", Describe(entity), position));
			}
		}
		if (blocked)
		{
			m_bBlocked = true;
			m_sObstacle = Describe(entity);
			return false;
		}
		return true;
	}

	protected bool TraceEntity(IEntity entity)
	{
		if (!entity || entity == m_ExcludedRoot || (m_ExcludedRoot && entity.GetRootParent() == m_ExcludedRoot))
			return false;
		return true;
	}

	static bool CompletionClear(AICF_ConstructionOrder receipt)
	{
		if (!receipt || !receipt.m_Composition || !receipt.m_Composition.GetOwner() || !receipt.m_Metadata || !receipt.m_bPaid)
			return false;
		AICF_ConstructionConfig config = new AICF_ConstructionConfig();
		AICF_ConstructionSiteSearch search = new AICF_ConstructionSiteSearch(config);
		// Принятая composition живёт по stock capture lifecycle; проверка здесь
		// только физическая, не требует прежней faction/commander authority.
		IEntity entity = receipt.m_Composition.GetOwner();
		if (!receipt.PlacementUnchanged())
		{
			receipt.m_sReason = "LAYOUT_TRANSFORM_CHANGED";
			return false;
		}
		vector transform[4];
		entity.GetWorldTransform(transform);
		AICF_ConstructionOrder check = new AICF_ConstructionOrder();
		check.m_Metadata = receipt.m_Metadata;
		check.m_sToken = receipt.m_sToken;
		check.m_Provider = receipt.m_Provider;
		check.m_fMinHeight = receipt.m_fMinHeight;
		check.m_fMaxHeight = receipt.m_fMaxHeight;
		foreach (AICF_ConstructionVolume exitVolume : receipt.m_aExits)
			check.m_aExits.Insert(exitVolume);
		Math3D.MatrixCopy(transform, check.m_aTransform);
		Bounds(check, transform, config.m_fMargin, check.m_vMin, check.m_vMax);
		bool clear = search.LiveClear(check, entity);
		receipt.m_sReason = check.m_sReason;
		return clear;
	}
}
