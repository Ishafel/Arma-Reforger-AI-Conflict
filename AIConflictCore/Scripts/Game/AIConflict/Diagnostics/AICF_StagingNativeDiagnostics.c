// Read-only native staging receipts. Called only by the existing issued / 5s
// progress / terminal audit cadence; it never grants progress or recovery.
class AICF_StagingNativeDiagnostics
{
	static const int PATH_LOG_LIMIT = 16;
	static const int PATH_ARC_LIMIT = 128;
	protected static bool Finite(vector point)
	{
		return Math.AbsFloat(point[0]) < float.MAX && Math.AbsFloat(point[1]) < float.MAX && Math.AbsFloat(point[2]) < float.MAX;
	}
	protected static string Action(AIActionBase action)
	{
		if (!action) return "NONE";
		return string.Format("%1:%2:priority_%3:suspended_%4:remove_%5", action.ToString(),
			typename.EnumToString(EAIActionState, action.GetActionState()), action.GetPriority(), action.GetActionIsSuspended(), action.GetRemoveAction());
	}
	protected static string MoveGoal(string prefix, AIActionBase action, vector tokenGoal)
	{
		SCR_AIMoveBehaviorBase move = SCR_AIMoveBehaviorBase.Cast(action);
		if (!move || !move.m_vPosition) return " " + prefix + "_move_goal_available=0";
		IEntity entity;
		float radius = -1;
		SCR_AIMoveIndividuallyBehavior individual = SCR_AIMoveIndividuallyBehavior.Cast(move);
		if (individual)
		{
			if (individual.m_Entity) entity = individual.m_Entity.m_Value;
			if (individual.m_Radius) radius = individual.m_Radius.m_Value;
		}
		return string.Format(" %1_move_goal_available=1 %1_move_goal=%2 %1_move_radius=%3 %1_move_entity=%4 %1_move_goal_matches_token=%5",
			prefix, move.m_vPosition.m_Value, radius, AICF_GroupTrace.Entity(entity), move.m_vPosition.m_Value == tokenGoal);
	}
	static void Report(AICF_TransportTrip trip, AICF_VehicleRequestState request, AICF_VehicleSpawnPlan plan,
		AICF_VehicleStagingActionToken token, string sourceEvent, string reason, int nowMs)
	{
		if (!Replication.IsServer() || !trip || !trip.IsValid() || trip.IsTerminal() || !request || trip.GetRequestState() != request ||
			!plan || request.GetSpawnPlan() != plan || !token || !trip.GetAssignment()) return;
		SCR_AIGroup group = trip.GetAssignment().GetGroup();
		if (!token.Matches(trip, request.GetRequestGeneration(), group)) return;
		AIAgent agent = token.GetAgent();
		IEntity actor = token.GetReservedEntity();
		if (!agent || agent.GetParentGroup() != group || agent.GetControlledEntity() != actor ||
			!AICF_VehicleBoardingMutationFence.IsAuthoritativeAIEntity(actor)) return;
		SCR_AIUtilityComponent utility = SCR_AIUtilityComponent.Cast(agent.FindComponent(SCR_AIUtilityComponent));
		if (!utility || utility.m_OwnerEntity != actor) return;
		SCR_AIMoveIndividuallyBehavior expected = token.GetAction();
		ChimeraCharacter character = ChimeraCharacter.Cast(actor);
		AIAgent controlAgent;
		if (character && character.GetAIControlComponent()) controlAgent = character.GetAIControlComponent().GetControlAIAgent();
		AIActionBase current = utility.GetCurrentAction();
		AIActionBase executed = utility.GetExecutedAction();
		vector position = actor.GetOrigin();
		string identity = string.Format("operation_id=%1 faction=%2 slot=%3 group_generation=%4 trip_generation=%5 request_generation=%6 member=%7 source_event=%8 reason=%9",
			trip.GetOperationId(), trip.GetFactionKey(), trip.GetSlotId(), trip.GetGroupGeneration(), trip.GetTripGeneration(), request.GetRequestGeneration(), actor.GetID(), sourceEvent, reason);
		identity += string.Format(" action_token=%1 member_component_rpl=%2 retry=%3 position=%4 plan_goal=%5 token_goal=%6",
			token.GetFence().GetToken(), AICF_VehicleBehaviorBinding.ComponentRpl(actor), token.GetRetryCount(), position, plan.GetStagingPosition(), token.GetTargetPosition());
		identity += string.Format(" actual_control_agent=%1 control_agent_exact=%2", AICF_GroupTrace.Entity(controlAgent), controlAgent == agent);
		string actions = string.Format(" expected=%1 current=%2 executed=%3 current_is_expected=%4 executed_is_expected=%5 utility_contains_expected=%6",
			Action(expected), Action(current), Action(executed), current == expected && expected != null, executed == expected && expected != null, token.IsActionOwnedByUtility());
		actions += MoveGoal("current", current, token.GetTargetPosition()) + MoveGoal("executed", executed, token.GetTargetPosition());
		if (expected)
		{
			IEntity entityTarget;
			if (expected.m_Entity) entityTarget = expected.m_Entity.m_Value;
			vector actualGoal;
			float radius = -1;
			if (expected.m_vPosition) actualGoal = expected.m_vPosition.m_Value;
			if (expected.m_Radius) radius = expected.m_Radius.m_Value;
			actions += string.Format(" actual_action_goal=%1 actual_action_radius=%2 actual_action_entity=%3 action_utility_matches=%4 actual_goal_matches_token=%5 actual_goal_finite=%6",
				actualGoal, radius, AICF_GroupTrace.Entity(entityTarget), expected.m_Utility == utility, actualGoal == token.GetTargetPosition(), Finite(actualGoal));
		}
		AICF_Stage4Diagnostics.Info("VEHICLE_STAGING_NATIVE_ACTION", identity + actions + " " + token.ObserveNativeDiagnosticMotion(nowMs));
		if (controlAgent != agent)
		{
			AICF_Stage4Diagnostics.Info("VEHICLE_STAGING_NATIVE_PATH", identity + " queried=0 reason=ACTOR_CONTROL_AGENT_MISMATCH remaining_distance_proven=0");
			return;
		}
		AIBaseMovementComponent movement = agent.GetMovementComponent();
		if (!movement || movement.GetAIAgent() != agent)
		{
			AICF_Stage4Diagnostics.Info("VEHICLE_STAGING_NATIVE_PATH", identity + " queried=0 reason=NO_EXACT_ACTOR_MOVEMENT_COMPONENT remaining_distance_proven=0");
			return;
		}
		array<vector> points = {};
		movement.GetCurrentPath(points);
		bool completed = movement.HasCompletedRequest(false);
		if (!token.Matches(trip, request.GetRequestGeneration(), group) || token.GetAction() != expected ||
			agent.GetControlledEntity() != actor || agent.GetMovementComponent() != movement || movement.GetAIAgent() != agent ||
			!character.GetAIControlComponent() || character.GetAIControlComponent().GetControlAIAgent() != agent ||
			utility.GetCurrentAction() != current || utility.GetExecutedAction() != executed)
		{
			AICF_Stage4Diagnostics.Info("VEHICLE_STAGING_NATIVE_PATH", identity + " queried=1 reason=CONTEXT_CHANGED_DISCARDED remaining_distance_proven=0");
			return;
		}
		int count = points.Count();
		string first = "NONE";
		string last = "NONE";
		if (count > 0) { first = points[0].ToString(); last = points[count - 1].ToString(); }
		bool finite = true;
		float arc;
		int examined = Math.Min(count, PATH_ARC_LIMIT);
		for (int index; index < examined; index++)
		{
			if (!Finite(points[index])) { finite = false; break; }
			if (index > 0)
			{
				float segment = vector.Distance(points[index - 1], points[index]);
				if (!(segment >= 0 && segment < float.MAX)) { finite = false; break; }
				arc += segment;
			}
		}
		if (!(arc >= 0 && arc < float.MAX)) finite = false;
		if (!finite) arc = -1;
		string path = string.Format(" queried=1 movement_component=%1 component_agent=%2 completed_request_ignore_time=%3 native_point_count=%4 first=%5 last=%6 raw_polyline_arc_m=%7 arc_examined_points=%8 arc_complete=%9",
			movement.ToString(), AICF_GroupTrace.Entity(movement.GetAIAgent()), completed, count, first, last, arc, examined, finite && count <= PATH_ARC_LIMIT);
		path += string.Format(" path_finite=%1 path_finite_scope=EXAMINED_PREFIX path_all_finite_proven=%2 emitted_points=%3 point_limit=%4 raw_native_order=1 remaining_distance_proven=0",
			finite, finite && count <= PATH_ARC_LIMIT, Math.Min(count, PATH_LOG_LIMIT), PATH_LOG_LIMIT);
		if (count > 0)
			path += string.Format(" first_finite=%1 last_finite=%2 first_to_actor_m=%3 last_to_actor_m=%4 first_to_token_goal_m=%5 last_to_token_goal_m=%6",
				Finite(points[0]), Finite(points[count - 1]), vector.Distance(position, points[0]), vector.Distance(position, points[count - 1]),
				vector.Distance(token.GetTargetPosition(), points[0]), vector.Distance(token.GetTargetPosition(), points[count - 1]));
		AICF_Stage4Diagnostics.Info("VEHICLE_STAGING_NATIVE_PATH", identity + path);
		for (int sample; sample < Math.Min(count, PATH_LOG_LIMIT); sample++)
			AICF_Stage4Diagnostics.Info("VEHICLE_STAGING_NATIVE_PATH_POINT", identity + string.Format(" index=%1 point=%2 finite=%3 raw_native_order=1", sample, points[sample], Finite(points[sample])));
	}
}
