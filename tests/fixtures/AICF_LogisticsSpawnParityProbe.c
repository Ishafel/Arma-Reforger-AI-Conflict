// Test-only: вместе с LogisticsRuntimeProbe в изолированной копии addon graph.
modded class AICF_ConstructionSiteSearch
{
	override int Step(AICF_ConstructionOrder order, AIPathfindingComponent pathfinding)
	{
		string directSetup;
		if (order && order.m_sToken.IndexOf("logistics-fixture-") == 0 && order.m_eType == AICF_EConstructionType.HEAVY_DEPOT &&
			System.GetCLIParam("aicfParityDirectDepotSetup", directSetup) && directSetup == "1")
		{
			// BeginCandidate уже проверил bounds и live collision композиции.
			// Это setup теста vehicle admission, не проверка строительства:
			// terrain grid, прямые exits и путь строителя здесь не проверяются.
			// Production vehicle surface/occupancy/reservation gates остаются.
			Print(string.Format("[AICF][SPAWN_PARITY_DEPOT_SETUP] test_only=1 token=%1 position=%2 bypass=construction_terrain_exits_worker_path vehicle_admission_unchanged=1", order.m_sToken, order.m_aTransform[3]));
			order.m_iStage = 3;
			order.m_sReason = "NAVMESH_UNAVAILABLE";
			return -1;
		}
		return super.Step(order, pathfinding);
	}
}

modded class SCR_EntitySpawnerSlotComponent
{
	bool AICF_ParityStockClear()
	{
		SCR_EntitySpawnerSlotComponentClass data = SCR_EntitySpawnerSlotComponentClass.Cast(GetComponentData(GetOwner()));
		if (!data) return false;
		FillExcludedEntities();
		TraceOBB trace = new TraceOBB();
		GetOwner().GetWorldTransform(trace.Mat);
		trace.Start = GetOwner().GetOrigin();
		trace.Mins = data.GetMinBoundsVector();
		trace.Maxs = data.GetMaxBoundsVector();
		trace.LayerMask = EPhysicsLayerPresets.Projectile;
		trace.Flags = TraceFlags.ENTS;
		trace.ExcludeArray = m_aExcludedEntities;
		GetGame().GetWorld().TracePosition(trace, AICF_ParityStockCallback);
		Print(string.Format("[AICF][SPAWN_PARITY_STOCK_TRACE] test_only=1 slot=%1 blocker=%2 min=%3 max=%4 position=%5", GetOwner().GetID(), trace.TraceEnt, trace.Mins, trace.Maxs, trace.Start));
		return !trace.TraceEnt;
	}

	protected bool AICF_ParityStockCallback(IEntity entity)
	{
		// Stock predicates без удаления wreck. Null physics не разыменовывается.
		if (entity.IsLoaded()) return false;
		Physics physics = entity.GetPhysics();
		if (!physics) return true;
		if (physics.GetSimulationState() == 0 || entity.FindComponent(BaseLoadoutClothComponent) || entity.FindComponent(WeaponComponent)) return false;
		if ((entity.GetFlags() & EntityFlags.PROXY) || ChimeraCharacter.Cast(entity)) return false;
		if (IsEntityDestroyed(entity) && Vehicle.Cast(entity) && m_RplComponent && !m_RplComponent.IsProxy()) return false;
		return true;
	}
}
// Production spawn, slot selection и reservations вызываются без подмены.
modded class AICF_VehicleSpawner
{
	protected ref array<string> m_aAICFParityProbed = {};
	protected int m_iAICFSpawnClearanceChecks;
	protected int m_iAICFSpawnClearancePassed;

	protected void AICF_CheckSpawnClearance(string name, bool passed)
	{
		m_iAICFSpawnClearanceChecks++;
		if (passed) m_iAICFSpawnClearancePassed++;
		Print(string.Format("[AICF][LOGISTICS_PROBE_SPAWN_CLEARANCE_CASE] test_only=1 name=%1 passed=%2", name, passed));
	}

	protected IEntity AICF_SpawnClearanceBox(AICF_LogisticsWorker w, vector localPosition, vector size)
	{
		EntitySpawnParams params = new EntitySpawnParams();
		params.TransformMode = ETransformMode.WORLD;
		for (int axis; axis < 4; axis++) params.Transform[axis] = w.m_aSpawnTransform[axis];
		params.Transform[3] = w.m_aSpawnTransform[3] + w.m_aSpawnTransform[0] * localPosition[0] +
			w.m_aSpawnTransform[1] * localPosition[1] + w.m_aSpawnTransform[2] * localPosition[2];
		IEntity entity = GetGame().SpawnEntity(GenericEntity, GetGame().GetWorld(), params);
		if (!entity) return null;
		entity.SetFlags(EntityFlags.TRACEABLE, false);
		ref PhysicsGeomDef geoms[] = {PhysicsGeomDef("spawn-probe", PhysicsGeom.CreateBox(size), "{D745FD8FC67DB26A}Common/Materials/Game/stone.gamemat", 0xffffffff)};
		if (!Physics.CreateStaticEx(entity, geoms))
		{
			delete entity;
			return null;
		}
		return entity;
	}

	protected void AICF_ProbeSpawnBody(AICF_LogisticsWorker w)
	{
		AICF_LogisticsVehicleFootprint footprint = AICF_LogisticsVehicleFootprint.Get(w.m_Entry.GetPrefab());
		vector center = (footprint.m_vMin + footprint.m_vMax) * 0.5;
		AICF_CheckSpawnClearance("free_body", LogisticsSpawnClear(w));

		// Свободный кузов не разрешает spawn с перекрытым выездом.
		IEntity front = AICF_SpawnClearanceBox(w, Vector(center[0], center[1], footprint.m_vMax[2] + 6), "8 4 1");
		IEntity rear = AICF_SpawnClearanceBox(w, Vector(center[0], center[1], footprint.m_vMin[2] - 6), "8 4 1");
		AICF_CheckSpawnClearance("both_exits_blocked_rejected", front && rear && !LogisticsSpawnClear(w));
		if (front) delete front;
		AICF_CheckSpawnClearance("rear_blocked_forward_clear", rear && LogisticsSpawnClear(w));
		if (rear) delete rear;

		IEntity nearby = AICF_SpawnClearanceBox(w, Vector(footprint.m_vMax[0] + 0.5, center[1], center[2]), "0.2 4 8");
		AICF_CheckSpawnClearance("nearby_fence_outside_body", nearby && LogisticsSpawnClear(w));
		if (nearby) delete nearby;

		IEntity canopy = AICF_SpawnClearanceBox(w, Vector(center[0], footprint.m_vMax[1] + 0.5, center[2]), "8 0.2 12");
		AICF_CheckSpawnClearance("canopy_above_body", canopy && LogisticsSpawnClear(w));
		if (canopy) delete canopy;

		TraceOBB overlap;
		IEntity solid = AICF_SpawnClearanceBox(w, center, "2 4 2");
		AICF_CheckSpawnClearance("solid_inside_body_rejected", solid && !LogisticsSpawnClear(w));
		AICF_CheckSpawnClearance("selected_pose_occupied_rejected", solid && !footprint.IsClear(GetGame().GetWorld(), w.m_aSpawnTransform, overlap));
		if (solid)
		{
			w.m_Depot.AddChild(solid, -1, EAddChildFlags.RECALC_LOCAL_TRANSFORM);
			solid.Update();
			AICF_CheckSpawnClearance("own_depot_physical_child_rejected", !footprint.IsClear(GetGame().GetWorld(), w.m_aSpawnTransform, overlap));
			w.m_Depot.RemoveChild(solid, true);
			delete solid;
		}
		AICF_CheckSpawnClearance("body_clear_after_obstacle_removed", LogisticsSpawnClear(w));

		// Второй worker того же depot не может занять уже зарезервированный slot.
		AICF_LogisticsWorker competitor = new AICF_LogisticsWorker();
		competitor.m_Faction = w.m_Faction;
		competitor.m_Production = w.m_Production;
		competitor.m_Entry = w.m_Entry;
		competitor.m_Depot = w.m_Depot;
		competitor.m_DepotId = w.m_DepotId;
		competitor.m_iSlot = w.m_iSlot + 100;
		competitor.m_iGeneration = w.m_iGeneration;
		competitor.m_Home = w.m_Home;
		competitor.m_SpawnSlot = w.m_SpawnSlot;
		competitor.m_SpawnSlotId = w.m_SpawnSlotId;
		for (int axis; axis < 4; axis++)
		{
			competitor.m_aSlotTransform[axis] = w.m_aSlotTransform[axis];
			competitor.m_aSpawnTransform[axis] = w.m_aSpawnTransform[axis];
		}
		AICF_CheckSpawnClearance("second_worker_reserved_pose_rejected", !LogisticsSpawnClear(competitor) && competitor.m_sSpawnRejectReason == "SHARED_SITE_RESERVED");
	}

	override bool SpawnLogistics(AICF_LogisticsWorker w, AICF_LogisticsResourceAdapter resources)
	{
		if (!w || !w.m_Entry || !w.m_Site || !LogisticsSpawnClear(w)) return super.SpawnLogistics(w, resources);
		string probeKey = string.Format("%1|%2", w.m_Entry.GetPrefab(), w.m_SpawnSlotId);
		if (m_aAICFParityProbed.Contains(probeKey)) return super.SpawnLogistics(w, resources);
		m_aAICFParityProbed.Insert(probeKey);
		AICF_LogisticsVehicleFootprint beforeFootprint = AICF_LogisticsVehicleFootprint.Get(w.m_Entry.GetPrefab());
		TraceOBB legacy;
		bool legacyClear = beforeFootprint.IsClear(GetGame().GetWorld(), w.m_aSpawnTransform, legacy);
		Print(string.Format("[AICF][SPAWN_PARITY_STOCK] prefab=%1 slot=%2 stock_readonly_clear=%3 ai_clear=%4 world_position=%5", w.m_Entry.GetPrefab(), w.m_SpawnSlotId, w.m_SpawnSlot.AICF_ParityStockClear(), LogisticsSpawnClear(w), w.m_aSpawnTransform[3]));
		AICF_ProbeSpawnBody(w);
		vector center = (beforeFootprint.m_vMin + beforeFootprint.m_vMax) * 0.5;
		IEntity lateObstacle = AICF_SpawnClearanceBox(w, center, "2 4 2");
		AICF_CheckSpawnClearance("occupancy_changed_before_commit", lateObstacle && !super.SpawnLogistics(w, resources) && !w.m_Vehicle);
		if (lateObstacle) delete lateObstacle;
		EntityID depotId = w.m_DepotId;
		w.m_DepotId = EntityID.INVALID;
		AICF_CheckSpawnClearance("stale_depot_before_commit", !super.SpawnLogistics(w, resources) && !w.m_Vehicle);
		w.m_DepotId = depotId;
		// Меняется реальная принадлежность production entity, не snapshot worker.
		SCR_FactionAffiliationComponent productionFaction = SCR_FactionAffiliationComponent.Cast(w.m_Production.GetOwner().FindComponent(SCR_FactionAffiliationComponent));
		Faction previousFaction;
		Faction otherFaction = GetGame().GetFactionManager().GetFactionByKey("USSR");
		if (otherFaction == w.m_Faction) otherFaction = GetGame().GetFactionManager().GetFactionByKey("US");
		bool ownerChangeRejected;
		if (productionFaction && otherFaction)
		{
			previousFaction = productionFaction.GetAffiliatedFaction();
			productionFaction.SetAffiliatedFaction(otherFaction);
			ownerChangeRejected = w.m_Production.GetFaction() == otherFaction && !super.SpawnLogistics(w, resources) && !w.m_Vehicle;
			productionFaction.SetAffiliatedFaction(previousFaction);
		}
		AICF_CheckSpawnClearance("live_production_owner_changed_before_commit", ownerChangeRejected && AICF_LogisticsDepotRegistry.Live(w));
		bool spawned = super.SpawnLogistics(w, resources);
		AICF_CheckSpawnClearance("production_vehicle_spawned", spawned && w.m_Vehicle != null);
		AICF_LogisticsVehicleFootprint footprint = AICF_LogisticsVehicleFootprint.Get(w.m_Entry.GetPrefab());
		TraceOBB body;
		AICF_CheckSpawnClearance("actual_vehicle_overlap_rejected", w.m_Vehicle && !footprint.IsClear(GetGame().GetWorld(), w.m_aSpawnTransform, body));
		AICF_CheckSpawnClearance("duplicate_spawn_rejected", w.m_Vehicle && !super.SpawnLogistics(w, resources));
		Print(string.Format("[AICF][LOGISTICS_PROBE_SPAWN_CLEARANCE_CONTRACT] test_only=1 passed=%1 total=%2 prefab=%3 slot=%4", m_iAICFSpawnClearancePassed, m_iAICFSpawnClearanceChecks, w.m_Entry.GetPrefab(), w.m_iSlot));
		return spawned;
	}
}
