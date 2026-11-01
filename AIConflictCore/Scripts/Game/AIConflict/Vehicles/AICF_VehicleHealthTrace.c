// Read-only native health/physics observation. Не участвует в gameplay решениях.
class AICF_VehicleHealthTrace
{
	static string Capture(Vehicle vehicle)
	{
		if (!Replication.IsServer()) return "health_status=UNAVAILABLE health_reason=NOT_AUTHORITY";
		if (!AICF_VehicleBoardingMutationFence.IsAuthoritativeReplicatedEntity(vehicle)) return "health_status=UNAVAILABLE health_reason=NO_EXACT_VEHICLE";
		EntityID vehicleId = vehicle.GetID();
		RplComponent rpl = RplComponent.Cast(vehicle.FindComponent(RplComponent));
		if (!rpl) return "health_status=UNAVAILABLE health_reason=NO_COMPONENT_RPL";
		string componentRpl = rpl.Id().ToString();
		SCR_DamageManagerComponent damage = SCR_DamageManagerComponent.GetDamageManager(vehicle);
		if (!damage) return "health_status=UNAVAILABLE health_reason=NO_DAMAGE_MANAGER";
		HitZone defaultZone = damage.GetDefaultHitZone();
		if (!defaultZone) return "health_status=UNAVAILABLE health_reason=NO_DEFAULT_HITZONE";
		Physics physics = vehicle.GetPhysics();
		vector position = vehicle.GetOrigin();
		vector yawPitchRoll = vehicle.GetYawPitchRoll();
		vector velocity;
		vector angularVelocity;
		if (physics)
		{
			velocity = physics.GetVelocity();
			angularVelocity = physics.GetAngularVelocity();
		}
		float terrainY = GetGame().GetWorld().GetSurfaceY(position[0], position[2]);
		bool canMove = SCR_AIVehicleUsability.VehicleCanMove(vehicle, damage);
		bool onFire = SCR_AIVehicleUsability.VehicleIsOnFire(vehicle, damage);
		float movementDamage = damage.GetMovementDamage();
		bool rootDamageEnabled = damage.IsDamageHandlingEnabled();
		float defaultHealth = defaultZone.GetHealthScaled();
		EDamageState defaultState = defaultZone.GetDamageState();
		float engineEfficiency = -1;
		float gearboxEfficiency = -1;
		float wheelsEfficiency = -1;
		int engineFunctional = -1;
		int gearboxFunctional = -1;
		SCR_VehicleDamageManagerComponent vehicleDamage = SCR_VehicleDamageManagerComponent.Cast(damage);
		if (vehicleDamage)
		{
			engineEfficiency = vehicleDamage.GetEngineEfficiency();
			gearboxEfficiency = vehicleDamage.GetGearboxEfficiency();
			engineFunctional = vehicleDamage.GetEngineFunctional();
			gearboxFunctional = vehicleDamage.GetGearboxFunctional();
		}
		SCR_WheeledDamageManagerComponent wheeledDamage = SCR_WheeledDamageManagerComponent.Cast(damage);
		if (wheeledDamage) wheelsEfficiency = wheeledDamage.GetWheelsEfficiency();
		string zones = CaptureZones(damage);
		if (!IsCurrent(vehicle, vehicleId, rpl, componentRpl, damage, defaultZone, physics)) return "health_status=DISCARDED health_reason=READ_CONTEXT_CHANGED";
		string result = string.Format("health_status=READ_ONLY health_vehicle=%1 health_component_rpl=%2 health_damage_type=%3 health_can_move=%4 health_on_fire=%5 health_movement_damage=%6 health_root_damage_enabled=%7",
			vehicleId, componentRpl, damage.Type().ToString(), canMove, onFire, movementDamage, rootDamageEnabled);
		result += string.Format(" health_default_scaled=%1 health_default_state=%2 health_engine_functional=%3 health_engine_efficiency=%4 health_gearbox_functional=%5 health_gearbox_efficiency=%6 health_wheels_efficiency=%7",
			defaultHealth, defaultState, engineFunctional, engineEfficiency, gearboxFunctional, gearboxEfficiency, wheelsEfficiency);
		result += string.Format(" health_position=%1 health_ypr=%2 health_velocity=%3 health_angular_velocity=%4 health_speed_mps=%5 health_terrain_y=%6 health_origin_surface_delta_m=%7 health_has_physics=%8",
			position, yawPitchRoll, velocity, angularVelocity, velocity.Length(), terrainY, position[1] - terrainY, physics != null);
		return result + " " + zones + " health_cause_proven=0";
	}

	protected static bool IsCurrent(Vehicle vehicle, EntityID vehicleId, RplComponent rpl, string componentRpl, SCR_DamageManagerComponent damage, HitZone defaultZone, Physics physics)
	{
		if (!Replication.IsServer() || !AICF_VehicleBoardingMutationFence.IsAuthoritativeReplicatedEntity(vehicle)) return false;
		if (vehicle.GetID() != vehicleId || vehicle.FindComponent(RplComponent) != rpl) return false;
		if (!rpl || rpl.Id().ToString() != componentRpl) return false;
		if (!damage || !defaultZone) return false;
		if (SCR_DamageManagerComponent.GetDamageManager(vehicle) != damage || damage.GetDefaultHitZone() != defaultZone) return false;
		if (vehicle.GetPhysics() != physics) return false;
		return true;
	}

	protected static string CaptureZones(SCR_DamageManagerComponent damage)
	{
		array<HitZone> zones = {};
		int nativeCount = damage.GetAllHitZonesInHierarchy(zones);
		int observed;
		int damaged;
		int destroyed;
		int burning;
		int childZones;
		int enabledChildZones;
		float worstHealth = 1;
		string worstName = "NONE";
		string worstType = "NONE";
		string worstOwner = "NONE";
		for (int index; index < zones.Count() && index < 64; index++)
		{
			HitZone zone = zones[index];
			if (!zone) continue;
			observed++;
			EDamageState state = zone.GetDamageState();
			if (state != EDamageState.UNDAMAGED) damaged++;
			if (state == EDamageState.DESTROYED) destroyed++;
			HitZoneContainerComponent container = zone.GetHitZoneContainer();
			DamageManagerComponent zoneDamage = DamageManagerComponent.Cast(container);
			SCR_DamageManagerComponent scriptedDamage = SCR_DamageManagerComponent.Cast(container);
			if (scriptedDamage && scriptedDamage.IsOnFire(zone)) burning++;
			if (container && container != damage)
			{
				childZones++;
				if (zoneDamage && zoneDamage.IsDamageHandlingEnabled()) enabledChildZones++;
			}
			float health = zone.GetHealthScaled();
			if (health >= worstHealth) continue;
			worstHealth = health;
			worstName = zone.GetName();
			worstType = zone.Type().ToString();
			worstOwner = "NONE";
			if (container && container.GetOwner()) worstOwner = container.GetOwner().GetID().ToString();
		}
		string result = string.Format("health_zones_native=%1 health_zones_observed=%2 health_zones_truncated=%3 health_zones_damaged=%4 health_zones_destroyed=%5 health_zones_burning=%6 health_child_zones=%7 health_child_zones_damage_enabled=%8",
			nativeCount, observed, zones.Count() > 64, damaged, destroyed, burning, childZones, enabledChildZones);
		return result + string.Format(" health_worst_zone_scaled=%1 health_worst_zone_name=%2 health_worst_zone_type=%3 health_worst_zone_owner=%4",
			worstHealth, worstName, worstType, worstOwner);
	}
}
