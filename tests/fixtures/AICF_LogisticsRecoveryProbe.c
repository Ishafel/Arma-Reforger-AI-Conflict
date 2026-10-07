// Test-only. Требует LogisticsRuntimeProbe; никаких client hooks.
modded class AICF_VehicleSpawner
{
	protected ref array<IEntity> m_aAICFRecoveryObstacles = {};
	protected ref set<string> m_aAICFRecoveryFactions = new set<string>();

	override bool SpawnLogistics(AICF_LogisticsWorker w, AICF_LogisticsResourceAdapter resources)
	{
		bool spawned = super.SpawnLogistics(w, resources);
		string mode;
		if (!spawned || !System.GetCLIParam("aicfLogisticsRecoveryProbe", mode) || (mode != "obstacle" && mode != "both") || m_aAICFRecoveryFactions.Contains(w.m_Faction.GetFactionKey())) return spawned;
		m_aAICFRecoveryFactions.Insert(w.m_Faction.GetFactionKey());
		AICF_LogisticsVehicleFootprint footprint = AICF_LogisticsVehicleFootprint.Get(w.m_Entry.GetPrefab());
		for (int side; side < 2; side++)
		{
			float offset = footprint.m_vMax[2] + 6;
			if (side == 1) offset = footprint.m_vMin[2] - 6;
			EntitySpawnParams params = new EntitySpawnParams();
			params.TransformMode = ETransformMode.WORLD;
			for (int axis; axis < 4; axis++) params.Transform[axis] = w.m_aSpawnTransform[axis];
			params.Transform[3] = w.m_aSpawnTransform[3] + w.m_aSpawnTransform[2] * offset + w.m_aSpawnTransform[1] * 2;
			IEntity obstacle = GetGame().SpawnEntity(GenericEntity, GetGame().GetWorld(), params);
			if (!obstacle) continue;
			obstacle.SetFlags(EntityFlags.TRACEABLE, false);
			ref PhysicsGeomDef geoms[] = {PhysicsGeomDef("recovery-probe", PhysicsGeom.CreateBox("8 4 1"), "{D745FD8FC67DB26A}Common/Materials/Game/stone.gamemat", 0xffffffff)};
			if (!Physics.CreateStaticEx(obstacle, geoms)) { delete obstacle; continue; }
			m_aAICFRecoveryObstacles.Insert(obstacle);
			w.Log("LOGISTICS_PROBE_RECOVERY_OBSTACLE", string.Format("test_only=1 reason=FINITE_FENCE_SIDES_OPEN obstacle=%1 position=%2 spawn_slot=%3", obstacle.GetID(), obstacle.GetOrigin(), w.m_SpawnSlotId));
		}
		return spawned;
	}

	void ~AICF_VehicleSpawner()
	{
		foreach (IEntity obstacle : m_aAICFRecoveryObstacles)
		{
			if (obstacle) delete obstacle;
		}
	}
}

modded class AICF_TransportTripController
{
	protected ref set<string> m_aAICFRecoveryExit = new set<string>();
	protected bool m_bAICFRecoveryContracts;

	override bool BeginLogisticsLeg(AICF_LogisticsWorker w, vector endpoint, AICF_ELogisticsPhase phase)
	{
		bool begun = super.BeginLogisticsLeg(w, endpoint, phase);
		string mode;
		if (!begun || !System.GetCLIParam("aicfLogisticsRecoveryProbe", mode)) return begun;
		if (!m_bAICFRecoveryContracts)
		{
			m_bAICFRecoveryContracts = true;
			int passed;
			int now = System.GetTickCount();
			AICF_LogisticsDriverInteraction context = new AICF_LogisticsDriverInteraction();
			context.Capture(w, now);
			if (context.Matches(w)) passed++;
			context.m_iGeneration++;
			if (!context.Matches(w)) passed++;
			context.Capture(w, now);
			context.m_sToken = "STALE_TOKEN";
			if (!context.Matches(w)) passed++;
			context.Capture(w, now);
			context.m_VehicleId = EntityID.INVALID;
			if (!context.Matches(w)) passed++;
			context.Capture(w, now);
			w.m_bStopped = true;
			if (!context.Matches(w)) passed++;
			w.m_bStopped = false;
			AICF_LogisticsDriverRecovery unknown = new AICF_LogisticsDriverRecovery();
			unknown.Capture(w, now);
			if (unknown.DeadlineReason(now + 59999).IsEmpty()) passed++;
			if (unknown.DeadlineReason(now + 60000) == "UNKNOWN_DRIVER_RECOVERY_DEADLINE") passed++;
			unknown.m_iWaitBeforeMs = 119000;
			if (unknown.DeadlineReason(now + 1000) == "DRIVER_INTERACTION_LEG_BUDGET") passed++;
			AICF_LogisticsRouteRecovery route = new AICF_LogisticsRouteRecovery();
			route.m_Context = context;
			route.m_bActive = true;
			route.m_iStartedAtMs = now;
			if (route.CanRenew(w, now)) passed++;
			if (!route.CanRenew(w, now + AICF_LogisticsConfig.RECOVERY_BUDGET_MS)) passed++;
			w.Log("LOGISTICS_PROBE_RECOVERY_CONTRACT", string.Format("test_only=1 passed=%1 total=10 physical_recovery=0", passed));
		}
		if ((mode == "exit" || mode == "both") && !m_aAICFRecoveryExit.Contains(w.m_Faction.GetFactionKey()) && w.Ready())
		{
			m_aAICFRecoveryExit.Insert(w.m_Faction.GetFactionKey());
			bool accepted = w.m_Driver.GetCompartmentAccessComponent().GetOutVehicle(EGetOutType.ANIMATED, -1, ECloseDoorAfterActions.CLOSE_DOOR, true);
			w.Log("LOGISTICS_PROBE_RECOVERY_EXIT", string.Format("test_only=1 reason=ANIMATED_EXIT_CAUSE_INJECTED accepted=%1", accepted));
		}
		return begun;
	}
}
