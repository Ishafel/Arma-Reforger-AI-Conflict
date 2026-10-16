// Только isolated stage. Исполняющийся native BT node сохраняет настоящий callstack.
class AICF_DefendAllocationWitness
{
	static AICF_MatchController s_Controller;
	static SCR_AIGroup s_US;
	static SCR_AIGroup s_USSR;
	static bool s_bUSInjected;
	static bool s_bUSSRInjected;
}

modded class SCR_AIAllocateActionsForDefendActivity
{
	override ENodeResult EOnTaskSimulate(AIAgent owner, float dt)
	{
		IEntity input;
		GetVariableIn(PORT_WAYPOINT, input);
		bool inject = (owner == AICF_DefendAllocationWitness.s_US && !AICF_DefendAllocationWitness.s_bUSInjected) ||
			(owner == AICF_DefendAllocationWitness.s_USSR && !AICF_DefendAllocationWitness.s_bUSSRInjected);
		ENodeResult result = super.EOnTaskSimulate(owner, dt);
		if (inject && SCR_DefendWaypoint.Cast(input) && result == ENodeResult.SUCCESS)
		{
			if (owner == AICF_DefendAllocationWitness.s_US) AICF_DefendAllocationWitness.s_bUSInjected = true;
			if (owner == AICF_DefendAllocationWitness.s_USSR) AICF_DefendAllocationWitness.s_bUSSRInjected = true;
			AICF_DefendAllocationWitness.s_Controller.AICF_AllocationReplace(this, SCR_AIGroup.Cast(owner), input);
		}
		return result;
	}

	bool AICF_ProbeActivityReady(SCR_AIGroup group)
	{
		// Те же native preconditions, что перед чтением WaypointIn в stock node.
		return m_groupOwner == group && m_Utility && m_Mailbox && m_RelatedActivity;
	}

	IEntity AICF_ProbeReadInput()
	{
		IEntity input;
		GetVariableIn(PORT_WAYPOINT, input);
		return input;
	}
}

modded class AICF_MatchController
{
	protected bool m_bAICFAllocationStarted;
	protected int m_iAICFAllocationChecks;
	protected int m_iAICFAllocationFailures;
	protected int m_iAICFAllocationAttempts;
	protected int m_iAICFAllocationReturns;
	protected AIWaypoint m_AICFAllocationUSWaypoint;
	protected AIWaypoint m_AICFAllocationUSSRWaypoint;

	protected void AICF_AllocationCheck(string side, string name, bool passed)
	{
		m_iAICFAllocationChecks++;
		if (!passed) m_iAICFAllocationFailures++;
		Print(string.Format("[AICF][DEFEND_ALLOCATION_CHECK] side=%1 name=%2 passed=%3", side, name, passed));
	}

	override protected void TryLogRosterReady()
	{
		super.TryLogRosterReady();
		string enabled;
		if (!m_bRosterReady || m_bAICFAllocationStarted || !System.GetCLIParam("aicfDefendAllocationProbe", enabled)) return;
		m_bAICFAllocationStarted = true;
		AICF_DefendAllocationWitness.s_Controller = this;
		AICF_DefendAllocationWitness.s_US = m_USState.GetSlot(0).GetGroup();
		AICF_DefendAllocationWitness.s_USSR = m_USSRState.GetSlot(0).GetGroup();
		AICF_AllocationPrepare(m_USState.GetSlot(0), m_USFaction, "US");
		AICF_AllocationPrepare(m_USSRState.GetSlot(0), m_USSRFaction, "USSR");
		GetGame().GetCallqueue().CallLater(AICF_AllocationFinish, 30000, false);
	}

	// Только focused fixture удерживает условия native-node проверки.
	override protected void Update() { if (!m_bAICFAllocationStarted) super.Update(); }
	override protected void CommanderTick() { if (!m_bAICFAllocationStarted) super.CommanderTick(); }
	override protected void ReliabilityTick() { if (!m_bAICFAllocationStarted) super.ReliabilityTick(); }

	protected void AICF_AllocationPrepare(AICF_GroupSlot slot, SCR_CampaignFaction faction, string side)
	{
		SCR_AIGroup group = slot.GetGroup();
		IEntity leader = AICF_GroupRuntime.ResolveAliveLeader(group);
		SCR_CampaignMilitaryBaseComponent target = slot.GetStrategicIntentTargetBase();
		AICF_AllocationCheck(side, "SETUP", group && leader && target);
		if (!group || !leader || !target) return;
		bool held = m_OrderPlanner.HoldPositionForTemporaryRouteReplan(slot, faction, target, leader.GetOrigin(), false);
		AICF_AllocationCheck(side, "HOLD_CREATED", held && SCR_DefendWaypoint.Cast(group.GetCurrentWaypoint()));
	}

	void AICF_AllocationReplace(SCR_AIAllocateActionsForDefendActivity node, SCR_AIGroup group, IEntity oldWaypoint)
	{
		string side = "US";
		AICF_GroupSlot slot = m_USState.GetSlot(0);
		SCR_CampaignFaction faction = m_USFaction;
		if (group == AICF_DefendAllocationWitness.s_USSR) { side = "USSR"; slot = m_USSRState.GetSlot(0); faction = m_USSRFaction; }
		AICF_AllocationCheck(side, "NATIVE_ALLOCATION_SUCCESS", true);
		AICF_AllocationCheck(side, "NATIVE_ACTIVITY", node.AICF_ProbeActivityReady(group));
		IEntity leader = AICF_GroupRuntime.ResolveAliveLeader(group);
		EntityID oldId = oldWaypoint.GetID();
		bool replaced = leader && m_OrderPlanner.AssignPlayerPointOrder(slot, faction, leader.GetOrigin() + "100 0 100");
		AIWaypoint move = slot.GetWaypoint();
		if (side == "US") m_AICFAllocationUSWaypoint = move;
		else m_AICFAllocationUSSRWaypoint = move;
		AICF_AllocationCheck(side, "MOVE_RESTORED", replaced && move && move.GetID() != oldId);
		AICF_AllocationCheck(side, "OLD_WAYPOINT_REMOVED", !oldWaypoint);
		AICF_AllocationCheck(side, "NATIVE_INPUT_EMPTY", !node.AICF_ProbeReadInput());
		m_iAICFAllocationAttempts++;
		// Повтор — на том же native node и в его текущем BT callstack.
		Print("[AICF][DEFEND_ALLOCATION_ATTEMPT] side=" + side + " calls=64 input=REMOVED_BY_PLANNER");
		bool failed = true;
		for (int i = 0; i < 64; i++)
			failed = (node.EOnTaskSimulate(group, 0.016) == ENodeResult.FAIL) && failed;
		m_iAICFAllocationReturns++;
		AICF_AllocationCheck(side, "STALE_64_FAILS", failed);
		array<AIWaypoint> queue = {};
		group.GetWaypoints(queue);
		AICF_AllocationCheck(side, "MOVE_UNCHANGED", slot.GetWaypoint() == move && queue.Contains(move));
		AICF_AllocationCheck(side, "GROUP_UNCHANGED", slot.GetGroup() == group);
	}

	protected void AICF_AllocationFinish()
	{
		AICF_AllocationCheck("US", "NEW_ORDER_CURRENT", m_AICFAllocationUSWaypoint && m_USState.GetSlot(0).GetWaypoint() == m_AICFAllocationUSWaypoint && AICF_DefendAllocationWitness.s_US.GetCurrentWaypoint() == m_AICFAllocationUSWaypoint);
		AICF_AllocationCheck("USSR", "NEW_ORDER_CURRENT", m_AICFAllocationUSSRWaypoint && m_USSRState.GetSlot(0).GetWaypoint() == m_AICFAllocationUSSRWaypoint && AICF_DefendAllocationWitness.s_USSR.GetCurrentWaypoint() == m_AICFAllocationUSSRWaypoint);
		Print(string.Format("[AICF][DEFEND_ALLOCATION_FINISHED] checks=%1 failures=%2 attempted=%3 returned=%4", m_iAICFAllocationChecks, m_iAICFAllocationFailures, m_iAICFAllocationAttempts, m_iAICFAllocationReturns));
		GetGame().RequestClose();
	}

	override protected void Stop(bool cleanupEntities)
	{
		if (GetGame()) GetGame().GetCallqueue().Remove(AICF_AllocationFinish);
		AICF_DefendAllocationWitness.s_Controller = null;
		AICF_DefendAllocationWitness.s_US = null;
		AICF_DefendAllocationWitness.s_USSR = null;
		AICF_DefendAllocationWitness.s_bUSInjected = false;
		AICF_DefendAllocationWitness.s_bUSSRInjected = false;
		super.Stop(cleanupEntities);
	}
}
