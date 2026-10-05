// Planner boundary: флаг temporary hold не заменяет исполняемый waypoint.
// Repair не трогает hold/recovery timers, generation и strategic assignment.
class AICF_TemporaryHoldRepair
{
	static bool Ensure(AICF_GroupSlot slot, SCR_CampaignFaction faction, ResourceName prefab)
	{
		if (!Replication.IsServer() || !slot || !faction || !slot.IsCombatReady() ||
			!slot.IsTemporaryRouteReplanHold() || slot.IsRecruitingInfantry())
			return false;
		SCR_AIGroup group = slot.GetGroup();
		SCR_CampaignMilitaryBaseComponent target = slot.GetTargetBase();
		if (!group || !target || group.GetFaction() != faction)
			return false;
		AIWaypoint oldWaypoint = slot.GetWaypoint();
		AIWaypoint current = group.GetCurrentWaypoint();
		// Чужой текущий приказ не удаляем и не вытесняем.
		if (current && current != oldWaypoint)
			return false;
		if (current && SCR_DefendWaypoint.Cast(current) &&
			slot.GetOwnedWaypointTerminalOutcome(current) != "GROUP_CALLBACK_COMPLETED")
			return true;
		EntityID groupId = group.GetID();
		int generation = slot.GetSpawnGeneration();
		int assignment = slot.GetStrategicAssignmentRevision();
		Resource resource = Resource.Load(prefab);
		if (!resource || !resource.IsValid())
			return false;
		EntitySpawnParams params = new EntitySpawnParams();
		params.TransformMode = ETransformMode.WORLD;
		params.Transform[3] = slot.GetTemporaryRouteReplanAnchor();
		IEntity entity = GetGame().SpawnEntityPrefabEx(prefab, false, params: params);
		SCR_DefendWaypoint hold = SCR_DefendWaypoint.Cast(entity);
		if (!hold)
		{
			if (entity)
				RplComponent.DeleteRplEntity(entity, false);
			return false;
		}
		hold.SetCompletionRadius(50);
		hold.SetCompletionType(EAIWaypointCompletionType.All);
		hold.SetHoldingTime(3600);
		if (!IsCurrent(slot, group, faction, target, oldWaypoint, groupId, generation, assignment) ||
			(group.GetCurrentWaypoint() && group.GetCurrentWaypoint() != oldWaypoint))
		{
			RplComponent.DeleteRplEntity(hold, false);
			return false;
		}
		group.AddWaypointAt(hold, 0);
		// AddWaypoint может синхронно вызвать смену владельца/приказа.
		if (!IsCurrent(slot, group, faction, target, oldWaypoint, groupId, generation, assignment) ||
			group.GetCurrentWaypoint() != hold || !slot.AssignObjective(target, hold))
		{
			group.RemoveWaypoint(hold);
			RplComponent.DeleteRplEntity(hold, false);
			return false;
		}
		if (oldWaypoint)
		{
			group.RemoveWaypoint(oldWaypoint);
			RplComponent.DeleteRplEntity(oldWaypoint, false);
		}
		bool executable = IsCurrent(slot, group, faction, target, hold, groupId, generation, assignment) &&
			group.GetCurrentWaypoint() == hold;
		AICF_Stage2Diagnostics.Info("TEMPORARY_HOLD_REPAIRED", string.Format(
			"faction=%1 numeric_slot=%2 group_generation=%3 assignment_revision=%4 executable=%5 episode_age_ms=%6 movement_confirmation=NONE",
			faction.GetFactionKey(), slot.GetSlotId(), generation, assignment, executable, slot.GetRouteRecoveryEpisode().GetAgeMs()));
		return executable;
	}

	protected static bool IsCurrent(AICF_GroupSlot slot, SCR_AIGroup group, SCR_CampaignFaction faction,
		SCR_CampaignMilitaryBaseComponent target, AIWaypoint waypoint, EntityID groupId, int generation, int assignment)
	{
		return group && group.GetID() == groupId && group.GetFaction() == faction && slot.GetGroup() == group &&
			slot.IsCombatReady() && slot.IsTemporaryRouteReplanHold() && !slot.IsRecruitingInfantry() &&
			slot.GetSpawnGeneration() == generation && slot.GetStrategicAssignmentRevision() == assignment &&
			slot.GetTargetBase() == target && slot.GetWaypoint() == waypoint;
	}
}
