// Явное удержание достигнутой точки внутри MOB не требует выхода из MOB.
// Policy только читает текущий intent, waypoint и физическое положение состава.
class AICF_PlayerMobHoldPolicy
{
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
