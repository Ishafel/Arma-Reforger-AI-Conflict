// The resource is the stock 1.7.0.54 Move waypoint, verified in Bohemia's Scenario Framework example.
class AICF_WaypointFactory
{
	protected static const ResourceName MOVE_WAYPOINT_PREFAB = "{750A8D1695BD6998}Prefabs/AI/Waypoints/AIWaypoint_Move.et";
	protected static const float COMPLETION_RADIUS_METERS = 5.0;

	AIWaypoint CreateAndAssign(SCR_AIGroup group, SCR_CampaignMilitaryBaseComponent target)
	{
		if (!Replication.IsServer())
		{
			AICF_Diagnostics.Error("WAYPOINT_CLIENT_CREATE_BLOCKED", "Waypoints may only be created by the server");
			return null;
		}

		if (!group || !target || !target.GetOwner())
		{
			AICF_Diagnostics.Error("WAYPOINT_INPUT_INVALID", "Group, target, or target owner is null");
			return null;
		}

		SCR_SpawnPoint targetSpawnPoint = target.GetSpawnPoint();
		if (!targetSpawnPoint)
		{
			AICF_Diagnostics.Error(
				"WAYPOINT_TARGET_POSITION_MISSING",
				string.Format("Target has no SCR_SpawnPoint: %1", AICF_Diagnostics.DescribeBase(target)));
			return null;
		}

		Resource waypointResource = Resource.Load(MOVE_WAYPOINT_PREFAB);
		if (!waypointResource || !waypointResource.IsValid())
		{
			AICF_Diagnostics.Error(
				"WAYPOINT_PREFAB_INVALID",
				string.Format("Stock Move waypoint resource is unavailable: %1", MOVE_WAYPOINT_PREFAB));
			return null;
		}

		EntitySpawnParams spawnParams = new EntitySpawnParams();
		spawnParams.TransformMode = ETransformMode.WORLD;
		vector targetPosition;
		vector targetRotation;
		targetSpawnPoint.GetPositionAndRotation(targetPosition, targetRotation);
		spawnParams.Transform[3] = targetPosition;

		IEntity spawnedEntity = GetGame().SpawnEntityPrefabEx(
			MOVE_WAYPOINT_PREFAB,
			false,
			params: spawnParams);
		AIWaypoint waypoint = AIWaypoint.Cast(spawnedEntity);
		if (!waypoint)
		{
			if (spawnedEntity)
				RplComponent.DeleteRplEntity(spawnedEntity, false);

			AICF_Diagnostics.Error("WAYPOINT_SPAWN_FAILED", "SpawnEntityPrefabEx did not return an AIWaypoint");
			return null;
		}

		waypoint.SetCompletionRadius(COMPLETION_RADIUS_METERS);
		// Defender prefabs may already contain orders. Index zero makes the Stage 0 target authoritative.
		group.AddWaypointAt(waypoint, 0);

		Faction groupFaction = group.GetFaction();
		string factionKey = "NONE";
		if (groupFaction)
			factionKey = groupFaction.GetFactionKey();

		AICF_Diagnostics.Info(
			"WAYPOINT_ASSIGNED",
			string.Format("faction=%1 target={%2} queue_index=0 completion_radius=%3",
				factionKey,
				AICF_Diagnostics.DescribeBase(target),
				COMPLETION_RADIUS_METERS));
		return waypoint;
	}
}
