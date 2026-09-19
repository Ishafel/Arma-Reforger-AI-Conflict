// Только read-only повтор geometry guards на зафиксированных пользователем
// transforms. Нет spawn gameplay layout, оплаты или обхода production guards.
modded class AICF_ConstructionSiteSearch
{
	bool AICF_ReplayBegin(AICF_ConstructionOrder order, vector position, vector angles)
	{
		Math3D.AnglesToMatrix(angles, order.m_aTransform);
		order.m_aTransform[3] = position;
		order.m_fYaw = angles[0];
		Bounds(order, order.m_aTransform, m_Config.m_fMargin, order.m_vMin, order.m_vMax);
		order.m_fMinHeight = position[1];
		order.m_fMaxHeight = position[1];
		order.m_iStage = 1;
		return InsideBounds(order);
	}
}

modded class AICF_ConstructionPlanner
{
	protected ref AICF_ConstructionMetadata m_AICFReplayMetadata;
	protected ref AICF_ConstructionOrder m_AICFReplayOrder;
	protected SCR_CampaignMilitaryBaseComponent m_AICFReplayBase;
	protected SCR_CampaignFaction m_AICFReplayFaction;
	protected int m_iAICFReplayCase;
	protected int m_iAICFReplayPhase;
	protected int m_iAICFReplayStarted;
	protected int m_iAICFReplayCaseStarted;
	protected int m_iAICFReplayPathSample;

	override void Update()
	{
		string enabled;
		if (!System.GetCLIParam("aicfConstructionReplay", enabled) || enabled != "1")
		{
			super.Update();
			return;
		}
		if (m_bStopped || !Replication.IsServer() || !m_Campaign || !m_Campaign.IsMaster())
			return;
		int now = System.GetTickCount();
		if (!m_iAICFReplayStarted)
		{
			m_iAICFReplayStarted = now;
			string providerPosition;
			System.GetCLIParam("aicfConstructionReplayProvider", providerPosition);
			array<SCR_CampaignMilitaryBaseComponent> bases = {};
			m_Campaign.GetBaseManager().GetBases(bases);
			float nearest = float.MAX;
			foreach (SCR_CampaignMilitaryBaseComponent base : bases)
			{
				if (!base || !base.GetMasterProvider() || !base.GetMasterProvider().GetOwner())
					continue;
				float distance = vector.DistanceSqXZ(providerPosition.ToVector(), base.GetMasterProvider().GetOwner().GetOrigin());
				if (distance < nearest)
				{
					nearest = distance;
					m_AICFReplayBase = base;
				}
			}
			m_AICFReplayFaction = m_Campaign.GetFactionByEnum(SCR_ECampaignFaction.OPFOR);
			m_AICFReplayMetadata = new AICF_ConstructionMetadata();
			m_AICFReplayMetadata.Load(AICF_ContentProfile.GetActive().GetConstructionPrefab("USSR", AICF_EConstructionType.LARGE_BARRACKS),
				AICF_EConstructionType.LARGE_BARRACKS, m_Manager, m_AICFReplayFaction);
			Print(string.Format("[AICF][CONSTRUCTION_REPLAY_BEGIN] test_only=1 nearest_provider_distance=%1 metadata_valid=%2", Math.Sqrt(nearest), m_AICFReplayMetadata.m_bValid));
		}
		if (!m_AICFReplayBase || !m_AICFReplayMetadata.m_bValid || now - m_iAICFReplayStarted > 600000 || m_iAICFReplayCase >= 4)
		{
			Print("[AICF][CONSTRUCTION_REPLAY_DONE] test_only=1 cases=" + m_iAICFReplayCase);
			Stop();
			GetGame().RequestClose();
			return;
		}
		if (!m_AICFReplayMetadata.m_bGeometryLoaded)
		{
			int geometry = m_AICFReplayMetadata.StepGeometry(m_Config.m_iMetadataEntriesPerTick, m_Config.m_iSliceMs);
			if (geometry < 0)
				m_iAICFReplayCase = 4;
			return;
		}
		if (!m_AICFReplayOrder)
		{
			string positionCLI, anglesCLI;
			System.GetCLIParam("aicfConstructionReplayPosition" + (m_iAICFReplayCase / 2), positionCLI);
			System.GetCLIParam("aicfConstructionReplayAngles" + (m_iAICFReplayCase / 2), anglesCLI);
			vector position = positionCLI.ToVector();
			vector angles = anglesCLI.ToVector();
			if (m_iAICFReplayCase % 2 == 1)
			{
				angles = "0 0 0";
				position[1] = GetGame().GetWorld().GetSurfaceY(position[0], position[2]);
			}
			m_AICFReplayOrder = new AICF_ConstructionOrder();
			m_AICFReplayOrder.m_sToken = "manual-replay-" + m_iAICFReplayCase;
			m_AICFReplayOrder.m_sFaction = "USSR";
			m_AICFReplayOrder.m_eType = AICF_EConstructionType.LARGE_BARRACKS;
			m_AICFReplayOrder.m_Metadata = m_AICFReplayMetadata;
			m_AICFReplayOrder.m_Base = m_AICFReplayBase;
			m_AICFReplayOrder.m_Provider = m_AICFReplayBase.GetMasterProvider();
			m_AICFReplayOrder.m_vProviderPosition = m_AICFReplayOrder.m_Provider.GetOwner().GetOrigin();
			string recordedProvider;
			System.GetCLIParam("aicfConstructionReplayProvider", recordedProvider);
			m_AICFReplayOrder.m_vProviderPosition = recordedProvider.ToVector();
			m_AICFReplayOrder.m_iStartedAt = now;
			m_iAICFReplayCaseStarted = now;
			bool bounds = m_Search.AICF_ReplayBegin(m_AICFReplayOrder, position, angles);
			Print(string.Format("[AICF][CONSTRUCTION_REPLAY_TRANSFORM] test_only=1 case=%1 position=%2 angles=%3 radius=%4 bounds=%5 provider=%6 min=%7 max=%8",
				m_iAICFReplayCase, position, angles, m_AICFReplayOrder.m_Provider.GetBuildingRadius(), bounds,
				m_AICFReplayOrder.m_vProviderPosition, m_AICFReplayMetadata.m_vMin, m_AICFReplayMetadata.m_vMax));
			AICF_ReplayCandidates(position);
			AIPathfindingComponent calibration = m_USSR.GetConstructionPathfinding();
			vector onMesh, nearby, calibrationHit;
			bool onMeshFound = calibration.GetClosestPositionOnNavmesh(position, "2 3 2", onMesh);
			bool nearbyFound = calibration.GetClosestPositionOnNavmesh(onMesh + "0.1 0 0", "0.2 1 0.2", nearby);
			Print(string.Format("[AICF][CONSTRUCTION_REPLAY_RAY_CALIBRATION] case=%1 on_mesh=%2 nearby_found=%3 from=%4 nearby=%5 self=%6 near=%7 far=%8", m_iAICFReplayCase,
				onMeshFound, nearbyFound, onMesh, nearby, calibration.RayTrace(onMesh, onMesh, calibrationHit), calibration.RayTrace(onMesh, nearby, calibrationHit), calibration.RayTrace(onMesh, onMesh + "5000 0 5000", calibrationHit)));
			m_iAICFReplayPhase = 0;
		}
		AICF_ConstructionOrder order = m_AICFReplayOrder;
		if (m_iAICFReplayPhase == 0)
		{
			order.m_sReason = "";
			bool clear = m_Search.LiveClear(order, null);
			if (order.m_sReason == "QUERY_BUDGET")
				return;
			Print(string.Format("[AICF][CONSTRUCTION_REPLAY_PHYSICS] test_only=1 case=%1 clear=%2 reason=%3 obstacle=%4", m_iAICFReplayCase, clear, order.m_sReason, order.m_sObstacle));
			bool access = AccessClear(order);
			Print(string.Format("[AICF][CONSTRUCTION_REPLAY_ACCESS] test_only=1 case=%1 clear=%2 reason=%3", m_iAICFReplayCase, access, order.m_sReason));
			m_iAICFReplayPhase = 1;
			return;
		}
		AIPathfindingComponent pathfinding = m_USSR.GetConstructionPathfinding();
		if (m_iAICFReplayPhase == 1)
		{
			int result = m_Search.Step(order, pathfinding);
			if (result < 0 || order.m_iStage >= 3)
			{
				Print(string.Format("[AICF][CONSTRUCTION_REPLAY_TERRAIN] test_only=1 case=%1 stage=%2 result=%3 reason=%4 samples=%5 min_y=%6 max_y=%7",
					m_iAICFReplayCase, order.m_iStage, result, order.m_sReason, order.m_iSample, order.m_fMinHeight, order.m_fMaxHeight));
				m_iAICFReplayPhase = 2;
				string validOnly;
				if (result < 0 && order.m_iStage < 3 && System.GetCLIParam("aicfConstructionReplayValidOnly", validOnly) && validOnly == "1")
				{
					Print("[AICF][CONSTRUCTION_REPLAY_PATH_SKIPPED] test_only=1 case=" + m_iAICFReplayCase + " reason=TERRAIN_REJECTED");
					m_iAICFReplayPhase = 3;
				}
				if (order.m_iStage >= 3 && result != 0)
				{
					Print(string.Format("[AICF][CONSTRUCTION_REPLAY_PATH] test_only=1 case=%1 result=%2 reason=%3 endpoint=%4 cursor=%5",
						m_iAICFReplayCase, result, order.m_sReason, order.m_vWork, order.m_iNavPathCursor));
					m_iAICFReplayPhase = 3;
				}
			}
		}
		else if (m_iAICFReplayPhase == 2)
		{
			int result = m_Search.ValidatePath(order, pathfinding);
			if (order.m_Path && now - m_iAICFReplayPathSample > 15000)
			{
				m_iAICFReplayPathSample = now;
				Print("[AICF][CONSTRUCTION_REPLAY_PATH_SAMPLE] case=" + m_iAICFReplayCase + " " + order.m_Path.Describe());
			}
			if (result != 0 || now - m_iAICFReplayCaseStarted > 120000)
			{
				Print(string.Format("[AICF][CONSTRUCTION_REPLAY_PATH] test_only=1 case=%1 result=%2 reason=%3 endpoint=%4 cursor=%5",
					m_iAICFReplayCase, result, order.m_sReason, order.m_vWork, order.m_iNavPathCursor));
				m_iAICFReplayPhase = 3;
			}
		}
		else if (m_iAICFReplayPhase == 3)
		{
			order.m_sReason = "";
			bool clear = m_Search.LiveClear(order, null);
			if (order.m_sReason == "QUERY_BUDGET")
				return;
			Print(string.Format("[AICF][CONSTRUCTION_REPLAY_FINAL_PHYSICS] test_only=1 case=%1 clear=%2 reason=%3 obstacle=%4", m_iAICFReplayCase, clear, order.m_sReason, order.m_sObstacle));
			m_iAICFReplayPhase = 4;
		}
		else if (m_iAICFReplayPhase == 4)
		{
			if (!AICF_ReplayWorkerPath(order, pathfinding))
				return;
			m_iAICFReplayCase++;
			m_AICFReplayOrder = null;
		}
	}

	// Проверяем ограниченный алгоритм также на endpoint, куда реальный
	// строитель дошёл. Здесь нет запрета входа в будущий общий footprint:
	// это независимое наблюдение, не разрешение placement.
	protected bool AICF_ReplayWorkerPath(AICF_ConstructionOrder order, AIPathfindingComponent pathfinding)
	{
		string workCLI;
		if (!System.GetCLIParam("aicfConstructionReplayWork" + (m_iAICFReplayCase / 2), workCLI) || !pathfinding)
			return true;
		if (!AICF_ConstructionSiteSearch.TakeQueries(order, 76))
			return false;
		vector start, rotation, from, endpoint, hit;
		order.m_Base.GetSpawnPoint().GetPositionAndRotation(start, rotation);
		bool startOnMesh = pathfinding.GetClosestPositionOnNavmesh(start, "8 5 8", from);
		bool endOnMesh = pathfinding.GetClosestPositionOnNavmesh(workCLI.ToVector(), "0.5 2 0.5", endpoint);
		bool direct;
		bool samePoint;
		int detours;
		if (startOnMesh && endOnMesh)
		{
			samePoint = pathfinding.RayTrace(from, from, hit);
			direct = pathfinding.RayTrace(from, endpoint, hit);
			for (int path; path < 24; path++)
			{
				int azimuth = path % 8;
				int ring = path / 8;
				float angle = azimuth * Math.PI2 / 8;
				vector via = from + Vector(Math.Sin(angle), 0, Math.Cos(angle)) * (8 + ring * 8);
				vector corrected;
				if (pathfinding.GetClosestPositionOnNavmesh(via, "1 5 1", corrected) &&
					pathfinding.RayTrace(from, corrected, hit) && pathfinding.RayTrace(corrected, endpoint, hit))
					detours++;
			}
		}
		Print(string.Format("[AICF][CONSTRUCTION_REPLAY_ACTUAL_WORK_PATH] test_only=1 case=%1 start_on_mesh=%2 end_on_mesh=%3 from=%4 endpoint=%5 direct=%6 detours=%7 same_point=%8",
			m_iAICFReplayCase, startOnMesh, endOnMesh, from, endpoint, direct, detours, samePoint));
		return true;
	}

	protected void AICF_ReplayCandidates(vector position)
	{
		float extent = vector.DistanceXZ(m_AICFReplayMetadata.m_vMin, m_AICFReplayMetadata.m_vMax) * 0.5 + m_Config.m_fMargin;
		float outer = Math.Max(0, m_AICFReplayOrder.m_Provider.GetBuildingRadius() - extent);
		vector center = (m_AICFReplayMetadata.m_vMin + m_AICFReplayMetadata.m_vMax) * 0.5;
		float nearest = float.MAX;
		int best;
		vector bestPosition;
		for (int attempt; attempt < m_Config.m_iAttempts; attempt++)
		{
			float yaw;
			vector offset = AICF_ConstructionSiteSearch.CandidateOffset(attempt, extent, outer, yaw);
			vector transform[4];
			Math3D.AnglesToMatrix(Vector(yaw, 0, 0), transform);
			vector root = m_AICFReplayOrder.m_vProviderPosition + offset - transform[0] * center[0] - transform[2] * center[2];
			float distance = vector.DistanceSqXZ(root, position);
			if (distance < nearest)
			{
				nearest = distance;
				best = attempt;
				bestPosition = root;
			}
		}
		Print(string.Format("[AICF][CONSTRUCTION_REPLAY_NEAREST_CANDIDATE] test_only=1 case=%1 count=%2 distance=%3 attempt=%4 position=%5 outer=%6",
			m_iAICFReplayCase, m_Config.m_iAttempts, Math.Sqrt(nearest), best, bestPosition, outer));
	}
}
