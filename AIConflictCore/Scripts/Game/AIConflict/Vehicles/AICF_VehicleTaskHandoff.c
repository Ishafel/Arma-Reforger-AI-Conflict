// Owns the boundary between strategic infantry tasking and vehicle utility.
// Occupant/physical clearance is intentionally not an input to order restore.
class AICF_VehicleTaskHandoff
{
	protected ref AICF_OrderPlanner m_OrderPlanner;
	protected ref AICF_ObjectiveGraph m_ObjectiveGraph;
	protected ref AICF_TargetSelector m_TargetSelector;

	void AICF_VehicleTaskHandoff(
		AICF_OrderPlanner orderPlanner,
		AICF_ObjectiveGraph objectiveGraph,
		AICF_TargetSelector targetSelector)
	{
		m_OrderPlanner = orderPlanner;
		m_ObjectiveGraph = objectiveGraph;
		m_TargetSelector = targetSelector;
	}

	bool SuspendInfantryOrder(
		AICF_TransportTrip trip,
		AICF_GroupSlot slot,
		string reason)
	{
		if (!IsCurrentAssignment(trip, slot) || !m_OrderPlanner)
			return false;

		AIWaypoint oldWaypoint = slot.GetWaypoint();
		if (!oldWaypoint)
			return true;
		if (oldWaypoint != trip.GetAssignment().GetMeaningfulInfantryWaypoint())
		{
			ReportWaypointBindMismatch(trip, slot, oldWaypoint, "SUSPEND_SNAPSHOT_MISMATCH");
			return false;
		}

		m_OrderPlanner.SuspendOrderForVehicle(slot);
		return slot.GetWaypoint() == null;
	}

	bool AttachVehicleUtility(AICF_TransportTrip trip)
	{
		if (!IsTripLeaseCurrent(trip))
			return false;
		SCR_AIGroup group = trip.GetAssignment().GetGroup();
		Vehicle vehicle = trip.GetLease().GetVehicle();
		SCR_AIVehicleUsageComponent usage = SCR_AIVehicleUsageComponent.Cast(
			vehicle.FindComponent(SCR_AIVehicleUsageComponent));
		SCR_AIGroupUtilityComponent groupUtility = group.GetGroupUtilityComponent();
		if (!usage || !groupUtility)
			return false;
		groupUtility.AddUsableVehicle(usage);
		return groupUtility.IsUsableVehicle(usage);
	}

	void DetachVehicleUtility(AICF_TransportTrip trip)
	{
		if (!trip || !trip.GetAssignment() || !trip.GetLease())
			return;
		SCR_AIGroup group = trip.GetAssignment().GetGroup();
		Vehicle vehicle = trip.GetLease().GetVehicle();
		if (!group || !vehicle)
			return;
		SCR_AIVehicleUsageComponent usage = SCR_AIVehicleUsageComponent.Cast(
			vehicle.FindComponent(SCR_AIVehicleUsageComponent));
		SCR_AIGroupUtilityComponent groupUtility = group.GetGroupUtilityComponent();
		if (usage && groupUtility && groupUtility.IsUsableVehicle(usage))
			groupUtility.RemoveUsableVehicle(usage);
	}

	bool BindVehicleWaypoint(
		AICF_TransportTrip trip,
		AIWaypoint waypoint,
		string waypointKind)
	{
		if (!trip || trip.IsTerminal() || !trip.GetAssignment() || !waypoint)
			return false;
		SCR_AIGroup group = trip.GetAssignment().GetGroup();
		if (!group)
			return false;
		array<AIWaypoint> waypointQueue = {};
		group.GetWaypoints(waypointQueue);
		if (!waypointQueue.Contains(waypoint))
			group.AddWaypointAt(waypoint, 0);
		waypointQueue.Clear();
		group.GetWaypoints(waypointQueue);
		return waypointQueue.Contains(waypoint);
	}

	void RemoveVehicleWaypoint(
		AICF_TransportTrip trip,
		AIWaypoint waypoint,
		string trigger,
		string reason)
	{
		if (!trip || !waypoint)
			return;
		SCR_AIGroup group = trip.GetAssignment().GetGroup();
		string details = FormatIdentity(trip, reason);
		details += string.Format(
			" waypoint=%1 waypoint_kind=VEHICLE owner=VEHICLE_HANDOFF remove_trigger=%2 remove_reason=%3",
			waypoint.GetID(),
			trigger,
			reason);
		details += string.Format(
			" target=%1",
			AICF_Stage1Diagnostics.BaseKey(trip.GetAssignment().GetTargetBase()));
		if (group)
			group.RemoveWaypoint(waypoint);
		RplComponent.DeleteRplEntity(waypoint, false);
		AICF_Stage35Diagnostics.Info("WAYPOINT_REMOVED", details);
	}

	// Queue detachment and replicated deletion are intentionally separate.
	// The controller must clear the exact phase-owned state reference while the
	// entity is still valid before it requests the destructive side effect.
	bool DetachVehicleWaypoint(AICF_TransportTrip trip, AIWaypoint waypoint)
	{
		// Queue ownership belongs to the exact phase-state pointer, not to the
		// continued health of the physical asset.  A destroyed/missing lease must
		// still be able to detach its own route before the controller commits the
		// terminal transition.  AttachVehicleUtility keeps the stronger live-lease
		// fence below.
		if (!IsExactTripOwnedVehicleWaypoint(trip, waypoint))
			return false;
		SCR_AIGroup group = trip.GetAssignment().GetGroup();
		array<AIWaypoint> waypointQueue = {};
		group.GetWaypoints(waypointQueue);
		if (waypointQueue.Contains(waypoint))
			group.RemoveWaypoint(waypoint);
		waypointQueue.Clear();
		group.GetWaypoints(waypointQueue);
		return !waypointQueue.Contains(waypoint);
	}

	protected bool IsExactTripOwnedVehicleWaypoint(
		AICF_TransportTrip trip,
		AIWaypoint waypoint)
	{
		if (!trip || !trip.IsValid() || trip.IsTerminal() ||
			!trip.GetAssignment() || !trip.GetAssignment().GetGroup() || !waypoint)
		{
			return false;
		}
		if (trip.GetPhase() == AICF_ETransportTripPhase.TRANSIT)
		{
			AICF_VehicleMovementState movement = trip.GetMovementState();
			return movement && (waypoint == movement.GetRouteWaypoint() ||
				waypoint == movement.GetSupersededRouteWaypoint());
		}
		if (trip.GetPhase() == AICF_ETransportTripPhase.DISMOUNT)
		{
			AICF_VehicleDismountState dismount = trip.GetDismountState();
			return dismount && (waypoint == dismount.GetDismountWaypoint() ||
				waypoint == dismount.GetSupersededDismountWaypoint());
		}
		return false;
	}

	void DeleteDetachedVehicleWaypoint(
		AICF_TransportTrip trip,
		AIWaypoint waypoint,
		string trigger,
		string reason)
	{
		if (!trip || !waypoint)
			return;
		string waypointId = waypoint.GetID().ToString();
		string details = FormatIdentity(trip, reason);
		details += string.Format(
			" waypoint=%1 waypoint_kind=VEHICLE owner=VEHICLE_HANDOFF remove_trigger=%2 remove_reason=%3",
			waypointId,
			trigger,
			reason);
		details += string.Format(
			" target=%1",
			AICF_Stage1Diagnostics.BaseKey(trip.GetAssignment().GetTargetBase()));
		RplComponent.DeleteRplEntity(waypoint, false);
		AICF_Stage35Diagnostics.Info("WAYPOINT_REMOVED", details);
	}

	bool RestoreInfantryOrder(
		AICF_TransportTrip trip,
		AICF_VehicleHandoffState handoffState,
		AICF_GroupSlot slot,
		SCR_CampaignFaction faction,
		string trigger,
		string reason)
	{
		int requestedAtMs = System.GetTickCount();
		if (!trip || !handoffState)
			return false;
		// Planning or the reliability layer may already have restored the exact
		// current order after the bounded mutation budget was spent. Observing that
		// authoritative proof is not another mutation attempt and must remain
		// available until the handoff deadline.
		if (ObserveExistingInfantryOrder(
			trip,
			handoffState,
			slot,
			faction,
			trigger,
			reason,
			requestedAtMs))
		{
			return true;
		}
		if (!handoffState.BeginOrderRestoreRequest())
			return false;

		AIWaypoint oldWaypoint;
		if (slot)
			oldWaypoint = slot.GetWaypoint();
		SCR_CampaignMilitaryBaseComponent restoreTarget;
		if (slot)
			restoreTarget = slot.GetTargetBase();
		string requested = FormatIdentity(trip, reason);
		requested += string.Format(
			" trigger=%1 reason=%2 target=%3 old_waypoint=%4 restore_pending=1 attempt=%5 max_attempts=%6",
			trigger,
			reason,
			AICF_Stage1Diagnostics.BaseKey(restoreTarget),
			WaypointId(oldWaypoint),
			handoffState.GetRestoreAttempts(),
			handoffState.GetMaximumRestoreAttempts());
		AICF_Stage35Diagnostics.Info("ORDER_RESTORE_REQUESTED", requested);

		if (!IsCurrentGroupIdentity(trip, slot) || !faction ||
			faction.GetFactionKey() != trip.GetFactionKey() || !m_OrderPlanner)
		{
			ReportOrderRestoreResult(
				trip,
				handoffState,
				slot,
				oldWaypoint,
				null,
				false,
				false,
				false,
				false,
				trigger,
				reason,
				"IDENTITY_OR_SLOT_NOT_CURRENT",
				requestedAtMs);
			return false;
		}

		bool plannerAccepted = IsExactWaypointCurrentAndQueued(slot, slot.GetWaypoint());
		if (!plannerAccepted)
			plannerAccepted = m_OrderPlanner.RebuildCurrentOrder(slot, faction, reason);
		if (!plannerAccepted && m_ObjectiveGraph && m_TargetSelector)
		{
			plannerAccepted = m_OrderPlanner.AssignOrder(
				slot,
				faction,
				m_ObjectiveGraph,
				m_TargetSelector,
				reason,
				restoreTarget);
		}

		AIWaypoint newWaypoint = slot.GetWaypoint();
		array<AIWaypoint> waypointQueue = {};
		int queueCount = slot.GetGroup().GetWaypoints(waypointQueue);
		bool waypointInQueue = newWaypoint && waypointQueue.Contains(newWaypoint);
		bool boundToGroup = slot.GetGroup() == trip.GetAssignment().GetGroup() && waypointInQueue;
		bool isCurrent = newWaypoint && slot.GetGroup().GetCurrentWaypoint() == newWaypoint;
		bool postconditionMeaningfulTask = plannerAccepted && slot.GetTargetBase() &&
			m_OrderPlanner.IsStrategicTargetValid(slot, faction, slot.GetTargetBase()) &&
			boundToGroup && isCurrent && waypointInQueue;
		bool restored = handoffState.RecordOrderRestoreResult(
			newWaypoint,
			boundToGroup,
			isCurrent,
			waypointInQueue,
			postconditionMeaningfulTask);

		string failureReason = "NONE";
		if (!plannerAccepted)
			failureReason = "PLANNER_REJECTED";
		else if (!restored)
			failureReason = "WAYPOINT_BIND_MISMATCH";
		ReportOrderRestoreResult(
			trip,
			handoffState,
			slot,
			oldWaypoint,
			newWaypoint,
			boundToGroup,
			isCurrent,
			waypointInQueue,
			postconditionMeaningfulTask,
			trigger,
			reason,
			failureReason,
			requestedAtMs,
			queueCount);
		if (!restored)
		{
			ReportWaypointBindMismatch(trip, slot, newWaypoint, failureReason);
			return false;
		}

		ReportRestoredInfantryOrder(trip, slot, reason);
		return true;
	}

	// Proof-only reconciliation never creates, removes, or reorders a waypoint.
	// It lets Handoff adopt Planning's already-committed current order even when
	// the bounded mutation budget has been exhausted.
	protected bool ObserveExistingInfantryOrder(
		AICF_TransportTrip trip,
		AICF_VehicleHandoffState handoffState,
		AICF_GroupSlot slot,
		SCR_CampaignFaction faction,
		string trigger,
		string reason,
		int observedAtMs)
	{
		if (!IsCurrentGroupIdentity(trip, slot) || !faction || !m_OrderPlanner ||
			faction.GetFactionKey() != trip.GetFactionKey())
		{
			return false;
		}
		AIWaypoint waypoint = slot.GetWaypoint();
		if (!IsExactWaypointCurrentAndQueued(slot, waypoint) ||
			!slot.GetTargetBase() ||
			!m_OrderPlanner.IsStrategicTargetValid(slot, faction, slot.GetTargetBase()))
		{
			return false;
		}

		array<AIWaypoint> waypointQueue = {};
		int queueCount = slot.GetGroup().GetWaypoints(waypointQueue);
		bool waypointInQueue = waypointQueue.Contains(waypoint);
		bool boundToGroup = slot.GetGroup() == trip.GetAssignment().GetGroup() &&
			waypointInQueue;
		bool isCurrent = slot.GetGroup().GetCurrentWaypoint() == waypoint;
		bool meaningfulTask = boundToGroup && isCurrent && waypointInQueue;
		if (!handoffState.RecordOrderRestoreResult(
			waypoint,
			boundToGroup,
			isCurrent,
			waypointInQueue,
			meaningfulTask))
		{
			return false;
		}

		ReportOrderRestoreResult(
			trip,
			handoffState,
			slot,
			waypoint,
			waypoint,
			boundToGroup,
			isCurrent,
			waypointInQueue,
			meaningfulTask,
			trigger + "_PROOF_ONLY",
			reason,
			"NONE",
			observedAtMs,
			queueCount);
		ReportRestoredInfantryOrder(trip, slot, reason);
		return true;
	}

	protected void ReportRestoredInfantryOrder(
		AICF_TransportTrip trip,
		AICF_GroupSlot slot,
		string reason)
	{
		// Existing Stage 2 durability auditing observes the same exact waypoint
		// for three stable polls after this immediate handoff proof.
		slot.BeginOrderRecoveryVerification("VEHICLE_HANDOFF");
		AICF_Stage3Diagnostics.Info(
			"INFANTRY_FALLBACK",
			FormatIdentity(trip, reason) + string.Format(
				" order_restored=1 target=%1",
				AICF_Stage1Diagnostics.BaseKey(slot.GetTargetBase())));
	}

	protected bool IsCurrentAssignment(AICF_TransportTrip trip, AICF_GroupSlot slot)
	{
		if (!trip || !trip.GetAssignment() || !slot || !slot.IsCombatReady())
			return false;
		AICF_StrategicAssignmentSnapshot assignment = trip.GetAssignment();
		return assignment.MatchesCurrent(
			assignment.GetFactionKey(),
			slot.GetSlotId(),
			slot.GetSpawnGeneration(),
			slot.GetGroup()) && slot.GetStrategicAssignmentRevision() ==
			assignment.GetAssignmentRevision();
	}

	// Terminal restore follows Planning's current slot truth. A newer strategic
	// revision must never block infantry recovery for the same live generation.
	protected bool IsCurrentGroupIdentity(AICF_TransportTrip trip, AICF_GroupSlot slot)
	{
		if (!trip || !trip.GetAssignment() || !slot || !slot.IsCombatReady())
			return false;
		AICF_StrategicAssignmentSnapshot assignment = trip.GetAssignment();
		return assignment.GetFactionKey() == trip.GetFactionKey() &&
			slot.GetSlotId() == trip.GetSlotId() &&
			slot.GetSpawnGeneration() == trip.GetGroupGeneration() &&
			slot.GetGroup() == assignment.GetGroup();
	}

	protected bool IsTripLeaseCurrent(AICF_TransportTrip trip)
	{
		if (!trip || trip.IsTerminal() || !trip.GetAssignment() || !trip.GetLease())
			return false;
		AICF_VehicleLease lease = trip.GetLease();
		return lease.HasPhysicalAsset() && lease.MatchesTripIdentity(
			trip.GetFactionKey(),
			trip.GetSlotId(),
			trip.GetGroupGeneration(),
			trip.GetTripGeneration());
	}

	protected bool IsExactWaypointCurrentAndQueued(AICF_GroupSlot slot, AIWaypoint waypoint)
	{
		if (!slot || !slot.GetGroup() || !waypoint)
			return false;
		array<AIWaypoint> waypointQueue = {};
		slot.GetGroup().GetWaypoints(waypointQueue);
		return waypointQueue.Contains(waypoint) &&
			slot.GetGroup().GetCurrentWaypoint() == waypoint;
	}

	protected void ReportOrderRestoreResult(
		AICF_TransportTrip trip,
		AICF_VehicleHandoffState handoffState,
		AICF_GroupSlot slot,
		AIWaypoint oldWaypoint,
		AIWaypoint newWaypoint,
		bool boundToGroup,
		bool isCurrent,
		bool waypointInQueue,
		bool postconditionMeaningfulTask,
		string trigger,
		string restoreReason,
		string failureReason,
		int requestedAtMs,
		int queueCount = 0)
	{
		string result = FormatIdentity(trip, restoreReason);
		result += string.Format(
			" success=%1 old_waypoint=%2 new_waypoint=%3 bound_to_group=%4 is_current=%5",
			handoffState.IsOrderRestored(),
			WaypointId(oldWaypoint),
			WaypointId(newWaypoint),
			boundToGroup,
			isCurrent);
		result += string.Format(
			" queue_count=%1 waypoint_in_queue=%2 postcondition_meaningful_task=%3 failure_reason=%4 latency_ms=%5 trigger=%6 restore_reason=%7",
			queueCount,
			waypointInQueue,
			postconditionMeaningfulTask,
			failureReason,
			System.GetTickCount(requestedAtMs),
			trigger,
			restoreReason);
		result += string.Format(
			" attempt=%1 max_attempts=%2",
			handoffState.GetRestoreAttempts(),
			handoffState.GetMaximumRestoreAttempts());
		AICF_Stage35Diagnostics.Info("ORDER_RESTORE_RESULT", result);
	}

	protected void ReportWaypointBindMismatch(
		AICF_TransportTrip trip,
		AICF_GroupSlot slot,
		AIWaypoint waypoint,
		string reason)
	{
		int queueCount;
		if (slot && slot.GetGroup())
		{
			array<AIWaypoint> waypointQueue = {};
			queueCount = slot.GetGroup().GetWaypoints(waypointQueue);
		}
		AICF_Stage35Diagnostics.Warning(
			"WAYPOINT_BIND_MISMATCH",
			FormatIdentity(trip, reason) + string.Format(
				" waypoint=%1 queue_count=%2",
				WaypointId(waypoint),
				queueCount));
	}

	protected string FormatIdentity(AICF_TransportTrip trip, string reason)
	{
		if (!trip)
		{
			return "faction=NONE slot=NONE numeric_slot=-1 group_generation=-1" +
				" trip_generation=-1 vehicle=NONE kind=NONE state=NONE";
		}
		string details = string.Format(
			"faction=%1 slot=%2 numeric_slot=%3 group_generation=%4 trip_generation=%5 operation_id=%6 causation_id=%7",
			trip.GetFactionKey(),
			trip.GetSlotKey(),
			trip.GetSlotId(),
			trip.GetGroupGeneration(),
			trip.GetTripGeneration(),
			trip.GetOperationId(),
			trip.GetCausationId());
		int leaseGeneration = -1;
		int vehicleGeneration = -1;
		string lifecycleId = "NONE";
		string vehicleId = "NONE";
		string vehicleKind = "NONE";
		AICF_VehicleLease lease = trip.GetLease();
		if (lease)
		{
			leaseGeneration = lease.GetLeaseGeneration();
			vehicleGeneration = lease.GetVehicleGeneration();
			lifecycleId = lease.GetVehicleLifecycleId();
			if (!lease.GetEntityIdString().IsEmpty())
				vehicleId = lease.GetEntityIdString();
			if (vehicleGeneration > 0)
				vehicleKind = typename.EnumToString(AICF_EVehicleKind, lease.GetKind());
		}
		details += string.Format(
			" lease_generation=%1 vehicle_generation=%2 vehicle_lifecycle_id=%3 reason=%4",
			leaseGeneration,
			vehicleGeneration,
			lifecycleId,
			reason);
		details += string.Format(
			" vehicle=%1 kind=%2 state=%3",
			vehicleId,
			vehicleKind,
			typename.EnumToString(AICF_ETransportTripPhase, trip.GetPhase()));
		return details;
	}

	protected string WaypointId(AIWaypoint waypoint)
	{
		if (!waypoint)
			return "NONE";
		return waypoint.GetID().ToString();
	}
}
