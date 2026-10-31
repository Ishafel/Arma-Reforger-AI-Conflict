// Только локальные строки/значения: eligibility snapshot не удерживает engine entities.
class AICF_VehicleFootPadReadback
{
	string m_sReason;
	string m_sGroup;
	vector m_vOrigin;
	ref array<string> m_aSamples = {};

	bool IsSuitable()
	{
		return m_sReason.IsEmpty() && m_aSamples.Count() == 9;
	}

	void Report(string identity, string phase)
	{
		string reason = m_sReason;
		if (reason.IsEmpty()) reason = "FOOT_PAD_COVERED";
		string context = identity + string.Format(" sample_phase=%1 group=%2 origin=%3 reason=%4 readonly=1 reachability_proven=0",
			phase, m_sGroup, m_vOrigin, reason);
		AICF_Stage3Diagnostics.Info("VEHICLE_FOOT_PAD_PROBE", context + string.Format(" samples=%1 suitable=%2", m_aSamples.Count(), IsSuitable()));
		foreach (string sample : m_aSamples)
			AICF_Stage3Diagnostics.Info("VEHICLE_FOOT_PAD_POINT", context + sample);
	}
}

// Проверяет пешее покрытие площадки; это не доказательство пути от бойцов к двери.
class AICF_VehicleBoardingSiteQuery
{
	static const float PAD_RADIUS_METERS = 4;
	static const float MAXIMUM_PROJECTION_XZ_METERS = 0.5;
	static const float MAXIMUM_PROJECTION_HEIGHT_METERS = 1;
	static const int MAXIMUM_ENTRY_DOORS = 16;

	protected static bool IsCurrent(SCR_AIGroup group, EntityID groupId, RplComponent rpl, string componentRpl,
		Faction faction, BaseWorld world, AIPathfindingComponent pathfinding, NavmeshWorldComponent navmesh)
	{
		if (!Replication.IsServer() || !group || group.GetID() != groupId) return false;
		if (!AICF_VehicleBoardingMutationFence.IsAuthoritativeReplicatedEntity(group)) return false;
		if (!rpl || group.FindComponent(RplComponent) != rpl || rpl.Id().ToString() != componentRpl) return false;
		if (group.GetFaction() != faction || group.GetWorld() != world) return false;
		if (!pathfinding || group.FindComponent(AIPathfindingComponent) != pathfinding) return false;
		return navmesh && pathfinding.GetNavmeshComponent() == navmesh;
	}

	static AICF_VehicleFootPadReadback Capture(SCR_AIGroup group, vector position)
	{
		AICF_VehicleFootPadReadback result = new AICF_VehicleFootPadReadback();
		result.m_sGroup = AICF_GroupTrace.Entity(group);
		result.m_vOrigin = position;
		result.m_sReason = "FOOT_PAD_GROUP_CONTEXT_UNAVAILABLE";
		if (!Replication.IsServer() || !group ||
			!AICF_VehicleBoardingMutationFence.IsAuthoritativeReplicatedEntity(group)) return result;
		RplComponent rpl = RplComponent.Cast(group.FindComponent(RplComponent));
		AIPathfindingComponent pathfinding = AIPathfindingComponent.Cast(group.FindComponent(AIPathfindingComponent));
		BaseWorld world = group.GetWorld();
		if (!rpl || !world || !pathfinding || !group.GetFaction()) return result;
		NavmeshWorldComponent navmesh = pathfinding.GetNavmeshComponent();
		result.m_sReason = "FOOT_PAD_GROUP_NAVMESH_UNAVAILABLE";
		if (!navmesh) return result;
		EntityID groupId = group.GetID();
		string componentRpl = rpl.Id().ToString();
		Faction faction = group.GetFaction();
		array<vector> offsets = { "0 0 0", "1 0 0", "-1 0 0", "0 0 1", "0 0 -1",
			"1 0 1", "1 0 -1", "-1 0 1", "-1 0 -1" };
		result.m_sReason = string.Empty;
		for (int sample; sample < offsets.Count(); sample++)
		{
			vector query = position + offsets[sample] * PAD_RADIUS_METERS;
			query[1] = world.GetSurfaceY(query[0], query[2]);
			bool loaded = navmesh.IsTileLoaded(query);
			bool requested = navmesh.IsTileRequested(query);
			vector projected;
			bool found = loaded && pathfinding.GetClosestPositionOnNavmesh(query, "2 2 2", projected);
			float delta = -1;
			float heightDelta = -1;
			string projection = "NONE";
			string reason = "COVERED";
			if (!loaded) reason = "FOOT_PAD_TILE_UNAVAILABLE";
			else if (!found) reason = "FOOT_PAD_PROJECTION_UNAVAILABLE";
			if (found)
			{
				delta = vector.DistanceXZ(query, projected);
				heightDelta = projected[1] - query[1];
				projection = projected.ToString();
				if (delta > MAXIMUM_PROJECTION_XZ_METERS) reason = "FOOT_PAD_PROJECTION_GAP";
				else if (Math.AbsFloat(heightDelta) > MAXIMUM_PROJECTION_HEIGHT_METERS) reason = "FOOT_PAD_PROJECTION_HEIGHT";
			}
			if (reason != "COVERED" && result.m_sReason.IsEmpty()) result.m_sReason = reason;
			result.m_aSamples.Insert(string.Format(" sample=%1 query=%2 tile_loaded=%3 tile_requested=%4 projection_found=%5 projection=%6 delta_xz_m=%7 height_delta_m=%8 result=%9",
				sample, query, loaded, requested, found, projection, delta, heightDelta, reason));
		}
		if (!IsCurrent(group, groupId, rpl, componentRpl, faction, world, pathfinding, navmesh))
		{
			result.m_sReason = "FOOT_PAD_CONTEXT_CHANGED";
			result.m_aSamples.Clear();
		}
		return result;
	}

	// Immediate sample предшествует возможному asynchronous navmesh carving машины.
	// Поздние ENTRY_PATH snapshots сравниваются с ним; ни один не доказывает BT cause.
	static void ReportSpawnedEntries(SCR_AIGroup group, Vehicle vehicle, string identity)
	{
		if (!Replication.IsServer() || !group || !vehicle) return;
		if (!AICF_VehicleBoardingMutationFence.IsAuthoritativeReplicatedEntity(group) ||
			!AICF_VehicleBoardingMutationFence.IsAuthoritativeReplicatedEntity(vehicle)) return;
		RplComponent groupRpl = RplComponent.Cast(group.FindComponent(RplComponent));
		RplComponent vehicleRpl = RplComponent.Cast(vehicle.FindComponent(RplComponent));
		AIPathfindingComponent pathfinding = AIPathfindingComponent.Cast(group.FindComponent(AIPathfindingComponent));
		if (!groupRpl || !vehicleRpl || !pathfinding || !group.GetFaction()) return;
		NavmeshWorldComponent navmesh = pathfinding.GetNavmeshComponent();
		BaseWorld world = group.GetWorld();
		if (!navmesh || !world) return;
		EntityID groupId = group.GetID();
		string groupComponentRpl = groupRpl.Id().ToString();
		Faction faction = group.GetFaction();
		EntityID vehicleId = vehicle.GetID();
		string vehicleComponentRpl = vehicleRpl.Id().ToString();
		vector position = vehicle.GetOrigin();
		vector angles = vehicle.GetYawPitchRoll();
		array<BaseCompartmentSlot> seats = {};
		AICF_VehiclePreset.Collect(vehicle, seats);
		array<string> keys = {};
		array<string> samples = {};
		foreach (BaseCompartmentSlot seat : seats)
		{
			BaseCompartmentManagerComponent manager = seat.GetManager();
			if (!manager) continue;
			array<int> doors = {};
			seat.GetAvailableDoorIndices(doors);
			for (int index; index < doors.Count() && keys.Count() < MAXIMUM_ENTRY_DOORS; index++)
			{
				int door = doors[index];
				string key = string.Format("%1:%2", AICF_VehiclePreset.RelativeOwnerIndex(seat), door);
				if (keys.Contains(key)) continue;
				keys.Insert(key);
				CompartmentDoorInfo info = manager.GetDoorInfo(door);
				PointInfo entry;
				if (info)
				{
					entry = info.GetAIEntryPointInfo();
					if (!entry) entry = info.GetEntryPointInfo();
				}
				bool loaded;
				bool requested;
				bool found;
				string entryText = "NONE";
				string projectedText = "NONE";
				float delta = -1;
				float heightDelta = -1;
				if (entry)
				{
					vector transform[4];
					entry.GetWorldTransform(transform);
					entryText = transform[3].ToString();
					loaded = navmesh.IsTileLoaded(transform[3]);
					requested = navmesh.IsTileRequested(transform[3]);
					vector projected;
					found = loaded && pathfinding.GetClosestPositionOnNavmesh(transform[3], "4 4 4", projected);
					if (found)
					{
						projectedText = projected.ToString();
						delta = vector.DistanceXZ(projected, transform[3]);
						heightDelta = projected[1] - transform[3][1];
					}
				}
				samples.Insert(string.Format(" door_key=%1 entry_defined=%2 entry_pos=%3 tile_loaded=%4 tile_requested=%5 projection_found=%6 projection=%7 delta_xz_m=%8 height_delta_m=%9",
					key, entry != null, entryText, loaded, requested, found, projectedText, delta, heightDelta));
			}
		}
		if (!IsCurrent(group, groupId, groupRpl, groupComponentRpl, faction, world, pathfinding, navmesh)) return;
		if (!AICF_VehicleBoardingMutationFence.IsAuthoritativeReplicatedEntity(vehicle) || vehicle.GetID() != vehicleId) return;
		if (vehicle.FindComponent(RplComponent) != vehicleRpl || vehicleRpl.Id().ToString() != vehicleComponentRpl) return;
		if (vector.Distance(vehicle.GetOrigin(), position) > 0.1 || vehicle.GetYawPitchRoll() != angles) return;
		string context = identity + string.Format(" sample_phase=POST_SPAWN_IMMEDIATE group=%1 vehicle=%2 vehicle_component_rpl=%3 position=%4 angles=%5 readonly=1 reachability_proven=0 async_carving_proven=0",
			AICF_GroupTrace.Entity(group), AICF_GroupTrace.Entity(vehicle), vehicleComponentRpl, position, angles);
		foreach (string sample : samples)
			AICF_Stage3Diagnostics.Info("VEHICLE_ENTRY_SITE_PROBE", context + sample);
	}
}
