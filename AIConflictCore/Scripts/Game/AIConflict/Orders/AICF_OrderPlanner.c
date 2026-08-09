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

	bool IsStrategicTargetValid(
		AICF_GroupSlot slot,
		SCR_CampaignFaction faction,
		SCR_CampaignMilitaryBaseComponent target)
	{
		return IsTargetValidForRole(slot, faction, target);
	}

	string GetOrderFailureReason(AICF_GroupSlot slot, SCR_CampaignFaction faction)
	{
		if (!slot || !faction)
			return "INPUT_INVALID";
		if (!slot.IsCombatReady() || !slot.GetGroup())
			return "GROUP_NOT_READY";
		// Strategic ownership/eligibility changes supersede waypoint lifecycle.
		// A completed waypoint on a freshly captured target is a retarget event,
		// not an unstable movement recovery that should consume stuck budget.
		if (!IsTargetValidForRole(slot, faction, slot.GetTargetBase()))
			return "TARGET_INVALID";
		if (!slot.GetWaypoint())
			return "WAYPOINT_REFERENCE_MISSING";
		if (slot.GetGroup().GetCurrentWaypoint() != slot.GetWaypoint())
			return "WAYPOINT_NOT_CURRENT";
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
		string failureReason,
		bool countsAsStuckRecovery = false)
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
			slot.BeginOrderRecoveryVerification(failureReason, countsAsStuckRecovery);
			AICF_Stage2Diagnostics.Info(
				"ORDER_RECOVERY_ISSUED",
				string.Format(
					"faction=%1 slot=%2 role=%3 cause=%4 target=%5 waypoint=%6 counts_as_stuck=%7 verification=PENDING_DURABILITY",
					faction.GetFactionKey(),
					slot.GetSlotId(),
					AICF_Stage1Diagnostics.RoleToString(slot.GetRole()),
					failureReason,
					AICF_Stage1Diagnostics.BaseKey(slot.GetTargetBase()),
					slot.GetWaypoint().GetID(),
					countsAsStuckRecovery));
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

	// Persistent movement failure is a local navigation failure, not a casualty.
	// Keep the same group, target and world position under a durable defend
	// waypoint. A later strategic graph revision may resume the objective from
	// this field position without spending tickets or spawning at a MOB.
	bool HoldPositionForPersistentStuck(
		AICF_GroupSlot slot,
		SCR_CampaignFaction faction,
		SCR_CampaignMilitaryBaseComponent target,
		vector fieldPosition)
	{
		if (!slot || !faction || !target || !slot.IsCombatReady() || !slot.GetGroup())
			return false;

		Resource waypointResource = Resource.Load(DEFEND_WAYPOINT_PREFAB);
		if (!waypointResource || !waypointResource.IsValid())
		{
			AICF_Stage1Diagnostics.Error("WAYPOINT_PREFAB_INVALID", DEFEND_WAYPOINT_PREFAB);
			return false;
		}

		EntitySpawnParams spawnParams = new EntitySpawnParams();
		spawnParams.TransformMode = ETransformMode.WORLD;
		spawnParams.Transform[3] = fieldPosition;
		IEntity spawnedEntity = GetGame().SpawnEntityPrefabEx(
			DEFEND_WAYPOINT_PREFAB,
			false,
			params: spawnParams);
		AIWaypoint fieldHold = AIWaypoint.Cast(spawnedEntity);
		if (!fieldHold)
		{
			if (spawnedEntity)
				RplComponent.DeleteRplEntity(spawnedEntity, false);
			return false;
		}
		fieldHold.SetCompletionRadius(DEFEND_RADIUS_METERS);

		SCR_AIGroup group = slot.GetGroup();
		AIWaypoint oldWaypoint = slot.GetWaypoint();
		if (oldWaypoint)
		{
			group.RemoveWaypoint(oldWaypoint);
			RplComponent.DeleteRplEntity(oldWaypoint, false);
		}

		group.AddWaypointAt(fieldHold, 0);
		slot.ClearObjective();
		if (!slot.AssignObjective(target, fieldHold))
		{
			group.RemoveWaypoint(fieldHold);
			RplComponent.DeleteRplEntity(fieldHold, false);
			return false;
		}

		slot.BeginPersistentStuckFieldHold();
		return true;
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

	void SuspendOrderForVehicle(AICF_GroupSlot slot)
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

		slot.SuspendObjectiveWaypoint();
	}

	bool TryResolveTargetPosition(
		SCR_CampaignMilitaryBaseComponent target,
		AICF_EGroupRole role,
		out vector targetPosition)
	{
		targetPosition = "0 0 0";
		if (!target || !target.GetOwner())
			return false;

		bool isRelay = role == AICF_EGroupRole.ATTACK && target.GetType() == SCR_ECampaignBaseType.RELAY;
		if (isRelay)
		{
			targetPosition = target.GetOwner().GetOrigin();
			return true;
		}

		if (role == AICF_EGroupRole.ATTACK)
		{
			array<SCR_SeizingComponent> capturePoints = {};
			target.GetCapturePoints(capturePoints);
			if (!capturePoints.IsEmpty() && capturePoints[0] && capturePoints[0].GetOwner())
				targetPosition = capturePoints[0].GetOwner().GetOrigin();
			else
				targetPosition = target.GetOwner().GetOrigin();
			return true;
		}

		SCR_SpawnPoint spawnPoint = target.GetSpawnPoint();
		if (!spawnPoint)
			return false;

		vector targetRotation;
		spawnPoint.GetPositionAndRotation(targetPosition, targetRotation);
		return true;
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
		if (!TryResolveTargetPosition(target, role, targetPosition))
			return null;

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

		// The setting is copied into the group only while this AICF waypoint is
		// current. Column reduces travel spread while stock combat behavior remains
		// free to seek cover near the objective.
		SCR_AIWaypoint scriptedWaypoint = SCR_AIWaypoint.Cast(waypoint);
		if (scriptedWaypoint)
		{
			scriptedWaypoint.AddSetting(SCR_AIGroupFormationSetting.Create(
				SCR_EAISettingOrigin.WAYPOINT,
				SCR_EAIGroupFormation.Column));
		}

		waypoint.SetCompletionRadius(completionRadius);
		return waypoint;
	}
}
