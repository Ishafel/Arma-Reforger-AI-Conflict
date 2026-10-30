// Разрешённая коррекция native AI: только текущий AI в пределах 25 м.
// Caller владеет решением о бое/посадке; helper не меняет trip, orders или ТС.
class AICF_VehicleShortRecovery
{
	static const float MAX_ACTOR_DISPLACEMENT_METERS = 25.0;
	static const float MAX_SEAT_HEIGHT_METERS = 5.0;
	static const float MAX_VEHICLE_SPEED_MPS = 1.0;

	static bool IsCurrentMember(AICF_TransportTrip trip, IEntity member)
	{
		if (!Replication.IsServer() || !trip || !trip.IsValid() || !trip.GetAssignment() || !trip.GetLease()) return false;
		SCR_AIGroup group = trip.GetAssignment().GetGroup();
		AICF_VehicleLease lease = trip.GetLease();
		Vehicle vehicle = lease.GetVehicle();
		if (!group || !vehicle || !lease.HasPhysicalAsset() ||
			!AICF_VehicleBoardingMutationFence.IsAuthoritativeReplicatedEntity(group) ||
			!AICF_VehicleBoardingMutationFence.IsAuthoritativeReplicatedEntity(vehicle) ||
			!AICF_VehicleBoardingMutationFence.IsAuthoritativeAIEntity(member)) return false;
		if (!lease.MatchesTripIdentity(trip.GetFactionKey(), trip.GetSlotId(), trip.GetGroupGeneration(), trip.GetTripGeneration())) return false;
		RplComponent rpl = RplComponent.Cast(vehicle.FindComponent(RplComponent));
		if (!rpl || !lease.MatchesEntityIdentity(vehicle, vehicle.GetID(), rpl.Id().ToString())) return false;
		ChimeraCharacter character = ChimeraCharacter.Cast(member);
		if (!character || !character.GetAIControlComponent() || !SCR_AIDamageHandling.IsConscious(character)) return false;
		AIAgent agent = character.GetAIControlComponent().GetControlAIAgent();
		if (!agent || agent.GetControlledEntity() != member || agent.GetParentGroup() != group) return false;
		AICF_GroupSlot slot = trip.GetVehiclePresetSlot();
		if (slot)
		{
			if (slot.GetGroup() != group || slot.GetSpawnGeneration() != trip.GetGroupGeneration() ||
				slot.GetVehiclePresetRevision() != trip.GetVehiclePresetRevision()) return false;
			bool found;
			for (int position; position < AICF_Stage1Config.MAX_GROUP_SIZE; position++)
				if (trip.VehiclePresetMember(position) == member && slot.GetRosterMember(position) == member) found = true;
			if (!found) return false;
		}
		return true;
	}

	static bool IsSlowVehicle(Vehicle vehicle)
	{
		Physics physics;
		if (vehicle) physics = vehicle.GetPhysics();
		return physics && physics.GetVelocity().Length() <= MAX_VEHICLE_SPEED_MPS;
	}

	static bool CanSeat(AICF_TransportTrip trip, IEntity member, BaseCompartmentSlot seat)
	{
		if (!IsCurrentMember(trip, member) || !seat) return false;
		Vehicle vehicle = trip.GetLease().GetVehicle();
		if (trip.IsTerminal() || seat.GetVehicle() != vehicle || !IsSlowVehicle(vehicle) ||
			!seat.IsCompartmentAccessible() || seat.GetOccupant() || seat.IsGetInLockedFor(member) ||
			(seat.IsReserved() && !seat.IsReservedBy(member))) return false;
		vector destination = seat.GetPosition();
		vector origin = member.GetOrigin();
		if (vector.Distance(origin, destination) > MAX_ACTOR_DISPLACEMENT_METERS ||
			Math.AbsFloat(origin[1] - destination[1]) > MAX_SEAT_HEIGHT_METERS) return false;
		if (SCR_AIVehicleUsability.VehicleIsOnFire(vehicle) || !SCR_AIVehicleUsability.VehicleCanMove(vehicle)) return false;
		ChimeraCharacter character = ChimeraCharacter.Cast(member);
		CompartmentAccessComponent access = character.GetCompartmentAccessComponent();
		return access && !character.IsInVehicle() && !access.IsInCompartment() &&
			!access.IsGettingIn() && !access.IsGettingOut() && !access.IsSwitchingSeatsAnim() &&
			!CompartmentAccessComponent.GetVehicleIn(member);
	}

	static bool TrySeat(AICF_TransportTrip trip, IEntity member, BaseCompartmentSlot seat, string reason,
		AICF_AIQuotaVehicleDispatch admitted = null, AICF_VehicleShortSeatAttempt attempt = null)
	{
		if (!CanSeat(trip, member, seat))
		{
			if (admitted) admitted.Cancel();
			return false;
		}
		Vehicle vehicle = trip.GetLease().GetVehicle();
		float distance = vector.Distance(member.GetOrigin(), seat.GetPosition());
		CompartmentAccessComponent access = ChimeraCharacter.Cast(member).GetCompartmentAccessComponent();
		int transitions = trip.GetTransitionCount();
		AICF_AIQuotaVehicleDispatch quota = admitted;
		if (!quota) quota = AICF_AIQuotaVehicleDispatch.Begin(member, vehicle, seat);
		if (!quota) return false;
		if (!quota.Matches(member, vehicle, seat) || trip.GetTransitionCount() != transitions || !CanSeat(trip, member, seat))
		{
			quota.Cancel();
			return false;
		}
		seat.SetReserved(member);
		if (!quota.Matches(member, vehicle, seat) || trip.GetTransitionCount() != transitions || !CanSeat(trip, member, seat) || !seat.IsReservedBy(member) ||
			ChimeraCharacter.Cast(member).GetCompartmentAccessComponent() != access)
		{
			quota.Cancel();
			return false;
		}
		string operationId = trip.GetOperationId();
		string memberId = AICF_GroupTrace.Entity(member);
		string seatKey = AICF_VehiclePreset.SeatKey(seat);
		quota.MarkDispatched();
		if (!quota.Matches(member, vehicle, seat) || trip.GetTransitionCount() != transitions || !CanSeat(trip, member, seat))
		{
			quota.Cancel();
			return false;
		}
		if (attempt && !attempt.Commit(trip, member, seat, System.GetTickCount()))
		{
			quota.Cancel();
			return false;
		}
		// Counter commit и native call смежны: без getters/logging/invokers.
		bool accepted = access.GetInVehicle(vehicle, seat, true, -1, ECloseDoorAfterActions.INVALID, false);
		quota.ObserveNativeResult(accepted);
		quota.Cancel();
		AICF_Stage3Diagnostics.Warning("VEHICLE_SHORT_SEAT_RECOVERY", string.Format(
			"operation_id=%1 member=%2 seat=%3 distance_m=%4 maximum_m=%5 accepted=%6 reason=%7 visible_allowed=1 vehicle_moved=0",
			operationId, memberId, seatKey, distance, MAX_ACTOR_DISPLACEMENT_METERS, accepted, reason));
		if (!accepted && IsCurrentMember(trip, member) && seat && seat.GetVehicle() == vehicle && seat.IsReservedBy(member)) seat.SetReserved(null);
		return accepted;
	}

	static bool TryExit(AICF_TransportTrip trip, IEntity member, string reason, AICF_ReboardSeatConflictScope conflict = null)
	{
		if (!IsCurrentMember(trip, member)) return false;
		if (conflict && (conflict.m_Trip != trip || conflict.m_Occupant != member || !conflict.IsCurrent())) return false;
		Vehicle vehicle = trip.GetLease().GetVehicle();
		if (!IsSlowVehicle(vehicle)) return false;
		ChimeraCharacter character = ChimeraCharacter.Cast(member);
		CompartmentAccessComponent access = character.GetCompartmentAccessComponent();
		if (!access) return false;
		BaseCompartmentSlot seat = access.GetCompartment();
		if (!seat || seat.GetVehicle() != vehicle || seat.GetOccupant() != member ||
			CompartmentAccessComponent.GetVehicleIn(member) != vehicle) return false;
		vector destination[4];
		if (!access.FindSuitableTeleportLocation(destination) ||
			vector.Distance(member.GetOrigin(), destination[3]) > MAX_ACTOR_DISPLACEMENT_METERS ||
			ChimeraWorldUtils.TryGetWaterSurfaceSimple(vehicle.GetWorld(), destination[3])) return false;
		access.InterruptVehicleActionQueue(true, true, true);
		if (!IsCurrentMember(trip, member) || !IsSlowVehicle(vehicle) || access.GetCompartment() != seat ||
			seat.GetVehicle() != vehicle || seat.GetOccupant() != member ||
			vector.Distance(member.GetOrigin(), destination[3]) > MAX_ACTOR_DISPLACEMENT_METERS) return false;
		if (conflict && !conflict.IsCurrent()) return false;
		if (conflict && !conflict.ReleaseOwnedReservationBeforeExit()) return false;
		if (conflict && (!conflict.IsBeforeExitCurrent() ||
			vector.Distance(member.GetOrigin(), destination[3]) > MAX_ACTOR_DISPLACEMENT_METERS)) return false;
		string operationId = trip.GetOperationId();
		string memberId = AICF_GroupTrace.Entity(member);
		string seatKey = AICF_VehiclePreset.SeatKey(seat);
		bool accepted = access.GetOutVehicle_NoDoor(destination, false, false, true);
		AICF_Stage3Diagnostics.Warning("VEHICLE_SHORT_EXIT_RECOVERY", string.Format(
			"operation_id=%1 member=%2 seat=%3 maximum_m=%4 accepted=%5 reason=%6 visible_allowed=1 vehicle_moved=0",
			operationId, memberId, seatKey, MAX_ACTOR_DISPLACEMENT_METERS, accepted, reason));
		return accepted;
	}

	// Удаление только физической связи исходного погибшего/бессознательного
	// члена экипажа. Его life state, damage и inventory остаются native.
	static bool IsIncapacitatedSeatCurrent(AICF_TransportTrip trip, BaseCompartmentSlot seat)
	{
		if (!Replication.IsServer() || !trip || !trip.IsValid() || !trip.GetAssignment() || !trip.GetLease() || !seat) return false;
		SCR_AIGroup group = trip.GetAssignment().GetGroup();
		AICF_VehicleLease lease = trip.GetLease();
		Vehicle vehicle = lease.GetVehicle();
		ChimeraCharacter member = ChimeraCharacter.Cast(seat.GetOccupant());
		if (!group || !vehicle || !member || seat.GetVehicle() != vehicle || !IsSlowVehicle(vehicle) ||
			!AICF_VehicleBoardingMutationFence.IsAuthoritativeReplicatedEntity(group) ||
			!AICF_VehicleBoardingMutationFence.IsAuthoritativeReplicatedEntity(vehicle) ||
			!AICF_VehicleBoardingMutationFence.IsAuthoritativeReplicatedEntity(member)) return false;
		if (!lease.MatchesTripIdentity(trip.GetFactionKey(), trip.GetSlotId(), trip.GetGroupGeneration(), trip.GetTripGeneration())) return false;
		RplComponent rpl = RplComponent.Cast(vehicle.FindComponent(RplComponent));
		if (!rpl || !lease.MatchesEntityIdentity(vehicle, vehicle.GetID(), rpl.Id().ToString())) return false;
		AICF_GroupSlot slot = trip.GetVehiclePresetSlot();
		if (!slot || slot.GetGroup() != group || slot.GetSpawnGeneration() != trip.GetGroupGeneration() ||
			slot.GetVehiclePresetRevision() != trip.GetVehiclePresetRevision()) return false;
		bool captured;
		for (int position; position < AICF_Stage1Config.MAX_GROUP_SIZE; position++)
			if (trip.VehiclePresetMember(position) == member) captured = true;
		if (!captured || !GetGame()) return false;
		PlayerManager players = GetGame().GetPlayerManager();
		if (!players || players.GetPlayerIdFromControlledEntity(member) > 0 ||
			SCR_PossessingManagerComponent.GetPlayerIdFromMainEntity(member) > 0) return false;
		CharacterControllerComponent controller = member.GetCharacterController();
		if (!controller || (controller.GetLifeState() != ECharacterLifeState.INCAPACITATED &&
			controller.GetLifeState() != ECharacterLifeState.DEAD)) return false;
		CompartmentAccessComponent access = member.GetCompartmentAccessComponent();
		return access && access.GetCompartment() == seat && CompartmentAccessComponent.GetVehicleIn(member) == vehicle;
	}

	static bool TryClearIncapacitatedSeat(AICF_TransportTrip trip, BaseCompartmentSlot seat, string reason)
	{
		if (!IsIncapacitatedSeatCurrent(trip, seat)) return false;
		ChimeraCharacter member = ChimeraCharacter.Cast(seat.GetOccupant());
		CompartmentAccessComponent access = member.GetCompartmentAccessComponent();
		vector destination[4];
		// Если native exit не найден, bounded eject на месте только освобождает
		// compartment; caller всё равно должен дождаться реального occupant readback.
		if (!access.FindSuitableTeleportLocation(destination) ||
			vector.Distance(member.GetOrigin(), destination[3]) > MAX_ACTOR_DISPLACEMENT_METERS ||
			ChimeraWorldUtils.TryGetWaterSurfaceSimple(member.GetWorld(), destination[3]))
			member.GetWorldTransform(destination);
		access.InterruptVehicleActionQueue(true, true, true);
		if (!IsIncapacitatedSeatCurrent(trip, seat) || seat.GetOccupant() != member ||
			vector.Distance(member.GetOrigin(), destination[3]) > MAX_ACTOR_DISPLACEMENT_METERS) return false;
		string operationId = trip.GetOperationId();
		string memberId = AICF_GroupTrace.Entity(member);
		string seatKey = AICF_VehiclePreset.SeatKey(seat);
		bool accepted = access.GetOutVehicle_NoDoor(destination, true, false, true);
		AICF_Stage3Diagnostics.Warning("VEHICLE_INCAPACITATED_SEAT_CLEARED", string.Format(
			"operation_id=%1 member=%2 seat=%3 accepted=%4 reason=%5 maximum_m=%6 life_state_unchanged=1 visible_allowed=1",
			operationId, memberId, seatKey, accepted, reason, MAX_ACTOR_DISPLACEMENT_METERS));
		return accepted;
	}
}
