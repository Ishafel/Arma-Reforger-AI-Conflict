// Read-only identity scope для native subgroup bookkeeping. Физические slots,
// health, actions, waypoint и trip clocks этот boundary не меняет.
class AICF_VehicleNativeSubgroup
{
	static bool CanReconcileHandle(int recordHandle, int pilotHandle)
	{
		return recordHandle < 0 || (recordHandle != AIGroupMovementComponent.DEFAULT_HANDLER_ID && recordHandle == pilotHandle);
	}

	static bool IsCurrentVehicleHandler(AICF_TransportTrip trip, SCR_AIGroupUtilityComponent utility, int handler)
	{
		if (handler < 0 || handler == AIGroupMovementComponent.DEFAULT_HANDLER_ID) return false;
		SCR_AIGroup group;
		Vehicle vehicle;
		SCR_AIGroupVehicle registered;
		AIAgent pilot;
		AIGroupMovementComponent movement;
		string reason;
		return Read(trip, utility, group, vehicle, registered, pilot, movement, reason) && pilot &&
			registered.GetSubgroupHandleId() == handler && movement.GetAgentMoveHandlerId(pilot) == handler;
	}
	static bool CapturedMember(AICF_TransportTrip trip, IEntity actor)
	{
		if (!actor) return false;
		for (int position; position < AICF_Stage1Config.MAX_GROUP_SIZE; position++)
			if (trip.VehiclePresetMember(position) == actor) return true;
		return false;
	}

	static bool Settled(IEntity actor, Vehicle vehicle)
	{
		ChimeraCharacter character = ChimeraCharacter.Cast(actor);
		if (!character || !character.IsInVehicle() || CompartmentAccessComponent.GetVehicleIn(character) != vehicle) return false;
		CompartmentAccessComponent access = character.GetCompartmentAccessComponent();
		return access && access.IsInCompartment() && !access.IsGettingIn() && !access.IsGettingOut() &&
			!access.IsSwitchingSeatsAnim() && access.GetCompartment() && access.GetCompartment().GetVehicle() == vehicle &&
			access.GetCompartment().GetOccupant() == actor;
	}

	static bool Read(AICF_TransportTrip trip, SCR_AIGroupUtilityComponent utility,
		out SCR_AIGroup group, out Vehicle vehicle, out SCR_AIGroupVehicle registered,
		out AIAgent pilotAgent, out AIGroupMovementComponent movement, out string reason)
	{
		reason = "TRIP_SCOPE";
		if (!Replication.IsServer() || !trip || !trip.IsValid() || trip.IsTerminal() || !utility ||
			!trip.GetVehiclePreset() || !trip.IsVehiclePresetCurrent() || trip.IsDeadlineReached(System.GetTickCount())) return false;
		if (trip.GetPhase() != AICF_ETransportTripPhase.TRANSIT && trip.GetPhase() != AICF_ETransportTripPhase.TACTICAL_DEPLOYMENT) return false;
		AICF_GroupSlot slot = trip.GetVehiclePresetSlot();
		AICF_StrategicAssignmentSnapshot assignment = trip.GetAssignment();
		AICF_VehicleLease lease = trip.GetLease();
		reason = "ASSIGNMENT_SCOPE";
		if (!assignment || !slot || !slot.IsCombatReady() || !lease || !lease.HasPhysicalAsset()) return false;
		group = assignment.GetGroup();
		vehicle = lease.GetVehicle();
		if (!group || !vehicle || slot.GetGroup() != group || slot.GetSpawnGeneration() != trip.GetGroupGeneration() ||
			slot.GetStrategicAssignmentRevision() != assignment.GetAssignmentRevision() ||
			slot.GetStrategicIntentRevision() != assignment.GetStrategicIntentRevision() || slot.GetSlotId() != trip.GetSlotId()) return false;
		if (assignment.GetTargetKind() != slot.GetTargetKind() || assignment.GetTargetPosition() != slot.GetTargetPosition() ||
			assignment.GetTargetBase() != slot.GetTargetBase()) return false;
		if (!group.GetFaction() || group.GetFaction().GetFactionKey() != trip.GetFactionKey() ||
			!lease.MatchesTripIdentity(trip.GetFactionKey(), trip.GetSlotId(), trip.GetGroupGeneration(), trip.GetTripGeneration())) return false;
		reason = "REPLICATED_IDENTITY";
		if (!AICF_VehicleBoardingMutationFence.IsAuthoritativeReplicatedEntity(group) ||
			!AICF_VehicleBoardingMutationFence.IsAuthoritativeReplicatedEntity(vehicle)) return false;
		RplComponent rpl = RplComponent.Cast(vehicle.FindComponent(RplComponent));
		if (!rpl || !lease.MatchesEntityIdentity(vehicle, vehicle.GetID(), rpl.Id().ToString())) return false;
		if (group.GetGroupUtilityComponent() != utility || utility.m_Owner != group || !utility.m_VehicleMgr) return false;
		registered = utility.m_VehicleMgr.FindVehicle(vehicle);
		if (!registered || registered.GetEntity() != vehicle) return false;
		AICF_VehicleMovementState state = trip.GetMovementState();
		reason = "PENDING_RECOVERY_OWNER";
		if (!state || state.GetNativeGate() || state.GetCrewRecoveryToken() || state.IsCrewRecoveryRoutePending() ||
			state.GetNativeCrewEntry(EAICompartmentType.Pilot) || state.GetNativeCrewEntry(EAICompartmentType.Turret)) return false;
		array<AIAgent> agents = {};
		group.GetAgents(agents);
		foreach (AIAgent agent : agents)
		{
			if (!agent || agent.GetParentGroup() != group || !agent.GetControlledEntity()) return false;
			IEntity actor = agent.GetControlledEntity();
			if (!CapturedMember(trip, actor)) return false;
			if (!AICF_GroupRuntime.IsAliveCharacter(actor)) continue;
			if (!AICF_VehicleBoardingMutationFence.IsAuthoritativeAIEntity(actor)) return false;
			SCR_AIUtilityComponent actorUtility = SCR_AIUtilityComponent.Cast(agent.FindComponent(SCR_AIUtilityComponent));
			if (!actorUtility || actorUtility.m_OwnerEntity != actor) return false;
			AIActionBase current = actorUtility.GetCurrentBehavior();
			if (current && current.GetActionState() == EAIActionState.RUNNING &&
				(SCR_AIGetInVehicle.Cast(current) || SCR_AIGetOutVehicle.Cast(current) || SCR_AIPerformActionBehavior.Cast(current)))
			{
				reason = "PENDING_NATIVE_ACTOR_ACTION";
				return false;
			}
		}
		BaseCompartmentManagerComponent manager = BaseCompartmentManagerComponent.Cast(vehicle.FindComponent(BaseCompartmentManagerComponent));
		reason = "COMPARTMENT_SCOPE";
		if (!manager) return false;
		array<BaseCompartmentSlot> seats = {};
		manager.GetCompartments(seats);
		IEntity pilot;
		foreach (BaseCompartmentSlot seat : seats)
		{
			if (!seat) return false;
			IEntity occupant = seat.GetOccupant();
			if (!occupant) continue;
			if (!CapturedMember(trip, occupant) || !AICF_VehicleBoardingMutationFence.IsAuthoritativeReplicatedEntity(occupant) || !Settled(occupant, vehicle)) return false;
			if (seat.GetType() != ECompartmentType.PILOT) continue;
			if (pilot) return false;
			pilot = occupant;
		}
		// COMBAT допускает отсутствие здорового Pilot до окончания боя; это
		// не разрешение подменять его или обходить BehaviorFlow crew policy.
		reason = "NO_FIT_PILOT_NO_MUTATION";
		if (!pilot || !AICF_GroupRuntime.IsAliveCharacter(pilot) || !SCR_AIDamageHandling.IsConscious(pilot)) return true;
		ChimeraCharacter character = ChimeraCharacter.Cast(pilot);
		if (!character || !character.GetAIControlComponent() || !AICF_VehicleBoardingMutationFence.IsAuthoritativeAIEntity(pilot)) return false;
		pilotAgent = character.GetAIControlComponent().GetControlAIAgent();
		if (!pilotAgent || pilotAgent.GetControlledEntity() != pilot || pilotAgent.GetParentGroup() != group ||
			!agents.Contains(pilotAgent) || registered.GetDriver() != pilot) return false;
		AICarMovementComponent car = AICarMovementComponent.Cast(pilotAgent.GetMovementComponent());
		reason = "PILOT_CAR_MOVEMENT";
		if (!car || !car.GetAIAgent() || car.GetAIAgent().GetControlledEntity() != vehicle) return false;
		movement = AIGroupMovementComponent.Cast(group.FindComponent(AIGroupMovementComponent));
		if (!movement) return false;
		reason = "READY";
		return true;
	}
}

modded class SCR_AIGroupUtilityComponent
{
	bool AICF_ReconcileExactVehicleSubgroup(AICF_TransportTrip trip)
	{
		SCR_AIGroup group;
		Vehicle vehicle;
		SCR_AIGroupVehicle registered;
		AIAgent pilot;
		AIGroupMovementComponent movement;
		string reason;
		if (!AICF_VehicleNativeSubgroup.Read(trip, this, group, vehicle, registered, pilot, movement, reason))
		{
			AICF_Stage3Diagnostics.Info("VEHICLE_NATIVE_SUBGROUP_REJECTED", "reason=" + reason);
			return false;
		}
		if (!pilot) return true;
		int before = registered.GetSubgroupHandleId();
		int pilotBefore = movement.GetAgentMoveHandlerId(pilot);
		if (!AICF_VehicleNativeSubgroup.CanReconcileHandle(before, pilotBefore))
		{
			AICF_Stage3Diagnostics.Info("VEHICLE_NATIVE_SUBGROUP_REJECTED", string.Format("reason=STALE_POSITIVE_HANDLER record=%1 pilot_handler=%2 physical_mutation=0", before, pilotBefore));
			return false;
		}
		bool repair = before < 0;
		if (repair && !OnJoinGroupFromVehicle(pilot, true)) return false;
		SCR_AIGroup repeatedGroup;
		Vehicle repeatedVehicle;
		SCR_AIGroupVehicle repeatedRecord;
		AIAgent repeatedPilot;
		AIGroupMovementComponent repeatedMovement;
		if (!AICF_VehicleNativeSubgroup.Read(trip, this, repeatedGroup, repeatedVehicle, repeatedRecord, repeatedPilot, repeatedMovement, reason) ||
			repeatedGroup != group || repeatedVehicle != vehicle || repeatedRecord != registered || repeatedPilot != pilot || repeatedMovement != movement) return false;
		int handle = registered.GetSubgroupHandleId();
		if (handle < 0 || handle == AIGroupMovementComponent.DEFAULT_HANDLER_ID || movement.GetAgentMoveHandlerId(pilot) != handle) return false;
		array<AIAgent> agents = {};
		group.GetAgents(agents);
		int joined;
		foreach (AIAgent agent : agents)
		{
			if (!agent || agent == pilot) continue;
			IEntity actor = agent.GetControlledEntity();
			if (!AICF_GroupRuntime.IsAliveCharacter(actor) || !SCR_AIDamageHandling.IsConscious(actor) ||
				!AICF_VehicleNativeSubgroup.Settled(actor, vehicle) || movement.GetAgentMoveHandlerId(agent) == handle) continue;
			if (!AICF_VehicleNativeSubgroup.Read(trip, this, repeatedGroup, repeatedVehicle, repeatedRecord, repeatedPilot, repeatedMovement, reason) ||
				repeatedGroup != group || repeatedVehicle != vehicle || repeatedRecord != registered || repeatedPilot != pilot || repeatedMovement != movement ||
				!AICF_VehicleBoardingMutationFence.IsAuthoritativeAIEntity(actor) || !AICF_VehicleNativeSubgroup.Settled(actor, vehicle)) return false;
			if (!OnJoinGroupFromVehicle(agent, false) || movement.GetAgentMoveHandlerId(agent) != handle) return false;
			joined++;
		}
		if (!AICF_VehicleNativeSubgroup.Read(trip, this, repeatedGroup, repeatedVehicle, repeatedRecord, repeatedPilot, repeatedMovement, reason) ||
			repeatedGroup != group || repeatedVehicle != vehicle || repeatedRecord != registered || repeatedPilot != pilot || repeatedMovement != movement ||
			registered.GetSubgroupHandleId() != handle || movement.GetAgentMoveHandlerId(pilot) != handle) return false;
		string details = string.Format("operation_id=%1 group=%2 vehicle=%3 pilot=%4 record_before=%5 pilot_before=%6 handler=%7 pilot_handler=%8 joined=%9",
			trip.GetOperationId(), AICF_GroupTrace.Entity(group), AICF_GroupTrace.Entity(vehicle), AICF_GroupTrace.Entity(pilot.GetControlledEntity()),
			before, pilotBefore, handle, movement.GetAgentMoveHandlerId(pilot), joined);
		details += string.Format(" native_reconcile=%1 component_rpl=%2 assignment_revision=%3 trip_generation=%4 lease_generation=%5 vehicle_generation=%6 phase=%7 physical_mutation=0",
			repair, trip.GetLease().GetRplId(), trip.GetAssignment().GetAssignmentRevision(), trip.GetTripGeneration(), trip.GetLease().GetLeaseGeneration(),
			trip.GetLease().GetVehicleGeneration(), typename.EnumToString(AICF_ETransportTripPhase, trip.GetPhase()));
		AICF_Stage3Diagnostics.Info("VEHICLE_NATIVE_SUBGROUP_BOUND", details);
		return true;
	}
}
