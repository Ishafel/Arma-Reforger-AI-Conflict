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
