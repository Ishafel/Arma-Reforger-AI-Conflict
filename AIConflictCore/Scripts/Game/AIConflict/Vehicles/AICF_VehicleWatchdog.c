// Pure runtime queries used by the coordinator. Recovery policy stays in the
// coordinator so this class cannot create waypoints or mutate a group.
class AICF_VehicleWatchdog
{
	bool IsDestroyed(AICF_VehicleRuntime runtime)
	{
		if (!runtime || !runtime.GetVehicle())
			return true;

		SCR_AIVehicleUsageComponent usage = runtime.GetVehicleUsage();
		return !usage || usage.GetDamageState() == EDamageState.DESTROYED;
	}

	bool CanMove(AICF_VehicleRuntime runtime)
	{
		if (!runtime || !runtime.GetVehicle())
			return false;

		return SCR_AIVehicleUsability.VehicleCanMove(runtime.GetVehicle()) &&
			!SCR_AIVehicleUsability.VehicleIsOnFire(runtime.GetVehicle());
	}

	bool IsOverturned(AICF_VehicleRuntime runtime)
	{
		if (!runtime || !runtime.GetVehicle())
			return false;

		vector transform[4];
		runtime.GetVehicle().GetWorldTransform(transform);
		return transform[1][1] < 0.25;
	}

	IEntity ResolveAliveDriver(AICF_VehicleRuntime runtime)
	{
		if (!runtime || !runtime.GetVehicleUsage())
			return null;

		PilotCompartmentSlot pilotSlot = runtime.GetVehicleUsage().GetPilotCompartmentSlot();
		if (!pilotSlot)
			return null;

		IEntity driver = pilotSlot.GetOccupant();
		if (!AICF_GroupRuntime.IsAliveCharacter(driver))
			return null;

		return driver;
	}

	IEntity ResolveAliveGunner(AICF_VehicleRuntime runtime)
	{
		if (!runtime || !runtime.GetVehicleUsage())
			return null;

		TurretCompartmentSlot turretSlot = runtime.GetVehicleUsage().GetTurretCompartmentSlot();
		if (!turretSlot)
			return null;

		IEntity gunner = turretSlot.GetOccupant();
		if (!AICF_GroupRuntime.IsAliveCharacter(gunner))
			return null;

		return gunner;
	}

	int CountAccessibleSeats(AICF_VehicleRuntime runtime, out bool hasPilot, out bool hasTurret)
	{
		hasPilot = false;
		hasTurret = false;
		if (!runtime || !runtime.GetVehicle())
			return 0;

		BaseCompartmentManagerComponent manager = BaseCompartmentManagerComponent.Cast(
			runtime.GetVehicle().FindComponent(BaseCompartmentManagerComponent));
		if (!manager)
			return 0;

		array<BaseCompartmentSlot> compartments = {};
		manager.GetCompartments(compartments);
		int count;
		foreach (BaseCompartmentSlot compartment : compartments)
		{
			if (!compartment || !compartment.IsCompartmentAccessible())
				continue;

			if (PilotCompartmentSlot.Cast(compartment))
				hasPilot = true;
			if (TurretCompartmentSlot.Cast(compartment))
				hasTurret = true;
			count++;
		}
		return count;
	}

	int CountAliveGroupMembersInVehicle(SCR_AIGroup group, Vehicle vehicle)
	{
		if (!group || !vehicle)
			return 0;

		array<AIAgent> agents = {};
		group.GetAgents(agents);
		int count;
		foreach (AIAgent agent : agents)
		{
			if (!agent)
				continue;

			IEntity entity = agent.GetControlledEntity();
			if (!AICF_GroupRuntime.IsAliveCharacter(entity))
				continue;

			if (CompartmentAccessComponent.GetVehicleIn(entity) == vehicle)
				count++;
		}
		return count;
	}

	bool AreAllAliveMembersInVehicle(SCR_AIGroup group, Vehicle vehicle)
	{
		int alive = AICF_GroupRuntime.CountAliveAgents(group);
		return alive > 0 && CountAliveGroupMembersInVehicle(group, vehicle) == alive;
	}

	bool AreAllAliveMembersOutOfVehicle(SCR_AIGroup group, Vehicle vehicle)
	{
		if (!group || !vehicle)
			return true;

		return CountAliveGroupMembersInVehicle(group, vehicle) == 0;
	}

	bool HasAliveOccupant(Vehicle vehicle)
	{
		if (!vehicle)
			return false;

		BaseCompartmentManagerComponent manager = BaseCompartmentManagerComponent.Cast(
			vehicle.FindComponent(BaseCompartmentManagerComponent));
		if (!manager)
			return false;

		array<BaseCompartmentSlot> compartments = {};
		manager.GetCompartments(compartments);
		foreach (BaseCompartmentSlot compartment : compartments)
		{
			if (compartment && AICF_GroupRuntime.IsAliveCharacter(compartment.GetOccupant()))
				return true;
		}
		return false;
	}

	bool IsGroupCohesiveAroundVehicle(SCR_AIGroup group, Vehicle vehicle, float maximumDistanceMeters)
	{
		if (!group || !vehicle)
			return false;

		float maximumDistanceSq = maximumDistanceMeters * maximumDistanceMeters;
		array<AIAgent> agents = {};
		group.GetAgents(agents);
		foreach (AIAgent agent : agents)
		{
			if (!agent)
				continue;

			IEntity entity = agent.GetControlledEntity();
			if (!AICF_GroupRuntime.IsAliveCharacter(entity) || CompartmentAccessComponent.GetVehicleIn(entity) == vehicle)
				continue;

			if (vector.DistanceSqXZ(entity.GetOrigin(), vehicle.GetOrigin()) > maximumDistanceSq)
				return false;
		}

		return true;
	}
}
