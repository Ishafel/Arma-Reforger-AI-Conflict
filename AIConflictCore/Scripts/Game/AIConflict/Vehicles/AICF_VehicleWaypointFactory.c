// Owns only transient Stage 3 waypoints. Strategic infantry waypoints remain
// owned by AICF_OrderPlanner and are restored after dismount or fallback.
class AICF_VehicleWaypointFactory
{
	protected static const ResourceName GET_IN_WAYPOINT_PREFAB = "{712F4795CF8B91C7}Prefabs/AI/Waypoints/AIWaypoint_GetIn.et";
	protected static const ResourceName MOVE_WAYPOINT_PREFAB = "{750A8D1695BD6998}Prefabs/AI/Waypoints/AIWaypoint_Move.et";
	protected static const ResourceName GET_OUT_WAYPOINT_PREFAB = "{C40316EE26846CAB}Prefabs/AI/Waypoints/AIWaypoint_GetOut.et";
	protected static const float BOARDING_COMPLETION_RADIUS_METERS = 12.0;
	protected static const float MOVE_COMPLETION_RADIUS_METERS = 25.0;

	SCR_BoardingEntityWaypoint CreateDriverBoardingWaypoint(Vehicle vehicle)
	{
		return CreateRoleBoardingWaypoint(vehicle, true, false, false);
	}

	SCR_BoardingEntityWaypoint CreatePassengerBoardingWaypoint(Vehicle vehicle)
	{
		return CreateRoleBoardingWaypoint(vehicle, false, false, true);
	}

	SCR_BoardingEntityWaypoint CreateGunnerBoardingWaypoint(Vehicle vehicle)
	{
		return CreateRoleBoardingWaypoint(vehicle, false, true, false);
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
		waypoint.SetCompletionRadius(BOARDING_COMPLETION_RADIUS_METERS);
		return waypoint;
	}

	void DeleteOwnedWaypoint(SCR_AIGroup group, AIWaypoint waypoint)
	{
		if (!waypoint)
			return;

		if (group)
			group.RemoveWaypoint(waypoint);
		RplComponent.DeleteRplEntity(waypoint, false);
	}

	protected SCR_BoardingEntityWaypoint CreateRoleBoardingWaypoint(
		Vehicle vehicle,
		bool allowDriver,
		bool allowGunner,
		bool allowCargo)
	{
		if (!vehicle)
			return null;

		SCR_BoardingEntityWaypoint waypoint = SCR_BoardingEntityWaypoint.Cast(SpawnWaypoint(GET_IN_WAYPOINT_PREFAB, vehicle.GetOrigin()));
		if (!waypoint)
			return null;

		waypoint.SetEntity(vehicle);
		waypoint.SetAllowance(allowDriver, allowGunner, allowCargo);
		waypoint.SetCompletionRadius(BOARDING_COMPLETION_RADIUS_METERS);
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
