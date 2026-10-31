// Узкое read-only разрешение ожидать переход своего exact preset occupant.
// Совпадение фракции или native group без сохранённой рассадки недостаточно.
class AICF_VehicleCrewRoleOwnership
{
	static bool IsCurrentExactPresetOccupant(AICF_TransportTrip trip, BaseCompartmentSlot seat,
		EAICompartmentType role, int nowMs)
	{
		if (!Replication.IsServer() || !trip || !trip.IsValid() || trip.IsTerminal() ||
			trip.GetPhase() != AICF_ETransportTripPhase.TRANSIT || trip.IsDeadlineReached(nowMs) ||
			!trip.GetVehiclePreset() || !trip.IsVehiclePresetCurrent() || !seat)
		{
			return false;
		}
		if ((role != EAICompartmentType.Pilot && role != EAICompartmentType.Turret) ||
			(role == EAICompartmentType.Pilot && seat.GetType() != ECompartmentType.PILOT) ||
			(role == EAICompartmentType.Turret && seat.GetType() != ECompartmentType.TURRET))
		{
			return false;
		}
		AICF_GroupSlot slot = trip.GetVehiclePresetSlot();
		AICF_StrategicAssignmentSnapshot assignment = trip.GetAssignment();
		AICF_VehicleLease lease = trip.GetLease();
		if (!slot || !slot.IsCombatReady() || !assignment || !assignment.IsValid() ||
			slot.GetGroup() != assignment.GetGroup() || slot.GetSpawnGeneration() != trip.GetGroupGeneration() ||
			slot.GetStrategicAssignmentRevision() != assignment.GetAssignmentRevision() ||
			slot.GetStrategicIntentRevision() != assignment.GetStrategicIntentRevision() ||
			!lease || !lease.MatchesTripIdentity(trip.GetFactionKey(), trip.GetSlotId(),
				trip.GetGroupGeneration(), trip.GetTripGeneration()))
		{
			return false;
		}
		Vehicle vehicle = lease.GetVehicle();
		SCR_AIGroup group = assignment.GetGroup();
		if (!AICF_VehicleBoardingMutationFence.IsAuthoritativeReplicatedEntity(group) ||
			!AICF_VehicleBoardingMutationFence.IsAuthoritativeReplicatedEntity(vehicle))
			return false;
		RplComponent rpl = RplComponent.Cast(vehicle.FindComponent(RplComponent));
		if (!lease.MatchesEntityIdentity(vehicle, vehicle.GetID(), rpl.Id().ToString()))
			return false;
		array<BaseCompartmentSlot> seats = {};
		AICF_VehiclePreset.Collect(vehicle, seats);
		AICF_VehiclePreset preset = trip.GetVehiclePreset();
		int index = seats.Find(seat);
		if (!preset.MatchesSeats(seats) || index < 0)
			return false;
		int position = preset.m_aMembers[index];
		ChimeraCharacter member = ChimeraCharacter.Cast(seat.GetOccupant());
		if (position < 0 || !member || trip.VehiclePresetMember(position) != member ||
			slot.GetRosterMember(position) != member || !AICF_GroupRuntime.IsAliveCharacter(member) ||
			!SCR_AIDamageHandling.IsConscious(member) ||
			!AICF_VehicleBoardingMutationFence.IsAuthoritativeAIEntity(member))
		{
			return false;
		}
		array<AIAgent> agents = {};
		group.GetAgents(agents);
		bool currentMember = false;
		foreach (AIAgent agent : agents)
		{
			if (agent && agent.GetParentGroup() == group && agent.GetControlledEntity() == member)
			{
				SCR_AIUtilityComponent utility = SCR_AIUtilityComponent.Cast(agent.FindComponent(SCR_AIUtilityComponent));
				currentMember = utility && utility.m_OwnerEntity == member;
				break;
			}
		}
		CompartmentAccessComponent access = member.GetCompartmentAccessComponent();
		return currentMember && access && member.IsInVehicle() && access.IsInCompartment() &&
			!access.IsSwitchingSeatsAnim() && access.GetCompartment() == seat &&
			CompartmentAccessComponent.GetVehicleIn(member) == vehicle;
	}
}
