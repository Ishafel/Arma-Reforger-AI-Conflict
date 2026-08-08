// Owns only AICF-created waypoints and replaces them without touching prefab orders.
class AICF_OrderPlanner
{
	protected static const ResourceName ATTACK_WAYPOINT_PREFAB = "{B3E7B8DC2BAB8ACC}Prefabs/AI/Waypoints/AIWaypoint_SearchAndDestroy.et";
	protected static const ResourceName DEFEND_WAYPOINT_PREFAB = "{93291E72AC23930F}Prefabs/AI/Waypoints/AIWaypoint_Defend.et";
	protected static const ResourceName RELAY_WAYPOINT_PREFAB = "{EAAE93F98ED5D218}Prefabs/AI/Waypoints/AIWaypoint_CaptureRelay.et";
	protected static const string RELAY_SMART_ACTION_TAG = "CapturePoint";
	protected static const float ATTACK_RADIUS_METERS = 20.0;
	protected static const float DEFEND_RADIUS_METERS = 50.0;
	protected static const float RELAY_RADIUS_METERS = 20.0;

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
			if (slot.MarkTargetUnavailableReported())
			{
				AICF_Stage1Diagnostics.Warning(
					"ORDER_TARGET_UNAVAILABLE",
					string.Format(
						"faction=%1 slot=%2 role=%3 reason=%4",
						faction.GetFactionKey(),
						slot.GetSlotId(),
						AICF_Stage1Diagnostics.RoleToString(slot.GetRole()),
						reason));
			}
			return false;
		}

		return ReplaceOrder(slot, faction, target, reason);
	}

	bool IsOrderValid(AICF_GroupSlot slot, SCR_CampaignFaction faction)
	{
		return GetOrderFailureReason(slot, faction).IsEmpty();
	}

	string GetOrderFailureReason(AICF_GroupSlot slot, SCR_CampaignFaction faction)
	{
		if (!slot || !faction)
			return "INPUT_INVALID";
		if (!slot.IsCombatReady() || !slot.GetGroup())
			return "GROUP_NOT_READY";
		if (!slot.GetWaypoint())
			return "WAYPOINT_REFERENCE_MISSING";
		if (slot.GetGroup().GetCurrentWaypoint() != slot.GetWaypoint())
			return "WAYPOINT_NOT_CURRENT";
		if (!IsTargetValidForRole(slot, faction, slot.GetTargetBase()))
			return "TARGET_INVALID";
		if (slot.GetRole() == AICF_EGroupRole.ATTACK &&
			slot.GetTargetBase().GetType() == SCR_ECampaignBaseType.RELAY &&
			!SCR_SmartActionWaypoint.Cast(slot.GetWaypoint()))
		{
			return "WAYPOINT_TYPE_INVALID";
		}

		return string.Empty;
	}

	bool RecoverOrder(
		AICF_GroupSlot slot,
		SCR_CampaignFaction faction,
		AICF_ObjectiveGraph graph,
		AICF_TargetSelector targetSelector,
		string failureReason)
	{
		if (!slot || !faction || !graph || !targetSelector || !slot.IsCombatReady())
			return false;

		SCR_CampaignMilitaryBaseComponent oldTarget = slot.GetTargetBase();
		bool recovered;
		if (IsTargetValidForRole(slot, faction, oldTarget))
			recovered = ReplaceOrder(slot, faction, oldTarget, "ORDER_RECOVERY");
		else
			recovered = AssignOrder(slot, faction, graph, targetSelector, "ORDER_RECOVERY", oldTarget);

		if (recovered)
		{
			AICF_Stage2Diagnostics.Info(
				"ORDER_RECOVERED",
				string.Format(
					"faction=%1 slot=%2 role=%3 cause=%4 target=%5",
					faction.GetFactionKey(),
					slot.GetSlotId(),
					AICF_Stage1Diagnostics.RoleToString(slot.GetRole()),
					failureReason,
					AICF_Stage1Diagnostics.BaseKey(slot.GetTargetBase())));
		}
		else
		{
			AICF_Stage2Diagnostics.Warning(
				"ORDER_RECOVERY_DEFERRED",
				string.Format(
					"faction=%1 slot=%2 cause=%3",
					faction.GetFactionKey(),
					slot.GetSlotId(),
					failureReason));
		}

		return recovered;
	}

	bool RebuildCurrentOrder(
		AICF_GroupSlot slot,
		SCR_CampaignFaction faction,
		string reason)
	{
		if (!slot || !faction || !slot.IsCombatReady() ||
			!IsTargetValidForRole(slot, faction, slot.GetTargetBase()))
		{
			return false;
		}

		return ReplaceOrder(slot, faction, slot.GetTargetBase(), reason);
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
		slot.ResetTargetUnavailableReport();

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

	protected bool IsTargetValidForRole(
		AICF_GroupSlot slot,
		SCR_CampaignFaction faction,
		SCR_CampaignMilitaryBaseComponent target)
	{
		if (!slot || !faction || !target || !target.GetOwner() || !target.IsInitialized())
			return false;

		if (slot.GetRole() == AICF_EGroupRole.ATTACK)
		{
			if (target.GetFaction() == faction || !target.IsValidTarget(faction))
				return false;

			if (target.GetType() == SCR_ECampaignBaseType.RELAY)
				return true;

			return !target.IsHQ();
		}

		return target == faction.GetMainBase() && target.GetFaction() == faction;
	}

	protected AIWaypoint CreateWaypoint(
		SCR_CampaignMilitaryBaseComponent target,
		AICF_EGroupRole role)
	{
		if (!target || !target.GetOwner())
			return null;

		ResourceName waypointPrefab = DEFEND_WAYPOINT_PREFAB;
		float completionRadius = DEFEND_RADIUS_METERS;
		bool isRelay = role == AICF_EGroupRole.ATTACK && target.GetType() == SCR_ECampaignBaseType.RELAY;
		if (isRelay)
		{
			waypointPrefab = RELAY_WAYPOINT_PREFAB;
			completionRadius = RELAY_RADIUS_METERS;
		}
		else if (role == AICF_EGroupRole.ATTACK)
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
		if (isRelay)
		{
			targetPosition = target.GetOwner().GetOrigin();
		}
		else if (role == AICF_EGroupRole.ATTACK)
		{
			array<SCR_SeizingComponent> capturePoints = {};
			target.GetCapturePoints(capturePoints);
			if (!capturePoints.IsEmpty() && capturePoints[0] && capturePoints[0].GetOwner())
				targetPosition = capturePoints[0].GetOwner().GetOrigin();
			else
				targetPosition = target.GetOwner().GetOrigin();
		}
		else
		{
			SCR_SpawnPoint spawnPoint = target.GetSpawnPoint();
			if (!spawnPoint)
				return null;

			spawnPoint.GetPositionAndRotation(targetPosition, targetRotation);
		}

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

		if (isRelay)
		{
			SCR_SmartActionWaypoint relayWaypoint = SCR_SmartActionWaypoint.Cast(waypoint);
			if (!relayWaypoint)
			{
				RplComponent.DeleteRplEntity(waypoint, false);
				AICF_Stage1Diagnostics.Error("RELAY_WAYPOINT_INVALID", RELAY_WAYPOINT_PREFAB);
				return null;
			}

			relayWaypoint.SetSmartActionEntity(target.GetOwner(), RELAY_SMART_ACTION_TAG);
			waypoint.SetCompletionType(EAIWaypointCompletionType.Any);
		}

		waypoint.SetCompletionRadius(completionRadius);
		return waypoint;
	}
}
