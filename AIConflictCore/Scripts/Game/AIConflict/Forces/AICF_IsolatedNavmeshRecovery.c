// Локальная коррекция единственного бойца на малом изолированном navmesh.
// Не выбирает стратегическую цель, не создаёт roster и не владеет waypoint.
class AICF_IsolatedNavmeshRecovery
{
	static bool TryRecover(AICF_GroupSlot slot, AICF_VehicleWatchdog watchdog, AICF_Stage3Config config)
	{
		if (!Replication.IsServer() || !slot || !slot.IsCombatReady() ||
			slot.IsRecruitingInfantry() || slot.HasUsedIsolatedNavmeshRecovery() ||
			!watchdog || !config || !config.GetHiddenRecoveryEnabled())
			return false;
		SCR_AIGroup group = slot.GetGroup();
		if (!group || AICF_GroupRuntime.CountAliveAgents(group) != 1)
			return false;
		ChimeraCharacter character = ChimeraCharacter.Cast(AICF_GroupRuntime.ResolveAliveLeader(group));
		if (!AICF_VehicleBoardingMutationFence.IsAuthoritativeAIEntity(character) ||
			CompartmentAccessComponent.GetVehicleIn(character))
			return false;
		CompartmentAccessComponent access = CompartmentAccessComponent.Cast(character.FindComponent(CompartmentAccessComponent));
		if (!access || access.IsGettingIn() || access.IsGettingOut())
			return false;
		float threat;
		if (!watchdog.IsHiddenRecoveryCombatSafe(group, threat))
			return false;
		AIPathfindingComponent path = AIPathfindingComponent.Cast(group.FindComponent(AIPathfindingComponent));
		if (!path || !path.GetNavmeshComponent())
			return false;
		NavmeshWorldComponent navmesh = path.GetNavmeshComponent();
		vector origin = character.GetOrigin();
		if (!navmesh.IsTileLoaded(origin))
			return false;
		vector snappedOrigin;
		if (!path.GetClosestPositionOnNavmesh(origin, "2 2 2", snappedOrigin))
			return false;
		// Ошибка/непрогруженный tile не доказывают изоляцию. Все успешные
		// probes должны остаться в пределах пяти метров от исходного полигона.
		for (int probe; probe < 8; probe++)
		{
			vector reachable;
			if (!navmesh.GetReachablePoint(snappedOrigin, 40, reachable) ||
				vector.DistanceXZ(snappedOrigin, reachable) > 5)
				return false;
		}
		int generation = slot.GetSpawnGeneration();
		int revision = slot.GetStrategicAssignmentRevision();
		AIWaypoint waypoint = slot.GetWaypoint();
		EntityID characterId = character.GetID();
		BaseWorld world = GetGame().GetWorld();
		int connectedCandidates;
		int blockedCandidates;
		// Детерминированный поиск от ближнего кольца к дальнему; никаких
		// привязок к карте, переносов к базе или пересечения стен.
		for (int ring; ring < 3; ring++)
		{
			float radius = 3 + ring * 2;
			for (int direction; direction < 8; direction++)
			{
				float angle = direction * 45 * Math.DEG2RAD;
				vector query = origin + Vector(Math.Cos(angle) * radius, 0, Math.Sin(angle) * radius);
				query[1] = world.GetSurfaceY(query[0], query[2]);
				vector destination;
				if (!navmesh.IsTileLoaded(query) ||
					!path.GetClosestPositionOnNavmesh(query, "1 2 1", destination) ||
					vector.DistanceXZ(origin, destination) < 2 ||
					vector.DistanceXZ(origin, destination) > 8 ||
					Math.AbsFloat(destination[1] - origin[1]) > 1.5 ||
					ChimeraWorldUtils.TryGetWaterSurfaceSimple(world, destination))
					continue;
				vector exitPoint;
				if (!navmesh.GetReachablePoint(destination, 40, exitPoint) ||
					vector.DistanceXZ(destination, exitPoint) < 25)
					continue;
				connectedCandidates++;
				if (!IsPhysicalPathClear(world, character, origin, destination))
				{
					blockedCandidates++;
					continue;
				}
				float nearestPlayer;
				string rejection;
				if (!watchdog.CanApplyHiddenRecovery(origin, destination,
					config.GetHiddenRecoveryPlayerRadiusMeters(), nearestPlayer, rejection))
				{
					AICF_Stage2Diagnostics.Info("ISOLATED_NAVMESH_RECOVERY_BLOCKED",
						string.Format("slot=%1 group_generation=%2 reason=%3", slot.GetSlotId(), generation, rejection));
					return false;
				}
				// Повторный identity/occupancy/combat/clearance fence непосредственно
				// перед единственной мутацией. Неуспешная postcondition не даёт retry.
				if (slot.GetGroup() != group || slot.GetSpawnGeneration() != generation ||
					slot.GetStrategicAssignmentRevision() != revision || slot.GetWaypoint() != waypoint ||
					character.GetID() != characterId || AICF_GroupRuntime.ResolveAliveLeader(group) != character ||
					AICF_GroupRuntime.CountAliveAgents(group) != 1 || character.GetOrigin() != origin ||
					!AICF_VehicleBoardingMutationFence.IsAuthoritativeAIEntity(character) ||
					CompartmentAccessComponent.GetVehicleIn(character) || access.IsGettingIn() || access.IsGettingOut() ||
					!watchdog.IsHiddenRecoveryCombatSafe(group, threat) ||
					!IsPhysicalPathClear(world, character, origin, destination))
					return false;
				slot.MarkIsolatedNavmeshRecoveryUsed();
				vector transform[4];
				character.GetWorldTransform(transform);
				transform[3] = destination;
				character.Teleport(transform);
				AICF_Stage2Diagnostics.Warning("ISOLATED_NAVMESH_RECOVERY_APPLIED",
					string.Format("faction=%1 slot=%2 group_generation=%3 assignment_revision=%4 entity=%5 before=%6 destination=%7 after=%8",
						group.GetFaction().GetFactionKey(), slot.GetSlotId(), generation, revision,
						characterId, origin, destination, character.GetOrigin()) + " movement_confirmation=PENDING max_shift_m=8");
				return vector.Distance(character.GetOrigin(), destination) <= 1;
			}
		}
		AICF_Stage2Diagnostics.Info("ISOLATED_NAVMESH_RECOVERY_BLOCKED",
			string.Format("slot=%1 group_generation=%2 reason=NO_CLEAR_LOCAL_EXIT connected_candidates=%3 blocked_candidates=%4",
				slot.GetSlotId(), generation, connectedCandidates, blockedCandidates));
		return false;
	}

	protected static bool IsPhysicalPathClear(BaseWorld world, IEntity character, vector origin, vector destination)
	{
		TraceBox trace = new TraceBox();
		trace.Start = origin + "0 0.15 0";
		trace.End = destination + "0 0.15 0";
		trace.Mins = "-0.3 0 -0.3";
		trace.Maxs = "0.3 1.6 0.3";
		trace.Exclude = character;
		trace.Flags = TraceFlags.ENTS | TraceFlags.WORLD | TraceFlags.OCEAN;
		trace.LayerMask = EPhysicsLayerPresets.Projectile;
		if (world.TraceMove(trace, null) < 1)
			return false;
		trace.Start = trace.End;
		return world.TracePosition(trace, null) >= 0;
	}
}
