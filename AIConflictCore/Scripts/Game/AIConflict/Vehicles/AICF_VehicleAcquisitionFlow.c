// Ephemeral data for one catalog candidate. It lives for one bounded attempt and
// is never authoritative vehicle/lease state; Fleet remains the accepted asset
// owner. The object keeps Enforce method frames small during metadata/live checks.
class AICF_VehicleAcquisitionCandidate
{
	ResourceName m_Prefab;
	int m_iIndex;
	int m_iCount;
	int m_iRequiredSeats;
	int m_iAvailableSeats;
	bool m_bHasPilot;
	bool m_bHasTurret;
	bool m_bEntityCreated;
	bool m_bEntityDeleted;
	bool m_bTryNextPrefab;
	string m_sFailureReason;
	Vehicle m_Vehicle;
}

// One bounded request/spawn phase. This component owns acquisition-local state
// and engine spawn through AICF_VehicleSpawner. It never transitions a trip,
// invokes another flow, restores infantry orders, starts boarding or cleans up
// an accepted asset; every decision is returned as AICF_TripOutcome.
class AICF_VehicleAcquisitionFlow
{
	protected static const int HARD_MAX_ATTEMPTS = 4;
	protected static const int HARD_MAX_BACKOFF_MS = 60000;

	protected ref AICF_Stage3Config m_Config;
	protected ref AICF_VehicleCatalog m_Catalog;
	protected ref AICF_VehicleSpawner m_Spawner;
	protected ref AICF_VehicleWatchdog m_Watchdog;
	protected ref AICF_GroupCohesionPolicy m_CohesionPolicy;
	protected SCR_GameModeCampaign m_Campaign;
	protected AICF_ConflictAdapter m_ConflictAdapter;

	void AICF_VehicleAcquisitionFlow(
		AICF_Stage3Config config,
		SCR_GameModeCampaign campaign,
		AICF_ConflictAdapter conflictAdapter,
		AICF_GroupCohesionPolicy cohesionPolicy)
	{
		m_Config = config;
		m_Campaign = campaign;
		m_ConflictAdapter = conflictAdapter;
		m_CohesionPolicy = cohesionPolicy;
		m_Catalog = new AICF_VehicleCatalog();
		m_Spawner = new AICF_VehicleSpawner();
		m_Watchdog = new AICF_VehicleWatchdog();
	}

	AICF_TripOutcome Update(
		AICF_TransportTrip trip,
		AICF_StrategicAssignmentSnapshot currentAssignment,
		AICF_FactionFleet fleet,
		SCR_CampaignFaction faction,
		int nowMs)
	{
		if (!IsAuthorityContextValid(trip, currentAssignment, fleet, faction))
			return FailClosed(trip, "ACQUISITION_IDENTITY_INVALID", "IDENTITY_GUARD");
		if (trip.IsTerminal())
			return FailClosed(trip, "ACQUISITION_ON_TERMINAL_TRIP", "TERMINAL_GUARD");

		AICF_VehicleRequestState requestState = trip.GetRequestState();
		if (!requestState)
			return FailClosed(trip, "REQUEST_STATE_MISSING", "STATE_GUARD");
		EnsureRequestStarted(trip, requestState, nowMs);

		AICF_TripOutcome contextOutcome = ObserveAssignmentContext(
			trip,
			currentAssignment,
			fleet,
			requestState,
			nowMs);
		if (contextOutcome)
			return contextOutcome;

		if (trip.IsDeadlineReached(nowMs) || requestState.IsDeadlineReached(nowMs))
			return EndRequest(trip, fleet, requestState, "REQUEST_DEADLINE_EXHAUSTED", nowMs);

		switch (trip.GetPhase())
		{
			case AICF_ETransportTripPhase.WAITING_FOR_SITE:
				return ProcessWaitingForSite(trip, fleet, faction, requestState, nowMs);
			case AICF_ETransportTripPhase.ACQUIRING:
				return ProcessAcquiring(trip, fleet, faction, requestState, nowMs);
		}

		return FailClosed(trip, "ACQUISITION_PHASE_MISMATCH", "PHASE_GUARD");
	}

	protected bool IsAuthorityContextValid(
		AICF_TransportTrip trip,
		AICF_StrategicAssignmentSnapshot currentAssignment,
		AICF_FactionFleet fleet,
		SCR_CampaignFaction faction)
	{
		if (!Replication.IsServer() || !m_Config || !m_Campaign ||
			!m_Campaign.IsMaster() || !m_ConflictAdapter || !trip || !trip.IsValid() ||
			!currentAssignment || !currentAssignment.IsValid() || !fleet || !faction)
		{
			return false;
		}
		if (!trip.IsCurrent(currentAssignment))
			return false;
		return trip.GetFactionKey() == fleet.GetFactionKey() &&
			trip.GetFactionKey() == faction.GetFactionKey();
	}

	protected void EnsureRequestStarted(
		AICF_TransportTrip trip,
		AICF_VehicleRequestState requestState,
		int nowMs)
	{
		if (requestState.GetRequestGeneration() > 0)
			return;
		int maximumAttempts = Math.Min(HARD_MAX_ATTEMPTS, m_Config.GetSpawnMaxAttempts());
		requestState.Begin(nowMs, trip.GetAbsoluteDeadlineMs(), maximumAttempts);
		string causationId = BuildCausationId(trip, requestState, "REQUESTED");
		AICF_Stage3Diagnostics.Info(
			"VEHICLE_REQUESTED",
			BuildIdentityContext(trip, requestState, trip.GetLease(), causationId) +
			" reason=LONG_RANGE_OPERATIONAL_ORDER cap_reserved=0");
	}

	protected AICF_TripOutcome ObserveAssignmentContext(
		AICF_TransportTrip trip,
		AICF_StrategicAssignmentSnapshot currentAssignment,
		AICF_FactionFleet fleet,
		AICF_VehicleRequestState requestState,
		int nowMs)
	{
		AICF_StrategicAssignmentSnapshot observed = trip.GetAssignment();
		if (currentAssignment.GetAssignmentRevision() < observed.GetAssignmentRevision() ||
			currentAssignment.GetBaseRevision() < observed.GetBaseRevision())
		{
			return FailClosed(trip, "STALE_ASSIGNMENT_REVISION", "CONTEXT_GUARD");
		}

		bool targetChanged = currentAssignment.GetTargetBase() != observed.GetTargetBase();
		bool assignmentChanged = currentAssignment.GetAssignmentRevision() !=
			observed.GetAssignmentRevision();
		bool baseChanged = currentAssignment.GetBaseRevision() != observed.GetBaseRevision();
		if (!targetChanged && !assignmentChanged && !baseChanged)
			return null;

		string reason = "BASE_REVISION_CHANGED";
		if (targetChanged || assignmentChanged)
			reason = "TARGET_CHANGED";
		int oldRequestGeneration = requestState.GetRequestGeneration();
		int discardedAttempts = requestState.GetAttemptCount();
		requestState.RecordContextReset(nowMs, reason);
		string causationId = BuildCausationId(trip, requestState, "CONTEXT_CHANGED");
		string details = BuildIdentityContext(trip, requestState, null, causationId);
		details += string.Format(
			" reason=%1 old_request_generation=%2 new_request_generation=%3 discarded_attempts=%4",
			reason,
			oldRequestGeneration,
			requestState.GetRequestGeneration(),
			discardedAttempts);
		details += string.Format(
			" old_target=%1 new_target=%2 old_base_revision=%3 new_base_revision=%4",
			AICF_Stage1Diagnostics.BaseKey(observed.GetTargetBase()),
			AICF_Stage1Diagnostics.BaseKey(currentAssignment.GetTargetBase()),
			observed.GetBaseRevision(),
			currentAssignment.GetBaseRevision());
		AICF_Stage3Diagnostics.Info("VEHICLE_REQUEST_CONTEXT_CHANGED", details);
		return AICF_TripOutcome.Retry(
			"RELEASE_RESERVATION_FOR_RETARGET:" + reason,
			causationId,
			nowMs);
	}

	protected AICF_TripOutcome ProcessWaitingForSite(
		AICF_TransportTrip trip,
		AICF_FactionFleet fleet,
		SCR_CampaignFaction faction,
		AICF_VehicleRequestState requestState,
		int nowMs)
	{
		if (trip.HasLease())
			return FailClosed(trip, "WAITING_FOR_SITE_WITH_LEASE", "WAIT_LEASE_GUARD");

		AICF_TripOutcome cohesionOutcome = ObserveBoundedCohesion(
			trip,
			fleet,
			requestState,
			nowMs);
		if (cohesionOutcome)
			return cohesionOutcome;
		if (requestState.GetNextAttemptAtMs() > nowMs)
		{
			return AICF_TripOutcome.Wait(
				"WAIT_PROBE_PENDING",
				BuildCausationId(trip, requestState, "WAIT_PENDING"));
		}

		int aliveCount = AICF_GroupRuntime.CountAliveAgents(trip.GetAssignment().GetGroup());
		if (aliveCount < m_Config.GetMinimumVehicleRequestAgents())
			return EndRequest(trip, fleet, requestState, "GROUP_NOT_COMBAT_READY", nowMs);

		AICF_EVehicleKind kind;
		if (!TryResolveVehicleKind(trip.GetAssignment(), kind))
			return FailClosed(trip, "VEHICLE_KIND_NOT_CONFIGURED", "KIND_POLICY");
		array<ResourceName> candidates = {};
		string causationId = BuildCausationId(trip, requestState, "WAIT_PREFLIGHT");
		string identity = BuildIdentityContext(trip, requestState, null, causationId);
		if (!m_Catalog.GetCandidatePrefabsForAcquisition(
			faction,
			kind,
			trip.GetAssignment(),
			identity,
			candidates))
		{
			return EndRequest(trip, fleet, requestState, "PREFAB_UNAVAILABLE", nowMs);
		}
		if (!HasConservativeCapacityCandidate(candidates, kind, aliveCount))
			return EndRequest(trip, fleet, requestState, "INSUFFICIENT_COMPARTMENTS", nowMs);

		if (!fleet.CanReserveLease(trip.GetAssignment()))
		{
			string admissionReason = ResolveLeaseAdmissionFailure(trip, fleet);
			if (admissionReason != "VEHICLE_CAP_UNAVAILABLE")
				return FailClosed(trip, admissionReason, "WAIT_LEASE_ADMISSION_GUARD");
			bool firstCapBlock = requestState.GetLastFailureReason() != "VEHICLE_CAP_UNAVAILABLE";
			int nextProbeAtMs = nowMs + m_Config.GetWaitProbeIntervalMs();
			requestState.EnterWaitingForSite(nowMs, nextProbeAtMs, "VEHICLE_CAP_UNAVAILABLE");
			if (firstCapBlock)
				ReportCapBlocked(trip, fleet, requestState, "WAIT_PREFLIGHT");
			ReportWaitHeartbeat(
				trip,
				fleet,
				requestState,
				"VEHICLE_CAP_UNAVAILABLE",
				nowMs,
				nextProbeAtMs);
			return AICF_TripOutcome.Wait("VEHICLE_CAP_UNAVAILABLE", causationId);
		}

		AICF_VehicleSpawnSiteSelection selection;
		bool siteReady = m_Spawner.TrySelectSiteForAcquisition(
			m_Campaign,
			faction,
			trip.GetAssignment().GetGroup(),
			m_ConflictAdapter,
			ResolveGroupPosition(trip.GetAssignment().GetGroup()),
			m_Config.GetMaximumSpawnDistanceMeters(),
			m_Config.GetMaximumReuseDistanceMeters(),
			identity,
			requestState.GetRequestGeneration(),
			requestState.GetAttemptCount(),
			true,
			selection);
		if (!siteReady)
		{
			if (!selection || !selection.m_bRetryable)
				return EndRequest(trip, fleet, requestState, "SITE_PREFLIGHT_TERMINAL", nowMs);
			int nextProbeAtMs = nowMs + m_Config.GetWaitProbeIntervalMs();
			requestState.EnterWaitingForSite(nowMs, nextProbeAtMs, selection.m_sFailureReason);
			ReportWaitHeartbeat(
				trip,
				fleet,
				requestState,
				selection.m_sFailureReason,
				nowMs,
				nextProbeAtMs);
			return AICF_TripOutcome.Wait(selection.m_sFailureReason, causationId);
		}

		int oldRequestGeneration = requestState.GetRequestGeneration();
		int waitAgeMs = GetWaitAgeMs(requestState, nowMs);
		requestState.RecordContextReset(nowMs, "ELIGIBLE_SITE_PREFLIGHT");
		requestState.ExitWaitingForSite();
		causationId = BuildCausationId(trip, requestState, "WAIT_RESOLVED");
		ReportWaitingExit(
			trip,
			requestState,
			selection,
			oldRequestGeneration,
			waitAgeMs,
			causationId);
		return AICF_TripOutcome.Retry("ELIGIBLE_SITE_PREFLIGHT", causationId, nowMs);
	}

	protected AICF_TripOutcome ProcessAcquiring(
		AICF_TransportTrip trip,
		AICF_FactionFleet fleet,
		SCR_CampaignFaction faction,
		AICF_VehicleRequestState requestState,
		int nowMs)
	{
		AICF_TripOutcome leaseOutcome = EnsureReservedLease(
			trip,
			fleet,
			requestState,
			nowMs);
		if (leaseOutcome)
			return leaseOutcome;

		AICF_VehicleLease lease = trip.GetLease();
		if (lease.HasPhysicalAsset())
		{
			return AICF_TripOutcome.StartBoarding(
				"ACCEPTED_ASSET_ALREADY_BOUND",
				BuildCausationId(trip, requestState, "BOUND_ASSET"));
		}
		if (lease.GetState() != AICF_EVehicleLeaseState.RESERVED)
			return FailClosed(trip, "ACQUISITION_LEASE_NOT_RESERVED", "LEASE_GUARD");

		if (!requestState.CanAttempt(nowMs))
		{
			if (requestState.GetAttemptCount() >= requestState.GetMaximumAttempts())
				return MoveToWaiting(trip, fleet, requestState, "ATTEMPTS_EXHAUSTED", nowMs);
			return AICF_TripOutcome.Wait(
				"SPAWN_BACKOFF",
				BuildCausationId(trip, requestState, "BACKOFF"));
		}
		if (!requestState.BeginAttempt(nowMs))
			return FailClosed(trip, "SPAWN_ATTEMPT_REJECTED", "ATTEMPT_GUARD");

		AICF_EVehicleKind kind;
		if (!TryResolveVehicleKind(trip.GetAssignment(), kind))
			return EndRequest(trip, fleet, requestState, "VEHICLE_KIND_NOT_CONFIGURED", nowMs);
		string causationId = BuildCausationId(trip, requestState, "SPAWN_ATTEMPT");
		string identity = BuildIdentityContext(trip, requestState, lease, causationId);
		ReportSpawnAttempt(trip, requestState, identity);

		array<ResourceName> candidates = {};
		if (!m_Catalog.GetCandidatePrefabsForAcquisition(
			faction,
			kind,
			trip.GetAssignment(),
			identity,
			candidates))
		{
			return EndRequest(trip, fleet, requestState, "PREFAB_UNAVAILABLE", nowMs);
		}

		AICF_VehicleSpawnSiteSelection selection;
		bool siteReady = m_Spawner.TrySelectSiteForAcquisition(
			m_Campaign,
			faction,
			trip.GetAssignment().GetGroup(),
			m_ConflictAdapter,
			ResolveGroupPosition(trip.GetAssignment().GetGroup()),
			m_Config.GetMaximumSpawnDistanceMeters(),
			m_Config.GetMaximumReuseDistanceMeters(),
			identity,
			requestState.GetRequestGeneration(),
			requestState.GetAttemptCount(),
			false,
			selection);
		if (!siteReady)
		{
			if (selection && selection.m_bRetryable)
				return HandleRetryableFailure(
					trip,
					fleet,
					requestState,
					selection,
					nowMs);
			return EndRequest(trip, fleet, requestState, "SPAWN_SITE_TERMINAL", nowMs);
		}

		return SpawnAcceptedCandidate(
			trip,
			fleet,
			faction,
			requestState,
			lease,
			kind,
			candidates,
			selection,
			identity,
			causationId,
			nowMs);
	}

	protected AICF_TripOutcome EnsureReservedLease(
		AICF_TransportTrip trip,
		AICF_FactionFleet fleet,
		AICF_VehicleRequestState requestState,
		int nowMs)
	{
		if (trip.HasLease())
		{
			AICF_VehicleLease existing = trip.GetLease();
			if (!existing.MatchesTripIdentity(
				trip.GetFactionKey(),
				trip.GetSlotId(),
				trip.GetGroupGeneration(),
				trip.GetTripGeneration()))
			{
				return FailClosed(trip, "LEASE_TRIP_IDENTITY_MISMATCH", "LEASE_GUARD");
			}
			return null;
		}

		if (!fleet.CanReserveLease(trip.GetAssignment()))
		{
			string admissionReason = ResolveLeaseAdmissionFailure(trip, fleet);
			if (admissionReason != "VEHICLE_CAP_UNAVAILABLE")
				return FailClosed(trip, admissionReason, "LEASE_ADMISSION_GUARD");
			bool firstCapBlock = requestState.GetLastFailureReason() != "VEHICLE_CAP_UNAVAILABLE";
			int nextProbeAtMs = nowMs + m_Config.GetWaitProbeIntervalMs();
			requestState.EnterWaitingForSite(nowMs, nextProbeAtMs, "VEHICLE_CAP_UNAVAILABLE");
			if (firstCapBlock)
				ReportCapBlocked(trip, fleet, requestState, "LEASE_RESERVATION");
			ReportRequestWaiting(trip, requestState, "VEHICLE_CAP_UNAVAILABLE", nextProbeAtMs);
			return AICF_TripOutcome.Retry(
				"ENTER_WAITING_FOR_SITE:VEHICLE_CAP_UNAVAILABLE",
				BuildCausationId(trip, requestState, "CAP_WAIT"),
				nextProbeAtMs);
		}

		AICF_VehicleLease lease;
		if (!fleet.TryReserveLease(trip.GetAssignment(), trip.GetTripGeneration(), lease))
			return FailClosed(trip, "LEASE_RESERVATION_COMMIT_REJECTED", "LEASE_RESERVATION");
		return AICF_TripOutcome.Retry(
			"LEASE_RESERVED_FOR_ATTACH",
			BuildCausationId(trip, requestState, "LEASE_RESERVED"),
			nowMs);
	}

	protected AICF_TripOutcome SpawnAcceptedCandidate(
		AICF_TransportTrip trip,
		AICF_FactionFleet fleet,
		SCR_CampaignFaction faction,
		AICF_VehicleRequestState requestState,
		AICF_VehicleLease lease,
		AICF_EVehicleKind kind,
		array<ResourceName> candidates,
		AICF_VehicleSpawnSiteSelection selection,
		string identity,
		string causationId,
		int nowMs)
	{
		int requiredSeats = AICF_GroupRuntime.CountAliveAgents(trip.GetAssignment().GetGroup());
		for (int candidateIndex; candidateIndex < candidates.Count(); candidateIndex++)
		{
			AICF_VehicleAcquisitionCandidate candidate = CreateCandidate(
				candidates[candidateIndex],
				candidateIndex,
				candidates.Count(),
				requiredSeats);
			if (!PreflightCandidate(trip, kind, candidates, candidate, identity))
				continue;
			if (!SpawnCandidate(trip, faction, kind, candidates, selection, candidate, identity))
			{
				if (candidate.m_bTryNextPrefab)
					continue;
				if (IsRetryableSpawnFailure(candidate.m_sFailureReason))
				{
					selection.m_sFailureReason = candidate.m_sFailureReason;
					selection.m_FailureBase = selection.m_Base;
					selection.m_bRetryable = true;
					return HandleRetryableFailure(
						trip,
						fleet,
						requestState,
						selection,
						nowMs);
				}
				return EndRequest(
					trip,
					fleet,
					requestState,
					candidate.m_sFailureReason,
					nowMs,
					true);
			}
			if (!ValidateLiveCandidate(trip, kind, candidates, candidate, identity))
				continue;
			return BindAcceptedCandidate(
				trip,
				fleet,
				faction,
				requestState,
				lease,
				kind,
				selection,
				candidate,
				causationId,
				nowMs);
		}

		return EndRequest(trip, fleet, requestState, "INSUFFICIENT_COMPARTMENTS", nowMs);
	}

	protected AICF_VehicleAcquisitionCandidate CreateCandidate(
		ResourceName prefab,
		int candidateIndex,
		int candidateCount,
		int requiredSeats)
	{
		AICF_VehicleAcquisitionCandidate candidate = new AICF_VehicleAcquisitionCandidate();
		candidate.m_Prefab = prefab;
		candidate.m_iIndex = candidateIndex;
		candidate.m_iCount = candidateCount;
		candidate.m_iRequiredSeats = requiredSeats;
		return candidate;
	}

	protected bool PreflightCandidate(
		AICF_TransportTrip trip,
		AICF_EVehicleKind kind,
		array<ResourceName> candidates,
		AICF_VehicleAcquisitionCandidate candidate,
		string identity)
	{
		int availableSeats;
		bool hasPilot;
		bool hasTurret;
		bool metadataKnown = m_Catalog.TryGetConservativeCapacity(
			candidate.m_Prefab,
			kind,
			availableSeats,
			hasPilot,
			hasTurret);
		candidate.m_iAvailableSeats = availableSeats;
		candidate.m_bHasPilot = hasPilot;
		candidate.m_bHasTurret = hasTurret;
		bool accepted = metadataKnown && IsCapacityAccepted(
			kind,
			candidate.m_iRequiredSeats,
			candidate.m_iAvailableSeats,
			candidate.m_bHasPilot,
			candidate.m_bHasTurret);
		ReportCandidateCapacity(trip, kind, candidate, accepted, "CATALOG", identity);
		if (accepted)
			return true;
		ReportCandidateFallback(
			trip,
			kind,
			candidates,
			candidate,
			"CAPACITY_PREFLIGHT_REJECTED",
			identity);
		return false;
	}

	protected bool SpawnCandidate(
		AICF_TransportTrip trip,
		SCR_CampaignFaction faction,
		AICF_EVehicleKind kind,
		array<ResourceName> candidates,
		AICF_VehicleSpawnSiteSelection selection,
		AICF_VehicleAcquisitionCandidate candidate,
		string identity)
	{
		SCR_AIVehicleUsageComponent usage;
		Vehicle spawnedVehicle;
		string failureReason;
		if (m_Spawner.TrySpawnSelectedSiteForAcquisition(
			m_Campaign,
			faction,
			candidate.m_Prefab,
			selection,
			identity,
			spawnedVehicle,
			usage,
			failureReason))
		{
			candidate.m_Vehicle = spawnedVehicle;
			candidate.m_bEntityCreated = true;
			return true;
		}
		candidate.m_sFailureReason = failureReason;
		candidate.m_bEntityCreated = DidSpawnFailureCreateCandidate(candidate.m_sFailureReason);
		candidate.m_bEntityDeleted = candidate.m_bEntityCreated;
		candidate.m_bTryNextPrefab = CanTryNextPrefab(
			kind,
			candidate.m_iIndex,
			candidates,
			candidate.m_sFailureReason);
		if (candidate.m_bTryNextPrefab)
		{
			ReportCandidateFallback(
				trip,
				kind,
				candidates,
				candidate,
				candidate.m_sFailureReason,
				identity);
		}
		return false;
	}

	protected bool ValidateLiveCandidate(
		AICF_TransportTrip trip,
		AICF_EVehicleKind kind,
		array<ResourceName> candidates,
		AICF_VehicleAcquisitionCandidate candidate,
		string identity)
	{
		int availableSeats;
		bool hasPilot;
		bool hasTurret;
		bool accepted = m_Watchdog.InspectVehicleCapacity(
			candidate.m_Vehicle,
			kind,
			candidate.m_iRequiredSeats,
			availableSeats,
			hasPilot,
			hasTurret);
		candidate.m_iAvailableSeats = availableSeats;
		candidate.m_bHasPilot = hasPilot;
		candidate.m_bHasTurret = hasTurret;
		ReportCandidateCapacity(trip, kind, candidate, accepted, "LIVE", identity);
		if (accepted)
			return true;
		RollbackUnacceptedCandidate(candidate);
		ReportCandidateFallback(
			trip,
			kind,
			candidates,
			candidate,
			"LIVE_CAPACITY_REJECTED",
			identity);
		return false;
	}

	protected AICF_TripOutcome BindAcceptedCandidate(
		AICF_TransportTrip trip,
		AICF_FactionFleet fleet,
		SCR_CampaignFaction faction,
		AICF_VehicleRequestState requestState,
		AICF_VehicleLease lease,
		AICF_EVehicleKind kind,
		AICF_VehicleSpawnSiteSelection selection,
		AICF_VehicleAcquisitionCandidate candidate,
		string causationId,
		int nowMs)
	{
		RplComponent rpl = RplComponent.Cast(candidate.m_Vehicle.FindComponent(RplComponent));
		string rplId;
		if (rpl)
			rplId = rpl.Id().ToString();
		if (rplId.IsEmpty())
		{
			RollbackUnacceptedCandidate(candidate);
			return EndRequest(trip, fleet, requestState, "RPL_IDENTITY_MISSING", nowMs, true);
		}
		if (!fleet.BindReservedLeaseVehicle(
			lease,
			candidate.m_Vehicle,
			rplId,
			candidate.m_Prefab,
			kind,
			candidate.m_iAvailableSeats,
			selection.m_vPosition))
		{
			RollbackUnacceptedCandidate(candidate);
			return EndRequest(trip, fleet, requestState, "FLEET_BIND_REJECTED", nowMs, true);
		}
		string details = BuildIdentityContext(trip, requestState, lease, causationId);
		details += string.Format(
			" reason=SPAWN_SUCCESS prefab=%1 base=%2 assigned_faction=%3 capacity=%4",
			candidate.m_Prefab,
			AICF_Stage1Diagnostics.BaseKey(selection.m_Base),
			faction.GetFactionKey(),
			candidate.m_iAvailableSeats);
		AICF_Stage3Diagnostics.Info("VEHICLE_SPAWNED", details);
		return AICF_TripOutcome.StartBoarding("NEW_VEHICLE", causationId);
	}

	protected void RollbackUnacceptedCandidate(AICF_VehicleAcquisitionCandidate candidate)
	{
		if (!candidate || !candidate.m_Vehicle)
			return;
		m_Spawner.DeleteUnboundCandidate(candidate.m_Vehicle);
		candidate.m_Vehicle = null;
		candidate.m_bEntityDeleted = true;
	}

	protected AICF_TripOutcome HandleRetryableFailure(
		AICF_TransportTrip trip,
		AICF_FactionFleet fleet,
		AICF_VehicleRequestState requestState,
		AICF_VehicleSpawnSiteSelection selection,
		int nowMs)
	{
		string reason = selection.m_sFailureReason;
		int attempt = requestState.GetAttemptCount();
		if (attempt >= requestState.GetMaximumAttempts())
			return MoveToWaiting(trip, fleet, requestState, reason, nowMs);

		int nextAttemptAtMs = nowMs + CalculateRetryDelayMs(attempt, reason);
		requestState.ScheduleRetry(nextAttemptAtMs, reason);
		string causationId = BuildCausationId(trip, requestState, "RETRY_SCHEDULED");
		string details = BuildIdentityContext(trip, requestState, trip.GetLease(), causationId);
		details += string.Format(
			" reason=%1 base=%2 retryable=1 retry_at_ms=%3 maximum_attempts=%4",
			reason,
			AICF_Stage1Diagnostics.BaseKey(selection.m_FailureBase),
			requestState.GetNextAttemptAtMs(),
			requestState.GetMaximumAttempts());
		if (reason == "SPAWN_FACTION_INITIALIZING")
			AICF_Stage3Diagnostics.Info("VEHICLE_SPAWN_DEFERRED", details);
		else
			AICF_Stage3Diagnostics.Warning("VEHICLE_SPAWN_SITE_REJECTED", details);
		return AICF_TripOutcome.Retry(reason, causationId, requestState.GetNextAttemptAtMs());
	}

	protected AICF_TripOutcome MoveToWaiting(
		AICF_TransportTrip trip,
		AICF_FactionFleet fleet,
		AICF_VehicleRequestState requestState,
		string reason,
		int nowMs)
	{
		int nextProbeAtMs = nowMs + m_Config.GetWaitProbeIntervalMs();
		requestState.EnterWaitingForSite(nowMs, nextProbeAtMs, reason);
		ReportRequestWaiting(trip, requestState, reason, nextProbeAtMs);
		return AICF_TripOutcome.Retry(
			"ENTER_WAITING_FOR_SITE:" + reason,
			BuildCausationId(trip, requestState, "ENTER_WAIT"),
			nextProbeAtMs);
	}

	protected AICF_TripOutcome EndRequest(
		AICF_TransportTrip trip,
		AICF_FactionFleet fleet,
		AICF_VehicleRequestState requestState,
		string reason,
		int nowMs,
		bool failClosed = false)
	{
		string outcomeFields =
			" outcome=BOUNDED_INFANTRY_FALLBACK vehicle_retry_suppressed=1 terminal_fail_closed=0";
		if (failClosed)
			outcomeFields =
				" outcome=TERMINAL_FAIL_CLOSED vehicle_retry_suppressed=1 terminal_fail_closed=1";
		string causationId = BuildCausationId(trip, requestState, "REQUEST_ENDED");
		string details = BuildIdentityContext(trip, requestState, null, causationId);
		details += string.Format(
			" reason=%1 wait_age_ms=%2 total_wait_age_ms=%3",
			reason,
			GetWaitAgeMs(requestState, nowMs),
			GetTotalWaitAgeMs(requestState, nowMs));
		details += string.Format(
			" cumulative_attempts=%1 target=%2 request_generation=%3",
			requestState.GetTotalAttemptCount(),
			AICF_Stage1Diagnostics.BaseKey(trip.GetAssignment().GetTargetBase()),
			requestState.GetRequestGeneration());
		details += outcomeFields;
		AICF_Stage35Diagnostics.Info("WAITING_FOR_SITE_EXIT", details);
		if (failClosed)
			return AICF_TripOutcome.TerminalFailClosed(reason, causationId);
		return AICF_TripOutcome.FallbackToFoot(reason, causationId);
	}

	protected AICF_TripOutcome ObserveBoundedCohesion(
		AICF_TransportTrip trip,
		AICF_FactionFleet fleet,
		AICF_VehicleRequestState requestState,
		int nowMs)
	{
		string waitReason = requestState.GetLastFailureReason();
		bool cohesionScoped = waitReason == "NO_BOARDING_SITE_WITHIN_RANGE" ||
			waitReason == "POST_APPROACH_COHESION_WAIT";
		if (!cohesionScoped)
		{
			requestState.ObserveCohesion(false, 0, nowMs);
			return null;
		}

		int aliveCount;
		float farthestFromLeaderMeters;
		float maximumPairMeters;
		string memberSamples;
		bool measured = m_Watchdog.MeasureAliveGroupSpread(
			trip.GetAssignment().GetGroup(),
			aliveCount,
			farthestFromLeaderMeters,
			maximumPairMeters,
			memberSamples);
		bool fragmented = measured && aliveCount > 1 &&
			maximumPairMeters > m_Config.GetCohesionDistanceMeters();
		int cohesionAgeMs = requestState.ObserveCohesion(fragmented, maximumPairMeters, nowMs);
		if (!fragmented)
			return null;

		int deadlineMs = m_Config.GetCohesionWaitTimeoutMs();
		if (!requestState.WasCohesionRecoveryAttempted() && cohesionAgeMs >= deadlineMs / 2)
		{
			requestState.MarkCohesionRecoveryAttempted();
			bool normalized = false;
			if (m_CohesionPolicy)
				normalized = m_CohesionPolicy.NormalizeAfterMovementFailure(
					trip.GetAssignment().GetGroup());
			string orderOutcome = "NORMALIZATION_REJECTED";
			if (normalized)
				orderOutcome = "NORMALIZATION_ACCEPTED";
			ReportCohesionOutcome(
				trip,
				requestState,
				"RECOVERY_ISSUED",
				cohesionAgeMs,
				aliveCount,
				farthestFromLeaderMeters,
				maximumPairMeters,
				normalized,
				orderOutcome,
				memberSamples);
			return AICF_TripOutcome.Wait(
				"COHESION_RECOVERY_ISSUED",
				BuildCausationId(trip, requestState, "COHESION_HALF"));
		}
		if (cohesionAgeMs < deadlineMs)
			return null;

		ReportCohesionOutcome(
			trip,
			requestState,
			"VEHICLE_REQUEST_ENDED",
			cohesionAgeMs,
			aliveCount,
			farthestFromLeaderMeters,
			maximumPairMeters,
			false,
			"INFANTRY_FALLBACK_REQUESTED",
			memberSamples);
		return EndRequest(
			trip,
			fleet,
			requestState,
			"BOARDING_RANGE_WAIT_EXHAUSTED",
			nowMs);
	}

	protected bool TryResolveVehicleKind(
		AICF_StrategicAssignmentSnapshot assignment,
		out AICF_EVehicleKind kind)
	{
		int slotId = assignment.GetSlotId();
		if (slotId < 0 || slotId >= AICF_Stage1Config.GROUP_SLOTS_PER_FACTION)
			return false;
		if (slotId < m_Config.GetTransportVehiclesPerFaction())
		{
			if (assignment.GetSlotKey() == "A0" || assignment.GetSlotKey() == "A1")
				kind = AICF_EVehicleKind.TRANSPORT;
			else
				kind = AICF_EVehicleKind.LIGHT_TRANSPORT;
			return true;
		}
		if (slotId - m_Config.GetTransportVehiclesPerFaction() <
			m_Config.GetArmedLightVehiclesPerFaction())
		{
			kind = AICF_EVehicleKind.ARMED_LIGHT;
			return true;
		}
		return false;
	}

	protected string ResolveLeaseAdmissionFailure(
		AICF_TransportTrip trip,
		AICF_FactionFleet fleet)
	{
		if (!trip || !fleet)
			return "LEASE_ADMISSION_CONTEXT_INVALID";
		AICF_StrategicAssignmentSnapshot assignment = trip.GetAssignment();
		if (!assignment || !assignment.IsValid())
			return "LEASE_ASSIGNMENT_INVALID";
		if (assignment.GetFactionKey() != fleet.GetFactionKey())
			return "LEASE_FACTION_IDENTITY_MISMATCH";
		if (fleet.HasLeaseForSlot(assignment.GetSlotId()))
			return "LEASE_SLOT_COLLISION";
		if (fleet.GetActiveOrReservedCount() >= fleet.GetMaximumActiveOrReserved())
			return "VEHICLE_CAP_UNAVAILABLE";
		return "LEASE_ADMISSION_REJECTED";
	}

	protected bool HasConservativeCapacityCandidate(
		array<ResourceName> candidates,
		AICF_EVehicleKind kind,
		int requiredSeats)
	{
		foreach (ResourceName candidate : candidates)
		{
			int capacity;
			bool hasPilot;
			bool hasTurret;
			if (!m_Catalog.TryGetConservativeCapacity(
				candidate,
				kind,
				capacity,
				hasPilot,
				hasTurret))
			{
				continue;
			}
			if (IsCapacityAccepted(kind, requiredSeats, capacity, hasPilot, hasTurret))
				return true;
		}
		return false;
	}

	protected bool IsCapacityAccepted(
		AICF_EVehicleKind kind,
		int requiredSeats,
		int availableSeats,
		bool hasPilot,
		bool hasTurret)
	{
		if (requiredSeats <= 0 || availableSeats < requiredSeats || !hasPilot)
			return false;
		return kind != AICF_EVehicleKind.ARMED_LIGHT ||
			(hasTurret && requiredSeats >= 2);
	}

	protected bool CanTryNextPrefab(
		AICF_EVehicleKind kind,
		int candidateIndex,
		array<ResourceName> candidates,
		string failureReason)
	{
		if (kind != AICF_EVehicleKind.LIGHT_TRANSPORT ||
			candidateIndex + 1 >= candidates.Count())
		{
			return false;
		}
		return failureReason == "AI_USAGE_INVALID" ||
			failureReason == "SPAWN_RETURNED_NULL" ||
			failureReason == "FACTION_COMPONENT_MISSING" ||
			failureReason == "FACTION_ASSIGNMENT_FAILED";
	}

	protected bool DidSpawnFailureCreateCandidate(string failureReason)
	{
		return failureReason == "AI_USAGE_INVALID" ||
			failureReason == "FACTION_COMPONENT_MISSING" ||
			failureReason == "FACTION_ASSIGNMENT_FAILED";
	}

	protected bool IsRetryableSpawnFailure(string failureReason)
	{
		return failureReason == "SPAWN_RETURNED_NULL";
	}

	protected int CalculateRetryDelayMs(int attempt, string reason)
	{
		int maximumBackoffMs = Math.Min(
			HARD_MAX_BACKOFF_MS,
			m_Config.GetRetryBackoffMaxMs());
		int delayMs = Math.Min(m_Config.GetRetryIntervalMs(), maximumBackoffMs);
		if (reason == "SPAWN_FACTION_INITIALIZING")
			return Math.Min(delayMs, 1000);
		for (int step = 1; step < attempt; step++)
			delayMs = Math.Min(delayMs * 2, maximumBackoffMs);
		return delayMs;
	}

	protected vector ResolveGroupPosition(SCR_AIGroup group)
	{
		if (!group)
			return vector.Zero;
		IEntity leader = AICF_GroupRuntime.ResolveAliveLeader(group);
		if (leader)
			return leader.GetOrigin();
		return group.GetOrigin();
	}

	protected string BuildCausationId(
		AICF_TransportTrip trip,
		AICF_VehicleRequestState requestState,
		string action)
	{
		int requestGeneration;
		int attempt;
		if (requestState)
		{
			requestGeneration = requestState.GetRequestGeneration();
			attempt = requestState.GetAttemptCount();
		}
		return string.Format(
			"%1:ACQ:%2:%3:%4",
			trip.GetOperationId(),
			requestGeneration,
			attempt,
			action);
	}

	protected string BuildIdentityContext(
		AICF_TransportTrip trip,
		AICF_VehicleRequestState requestState,
		AICF_VehicleLease lease,
		string causationId)
	{
		int requestGeneration;
		int attempt;
		if (requestState)
		{
			requestGeneration = requestState.GetRequestGeneration();
			attempt = requestState.GetAttemptCount();
		}
		int leaseGeneration;
		int vehicleGeneration;
		string lifecycleId = "UNBOUND";
		string entityId = "NONE";
		string rplId = "NONE";
		string prefab = "NONE";
		string kind = "NONE";
		if (lease)
		{
			leaseGeneration = lease.GetLeaseGeneration();
			vehicleGeneration = lease.GetVehicleGeneration();
			if (!lease.GetVehicleLifecycleId().IsEmpty())
				lifecycleId = lease.GetVehicleLifecycleId();
			if (!lease.GetEntityIdString().IsEmpty())
				entityId = lease.GetEntityIdString();
			if (!lease.GetRplId().IsEmpty())
				rplId = lease.GetRplId();
			if (!lease.GetPrefab().IsEmpty())
			{
				prefab = lease.GetPrefab();
				kind = AICF_Stage3Diagnostics.KindToString(lease.GetKind());
			}
		}
		string tripState = typename.EnumToString(AICF_ETransportTripPhase, trip.GetPhase());

		string details = string.Format(
			"faction=%1 slot=%2 numeric_slot=%3 group_generation=%4 trip_generation=%5",
			trip.GetFactionKey(),
			trip.GetSlotKey(),
			trip.GetSlotId(),
			trip.GetGroupGeneration(),
			trip.GetTripGeneration());
		details += string.Format(
			" request_generation=%1 attempt=%2 lease_generation=%3 vehicle_generation=%4",
			requestGeneration,
			attempt,
			leaseGeneration,
			vehicleGeneration);
		details += string.Format(
			" vehicle_lifecycle_id=%1 vehicle=%2 entity=%3 entity_id=%4 rpl_id=%5",
			lifecycleId,
			entityId,
			entityId,
			entityId,
			rplId);
		details += string.Format(
			" kind=%1 state=%2 prefab=%3",
			kind,
			tripState,
			prefab);
		details += string.Format(
			" operation_id=%1 causation_id=%2",
			trip.GetOperationId(),
			causationId);
		return details;
	}

	protected void ReportSpawnAttempt(
		AICF_TransportTrip trip,
		AICF_VehicleRequestState requestState,
		string identity)
	{
		string details = identity;
		details += string.Format(
			" reason=SPAWN_PREFLIGHT maximum_attempts=%1 base_revision=%2 total_attempts=%3",
			requestState.GetMaximumAttempts(),
			trip.GetAssignment().GetBaseRevision(),
			requestState.GetTotalAttemptCount());
		AICF_Stage3Diagnostics.Info("VEHICLE_SPAWN_ATTEMPT", details);
	}

	protected void ReportCandidateCapacity(
		AICF_TransportTrip trip,
		AICF_EVehicleKind kind,
		AICF_VehicleAcquisitionCandidate candidate,
		bool accepted,
		string stage,
		string identity)
	{
		ReportCapacity(
			trip,
			kind,
			candidate.m_Prefab,
			candidate.m_iIndex,
			candidate.m_iCount,
			candidate.m_iRequiredSeats,
			candidate.m_iAvailableSeats,
			candidate.m_bHasPilot,
			candidate.m_bHasTurret,
			accepted,
			stage,
			candidate.m_bEntityCreated,
			identity);
	}

	protected void ReportCandidateFallback(
		AICF_TransportTrip trip,
		AICF_EVehicleKind kind,
		array<ResourceName> candidates,
		AICF_VehicleAcquisitionCandidate candidate,
		string reason,
		string identity)
	{
		ReportTransportFallback(
			trip,
			kind,
			candidate.m_Prefab,
			candidate.m_iIndex,
			candidates,
			reason,
			candidate.m_iRequiredSeats,
			candidate.m_iAvailableSeats,
			candidate.m_bEntityCreated,
			candidate.m_bEntityDeleted,
			identity);
	}

	protected void ReportCapacity(
		AICF_TransportTrip trip,
		AICF_EVehicleKind kind,
		ResourceName prefab,
		int candidateIndex,
		int candidateCount,
		int requiredSeats,
		int availableSeats,
		bool hasPilot,
		bool hasTurret,
		bool accepted,
		string stage,
		bool entityCreated,
		string identity)
	{
		string details = identity;
		details += string.Format(
			" kind=%1 candidate_index=%2 candidates=%3 prefab=%4 stage=%5",
			AICF_Stage3Diagnostics.KindToString(kind),
			candidateIndex,
			candidateCount,
			prefab,
			stage);
		details += string.Format(
			" required=%1 available=%2 pilot=%3 turret=%4 accepted=%5 policy=ALL_OR_FALLBACK",
			requiredSeats,
			availableSeats,
			hasPilot,
			hasTurret,
			accepted);
		details += string.Format(
			" entity_created=%1 generation_committed=0",
			entityCreated);
		AICF_Stage35Diagnostics.Info("VEHICLE_CAPACITY_PREFLIGHT", details);
	}

	protected void ReportTransportFallback(
		AICF_TransportTrip trip,
		AICF_EVehicleKind kind,
		ResourceName rejectedPrefab,
		int candidateIndex,
		array<ResourceName> candidates,
		string reason,
		int requiredSeats,
		int availableSeats,
		bool entityCreated,
		bool entityDeleted,
		string identity)
	{
		if (kind != AICF_EVehicleKind.LIGHT_TRANSPORT ||
			candidateIndex + 1 >= candidates.Count())
		{
			return;
		}
		string details = identity;
		details += string.Format(
			" rejected_prefab=%1 reason=%2 required=%3 available=%4",
			rejectedPrefab,
			reason,
			requiredSeats,
			availableSeats);
		details += string.Format(
			" next_prefab=%1 fallback_kind=TRANSPORT entity_created=%2 entity_deleted=%3 generation_committed=0",
			candidates[candidateIndex + 1],
			entityCreated,
			entityDeleted);
		AICF_Stage35Diagnostics.Info("VEHICLE_TRANSPORT_FALLBACK", details);
	}

	protected void ReportRequestWaiting(
		AICF_TransportTrip trip,
		AICF_VehicleRequestState requestState,
		string reason,
		int nextProbeAtMs)
	{
		string causationId = BuildCausationId(trip, requestState, "REQUEST_WAITING");
		string details = BuildIdentityContext(trip, requestState, null, causationId);
		details += string.Format(
			" reason=%1 attempts=%2 next_probe_at_ms=%3 cap_reserved=0",
			reason,
			requestState.GetAttemptCount(),
			nextProbeAtMs);
		AICF_Stage3Diagnostics.Info("VEHICLE_REQUEST_WAITING", details);
	}

	protected void ReportCapBlocked(
		AICF_TransportTrip trip,
		AICF_FactionFleet fleet,
		AICF_VehicleRequestState requestState,
		string gate)
	{
		AICF_EVehicleKind requestedKind;
		string requestedKindName = "NONE";
		if (TryResolveVehicleKind(trip.GetAssignment(), requestedKind))
			requestedKindName = AICF_Stage3Diagnostics.KindToString(requestedKind);
		string causationId = BuildCausationId(trip, requestState, "CAP_BLOCKED");
		string details = BuildIdentityContext(trip, requestState, null, causationId);
		details += string.Format(
			" gate=%1 active=%2 reserved=%3 active_or_reserved=%4",
			gate,
			fleet.GetActiveCount(),
			fleet.GetReservedCount(),
			fleet.GetActiveOrReservedCount());
		details += string.Format(
			" reason=VEHICLE_CAP_UNAVAILABLE requested_kind=%1 limit=%2 cap_reserved=0 next_action=WAITING_FOR_SITE",
			requestedKindName,
			fleet.GetMaximumActiveOrReserved());
		AICF_Stage3Diagnostics.Info("VEHICLE_CAP_BLOCKED", details);
	}

	protected void ReportWaitHeartbeat(
		AICF_TransportTrip trip,
		AICF_FactionFleet fleet,
		AICF_VehicleRequestState requestState,
		string reason,
		int nowMs,
		int nextProbeAtMs)
	{
		string causationId = BuildCausationId(trip, requestState, "WAIT_HEARTBEAT");
		string details = BuildIdentityContext(trip, requestState, null, causationId);
		details += string.Format(
			" reason=%1 wait_age_ms=%2 total_wait_age_ms=%3 next_probe_at_ms=%4",
			reason,
			GetWaitAgeMs(requestState, nowMs),
			GetTotalWaitAgeMs(requestState, nowMs),
			nextProbeAtMs);
		details += string.Format(
			" cumulative_attempts=%1 context_resets=%2 context_reset_reason=%3 cohesion_spread_m=%4 cohesion_wait_age_ms=%5",
			requestState.GetTotalAttemptCount(),
			requestState.GetContextResetCount(),
			requestState.GetContextResetReason(),
			requestState.GetCohesionSpreadMeters(),
			GetCohesionWaitAgeMs(requestState, nowMs));
		details += string.Format(
			" active_or_reserved=%1 limit=%2 cap_reserved=0",
			fleet.GetActiveOrReservedCount(),
			fleet.GetMaximumActiveOrReserved());
		AICF_Stage3Diagnostics.Info("VEHICLE_SPAWN_WAIT_HEARTBEAT", details);
	}

	protected void ReportWaitingExit(
		AICF_TransportTrip trip,
		AICF_VehicleRequestState requestState,
		AICF_VehicleSpawnSiteSelection selection,
		int oldRequestGeneration,
		int waitAgeMs,
		string causationId)
	{
		string details = BuildIdentityContext(trip, requestState, null, causationId);
		details += string.Format(
			" outcome=ELIGIBLE_SITE_PREFLIGHT wait_age_ms=%1 cumulative_attempts=%2 old_request_generation=%3",
			waitAgeMs,
			requestState.GetTotalAttemptCount(),
			oldRequestGeneration);
		details += string.Format(
			" base_revision=%1 target=%2 base=%3 cap_reserved=0 vehicle_retry_suppressed=0",
			trip.GetAssignment().GetBaseRevision(),
			AICF_Stage1Diagnostics.BaseKey(trip.GetAssignment().GetTargetBase()),
			AICF_Stage1Diagnostics.BaseKey(selection.m_Base));
		AICF_Stage35Diagnostics.Info("WAITING_FOR_SITE_EXIT", details);
		AICF_Stage3Diagnostics.Info(
			"VEHICLE_REQUEST_RESUMED",
			details + " wake=ELIGIBLE_SITE_PREFLIGHT entity_created=0");
	}

	protected void ReportCohesionOutcome(
		AICF_TransportTrip trip,
		AICF_VehicleRequestState requestState,
		string outcome,
		int waitAgeMs,
		int aliveCount,
		float farthestFromLeaderMeters,
		float maximumPairMeters,
		bool normalized,
		string orderOutcome,
		string memberSamples)
	{
		string waitReason = requestState.GetLastFailureReason();
		int deadlineMs = m_Config.GetCohesionWaitTimeoutMs();
		string causationId = BuildCausationId(trip, requestState, "COHESION");
		string details = BuildIdentityContext(trip, requestState, null, causationId);
		details += string.Format(
			" outcome=%1 wait_reason=%2 wait_age_ms=%3 deadline_ms=%4 alive=%5",
			outcome,
			waitReason,
			waitAgeMs,
			deadlineMs,
			aliveCount);
		details += string.Format(
			" farthest_from_leader_m=%1 maximum_pair_m=%2 threshold_m=%3 normalized=%4 order_outcome=%5",
			farthestFromLeaderMeters,
			maximumPairMeters,
			m_Config.GetCohesionDistanceMeters(),
			normalized,
			orderOutcome);
		details += string.Format(" members=[%1]", memberSamples);
		AICF_Stage35Diagnostics.Info("COHESION_OUTCOME", details);
	}

	protected int GetWaitAgeMs(AICF_VehicleRequestState requestState, int nowMs)
	{
		if (requestState.GetWaitingStartedAtMs() <= 0)
			return 0;
		return Math.Max(0, nowMs - requestState.GetWaitingStartedAtMs());
	}

	protected int GetTotalWaitAgeMs(AICF_VehicleRequestState requestState, int nowMs)
	{
		if (requestState.GetTotalWaitingStartedAtMs() <= 0)
			return 0;
		return Math.Max(0, nowMs - requestState.GetTotalWaitingStartedAtMs());
	}

	protected int GetCohesionWaitAgeMs(AICF_VehicleRequestState requestState, int nowMs)
	{
		if (!requestState || requestState.GetCohesionStartedAtMs() <= 0)
			return 0;
		return Math.Max(0, nowMs - requestState.GetCohesionStartedAtMs());
	}

	protected AICF_TripOutcome FailClosed(
		AICF_TransportTrip trip,
		string reason,
		string action)
	{
		string causationId = "ACQUISITION:INVALID";
		if (trip)
			causationId = BuildCausationId(trip, trip.GetRequestState(), action);
		return AICF_TripOutcome.TerminalFailClosed(reason, causationId);
	}
}
