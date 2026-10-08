// Явный перенос существующей машины. Без замены entity, экипажа и lease.
class AICF_FIAGarrisonRecovery
{
	static const float PLAYER_RADIUS = 50;
	protected ref AICF_VehicleTaskHandoff m_Handoff = new AICF_VehicleTaskHandoff(null, null, null);

	static bool OutsidePlayerRadius(vector player, vector source, vector destination)
	{
		return vector.DistanceXZ(player, source) > PLAYER_RADIUS && vector.DistanceXZ(player, destination) > PLAYER_RADIUS;
	}

	static bool PlayersClear(vector source, vector destination)
	{
		if (!Replication.IsServer() || !GetGame()) return false;
		PlayerManager players = GetGame().GetPlayerManager();
		if (!players) return false;
		array<int> ids = {};
		players.GetAllPlayers(ids);
		foreach (int id : ids)
		{
			IEntity controlled = players.GetPlayerControlledEntity(id);
			IEntity main = SCR_PossessingManagerComponent.GetPlayerMainEntity(id);
			if (controlled && !OutsidePlayerRadius(controlled.GetOrigin(), source, destination)) return false;
			if (main && !OutsidePlayerRadius(main.GetOrigin(), source, destination)) return false;
		}
		return true;
	}

	static void Blocked(AICF_FIAGarrison g, string reason)
	{
		if (g.m_sRecoveryBlocked == reason) return;
		g.m_sRecoveryBlocked = reason;
		g.Log("FIA_GARRISON_RECOVERY_BLOCKED", "reason=" + reason + " player_radius_m=50");
	}

	static bool OccupantsSafe(AICF_FIAGarrison g)
	{
		if (!g || g.m_bRetired || !g.BaseIdentity() || !g.VehicleIdentity() || !g.GroupIdentity()) return false;
		array<BaseCompartmentSlot> seats = {};
		AICF_FIAPatrolCrew.Seats(g.m_Vehicle, seats);
		foreach (BaseCompartmentSlot seat : seats)
		{
			if (!seat || seat.GetVehicle() != g.m_Vehicle) return false;
			if (seat.GetOccupant() && !g.OwnsMember(seat.GetOccupant())) return false;
			if (seat.IsReserved() && (!seat.GetOccupant() || !seat.IsReservedBy(seat.GetOccupant()))) return false;
		}
		foreach (ChimeraCharacter member : g.m_aCrew)
		{
			if (!member || !g.OwnsMember(member)) continue;
			CompartmentAccessComponent access = member.GetCompartmentAccessComponent();
			if (!access || access.IsGettingIn() || access.IsGettingOut()) return false;
		}
		for (int i; i < g.m_aEssentialSeats.Count(); i++)
		{
			if (g.m_aEssentialSeats[i] && g.OwnsMember(g.m_aCrew[i]) && g.m_aSeats[i].GetOccupant() != g.m_aCrew[i]) return false;
		}
		return true;
	}

	bool Candidate(AICF_FIAGarrison g, int index, out vector pose[4])
	{
		SCR_AIWorld ai = SCR_AIWorld.Cast(GetGame().GetAIWorld());
		if (!ai || !ai.GetRoadNetworkManager()) return false;
		vector source = g.m_Vehicle.GetOrigin();
		vector direction = g.m_vPatrolTarget - source;
		direction[1] = 0;
		float length = direction.Length();
		if (length < 18) return false;
		direction.Normalize();
		float distance = Math.Min(length, 20 + (index / 3) * 15);
		float offset;
		if (index % 3 == 1) offset = 6;
		if (index % 3 == 2) offset = -6;
		vector requested = source + direction * distance + Vector(direction[2], 0, -direction[0]) * offset;
		vector road;
		if (!ai.GetRoadNetworkManager().GetReachableWaypointInRoad(requested, requested, 10, road)) return false;
		if (vector.DistanceXZ(road, requested) > 10 || vector.DistanceXZ(source, road) < 15 ||
			vector.DistanceXZ(source, road) > 80 || vector.Dot(road - source, direction) < 10 ||
			vector.DistanceXZ(road, g.m_vHome) > AICF_FIAGarrisonPatrol.ROUTE_RADIUS) return false;
		int roadDirection = g.m_iPatrolDirection;
		vector next;
		if (!AICF_FIAGarrisonPatrol.FindRoadEndpoint(ai.GetRoadNetworkManager(), road, g.m_vHome, roadDirection, next)) return false;
		BaseWorld world = g.m_Vehicle.GetWorld();
		AICF_LogisticsVehicleFootprint footprint = AICF_LogisticsVehicleFootprint.Get(g.m_sPrefab);
		if (!world || !footprint || !footprint.m_bValid || ChimeraWorldUtils.TryGetWaterSurfaceSimple(world, road)) return false;
		float x = road[0];
		float z = road[2];
		vector up = Vector(world.GetSurfaceY(x - 1, z) - world.GetSurfaceY(x + 1, z), 2,
			world.GetSurfaceY(x, z - 1) - world.GetSurfaceY(x, z + 1));
		up.Normalize();
		direction[1] = -(up[0] * direction[0] + up[2] * direction[2]) / up[1];
		direction.Normalize();
		Math3D.DirectionAndUpMatrix(direction, up, pose);
		pose[3] = road;
		pose[3][1] = world.GetSurfaceY(x, z);
		if (!AICF_LogisticsSpawnGeometry.FitToSurface(world, footprint, pose)) return false;
		TraceOBB body, exitTrace;
		vector exitPosition;
		return footprint.IsClear(world, pose, body) && AICF_LogisticsSpawnGeometry.ExitClear(world, footprint, pose, exitPosition, exitTrace);
	}

	bool TryRelocate(AICF_FIAGarrison g, int now, string reason)
	{
		if (!Replication.IsServer() || !g || now < g.m_iRecoveryNextMs) return false;
		g.m_iRecoveryNextMs = now + 10000;
		if (!OccupantsSafe(g) || !g.CanDrive() || g.m_bDisembarking) { Blocked(g, "IDENTITY_OCCUPANCY"); return false; }
		Physics physics = g.m_Vehicle.GetPhysics();
		if (!physics || physics.GetVelocity().Length() > 3) { Blocked(g, "VEHICLE_MOVING"); return false; }
		vector source = g.m_Vehicle.GetOrigin();
		if (!AICF_FIAGarrisonRecovery.PlayersClear(source, source)) { Blocked(g, "PLAYER_SOURCE"); return false; }
		for (int candidate; candidate < 12; candidate++)
		{
			vector pose[4];
			if (!Candidate(g, candidate, pose)) continue;
			if (!AICF_FIAGarrisonRecovery.PlayersClear(source, pose[3])) { Blocked(g, "PLAYER_DESTINATION"); continue; }
			AICF_LogisticsVehicleFootprint footprint = AICF_LogisticsVehicleFootprint.Get(g.m_sPrefab);
			TraceOBB body;
			// Последняя проверка прямо перед синхронной мутацией; чужих occupants не переносим.
			if (!OccupantsSafe(g) || !g.CanDrive() || !footprint.IsClear(g.m_Vehicle.GetWorld(), pose, body) ||
				!AICF_FIAGarrisonRecovery.PlayersClear(g.m_Vehicle.GetOrigin(), pose[3])) return false;
			m_Handoff.ClearFIAGarrisonWaypoint(g);
			physics.SetVelocity(vector.Zero);
			physics.SetAngularVelocity(vector.Zero);
			if (!g.m_Vehicle.SetWorldTransform(pose)) { Blocked(g, "TRANSFORM_REJECTED"); return false; }
			g.m_iVehicleTeleports++;
			g.m_iPatrolFailures = 0;
			g.m_bCrewRecoveryPending = false;
			g.m_bPatrolMoveFailed = false;
			g.m_vPatrolProgress = g.m_Vehicle.GetOrigin();
			g.m_iPatrolProgressAtMs = now;
			g.m_iPatrolRetryAtMs = now + 2000;
			g.m_iRecoveryNextMs = now + 30000;
			g.m_sRecoveryBlocked = string.Empty;
			g.Log("FIA_GARRISON_VEHICLE_TELEPORTED", string.Format("reason=%1 from=%2 to=%3 leg=%4 count=%5 player_radius_m=50", reason, source, g.m_Vehicle.GetOrigin(), g.m_iPatrolLeg, g.m_iVehicleTeleports));
			return true;
		}
		Blocked(g, "NO_CLEAR_ROUTE_POSITION");
		return false;
	}
}
