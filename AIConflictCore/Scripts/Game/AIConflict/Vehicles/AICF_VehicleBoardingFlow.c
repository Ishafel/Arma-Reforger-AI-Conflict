// Bounded initial boarding for one authoritative TransportTrip. This flow owns
// only per-member approach actions and exact Pilot/Turret/Cargo reservations.
// It never creates group waypoints, attaches vehicle utility, changes trip
// state, or invokes another lifecycle component.
class AICF_VehicleBoardingFlow
{
	protected static const float STAGING_THRESHOLD_METERS = 75.0;
	protected static const float APPROACH_ACTION_RADIUS_METERS = 70.0;
	protected static const float APPROACH_PROGRESS_METERS = 2.0;
	protected static const int APPROACH_STALL_MS = 15000;
	protected static const int APPROACH_MAX_RETRIES = 1;
	protected static const int CREW_MAX_RETRIES = 1;
	protected static const int SETTLED_POLLS_REQUIRED = 2;
	protected static const int TRANSITION_GRACE_MS = 10000;
	protected static const int PROGRESS_FRESH_MS = 10000;
	protected static const int OWNERSHIP_AUDIT_INTERVAL_MS = 10000;
	protected static const int MAX_ACTION_TOKENS = 16;
	protected ref AICF_Stage3Config m_Config;
	protected ref AICF_VehicleWatchdog m_Watchdog;

	void AICF_VehicleBoardingFlow(
		AICF_Stage3Config config,
		AICF_VehicleWatchdog watchdog)
	{
		m_Config = config;
		m_Watchdog = watchdog;
	}

	// Called once after the controller commits BOARDING. Every failure is a
	// typed outcome; the controller remains the only transition authority.
	AICF_TripOutcome Begin(AICF_TransportTrip trip, string causationId)
	{
		SCR_AIGroup group;
		AICF_VehicleLease lease;
		Vehicle vehicle;
		AICF_VehicleBoardingState state;
		string invalidReason;
		if (!ResolveContext(trip, causationId, group, lease, vehicle, state, invalidReason))
			return AICF_TripOutcome.TerminalFailClosed(invalidReason, causationId);
		if (state.GetStartedAtMs() > 0)
			return AICF_TripOutcome.Wait("BOARDING_ALREADY_STARTED", causationId);

		string healthReason;
		if (!InspectVehicleHealth(vehicle, healthReason))
			return Reject(trip, lease, causationId, healthReason);

		int aliveCount;
		float leaderDistanceMeters;
		float nearestDistanceMeters;
		float farthestDistanceMeters;
		string distanceSamples;
		if (!m_Watchdog.MeasureAliveGroupDistances(
			group,
			vehicle,
			aliveCount,
			leaderDistanceMeters,
			nearestDistanceMeters,
			farthestDistanceMeters,
			distanceSamples) || aliveCount <= 0)
		{
			return Reject(trip, lease, causationId, "BOARDING_GROUP_EMPTY");
		}

		float maximumDistanceMeters = m_Config.GetMaximumReuseDistanceMeters();
		if (farthestDistanceMeters > maximumDistanceMeters)
		{
			string rejectedDetails = DescribeContext(trip, lease, causationId, "VEHICLE_TOO_FAR");
			rejectedDetails += string.Format(
				" alive=%1 leader_m=%2 nearest_m=%3 farthest_m=%4 maximum_m=%5 members=[%6]",
				aliveCount,
				leaderDistanceMeters,
				nearestDistanceMeters,
				farthestDistanceMeters,
				maximumDistanceMeters,
				distanceSamples);
			AICF_Stage3Diagnostics.Warning("BOARDING_REJECTED", rejectedDetails);
			return AICF_TripOutcome.FallbackToFoot("VEHICLE_TOO_FAR", causationId);
		}

		int mountedCount;
		int accessibleSeats;
		bool driverAvailable;
		bool gunnerAvailable;
		string capacityReason;
		if (!InspectCapacity(
			group,
			lease,
			vehicle,
			aliveCount,
			mountedCount,
			accessibleSeats,
			driverAvailable,
			gunnerAvailable,
			capacityReason))
		{
			string capacityDetails = DescribeContext(trip, lease, causationId, capacityReason);
			capacityDetails += string.Format(
				" alive=%1 mounted=%2 empty_accessible=%3 available_capacity=%4 driver_available=%5 gunner_available=%6",
				aliveCount,
				mountedCount,
				accessibleSeats,
				accessibleSeats + mountedCount,
				driverAvailable,
				gunnerAvailable);
			AICF_Stage3Diagnostics.Warning("BOARDING_REJECTED", capacityDetails);
			return AICF_TripOutcome.FallbackToFoot(capacityReason, causationId);
		}

		bool driverSettled = IsDriverSettled(group, vehicle);
		bool gunnerSettled = IsGunnerSettled(group, vehicle, lease.GetKind());
		bool driverPhasePlanned = !driverSettled;
		bool gunnerPhasePlanned = lease.GetKind() == AICF_EVehicleKind.ARMED_LIGHT &&
			(!gunnerSettled || driverPhasePlanned);
		float stagingThresholdMeters = GetStagingThresholdMeters();
		bool approachPlanned = farthestDistanceMeters > stagingThresholdMeters;
		int plannedPhaseCount = CalculatePlannedPhaseCount(
			approachPlanned,
			driverPhasePlanned,
			gunnerPhasePlanned);
		int nowMs = System.GetTickCount();
		int phaseTimeoutMs = m_Config.GetBoardingTimeoutMs();
		int totalTimeoutMs = phaseTimeoutMs * plannedPhaseCount;
		int totalDeadlineMs = nowMs + totalTimeoutMs;
		if (trip.GetAbsoluteDeadlineMs() > 0 && totalDeadlineMs > trip.GetAbsoluteDeadlineMs())
			totalDeadlineMs = trip.GetAbsoluteDeadlineMs();
		if (totalDeadlineMs <= nowMs)
			return Reject(trip, lease, causationId, "BOARDING_TRIP_DEADLINE_EXCEEDED");

		int interruptedActions = m_Watchdog.ResetGroupVehicleActions(group);
		state.Begin(
			nowMs,
			totalDeadlineMs,
			plannedPhaseCount,
			m_Config.GetPassengerMaxRetries());
		state.ConfigureImmutablePlan(
			phaseTimeoutMs,
			driverPhasePlanned,
			gunnerPhasePlanned);
		string startedDetails = DescribeContext(trip, lease, causationId, "ROLE_ORDERED_EXACT_BOARDING");
		startedDetails += string.Format(
			" alive=%1 mounted=%2 empty_accessible=%3 planned_phases=%4 phase_timeout_ms=%5 total_timeout_ms=%6 total_deadline_ms=%7",
			aliveCount,
			mountedCount,
			accessibleSeats,
			plannedPhaseCount,
			phaseTimeoutMs,
			totalTimeoutMs,
			totalDeadlineMs);
		startedDetails += string.Format(
			" leader_m=%1 nearest_m=%2 farthest_m=%3 staging_threshold_m=%4 approach_planned=%5 members=[%6]",
			leaderDistanceMeters,
			nearestDistanceMeters,
			farthestDistanceMeters,
			stagingThresholdMeters,
			approachPlanned,
			distanceSamples);
		startedDetails += string.Format(
			" driver_phase_planned=%1 gunner_phase_planned=%2 effective_total_timeout_ms=%3 tracked_action_fences=%4 interrupted_actions=%5",
			driverPhasePlanned,
			gunnerPhasePlanned,
			totalDeadlineMs - nowMs,
			state.GetActionFenceCount(),
			interruptedActions);
		AICF_Stage3Diagnostics.Info("VEHICLE_ASSIGNED", DescribeContext(trip, lease, causationId, "BOARDING_BEGIN"));
		AICF_Stage3Diagnostics.Info("BOARDING_STARTED", startedDetails);

		if (approachPlanned)
			return BeginApproach(trip, lease, group, state, causationId);
		return ContinueRoleOrder(trip, lease, group, state, causationId);
	}

	AICF_TripOutcome Tick(AICF_TransportTrip trip, string causationId)
	{
		SCR_AIGroup group;
		AICF_VehicleLease lease;
		Vehicle vehicle;
		AICF_VehicleBoardingState state;
		string invalidReason;
		if (!ResolveContext(trip, causationId, group, lease, vehicle, state, invalidReason))
			return AICF_TripOutcome.TerminalFailClosed(invalidReason, causationId);
		if (state.GetStartedAtMs() <= 0 || !state.GetTokens())
			return AICF_TripOutcome.TerminalFailClosed("BOARDING_STATE_NOT_BEGUN", causationId);

		string healthReason;
		if (!InspectVehicleHealth(vehicle, healthReason))
			return Reject(trip, lease, causationId, healthReason);

		int aliveCount;
		int linkedCount;
		int compartmentCount;
		int gettingInCount;
		int gettingOutCount;
		int characterVehicleCount;
		int settledCount;
		float nearestDistanceMeters;
		float farthestDistanceMeters;
		string memberSamples;
		if (!m_Watchdog.InspectBoardingProgress(
			group,
			vehicle,
			aliveCount,
			linkedCount,
			compartmentCount,
			gettingInCount,
			gettingOutCount,
			characterVehicleCount,
			settledCount,
			nearestDistanceMeters,
			farthestDistanceMeters,
			memberSamples))
		{
			return Reject(trip, lease, causationId, "BOARDING_PROGRESS_INPUT_INVALID");
		}

		int nowMs = System.GetTickCount();
		if (farthestDistanceMeters > m_Config.GetMaximumReuseDistanceMeters())
			return Reject(trip, lease, causationId, "BOARDING_MEMBER_EXCEEDED_MAXIMUM_DISTANCE");
		bool progressAdvanced = state.ObserveProgress(
			linkedCount,
			compartmentCount,
			gettingInCount,
			characterVehicleCount,
			settledCount,
			farthestDistanceMeters,
			nowMs);
		if (progressAdvanced)
			ReportProgress(trip, lease, state, causationId, aliveCount, linkedCount, compartmentCount,
				gettingInCount, gettingOutCount, characterVehicleCount, settledCount,
				nearestDistanceMeters, farthestDistanceMeters, memberSamples);
		ReportActionChanges(trip, lease, state, causationId);
		if (state.MarkOwnershipAuditDue(nowMs, OWNERSHIP_AUDIT_INTERVAL_MS))
			ReportOwnershipAudit(trip, lease, state, causationId, memberSamples);

		AICF_TripOutcome stepOutcome;
		if (state.IsRoleResetAttempted() && !state.IsRoleRetryIssued())
		{
			stepOutcome = ProcessRoleReset(trip, lease, group, state, causationId);
		}
		else
		{
			switch (state.GetPhase())
			{
				case AICF_EVehicleBoardingPhase.APPROACH:
					stepOutcome = ProcessApproach(
						trip, lease, group, state, causationId, farthestDistanceMeters,
						aliveCount, nearestDistanceMeters, memberSamples);
					break;
				case AICF_EVehicleBoardingPhase.DRIVER:
					stepOutcome = ProcessCrewPhase(
						trip, lease, group, state, causationId,
						EAICompartmentType.Pilot, farthestDistanceMeters);
					break;
				case AICF_EVehicleBoardingPhase.GUNNER:
					stepOutcome = ProcessCrewPhase(
						trip, lease, group, state, causationId,
						EAICompartmentType.Turret, farthestDistanceMeters);
					break;
				case AICF_EVehicleBoardingPhase.PASSENGERS:
					stepOutcome = ProcessPassengers(
						trip, lease, group, state, causationId,
						aliveCount, linkedCount, gettingInCount, gettingOutCount,
						settledCount, memberSamples);
					break;
				default:
					return AICF_TripOutcome.TerminalFailClosed("BOARDING_PHASE_INVALID", causationId);
			}
		}
		if (stepOutcome && stepOutcome.GetKind() != AICF_ETripOutcomeKind.WAIT)
			return stepOutcome;
		return EnforceDeadline(
			trip,
			lease,
			state,
			causationId,
			gettingInCount,
			aliveCount,
			linkedCount,
			settledCount,
			memberSamples);
	}

	// Idempotent exactly-once phase-exit effect. The controller calls this
	// before CommitTransition resets VehicleBoardingState.
	void Exit(AICF_TransportTrip trip, string reason)
	{
		if (!trip || !trip.GetBoardingState())
			return;
		AICF_VehicleBoardingState state = trip.GetBoardingState();
		if (!state.ApplyExitEffectsOwnerSafe())
			return;
		AICF_VehicleLease lease = trip.GetLease();
		AICF_Stage3Diagnostics.Info(
			"BOARDING_EXIT",
			DescribeContext(trip, lease, trip.GetCausationId(), reason) +
			" actions_cancelled_owner_safe=1 reservations_released_owner_safe=1");
	}

	protected bool ResolveContext(
		AICF_TransportTrip trip,
		string causationId,
		out SCR_AIGroup group,
		out AICF_VehicleLease lease,
		out Vehicle vehicle,
		out AICF_VehicleBoardingState state,
		out string failureReason)
	{
		failureReason = "BOARDING_CONTEXT_INVALID";
		if (!m_Config || !m_Watchdog || !trip || causationId.IsEmpty() ||
			trip.GetPhase() != AICF_ETransportTripPhase.BOARDING)
		{
			return false;
		}
		lease = trip.GetLease();
		if (!lease || !lease.MatchesTripIdentity(
			trip.GetFactionKey(),
			trip.GetSlotId(),
			trip.GetGroupGeneration(),
			trip.GetTripGeneration()) || !lease.HasPhysicalAsset())
		{
			failureReason = "BOARDING_LEASE_IDENTITY_INVALID";
			return false;
		}
		vehicle = lease.GetVehicle();
		string liveRplId = ResolveLiveRplId(vehicle);
		if (!vehicle || liveRplId.IsEmpty() ||
			!lease.MatchesEntityIdentity(vehicle, vehicle.GetID(), liveRplId))
		{
			failureReason = "BOARDING_VEHICLE_IDENTITY_INVALID";
			return false;
		}
		if (!trip.GetAssignment())
			return false;
		group = trip.GetAssignment().GetGroup();
		if (!group || trip.GetAssignment().GetGroupGeneration() != trip.GetGroupGeneration())
		{
			failureReason = "BOARDING_GROUP_GENERATION_INVALID";
			return false;
		}
		state = trip.GetBoardingState();
		if (!state)
		{
			failureReason = "BOARDING_STATE_MISSING";
			return false;
		}
		return true;
	}

	protected bool InspectVehicleHealth(Vehicle vehicle, out string failureReason)
	{
		failureReason = string.Empty;
		if (!vehicle)
		{
			failureReason = "VEHICLE_MISSING_DURING_BOARDING";
			return false;
		}
		SCR_AIVehicleUsageComponent usage = ResolveUsage(vehicle);
		if (!usage || usage.GetDamageState() == EDamageState.DESTROYED)
		{
			failureReason = "VEHICLE_DESTROYED_DURING_BOARDING";
			return false;
		}
		if (SCR_AIVehicleUsability.VehicleIsOnFire(vehicle))
		{
			failureReason = "VEHICLE_ON_FIRE_DURING_BOARDING";
			return false;
		}
		vector transform[4];
		vehicle.GetWorldTransform(transform);
		if (transform[1][1] < 0.25)
		{
			failureReason = "VEHICLE_OVERTURNED_DURING_BOARDING";
			return false;
		}
		if (!SCR_AIVehicleUsability.VehicleCanMove(vehicle))
		{
			failureReason = "VEHICLE_IMMOBILE_DURING_BOARDING";
			return false;
		}
		return true;
	}

	protected bool InspectCapacity(
		SCR_AIGroup group,
		AICF_VehicleLease lease,
		Vehicle vehicle,
		int aliveCount,
		out int mountedCount,
		out int accessibleSeats,
		out bool driverAvailable,
		out bool gunnerAvailable,
		out string failureReason)
	{
		failureReason = "INSUFFICIENT_COMPARTMENTS";
		bool freePilot;
		bool freeTurret;
		accessibleSeats = m_Watchdog.CountAccessibleSeatsForVehicle(
			vehicle,
			lease.GetKind(),
			freePilot,
			freeTurret);
		mountedCount = m_Watchdog.CountAliveGroupMembersInVehicle(group, vehicle);
		IEntity pilotOccupant = ResolveRoleOccupant(vehicle, EAICompartmentType.Pilot);
		driverAvailable = freePilot || m_Watchdog.IsAliveGroupMember(group, pilotOccupant);
		gunnerAvailable = true;
		if (lease.GetKind() == AICF_EVehicleKind.ARMED_LIGHT)
		{
			IEntity turretOccupant = ResolveRoleOccupant(vehicle, EAICompartmentType.Turret);
			gunnerAvailable = freeTurret || m_Watchdog.IsAliveGroupMember(group, turretOccupant);
		}
		if (aliveCount <= 0 || lease.GetCapacity() < aliveCount ||
			accessibleSeats + mountedCount < aliveCount || !driverAvailable)
		{
			return false;
		}
		if (lease.GetKind() == AICF_EVehicleKind.ARMED_LIGHT &&
			(aliveCount < 2 || !gunnerAvailable))
		{
			return false;
		}
		return true;
	}

	protected AICF_TripOutcome BeginApproach(
		AICF_TransportTrip trip,
		AICF_VehicleLease lease,
		SCR_AIGroup group,
		AICF_VehicleBoardingState state,
		string causationId)
	{
		state.BeginPhase(AICF_EVehicleBoardingPhase.APPROACH, 1, System.GetTickCount());
		string failureReason;
		if (!IssueApproachActions(trip, lease, group, state, causationId, null, failureReason))
			return Reject(trip, lease, causationId, failureReason);
		ReportPhaseStarted(trip, lease, state, causationId, "EXACT_PER_MEMBER_MOVE_NO_VEHICLE_UTILITY");
		return AICF_TripOutcome.Wait("BOARDING_APPROACH_ACTIVE", causationId);
	}

	protected AICF_TripOutcome ProcessApproach(
		AICF_TransportTrip trip,
		AICF_VehicleLease lease,
		SCR_AIGroup group,
		AICF_VehicleBoardingState state,
		string causationId,
		float farthestDistanceMeters,
		int aliveCount,
		float nearestDistanceMeters,
		string memberSamples)
	{
		string failureReason;
		if (!MaintainApproachActions(trip, lease, group, state, causationId, null, failureReason))
			return Reject(trip, lease, causationId, failureReason);
		bool staged = farthestDistanceMeters <= GetStagingThresholdMeters();
		if (state.ObserveStaging(staged) < SETTLED_POLLS_REQUIRED)
			return AICF_TripOutcome.Wait("BOARDING_APPROACH_PENDING", causationId);
		state.GetTokens().CancelAllOwnerSafe();
		string details = DescribeContext(trip, lease, causationId, "ALL_ALIVE_MEMBERS_STAGED");
		details += string.Format(
			" alive=%1 nearest_m=%2 farthest_m=%3 threshold_m=%4 members=[%5]",
			aliveCount,
			nearestDistanceMeters,
			farthestDistanceMeters,
			GetStagingThresholdMeters(),
			memberSamples);
		AICF_Stage3Diagnostics.Info("BOARDING_APPROACH_COMPLETE", details);
		return ContinueRoleOrder(trip, lease, group, state, causationId);
	}

	protected AICF_TripOutcome ContinueRoleOrder(
		AICF_TransportTrip trip,
		AICF_VehicleLease lease,
		SCR_AIGroup group,
		AICF_VehicleBoardingState state,
		string causationId)
	{
		Vehicle vehicle = lease.GetVehicle();
		bool driverSettled = IsDriverSettled(group, vehicle);
		if (!driverSettled)
		{
			if (CountWrongSeatMembers(group, vehicle, lease.GetKind()) > 0)
				return BeginRoleReset(
					trip, lease, group, state, causationId, AICF_EVehicleBoardingPhase.DRIVER);
			if (!state.IsDriverPhasePlanned())
				return Reject(trip, lease, causationId, "DRIVER_LOST_BEFORE_ROLE_PHASE");
			return BeginCrewPhase(
				trip, lease, group, state, causationId, EAICompartmentType.Pilot, null);
		}

		if (lease.GetKind() == AICF_EVehicleKind.ARMED_LIGHT &&
			!IsGunnerSettled(group, vehicle, lease.GetKind()))
		{
			if (CountWrongSeatMembers(group, vehicle, lease.GetKind()) > 0)
				return BeginRoleReset(
					trip, lease, group, state, causationId, AICF_EVehicleBoardingPhase.GUNNER);
			if (!state.IsGunnerPhasePlanned())
				return Reject(trip, lease, causationId, "GUNNER_LOST_BEFORE_ROLE_PHASE");
			return BeginCrewPhase(
				trip, lease, group, state, causationId, EAICompartmentType.Turret, null);
		}

		return BeginPassengerPhase(trip, lease, group, state, causationId);
	}

	protected AICF_TripOutcome BeginRoleReset(
		AICF_TransportTrip trip,
		AICF_VehicleLease lease,
		SCR_AIGroup group,
		AICF_VehicleBoardingState state,
		string causationId,
		AICF_EVehicleBoardingPhase nextPhase)
	{
		int nowMs = System.GetTickCount();
		state.BeginPhase(nextPhase, state.GetCurrentPhaseIndex() + 1, nowMs);
		state.BeginRoleReset(nowMs, nextPhase);
		int requested;
		string failureReason;
		if (!RequestAnimatedWrongSeatExit(
			trip, lease, group, state, causationId, requested, failureReason))
		{
			return RejectRoleViolation(trip, lease, state, causationId, failureReason);
		}
		string details = DescribeContext(trip, lease, causationId, "MOUNTED_OUTSIDE_REQUIRED_ROLE_ORDER");
		details += string.Format(
			" next_phase=%1 requested=%2 reset_timeout_ms=%3 exact_eject=0 teleport=0",
			typename.EnumToString(AICF_EVehicleBoardingPhase, nextPhase),
			requested,
			GetRoleResetTimeoutMs());
		AICF_Stage3Diagnostics.Warning("BOARDING_ROLE_RESET", details);
		return AICF_TripOutcome.Wait("BOARDING_ROLE_RESET_ACTIVE", causationId);
	}

	protected AICF_TripOutcome ProcessRoleReset(
		AICF_TransportTrip trip,
		AICF_VehicleLease lease,
		SCR_AIGroup group,
		AICF_VehicleBoardingState state,
		string causationId)
	{
		int wrongSeatCount = CountWrongSeatMembers(group, lease.GetVehicle(), lease.GetKind());
		if (wrongSeatCount <= 0)
		{
			AICF_EVehicleBoardingPhase nextPhase = state.GetRoleResetNextPhase();
			state.MarkRoleRetryIssued();
			AICF_Stage3Diagnostics.Info(
				"BOARDING_ROLE_RETRY",
				DescribeContext(trip, lease, causationId, "ANIMATED_ROLE_RESET_COMPLETE") +
				string.Format(
					" next_phase=%1 allowance=EXACT_ROLE_ONLY",
					typename.EnumToString(AICF_EVehicleBoardingPhase, nextPhase)));
			if (nextPhase == AICF_EVehicleBoardingPhase.DRIVER)
				return BeginCrewPhase(
					trip, lease, group, state, causationId, EAICompartmentType.Pilot, null);
			if (nextPhase == AICF_EVehicleBoardingPhase.GUNNER)
				return BeginCrewPhase(
					trip, lease, group, state, causationId, EAICompartmentType.Turret, null);
			return RejectRoleViolation(trip, lease, state, causationId, "ROLE_RESET_NEXT_PHASE_INVALID");
		}
		int ageMs = state.GetRoleResetAgeMs(System.GetTickCount());
		if (ageMs >= GetRoleResetTimeoutMs())
			return RejectRoleViolation(trip, lease, state, causationId, "ROLE_RESET_DEADLINE_EXCEEDED");
		return AICF_TripOutcome.Wait("BOARDING_ROLE_RESET_PENDING", causationId);
	}

	protected AICF_TripOutcome BeginCrewPhase(
		AICF_TransportTrip trip,
		AICF_VehicleLease lease,
		SCR_AIGroup group,
		AICF_VehicleBoardingState state,
		string causationId,
		EAICompartmentType role,
		AIAgent preferredAgent)
	{
		AICF_EVehicleBoardingPhase phase = AICF_EVehicleBoardingPhase.DRIVER;
		if (role == EAICompartmentType.Turret)
			phase = AICF_EVehicleBoardingPhase.GUNNER;
		state.BeginPhase(phase, state.GetCurrentPhaseIndex() + 1, System.GetTickCount());
		AICF_VehicleBoardingActionToken crewToken;
		string failureReason;
		if (!IssueCrewAction(
			trip, lease, group, state, causationId, role, preferredAgent, 0, crewToken, failureReason))
		{
			return Reject(trip, lease, causationId, failureReason);
		}
		IEntity excludedEntity;
		if (crewToken)
			excludedEntity = crewToken.GetReservedEntity();
		if (!IssueApproachActions(
			trip, lease, group, state, causationId, excludedEntity, failureReason))
		{
			return Reject(trip, lease, causationId, failureReason);
		}
		string allowance = "PILOT_EXACT_ACTION";
		if (role == EAICompartmentType.Turret)
			allowance = "TURRET_EXACT_ACTION";
		ReportPhaseStarted(trip, lease, state, causationId, allowance);
		return AICF_TripOutcome.Wait("BOARDING_CREW_ACTION_ACTIVE", causationId);
	}

	protected AICF_TripOutcome ProcessCrewPhase(
		AICF_TransportTrip trip,
		AICF_VehicleLease lease,
		SCR_AIGroup group,
		AICF_VehicleBoardingState state,
		string causationId,
		EAICompartmentType role,
		float farthestDistanceMeters)
	{
		AICF_VehicleBoardingActionToken crewToken = FindCrewToken(state, role);
		IEntity roleOccupant = ResolveRoleOccupant(lease.GetVehicle(), role);
		bool occupantSettled = m_Watchdog.IsAliveGroupMember(group, roleOccupant) &&
			m_Watchdog.IsMemberSettledInVehicle(roleOccupant, lease.GetVehicle());
		if (occupantSettled && crewToken && roleOccupant != crewToken.GetReservedEntity())
			return RejectRoleViolation(trip, lease, state, causationId, "EXACT_ROLE_STOLEN");
		IEntity excludedEntity;
		if (crewToken)
			excludedEntity = crewToken.GetReservedEntity();
		string approachFailure;
		if (!MaintainApproachActions(
			trip, lease, group, state, causationId, excludedEntity, approachFailure))
		{
			return Reject(trip, lease, causationId, approachFailure);
		}

		if (!occupantSettled)
		{
			string crewFailure;
			if (!MaintainCrewAction(
				trip, lease, group, state, causationId, role, crewToken, crewFailure))
			{
				return Reject(trip, lease, causationId, crewFailure);
			}
			return AICF_TripOutcome.Wait("BOARDING_CREW_PENDING", causationId);
		}

		bool staged = farthestDistanceMeters <= GetStagingThresholdMeters();
		if (state.ObserveStaging(staged) < SETTLED_POLLS_REQUIRED)
			return AICF_TripOutcome.Wait("BOARDING_NONCREW_STAGING_PENDING", causationId);
		if (crewToken)
		{
			crewToken.ReleaseTrackingOwnerSafe();
			state.RemoveActionFence(crewToken.GetFence());
			state.GetTokens().Remove(crewToken);
		}
		string eventName = "DRIVER_ASSIGNED";
		if (role == EAICompartmentType.Turret)
			eventName = "GUNNER_ASSIGNED";
		AICF_Stage3Diagnostics.Info(
			eventName,
			DescribeContext(trip, lease, causationId, "EXACT_ROLE_SETTLED") +
			string.Format(" agent=%1", roleOccupant.GetID()));
		return ContinueRoleOrder(trip, lease, group, state, causationId);
	}

	protected AICF_TripOutcome BeginPassengerPhase(
		AICF_TransportTrip trip,
		AICF_VehicleLease lease,
		SCR_AIGroup group,
		AICF_VehicleBoardingState state,
		string causationId)
	{
		state.BeginPhase(
			AICF_EVehicleBoardingPhase.PASSENGERS,
			state.GetCurrentPhaseIndex() + 1,
			System.GetTickCount());
		int issuedCount;
		string failureReason;
		if (!IssuePassengerPlan(
			trip, lease, group, state, causationId, issuedCount, failureReason))
		{
			return Reject(trip, lease, causationId, failureReason);
		}
		ReportPhaseStarted(trip, lease, state, causationId, "EXACT_PER_MEMBER_CARGO_AFTER_CREW");
		AICF_Stage3Diagnostics.Info(
			"PASSENGERS_ASSIGNED",
			DescribeContext(trip, lease, causationId, "ROLE_ORDERED_GET_IN") +
			string.Format(
				" issued=%1 policy=ATOMIC_EXACT_CARGO_AFTER_MANDATORY_CREW",
				issuedCount));
		return AICF_TripOutcome.Wait("BOARDING_PASSENGERS_ACTIVE", causationId);
	}

	protected AICF_TripOutcome ProcessPassengers(
		AICF_TransportTrip trip,
		AICF_VehicleLease lease,
		SCR_AIGroup group,
		AICF_VehicleBoardingState state,
		string causationId,
		int aliveCount,
		int linkedCount,
		int gettingInCount,
		int gettingOutCount,
		int settledCount,
		string memberSamples)
	{
		bool driverSettled = IsDriverSettled(group, lease.GetVehicle());
		bool gunnerRequired = lease.GetKind() == AICF_EVehicleKind.ARMED_LIGHT;
		bool gunnerSettled = IsGunnerSettled(group, lease.GetVehicle(), lease.GetKind());
		if (!driverSettled || (gunnerRequired && !gunnerSettled))
		{
			string lostRole = "DRIVER";
			if (driverSettled)
				lostRole = "GUNNER";
			string lostDetails = DescribeContext(
				trip, lease, causationId, "CREW_ROLE_LOST_DURING_BOARDING");
			lostDetails += string.Format(
				" phase=PASSENGERS lost_role=%1 driver_settled=%2 gunner_required=%3 gunner_settled=%4",
				lostRole,
				driverSettled,
				gunnerRequired,
				gunnerSettled);
			lostDetails += string.Format(
				" alive=%1 linked=%2 getting_in=%3 getting_out=%4 settled=%5 tracked_crew_action=NONE tracked_crew_action_current=0",
				aliveCount,
				linkedCount,
				gettingInCount,
				gettingOutCount,
				settledCount);
			lostDetails += string.Format(" members=[%1]", memberSamples);
			AICF_Stage3Diagnostics.Warning(
				"BOARDING_CREW_ROLE_LOST",
				lostDetails);
			return Reject(trip, lease, causationId, "CREW_ROLE_LOST_DURING_BOARDING");
		}
		string failureReason;
		if (!MaintainPassengerActions(
			trip, lease, group, state, causationId, failureReason))
		{
			AICF_Stage3Diagnostics.Warning(
				"PASSENGER_BOARDING_ACTION_FAILED",
				DescribeContext(trip, lease, causationId, failureReason));
			return Reject(trip, lease, causationId, failureReason);
		}
		int mountedCount = m_Watchdog.CountAliveGroupMembersInVehicle(group, lease.GetVehicle());
		bool transitionsClear = gettingInCount == 0 && gettingOutCount == 0 &&
			settledCount == aliveCount &&
			m_Watchdog.AreAllAliveMembersSettledInVehicle(group, lease.GetVehicle());
		int settledPolls = state.ObserveSettled(aliveCount, mountedCount, transitionsClear);
		if (settledPolls < SETTLED_POLLS_REQUIRED)
			return AICF_TripOutcome.Wait("BOARDING_SETTLED_CONFIRMATION_PENDING", causationId);
		state.GetTokens().CancelAllOwnerSafe();
		string completeDetails = DescribeContext(trip, lease, causationId, "ALL_ALIVE_MEMBERS_MOUNTED");
		completeDetails += string.Format(
			" alive=%1 mounted=%2 settled=%3 settled_polls=%4 max_linked=%5 max_compartment=%6",
			aliveCount,
			mountedCount,
			settledCount,
			settledPolls,
			state.GetMaxLinkedCount(),
			state.GetMaxCompartmentCount());
		completeDetails += string.Format(
			" max_getting_in=%1 max_character_vehicle=%2 max_settled=%3",
			state.GetMaxGettingInCount(),
			state.GetMaxCharacterVehicleCount(),
			state.GetMaxSettledCount());
		AICF_Stage3Diagnostics.Info("BOARDING_COMPLETE", completeDetails);
		return AICF_TripOutcome.StartMovement("BOARDING_COMPLETE", causationId);
	}

	protected AICF_TripOutcome EnforceDeadline(
		AICF_TransportTrip trip,
		AICF_VehicleLease lease,
		AICF_VehicleBoardingState state,
		string causationId,
		int gettingInCount,
		int aliveCount,
		int linkedCount,
		int settledCount,
		string memberSamples)
	{
		int nowMs = System.GetTickCount();
		if (!state.IsSoftDeadlineReached(nowMs))
			return AICF_TripOutcome.Wait("BOARDING_ACTIVE", causationId);
		bool graceEligible = gettingInCount > 0 ||
			state.HasRecentProgress(nowMs, PROGRESS_FRESH_MS);
		bool graceWasEvaluated = state.IsGraceEvaluated();
		bool graceGranted = state.EvaluateOneProgressGrace(
			graceEligible,
			nowMs,
			TRANSITION_GRACE_MS);
		if (graceGranted && !graceWasEvaluated)
		{
			string graceDetails = DescribeContext(trip, lease, causationId, "VERIFIED_PROGRESS_AT_SOFT_DEADLINE");
			graceDetails += string.Format(
				" phase=%1 grace_ms=%2 phase_deadline_ms=%3 total_deadline_ms=%4 getting_in=%5",
				typename.EnumToString(AICF_EVehicleBoardingPhase, state.GetPhase()),
				TRANSITION_GRACE_MS,
				state.GetPhaseDeadlineMs(),
				state.GetAbsoluteDeadlineMs(),
				gettingInCount);
			AICF_Stage3Diagnostics.Info("BOARDING_TRANSITION_GRACE", graceDetails);
		}
		if (state.CanWaitInGrace(nowMs))
			return AICF_TripOutcome.Wait("BOARDING_PROGRESS_GRACE_ACTIVE", causationId);

		string reason = "BOARDING_PHASE_DEADLINE_EXCEEDED";
		if (state.GetAbsoluteDeadlineMs() > 0 && nowMs >= state.GetAbsoluteDeadlineMs())
			reason = "BOARDING_TOTAL_DEADLINE_EXCEEDED";
		if (state.IsGraceGranted())
			reason = "BOARDING_PROGRESS_GRACE_DEADLINE_EXCEEDED";
		string cause = ResolveBoardingTimeoutCause(state.GetPhase());
		string deadlineScope = ResolveBoardingDeadlineScope(state, nowMs);
		SCR_AIGroup group = trip.GetAssignment().GetGroup();
		int mountedCount = m_Watchdog.CountAliveGroupMembersInVehicle(group, lease.GetVehicle());
		bool driverSettled = IsDriverSettled(group, lease.GetVehicle());
		bool gunnerRequired = lease.GetKind() == AICF_EVehicleKind.ARMED_LIGHT;
		bool gunnerSettled = IsGunnerSettled(group, lease.GetVehicle(), lease.GetKind());
		string timeoutDetails = DescribeContext(trip, lease, causationId, reason);
		timeoutDetails += string.Format(
			" phase=%1 cause=%2 deadline_scope=%3 alive=%4 mounted=%5 linked=%6 settled=%7",
			typename.EnumToString(AICF_EVehicleBoardingPhase, state.GetPhase()),
			cause,
			deadlineScope,
			aliveCount,
			mountedCount,
			linkedCount,
			settledCount);
		timeoutDetails += string.Format(
			" phase_started_ms=%1 phase_deadline_ms=%2 phase_age_ms=%3 total_started_ms=%4 total_deadline_ms=%5 total_age_ms=%6",
			state.GetPhaseStartedAtMs(),
			state.GetPhaseDeadlineMs(),
			nowMs - state.GetPhaseStartedAtMs(),
			state.GetStartedAtMs(),
			state.GetAbsoluteDeadlineMs(),
			nowMs - state.GetStartedAtMs());
		timeoutDetails += string.Format(
			" planned_phases=%1 grace_granted=%2 driver_settled=%3 gunner_required=%4 gunner_settled=%5 group_boarding_waypoint=0 vehicle_utility_attached=0",
			state.GetPlannedPhaseCount(),
			state.IsGraceGranted(),
			driverSettled,
			gunnerRequired,
			gunnerSettled);
		timeoutDetails += string.Format(
			" max_linked=%1 max_compartment=%2 max_getting_in=%3 max_character_vehicle=%4 max_settled=%5 best_farthest_m=%6",
			state.GetMaxLinkedCount(),
			state.GetMaxCompartmentCount(),
			state.GetMaxGettingInCount(),
			state.GetMaxCharacterVehicleCount(),
			state.GetMaxSettledCount(),
			state.GetBestFarthestDistanceMeters());
		timeoutDetails += string.Format(" members=[%1]", memberSamples);
		timeoutDetails += string.Format(
			" exact_tokens=[%1]",
			DescribeExactCargoTokens(state, lease));
		AICF_Stage3Diagnostics.Warning("BOARDING_TIMEOUT", timeoutDetails);
		return AICF_TripOutcome.FallbackToFoot(reason, causationId);
	}

	protected string ResolveBoardingTimeoutCause(AICF_EVehicleBoardingPhase phase)
	{
		switch (phase)
		{
			case AICF_EVehicleBoardingPhase.APPROACH: return "APPROACH_NOT_COMPLETE";
			case AICF_EVehicleBoardingPhase.DRIVER: return "DRIVER_NOT_ASSIGNED";
			case AICF_EVehicleBoardingPhase.GUNNER: return "GUNNER_NOT_ASSIGNED";
			case AICF_EVehicleBoardingPhase.PASSENGERS: return "PASSENGERS_NOT_MOUNTED";
		}
		return "BOARDING_PHASE_INVALID";
	}

	protected string ResolveBoardingDeadlineScope(
		AICF_VehicleBoardingState state,
		int nowMs)
	{
		if (state.IsGraceGranted())
			return "PROGRESS_GRACE";
		if (state.GetAbsoluteDeadlineMs() > 0 && nowMs >= state.GetAbsoluteDeadlineMs())
			return "TOTAL";
		return "PHASE";
	}

	protected AICF_TripOutcome Reject(
		AICF_TransportTrip trip,
		AICF_VehicleLease lease,
		string causationId,
		string reason)
	{
		AICF_Stage3Diagnostics.Warning(
			"BOARDING_REJECTED",
			DescribeContext(trip, lease, causationId, reason));
		return AICF_TripOutcome.FallbackToFoot(reason, causationId);
	}

	protected AICF_TripOutcome RejectRoleViolation(
		AICF_TransportTrip trip,
		AICF_VehicleLease lease,
		AICF_VehicleBoardingState state,
		string causationId,
		string cause)
	{
		string details = DescribeContext(trip, lease, causationId, "ROLE_ORDERING_FAILED");
		details += string.Format(
			" cause=%1 reset_age_ms=%2 retry_issued=%3 exact_eject=0 teleport=0",
			cause,
			state.GetRoleResetAgeMs(System.GetTickCount()),
			state.IsRoleRetryIssued());
		AICF_Stage3Diagnostics.Warning("BOARDING_ROLE_VIOLATION", details);
		return AICF_TripOutcome.FallbackToFoot("BOARDING_ROLE_VIOLATION", causationId);
	}

	protected int CalculatePlannedPhaseCount(
		bool approachPlanned,
		bool driverPlanned,
		bool gunnerPlanned)
	{
		int planned = 1;
		if (approachPlanned)
			planned++;
		if (driverPlanned)
			planned++;
		if (gunnerPlanned)
			planned++;
		return planned;
	}

	protected float GetStagingThresholdMeters()
	{
		return Math.Min(STAGING_THRESHOLD_METERS, m_Config.GetMaximumReuseDistanceMeters());
	}

	protected int GetRoleResetTimeoutMs()
	{
		int timeoutMs = m_Config.GetBoardingTimeoutMs() / 2;
		if (timeoutMs > 10000)
			timeoutMs = 10000;
		if (timeoutMs < 1000)
			timeoutMs = 1000;
		return timeoutMs;
	}

	protected SCR_AIVehicleUsageComponent ResolveUsage(Vehicle vehicle)
	{
		if (!vehicle)
			return null;
		return SCR_AIVehicleUsageComponent.Cast(
			vehicle.FindComponent(SCR_AIVehicleUsageComponent));
	}

	protected string ResolveLiveRplId(Vehicle vehicle)
	{
		if (!vehicle)
			return string.Empty;
		RplComponent rpl = RplComponent.Cast(vehicle.FindComponent(RplComponent));
		if (!rpl)
			return string.Empty;
		return rpl.Id().ToString();
	}

	protected bool IsLeaseLiveIdentity(AICF_VehicleLease lease)
	{
		if (!lease || !lease.HasPhysicalAsset() || !lease.GetVehicle())
			return false;
		Vehicle vehicle = lease.GetVehicle();
		string liveRplId = ResolveLiveRplId(vehicle);
		return !liveRplId.IsEmpty() &&
			lease.MatchesEntityIdentity(vehicle, vehicle.GetID(), liveRplId);
	}

	protected IEntity ResolveRoleOccupant(Vehicle vehicle, EAICompartmentType role)
	{
		SCR_AIVehicleUsageComponent usage = ResolveUsage(vehicle);
		BaseCompartmentSlot roleSlot = ResolveRoleSlot(usage, role);
		if (!roleSlot)
			return null;
		return roleSlot.GetOccupant();
	}

	protected BaseCompartmentSlot ResolveRoleSlot(
		SCR_AIVehicleUsageComponent usage,
		EAICompartmentType role)
	{
		if (!usage)
			return null;
		if (role == EAICompartmentType.Pilot)
			return usage.GetPilotCompartmentSlot();
		if (role == EAICompartmentType.Turret)
			return usage.GetTurretCompartmentSlot();
		return null;
	}

	protected bool IsDriverSettled(SCR_AIGroup group, Vehicle vehicle)
	{
		IEntity driver = ResolveRoleOccupant(vehicle, EAICompartmentType.Pilot);
		return m_Watchdog.IsAliveGroupMember(group, driver) &&
			m_Watchdog.IsMemberSettledInVehicle(driver, vehicle);
	}

	protected bool IsGunnerSettled(
		SCR_AIGroup group,
		Vehicle vehicle,
		AICF_EVehicleKind kind)
	{
		if (kind != AICF_EVehicleKind.ARMED_LIGHT)
			return true;
		IEntity gunner = ResolveRoleOccupant(vehicle, EAICompartmentType.Turret);
		return m_Watchdog.IsAliveGroupMember(group, gunner) &&
			m_Watchdog.IsMemberSettledInVehicle(gunner, vehicle);
	}

	protected int CountWrongSeatMembers(
		SCR_AIGroup group,
		Vehicle vehicle,
		AICF_EVehicleKind kind)
	{
		if (!group || !vehicle)
			return 0;
		IEntity driver;
		IEntity gunner;
		if (IsDriverSettled(group, vehicle))
			driver = ResolveRoleOccupant(vehicle, EAICompartmentType.Pilot);
		if (driver && kind == AICF_EVehicleKind.ARMED_LIGHT &&
			IsGunnerSettled(group, vehicle, kind))
		{
			gunner = ResolveRoleOccupant(vehicle, EAICompartmentType.Turret);
		}
		array<AIAgent> agents = {};
		group.GetAgents(agents);
		int wrongCount;
		foreach (AIAgent agent : agents)
		{
			if (!agent || agent.GetParentGroup() != group)
				continue;
			IEntity entity = agent.GetControlledEntity();
			if (!AICF_GroupRuntime.IsAliveCharacter(entity) ||
				CompartmentAccessComponent.GetVehicleIn(entity) != vehicle)
			{
				continue;
			}
			if (entity != driver && entity != gunner)
				wrongCount++;
		}
		return wrongCount;
	}

	protected bool RequestAnimatedWrongSeatExit(
		AICF_TransportTrip trip,
		AICF_VehicleLease lease,
		SCR_AIGroup group,
		AICF_VehicleBoardingState state,
		string causationId,
		out int requestedCount,
		out string failureReason)
	{
		requestedCount = 0;
		failureReason = string.Empty;
		if (!IsLeaseLiveIdentity(lease))
		{
			failureReason = "ROLE_RESET_LIVE_IDENTITY_MISMATCH";
			return false;
		}
		Vehicle vehicle = lease.GetVehicle();
		IEntity driver;
		IEntity gunner;
		if (IsDriverSettled(group, vehicle))
			driver = ResolveRoleOccupant(vehicle, EAICompartmentType.Pilot);
		if (driver && lease.GetKind() == AICF_EVehicleKind.ARMED_LIGHT &&
			IsGunnerSettled(group, vehicle, lease.GetKind()))
		{
			gunner = ResolveRoleOccupant(vehicle, EAICompartmentType.Turret);
		}
		array<AIAgent> agents = {};
		group.GetAgents(agents);
		foreach (AIAgent agent : agents)
		{
			if (!agent || agent.GetParentGroup() != group)
				continue;
			ChimeraCharacter character = ChimeraCharacter.Cast(agent.GetControlledEntity());
			if (!AICF_GroupRuntime.IsAliveCharacter(character) ||
				CompartmentAccessComponent.GetVehicleIn(character) != vehicle ||
				character == driver || character == gunner)
			{
				continue;
			}
			CompartmentAccessComponent access = character.GetCompartmentAccessComponent();
			if (!access)
			{
				failureReason = "ROLE_RESET_ACCESS_MISSING";
				return false;
			}
			BaseCompartmentSlot compartment = access.GetCompartment();
			AICF_VehicleAsyncFence fence = CreateFence(
				trip, lease, state, causationId, "ROLE_RESET", agent);
			AICF_VehicleBoardingActionToken token = new AICF_VehicleBoardingActionToken(
				fence,
				agent,
				character,
				vehicle,
				null,
				null,
				compartment,
				EAICompartmentType.Cargo,
				0,
				0);
			if (!TrackToken(state, token))
			{
				failureReason = "ROLE_RESET_TOKEN_LIMIT";
				return false;
			}
			access.InterruptVehicleActionQueue(true, true, true);
			bool accepted = access.GetOutVehicle(
				EGetOutType.ANIMATED,
				-1,
				ECloseDoorAfterActions.INVALID,
				false,
				false);
			ReportPassengerAction(
				"PASSENGER_ACTION_TRANSITION",
				trip, lease, token, causationId, "WRONG_SEAT_ANIMATED_GET_OUT_REQUESTED", accepted);
			if (!accepted && !access.IsGettingOut())
			{
				failureReason = "ROLE_RESET_ANIMATED_GET_OUT_REJECTED";
				return false;
			}
			requestedCount++;
		}
		return requestedCount > 0;
	}

	protected bool IssueApproachActions(
		AICF_TransportTrip trip,
		AICF_VehicleLease lease,
		SCR_AIGroup group,
		AICF_VehicleBoardingState state,
		string causationId,
		IEntity excludedEntity,
		out string failureReason)
	{
		failureReason = string.Empty;
		Vehicle vehicle = lease.GetVehicle();
		float thresholdMeters = GetStagingThresholdMeters();
		float radiusMeters = Math.Max(
			5.0,
			Math.Min(APPROACH_ACTION_RADIUS_METERS, thresholdMeters - 5.0));
		array<AIAgent> agents = {};
		group.GetAgents(agents);
		int aliveCount;
		foreach (AIAgent agent : agents)
		{
			if (!agent || agent.GetParentGroup() != group)
				continue;
			IEntity entity = agent.GetControlledEntity();
			if (!AICF_GroupRuntime.IsAliveCharacter(entity))
				continue;
			aliveCount++;
			if (entity == excludedEntity ||
				CompartmentAccessComponent.GetVehicleIn(entity) == vehicle)
			{
				continue;
			}
			float distanceMeters = vector.DistanceXZ(entity.GetOrigin(), vehicle.GetOrigin());
			if (distanceMeters <= thresholdMeters || state.GetTokens().FindByAgent(agent))
				continue;
			if (!IssueOneApproach(
				trip, lease, state, causationId, agent, distanceMeters, radiusMeters, 0))
			{
				failureReason = string.Format("APPROACH_ACTION_CREATE_FAILED_MEMBER_%1", entity.GetID());
				return false;
			}
		}
		if (aliveCount <= 0)
		{
			failureReason = "APPROACH_GROUP_EMPTY";
			return false;
		}
		return true;
	}

	protected bool MaintainApproachActions(
		AICF_TransportTrip trip,
		AICF_VehicleLease lease,
		SCR_AIGroup group,
		AICF_VehicleBoardingState state,
		string causationId,
		IEntity excludedEntity,
		out string failureReason)
	{
		failureReason = string.Empty;
		Vehicle vehicle = lease.GetVehicle();
		float thresholdMeters = GetStagingThresholdMeters();
		float radiusMeters = Math.Max(
			5.0,
			Math.Min(APPROACH_ACTION_RADIUS_METERS, thresholdMeters - 5.0));
		AICF_VehicleBoardingTokenSet tokens = state.GetTokens();
		for (int tokenIndex = tokens.Count() - 1; tokenIndex >= 0; tokenIndex--)
		{
			AICF_VehicleBoardingActionToken token = tokens.Get(tokenIndex);
			if (!token || !token.GetApproachAction())
				continue;
			AIAgent tokenAgent = token.GetAgent();
			IEntity tokenEntity;
			if (tokenAgent)
				tokenEntity = tokenAgent.GetControlledEntity();
			bool relevant = token.Matches(trip, lease, group) && tokenEntity != excludedEntity &&
				CompartmentAccessComponent.GetVehicleIn(tokenEntity) != vehicle;
			if (!relevant)
			{
				token.CancelOwnerSafe();
				state.RemoveActionFence(token.GetFence());
				tokens.Remove(token);
				continue;
			}
			float distanceMeters = vector.DistanceXZ(tokenEntity.GetOrigin(), vehicle.GetOrigin());
			if (distanceMeters <= thresholdMeters)
			{
				token.CancelOwnerSafe();
				state.RemoveActionFence(token.GetFence());
				tokens.Remove(token);
				continue;
			}
			token.ObserveSpatialProgress(distanceMeters, APPROACH_PROGRESS_METERS);
			EAIActionState actionState = token.GetActionState();
			bool terminal = actionState == EAIActionState.COMPLETED ||
				actionState == EAIActionState.FAILED;
			bool stalled = token.GetProgressAgeMs() >= APPROACH_STALL_MS;
			if (!terminal && !stalled)
				continue;
			if (token.GetRetryCount() >= APPROACH_MAX_RETRIES)
			{
				failureReason = string.Format(
					"APPROACH_MEMBER_%1_BUDGET_EXHAUSTED",
					tokenEntity.GetID());
				string stalledDetails = DescribeTokenContext(
					trip, lease, token, causationId, "BOUNDED_MEMBER_STALL");
				stalledDetails += string.Format(
					" member=%1 distance_m=%2 threshold_m=%3 progress_age_ms=%4 maximum_retries=%5",
					tokenEntity.GetID(),
					distanceMeters,
					thresholdMeters,
					token.GetProgressAgeMs(),
					APPROACH_MAX_RETRIES);
				stalledDetails += string.Format(
					" terminal=%1 stalled=%2 committed_failure=1 next_action=FALLBACK_TO_FOOT",
					terminal,
					stalled);
				AICF_Stage3Diagnostics.Warning(
					"BOARDING_APPROACH_MEMBER_STALLED",
					stalledDetails);
				ReportPassengerAction(
					"PASSENGER_ACTION_FAILURE",
					trip, lease, token, causationId, failureReason, false);
				return false;
			}
			int retryCount = token.GetRetryCount() + 1;
			token.CancelOwnerSafe();
			state.RemoveActionFence(token.GetFence());
			tokens.Remove(token);
			if (!IssueOneApproach(
				trip, lease, state, causationId, tokenAgent, distanceMeters, radiusMeters, retryCount))
			{
				failureReason = string.Format(
					"APPROACH_RETRY_CREATE_FAILED_MEMBER_%1",
					tokenEntity.GetID());
				return false;
			}
			AICF_Stage3Diagnostics.Warning(
				"BOARDING_APPROACH_REISSUED",
				DescribeContext(trip, lease, causationId, "BOUNDED_MEMBER_RETRY") +
				string.Format(
					" member=%1 distance_m=%2 retry=%3 maximum_retries=%4 previous_state=%5",
					tokenEntity.GetID(),
					distanceMeters,
					retryCount,
					APPROACH_MAX_RETRIES,
					typename.EnumToString(EAIActionState, actionState)));
		}
		return IssueApproachActions(
			trip, lease, group, state, causationId, excludedEntity, failureReason);
	}

	protected bool IssueOneApproach(
		AICF_TransportTrip trip,
		AICF_VehicleLease lease,
		AICF_VehicleBoardingState state,
		string causationId,
		AIAgent agent,
		float distanceMeters,
		float radiusMeters,
		int retryCount)
	{
		if (!agent || !IsLeaseLiveIdentity(lease))
			return false;
		IEntity entity = agent.GetControlledEntity();
		SCR_AIUtilityComponent utility = ResolveOwnedUtility(agent, entity);
		if (!utility)
			return false;
		SCR_AIMoveIndividuallyBehavior action = new SCR_AIMoveIndividuallyBehavior(
			utility,
			null,
			lease.GetVehicle().GetOrigin(),
			SCR_AIActionBase.PRIORITY_BEHAVIOR_MOVE_INDIVIDUALLY,
			SCR_AIActionBase.PRIORITY_LEVEL_PLAYER,
			lease.GetVehicle(),
			radiusMeters);
		AICF_VehicleAsyncFence fence = CreateFence(
			trip, lease, state, causationId, "APPROACH", agent);
		AICF_VehicleBoardingActionToken token = new AICF_VehicleBoardingActionToken(
			fence,
			agent,
			entity,
			lease.GetVehicle(),
			action,
			null,
			null,
			EAICompartmentType.Cargo,
			retryCount,
			distanceMeters);
		if (!TrackToken(state, token))
			return false;
		utility.AddAction(action);
		return true;
	}

	protected bool IssueCrewAction(
		AICF_TransportTrip trip,
		AICF_VehicleLease lease,
		SCR_AIGroup group,
		AICF_VehicleBoardingState state,
		string causationId,
		EAICompartmentType role,
		AIAgent preferredAgent,
		int retryCount,
		out AICF_VehicleBoardingActionToken issuedToken,
		out string failureReason)
	{
		failureReason = string.Empty;
		issuedToken = null;
		if (!IsLeaseLiveIdentity(lease))
		{
			failureReason = "MANDATORY_CREW_LIVE_IDENTITY_MISMATCH";
			return false;
		}
		SCR_AIVehicleUsageComponent usage = ResolveUsage(lease.GetVehicle());
		BaseCompartmentSlot roleSlot = ResolveRoleSlot(usage, role);
		if (!roleSlot || !roleSlot.IsCompartmentAccessible() || roleSlot.GetOccupant() ||
			(roleSlot.IsReserved() && (!preferredAgent ||
			!roleSlot.IsReservedBy(preferredAgent.GetControlledEntity()))))
		{
			failureReason = "MANDATORY_ROLE_SLOT_UNAVAILABLE";
			return false;
		}
		IEntity excludedEntity;
		EAICompartmentType otherRole = EAICompartmentType.Turret;
		if (role == EAICompartmentType.Turret)
			otherRole = EAICompartmentType.Pilot;
		excludedEntity = ResolveRoleOccupant(lease.GetVehicle(), otherRole);
		AIAgent agent = SelectCrewAgent(group, preferredAgent, excludedEntity);
		if (!agent)
		{
			failureReason = "MANDATORY_CREW_AGENT_UNAVAILABLE";
			return false;
		}
		IEntity entity = agent.GetControlledEntity();
		SCR_AIUtilityComponent utility = ResolveOwnedUtility(agent, entity);
		if (!utility)
		{
			failureReason = "MANDATORY_CREW_UTILITY_INVALID";
			return false;
		}
		roleSlot.SetReserved(entity);
		if (!roleSlot.IsReservedBy(entity))
		{
			failureReason = "MANDATORY_ROLE_RESERVATION_FAILED";
			return false;
		}
		SCR_AIGetInVehicle action = new SCR_AIGetInVehicle(
			utility,
			null,
			lease.GetVehicle(),
			roleSlot,
			role,
			SCR_AIActionBase.PRIORITY_BEHAVIOR_GET_IN_VEHICLE,
			SCR_AIActionBase.PRIORITY_LEVEL_NORMAL);
		AICF_VehicleAsyncFence fence = CreateFence(
			trip, lease, state, causationId, "CREW", agent);
		issuedToken = new AICF_VehicleBoardingActionToken(
			fence,
			agent,
			entity,
			lease.GetVehicle(),
			null,
			action,
			roleSlot,
			role,
			retryCount,
			vector.DistanceXZ(entity.GetOrigin(), lease.GetVehicle().GetOrigin()));
		if (!TrackToken(state, issuedToken))
		{
			issuedToken.ReleaseReservationOwnerSafe();
			issuedToken = null;
			failureReason = "MANDATORY_CREW_TOKEN_LIMIT";
			return false;
		}
		utility.AddAction(action);
		ReportPassengerAction(
			"PASSENGER_ACTION_TRANSITION",
			trip, lease, issuedToken, causationId, "EXACT_MANDATORY_ROLE_ISSUED", true);
		return true;
	}

	protected bool MaintainCrewAction(
		AICF_TransportTrip trip,
		AICF_VehicleLease lease,
		SCR_AIGroup group,
		AICF_VehicleBoardingState state,
		string causationId,
		EAICompartmentType role,
		AICF_VehicleBoardingActionToken token,
		out string failureReason)
	{
		failureReason = string.Empty;
		if (!token || !token.Matches(trip, lease, group))
		{
			if (token)
			{
				token.CancelOwnerSafe();
				state.RemoveActionFence(token.GetFence());
				state.GetTokens().Remove(token);
			}
			failureReason = "MANDATORY_CREW_TOKEN_STALE";
			return false;
		}
		CompartmentAccessComponent access = ResolveAccess(token.GetReservedEntity());
		if (access && (access.IsGettingIn() || access.IsGettingOut()))
			return true;
		EAIActionState actionState = token.GetActionState();
		if (actionState != EAIActionState.COMPLETED && actionState != EAIActionState.FAILED)
			return true;
		if (token.GetRetryCount() >= CREW_MAX_RETRIES)
		{
			failureReason = "MANDATORY_CREW_ACTION_BUDGET_EXHAUSTED";
			ReportPassengerAction(
				"PASSENGER_ACTION_FAILURE",
				trip, lease, token, causationId, failureReason, false);
			return false;
		}
		AIAgent retryAgent = token.GetAgent();
		int retryCount = token.GetRetryCount() + 1;
		token.CancelOwnerSafe();
		state.RemoveActionFence(token.GetFence());
		state.GetTokens().Remove(token);
		AICF_VehicleBoardingActionToken retryToken;
		if (!IssueCrewAction(
			trip, lease, group, state, causationId, role, retryAgent,
			retryCount, retryToken, failureReason))
		{
			return false;
		}
		AICF_Stage3Diagnostics.Warning(
			"PASSENGER_BOARDING_REISSUED",
			DescribeTokenContext(trip, lease, retryToken, causationId, "MANDATORY_CREW_REISSUED"));
		return true;
	}

	protected AICF_VehicleBoardingActionToken FindCrewToken(
		AICF_VehicleBoardingState state,
		EAICompartmentType role)
	{
		AICF_VehicleBoardingTokenSet tokens = state.GetTokens();
		for (int index = 0; index < tokens.Count(); index++)
		{
			AICF_VehicleBoardingActionToken token = tokens.Get(index);
			if (token && token.GetGetInAction() && token.GetCompartmentType() == role)
				return token;
		}
		return null;
	}

	protected AIAgent SelectCrewAgent(
		SCR_AIGroup group,
		AIAgent preferredAgent,
		IEntity excludedEntity)
	{
		if (preferredAgent && preferredAgent.GetParentGroup() == group &&
			preferredAgent.GetControlledEntity() != excludedEntity &&
			AICF_GroupRuntime.IsAliveCharacter(preferredAgent.GetControlledEntity()))
		{
			return preferredAgent;
		}
		array<AIAgent> agents = {};
		group.GetAgents(agents);
		foreach (AIAgent agent : agents)
		{
			if (!agent || agent.GetParentGroup() != group)
				continue;
			IEntity entity = agent.GetControlledEntity();
			if (entity != excludedEntity && AICF_GroupRuntime.IsAliveCharacter(entity) &&
				CompartmentAccessComponent.GetVehicleIn(entity) == null)
			{
				return agent;
			}
		}
		return null;
	}

	protected bool IssuePassengerPlan(
		AICF_TransportTrip trip,
		AICF_VehicleLease lease,
		SCR_AIGroup group,
		AICF_VehicleBoardingState state,
		string causationId,
		out int issuedCount,
		out string failureReason)
	{
		issuedCount = 0;
		failureReason = string.Empty;
		if (!IsLeaseLiveIdentity(lease))
		{
			failureReason = "PASSENGER_LIVE_IDENTITY_MISMATCH";
			return false;
		}
		array<AIAgent> pendingAgents = {};
		array<IEntity> pendingEntities = {};
		array<SCR_AIUtilityComponent> pendingUtilities = {};
		array<BaseCompartmentSlot> assignedCompartments = {};
		array<AIAgent> agents = {};
		group.GetAgents(agents);
		foreach (AIAgent agent : agents)
		{
			if (!agent || agent.GetParentGroup() != group)
				continue;
			IEntity entity = agent.GetControlledEntity();
			if (!AICF_GroupRuntime.IsAliveCharacter(entity))
				continue;
			if (m_Watchdog.IsMemberSettledInVehicle(entity, lease.GetVehicle()))
			{
				if (!IsSupportedSettledCompartment(lease, entity))
				{
					failureReason = "PASSENGER_PREMOUNTED_COMPARTMENT_UNSUPPORTED";
					return false;
				}
				continue;
			}
			if (CompartmentAccessComponent.GetVehicleIn(entity) == lease.GetVehicle())
			{
				failureReason = "PASSENGER_UNOWNED_TRANSITION_PRESENT";
				return false;
			}
			SCR_AIUtilityComponent utility = ResolveOwnedUtility(agent, entity);
			if (!utility)
			{
				failureReason = "PASSENGER_UTILITY_INVALID";
				return false;
			}
			BaseCompartmentSlot compartment;
			if (!FindAvailableCargoCompartment(
				lease.GetVehicle(), entity, assignedCompartments, compartment))
			{
				failureReason = "PASSENGER_EXACT_CARGO_UNAVAILABLE";
				return false;
			}
			pendingAgents.Insert(agent);
			pendingEntities.Insert(entity);
			pendingUtilities.Insert(utility);
			assignedCompartments.Insert(compartment);
		}
		if (state.GetTokens().Count() + pendingAgents.Count() > MAX_ACTION_TOKENS)
		{
			failureReason = "PASSENGER_TOKEN_LIMIT";
			return false;
		}

		// Atomic mapping: every exact CargoCompartmentSlot is reserved and
		// verified before the first SCR_AIGetInVehicle action is added.
		for (int reserveIndex = 0; reserveIndex < assignedCompartments.Count(); reserveIndex++)
		{
			assignedCompartments[reserveIndex].SetReserved(pendingEntities[reserveIndex]);
			if (!assignedCompartments[reserveIndex].IsReservedBy(pendingEntities[reserveIndex]))
			{
				RollbackReservations(assignedCompartments, pendingEntities);
				failureReason = "PASSENGER_ATOMIC_RESERVATION_FAILED";
				return false;
			}
		}

		for (int actionIndex = 0; actionIndex < pendingAgents.Count(); actionIndex++)
		{
			SCR_AIGetInVehicle action = new SCR_AIGetInVehicle(
				pendingUtilities[actionIndex],
				null,
				lease.GetVehicle(),
				assignedCompartments[actionIndex],
				EAICompartmentType.Cargo,
				SCR_AIActionBase.PRIORITY_BEHAVIOR_GET_IN_VEHICLE,
				SCR_AIActionBase.PRIORITY_LEVEL_PLAYER);
			AICF_VehicleAsyncFence fence = CreateFence(
				trip, lease, state, causationId, "CARGO", pendingAgents[actionIndex]);
			AICF_VehicleBoardingActionToken token = new AICF_VehicleBoardingActionToken(
				fence,
				pendingAgents[actionIndex],
				pendingEntities[actionIndex],
				lease.GetVehicle(),
				null,
				action,
				assignedCompartments[actionIndex],
				EAICompartmentType.Cargo,
				0,
				vector.DistanceXZ(
					pendingEntities[actionIndex].GetOrigin(),
					lease.GetVehicle().GetOrigin()));
			if (!TrackToken(state, token))
			{
				RollbackReservations(assignedCompartments, pendingEntities);
				state.GetTokens().CancelAllOwnerSafe();
				failureReason = "PASSENGER_TOKEN_TRACK_FAILED";
				return false;
			}
			pendingUtilities[actionIndex].AddAction(action);
			issuedCount++;
			ReportPassengerAction(
				"PASSENGER_ACTION_TRANSITION",
				trip, lease, token, causationId, "EXACT_CARGO_ISSUED", true);
		}
		return true;
	}

	protected bool MaintainPassengerActions(
		AICF_TransportTrip trip,
		AICF_VehicleLease lease,
		SCR_AIGroup group,
		AICF_VehicleBoardingState state,
		string causationId,
		out string failureReason)
	{
		failureReason = string.Empty;
		AICF_VehicleBoardingTokenSet tokens = state.GetTokens();
		for (int tokenIndex = tokens.Count() - 1; tokenIndex >= 0; tokenIndex--)
		{
			AICF_VehicleBoardingActionToken token = tokens.Get(tokenIndex);
			if (!token || !token.GetGetInAction() ||
				token.GetCompartmentType() != EAICompartmentType.Cargo)
			{
				continue;
			}
			AIAgent agent = token.GetAgent();
			IEntity entity;
			if (agent)
				entity = agent.GetControlledEntity();
			if (!token.Matches(trip, lease, group))
			{
				token.CancelOwnerSafe();
				state.RemoveActionFence(token.GetFence());
				tokens.Remove(token);
				if (!entity || !AICF_GroupRuntime.IsAliveCharacter(entity))
					continue;
				failureReason = "PASSENGER_ACTION_FENCE_STALE";
				return false;
			}
			if (m_Watchdog.IsMemberSettledInVehicle(entity, lease.GetVehicle()))
			{
				if (!token.IsExactCompartmentSettled(m_Watchdog, lease.GetVehicle()))
				{
					failureReason = "PASSENGER_WRONG_EXACT_COMPARTMENT";
					ReportPassengerAction(
						"PASSENGER_ACTION_FAILURE",
						trip, lease, token, causationId, failureReason, false);
					return false;
				}
				ReportPassengerAction(
					"PASSENGER_ACTION_TRANSITION",
					trip, lease, token, causationId, "EXACT_CARGO_SETTLED", true);
				token.ReleaseTrackingOwnerSafe();
				state.RemoveActionFence(token.GetFence());
				tokens.Remove(token);
				continue;
			}
			CompartmentAccessComponent access = ResolveAccess(entity);
			if (CompartmentAccessComponent.GetVehicleIn(entity) == lease.GetVehicle() ||
				(access && (access.IsGettingIn() || access.IsGettingOut())))
			{
				continue;
			}
			if (token.IsHiddenExactSeatRecoveryPending())
			{
				if (!m_Config.GetHiddenRecoveryEnabled())
				{
					failureReason = "PASSENGER_HIDDEN_EXACT_CARGO_DISABLED";
					ReportPassengerAction(
						"PASSENGER_ACTION_FAILURE",
						trip, lease, token, causationId, failureReason, false);
					return false;
				}
				float nearestPlayerMeters;
				string hiddenFenceReason;
				float playerRadiusMeters = m_Config.GetHiddenRecoveryPlayerRadiusMeters();
				if (!m_Watchdog.CanApplyHiddenRecovery(
					entity.GetOrigin(),
					lease.GetVehicle().GetOrigin(),
					playerRadiusMeters,
					nearestPlayerMeters,
					hiddenFenceReason))
				{
					failureReason = "PASSENGER_HIDDEN_EXACT_CARGO_FENCE_REJECTED";
					AICF_Stage3Diagnostics.Warning(
						"PASSENGER_HIDDEN_EXACT_CARGO_REJECTED",
						DescribeTokenContext(
							trip, lease, token, causationId, hiddenFenceReason) +
						string.Format(
							" player_radius_m=%1 nearest_player_m=%2",
							playerRadiusMeters,
							nearestPlayerMeters));
					return false;
				}
				if (!token.ApplyHiddenExactSeatRecovery())
				{
					failureReason = "PASSENGER_HIDDEN_EXACT_CARGO_APPLY_REJECTED";
					ReportPassengerAction(
						"PASSENGER_ACTION_FAILURE",
						trip, lease, token, causationId, failureReason, false);
					return false;
				}
				AICF_Stage3Diagnostics.Warning(
					"PASSENGER_HIDDEN_EXACT_CARGO_FORCED",
					DescribeTokenContext(
						trip, lease, token, causationId, "NO_PLAYER_PROXIMITY_FORCE_TELEPORT") +
					string.Format(
						" player_radius_m=%1 nearest_player_m=%2",
						playerRadiusMeters,
						nearestPlayerMeters));
				// At most one physical mutation is supervised per scheduler tick. The
				// next tick verifies that the same entity settled in the same Cargo slot.
				return true;
			}
			float distanceMeters = vector.DistanceXZ(
				entity.GetOrigin(),
				lease.GetVehicle().GetOrigin());
			token.ObserveSpatialProgress(distanceMeters, APPROACH_PROGRESS_METERS);
			EAIActionState actionState = token.GetActionState();
			bool actionTerminal = actionState == EAIActionState.COMPLETED ||
				actionState == EAIActionState.FAILED;
			bool runningStalled = actionState == EAIActionState.RUNNING &&
				token.GetProgressAgeMs() >= m_Config.GetPassengerStallMs();
			if (!actionTerminal && !runningStalled)
				continue;
			if (token.GetRetryCount() >= m_Config.GetPassengerMaxRetries())
			{
				if (runningStalled && m_Config.GetHiddenRecoveryEnabled() &&
					token.ScheduleHiddenExactSeatRecovery())
				{
					AICF_Stage3Diagnostics.Warning(
						"PASSENGER_HIDDEN_EXACT_CARGO_SCHEDULED",
						DescribeTokenContext(
							trip, lease, token, causationId, "REPEATED_RUNNING_STALL") +
						string.Format(
							" stall_ms=%1 normal_retry_budget=%2 apply=NEXT_TICK",
							m_Config.GetPassengerStallMs(),
							m_Config.GetPassengerMaxRetries()));
					return true;
				}
				failureReason = "PASSENGER_ACTION_BUDGET_EXHAUSTED";
				if (runningStalled)
					failureReason = "PASSENGER_EXACT_CARGO_STALL_BUDGET_EXHAUSTED";
				ReportPassengerAction(
					"PASSENGER_ACTION_FAILURE",
					trip, lease, token, causationId, failureReason, false);
				return false;
			}
			if (runningStalled)
			{
				AICF_Stage3Diagnostics.Warning(
					"PASSENGER_EXACT_CARGO_STALLED",
					DescribeTokenContext(
						trip, lease, token, causationId, "RUNNING_NO_SPATIAL_PROGRESS"));
			}
			int retryCount = token.GetRetryCount() + 1;
			AIAgent retryAgent = token.GetAgent();
			BaseCompartmentSlot retryCompartment = token.GetCompartment();
			token.CancelOwnerSafe();
			state.RemoveActionFence(token.GetFence());
			tokens.Remove(token);
			AICF_VehicleBoardingActionToken retryToken;
			if (!ReissueExactCargo(
				trip, lease, state, causationId, retryAgent,
				retryCompartment, retryCount, retryToken))
			{
				failureReason = "PASSENGER_EXACT_RETRY_UNAVAILABLE";
				if (runningStalled)
					failureReason = "PASSENGER_EXACT_CARGO_STALL_RETRY_UNAVAILABLE";
				return false;
			}
			AICF_Stage3Diagnostics.Warning(
				"PASSENGER_BOARDING_REISSUED",
				DescribeTokenContext(trip, lease, retryToken, causationId, "EXACT_CARGO_REISSUED"));
			// Avoid immediately restarting several actions that may contend for the
			// same vehicle entry. The next scheduler tick may supervise the next one.
			if (runningStalled)
				return true;
		}

		array<AIAgent> agents = {};
		group.GetAgents(agents);
		foreach (AIAgent currentAgent : agents)
		{
			if (!currentAgent || currentAgent.GetParentGroup() != group)
				continue;
			IEntity currentEntity = currentAgent.GetControlledEntity();
			if (!AICF_GroupRuntime.IsAliveCharacter(currentEntity))
				continue;
			if (m_Watchdog.IsMemberSettledInVehicle(currentEntity, lease.GetVehicle()))
			{
				if (!IsSupportedSettledCompartment(lease, currentEntity))
				{
					failureReason = "PASSENGER_SETTLED_COMPARTMENT_UNSUPPORTED";
					return false;
				}
				continue;
			}
			if (tokens.FindByAgent(currentAgent))
				continue;
			failureReason = "PASSENGER_ACTION_TOKEN_MISSING";
			return false;
		}
		return true;
	}

	protected bool ReissueExactCargo(
		AICF_TransportTrip trip,
		AICF_VehicleLease lease,
		AICF_VehicleBoardingState state,
		string causationId,
		AIAgent agent,
		BaseCompartmentSlot compartment,
		int retryCount,
		out AICF_VehicleBoardingActionToken issuedToken)
	{
		issuedToken = null;
		if (!agent || !IsLeaseLiveIdentity(lease) || !compartment ||
			!CargoCompartmentSlot.Cast(compartment) ||
			compartment.GetVehicle() != lease.GetVehicle())
			return false;
		IEntity entity = agent.GetControlledEntity();
		if (!AICF_VehicleBoardingMutationFence.IsAuthoritativeAIEntity(entity) ||
			!AICF_VehicleBoardingMutationFence.IsAuthoritativeReplicatedEntity(lease.GetVehicle()))
			return false;
		CompartmentAccessComponent access = ResolveAccess(entity);
		if (!access || access.IsInCompartment() || CompartmentAccessComponent.GetVehicleIn(entity) ||
			access.IsGettingIn() || access.IsGettingOut())
		{
			return false;
		}
		SCR_AIUtilityComponent utility = ResolveOwnedUtility(agent, entity);
		if (!utility || !compartment.IsCompartmentAccessible() || compartment.GetOccupant() ||
			(compartment.IsReserved() && !compartment.IsReservedBy(entity)) ||
			compartment.IsGetInLockedFor(entity))
		{
			return false;
		}
		compartment.SetReserved(entity);
		if (!compartment.IsReservedBy(entity))
			return false;
		SCR_AIGetInVehicle action = new SCR_AIGetInVehicle(
			utility,
			null,
			lease.GetVehicle(),
			compartment,
			EAICompartmentType.Cargo,
			SCR_AIActionBase.PRIORITY_BEHAVIOR_GET_IN_VEHICLE,
			SCR_AIActionBase.PRIORITY_LEVEL_PLAYER);
		AICF_VehicleAsyncFence fence = CreateFence(
			trip, lease, state, causationId, "CARGO_RETRY", agent);
		issuedToken = new AICF_VehicleBoardingActionToken(
			fence,
			agent,
			entity,
			lease.GetVehicle(),
			null,
			action,
			compartment,
			EAICompartmentType.Cargo,
			retryCount,
			vector.DistanceXZ(entity.GetOrigin(), lease.GetVehicle().GetOrigin()));
		if (!TrackToken(state, issuedToken))
		{
			issuedToken.ReleaseReservationOwnerSafe();
			issuedToken = null;
			return false;
		}
		utility.AddAction(action);
		ReportPassengerAction(
			"PASSENGER_ACTION_TRANSITION",
			trip, lease, issuedToken, causationId, "EXACT_CARGO_RETRY_ISSUED", true);
		return true;
	}

	protected bool FindAvailableCargoCompartment(
		Vehicle vehicle,
		IEntity entity,
		array<BaseCompartmentSlot> excluded,
		out BaseCompartmentSlot selected)
	{
		selected = null;
		if (!vehicle || !entity)
			return false;
		BaseCompartmentManagerComponent manager = BaseCompartmentManagerComponent.Cast(
			vehicle.FindComponent(BaseCompartmentManagerComponent));
		if (!manager)
			return false;
		array<BaseCompartmentSlot> compartments = {};
		manager.GetCompartments(compartments);
		foreach (BaseCompartmentSlot compartment : compartments)
		{
			CargoCompartmentSlot cargo = CargoCompartmentSlot.Cast(compartment);
			if (!cargo || excluded.Contains(compartment) ||
				!compartment.IsCompartmentAccessible() || compartment.GetOccupant() ||
				compartment.IsReserved() || compartment.IsGetInLockedFor(entity))
			{
				continue;
			}
			selected = compartment;
			return true;
		}
		return false;
	}

	protected void RollbackReservations(
		array<BaseCompartmentSlot> compartments,
		array<IEntity> entities)
	{
		for (int index = 0; index < compartments.Count() && index < entities.Count(); index++)
		{
			if (compartments[index] && entities[index] &&
				compartments[index].IsReservedBy(entities[index]))
			{
				compartments[index].SetReserved(null);
			}
		}
	}

	protected bool IsSupportedSettledCompartment(
		AICF_VehicleLease lease,
		IEntity entity)
	{
		if (!lease || !m_Watchdog.IsMemberSettledInVehicle(entity, lease.GetVehicle()))
			return false;
		CompartmentAccessComponent access = ResolveAccess(entity);
		BaseCompartmentSlot compartment;
		if (access)
			compartment = access.GetCompartment();
		if (!compartment)
			return false;
		if (PilotCompartmentSlot.Cast(compartment) || CargoCompartmentSlot.Cast(compartment))
			return true;
		return lease.GetKind() == AICF_EVehicleKind.ARMED_LIGHT &&
			TurretCompartmentSlot.Cast(compartment);
	}

	protected SCR_AIUtilityComponent ResolveOwnedUtility(AIAgent agent, IEntity entity)
	{
		if (!agent || !AICF_GroupRuntime.IsAliveCharacter(entity))
			return null;
		SCR_AIUtilityComponent utility = SCR_AIUtilityComponent.Cast(
			agent.FindComponent(SCR_AIUtilityComponent));
		if (!utility || utility.m_OwnerEntity != entity)
			return null;
		return utility;
	}

	protected CompartmentAccessComponent ResolveAccess(IEntity entity)
	{
		ChimeraCharacter character = ChimeraCharacter.Cast(entity);
		if (!character)
			return null;
		return character.GetCompartmentAccessComponent();
	}

	protected AICF_VehicleAsyncFence CreateFence(
		AICF_TransportTrip trip,
		AICF_VehicleLease lease,
		AICF_VehicleBoardingState state,
		string causationId,
		string actionKind,
		AIAgent agent)
	{
		string agentId = "NONE";
		if (agent && agent.GetControlledEntity())
			agentId = agent.GetControlledEntity().GetID().ToString();
		int sequence = state.GetTokens().NextActionSequence();
		string token = trip.GetOperationId() + "/BOARDING/" + actionKind;
		token += string.Format("/%1/%2", agentId, sequence);
		return new AICF_VehicleAsyncFence(
			trip.GetFactionKey(),
			trip.GetSlotId(),
			trip.GetGroupGeneration(),
			trip.GetTripGeneration(),
			lease.GetLeaseGeneration(),
			lease.GetVehicleGeneration(),
			lease.GetEntityId(),
			lease.GetRplId(),
			token);
	}

	protected bool TrackToken(
		AICF_VehicleBoardingState state,
		AICF_VehicleBoardingActionToken token)
	{
		if (!state || !state.GetTokens() || !token || !token.GetFence())
			return false;
		if (!state.TrackActionFence(token.GetFence(), MAX_ACTION_TOKENS))
			return false;
		if (!state.GetTokens().Track(token, MAX_ACTION_TOKENS))
		{
			state.RemoveActionFence(token.GetFence());
			return false;
		}
		return true;
	}

	protected void ReportPhaseStarted(
		AICF_TransportTrip trip,
		AICF_VehicleLease lease,
		AICF_VehicleBoardingState state,
		string causationId,
		string allowance)
	{
		string details = DescribeContext(trip, lease, causationId, "ROLE_SEAT_RESERVATION");
		details += string.Format(
			" phase=%1 phase_index=%2 planned_phases=%3 allowance=%4 phase_deadline_ms=%5 total_deadline_ms=%6",
			typename.EnumToString(AICF_EVehicleBoardingPhase, state.GetPhase()),
			state.GetCurrentPhaseIndex(),
			state.GetPlannedPhaseCount(),
			allowance,
			state.GetPhaseDeadlineMs(),
			state.GetAbsoluteDeadlineMs());
		AICF_Stage3Diagnostics.Info("BOARDING_PHASE_STARTED", details);
	}

	protected void ReportProgress(
		AICF_TransportTrip trip,
		AICF_VehicleLease lease,
		AICF_VehicleBoardingState state,
		string causationId,
		int aliveCount,
		int linkedCount,
		int compartmentCount,
		int gettingInCount,
		int gettingOutCount,
		int characterVehicleCount,
		int settledCount,
		float nearestDistanceMeters,
		float farthestDistanceMeters,
		string memberSamples)
	{
		string details = DescribeContext(trip, lease, causationId, "PHYSICAL_PROGRESS_ADVANCED");
		details += string.Format(
			" phase=%1 alive=%2 linked=%3 compartment=%4 getting_in=%5 getting_out=%6",
			typename.EnumToString(AICF_EVehicleBoardingPhase, state.GetPhase()),
			aliveCount,
			linkedCount,
			compartmentCount,
			gettingInCount,
			gettingOutCount);
		details += string.Format(
			" character_vehicle=%1 settled=%2 nearest_m=%3 farthest_m=%4 best_farthest_m=%5 members=[%6]",
			characterVehicleCount,
			settledCount,
			nearestDistanceMeters,
			farthestDistanceMeters,
			state.GetBestFarthestDistanceMeters(),
			memberSamples);
		AICF_Stage3Diagnostics.Info("BOARDING_PROGRESS", details);
	}

	protected void ReportActionChanges(
		AICF_TransportTrip trip,
		AICF_VehicleLease lease,
		AICF_VehicleBoardingState state,
		string causationId)
	{
		AICF_VehicleBoardingTokenSet tokens = state.GetTokens();
		for (int index = 0; index < tokens.Count(); index++)
		{
			AICF_VehicleBoardingActionToken token = tokens.Get(index);
			if (!token || !token.GetGetInAction())
				continue;
			bool linked;
			bool gettingIn;
			bool gettingOut;
			EAIActionState actionState;
			if (!token.ObserveTransitionChange(
				lease.GetVehicle(), linked, gettingIn, gettingOut, actionState))
			{
				continue;
			}
			ReportPassengerAction(
				"PASSENGER_ACTION_TRANSITION",
				trip, lease, token, causationId, "ACTION_STATE_CHANGED", true);
		}
	}

	protected void ReportOwnershipAudit(
		AICF_TransportTrip trip,
		AICF_VehicleLease lease,
		AICF_VehicleBoardingState state,
		string causationId,
		string memberSamples)
	{
		string details = DescribeContext(trip, lease, causationId, "RATE_LIMITED_ACTION_OWNERSHIP");
		details += string.Format(
			" phase=%1 token_count=%2 fence_count=%3 members=[%4]",
			typename.EnumToString(AICF_EVehicleBoardingPhase, state.GetPhase()),
			state.GetTokens().Count(),
			state.GetActionFenceCount(),
			memberSamples);
		AICF_Stage3Diagnostics.Info("BOARDING_ACTION_OWNERSHIP", details);
	}

	protected void ReportPassengerAction(
		string eventName,
		AICF_TransportTrip trip,
		AICF_VehicleLease lease,
		AICF_VehicleBoardingActionToken token,
		string causationId,
		string reason,
		bool accepted)
	{
		string details = DescribeTokenContext(trip, lease, token, causationId, reason);
		details += string.Format(" accepted=%1", accepted);
		if (eventName == "PASSENGER_ACTION_FAILURE")
			AICF_Stage3Diagnostics.Warning(eventName, details);
		else
			AICF_Stage3Diagnostics.Info(eventName, details);
	}

	protected string DescribeTokenContext(
		AICF_TransportTrip trip,
		AICF_VehicleLease lease,
		AICF_VehicleBoardingActionToken token,
		string causationId,
		string reason)
	{
		string details = DescribeContext(trip, lease, causationId, reason);
		if (!token)
			return details + " action_token=NONE";
		return details + " " + DescribeTokenState(token, lease);
	}

	protected string DescribeExactCargoTokens(
		AICF_VehicleBoardingState state,
		AICF_VehicleLease lease)
	{
		if (!state)
			return "NONE";
		string samples;
		AICF_VehicleBoardingTokenSet tokens = state.GetTokens();
		for (int index = 0; index < tokens.Count(); index++)
		{
			AICF_VehicleBoardingActionToken token = tokens.Get(index);
			if (!token || !token.GetGetInAction() ||
				token.GetCompartmentType() != EAICompartmentType.Cargo)
			{
				continue;
			}
			if (!samples.IsEmpty())
				samples += ",";
			samples += DescribeTokenState(token, lease);
		}
		if (samples.IsEmpty())
			return "NONE";
		return samples;
	}

	protected string DescribeTokenState(
		AICF_VehicleBoardingActionToken token,
		AICF_VehicleLease lease)
	{
		if (!token)
			return "action_token=NONE";
		string details;
		string agentId = "NONE";
		if (token.GetReservedEntity())
			agentId = token.GetReservedEntity().GetID().ToString();
		CompartmentAccessComponent access = ResolveAccess(token.GetReservedEntity());
		bool linked = token.GetReservedEntity() && lease && lease.GetVehicle() &&
			CompartmentAccessComponent.GetVehicleIn(token.GetReservedEntity()) == lease.GetVehicle();
		bool gettingIn;
		bool gettingOut;
		if (access)
		{
			gettingIn = access.IsGettingIn();
			gettingOut = access.IsGettingOut();
		}
		bool reserved;
		bool accessible;
		bool getInLocked;
		bool getInLockedFor;
		bool occupied;
		string occupantId = "NONE";
		int availableDoors = -1;
		BaseCompartmentSlot compartment = token.GetCompartment();
		if (compartment)
		{
			accessible = compartment.IsCompartmentAccessible();
			getInLocked = compartment.IsGetInLocked();
			IEntity occupant = compartment.GetOccupant();
			occupied = occupant != null;
			if (occupant)
				occupantId = occupant.GetID().ToString();
			array<int> doorIndices = {};
			availableDoors = compartment.GetAvailableDoorIndices(doorIndices);
			if (token.GetReservedEntity())
			{
				reserved = compartment.IsReservedBy(token.GetReservedEntity());
				getInLockedFor = compartment.IsGetInLockedFor(token.GetReservedEntity());
			}
		}
		details += string.Format(
			"agent=%1 action_token=%2 retry=%3 transition_age_ms=%4 owner_valid=%5 physical_owner_safe=%6 action_current=%7 action_owned=%8 exact_compartment_vehicle=%9",
			agentId,
			token.GetActionToken(),
			token.GetRetryCount(),
			token.GetTransitionAgeMs(),
			token.IsOwnerValid(),
			token.IsPhysicalMutationOwnerSafe(),
			token.IsTrackedActionCurrent(),
			token.IsTrackedActionOwnedByUtility(),
			token.IsExactCompartmentTarget());
		details += string.Format(
			" exact_compartment_mutation_safe=%1 hidden_recovery_pending=%2 hidden_recovery_attempted=%3",
			token.IsExactCompartmentMutationSafe(),
			token.IsHiddenExactSeatRecoveryPending(),
			token.WasHiddenExactSeatRecoveryAttempted());
		details += string.Format(
			" exact_reserved_manager=%1 exact_reserved_slot=%2 actual_manager=%3 actual_slot=%4 reserved_by_owner=%5",
			token.GetAssignedManagerId(),
			token.GetAssignedSlotId(),
			token.GetActualManagerId(),
			token.GetActualSlotId(),
			reserved);
		details += string.Format(
			" current_distance_m=%1 best_distance_m=%2 progress_age_ms=%3 accessible=%4 get_in_locked=%5 get_in_locked_for=%6",
			token.GetCurrentDistanceMeters(),
			token.GetBestDistanceMeters(),
			token.GetProgressAgeMs(),
			accessible,
			getInLocked,
			getInLockedFor);
		details += string.Format(
			" occupied=%1 occupant=%2 available_doors=%3 linked=%4 getting_in=%5 getting_out=%6 action_state=%7 compartment_type=%8",
			occupied,
			occupantId,
			availableDoors,
			linked,
			gettingIn,
			gettingOut,
			typename.EnumToString(EAIActionState, token.GetActionState()),
			typename.EnumToString(EAICompartmentType, token.GetCompartmentType()));
		return details;
	}

	protected string DescribeContext(
		AICF_TransportTrip trip,
		AICF_VehicleLease lease,
		string causationId,
		string reason)
	{
		if (!trip)
			return string.Format(
				"reason=%1 trip=NONE causation_id=%2 vehicle=NONE entity_id=NONE kind=NONE state=NONE prefab=NONE rpl_id=NONE",
				reason,
				causationId);
		string details = string.Format(
			"faction=%1 slot=%2 numeric_slot=%3 group_generation=%4 trip_generation=%5",
			trip.GetFactionKey(),
			trip.GetSlotKey(),
			trip.GetSlotId(),
			trip.GetGroupGeneration(),
			trip.GetTripGeneration());
		details += string.Format(
			" operation_id=%1 causation_id=%2 reason=%3",
			trip.GetOperationId(),
			causationId,
			reason);
		string tripState = typename.EnumToString(AICF_ETransportTripPhase, trip.GetPhase());
		if (!lease)
			return details + string.Format(
				" lease=NONE vehicle=NONE entity_id=NONE kind=NONE state=%1 prefab=NONE rpl_id=NONE",
				tripState);
		string entityId = lease.GetEntityIdString();
		if (entityId.IsEmpty())
			entityId = "NONE";
		string prefab = lease.GetPrefab();
		if (prefab.IsEmpty())
			prefab = "NONE";
		details += string.Format(
			" lease_generation=%1 vehicle_generation=%2 vehicle_lifecycle_id=%3 entity_id=%4 rpl_id=%5",
			lease.GetLeaseGeneration(),
			lease.GetVehicleGeneration(),
			lease.GetVehicleLifecycleId(),
			lease.GetEntityIdString(),
			lease.GetRplId());
		details += string.Format(
			" vehicle=%1 kind=%2 state=%3 prefab=%4",
			entityId,
			AICF_Stage3Diagnostics.KindToString(lease.GetKind()),
			tripState,
			prefab);
		return details;
	}
}
