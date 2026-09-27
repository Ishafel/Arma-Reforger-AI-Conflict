// Только isolated stage. Реальные group/waypoint, смена приказа — через planner.
class AICF_DefendInputProbeNode : SCR_AIGetDefendWaypointParameters
{
	SCR_DefendWaypoint Resolve(AIAgent owner, bool hasInput, IEntity input)
	{
		return AICF_ResolveDefendWaypoint(owner, hasInput, input);
	}
}

modded class AICF_MatchController
{
	protected bool m_bAICFDefendProbeStarted;
	protected int m_iAICFDefendChecks;
	protected int m_iAICFDefendFailures;

	protected void AICF_DefendCheck(string name, bool passed)
	{
		m_iAICFDefendChecks++;
		if (!passed)
			m_iAICFDefendFailures++;
		Print(string.Format("[AICF][DEFEND_INPUT_CHECK] name=%1 passed=%2", name, passed));
	}

	protected void AICF_TestDefendInput(AICF_GroupSlot slot, SCR_CampaignFaction faction, string side)
	{
		SCR_AIGroup group = slot.GetGroup();
		IEntity leader = AICF_GroupRuntime.ResolveAliveLeader(group);
		SCR_CampaignMilitaryBaseComponent target = slot.GetStrategicIntentTargetBase();
		AICF_DefendCheck(side + "_SETUP", group && leader && target);
		if (!group || !leader || !target)
			return;
		AICF_DefendInputProbeNode node = new AICF_DefendInputProbeNode();
		bool held = m_OrderPlanner.HoldPositionForTemporaryRouteReplan(slot, faction, target, leader.GetOrigin(), false);
		SCR_DefendWaypoint defend = SCR_DefendWaypoint.Cast(group.GetCurrentWaypoint());
		AICF_DefendCheck(side + "_DEFEND_CREATED", held && defend);
		if (!held || !defend)
			return;
		AICF_DefendCheck(side + "_FALLBACK_DEFEND", node.Resolve(group, false, null) == defend);
		AICF_DefendCheck(side + "_EXPLICIT_DEFEND", node.Resolve(null, true, defend) == defend);
		AICF_DefendCheck(side + "_EXPLICIT_NULL_NO_FALLBACK", !node.Resolve(group, true, null));
		AICF_DefendCheck(side + "_WRONG_ENTITY_NO_FALLBACK", !node.Resolve(group, true, leader));
		AICF_DefendCheck(side + "_MISSING_OWNER", !node.Resolve(null, false, null));
		AICF_DefendCheck(side + "_NATIVE_DEFEND_SUCCESS", node.EOnTaskSimulate(group, 0.016) == ENodeResult.SUCCESS);
		slot.ClearTemporaryRouteReplanHold();
		bool restored = m_OrderPlanner.AssignOrder(slot, faction, m_ObjectiveGraph, m_TargetSelector, "DEFEND_INPUT_PROBE_RESTORE");
		AIWaypoint move = group.GetCurrentWaypoint();
		AICF_DefendCheck(side + "_MOVE_RESTORED", restored && move && !SCR_DefendWaypoint.Cast(move));
		bool failed = true;
		for (int i; i < 64; i++)
			failed = (node.EOnTaskSimulate(group, 0.016) == ENodeResult.FAIL) && failed;
		AICF_DefendCheck(side + "_STALE_DEFEND_64_FAILS", failed);
		AICF_DefendCheck(side + "_MOVE_UNCHANGED", slot.GetGroup() == group && slot.GetWaypoint() == move && group.GetCurrentWaypoint() == move);
		AICF_DefendCheck(side + "_EXPLICIT_MOVE_REJECTED", !node.Resolve(group, true, move));
		AICF_DefendCheck(side + "_INTENT_UNCHANGED", slot.GetStrategicIntentTargetBase() == target);
	}

	override protected void TryLogRosterReady()
	{
		super.TryLogRosterReady();
		string enabled;
		if (!m_bRosterReady || m_bAICFDefendProbeStarted ||
			!System.GetCLIParam("aicfDefendInputProbe", enabled) || enabled != "1")
			return;
		m_bAICFDefendProbeStarted = true;
		AICF_TestDefendInput(m_USState.GetSlot(0), m_USFaction, "US");
		AICF_TestDefendInput(m_USSRState.GetSlot(0), m_USSRFaction, "USSR");
		GetGame().GetCallqueue().CallLater(AICF_FinishDefendProbe, 60000, false);
	}

	protected void AICF_FinishDefendProbe()
	{
		Print(string.Format("[AICF][DEFEND_INPUT_FINISHED] checks=%1 failures=%2", m_iAICFDefendChecks, m_iAICFDefendFailures));
		GetGame().RequestClose();
	}

	override protected void Stop(bool cleanupEntities)
	{
		if (GetGame())
			GetGame().GetCallqueue().Remove(AICF_FinishDefendProbe);
		super.Stop(cleanupEntities);
	}
}
