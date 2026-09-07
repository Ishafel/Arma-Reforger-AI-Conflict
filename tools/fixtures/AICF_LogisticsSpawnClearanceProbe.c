// Test-only: вместе с LogisticsRuntimeProbe в изолированной копии addon graph.
// Production spawn, slot selection и reservations вызываются без подмены.
modded class AICF_VehicleSpawner
{
	protected bool m_bAICFSpawnClearanceProbed;
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
		if (m_bAICFSpawnClearanceProbed || !w || !w.m_Site || !LogisticsSpawnClear(w)) return super.SpawnLogistics(w, resources);
		m_bAICFSpawnClearanceProbed = true;
		AICF_ProbeSpawnBody(w);
		bool spawned = super.SpawnLogistics(w, resources);
		AICF_CheckSpawnClearance("production_vehicle_spawned", spawned && w.m_Vehicle != null);
		AICF_LogisticsVehicleFootprint footprint = AICF_LogisticsVehicleFootprint.Get(w.m_Entry.GetPrefab());
		TraceOBB body;
		AICF_CheckSpawnClearance("actual_vehicle_overlap_rejected", w.m_Vehicle && !footprint.IsClear(GetGame().GetWorld(), w.m_aSpawnTransform, body));
		Print(string.Format("[AICF][LOGISTICS_PROBE_SPAWN_CLEARANCE_CONTRACT] test_only=1 passed=%1 total=%2 prefab=%3 slot=%4", m_iAICFSpawnClearancePassed, m_iAICFSpawnClearanceChecks, w.m_Entry.GetPrefab(), w.m_iSlot));
		return spawned;
	}
}
