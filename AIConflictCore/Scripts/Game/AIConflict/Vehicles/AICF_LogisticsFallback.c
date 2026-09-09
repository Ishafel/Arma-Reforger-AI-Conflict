// Контекст ограниченного восстановления. World/boarding effects остаются в flows,
// снятие native actions и waypoint — в handoff, job и фаза — у их владельцев.
class AICF_LogisticsFallback : AICF_LogisticsDriverInteraction
{
	static const int MAX_PER_LEG = 4;
	static const int TIMEOUT_MS = 45000;
	static const int CANDIDATES = 15;
	bool m_bRelocate;
	bool m_bRelocated;
	int m_iSeatAttempts;
	int m_iNextSeatMs;
	int m_iCandidate;
	vector m_vBypassDirection;
	ref AICF_LogisticsResourcePool m_CargoPool;
	string m_sCause;

	static bool Recoverable(string reason)
	{
		return reason == "BOUNDED_ROUTE_RECOVERY_EXHAUSTED" ||
			reason == "DRIVER_INTERACTION_NATIVE_ACTION_FAILED" || reason == "DRIVER_INTERACTION_NATIVE_CHAIN_LOST" ||
			reason == "DRIVER_INTERACTION_GROUP_ACTIVITY_LOST" || reason == "DRIVER_INTERACTION_TARGET_LOST" ||
			reason == "DRIVER_INTERACTION_NO_PROGRESS" || reason == "DRIVER_INTERACTION_DEADLINE" ||
			reason == "DRIVER_INTERACTION_LEG_BUDGET" || reason == "DRIVER_INTERACTION_PHYSICAL_CONTEXT_LOST" ||
			reason == "UNKNOWN_DRIVER_RETURN_FAILED" || reason == "UNKNOWN_DRIVER_RECOVERY_DEADLINE" ||
			reason == "UNKNOWN_DRIVER_PHYSICAL_CONTEXT_LOST";
	}

	static AICF_LogisticsFallback Create(AICF_LogisticsWorker w, string reason, int now)
	{
		if (!Replication.IsServer() || !Recoverable(reason) || !Moving(w) ||
			w.m_iFallbackAttempts >= MAX_PER_LEG || !w.m_RouteRecovery ||
			!w.m_RouteRecovery.m_Context || !w.m_RouteRecovery.m_Context.Matches(w)) return null;
		if (w.m_DriverInteraction && !w.m_DriverInteraction.Matches(w)) return null;
		AICF_LogisticsFallback recovery = new AICF_LogisticsFallback();
		recovery.Capture(w, now);
		recovery.m_CargoPool = w.m_CargoPool;
		recovery.m_sCause = reason;
		recovery.m_bRelocate = reason == "BOUNDED_ROUTE_RECOVERY_EXHAUSTED";
		recovery.m_vBypassDirection = w.m_vEndpoint - w.m_Vehicle.GetOrigin();
		if (w.m_DriverInteraction && w.m_DriverInteraction.m_Target)
		{
			recovery.m_bRelocate = true;
			recovery.m_vBypassDirection = w.m_DriverInteraction.m_Target.GetOrigin() - w.m_Vehicle.GetOrigin();
		}
		recovery.m_vBypassDirection[1] = 0;
		if (recovery.m_vBypassDirection.Length() < 1) recovery.m_vBypassDirection = w.m_vEndpoint - w.m_Vehicle.GetOrigin();
		recovery.m_vBypassDirection[1] = 0;
		recovery.m_vBypassDirection.Normalize();
		if (!recovery.Safe(w, now)) return null;
		return recovery;
	}

	bool Safe(AICF_LogisticsWorker w, int now)
	{
		if (!Replication.IsServer() || !Matches(w) || w.HasForeignOccupant(true) ||
			w.m_bCargoFault || !w.m_bCustody || w.m_CargoPool != m_CargoPool || !m_CargoPool || !m_CargoPool.Valid() ||
			!DeadlineReason(now).IsEmpty() || w.LegAgeMs(now) >= AICF_LogisticsConfig.LEG_TIMEOUT_MS) return false;
		if (m_Job && now >= m_Job.m_iExpiresAtMs) return false;
		if (m_Job && (!m_Job.m_Destination || !m_Job.m_Destination.IdentityValid() ||
			(!m_Job.m_bLoaded && (!m_Job.m_Source || !m_Job.m_Source.IdentityValid())))) return false;
		if (SCR_Faction.GetEntityFaction(m_Vehicle) != w.m_Faction) return false;
		SCR_AIVehicleUsageComponent usage = SCR_AIVehicleUsageComponent.Cast(m_Vehicle.FindComponent(SCR_AIVehicleUsageComponent));
		if (!usage || usage.GetDamageState() == EDamageState.DESTROYED || SCR_AIVehicleUsability.VehicleIsOnFire(m_Vehicle)) return false;
		CompartmentAccessComponent access = m_Driver.GetCompartmentAccessComponent();
		IEntity linked = CompartmentAccessComponent.GetVehicleIn(m_Driver);
		return access && (!linked || linked == m_Vehicle) && (!access.GetCompartment() || access.GetCompartment() == m_Seat) &&
			vector.Distance(m_Driver.GetOrigin(), m_Vehicle.GetOrigin()) <= 100;
	}

	override string DeadlineReason(int now)
	{
		if (now - m_iStartedAtMs >= TIMEOUT_MS) return "LOGISTICS_FALLBACK_DEADLINE";
		return string.Empty;
	}

	override bool CanRenew(AICF_LogisticsWorker w, int now)
	{
		return Safe(w, now) && now - m_iSampleAtMs < 5000;
	}

	override AICF_TripOutcome Poll(AICF_LogisticsWorker w, int now)
	{
		if (!Safe(w, now)) return AICF_TripOutcome.TerminalFailClosed("LOGISTICS_FALLBACK_CONTEXT_LOST", m_sToken);
		w.m_iDriverWaitMs += now - m_iSampleAtMs;
		m_iSampleAtMs = now;
		w.m_iStationaryAtMs = 0;
		if (w.Ready() && (!m_bRelocate || m_bRelocated)) return AICF_TripOutcome.CompleteTrip("LOGISTICS_FALLBACK_EXACT_READY", m_sToken);
		return AICF_TripOutcome.Wait("LOGISTICS_FALLBACK_PENDING", m_sToken);
	}

	// Пользователь разрешил видимые переносы логистики. Камера/LoS не являются
	// veto; близкое физическое использование игроком остаётся защищённым.
	static bool PlayersClear(vector source, vector destination)
	{
		PlayerManager players = GetGame().GetPlayerManager();
		if (!Replication.IsServer() || !players) return false;
		array<int> ids = {};
		players.GetAllPlayers(ids);
		foreach (int id : ids)
		{
			IEntity controlled = players.GetPlayerControlledEntity(id);
			IEntity main = SCR_PossessingManagerComponent.GetPlayerMainEntity(id);
			if (controlled && (vector.Distance(controlled.GetOrigin(), source) < 8 || vector.Distance(controlled.GetOrigin(), destination) < 8)) return false;
			if (main && (vector.Distance(main.GetOrigin(), source) < 8 || vector.Distance(main.GetOrigin(), destination) < 8)) return false;
		}
		return true;
	}

	bool Candidate(AICF_LogisticsWorker w, int index, out vector pose[4])
	{
		SCR_AIWorld ai = SCR_AIWorld.Cast(GetGame().GetAIWorld());
		if (!ai || !ai.GetRoadNetworkManager() || !w.m_Entry) return false;
		AICF_LogisticsVehicleFootprint footprint = AICF_LogisticsVehicleFootprint.Get(w.m_Entry.GetPrefab());
		if (!footprint || !footprint.m_bValid) return false;
		vector side = Vector(m_vBypassDirection[2], 0, -m_vBypassDirection[0]);
		int ring = index / 5;
		int bearing = index % 5;
		float offset;
		if (bearing == 1) offset = 15;
		if (bearing == 2) offset = -15;
		if (bearing == 3) offset = 30;
		if (bearing == 4) offset = -30;
		vector origin = m_Vehicle.GetOrigin();
		vector goal = origin + m_vBypassDirection * (20 + ring * 20) + side * offset;
		vector road;
		// Начальная точка query лежит за препятствием: закрытый navlink у машины
		// не должен запрещать сам перенос через него.
		if (!ai.GetRoadNetworkManager().GetReachableWaypointInRoad(goal, goal, 20, road)) return false;
		float displacement = vector.DistanceXZ(origin, road);
		if (displacement < 12 || displacement > 90 || vector.DistanceXZ(road, goal) > 20 ||
			vector.DistanceXZ(road, w.m_vEndpoint) < 25) return false;
		BaseWorld world = m_Vehicle.GetWorld();
		vector forward = w.m_vEndpoint - road;
		forward[1] = 0;
		forward.Normalize();
		vector up = Vector(world.GetSurfaceY(road[0] - 1, road[2]) - world.GetSurfaceY(road[0] + 1, road[2]), 2,
			world.GetSurfaceY(road[0], road[2] - 1) - world.GetSurfaceY(road[0], road[2] + 1));
		up.Normalize();
		if (up[1] < 0.92) return false;
		forward[1] = -(up[0] * forward[0] + up[2] * forward[2]) / up[1];
		forward.Normalize();
		Math3D.DirectionAndUpMatrix(forward, up, pose);
		pose[3] = road;
		pose[3][1] = world.GetSurfaceY(road[0], road[2]);
		if (!AICF_LogisticsSpawnGeometry.FitToSurface(world, footprint, pose)) return false;
		TraceOBB body;
		return footprint.IsClear(world, pose, body);
	}
}
