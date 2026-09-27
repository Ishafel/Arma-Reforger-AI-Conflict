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

	protected bool AICF_HandleFailedMovement(int result, int handler, bool related, vector location)
	{
		if (!Replication.IsServer() || result != EMoveError.UNKNOWN ||
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
		if (!slot || !activity || !slot.ReportFailedMovement(m_Group, activity.m_RelatedWaypoint))
			return false;
		AIActionBase failedAction = m_GroupUtilityComponent.GetExecutedAction();
		m_GroupUtilityComponent.OnMoveFailed(result, null, related, location);
		// Invoker может синхронно сменить assignment/action. Новый action не трогаем.
		if (failedAction && slot.HasFailedMovement() && m_GroupUtilityComponent.GetExecutedAction() == failedAction)
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
