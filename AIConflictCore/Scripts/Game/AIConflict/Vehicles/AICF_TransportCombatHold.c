// Read-only proof of the exact native combat task for this current trip.
// VehicleCombat is group-wide: its vehicle manager must contain our exact lease.
class AICF_TransportCombatHold
{
	static bool IsCurrentCombat(AICF_TransportTrip trip)
	{
		return ResolveCurrentCombat(trip) != null;
	}

	static SCR_AIVehicleCombatActivity ResolveCurrentCombat(AICF_TransportTrip trip)
	{
		if (!Replication.IsServer() || !trip || !trip.IsValid() || trip.IsTerminal() ||
			trip.GetPhase() != AICF_ETransportTripPhase.TRANSIT ||
			!trip.GetVehiclePreset() || !trip.IsVehiclePresetCurrent() ||
			trip.IsDeadlineReached(System.GetTickCount()))
		{
			return null;
		}
		AICF_VehicleMovementState movement = trip.GetMovementState();
		// Existing asynchronous owners keep advancing their own immutable clocks.
		// Combat observation must not hide a gate/crew token or its safety checks.
		if (!movement || movement.GetCrewRecoveryToken() || movement.GetNativeGate())
			return null;
		AICF_StrategicAssignmentSnapshot assignment = trip.GetAssignment();
		AICF_VehicleLease lease = trip.GetLease();
		if (!assignment || !assignment.IsValid() || !lease || !lease.HasPhysicalAsset() ||
			!lease.MatchesTripIdentity(trip.GetFactionKey(), trip.GetSlotId(),
				trip.GetGroupGeneration(), trip.GetTripGeneration()))
		{
			return null;
		}
		SCR_AIGroup group = assignment.GetGroup();
		Vehicle vehicle = lease.GetVehicle();
		if (!AICF_VehicleBoardingMutationFence.IsAuthoritativeReplicatedEntity(group) ||
			!AICF_VehicleBoardingMutationFence.IsAuthoritativeReplicatedEntity(vehicle))
		{
			return null;
		}
		RplComponent rpl = RplComponent.Cast(vehicle.FindComponent(RplComponent));
		if (!lease.MatchesEntityIdentity(vehicle, vehicle.GetID(), rpl.Id().ToString()))
			return null;
		SCR_AIVehicleUsageComponent usage = SCR_AIVehicleUsageComponent.Cast(
			vehicle.FindComponent(SCR_AIVehicleUsageComponent));
		if (!usage || usage.GetDamageState() == EDamageState.DESTROYED ||
			SCR_AIVehicleUsability.VehicleIsOnFire(vehicle) ||
			!SCR_AIVehicleUsability.VehicleCanMove(vehicle))
		{
			return null;
		}
		vector transform[4];
		vehicle.GetWorldTransform(transform);
		if (transform[1][1] < 0.25)
			return null;
		SCR_AIGroupUtilityComponent utility = group.GetGroupUtilityComponent();
		if (!utility || utility.m_Owner != group || !utility.m_VehicleMgr)
			return null;
		SCR_AIVehicleCombatActivity activity = SCR_AIVehicleCombatActivity.Cast(utility.GetCurrentAction());
		if (!activity || activity.m_Utility != utility || activity.GetRemoveAction() ||
			activity.GetActionIsSuspended())
		{
			return null;
		}
		// Pinned 1.8.0.13 OnActionSelected omits super, so a selected combat
		// activity can remain EVALUATED. A merely queued EVALUATED action is not proof.
		EAIActionState actionState = activity.GetActionState();
		if (actionState != EAIActionState.RUNNING && actionState != EAIActionState.EVALUATED)
			return null;
		array<ref AIActionBase> actions = {};
		utility.GetActions(actions);
		if (!actions.Contains(activity))
			return null;
		SCR_AIGroupVehicle registered = utility.m_VehicleMgr.FindVehicle(vehicle);
		if (!registered || registered.GetEntity() != vehicle ||
			registered.GetVehicleUsageComponent() != usage || registered.IsStatic())
		{
			return null;
		}
		if (!HasOnlyCurrentAIActors(trip, group, vehicle))
			return null;
		return activity;
	}

	protected static bool HasOnlyCurrentAIActors(AICF_TransportTrip trip, SCR_AIGroup group, Vehicle vehicle)
	{
		array<AIAgent> agents = {};
		array<IEntity> members = {};
		group.GetAgents(agents);
		foreach (AIAgent agent : agents)
		{
			if (!agent)
				continue;
			IEntity member = agent.GetControlledEntity();
			if (!AICF_GroupRuntime.IsAliveCharacter(member))
				continue;
			if (agent.GetParentGroup() != group ||
				!AICF_VehicleBoardingMutationFence.IsAuthoritativeAIEntity(member))
			{
				return false;
			}
			IEntity linkedVehicle = CompartmentAccessComponent.GetVehicleIn(member);
			if (linkedVehicle && linkedVehicle != vehicle)
				return false;
			members.Insert(member);
		}
		if (members.IsEmpty())
			return false;
		array<BaseCompartmentSlot> seats = {};
		AICF_VehiclePreset.Collect(vehicle, seats);
		AICF_VehiclePreset preset = trip.GetVehiclePreset();
		if (!preset || seats.IsEmpty() || !preset.MatchesSeats(seats))
			return false;
		int assigned;
		for (int index; index < seats.Count(); index++)
		{
			BaseCompartmentSlot seat = seats[index];
			if (!seat || seat.GetVehicle() != vehicle)
				return false;
			IEntity occupant = seat.GetOccupant();
			if (occupant && (!members.Contains(occupant) ||
				!AICF_VehicleBoardingMutationFence.IsAuthoritativeAIEntity(occupant)))
			{
				return false;
			}
			int position = preset.m_aMembers[index];
			if (position < 0)
			{
				if (occupant)
					return false;
				continue;
			}
			IEntity expected = trip.VehiclePresetMember(position);
			if (!expected || !members.Contains(expected) || (occupant && occupant != expected))
				return false;
			assigned++;
		}
		return assigned == members.Count();
	}
}
