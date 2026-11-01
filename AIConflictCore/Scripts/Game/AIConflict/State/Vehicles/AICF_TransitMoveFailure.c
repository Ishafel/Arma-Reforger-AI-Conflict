// Один exact native отказ перед stock GetOut/UNKNOWN debug latch. Callback
// только передаёт receipt; короткая коррекция остаётся у Transit и его бюджета.
class AICF_TransitMoveFailure
{
	protected AICF_TransportTrip m_Trip;
	protected AICF_StrategicAssignmentSnapshot m_Assignment;
	protected AICF_VehicleLease m_Lease;
	protected SCR_AIGroup m_Group;
	protected EntityID m_GroupId;
	protected Vehicle m_Vehicle;
	protected EntityID m_VehicleId;
	protected string m_sVehicleRpl;
	protected int m_iLeaseGeneration;
	protected int m_iVehicleGeneration;
	protected int m_iRouteGeneration;
	protected int m_iAssignmentRevision;
	protected int m_iIntentRevision;
	protected AIWaypoint m_Waypoint;
	protected SCR_AIGroupUtilityComponent m_Utility;
	protected AIActionBase m_Action;
	protected int m_iResult;
	protected int m_iHandler;
	protected int m_iCapturedAtMs;
	protected vector m_vFailureLocation;
	protected vector m_vVehiclePosition;
	protected string m_sOperation;
	protected int m_iCapturedAttempts;
	protected int m_iMaximumAttempts;
	protected float m_fCapturedSpeed;
	protected string m_sInspectionReason;
	protected bool m_bDelivered;
	protected ref array<IEntity> m_aMembers = {};
	protected ref array<EntityID> m_aMemberIds = {};

	static bool Supports(int result)
	{
		return result == EMoveError.STUCK || result == EMoveError.UNREACHABLE || result == EMoveError.UNKNOWN;
	}

	static AICF_TransitMoveFailure Capture(AICF_TransportTrip trip,
		SCR_AIGroupUtilityComponent utility, int result, int handler, bool related, vector location)
	{
		string reason;
		AICF_TransitMoveFailure receipt = CaptureWithReason(trip, utility, result, handler, related, location, reason);
		if (!receipt && Replication.IsServer() && trip && trip.GetPhase() == AICF_ETransportTripPhase.TRANSIT &&
			Supports(result) && related && handler != AIGroupMovementComponent.DEFAULT_HANDLER_ID)
		{
			string details = string.Format("operation_id=%1 reason=%2 handler=%3 native_result=%4 callback_delivered=0 recovery_requested=0",
				trip.GetOperationId(), reason, handler, typename.EnumToString(EMoveError, result));
			if (utility)
			{
				SCR_AIGroupVehicle nativeVehicle;
				if (utility.m_VehicleMgr) nativeVehicle = utility.m_VehicleMgr.FindVehicleBySubgroupId(handler);
				IEntity vehicle;
				if (nativeVehicle) vehicle = nativeVehicle.GetEntity();
				details += string.Format(" native_handler_vehicle=%1 current_action=%2 executed_action=%3",
					AICF_GroupTrace.Entity(vehicle), AICF_GroupTrace.Action(utility.GetCurrentAction()), AICF_GroupTrace.Action(utility.GetExecutedAction()));
			}
			if (trip.GetLease() && trip.GetLease().GetVehicle())
			{
				Vehicle physical = trip.GetLease().GetVehicle();
				RplComponent component = RplComponent.Cast(physical.FindComponent(RplComponent));
				string componentId = "NONE";
				if (component) componentId = component.Id().ToString();
				details += string.Format(" lease_rpl=%1 component_rpl=%2 find_item_rpl=%3 lease_trip=%4 trip=%5 lease_group_generation=%6 group_generation=%7",
					trip.GetLease().GetRplId(), componentId, Replication.FindItemId(physical).ToString(), trip.GetLease().GetTripGeneration(),
					trip.GetTripGeneration(), trip.GetLease().GetGroupGeneration(), trip.GetGroupGeneration());
			}
			AICF_Stage3Diagnostics.Info("TRANSPORT_NATIVE_MOVE_FAILURE_REJECTED", details);
		}
		return receipt;
	}

	protected static AICF_TransitMoveFailure CaptureWithReason(AICF_TransportTrip trip,
		SCR_AIGroupUtilityComponent utility, int result, int handler, bool related, vector location, out string reason)
	{
		reason = "PORT_OR_AUTHORITY";
		if (!Replication.IsServer() || !Supports(result) || !related ||
			handler < 0 || handler == AIGroupMovementComponent.DEFAULT_HANDLER_ID || !trip || !utility) return null;
		reason = "EXACT_PILOT_VEHICLE_HANDLER";
		if (!AICF_VehicleNativeSubgroup.IsCurrentVehicleHandler(trip, utility, handler)) return null;
		AICF_VehicleMovementState state = trip.GetMovementState();
		reason = "MOBILITY_BUDGET_EXHAUSTED";
		if (!state || state.GetMobilityRecoveryAttempts() >= state.GetMaximumMobilityRecoveryAttempts()) return null;
		AICF_TransitMoveFailure existing = state.GetTransitMoveFailure();
		if (existing && existing.Matches(trip) && existing.m_iHandler == handler &&
			existing.m_iResult == result && existing.m_Utility == utility &&
			existing.m_Action == utility.GetExecutedAction()) return existing;
		reason = "PENDING_RECEIPT_OR_ASSIGNMENT";
		if (existing || !trip.GetAssignment() || !trip.GetLease()) return null;
		SCR_AIGroup group = trip.GetAssignment().GetGroup();
		reason = "GROUP_UTILITY";
		if (!group || group.GetGroupUtilityComponent() != utility || !utility.m_VehicleMgr) return null;
		SCR_AIGroupVehicle nativeVehicle = utility.m_VehicleMgr.FindVehicleBySubgroupId(handler);
		reason = "NATIVE_HANDLER_VEHICLE";
		if (!nativeVehicle || nativeVehicle.GetEntity() != trip.GetLease().GetVehicle()) return null;
		SCR_AIMoveActivity move = SCR_AIMoveActivity.Cast(utility.GetCurrentAction());
		reason = "CURRENT_EXECUTED_MOVE_ACTION";
		if (!move || move.m_Utility != utility || move.GetRemoveAction() || move.GetActionIsSuspended() ||
			utility.GetExecutedAction() != move || move.m_RelatedWaypoint != state.GetRouteWaypoint() ||
			move.GetActionState() != EAIActionState.RUNNING) return null;
		AICF_TransitMoveFailure receipt = new AICF_TransitMoveFailure();
		receipt.m_Trip = trip;
		receipt.m_Assignment = trip.GetAssignment();
		receipt.m_Lease = trip.GetLease();
		receipt.m_Group = group;
		receipt.m_GroupId = group.GetID();
		receipt.m_Vehicle = trip.GetLease().GetVehicle();
		reason = "PHYSICAL_VEHICLE";
		if (!receipt.m_Vehicle) return null;
		receipt.m_VehicleId = receipt.m_Vehicle.GetID();
		RplComponent vehicleRpl = RplComponent.Cast(receipt.m_Vehicle.FindComponent(RplComponent));
		reason = "VEHICLE_COMPONENT_RPL";
		if (!vehicleRpl) return null;
		receipt.m_sVehicleRpl = vehicleRpl.Id().ToString();
		receipt.m_iLeaseGeneration = receipt.m_Lease.GetLeaseGeneration();
		receipt.m_iVehicleGeneration = receipt.m_Lease.GetVehicleGeneration();
		receipt.m_iRouteGeneration = state.GetRouteGeneration();
		receipt.m_iAssignmentRevision = receipt.m_Assignment.GetAssignmentRevision();
		receipt.m_iIntentRevision = receipt.m_Assignment.GetStrategicIntentRevision();
		receipt.m_Waypoint = state.GetRouteWaypoint();
		receipt.m_Utility = utility;
		receipt.m_Action = move;
		receipt.m_iResult = result;
		receipt.m_iHandler = handler;
		receipt.m_iCapturedAtMs = System.GetTickCount();
		receipt.m_vFailureLocation = location;
		receipt.m_vVehiclePosition = receipt.m_Vehicle.GetOrigin();
		Physics physics = receipt.m_Vehicle.GetPhysics();
		reason = "PHYSICS";
		if (!physics) return null;
		receipt.m_fCapturedSpeed = physics.GetVelocity().Length();
		receipt.m_sOperation = trip.GetOperationId();
		receipt.m_iCapturedAttempts = state.GetMobilityRecoveryAttempts();
		receipt.m_iMaximumAttempts = state.GetMaximumMobilityRecoveryAttempts();
		array<AIAgent> agents = {};
		group.GetAgents(agents);
		foreach (AIAgent agent : agents)
		{
			reason = "MISSING_ROSTER_ACTOR";
			if (!agent || !agent.GetControlledEntity()) return null;
			receipt.m_aMembers.Insert(agent.GetControlledEntity());
			receipt.m_aMemberIds.Insert(agent.GetControlledEntity().GetID());
		}
		if (!receipt.Matches(trip)) { reason = receipt.m_sInspectionReason; return null; }
		reason = "READY";
		state.SetTransitMoveFailure(receipt);
		return receipt;
	}

	bool Matches(AICF_TransportTrip trip)
	{
		m_sInspectionReason = "TRIP_SCOPE";
		if (!Replication.IsServer() || !trip || trip != m_Trip || !trip.IsValid() || trip.IsTerminal() ||
			trip.GetPhase() != AICF_ETransportTripPhase.TRANSIT || trip.IsDeadlineReached(System.GetTickCount()) ||
			!trip.IsVehiclePresetCurrent() || !trip.GetVehiclePreset()) return false;
		m_sInspectionReason = "LEASE_GROUP_VEHICLE_IDENTITY";
		if (!m_Assignment || !m_Lease || trip.GetAssignment() != m_Assignment || trip.GetLease() != m_Lease || !m_Lease.HasPhysicalAsset() ||
			!m_Group || m_Group.GetID() != m_GroupId || m_Assignment.GetGroup() != m_Group ||
			!m_Vehicle || !m_Lease.MatchesEntityIdentity(m_Vehicle, m_VehicleId, m_sVehicleRpl) ||
			m_Lease.GetLeaseGeneration() != m_iLeaseGeneration || m_Lease.GetVehicleGeneration() != m_iVehicleGeneration ||
			!m_Lease.MatchesTripIdentity(trip.GetFactionKey(), trip.GetSlotId(), trip.GetGroupGeneration(), trip.GetTripGeneration())) return false;
		RplComponent vehicleRpl = RplComponent.Cast(m_Vehicle.FindComponent(RplComponent));
		m_sInspectionReason = "CURRENT_COMPONENT_RPL";
		if (!vehicleRpl || vehicleRpl.Id().ToString() != m_sVehicleRpl) return false;
		AICF_GroupSlot slot = trip.GetVehiclePresetSlot();
		m_sInspectionReason = "SLOT_ASSIGNMENT_IDENTITY";
		if (!slot || !slot.IsCombatReady() || slot.GetStrategicAssignmentRevision() != m_iAssignmentRevision ||
			slot.GetStrategicIntentRevision() != m_iIntentRevision || trip.GetAssignment().GetAssignmentRevision() != m_iAssignmentRevision ||
			m_Assignment.GetTargetPosition() != slot.GetTargetPosition() || m_Assignment.GetTargetBase() != slot.GetTargetBase() ||
			!m_Group.GetFaction() || m_Group.GetFaction().GetFactionKey() != trip.GetFactionKey()) return false;
		AICF_VehicleMovementState state = trip.GetMovementState();
		m_sInspectionReason = "MOVEMENT_OWNER";
		if (state.GetRouteGeneration() != m_iRouteGeneration || state.GetRouteWaypoint() != m_Waypoint ||
			!state.IsRouteWaypointBound() || state.GetMobilityRecoveryAttempts() >= state.GetMaximumMobilityRecoveryAttempts() ||
			state.GetCrewRecoveryToken() || state.GetNativeGate() || state.IsCrewRecoveryRoutePending() ||
			state.GetNativeCrewEntry(EAICompartmentType.Pilot) || state.GetNativeCrewEntry(EAICompartmentType.Turret)) return false;
		m_sInspectionReason = "AUTHORITY_OR_CURRENT_NATIVE_ACTION";
		if (!AICF_VehicleBoardingMutationFence.IsAuthoritativeReplicatedEntity(m_Group) ||
			!AICF_VehicleBoardingMutationFence.IsAuthoritativeReplicatedEntity(m_Vehicle) ||
			!m_Utility || !m_Action || m_Utility.m_Owner != m_Group || m_Action.GetRemoveAction() || m_Action.GetActionIsSuspended() ||
			m_Group.GetGroupUtilityComponent() != m_Utility || m_Utility.GetCurrentAction() != m_Action ||
			m_Utility.GetExecutedAction() != m_Action || m_Group.GetCurrentWaypoint() != m_Waypoint) return false;
		array<AIWaypoint> queue = {};
		m_Group.GetWaypoints(queue);
		m_sInspectionReason = "ROUTE_QUEUE_OR_MOBILITY";
		if (!queue.Contains(m_Waypoint) || !SCR_AIVehicleUsability.VehicleCanMove(m_Vehicle)) return false;
		SCR_VehicleDamageManagerComponent damage = SCR_VehicleDamageManagerComponent.Cast(m_Vehicle.FindComponent(SCR_VehicleDamageManagerComponent));
		m_sInspectionReason = "DAMAGE_MANAGER";
		if (!damage || damage.GetMovementDamage() >= 1) return false;
		AICF_VehicleWatchdog watchdog = new AICF_VehicleWatchdog();
		m_sInspectionReason = "SETTLED_ROSTER";
		if (!watchdog.AreAllAliveMembersSettledInVehicle(m_Group, m_Vehicle)) return false;
		array<AIAgent> agents = {};
		m_Group.GetAgents(agents);
		m_sInspectionReason = "ROSTER_COUNT";
		if (agents.Count() != m_aMembers.Count() || agents.IsEmpty()) return false;
		foreach (AIAgent agent : agents)
		{
			m_sInspectionReason = "ORIGINAL_AI_ROSTER";
			if (!agent || agent.GetParentGroup() != m_Group) return false;
			IEntity member = agent.GetControlledEntity();
			int index = m_aMembers.Find(member);
			if (index < 0 || !member || member.GetID() != m_aMemberIds[index] ||
				!AICF_VehicleBoardingMutationFence.IsAuthoritativeAIEntity(member) ||
				!AICF_GroupRuntime.IsAliveCharacter(member)) return false;
		}
		BaseCompartmentManagerComponent manager = BaseCompartmentManagerComponent.Cast(m_Vehicle.FindComponent(BaseCompartmentManagerComponent));
		m_sInspectionReason = "COMPARTMENT_MANAGER";
		if (!manager) return false;
		array<BaseCompartmentSlot> seats = {};
		manager.GetCompartments(seats);
		foreach (BaseCompartmentSlot seat : seats)
		{
			m_sInspectionReason = "FOREIGN_OCCUPANT";
			if (!seat) return false;
			IEntity occupant = seat.GetOccupant();
			if (occupant && m_aMembers.Find(occupant) < 0) return false;
		}
		m_sInspectionReason = "READY";
		return AICF_VehicleNativeSubgroup.IsCurrentVehicleHandler(trip, m_Utility, m_iHandler);
	}

	bool WasDelivered() { return m_bDelivered; }
	// Только короткий хвост того же снятого native action. Новый action с теми
	// же координатами, новая route или настоящий повторный STUCK не дедуплицируются.
	bool CanDiscardRetiredNativeFailure(SCR_AIGroup group, SCR_AIGroupUtilityComponent utility,
		int result, int handler, bool related, vector location)
	{
		int age = System.GetTickCount() - m_iCapturedAtMs;
		if (!Replication.IsServer() || !m_bDelivered || age < 0 || age > 5000 || !related ||
			result != m_iResult || handler != m_iHandler || vector.Distance(location, m_vFailureLocation) > 0.1) return false;
		if (!m_Trip || m_Trip.IsTerminal() || m_Trip.GetPhase() != AICF_ETransportTripPhase.TRANSIT ||
			m_Trip.IsDeadlineReached(System.GetTickCount()) || !m_Trip.IsVehiclePresetCurrent() ||
			m_Trip.GetAssignment() != m_Assignment || m_Trip.GetLease() != m_Lease ||
			!m_Lease || !m_Lease.HasPhysicalAsset() || !OwnsVehicle(m_Lease.GetVehicle())) return false;
		RplComponent rpl = RplComponent.Cast(m_Vehicle.FindComponent(RplComponent));
		if (!rpl || rpl.Id().ToString() != m_sVehicleRpl || !SCR_AIVehicleUsability.VehicleCanMove(m_Vehicle)) return false;
		if (!group || group != m_Group || group.GetID() != m_GroupId || !utility || !m_Utility || utility != m_Utility || !m_Action ||
			utility.GetCurrentAction() != m_Action || utility.GetExecutedAction() != m_Action) return false;
		if (m_Action.GetActionState() != EAIActionState.FAILED && !m_Action.GetRemoveAction()) return false;
		AICF_VehicleMovementState state = m_Trip.GetMovementState();
		if (!state || state.GetRouteGeneration() <= m_iRouteGeneration || state.GetRouteWaypoint() == m_Waypoint) return false;
		array<AIWaypoint> queue = {};
		group.GetWaypoints(queue);
		if (state.GetSupersededRouteWaypoint() != m_Waypoint && queue.Contains(m_Waypoint)) return false;
		AICF_Stage3Diagnostics.Info("TRANSPORT_NATIVE_MOVE_FAILURE_RETIRED_DUPLICATE",
			Details() + string.Format(" retired_source_age_ms=%1 current_route_generation=%2 current_action_failed=1 effects=NONE", age, state.GetRouteGeneration()));
		return true;
	}
	bool OwnsVehicle(IEntity vehicle) { return m_Vehicle && vehicle == m_Vehicle && m_Vehicle.GetID() == m_VehicleId; }
	bool ConfirmDelivery()
	{
		if (m_bDelivered || !Matches(m_Trip)) return false;
		m_bDelivered = true;
		return true;
	}
	bool NeedsPhysicalCorrection()
	{
		if (m_iResult != EMoveError.STUCK || m_fCapturedSpeed > 1 || !m_Vehicle || m_Vehicle.GetID() != m_VehicleId) return false;
		Physics physics = m_Vehicle.GetPhysics();
		return physics && physics.GetVelocity().Length() <= 1;
	}
	string Reason() { return "NATIVE_" + typename.EnumToString(EMoveError, m_iResult); }
	string Details()
	{
		string details = string.Format(" operation_id=%1 group=%2 vehicle=%3 handler=%4 native_result=%5 captured_at_ms=%6 failure_location=%7 route_generation=%8",
			m_sOperation, m_GroupId, m_VehicleId, m_iHandler, Reason(), m_iCapturedAtMs, m_vFailureLocation, m_iRouteGeneration);
		details += string.Format(" vehicle_position=%1 waypoint=%2 mobility_attempt=%3 maximum_attempts=%4 physical_cause_proven=0 native_callback_delivered=%5",
			m_vVehiclePosition, AICF_GroupTrace.Entity(m_Waypoint), m_iCapturedAttempts, m_iMaximumAttempts, m_bDelivered);
		details += string.Format(" captured_speed_mps=%1", m_fCapturedSpeed);
		details += string.Format(" captured_component_rpl=%1", m_sVehicleRpl);
		return details;
	}
}
