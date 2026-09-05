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
		order.m_aTransform[3] = order.m_vProviderPosition + offset;
		order.m_aTransform[3][1] = GetGame().GetWorld().GetSurfaceY(order.m_aTransform[3][0], order.m_aTransform[3][2]);
		Bounds(order, order.m_aTransform, m_Config.m_fMargin, order.m_vMin, order.m_vMax);
		order.m_fMinHeight = order.m_aTransform[3][1];
		order.m_fMaxHeight = order.m_fMinHeight;
		order.m_iSample = 0;
		order.m_iNavRetry = 0;
		order.m_iNavPathCursor = 0;
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
		// 128 разных центров до повторного обхода с другим поворотом. Все восемь
		// orientations представлены с первых ticks. После полного обхода восьми
		// поворотов продолжается последовательность новых центров.
		int batch = attempt / 1024;
		int pass = attempt / 128;
		int position = (attempt % 128) + batch * 128;
		int orientation = (position * 3 + pass) % 8;
		yaw = orientation * 45;
		float radial = Fraction((position + 1) * 0.41421356);
		float angle = Fraction(position * 0.61803399) * Math.PI2;
		// Основная доля точек — вокруг HQ, за габаритом нового здания, а не
		// внутри занятого центра. Каждая четвёртая исследует полный radius.
		float inner = Math.Min(extent + 8, outer);
		float limit = outer;
		if (position % 4 != 3)
			limit = Math.Min(outer, extent * 4);
		limit = Math.Max(inner, limit);
		float radius = Math.Lerp(inner, limit, radial);
		return Vector(Math.Sin(angle) * radius, 0, Math.Cos(angle) * radius);
	}

	protected static float Fraction(float value)
	{
		return value - Math.Floor(value);
	}

	protected bool InsideBounds(AICF_ConstructionOrder order)
	{
		return InsideVolume(order, order.m_vMin, order.m_vMax);
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
		float radius = order.m_Provider.GetBuildingRadius();
		for (int corner; corner < 4; corner++)
		{
			vector point = mins;
			if (corner & 1)
				point[0] = maxs[0];
			if (corner & 2)
				point[2] = maxs[2];
			if (point[0] < worldMin[0] || point[0] > worldMax[0] || point[2] < worldMin[2] || point[2] > worldMax[2] ||
				vector.DistanceSqXZ(point, order.m_Provider.GetOwner().GetOrigin()) > radius * radius)
			{
				order.m_sReason = "OUTSIDE_BUILDING_OR_WORLD_BOUNDS";
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
			if (order.m_fMaxHeight - order.m_fMinHeight > m_Config.m_fHeightDelta)
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
		// Доступ именно к endpoint, который затем использует worker. Он находится
		// за полной геометрией и рабочим отступом, включая границы stock layout.
		return ValidatePath(order, pathfinding);
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
		if (!pathfinding || !pathfinding.GetNavmeshComponent())
		{
			order.m_sReason = "NAVMESH_UNAVAILABLE";
			return -1;
		}
		if (!TakeQueries(order, 5))
			return 0;
		BaseWorld world = GetGame().GetWorld();
		vector home = order.m_vProviderPosition;
		int side = order.m_iNavPathCursor / 25;
		if (side >= 4)
		{
			order.m_sReason = "WORKER_ENDPOINT_UNREACHABLE";
			return -1;
		}
		switch (side)
		{
			case 0: order.m_vWork = Vector(order.m_vMin[0] - 2, 0, Math.Clamp(home[2], order.m_vMin[2], order.m_vMax[2])); break;
			case 1: order.m_vWork = Vector(order.m_vMax[0] + 2, 0, Math.Clamp(home[2], order.m_vMin[2], order.m_vMax[2])); break;
			case 2: order.m_vWork = Vector(Math.Clamp(home[0], order.m_vMin[0], order.m_vMax[0]), 0, order.m_vMin[2] - 2); break;
			case 3: order.m_vWork = Vector(Math.Clamp(home[0], order.m_vMin[0], order.m_vMax[0]), 0, order.m_vMax[2] + 2); break;
		}
		order.m_vWork[1] = world.GetSurfaceY(order.m_vWork[0], order.m_vWork[2]);
		vector start, rotation;
		if (!order.m_Base.GetSpawnPoint())
			return -1;
		order.m_Base.GetSpawnPoint().GetPositionAndRotation(start, rotation);
		NavmeshWorldComponent navmesh = pathfinding.GetNavmeshComponent();
		if (!navmesh.IsTileLoaded(start) || !navmesh.IsTileLoaded(order.m_vWork))
		{
			if (order.m_iNavRetry++ >= 10)
			{
				order.m_sReason = "NAVMESH_TILE_TIMEOUT";
				return -1;
			}
			if (!navmesh.IsTileRequested(start))
				navmesh.LoadTileIn(start);
			if (!navmesh.IsTileRequested(order.m_vWork))
				navmesh.LoadTileIn(order.m_vWork);
			order.m_sReason = "NAVMESH_TILE_LOADING";
			return 0;
		}
		vector from, endpoint, hit;
		if (!pathfinding.GetClosestPositionOnNavmesh(start, "8 5 8", from) ||
			!pathfinding.GetClosestPositionOnNavmesh(order.m_vWork, "0.5 2 0.5", endpoint) ||
			vector.DistanceSqXZ(endpoint, order.m_vWork) > 0.25)
		{
			order.m_iNavPathCursor = (side + 1) * 25;
			return 0;
		}
		if (!WorkClearOfExits(order, endpoint))
		{
			order.m_sReason = "WORKER_ENDPOINT_IN_EXIT";
			order.m_iNavPathCursor = (side + 1) * 25;
			return 0;
		}
		// Конечный двухсегментный путь. Оба отрезка обязаны лежать на navmesh;
		// это проверка связности, а не замена worker pathfinding/waypoint.
		int pathStarted = System.GetTickCount();
		for (int batch; batch < 6 && order.m_iNavPathCursor / 25 == side && System.GetTickCount() - pathStarted < m_Config.m_iSliceMs; batch++)
		{
			if (!TakeQueries(order, 3))
				return 0;
			int path = order.m_iNavPathCursor++ % 25;
			bool reachable;
			if (path == 0)
				reachable = !AICF_ConstructionPlanner.SegmentIntersects(from, endpoint, order.m_vMin, order.m_vMax) && pathfinding.RayTrace(from, endpoint, hit);
			else
			{
				int azimuth = (path - 1) % 8;
				int ring = (path - 1) / 8;
				float angle = azimuth * Math.PI2 / 8;
				vector via = from + Vector(Math.Sin(angle), 0, Math.Cos(angle)) * (8 + ring * 8);
				vector corrected;
				if (pathfinding.GetClosestPositionOnNavmesh(via, "1 5 1", corrected) &&
					!AICF_ConstructionPlanner.SegmentIntersects(from, corrected, order.m_vMin, order.m_vMax) &&
					!AICF_ConstructionPlanner.SegmentIntersects(corrected, endpoint, order.m_vMin, order.m_vMax))
					reachable = pathfinding.RayTrace(from, corrected, hit) && pathfinding.RayTrace(corrected, endpoint, hit);
			}
			if (reachable)
			{
				order.m_vWork = endpoint;
				order.m_sReason = "SITE_VALIDATED";
				return 1;
			}
		}
		return 0;
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
		if (!InsideVolume(order, mins, maxs))
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
		foreach (AICF_ConstructionVolume exitVolume : receipt.m_aExits)
			check.m_aExits.Insert(exitVolume);
		Math3D.MatrixCopy(transform, check.m_aTransform);
		Bounds(check, transform, config.m_fMargin, check.m_vMin, check.m_vMax);
		bool clear = search.LiveClear(check, entity);
		receipt.m_sReason = check.m_sReason;
		return clear;
	}
}
