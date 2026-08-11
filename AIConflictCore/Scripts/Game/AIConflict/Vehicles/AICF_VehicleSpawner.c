// Compact result object for one safe base. Keeping per-position surface and
// roster samples out of TrySpawn avoids exhausting the Enforce compiler's local
// frame while retaining the exact rejection/selection evidence for the caller.
class AICF_VehicleSpawnSiteEvaluation
{
	SCR_CampaignMilitaryBaseComponent m_Base;
	string m_sResult;
	string m_sTraceDetails;
	vector m_vPosition;
	int m_iCandidatePositionIndex = -1;
	int m_iCandidatePositionCount;
	string m_sSurfaceKind;
	float m_fFootprintHeightDeltaMeters = -1.0;
	int m_iSurfaceProbeCount;
	bool m_bHasBoardingRejection;
	vector m_vBoardingRejectionPosition;
	int m_iAliveCount;
	float m_fLeaderDistanceMeters;
	float m_fNearestDistanceMeters;
	float m_fFarthestDistanceMeters;
	string m_sMemberSamples;
}

// Tries safe friendly Conflict bases from nearest to farthest and uses the same
// stock empty-terrain query as initial HQ vehicles. The caller reserves cap
// capacity before entering this class, so spawn validation and creation are
// atomic on authority.
class AICF_VehicleSpawner
{
	protected static const float SPAWN_SEARCH_RADIUS_METERS = 45.0;
	protected static const float VEHICLE_CLEARANCE_RADIUS_METERS = 8.0;
	protected static const int MAX_SPAWN_POSITIONS_PER_BASE = 16;
	protected static const float SURFACE_PROBE_RADIUS_METERS = 6.0;
	protected static const float MAX_FOOTPRINT_HEIGHT_DELTA_METERS = 4.0;

	bool TrySpawn(
		SCR_GameModeCampaign campaign,
		SCR_CampaignFaction faction,
		AICF_GroupSlot slot,
		AICF_ConflictAdapter conflictAdapter,
		ResourceName prefab,
		vector preferredPosition,
		float maximumSpawnDistanceMeters,
		float maximumBoardingDistanceMeters,
		bool preflightOnly,
		AICF_VehicleRuntime runtime,
		out Vehicle vehicle,
		out SCR_AIVehicleUsageComponent vehicleUsage,
		out SCR_CampaignMilitaryBaseComponent spawnBase,
		out string failureReason,
		out bool retryable,
		out SCR_CampaignMilitaryBaseComponent failureBase)
	{
		vehicle = null;
		vehicleUsage = null;
		spawnBase = null;
		failureReason = "INVALID_SPAWN_REQUEST";
		retryable = false;
		failureBase = null;
		if (!Replication.IsServer() || !campaign || !campaign.IsMaster() || !faction || !slot || !conflictAdapter || !runtime || prefab.IsEmpty())
			return false;

		array<SCR_CampaignMilitaryBaseComponent> candidates = {};
		SCR_CampaignMilitaryBaseComponent mainBase = faction.GetMainBase();
		if (mainBase)
			candidates.Insert(mainBase);

		SCR_CampaignMilitaryBaseManager baseManager = campaign.GetBaseManager();
		if (baseManager)
		{
			array<SCR_CampaignMilitaryBaseComponent> bases = {};
			baseManager.GetBases(bases);
			foreach (SCR_CampaignMilitaryBaseComponent base : bases)
			{
				if (base && !candidates.Contains(base))
					candidates.Insert(base);
			}
		}

		array<SCR_CampaignMilitaryBaseComponent> safeCandidates = {};
		string candidateTrace;
		int actionableCandidateCount;
		int hostileCandidateCount;
		string firstRejectionReason;
		SCR_CampaignMilitaryBaseComponent firstRejectedBase;
		string meaningfulRejectionReason;
		SCR_CampaignMilitaryBaseComponent meaningfulRejectedBase;
		foreach (SCR_CampaignMilitaryBaseComponent candidate : candidates)
		{
			float preferredDistanceMeters = Math.Sqrt(vector.DistanceSqXZ(
				candidate.GetOwner().GetOrigin(),
				preferredPosition));
			string candidateKey = AICF_Stage1Diagnostics.BaseKey(candidate);
			string rejectionReason = conflictAdapter.GetSpawnRejectionReason(candidate, faction);
			if (!rejectionReason.IsEmpty())
			{
				if (rejectionReason == "ENEMY_OWNED")
				{
					hostileCandidateCount++;
				}
				else
				{
					AppendCandidateTrace(
						candidateTrace,
						candidateKey,
						rejectionReason,
						preferredDistanceMeters,
						string.Empty);
					actionableCandidateCount++;
				}
				if (firstRejectionReason.IsEmpty())
				{
					firstRejectionReason = rejectionReason;
					firstRejectedBase = candidate;
				}

				// Hostile bases are expected entries in the global base list. Preserve
				// actionable owned-base failures (contested, inactive spawn point,
				// initialization race) instead of flooding every retry with ENEMY_OWNED.
				if (rejectionReason != "ENEMY_OWNED" && meaningfulRejectionReason.IsEmpty())
				{
					meaningfulRejectionReason = rejectionReason;
					meaningfulRejectedBase = candidate;
				}
				continue;
			}

			// Stable insertion keeps the candidate walk deterministic while avoiding
			// comparator/delegate syntax that is not available in Enforce 1.7.
			float distanceSq = preferredDistanceMeters * preferredDistanceMeters;
			int insertIndex = safeCandidates.Count();
			for (int safeIndex = 0; safeIndex < safeCandidates.Count(); safeIndex++)
			{
				float sortedDistanceSq = vector.DistanceSqXZ(safeCandidates[safeIndex].GetOwner().GetOrigin(), preferredPosition);
				string sortedCandidateKey = AICF_Stage1Diagnostics.BaseKey(safeCandidates[safeIndex]);
				if (distanceSq < sortedDistanceSq ||
					(distanceSq == sortedDistanceSq && candidateKey.Compare(sortedCandidateKey) < 0))
				{
					insertIndex = safeIndex;
					break;
				}
			}
			if (insertIndex >= safeCandidates.Count())
				safeCandidates.Insert(candidate);
			else
				safeCandidates.InsertAt(candidate, insertIndex);
		}

		if (safeCandidates.IsEmpty())
		{
			ReportCandidateEvaluation(
				runtime,
				preflightOnly,
				preferredPosition,
				candidates.Count(),
				hostileCandidateCount,
				actionableCandidateCount,
				safeCandidates.Count(),
				candidateTrace);
			failureReason = meaningfulRejectionReason;
			failureBase = meaningfulRejectedBase;
			if (failureReason.IsEmpty())
			{
				failureReason = firstRejectionReason;
				failureBase = firstRejectedBase;
			}
			if (failureReason.IsEmpty())
				failureReason = "NO_SAFE_SPAWN_AVAILABLE";
			retryable = true;
			return false;
		}

		string siteFailureReason;
		SCR_CampaignMilitaryBaseComponent siteFailureBase;
		AICF_VehicleSpawnSiteEvaluation boardingRejectionSite;
		AICF_VehicleSpawnSiteEvaluation selectedSite;
		foreach (SCR_CampaignMilitaryBaseComponent safeCandidate : safeCandidates)
		{
			float candidateDistanceSq = vector.DistanceSqXZ(safeCandidate.GetOwner().GetOrigin(), preferredPosition);
			float candidateDistanceMeters = Math.Sqrt(candidateDistanceSq);
			string safeCandidateKey = AICF_Stage1Diagnostics.BaseKey(safeCandidate);
			if (maximumSpawnDistanceMeters > 0 && candidateDistanceSq > maximumSpawnDistanceMeters * maximumSpawnDistanceMeters)
			{
				AppendCandidateTrace(
					candidateTrace,
					safeCandidateKey,
					"TOO_FAR",
					candidateDistanceMeters,
					string.Format("maximum_spawn_m=%1", maximumSpawnDistanceMeters));
				actionableCandidateCount++;
				// Distance-only rejection is the least precise site failure. Keep the
				// nearest one unless a terrain/member check produces a stronger cause.
				if (siteFailureReason.IsEmpty())
				{
					siteFailureReason = "TOO_FAR";
					siteFailureBase = safeCandidate;
				}
				continue;
			}

			AICF_VehicleSpawnSiteEvaluation site = EvaluateBaseSpawnPositions(
				runtime,
				faction,
				slot,
				safeCandidate,
				maximumBoardingDistanceMeters,
				preflightOnly);
			AppendCandidateTrace(
				candidateTrace,
				safeCandidateKey,
				site.m_sResult,
				candidateDistanceMeters,
				site.m_sTraceDetails);
			actionableCandidateCount++;

			if (site.m_sResult == "NO_BOARDING_SITE_WITHIN_RANGE" &&
				site.m_bHasBoardingRejection && !boardingRejectionSite)
			{
				boardingRejectionSite = site;
				siteFailureReason = "NO_BOARDING_SITE_WITHIN_RANGE";
				siteFailureBase = safeCandidate;
			}

			if (site.m_sResult == "SELECTED")
			{
				selectedSite = site;
				spawnBase = safeCandidate;
				break;
			}
			if (site.m_sResult == "GROUP_NOT_READY")
			{
				siteFailureReason = "GROUP_NOT_READY";
				siteFailureBase = safeCandidate;
				break;
			}
			if (site.m_sResult == "NO_EMPTY_TERRAIN")
			{
				if (siteFailureReason.IsEmpty() || siteFailureReason == "TOO_FAR")
				{
					siteFailureReason = "NO_EMPTY_TERRAIN";
					siteFailureBase = safeCandidate;
				}
				continue;
			}
			if (site.m_sResult == "WATER_OR_UNDRIVABLE_SURFACE")
			{
				if (siteFailureReason.IsEmpty() || siteFailureReason == "TOO_FAR" || siteFailureReason == "NO_EMPTY_TERRAIN")
				{
					siteFailureReason = "WATER_OR_UNDRIVABLE_SURFACE";
					siteFailureBase = safeCandidate;
				}
			}
		}

		ReportCandidateEvaluation(
			runtime,
			preflightOnly,
			preferredPosition,
			candidates.Count(),
			hostileCandidateCount,
			actionableCandidateCount,
			safeCandidates.Count(),
			candidateTrace);

		// Report only the nearest exact boarding rejection. Reporting each failed
		// candidate would rotate the runtime's one-shot key and recreate warning spam.
		if (boardingRejectionSite)
		{
			string reportKey = string.Format(
				"SITE:%1:%2:%3:%4",
				runtime.GetRequestGeneration(),
				runtime.GetSpawnAttempt(),
				AICF_Stage1Diagnostics.BaseKey(boardingRejectionSite.m_Base),
				"NO_BOARDING_SITE_WITHIN_RANGE");
			if (runtime.MarkSpawnIssueReported(reportKey))
			{
				string siteSample = string.Format(
					"%1,%2,%3",
					boardingRejectionSite.m_vBoardingRejectionPosition[0],
					boardingRejectionSite.m_vBoardingRejectionPosition[1],
					boardingRejectionSite.m_vBoardingRejectionPosition[2]);
				string rejectionDetails = string.Format(
						"%1 base=%2 retryable=1 alive=%3 leader_m=%4 nearest_m=%5 farthest_m=%6 maximum_m=%7 site=[%8] member_samples=[%9]",
						runtime.DescribeContext("NO_BOARDING_SITE_WITHIN_RANGE"),
						AICF_Stage1Diagnostics.BaseKey(boardingRejectionSite.m_Base),
						boardingRejectionSite.m_iAliveCount,
						boardingRejectionSite.m_fLeaderDistanceMeters,
						boardingRejectionSite.m_fNearestDistanceMeters,
						boardingRejectionSite.m_fFarthestDistanceMeters,
						maximumBoardingDistanceMeters,
						siteSample,
						boardingRejectionSite.m_sMemberSamples);
				if (preflightOnly)
					AICF_Stage3Diagnostics.Info("VEHICLE_SPAWN_SITE_REJECTED", rejectionDetails);
				else
					AICF_Stage3Diagnostics.Warning("VEHICLE_SPAWN_SITE_REJECTED", rejectionDetails);
			}
		}

		if (!spawnBase)
		{
			failureReason = siteFailureReason;
			failureBase = siteFailureBase;
			if (failureReason.IsEmpty())
				failureReason = "NO_SAFE_SPAWN_AVAILABLE";
			retryable = true;
			return false;
		}

		// If an owned candidate was unsafe but another safe base was selected,
		// retain one causal acceptance event without logging every hostile base.
		if (meaningfulRejectedBase && runtime.MarkSpawnIssueReported(
			string.Format(
				"SITE:%1:%2:%3:%4",
				runtime.GetRequestGeneration(),
				runtime.GetSpawnAttempt(),
				AICF_Stage1Diagnostics.BaseKey(meaningfulRejectedBase),
				meaningfulRejectionReason)))
		{
			AICF_Stage3Diagnostics.Info(
				"VEHICLE_SPAWN_SITE_REJECTED",
				string.Format("%1 base=%2 retryable=1", runtime.DescribeContext(meaningfulRejectionReason), AICF_Stage1Diagnostics.BaseKey(meaningfulRejectedBase)));
		}

		if (preflightOnly)
		{
			failureReason = string.Empty;
			failureBase = null;
			string preflightSurfaceTelemetry = FormatSelectedSurfaceTelemetry(
				selectedSite.m_vPosition,
				selectedSite.m_iCandidatePositionIndex,
				selectedSite.m_iCandidatePositionCount,
				selectedSite.m_sSurfaceKind,
				selectedSite.m_fFootprintHeightDeltaMeters,
				selectedSite.m_iSurfaceProbeCount);
			AICF_Stage3Diagnostics.Info(
				"VEHICLE_SPAWN_PREFLIGHT_READY",
				string.Format(
					"%1 base=%2 owner=%3 contested=0 entity_created=0%4",
					runtime.DescribeContext("SAFE_FRIENDLY_BASE"),
					AICF_Stage1Diagnostics.BaseKey(spawnBase),
					faction.GetFactionKey(),
					preflightSurfaceTelemetry));
			return true;
		}

		string selectedDetails = string.Format(
			"%1 base=%2 owner=%3 contested=0",
			runtime.DescribeContext("SAFE_FRIENDLY_BASE"),
			AICF_Stage1Diagnostics.BaseKey(spawnBase),
			faction.GetFactionKey());
		selectedDetails += FormatSelectedSurfaceTelemetry(
			selectedSite.m_vPosition,
			selectedSite.m_iCandidatePositionIndex,
			selectedSite.m_iCandidatePositionCount,
			selectedSite.m_sSurfaceKind,
			selectedSite.m_fFootprintHeightDeltaMeters,
			selectedSite.m_iSurfaceProbeCount);
		AICF_Stage3Diagnostics.Info(
			"VEHICLE_SPAWN_SITE_SELECTED",
			selectedDetails);

		EntitySpawnParams params = new EntitySpawnParams();
		params.TransformMode = ETransformMode.WORLD;
		params.Transform[3] = selectedSite.m_vPosition;
		vehicle = Vehicle.Cast(GetGame().SpawnEntityPrefabEx(prefab, false, params: params));
		if (!vehicle)
		{
			failureReason = "SPAWN_RETURNED_NULL";
			failureBase = spawnBase;
			return false;
		}

		SCR_FactionAffiliationComponent affiliation = SCR_FactionAffiliationComponent.Cast(vehicle.FindComponent(SCR_FactionAffiliationComponent));
		if (!affiliation)
		{
			failureReason = "FACTION_COMPONENT_MISSING";
			failureBase = spawnBase;
			RplComponent.DeleteRplEntity(vehicle, false);
			vehicle = null;
			return false;
		}

		// Stock faction catalog prefabs may intentionally have no active faction.
		// The catalog selection authorizes the prefab; authoritative runtime spawn
		// must assign the active faction before AI/group systems inspect the vehicle.
		affiliation.SetAffiliatedFaction(faction);
		Faction vehicleFaction = affiliation.GetAffiliatedFaction();
		if (!vehicleFaction || vehicleFaction.GetFactionKey() != faction.GetFactionKey())
		{
			failureReason = "FACTION_ASSIGNMENT_FAILED";
			failureBase = spawnBase;
			RplComponent.DeleteRplEntity(vehicle, false);
			vehicle = null;
			return false;
		}

		vehicleUsage = SCR_AIVehicleUsageComponent.Cast(vehicle.FindComponent(SCR_AIVehicleUsageComponent));
		if (!vehicleUsage || !vehicleUsage.CanBePiloted() || !vehicleUsage.IsVehicleTypeValid())
		{
			failureReason = "AI_USAGE_INVALID";
			failureBase = spawnBase;
			RplComponent.DeleteRplEntity(vehicle, false);
			vehicle = null;
			vehicleUsage = null;
			return false;
		}

		Physics physicsComponent = vehicle.GetPhysics();
		if (physicsComponent)
			physicsComponent.SetVelocity("0 -0.1 0");

		failureReason = string.Empty;
		failureBase = null;
		return true;
	}

	protected AICF_VehicleSpawnSiteEvaluation EvaluateBaseSpawnPositions(
		AICF_VehicleRuntime runtime,
		SCR_CampaignFaction faction,
		AICF_GroupSlot slot,
		SCR_CampaignMilitaryBaseComponent candidateBase,
		float maximumBoardingDistanceMeters,
		bool preflightOnly)
	{
		AICF_VehicleSpawnSiteEvaluation result = new AICF_VehicleSpawnSiteEvaluation();
		result.m_Base = candidateBase;
		vector searchCenter = candidateBase.GetOwner().GetOrigin();
		SCR_SpawnPoint baseSpawnPoint = candidateBase.GetSpawnPoint();
		if (baseSpawnPoint)
			searchCenter = baseSpawnPoint.GetOrigin();

		array<vector> candidatePositions = {};
		result.m_iCandidatePositionCount = SCR_WorldTools.FindAllEmptyTerrainPositions(
			candidatePositions,
			searchCenter,
			SPAWN_SEARCH_RADIUS_METERS,
			VEHICLE_CLEARANCE_RADIUS_METERS,
			maxResults: MAX_SPAWN_POSITIONS_PER_BASE,
			world: candidateBase.GetOwner().GetWorld());
		if (result.m_iCandidatePositionCount <= 0)
		{
			result.m_sResult = "NO_EMPTY_TERRAIN";
			return result;
		}

		bool foundValidatedSurface;
		int rejectedSurfaceCount;
		for (int candidatePositionIndex; candidatePositionIndex < result.m_iCandidatePositionCount; candidatePositionIndex++)
		{
			vector candidatePosition = candidatePositions[candidatePositionIndex];
			string surfaceKind;
			bool waterDetected;
			float footprintHeightDeltaMeters;
			int surfaceProbeCount;
			if (!IsWheeledSpawnSurfaceSuitable(
				candidatePosition,
				candidateBase.GetOwner().GetWorld(),
				surfaceKind,
				waterDetected,
				footprintHeightDeltaMeters,
				surfaceProbeCount))
			{
				rejectedSurfaceCount++;
				ReportSurfaceCandidateRejected(
					runtime,
					faction,
					slot,
					candidateBase,
					candidatePosition,
					candidatePositionIndex,
					result.m_iCandidatePositionCount,
					preflightOnly,
					surfaceKind,
					waterDetected,
					footprintHeightDeltaMeters,
					surfaceProbeCount);
				continue;
			}
			foundValidatedSurface = true;

			int aliveCount;
			float leaderDistanceMeters;
			float nearestDistanceMeters;
			float farthestDistanceMeters;
			string memberSamples;
			MeasureAliveGroupDistancesToPosition(
				slot.GetGroup(),
				candidatePosition,
				aliveCount,
				leaderDistanceMeters,
				nearestDistanceMeters,
				farthestDistanceMeters,
				memberSamples);
			if (aliveCount <= 0)
			{
				result.m_sResult = "GROUP_NOT_READY";
				result.m_sTraceDetails = "alive=0";
				return result;
			}

			if (maximumBoardingDistanceMeters > 0 && farthestDistanceMeters > maximumBoardingDistanceMeters)
			{
				if (!result.m_bHasBoardingRejection)
				{
					result.m_bHasBoardingRejection = true;
					result.m_vBoardingRejectionPosition = candidatePosition;
					result.m_iAliveCount = aliveCount;
					result.m_fLeaderDistanceMeters = leaderDistanceMeters;
					result.m_fNearestDistanceMeters = nearestDistanceMeters;
					result.m_fFarthestDistanceMeters = farthestDistanceMeters;
					result.m_sMemberSamples = memberSamples;
				}
				continue;
			}

			result.m_sResult = "SELECTED";
			result.m_vPosition = candidatePosition;
			result.m_iCandidatePositionIndex = candidatePositionIndex;
			result.m_sSurfaceKind = surfaceKind;
			result.m_fFootprintHeightDeltaMeters = footprintHeightDeltaMeters;
			result.m_iSurfaceProbeCount = surfaceProbeCount;
			result.m_sTraceDetails = string.Format(
				"alive=%1|nearest_m=%2|farthest_m=%3|candidate_index=%4",
				aliveCount,
				nearestDistanceMeters,
				farthestDistanceMeters,
				candidatePositionIndex);
			return result;
		}

		if (!foundValidatedSurface)
		{
			result.m_sResult = "WATER_OR_UNDRIVABLE_SURFACE";
			result.m_sTraceDetails = string.Format(
				"rejected=%1|evaluated=%2",
				rejectedSurfaceCount,
				result.m_iCandidatePositionCount);
			return result;
		}

		result.m_sResult = "NO_BOARDING_SITE_WITHIN_RANGE";
		result.m_sTraceDetails = string.Format(
			"evaluated=%1|maximum_boarding_m=%2",
			result.m_iCandidatePositionCount,
			maximumBoardingDistanceMeters);
		return result;
	}

	protected bool IsWheeledSpawnSurfaceSuitable(
		vector candidatePosition,
		BaseWorld world,
		out string surfaceKind,
		out bool waterDetected,
		out float footprintHeightDeltaMeters,
		out int surfaceProbeCount)
	{
		surfaceKind = "WORLD_UNAVAILABLE";
		waterDetected = false;
		footprintHeightDeltaMeters = -1.0;
		surfaceProbeCount = 0;
		if (!world)
			return false;

		array<vector> probeOffsets = {};
		probeOffsets.Insert(vector.Zero);
		probeOffsets.Insert(Vector(SURFACE_PROBE_RADIUS_METERS, 0, 0));
		probeOffsets.Insert(Vector(-SURFACE_PROBE_RADIUS_METERS, 0, 0));
		probeOffsets.Insert(Vector(0, 0, SURFACE_PROBE_RADIUS_METERS));
		probeOffsets.Insert(Vector(0, 0, -SURFACE_PROBE_RADIUS_METERS));
		bool hasTerrainHeight;
		float minimumTerrainY;
		float maximumTerrainY;
		foreach (vector probeOffset : probeOffsets)
		{
			vector probePosition = candidatePosition + probeOffset;
			float terrainY = world.GetSurfaceY(probePosition[0], probePosition[2]);
			probePosition[1] = terrainY;
			surfaceProbeCount++;
			if (ChimeraWorldUtils.TryGetWaterSurfaceSimple(world, probePosition))
			{
				waterDetected = true;
				surfaceKind = "WATER";
				return false;
			}

			if (!hasTerrainHeight)
			{
				minimumTerrainY = terrainY;
				maximumTerrainY = terrainY;
				hasTerrainHeight = true;
			}
			else
			{
				minimumTerrainY = Math.Min(minimumTerrainY, terrainY);
				maximumTerrainY = Math.Max(maximumTerrainY, terrainY);
			}
		}

		footprintHeightDeltaMeters = maximumTerrainY - minimumTerrainY;
		if (footprintHeightDeltaMeters > MAX_FOOTPRINT_HEIGHT_DELTA_METERS)
		{
			surfaceKind = "UNEVEN_TERRAIN";
			return false;
		}

		surfaceKind = "LAND";
		return true;
	}

	protected void ReportSurfaceCandidateRejected(
		AICF_VehicleRuntime runtime,
		SCR_CampaignFaction faction,
		AICF_GroupSlot slot,
		SCR_CampaignMilitaryBaseComponent candidateBase,
		vector candidatePosition,
		int candidatePositionIndex,
		int candidatePositionCount,
		bool preflightOnly,
		string surfaceKind,
		bool waterDetected,
		float footprintHeightDeltaMeters,
		int surfaceProbeCount)
	{
		string details = string.Format(
			"faction=%1 slot=%2 base=%3 reason=WATER_OR_UNDRIVABLE_SURFACE",
			faction.GetFactionKey(),
			slot.GetSlotKey(),
			AICF_Stage1Diagnostics.BaseKey(candidateBase));
		details += string.Format(
			" candidate_index=%1 candidates=%2 preflight=%3 request_generation=%4",
			candidatePositionIndex,
			candidatePositionCount,
			preflightOnly,
			runtime.GetRequestGeneration());
		details += string.Format(
			" origin=[%1,%2,%3] surface=%4 water=%5",
			candidatePosition[0],
			candidatePosition[1],
			candidatePosition[2],
			surfaceKind,
			waterDetected);
		details += string.Format(
			" footprint_delta_m=%1 probes=%2",
			footprintHeightDeltaMeters,
			surfaceProbeCount);
		AICF_Stage35Diagnostics.Info("VEHICLE_SPAWN_CANDIDATE_REJECTED", details);
	}

	protected string FormatSelectedSurfaceTelemetry(
		vector position,
		int candidatePositionIndex,
		int candidatePositionCount,
		string surfaceKind,
		float footprintHeightDeltaMeters,
		int surfaceProbeCount)
	{
		string details = string.Format(
			" origin=[%1,%2,%3] surface=%4",
			position[0],
			position[1],
			position[2],
			surfaceKind);
		details += string.Format(
			" water=0 footprint_delta_m=%1 probes=%2 candidate_index=%3 candidates=%4",
			footprintHeightDeltaMeters,
			surfaceProbeCount,
			candidatePositionIndex,
			candidatePositionCount);
		return details;
	}

	protected void AppendCandidateTrace(
		inout string candidateTrace,
		string baseKey,
		string result,
		float preferredDistanceMeters,
		string details)
	{
		if (!candidateTrace.IsEmpty())
			candidateTrace += ";";
		candidateTrace += string.Format(
			"%1:result=%2|preferred_m=%3",
			baseKey,
			result,
			preferredDistanceMeters);
		if (!details.IsEmpty())
			candidateTrace += "|" + details;
	}

	protected void ReportCandidateEvaluation(
		AICF_VehicleRuntime runtime,
		bool preflightOnly,
		vector preferredPosition,
		int candidateCount,
		int hostileCandidateCount,
		int actionableCandidateCount,
		int safeCandidateCount,
		string candidateTrace)
	{
		if (!runtime)
			return;
		if (candidateTrace.IsEmpty())
			candidateTrace = "NONE_ACTIONABLE";
		string mode = "SPAWN_ATTEMPT";
		if (preflightOnly)
			mode = "WAIT_PREFLIGHT";
		string details = string.Format(
			"%1 mode=%2 request_generation=%3 attempt=%4 bases_total=%5 hostile_skipped=%6 safe_candidates=%7 actionable=%8",
			runtime.DescribeContext("CANDIDATE_EVALUATION"),
			mode,
			runtime.GetRequestGeneration(),
			runtime.GetSpawnAttempt(),
			candidateCount,
			hostileCandidateCount,
			safeCandidateCount,
			actionableCandidateCount);
		details += string.Format(
			" preferred=[%1,%2,%3] candidates=[%4]",
			preferredPosition[0],
			preferredPosition[1],
			preferredPosition[2],
			candidateTrace);
		AICF_Stage3Diagnostics.Info("VEHICLE_SPAWN_CANDIDATES_EVALUATED", details);
	}

	protected bool MeasureAliveGroupDistancesToPosition(
		SCR_AIGroup group,
		vector position,
		out int aliveCount,
		out float leaderDistanceMeters,
		out float nearestDistanceMeters,
		out float farthestDistanceMeters,
		out string memberSamples)
	{
		aliveCount = 0;
		leaderDistanceMeters = -1.0;
		nearestDistanceMeters = float.MAX;
		farthestDistanceMeters = -1.0;
		memberSamples = string.Empty;
		if (!group)
			return false;

		IEntity leader = AICF_GroupRuntime.ResolveAliveLeader(group);
		array<AIAgent> agents = {};
		group.GetAgents(agents);
		foreach (AIAgent agent : agents)
		{
			if (!agent)
				continue;

			IEntity entity = agent.GetControlledEntity();
			if (!AICF_GroupRuntime.IsAliveCharacter(entity))
				continue;

			float distanceMeters = Math.Sqrt(vector.DistanceSqXZ(entity.GetOrigin(), position));
			aliveCount++;
			nearestDistanceMeters = Math.Min(nearestDistanceMeters, distanceMeters);
			farthestDistanceMeters = Math.Max(farthestDistanceMeters, distanceMeters);
			if (entity == leader)
				leaderDistanceMeters = distanceMeters;

			if (!memberSamples.IsEmpty())
				memberSamples += ",";
			memberSamples += string.Format("%1:%2", entity.GetID(), distanceMeters);
		}

		if (aliveCount <= 0)
		{
			nearestDistanceMeters = -1.0;
			return false;
		}

		return true;
	}
}
