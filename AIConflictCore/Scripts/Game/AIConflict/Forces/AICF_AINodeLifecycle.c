// Script Diff 1.8.0.13: ActivityDefend может дойти до чтения параметров уже
// после смены current waypoint. Невалидный тип завершает устаревшую ветку,
// а не вызывает stock NodeError. Явный пустой вход не заменяем current waypoint.
modded class SCR_AIGetDefendWaypointParameters
{
	override ENodeResult EOnTaskSimulate(AIAgent owner, float dt)
	{
		IEntity inputWaypoint;
		bool hasInput = GetVariableIn(PORT_WAYPOINT_IN, inputWaypoint);
		if (!AICF_ResolveDefendWaypoint(owner, hasInput, inputWaypoint))
			return ENodeResult.FAIL;
		return super.EOnTaskSimulate(owner, dt);
	}

	protected SCR_DefendWaypoint AICF_ResolveDefendWaypoint(AIAgent owner, bool hasInput, IEntity inputWaypoint)
	{
		if (!hasInput)
		{
			AIGroup group = AIGroup.Cast(owner);
			if (!group)
				return null;
			inputWaypoint = group.GetCurrentWaypoint();
		}
		return SCR_DefendWaypoint.Cast(inputWaypoint);
	}
}

// Удалённый planner waypoint обнуляет вход ещё исполняющегося ActivityPerformAction.
// Для управляемой группы завершаем эту ветку и снимаем её smart-action callbacks.
// Непустой вход и все неуправляемые группы сохраняют stock validation.
modded class SCR_AIGetSmartActionsState
{
	override ENodeResult EOnTaskSimulate(AIAgent owner, float dt)
	{
		AIWaypoint waypoint;
		GetVariableIn(WAYPOINT_ENTITY_IN, waypoint);
		SCR_AIGroup group = SCR_AIGroup.Cast(owner);
		AICF_MatchController controller = AICF_MatchController.GetActiveController();
		if (!waypoint && Replication.IsServer() && group && controller && controller.FindManagedInfantrySlot(group))
		{
			// Stock OnAbort вызывает private ResetVariables, включая unregister.
			super.OnAbort(owner, null);
			Print("[AICF][AI_SMART_ACTION_CANCELLED] reason=WAYPOINT_INPUT_REMOVED");
			return ENodeResult.FAIL;
		}
		return super.EOnTaskSimulate(owner, dt);
	}
}

// При отмене вложенного door/navlink BT вход AgentIn уже может быть снят.
// Менять LOD без агента нельзя; штатная проверка при исполнении остаётся строгой.
modded class SCR_AIToggleMaxLOD
{
	override void OnAbort(AIAgent owner, Node nodeCausingAbort)
	{
		if (m_bPerformOnAbort && !m_bAbortFinished)
		{
			AIAgent agent;
			if (!GetVariableIn(PORT_AGENT, agent))
			{
				m_bAbortFinished = true;
				Print("[AICF][AI_MAXLOD_ABORT_SKIPPED] reason=AGENT_INPUT_UNAVAILABLE", LogLevel.WARNING);
				return;
			}
		}
		super.OnAbort(owner, nodeCausingAbort);
	}
}

// Stable controller переживает потерю последнего бойца. Запоздавший результат
// движения не должен читать origin отсутствующего лидера или завершать waypoint.
modded class SCR_AIProcessFailedMovementResult
{
	protected bool m_bAICFMoveContextReported;

	override void OnEnter(AIAgent owner)
	{
		super.OnEnter(owner);
		m_bAICFMoveContextReported = false;
	}

	override ENodeResult EOnTaskSimulate(AIAgent owner, float dt)
	{
		if (!m_Group || !m_GroupUtilityComponent || !m_Group.GetLeaderEntity())
			return ENodeResult.FAIL;
		int result, handler;
		bool related;
		if (GetVariableIn(PORT_MOVE_RESULT, result))
		{
			// Как в stock: необязательные порты оставляют исходные значения.
			bool hasHandler = GetVariableIn(PORT_HANDLER_ID, handler);
			bool hasRelated = GetVariableIn(PORT_IS_WAYPOINT_RELATED, related);
			vector location;
			GetVariableIn(PORT_MOVE_LOCATION, location);
			if (AICF_FIAGarrisonMovementPolicy.Handle(m_Group, m_GroupUtilityComponent, result, handler, related, location))
				return ENodeResult.FAIL;
			if (AICF_FIAPatrolMovementPolicy.Handle(m_Group, m_GroupUtilityComponent, result, handler, related, location))
				return ENodeResult.FAIL;
			if (AICF_HandleDefendArrival(result, handler, related, location))
				return ENodeResult.FAIL;
			if (AICF_HandleFailedMovement(result, handler, related, location))
				return ENodeResult.FAIL;
			if (result == EMoveError.UNKNOWN && !m_bAICFMoveContextReported)
			{
				m_bAICFMoveContextReported = true;
				AICF_MatchController controller = AICF_MatchController.GetActiveController();
				AICF_GroupSlot slot;
				if (controller)
					slot = controller.FindManagedInfantrySlot(m_Group);
				SCR_AIActivityBase activity = SCR_AIActivityBase.Cast(m_GroupUtilityComponent.GetCurrentAction());
				AIWaypoint relatedWaypoint, ownedWaypoint;
				if (activity)
					relatedWaypoint = activity.m_RelatedWaypoint;
				if (slot)
					ownedWaypoint = slot.GetWaypoint();
				Print(string.Format("[AICF][AI_MOVE_CONTEXT] group=%1 handler=%2 handler_input=%3 related=%4 related_input=%5 slot=%6 activity=%7 related_waypoint=%8 owned_waypoint=%9",
					m_Group.GetID(), handler, hasHandler, related, hasRelated, slot, activity, relatedWaypoint, ownedWaypoint));
				Print(string.Format("[AICF][AI_MOVE_CURRENT] group=%1 current_waypoint=%2 location=%3", m_Group.GetID(), m_Group.GetCurrentWaypoint(), location));
			}
		}
		return super.EOnTaskSimulate(owner, dt);
	}

	protected bool AICF_HandleDefendArrival(int result, int handler, bool related, vector location)
	{
		if (!Replication.IsServer() || !related || !AICF_IsInfantryMoveHandler(handler))
			return false;
		if (result != EMoveError.STOPPED && result != EMoveError.STUCK && result != EMoveError.UNREACHABLE &&
			result != EMoveError.ENTITY_CANT_MOVE && result != EMoveError.ENTITY_NOT_MOVABLE)
			return false;
		AICF_MatchController controller = AICF_MatchController.GetActiveController();
		if (!controller)
			return false;
		return AICF_OrderPlanner.TryHandoffDefendArrival(controller.FindManagedInfantrySlot(m_Group), m_Group,
			m_GroupUtilityComponent, result, location);
	}

	protected bool AICF_HandleFailedMovement(int result, int handler, bool related, vector location)
	{
		if (!Replication.IsServer() ||
			!related ||
			!m_Group || !m_Group.GetLeaderEntity() || !m_GroupUtilityComponent)
			return false;
		if (!AICF_IsInfantryMoveHandler(handler))
			return false;
		AICF_MatchController controller = AICF_MatchController.GetActiveController();
		AICF_GroupSlot slot;
		if (controller)
			slot = controller.FindManagedInfantrySlot(m_Group);
		SCR_AIActivityBase activity = SCR_AIActivityBase.Cast(m_GroupUtilityComponent.GetCurrentAction());
		if (!activity)
			return false;
		AIWaypoint failedWaypoint = activity.m_RelatedWaypoint;
		if (!slot)
		{
			if (result != EMoveError.UNKNOWN)
				return false;
			return AICF_HandleBuilderFailedMovement(result, related, location, failedWaypoint);
		}
		if (result != EMoveError.UNKNOWN)
		{
			if (!SCR_DefendWaypoint.Cast(failedWaypoint) || failedWaypoint != slot.GetWaypoint())
				return false;
			if (result != EMoveError.STOPPED && result != EMoveError.STUCK && result != EMoveError.UNREACHABLE &&
				result != EMoveError.ENTITY_CANT_MOVE && result != EMoveError.ENTITY_NOT_MOVABLE)
				return false;
		}
		bool recruitmentFailure = slot.ReportRecruitmentFailedMovement(m_Group, failedWaypoint, result);
		if (!recruitmentFailure && !slot.ReportFailedMovement(m_Group, failedWaypoint, result))
			return false;
		AIActionBase failedAction = m_GroupUtilityComponent.GetExecutedAction();
		m_GroupUtilityComponent.OnMoveFailed(result, null, related, location);
		// Invoker может синхронно сменить assignment/action. Новый action не трогаем.
		if (failedAction && (slot.HasFailedMovement() || slot.HasRecruitmentMovementFailure(failedWaypoint)) &&
			m_GroupUtilityComponent.GetExecutedAction() == failedAction)
			failedAction.Fail();
		return true;
	}

	protected bool AICF_HandleBuilderFailedMovement(int result, bool related, vector location, AIWaypoint waypoint)
	{
		AICF_BaseBuilder builder = AICF_BaseBuilderService.ReportFailedMovement(m_Group, waypoint);
		if (!builder)
			return false;
		AICF_BaseBuilderMoveFailure failure = builder.m_MoveFailure;
		AIActionBase failedAction = m_GroupUtilityComponent.GetExecutedAction();
		m_GroupUtilityComponent.OnMoveFailed(result, null, related, location);
		if (AICF_BaseBuilderService.IsMoveFailureCurrent(builder, failure) && failedAction &&
			m_GroupUtilityComponent.GetExecutedAction() == failedAction)
			failedAction.Fail();
		return true;
	}

	protected bool AICF_IsInfantryMoveHandler(int handler)
	{
		if (handler == AIGroupMovementComponent.DEFAULT_HANDLER_ID)
			return true;
		// Native failure без назначенного handler наблюдается как -1. Такой же
		// sentinel бывает у ещё не назначенной машины: одной проверки знака мало.
		if (handler != -1 || !m_GroupUtilityComponent.m_VehicleMgr ||
			m_GroupUtilityComponent.m_VehicleMgr.FindVehicleBySubgroupId(handler))
			return false;
		array<AIAgent> agents = {};
		m_Group.GetAgents(agents);
		int alive;
		foreach (AIAgent agent : agents)
		{
			IEntity member;
			if (agent)
				member = agent.GetControlledEntity();
			if (!AICF_GroupRuntime.IsAliveCharacter(member))
				continue;
			CompartmentAccessComponent access = CompartmentAccessComponent.Cast(member.FindComponent(CompartmentAccessComponent));
			if (!access || access.IsInCompartment() || access.IsGettingIn() || access.IsGettingOut() ||
				CompartmentAccessComponent.GetVehicleIn(member))
				return false;
			alive++;
		}
		return alive > 0;
	}
}

// Владелец реакции BT на failed movement только для зарегистрированного FIA patrol.
class AICF_FIAPatrolMovementPolicy
{
	static bool Handle(SCR_AIGroup group, SCR_AIGroupUtilityComponent utility, int result, int handler, bool related, vector location)
	{
		if (!Replication.IsServer() || result != EMoveError.UNKNOWN || !related ||
			!group || !utility) return false;
		AICF_MatchController controller = AICF_MatchController.GetActiveController();
		SCR_AIActivityBase activity = SCR_AIActivityBase.Cast(utility.GetCurrentAction());
		if (!controller || !activity || !activity.m_RelatedWaypoint || group.GetGroupUtilityComponent() != utility) return false;
		IEntity vehicle;
		if (handler != AIGroupMovementComponent.DEFAULT_HANDLER_ID)
		{
			if (!utility.m_VehicleMgr) return false;
			SCR_AIGroupVehicle groupVehicle = utility.m_VehicleMgr.FindVehicleBySubgroupId(handler);
			if (groupVehicle) vehicle = groupVehicle.GetEntity();
			else if (handler != -1) return false;
		}
		AICF_FIAPatrol p = controller.ReportFIAPatrolFailedMovement(group, activity.m_RelatedWaypoint, vehicle);
		if (!p) return false;
		AICF_FIAPatrolMoveFailure failure = p.m_MoveFailure;
		AIActionBase failedAction = utility.GetExecutedAction();
		utility.OnMoveFailed(result, vehicle, related, location);
		// Invoker может заменить waypoint, leg или завершить lifecycle синхронно.
		if (failure && failure.IsCurrent(p) && p.m_MoveFailure == failure && failedAction &&
			utility.GetExecutedAction() == failedAction) failedAction.Fail();
		return true;
	}

}
