// The sole mutation authority for TransportTrip phase and assignment revision.
// Phase flows return typed outcomes; this controller applies them, performs the
// exactly-once cross-boundary effects, and never implements engine mechanics.
class AICF_TransportTripController
{
	protected static const int ORDER_RESTORE_DEADLINE_MS = 30000;
	protected static const int ORDER_RESTORE_MAX_ATTEMPTS = 2;
	protected static const int TERMINAL_AUDIT_INTERVAL_MS = 30000;
	protected static const int CLEANUP_STABLE_CLEAR_MS = 5000;

	protected ref AICF_Stage3Config m_Config;
	protected SCR_GameModeCampaign m_Campaign;
	protected AICF_ConflictAdapter m_ConflictAdapter;
	protected ref AICF_VehicleAcquisitionFlow m_AcquisitionFlow;
	protected ref AICF_VehicleBoardingFlow m_BoardingFlow;
	protected ref AICF_VehicleTransitFlow m_TransitFlow;
	protected ref AICF_VehicleDismountFlow m_DismountFlow;
	protected ref AICF_VehicleTaskHandoff m_Handoff;
	protected ref AICF_VehicleCleanupManager m_CleanupManager;
	protected ref AICF_VehicleDomainDiagnostics m_Diagnostics;

	void AICF_TransportTripController(
		AICF_Stage3Config config,
		SCR_GameModeCampaign campaign,
		AICF_ConflictAdapter conflictAdapter,
		AICF_GroupCohesionPolicy cohesionPolicy,
		AICF_OrderPlanner orderPlanner,
		AICF_ObjectiveGraph objectiveGraph,
		AICF_TargetSelector targetSelector)
	{
		m_Config = config;
		if (!m_Config)
			m_Config = new AICF_Stage3Config();
		m_Campaign = campaign;
		m_ConflictAdapter = conflictAdapter;
		AICF_VehicleWatchdog watchdog = new AICF_VehicleWatchdog();
		AICF_VehicleWaypointFactory waypointFactory = new AICF_VehicleWaypointFactory();
		m_AcquisitionFlow = new AICF_VehicleAcquisitionFlow(
			m_Config,
			campaign,
			conflictAdapter,
			cohesionPolicy);
		m_BoardingFlow = new AICF_VehicleBoardingFlow(m_Config, watchdog);
		m_TransitFlow = new AICF_VehicleTransitFlow(m_Config, waypointFactory, watchdog);
		m_DismountFlow = new AICF_VehicleDismountFlow(m_Config, watchdog, waypointFactory);
		m_Handoff = new AICF_VehicleTaskHandoff(orderPlanner, objectiveGraph, targetSelector);
		m_Diagnostics = new AICF_VehicleDomainDiagnostics();
	}

	// Composition is completed by the thin facade. Cleanup remains an
	// independent asset lifecycle and never receives phase-mutation authority.
	void SetCleanupManager(AICF_VehicleCleanupManager cleanupManager)
	{
		m_CleanupManager = cleanupManager;
	}

	// The returned outcome is observational: it has already been applied here.
	AICF_TripOutcome Tick(
		AICF_TransportTrip trip,
		AICF_StrategicAssignmentSnapshot currentAssignment,
		AICF_GroupSlot slot,
		AICF_FactionFleet fleet,
		SCR_CampaignFaction faction)
	{
		if (!trip)
			return AICF_TripOutcome.TerminalFailClosed(
				"TRIP_MISSING",
				"controller-trip-missing");
		if (!HasCurrentIdentity(trip, currentAssignment, slot, fleet, faction))
		{
			// Tick may itself be a stale scheduler callback. It has no authority to
			// terminate a newer Trip. The facade uses the explicit, identity-scoped
			// TerminateStaleTrip path after observing an actual replacement.
			return AICF_TripOutcome.Wait(
				"STALE_CONTROLLER_CALLBACK_CANCELLED",
				BuildCausationId(trip, "STALE_CALLBACK"));
		}
		if (!IsAuthorityReady())
		{
			return AICF_TripOutcome.Wait(
				"CONTROLLER_AUTHORITY_NOT_READY",
				BuildCausationId(trip, "AUTHORITY_WAIT"));
		}
		if (HasStaleRevision(trip, currentAssignment))
		{
			return AICF_TripOutcome.Wait(
				"STALE_ASSIGNMENT_CALLBACK_CANCELLED",
				BuildCausationId(trip, "STALE_REVISION"));
		}
		if (trip.IsTerminal())
			return TickTerminalPostconditions(trip, currentAssignment, slot, fleet, faction);

		if (IsAcquisitionPhase(trip.GetPhase()))
			return TickAcquisition(trip, currentAssignment, slot, fleet, faction);
		AICF_TripOutcome retargetOutcome = ApplyCurrentRetarget(
			trip,
			currentAssignment,
			slot,
			fleet,
			faction);
		if (retargetOutcome)
			return retargetOutcome;
		return DispatchCurrentPhase(trip, currentAssignment, slot, fleet, faction);
	}

	// Explicit replacement/stale path. It never restores onto the old group.
	// The physical lease remains attached until terminal clearance and cleanup
	// have accepted it, so the registry cannot admit a replacement prematurely.
	AICF_TripOutcome TerminateStaleTrip(
		AICF_TransportTrip trip,
		AICF_FactionFleet fleet,
		string reason)
	{
		if (!trip)
			return AICF_TripOutcome.TerminalFailClosed(reason, "stale-trip-missing");
		string causationId = BuildCausationId(trip, "STALE_TERMINATION");
		if (!trip.IsValid() || !fleet || reason.IsEmpty() ||
			fleet.GetFactionKey() != trip.GetFactionKey() ||
			!IsStaleTerminationLeaseCurrent(trip, fleet))
		{
			return AICF_TripOutcome.Wait(
				"STALE_TERMINATION_IDENTITY_REJECTED",
				causationId);
		}
		if (trip.IsTerminal())
		{
			AICF_TripOutcome terminalOutcome;
			if (trip.GetHandoffState().IsLeaseReleaseRequested())
				terminalOutcome = AdvanceLeaseCleanup(trip, fleet, causationId);
			else
				terminalOutcome = ContinueTerminalClearanceWithoutRestore(trip, fleet, causationId);
			AuditTerminalPostconditions(trip, null, null);
			return terminalOutcome;
		}
		if (!trip.IsTransitionAllowedTo(AICF_ETransportTripPhase.FAILED_CLOSED))
		{
			return AICF_TripOutcome.TerminalFailClosed(
				"STALE_TERMINATION_PREFLIGHT_REJECTED",
				causationId);
		}

		ReleaseEmptyReservationIfPresent(trip, fleet);
		m_Handoff.DetachVehicleUtility(trip);
		if (!TransitionTo(
			trip,
			AICF_ETransportTripPhase.FAILED_CLOSED,
			reason,
			causationId))
		{
			return AICF_TripOutcome.TerminalFailClosed(
				"STALE_TERMINATION_COMMIT_REJECTED",
				causationId);
		}
		BeginHandoffEvidence(trip, false);
		BeginTerminalClearanceIfRequired(trip, causationId);
		return AICF_TripOutcome.TerminalFailClosed(reason, causationId);
	}

	protected bool IsStaleTerminationLeaseCurrent(
		AICF_TransportTrip trip,
		AICF_FactionFleet fleet)
	{
		AICF_VehicleLease lease = trip.GetLease();
		if (!lease)
			return true;
		if (!lease.MatchesTripIdentity(
			trip.GetFactionKey(),
			trip.GetSlotId(),
			trip.GetGroupGeneration(),
			trip.GetTripGeneration()))
		{
			return false;
		}
		if (lease.GetState() == AICF_EVehicleLeaseState.RELEASED)
			return trip.IsTerminal() && trip.GetHandoffState().IsCleanupQueueAccepted();
		return fleet.FindLeaseForSlot(
			trip.GetSlotId(),
			trip.GetGroupGeneration()) == lease;
	}

	bool HasPendingLeaseRelease(AICF_TransportTrip trip)
	{
		return trip && trip.IsTerminal() && trip.GetLease() &&
			trip.GetHandoffState().IsLeaseReleaseRequested();
	}

	AICF_VehicleLease GetPendingLeaseRelease(AICF_TransportTrip trip)
	{
		if (!HasPendingLeaseRelease(trip))
			return null;
		return trip.GetLease();
	}

	string GetPendingLeaseReleaseReason(AICF_TransportTrip trip)
	{
		if (!HasPendingLeaseRelease(trip))
			return string.Empty;
		return trip.GetTerminalReason();
	}

	bool CanRetireTrip(AICF_TransportTrip trip)
	{
		if (!trip || !trip.IsTerminal() || trip.GetLease())
			return false;
		AICF_VehicleHandoffState state = trip.GetHandoffState();
		// Retained fail-closed means CleanupManager/Fleet deliberately own the
		// protected physical asset and its cap-holding lease indefinitely.  The
		// Trip no longer owns either and may retire without claiming release or
		// clearance success.
		if (state.IsCleanupRetainedFailClosed() &&
			state.IsCleanupOwnershipAcceptedTerminal())
			return true;
		return !state.IsLeaseReleaseRequested() || state.IsCleanupReleaseComplete();
	}

	// Stop is an ownership boundary, not merely a scheduler shutdown. Every
	// live phase is exited through the same exactly-once transition path so its
	// action tokens, reservations, vehicle waypoint and utility cannot survive
	// after the facade is released. Physical-asset disposition remains the
	// independent CleanupManager/facade decision.
	AICF_TripOutcome AbortForStop(
		AICF_TransportTrip trip,
		AICF_GroupSlot slot,
		AICF_FactionFleet fleet,
		SCR_CampaignFaction faction,
		bool restoreInfantryOrder,
		string reason)
	{
		if (!trip || !trip.IsValid())
			return AICF_TripOutcome.TerminalFailClosed(
				"STOP_TRIP_IDENTITY_INVALID",
				"controller-stop-invalid");
		if (reason.IsEmpty())
			reason = "COORDINATOR_STOP";
		string causationId = BuildCausationId(trip, "STOP_ABORT");
		bool committedTerminal;
		if (!trip.IsTerminal())
		{
			// An empty reservation has no physical cleanup obligation and must not
			// remain counted after its scheduler is stopped.
			ReleaseEmptyReservationIfPresent(trip, fleet);
			committedTerminal = TransitionTo(
				trip,
				AICF_ETransportTripPhase.FAILED_CLOSED,
				reason,
				causationId);
			if (committedTerminal)
				PrepareStopHandoffEvidence(trip);
		}
		else
		{
			committedTerminal = true;
			// Terminal exact-clearance may still own bounded per-member guidance.
			// It is separate from the Trip phase and therefore needs an explicit
			// stop cancellation even though no further transition is legal.
			CancelResidualOwnedEffectsForStop(trip, reason);
		}

		if (!committedTerminal)
		{
			CancelResidualOwnedEffectsForStop(trip, reason);
			AICF_Stage3Diagnostics.Error(
				"VEHICLE_STOP_PHASE_ABORT_FAILED",
				FormatIdentity(trip, causationId, reason));
		}
		m_Handoff.DetachVehicleUtility(trip);
		if (restoreInfantryOrder && slot && faction &&
			!trip.GetHandoffState().IsOrderRestored())
		{
			EnsureHandoffStarted(trip.GetHandoffState(), System.GetTickCount());
			m_Handoff.RestoreInfantryOrder(
				trip,
				trip.GetHandoffState(),
				slot,
				faction,
				"COORDINATOR_STOP",
				reason);
		}
		if (!committedTerminal)
		{
			return AICF_TripOutcome.TerminalFailClosed(
				"STOP_PHASE_EXIT_REJECTED",
				causationId);
		}
		if (trip.GetPhase() == AICF_ETransportTripPhase.FAILED_CLOSED)
			return AICF_TripOutcome.TerminalFailClosed(reason, causationId);
		return AICF_TripOutcome.Wait("STOP_SIDE_EFFECTS_ABORTED", causationId);
	}

	protected AICF_TripOutcome TickAcquisition(
		AICF_TransportTrip trip,
		AICF_StrategicAssignmentSnapshot currentAssignment,
		AICF_GroupSlot slot,
		AICF_FactionFleet fleet,
		SCR_CampaignFaction faction)
	{
		bool assignmentChanged = HasAssignmentChange(trip, currentAssignment);
		AICF_TripOutcome outcome = m_AcquisitionFlow.Update(
			trip,
			currentAssignment,
			fleet,
			faction,
			System.GetTickCount());
		if (assignmentChanged)
		{
			string causationId = ResolveCausationId(trip, outcome, "ACQUISITION_RETARGET");
			if (!CommitRetarget(trip, currentAssignment, causationId))
			{
				return EndVehicleControl(
					trip,
					slot,
					fleet,
					faction,
					AICF_ETransportTripPhase.FAILED_CLOSED,
					"ACQUISITION_RETARGET_COMMIT_REJECTED",
					causationId,
					true);
			}
		}
		return ApplyOutcome(trip, outcome, currentAssignment, slot, fleet, faction);
	}

	protected AICF_TripOutcome DispatchCurrentPhase(
		AICF_TransportTrip trip,
		AICF_StrategicAssignmentSnapshot currentAssignment,
		AICF_GroupSlot slot,
		AICF_FactionFleet fleet,
		SCR_CampaignFaction faction)
	{
		string causationId = trip.GetCausationId();
		AICF_TripOutcome outcome;
		switch (trip.GetPhase())
		{
			case AICF_ETransportTripPhase.BOARDING:
				outcome = m_BoardingFlow.Tick(trip, causationId);
				break;
			case AICF_ETransportTripPhase.TRANSIT:
				outcome = m_TransitFlow.Tick(trip, causationId);
				break;
			case AICF_ETransportTripPhase.DISMOUNT:
				outcome = m_DismountFlow.ProcessNormalDismount(trip, causationId);
				break;
			case AICF_ETransportTripPhase.HANDOFF:
				outcome = TickHandoff(trip, slot, faction);
				break;
			default:
				outcome = AICF_TripOutcome.TerminalFailClosed(
					"CONTROLLER_PHASE_UNDISPATCHABLE",
					BuildCausationId(trip, "PHASE_GUARD"));
		}
		return ApplyOutcome(trip, outcome, currentAssignment, slot, fleet, faction);
	}

	protected AICF_TripOutcome ApplyOutcome(
		AICF_TransportTrip trip,
		AICF_TripOutcome outcome,
		AICF_StrategicAssignmentSnapshot currentAssignment,
		AICF_GroupSlot slot,
		AICF_FactionFleet fleet,
		SCR_CampaignFaction faction)
	{
		if (!outcome)
		{
			return EndVehicleControl(
				trip, slot, fleet, faction,
				AICF_ETransportTripPhase.FAILED_CLOSED,
				"FLOW_OUTCOME_MISSING",
				BuildCausationId(trip, "OUTCOME_GUARD"),
				true);
		}
		AIWaypoint waypointForRemoval = outcome.GetWaypointForRemoval();
		if (waypointForRemoval)
		{
			m_Handoff.RemoveVehicleWaypoint(
				trip,
				waypointForRemoval,
				"FLOW_ROLLBACK",
				outcome.GetReason());
		}

		switch (outcome.GetKind())
		{
			case AICF_ETripOutcomeKind.WAIT:
				return outcome;
			case AICF_ETripOutcomeKind.RETRY:
				return ApplyRetry(trip, outcome, currentAssignment, slot, fleet, faction);
			case AICF_ETripOutcomeKind.START_BOARDING:
				return StartBoarding(trip, outcome, currentAssignment, slot, fleet, faction);
			case AICF_ETripOutcomeKind.START_MOVEMENT:
				return StartOrReconcileMovement(
					trip, outcome, currentAssignment, slot, fleet, faction);
			case AICF_ETripOutcomeKind.START_DISMOUNT:
				return StartOrReconcileDismount(
					trip, outcome, currentAssignment, slot, fleet, faction);
			case AICF_ETripOutcomeKind.COMPLETE_TRIP:
				return ApplyComplete(trip, outcome, currentAssignment, slot, fleet, faction);
			case AICF_ETripOutcomeKind.FALLBACK_TO_FOOT:
				return EndVehicleControl(
					trip, slot, fleet, faction,
					AICF_ETransportTripPhase.FALLBACK,
					outcome.GetReason(), outcome.GetCausationId(), true);
			case AICF_ETripOutcomeKind.RELEASE_LEASE:
				return ForwardLeaseRelease(trip, outcome);
			case AICF_ETripOutcomeKind.TERMINAL_FAIL_CLOSED:
				return EndVehicleControl(
					trip, slot, fleet, faction,
					AICF_ETransportTripPhase.FAILED_CLOSED,
					outcome.GetReason(), outcome.GetCausationId(), true);
		}
		return outcome;
	}

	protected AICF_TripOutcome ApplyRetry(
		AICF_TransportTrip trip,
		AICF_TripOutcome outcome,
		AICF_StrategicAssignmentSnapshot currentAssignment,
		AICF_GroupSlot slot,
		AICF_FactionFleet fleet,
		SCR_CampaignFaction faction)
	{
		if (outcome.GetReason() == "LEASE_RESERVED_FOR_ATTACH")
		{
			if (!AttachReservedLease(trip, fleet))
			{
				return EndVehicleControl(
					trip, slot, fleet, faction,
					AICF_ETransportTripPhase.FAILED_CLOSED,
					"LEASE_ATTACH_REJECTED",
					outcome.GetCausationId(), true);
			}
			return outcome;
		}
		if (outcome.GetReason().Contains("RELEASE_RESERVATION_FOR_RETARGET:"))
		{
			if (!ReleaseEmptyReservationIfPresent(trip, fleet))
			{
				return EndVehicleControl(
					trip, slot, fleet, faction,
					AICF_ETransportTripPhase.FAILED_CLOSED,
					"RETARGET_RESERVATION_RELEASE_REJECTED",
					outcome.GetCausationId(), true);
			}
			return outcome;
		}
		if (outcome.GetReason().Contains("ENTER_WAITING_FOR_SITE:"))
		{
			if (trip.GetPhase() == AICF_ETransportTripPhase.ACQUIRING &&
				!trip.IsTransitionAllowedTo(AICF_ETransportTripPhase.WAITING_FOR_SITE))
			{
				return EndVehicleControl(
					trip, slot, fleet, faction,
					AICF_ETransportTripPhase.FAILED_CLOSED,
					"WAITING_TRANSITION_PREFLIGHT_REJECTED",
					outcome.GetCausationId(), true);
			}
			if (!ReleaseEmptyReservationIfPresent(trip, fleet))
			{
				return EndVehicleControl(
					trip, slot, fleet, faction,
					AICF_ETransportTripPhase.FAILED_CLOSED,
					"WAITING_RESERVATION_RELEASE_REJECTED",
					outcome.GetCausationId(), true);
			}
			if (trip.GetPhase() == AICF_ETransportTripPhase.ACQUIRING &&
				!TransitionTo(
					trip,
					AICF_ETransportTripPhase.WAITING_FOR_SITE,
					outcome.GetReason(),
					outcome.GetCausationId()))
			{
				return EndVehicleControl(
					trip, slot, fleet, faction,
					AICF_ETransportTripPhase.FAILED_CLOSED,
					"WAITING_TRANSITION_REJECTED",
					outcome.GetCausationId(), true);
			}
			return outcome;
		}

		if (trip.GetPhase() == AICF_ETransportTripPhase.WAITING_FOR_SITE)
		{
			if (!TransitionTo(
				trip,
				AICF_ETransportTripPhase.ACQUIRING,
				outcome.GetReason(),
				outcome.GetCausationId()))
			{
				return EndVehicleControl(
					trip, slot, fleet, faction,
					AICF_ETransportTripPhase.FAILED_CLOSED,
					"ACQUISITION_TRANSITION_REJECTED",
					outcome.GetCausationId(), true);
			}
		}
		return outcome;
	}

	protected AICF_TripOutcome StartBoarding(
		AICF_TransportTrip trip,
		AICF_TripOutcome outcome,
		AICF_StrategicAssignmentSnapshot currentAssignment,
		AICF_GroupSlot slot,
		AICF_FactionFleet fleet,
		SCR_CampaignFaction faction)
	{
		if (trip.GetPhase() != AICF_ETransportTripPhase.ACQUIRING ||
			!TransitionTo(
				trip,
				AICF_ETransportTripPhase.BOARDING,
				outcome.GetReason(),
				outcome.GetCausationId()))
		{
			return EndVehicleControl(
				trip, slot, fleet, faction,
				AICF_ETransportTripPhase.FAILED_CLOSED,
				"BOARDING_TRANSITION_REJECTED",
				outcome.GetCausationId(), true);
		}
		if (!m_Handoff.SuspendInfantryOrder(trip, slot, "BOARDING_BEGIN"))
		{
			return EndVehicleControl(
				trip, slot, fleet, faction,
				AICF_ETransportTripPhase.FAILED_CLOSED,
				"STRATEGIC_WAYPOINT_SUSPEND_REJECTED",
				outcome.GetCausationId(), true);
		}
		AICF_TripOutcome beginOutcome = m_BoardingFlow.Begin(
			trip,
			outcome.GetCausationId());
		return ApplyOutcome(trip, beginOutcome, currentAssignment, slot, fleet, faction);
	}

	protected AICF_TripOutcome StartOrReconcileMovement(
		AICF_TransportTrip trip,
		AICF_TripOutcome outcome,
		AICF_StrategicAssignmentSnapshot currentAssignment,
		AICF_GroupSlot slot,
		AICF_FactionFleet fleet,
		SCR_CampaignFaction faction)
	{
		if (trip.GetPhase() == AICF_ETransportTripPhase.BOARDING)
		{
			if (!TransitionTo(
				trip,
				AICF_ETransportTripPhase.TRANSIT,
				outcome.GetReason(),
					outcome.GetCausationId()))
			{
				return EndVehicleControl(
					trip, slot, fleet, faction,
					AICF_ETransportTripPhase.FAILED_CLOSED,
					"TRANSIT_TRANSITION_REJECTED",
					outcome.GetCausationId(), true);
			}
			if (!m_Handoff.AttachVehicleUtility(trip))
			{
				return EndVehicleControl(
					trip, slot, fleet, faction,
					AICF_ETransportTripPhase.FAILED_CLOSED,
					"VEHICLE_UTILITY_ATTACH_REJECTED",
					outcome.GetCausationId(), true);
			}
			AICF_TripOutcome routeOutcome = m_TransitFlow.PrepareRoute(
				trip,
				outcome.GetCausationId());
			return ApplyOutcome(trip, routeOutcome, currentAssignment, slot, fleet, faction);
		}
		if (trip.GetPhase() != AICF_ETransportTripPhase.TRANSIT)
		{
			return EndVehicleControl(
				trip, slot, fleet, faction,
				AICF_ETransportTripPhase.FAILED_CLOSED,
				"MOVEMENT_OUTCOME_PHASE_MISMATCH",
				outcome.GetCausationId(), true);
		}
		if (!ReconcileTransitWaypoints(trip, outcome.GetReason()))
		{
			return EndVehicleControl(
				trip, slot, fleet, faction,
				AICF_ETransportTripPhase.FAILED_CLOSED,
				"TRANSIT_WAYPOINT_RECONCILIATION_REJECTED",
				outcome.GetCausationId(), true);
		}
		return outcome;
	}

	protected AICF_TripOutcome StartOrReconcileDismount(
		AICF_TransportTrip trip,
		AICF_TripOutcome outcome,
		AICF_StrategicAssignmentSnapshot currentAssignment,
		AICF_GroupSlot slot,
		AICF_FactionFleet fleet,
		SCR_CampaignFaction faction)
	{
		if (trip.GetPhase() == AICF_ETransportTripPhase.TRANSIT)
		{
			if (!TransitionTo(
				trip,
				AICF_ETransportTripPhase.DISMOUNT,
				outcome.GetReason(),
					outcome.GetCausationId()))
			{
				return EndVehicleControl(
					trip, slot, fleet, faction,
					AICF_ETransportTripPhase.FAILED_CLOSED,
					"DISMOUNT_TRANSITION_REJECTED",
					outcome.GetCausationId(), true);
			}
			AICF_TripOutcome beginOutcome = m_DismountFlow.Begin(
				trip,
				outcome.GetCausationId());
			return ApplyOutcome(trip, beginOutcome, currentAssignment, slot, fleet, faction);
		}
		if (trip.GetPhase() != AICF_ETransportTripPhase.DISMOUNT)
		{
			return EndVehicleControl(
				trip, slot, fleet, faction,
				AICF_ETransportTripPhase.FAILED_CLOSED,
				"DISMOUNT_OUTCOME_PHASE_MISMATCH",
				outcome.GetCausationId(), true);
		}
		if (!ReconcileDismountWaypoints(trip, outcome.GetReason()))
		{
			return EndVehicleControl(
				trip, slot, fleet, faction,
				AICF_ETransportTripPhase.FAILED_CLOSED,
				"DISMOUNT_WAYPOINT_RECONCILIATION_REJECTED",
				outcome.GetCausationId(), true);
		}
		return outcome;
	}

	protected AICF_TripOutcome ApplyComplete(
		AICF_TransportTrip trip,
		AICF_TripOutcome outcome,
		AICF_StrategicAssignmentSnapshot currentAssignment,
		AICF_GroupSlot slot,
		AICF_FactionFleet fleet,
		SCR_CampaignFaction faction)
	{
		if (trip.GetPhase() == AICF_ETransportTripPhase.DISMOUNT)
			return StartHandoff(trip, outcome, currentAssignment, slot, fleet, faction);
		if (trip.GetPhase() != AICF_ETransportTripPhase.HANDOFF ||
			!trip.GetHandoffState().IsOrderRestored())
		{
			return EndVehicleControl(
				trip, slot, fleet, faction,
				AICF_ETransportTripPhase.FAILED_CLOSED,
				"COMPLETE_WITHOUT_STRICT_HANDOFF_PROOF",
				outcome.GetCausationId(), true);
		}
		if (!TransitionTo(
			trip,
			AICF_ETransportTripPhase.COMPLETE,
			outcome.GetReason(),
			outcome.GetCausationId()))
		{
			return EndVehicleControl(
				trip, slot, fleet, faction,
				AICF_ETransportTripPhase.FAILED_CLOSED,
				"COMPLETE_TRANSITION_REJECTED",
				outcome.GetCausationId(), true);
		}
		return ForwardLeaseRelease(trip, outcome);
	}

	protected AICF_TripOutcome StartHandoff(
		AICF_TransportTrip trip,
		AICF_TripOutcome outcome,
		AICF_StrategicAssignmentSnapshot currentAssignment,
		AICF_GroupSlot slot,
		AICF_FactionFleet fleet,
		SCR_CampaignFaction faction)
	{
		if (!TransitionTo(
			trip,
			AICF_ETransportTripPhase.HANDOFF,
			outcome.GetReason(),
			outcome.GetCausationId()))
		{
			return EndVehicleControl(
				trip, slot, fleet, faction,
				AICF_ETransportTripPhase.FAILED_CLOSED,
				"HANDOFF_TRANSITION_REJECTED",
				outcome.GetCausationId(), true);
		}
		m_Handoff.DetachVehicleUtility(trip);
		// Managed dismount is not the full cleanup safety proof. Foreign/player
		// occupants, transitions, 15 m proximity and stable-clear remain pending.
		BeginHandoffEvidence(trip, false);
		// Full asset clearance begins independently of strategic order restore.
		// Cleanup acceptance may advance while HANDOFF proves the infantry task.
		trip.GetHandoffState().RequestLeaseRelease();
		AICF_TripOutcome handoffOutcome = TickHandoff(trip, slot, faction);
		return ApplyOutcome(trip, handoffOutcome, currentAssignment, slot, fleet, faction);
	}

	protected AICF_TripOutcome TickHandoff(
		AICF_TransportTrip trip,
		AICF_GroupSlot slot,
		SCR_CampaignFaction faction)
	{
		AICF_VehicleHandoffState state = trip.GetHandoffState();
		EnsureHandoffStarted(state, System.GetTickCount());
		if (!state.IsOrderRestored())
			m_Handoff.RestoreInfantryOrder(
				trip,
				state,
				slot,
				faction,
				"VEHICLE_CONTROL_ENDED",
				trip.GetLastTransitionReason());
		if (state.IsOrderRestored())
		{
			return AICF_TripOutcome.CompleteTrip(
				"HANDOFF_STRICT_PROOF_COMPLETE",
				BuildCausationId(trip, "HANDOFF_COMPLETE"));
		}
		if (System.GetTickCount() >= state.GetAbsoluteOrderDeadlineMs())
		{
			return AICF_TripOutcome.TerminalFailClosed(
				"ORDER_RESTORE_DEADLINE_MISSED",
				BuildCausationId(trip, "HANDOFF_DEADLINE"));
		}
		return AICF_TripOutcome.Wait(
			"ORDER_RESTORE_PENDING",
			BuildCausationId(trip, "HANDOFF_WAIT"));
	}

	protected AICF_TripOutcome EndVehicleControl(
		AICF_TransportTrip trip,
		AICF_GroupSlot slot,
		AICF_FactionFleet fleet,
		SCR_CampaignFaction faction,
		AICF_ETransportTripPhase terminalPhase,
		string reason,
		string causationId,
		bool restoreCurrentOrder)
	{
		if (!trip)
			return AICF_TripOutcome.TerminalFailClosed(reason, causationId);
		if (trip.IsTerminal())
			return TickTerminalPostconditions(trip, trip.GetAssignment(), slot, fleet, faction);
		if (!trip.IsTransitionAllowedTo(terminalPhase))
			return AICF_TripOutcome.TerminalFailClosed("TERMINAL_PREFLIGHT_REJECTED", causationId);
		ReleaseEmptyReservationIfPresent(trip, fleet);
		m_Handoff.DetachVehicleUtility(trip);
		if (!TransitionTo(trip, terminalPhase, reason, causationId))
			return AICF_TripOutcome.TerminalFailClosed("TERMINAL_COMMIT_REJECTED", causationId);

		BeginHandoffEvidence(trip, !trip.GetLease());
		if (restoreCurrentOrder && slot && faction)
		{
			m_Handoff.RestoreInfantryOrder(
				trip,
				trip.GetHandoffState(),
				slot,
				faction,
				"VEHICLE_CONTROL_ENDED",
				reason);
		}
		BeginTerminalClearanceIfRequired(trip, causationId);
		if (terminalPhase == AICF_ETransportTripPhase.FALLBACK)
			return AICF_TripOutcome.FallbackToFoot(reason, causationId);
		return AICF_TripOutcome.TerminalFailClosed(reason, causationId);
	}

	protected AICF_TripOutcome TickTerminalPostconditions(
		AICF_TransportTrip trip,
		AICF_StrategicAssignmentSnapshot currentAssignment,
		AICF_GroupSlot slot,
		AICF_FactionFleet fleet,
		SCR_CampaignFaction faction)
	{
		AICF_VehicleHandoffState state = trip.GetHandoffState();
		EnsureHandoffStarted(state, System.GetTickCount());
		if (!state.IsOrderRestored() &&
			HasCurrentIdentity(trip, currentAssignment, slot, fleet, faction))
		{
			m_Handoff.RestoreInfantryOrder(
				trip,
				state,
				slot,
				faction,
				"TERMINAL_ORDER_RESTORE",
				trip.GetTerminalReason());
		}
		AICF_TripOutcome outcome;
		if (!trip.GetLease() && state.IsLeaseReleaseRequested() &&
			!state.IsCleanupReleaseComplete() &&
			!state.IsCleanupOwnershipAcceptedTerminal())
		{
			outcome = AICF_TripOutcome.TerminalFailClosed(
				"LEASE_DETACHED_BEFORE_CLEANUP_PROOF",
				trip.GetCausationId());
		}
		else if (!trip.GetLease())
		{
			outcome = AICF_TripOutcome.Wait(
				"TERMINAL_POSTCONDITIONS_OWNERSHIP_COMPLETE",
				trip.GetCausationId());
		}
		else if (state.IsLeaseReleaseRequested())
		{
			outcome = AdvanceLeaseCleanup(
				trip,
				fleet,
				BuildCausationId(trip, "CLEANUP_GATE"));
		}
		else
		{
			outcome = ContinueTerminalClearanceWithoutRestore(
				trip,
				fleet,
				BuildCausationId(trip, "TERMINAL_CLEARANCE"));
		}
		AuditTerminalPostconditions(trip, currentAssignment, slot);
		return outcome;
	}

	protected AICF_TripOutcome ContinueTerminalClearanceWithoutRestore(
		AICF_TransportTrip trip,
		AICF_FactionFleet fleet,
		string causationId)
	{
		if (!trip.GetLease())
		{
			trip.GetHandoffState().RecordClearanceResult(true);
			return AICF_TripOutcome.Wait("NO_LEASE_TO_CLEAR", causationId);
		}
		if (trip.GetHandoffState().IsLeaseReleaseRequested())
			return AdvanceLeaseCleanup(trip, fleet, causationId);
		if (!trip.GetLease().HasPhysicalAsset())
		{
			if (!ReleaseEmptyReservationIfPresent(trip, fleet))
				return AICF_TripOutcome.TerminalFailClosed("TERMINAL_LEASE_NOT_RELEASABLE", causationId);
			trip.GetHandoffState().RecordClearanceResult(true);
			return AICF_TripOutcome.Wait("EMPTY_RESERVATION_RELEASED", causationId);
		}
		AICF_VehicleDismountState dismountState = trip.GetDismountState();
		if (dismountState.GetStartedAtMs() <= 0)
			return BeginTerminalClearanceIfRequired(trip, causationId);
		AICF_TripOutcome outcome = m_DismountFlow.ProcessTerminalClearance(trip, causationId);
		if (outcome && outcome.GetKind() == AICF_ETripOutcomeKind.RELEASE_LEASE)
			return ForwardLeaseRelease(trip, outcome);
		return outcome;
	}

	protected AICF_TripOutcome BeginTerminalClearanceIfRequired(
		AICF_TransportTrip trip,
		string causationId)
	{
		if (!trip.GetLease())
		{
			trip.GetHandoffState().RecordClearanceResult(true);
			return AICF_TripOutcome.Wait("NO_TERMINAL_CLEARANCE_REQUIRED", causationId);
		}
		if (!trip.GetLease().HasPhysicalAsset())
			return AICF_TripOutcome.Wait("TERMINAL_ASSET_IDENTITY_PENDING", causationId);
		if (trip.GetDismountState().GetStartedAtMs() > 0)
			return AICF_TripOutcome.Wait("TERMINAL_CLEARANCE_ALREADY_STARTED", causationId);
		return m_DismountFlow.BeginTerminalClearance(trip, causationId);
	}

	protected AICF_TripOutcome ForwardLeaseRelease(
		AICF_TransportTrip trip,
		AICF_TripOutcome outcome)
	{
		// Managed clearance only requests the independent full cleanup scan.
		// No Fleet.ReleaseLease bypass and no clearance_safe claim are permitted.
		trip.GetHandoffState().RequestLeaseRelease();
		return AICF_TripOutcome.Wait(
			"LEASE_RELEASE_PENDING_CLEANUP_GATE:" + outcome.GetReason(),
			outcome.GetCausationId());
	}

	// Called only after a terminal transition has already been observable for a
	// full scheduler turn. Queue acceptance transfers cleanup responsibility,
	// but the Trip retains its immutable lease identity until release completes.
	protected AICF_TripOutcome AdvanceLeaseCleanup(
		AICF_TransportTrip trip,
		AICF_FactionFleet fleet,
		string causationId)
	{
		if (!trip || !trip.IsTerminal() || !fleet)
			return AICF_TripOutcome.TerminalFailClosed("CLEANUP_CONTEXT_INVALID", causationId);
		AICF_VehicleHandoffState state = trip.GetHandoffState();
		AICF_VehicleLease lease = trip.GetLease();
		if (!lease)
		{
			if (state.IsCleanupOwnershipAcceptedTerminal())
				return AICF_TripOutcome.Wait("CLEANUP_RETAINED_OWNERSHIP_COMPLETE", causationId);
			if (!state.IsCleanupReleaseComplete())
				return AICF_TripOutcome.TerminalFailClosed("CLEANUP_LEASE_IDENTITY_LOST", causationId);
			return AICF_TripOutcome.Wait("CLEANUP_RELEASE_ALREADY_COMPLETE", causationId);
		}
		if (!m_CleanupManager)
			return AICF_TripOutcome.Wait("CLEANUP_MANAGER_NOT_READY", causationId);

		if (!state.IsCleanupQueueAttempted())
		{
			if (!state.BeginCleanupQueueAttempt())
				return AICF_TripOutcome.TerminalFailClosed("CLEANUP_QUEUE_FENCE_REJECTED", causationId);
			AICF_VehicleCleanupOutcome queued = m_CleanupManager.QueueLeaseRelease(
				trip,
				fleet,
				lease,
				ResolveCleanupDisposition(lease),
				BuildCleanupTrigger(trip),
				causationId,
				System.GetTickCount());
			if (!queued)
			{
				state.RecordCleanupQueueResult(false, false, "NONE", "CLEANUP_OUTCOME_MISSING");
				return AICF_TripOutcome.TerminalFailClosed("CLEANUP_OUTCOME_MISSING", causationId);
			}
			string failureReason;
			if (!queued.IsAccepted())
				failureReason = queued.GetReason();
			state.RecordCleanupQueueResult(
				queued.IsAccepted(),
				queued.IsReleaseComplete(),
				queued.GetActionToken(),
				failureReason);
			if (!queued.IsAccepted())
			{
				state.RecordCleanupObservation(
					false,
					false,
					queued.IsTerminalFailure(),
					queued.GetReason());
				return AICF_TripOutcome.TerminalFailClosed(
					"CLEANUP_QUEUE_REJECTED:" + queued.GetReason(),
					causationId);
			}
		}

		AICF_VehicleCleanupQuery query = m_CleanupManager.QueryLease(lease);
		if (!query || !query.IsTracked())
		{
			state.RecordCleanupObservation(
				false,
				false,
				true,
				"CLEANUP_JOB_NOT_TRACKED");
			return AICF_TripOutcome.TerminalFailClosed("CLEANUP_JOB_NOT_TRACKED", causationId);
		}
		state.RecordCleanupObservation(
			query.IsClearanceSafe(),
			query.IsReleaseComplete(),
			query.IsRetainedFailClosed(),
			ResolveCleanupFailure(query));
		if (query.IsRetainedFailClosed() && !query.IsReleaseComplete())
		{
			// CleanupManager keeps the exact lease/job/fence after fail-closed.  Drop
			// only the Trip reference so the terminal scheduler cannot wait forever;
			// Fleet continues counting the FAILED_CLOSED lease and no replacement is
			// admitted for this slot.
			if (!m_CleanupManager.AcknowledgeRetainedLease(
				lease,
				query.GetActionToken()))
			{
				return AICF_TripOutcome.TerminalFailClosed(
					"CLEANUP_RETAINED_OWNERSHIP_ACK_REJECTED",
					causationId);
			}
			state.RecordCleanupOwnershipAcceptedTerminal();
			if (!trip.DetachLease(lease))
			{
				return AICF_TripOutcome.TerminalFailClosed(
					"CLEANUP_RETAINED_LEASE_TRANSFER_REJECTED",
					causationId);
			}
			return AICF_TripOutcome.TerminalFailClosed(
				"CLEANUP_RETAINED_FAIL_CLOSED:" + query.GetBlockerSignature(),
				causationId);
		}
		if (!query.IsReleaseComplete())
		{
			return AICF_TripOutcome.Wait(
				"LEASE_RELEASE_PENDING_CLEANUP_GATE:" + query.GetNextAction(),
				causationId);
		}
		if (!query.IsClearanceSafe())
			return AICF_TripOutcome.TerminalFailClosed("CLEANUP_COMPLETE_WITHOUT_CLEARANCE", causationId);
		if (!trip.DetachLease(lease))
			return AICF_TripOutcome.TerminalFailClosed("CLEANUP_LEASE_DETACH_REJECTED", causationId);
		if (!m_CleanupManager.AcknowledgeLeaseRelease(lease, query.GetActionToken()))
		{
			state.RecordCleanupObservation(
				true,
				true,
				true,
				"CLEANUP_ACKNOWLEDGEMENT_REJECTED");
			return AICF_TripOutcome.TerminalFailClosed(
				"CLEANUP_ACKNOWLEDGEMENT_REJECTED",
				causationId);
		}
		return AICF_TripOutcome.Wait("LEASE_RELEASE_COMPLETE", causationId);
	}

	protected string ResolveCleanupFailure(AICF_VehicleCleanupQuery query)
	{
		if (!query || !query.IsRetainedFailClosed())
			return string.Empty;
		if (!query.GetBlockerSignature().IsEmpty())
			return query.GetBlockerSignature();
		return "CLEANUP_RETAINED_FAIL_CLOSED";
	}

	protected AICF_EVehicleReleaseDisposition ResolveCleanupDisposition(
		AICF_VehicleLease lease)
	{
		if (!lease || !lease.HasPhysicalAsset())
			return AICF_EVehicleReleaseDisposition.DESTRUCTIVE_RETIREMENT;
		Vehicle vehicle = lease.GetVehicle();
		if (!vehicle || vehicle.GetID() != lease.GetEntityId())
			return AICF_EVehicleReleaseDisposition.DESTRUCTIVE_RETIREMENT;
		SCR_AIVehicleUsageComponent usage = SCR_AIVehicleUsageComponent.Cast(
			vehicle.FindComponent(SCR_AIVehicleUsageComponent));
		if (!usage || usage.GetDamageState() == EDamageState.DESTROYED ||
			SCR_AIVehicleUsability.VehicleIsOnFire(vehicle) ||
			!SCR_AIVehicleUsability.VehicleCanMove(vehicle))
		{
			return AICF_EVehicleReleaseDisposition.DESTRUCTIVE_RETIREMENT;
		}
		vector transform[4];
		vehicle.GetWorldTransform(transform);
		if (transform[1][1] < 0.25)
			return AICF_EVehicleReleaseDisposition.DESTRUCTIVE_RETIREMENT;
		return AICF_EVehicleReleaseDisposition.FUNCTIONAL_WORLD_POOL;
	}

	protected string BuildCleanupTrigger(AICF_TransportTrip trip)
	{
		return "TRIP_TERMINAL_" + typename.EnumToString(
			AICF_ETransportTripPhase,
			trip.GetPhase());
	}

	protected void AuditTerminalPostconditions(
		AICF_TransportTrip trip,
		AICF_StrategicAssignmentSnapshot currentAssignment,
		AICF_GroupSlot slot)
	{
		if (!trip || !trip.IsTerminal() || !m_Diagnostics)
			return;
		int nowMs = System.GetTickCount();
		AICF_VehicleHandoffState handoffState = trip.GetHandoffState();
		AICF_VehicleDismountState dismountState = trip.GetDismountState();
		bool groupCurrent = IsAuditGroupCurrent(trip, currentAssignment, slot);
		bool restorePending = !handoffState.IsOrderRestored();
		bool infantryWaypoint = handoffState.IsOrderRestored();
		bool meaningfulTask = groupCurrent && infantryWaypoint &&
			handoffState.HasMeaningfulTask();
		bool vehicleWaypoint = HasStagedVehicleWaypoint(trip);

		string nextAction = ResolveTerminalNextAction(handoffState, trip.GetLease());
		int cleanupDueMs;
		string cleanupSamples = "NONE";
		AICF_VehicleCleanupQuery cleanupQuery;
		if (m_CleanupManager && trip.GetLease() && handoffState.IsCleanupQueueAccepted())
			cleanupQuery = m_CleanupManager.QueryLease(trip.GetLease());
		if (cleanupQuery && cleanupQuery.IsTracked())
		{
			nextAction = cleanupQuery.GetNextAction();
			if (cleanupQuery.IsReleasePending())
			{
				cleanupDueMs = Math.Max(
					0,
					CLEANUP_STABLE_CLEAR_MS - cleanupQuery.GetStableClearMs());
			}
			cleanupSamples = string.Format(
				"blocker=%1 action_token=%2 delete_attempts=%3",
				cleanupQuery.GetBlockerSignature(),
				cleanupQuery.GetActionToken(),
				cleanupQuery.GetDeleteAttempts());
		}
		if (restorePending)
			nextAction = "RESTORE_INFANTRY_ORDER";

		string terminalState = typename.EnumToString(
			AICF_ETransportTripPhase,
			trip.GetPhase());
		string signature = string.Format(
			"%1:%2:%3:%4:%5:%6",
			terminalState,
			restorePending,
			meaningfulTask,
			dismountState.GetLogicalOccupants(),
			dismountState.GetTransitions(),
			dismountState.GetInsideBounds());
		signature += string.Format(
			":%1:%2:%3:%4:%5",
			infantryWaypoint,
			vehicleWaypoint,
			handoffState.IsCleanupQueueAccepted(),
			handoffState.IsCleanupReleaseComplete(),
			nextAction);
		if (!handoffState.ShouldAuditAbandonedExit(
			signature,
			nowMs,
			TERMINAL_AUDIT_INTERVAL_MS))
		{
			return;
		}

		int pendingAgeMs;
		if (restorePending)
			pendingAgeMs = Math.Max(0, nowMs - handoffState.GetStartedAtMs());
		SCR_AIGroup group = trip.GetAssignment().GetGroup();
		AICF_VehicleTerminalClearanceObservation observation =
			new AICF_VehicleTerminalClearanceObservation(
				AICF_GroupRuntime.CountAliveAgents(group),
				dismountState.GetLogicalOccupants(),
				dismountState.GetTransitions(),
				dismountState.GetInsideBounds(),
				dismountState.GetForceClearanceAttempts(),
				dismountState.GetGuidanceAttempts(),
				nextAction,
				cleanupSamples);
		m_Diagnostics.AuditAbandonedExit(
			trip,
			terminalState,
			Math.Max(0, nowMs - trip.GetPhaseStartedAtMs()),
			pendingAgeMs,
			groupCurrent,
			restorePending,
			meaningfulTask,
			infantryWaypoint,
			vehicleWaypoint,
			cleanupDueMs,
			observation,
			trip.GetCausationId());
	}

	protected bool IsAuditGroupCurrent(
		AICF_TransportTrip trip,
		AICF_StrategicAssignmentSnapshot currentAssignment,
		AICF_GroupSlot slot)
	{
		if (!trip || !currentAssignment || !slot || !trip.IsCurrent(currentAssignment))
			return false;
		return slot.GetSlotId() == trip.GetSlotId() &&
			slot.GetSpawnGeneration() == trip.GetGroupGeneration() &&
			slot.GetGroup() == trip.GetAssignment().GetGroup();
	}

	protected bool HasStagedVehicleWaypoint(AICF_TransportTrip trip)
	{
		if (!trip)
			return false;
		AICF_VehicleMovementState movementState = trip.GetMovementState();
		if (movementState.GetRouteWaypoint() || movementState.GetSupersededRouteWaypoint())
			return true;
		AICF_VehicleDismountState dismountState = trip.GetDismountState();
		return dismountState.GetDismountWaypoint() ||
			dismountState.GetSupersededDismountWaypoint();
	}

	protected string ResolveTerminalNextAction(
		AICF_VehicleHandoffState state,
		AICF_VehicleLease lease)
	{
		if (!lease)
			return "CLEANUP_COMPLETE";
		if (!state.IsLeaseReleaseRequested())
			return "WAIT_MANAGED_CLEARANCE";
		if (!m_CleanupManager)
			return "WAIT_CLEANUP_MANAGER";
		if (!state.IsCleanupQueueAttempted())
			return "QUEUE_FULL_CLEARANCE";
		if (state.IsCleanupRetainedFailClosed())
			return "RETAIN_FAIL_CLOSED";
		if (state.IsCleanupReleaseComplete())
			return "CLEANUP_COMPLETE";
		return "WAIT_PROTECTED_CLEARANCE";
	}

	protected AICF_TripOutcome ApplyCurrentRetarget(
		AICF_TransportTrip trip,
		AICF_StrategicAssignmentSnapshot currentAssignment,
		AICF_GroupSlot slot,
		AICF_FactionFleet fleet,
		SCR_CampaignFaction faction)
	{
		if (!HasAssignmentChange(trip, currentAssignment))
			return null;
		string causationId = BuildCausationId(trip, "SAFE_REUSE_RETARGET");
		if (trip.GetPhase() == AICF_ETransportTripPhase.TRANSIT)
		{
			string vehicleFailure = m_TransitFlow.InspectRouteAssetFailure(trip);
			if (!vehicleFailure.IsEmpty())
			{
				return EndVehicleControl(
					trip, slot, fleet, faction,
					AICF_ETransportTripPhase.FALLBACK,
					vehicleFailure,
					causationId, true);
			}
		}
		if (!CommitRetarget(trip, currentAssignment, causationId))
		{
			return EndVehicleControl(
				trip, slot, fleet, faction,
				AICF_ETransportTripPhase.FAILED_CLOSED,
				"SAFE_REUSE_RETARGET_COMMIT_REJECTED",
				causationId, true);
		}
		if ((trip.GetPhase() == AICF_ETransportTripPhase.BOARDING ||
			trip.GetPhase() == AICF_ETransportTripPhase.TRANSIT ||
			trip.GetPhase() == AICF_ETransportTripPhase.DISMOUNT) &&
			!m_Handoff.SuspendInfantryOrder(trip, slot, "SAFE_REUSE_RETARGET"))
		{
			return EndVehicleControl(
				trip, slot, fleet, faction,
				AICF_ETransportTripPhase.FAILED_CLOSED,
				"RETARGET_STRATEGIC_WAYPOINT_SUSPEND_REJECTED",
				causationId, true);
		}
		if (trip.GetPhase() != AICF_ETransportTripPhase.TRANSIT)
			return null;
		AICF_TripOutcome routeOutcome = m_TransitFlow.PrepareRoute(trip, causationId);
		return ApplyOutcome(trip, routeOutcome, currentAssignment, slot, fleet, faction);
	}

	protected bool CommitRetarget(
		AICF_TransportTrip trip,
		AICF_StrategicAssignmentSnapshot currentAssignment,
		string causationId)
	{
		AICF_VehicleLease expectedLease = trip.GetLease();
		int expectedVehicleGeneration = -1;
		string expectedLifecycleId = "NONE";
		if (expectedLease)
		{
			expectedVehicleGeneration = expectedLease.GetVehicleGeneration();
			expectedLifecycleId = expectedLease.GetVehicleLifecycleId();
		}
		if (!trip.CommitRetarget(currentAssignment, causationId))
			return false;
		if (trip.GetLease() != expectedLease)
			return false;
		if (expectedLease && (expectedVehicleGeneration != expectedLease.GetVehicleGeneration() ||
			expectedLifecycleId != expectedLease.GetVehicleLifecycleId()))
		{
			return false;
		}
		if (expectedLease && expectedLease.HasPhysicalAsset())
		{
			AICF_Stage3Diagnostics.Info(
				"VEHICLE_REUSED",
				FormatIdentity(trip, causationId, "SAFE_REUSE_RETARGET") + string.Format(
					" assignment_revision=%1 base_revision=%2 target=%3",
					currentAssignment.GetAssignmentRevision(),
					currentAssignment.GetBaseRevision(),
					AICF_Stage1Diagnostics.BaseKey(currentAssignment.GetTargetBase())));
		}
		return true;
	}

	protected bool TransitionTo(
		AICF_TransportTrip trip,
		AICF_ETransportTripPhase nextPhase,
		string reason,
		string causationId)
	{
		if (!trip || reason.IsEmpty() || causationId.IsEmpty() ||
			!trip.CanTransitionTo(nextPhase))
			return false;
		AICF_ETransportTripPhase previousPhase = trip.GetPhase();
		if (!ExitPhaseEffects(trip, previousPhase, reason))
			return false;
		if (!trip.CommitTransition(nextPhase, reason, causationId, System.GetTickCount()))
			return false;
		m_Diagnostics.CommittedVehicleTransition(
			trip,
			previousPhase,
			nextPhase,
			reason,
			causationId);
		if (nextPhase == AICF_ETransportTripPhase.FALLBACK)
		{
			m_Diagnostics.CommittedVehicleAbandoned(
				trip,
				previousPhase,
				reason,
				causationId);
		}
		if ((nextPhase == AICF_ETransportTripPhase.FALLBACK ||
			nextPhase == AICF_ETransportTripPhase.FAILED_CLOSED) &&
			reason.Contains("DESTROYED"))
		{
			m_Diagnostics.CommittedVehicleDestroyed(
				trip,
				previousPhase,
				nextPhase,
				reason,
				causationId);
		}
		return true;
	}

	protected bool ExitPhaseEffects(
		AICF_TransportTrip trip,
		AICF_ETransportTripPhase phase,
		string reason)
	{
		switch (phase)
		{
			case AICF_ETransportTripPhase.BOARDING:
				m_BoardingFlow.Exit(trip, reason);
				return trip.GetBoardingState().AreExitEffectsApplied();
			case AICF_ETransportTripPhase.TRANSIT:
				return ExitTransit(trip, reason);
			case AICF_ETransportTripPhase.DISMOUNT:
				return ExitDismount(trip, reason);
		}
		return true;
	}

	protected bool ExitTransit(AICF_TransportTrip trip, string reason)
	{
		m_TransitFlow.AbortOwnedActions(trip);
		bool removed = RemoveTransitWaypoints(trip, "PHASE_EXIT", reason);
		m_Handoff.DetachVehicleUtility(trip);
		return removed;
	}

	protected bool ExitDismount(AICF_TransportTrip trip, string reason)
	{
		m_DismountFlow.Exit(trip, reason);
		return RemoveDismountWaypoints(trip, "PHASE_EXIT", reason);
	}

	protected bool ReconcileTransitWaypoints(AICF_TransportTrip trip, string reason)
	{
		AIWaypoint superseded = m_TransitFlow.GetSupersededRouteWaypoint(trip);
		if (superseded)
		{
			if (!ReleaseSupersededTransitWaypoint(
				trip,
				superseded,
				"SAFE_REUSE",
				reason))
			{
				return false;
			}
			// Queue removal and binding are deliberately split across ticks. This
			// also leaves an active exact-crew recovery free to stage its successor
			// route before reconciliation requires a current waypoint.
			return true;
		}
		AIWaypoint current = m_TransitFlow.GetRouteWaypoint(trip);
		if (!current)
			return true;
		bool bound = m_Handoff.BindVehicleWaypoint(trip, current, "ROAD_ROUTE");
		return m_TransitFlow.ConfirmRouteWaypointBound(trip, current, bound);
	}

	protected bool ReconcileDismountWaypoints(AICF_TransportTrip trip, string reason)
	{
		AIWaypoint superseded = m_DismountFlow.GetSupersededDismountWaypoint(trip);
		if (superseded)
		{
			if (!ReleaseSupersededDismountWaypoint(
				trip,
				superseded,
				"NORMAL_REISSUE",
				reason))
			{
				return false;
			}
			return true;
		}
		AIWaypoint current = m_DismountFlow.GetDismountWaypoint(trip);
		if (!current)
			return true;
		bool bound = m_Handoff.BindVehicleWaypoint(trip, current, "GET_OUT");
		return m_DismountFlow.ConfirmDismountWaypointBound(trip, current, bound);
	}

	protected bool RemoveTransitWaypoints(
		AICF_TransportTrip trip,
		string trigger,
		string reason)
	{
		AIWaypoint superseded = m_TransitFlow.GetSupersededRouteWaypoint(trip);
		if (superseded)
		{
			if (!ReleaseSupersededTransitWaypoint(
				trip,
				superseded,
				trigger,
				reason))
			{
				return false;
			}
		}
		AIWaypoint current = m_TransitFlow.GetRouteWaypoint(trip);
		if (!current)
			return true;
		return ReleaseCurrentTransitWaypoint(trip, current, trigger, reason);
	}

	protected bool ReleaseSupersededTransitWaypoint(
		AICF_TransportTrip trip,
		AIWaypoint waypoint,
		string trigger,
		string reason)
	{
		if (!m_Handoff.DetachVehicleWaypoint(trip, waypoint))
			return false;
		if (!m_TransitFlow.ConfirmSupersededWaypointRemoved(trip, waypoint))
			return false;
		m_Handoff.DeleteDetachedVehicleWaypoint(trip, waypoint, trigger, reason);
		return true;
	}

	protected bool ReleaseCurrentTransitWaypoint(
		AICF_TransportTrip trip,
		AIWaypoint waypoint,
		string trigger,
		string reason)
	{
		if (!m_Handoff.DetachVehicleWaypoint(trip, waypoint))
			return false;
		if (!m_TransitFlow.ConfirmRouteWaypointRemoved(trip, waypoint))
			return false;
		m_Handoff.DeleteDetachedVehicleWaypoint(trip, waypoint, trigger, reason);
		return true;
	}

	protected bool RemoveDismountWaypoints(
		AICF_TransportTrip trip,
		string trigger,
		string reason)
	{
		AIWaypoint superseded = m_DismountFlow.GetSupersededDismountWaypoint(trip);
		if (superseded)
		{
			if (!ReleaseSupersededDismountWaypoint(
				trip,
				superseded,
				trigger,
				reason))
			{
				return false;
			}
		}
		AIWaypoint current = m_DismountFlow.GetDismountWaypoint(trip);
		if (!current)
			return true;
		return ReleaseCurrentDismountWaypoint(trip, current, trigger, reason);
	}

	protected bool ReleaseSupersededDismountWaypoint(
		AICF_TransportTrip trip,
		AIWaypoint waypoint,
		string trigger,
		string reason)
	{
		if (!m_Handoff.DetachVehicleWaypoint(trip, waypoint))
			return false;
		if (!m_DismountFlow.ConfirmSupersededWaypointRemoved(trip, waypoint))
			return false;
		m_Handoff.DeleteDetachedVehicleWaypoint(trip, waypoint, trigger, reason);
		return true;
	}

	protected bool ReleaseCurrentDismountWaypoint(
		AICF_TransportTrip trip,
		AIWaypoint waypoint,
		string trigger,
		string reason)
	{
		if (!m_Handoff.DetachVehicleWaypoint(trip, waypoint))
			return false;
		if (!m_DismountFlow.ConfirmDismountWaypointRemoved(trip, waypoint))
			return false;
		m_Handoff.DeleteDetachedVehicleWaypoint(trip, waypoint, trigger, reason);
		return true;
	}

	protected bool ReleaseEmptyReservationIfPresent(
		AICF_TransportTrip trip,
		AICF_FactionFleet fleet)
	{
		AICF_VehicleLease lease = trip.GetLease();
		if (!lease)
			return true;
		if (lease.HasPhysicalAsset())
			return true;
		if (lease.GetState() != AICF_EVehicleLeaseState.RESERVED || !fleet)
			return false;
		// Fleet commits cancellation first. Only then may the controller clear
		// the Trip reference; a Fleet failure therefore cannot orphan cap state.
		if (!fleet.ReleaseEmptyReservation(lease))
			return false;
		return trip.DetachLease(lease);
	}

	protected bool AttachReservedLease(
		AICF_TransportTrip trip,
		AICF_FactionFleet fleet)
	{
		if (!trip || trip.GetLease() || !fleet)
			return false;
		AICF_VehicleLease lease = fleet.FindLeaseForSlot(
			trip.GetSlotId(),
			trip.GetGroupGeneration());
		if (!lease || lease.GetTripGeneration() != trip.GetTripGeneration() ||
			lease.GetState() != AICF_EVehicleLeaseState.RESERVED)
		{
			return false;
		}
		if (trip.TryAttachLease(lease))
			return true;
		fleet.ReleaseEmptyReservation(lease);
		return false;
	}

	protected void BeginHandoffEvidence(AICF_TransportTrip trip, bool clearanceSafe)
	{
		AICF_VehicleHandoffState state = trip.GetHandoffState();
		EnsureHandoffStarted(state, System.GetTickCount());
		state.RecordClearanceResult(clearanceSafe);
	}

	protected void PrepareStopHandoffEvidence(AICF_TransportTrip trip)
	{
		AICF_VehicleHandoffState state = trip.GetHandoffState();
		EnsureHandoffStarted(state, System.GetTickCount());
		if (!trip.GetLease())
		{
			state.RecordClearanceResult(true);
		}
	}

	protected bool CancelResidualOwnedEffectsForStop(
		AICF_TransportTrip trip,
		string reason)
	{
		if (!trip)
			return false;
		bool cancelled = true;
		AICF_VehicleBoardingState boardingState = trip.GetBoardingState();
		if (boardingState && boardingState.GetStartedAtMs() > 0)
		{
			m_BoardingFlow.Exit(trip, reason);
			if (!boardingState.AreExitEffectsApplied())
				cancelled = false;
		}
		m_TransitFlow.AbortOwnedActions(trip);
		if (!RemoveTransitWaypoints(trip, "COORDINATOR_STOP", reason))
			cancelled = false;
		m_DismountFlow.Exit(trip, reason);
		if (!RemoveDismountWaypoints(trip, "COORDINATOR_STOP", reason))
			cancelled = false;
		m_Handoff.DetachVehicleUtility(trip);
		return cancelled;
	}

	protected void EnsureHandoffStarted(AICF_VehicleHandoffState state, int nowMs)
	{
		if (state.GetStartedAtMs() > 0)
			return;
		state.Begin(
			nowMs,
			nowMs + ORDER_RESTORE_DEADLINE_MS,
			ORDER_RESTORE_MAX_ATTEMPTS);
	}

	protected bool IsAuthorityReady()
	{
		return Replication.IsServer() && m_Campaign && m_Campaign.IsMaster() &&
			m_ConflictAdapter && m_AcquisitionFlow && m_BoardingFlow &&
			m_TransitFlow && m_DismountFlow && m_Handoff;
	}

	protected bool HasCurrentIdentity(
		AICF_TransportTrip trip,
		AICF_StrategicAssignmentSnapshot currentAssignment,
		AICF_GroupSlot slot,
		AICF_FactionFleet fleet,
		SCR_CampaignFaction faction)
	{
		if (!trip || !currentAssignment || !currentAssignment.IsValid() ||
			!slot || !fleet || !faction || !trip.IsCurrent(currentAssignment))
		{
			return false;
		}
		if (trip.GetFactionKey() != fleet.GetFactionKey() ||
			trip.GetFactionKey() != faction.GetFactionKey())
		{
			return false;
		}
		return slot.GetSlotId() == currentAssignment.GetSlotId() &&
			slot.GetSlotKey() == currentAssignment.GetSlotKey() &&
			slot.GetSpawnGeneration() == currentAssignment.GetGroupGeneration() &&
			slot.GetGroup() == currentAssignment.GetGroup() &&
			slot.GetStrategicAssignmentRevision() == currentAssignment.GetAssignmentRevision();
	}

	protected bool HasStaleRevision(
		AICF_TransportTrip trip,
		AICF_StrategicAssignmentSnapshot currentAssignment)
	{
		return currentAssignment.GetAssignmentRevision() <
			trip.GetAssignment().GetAssignmentRevision() ||
			currentAssignment.GetBaseRevision() < trip.GetAssignment().GetBaseRevision();
	}

	protected bool HasAssignmentChange(
		AICF_TransportTrip trip,
		AICF_StrategicAssignmentSnapshot currentAssignment)
	{
		AICF_StrategicAssignmentSnapshot observed = trip.GetAssignment();
		return currentAssignment.GetAssignmentRevision() != observed.GetAssignmentRevision() ||
			currentAssignment.GetBaseRevision() != observed.GetBaseRevision() ||
			currentAssignment.GetTargetBase() != observed.GetTargetBase();
	}

	protected bool IsAcquisitionPhase(AICF_ETransportTripPhase phase)
	{
		return phase == AICF_ETransportTripPhase.WAITING_FOR_SITE ||
			phase == AICF_ETransportTripPhase.ACQUIRING;
	}

	protected string ResolveCausationId(
		AICF_TransportTrip trip,
		AICF_TripOutcome outcome,
		string suffix)
	{
		if (outcome && !outcome.GetCausationId().IsEmpty())
			return outcome.GetCausationId();
		return BuildCausationId(trip, suffix);
	}

	protected string BuildCausationId(AICF_TransportTrip trip, string suffix)
	{
		if (!trip)
			return "controller-no-trip-" + suffix;
		return string.Format(
			"%1-%2-%3",
			trip.GetOperationId(),
			suffix,
			trip.GetTransitionCount() + 1);
	}

	protected string FormatIdentity(
		AICF_TransportTrip trip,
		string causationId,
		string reason)
	{
		string details = string.Format(
			"faction=%1 slot=%2 group_generation=%3 trip_generation=%4 operation_id=%5",
			trip.GetFactionKey(),
			trip.GetSlotKey(),
			trip.GetGroupGeneration(),
			trip.GetTripGeneration(),
			trip.GetOperationId());
		details += string.Format(
			" causation_id=%1 reason=%2",
			causationId,
			reason);
		AICF_VehicleLease lease = trip.GetLease();
		if (!lease)
		{
			return details +
				" lease_generation=-1 vehicle_generation=-1 vehicle_lifecycle_id=NONE" +
				" vehicle=NONE kind=NONE state=" + typename.EnumToString(
					AICF_ETransportTripPhase,
					trip.GetPhase());
		}
		details += string.Format(
			" lease_generation=%1 vehicle_generation=%2 vehicle_lifecycle_id=%3",
			lease.GetLeaseGeneration(),
			lease.GetVehicleGeneration(),
			lease.GetVehicleLifecycleId());
		string vehicleId = lease.GetEntityIdString();
		if (vehicleId.IsEmpty())
			vehicleId = "NONE";
		string vehicleKind = "NONE";
		if (lease.GetVehicleGeneration() > 0)
			vehicleKind = typename.EnumToString(AICF_EVehicleKind, lease.GetKind());
		details += string.Format(
			" vehicle=%1 kind=%2 state=%3",
			vehicleId,
			vehicleKind,
			typename.EnumToString(AICF_ETransportTripPhase, trip.GetPhase()));
		return details;
	}
}
