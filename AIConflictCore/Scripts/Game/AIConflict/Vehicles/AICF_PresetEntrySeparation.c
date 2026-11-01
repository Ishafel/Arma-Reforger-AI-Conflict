class AICF_PresetEntryDoorSet
{
	ref array<string> m_aKeys = {};
	ref array<vector> m_aPoints = {};
}

// Read-only pair permission. Не выбирает ближайшую дверь и не меняет native action.
class AICF_PresetEntrySeparation
{
	static bool CanPair(AICF_TransportTrip trip, AICF_VehicleLease lease, SCR_AIGroup group,
		AICF_VehicleBoardingActionToken candidate, AICF_VehicleBoardingActionToken active)
	{
		if (!candidate || !active || candidate == active || !lease || !lease.GetVehicle()) return false;
		Vehicle vehicle = lease.GetVehicle();
		vector origin = vehicle.GetOrigin();
		vector angles = vehicle.GetYawPitchRoll();
		AICF_PresetEntryDoorSet first = Capture(trip, lease, group, candidate);
		AICF_PresetEntryDoorSet second = Capture(trip, lease, group, active);
		if (!AreIndependent(first, second)) return false;
		if (!candidate.Matches(trip, lease, group) || !active.Matches(trip, lease, group)) return false;
		if (!candidate.IsPhysicalMutationOwnerSafe() || !active.IsPhysicalMutationOwnerSafe()) return false;
		if (vector.Distance(vehicle.GetOrigin(), origin) > 0.1 || vector.Distance(vehicle.GetYawPitchRoll(), angles) > 0.1) return false;
		return true;
	}

	protected static AICF_PresetEntryDoorSet Capture(AICF_TransportTrip trip, AICF_VehicleLease lease,
		SCR_AIGroup group, AICF_VehicleBoardingActionToken token)
	{
		if (!Replication.IsServer() || !trip || !trip.IsVehiclePresetCurrent() || !lease || !group || !token) return null;
		if (!token.Matches(trip, lease, group) || !token.IsPhysicalMutationOwnerSafe() || !token.IsOwnerValid()) return null;
		Vehicle vehicle = lease.GetVehicle();
		BaseCompartmentSlot seat = token.GetCompartment();
		IEntity member = token.GetReservedEntity();
		if (!vehicle || !seat || seat.GetVehicle() != vehicle || !AICF_VehicleBoardingMutationFence.IsAuthoritativeAIEntity(member)) return null;
		if (seat.GetType() == ECompartmentType.TURRET || token.GetCompartmentType() == EAICompartmentType.Turret) return null;
		if (seat.GetOccupant() && seat.GetOccupant() != member) return null;
		if (!seat.IsCompartmentAccessible() || seat.IsGetInLockedFor(member) || (seat.IsReserved() && !seat.IsReservedBy(member))) return null;
		ChimeraCharacter character = ChimeraCharacter.Cast(member);
		if (!character) return null;
		CompartmentAccessComponent access = character.GetCompartmentAccessComponent();
		if (!access || access.IsGettingOut() || access.IsSwitchingSeatsAnim()) return null;
		// Физический transition может принадлежать другому native GetIn того же бойца.
		// Неизвестный/другой compartment не доказывает двери ожидаемого token seat.
		if (access.IsGettingIn() && access.GetCompartment() != seat) return null;
		IEntity linkedVehicle = CompartmentAccessComponent.GetVehicleIn(member);
		if (linkedVehicle && linkedVehicle != vehicle) return null;
		BaseCompartmentManagerComponent manager = seat.GetManager();
		if (!manager || !manager.GetOwner()) return null;
		IEntity owner = manager.GetOwner();
		if (owner != vehicle && owner.GetRootParent() != vehicle) return null;
		int ordinal = AICF_VehiclePreset.RelativeOwnerIndex(seat);
		if (ordinal < 0) return null;
		array<int> doors = {};
		seat.GetAvailableDoorIndices(doors);
		if (doors.IsEmpty() || doors.Count() > 16) return null;
		AICF_PresetEntryDoorSet result = new AICF_PresetEntryDoorSet();
		foreach (int door : doors)
		{
			if (!seat.IsAvailableFromDoor(door)) return null;
			CompartmentDoorInfo info = manager.GetDoorInfo(door);
			if (!info) return null;
			PointInfo entry = info.GetAIEntryPointInfo();
			if (!entry) entry = info.GetEntryPointInfo();
			if (!entry || !entry.GetOwner()) return null;
			IEntity pointOwner = entry.GetOwner();
			if (pointOwner != vehicle && pointOwner.GetRootParent() != vehicle) return null;
			vector transform[4];
			entry.GetWorldTransform(transform);
			string key = string.Format("%1:%2:%3", ordinal, seat.GetCompartmentMgrID(), door);
			if (result.m_aKeys.Contains(key)) return null;
			result.m_aKeys.Insert(key);
			result.m_aPoints.Insert(transform[3]);
		}
		if (!token.Matches(trip, lease, group) || !token.IsPhysicalMutationOwnerSafe()) return null;
		if (seat.GetManager() != manager || manager.GetOwner() != owner) return null;
		if (character.GetCompartmentAccessComponent() != access) return null;
		if (access.IsGettingOut() || access.IsSwitchingSeatsAnim()) return null;
		if (access.IsGettingIn() && access.GetCompartment() != seat) return null;
		return result;
	}

	static bool AreIndependent(AICF_PresetEntryDoorSet first, AICF_PresetEntryDoorSet second)
	{
		if (!first || !second || first.m_aKeys.IsEmpty() || second.m_aKeys.IsEmpty()) return false;
		if (first.m_aKeys.Count() != first.m_aPoints.Count() || second.m_aKeys.Count() != second.m_aPoints.Count()) return false;
		foreach (int left, string key : first.m_aKeys)
		{
			if (second.m_aKeys.Contains(key)) return false;
			foreach (vector other : second.m_aPoints)
				if (vector.DistanceXZ(first.m_aPoints[left], other) < 2.5) return false;
		}
		return true;
	}
}
