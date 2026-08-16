// Construction-only factory for transient vehicle waypoints. Queue ownership
// and deletion belong exclusively to AICF_VehicleTaskHandoff.
class AICF_VehicleWaypointFactory
{
	protected static const ResourceName MOVE_WAYPOINT_PREFAB = "{750A8D1695BD6998}Prefabs/AI/Waypoints/AIWaypoint_Move.et";
	protected static const ResourceName GET_OUT_WAYPOINT_PREFAB = "{C40316EE26846CAB}Prefabs/AI/Waypoints/AIWaypoint_GetOut.et";
	protected static const float DISMOUNT_COMPLETION_RADIUS_METERS = 12.0;
	protected static const float MOVE_COMPLETION_RADIUS_METERS = 25.0;
	protected static const float MINIMUM_STAGING_COMPLETION_RADIUS_METERS = 5.0;

	AIWaypoint CreateSpawnStagingWaypoint(
		vector stagingPosition,
		float completionRadiusMeters)
	{
		if (completionRadiusMeters < MINIMUM_STAGING_COMPLETION_RADIUS_METERS)
			completionRadiusMeters = MINIMUM_STAGING_COMPLETION_RADIUS_METERS;
		AIWaypoint waypoint = SpawnWaypoint(MOVE_WAYPOINT_PREFAB, stagingPosition);
		if (waypoint)
		{
			waypoint.SetCompletionRadius(completionRadiusMeters);
			// The stock Move prefab may complete when the first member arrives.
			// Staging is an all-living-members contract, so the engine waypoint and
			// the explicit spawn proof must use the same completion semantics.
			waypoint.SetCompletionType(EAIWaypointCompletionType.All);
		}
		return waypoint;
	}

	AIWaypoint CreateMoveWaypoint(
		vector fromPosition,
		vector targetPosition,
		float targetRangeMeters,
		out vector resolvedPosition,
		out string routeMode)
	{
		resolvedPosition = targetPosition;
		routeMode = "DIRECT_NO_AI_WORLD";
		if (targetRangeMeters < MOVE_COMPLETION_RADIUS_METERS)
			targetRangeMeters = MOVE_COMPLETION_RADIUS_METERS;

		SCR_AIWorld aiWorld = SCR_AIWorld.Cast(GetGame().GetAIWorld());
		if (aiWorld)
		{
			RoadNetworkManager roadNetworkManager = aiWorld.GetRoadNetworkManager();
			if (roadNetworkManager)
			{
				vector roadPosition;
				if (roadNetworkManager.GetReachableWaypointInRoad(fromPosition, targetPosition, targetRangeMeters, roadPosition))
				{
					resolvedPosition = roadPosition;
					routeMode = "ROAD_REACHABLE";
				}
				else
				{
					routeMode = "DIRECT_NO_REACHABLE_ROAD";
				}
			}
			else
			{
				routeMode = "DIRECT_NO_ROAD_NETWORK";
			}
		}

		AIWaypoint waypoint = SpawnWaypoint(MOVE_WAYPOINT_PREFAB, resolvedPosition);
		if (waypoint)
		{
			float completionRadius = targetRangeMeters - vector.Distance(resolvedPosition, targetPosition);
			if (completionRadius < 1.0)
				completionRadius = 1.0;
			waypoint.SetCompletionRadius(completionRadius);
		}
		return waypoint;
	}

	SCR_BoardingWaypoint CreateDismountWaypoint(Vehicle vehicle)
	{
		if (!vehicle)
			return null;

		SCR_BoardingWaypoint waypoint = SCR_BoardingWaypoint.Cast(SpawnWaypoint(GET_OUT_WAYPOINT_PREFAB, vehicle.GetOrigin()));
		if (!waypoint)
			return null;

		waypoint.SetAllowance(true, true, true);
		waypoint.SetCompletionRadius(DISMOUNT_COMPLETION_RADIUS_METERS);
		return waypoint;
	}

	protected AIWaypoint SpawnWaypoint(ResourceName prefab, vector position)
	{
		Resource resource = Resource.Load(prefab);
		if (!resource || !resource.IsValid())
		{
			AICF_Stage3Diagnostics.Error("VEHICLE_WAYPOINT_PREFAB_INVALID", string.Format("prefab=%1", prefab));
			return null;
		}

		position[1] = GetGame().GetWorld().GetSurfaceY(position[0], position[2]);
		EntitySpawnParams params = new EntitySpawnParams();
		params.TransformMode = ETransformMode.WORLD;
		params.Transform[3] = position;
		AIWaypoint waypoint = AIWaypoint.Cast(GetGame().SpawnEntityPrefabEx(prefab, false, params: params));
		if (!waypoint)
			AICF_Stage3Diagnostics.Error("VEHICLE_WAYPOINT_SPAWN_FAILED", string.Format("prefab=%1", prefab));
		return waypoint;
	}
}
