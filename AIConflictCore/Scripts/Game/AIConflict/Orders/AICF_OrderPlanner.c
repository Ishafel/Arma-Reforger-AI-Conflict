// Owns only AICF-created waypoints and replaces them without touching prefab orders.
class AICF_OrderPlanner
{
	protected static const ResourceName ATTACK_WAYPOINT_PREFAB = "{B3E7B8DC2BAB8ACC}Prefabs/AI/Waypoints/AIWaypoint_SearchAndDestroy.et";
	protected static const ResourceName DEFEND_WAYPOINT_PREFAB = "{93291E72AC23930F}Prefabs/AI/Waypoints/AIWaypoint_Defend.et";
	protected static const float ATTACK_RADIUS_METERS = 75.0;
	protected static const float DEFEND_RADIUS_METERS = 50.0;

	bool AssignOrder(
		AICF_GroupSlot slot,
		SCR_CampaignFaction faction,
		AICF_ObjectiveGraph graph,
		AICF_TargetSelector targetSelector,
		string reason,
		SCR_CampaignMilitaryBaseComponent excludedTarget = null)
	{
		if (!Replication.IsServer() || !slot || !faction || !graph || !targetSelector)
			return false;

		SCR_CampaignMilitaryBaseComponent target;
		if (slot.GetRole() == AICF_EGroupRole.ATTACK)
			target = targetSelector.SelectAttackTarget(graph, faction, excludedTarget);
		else
			target = targetSelector.SelectDefendTarget(faction);

		if (!target)
		{
			AICF_Stage1Diagnostics.Warning(
				"ORDER_TARGET_UNAVAILABLE",
				string.Format(
					"faction=%1 slot=%2 role=%3 reason=%4",
					faction.GetFactionKey(),
					slot.GetSlotId(),
					AICF_Stage1Diagnostics.RoleToString(slot.GetRole()),
					reason));
			return false;
		}

		return ReplaceOrder(slot, faction, target, reason);
	}

	bool IsOrderValid(AICF_GroupSlot slot, SCR_CampaignFaction faction)
	{
		if (!slot || !faction || !slot.IsCombatReady() || !slot.GetWaypoint())
			return false;

		if (slot.GetGroup().GetCurrentWaypoint() != slot.GetWaypoint())
			return false;

		SCR_CampaignMilitaryBaseComponent target = slot.GetTargetBase();
		if (!target || !target.GetOwner() || !target.IsInitialized())
			return false;

		if (slot.GetRole() == AICF_EGroupRole.ATTACK)
			return target.GetFaction() != faction && target.IsValidTarget(faction);

		return target == faction.GetMainBase() && target.GetFaction() == faction;
	}

	void ClearOrder(AICF_GroupSlot slot)
	{
		if (!slot)
			return;

		SCR_AIGroup group = slot.GetGroup();
		AIWaypoint waypoint = slot.GetWaypoint();
		if (waypoint)
		{
			if (group)
				group.RemoveWaypoint(waypoint);

			RplComponent.DeleteRplEntity(waypoint, false);
		}

		slot.ClearObjective();
	}

	protected bool ReplaceOrder(
		AICF_GroupSlot slot,
		SCR_CampaignFaction faction,
		SCR_CampaignMilitaryBaseComponent target,
		string reason)
	{
		SCR_AIGroup group = slot.GetGroup();
		if (!group || !slot.IsCombatReady())
			return false;

		AIWaypoint newWaypoint = CreateWaypoint(target, slot.GetRole());
		if (!newWaypoint)
			return false;

		AIWaypoint oldWaypoint = slot.GetWaypoint();
		SCR_CampaignMilitaryBaseComponent oldTarget = slot.GetTargetBase();
		if (oldWaypoint)
		{
			group.RemoveWaypoint(oldWaypoint);
			RplComponent.DeleteRplEntity(oldWaypoint, false);
		}

		group.AddWaypointAt(newWaypoint, 0);
		slot.ClearObjective();
		if (!slot.AssignObjective(target, newWaypoint))
		{
			group.RemoveWaypoint(newWaypoint);
			RplComponent.DeleteRplEntity(newWaypoint, false);
			return false;
		}

		if (oldTarget && oldTarget != target)
		{
			AICF_Stage1Diagnostics.Info(
				"TARGET_REASSIGNED",
				string.Format(
					"faction=%1 slot=%2 role=%3 old_target=%4 new_target=%5 reason=%6",
					faction.GetFactionKey(),
					slot.GetSlotId(),
					AICF_Stage1Diagnostics.RoleToString(slot.GetRole()),
					AICF_Stage1Diagnostics.BaseKey(oldTarget),
					AICF_Stage1Diagnostics.BaseKey(target),
					reason));
		}
		else
		{
			AICF_Stage1Diagnostics.Info(
				"ORDER_ASSIGNED",
				string.Format(
					"faction=%1 slot=%2 role=%3 target=%4 reason=%5",
					faction.GetFactionKey(),
					slot.GetSlotId(),
					AICF_Stage1Diagnostics.RoleToString(slot.GetRole()),
					AICF_Stage1Diagnostics.BaseKey(target),
					reason));
		}
		return true;
	}

	protected AIWaypoint CreateWaypoint(
		SCR_CampaignMilitaryBaseComponent target,
		AICF_EGroupRole role)
	{
		if (!target || !target.GetOwner())
			return null;

		SCR_SpawnPoint spawnPoint = target.GetSpawnPoint();
		if (!spawnPoint)
			return null;

		ResourceName waypointPrefab = DEFEND_WAYPOINT_PREFAB;
		float completionRadius = DEFEND_RADIUS_METERS;
		if (role == AICF_EGroupRole.ATTACK)
		{
			waypointPrefab = ATTACK_WAYPOINT_PREFAB;
			completionRadius = ATTACK_RADIUS_METERS;
		}

		Resource waypointResource = Resource.Load(waypointPrefab);
		if (!waypointResource || !waypointResource.IsValid())
		{
			AICF_Stage1Diagnostics.Error("WAYPOINT_PREFAB_INVALID", waypointPrefab);
			return null;
		}

		vector targetPosition;
		vector targetRotation;
		spawnPoint.GetPositionAndRotation(targetPosition, targetRotation);

		EntitySpawnParams spawnParams = new EntitySpawnParams();
		spawnParams.TransformMode = ETransformMode.WORLD;
		spawnParams.Transform[3] = targetPosition;

		IEntity spawnedEntity = GetGame().SpawnEntityPrefabEx(waypointPrefab, false, params: spawnParams);
		AIWaypoint waypoint = AIWaypoint.Cast(spawnedEntity);
		if (!waypoint)
		{
			if (spawnedEntity)
				RplComponent.DeleteRplEntity(spawnedEntity, false);
			return null;
		}

		waypoint.SetCompletionRadius(completionRadius);
		return waypoint;
	}
}
