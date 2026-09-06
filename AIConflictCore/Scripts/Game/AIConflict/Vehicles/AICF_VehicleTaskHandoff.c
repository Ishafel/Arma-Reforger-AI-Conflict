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
		// A destroyed group has no authoritative queue left to detach from. The
		// exact plan pointer still proves ownership and lets the controller delete
		// the orphaned waypoint entity and release the site reservation.
		if (!group)
			return true;
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
		if (!trip || !trip.GetAssignment() || !waypoint)
		{
			return false;
		}
		AICF_ETransportTripPhase phase = trip.GetPhase();
		if (phase == AICF_ETransportTripPhase.SITE_PLANNED ||
			phase == AICF_ETransportTripPhase.APPROACHING_SITE ||
			phase == AICF_ETransportTripPhase.STAGING_CONFIRMED ||
			phase == AICF_ETransportTripPhase.SPAWN_COMMIT)
		{
			AICF_VehicleSpawnPlan plan = trip.GetRequestState().GetSpawnPlan();
			return plan && waypoint == plan.GetApproachWaypoint();
		}
		if (!trip.IsValid() || trip.IsTerminal() || !trip.GetAssignment().GetGroup())
			return false;
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
		bool requiresLoneSurvivorRetreat = slot &&
			AICF_GroupRuntime.CountAliveAgents(slot.GetGroup()) == 1 &&
			!slot.IsLoneSurvivorRetreat();
		if (!requiresLoneSurvivorRetreat && ObserveExistingInfantryOrder(
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

		int alive = AICF_GroupRuntime.CountAliveAgents(slot.GetGroup());
		if (alive <= 0)
		{
			AICF_Stage3Diagnostics.Info(
				"INFANTRY_FALLBACK_DEFERRED",
				FormatIdentity(trip, reason) +
					" alive=0 handoff_mode=GROUP_REPLACEMENT transition_owner=GROUP_LIFECYCLE order_restored=0");
			return false;
		}
		bool loneSurvivorRetreat = alive == 1;
		bool plannerAccepted = IsExactWaypointCurrentAndQueued(slot, slot.GetWaypoint()) &&
			m_OrderPlanner.IsOrderValid(slot, faction);
		if (loneSurvivorRetreat && !slot.IsSystemHoldOrder() &&
			!slot.IsLoneSurvivorRetreat())
		{
			plannerAccepted = m_OrderPlanner.AssignLoneSurvivorRetreat(
				slot,
				faction,
				"VEHICLE_FALLBACK_LONE_SURVIVOR");
		}
		else if (!plannerAccepted)
		{
			plannerAccepted = m_OrderPlanner.RebuildCurrentOrder(slot, faction, reason);
		}
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
		bool postconditionMeaningfulTask = plannerAccepted &&
			slot.HasStrategicDestination() &&
			m_OrderPlanner.IsOrderValid(slot, faction) &&
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
			!slot.HasStrategicDestination() ||
			!m_OrderPlanner.IsOrderValid(slot, faction))
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
		// Existing Stage 2 durability auditing observes an operational waypoint for
		// three stable polls after this immediate handoff proof. SYSTEM_HOLD is an
		// awaiting-command safety state: its dedicated reliability branch may rebuild
		// the same HQ Defend waypoint, but it must never consume generic stuck/task-
		// loss recovery budgets.
		if (!slot.IsSystemHoldOrder())
			slot.BeginOrderRecoveryVerification("VEHICLE_HANDOFF");
		string handoffMode = "OPERATIONAL_ORDER_RESTORE";
		if (slot.IsLoneSurvivorRetreat())
			handoffMode = "LONE_SURVIVOR_RETREAT";
		AICF_Stage3Diagnostics.Info(
			"INFANTRY_FALLBACK",
			FormatIdentity(trip, reason) + string.Format(
				" order_restored=1 target=%1 handoff_mode=%2 alive=%3 transition_owner=VEHICLE_HANDOFF",
				AICF_Stage1Diagnostics.BaseKey(slot.GetTargetBase()),
				handoffMode,
				AICF_GroupRuntime.CountAliveAgents(slot.GetGroup())));
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

	// Вспомогательные physical logistics jobs того же domain owner.

	bool BindLogisticsUtility(AICF_LogisticsWorker w)
	{
		if (!w.Ready()) return false;
		SCR_AIVehicleUsageComponent usage = SCR_AIVehicleUsageComponent.Cast(w.m_Vehicle.FindComponent(SCR_AIVehicleUsageComponent));
		SCR_AIGroupUtilityComponent utility = w.m_Group.GetGroupUtilityComponent();
		if (!usage || !utility) return false;
		utility.AddUsableVehicle(usage);
		return utility.IsUsableVehicle(usage);
	}

	void ClearLogisticsWaypoint(AICF_LogisticsWorker w)
	{
		if (!w || !w.m_Waypoint) return;
		AIWaypoint waypoint = w.m_Waypoint;
		if (waypoint.GetID() != w.m_WaypointId) return;
		if (w.m_Group && !w.GroupIdentity()) return;
		if (w.GroupIdentity()) w.m_Group.RemoveWaypoint(waypoint);
		w.m_Waypoint = null;
		w.m_WaypointId = EntityID.INVALID;
		RplComponent.DeleteRplEntity(waypoint, false);
	}

	bool BindLogisticsWaypoint(AICF_LogisticsWorker w, AIWaypoint waypoint)
	{
		if (!waypoint) return false;
		if (!w.Ready() || w.m_Waypoint)
		{
			RplComponent.DeleteRplEntity(waypoint, false);
			return false;
		}
		w.m_Waypoint = waypoint;
		w.m_WaypointId = waypoint.GetID();
		w.m_Group.AddWaypointAt(waypoint, 0);
		array<AIWaypoint> queue = {};
		w.m_Group.GetWaypoints(queue);
		return queue.Contains(waypoint);
	}

	void DetachLogisticsUtility(AICF_LogisticsWorker w)
	{
		ClearLogisticsWaypoint(w);
		if (!w.GroupIdentity() || !w.m_Vehicle || w.m_Vehicle.GetID() != w.m_VehicleId) return;
		SCR_AIVehicleUsageComponent usage = SCR_AIVehicleUsageComponent.Cast(w.m_Vehicle.FindComponent(SCR_AIVehicleUsageComponent));
		SCR_AIGroupUtilityComponent utility = w.m_Group.GetGroupUtilityComponent();
		if (usage && utility && utility.IsUsableVehicle(usage)) utility.RemoveUsableVehicle(usage);
	}

	// Только terminal teardown. Во время ожидания штатные actions не изменяются.
	void CancelLogisticsDriverInteraction(AICF_LogisticsWorker w, string reason)
	{
		if (!Replication.IsServer() || !w || !w.m_DriverInteraction) return;
		AICF_LogisticsDriverInteraction wait = w.m_DriverInteraction;
		wait.Log(w, "LOGISTICS_DRIVER_INTERACTION_FAILED", reason, System.GetTickCount());
		// Не применять старый action к replacement/новому владельцу seat.
		if (wait.m_Utility && wait.m_Seat && w.DriverIdentity() && w.m_iGeneration == wait.m_iGeneration && w.m_Group == wait.m_Group && w.m_GroupId == wait.m_GroupId &&
			w.m_Driver == wait.m_Driver && w.m_DriverId == wait.m_DriverId && w.VehicleIdentity() &&
			w.m_Vehicle == wait.m_Vehicle && w.m_Lease == wait.m_Lease && w.m_Seat == wait.m_Seat &&
			AICF_LogisticsDriverInteraction.Utility(w) == wait.m_Utility)
		{
			array<ref AIActionBase> actions = {};
			wait.m_Utility.GetActions(actions);
			// Complete не вызывает GetInVehicle.OnActionFailed с teleport/новым get-out.
			if (AICF_LogisticsDriverInteraction.Live(wait.m_Return) && actions.Contains(wait.m_Return) &&
				wait.m_Return.m_CompartmentToGetIn.m_Value == wait.m_Seat && wait.m_Return.m_Vehicle.m_Value == wait.m_Seat.GetOwner())
			{
				// Callback Complete штатно снимает reservation и меняет accessibility.
				// Чужое место защищаем, обнуляя только параметр снимаемого exact action.
				IEntity occupant = wait.m_Seat.GetOccupant();
				if ((occupant && occupant != w.m_Driver) || (wait.m_Seat.IsReserved() && !wait.m_Seat.IsReservedBy(w.m_Driver)))
					wait.m_Return.m_CompartmentToGetIn.m_Value = null;
				wait.m_Return.Complete();
			}
			if (AICF_LogisticsDriverInteraction.Live(wait.m_Action) && actions.Contains(wait.m_Action) &&
				wait.m_Action.m_SmartActionComponent.m_Value == wait.m_SmartAction && wait.m_SmartAction &&
				(!wait.m_SmartAction.GetUser() || wait.m_SmartAction.GetUser().GetControlledEntity() == w.m_Driver)) wait.m_Action.Fail();
			if (AICF_LogisticsDriverInteraction.Live(wait.m_Exit) && actions.Contains(wait.m_Exit)) wait.m_Exit.Complete();
		}
		w.m_DriverInteraction = null;
	}

}
