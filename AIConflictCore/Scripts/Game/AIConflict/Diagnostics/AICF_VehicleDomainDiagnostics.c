// Immutable caller-observed terminal sample. It does not own or mutate the
// corresponding dismount/handoff state.
class AICF_VehicleTerminalClearanceObservation
{
	protected int m_iAlive;
	protected int m_iLogicalOccupants;
	protected int m_iTransitions;
	protected int m_iInsideBounds;
	protected int m_iForceAttempts;
	protected int m_iClearanceAttempts;
	protected string m_sNextAction;
	protected string m_sSamples;

	void AICF_VehicleTerminalClearanceObservation(
		int alive,
		int logicalOccupants,
		int transitions,
		int insideBounds,
		int forceAttempts,
		int clearanceAttempts,
		string nextAction,
		string samples)
	{
		m_iAlive = alive;
		m_iLogicalOccupants = logicalOccupants;
		m_iTransitions = transitions;
		m_iInsideBounds = insideBounds;
		m_iForceAttempts = forceAttempts;
		m_iClearanceAttempts = clearanceAttempts;
		m_sNextAction = nextAction;
		m_sSamples = samples;
	}

	int GetAlive() { return m_iAlive; }
	int GetLogicalOccupants() { return m_iLogicalOccupants; }
	int GetTransitions() { return m_iTransitions; }
	int GetInsideBounds() { return m_iInsideBounds; }
	int GetForceAttempts() { return m_iForceAttempts; }
	int GetClearanceAttempts() { return m_iClearanceAttempts; }
	string GetNextAction() { return m_sNextAction; }
	string GetSamples() { return m_sSamples; }
}

// Observation-only vehicle-domain formatter. It owns no lifecycle state and
// never decides a transition, order, clearance or cleanup action. Committed*
// methods are called exactly once by the transition owner after commit. Audit*
// methods are called only after the owning state reports an on-change or
// rate-limited audit deadline; this deliberately contains no second timer.
class AICF_VehicleDomainDiagnostics
{
	void CommittedVehicleTransition(
		AICF_TransportTrip trip,
		AICF_ETransportTripPhase previousPhase,
		AICF_ETransportTripPhase nextPhase,
		string reason,
		string causationId)
	{
		string details = FormatTripIdentity(trip, causationId, reason);
		details += string.Format(
			" previous_state=%1 state=%2 transition_count=%3 committed=1",
			typename.EnumToString(AICF_ETransportTripPhase, previousPhase),
			typename.EnumToString(AICF_ETransportTripPhase, nextPhase),
			trip.GetTransitionCount());
		details += string.Format(
			" trip_started_at_ms=%1 phase_started_at_ms=%2 absolute_deadline_ms=%3",
			trip.GetStartedAtMs(),
			trip.GetPhaseStartedAtMs(),
			trip.GetAbsoluteDeadlineMs());
		AICF_Stage3Diagnostics.Info("VEHICLE_STATE_CHANGED", details);
	}

	void CommittedVehicleAbandoned(
		AICF_TransportTrip trip,
		AICF_ETransportTripPhase previousPhase,
		string reason,
		string causationId)
	{
		string details = FormatTripIdentity(trip, causationId, reason);
		details += string.Format(
			" previous_state=%1 terminal_state=FALLBACK committed=1 transition_count=%2",
			typename.EnumToString(AICF_ETransportTripPhase, previousPhase),
			trip.GetTransitionCount());
		AICF_Stage3Diagnostics.Warning("VEHICLE_ABANDONED", details);
	}

	void CommittedVehicleDestroyed(
		AICF_TransportTrip trip,
		AICF_ETransportTripPhase previousPhase,
		AICF_ETransportTripPhase terminalPhase,
		string reason,
		string causationId)
	{
		string details = FormatTripIdentity(trip, causationId, reason);
		details += string.Format(
			" previous_state=%1 terminal_state=%2 committed=1 transition_count=%3",
			typename.EnumToString(AICF_ETransportTripPhase, previousPhase),
			typename.EnumToString(AICF_ETransportTripPhase, terminalPhase),
			trip.GetTransitionCount());
		AICF_Stage3Diagnostics.Warning("VEHICLE_DESTROYED", details);
	}

	void MeaningfulTaskLost(
		AICF_StrategicAssignmentSnapshot assignment,
		string operationId,
		string causationId,
		int alive,
		string vehicleState,
		bool infantryWaypoint,
		bool vehicleWaypoint,
		int tasklessAgeMs,
		bool atMob,
		string lossTrigger,
		string lossReason)
	{
		string details = FormatAssignmentIdentity(assignment, operationId, causationId);
		details += FormatMeaningfulTaskObservation(
			assignment,
			alive,
			vehicleState,
			infantryWaypoint,
			vehicleWaypoint,
			tasklessAgeMs,
			atMob);
		details += string.Format(
			" loss_trigger=%1 loss_reason=%2 meaningful_task=0",
			lossTrigger,
			lossReason);
		AICF_Stage35Diagnostics.Warning("MEANINGFUL_TASK_LOST", details);
	}

	void MeaningfulTaskRecovered(
		AICF_StrategicAssignmentSnapshot assignment,
		string operationId,
		string causationId,
		int alive,
		string vehicleState,
		bool infantryWaypoint,
		bool vehicleWaypoint,
		int tasklessAgeMs,
		bool atMob,
		string recoveryTrigger)
	{
		string details = FormatAssignmentIdentity(assignment, operationId, causationId);
		details += FormatMeaningfulTaskObservation(
			assignment,
			alive,
			vehicleState,
			infantryWaypoint,
			vehicleWaypoint,
			tasklessAgeMs,
			atMob);
		details += string.Format(
			" recovery_trigger=%1 meaningful_task=1",
			recoveryTrigger);
		AICF_Stage35Diagnostics.Info("MEANINGFUL_TASK_RECOVERED", details);
	}

	void MeaningfulTaskDeadlineMissed(
		AICF_StrategicAssignmentSnapshot assignment,
		string operationId,
		string causationId,
		int alive,
		string vehicleState,
		bool infantryWaypoint,
		bool vehicleWaypoint,
		int tasklessAgeMs,
		int deadlineMs,
		bool atMob)
	{
		string details = FormatAssignmentIdentity(assignment, operationId, causationId);
		details += FormatMeaningfulTaskObservation(
			assignment,
			alive,
			vehicleState,
			infantryWaypoint,
			vehicleWaypoint,
			tasklessAgeMs,
			atMob);
		details += string.Format(
			" deadline_ms=%1 postcondition_meaningful_task=0",
			deadlineMs);
		AICF_Stage35Diagnostics.Error("MEANINGFUL_TASK_DEADLINE_MISSED", details);
	}

	void OrderRestoreRequested(
		AICF_TransportTrip trip,
		string trigger,
		string reason,
		AIWaypoint oldWaypoint,
		string target,
		string vehicleState,
		int tasklessAgeMs,
		int requestedAtMs,
		string causationId)
	{
		string details = FormatTripIdentity(trip, causationId, reason);
		details += string.Format(
			" trigger=%1 old_waypoint=%2 target=%3 vehicle_state=%4",
			trigger,
			WaypointId(oldWaypoint),
			target,
			vehicleState);
		details += string.Format(
			" taskless_age_ms=%1 requested_at_ms=%2 restore_pending=1",
			tasklessAgeMs,
			requestedAtMs);
		AICF_Stage35Diagnostics.Info("ORDER_RESTORE_REQUESTED", details);
	}

	// The successful API has no caller-supplied proof booleans. Reaching this
	// method is the handoff owner's assertion that all four strict proofs were
	// already observed on the exact waypoint in the exact group queue.
	void OrderRestoreSucceeded(
		AICF_TransportTrip trip,
		string trigger,
		string reason,
		AIWaypoint oldWaypoint,
		AIWaypoint newWaypoint,
		int queueCount,
		int latencyMs,
		string causationId)
	{
		string details = FormatTripIdentity(trip, causationId, reason);
		details += string.Format(
			" trigger=%1 success=1 old_waypoint=%2 new_waypoint=%3 queue_count=%4",
			trigger,
			WaypointId(oldWaypoint),
			WaypointId(newWaypoint),
			queueCount);
		details += " bound_to_group=1 is_current=1 waypoint_in_queue=1 postcondition_meaningful_task=1";
		details += string.Format(
			" failure_reason=NONE latency_ms=%1 restore_pending=0",
			latencyMs);
		AICF_Stage35Diagnostics.Info("ORDER_RESTORE_RESULT", details);
	}

	void OrderRestoreFailed(
		AICF_TransportTrip trip,
		string trigger,
		string reason,
		AIWaypoint oldWaypoint,
		AIWaypoint newWaypoint,
		bool boundToGroup,
		bool isCurrent,
		bool waypointInQueue,
		bool postconditionMeaningfulTask,
		int queueCount,
		string failureReason,
		int latencyMs,
		string causationId)
	{
		string details = FormatTripIdentity(trip, causationId, reason);
		details += string.Format(
			" trigger=%1 success=0 old_waypoint=%2 new_waypoint=%3 bound_to_group=%4 is_current=%5",
			trigger,
			WaypointId(oldWaypoint),
			WaypointId(newWaypoint),
			boundToGroup,
			isCurrent);
		details += string.Format(
			" queue_count=%1 waypoint_in_queue=%2 postcondition_meaningful_task=%3 failure_reason=%4 latency_ms=%5",
			queueCount,
			waypointInQueue,
			postconditionMeaningfulTask,
			failureReason,
			latencyMs);
		AICF_Stage35Diagnostics.Info("ORDER_RESTORE_RESULT", details);
	}

	void WaypointRemoved(
		AICF_TransportTrip trip,
		AIWaypoint waypoint,
		string waypointKind,
		string owner,
		string trigger,
		string reason,
		string target,
		string causationId)
	{
		string details = FormatTripIdentity(trip, causationId, reason);
		details += string.Format(
			" waypoint=%1 waypoint_kind=%2 owner=%3 remove_trigger=%4 remove_reason=%5 target=%6",
			WaypointId(waypoint),
			waypointKind,
			owner,
			trigger,
			reason,
			target);
		AICF_Stage35Diagnostics.Info("WAYPOINT_REMOVED", details);
	}

	void WaypointBindMismatch(
		AICF_TransportTrip trip,
		AIWaypoint waypoint,
		int queueCount,
		string owner,
		string reason,
		string causationId)
	{
		string details = FormatTripIdentity(trip, causationId, reason);
		details += string.Format(
			" waypoint=%1 queue_count=%2 owner=%3 bound_to_group=0 waypoint_in_queue=0",
			WaypointId(waypoint),
			queueCount,
			owner);
		AICF_Stage35Diagnostics.Warning("WAYPOINT_BIND_MISMATCH", details);
	}

	void AuditAbandonedExit(
		AICF_TransportTrip trip,
		string terminalState,
		int stateAgeMs,
		int pendingAgeMs,
		bool groupCurrent,
		bool restorePending,
		bool meaningfulTask,
		bool infantryWaypoint,
		bool vehicleWaypoint,
		int cleanupDueMs,
		AICF_VehicleTerminalClearanceObservation observation,
		string causationId)
	{
		string details = FormatTripIdentity(trip, causationId, "TERMINAL_HANDOFF_AUDIT");
		details += string.Format(
			" terminal_state=%1 state_age_ms=%2 pending_age_ms=%3 group_current=%4 alive=%5",
			terminalState,
			stateAgeMs,
			pendingAgeMs,
			groupCurrent,
			observation.GetAlive());
		details += string.Format(
			" logical_occupants=%1 transitions=%2 inside_bounds=%3 restore_pending=%4 meaningful_task=%5",
			observation.GetLogicalOccupants(),
			observation.GetTransitions(),
			observation.GetInsideBounds(),
			restorePending,
			meaningfulTask);
		details += string.Format(
			" infantry_waypoint=%1 vehicle_waypoint=%2 force_attempts=%3 clearance_attempts=%4 cleanup_due_ms=%5",
			infantryWaypoint,
			vehicleWaypoint,
			observation.GetForceAttempts(),
			observation.GetClearanceAttempts(),
			cleanupDueMs);
		details += string.Format(
			" next_action=%1 samples=[%2]",
			observation.GetNextAction(),
			observation.GetSamples());
		AICF_Stage35Diagnostics.Info("ABANDONED_EXIT_AUDIT", details);
	}

	void ForceDismountMember(
		AICF_TransportTrip trip,
		IEntity member,
		int compartmentManager,
		int compartmentSlot,
		bool exactOwnerValid,
		bool directAccepted,
		bool ejectRequested,
		bool ejectImmediate,
		bool linkedAfter,
		bool gettingIn,
		bool gettingOut,
		int attempt,
		int maximumAttempts,
		bool exactEscalation,
		string causationId)
	{
		string memberId = "NONE";
		if (member)
			memberId = member.GetID().ToString();
		string details = FormatTripIdentity(trip, causationId, "PROTECTED_MEMBER_FORCE_EXIT");
		details += string.Format(
			" member=%1 compartment_manager=%2 compartment_slot=%3 exact_owner_valid=%4",
			memberId,
			compartmentManager,
			compartmentSlot,
			exactOwnerValid);
		details += string.Format(
			" direct_accepted=%1 eject_requested=%2 eject_immediate=%3 linked_after=%4",
			directAccepted,
			ejectRequested,
			ejectImmediate,
			linkedAfter);
		details += string.Format(
			" getting_in=%1 getting_out=%2 attempt=%3 maximum_attempts=%4 exact_escalation=%5",
			gettingIn,
			gettingOut,
			attempt,
			maximumAttempts,
			exactEscalation);
		AICF_Stage35Diagnostics.Info("FORCE_DISEMBARK_MEMBER", details);
	}

	void IdleDeadlineSuppressed(
		AICF_StrategicAssignmentSnapshot assignment,
		string operationId,
		string causationId,
		string suppressionRule,
		bool suppressionActive,
		bool atMob,
		float distanceToMobMeters,
		bool meaningfulTask,
		string allowedReason,
		string vehicleState,
		int tasklessAgeMs)
	{
		string details = FormatAssignmentIdentity(assignment, operationId, causationId);
		details += string.Format(
			" scope=MOB suppression_rule=%1 suppression_active=%2 at_mob=%3 distance_to_mob_m=%4",
			suppressionRule,
			suppressionActive,
			atMob,
			distanceToMobMeters);
		details += string.Format(
			" meaningful_task=%1 allowed_idle_reason=%2 vehicle_state=%3 taskless_age_ms=%4",
			meaningfulTask,
			allowedReason,
			vehicleState,
			tasklessAgeMs);
		AICF_Stage35Diagnostics.Info("IDLE_DEADLINE_SUPPRESSED", details);
	}

	void CohesionOutcome(
		AICF_TransportTrip trip,
		string outcome,
		string waitReason,
		int waitAgeMs,
		int deadlineMs,
		int alive,
		float farthestFromLeaderMeters,
		float maximumPairMeters,
		float thresholdMeters,
		bool normalized,
		string orderOutcome,
		string memberSamples,
		string causationId)
	{
		string details = FormatTripIdentity(trip, causationId, "BOUNDED_COHESION_OBSERVATION");
		details += string.Format(
			" outcome=%1 wait_reason=%2 wait_age_ms=%3 deadline_ms=%4 alive=%5",
			outcome,
			waitReason,
			waitAgeMs,
			deadlineMs,
			alive);
		details += string.Format(
			" farthest_from_leader_m=%1 maximum_pair_m=%2 threshold_m=%3 normalized=%4",
			farthestFromLeaderMeters,
			maximumPairMeters,
			thresholdMeters,
			normalized);
		details += string.Format(
			" order_outcome=%1 members=[%2]",
			orderOutcome,
			memberSamples);
		AICF_Stage35Diagnostics.Info("COHESION_OUTCOME", details);
	}

	void WaitingForSiteExit(
		AICF_TransportTrip trip,
		string outcome,
		string reason,
		int waitAgeMs,
		int totalWaitAgeMs,
		int cumulativeAttempts,
		string target,
		int requestGeneration,
		int oldRequestGeneration,
		int baseRevision,
		bool vehicleRetrySuppressed,
		string causationId)
	{
		string details = FormatTripIdentity(trip, causationId, reason);
		details += string.Format(
			" outcome=%1 wait_age_ms=%2 total_wait_age_ms=%3 cumulative_attempts=%4 target=%5",
			outcome,
			waitAgeMs,
			totalWaitAgeMs,
			cumulativeAttempts,
			target);
		details += string.Format(
			" request_generation=%1 old_request_generation=%2 base_revision=%3 vehicle_retry_suppressed=%4",
			requestGeneration,
			oldRequestGeneration,
			baseRevision,
			vehicleRetrySuppressed);
		AICF_Stage35Diagnostics.Info("WAITING_FOR_SITE_EXIT", details);
	}

	void AuditSpawnWait(
		AICF_TransportTrip trip,
		AICF_VehicleRequestState requestState,
		string reason,
		int waitAgeMs,
		int totalWaitAgeMs,
		int cohesionWaitAgeMs,
		int nextDeadlineMs,
		string memberSamples,
		string causationId)
	{
		string details = FormatTripIdentity(trip, causationId, reason);
		if (requestState)
		{
			details += string.Format(
				" request_generation=%1 attempts=%2 cumulative_attempts=%3 next_attempt_at_ms=%4",
				requestState.GetRequestGeneration(),
				requestState.GetAttemptCount(),
				requestState.GetTotalAttemptCount(),
				requestState.GetNextAttemptAtMs());
			details += string.Format(
				" context_reset_count=%1 context_reset_reason=%2 cohesion_spread_m=%3",
				requestState.GetContextResetCount(),
				requestState.GetContextResetReason(),
				requestState.GetCohesionSpreadMeters());
		}
		details += string.Format(
			" wait_age_ms=%1 total_wait_age_ms=%2 cohesion_wait_age_ms=%3 next_deadline_ms=%4",
			waitAgeMs,
			totalWaitAgeMs,
			cohesionWaitAgeMs,
			nextDeadlineMs);
		details += string.Format(" members=[%1]", memberSamples);
		AICF_Stage3Diagnostics.Info("VEHICLE_SPAWN_WAIT_HEARTBEAT", details);
	}

	void AuditTerminalPending(
		AICF_TransportTrip trip,
		int pendingAgeMs,
		bool restorePending,
		bool meaningfulTask,
		AICF_VehicleTerminalClearanceObservation observation,
		string causationId)
	{
		string details = FormatTripIdentity(trip, causationId, "TERMINAL_PENDING_AUDIT");
		details += string.Format(
			" pending_age_ms=%1 alive=%2 logical_occupants=%3 transitions=%4 inside_bounds=%5",
			pendingAgeMs,
			observation.GetAlive(),
			observation.GetLogicalOccupants(),
			observation.GetTransitions(),
			observation.GetInsideBounds());
		details += string.Format(
			" restore_pending=%1 meaningful_task=%2 force_attempts=%3 clearance_attempts=%4",
			restorePending,
			meaningfulTask,
			observation.GetForceAttempts(),
			observation.GetClearanceAttempts());
		details += string.Format(
			" next_action=%1 samples=[%2]",
			observation.GetNextAction(),
			observation.GetSamples());
		AICF_Stage3Diagnostics.Info("TERMINAL_PENDING_CLEARANCE", details);
	}

	void AuditCleanupSnapshot(
		AICF_VehicleCleanupSnapshot snapshot,
		AICF_VehicleCleanupState cleanupState,
		FactionKey factionKey,
		string operationId,
		string causationId,
		int protectedOccupants,
		int playerTransitions,
		int nearbyPlayers,
		int stableClearMs,
		string nextAction,
		string blockerSamples)
	{
		string details = string.Format(
			"faction=%1 operation_id=%2 causation_id=%3 ",
			factionKey,
			operationId,
			causationId);
		details += FormatCleanupSnapshot(snapshot);
		if (cleanupState)
		{
			details += string.Format(
				" cleanup_started_at_ms=%1 cleanup_deadline_ms=%2 stable_clear_started_at_ms=%3 delete_attempts=%4",
				cleanupState.GetStartedAtMs(),
				cleanupState.GetAbsoluteDeadlineMs(),
				cleanupState.GetStableClearStartedAtMs(),
				cleanupState.GetDeleteAttempts());
			details += string.Format(
				" delete_confirmation_pending=%1 stopped_fail_closed=%2 blocker_signature=%3",
				cleanupState.IsDeleteConfirmationPending(),
				cleanupState.IsStoppedFailClosed(),
				cleanupState.GetBlockerSignature());
		}
		details += string.Format(
			" protected_occupants=%1 player_transitions=%2 nearby_players=%3 stable_clear_ms=%4",
			protectedOccupants,
			playerTransitions,
			nearbyPlayers,
			stableClearMs);
		details += string.Format(
			" next_action=%1 blocker_samples=[%2]",
			nextAction,
			blockerSamples);
		AICF_Stage3Diagnostics.Info("VEHICLE_CLEANUP_AUDIT", details);
	}

	void FleetHeartbeat(
		AICF_FleetRegistry registry,
		int managedGroups,
		int managedAgents,
		int managedWaypoints,
		int trackedEntities,
		int retainedPhysical)
	{
		int fleetCount;
		int active;
		int reserved;
		int releasePending;
		int failedClosed;
		int capHeld;
		int worldPool;
		int totalCap;
		int usActive;
		int usReserved;
		int usWorldPool;
		int ussrActive;
		int ussrReserved;
		int ussrWorldPool;
		string fleetDetails;
		if (registry)
		{
			fleetCount = registry.GetFleetCount();
			for (int index = 0; index < fleetCount; index++)
			{
				AICF_FactionFleet fleet = registry.GetFleet(index);
				if (!fleet)
					continue;
				active += fleet.GetActiveCount();
				reserved += fleet.GetReservedCount();
				releasePending += fleet.GetReleasePendingCount();
				failedClosed += fleet.GetFailedClosedCount();
				capHeld += fleet.GetActiveOrReservedCount();
				worldPool += fleet.GetWorldPoolCount();
				totalCap += fleet.GetMaximumActiveOrReserved();
				if (fleet.GetFactionKey() == "US")
				{
					usActive = fleet.GetActiveCount();
					usReserved = fleet.GetReservedCount();
					usWorldPool = fleet.GetWorldPoolCount();
				}
				else if (fleet.GetFactionKey() == "USSR")
				{
					ussrActive = fleet.GetActiveCount();
					ussrReserved = fleet.GetReservedCount();
					ussrWorldPool = fleet.GetWorldPoolCount();
				}
				fleetDetails += string.Format(
					" fleet_%1_faction=%2 fleet_%1_active=%3 fleet_%1_reserved=%4",
					index,
					fleet.GetFactionKey(),
					fleet.GetActiveCount(),
					fleet.GetReservedCount());
				fleetDetails += string.Format(
					" fleet_%1_world_pool=%2 fleet_%1_cap=%3",
					index,
					fleet.GetWorldPoolCount(),
					fleet.GetMaximumActiveOrReserved());
				fleetDetails += string.Format(
					" fleet_%1_release_pending=%2 fleet_%1_failed_closed=%3 fleet_%1_cap_held=%4",
					index,
					fleet.GetReleasePendingCount(),
					fleet.GetFailedClosedCount(),
					fleet.GetActiveOrReservedCount());
			}
		}
		string details = string.Format(
			"managed_groups=%1 managed_agents=%2 managed_waypoints=%3 tracked_entities=%4 fleet_count=%5",
			managedGroups,
			managedAgents,
			managedWaypoints,
			trackedEntities,
			fleetCount);
		details += string.Format(
			" active=%1 reserved=%2 active_or_reserved=%3 world_pool=%4 vehicle_cap=%5 retained_physical=%6",
			active,
			reserved,
			capHeld,
			worldPool,
			totalCap,
			retainedPhysical);
		details += string.Format(
			" release_pending=%1 failed_closed=%2 cap_held=%3",
			releasePending,
			failedClosed,
			capHeld);
		details += string.Format(
			" us_active=%1 us_reserved=%2 us_world_pool=%3 ussr_active=%4 ussr_reserved=%5 ussr_world_pool=%6",
			usActive,
			usReserved,
			usWorldPool,
			ussrActive,
			ussrReserved,
			ussrWorldPool);
		details += fleetDetails;
		AICF_Stage35Diagnostics.Info("FORCE_HEARTBEAT", details);
	}

	protected string FormatTripIdentity(
		AICF_TransportTrip trip,
		string causationId,
		string reason)
	{
		if (!trip)
		{
			return string.Format(
				"faction=NONE slot=NONE numeric_slot=-1 group_generation=-1 trip_generation=-1 operation_id=NONE causation_id=%1 reason=%2 vehicle=NONE kind=NONE state=NONE",
				causationId,
				reason);
		}
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
		details += FormatLeaseIdentity(trip.GetLease());
		string vehicleId = "NONE";
		string vehicleKind = "NONE";
		if (trip.GetLease())
		{
			if (!trip.GetLease().GetEntityIdString().IsEmpty())
				vehicleId = trip.GetLease().GetEntityIdString();
			vehicleKind = AICF_Stage3Diagnostics.KindToString(trip.GetLease().GetKind());
		}
		details += string.Format(
			" vehicle=%1 kind=%2 state=%3",
			vehicleId,
			vehicleKind,
			AICF_Stage3Diagnostics.TripPhaseToString(trip.GetPhase()));
		return details;
	}

	protected string FormatLeaseIdentity(AICF_VehicleLease lease)
	{
		if (!lease)
		{
			return " lease_generation=-1 vehicle_generation=-1 vehicle_lifecycle_id=NONE entity_id=NONE rpl_id=NONE prefab=NONE";
		}
		return string.Format(
			" lease_generation=%1 vehicle_generation=%2 vehicle_lifecycle_id=%3 entity_id=%4 rpl_id=%5 prefab=%6",
			lease.GetLeaseGeneration(),
			lease.GetVehicleGeneration(),
			lease.GetVehicleLifecycleId(),
			lease.GetEntityIdString(),
			lease.GetRplId(),
			lease.GetPrefab());
	}

	protected string FormatAssignmentIdentity(
		AICF_StrategicAssignmentSnapshot assignment,
		string operationId,
		string causationId)
	{
		if (!assignment)
		{
			return string.Format(
				"faction=NONE slot=NONE numeric_slot=-1 group_generation=-1 operation_id=%1 causation_id=%2",
				operationId,
				causationId);
		}
		string details = string.Format(
			"faction=%1 slot=%2 numeric_slot=%3 group_generation=%4 assignment_revision=%5 base_revision=%6",
			assignment.GetFactionKey(),
			assignment.GetSlotKey(),
			assignment.GetSlotId(),
			assignment.GetGroupGeneration(),
			assignment.GetAssignmentRevision(),
			assignment.GetBaseRevision());
		details += string.Format(
			" operation_id=%1 causation_id=%2",
			operationId,
			causationId);
		return details;
	}

	protected string FormatMeaningfulTaskObservation(
		AICF_StrategicAssignmentSnapshot assignment,
		int alive,
		string vehicleState,
		bool infantryWaypoint,
		bool vehicleWaypoint,
		int tasklessAgeMs,
		bool atMob)
	{
		string target = "NONE";
		string role = "NONE";
		string posture = "NONE";
		if (assignment)
		{
			target = AICF_Stage1Diagnostics.BaseKey(assignment.GetTargetBase());
			role = AICF_Stage1Diagnostics.RoleToString(assignment.GetRole());
			posture = assignment.GetPosture();
		}
		string details = string.Format(
			" alive=%1 target=%2 role=%3 posture=%4 vehicle_state=%5",
			alive,
			target,
			role,
			posture,
			vehicleState);
		details += string.Format(
			" infantry_waypoint=%1 vehicle_waypoint=%2 taskless_age_ms=%3 at_mob=%4",
			infantryWaypoint,
			vehicleWaypoint,
			tasklessAgeMs,
			atMob);
		return details;
	}

	protected string FormatCleanupSnapshot(AICF_VehicleCleanupSnapshot snapshot)
	{
		if (!snapshot)
		{
			return "vehicle_lifecycle_id=NONE vehicle_generation=-1 last_entity_id=NONE last_rpl_id=NONE last_origin=0 0 0 prefab=NONE release_time_ms=-1 cleanup_trigger=NONE";
		}
		string details = string.Format(
			"vehicle_lifecycle_id=%1 vehicle_generation=%2 last_entity_id=%3 last_rpl_id=%4",
			snapshot.GetVehicleLifecycleId(),
			snapshot.GetVehicleGeneration(),
			snapshot.GetLastEntityIdString(),
			snapshot.GetLastRplId());
		details += string.Format(
			" last_origin=%1 prefab=%2 release_time_ms=%3 cleanup_trigger=%4",
			snapshot.GetLastOrigin(),
			snapshot.GetPrefab(),
			snapshot.GetReleaseAtMs(),
			snapshot.GetCleanupTrigger());
		return details;
	}

	protected string WaypointId(AIWaypoint waypoint)
	{
		if (!waypoint)
			return "NONE";
		return waypoint.GetID().ToString();
	}
}
