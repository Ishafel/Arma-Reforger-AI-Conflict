// Owns normal GetOut, physical clearance guidance and the isolated bounded
// terminal exact-clearance tools. It never restores orders, releases a lease,
// transitions a Trip or calls another flow.
class AICF_VehicleDismountFlow
{
	protected static const int CONTINUOUS_CLEAR_MS = 5000;
	protected static const int GUIDANCE_DELAY_MS = 3000;
	protected static const int GUIDANCE_MAX_ATTEMPTS = 3;
	protected static const int FORCE_MAX_ATTEMPTS = 3;
	protected static const int HIDDEN_RECOVERY_AUDIT_INTERVAL_MS = 5000;
	protected static const float CLEARANCE_MARGIN_METERS = 1.5;
	protected ref AICF_Stage3Config m_Config;
	protected ref AICF_VehicleWatchdog m_Watchdog;
	protected ref AICF_VehicleWaypointFactory m_WaypointFactory;

	void AICF_VehicleDismountFlow(
		AICF_Stage3Config config,
		AICF_VehicleWatchdog watchdog,
		AICF_VehicleWaypointFactory waypointFactory)
	{
		m_Config = config;
		m_Watchdog = watchdog;
		m_WaypointFactory = waypointFactory;
	}

	AICF_TripOutcome Begin(AICF_TransportTrip trip, string causationId)
	{
		if (!IsAuthoritativeTripAssetCurrent(trip))
			return AICF_TripOutcome.TerminalFailClosed("DISEMBARK_IDENTITY_INVALID", causationId);
		int nowMs = System.GetTickCount();
		int normalTimeoutMs = Math.Max(1000, m_Config.GetBoardingTimeoutMs());
		AICF_VehicleDismountState state = trip.GetDismountState();
		state.Begin(
			nowMs,
			nowMs + normalTimeoutMs,
			nowMs + normalTimeoutMs * 2,
			GUIDANCE_MAX_ATTEMPTS,
			FORCE_MAX_ATTEMPTS);
		int interrupted = m_Watchdog.ResetGroupVehicleActions(trip.GetAssignment().GetGroup());
		AIWaypoint rejectedWaypoint;
		if (!IssueNormalDismountWaypoint(trip, rejectedWaypoint))
		{
			return AICF_TripOutcome.FallbackToFootWithWaypoint(
				"DISEMBARK_WAYPOINT_FAILED",
				causationId,
				rejectedWaypoint);
		}
		AICF_Stage3Diagnostics.Info(
			"DISEMBARK_STARTED",
			FormatIdentity(trip, "NORMAL_GET_OUT") + string.Format(
				" interrupted_actions=%1 normal_deadline_ms=%2 terminal_deadline_ms=%3",
				interrupted,
				state.GetNormalDeadlineMs(),
				state.GetTerminalDeadlineMs()));
		return AICF_TripOutcome.StartDismount("DISEMBARK_WAYPOINT_BIND_REQUIRED", causationId);
	}

	AIWaypoint GetDismountWaypoint(AICF_TransportTrip trip)
	{
		if (!trip || !trip.GetDismountState())
			return null;
		return trip.GetDismountState().GetDismountWaypoint();
	}

	AIWaypoint GetSupersededDismountWaypoint(AICF_TransportTrip trip)
	{
		if (!trip || !trip.GetDismountState())
			return null;
		return trip.GetDismountState().GetSupersededDismountWaypoint();
	}

	bool ConfirmDismountWaypointBound(
		AICF_TransportTrip trip,
		AIWaypoint expected,
		bool boundToCurrentGroup)
	{
		if (!IsAuthoritativeTripAssetCurrent(trip))
			return false;
		return trip.GetDismountState().ConfirmDismountWaypointBound(
			expected,
			boundToCurrentGroup);
	}

	bool ConfirmSupersededWaypointRemoved(
		AICF_TransportTrip trip,
		AIWaypoint expected)
	{
		if (!trip || !trip.GetDismountState())
			return false;
		return trip.GetDismountState().ConfirmSupersededDismountWaypointRemoved(expected);
	}

	bool ConfirmDismountWaypointRemoved(
		AICF_TransportTrip trip,
		AIWaypoint expected)
	{
		if (!trip || !trip.GetDismountState())
			return false;
		return trip.GetDismountState().ConfirmDismountWaypointRemoved(expected);
	}

	AICF_TripOutcome ProcessNormalDismount(AICF_TransportTrip trip, string causationId)
	{
		if (!IsAuthoritativeTripAssetCurrent(trip))
			return AICF_TripOutcome.TerminalFailClosed("DISEMBARK_IDENTITY_INVALID", causationId);
		AICF_VehicleDismountState state = trip.GetDismountState();
		int nowMs = System.GetTickCount();
		if (state.GetSupersededDismountWaypoint() ||
			(state.GetDismountWaypoint() && !state.IsDismountWaypointBound()))
		{
			return AICF_TripOutcome.StartDismount(
				"DISEMBARK_WAYPOINT_RECONCILIATION_REQUIRED",
				causationId);
		}
		AICF_DismountClearanceSample sample = InspectCurrentManagedClearance(trip);
		if (!sample)
			return AICF_TripOutcome.TerminalFailClosed("DISEMBARK_SAMPLE_INVALID", causationId);
		int clearPolls = state.RecordClearanceSample(
			sample.m_iLogicalOccupants,
			sample.m_iTransitions,
			sample.m_iInsideBounds,
			nowMs);
		int continuousClearMs = state.GetContinuousClearMs(nowMs);
		MaintainGuidanceTokens(trip);
		if (sample.m_bSafelyClear && continuousClearMs >= CONTINUOUS_CLEAR_MS)
		{
			CancelGuidanceTokens(trip);
			AICF_Stage3Diagnostics.Info(
				"DISEMBARK_COMPLETE",
				FormatIdentity(trip, "ALL_PROTECTED_MEMBERS_SAFELY_CLEAR") + string.Format(
					" clear_polls=%1 continuous_clear_ms=%2 required_clear_ms=%3",
					clearPolls,
					continuousClearMs,
					CONTINUOUS_CLEAR_MS));
			return AICF_TripOutcome.CompleteTrip("DISEMBARK_COMPLETE", causationId);
		}

		bool physicalOnlyBlocked = sample.m_iLogicalOccupants == 0 &&
			sample.m_iTransitions == 0 && sample.m_iInsideBounds > 0;
		if (physicalOnlyBlocked && nowMs - state.GetStartedAtMs() >= GUIDANCE_DELAY_MS)
			TryGuidePhysicallyBlockedMembers(trip, sample);

		int normalDurationMs = state.GetNormalDeadlineMs() - state.GetStartedAtMs();
		if (!state.WasNormalReissueAttempted() &&
			nowMs - state.GetStartedAtMs() >= normalDurationMs / 2 &&
			(sample.m_iLogicalOccupants > 0 || sample.m_iTransitions > 0))
		{
			state.MarkNormalReissueAttempted();
			m_Watchdog.ResetGroupVehicleActions(trip.GetAssignment().GetGroup());
			AIWaypoint rejectedWaypoint;
			if (!IssueNormalDismountWaypoint(trip, rejectedWaypoint))
			{
				return AICF_TripOutcome.FallbackToFootWithWaypoint(
					"DISEMBARK_REISSUE_WAYPOINT_FAILED",
					causationId,
					rejectedWaypoint);
			}
			AICF_Stage3Diagnostics.Warning(
				"DISEMBARK_REISSUED",
				FormatClearance(trip, sample, "HALF_DEADLINE_REISSUE"));
			return AICF_TripOutcome.StartDismount(
				"DISEMBARK_REISSUE_RECONCILIATION_REQUIRED",
				causationId);
		}

		if (nowMs >= state.GetNormalDeadlineMs())
		{
			AICF_Stage3Diagnostics.Warning(
				"DISEMBARK_TIMEOUT",
				FormatClearance(trip, sample, "NORMAL_DEADLINE_REACHED"));
			return AICF_TripOutcome.FallbackToFoot("DISEMBARK_TIMEOUT", causationId);
		}
		return AICF_TripOutcome.Wait("DISEMBARK_PENDING", causationId);
	}

	AICF_TripOutcome BeginTerminalClearance(AICF_TransportTrip trip, string causationId)
	{
		if (trip && trip.GetDismountState() &&
			trip.GetDismountState().IsTerminalClearanceStopped())
		{
			return AICF_TripOutcome.Wait("TERMINAL_CLEARANCE_STOPPED_FAIL_CLOSED", causationId);
		}
		if (!IsAuthoritativeTripAssetCurrent(trip))
		{
			if (trip && trip.GetDismountState())
				trip.GetDismountState().StopTerminalClearance();
			return AICF_TripOutcome.TerminalFailClosed("TERMINAL_CLEARANCE_IDENTITY_INVALID", causationId);
		}
		int nowMs = System.GetTickCount();
		int terminalTimeoutMs = Math.Max(1000, m_Config.GetBoardingTimeoutMs());
		CancelGuidanceTokens(trip);
		SCR_AIGroup group = trip.GetAssignment().GetGroup();
		int interrupted = m_Watchdog.ResetGroupVehicleActions(group);
		trip.GetDismountState().BeginTerminal(
			nowMs,
			nowMs + terminalTimeoutMs,
			FORCE_MAX_ATTEMPTS);
		AICF_Stage3Diagnostics.Info(
			"TERMINAL_CLEARANCE_STARTED",
			FormatIdentity(trip, "TERMINAL_ACTION_QUEUES_RESET") + string.Format(
				" interrupted_actions=%1 terminal_deadline_ms=%2 continuous_clear_required_ms=%3",
				interrupted,
				trip.GetDismountState().GetTerminalDeadlineMs(),
				CONTINUOUS_CLEAR_MS));
		return AICF_TripOutcome.Wait("TERMINAL_CLEARANCE_STARTED", causationId);
	}

	// Called by the controller after vehicle control has already ended and order
	// restore has been requested. It can only advance physical clearance.
	AICF_TripOutcome ProcessTerminalClearance(AICF_TransportTrip trip, string causationId)
	{
		if (!IsAuthoritativeTripAssetCurrent(trip))
		{
			if (trip && trip.GetDismountState())
				trip.GetDismountState().StopTerminalClearance();
			return AICF_TripOutcome.TerminalFailClosed("TERMINAL_CLEARANCE_IDENTITY_INVALID", causationId);
		}
		AICF_VehicleDismountState state = trip.GetDismountState();
		if (state.IsTerminalClearanceStopped())
			return AICF_TripOutcome.Wait("TERMINAL_CLEARANCE_STOPPED_FAIL_CLOSED", causationId);
		int nowMs = System.GetTickCount();
		AICF_DismountClearanceSample sample = InspectCurrentManagedClearance(trip);
		if (!sample)
			return AICF_TripOutcome.TerminalFailClosed("TERMINAL_CLEARANCE_SAMPLE_INVALID", causationId);
		int clearPolls = state.RecordClearanceSample(
			sample.m_iLogicalOccupants,
			sample.m_iTransitions,
			sample.m_iInsideBounds,
			nowMs);
		int continuousClearMs = state.GetContinuousClearMs(nowMs);
		if (sample.m_bSafelyClear && continuousClearMs >= CONTINUOUS_CLEAR_MS)
		{
			CancelGuidanceTokens(trip);
			AICF_Stage3Diagnostics.Info(
				"TERMINAL_CLEARANCE_SAFE",
				FormatClearance(trip, sample, "CONTINUOUS_CLEAR_PROVEN") + string.Format(
					" clear_polls=%1 continuous_clear_ms=%2 required_clear_ms=%3",
					clearPolls,
					continuousClearMs,
					CONTINUOUS_CLEAR_MS));
			return AICF_TripOutcome.ReleaseLease("TERMINAL_CLEARANCE_SAFE", causationId);
		}

		if (nowMs >= state.GetTerminalDeadlineMs())
		{
			state.StopTerminalClearance();
			AICF_Stage3Diagnostics.Error(
				"FALLBACK_DISEMBARK_FAILED",
				FormatClearance(trip, sample, "TERMINAL_DEADLINE_REACHED"));
			// RELEASE_LEASE is only a request to transfer the still-uncleared asset
			// to CleanupManager. Its independent protected-clearance scan remains
			// the sole authority allowed to prove clear and release/delete it.
			return AICF_TripOutcome.ReleaseLease(
				"FALLBACK_DISEMBARK_FAILED",
				causationId);
		}

		if (sample.m_iLogicalOccupants > 0 || sample.m_iTransitions > 0)
			TryForceExactManagedExit(trip, sample);
		else if (sample.m_iInsideBounds > 0)
			TryRelocateExactManagedMembers(trip, sample);
		return AICF_TripOutcome.Wait("TERMINAL_CLEARANCE_PENDING", causationId);
	}

	void Exit(AICF_TransportTrip trip, string reason)
	{
		if (!trip)
			return;
		CancelGuidanceTokens(trip);
	}

	protected bool IssueNormalDismountWaypoint(
		AICF_TransportTrip trip,
		out AIWaypoint rejectedWaypoint)
	{
		rejectedWaypoint = null;
		Vehicle vehicle = trip.GetLease().GetVehicle();
		SCR_BoardingWaypoint waypoint = m_WaypointFactory.CreateDismountWaypoint(vehicle);
		if (!waypoint)
			return false;
		if (trip.GetDismountState().StageDismountWaypoint(waypoint))
			return true;
		rejectedWaypoint = waypoint;
		return false;
	}

	protected AICF_DismountClearanceSample InspectCurrentManagedClearance(AICF_TransportTrip trip)
	{
		AICF_DismountClearanceSample sample = new AICF_DismountClearanceSample();
		sample.m_bSafelyClear = m_Watchdog.InspectProtectedMemberDismountClearance(
			trip.GetAssignment().GetGroup(),
			trip.GetLease().GetVehicle(),
			sample.m_iLogicalOccupants,
			sample.m_iTransitions,
			sample.m_iInsideBounds,
			sample.m_sMemberSamples);
		return sample;
	}

	protected void TryGuidePhysicallyBlockedMembers(
		AICF_TransportTrip trip,
		AICF_DismountClearanceSample sample)
	{
		AICF_VehicleDismountState state = trip.GetDismountState();
		if (!state.RecordGuidanceAttempt())
			return;
		int issued;
		int alreadyActive;
		int searched;
		IssueGuidanceForMembers(trip, issued, alreadyActive, searched);
		AICF_Stage3Diagnostics.Info(
			"DISEMBARK_CLEARANCE_GUIDANCE",
			FormatClearance(trip, sample, "LOGICALLY_OUT_INSIDE_BOUNDS") + string.Format(
				" newly_issued=%1 already_active=%2 search_attempts=%3 attempt=%4 maximum_attempts=%5",
				issued,
				alreadyActive,
				searched,
				state.GetGuidanceAttempts(),
				GUIDANCE_MAX_ATTEMPTS));
	}

	protected void IssueGuidanceForMembers(
		AICF_TransportTrip trip,
		out int issued,
		out int alreadyActive,
		out int searched)
	{
		issued = 0;
		alreadyActive = 0;
		searched = 0;
		SCR_AIGroup group = trip.GetAssignment().GetGroup();
		Vehicle vehicle = trip.GetLease().GetVehicle();
		vector boundsMin;
		vector boundsMax;
		vehicle.GetBounds(boundsMin, boundsMax);
		float clearanceRadius = ResolveClearanceRadius(boundsMin, boundsMax, 3.0);
		array<AIAgent> agents = {};
		group.GetAgents(agents);
		int directionIndex;
		foreach (AIAgent agent : agents)
		{
			if (!IsExactAliveCurrentMember(group, agent))
				continue;
			ChimeraCharacter character = ChimeraCharacter.Cast(agent.GetControlledEntity());
			if (!IsPhysicalOnlyBlocker(character, vehicle, boundsMin, boundsMax))
			{
				CancelGuidanceToken(trip, trip.GetDismountState().FindGuidanceToken(agent));
				continue;
			}
			AICF_VehicleDismountActionToken existing = trip.GetDismountState().FindGuidanceToken(agent);
			if (IsGuidanceActionActive(existing, trip))
			{
				alreadyActive++;
				continue;
			}
			CancelGuidanceToken(trip, existing);
			vector safePosition;
			searched++;
			if (!FindGuidancePosition(vehicle, character, boundsMin, boundsMax,
				clearanceRadius, directionIndex, safePosition))
				continue;
			directionIndex++;
			if (IssueGuidanceAction(trip, agent, character, safePosition))
				issued++;
		}
	}

	protected bool IssueGuidanceAction(
		AICF_TransportTrip trip,
		AIAgent agent,
		IEntity entity,
		vector safePosition)
	{
		if (!AICF_VehicleBoardingMutationFence.IsAuthoritativeAIEntity(entity))
			return false;
		SCR_AIUtilityComponent utility = SCR_AIUtilityComponent.Cast(
			agent.FindComponent(SCR_AIUtilityComponent));
		if (!utility || utility.m_OwnerEntity != entity)
			return false;
		SCR_AIMoveIndividuallyBehavior action = new SCR_AIMoveIndividuallyBehavior(
			utility,
			null,
			safePosition,
			SCR_AIActionBase.PRIORITY_BEHAVIOR_MOVE_INDIVIDUALLY,
			SCR_AIActionBase.PRIORITY_LEVEL_PLAYER,
			null,
			1.5);
		string tokenId = string.Format(
			"%1-dismount-%2-%3",
			trip.GetOperationId(),
			entity.GetID(),
			trip.GetDismountState().GetGuidanceAttempts());
		AICF_VehicleAsyncFence fence = CreateFence(trip, tokenId);
		if (!fence)
			return false;
		utility.AddAction(action);
		trip.GetDismountState().TrackGuidanceToken(new AICF_VehicleDismountActionToken(
			agent,
			entity,
			action,
			fence));
		return true;
	}

	protected void MaintainGuidanceTokens(AICF_TransportTrip trip)
	{
		AICF_VehicleDismountState state = trip.GetDismountState();
		for (int index = state.GetGuidanceTokenCount() - 1; index >= 0; index--)
		{
			AICF_VehicleDismountActionToken token = state.GetGuidanceToken(index);
			if (!IsGuidanceActionActive(token, trip))
				CancelGuidanceToken(trip, token);
		}
	}

	protected bool IsGuidanceActionActive(
		AICF_VehicleDismountActionToken token,
		AICF_TransportTrip trip)
	{
		if (!token || !token.GetFence() || !token.GetFence().MatchesTrip(trip) ||
			!token.GetFence().MatchesLease(trip.GetLease()))
			return false;
		AIAgent agent = token.GetAgent();
		if (!agent || agent.GetControlledEntity() != token.GetReservedEntity() ||
			agent.GetParentGroup() != trip.GetAssignment().GetGroup())
			return false;
		SCR_AIMoveIndividuallyBehavior action = token.GetAction();
		if (!action)
			return false;
		EAIActionState actionState = action.GetActionState();
		return actionState != EAIActionState.COMPLETED && actionState != EAIActionState.FAILED;
	}

	protected void CancelGuidanceTokens(AICF_TransportTrip trip)
	{
		if (!trip)
			return;
		AICF_VehicleDismountState state = trip.GetDismountState();
		for (int index = state.GetGuidanceTokenCount() - 1; index >= 0; index--)
			CancelGuidanceToken(trip, state.GetGuidanceToken(index));
	}

	protected void CancelGuidanceToken(
		AICF_TransportTrip trip,
		AICF_VehicleDismountActionToken token)
	{
		if (!trip || !token)
			return;
		AIAgent agent = token.GetAgent();
		IEntity entity = token.GetReservedEntity();
		SCR_AIUtilityComponent utility;
		if (agent)
			utility = SCR_AIUtilityComponent.Cast(agent.FindComponent(SCR_AIUtilityComponent));
		SCR_AIMoveIndividuallyBehavior action = token.GetAction();
		if (action && entity && utility && utility.m_OwnerEntity == entity &&
			AICF_VehicleBoardingMutationFence.IsAuthoritativeAIEntity(entity))
		{
			EAIActionState actionState = action.GetActionState();
			if (actionState != EAIActionState.COMPLETED && actionState != EAIActionState.FAILED)
				action.Fail();
		}
		trip.GetDismountState().RemoveGuidanceToken(token);
	}

	protected void TryForceExactManagedExit(
		AICF_TransportTrip trip,
		AICF_DismountClearanceSample sample)
	{
		AICF_VehicleDismountState state = trip.GetDismountState();
		Vehicle vehicle = trip.GetLease().GetVehicle();
		float nearestPlayerMeters;
		string rejectionReason;
		if (!CanApplyTerminalHiddenRecovery(
			vehicle.GetOrigin(),
			vehicle.GetOrigin(),
			nearestPlayerMeters,
			rejectionReason))
		{
			AuditHiddenRecoveryDeferred(
				trip,
				sample,
				rejectionReason,
				nearestPlayerMeters);
			return;
		}
		if (!state.RecordForceClearanceAttempt())
			return;
		SCR_AIGroup group = trip.GetAssignment().GetGroup();
		int interrupted = m_Watchdog.ResetGroupVehicleActions(group);
		array<AIAgent> agents = {};
		group.GetAgents(agents);
		int forced;
		foreach (AIAgent agent : agents)
		{
			if (!IsExactAliveCurrentMember(group, agent))
				continue;
			ChimeraCharacter character = ChimeraCharacter.Cast(agent.GetControlledEntity());
			if (CompartmentAccessComponent.GetVehicleIn(character) != vehicle)
				continue;
			if (ForceOneManagedMemberOut(trip, character, state.GetForceClearanceAttempts()))
				forced++;
		}
		AICF_Stage3Diagnostics.Warning(
			"FALLBACK_FORCE_DISEMBARK",
			FormatClearance(trip, sample, "BOUNDED_TERMINAL_FORCE") + string.Format(
				" forced=%1 interrupted_actions=%2 attempt=%3 maximum_attempts=%4",
				forced,
				interrupted,
				state.GetForceClearanceAttempts(),
				FORCE_MAX_ATTEMPTS));
	}

	protected bool ForceOneManagedMemberOut(
		AICF_TransportTrip trip,
		ChimeraCharacter character,
		int forceAttempt)
	{
		Vehicle vehicle = trip.GetLease().GetVehicle();
		if (!AICF_VehicleBoardingMutationFence.IsAuthoritativeAIEntity(character) ||
			!AICF_VehicleBoardingMutationFence.IsAuthoritativeReplicatedEntity(vehicle))
		{
			return false;
		}
		CompartmentAccessComponent access = character.GetCompartmentAccessComponent();
		if (!access)
			return false;
		BaseCompartmentSlot compartment = access.GetCompartment();
		bool exactOwner = compartment && compartment.GetVehicle() == vehicle &&
			compartment.GetOccupant() == character;
		access.InterruptVehicleActionQueue(true, true, true);
		bool useEject = exactOwner && forceAttempt > 1;
		bool directAccepted;
		vector exitTransform[4];
		if (!useEject && access.FindSuitableTeleportLocation(exitTransform))
			directAccepted = access.GetOutVehicle_NoDoor(exitTransform, false, false, true);
		if (!useEject && !directAccepted)
			directAccepted = access.GetOutVehicle(
				EGetOutType.TELEPORT,
				-1,
				ECloseDoorAfterActions.INVALID,
				false,
				true);
		bool ejectRequested;
		bool ejectImmediate;
		if (exactOwner && (useEject || !directAccepted))
			ejectRequested = compartment.EjectOccupant(true, false, ejectImmediate, false);
		bool linkedAfter = CompartmentAccessComponent.GetVehicleIn(character) == vehicle ||
			character.IsInVehicle() || access.IsInCompartment();
		int compartmentSlot = -1;
		int compartmentManager = -1;
		if (compartment)
		{
			compartmentSlot = compartment.GetCompartmentSlotID();
			compartmentManager = compartment.GetCompartmentMgrID();
		}
		string details = FormatIdentity(trip, "PROTECTED_MEMBER_FORCE_EXIT");
		details += string.Format(
			" member=%1 compartment_slot=%2 compartment_manager=%3 direct_accepted=%4 eject_requested=%5 eject_immediate=%6",
			character.GetID(),
			compartmentSlot,
			compartmentManager,
			directAccepted,
			ejectRequested,
			ejectImmediate);
		details += string.Format(
			" linked_after=%1 getting_in=%2 getting_out=%3 force_attempt=%4 maximum_attempts=%5 exact_owner_valid=%6 escalation=TERMINAL_ONLY immediate_result=%7",
			linkedAfter,
			access.IsGettingIn(),
			access.IsGettingOut(),
			forceAttempt,
			FORCE_MAX_ATTEMPTS,
			exactOwner,
			!linkedAfter);
		AICF_Stage35Diagnostics.Info("FORCE_DISEMBARK_MEMBER", details);
		return directAccepted || ejectRequested;
	}

	protected void TryRelocateExactManagedMembers(
		AICF_TransportTrip trip,
		AICF_DismountClearanceSample sample)
	{
		AICF_VehicleDismountState state = trip.GetDismountState();
		Vehicle vehicle = trip.GetLease().GetVehicle();
		float nearestPlayerMeters;
		string rejectionReason;
		if (!CanApplyTerminalHiddenRecovery(
			vehicle.GetOrigin(),
			vehicle.GetOrigin(),
			nearestPlayerMeters,
			rejectionReason))
		{
			AuditHiddenRecoveryDeferred(
				trip,
				sample,
				rejectionReason,
				nearestPlayerMeters);
			return;
		}
		if (!state.RecordForceClearanceAttempt())
			return;
		SCR_AIGroup group = trip.GetAssignment().GetGroup();
		int interrupted = m_Watchdog.ResetGroupVehicleActions(group);
		vector boundsMin;
		vector boundsMax;
		vehicle.GetBounds(boundsMin, boundsMax);
		float clearanceRadius = ResolveClearanceRadius(boundsMin, boundsMax, 2.0);
		array<AIAgent> agents = {};
		group.GetAgents(agents);
		int relocated;
		int playerFenced;
		int directionIndex;
		foreach (AIAgent agent : agents)
		{
			if (!IsExactAliveCurrentMember(group, agent))
				continue;
			ChimeraCharacter character = ChimeraCharacter.Cast(agent.GetControlledEntity());
			if (!IsPhysicalOnlyBlocker(character, vehicle, boundsMin, boundsMax))
				continue;
			CompartmentAccessComponent access = character.GetCompartmentAccessComponent();
			if (access)
				access.InterruptVehicleActionQueue(true, true, true);
			vector safePosition;
			if (!FindGuidancePosition(vehicle, character, boundsMin, boundsMax,
				clearanceRadius, directionIndex, safePosition))
				continue;
			directionIndex++;
			if (!CanApplyTerminalHiddenRecovery(
				character.GetOrigin(),
				safePosition,
				nearestPlayerMeters,
				rejectionReason))
			{
				playerFenced++;
				continue;
			}
			if (!AICF_VehicleBoardingMutationFence.IsAuthoritativeAIEntity(character))
				continue;
			vector transform[4];
			character.GetWorldTransform(transform);
			transform[3] = safePosition;
			character.Teleport(transform);
			relocated++;
		}
		AICF_Stage3Diagnostics.Warning(
			"DISEMBARK_CLEARANCE_RECOVERY",
			FormatClearance(trip, sample, "TERMINAL_EXACT_RELOCATION") + string.Format(
				" relocated=%1 player_fenced=%2 interrupted_actions=%3 attempt=%4 maximum_attempts=%5 terminal_only=1 hidden_radius_m=%6",
				relocated,
				playerFenced,
				interrupted,
				state.GetForceClearanceAttempts(),
				FORCE_MAX_ATTEMPTS,
				m_Config.GetHiddenRecoveryPlayerRadiusMeters()));
	}

	protected bool CanApplyTerminalHiddenRecovery(
		vector source,
		vector destination,
		out float nearestPlayerMeters,
		out string rejectionReason)
	{
		nearestPlayerMeters = -1.0;
		rejectionReason = string.Empty;
		if (!m_Config.GetHiddenRecoveryEnabled())
		{
			rejectionReason = "HIDDEN_RECOVERY_DISABLED";
			return false;
		}
		return m_Watchdog.CanApplyHiddenRecovery(
			source,
			destination,
			m_Config.GetHiddenRecoveryPlayerRadiusMeters(),
			nearestPlayerMeters,
			rejectionReason);
	}

	protected void AuditHiddenRecoveryDeferred(
		AICF_TransportTrip trip,
		AICF_DismountClearanceSample sample,
		string rejectionReason,
		float nearestPlayerMeters)
	{
		int nowMs = System.GetTickCount();
		if (!trip.GetDismountState().ShouldAuditHiddenRecoveryRejection(
			rejectionReason,
			nowMs,
			HIDDEN_RECOVERY_AUDIT_INTERVAL_MS))
		{
			return;
		}
		AICF_Stage3Diagnostics.Info(
			"TERMINAL_CLEARANCE_RECOVERY_DEFERRED",
			FormatClearance(trip, sample, rejectionReason) + string.Format(
				" hidden_radius_m=%1 nearest_player_m=%2 action=CONTINUE_SAFE_POLLING",
				m_Config.GetHiddenRecoveryPlayerRadiusMeters(),
				nearestPlayerMeters));
	}

	protected bool FindGuidancePosition(
		Vehicle vehicle,
		IEntity entity,
		vector boundsMin,
		vector boundsMax,
		float clearanceRadius,
		int directionIndex,
		out vector safePosition)
	{
		vector localOrigin = vehicle.CoordToLocal(entity.GetOrigin());
		vector direction = Vector(localOrigin[0], 0, localOrigin[2]);
		if (direction.LengthSq() < 0.01)
		{
			switch (directionIndex % 4)
			{
				case 0: direction = "1 0 0"; break;
				case 1: direction = "-1 0 0"; break;
				case 2: direction = "0 0 1"; break;
				default: direction = "0 0 -1"; break;
			}
		}
		direction.Normalize();
		for (int attempt; attempt < 3; attempt++)
		{
			vector center = vehicle.CoordToParent(direction * (clearanceRadius + attempt * 3.0));
			if (!SCR_WorldTools.FindEmptyTerrainPosition(safePosition, center, 3.0, 0.75, 2.0))
				continue;
			if (IsInsideExpandedBounds(vehicle.CoordToLocal(safePosition), boundsMin, boundsMax) ||
				ChimeraWorldUtils.TryGetWaterSurfaceSimple(vehicle.GetWorld(), safePosition))
				continue;
			return true;
		}
		return false;
	}

	protected bool IsPhysicalOnlyBlocker(
		ChimeraCharacter character,
		Vehicle vehicle,
		vector boundsMin,
		vector boundsMax)
	{
		if (!character)
			return false;
		CompartmentAccessComponent access = character.GetCompartmentAccessComponent();
		if (CompartmentAccessComponent.GetVehicleIn(character) == vehicle ||
			character.IsInVehicle() ||
			(access && (access.IsInCompartment() || access.IsGettingIn() || access.IsGettingOut())))
			return false;
		return IsInsideExpandedBounds(vehicle.CoordToLocal(character.GetOrigin()), boundsMin, boundsMax);
	}

	protected bool IsInsideExpandedBounds(vector localOrigin, vector boundsMin, vector boundsMax)
	{
		return localOrigin[0] >= boundsMin[0] - CLEARANCE_MARGIN_METERS &&
			localOrigin[0] <= boundsMax[0] + CLEARANCE_MARGIN_METERS &&
			localOrigin[1] >= boundsMin[1] - CLEARANCE_MARGIN_METERS &&
			localOrigin[1] <= boundsMax[1] + CLEARANCE_MARGIN_METERS &&
			localOrigin[2] >= boundsMin[2] - CLEARANCE_MARGIN_METERS &&
			localOrigin[2] <= boundsMax[2] + CLEARANCE_MARGIN_METERS;
	}

	protected float ResolveClearanceRadius(vector boundsMin, vector boundsMax, float padding)
	{
		return Math.Max(
			Math.Max(Math.AbsFloat(boundsMin[0]), Math.AbsFloat(boundsMax[0])),
			Math.Max(Math.AbsFloat(boundsMin[2]), Math.AbsFloat(boundsMax[2]))) + padding;
	}

	protected bool IsExactAliveCurrentMember(SCR_AIGroup group, AIAgent agent)
	{
		return group && agent && agent.GetParentGroup() == group &&
			AICF_VehicleBoardingMutationFence.IsAuthoritativeAIEntity(
				agent.GetControlledEntity());
	}

	protected bool IsAuthoritativeTripAssetCurrent(AICF_TransportTrip trip)
	{
		if (!Replication.IsServer() || !trip || !trip.GetAssignment() || !trip.GetLease())
			return false;
		AICF_VehicleLease lease = trip.GetLease();
		Vehicle vehicle = lease.GetVehicle();
		if (!vehicle ||
			!AICF_VehicleBoardingMutationFence.IsAuthoritativeReplicatedEntity(vehicle) ||
			!lease.MatchesTripIdentity(
			trip.GetFactionKey(),
			trip.GetSlotId(),
			trip.GetGroupGeneration(),
			trip.GetTripGeneration()))
			return false;
		string rplId = ResolveRplId(vehicle);
		return lease.MatchesEntityIdentity(vehicle, vehicle.GetID(), rplId);
	}

	protected AICF_VehicleAsyncFence CreateFence(AICF_TransportTrip trip, string token)
	{
		if (!IsAuthoritativeTripAssetCurrent(trip))
			return null;
		AICF_VehicleLease lease = trip.GetLease();
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

	protected string ResolveRplId(IEntity entity)
	{
		if (!entity)
			return "NONE";
		RplComponent rpl = RplComponent.Cast(entity.FindComponent(RplComponent));
		if (!rpl)
			return "NONE";
		return rpl.Id().ToString();
	}

	protected string FormatClearance(
		AICF_TransportTrip trip,
		AICF_DismountClearanceSample sample,
		string reason)
	{
		string details = FormatIdentity(trip, reason);
		details += string.Format(
			" logical_occupants=%1 transitions=%2 inside_bounds=%3 force_attempts=%4 clearance_attempts=%5",
			sample.m_iLogicalOccupants,
			sample.m_iTransitions,
			sample.m_iInsideBounds,
			trip.GetDismountState().GetForceClearanceAttempts(),
			trip.GetDismountState().GetGuidanceAttempts());
		details += string.Format(
			" clearance_safe=%1 continuous_clear_ms=%2 required_clear_ms=%3 next_action=%4 samples=[%5]",
			sample.m_bSafelyClear,
			trip.GetDismountState().GetContinuousClearMs(System.GetTickCount()),
			CONTINUOUS_CLEAR_MS,
			reason,
			sample.m_sMemberSamples);
		return details;
	}

	protected string FormatIdentity(AICF_TransportTrip trip, string reason)
	{
		AICF_VehicleLease lease = trip.GetLease();
		string details = string.Format(
			"faction=%1 slot=%2 group_generation=%3 trip_generation=%4 operation_id=%5 causation_id=%6",
			trip.GetFactionKey(),
			trip.GetSlotKey(),
			trip.GetGroupGeneration(),
			trip.GetTripGeneration(),
			trip.GetOperationId(),
			trip.GetCausationId());
		details += string.Format(
			" lease_generation=%1 vehicle_generation=%2 vehicle_lifecycle_id=%3 entity=%4 rpl_id=%5 reason=%6",
			lease.GetLeaseGeneration(),
			lease.GetVehicleGeneration(),
			lease.GetVehicleLifecycleId(),
			lease.GetEntityIdString(),
			lease.GetRplId(),
			reason);
		details += string.Format(
			" vehicle=%1 kind=%2 state=%3 prefab=%4 entity_id=%5",
			lease.GetEntityIdString(),
			AICF_Stage3Diagnostics.KindToString(lease.GetKind()),
			AICF_Stage3Diagnostics.TripPhaseToString(trip.GetPhase()),
			lease.GetPrefab(),
			lease.GetEntityIdString());
		return details;
	}
}

class AICF_DismountClearanceSample
{
	bool m_bSafelyClear;
	int m_iLogicalOccupants;
	int m_iTransitions;
	int m_iInsideBounds;
	string m_sMemberSamples;
}
