// Read-only доказательство штатного ожидания первой удалённой цели внутри машины.
// Наличие TRANSIT само по себе не является исполняемой задачей или idle exception.
class AICF_VehiclePresetWaitPolicy
{
	static bool IsCurrentMountedWait(AICF_TransportTrip trip, int currentBaseRevision)
	{
		if (!Replication.IsServer() || !trip ||
			trip.GetPhase() != AICF_ETransportTripPhase.TRANSIT ||
			!trip.IsPresetWaitingForDestination() || !trip.GetVehiclePreset() ||
			!trip.IsVehiclePresetCurrent() || trip.IsDeadlineReached(System.GetTickCount()))
		{
			return false;
		}
		AICF_GroupSlot slot = trip.GetVehiclePresetSlot();
		AICF_StrategicAssignmentSnapshot assignment = trip.GetAssignment();
		AICF_VehicleMovementState movement = trip.GetMovementState();
		// BASE хранит resolved native spawn, slot — origin базы. Только POSITION
		// задаёт один и тот же exact endpoint в обоих представлениях.
		if (!slot || !slot.IsCombatReady() || !assignment || !assignment.IsValid() ||
			!movement || movement.GetRouteGeneration() != 0 ||
			movement.GetRouteWaypoint() || movement.GetSupersededRouteWaypoint() ||
			assignment.GetAssignmentRevision() != slot.GetStrategicAssignmentRevision() ||
			assignment.GetStrategicIntentRevision() != slot.GetStrategicIntentRevision() ||
			currentBaseRevision < 0 || assignment.GetBaseRevision() != currentBaseRevision ||
			assignment.GetTargetKind() != slot.GetTargetKind() ||
			assignment.GetTargetBase() != slot.GetTargetBase() ||
			(slot.GetTargetKind() == AICF_EOrderTargetKind.POSITION &&
				assignment.GetTargetPosition() != slot.GetTargetPosition()) ||
			movement.GetAssignmentRevision() != assignment.GetAssignmentRevision() ||
			movement.GetTacticalTarget() != assignment.GetTargetPosition())
		{
			return false;
		}
		SCR_AIGroup group = assignment.GetGroup();
		AICF_VehicleLease lease = trip.GetLease();
		if (!group || !lease || !lease.MatchesTripIdentity(
			trip.GetFactionKey(), trip.GetSlotId(), trip.GetGroupGeneration(), trip.GetTripGeneration()))
		{
			return false;
		}
		Vehicle vehicle = lease.GetVehicle();
		if (!AICF_VehicleBoardingMutationFence.IsAuthoritativeReplicatedEntity(vehicle))
			return false;
		RplComponent rpl = RplComponent.Cast(vehicle.FindComponent(RplComponent));
		if (!lease.MatchesEntityIdentity(vehicle, vehicle.GetID(), rpl.Id().ToString()))
			return false;

		AICF_VehiclePreset preset = trip.GetVehiclePreset();
		array<BaseCompartmentSlot> seats = {};
		AICF_VehiclePreset.Collect(vehicle, seats);
		if (!preset.MatchesSeats(seats))
			return false;
		array<AIAgent> agents = {};
		group.GetAgents(agents);
		int assigned;
		for (int index; index < seats.Count(); index++)
		{
			int position = preset.m_aMembers[index];
			if (position < 0)
			{
				if (seats[index].GetOccupant())
					return false;
				continue;
			}
			ChimeraCharacter member = ChimeraCharacter.Cast(trip.VehiclePresetMember(position));
			if (!member || slot.GetRosterMember(position) != member ||
				!AICF_VehicleBoardingMutationFence.IsAuthoritativeAIEntity(member))
			{
				return false;
			}
			bool currentMember = false;
			foreach (AIAgent agent : agents)
			{
				if (agent && agent.GetParentGroup() == group && agent.GetControlledEntity() == member)
					currentMember = true;
			}
			CompartmentAccessComponent access = member.GetCompartmentAccessComponent();
			if (!currentMember || !access || !member.IsInVehicle() || !access.IsInCompartment() ||
				access.IsGettingIn() || access.IsGettingOut() || access.IsSwitchingSeatsAnim() ||
				access.GetCompartment() != seats[index] || seats[index].GetOccupant() != member ||
				CompartmentAccessComponent.GetVehicleIn(member) != vehicle)
			{
				return false;
			}
			assigned++;
		}
		return assigned > 0 && assigned == AICF_GroupRuntime.CountAliveAgents(group);
	}
}
