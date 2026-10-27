// Read-only распознавание штатного group-wide отказа от машины после failed move.
// Не принимает native action во владение и не конкурирует с его высадкой.
class AICF_TransportNativeAbandon
{
	static bool Observe(AICF_TransportTrip trip, out string details)
	{
		details = string.Empty;
		if (!Replication.IsServer() || !trip || !trip.IsValid() || trip.IsTerminal()) return false;
		if (trip.GetPhase() != AICF_ETransportTripPhase.TRANSIT || !trip.GetAssignment()) return false;
		AICF_StrategicAssignmentSnapshot assignment = trip.GetAssignment();
		SCR_AIGroup group = assignment.GetGroup();
		AICF_VehicleLease lease = trip.GetLease();
		if (!group || !lease || !lease.HasPhysicalAsset()) return false;
		Vehicle vehicle = lease.GetVehicle();
		SCR_AIGroupUtilityComponent utility = group.GetGroupUtilityComponent();
		if (!utility || utility.m_Owner != group || !utility.m_VehicleMgr) return false;
		SCR_AIGetOutActivity activity = SCR_AIGetOutActivity.Cast(utility.GetCurrentAction());
		if (!IsExecutingAbandonActivity(activity, utility, vehicle)) return false;
		AICF_VehicleMovementState movement = trip.GetMovementState();
		if (HasCompetingOwner(movement)) return false;
		if (!movement.IsRouteWaypointBound() || movement.GetRouteGeneration() <= 0) return false;
		if (movement.GetAssignmentRevision() != assignment.GetAssignmentRevision()) return false;
		AIWaypoint waypoint = movement.GetRouteWaypoint();
		if (!waypoint || group.GetCurrentWaypoint() != waypoint) return false;
		EntityID waypointId = waypoint.GetID();
		int assignmentRevision = assignment.GetAssignmentRevision();
		int intentRevision = assignment.GetStrategicIntentRevision();
		int routeGeneration = movement.GetRouteGeneration();
		array<AIWaypoint> queue = {};
		group.GetWaypoints(queue);
		if (!queue.Contains(waypoint)) return false;
		AICF_VehiclePresetNativeSeatBinding binding = AICF_VehiclePresetNativeSeatBinding.Capture(trip);
		if (!binding || !binding.IsCurrent()) return false;
		SCR_AIGroupVehicle registered = utility.m_VehicleMgr.FindVehicle(vehicle);
		if (!registered || registered.GetEntity() != vehicle || registered.IsStatic()) return false;
		SCR_AIVehicleUsageComponent usage = SCR_AIVehicleUsageComponent.Cast(vehicle.FindComponent(SCR_AIVehicleUsageComponent));
		if (!usage || registered.GetVehicleUsageComponent() != usage) return false;
		array<BaseCompartmentSlot> seats = {};
		AICF_VehiclePreset.Collect(vehicle, seats);
		AICF_VehiclePreset preset = trip.GetVehiclePreset();
		string rosterReason;
		if (!preset || !preset.ValidateRoster(trip.GetVehiclePresetSlot(), seats, rosterReason)) return false;
		for (int index; index < seats.Count(); index++)
		{
			if (!seats[index] || seats[index].GetVehicle() != vehicle) return false;
			int position = preset.m_aMembers[index];
			if (position < 0 && seats[index].IsReserved()) return false;
			if (position >= 0 && seats[index].IsReserved() && !seats[index].IsReservedBy(trip.VehiclePresetMember(position))) return false;
		}
		if (!binding.IsCurrent() || trip.GetAssignment() != assignment || trip.GetMovementState() != movement) return false;
		if (HasCompetingOwner(movement)) return false;
		if (assignment.GetAssignmentRevision() != assignmentRevision || assignment.GetStrategicIntentRevision() != intentRevision) return false;
		if (!waypoint || !usage || SCR_AIVehicleUsageComponent.Cast(vehicle.FindComponent(SCR_AIVehicleUsageComponent)) != usage) return false;
		if (movement.GetRouteGeneration() != routeGeneration || movement.GetRouteWaypoint() != waypoint || waypoint.GetID() != waypointId) return false;
		if (group.GetCurrentWaypoint() != waypoint || group.GetGroupUtilityComponent() != utility) return false;
		array<AIWaypoint> repeatedQueue = {};
		group.GetWaypoints(repeatedQueue);
		if (!repeatedQueue.Contains(waypoint)) return false;
		if (!utility.m_VehicleMgr || !registered || registered.GetEntity() != vehicle) return false;
		if (utility.m_VehicleMgr.FindVehicle(vehicle) != registered || registered.GetVehicleUsageComponent() != usage) return false;
		if (!IsExecutingAbandonActivity(activity, utility, vehicle)) return false;
		details = string.Format(" group=%1 vehicle=%2 native_activity=%3 native_action_state=%4 native_priority=%5 native_priority_level=%6 route_generation=%7 assignment_revision=%8 intent_revision=%9",
			AICF_GroupTrace.Entity(group), AICF_GroupTrace.Entity(vehicle), activity,
			typename.EnumToString(EAIActionState, activity.GetActionState()), activity.GetPriority(), activity.EvaluatePriorityLevel(), routeGeneration, assignmentRevision, intentRevision);
		details += string.Format(" native_related_waypoint=NONE native_boarding_parameters=NONE roster_members=%1 vehicle_component_rpl=%2 group_component_rpl=%3 native_actions_unchanged=1 progress_clocks_unchanged=1",
			binding.m_aMembers.Count(), binding.m_sVehicleRpl, binding.m_sGroupRpl);
		return true;
	}

	protected static bool HasCompetingOwner(AICF_VehicleMovementState movement)
	{
		if (!movement || movement.GetCrewRecoveryToken() || movement.IsCrewRecoveryRoutePending()) return true;
		return movement.GetNativeCrewEntry(EAICompartmentType.Pilot) || movement.GetNativeCrewEntry(EAICompartmentType.Turret) || movement.GetNativeGate();
	}

	protected static bool IsExecutingAbandonActivity(SCR_AIGetOutActivity activity, SCR_AIGroupUtilityComponent utility, Vehicle vehicle)
	{
		if (!activity || !utility || !vehicle) return false;
		if (activity.m_Utility != utility || utility.GetCurrentAction() != activity || utility.GetExecutedAction() != activity) return false;
		if (activity.GetRemoveAction() || activity.GetActionIsSuspended() || activity.GetActionState() != EAIActionState.RUNNING) return false;
		if (!activity.m_Vehicle || activity.m_Vehicle.m_Value != vehicle) return false;
		if (activity.m_RelatedWaypoint || !activity.m_BoardingParameters || activity.m_BoardingParameters.m_Value) return false;
		if (activity.GetPriority() != SCR_AIActionBase.PRIORITY_BEHAVIOR_GET_OUT_VEHICLE_HIGH_PRIORITY) return false;
		if (activity.EvaluatePriorityLevel() != SCR_AIActionBase.PRIORITY_LEVEL_PLAYER) return false;
		array<ref AIActionBase> actions = {};
		utility.GetActions(actions);
		return actions.Contains(activity);
	}
}
