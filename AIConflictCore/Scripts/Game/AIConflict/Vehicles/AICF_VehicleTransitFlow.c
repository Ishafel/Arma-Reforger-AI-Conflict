// Sole owner of route creation, transit progress, exact crew recovery and
// guarded mobility recovery. It never transitions a Trip and never calls any
// other phase flow, controller, handoff or cleanup component.
class AICF_VehicleTransitFlow
{
	protected static const int MOTION_REPORT_INTERVAL_MS = 10000;
	protected static const int CREW_ACTION_TELEMETRY_INTERVAL_MS = 10000;
	protected static const int RECOVERY_RETRY_DELAY_MS = 1000;
	protected static const int HARD_MAX_CREW_RECOVERIES = 2;
	protected static const int HARD_MAX_MOBILITY_RECOVERIES = 2;
	protected static const float UNSTUCK_PLAYER_RADIUS_METERS = 35.0;
	protected static const float UNSTUCK_OFFSET_METERS = 8.0;
	protected static const float UNSTUCK_SEARCH_RADIUS_METERS = 3.0;
	protected static const float UNSTUCK_MIN_DISPLACEMENT_METERS = 4.0;
	protected static const float UNSTUCK_MAX_DISPLACEMENT_METERS = 15.0;
	protected static const float UNSTUCK_HAZARD_CLEARANCE_METERS = 6.0;

	protected ref AICF_Stage3Config m_Config;
	protected ref AICF_VehicleWaypointFactory m_WaypointFactory;
	protected ref AICF_VehicleWatchdog m_Watchdog;
	protected bool m_bUnstuckHazardDetected;

	void AICF_VehicleTransitFlow(
		AICF_Stage3Config config,
		AICF_VehicleWaypointFactory waypointFactory = null,
		AICF_VehicleWatchdog watchdog = null)
	{
		m_Config = config;
		if (!m_Config)
			m_Config = new AICF_Stage3Config();
		m_WaypointFactory = waypointFactory;
		if (!m_WaypointFactory)
			m_WaypointFactory = new AICF_VehicleWaypointFactory();
		m_Watchdog = watchdog;
		if (!m_Watchdog)
			m_Watchdog = new AICF_VehicleWatchdog();
	}

	AIWaypoint GetRouteWaypoint(AICF_TransportTrip trip)
	{
		if (!trip)
			return null;
		return trip.GetMovementState().GetRouteWaypoint();
	}

	AIWaypoint GetSupersededRouteWaypoint(AICF_TransportTrip trip)
	{
		if (!trip)
			return null;
		return trip.GetMovementState().GetSupersededRouteWaypoint();
	}

	bool ConfirmRouteWaypointBound(
		AICF_TransportTrip trip,
		AIWaypoint expected,
		bool boundToCurrentGroup)
	{
		if (!IsTripLeaseIdentityCurrent(trip))
			return false;
		return trip.GetMovementState().ConfirmRouteWaypointBound(
			expected,
			boundToCurrentGroup);
	}

	bool ConfirmSupersededWaypointRemoved(
		AICF_TransportTrip trip,
		AIWaypoint expected)
	{
		if (!trip || !trip.GetMovementState())
			return false;
		return trip.GetMovementState().ConfirmSupersededRouteWaypointRemoved(expected);
	}

	bool ConfirmRouteWaypointRemoved(
		AICF_TransportTrip trip,
		AIWaypoint expected)
	{
		if (!trip || !trip.GetMovementState())
			return false;
		return trip.GetMovementState().ClearRouteWaypoint(expected);
	}

	string InspectRouteAssetFailure(AICF_TransportTrip trip)
	{
		if (!trip || !trip.GetLease() || !trip.GetLease().HasPhysicalAsset())
			return "VEHICLE_DESTROYED";
		return InspectVehicleFailure(trip.GetLease().GetVehicle());
	}

	AICF_TripOutcome PrepareRoute(AICF_TransportTrip trip, string causationId)
	{
		string invalidReason;
		if (!ValidateContext(trip, causationId, false, invalidReason))
			return AICF_TripOutcome.TerminalFailClosed(invalidReason, causationId);
		string vehicleFailure = InspectRouteAssetFailure(trip);
		if (!vehicleFailure.IsEmpty())
		{
			AbortOwnedActions(trip);
			return AICF_TripOutcome.FallbackToFoot(vehicleFailure, causationId);
		}

		int nowMs = System.GetTickCount();
		if (trip.IsDeadlineReached(nowMs))
			return AICF_TripOutcome.FallbackToFoot("TRANSIT_DEADLINE_EXPIRED", causationId);
		AICF_VehicleMovementState state = trip.GetMovementState();
		EnsureStateStarted(trip, state, nowMs);
		if (state.GetSupersededRouteWaypoint())
			return AICF_TripOutcome.StartMovement("ROUTE_RECONCILIATION_REQUIRED", causationId);

		AICF_StrategicAssignmentSnapshot assignment = trip.GetAssignment();
		if (state.GetRouteWaypoint() &&
			state.GetAssignmentRevision() == assignment.GetAssignmentRevision())
		{
			if (!state.IsRouteWaypointBound())
				return AICF_TripOutcome.StartMovement("ROUTE_BIND_REQUIRED", causationId);
			return AICF_TripOutcome.Wait("ROUTE_ALREADY_CURRENT", causationId);
		}

		Vehicle vehicle = trip.GetLease().GetVehicle();
		float targetDistanceMeters = vector.DistanceXZ(
			vehicle.GetOrigin(),
			assignment.GetTargetPosition());
		if (targetDistanceMeters <= m_Config.GetDismountDistanceMeters())
			return AICF_TripOutcome.StartDismount("TACTICAL_TARGET_WITHIN_DISMOUNT_RANGE", causationId);

		bool safeReuseRetarget = state.GetAssignmentRevision() >= 0 &&
			state.GetAssignmentRevision() != assignment.GetAssignmentRevision();
		return CreateAndStageRoute(
			trip,
			causationId,
			safeReuseRetarget,
			false,
			string.Empty,
			false,
			false);
	}

	AICF_TripOutcome Tick(AICF_TransportTrip trip, string causationId)
	{
		string invalidReason;
		if (!ValidateContext(trip, causationId, true, invalidReason))
			return AICF_TripOutcome.TerminalFailClosed(invalidReason, causationId);
		string vehicleFailure = InspectRouteAssetFailure(trip);
		if (!vehicleFailure.IsEmpty())
		{
			AbortOwnedActions(trip);
			return AICF_TripOutcome.FallbackToFoot(vehicleFailure, causationId);
		}

		int nowMs = System.GetTickCount();
		if (trip.IsDeadlineReached(nowMs))
		{
			AbortOwnedActions(trip);
			return AICF_TripOutcome.FallbackToFoot("TRANSIT_DEADLINE_EXPIRED", causationId);
		}

		AICF_VehicleMovementState state = trip.GetMovementState();
		EnsureStateStarted(trip, state, nowMs);
		// Exact crew recovery owns a deliberately suspended route. Advance that
		// token before interpreting superseded/current waypoint shape as route
		// reconciliation work.
		AICF_TripOutcome crewOutcome = ProcessMandatoryCrew(trip, causationId, nowMs);
		if (crewOutcome)
			return crewOutcome;
		if (state.GetAssignmentRevision() != trip.GetAssignment().GetAssignmentRevision())
			return PrepareRoute(trip, causationId);
		if (state.GetSupersededRouteWaypoint())
			return AICF_TripOutcome.StartMovement("ROUTE_RECONCILIATION_REQUIRED", causationId);
		if (!state.GetRouteWaypoint())
		{
			if (state.IsCrewRecoveryRoutePending())
				return PrepareCrewRecoveryRoute(trip, causationId);
			return PrepareRoute(trip, causationId);
		}
		if (!state.IsRouteWaypointBound())
			return AICF_TripOutcome.StartMovement("ROUTE_BIND_REQUIRED", causationId);

		Vehicle vehicle = trip.GetLease().GetVehicle();
		float targetDistanceMeters = vector.DistanceXZ(
			vehicle.GetOrigin(),
			state.GetTacticalTarget());
		if (targetDistanceMeters <= m_Config.GetDismountDistanceMeters())
		{
			AbortOwnedActions(trip);
			return AICF_TripOutcome.StartDismount("DISEMBARK_POSITION_REACHED", causationId);
		}

		if (!m_Watchdog.IsGroupCohesiveAroundVehicle(
			trip.GetAssignment().GetGroup(),
			vehicle,
			m_Config.GetCohesionDistanceMeters()))
		{
			return AICF_TripOutcome.FallbackToFoot("GROUP_COHESION_EXCEEDED", causationId);
		}

		return ObserveProgressAndRecover(
			trip,
			causationId,
			nowMs,
			targetDistanceMeters);
	}

	void AbortOwnedActions(AICF_TransportTrip trip)
	{
		if (!trip || !trip.GetMovementState())
			return;
		AICF_VehicleMovementState state = trip.GetMovementState();
		AICF_VehicleCrewRecoveryToken token = state.GetCrewRecoveryToken();
		if (!token)
			return;
		token.CancelOwnerSafe();
		state.ClearCrewRecoveryToken(token);
	}

	protected void EnsureStateStarted(
		AICF_TransportTrip trip,
		AICF_VehicleMovementState state,
		int nowMs)
	{
		if (state.GetStartedAtMs() > 0)
			return;
		int maximumRecoveries = Math.Min(
			HARD_MAX_MOBILITY_RECOVERIES,
			Math.Max(0, m_Config.GetMaxRecoveries()));
		int maximumCrewRecoveries = Math.Min(
			HARD_MAX_CREW_RECOVERIES,
			Math.Max(0, m_Config.GetMaxRecoveries()));
		state.Begin(
			nowMs,
			trip.GetAbsoluteDeadlineMs(),
			maximumCrewRecoveries,
			maximumRecoveries);
	}

	protected AICF_TripOutcome CreateAndStageRoute(
		AICF_TransportTrip trip,
		string causationId,
		bool safeReuseRetarget,
		bool recovery,
		string recoveryReason,
		bool requireRouteProgress,
		bool unstuckRelocated)
	{
		AICF_VehicleMovementState state = trip.GetMovementState();
		AICF_StrategicAssignmentSnapshot assignment = trip.GetAssignment();
		Vehicle vehicle = trip.GetLease().GetVehicle();
		vector tacticalTarget = assignment.GetTargetPosition();
		vector routeEndpoint;
		string routeMode;
		AIWaypoint waypoint = m_WaypointFactory.CreateMoveWaypoint(
			vehicle.GetOrigin(),
			tacticalTarget,
			m_Config.GetDismountDistanceMeters(),
			routeEndpoint,
			routeMode);
		if (!waypoint)
			return AICF_TripOutcome.FallbackToFoot("VEHICLE_ROUTE_WAYPOINT_FAILED", causationId);

		int nowMs = System.GetTickCount();
		if (!state.StageRouteWaypoint(
			waypoint,
			tacticalTarget,
			routeEndpoint,
			routeMode,
			assignment.GetAssignmentRevision(),
			vehicle.GetOrigin(),
			nowMs))
		{
			return AICF_TripOutcome.TerminalFailClosedWithWaypoint(
				"ROUTE_STATE_STAGE_REJECTED",
				causationId,
				waypoint);
		}

		if (recovery)
		{
			state.ArmRecoveryEvidence(
				recoveryReason,
				requireRouteProgress,
				unstuckRelocated,
				vehicle.GetOrigin(),
				vector.DistanceXZ(vehicle.GetOrigin(), routeEndpoint),
				nowMs);
		}
		if (state.IsCrewRecoveryRoutePending())
			state.ClearCrewRecoveryRoutePending();
		ReportRouteAssigned(trip, safeReuseRetarget, recovery, recoveryReason);
		return AICF_TripOutcome.StartMovement("ROUTE_READY_FOR_HANDOFF_BIND", causationId);
	}

	protected AICF_TripOutcome PrepareCrewRecoveryRoute(
		AICF_TransportTrip trip,
		string causationId)
	{
		return CreateAndStageRoute(
			trip,
			causationId,
			false,
			true,
			"CREW_RECOVERY",
			false,
			false);
	}

	protected AICF_TripOutcome ObserveProgressAndRecover(
		AICF_TransportTrip trip,
		string causationId,
		int nowMs,
		float targetDistanceMeters)
	{
		AICF_VehicleMovementState state = trip.GetMovementState();
		Vehicle vehicle = trip.GetLease().GetVehicle();
		float routeDistanceMeters = vector.DistanceXZ(
			vehicle.GetOrigin(),
			state.GetRouteEndpoint());
		bool routeProgress = state.ObserveRouteProgress(
			routeDistanceMeters,
			m_Config.GetProgressMeters(),
			nowMs);
		bool physicalMotion = state.ObservePhysicalMotion(
			vehicle.GetOrigin(),
			m_Config.GetMotionMeters(),
			nowMs);
		ReportProgress(trip, routeProgress, physicalMotion, routeDistanceMeters, targetDistanceMeters, nowMs);
		if (state.CanConfirmRecoveryEvidence())
			ReportAndConfirmRecovery(trip, routeDistanceMeters, targetDistanceMeters);

		bool stationary = nowMs - state.GetLastPhysicalMotionAtMs() >=
			m_Config.GetStuckTimeoutMs();
		bool objectiveStalled = nowMs - state.GetLastRouteProgressAtMs() >=
			m_Config.GetObjectiveProgressTimeoutMs();
		if (!stationary && !objectiveStalled)
			return AICF_TripOutcome.Wait("TRANSIT_PROGRESS_WITHIN_DEADLINES", causationId);

		// VehicleCanMove is damage evidence only. It is queried strictly after an
		// authoritative no-progress timeout, never as progress evidence itself.
		bool vehicleCanMove = SCR_AIVehicleUsability.VehicleCanMove(vehicle);
		string stuckReason = "NO_OBJECTIVE_PROGRESS";
		if (stationary)
			stuckReason = "NO_PHYSICAL_MOVEMENT";
		if (stationary && !vehicleCanMove)
			stuckReason = "MOVEMENT_DAMAGE_WITHOUT_PROGRESS";
		ReportStuckDetected(
			trip,
			stuckReason,
			routeDistanceMeters,
			targetDistanceMeters,
			nowMs,
			vehicleCanMove);

		if (!vehicleCanMove)
		{
			if (state.IsRecoveryEvidencePending())
				ReportPendingRecoveryFailure(trip, stuckReason, true);
			return FailRecovery(
				trip,
				"VEHICLE_RECOVERY_MOBILITY_UNAVAILABLE",
				"MOBILITY",
				causationId);
		}
		if (!state.BeginMobilityRecovery())
		{
			if (state.IsRecoveryEvidencePending())
				ReportPendingRecoveryFailure(trip, stuckReason, true);
			return FailRecovery(
				trip,
				"VEHICLE_STUCK_PERSISTENT",
				"MOBILITY",
				causationId);
		}
		if (state.IsRecoveryEvidencePending())
			ReportPendingRecoveryFailure(trip, stuckReason, false);
		return BeginMobilityRecovery(
			trip,
			causationId,
			stuckReason,
			stationary);
	}

	protected AICF_TripOutcome ProcessMandatoryCrew(
		AICF_TransportTrip trip,
		string causationId,
		int nowMs)
	{
		AICF_VehicleMovementState state = trip.GetMovementState();
		AICF_VehicleCrewRecoveryToken token = state.GetCrewRecoveryToken();
		if (token)
			return ProcessCrewRecoveryToken(trip, token, causationId, nowMs);

		IEntity driver = ResolveAliveRoleOccupant(trip, EAICompartmentType.Pilot);
		if (!driver)
			return StartCrewRecovery(trip, EAICompartmentType.Pilot, causationId, nowMs);
		if (!m_Watchdog.IsMemberSettledInVehicle(driver, trip.GetLease().GetVehicle()))
			return AICF_TripOutcome.FallbackToFoot("DRIVER_NOT_SETTLED_OUTSIDE_RECOVERY", causationId);
		state.SetLastDriver(driver);

		if (trip.GetLease().GetKind() != AICF_EVehicleKind.ARMED_LIGHT)
			return null;
		IEntity gunner = ResolveAliveRoleOccupant(trip, EAICompartmentType.Turret);
		if (!gunner)
			return StartCrewRecovery(trip, EAICompartmentType.Turret, causationId, nowMs);
		if (!m_Watchdog.IsMemberSettledInVehicle(gunner, trip.GetLease().GetVehicle()))
			return AICF_TripOutcome.FallbackToFoot("GUNNER_NOT_SETTLED_OUTSIDE_RECOVERY", causationId);
		state.SetLastGunner(gunner);
		return null;
	}

	protected AICF_TripOutcome StartCrewRecovery(
		AICF_TransportTrip trip,
		EAICompartmentType role,
		string causationId,
		int nowMs)
	{
		AICF_VehicleMovementState state = trip.GetMovementState();
		if (!state.BeginCrewRecovery())
		{
			string exhaustedReason = "DRIVER_RECOVERY_EXHAUSTED";
			if (role == EAICompartmentType.Turret)
				exhaustedReason = "GUNNER_RECOVERY_EXHAUSTED";
			return FailRecovery(trip, exhaustedReason, "CREW", causationId);
		}

		BaseCompartmentSlot roleSlot = ResolveRoleSlot(trip.GetLease().GetVehicle(), role);
		if (!roleSlot || !roleSlot.IsCompartmentAccessible() || roleSlot.IsReserved())
		{
			return FailRecovery(
				trip,
				"CREW_ROLE_RESERVATION_UNAVAILABLE",
				"CREW",
				causationId);
		}
		IEntity occupant = roleSlot.GetOccupant();
		if (occupant && SCR_AIDamageHandling.IsConscious(occupant))
		{
			return FailRecovery(
				trip,
				"CREW_ROLE_OCCUPIED_BY_FOREIGN_ENTITY",
				"CREW",
				causationId);
		}

		IEntity excludedEntity;
		IEntity preferredEntity = state.GetLastDriver();
		if (role == EAICompartmentType.Pilot)
			excludedEntity = ResolveAliveRoleOccupant(trip, EAICompartmentType.Turret);
		else
		{
			preferredEntity = state.GetLastGunner();
			excludedEntity = ResolveAliveRoleOccupant(trip, EAICompartmentType.Pilot);
		}
		AIAgent agent = SelectCrewRecoveryAgent(
			trip.GetAssignment().GetGroup(),
			preferredEntity,
			excludedEntity);
		if (!agent)
			return FailRecovery(trip, "CREW_RECOVERY_AGENT_UNAVAILABLE", "CREW", causationId);
		IEntity reservedEntity = agent.GetControlledEntity();
		SCR_AIUtilityComponent utility = SCR_AIUtilityComponent.Cast(
			agent.FindComponent(SCR_AIUtilityComponent));
		if (!utility || utility.m_OwnerEntity != reservedEntity)
			return FailRecovery(trip, "CREW_RECOVERY_OWNER_INVALID", "CREW", causationId);

		AICF_VehicleLease lease = trip.GetLease();
		string roleName = CrewRoleName(role);
		string actionToken = string.Format(
			"%1-crew-%2-%3",
			trip.GetOperationId(),
			roleName,
			state.GetCrewRecoveryAttempts());
		AICF_VehicleAsyncFence fence = new AICF_VehicleAsyncFence(
			trip.GetFactionKey(),
			trip.GetSlotId(),
			trip.GetGroupGeneration(),
			trip.GetTripGeneration(),
			lease.GetLeaseGeneration(),
			lease.GetVehicleGeneration(),
			lease.GetEntityId(),
			lease.GetRplId(),
			actionToken);

		roleSlot.SetReserved(reservedEntity);
		SCR_AIGetInVehicle action = new SCR_AIGetInVehicle(
			utility,
			null,
			lease.GetVehicle(),
			roleSlot,
			role,
			SCR_AIActionBase.PRIORITY_BEHAVIOR_GET_IN_VEHICLE,
			SCR_AIActionBase.PRIORITY_LEVEL_NORMAL);
		utility.AddAction(action);
		int actionDeadlineMs = Math.Min(
			trip.GetAbsoluteDeadlineMs(),
			nowMs + m_Config.GetBoardingTimeoutMs());
		AICF_VehicleCrewRecoveryToken token = new AICF_VehicleCrewRecoveryToken(
			fence,
			agent,
			reservedEntity,
			lease.GetVehicle(),
			action,
			roleSlot,
			role,
			state.GetCrewRecoveryAttempts(),
			nowMs,
			actionDeadlineMs);
		state.TrackCrewRecoveryToken(token);
		state.SuspendRouteWaypoint();

		string lostEvent = "DRIVER_LOST";
		if (role == EAICompartmentType.Turret)
			lostEvent = "GUNNER_LOST";
		AICF_Stage3Diagnostics.Warning(
			lostEvent,
			FormatIdentity(trip, "MANDATORY_CREW_ROLE_MISSING"));
		AICF_Stage3Diagnostics.Info(
			"VEHICLE_RECOVERY_STARTED",
			FormatIdentity(trip, "EXACT_CREW_ROLE_ACTION") + string.Format(
				" role=%1 agent=%2 action_token=%3 crew_attempt=%4 mobility_attempts=%5",
				roleName,
				reservedEntity.GetID(),
				actionToken,
				state.GetCrewRecoveryAttempts(),
				state.GetMobilityRecoveryAttempts()));
		ReportCrewAction(trip, token, nowMs, true);
		return AICF_TripOutcome.Wait("EXACT_CREW_RECOVERY_ACTION_ACTIVE", causationId);
	}

	protected AICF_TripOutcome ProcessCrewRecoveryToken(
		AICF_TransportTrip trip,
		AICF_VehicleCrewRecoveryToken token,
		string causationId,
		int nowMs)
	{
		AICF_VehicleMovementState state = trip.GetMovementState();
		if (!token.Matches(trip, trip.GetLease(), trip.GetAssignment().GetGroup()))
		{
			token.CancelOwnerSafe();
			state.ClearCrewRecoveryToken(token);
			return AICF_TripOutcome.TerminalFailClosed("STALE_CREW_RECOVERY_TOKEN", causationId);
		}

		if (ResolveRoleSlot(trip.GetLease().GetVehicle(), token.GetRole()) !=
			token.GetCompartment())
		{
			token.CancelOwnerSafe();
			state.ClearCrewRecoveryToken(token);
			return AICF_TripOutcome.TerminalFailClosed("CREW_RECOVERY_COMPARTMENT_IDENTITY_CHANGED", causationId);
		}
		if (token.GetRole() == EAICompartmentType.Turret &&
			!IsRoleSettled(trip, EAICompartmentType.Pilot))
		{
			token.CancelOwnerSafe();
			state.ClearCrewRecoveryToken(token);
			return StartCrewRecovery(trip, EAICompartmentType.Pilot, causationId, nowMs);
		}

		ReportCrewAction(trip, token, nowMs, false);
		if (token.IsExactCompartmentSettled(m_Watchdog))
		{
			IEntity settledEntity = token.GetReservedEntity();
			EAICompartmentType settledRole = token.GetRole();
			token.ReleaseTrackingOwnerSafe();
			state.ClearCrewRecoveryToken(token);
			if (settledRole == EAICompartmentType.Pilot)
				state.SetLastDriver(settledEntity);
			else
				state.SetLastGunner(settledEntity);

			string reassignedEvent = "DRIVER_REASSIGNED";
			if (settledRole == EAICompartmentType.Turret)
				reassignedEvent = "GUNNER_REASSIGNED";
			AICF_Stage3Diagnostics.Info(
				reassignedEvent,
				FormatIdentity(trip, "EXACT_CREW_COMPARTMENT_CONFIRMED") +
					string.Format(" agent=%1 action_token=%2", settledEntity.GetID(), token.GetActionToken()));

			if (settledRole == EAICompartmentType.Pilot &&
				trip.GetLease().GetKind() == AICF_EVehicleKind.ARMED_LIGHT &&
				!IsRoleSettled(trip, EAICompartmentType.Turret))
			{
				return StartCrewRecovery(trip, EAICompartmentType.Turret, causationId, nowMs);
			}
			state.MarkCrewRecoveryRoutePending();
			AICF_Stage3Diagnostics.Info(
				"VEHICLE_CREW_RECOVERY_SETTLED",
				FormatIdentity(trip, "ALL_REQUIRED_CREW_SETTLED") +
					" evidence=PENDING_PHYSICAL_MOTION");
			return PrepareCrewRecoveryRoute(trip, causationId);
		}

		EAIActionState actionState = token.GetActionState();
		if (!token.IsOwnerValid() || actionState == EAIActionState.FAILED ||
			token.IsDeadlineReached(nowMs))
		{
			string failureReason = "CREW_RECOVERY_OWNER_INVALID";
			if (actionState == EAIActionState.FAILED)
				failureReason = "CREW_RECOVERY_ACTION_FAILED";
			else if (token.IsDeadlineReached(nowMs))
				failureReason = "CREW_RECOVERY_TIMEOUT";
			EAICompartmentType role = token.GetRole();
			token.CancelOwnerSafe();
			state.ClearCrewRecoveryToken(token);
			AICF_Stage3Diagnostics.Warning(
				"VEHICLE_CREW_RECOVERY_RETRY",
				FormatIdentity(trip, failureReason) + string.Format(
					" role=%1 attempt=%2 next_deadline_ms=%3",
					CrewRoleName(role),
					state.GetCrewRecoveryAttempts(),
					Math.Min(trip.GetAbsoluteDeadlineMs(), nowMs + RECOVERY_RETRY_DELAY_MS)));
			return AICF_TripOutcome.Retry(
				failureReason,
				causationId,
				Math.Min(trip.GetAbsoluteDeadlineMs(), nowMs + RECOVERY_RETRY_DELAY_MS));
		}
		return AICF_TripOutcome.Wait("CREW_RECOVERY_TRANSITION_PENDING", causationId);
	}

	protected AICF_TripOutcome BeginMobilityRecovery(
		AICF_TransportTrip trip,
		string causationId,
		string stuckReason,
		bool stationary)
	{
		AICF_VehicleMovementState state = trip.GetMovementState();
		Vehicle vehicle = trip.GetLease().GetVehicle();
		vector recoveryOrigin = vehicle.GetOrigin();
		vector relocatedPosition = recoveryOrigin;
		string unstuckMode = "ROUTE_REBUILD_ONLY";
		bool relocated = false;
		if (stationary)
		{
			AICF_Stage3Diagnostics.Info(
				"VEHICLE_UNSTUCK_STARTED",
				FormatIdentity(trip, stuckReason) + string.Format(
					" attempt=%1 maximum_attempts=%2 origin=%3",
					state.GetMobilityRecoveryAttempts(),
					HARD_MAX_MOBILITY_RECOVERIES,
					recoveryOrigin));
			relocated = TryRelocateVehicleForUnstuck(
				trip,
				state.GetTacticalTarget(),
				relocatedPosition,
				unstuckMode);
		}

		state.SuspendRouteWaypoint();
		AICF_Stage3Diagnostics.Info(
			"VEHICLE_RECOVERY_STARTED",
			FormatIdentity(trip, stuckReason) + string.Format(
				" mobility_attempt=%1 maximum_attempts=%2 origin=%3 unstuck_mode=%4",
				state.GetMobilityRecoveryAttempts(),
				HARD_MAX_MOBILITY_RECOVERIES,
				recoveryOrigin,
				unstuckMode));
		if (stationary)
		{
			AICF_Stage3Diagnostics.Info(
				"VEHICLE_UNSTUCK_ATTEMPT",
				FormatIdentity(trip, stuckReason) + string.Format(
					" attempt=%1 relocated=%2 mode=%3 from=%4 to=%5 displacement_m=%6 evidence=PENDING",
					state.GetMobilityRecoveryAttempts(),
					relocated,
					unstuckMode,
					recoveryOrigin,
					relocatedPosition,
					vector.DistanceXZ(recoveryOrigin, relocatedPosition)));
			if (!relocated)
			{
				AICF_Stage3Diagnostics.Warning(
					"VEHICLE_UNSTUCK_FAILED",
					FormatIdentity(trip, unstuckMode) + string.Format(
						" attempt=%1 relocated=0 evidence=NONE final=0",
						state.GetMobilityRecoveryAttempts()));
			}
		}

		AICF_TripOutcome outcome = CreateAndStageRoute(
			trip,
			causationId,
			false,
			true,
			stuckReason,
			stuckReason == "NO_OBJECTIVE_PROGRESS",
			relocated);
		if (outcome.GetKind() == AICF_ETripOutcomeKind.START_MOVEMENT)
		{
			AICF_Stage3Diagnostics.Info(
				"VEHICLE_STUCK_RECOVERY",
				FormatIdentity(trip, stuckReason) + string.Format(
					" action=REBUILD_ROUTE attempt=%1 route_mode=%2 endpoint_offset_m=%3",
					state.GetMobilityRecoveryAttempts(),
					state.GetRouteMode(),
					vector.DistanceXZ(state.GetRouteEndpoint(), state.GetTacticalTarget())));
		}
		return outcome;
	}

	protected bool TryRelocateVehicleForUnstuck(
		AICF_TransportTrip trip,
		vector targetPosition,
		out vector relocatedPosition,
		out string mode)
	{
		Vehicle vehicle = trip.GetLease().GetVehicle();
		relocatedPosition = vehicle.GetOrigin();
		mode = "ROUTE_REBUILD_ONLY";
		if (!Replication.IsServer())
		{
			mode = "REJECTED_NOT_AUTHORITY";
			return false;
		}

		string safetyReason;
		if (!m_Watchdog.CanSafelyRelocateVehicle(
			trip.GetAssignment().GetGroup(),
			vehicle,
			UNSTUCK_PLAYER_RADIUS_METERS,
			safetyReason))
		{
			mode = string.Format("REJECTED_%1", safetyReason);
			return false;
		}

		vector originalPosition = vehicle.GetOrigin();
		vector targetDirection = targetPosition - originalPosition;
		targetDirection[1] = 0;
		if (targetDirection.LengthSq() < 0.01)
		{
			vector currentTransform[4];
			vehicle.GetWorldTransform(currentTransform);
			targetDirection = currentTransform[2];
			targetDirection[1] = 0;
		}
		if (targetDirection.LengthSq() < 0.01)
			targetDirection = "0 0 1";
		targetDirection.Normalize();
		vector rightDirection = Vector(targetDirection[2], 0, -targetDirection[0]);

		array<vector> searchDirections = {};
		searchDirections.Insert(targetDirection);
		searchDirections.Insert(-targetDirection);
		searchDirections.Insert(rightDirection);
		searchDirections.Insert(-rightDirection);
		searchDirections.Insert((targetDirection + rightDirection).Normalized());
		searchDirections.Insert((targetDirection - rightDirection).Normalized());

		vector boundsMin;
		vector boundsMax;
		vehicle.GetBounds(boundsMin, boundsMax);
		float clearanceRadius = Math.Max(
			Math.Max(Math.AbsFloat(boundsMin[0]), Math.AbsFloat(boundsMax[0])),
			Math.Max(Math.AbsFloat(boundsMin[2]), Math.AbsFloat(boundsMax[2]))) + 0.5;
		float clearanceHeight = Math.Max(2.0, boundsMax[1] - boundsMin[1] + 1.0);
		foreach (vector searchDirection : searchDirections)
		{
			vector searchCenter = originalPosition + searchDirection * UNSTUCK_OFFSET_METERS;
			vector candidate;
			if (!SCR_WorldTools.FindEmptyTerrainPosition(
				candidate,
				searchCenter,
				UNSTUCK_SEARCH_RADIUS_METERS,
				clearanceRadius,
				clearanceHeight,
				TraceFlags.ENTS | TraceFlags.OCEAN,
				vehicle.GetWorld()))
				continue;

			float displacementMeters = vector.DistanceXZ(originalPosition, candidate);
			if (displacementMeters < UNSTUCK_MIN_DISPLACEMENT_METERS ||
				displacementMeters > UNSTUCK_MAX_DISPLACEMENT_METERS ||
				!IsUnstuckCandidateHazardClear(candidate))
				continue;

			vector angles = targetDirection.VectorToAngles();
			angles[1] = 0;
			angles[2] = 0;
			vector relocatedTransform[4];
			Math3D.AnglesToMatrix(angles, relocatedTransform);
			relocatedTransform[3] = candidate;
			Physics physics = vehicle.GetPhysics();
			if (physics)
			{
				physics.SetVelocity(vector.Zero);
				physics.SetAngularVelocity(vector.Zero);
			}
			if (!vehicle.SetWorldTransform(relocatedTransform))
				continue;

			relocatedPosition = candidate;
			mode = "SAFE_TERRAIN_REPOSITION";
			return true;
		}

		mode = "NO_SAFE_RELOCATION_POSITION";
		return false;
	}

	protected bool IsUnstuckCandidateHazardClear(vector candidate)
	{
		m_bUnstuckHazardDetected = false;
		BaseWorld world = GetGame().GetWorld();
		if (!world)
			return false;
		world.QueryEntitiesBySphere(
			candidate,
			UNSTUCK_HAZARD_CLEARANCE_METERS,
			EvaluateUnstuckHazard,
			null,
			EQueryEntitiesFlags.ALL);
		return !m_bUnstuckHazardDetected;
	}

	protected bool EvaluateUnstuckHazard(IEntity entity)
	{
		if (!entity)
			return true;
		SCR_PressureTriggerComponent pressureTrigger = SCR_PressureTriggerComponent.Cast(
			entity.FindComponent(SCR_PressureTriggerComponent));
		if (pressureTrigger && pressureTrigger.IsActivated())
		{
			m_bUnstuckHazardDetected = true;
			return false;
		}
		ChimeraCharacter character = ChimeraCharacter.Cast(entity);
		if (!character)
			return true;
		CharacterControllerComponent controller = character.GetCharacterController();
		if (controller && controller.GetLifeState() != ECharacterLifeState.DEAD)
		{
			m_bUnstuckHazardDetected = true;
			return false;
		}
		return true;
	}

	protected BaseCompartmentSlot ResolveRoleSlot(Vehicle vehicle, EAICompartmentType role)
	{
		if (!vehicle)
			return null;
		SCR_AIVehicleUsageComponent usage = SCR_AIVehicleUsageComponent.Cast(
			vehicle.FindComponent(SCR_AIVehicleUsageComponent));
		if (!usage)
			return null;
		if (role == EAICompartmentType.Pilot)
			return usage.GetPilotCompartmentSlot();
		if (role == EAICompartmentType.Turret)
			return usage.GetTurretCompartmentSlot();
		return null;
	}

	protected IEntity ResolveAliveRoleOccupant(
		AICF_TransportTrip trip,
		EAICompartmentType role)
	{
		BaseCompartmentSlot slot = ResolveRoleSlot(trip.GetLease().GetVehicle(), role);
		if (!slot)
			return null;
		IEntity occupant = slot.GetOccupant();
		if (!m_Watchdog.IsAliveGroupMember(trip.GetAssignment().GetGroup(), occupant))
			return null;
		return occupant;
	}

	protected bool IsRoleSettled(AICF_TransportTrip trip, EAICompartmentType role)
	{
		IEntity occupant = ResolveAliveRoleOccupant(trip, role);
		return occupant && m_Watchdog.IsMemberSettledInVehicle(
			occupant,
			trip.GetLease().GetVehicle());
	}

	protected AIAgent SelectCrewRecoveryAgent(
		SCR_AIGroup group,
		IEntity preferredEntity,
		IEntity excludedEntity)
	{
		if (!group)
			return null;
		array<AIAgent> agents = {};
		group.GetAgents(agents);
		AIAgent firstAvailable;
		foreach (AIAgent agent : agents)
		{
			if (!agent)
				continue;
			IEntity entity = agent.GetControlledEntity();
			if (!AICF_GroupRuntime.IsAliveCharacter(entity) || entity == excludedEntity)
				continue;
			if (entity == preferredEntity)
				return agent;
			if (!firstAvailable)
				firstAvailable = agent;
		}
		return firstAvailable;
	}

	protected string InspectVehicleFailure(Vehicle vehicle)
	{
		if (!vehicle)
			return "VEHICLE_DESTROYED";
		SCR_AIVehicleUsageComponent usage = SCR_AIVehicleUsageComponent.Cast(
			vehicle.FindComponent(SCR_AIVehicleUsageComponent));
		if (!usage || usage.GetDamageState() == EDamageState.DESTROYED)
			return "VEHICLE_DESTROYED";
		if (SCR_AIVehicleUsability.VehicleIsOnFire(vehicle))
			return "VEHICLE_ON_FIRE";

		vector transform[4];
		vehicle.GetWorldTransform(transform);
		if (transform[1][1] < 0.25)
			return "VEHICLE_OVERTURNED";
		return string.Empty;
	}

	protected bool ValidateContext(
		AICF_TransportTrip trip,
		string causationId,
		bool requireTransitPhase,
		out string failureReason)
	{
		failureReason = string.Empty;
		if (!trip || !trip.IsValid() || trip.IsTerminal())
		{
			failureReason = "TRANSIT_TRIP_INVALID_OR_TERMINAL";
			return false;
		}
		if (causationId.IsEmpty())
		{
			failureReason = "TRANSIT_CAUSATION_ID_MISSING";
			return false;
		}
		if (requireTransitPhase && trip.GetPhase() != AICF_ETransportTripPhase.TRANSIT)
		{
			failureReason = "TRANSIT_PHASE_NOT_CURRENT";
			return false;
		}
		if (!requireTransitPhase &&
			trip.GetPhase() != AICF_ETransportTripPhase.BOARDING &&
			trip.GetPhase() != AICF_ETransportTripPhase.TRANSIT)
		{
			failureReason = "ROUTE_PREPARE_PHASE_INVALID";
			return false;
		}

		AICF_StrategicAssignmentSnapshot assignment = trip.GetAssignment();
		AICF_VehicleLease lease = trip.GetLease();
		if (!assignment || !assignment.IsValid() || !assignment.GetGroup() ||
			!assignment.GetTargetBase())
		{
			failureReason = "TRANSIT_ASSIGNMENT_INVALID";
			return false;
		}
		if (!lease || !lease.HasPhysicalAsset() || !lease.MatchesTripIdentity(
			trip.GetFactionKey(),
			trip.GetSlotId(),
			trip.GetGroupGeneration(),
			trip.GetTripGeneration()))
		{
			failureReason = "TRANSIT_LEASE_IDENTITY_INVALID";
			return false;
		}
		if (!lease.MatchesEntityIdentity(
			lease.GetVehicle(),
			lease.GetEntityId(),
			lease.GetRplId()))
		{
			failureReason = "TRANSIT_ASSET_IDENTITY_INVALID";
			return false;
		}
		return true;
	}

	protected bool IsTripLeaseIdentityCurrent(AICF_TransportTrip trip)
	{
		if (!trip || trip.IsTerminal() || !trip.GetLease())
			return false;
		AICF_VehicleLease lease = trip.GetLease();
		return lease.HasPhysicalAsset() && lease.MatchesTripIdentity(
			trip.GetFactionKey(),
			trip.GetSlotId(),
			trip.GetGroupGeneration(),
			trip.GetTripGeneration()) && lease.MatchesEntityIdentity(
			lease.GetVehicle(),
			lease.GetEntityId(),
			lease.GetRplId());
	}

	protected void ReportRouteAssigned(
		AICF_TransportTrip trip,
		bool safeReuseRetarget,
		bool recovery,
		string recoveryReason)
	{
		AICF_VehicleMovementState state = trip.GetMovementState();
		string details = FormatIdentity(trip, "ROUTE_COMMITTED");
		details += string.Format(
			" target=%1 assignment_revision=%2 route_generation=%3 route_mode=%4",
			AICF_Stage1Diagnostics.BaseKey(trip.GetAssignment().GetTargetBase()),
			state.GetAssignmentRevision(),
			state.GetRouteGeneration(),
			state.GetRouteMode());
		details += string.Format(
			" tactical_target=%1 road_endpoint=%2 endpoint_offset_m=%3 dismount_distance_m=%4",
			state.GetTacticalTarget(),
			state.GetRouteEndpoint(),
			vector.DistanceXZ(state.GetRouteEndpoint(), state.GetTacticalTarget()),
			m_Config.GetDismountDistanceMeters());
		details += string.Format(
			" safe_reuse_retarget=%1 recovery=%2 recovery_reason=%3",
			safeReuseRetarget,
			recovery,
			recoveryReason);
		AICF_Stage3Diagnostics.Info("VEHICLE_ROUTE_ASSIGNED", details);
		if (state.GetRouteMode() != "ROAD_REACHABLE")
		{
			AICF_Stage3Diagnostics.Warning(
				"VEHICLE_ROUTE_DIRECT_FALLBACK",
				FormatIdentity(trip, state.GetRouteMode()) + string.Format(
					" tactical_target=%1 direct_endpoint=%2",
					state.GetTacticalTarget(),
					state.GetRouteEndpoint()));
		}
		if (safeReuseRetarget)
		{
			AICF_Stage3Diagnostics.Info(
				"VEHICLE_SAFE_REUSE_RETARGET",
				FormatIdentity(trip, "STRATEGIC_TARGET_REVISION_CHANGED") + string.Format(
					" same_entity_id=%1 same_rpl_id=%2 new_assignment_revision=%3",
					trip.GetLease().GetEntityIdString(),
					trip.GetLease().GetRplId(),
					trip.GetAssignment().GetAssignmentRevision()));
		}
	}

	protected void ReportProgress(
		AICF_TransportTrip trip,
		bool routeProgress,
		bool physicalMotion,
		float routeDistanceMeters,
		float targetDistanceMeters,
		int nowMs)
	{
		if (routeProgress)
		{
			AICF_Stage3Diagnostics.Info(
				"VEHICLE_PROGRESS",
				FormatIdentity(trip, "ROUTE_DISTANCE_REDUCED") + string.Format(
					" route_distance_m=%1 target_distance_m=%2 road_endpoint=%3 tactical_target=%4",
					routeDistanceMeters,
					targetDistanceMeters,
					trip.GetMovementState().GetRouteEndpoint(),
					trip.GetMovementState().GetTacticalTarget()));
		}
		else if (physicalMotion && trip.GetMovementState().MarkMotionReportDue(
			nowMs,
			MOTION_REPORT_INTERVAL_MS))
		{
			AICF_Stage3Diagnostics.Info(
				"VEHICLE_MOTION",
				FormatIdentity(trip, "PHYSICAL_MOVEMENT_WITHOUT_ROUTE_REDUCTION") +
					string.Format(
						" route_distance_m=%1 target_distance_m=%2",
						routeDistanceMeters,
						targetDistanceMeters));
		}
	}

	protected void ReportAndConfirmRecovery(
		AICF_TransportTrip trip,
		float routeDistanceMeters,
		float targetDistanceMeters)
	{
		AICF_VehicleMovementState state = trip.GetMovementState();
		string recoveryReason = state.GetPendingRecoveryReason();
		bool relocated = state.WasPendingUnstuckRelocated();
		string evidence = "PHYSICAL_MOTION";
		if (state.RecoveryRequiresRouteProgress())
			evidence = "PHYSICAL_MOTION_AND_ROUTE_PROGRESS";
		string details = FormatIdentity(trip, recoveryReason) + string.Format(
			" evidence=%1 relocated=%2 route_distance_m=%3 target_distance_m=%4",
			evidence,
			relocated,
			routeDistanceMeters,
			targetDistanceMeters);
		AICF_Stage3Diagnostics.Info("VEHICLE_RECOVERY_SUCCEEDED", details);
		if (recoveryReason == "CREW_RECOVERY")
			AICF_Stage3Diagnostics.Info("VEHICLE_CREW_RECOVERY_SUCCEEDED", details);
		if (relocated)
			AICF_Stage3Diagnostics.Info("VEHICLE_UNSTUCK_SUCCEEDED", details);
		state.ConfirmRecoveryEvidence();
	}

	protected void ReportStuckDetected(
		AICF_TransportTrip trip,
		string stuckReason,
		float routeDistanceMeters,
		float targetDistanceMeters,
		int nowMs,
		bool vehicleCanMove)
	{
		AICF_VehicleMovementState state = trip.GetMovementState();
		float movementDamage = 1.0;
		SCR_DamageManagerComponent damageManager = SCR_DamageManagerComponent.GetDamageManager(
			trip.GetLease().GetVehicle());
		if (damageManager)
			movementDamage = damageManager.GetMovementDamage();
		string details = FormatIdentity(trip, stuckReason) + string.Format(
			" route_distance_m=%1 target_distance_m=%2 route_progress_age_ms=%3 motion_age_ms=%4",
			routeDistanceMeters,
			targetDistanceMeters,
			nowMs - state.GetLastRouteProgressAtMs(),
			nowMs - state.GetLastPhysicalMotionAtMs());
		details += string.Format(
			" movement_damage=%1 movement_usable=%2 stationary_timeout_ms=%3 objective_timeout_ms=%4",
			movementDamage,
			vehicleCanMove,
			m_Config.GetStuckTimeoutMs(),
			m_Config.GetObjectiveProgressTimeoutMs());
		details += string.Format(
			" mobility_attempt=%1 recovery_pending=%2 pending_reason=%3",
			state.GetMobilityRecoveryAttempts() + 1,
			state.IsRecoveryEvidencePending(),
			state.GetPendingRecoveryReason());
		AICF_Stage3Diagnostics.Warning("VEHICLE_STUCK_DETECTED", details);
	}

	protected void ReportPendingRecoveryFailure(
		AICF_TransportTrip trip,
		string currentReason,
		bool finalFailure)
	{
		AICF_VehicleMovementState state = trip.GetMovementState();
		string details = FormatIdentity(trip, state.GetPendingRecoveryReason()) + string.Format(
			" evidence=NONE physical_evidence=%1 route_evidence=%2 requires_route_progress=%3 final=%4",
			state.HasRecoveryPhysicalEvidence(),
			state.HasRecoveryRouteEvidence(),
			state.RecoveryRequiresRouteProgress(),
			finalFailure);
		details += string.Format(
			" next_reason=%1 relocated=%2 mobility_attempts=%3",
			currentReason,
			state.WasPendingUnstuckRelocated(),
			state.GetMobilityRecoveryAttempts());
		AICF_Stage3Diagnostics.Warning("VEHICLE_RECOVERY_EVIDENCE_MISSING", details);
		if (state.WasPendingUnstuckRelocated())
			AICF_Stage3Diagnostics.Warning("VEHICLE_UNSTUCK_FAILED", details);
	}

	protected AICF_TripOutcome FailRecovery(
		AICF_TransportTrip trip,
		string reason,
		string scope,
		string causationId)
	{
		AICF_VehicleMovementState state = trip.GetMovementState();
		string details = FormatIdentity(trip, reason);
		details += string.Format(
			" recovery_scope=%1 crew_attempts=%2 crew_budget=%3 mobility_attempts=%4 mobility_budget=%5 final=1 next_action=FALLBACK_TO_FOOT",
			scope,
			state.GetCrewRecoveryAttempts(),
			state.GetMaximumCrewRecoveryAttempts(),
			state.GetMobilityRecoveryAttempts(),
			state.GetMaximumMobilityRecoveryAttempts());
		AICF_Stage3Diagnostics.Warning("VEHICLE_RECOVERY_FAILED", details);
		return AICF_TripOutcome.FallbackToFoot(reason, causationId);
	}

	protected void ReportCrewAction(
		AICF_TransportTrip trip,
		AICF_VehicleCrewRecoveryToken token,
		int nowMs,
		bool force)
	{
		bool linked;
		bool gettingIn;
		bool gettingOut;
		EAIActionState actionState;
		bool changed = token.ObserveTransitionChange(
			linked,
			gettingIn,
			gettingOut,
			actionState);
		if (!force && !token.MarkTelemetryDue(
			changed,
			nowMs,
			CREW_ACTION_TELEMETRY_INTERVAL_MS))
			return;
		if (force)
			token.MarkTelemetryDue(true, nowMs, CREW_ACTION_TELEMETRY_INTERVAL_MS);

		string details = FormatIdentity(trip, "CREW_ACTION_TRANSITION") + string.Format(
			" agent=%1 action_token=%2 role=%3 action_state=%4 owner_valid=%5",
			token.GetReservedEntity().GetID(),
			token.GetActionToken(),
			CrewRoleName(token.GetRole()),
			typename.EnumToString(EAIActionState, actionState),
			token.IsOwnerValid());
		details += string.Format(
			" assigned_mgr=%1 assigned_slot=%2 reserved_by_owner=%3 actual_mgr=%4 actual_slot=%5",
			token.GetAssignedManagerId(),
			token.GetAssignedSlotId(),
			token.IsReservationOwned(),
			token.GetActualManagerId(),
			token.GetActualSlotId());
		details += string.Format(
			" linked=%1 getting_in=%2 getting_out=%3 retry=%4 transition_age_ms=%5",
			linked,
			gettingIn,
			gettingOut,
			token.GetAttempt(),
			token.GetTransitionAgeMs(nowMs));
		AICF_Stage3Diagnostics.Info("VEHICLE_CREW_ACTION", details);
	}

	protected string FormatIdentity(AICF_TransportTrip trip, string reason)
	{
		string details = string.Format(
			"faction=%1 slot=%2 numeric_slot=%3 group_generation=%4 trip_generation=%5 operation_id=%6 causation_id=%7",
			trip.GetFactionKey(),
			trip.GetSlotKey(),
			trip.GetSlotId(),
			trip.GetGroupGeneration(),
			trip.GetTripGeneration(),
			trip.GetOperationId(),
			trip.GetCausationId());
		details += string.Format(
			" lease_generation=%1 vehicle_generation=%2 vehicle_lifecycle_id=%3 entity_id=%4",
			trip.GetLease().GetLeaseGeneration(),
			trip.GetLease().GetVehicleGeneration(),
			trip.GetLease().GetVehicleLifecycleId(),
			trip.GetLease().GetEntityIdString());
		details += string.Format(
			" rpl_id=%1 reason=%2",
			trip.GetLease().GetRplId(),
			reason);
		details += string.Format(
			" vehicle=%1 kind=%2 state=%3 prefab=%4",
			trip.GetLease().GetEntityIdString(),
			AICF_Stage3Diagnostics.KindToString(trip.GetLease().GetKind()),
			AICF_Stage3Diagnostics.TripPhaseToString(trip.GetPhase()),
			trip.GetLease().GetPrefab());
		return details;
	}

	protected string CrewRoleName(EAICompartmentType role)
	{
		if (role == EAICompartmentType.Pilot)
			return "PILOT";
		if (role == EAICompartmentType.Turret)
			return "TURRET";
		return "UNSUPPORTED";
	}
}
