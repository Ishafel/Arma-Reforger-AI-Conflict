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

// Immutable-for-the-caller result of the acquisition preflight. Selection and
// entity creation are deliberately separate: WAITING_FOR_SITE can probe without
// holding cap or allocating an entity, and a rejected surface can never advance
// the fleet's accepted vehicle generation.
class AICF_VehicleSpawnSiteSelection
{
	SCR_CampaignMilitaryBaseComponent m_Base;
	SCR_CampaignMilitaryBaseComponent m_FailureBase;
	vector m_vPosition;
	string m_sFailureReason;
	string m_sSurfaceKind;
	float m_fFootprintHeightDeltaMeters = -1.0;
	int m_iCandidatePositionIndex = -1;
	int m_iCandidatePositionCount;
	int m_iSurfaceProbeCount;
	bool m_bRetryable;

	bool IsSelected()
	{
		return m_Base != null && m_sFailureReason.IsEmpty();
	}
}

// A site reservation is intentionally lighter than a fleet lease. It prevents
// two groups from walking to the same delivery pad, but it neither consumes
// vehicle cap nor authorizes entity creation.
class AICF_VehicleSpawnSiteReservation
{
	protected string m_sOwnerToken;
	protected FactionKey m_sFactionKey;
	protected int m_iSlotId;
	protected int m_iGroupGeneration;
	protected int m_iTripGeneration;
	protected int m_iRequestGeneration;
	protected SCR_CampaignMilitaryBaseComponent m_Base;
	protected vector m_vSpawnPosition;
	protected int m_iReservedAtMs;
	protected int m_iExpiresAtMs;

	void AICF_VehicleSpawnSiteReservation(
		string ownerToken,
		FactionKey factionKey,
		int slotId,
		int groupGeneration,
		int tripGeneration,
		int requestGeneration,
		SCR_CampaignMilitaryBaseComponent base,
		vector spawnPosition,
		int reservedAtMs,
		int expiresAtMs)
	{
		m_sOwnerToken = ownerToken;
		m_sFactionKey = factionKey;
		m_iSlotId = slotId;
		m_iGroupGeneration = groupGeneration;
		m_iTripGeneration = tripGeneration;
		m_iRequestGeneration = requestGeneration;
		m_Base = base;
		m_vSpawnPosition = spawnPosition;
		m_iReservedAtMs = reservedAtMs;
		m_iExpiresAtMs = expiresAtMs;
	}

	string GetOwnerToken() { return m_sOwnerToken; }
	FactionKey GetFactionKey() { return m_sFactionKey; }
	int GetSlotId() { return m_iSlotId; }
	int GetGroupGeneration() { return m_iGroupGeneration; }
	int GetTripGeneration() { return m_iTripGeneration; }
	int GetRequestGeneration() { return m_iRequestGeneration; }
	SCR_CampaignMilitaryBaseComponent GetBase() { return m_Base; }
	vector GetSpawnPosition() { return m_vSpawnPosition; }
	int GetReservedAtMs() { return m_iReservedAtMs; }
	int GetExpiresAtMs() { return m_iExpiresAtMs; }

	bool IsExpired(int nowMs)
	{
		return m_iExpiresAtMs > 0 && nowMs >= m_iExpiresAtMs;
	}

	bool MatchesTrip(AICF_TransportTrip trip, int requestGeneration)
	{
		return trip && requestGeneration == m_iRequestGeneration &&
			m_sFactionKey == trip.GetFactionKey() && m_iSlotId == trip.GetSlotId() &&
			m_iGroupGeneration == trip.GetGroupGeneration() &&
			m_iTripGeneration == trip.GetTripGeneration();
	}
}

// Persistent acquisition plan. The exact pad and staging point survive all
// approach polls, so SPAWN_COMMIT cannot silently move the vehicle elsewhere.
class AICF_VehicleSpawnPlan
{
	protected ref AICF_VehicleSpawnSiteSelection m_Selection;
	protected ref AICF_VehicleSpawnSiteReservation m_Reservation;
	protected vector m_vStagingPosition;
	protected AIWaypoint m_ApproachWaypoint;
	protected int m_iPlannedAtMs;
	protected int m_iApproachDeadlineMs;
	protected int m_iAllStagedSinceMs;
	protected int m_iStagedCount;
	protected int m_iAliveCount;
	protected int m_iLastProgressReportAtMs;
	protected int m_iLastReportedStagedCount = -1;
	protected int m_iApproachWaypointIssuedAtMs;
	protected int m_iApproachWaypointReissueCount;

	void AICF_VehicleSpawnPlan(
		AICF_VehicleSpawnSiteSelection selection,
		AICF_VehicleSpawnSiteReservation reservation,
		vector stagingPosition,
		int plannedAtMs,
		int approachDeadlineMs)
	{
		m_Selection = selection;
		m_Reservation = reservation;
		m_vStagingPosition = stagingPosition;
		m_iPlannedAtMs = plannedAtMs;
		m_iApproachDeadlineMs = approachDeadlineMs;
	}

	AICF_VehicleSpawnSiteSelection GetSelection() { return m_Selection; }
	AICF_VehicleSpawnSiteReservation GetReservation() { return m_Reservation; }
	vector GetSpawnPosition() { return m_Selection.m_vPosition; }
	vector GetStagingPosition() { return m_vStagingPosition; }
	AIWaypoint GetApproachWaypoint() { return m_ApproachWaypoint; }
	int GetPlannedAtMs() { return m_iPlannedAtMs; }
	int GetApproachDeadlineMs() { return m_iApproachDeadlineMs; }
	int GetAllStagedSinceMs() { return m_iAllStagedSinceMs; }
	int GetStagedCount() { return m_iStagedCount; }
	int GetAliveCount() { return m_iAliveCount; }
	int GetApproachWaypointReissueCount() { return m_iApproachWaypointReissueCount; }

	bool IsValid()
	{
		return m_Selection && m_Selection.IsSelected() && m_Reservation &&
			m_iPlannedAtMs > 0 && m_iApproachDeadlineMs > m_iPlannedAtMs;
	}

	bool BindApproachWaypoint(AIWaypoint waypoint)
	{
		if (!waypoint || m_ApproachWaypoint)
			return false;
		m_ApproachWaypoint = waypoint;
		return true;
	}

	bool ClearApproachWaypoint(AIWaypoint expected)
	{
		if (!expected || expected != m_ApproachWaypoint)
			return false;
		m_ApproachWaypoint = null;
		return true;
	}

	void RecordApproachWaypointIssued(int nowMs, bool reissue)
	{
		m_iApproachWaypointIssuedAtMs = nowMs;
		if (reissue)
			m_iApproachWaypointReissueCount++;
	}

	bool CanReissueApproachWaypoint(int nowMs, int cooldownMs, int maximumReissues)
	{
		return m_iApproachWaypointIssuedAtMs > 0 &&
			m_iApproachWaypointReissueCount < maximumReissues &&
			nowMs - m_iApproachWaypointIssuedAtMs >= cooldownMs;
	}

	bool ObserveStaging(int stagedCount, int aliveCount, int nowMs, int holdMs)
	{
		bool rosterChanged = aliveCount != m_iAliveCount;
		m_iStagedCount = stagedCount;
		m_iAliveCount = aliveCount;
		if (aliveCount <= 0 || stagedCount != aliveCount)
		{
			m_iAllStagedSinceMs = 0;
			return false;
		}
		// A casualty or a newly attached member changes the set whose continuous
		// presence is being proven, even when every remaining member is in range.
		if (rosterChanged || m_iAllStagedSinceMs <= 0)
			m_iAllStagedSinceMs = nowMs;
		return nowMs - m_iAllStagedSinceMs >= Math.Max(0, holdMs);
	}

	bool ShouldReportProgress(int nowMs, int intervalMs)
	{
		if (m_iLastReportedStagedCount != m_iStagedCount ||
			m_iLastProgressReportAtMs <= 0 ||
			nowMs - m_iLastProgressReportAtMs >= intervalMs)
		{
			m_iLastReportedStagedCount = m_iStagedCount;
			m_iLastProgressReportAtMs = nowMs;
			return true;
		}
		return false;
	}
}

// Per-call search aggregate keeps the Enforce local frame small and makes the
// deterministic base walk explicit. It is not retained by a trip.
class AICF_VehicleSpawnSearchState
{
	ref array<SCR_CampaignMilitaryBaseComponent> m_AllCandidates = {};
	ref array<SCR_CampaignMilitaryBaseComponent> m_SafeCandidates = {};
	string m_sCandidateTrace;
	string m_sFirstRejectionReason;
	string m_sMeaningfulRejectionReason;
	SCR_CampaignMilitaryBaseComponent m_FirstRejectedBase;
	SCR_CampaignMilitaryBaseComponent m_MeaningfulRejectedBase;
	int m_iActionableCandidateCount;
	int m_iHostileCandidateCount;
}

// Tries safe friendly Conflict bases from nearest to farthest and uses the same
// stock empty-terrain query as initial HQ vehicles. New WAIT preflight is
// explicitly cap-free and entity-free; SPAWN_COMMIT reserves a lease before it
// calls the separate authoritative entity-creation helper.
class AICF_VehicleSpawner
{
	// Dense bases on Arland frequently have no eight-metre-clear position inside
	// the old 45 m ring. The wider query stays tied to an authoritative safe base,
	// and every exact result still passes the existing surface and roster fences.
	protected static const float SPAWN_SEARCH_RADIUS_METERS = 90.0;
	protected static const float VEHICLE_CLEARANCE_RADIUS_METERS = 8.0;
	protected static const int MAX_SPAWN_POSITIONS_PER_BASE = 16;
	protected static const float SURFACE_PROBE_RADIUS_METERS = 6.0;
	protected static const float MAX_FOOTPRINT_HEIGHT_DELTA_METERS = 4.0;
	protected static const float SITE_RESERVATION_SEPARATION_METERS = 20.0;
	protected static const float COMMIT_RECHECK_RADIUS_METERS = 3.0;
	protected ref array<ref AICF_VehicleSpawnSiteReservation> m_aSiteReservations = {};

	bool TryReserveSelectedSite(
		AICF_TransportTrip trip,
		AICF_VehicleRequestState requestState,
		AICF_VehicleSpawnSiteSelection selection,
		int nowMs,
		int expiresAtMs,
		out AICF_VehicleSpawnSiteReservation reservation)
	{
		reservation = null;
		if (!trip || !requestState || !selection || !selection.IsSelected() ||
			expiresAtMs <= nowMs)
		{
			return false;
		}
		PurgeExpiredSiteReservations(nowMs);
		foreach (AICF_VehicleSpawnSiteReservation active : m_aSiteReservations)
		{
			if (!active || active.GetBase() != selection.m_Base)
				continue;
			if (vector.DistanceSqXZ(active.GetSpawnPosition(), selection.m_vPosition) <
				SITE_RESERVATION_SEPARATION_METERS * SITE_RESERVATION_SEPARATION_METERS)
			{
				return false;
			}
		}

		string ownerToken = string.Format(
			"site-%1-%2-%3-%4-%5",
			trip.GetFactionKey(),
			trip.GetSlotId(),
			trip.GetGroupGeneration(),
			trip.GetTripGeneration(),
			requestState.GetRequestGeneration());
		reservation = new AICF_VehicleSpawnSiteReservation(
			ownerToken,
			trip.GetFactionKey(),
			trip.GetSlotId(),
			trip.GetGroupGeneration(),
			trip.GetTripGeneration(),
			requestState.GetRequestGeneration(),
			selection.m_Base,
			selection.m_vPosition,
			nowMs,
			expiresAtMs);
		m_aSiteReservations.Insert(reservation);
		return true;
	}

	bool IsSiteReservationCurrent(
		AICF_VehicleSpawnSiteReservation reservation,
		AICF_TransportTrip trip,
		AICF_VehicleRequestState requestState,
		int nowMs)
	{
		PurgeExpiredSiteReservations(nowMs);
		return reservation && trip && requestState &&
			reservation.MatchesTrip(trip, requestState.GetRequestGeneration()) &&
			m_aSiteReservations.Contains(reservation);
	}

	bool ReleaseSelectedSite(AICF_VehicleSpawnSiteReservation expected)
	{
		if (!expected || !m_aSiteReservations.Contains(expected))
			return false;
		m_aSiteReservations.RemoveItem(expected);
		return true;
	}

	bool MeasureStagingReadiness(
		SCR_AIGroup group,
		vector stagingPosition,
		float stagingRadiusMeters,
		out int stagedCount,
		out int aliveCount,
		out float farthestDistanceMeters,
		out string memberSamples)
	{
		stagedCount = 0;
		aliveCount = 0;
		farthestDistanceMeters = -1.0;
		memberSamples = string.Empty;
		if (!group || stagingRadiusMeters <= 0)
			return false;
		array<AIAgent> agents = {};
		group.GetAgents(agents);
		foreach (AIAgent agent : agents)
		{
			IEntity entity;
			if (agent)
				entity = agent.GetControlledEntity();
			if (!AICF_GroupRuntime.IsAliveCharacter(entity))
				continue;
			float distanceMeters = Math.Sqrt(vector.DistanceSqXZ(
				entity.GetOrigin(),
				stagingPosition));
			aliveCount++;
			farthestDistanceMeters = Math.Max(farthestDistanceMeters, distanceMeters);
			if (distanceMeters <= stagingRadiusMeters)
				stagedCount++;
			if (!memberSamples.IsEmpty())
				memberSamples += ",";
			memberSamples += string.Format("%1:%2", entity.GetID(), distanceMeters);
		}
		return aliveCount > 0;
	}

	bool RevalidateSpawnCommit(
		SCR_GameModeCampaign campaign,
		SCR_CampaignFaction faction,
		SCR_AIGroup group,
		AICF_ConflictAdapter conflictAdapter,
		AICF_VehicleSpawnPlan plan,
		float stagingRadiusMeters,
		int minimumAliveCount,
		out int stagedCount,
		out int aliveCount,
		out float farthestDistanceMeters,
		out string failureReason)
	{
		failureReason = "SPAWN_PLAN_INVALID";
		stagedCount = 0;
		aliveCount = 0;
		farthestDistanceMeters = -1.0;
		if (!Replication.IsServer() || !campaign || !campaign.IsMaster() ||
			!faction || !group || !conflictAdapter || !plan || !plan.IsValid())
		{
			return false;
		}

		AICF_VehicleSpawnSiteSelection selection = plan.GetSelection();
		failureReason = conflictAdapter.GetSpawnRejectionReason(selection.m_Base, faction);
		if (!failureReason.IsEmpty())
			return false;
		if (!selection.m_Base.GetOwner())
		{
			failureReason = "BASE_ENTITY_UNAVAILABLE";
			return false;
		}

		string memberSamples;
		if (!MeasureStagingReadiness(
			group,
			plan.GetStagingPosition(),
			stagingRadiusMeters,
			stagedCount,
			aliveCount,
			farthestDistanceMeters,
			memberSamples) || aliveCount < minimumAliveCount)
		{
			failureReason = "GROUP_NOT_COMBAT_READY";
			return false;
		}
		if (stagedCount != aliveCount)
		{
			failureReason = "STAGING_NO_LONGER_CONFIRMED";
			return false;
		}

		string surfaceKind;
		bool waterDetected;
		float footprintDeltaMeters;
		int surfaceProbeCount;
		if (!IsWheeledSpawnSurfaceSuitable(
			selection.m_vPosition,
			selection.m_Base.GetOwner().GetWorld(),
			surfaceKind,
			waterDetected,
			footprintDeltaMeters,
			surfaceProbeCount))
		{
			failureReason = "WATER_OR_UNDRIVABLE_SURFACE";
			return false;
		}

		array<vector> exactPositions = {};
		int exactCount = SCR_WorldTools.FindAllEmptyTerrainPositions(
			exactPositions,
			selection.m_vPosition,
			COMMIT_RECHECK_RADIUS_METERS,
			VEHICLE_CLEARANCE_RADIUS_METERS,
			maxResults: 1,
			world: selection.m_Base.GetOwner().GetWorld());
		if (exactCount <= 0 || exactPositions.IsEmpty() ||
			vector.DistanceSqXZ(exactPositions[0], selection.m_vPosition) >
			COMMIT_RECHECK_RADIUS_METERS * COMMIT_RECHECK_RADIUS_METERS)
		{
			failureReason = "SPAWN_PAD_OCCUPIED";
			return false;
		}
		failureReason = string.Empty;
		return true;
	}

	protected void PurgeExpiredSiteReservations(int nowMs)
	{
		for (int reservationIndex = m_aSiteReservations.Count() - 1; reservationIndex >= 0; reservationIndex--)
		{
			AICF_VehicleSpawnSiteReservation reservation =
				m_aSiteReservations[reservationIndex];
			if (!reservation || reservation.IsExpired(nowMs))
				m_aSiteReservations.Remove(reservationIndex);
		}
	}

	// New acquisition boundary. It performs every strategic/site/surface/member
	// check but never allocates an entity. The caller may use it from a cap-free
	// WAITING_FOR_SITE probe or immediately before a SPAWN_COMMIT attempt.
	bool TrySelectSiteForAcquisition(
		SCR_GameModeCampaign campaign,
		SCR_CampaignFaction faction,
		SCR_AIGroup group,
		AICF_ConflictAdapter conflictAdapter,
		vector preferredPosition,
		float maximumSpawnDistanceMeters,
		float maximumBoardingDistanceMeters,
		string identityContext,
		int requestGeneration,
		int attempt,
		bool preflightOnly,
		out AICF_VehicleSpawnSiteSelection selection)
	{
		selection = new AICF_VehicleSpawnSiteSelection();
		selection.m_sFailureReason = "INVALID_SPAWN_REQUEST";
		if (!Replication.IsServer() || !campaign || !campaign.IsMaster() ||
			!faction || !group || !conflictAdapter)
		{
			return false;
		}

		AICF_VehicleSpawnSearchState search = BuildAcquisitionCandidateSet(
			campaign,
			faction,
			conflictAdapter,
			preferredPosition);
		if (!search)
			return false;

		if (search.m_SafeCandidates.IsEmpty())
		{
			SelectAcquisitionSearchFailure(search, selection);
			selection.m_bRetryable = true;
			ReportAcquisitionCandidateEvaluation(
				search,
				preferredPosition,
				identityContext,
				requestGeneration,
				attempt,
				preflightOnly);
			return false;
		}

		EvaluateAcquisitionSafeSites(
			search,
			faction,
			group,
			preferredPosition,
			maximumSpawnDistanceMeters,
			maximumBoardingDistanceMeters,
			identityContext,
			requestGeneration,
			attempt,
			preflightOnly,
			selection);
		ReportAcquisitionCandidateEvaluation(
			search,
			preferredPosition,
			identityContext,
			requestGeneration,
			attempt,
			preflightOnly);
		if (!selection.IsSelected())
		{
			selection.m_bRetryable = true;
			if (selection.m_sFailureReason.IsEmpty())
				selection.m_sFailureReason = "NO_SAFE_SPAWN_AVAILABLE";
			return false;
		}

		ReportAcquisitionSelectedSite(
			faction,
			selection,
			identityContext,
			requestGeneration,
			attempt,
			preflightOnly);
		return true;
	}

	// The only entity-creating helper used by the new acquisition flow. Fleet
	// binding happens later, after live capacity inspection; therefore this
	// method never advances accepted generation or mutates cap ownership.
	bool TrySpawnSelectedSiteForAcquisition(
		SCR_GameModeCampaign campaign,
		SCR_CampaignFaction faction,
		ResourceName prefab,
		AICF_VehicleSpawnSiteSelection selection,
		string identityContext,
		out Vehicle vehicle,
		out SCR_AIVehicleUsageComponent vehicleUsage,
		out string failureReason)
	{
		vehicle = null;
		vehicleUsage = null;
		failureReason = "INVALID_SPAWN_REQUEST";
		if (!Replication.IsServer() || !campaign || !campaign.IsMaster() ||
			!faction || !selection || !selection.IsSelected() || prefab.IsEmpty())
		{
			return false;
		}

		AICF_Stage3Diagnostics.Info(
			"VEHICLE_SPAWN_SITE_SELECTED",
			FormatAcquisitionSelectedSiteDetails(faction, selection, identityContext));
		vehicle = SpawnSelectedPrefab(prefab, selection.m_vPosition);
		if (!vehicle)
		{
			failureReason = "SPAWN_RETURNED_NULL";
			return false;
		}

		SCR_FactionAffiliationComponent affiliation = SCR_FactionAffiliationComponent.Cast(
			vehicle.FindComponent(SCR_FactionAffiliationComponent));
		if (!affiliation)
		{
			failureReason = "FACTION_COMPONENT_MISSING";
			DeleteUnboundCandidate(vehicle);
			vehicle = null;
			return false;
		}

		affiliation.SetAffiliatedFaction(faction);
		Faction vehicleFaction = affiliation.GetAffiliatedFaction();
		if (!vehicleFaction || vehicleFaction.GetFactionKey() != faction.GetFactionKey())
		{
			failureReason = "FACTION_ASSIGNMENT_FAILED";
			DeleteUnboundCandidate(vehicle);
			vehicle = null;
			return false;
		}

		vehicleUsage = SCR_AIVehicleUsageComponent.Cast(
			vehicle.FindComponent(SCR_AIVehicleUsageComponent));
		if (!vehicleUsage || !vehicleUsage.CanBePiloted() || !vehicleUsage.IsVehicleTypeValid())
		{
			failureReason = "AI_USAGE_INVALID";
			DeleteUnboundCandidate(vehicle);
			vehicle = null;
			vehicleUsage = null;
			return false;
		}

		Physics physicsComponent = vehicle.GetPhysics();
		if (physicsComponent)
			physicsComponent.SetVelocity("0 -0.1 0");
		failureReason = string.Empty;
		return true;
	}

	void DeleteUnboundCandidate(Vehicle vehicle)
	{
		if (!vehicle || !Replication.IsServer())
			return;
		RplComponent.DeleteRplEntity(vehicle, false);
	}

	protected Vehicle SpawnSelectedPrefab(ResourceName prefab, vector position)
	{
		EntitySpawnParams params = new EntitySpawnParams();
		params.TransformMode = ETransformMode.WORLD;
		params.Transform[3] = position;
		return Vehicle.Cast(GetGame().SpawnEntityPrefabEx(prefab, false, params: params));
	}

	protected AICF_VehicleSpawnSearchState BuildAcquisitionCandidateSet(
		SCR_GameModeCampaign campaign,
		SCR_CampaignFaction faction,
		AICF_ConflictAdapter conflictAdapter,
		vector preferredPosition)
	{
		AICF_VehicleSpawnSearchState search = new AICF_VehicleSpawnSearchState();
		SCR_CampaignMilitaryBaseComponent mainBase = faction.GetMainBase();
		if (mainBase)
			search.m_AllCandidates.Insert(mainBase);

		SCR_CampaignMilitaryBaseManager baseManager = campaign.GetBaseManager();
		if (baseManager)
		{
			array<SCR_CampaignMilitaryBaseComponent> bases = {};
			baseManager.GetBases(bases);
			foreach (SCR_CampaignMilitaryBaseComponent base : bases)
			{
				if (base && !search.m_AllCandidates.Contains(base))
					search.m_AllCandidates.Insert(base);
			}
		}

		foreach (SCR_CampaignMilitaryBaseComponent candidate : search.m_AllCandidates)
			ClassifyAcquisitionBase(search, candidate, faction, conflictAdapter, preferredPosition);
		return search;
	}

	protected void ClassifyAcquisitionBase(
		AICF_VehicleSpawnSearchState search,
		SCR_CampaignMilitaryBaseComponent candidate,
		SCR_CampaignFaction faction,
		AICF_ConflictAdapter conflictAdapter,
		vector preferredPosition)
	{
		string candidateKey = AICF_Stage1Diagnostics.BaseKey(candidate);
		string rejectionReason;
		if (!candidate || !candidate.GetOwner())
			rejectionReason = "BASE_ENTITY_UNAVAILABLE";
		else
			rejectionReason = conflictAdapter.GetSpawnRejectionReason(candidate, faction);
		float preferredDistanceMeters = -1.0;
		if (candidate && candidate.GetOwner())
		{
			preferredDistanceMeters = Math.Sqrt(vector.DistanceSqXZ(
				candidate.GetOwner().GetOrigin(),
				preferredPosition));
		}
		if (!rejectionReason.IsEmpty())
		{
			if (rejectionReason == "ENEMY_OWNED")
				search.m_iHostileCandidateCount++;
			else
			{
				AppendCandidateTrace(
					search.m_sCandidateTrace,
					candidateKey,
					rejectionReason,
					preferredDistanceMeters,
					string.Empty);
				search.m_iActionableCandidateCount++;
			}
			if (search.m_sFirstRejectionReason.IsEmpty())
			{
				search.m_sFirstRejectionReason = rejectionReason;
				search.m_FirstRejectedBase = candidate;
			}
			if (rejectionReason != "ENEMY_OWNED" && search.m_sMeaningfulRejectionReason.IsEmpty())
			{
				search.m_sMeaningfulRejectionReason = rejectionReason;
				search.m_MeaningfulRejectedBase = candidate;
			}
			return;
		}

		InsertAcquisitionSafeCandidate(search.m_SafeCandidates, candidate, preferredPosition);
	}

	protected void InsertAcquisitionSafeCandidate(
		array<SCR_CampaignMilitaryBaseComponent> safeCandidates,
		SCR_CampaignMilitaryBaseComponent candidate,
		vector preferredPosition)
	{
		float distanceSq = vector.DistanceSqXZ(candidate.GetOwner().GetOrigin(), preferredPosition);
		string candidateKey = AICF_Stage1Diagnostics.BaseKey(candidate);
		int insertIndex = safeCandidates.Count();
		for (int safeIndex = 0; safeIndex < safeCandidates.Count(); safeIndex++)
		{
			SCR_CampaignMilitaryBaseComponent sortedCandidate = safeCandidates[safeIndex];
			float sortedDistanceSq = vector.DistanceSqXZ(
				sortedCandidate.GetOwner().GetOrigin(),
				preferredPosition);
			string sortedKey = AICF_Stage1Diagnostics.BaseKey(sortedCandidate);
			if (distanceSq < sortedDistanceSq ||
				(distanceSq == sortedDistanceSq && candidateKey.Compare(sortedKey) < 0))
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

	protected void EvaluateAcquisitionSafeSites(
		AICF_VehicleSpawnSearchState search,
		SCR_CampaignFaction faction,
		SCR_AIGroup group,
		vector preferredPosition,
		float maximumSpawnDistanceMeters,
		float maximumBoardingDistanceMeters,
		string identityContext,
		int requestGeneration,
		int attempt,
		bool preflightOnly,
		AICF_VehicleSpawnSiteSelection selection)
	{
		foreach (SCR_CampaignMilitaryBaseComponent candidate : search.m_SafeCandidates)
		{
			float preferredDistanceMeters = Math.Sqrt(vector.DistanceSqXZ(
				candidate.GetOwner().GetOrigin(),
				preferredPosition));
			if (maximumSpawnDistanceMeters > 0 && preferredDistanceMeters > maximumSpawnDistanceMeters)
			{
				AppendCandidateTrace(
					search.m_sCandidateTrace,
					AICF_Stage1Diagnostics.BaseKey(candidate),
					"TOO_FAR",
					preferredDistanceMeters,
					string.Format("maximum_spawn_m=%1", maximumSpawnDistanceMeters));
				search.m_iActionableCandidateCount++;
				RecordAcquisitionSiteFailure(selection, candidate, "TOO_FAR");
				continue;
			}

			AICF_VehicleSpawnSiteEvaluation site = EvaluateAcquisitionBasePositions(
				faction,
				group,
				candidate,
				maximumBoardingDistanceMeters,
				identityContext,
				requestGeneration,
				attempt,
				preflightOnly);
			AppendCandidateTrace(
				search.m_sCandidateTrace,
				AICF_Stage1Diagnostics.BaseKey(candidate),
				site.m_sResult,
				preferredDistanceMeters,
				site.m_sTraceDetails);
			search.m_iActionableCandidateCount++;
			if (site.m_sResult == "SELECTED")
			{
				CopyAcquisitionSelectedSite(site, selection);
				return;
			}
			RecordAcquisitionSiteFailure(selection, candidate, site.m_sResult);
		}
	}

	protected AICF_VehicleSpawnSiteEvaluation EvaluateAcquisitionBasePositions(
		SCR_CampaignFaction faction,
		SCR_AIGroup group,
		SCR_CampaignMilitaryBaseComponent candidateBase,
		float maximumBoardingDistanceMeters,
		string identityContext,
		int requestGeneration,
		int attempt,
		bool preflightOnly)
	{
		AICF_VehicleSpawnSiteEvaluation result = new AICF_VehicleSpawnSiteEvaluation();
		result.m_Base = candidateBase;
		IEntity candidateOwner;
		if (candidateBase)
			candidateOwner = candidateBase.GetOwner();
		if (!candidateOwner)
		{
			result.m_sResult = "BASE_ENTITY_UNAVAILABLE";
			return result;
		}
		vector searchCenter = candidateOwner.GetOrigin();
		SCR_SpawnPoint baseSpawnPoint = candidateBase.GetSpawnPoint();
		if (baseSpawnPoint)
			searchCenter = baseSpawnPoint.GetOrigin();

		array<vector> positions = {};
		result.m_iCandidatePositionCount = SCR_WorldTools.FindAllEmptyTerrainPositions(
			positions,
			searchCenter,
			SPAWN_SEARCH_RADIUS_METERS,
			VEHICLE_CLEARANCE_RADIUS_METERS,
			maxResults: MAX_SPAWN_POSITIONS_PER_BASE,
			world: candidateOwner.GetWorld());
		if (result.m_iCandidatePositionCount <= 0)
		{
			result.m_sResult = "NO_EMPTY_TERRAIN";
			return result;
		}

		int rejectedSurfaceCount;
		bool foundValidatedSurface;
		for (int positionIndex; positionIndex < result.m_iCandidatePositionCount; positionIndex++)
		{
			if (EvaluateAcquisitionPosition(
				result,
				faction,
				group,
				candidateBase,
				positions[positionIndex],
				positionIndex,
				maximumBoardingDistanceMeters,
				identityContext,
				requestGeneration,
				attempt,
				preflightOnly))
			{
				return result;
			}
			if (result.m_sSurfaceKind == "REJECTED")
				rejectedSurfaceCount++;
			else
				foundValidatedSurface = true;
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
		if (result.m_bHasBoardingRejection)
		{
			result.m_sTraceDetails = string.Format(
				"evaluated=%1|maximum_boarding_m=%2|alive=%3|leader_m=%4|nearest_m=%5|farthest_m=%6|member_samples=[%7]",
				result.m_iCandidatePositionCount,
				maximumBoardingDistanceMeters,
				result.m_iAliveCount,
				result.m_fLeaderDistanceMeters,
				result.m_fNearestDistanceMeters,
				result.m_fFarthestDistanceMeters,
				result.m_sMemberSamples);
		}
		else
		{
			result.m_sTraceDetails = string.Format(
				"evaluated=%1|maximum_boarding_m=%2",
				result.m_iCandidatePositionCount,
				maximumBoardingDistanceMeters);
		}
		return result;
	}

	protected bool EvaluateAcquisitionPosition(
		AICF_VehicleSpawnSiteEvaluation result,
		SCR_CampaignFaction faction,
		SCR_AIGroup group,
		SCR_CampaignMilitaryBaseComponent candidateBase,
		vector position,
		int positionIndex,
		float maximumBoardingDistanceMeters,
		string identityContext,
		int requestGeneration,
		int attempt,
		bool preflightOnly)
	{
		string surfaceKind;
		bool waterDetected;
		float footprintDeltaMeters;
		int surfaceProbeCount;
		if (!IsWheeledSpawnSurfaceSuitable(
			position,
			candidateBase.GetOwner().GetWorld(),
			surfaceKind,
			waterDetected,
			footprintDeltaMeters,
			surfaceProbeCount))
		{
			ReportAcquisitionSurfaceRejected(
				faction,
				candidateBase,
				position,
				positionIndex,
				result.m_iCandidatePositionCount,
				identityContext,
				requestGeneration,
				attempt,
				preflightOnly,
				surfaceKind,
				waterDetected,
				footprintDeltaMeters,
				surfaceProbeCount);
			result.m_sSurfaceKind = "REJECTED";
			return false;
		}

		result.m_sSurfaceKind = surfaceKind;
		int aliveCount;
		float leaderDistanceMeters;
		float nearestDistanceMeters;
		float farthestDistanceMeters;
		string memberSamples;
		MeasureAliveGroupDistancesToPosition(
			group,
			position,
			aliveCount,
			leaderDistanceMeters,
			nearestDistanceMeters,
			farthestDistanceMeters,
			memberSamples);
		if (aliveCount <= 0)
		{
			result.m_sResult = "GROUP_NOT_READY";
			result.m_sTraceDetails = "alive=0";
			return true;
		}
		if (maximumBoardingDistanceMeters > 0 &&
			farthestDistanceMeters > maximumBoardingDistanceMeters)
		{
			if (!result.m_bHasBoardingRejection)
			{
				result.m_bHasBoardingRejection = true;
				result.m_vBoardingRejectionPosition = position;
				result.m_iAliveCount = aliveCount;
				result.m_fLeaderDistanceMeters = leaderDistanceMeters;
				result.m_fNearestDistanceMeters = nearestDistanceMeters;
				result.m_fFarthestDistanceMeters = farthestDistanceMeters;
				result.m_sMemberSamples = memberSamples;
			}
			return false;
		}

		result.m_sResult = "SELECTED";
		result.m_vPosition = position;
		result.m_iCandidatePositionIndex = positionIndex;
		result.m_sSurfaceKind = surfaceKind;
		result.m_fFootprintHeightDeltaMeters = footprintDeltaMeters;
		result.m_iSurfaceProbeCount = surfaceProbeCount;
		result.m_sTraceDetails = string.Format(
			"alive=%1|nearest_m=%2|farthest_m=%3|candidate_index=%4",
			aliveCount,
			nearestDistanceMeters,
			farthestDistanceMeters,
			positionIndex);
		return true;
	}

	protected void SelectAcquisitionSearchFailure(
		AICF_VehicleSpawnSearchState search,
		AICF_VehicleSpawnSiteSelection selection)
	{
		selection.m_sFailureReason = search.m_sMeaningfulRejectionReason;
		selection.m_FailureBase = search.m_MeaningfulRejectedBase;
		if (!selection.m_sFailureReason.IsEmpty())
			return;
		selection.m_sFailureReason = search.m_sFirstRejectionReason;
		selection.m_FailureBase = search.m_FirstRejectedBase;
		if (selection.m_sFailureReason.IsEmpty())
			selection.m_sFailureReason = "NO_SAFE_SPAWN_AVAILABLE";
	}

	protected void RecordAcquisitionSiteFailure(
		AICF_VehicleSpawnSiteSelection selection,
		SCR_CampaignMilitaryBaseComponent candidate,
		string reason)
	{
		if (reason.IsEmpty() || reason == "SELECTED")
			return;
		bool replace = selection.m_sFailureReason.IsEmpty() ||
			selection.m_sFailureReason == "TOO_FAR";
		if (reason == "NO_BOARDING_SITE_WITHIN_RANGE" || reason == "GROUP_NOT_READY")
			replace = true;
		if (!replace)
			return;
		selection.m_sFailureReason = reason;
		selection.m_FailureBase = candidate;
	}

	protected void CopyAcquisitionSelectedSite(
		AICF_VehicleSpawnSiteEvaluation site,
		AICF_VehicleSpawnSiteSelection selection)
	{
		selection.m_Base = site.m_Base;
		selection.m_FailureBase = null;
		selection.m_vPosition = site.m_vPosition;
		selection.m_sFailureReason = string.Empty;
		selection.m_sSurfaceKind = site.m_sSurfaceKind;
		selection.m_fFootprintHeightDeltaMeters = site.m_fFootprintHeightDeltaMeters;
		selection.m_iCandidatePositionIndex = site.m_iCandidatePositionIndex;
		selection.m_iCandidatePositionCount = site.m_iCandidatePositionCount;
		selection.m_iSurfaceProbeCount = site.m_iSurfaceProbeCount;
		selection.m_bRetryable = false;
	}

	protected void ReportAcquisitionSurfaceRejected(
		SCR_CampaignFaction faction,
		SCR_CampaignMilitaryBaseComponent candidateBase,
		vector position,
		int positionIndex,
		int positionCount,
		string identityContext,
		int requestGeneration,
		int attempt,
		bool preflightOnly,
		string surfaceKind,
		bool waterDetected,
		float footprintDeltaMeters,
		int surfaceProbeCount)
	{
		string details = identityContext;
		details += string.Format(
			" base=%1 reason=WATER_OR_UNDRIVABLE_SURFACE candidate_index=%2 candidates=%3",
			AICF_Stage1Diagnostics.BaseKey(candidateBase),
			positionIndex,
			positionCount);
		details += string.Format(
			" preflight=%1 request_generation=%2 attempt=%3",
			preflightOnly,
			requestGeneration,
			attempt);
		details += string.Format(
			" origin=[%1,%2,%3] surface=%4 water=%5",
			position[0],
			position[1],
			position[2],
			surfaceKind,
			waterDetected);
		details += string.Format(
			" footprint_delta_m=%1 probes=%2 entity_created=0 generation_committed=0",
			footprintDeltaMeters,
			surfaceProbeCount);
		AICF_Stage35Diagnostics.Info("VEHICLE_SPAWN_CANDIDATE_REJECTED", details);
	}

	protected void ReportAcquisitionCandidateEvaluation(
		AICF_VehicleSpawnSearchState search,
		vector preferredPosition,
		string identityContext,
		int requestGeneration,
		int attempt,
		bool preflightOnly)
	{
		string candidateTrace = search.m_sCandidateTrace;
		if (candidateTrace.IsEmpty())
			candidateTrace = "NONE_ACTIONABLE";
		string mode = "SPAWN_ATTEMPT";
		if (preflightOnly)
			mode = "WAIT_PREFLIGHT";
		string details = identityContext;
		details += string.Format(
			" mode=%1 request_generation=%2 attempt=%3 bases_total=%4",
			mode,
			requestGeneration,
			attempt,
			search.m_AllCandidates.Count());
		details += string.Format(
			" hostile_skipped=%1 safe_candidates=%2 actionable=%3",
			search.m_iHostileCandidateCount,
			search.m_SafeCandidates.Count(),
			search.m_iActionableCandidateCount);
		details += string.Format(
			" preferred=[%1,%2,%3] candidates=[%4]",
			preferredPosition[0],
			preferredPosition[1],
			preferredPosition[2],
			candidateTrace);
		AICF_Stage3Diagnostics.Info("VEHICLE_SPAWN_CANDIDATES_EVALUATED", details);
	}

	protected void ReportAcquisitionSelectedSite(
		SCR_CampaignFaction faction,
		AICF_VehicleSpawnSiteSelection selection,
		string identityContext,
		int requestGeneration,
		int attempt,
		bool preflightOnly)
	{
		if (!preflightOnly)
			return;
		string details = FormatAcquisitionSelectedSiteDetails(faction, selection, identityContext);
		details += string.Format(
			" request_generation=%1 attempt=%2 entity_created=0",
			requestGeneration,
			attempt);
		AICF_Stage3Diagnostics.Info("VEHICLE_SPAWN_PREFLIGHT_READY", details);
	}

	protected string FormatAcquisitionSelectedSiteDetails(
		SCR_CampaignFaction faction,
		AICF_VehicleSpawnSiteSelection selection,
		string identityContext)
	{
		string details = identityContext;
		details += string.Format(
			" base=%1 owner=%2 contested=0",
			AICF_Stage1Diagnostics.BaseKey(selection.m_Base),
			faction.GetFactionKey());
		details += FormatSelectedSurfaceTelemetry(
			selection.m_vPosition,
			selection.m_iCandidatePositionIndex,
			selection.m_iCandidatePositionCount,
			selection.m_sSurfaceKind,
			selection.m_fFootprintHeightDeltaMeters,
			selection.m_iSurfaceProbeCount);
		return details;
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
