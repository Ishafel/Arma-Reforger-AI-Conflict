// Tries safe friendly Conflict bases from nearest to farthest and uses the same
// stock empty-terrain query as initial HQ vehicles. The caller reserves cap
// capacity before entering this class, so spawn validation and creation are
// atomic on authority.
class AICF_VehicleSpawner
{
	protected static const float SPAWN_SEARCH_RADIUS_METERS = 45.0;
	protected static const float VEHICLE_CLEARANCE_RADIUS_METERS = 8.0;

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

		vector position;
		string siteFailureReason;
		SCR_CampaignMilitaryBaseComponent siteFailureBase;
		SCR_CampaignMilitaryBaseComponent boardingRejectionBase;
		vector boardingRejectionPosition;
		int boardingRejectionAliveCount;
		float boardingRejectionLeaderDistanceMeters;
		float boardingRejectionNearestDistanceMeters;
		float boardingRejectionFarthestDistanceMeters;
		string boardingRejectionMemberSamples;
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

			vector candidatePosition;
			if (!SCR_WorldTools.FindEmptyTerrainPosition(
				candidatePosition,
				safeCandidate.GetOwner().GetOrigin(),
				SPAWN_SEARCH_RADIUS_METERS,
				VEHICLE_CLEARANCE_RADIUS_METERS))
			{
				AppendCandidateTrace(
					candidateTrace,
					safeCandidateKey,
					"NO_EMPTY_TERRAIN",
					candidateDistanceMeters,
					string.Empty);
				actionableCandidateCount++;
				if (siteFailureReason.IsEmpty() || siteFailureReason == "TOO_FAR")
				{
					siteFailureReason = "NO_EMPTY_TERRAIN";
					siteFailureBase = safeCandidate;
				}
				continue;
			}

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
				AppendCandidateTrace(
					candidateTrace,
					safeCandidateKey,
					"GROUP_NOT_READY",
					candidateDistanceMeters,
					"alive=0");
				actionableCandidateCount++;
				siteFailureReason = "GROUP_NOT_READY";
				siteFailureBase = safeCandidate;
				break;
			}
			if (maximumBoardingDistanceMeters > 0 && aliveCount > 0 &&
				farthestDistanceMeters > maximumBoardingDistanceMeters)
			{
				AppendCandidateTrace(
					candidateTrace,
					safeCandidateKey,
					"NO_BOARDING_SITE_WITHIN_RANGE",
					candidateDistanceMeters,
					string.Format(
						"alive=%1|nearest_m=%2|farthest_m=%3|maximum_boarding_m=%4",
						aliveCount,
						nearestDistanceMeters,
						farthestDistanceMeters,
						maximumBoardingDistanceMeters));
				actionableCandidateCount++;
				if (siteFailureReason != "NO_BOARDING_SITE_WITHIN_RANGE")
				{
					siteFailureReason = "NO_BOARDING_SITE_WITHIN_RANGE";
					siteFailureBase = safeCandidate;
					boardingRejectionBase = safeCandidate;
					boardingRejectionPosition = candidatePosition;
					boardingRejectionAliveCount = aliveCount;
					boardingRejectionLeaderDistanceMeters = leaderDistanceMeters;
					boardingRejectionNearestDistanceMeters = nearestDistanceMeters;
					boardingRejectionFarthestDistanceMeters = farthestDistanceMeters;
					boardingRejectionMemberSamples = memberSamples;
				}
				continue;
			}

			spawnBase = safeCandidate;
			position = candidatePosition;
			AppendCandidateTrace(
				candidateTrace,
				safeCandidateKey,
				"SELECTED",
				candidateDistanceMeters,
				string.Format(
					"alive=%1|nearest_m=%2|farthest_m=%3",
					aliveCount,
					nearestDistanceMeters,
					farthestDistanceMeters));
			actionableCandidateCount++;
			break;
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
		if (boardingRejectionBase)
		{
			string reportKey = string.Format(
				"SITE:%1:%2:%3:%4",
				runtime.GetRequestGeneration(),
				runtime.GetSpawnAttempt(),
				AICF_Stage1Diagnostics.BaseKey(boardingRejectionBase),
				"NO_BOARDING_SITE_WITHIN_RANGE");
			if (runtime.MarkSpawnIssueReported(reportKey))
			{
				string siteSample = string.Format("%1,%2,%3", boardingRejectionPosition[0], boardingRejectionPosition[1], boardingRejectionPosition[2]);
				string rejectionDetails = string.Format(
						"%1 base=%2 retryable=1 alive=%3 leader_m=%4 nearest_m=%5 farthest_m=%6 maximum_m=%7 site=[%8] member_samples=[%9]",
						runtime.DescribeContext("NO_BOARDING_SITE_WITHIN_RANGE"),
						AICF_Stage1Diagnostics.BaseKey(boardingRejectionBase),
						boardingRejectionAliveCount,
						boardingRejectionLeaderDistanceMeters,
						boardingRejectionNearestDistanceMeters,
						boardingRejectionFarthestDistanceMeters,
						maximumBoardingDistanceMeters,
						siteSample,
						boardingRejectionMemberSamples);
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
			AICF_Stage3Diagnostics.Info(
				"VEHICLE_SPAWN_PREFLIGHT_READY",
				string.Format("%1 base=%2 owner=%3 contested=0 entity_created=0", runtime.DescribeContext("SAFE_FRIENDLY_BASE"), AICF_Stage1Diagnostics.BaseKey(spawnBase), faction.GetFactionKey()));
			return true;
		}

		AICF_Stage3Diagnostics.Info(
			"VEHICLE_SPAWN_SITE_SELECTED",
			string.Format("%1 base=%2 owner=%3 contested=0", runtime.DescribeContext("SAFE_FRIENDLY_BASE"), AICF_Stage1Diagnostics.BaseKey(spawnBase), faction.GetFactionKey()));

		EntitySpawnParams params = new EntitySpawnParams();
		params.TransformMode = ETransformMode.WORLD;
		params.Transform[3] = position;
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
