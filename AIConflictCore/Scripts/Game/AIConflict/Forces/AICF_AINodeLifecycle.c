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
	override ENodeResult EOnTaskSimulate(AIAgent owner, float dt)
	{
		if (!m_Group || !m_GroupUtilityComponent || !m_Group.GetLeaderEntity())
			return ENodeResult.FAIL;
		return super.EOnTaskSimulate(owner, dt);
	}
}
