// Controlled historical geometry; protection helper copied from frozen Multi fixture.
class AICF_MultiHierarchyProtection
{
	static string SelectedScope()
	{
		string scope;
		if (!System.GetCLIParam("aicfVehicleMultiProtectionScope", scope)) return "ROOT_ONLY";
		return scope;
	}

	static bool ScopeAllowed(string mode, string scope)
	{
		if (scope == "ROOT_ONLY") return true;
		return mode == "CAPACITY" && scope == "HIERARCHY";
	}

	protected static bool UnderRoot(DamageManagerComponent root, DamageManagerComponent manager)
	{
		HitZoneContainerComponent current = manager;
		for (int depth; current && depth < 64; depth++)
		{
			if (current == root) return true;
			current = current.GetParentHitZoneContainer();
		}
		return false;
	}

	protected static bool Collect(IEntity entity, DamageManagerComponent root, array<HitZone> zones,
		array<DamageManagerComponent> managers, array<IEntity> owners, out int nativeCount, out string reason)
	{
		zones.Clear();
		managers.Clear();
		owners.Clear();
		nativeCount = root.GetAllHitZonesInHierarchy(zones);
		if (nativeCount <= 0 || nativeCount != zones.Count() || zones.Count() > 512)
		{
			reason = "INCOMPLETE_HIERARCHY";
			return false;
		}
		managers.Insert(root);
		owners.Insert(entity);
		foreach (HitZone zone : zones)
		{
			if (!zone) { reason = "NULL_ZONE"; return false; }
			DamageManagerComponent manager = DamageManagerComponent.Cast(zone.GetHitZoneContainer());
			if (!manager) { reason = "NON_DAMAGE_CONTAINER"; return false; }
			IEntity owner = manager.GetOwner();
			if (!owner || owner.GetWorld() != entity.GetWorld() || owner.FindComponent(DamageManagerComponent) != manager)
			{
				reason = "MANAGER_OWNER_MISMATCH";
				return false;
			}
			if (!UnderRoot(root, manager)) { reason = "FOREIGN_HIERARCHY"; return false; }
			if (managers.Contains(manager)) continue;
			managers.Insert(manager);
			owners.Insert(owner);
		}
		return true;
	}

	static bool Apply(IEntity entity, out string details)
	{
		details = "protection_status=REJECTED protection_errors=1 protection_reason=CONTEXT";
		string mode;
		if (!Replication.IsServer() || !System.GetCLIParam("aicfVehicleMultiMode", mode) || mode != "CAPACITY") return false;
		if (SelectedScope() != "HIERARCHY" || !entity || !GetGame() || entity.GetWorld() != GetGame().GetWorld()) return false;
		EntityID identity = entity.GetID();
		RplComponent rpl = RplComponent.Cast(entity.FindComponent(RplComponent));
		if (!rpl || !rpl.IsMaster() || rpl.IsProxy()) return false;
		DamageManagerComponent root = DamageManagerComponent.Cast(entity.FindComponent(DamageManagerComponent));
		if (!root || root.GetOwner() != entity) return false;
		array<HitZone> zones = {};
		array<DamageManagerComponent> managers = {};
		array<IEntity> owners = {};
		int nativeCount;
		string reason;
		if (!Collect(entity, root, zones, managers, owners, nativeCount, reason))
		{
			details = "protection_status=REJECTED protection_errors=1 protection_reason=" + reason;
			return false;
		}
		int enabledBefore;
		foreach (int index, DamageManagerComponent manager : managers)
		{
			if (manager.GetOwner() != owners[index]) return false;
			if (!manager.IsDamageHandlingEnabled()) continue;
			enabledBefore++;
			manager.EnableDamageHandling(false);
		}
		array<HitZone> repeatedZones = {};
		array<DamageManagerComponent> repeatedManagers = {};
		array<IEntity> repeatedOwners = {};
		int repeatedCount;
		if (!Collect(entity, root, repeatedZones, repeatedManagers, repeatedOwners, repeatedCount, reason))
		{
			details = "protection_status=REJECTED protection_errors=1 protection_reason=READBACK_" + reason;
			return false;
		}
		int errors;
		int enabledAfter;
		if (nativeCount != repeatedCount || zones.Count() != repeatedZones.Count() || managers.Count() != repeatedManagers.Count()) errors++;
		foreach (HitZone originalZone : zones)
			if (!repeatedZones.Contains(originalZone)) errors++;
		foreach (int managerIndex, DamageManagerComponent originalManager : managers)
		{
			int repeatedIndex = repeatedManagers.Find(originalManager);
			if (repeatedIndex < 0) { errors++; continue; }
			if (repeatedOwners[repeatedIndex] != owners[managerIndex] || originalManager.GetOwner() != owners[managerIndex]) errors++;
			if (originalManager.IsDamageHandlingEnabled()) enabledAfter++;
		}
		if (entity.GetID() != identity || entity.FindComponent(RplComponent) != rpl || !rpl.IsMaster() || rpl.IsProxy()) errors++;
		if (entity.FindComponent(DamageManagerComponent) != root || root.GetOwner() != entity) errors++;
		details = string.Format("protection_errors=%1 protection_zones=%2 protection_zones_repeated=%3 protection_managers_covered=%4 protection_child_managers=%5 protection_enabled_before=%6 protection_enabled_after=%7",
			errors, nativeCount, repeatedCount, managers.Count(), managers.Count() - 1, enabledBefore, enabledAfter);
		bool complete = errors == 0 && enabledAfter == 0;
		if (complete) details += " protection_status=COVERED protection_reason=NONE";
		else details += " protection_status=REJECTED protection_reason=READBACK_FAILED";
		return complete;
	}
}

// Только historical CONTROLLED_SITE_SELECTION_FAULT. Native roster/path не подменяются.
class AICF_UnstuckProbe
{
	static SCR_AIGroup s_Group;
	static AICF_MatchController s_Owner;
	static EntityID s_GroupId;
	static string s_GroupRpl;
	static AICF_TransportTrip s_Trip;
	static AICF_GroupSlot s_Slot;
	static AICF_UnstuckPathHint s_PreHint;
	static vector s_Repaired;
	static int s_RepairAt;
	static int s_Repairs;
	static bool s_Forward;
	static bool s_PreProven;
	static bool s_LocalThree;
	static bool s_FullNative;
	static bool s_OriginalRequested;
	static bool s_SafePadReady;
	static bool s_SafePadFailed;
	static vector s_SafePad;
	static bool s_FarScopeReady;
	static int s_Purchases;
	static int s_DiagnosticRays;
	static bool s_PoseFault;
	static bool s_NeighborMetadataDone;
	static bool NeighborDiagnostics()
	{
		string value;
		return System.GetCLIParam("aicfUnstuckRoadGoalDiagnostics", value) && value == "1";
	}
	static bool BrakeDispatch()
	{
		string value;
		return System.GetCLIParam("aicfUnstuckBrakeDispatch", value) && value == "1";
	}
	static bool HistoricalCorridor()
	{
		string value;
		return System.GetCLIParam("aicfUnstuckHistoricalCorridor", value) && value == "1";
	}
	static vector FarGoal()
	{
		if (HistoricalCorridor() && s_OriginalRequested) return "1475.78 36.6684 2473.24";
		if (HistoricalCorridor()) return "684.711 1.5 2276.18";
		return "1475.78 36.6684 2473.24";
	}
	static bool ReturnOriginal()
	{
		string value;
		return HistoricalCorridor() && System.GetCLIParam("aicfUnstuckCorridorReturnOriginal", value) && value == "1";
	}
	static bool FreshSetup()
	{
		string setup;
		return System.GetCLIParam("aicfUnstuckPathProbeSetup", setup) && (setup == "FRESH56" || setup == "ROAD240");
	}
	static bool RoadSetup()
	{
		string setup;
		return System.GetCLIParam("aicfUnstuckPathProbeSetup", setup) && setup == "ROAD240";
	}
	static bool DiagnosticsOnly()
	{
		string value;
		return System.GetCLIParam("aicfUnstuckPathProbeDiagnostics", value) && value == "1";
	}
	static vector BirthPad()
	{
		if (FreshSetup() && s_SafePadReady) return s_SafePad;
		return Pad();
	}
	static string Mode()
	{
		string value;
		if (!Replication.IsServer() || !System.GetCLIParam("aicfUnstuckPathProbe", value)) return string.Empty;
		if (value != "BEFORE" && value != "AFTER") return string.Empty;
		return value;
	}
	static vector Pad()
	{
		// Actual MIN3 first STUCK origin, not the later off-path correction.
		if (HistoricalCorridor()) return "1581.85 8.59795 2001.13";
		vector pad = "1580.21 11.062 2009.42";
		pad[1] = GetGame().GetWorld().GetSurfaceY(pad[0], pad[2]) + 0.05;
		return pad;
	}
	static bool GroupCurrent(SCR_AIGroup group)
	{
		return group && group == s_Group && group.GetID() == s_GroupId && AICF_VehicleBehaviorBinding.ComponentRpl(group) == s_GroupRpl;
	}
	static bool SelectLocalSafePad(BaseWorld world, out vector selected)
	{
		if (s_SafePadReady) { selected = s_SafePad; return true; }
		if (s_SafePadFailed || !world) return false;
		if (RoadSetup()) return SelectRoadSafePad(world, selected);
		array<vector> directions = {"1 0 0", "-1 0 0", "0 0 1", "0 0 -1", "0.7071068 0 0.7071068", "-0.7071068 0 0.7071068", "0.7071068 0 -0.7071068", "-0.7071068 0 -0.7071068"};
		array<float> offsets = {8, 12, 15};
		float maximumDistance = 15;
		if (FreshSetup())
		{
			directions = {"0 0 -1", "0.7071068 0 -0.7071068", "-0.7071068 0 -0.7071068", "1 0 0", "-1 0 0", "0 0 1", "0.7071068 0 0.7071068", "-0.7071068 0 0.7071068"};
			offsets = {24, 40, 56};
			maximumDistance = 56;
		}
		vector historical = Pad();
		RoadMetadata(world, historical, 0);
		int tested;
		foreach (float offset : offsets)
			foreach (vector direction : directions)
			{
				tested++;
				vector center = historical + direction * offset;
				center[1] = world.GetSurfaceY(center[0], center[2]);
				vector candidate;
				bool empty = SCR_WorldTools.FindEmptyTerrainPosition(candidate, center, 8, 8, 2, TraceFlags.ENTS | TraceFlags.OCEAN, world);
				float distance = vector.Distance(historical, candidate);
				bool local = empty && distance <= maximumDistance;
				bool cylinder = SCR_WorldTools.TraceCilinderUtil(candidate + "0 1 0", 8, 2, TraceFlags.ENTS | TraceFlags.OCEAN, world);
				CollisionReceipt(world, candidate, tested, empty, cylinder);
				RoadMetadata(world, candidate, tested);
				Print(string.Format("[AICF][UNSTUCK_PROBE_LOCAL_SPAWN_QUERY] index=%1 center=%2 candidate=%3 empty=%4 distance_3d=%5 local=%6 cylinder8=%7 maximum_queries=24 maximum_distance_3d=%8 spawn_only=1",
					tested, center, candidate, empty, distance, local, cylinder, maximumDistance));
				if (!local || !cylinder) continue;
				s_SafePad = candidate;
				s_SafePadReady = true;
				selected = candidate;
				return true;
			}
		s_SafePadFailed = true;
		return false;
	}
	static bool RoadPoint(array<vector> points, array<float> arcs, float arc, out vector point)
	{
		if (!points || !arcs || !(Math.AbsFloat(arc) <= 10000000)) return false;
		if (points.Count() < 2 || points.Count() > 1024 || arcs.Count() != points.Count()) return false;
		if (arc < 0 || arc > arcs[arcs.Count() - 1]) return false;
		for (int index = 1; index < points.Count(); index++)
		{
			if (arc > arcs[index]) continue;
			float length = arcs[index] - arcs[index - 1];
			if (length <= 0.01) continue;
			point = points[index - 1] + (points[index] - points[index - 1]) * ((arc - arcs[index - 1]) / length);
			return true;
		}
		return false;
	}
	static bool SelectRoadSafePad(BaseWorld world, out vector selected)
	{
		SCR_AIWorld ai = SCR_AIWorld.Cast(GetGame().GetAIWorld());
		if (!ai || !ai.GetRoadNetworkManager()) { s_SafePadFailed = true; return false; }
		RoadNetworkManager roads = ai.GetRoadNetworkManager();
		vector historical = Pad();
		vector goal = FarGoal();
		BaseRoad road;
		float roadDistance;
		roads.GetClosestRoad(historical, road, roadDistance, true);
		array<vector> points = {};
		int count;
		if (road) count = road.GetPoints(points);
		if (!road || count != points.Count() || count < 2 || count > 1024) { s_SafePadFailed = true; return false; }
		array<float> arcs = {0};
		float nearest = float.MAX;
		float originArc;
		vector projected;
		for (int index; index < points.Count(); index++)
		{
			vector current = points[index];
			for (int axis; axis < 3; axis++)
				if (!(Math.AbsFloat(current[axis]) <= 1000000)) { s_SafePadFailed = true; return false; }
			if (index == 0) continue;
			vector delta = current - points[index - 1];
			float length = delta.Length();
			arcs.Insert(arcs[index - 1] + length);
			vector flat = delta;
			flat[1] = 0;
			if (flat.LengthSq() < 0.01) continue;
			float fraction = Math.Clamp(vector.Dot(historical - points[index - 1], flat) / flat.LengthSq(), 0, 1);
			vector point = points[index - 1] + delta * fraction;
			float distance = vector.DistanceXZ(historical, point);
			if (distance >= nearest) continue;
			nearest = distance;
			projected = point;
			originArc = arcs[index - 1] + length * fraction;
		}
		if (nearest == float.MAX) { s_SafePadFailed = true; return false; }
		vector positive, negative;
		int preferred = -1;
		bool positiveValid = RoadPoint(points, arcs, originArc + 20, positive);
		bool negativeValid = RoadPoint(points, arcs, originArc - 20, negative);
		if (positiveValid && (!negativeValid || vector.DistanceXZ(positive, goal) > vector.DistanceXZ(negative, goal))) preferred = 1;
		bool safeBehind, safeFront;
		vector behind, front;
		int tested;
		for (int step = 1; step <= 12; step++)
			for (int side; side < 2; side++)
			{
				tested++;
				int direction = preferred;
				if (side == 1) direction = -preferred;
				float arc = originArc + direction * step * 20;
				vector nativePoint;
				if (!RoadPoint(points, arcs, arc, nativePoint))
				{
					Print(string.Format("[AICF][UNSTUCK_PROBE_ROAD_SPAWN_QUERY] query=%1 arc=%2 result=OUTSIDE_POLYLINE maximum_queries=24 initial_spawn_only=1", tested, arc));
					continue;
				}
				vector endpoint, candidate;
				bool reachable = roads.GetReachableWaypointInRoad(nativePoint, goal, 10, endpoint);
				bool empty = SCR_WorldTools.FindEmptyTerrainPosition(candidate, nativePoint, 8, 8, 2, TraceFlags.ENTS | TraceFlags.OCEAN, world);
				bool cylinder = SCR_WorldTools.TraceCilinderUtil(candidate + "0 1 0", 8, 2, TraceFlags.ENTS | TraceFlags.OCEAN, world);
				float displacement = vector.Distance(historical, candidate);
				bool isBehind = vector.DistanceXZ(candidate, goal) > vector.DistanceXZ(projected, goal);
				bool valid = reachable && empty && cylinder && displacement <= 240;
				vector nextPoint, tangent;
				bool tangentKnown = RoadPoint(points, arcs, arc + 1, nextPoint);
				if (tangentKnown) tangent = (nextPoint - nativePoint).Normalized();
				CollisionReceipt(world, candidate, tested, empty, cylinder);
				string details = string.Format("query=%1 road=%2 arc=%3 projection_arc=%4 native_point=%5 candidate=%6 reachable=%7 empty=%8 cylinder8=%9",
					tested, road, arc, originArc, nativePoint, candidate, reachable, empty, cylinder);
				details += string.Format(" displacement_3d=%1 behind_projection_relative_goal=%2 valid=%3 endpoint=%4 raw_goal=%5 maximum_queries=24 maximum_arc_offset=240 initial_spawn_only=1 live_teleports=0 drivable_proven=0", displacement, isBehind, valid, endpoint, goal);
				details += string.Format(" native_tangent=%1 tangent_known=%2 spawn_orientation_policy=UNCHANGED_OWNED_BASE", tangent, tangentKnown);
				Print("[AICF][UNSTUCK_PROBE_ROAD_SPAWN_QUERY] " + details);
				if (!valid) continue;
				if (isBehind && !safeBehind) { safeBehind = true; behind = candidate; }
				if (!isBehind && !safeFront) { safeFront = true; front = candidate; }
			}
		if (!safeBehind && !safeFront) { s_SafePadFailed = true; return false; }
		s_SafePad = front;
		if (safeBehind) s_SafePad = behind;
		s_SafePadReady = true;
		selected = s_SafePad;
		Print(string.Format("[AICF][UNSTUCK_PROBE_ROAD_SPAWN_SELECTED] selected=%1 behind=%2 tested=%3 native_points=%4 projected=%5 origin_arc=%6 closest_distance=%7 initial_spawn_only=1 commit_checks_unchanged=1 drivable_proven=0", selected, safeBehind, tested, points.Count(), projected, originArc, roadDistance));
		if (NeighborDiagnostics()) NeighborGoalMetadata(roads, road, points, arcs, originArc, projected, selected);
		return true;
	}
	static bool CorridorContains(float sourceArc, float joinArc, float projectionArc)
	{
		return Math.Min(sourceArc, joinArc) <= projectionArc && Math.Max(sourceArc, joinArc) >= projectionArc;
	}
	static bool NeighborInspectionComplete(int nativeCount, int returnedCount, int examinedCount, bool entriesValid)
	{
		return nativeCount >= 0 && returnedCount <= 16 && nativeCount == returnedCount && examinedCount == returnedCount && entriesValid;
	}
	static void NeighborGoalMetadata(RoadNetworkManager roads, BaseRoad originRoad, array<vector> points,
		array<float> arcs, float projectionArc, vector projection, vector source)
	{
		if (!roads || !originRoad || !points || !arcs || points.Count() < 2 || points.Count() > 1024 || arcs.Count() != points.Count()) return;
		float sourceArc;
		float best = float.MAX;
		for (int segment = 1; segment < points.Count(); segment++)
		{
			vector delta = points[segment] - points[segment - 1];
			vector flat = delta;
			flat[1] = 0;
			if (flat.LengthSq() < 0.01) continue;
			float fraction = Math.Clamp(vector.Dot(source - points[segment - 1], flat) / flat.LengthSq(), 0, 1);
			vector candidate = points[segment - 1] + delta * fraction;
			float distance = vector.DistanceXZ(source, candidate);
			if (distance >= best) continue;
			best = distance;
			sourceArc = arcs[segment - 1] + delta.Length() * fraction;
		}
		if (best == float.MAX) return;
		int edge;
		float joinArc;
		if (sourceArc < projectionArc) { edge = points.Count() - 1; joinArc = arcs[arcs.Count() - 1]; }
		vector join = points[edge];
		bool corridor = CorridorContains(sourceArc, joinArc, projectionArc);
		float historicalDistance = vector.Distance(Pad(), projection);
		array<BaseRoad> neighbors = {};
		int nativeRoads = roads.GetRoadsInAABB(join - "15 15 15", join + "15 15 15", neighbors);
		bool complete = nativeRoads == neighbors.Count() && neighbors.Count() <= 16;
		int queries, matching, examinedRoads;
		for (int roadIndex; roadIndex < neighbors.Count() && roadIndex < 16 && queries < 4; roadIndex++)
		{
			examinedRoads++;
			BaseRoad neighbor = neighbors[roadIndex];
			if (!neighbor) { complete = false; continue; }
			if (neighbor == originRoad) continue;
			array<vector> other = {};
			int nativePoints = neighbor.GetPoints(other);
			if (nativePoints != other.Count() || other.Count() < 2 || other.Count() > 1024) { complete = false; continue; }
			bool finite = true;
			foreach (vector point : other)
				for (int axis; axis < 3; axis++)
					if (!(Math.AbsFloat(point[axis]) <= 1000000)) finite = false;
			if (!finite) { complete = false; continue; }
			for (int side; side < 2 && queries < 4; side++)
			{
				int joinIndex;
				int goalIndex = other.Count() - 1;
				if (side == 1) { joinIndex = goalIndex; goalIndex = 0; }
				float joinDistance = vector.Distance(join, other[joinIndex]);
				if (joinDistance > 3) continue;
				vector goal = other[goalIndex];
				float goalHistorical = vector.Distance(Pad(), goal);
				float goalSource = vector.Distance(source, goal);
				if (goalHistorical <= 200 || goalSource <= 200) continue;
				queries++;
				vector endpoint;
				bool reachable = roads.GetReachableWaypointInRoad(source, goal, 10, endpoint);
				bool eligible = corridor && historicalDistance <= 15 && reachable && vector.Distance(endpoint, goal) <= 10;
				if (eligible) matching++;
				string row = string.Format("query=%1 origin_road=%2 neighbor=%3 source=%4 goal=%5 join=%6 neighbor_join=%7 join_distance_3d=%8 reachable=%9", queries, originRoad, neighbor, source, goal, join, other[joinIndex], joinDistance, reachable);
				row += string.Format(" source_arc=%1 join_arc=%2 projection_arc=%3 projection=%4 historical_projection_distance_3d=%5 goal_historical_distance_3d=%6 goal_source_distance_3d=%7 endpoint=%8 corridor_candidate=%9", sourceArc, joinArc, projectionArc, projection, historicalDistance, goalHistorical, goalSource, endpoint, eligible);
				row += " radius=10 road_limit=16 goal_limit=4 native_point_limit=1024 readonly=1 drivable_proven=0 no_goal_order_issued=1";
				Print("[AICF][UNSTUCK_PROBE_NEIGHBOR_GOAL] " + row);
			}
		}
		complete = NeighborInspectionComplete(nativeRoads, neighbors.Count(), examinedRoads, complete);
		s_NeighborMetadataDone = true;
		string summary = string.Format("[AICF][UNSTUCK_PROBE_NEIGHBOR_SUMMARY] native_roads=%1 actual_roads=%2 complete=%3 queries=%4 matching=%5 source_arc=%6 join_arc=%7 historical_projection_distance_3d=%8 readonly=1 drivable_proven=0 no_goal_order_issued=1", nativeRoads, neighbors.Count(), complete, queries, matching, sourceArc, joinArc, historicalDistance);
		summary += string.Format(" examined_roads=%1 goal_cap_reached=%2 goal_search_complete=%3", examinedRoads, queries >= 4, complete && queries < 4);
		Print(summary);
	}
	static void RoadMetadata(BaseWorld world, vector from, int query)
	{
		if (query < 0 || query > 24 || !world || !GetGame()) return;
		SCR_AIWorld ai = SCR_AIWorld.Cast(GetGame().GetAIWorld());
		if (!ai || !ai.GetRoadNetworkManager()) return;
		RoadNetworkManager roads = ai.GetRoadNetworkManager();
		vector target = FarGoal();
		vector endpoint;
		bool reachable = roads.GetReachableWaypointInRoad(from, target, 10, endpoint);
		BaseRoad road;
		float roadDistance;
		int closestStatus = roads.GetClosestRoad(from, road, roadDistance, true);
		array<vector> points = {};
		int nativePoints;
		if (road) nativePoints = road.GetPoints(points);
		float nearest = float.MAX;
		vector projected;
		vector tangent;
		bool finite = true;
		for (int index = 1; index < points.Count() && index < 1024; index++)
		{
			vector previous = points[index - 1];
			vector current = points[index];
			for (int axis; axis < 3; axis++)
				if (!(Math.AbsFloat(current[axis]) <= 1000000) || !(Math.AbsFloat(previous[axis]) <= 1000000)) finite = false;
			if (!finite) break;
			vector delta = points[index] - points[index - 1];
			vector flat = delta;
			flat[1] = 0;
			if (flat.LengthSq() < 0.01) continue;
			float fraction = Math.Clamp(vector.Dot(from - points[index - 1], flat) / flat.LengthSq(), 0, 1);
			vector point = points[index - 1] + delta * fraction;
			float distance = vector.DistanceXZ(from, point);
			if (distance >= nearest) continue;
			nearest = distance;
			projected = point;
			tangent = flat.Normalized();
		}
		bool complete = finite && road && nativePoints == points.Count() && points.Count() >= 2 && points.Count() <= 1024;
		bool cylinder;
		bool cylinderQueried = complete && nearest < float.MAX;
		if (cylinderQueried)
		{
			cylinder = SCR_WorldTools.TraceCilinderUtil(projected + "0 1 0", 8, 2, TraceFlags.ENTS | TraceFlags.OCEAN, world);
			CollisionRay(world, projected + "0 2 0", projected - "0 2 0", TraceFlags.ENTS | TraceFlags.WORLD, query, "ROAD_POINT_DOWN");
		}
		string details = string.Format("query=%1 from=%2 target=%3 radius=10 reachable=%4 endpoint=%5 endpoint_target_3d=%6 closest_status=%7 road=%8 road_distance=%9",
			query, from, target, reachable, endpoint, vector.Distance(endpoint, target), closestStatus, road, roadDistance);
		details += string.Format(" native_points=%1 actual_points=%2 bounded_points=1024 complete=%3 projected=%4 tangent=%5 projected_from_xz=%6 projected_historical_3d=%7 cylinder8=%8 readonly=1 drivable_proven=0",
			nativePoints, points.Count(), complete, projected, tangent, nearest, vector.Distance(projected, Pad()), cylinder);
		details += " cylinder_queried=" + cylinderQueried.ToString();
		Print("[AICF][UNSTUCK_PROBE_ROAD_METADATA] " + details);
	}
	static string NativeFlags(AICF_TransportTrip trip, Vehicle vehicle)
	{
		if (!GetGame() || !GetGame().GetAIWorld() || !trip || !vehicle || !trip.GetAssignment() || !trip.GetAssignment().GetGroup()) return " native_flags_scope=UNPROVEN";
		AICF_UnstuckMutationScope scope = AICF_UnstuckMutationScope.Capture(trip, vehicle);
		if (!scope) return " native_flags_scope=UNPROVEN";
		SCR_AIGroup group;
		Vehicle actual;
		SCR_AIGroupVehicle record;
		AIAgent pilot;
		AIGroupMovementComponent movement;
		string reason;
		SCR_AIGroupUtilityComponent utility = trip.GetAssignment().GetGroup().GetGroupUtilityComponent();
		if (!AICF_VehicleNativeSubgroup.Read(trip, utility, group, actual, record, pilot, movement, reason) || actual != vehicle || !pilot) return " native_flags_scope=UNPROVEN reason=" + reason;
		AICarMovementComponent car = AICarMovementComponent.Cast(pilot.GetMovementComponent());
		if (!car || !car.GetAIAgent() || car.GetAIAgent().GetControlledEntity() != vehicle) return " native_flags_scope=UNPROVEN reason=CAR_COMPONENT";
		AIAgent carAgent = car.GetAIAgent();
		array<AIAgent> worldAgents = {};
		GetGame().GetAIWorld().GetAIAgents(worldAgents);
		bool complete = worldAgents.Count() <= 4096;
		int matches;
		for (int index; index < worldAgents.Count() && index < 4096; index++)
		{
			AIAgent agent = worldAgents[index];
			if (!agent) { complete = false; continue; }
			if (agent.GetControlledEntity() == vehicle) matches++;
		}
		if (!complete || matches != 1 || !carAgent || !pilot || !record || !car || !worldAgents.Contains(carAgent) || !scope.IsCurrent()) return " native_flags_scope=UNPROVEN reason=WORLD_IDENTITY";
		if (!AICF_VehicleNativeSubgroup.IsCurrentVehicleHandler(trip, utility, record.GetSubgroupHandleId())) return " native_flags_scope=UNPROVEN reason=HANDLER";
		string flags = string.Format(" native_flags_scope=PROVEN car_agent=%1 car_active=%2 car_counted=%3 car_lod=%4 pilot_agent=%5 pilot_active=%6 pilot_counted=%7 pilot_lod=%8 native_handler=%9",
			carAgent.GetID(), carAgent.IsAIActivated(), carAgent.CountTowardsAIWorldLimit(), carAgent.GetLOD(), pilot.GetID(), pilot.IsAIActivated(), pilot.CountTowardsAIWorldLimit(), pilot.GetLOD(), record.GetSubgroupHandleId());
		flags += string.Format(" car_permanent_lod=%1 pilot_permanent_lod=%2 max_lod=%3 navlink=%4 world_unique=%5 world_complete=%6 readonly=1",
			carAgent.GetPermanentLOD(), pilot.GetPermanentLOD(), AIAgent.GetMaxLOD(), AICF_GroupTrace.Entity(car.GetLastNavlinkEntity()), matches, complete);
		array<vector> path = {};
		car.GetCurrentPath(path);
		flags += string.Format(" native_path_count=%1 path_progress=0 current_action=%2 executed_action=%3", path.Count(), utility.GetCurrentAction(), utility.GetExecutedAction());
		if (utility.GetCurrentAction()) flags += " current_action_state=" + utility.GetCurrentAction().GetActionState().ToString();
		if (utility.GetExecutedAction()) flags += " executed_action_state=" + utility.GetExecutedAction().GetActionState().ToString();
		if (!path.IsEmpty()) flags += string.Format(" native_first=%1 native_last=%2 global_goal=%3 lease=%4", path[0], path[path.Count() - 1], trip.GetAssignment().GetTargetPosition(), trip.GetLease().GetLeaseGeneration());
		for (int pathIndex; pathIndex < path.Count() && pathIndex < 3; pathIndex++) flags += string.Format(" native_prefix_%1=%2", pathIndex, path[pathIndex]);
		if (vehicle.GetPhysics()) flags += string.Format(" speed=%1 velocity=%2", vehicle.GetPhysics().GetVelocity().Length(), vehicle.GetPhysics().GetVelocity());
		vector boundsMin, boundsMax;
		vehicle.GetBounds(boundsMin, boundsMax);
		flags += string.Format(" bounds_min=%1 bounds_max=%2 ypr=%3", boundsMin, boundsMax, vehicle.GetYawPitchRoll());
		SCR_DamageManagerComponent damage = SCR_DamageManagerComponent.GetDamageManager(vehicle);
		if (damage && damage.GetDefaultHitZone()) flags += string.Format(" health=%1 movement_damage=%2 fire=%3", damage.GetDefaultHitZone().GetHealthScaled(), damage.GetMovementDamage(), SCR_AIVehicleUsability.VehicleIsOnFire(vehicle));
		array<AIAgent> repeated = {};
		GetGame().GetAIWorld().GetAIAgents(repeated);
		bool repeatedComplete = repeated.Count() <= 4096;
		int repeatedMatches;
		for (int repeatedIndex; repeatedIndex < repeated.Count() && repeatedIndex < 4096; repeatedIndex++)
		{
			AIAgent repeatedAgent = repeated[repeatedIndex];
			if (!repeatedAgent) { repeatedComplete = false; continue; }
			if (repeatedAgent.GetControlledEntity() == vehicle) repeatedMatches++;
		}
		if (!repeatedComplete || repeatedMatches != 1 || !carAgent || !repeated.Contains(carAgent)) return " native_flags_scope=UNPROVEN reason=REPEATED_WORLD";
		if (!scope.IsCurrent() || !car || !pilot || car.GetAIAgent() != carAgent || carAgent.GetControlledEntity() != vehicle || pilot.GetMovementComponent() != car) return " native_flags_scope=UNPROVEN reason=RECHECK";
		return flags;
	}
	static int OwnRosterLineage(IEntity entity)
	{
		if (!entity || !s_Slot || !GroupCurrent(s_Slot.GetGroup())) return -1;
		IEntity current = entity;
		for (int depth; current && depth < 16; depth++)
		{
			for (int member; member < 5; member++)
				if (s_Slot.GetRosterMember(member) == current) return 1;
			current = current.GetParent();
		}
		if (current) return -1;
		return 0;
	}
	static int CollisionRay(BaseWorld world, vector start, vector end, TraceFlags flags, int query, string ray)
	{
		if (s_DiagnosticRays >= 96) return -1;
		s_DiagnosticRays++;
		TraceParam trace = new TraceParam();
		trace.Flags = flags;
		trace.Start = start;
		trace.End = end;
		float coefficient = world.TraceMove(trace, null);
		IEntity hit = trace.TraceEnt;
		string kind = "NONE";
		string prefab = "NONE";
		vector boundsMin, boundsMax;
		if (hit)
		{
			kind = hit.Type().ToString();
			if (hit.GetPrefabData()) prefab = hit.GetPrefabData().GetPrefabName();
			hit.GetWorldBounds(boundsMin, boundsMax);
		}
		string detail = string.Format("query=%1 ray=%2 native_coefficient=%3 hit=%4 type=%5 prefab=%6 rpl=%7 own_roster_lineage=%8 diagnostic_ray=%9",
			query, ray, coefficient, AICF_GroupTrace.Entity(hit), kind, prefab, AICF_VehicleBehaviorBinding.ComponentRpl(hit), OwnRosterLineage(hit), s_DiagnosticRays);
		detail += string.Format(" world_bounds_min=%1 world_bounds_max=%2 start=%3 end=%4 maximum_diagnostic_rays=96 find_false_cause_proven=0 readonly=1",
			boundsMin, boundsMax, start, end);
		Print("[AICF][UNSTUCK_PROBE_COLLISION_RAY] " + detail);
		if (coefficient < 1) return 1;
		return 0;
	}
	static void CollisionReceipt(BaseWorld world, vector candidate, int query, bool found, bool cylinder)
	{
		array<vector> directions = {"8 1 0", "-8 1 0", "0 1 8", "0 1 -8"};
		vector center = candidate + "0 1 0";
		int matched = -1;
		foreach (int index, vector direction : directions)
		{
			int result = CollisionRay(world, center + direction, center - direction, TraceFlags.ENTS | TraceFlags.OCEAN, query, "CYLINDER_" + index.ToString());
			if (result < 0) break;
			matched = result;
			if (result == 1) break;
		}
		if (!found) CollisionRay(world, candidate + "0 0.1 0", candidate + "0 10.1 0", TraceFlags.ENTS | TraceFlags.OCEAN | TraceFlags.WORLD, query, "UP_INSIDE_ROCK");
		float east = world.GetSurfaceY(candidate[0] + 8, candidate[2]);
		float west = world.GetSurfaceY(candidate[0] - 8, candidate[2]);
		float north = world.GetSurfaceY(candidate[0], candidate[2] + 8);
		float south = world.GetSurfaceY(candidate[0], candidate[2] - 8);
		Print(string.Format("[AICF][UNSTUCK_PROBE_COLLISION_READBACK] query=%1 native_cylinder8=%2 matched_ray_result=%3 find_result=%4 terrain_east=%5 terrain_west=%6 terrain_north=%7 terrain_south=%8 join_claim=UNPROVEN readonly=1",
			query, cylinder, matched, found, east, west, north, south));
	}
}
modded class AICF_EconomySystem
{
	override bool PurchaseVehiclePreset(AICF_TransportTrip trip, SCR_CampaignFaction faction,
		SCR_CampaignMilitaryBaseComponent base, ResourceName prefab, out int paid)
	{
		bool purchased = super.PurchaseVehiclePreset(trip, faction, base, prefab, paid);
		if (!AICF_UnstuckProbe.Mode().IsEmpty() && purchased && paid > 0 && trip && trip.GetAssignment() &&
			AICF_UnstuckProbe.GroupCurrent(trip.GetAssignment().GetGroup()))
		{
			AICF_UnstuckProbe.s_Purchases++;
			Print(string.Format("[AICF][UNSTUCK_PROBE_PURCHASE] operation=%1 paid=%2 actual_success=%3 purchases=%4 transaction_unchanged=1",
				trip.GetOperationId(), paid, purchased, AICF_UnstuckProbe.s_Purchases));
		}
		return purchased;
	}
}
modded class AICF_GroupSpawner
{
	override SCR_AIGroup SpawnGroup(SCR_CampaignFaction faction, SCR_CampaignMilitaryBaseComponent spawnBase,
		int slotId, int desiredSize, bool managedDeployment = true)
	{
		SCR_AIGroup group = super.SpawnGroup(faction, spawnBase, slotId, desiredSize, managedDeployment);
		if (AICF_UnstuckProbe.Mode().IsEmpty() || !managedDeployment || !faction || faction.GetFactionKey() != "USSR" || slotId != 0 || desiredSize != 5) return group;
		if (!AICF_UnstuckProbe.s_Owner || AICF_MatchController.GetActiveController() != AICF_UnstuckProbe.s_Owner) return group;
		if (!group || group.GetAgentsCount() != 0 || group.GetIntendedActiveCount() != 0 || AICF_UnstuckProbe.s_Group) return group;
		vector previous = group.GetOrigin();
		AICF_UnstuckProbe.s_Group = group;
		AICF_UnstuckProbe.s_GroupId = group.GetID();
		AICF_UnstuckProbe.s_GroupRpl = AICF_VehicleBehaviorBinding.ComponentRpl(group);
		if (AICF_UnstuckProbe.FreshSetup())
		{
			vector initial;
			if (!AICF_UnstuckProbe.SelectLocalSafePad(group.GetWorld(), initial)) return group;
			if (group.GetAgentsCount() != 0 || group.GetIntendedActiveCount() != 0 ||
				AICF_MatchController.GetActiveController() != AICF_UnstuckProbe.s_Owner) return group;
		}
		group.SetOrigin(AICF_UnstuckProbe.BirthPad());
		Print(string.Format("[AICF][UNSTUCK_PROBE_EMPTY_BIRTH] group=%1 rpl=%2 actual_agents=%3 intended=%4 requested=0 previous=%5 position=%6 test_only=1",
			group.GetID(), AICF_UnstuckProbe.s_GroupRpl, group.GetAgentsCount(), group.GetIntendedActiveCount(), previous, group.GetOrigin()) +
			string.Format(" fresh_safe56=%1 road240=%2 initial_birth_only=1 recovery_maximum15_unchanged=1", AICF_UnstuckProbe.FreshSetup() && !AICF_UnstuckProbe.RoadSetup(), AICF_UnstuckProbe.RoadSetup()));
		return group;
	}
}
modded class AICF_VehicleSpawner
{
	override bool TrySelectSiteForAcquisition(SCR_GameModeCampaign campaign, SCR_CampaignFaction faction, SCR_AIGroup group,
		AICF_ConflictAdapter conflictAdapter, vector preferredPosition, float maximumSpawnDistanceMeters,
		float maximumBoardingDistanceMeters, string identityContext, int requestGeneration, int attempt,
		bool preflightOnly, out AICF_VehicleSpawnSiteSelection selection)
	{
		if (AICF_UnstuckProbe.Mode().IsEmpty() || !AICF_UnstuckProbe.GroupCurrent(group) || !faction || faction.GetFactionKey() != "USSR")
			return super.TrySelectSiteForAcquisition(campaign, faction, group, conflictAdapter, preferredPosition,
				maximumSpawnDistanceMeters, maximumBoardingDistanceMeters, identityContext, requestGeneration, attempt, preflightOnly, selection);
		if (AICF_MatchController.GetActiveController() != AICF_UnstuckProbe.s_Owner) return false;
		SCR_CampaignMilitaryBaseComponent base = faction.GetMainBase();
		if (!base || !base.GetSpawnPoint()) return false;
		vector basePosition, baseRotation;
		base.GetSpawnPoint().GetPositionAndRotation(basePosition, baseRotation);
		bool selected = super.TrySelectSiteForAcquisition(campaign, faction, group, conflictAdapter, basePosition,
			maximumSpawnDistanceMeters, 0, identityContext, requestGeneration, attempt, preflightOnly, selection);
		if (!selected || !selection || !selection.IsSelected() || !AICF_UnstuckProbe.GroupCurrent(group)) return false;
		if (AICF_MatchController.GetActiveController() != AICF_UnstuckProbe.s_Owner) return false;
		vector previous = selection.m_vPosition;
		vector safePad;
		if (!AICF_UnstuckProbe.SelectLocalSafePad(base.GetOwner().GetWorld(), safePad)) return false;
		if (!AICF_UnstuckProbe.GroupCurrent(group) || AICF_MatchController.GetActiveController() != AICF_UnstuckProbe.s_Owner) return false;
		selection.m_vPosition = safePad;
		Print(string.Format("[AICF][UNSTUCK_PROBE_SITE_SELECTION] group=%1 selected_base=%2 original=%3 position=%4 request=%5 attempt=%6 preflight=%7 classification=CONTROLLED_SITE_SELECTION_FAULT commit_checks_unchanged=1",
			group.GetID(), selection.m_Base, previous, selection.m_vPosition, requestGeneration, attempt, preflightOnly));
		return true;
	}
}
// The ordinary parking token retains its exact-vehicle and human/foreign fences.
// This opt-in test owns one initial false->true write during TRANSIT only.
modded class AICF_VehicleParking
{
	string AICF_UPDispatchGuardReceipt(AICF_TransportTrip trip)
	{
		if (!trip || !trip.GetBehaviorState() || !m_Controller || !m_Vehicle) return "exact=UNKNOWN native_brake=UNKNOWN";
		return string.Format("exact=%1 usable=%2 no_foreign_human=%3 native_brake=%4 production_parking=%5 stopped=%6 preset_current=%7 phase=%8",
			IsExactVehicle(trip), IsUsableVehicle(), HasNoForeignOrHumanUse(), m_Controller.GetPersistentHandBrake(),
			trip.GetBehaviorState().m_Parking != null, m_bStopped, trip.IsVehiclePresetCurrent(), trip.GetPhase());
	}
	bool AICF_UPWaitForNativeRelease(AICF_TransportTrip trip)
	{
		if (!IsExactVehicle(trip) || !trip.IsValid() || trip.IsTerminal() || !trip.IsVehiclePresetCurrent()) return false;
		if (trip.GetPhase() != AICF_ETransportTripPhase.TRANSIT || trip.GetBehaviorState().m_Parking) return false;
		return IsUsableVehicle() && HasNoForeignOrHumanUse() && m_Controller.GetPersistentHandBrake();
	}
	bool AICF_UPNativeBrakeFalse(AICF_TransportTrip trip)
	{
		return IsExactVehicle(trip) && IsUsableVehicle() && HasNoForeignOrHumanUse() && !m_Controller.GetPersistentHandBrake();
	}
	bool AICF_UPBeginDispatchBlock(AICF_TransportTrip trip)
	{
		if (!IsExactVehicle(trip) || !trip.IsValid() || trip.IsTerminal() || !trip.IsVehiclePresetCurrent()) return false;
		if (trip.GetPhase() != AICF_ETransportTripPhase.TRANSIT || trip.GetBehaviorState().m_Parking) return false;
		if (!IsUsableVehicle() || !HasNoForeignOrHumanUse() || m_Controller.GetPersistentHandBrake()) return false;
		if (!IsExactVehicle(trip) || !HasNoForeignOrHumanUse() || m_Controller.GetPersistentHandBrake()) return false;
		m_Controller.SetPersistentHandBrake(true);
		m_bOwnsBrake = m_Controller.GetPersistentHandBrake();
		return m_bOwnsBrake;
	}
	bool AICF_UPDispatchBlockCurrent(AICF_TransportTrip trip)
	{
		return m_bOwnsBrake && IsExactVehicle(trip) && HasNoForeignOrHumanUse() && m_Controller.GetPersistentHandBrake();
	}
	bool AICF_UPObserveDispatchBlock(AICF_TransportTrip trip, out string details)
	{
		bool exact = IsExactVehicle(trip);
		bool noForeign = HasNoForeignOrHumanUse();
		bool knownController = m_Controller != null;
		bool brake = knownController && m_Controller.GetPersistentHandBrake();
		bool own = m_bOwnsBrake;
		string failed = "NONE";
		if (!own) failed = "OWN_FLAG";
		else if (!exact) failed = "EXACT_IDENTITY";
		else if (!noForeign) failed = "FOREIGN_OR_HUMAN";
		else if (!knownController) failed = "CONTROLLER_MISSING";
		else if (!brake) failed = "NATIVE_BRAKE_FALSE";
		details = string.Format("own=%1 exact=%2 no_foreign_human=%3 controller_known=%4 native_brake=%5 failed=%6",
			own, exact, noForeign, knownController, brake, failed);
		return own && exact && noForeign && knownController && brake;
	}
	bool AICF_UPReleaseDispatchBlock(AICF_TransportTrip trip)
	{
		if (AICF_UPDispatchBlockReleased()) return true;
		if (!trip || !trip.GetBehaviorState() || trip.GetBehaviorState().m_Parking) return false;
		if (!IsExactVehicle(trip) || !HasNoForeignOrHumanUse()) return false;
		Stop(trip);
		return AICF_UPDispatchBlockReleased();
	}
	bool AICF_UPDispatchBlockReleased() { return m_bStopped && !m_bOwnsBrake; }
}
modded class AICF_VehicleCoordinator
{
	override void Update(AICF_FactionState usState, SCR_CampaignFaction usFaction, AICF_FactionState ussrState,
		SCR_CampaignFaction ussrFaction, int baseRevision, bool dispatchTrips)
	{
		AICF_TransportTrip trip = AICF_UnstuckProbe.s_Trip;
		if (!AICF_UnstuckProbe.s_FarScopeReady) trip = null;
		if (AICF_UnstuckProbe.FreshSetup() && !AICF_UnstuckProbe.s_PoseFault && !AICF_UnstuckProbe.BrakeDispatch()) trip = null;
		Vehicle car;
		int attempts;
		vector origin;
		AICF_UnstuckPathHint hint;
		string hintReceipt = "pre_scope=UNPROVEN";
		if (trip && GetTrip(AICF_UnstuckProbe.s_Slot) == trip && trip.GetPhase() == AICF_ETransportTripPhase.TRANSIT && trip.GetLease())
		{
			car = trip.GetLease().GetVehicle();
			if (car)
			{
				origin = car.GetOrigin();
				attempts = trip.GetMovementState().GetMobilityRecoveryAttempts();
				hint = AICF_UnstuckPathHint.Capture(trip, origin);
				if (hint) hintReceipt = hint.AICF_UPReceipt();
			}
		}
		super.Update(usState, usFaction, ussrState, ussrFaction, baseRevision, dispatchTrips);
		if (!car || !trip || trip.GetPhase() != AICF_ETransportTripPhase.TRANSIT || !trip.GetLease() || trip.GetLease().GetVehicle() != car) return;
		if (trip.GetMovementState().GetMobilityRecoveryAttempts() <= attempts || vector.DistanceXZ(origin, car.GetOrigin()) < 4) return;
		AICF_UnstuckProbe.s_Repairs++;
		AICF_UnstuckProbe.s_Repaired = car.GetOrigin();
		AICF_UnstuckProbe.s_RepairAt = System.GetTickCount();
		AICF_UnstuckProbe.s_Forward = hint && hint.Prefers(car.GetOrigin());
		AICF_UnstuckProbe.s_PreProven = hint != null;
		string detail = "hint_only=1 path_progress=0 hint_kind=UNKNOWN";
		if (hint) detail = hint.Details();
		AICF_UnstuckProbe.s_LocalThree = hint && detail.Contains("hint_kind=LOCAL_FRAGMENT") && detail.Contains("hint_points=3 ");
		AICF_UnstuckProbe.s_FullNative = hint && detail.Contains("hint_kind=FULL_NATIVE_PATH");
		Print(string.Format("[AICF][UNSTUCK_PROBE_CORRECTION] mode=%1 operation=%2 car=%3 from=%4 to=%5 actual_distance=%6 attempt=%7 pre_hint=%8 forward=%9",
			AICF_UnstuckProbe.Mode(), trip.GetOperationId(), car.GetID().ToString(), origin, car.GetOrigin(), vector.Distance(origin, car.GetOrigin()),
			trip.GetMovementState().GetMobilityRecoveryAttempts(), hint != null, AICF_UnstuckProbe.s_Forward) + detail + " " + hintReceipt);
	}
}
modded class AICF_UnstuckPathHint
{
	string AICF_UPReceipt()
	{
		if (!IsCurrent()) return "pre_scope=UNPROVEN";
		string details = string.Format("pre_scope=PROVEN pre_car_agent=%1 pre_pilot_agent=%2 pre_car_active=%3 pre_car_counted=%4 pre_car_lod=%5 pre_pilot_active=%6 pre_handler=%7 pre_points=%8 pre_origin=%9",
			m_CarAgent.GetID(), m_Pilot.GetID(), m_CarAgent.IsAIActivated(), m_CarAgent.CountTowardsAIWorldLimit(), m_CarAgent.GetLOD(),
			m_Pilot.IsAIActivated(), m_iHandler, m_aPoints.Count(), m_vOrigin);
		if (!m_aPoints.IsEmpty()) details += string.Format(" pre_native_first=%1 pre_native_last=%2 pre_global_endpoint=%3 pre_global_goal=%4",
			m_aPoints[0], m_aPoints[m_aPoints.Count() - 1], m_vEndpoint, m_vGoal);
		return details;
	}
}
modded class AICF_MatchController
{
	protected int m_iUPStarted;
	protected int m_iUPNextReport;
	protected int m_iUPMoving;
	protected int m_iUPObservedRepairAt;
	protected bool m_bUPConfigured;
	protected bool m_bUPDone;
	protected bool m_bUPFarOrdered;
	protected AICF_TransportTrip m_UPTrip;
	protected Vehicle m_UPVehicle;
	protected EntityID m_UPVehicleId;
	protected string m_sUPVehicleRpl;
	protected int m_iUPDeadline;
	protected ref array<EntityID> m_aUPOriginals = {};
	protected ref array<string> m_aUPOriginalRpl = {};
	protected ref array<float> m_aUPHealth = {};
	protected float m_fUPVehicleHealth;
	protected string m_sUPSetupOperation;
	protected AICF_TransportTrip m_UPSetupTrip;
	protected AICF_VehicleLease m_UPSetupLease;
	protected int m_iUPSetupLeaseGeneration;
	protected int m_iUPSetupVehicleGeneration;
	protected int m_iUPSetupDeadline;
	protected int m_iUPFarCommandAt;
	protected bool m_bUPSetupWaiting;
	protected ref array<vector> m_aUPBirthPositions = {};
	protected bool m_bUPTruce;
	protected bool m_bUPDiagnosticFailure;
	protected ref AICF_VehicleParking m_UPDispatchBrake;
	protected bool m_bUPDispatchReleased;
	protected vector m_vUPDispatchReleasedAt;
	protected int m_iUPDispatchPollAt;
	protected int m_iUPDispatchMotionPolls;
	protected bool m_bUPDispatchNativePreempted;
	protected bool m_bUPDispatchNativeOriginCaptured;
	protected vector m_vUPDispatchNativeOrigin;
	protected int m_iUPDispatchReadyPollAt;
	protected int m_iUPDispatchReadyPolls;
	protected int m_iUPCorridorMotionAt;
	protected EntityID m_UPOriginalOldWaypointId;
	protected int m_iUPOriginalOldRoute;
	protected int m_iUPOriginalAssignment;
	protected int m_iUPOriginalIntent;
	protected bool m_bUPOriginalBound;
	protected AIWaypoint m_UPOriginalWaypoint;
	protected int m_iUPOriginalRoute;
	override AICF_TransitMoveFailure ReportVehicleTransitMoveFailure(SCR_AIGroup group,
		SCR_AIGroupUtilityComponent utility, int result, int handler, bool related, vector location)
	{
		if (AICF_UnstuckProbe.BrakeDispatch() && AICF_UPCurrent() && AICF_UnstuckProbe.GroupCurrent(group) && m_UPDispatchBrake && !m_bUPDispatchReleased)
		{
			m_bUPDispatchNativePreempted = true;
			AICF_UPReleaseBrake("BEFORE_UNCHANGED_NATIVE_FAILURE_SUPER");
		}
		if (!AICF_UnstuckProbe.Mode().IsEmpty() && AICF_UPCurrent() && AICF_UnstuckProbe.GroupCurrent(group) &&
			m_VehicleCoordinator && AICF_UnstuckProbe.s_Slot)
		{
			AICF_TransportTrip captured = m_VehicleCoordinator.GetTrip(AICF_UnstuckProbe.s_Slot);
			if (captured && captured.GetLease() && captured.GetLease().GetVehicle())
			{
				Vehicle vehicle = captured.GetLease().GetVehicle();
				string flagReceipt = AICF_UnstuckProbe.NativeFlags(captured, vehicle);
				Print(string.Format("[AICF][UNSTUCK_PROBE_BEFORE_NATIVE_FAILURE] operation=%1 car=%2 result=%3 handler=%4 related=%5 location=%6 origin=%7 physics=%8 before_super=1",
					captured.GetOperationId(), vehicle.GetID(), result, handler, related, location, vehicle.GetOrigin(), vehicle.GetPhysics()) + flagReceipt);
				if (AICF_UnstuckProbe.DiagnosticsOnly()) m_bUPDiagnosticFailure = true;
			}
		}
		return super.ReportVehicleTransitMoveFailure(group, utility, result, handler, related, location);
	}
	protected bool AICF_UPHealth(IEntity entity, out float health)
	{
		if (!entity) return false;
		DamageManagerComponent damage = DamageManagerComponent.Cast(entity.FindComponent(DamageManagerComponent));
		if (!damage || !damage.GetDefaultHitZone()) return false;
		health = damage.GetDefaultHitZone().GetHealthScaled();
		return health >= 0 && health <= 1;
	}
	protected bool AICF_UPOriginalsCurrent(AICF_TransportTrip trip, Vehicle car)
	{
		if (!trip || !car) return false;
		if (!AICF_UPCurrent() || trip != m_UPTrip || trip.GetLease() != m_UPSetupLease || !m_UPSetupLease) return false;
		if (car != m_UPVehicle || car.GetID() != m_UPVehicleId || AICF_VehicleBehaviorBinding.ComponentRpl(car) != m_sUPVehicleRpl) return false;
		if (trip.GetAbsoluteDeadlineMs() != m_iUPDeadline || trip.GetMovementState().GetMaximumMobilityRecoveryAttempts() != 2) return false;
		if (m_UPSetupLease.GetLeaseGeneration() != m_iUPSetupLeaseGeneration || m_UPSetupLease.GetVehicleGeneration() != m_iUPSetupVehicleGeneration) return false;
		float health;
		if (!AICF_UPHealth(car, health) || health != m_fUPVehicleHealth || health != 1) return false;
		for (int index; index < 5; index++)
		{
			IEntity actor = m_USSRState.GetSlot(0).GetRosterMember(index);
			if (!actor || actor.GetID() != m_aUPOriginals[index] || AICF_VehicleBehaviorBinding.ComponentRpl(actor) != m_aUPOriginalRpl[index]) return false;
			if (!AICF_GroupRuntime.IsAliveCharacter(actor) || !SCR_AIDamageHandling.IsConscious(actor)) return false;
			if (!AICF_VehicleBoardingMutationFence.IsAuthoritativeAIEntity(actor) || !AICF_VehicleNativeSubgroup.Settled(actor, car)) return false;
			if (!AICF_UPHealth(actor, health) || health != m_aUPHealth[index]) return false;
		}
		return true;
	}
	protected bool AICF_UPApplyPoseFault(AICF_TransportTrip trip, Vehicle car)
	{
		if (AICF_UnstuckProbe.s_PoseFault || !AICF_UnstuckProbe.s_FarScopeReady || !AICF_UPOriginalsCurrent(trip, car)) return false;
		AICF_UnstuckMutationScope scope = AICF_UnstuckMutationScope.Capture(trip, car);
		if (!scope || !car.GetPhysics() || trip.GetMovementState().GetMobilityRecoveryAttempts() != 0) return false;
		if (trip.GetMovementState().GetNativeGate() || !trip.GetMovementState().IsRouteWaypointBound()) return false;
		SCR_AIVehicleUsageComponent usage = SCR_AIVehicleUsageComponent.Cast(car.FindComponent(SCR_AIVehicleUsageComponent));
		SCR_DamageManagerComponent damage = SCR_DamageManagerComponent.GetDamageManager(car);
		if (!usage || usage.GetDamageState() != EDamageState.UNDAMAGED || !damage || damage.GetMovementDamage() != 0 || SCR_AIVehicleUsability.VehicleIsOnFire(car)) return false;
		if (!SCR_AIVehicleUsability.VehicleCanMove(car)) return false;
		vector before = car.GetOrigin();
		vector pose[4];
		car.GetWorldTransform(pose);
		pose[3] = AICF_UnstuckProbe.Pad();
		float distance = vector.Distance(before, pose[3]);
		bool localFault = distance > 0 && distance <= 15;
		if (!localFault) return false;
		AICF_VehicleWatchdog watchdog = new AICF_VehicleWatchdog();
		string reason;
		float nearest;
		if (!watchdog.CanSafelyRelocateVehicle(trip.GetAssignment().GetGroup(), car, 50, false, reason)) return false;
		if (!watchdog.CanApplyHiddenRecovery(before, pose[3], 50, nearest, reason)) return false;
		TraceOBB body = new TraceOBB();
		for (int axis; axis < 3; axis++) body.Mat[axis] = pose[axis];
		car.GetBounds(body.Mins, body.Maxs);
		body.Start = pose[3];
		body.Flags = TraceFlags.ENTS;
		body.LayerMask = EPhysicsLayerPresets.Vehicle;
		body.Exclude = car;
		float penetration = car.GetWorld().TracePosition(body, null);
		Print(string.Format("[AICF][UNSTUCK_PROBE_POSE_CLEARANCE] source=%1 destination=%2 distance_3d=%3 penetration=%4 hit=%5 unchanged_orientation=1", before, pose[3], distance, penetration, AICF_GroupTrace.Entity(body.TraceEnt)));
		if (penetration < 0 || !scope.IsCurrent() || !AICF_UPOriginalsCurrent(trip, car)) return false;
		if (car.GetOrigin() != before || !car.GetPhysics() || !scope.IsCurrent()) return false;
		AICF_UnstuckProbe.s_PoseFault = true;
		bool applied = car.SetWorldTransform(pose);
		Print(string.Format("[AICF][UNSTUCK_PROBE_POSE_FAULT] applied=%1 source=%2 actual=%3 distance_3d=%4 deadline=%5 attempts=%6 car=%7 operation=%8 rpl=%9 maximum_attempts=2 setup_fault_only=1 native_progress=0 orientation_preserved=1 health_writes=0",
			applied, before, car.GetOrigin(), vector.Distance(before, car.GetOrigin()), trip.GetAbsoluteDeadlineMs(), trip.GetMovementState().GetMobilityRecoveryAttempts(),
			car.GetID().ToString(), trip.GetOperationId(), AICF_VehicleBehaviorBinding.ComponentRpl(car)));
		return applied && trip.GetAbsoluteDeadlineMs() == m_iUPDeadline && AICF_UPOriginalsCurrent(trip, car);
	}
	protected void AICF_UPTruce()
	{
		if (m_bUPTruce) return;
		SCR_FactionManager manager = SCR_FactionManager.Cast(GetGame().GetFactionManager());
		if (!manager) return;
		array<Faction> factions = {};
		manager.GetFactionsList(factions);
		if (factions.Count() < 2) return;
		for (int first; first < factions.Count(); first++)
			for (int second = first + 1; second < factions.Count(); second++)
			{
				SCR_Faction a = SCR_Faction.Cast(factions[first]);
				SCR_Faction b = SCR_Faction.Cast(factions[second]);
				if (!a || !b) continue;
				manager.SetFactionsFriendly(a, b);
				if (!a.IsFactionFriendly(b) || !b.IsFactionFriendly(a)) return;
			}
		m_bUPTruce = true;
		Print("[AICF][UNSTUCK_PROBE_ENV] truce=1 protection=HIERARCHY health_writes=0 classification=CONTROLLED_SITE_SELECTION_FAULT");
	}

	override protected bool SpawnInitialRoster(AICF_FactionState factionState, SCR_CampaignFaction faction)
	{
		if (!AICF_UnstuckProbe.Mode().IsEmpty())
		{
			if (!AICF_UnstuckProbe.s_Owner) AICF_UnstuckProbe.s_Owner = this;
			if (AICF_UnstuckProbe.s_Owner != this) return super.SpawnInitialRoster(factionState, faction);
			AICF_UPTruce();
			if (!m_iUPStarted) m_iUPStarted = System.GetTickCount();
			if (factionState == m_USSRState)
			{
				factionState.GetSlot(0).SetUnitType(AICF_EGroupUnitType.MOTORIZED_LIGHT);
				factionState.GetSlot(0).SetDesiredSize(5);
			}
		}
		return super.SpawnInitialRoster(factionState, faction);
	}
	protected bool AICF_UPCurrent()
	{
		return Replication.IsServer() && !m_bStopped && m_bStarted && AICF_MatchController.GetActiveController() == this &&
			AICF_UnstuckProbe.s_Owner == this &&
			m_USSRState && m_USSRFaction && m_USSRState.GetSlot(0) && AICF_UnstuckProbe.GroupCurrent(m_USSRState.GetSlot(0).GetGroup());
	}
	protected void AICF_UPFinish(bool passed, string reason)
	{
		if (m_bUPDone || m_bStopped || AICF_MatchController.GetActiveController() != this || AICF_UnstuckProbe.s_Owner != this) return;
		AICF_UPReleaseBrake("FINISH_" + reason);
		m_bUPDone = true;
		Print(string.Format("[AICF][UNSTUCK_PROBE_FINISHED] mode=%1 passed=%2 reason=%3 repairs=%4 forward=%5 native_motion_polls=%6 classification=CONTROLLED_SITE_SELECTION_FAULT",
			AICF_UnstuckProbe.Mode(), passed, reason, AICF_UnstuckProbe.s_Repairs, AICF_UnstuckProbe.s_Forward, m_iUPMoving));
		AICF_UnstuckProbe.s_Trip = null;
		AICF_UnstuckProbe.s_Slot = null;
		GetGame().RequestClose();
	}
	protected void AICF_UPReleaseBrake(string reason)
	{
		if (!m_UPDispatchBrake) return;
		bool released = m_UPDispatchBrake.AICF_UPReleaseDispatchBlock(m_UPTrip);
		Print(string.Format("[AICF][UNSTUCK_BRAKE_RELEASE_RECEIPT] reason=%1 released=%2 native_preempted=%3 identity_safe_release_required=1 unreleased_is_not_pass=1",
			reason, released, m_bUPDispatchNativePreempted));
	}
	protected bool AICF_UPMoveActivityReady(AICF_TransportTrip trip)
	{
		if (!trip || !trip.GetAssignment() || !trip.GetAssignment().GetGroup() || !trip.GetMovementState()) return false;
		SCR_AIGroupUtilityComponent utility = trip.GetAssignment().GetGroup().GetGroupUtilityComponent();
		if (!utility) return false;
		SCR_AIMoveActivity current = SCR_AIMoveActivity.Cast(utility.GetCurrentAction());
		SCR_AIMoveActivity executed = SCR_AIMoveActivity.Cast(utility.GetExecutedAction());
		AIWaypoint waypoint = trip.GetMovementState().GetRouteWaypoint();
		return current && current == executed && waypoint && current.m_RelatedWaypoint == waypoint &&
			current.GetActionState() == EAIActionState.RUNNING && trip.GetMovementState().IsRouteWaypointBound();
	}
	protected bool AICF_UPOriginalRetargetReady(AICF_GroupSlot slot, AICF_TransportTrip trip, Vehicle car)
	{
		if (!slot || !trip || !car || !AICF_UPOriginalsCurrent(trip, car) || m_VehicleCoordinator.GetTrip(slot) != trip)
		{
			AICF_UPFinish(false, "CORRIDOR_RETARGET_ORIGINAL_CONTEXT_LOST");
			return false;
		}
		if (slot.GetGroup() != trip.GetAssignment().GetGroup() || slot.GetSpawnGeneration() != trip.GetGroupGeneration())
		{
			AICF_UPFinish(false, "CORRIDOR_RETARGET_ORIGINAL_GROUP_EPOCH_CHANGED");
			return false;
		}
		AICF_VehicleMovementState state = trip.GetMovementState();
		if (!state || state.GetMobilityRecoveryAttempts() != 0)
		{
			AICF_UPFinish(false, "CORRIDOR_RETARGET_BUDGET_CHANGED_BEFORE_CONTROLLED_FAULT");
			return false;
		}
		if (!AICF_UnstuckProbe.s_OriginalRequested)
		{
			float approach = vector.Distance(car.GetOrigin(), AICF_UnstuckProbe.Pad());
			if (approach > 30 || !AICF_UPMoveActivityReady(trip)) return false;
			if (state.GetMobilityRecoveryAttempts() != 0 || !state.GetRouteWaypoint() || !AICF_UnstuckMutationScope.Capture(trip, car))
			{
				AICF_UPFinish(false, "CORRIDOR_RETARGET_BEFORE_FAULT_BUDGET_OR_OWNER_CHANGED");
				return false;
			}
			m_UPOriginalOldWaypointId = state.GetRouteWaypoint().GetID();
			m_iUPOriginalOldRoute = state.GetRouteGeneration();
			if (!m_OrderPlanner.AssignPlayerPointOrder(slot, m_USSRFaction, "1475.78 36.6684 2473.24", m_VehicleCoordinator.IsInfantryOrderSuspended(slot)))
			{
				AICF_UPFinish(false, "CORRIDOR_NORMAL_ORIGINAL_COMMAND_REJECTED");
				return false;
			}
			if (!trip || !car || !AICF_UPOriginalsCurrent(trip, car) || m_VehicleCoordinator.GetTrip(slot) != trip ||
				slot.GetGroup() != trip.GetAssignment().GetGroup() || slot.GetSpawnGeneration() != trip.GetGroupGeneration())
			{
				AICF_UPFinish(false, "CORRIDOR_NORMAL_COMMAND_CHANGED_CONTEXT_OR_ORIGINAL_DEADLINE");
				return false;
			}
			AICF_UnstuckProbe.s_OriginalRequested = true;
			m_iUPOriginalAssignment = slot.GetStrategicAssignmentRevision();
			m_iUPOriginalIntent = slot.GetStrategicIntentRevision();
			Print(string.Format("[AICF][UNSTUCK_CORRIDOR_RETARGET_REQUEST] operation=%1 car=%2 approach_3d=%3 old_wp=%4 old_route=%5 assignment=%6 intent=%7 deadline=%8 normal_owner_command=1 purchases=1 budget_before=0",
				trip.GetOperationId(), car.GetID().ToString(), approach, m_UPOriginalOldWaypointId.ToString(), m_iUPOriginalOldRoute,
				m_iUPOriginalAssignment, m_iUPOriginalIntent, m_iUPDeadline));
			return false;
		}
		if (trip.GetAssignment().GetAssignmentRevision() != m_iUPOriginalAssignment || slot.GetStrategicAssignmentRevision() != m_iUPOriginalAssignment ||
			trip.GetAssignment().GetStrategicIntentRevision() != m_iUPOriginalIntent || slot.GetStrategicIntentRevision() != m_iUPOriginalIntent) return false;
		if (!AICF_UPMoveActivityReady(trip) || !state.GetRouteWaypoint() || state.GetRouteWaypoint().GetID() == m_UPOriginalOldWaypointId) return false;
		if (m_bUPOriginalBound)
		{
			if (state.GetRouteWaypoint() != m_UPOriginalWaypoint || state.GetRouteGeneration() != m_iUPOriginalRoute)
			{
				AICF_UPFinish(false, "CORRIDOR_BOUND_ORIGINAL_ROUTE_CHANGED_BEFORE_FAULT");
				return false;
			}
		}
		AICF_UnstuckMutationScope scope = AICF_UnstuckMutationScope.Capture(trip, car);
		if (!scope) return false;
		SCR_AIGroup group;
		Vehicle actual;
		SCR_AIGroupVehicle record;
		AIAgent pilot;
		AIGroupMovementComponent movement;
		string reason;
		SCR_AIGroupUtilityComponent utility = trip.GetAssignment().GetGroup().GetGroupUtilityComponent();
		if (!AICF_VehicleNativeSubgroup.Read(trip, utility, group, actual, record, pilot, movement, reason) || actual != car || !pilot || !record) return false;
		AICarMovementComponent nativeCar = AICarMovementComponent.Cast(pilot.GetMovementComponent());
		if (!nativeCar || !nativeCar.GetAIAgent() || nativeCar.GetAIAgent().GetControlledEntity() != car) return false;
		array<vector> path = {};
		array<vector> repeated = {};
		nativeCar.GetCurrentPath(path);
		if (!AICF_VehicleNativeRouteProgress.ValidatePath(path, state.GetRouteEndpoint())) return false;
		nativeCar.GetCurrentPath(repeated);
		if (!trip || !car || !state || !nativeCar || !record || !pilot || !utility) return false;
		if (!AICF_VehicleNativeRouteProgress.SamePath(path, repeated) || !scope.IsCurrent() || !AICF_UPMoveActivityReady(trip)) return false;
		if (!AICF_VehicleNativeSubgroup.IsCurrentVehicleHandler(trip, utility, record.GetSubgroupHandleId()) || !AICF_UPOriginalsCurrent(trip, car)) return false;
		if (state.GetMobilityRecoveryAttempts() != 0 || !state.GetRouteWaypoint() || !scope.IsCurrent() || !AICF_UPMoveActivityReady(trip)) return false;
		if (m_bUPOriginalBound) return true;
		m_bUPOriginalBound = true;
		m_UPOriginalWaypoint = state.GetRouteWaypoint();
		m_iUPOriginalRoute = state.GetRouteGeneration();
		Print(string.Format("[AICF][UNSTUCK_CORRIDOR_ORIGINAL_BOUND] operation=%1 car=%2 deadline=%3 assignment=%4 intent=%5 new_wp=%6 new_route=%7 native_points=%8 endpoint=%9 normal_adoption=1 same_trip=1 same_lease=1 purchases=1 budget_before=0",
			trip.GetOperationId(), car.GetID().ToString(), m_iUPDeadline, m_iUPOriginalAssignment, m_iUPOriginalIntent,
			m_UPOriginalWaypoint.GetID().ToString(), m_iUPOriginalRoute, path.Count(), state.GetRouteEndpoint()));
		return true;
	}
	protected void AICF_UPBrakeTick(AICF_TransportTrip trip, Vehicle car, int now)
	{
		if (!AICF_UPOriginalsCurrent(trip, car) || AICF_UnstuckProbe.s_Purchases != 1 ||
			!AICF_UnstuckMutationScope.Capture(trip, car))
		{
			AICF_UPFinish(false, "BRAKE_IDENTITY_HEALTH_ORIGINAL_DEADLINE_CHANGED");
			return;
		}
		AICF_VehicleMovementState movement = trip.GetMovementState();
		if (!m_UPDispatchBrake)
		{
			if (movement.GetMobilityRecoveryAttempts() != 0 || movement.GetNativeGate() || !movement.IsRouteWaypointBound()) return;
			if (!m_bUPDispatchNativeOriginCaptured)
			{
				m_bUPDispatchNativeOriginCaptured = true;
				m_vUPDispatchNativeOrigin = car.GetOrigin();
			}
			AICF_VehicleParking candidate = new AICF_VehicleParking(trip);
			if (candidate.AICF_UPWaitForNativeRelease(trip))
			{
				Print("[AICF][UNSTUCK_BRAKE_WAIT_NATIVE_RELEASE] " + candidate.AICF_UPDispatchGuardReceipt(trip) + " owned_writes=0 original_300s_bound=1");
				m_iUPDispatchReadyPolls = 0;
				return;
			}
			if (!candidate.AICF_UPNativeBrakeFalse(trip))
			{
				Print("[AICF][UNSTUCK_BRAKE_REJECTED] " + candidate.AICF_UPDispatchGuardReceipt(trip));
				AICF_UPFinish(false, "BRAKE_PRE_MOTION_CONTEXT_REJECTED");
				return;
			}
			if (now - m_iUPDispatchReadyPollAt < 500) return;
			m_iUPDispatchReadyPollAt = now;
			float nativeBefore = vector.Distance(m_vUPDispatchNativeOrigin, car.GetOrigin());
			bool moveReady = AICF_UPMoveActivityReady(trip);
			if (!moveReady || nativeBefore < 10 || !car.GetPhysics() || car.GetPhysics().GetVelocity().Length() < 1)
			{
				m_iUPDispatchReadyPolls = 0;
				return;
			}
			m_iUPDispatchReadyPolls++;
			Print(string.Format("[AICF][UNSTUCK_BRAKE_PRE_NATIVE_MOTION] operation=%1 car=%2 native_distance=%3 distinct_polls=%4 observed_at=%5 current_executed_move=1 native_brake=0 attempts=0",
				trip.GetOperationId(), car.GetID().ToString(), nativeBefore, m_iUPDispatchReadyPolls, now));
			if (m_iUPDispatchReadyPolls < 3) return;
			if (!candidate.AICF_UPBeginDispatchBlock(trip))
			{
				Print("[AICF][UNSTUCK_BRAKE_REJECTED] " + candidate.AICF_UPDispatchGuardReceipt(trip));
				AICF_UPFinish(false, "BRAKE_INITIAL_FALSE_OR_OWNERSHIP_UNPROVEN");
				return;
			}
			m_UPDispatchBrake = candidate;
			Print(string.Format("[AICF][UNSTUCK_BRAKE_APPLIED] operation=%1 car=%2 rpl=%3 deadline=%4 attempts=0 initial_flag=0 own_write=1 fake_stuck=0 classification=CONTROLLED_BRAKE_BLOCK",
				trip.GetOperationId(), car.GetID().ToString(), AICF_VehicleBehaviorBinding.ComponentRpl(car), m_iUPDeadline));
			return;
		}
		if (!m_bUPDispatchReleased)
		{
			string blockDetails;
			bool blockCurrent = m_UPDispatchBrake.AICF_UPObserveDispatchBlock(trip, blockDetails);
			AICF_TransportNativeGate gate = movement.GetNativeGate();
			Print("[AICF][UNSTUCK_BRAKE_OBSERVE] " + blockDetails + string.Format(" gate=%1 gate_present=%2", gate, gate != null));
			if (!blockCurrent || gate)
			{
				AICF_UPFinish(false, "BRAKE_NATIVE_OVERRIDE_OR_GATE_PREEMPTED_COVERAGE");
				return;
			}
			if (movement.GetMobilityRecoveryAttempts() != 2 || AICF_UnstuckProbe.s_Repairs != 2) return;
			if (movement.GetUnstuckFailedSites().Count() != 2 || !movement.IsRouteWaypointBound())
			{
				AICF_UPFinish(false, "BRAKE_TWO_PROVEN_ORIGINS_OR_BOUND_ROUTE_MISSING");
				return;
			}
			m_UPDispatchBrake.AICF_UPReleaseDispatchBlock(trip);
			CarControllerComponent controller = CarControllerComponent.Cast(car.FindComponent(CarControllerComponent));
			if (!m_UPDispatchBrake.AICF_UPDispatchBlockReleased() || !controller || controller.GetPersistentHandBrake())
			{
				AICF_UPFinish(false, "BRAKE_OWN_RELEASE_NOT_CONFIRMED");
				return;
			}
			m_bUPDispatchReleased = true;
			m_vUPDispatchReleasedAt = car.GetOrigin();
			Print(string.Format("[AICF][UNSTUCK_BRAKE_RELEASED] operation=%1 car=%2 origin=%3 deadline=%4 attempts=2 failed_sites=2 persistent_brake=0 physical_relocations=2 cycle_case_proven=0",
				trip.GetOperationId(), car.GetID().ToString(), car.GetOrigin(), m_iUPDeadline));
			return;
		}
		if (now - m_iUPDispatchPollAt < 500) return;
		m_iUPDispatchPollAt = now;
		float distance = vector.Distance(m_vUPDispatchReleasedAt, car.GetOrigin());
		if (car.GetPhysics() && car.GetPhysics().GetVelocity().Length() >= 1 && distance >= 10) m_iUPDispatchMotionPolls++;
		if (m_iUPDispatchMotionPolls >= 3 && movement.GetMobilityRecoveryAttempts() == 2)
		{
			Print(string.Format("[AICF][UNSTUCK_BRAKE_NATIVE_MOTION] operation=%1 car=%2 actual_distance=%3 distinct_polls=%4 deadline=%5 attempts=2 health_unchanged=1 purchases=1 cycle_case_proven=0",
				trip.GetOperationId(), car.GetID().ToString(), distance, m_iUPDispatchMotionPolls, m_iUPDeadline));
			AICF_UPFinish(true, "CONTROLLED_BRAKE_TWO_REAL_CORRECTIONS_AND_NATIVE_10M");
		}
	}
	override protected void ProcessFaction(AICF_FactionState factionState, SCR_CampaignFaction faction)
	{
		super.ProcessFaction(factionState, faction);
		if (AICF_UnstuckProbe.NeighborDiagnostics()) return;
		if (AICF_UnstuckProbe.Mode().IsEmpty() || m_bUPConfigured || m_bUPDone || factionState != m_USSRState || !AICF_UPCurrent()) return;
		AICF_GroupSlot slot = factionState.GetSlot(0);
		if (!slot.IsCombatReady() || slot.GetState() != AICF_EGroupSlotState.READY) return;
		int actual, wrong, dead;
		if (!AICF_GroupRuntime.HasExactFactionRoster(slot.GetGroup(), "USSR", 5, actual, wrong, dead)) return;
		if (!m_VehicleCoordinator || m_VehicleCoordinator.GetTrip(slot)) { AICF_UPFinish(false, "PRESET_REQUIRED_BEFORE_FIRST_TRIP"); return; }
		ResourceName prefab = "{259EE7B78C51B624}Prefabs/Vehicles/Wheeled/UAZ469/UAZ469.et";
		AICF_VehiclePresetCatalog catalog = new AICF_VehiclePresetCatalog(faction);
		string reason;
		if (!catalog.Inspect(prefab, reason) || catalog.Seats().Count() != 5) { catalog.Clear(); AICF_UPFinish(false, "MODEL_INSPECTION"); return; }
		AICF_VehiclePreset recipe = catalog.Draft(prefab);
		int member;
		for (int mandatory; mandatory < catalog.Seats().Count(); mandatory++)
			if (catalog.Seats()[mandatory].GetType() == ECompartmentType.PILOT) recipe.Assign(mandatory, member++);
		for (int seat; seat < catalog.Seats().Count(); seat++)
			if (recipe.m_aMembers[seat] < 0) recipe.Assign(seat, member++);
		bool valid = recipe.ValidateRoster(slot, catalog.Seats(), reason) && PreflightVehiclePreset(slot, faction, recipe, reason);
		catalog.Clear();
		if (!valid) { AICF_UPFinish(false, "PRESET_PREFLIGHT_" + reason); return; }
		if (!AICF_UPCurrent() || !slot.SetVehiclePreset(recipe, slot.GetVehiclePresetRevision())) { AICF_UPFinish(false, "PRESET_APPLY"); return; }
		if (!m_bUPTruce) { AICF_UPFinish(false, "TRUCE_NOT_CONFIRMED"); return; }
		if (!AICF_UPCurrent() || !m_OrderPlanner.AssignPlayerPointOrder(slot, faction, AICF_UnstuckProbe.BirthPad(), false)) { AICF_UPFinish(false, "OWNER_NEAR_POINT_ORDER"); return; }
		for (int index; index < 5; index++)
		{
			IEntity actor = slot.GetRosterMember(index);
			if (!actor || vector.DistanceXZ(actor.GetOrigin(), AICF_UnstuckProbe.BirthPad()) > 30) { AICF_UPFinish(false, "ACTUAL_NATIVE_BIRTH_NOT_LOCAL"); return; }
			m_aUPOriginals.Insert(actor.GetID());
			m_aUPBirthPositions.Insert(actor.GetOrigin());
			string actorRpl = AICF_VehicleBehaviorBinding.ComponentRpl(actor);
			if (actorRpl.IsEmpty() || actorRpl == "NONE" || m_aUPOriginalRpl.Contains(actorRpl)) { AICF_UPFinish(false, "ACTOR_RPL_UNPROVEN"); return; }
			m_aUPOriginalRpl.Insert(actorRpl);
			float health;
			if (!AICF_UPHealth(actor, health)) { AICF_UPFinish(false, "ACTOR_HEALTH_UNPROVEN"); return; }
			m_aUPHealth.Insert(health);
			string protection;
			if (!AICF_MultiHierarchyProtection.Apply(actor, protection)) { AICF_UPFinish(false, "ACTOR_PROTECTION_" + protection); return; }
		}
		m_bUPConfigured = true;
		AICF_UnstuckProbe.s_Slot = slot;
		Print(string.Format("[AICF][UNSTUCK_PROBE_ACTUAL_READY] group=%1 generation=%2 actual=%3 registered=%4 original_ids=%5 original_rpls=%6 physical_birth_positions=%7 ready_owner=ProcessFaction fake_ready=0 actor_teleports=0",
			slot.GetGroup().GetID(), slot.GetSpawnGeneration(), actual, FindManagedInfantrySlot(slot.GetGroup()) == slot, m_aUPOriginals, m_aUPOriginalRpl, m_aUPBirthPositions));
	}
	override protected void Update()
	{
		super.Update();
		if (AICF_UnstuckProbe.Mode().IsEmpty() || m_bUPDone || m_bStopped || AICF_MatchController.GetActiveController() != this || AICF_UnstuckProbe.s_Owner != this) return;
		int now = System.GetTickCount();
		if (now - m_iUPStarted > 300000) { AICF_UPFinish(false, "BOUNDED_NATIVE_SETUP_OR_MOTION_TIMEOUT"); return; }
		if (!AICF_UPCurrent()) return;
		if (AICF_UnstuckProbe.s_NeighborMetadataDone && m_bRosterReady) { AICF_UPFinish(false, "READONLY_NATIVE_NEIGHBOR_GOALS_CAPTURED"); return; }
		if (m_bUPDispatchNativePreempted) { AICF_UPFinish(false, "BRAKE_ACTUAL_NATIVE_FAILURE_PREEMPTED_WATCHDOG_COVERAGE"); return; }
		if (m_bUPDiagnosticFailure) { AICF_UPFinish(false, "DIAGNOSTIC_NATIVE_FAILURE_CAPTURED"); return; }
		if (AICF_UnstuckProbe.s_SafePadFailed)
		{
			string failed = "NO_LOCAL_SAFE_8M_PAD_WITHIN_15M";
			if (AICF_UnstuckProbe.FreshSetup()) failed = "NO_FRESH_SAFE_8M_PAD_WITHIN_56M_TESTED24";
			if (AICF_UnstuckProbe.RoadSetup()) failed = "NO_SAFE_NATIVE_ROAD_PAD_TESTED24_ARC240";
			AICF_UPFinish(false, failed);
			return;
		}
		if (!m_bUPConfigured || !m_bRosterReady || !m_VehicleCoordinator) return;
		AICF_GroupSlot slot = m_USSRState.GetSlot(0);
		AICF_TransportTrip trip = m_VehicleCoordinator.GetTrip(slot);
		if (!trip) return;
		AICF_UnstuckProbe.s_Trip = trip;
		if (trip.IsTerminal()) { AICF_UPFinish(false, "ACTUAL_TRIP_TERMINAL_" + trip.GetTerminalReason()); return; }
		Vehicle car;
		if (trip.GetLease()) car = trip.GetLease().GetVehicle();
		if (car && !car.GetPhysics()) { AICF_UPFinish(false, "VEHICLE_PHYSICS_UNPROVEN"); return; }
		if (car && !m_UPVehicle)
		{
			m_UPVehicle = car; m_UPVehicleId = car.GetID(); m_sUPVehicleRpl = AICF_VehicleBehaviorBinding.ComponentRpl(car);
			if (!AICF_UPHealth(car, m_fUPVehicleHealth)) { AICF_UPFinish(false, "VEHICLE_HEALTH_UNPROVEN"); return; }
		}
		if (car)
		{
			string details;
			AICF_MultiHierarchyProtection.Apply(car, details);
			for (int actorIndex; actorIndex < 5; actorIndex++) AICF_MultiHierarchyProtection.Apply(slot.GetRosterMember(actorIndex), details);
		}
		if (trip.GetPhase() != AICF_ETransportTripPhase.TRANSIT || !car) return;
		if (!m_bUPFarOrdered)
		{
			bool settled = trip.IsVehiclePresetCurrent();
			for (int seated; seated < 5; seated++)
				if (!AICF_VehicleNativeSubgroup.Settled(slot.GetRosterMember(seated), car)) settled = false;
			if (!settled || !AICF_UPCurrent()) return;
			m_UPSetupTrip = trip;
			m_UPSetupLease = trip.GetLease();
			m_iUPSetupLeaseGeneration = m_UPSetupLease.GetLeaseGeneration();
			m_iUPSetupVehicleGeneration = m_UPSetupLease.GetVehicleGeneration();
			m_iUPSetupDeadline = trip.GetAbsoluteDeadlineMs();
			m_iUPFarCommandAt = System.GetTickCount();
			m_bUPSetupWaiting = trip.IsPresetWaitingForDestination();
			if (!m_OrderPlanner.AssignPlayerPointOrder(slot, m_USSRFaction, AICF_UnstuckProbe.FarGoal(), m_VehicleCoordinator.IsInfantryOrderSuspended(slot)))
			{
				AICF_UPFinish(false, "OWNER_FAR_POINT_ORDER"); return;
			}
			m_bUPFarOrdered = true;
			m_sUPSetupOperation = trip.GetOperationId();
			Print(string.Format("[AICF][UNSTUCK_PROBE_FAR_ORDER] setup_operation=%1 vehicle=%2 actual_originals_seated=5 target=%3 phase_not_written=1", m_sUPSetupOperation, car.GetID(), slot.GetTargetPosition()));
			return;
		}
		if (!trip.GetAssignment() || trip.GetAssignment().GetTargetPosition() != AICF_UnstuckProbe.FarGoal()) return;
		if (!m_UPTrip)
		{
			if (!trip.GetMovementState().IsRouteWaypointBound() || !trip.GetMovementState().GetRouteWaypoint()) return;
			if (trip.IsPresetWaitingForDestination()) return;
			if (trip != m_UPSetupTrip || trip.GetLease() != m_UPSetupLease || !m_UPSetupLease ||
				m_UPSetupLease.GetLeaseGeneration() != m_iUPSetupLeaseGeneration || m_UPSetupLease.GetVehicleGeneration() != m_iUPSetupVehicleGeneration)
			{
				AICF_UPFinish(false, "SETUP_TO_FAR_TRIP_OR_LEASE_CHANGED"); return;
			}
			int farDeadline = trip.GetAbsoluteDeadlineMs();
			int elapsedAdoption = System.GetTickCount() - m_iUPFarCommandAt;
			int deadlineDelta = farDeadline - m_iUPSetupDeadline;
			bool ordinaryWait = elapsedAdoption >= 0 && (deadlineDelta == 0 || (m_bUPSetupWaiting && deadlineDelta >= 0 && deadlineDelta <= elapsedAdoption));
			if (!ordinaryWait || AICF_UnstuckProbe.s_Purchases != 1 || car != m_UPVehicle || car.GetID() != m_UPVehicleId)
			{
				AICF_UPFinish(false, "SETUP_TO_FAR_CLOCK_PURCHASE_OR_CAR_CHANGED"); return;
			}
			m_UPTrip = trip;
			m_iUPDeadline = farDeadline;
			AICF_UnstuckProbe.s_FarScopeReady = true;
			if (AICF_UnstuckProbe.HistoricalCorridor())
				Print(string.Format("[AICF][UNSTUCK_CORRIDOR_SCOPE] operation=%1 car=%2 goal=%3 fault=%4 deadline=%5 historical_source=MIN3_FIRST_STUCK_ORIGIN historical_yaw=UNKNOWN setup_fault_preserves_actual_orientation=1 native_drivable_proven=0 local_or_full_hint_required_after_fault=1",
					trip.GetOperationId(), car.GetID().ToString(), AICF_UnstuckProbe.FarGoal(), AICF_UnstuckProbe.Pad(), m_iUPDeadline));
			Print(string.Format("[AICF][UNSTUCK_PROBE_FAR_SCOPE] setup_operation=%1 far_operation=%2 deadline=%3 maximum_attempts=%4 same_vehicle=%5 same_trip=1 same_lease=1 purchases=%6 normal_wait_delta_ms=%7 adoption_elapsed_ms=%8",
				m_sUPSetupOperation, trip.GetOperationId(), m_iUPDeadline, trip.GetMovementState().GetMaximumMobilityRecoveryAttempts(), car.GetID() == m_UPVehicleId,
				AICF_UnstuckProbe.s_Purchases, deadlineDelta, elapsedAdoption));
		}
		if (now >= m_iUPNextReport)
		{
			m_iUPNextReport = now + 1000;
			string nativeReceipt = AICF_UnstuckProbe.NativeFlags(trip, car);
			Print(string.Format("[AICF][UNSTUCK_PROBE_NATIVE] operation=%1 car=%2 phase=TRANSIT position=%3 velocity=%4 attempts=%5 route=%6 deadline=%7 exact_registry=%8",
				trip.GetOperationId(), car.GetID().ToString(), car.GetOrigin(), car.GetPhysics().GetVelocity(), trip.GetMovementState().GetMobilityRecoveryAttempts(),
				trip.GetMovementState().GetRouteGeneration(), trip.GetAbsoluteDeadlineMs(), FindManagedInfantrySlot(slot.GetGroup()) == slot) + nativeReceipt);
		}
		if (AICF_UnstuckProbe.BrakeDispatch())
		{
			AICF_UPBrakeTick(trip, car, now);
			return;
		}
		if (AICF_UnstuckProbe.ReturnOriginal() && !AICF_UnstuckProbe.s_PoseFault && !AICF_UPOriginalRetargetReady(slot, trip, car)) return;
		if (AICF_UnstuckProbe.FreshSetup() && !AICF_UnstuckProbe.s_PoseFault && vector.Distance(car.GetOrigin(), AICF_UnstuckProbe.Pad()) <= 15)
		{
			if (!AICF_UPApplyPoseFault(trip, car)) AICF_UPFinish(false, "BOUNDED_POSE_FAULT_CONTEXT_OR_DESTINATION_REJECTED");
			return;
		}
		if (!AICF_UnstuckProbe.s_RepairAt) return;
		if (m_iUPObservedRepairAt != AICF_UnstuckProbe.s_RepairAt)
		{
			m_iUPObservedRepairAt = AICF_UnstuckProbe.s_RepairAt;
			m_iUPMoving = 0;
		}
		bool health = trip == m_UPTrip && car == m_UPVehicle && car.GetID() == m_UPVehicleId;
		if (health) health = AICF_VehicleBehaviorBinding.ComponentRpl(car) == m_sUPVehicleRpl;
		if (health) health = trip.GetAbsoluteDeadlineMs() == m_iUPDeadline;
		float currentHealth;
		if (health) health = AICF_UPHealth(car, currentHealth) && currentHealth == m_fUPVehicleHealth;
		for (int original; original < 5; original++)
		{
			IEntity actor = slot.GetRosterMember(original);
			if (!actor || actor.GetID() != m_aUPOriginals[original]) health = false;
			if (health) health = AICF_VehicleBehaviorBinding.ComponentRpl(actor) == m_aUPOriginalRpl[original];
			if (health) health = AICF_UPHealth(actor, currentHealth) && currentHealth == m_aUPHealth[original];
		}
		if (!health) { AICF_UPFinish(false, "ORIGINAL_IDENTITY_HEALTH_OR_DEADLINE_CHANGED"); return; }
		if (trip.GetLease() != m_UPSetupLease || !m_UPSetupLease || AICF_UnstuckProbe.s_Purchases != 1 ||
			m_UPSetupLease.GetLeaseGeneration() != m_iUPSetupLeaseGeneration || m_UPSetupLease.GetVehicleGeneration() != m_iUPSetupVehicleGeneration)
		{
			AICF_UPFinish(false, "FAR_LEASE_OR_PURCHASE_CHANGED"); return;
		}
		if (AICF_UnstuckProbe.HistoricalCorridor())
		{
			bool actualHint = AICF_UnstuckProbe.s_PreProven && (AICF_UnstuckProbe.s_LocalThree || AICF_UnstuckProbe.s_FullNative);
			if (AICF_UnstuckProbe.Mode() == "BEFORE" && actualHint && !AICF_UnstuckProbe.s_Forward)
			{
				AICF_UPFinish(true, "CORRIDOR_BEFORE_ACTUAL_NONFORWARD_CORRECTION");
				return;
			}
			float nativeDistance = vector.Distance(AICF_UnstuckProbe.s_Repaired, car.GetOrigin());
			if (now - m_iUPCorridorMotionAt >= 500 && car.GetPhysics().GetVelocity().Length() >= 1 && nativeDistance >= 10)
			{
				m_iUPCorridorMotionAt = now;
				m_iUPMoving++;
				Print(string.Format("[AICF][UNSTUCK_CORRIDOR_NATIVE_MOTION] operation=%1 car=%2 rpl=%3 actual_distance=%4 distinct_polls=%5 observed_at=%6 deadline=%7 attempts=%8 hint_only=1 path_progress=0 maximum_attempts=2 health_unchanged=1 purchases=1 pose_fault=1",
					trip.GetOperationId(), car.GetID().ToString(), AICF_VehicleBehaviorBinding.ComponentRpl(car), nativeDistance, m_iUPMoving, now,
					m_iUPDeadline, trip.GetMovementState().GetMobilityRecoveryAttempts()));
			}
			if (AICF_UnstuckProbe.Mode() == "AFTER" && actualHint && AICF_UnstuckProbe.s_Forward && m_iUPMoving >= 3 &&
				vector.DistanceXZ(AICF_UnstuckProbe.s_Repaired, car.GetOrigin()) >= 10 && trip.GetMovementState().GetMaximumMobilityRecoveryAttempts() == 2)
				AICF_UPFinish(true, "CORRIDOR_AFTER_ACTUAL_FORWARD_CORRECTION_AND_NATIVE_10M");
			return;
		}
		if (AICF_UnstuckProbe.Mode() == "BEFORE" && AICF_UnstuckProbe.s_PreProven && AICF_UnstuckProbe.s_LocalThree && !AICF_UnstuckProbe.s_Forward)
		{
			AICF_UPFinish(true, "BEFORE_ACTUAL_NONFORWARD_CORRECTION"); return;
		}
		if (car.GetPhysics().GetVelocity().Length() >= 1) m_iUPMoving++;
		if (AICF_UnstuckProbe.Mode() == "AFTER" && AICF_UnstuckProbe.s_PreProven && AICF_UnstuckProbe.s_LocalThree && AICF_UnstuckProbe.s_Forward && m_iUPMoving >= 3 &&
			vector.DistanceXZ(AICF_UnstuckProbe.s_Repaired, car.GetOrigin()) >= 10 && trip.GetMovementState().GetMaximumMobilityRecoveryAttempts() == 2)
			AICF_UPFinish(true, "AFTER_ACTUAL_FORWARD_CORRECTION_AND_NATIVE_10M");
	}
	override protected void Stop(bool cleanupEntities)
	{
		AICF_UPReleaseBrake("STOP");
		if (AICF_UnstuckProbe.s_Owner == this)
		{
			AICF_UnstuckProbe.s_Trip = null;
			AICF_UnstuckProbe.s_Slot = null;
			AICF_UnstuckProbe.s_Group = null;
			AICF_UnstuckProbe.s_PreHint = null;
			AICF_UnstuckProbe.s_Owner = null;
			AICF_UnstuckProbe.s_SafePadReady = false;
			AICF_UnstuckProbe.s_SafePadFailed = false;
			AICF_UnstuckProbe.s_FarScopeReady = false;
			AICF_UnstuckProbe.s_PoseFault = false;
			AICF_UnstuckProbe.s_DiagnosticRays = 0;
			AICF_UnstuckProbe.s_NeighborMetadataDone = false;
			AICF_UnstuckProbe.s_OriginalRequested = false;
		}
		super.Stop(cleanupEntities);
	}
}
