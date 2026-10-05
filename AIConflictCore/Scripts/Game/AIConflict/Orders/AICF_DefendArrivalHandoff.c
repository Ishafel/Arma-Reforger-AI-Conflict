// Завершение движения не завершает длительное удержание Defend waypoint.
// Вызывается planner boundary только для пехоты, физически прибывшей всей группой.
class AICF_DefendArrivalHandoff
{
	static bool TryHandle(AICF_GroupSlot slot, SCR_AIGroup group, SCR_AIGroupUtilityComponent utility, int result, vector location)
	{
		if (!Replication.IsServer() || !slot || !slot.IsCombatReady() || slot.GetGroup() != group || !group ||
			slot.GetUnitType() != AICF_EGroupUnitType.INFANTRY || slot.HasFailedMovement())
			return false;
		SCR_DefendWaypoint waypoint = SCR_DefendWaypoint.Cast(slot.GetWaypoint());
		if (!waypoint || group.GetCurrentWaypoint() != waypoint || !utility || group.GetGroupUtilityComponent() != utility ||
			!IsFormationAtWaypoint(group, waypoint))
			return false;
		SCR_AIMoveActivity move = SCR_AIMoveActivity.Cast(utility.GetExecutedAction());
		if (!move || utility.GetCurrentAction() != move || move.m_RelatedWaypoint != waypoint)
			return false;
		int generation = slot.GetSpawnGeneration();
		int assignment = slot.GetStrategicAssignmentRevision();
		EntityID groupId = group.GetID();
		Faction faction = group.GetFaction();
		utility.OnMoveFailed(result, null, true, location);
		// OnMoveFailed invoker может синхронно сменить владельца или приказ.
		if (!IsCurrent(slot, group, waypoint, generation, assignment, groupId, faction) ||
			utility.GetExecutedAction() != move || utility.GetCurrentAction() != move)
			return true;
		move.Fail();
		if (!IsCurrent(slot, group, waypoint, generation, assignment, groupId, faction))
			return true;
		// Сохраняем существующую Defend activity, включая её исходный holding timer.
		array<ref AIActionBase> actions = {};
		utility.GetActions(actions);
		foreach (AIActionBase action : actions)
		{
			SCR_AIDefendActivity defend = SCR_AIDefendActivity.Cast(action);
			if (defend && defend.m_RelatedWaypoint == waypoint &&
				defend.GetActionState() != EAIActionState.FAILED && defend.GetActionState() != EAIActionState.COMPLETED)
			{
				Log(slot, group, waypoint, result, "EXISTING_ACTIVITY");
				return true;
			}
		}
		SCR_AIDefendActivity activity = new SCR_AIDefendActivity(utility, waypoint, vector.Zero);
		utility.AddAction(activity);
		Log(slot, group, waypoint, result, "ACTIVITY_SUBMITTED");
		return true;
	}

	static bool IsFormationAtWaypoint(SCR_AIGroup group, AIWaypoint waypoint)
	{
		if (!group || !waypoint || !AICF_GroupRuntime.ResolveAliveLeader(group))
			return false;
		float radius = waypoint.GetCompletionRadius();
		if (radius <= 0)
			return false;
		array<AIAgent> agents = {};
		group.GetAgents(agents);
		int alive;
		foreach (AIAgent agent : agents)
		{
			IEntity member;
			if (agent)
				member = agent.GetControlledEntity();
			if (!AICF_GroupRuntime.IsAliveCharacter(member))
				continue;
			CharacterControllerComponent controller = CharacterControllerComponent.Cast(member.FindComponent(CharacterControllerComponent));
			if (!controller || controller.IsPlayerControlled() || agent.GetParentGroup() != group)
				return false;
			CompartmentAccessComponent access = CompartmentAccessComponent.Cast(member.FindComponent(CompartmentAccessComponent));
			if (!access || access.IsInCompartment() || access.IsGettingIn() || access.IsGettingOut() ||
				vector.DistanceSqXZ(member.GetOrigin(), waypoint.GetOrigin()) > radius * radius)
				return false;
			alive++;
		}
		return alive > 0;
	}

	protected static bool IsCurrent(AICF_GroupSlot slot, SCR_AIGroup group, AIWaypoint waypoint, int generation, int assignment, EntityID groupId, Faction faction)
	{
		if (!group || !waypoint || slot.GetGroup() != group || group.GetID() != groupId || group.GetFaction() != faction)
			return false;
		return slot.GetSpawnGeneration() == generation && slot.GetStrategicAssignmentRevision() == assignment &&
			slot.GetWaypoint() == waypoint && group.GetCurrentWaypoint() == waypoint && slot.IsCombatReady() &&
			IsFormationAtWaypoint(group, waypoint);
	}

	protected static void Log(AICF_GroupSlot slot, SCR_AIGroup group, AIWaypoint waypoint, int result, string action)
	{
		AICF_Stage35Diagnostics.Info("DEFEND_ARRIVAL_HANDOFF", string.Format(
			"group=%1 numeric_slot=%2 group_generation=%3 assignment_revision=%4 waypoint=%5 move_result=%6 action=%7 recovery_confirmed=0",
			group.GetID(), slot.GetSlotId(), slot.GetSpawnGeneration(), slot.GetStrategicAssignmentRevision(), waypoint.GetID(), result, action));
	}
}
