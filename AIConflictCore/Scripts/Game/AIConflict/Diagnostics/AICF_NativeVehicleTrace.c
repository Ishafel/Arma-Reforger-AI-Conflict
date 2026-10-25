// Наблюдаем native выбор actions, сохраняя исходные callbacks ровно один раз.
class AICF_NativeVehicleTrace
{
	static void Record(string kind, AIActionBase action, SCR_AIUtilityComponent utility, IEntity vehicle, BaseCompartmentSlot seat = null)
	{
		if (!Replication.IsServer() || !action || !utility || !utility.m_OwnerEntity) return;
		ChimeraCharacter character = ChimeraCharacter.Cast(utility.m_OwnerEntity);
		if (!character || !character.GetAIControlComponent()) return;
		AIAgent agent = character.GetAIControlComponent().GetControlAIAgent();
		if (!agent) return;
		SCR_AIGroup group = SCR_AIGroup.Cast(agent.GetParentGroup());
		if (!group) return;
		string details = string.Format("kind=%1 member=%2 group=%3 vehicle=%4 member_pos=%5 vehicle_pos=%6 related_activity=%7 priority=%8 priority_level=%9",
			kind, AICF_GroupTrace.Entity(character), AICF_GroupTrace.Entity(group), AICF_GroupTrace.Entity(vehicle),
			AICF_GroupTrace.Position(character), AICF_GroupTrace.Position(vehicle), AICF_GroupTrace.Action(action.GetRelatedGroupActivity()),
			action.GetPriority(), action.EvaluatePriorityLevel());
		if (seat) details += " seat=" + AICF_VehiclePreset.SeatKey(seat);
		details += " action=" + AICF_GroupTrace.Action(action);
		SCR_AIGroupUtilityComponent groupUtility = group.GetGroupUtilityComponent();
		if (groupUtility) details += " group_action=" + AICF_GroupTrace.Action(groupUtility.GetCurrentAction());
		AICF_Stage3Diagnostics.Info("NATIVE_VEHICLE_ACTION_SELECTED", details);
	}
}

modded class SCR_AIGroupUtilityComponent
{
	override void OnMoveFailed(int moveResult, IEntity vehicleUsed, bool isWaypointRelated, vector moveLocation)
	{
		if (Replication.IsServer())
		{
			SCR_AIGroup group = SCR_AIGroup.Cast(GetAIAgent());
			AICF_Stage3Diagnostics.Info("NATIVE_GROUP_MOVE_FAILED", string.Format(
				"group=%1 result=%2 vehicle=%3 waypoint_related=%4 location=%5 leader_pos=%6 group_action=%7",
				AICF_GroupTrace.Entity(group), typename.EnumToString(EMoveError, moveResult), AICF_GroupTrace.Entity(vehicleUsed),
				isWaypointRelated, moveLocation, AICF_GroupTrace.Position(AICF_GroupRuntime.ResolveAliveLeader(group)), AICF_GroupTrace.Action(GetCurrentAction())));
		}
		super.OnMoveFailed(moveResult, vehicleUsed, isWaypointRelated, moveLocation);
	}
}

modded class SCR_AIUtilityComponent
{
	override void WrapBehaviorOutsideOfVehicle(SCR_AIActionBase action)
	{
		if (Replication.IsServer() && m_OwnerEntity && action)
			AICF_NativeVehicleTrace.Record("OUTSIDE_BEHAVIOR_REQUEST", action, this, CompartmentAccessComponent.GetVehicleIn(m_OwnerEntity));
		super.WrapBehaviorOutsideOfVehicle(action);
	}
}

[BaseContainerProps()]
modded class SCR_AIGoalReaction_GetOutVehicle
{
	override void PerformReaction(notnull SCR_AIUtilityComponent utility, SCR_AIMessageBase message)
	{
		SCR_AIMessage_GetOut msg = SCR_AIMessage_GetOut.Cast(message);
		if (Replication.IsServer() && msg)
			AICF_Stage3Diagnostics.Info("NATIVE_GET_OUT_MESSAGE", string.Format(
				"member=%1 vehicle=%2 related_activity=%3 related_waypoint=%4 priority_level=%5 text=[%6]",
				AICF_GroupTrace.Entity(utility.m_OwnerEntity), AICF_GroupTrace.Entity(msg.m_Vehicle),
				AICF_GroupTrace.Action(msg.m_RelatedGroupActivity), AICF_GroupTrace.Entity(msg.m_RelatedWaypoint), msg.m_fPriorityLevel, msg.GetText()));
		super.PerformReaction(utility, message);
	}
}

modded class SCR_AIGetInVehicle
{
	override void OnActionSelected()
	{
		super.OnActionSelected();
		if (Replication.IsServer()) AICF_NativeVehicleTrace.Record("GET_IN", this, m_Utility, m_Vehicle.m_Value, m_CompartmentToGetIn.m_Value);
	}
}

modded class SCR_AIGetOutVehicle
{
	override void OnActionSelected()
	{
		super.OnActionSelected();
		if (Replication.IsServer()) AICF_NativeVehicleTrace.Record("GET_OUT", this, m_Utility, m_Vehicle.m_Value);
	}
}
