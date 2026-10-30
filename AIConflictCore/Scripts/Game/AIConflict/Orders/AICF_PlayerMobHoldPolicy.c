// Явное удержание достигнутой точки внутри MOB не требует выхода из MOB.
// Policy только читает текущий intent, waypoint и физическое положение состава.
class AICF_PlayerMobHoldPolicy
{
	// Текущая пешая задача внутри MOB не требует движения наружу. Это не arrival
	// и не разрешение idle: native task и прежний movement recovery продолжаются.
	static bool HasCurrentInternalTask(
		AICF_GroupSlot slot,
		SCR_CampaignMilitaryBaseComponent mainBase,
		float mobRadiusMeters)
	{
		if (!HasInternalPlayerIntent(slot, mainBase, mobRadiusMeters)) return false;
		SCR_AIGroup group = slot.GetGroup();
		SCR_DefendWaypoint waypoint = SCR_DefendWaypoint.Cast(slot.GetWaypoint());
		if (!group || !waypoint || group.GetCurrentWaypoint() != waypoint) return false;
		SCR_AIGroupUtilityComponent utility = group.GetGroupUtilityComponent();
		if (!utility) return false;
		SCR_AIActivityBase activity = SCR_AIActivityBase.Cast(utility.GetCurrentAction());
		if (!IsRunningInternalActivity(utility, activity, waypoint)) return false;

		int generation = slot.GetSpawnGeneration();
		int assignment = slot.GetStrategicAssignmentRevision();
		int intent = slot.GetPlayerStrategicIntentRevision();
		EntityID groupId = group.GetID();
		IEntity mobOwner = mainBase.GetOwner();
		if (!mobOwner) return false;
		vector mobOrigin = mobOwner.GetOrigin();
		vector targetPosition = slot.GetTargetPosition();
		array<AIWaypoint> waypoints = {};
		group.GetWaypoints(waypoints);
		if (!waypoints.Contains(waypoint) || !IsFootGroup(group)) return false;
		if (!group) return false;
		waypoints.Clear();
		group.GetWaypoints(waypoints);
		if (!waypoints.Contains(waypoint)) return false;
		if (vector.DistanceSqXZ(targetPosition, mobOrigin) > mobRadiusMeters * mobRadiusMeters ||
			vector.DistanceSqXZ(waypoint.GetOrigin(), targetPosition) > 1.0 ||
			vector.DistanceSqXZ(waypoint.GetOrigin(), mobOrigin) > mobRadiusMeters * mobRadiusMeters) return false;

		// Native getters выше не дают разрешение для сменившегося приказа/группы.
		if (!group || group.GetID() != groupId || group.GetGroupUtilityComponent() != utility ||
			group.GetCurrentWaypoint() != waypoint || !IsRunningInternalActivity(utility, activity, waypoint) ||
			!mainBase || mainBase.GetOwner() != mobOwner || !mobOwner || mobOwner.GetOrigin() != mobOrigin ||
			!waypoint || vector.DistanceSqXZ(waypoint.GetOrigin(), targetPosition) > 1.0) return false;
		return HasInternalPlayerIntent(slot, mainBase, mobRadiusMeters) && slot.GetGroup() == group &&
			slot.GetWaypoint() == waypoint && slot.GetSpawnGeneration() == generation &&
			slot.GetStrategicAssignmentRevision() == assignment && slot.GetPlayerStrategicIntentRevision() == intent &&
			slot.GetTargetPosition() == targetPosition;
	}

	protected static bool HasInternalPlayerIntent(AICF_GroupSlot slot, SCR_CampaignMilitaryBaseComponent mainBase, float mobRadiusMeters)
	{
		return Replication.IsServer() && slot && slot.IsCombatReady() && mainBase && mainBase.GetOwner() && mobRadiusMeters > 0 &&
			slot.GetDecisionAuthority() == AICF_EStrategicDecisionAuthority.PLAYER_COMMAND && slot.HasPlayerStrategicIntent() &&
			slot.GetPlayerStrategicIntentTargetKind() == AICF_EOrderTargetKind.POSITION && slot.GetTargetKind() == AICF_EOrderTargetKind.POSITION &&
			slot.GetStrategicIntentPosture() == "MOVE_AND_HOLD" && slot.GetOperationalPosture() == "MOVE_AND_HOLD" &&
			slot.GetTargetPosition() == slot.GetPlayerStrategicIntentTargetPosition() &&
			!slot.HasPendingOrderRecovery() && !slot.HasFailedMovement() && !slot.IsApproachRouteWaypoint();
	}

	protected static bool IsRunningInternalActivity(SCR_AIGroupUtilityComponent utility, SCR_AIActivityBase activity, AIWaypoint waypoint)
	{
		if (!utility || !activity || !waypoint ||
			(activity.Type() != SCR_AIMoveActivity && activity.Type() != SCR_AIDefendActivity)) return false;
		return activity.GetActionState() == EAIActionState.RUNNING && !activity.GetActionIsSuspended() && !activity.GetRemoveAction() &&
			activity.m_RelatedWaypoint == waypoint && utility.GetCurrentAction() == activity && utility.GetExecutedAction() == activity;
	}

	protected static bool IsFootGroup(SCR_AIGroup group)
	{
		if (!group) return false;
		array<AIAgent> agents = {};
		group.GetAgents(agents);
		if (!group || agents.IsEmpty() || agents.Count() > AICF_Stage1Config.MAX_GROUP_SIZE) return false;
		int alive;
		foreach (AIAgent agent : agents)
		{
			if (!agent) return false;
			ChimeraCharacter member = ChimeraCharacter.Cast(agent.GetControlledEntity());
			if (!member || !member.GetCharacterController()) return false;
			if (!AICF_GroupRuntime.IsAliveCharacter(member)) continue;
			AIControlComponent control = member.GetAIControlComponent();
			CompartmentAccessComponent access = member.GetCompartmentAccessComponent();
			if (!member || !agent || !control || control.GetControlAIAgent() != agent || agent.GetParentGroup() != group || !access ||
				!SCR_AIDamageHandling.IsConscious(member) || member.IsInVehicle() || access.IsInCompartment() ||
				access.IsGettingIn() || access.IsGettingOut() || access.IsSwitchingSeatsAnim() ||
				CompartmentAccessComponent.GetVehicleIn(member)) return false;
			alive++;
		}
		return alive > 0;
	}

	static bool IsReachedCurrentHold(
		AICF_GroupSlot slot,
		SCR_CampaignMilitaryBaseComponent mainBase,
		float mobRadiusMeters)
	{
		if (!Replication.IsServer() || !slot || !slot.IsCombatReady() ||
			!mainBase || !mainBase.GetOwner() || mobRadiusMeters <= 0 ||
			slot.GetDecisionAuthority() != AICF_EStrategicDecisionAuthority.PLAYER_COMMAND ||
			!slot.HasPlayerStrategicIntent() ||
			slot.GetPlayerStrategicIntentTargetKind() != AICF_EOrderTargetKind.POSITION ||
			slot.GetTargetKind() != AICF_EOrderTargetKind.POSITION ||
			slot.GetStrategicIntentPosture() != "MOVE_AND_HOLD" ||
			slot.GetOperationalPosture() != "MOVE_AND_HOLD" ||
			slot.GetTargetPosition() != slot.GetPlayerStrategicIntentTargetPosition() ||
			slot.HasPendingOrderRecovery() || slot.HasFailedMovement() ||
			slot.IsApproachRouteWaypoint())
		{
			return false;
		}

		SCR_AIGroup group = slot.GetGroup();
		SCR_DefendWaypoint waypoint = SCR_DefendWaypoint.Cast(slot.GetWaypoint());
		if (!group || !waypoint || group.GetCurrentWaypoint() != waypoint)
			return false;
		array<AIWaypoint> waypoints = {};
		group.GetWaypoints(waypoints);
		if (!waypoints.Contains(waypoint))
			return false;

		vector targetPosition = slot.GetTargetPosition();
		vector mobOrigin = mainBase.GetOwner().GetOrigin();
		if (vector.DistanceSqXZ(targetPosition, mobOrigin) > mobRadiusMeters * mobRadiusMeters ||
			vector.DistanceSqXZ(waypoint.GetOrigin(), targetPosition) > 1.0 ||
			vector.DistanceSqXZ(waypoint.GetOrigin(), mobOrigin) > mobRadiusMeters * mobRadiusMeters)
		{
			return false;
		}

		return AICF_DefendArrivalHandoff.IsFormationAtWaypoint(group, waypoint);
	}
}
