// Только изолированный stage. Defend удерживает stock ALL без completion,
// воспроизводя зависшее ожидание; planner, watchdog и 30-секундный timer — production.
modded class AICF_OrderPlanner
{
	string m_sAICFProbeRebuildReason;
	bool m_bAICFProbeTimeoutWithoutCallback;

	override bool RebuildCurrentOrder(AICF_GroupSlot slot, SCR_CampaignFaction faction,
		string reason, bool recoverStuckRoute = false)
	{
		bool timeoutProof = reason == "STUCK_ROUTE_COMPLETION_TIMEOUT" &&
			slot.GetOwnedWaypointTerminalOutcome(slot.GetWaypoint()).IsEmpty() &&
			slot.GetStuckRouteCompletionWaitAgeMs() >= 30000;
		bool issued = super.RebuildCurrentOrder(slot, faction, reason, recoverStuckRoute);
		if (issued)
		{
			m_sAICFProbeRebuildReason = reason;
			m_bAICFProbeTimeoutWithoutCallback = timeoutProof;
		}
		return issued;
	}

	override protected AIWaypoint CreateStuckRouteWaypoint(AICF_GroupSlot slot,
		SCR_CampaignFaction faction, SCR_CampaignMilitaryBaseComponent target)
	{
		string enabled;
		if (!System.GetCLIParam("aicfStuckWaitProbe", enabled) || enabled != "1")
			return super.CreateStuckRouteWaypoint(slot, faction, target);
		IEntity leader = AICF_GroupRuntime.ResolveAliveLeader(slot.GetGroup());
		if (!leader)
			return null;
		EntitySpawnParams params = new EntitySpawnParams();
		params.TransformMode = ETransformMode.WORLD;
		params.Transform[3] = leader.GetOrigin();
		AIWaypoint waypoint = AIWaypoint.Cast(GetGame().SpawnEntityPrefabEx(DEFEND_WAYPOINT_PREFAB, false, params: params));
		if (waypoint)
		{
			waypoint.SetCompletionType(EAIWaypointCompletionType.All);
			waypoint.SetCompletionRadius(20);
			SCR_TimedWaypoint.Cast(waypoint).SetHoldingTime(3600);
		}
		return waypoint;
	}
}

modded class AICF_GroupSlot
{
	int AICF_ProbeProgressClock() { return m_iLastProgressAtMs; }
	void AICF_ProbeCompletion(AIWaypoint waypoint) { OnOwnedWaypointCompleted(waypoint); }
	void AICF_ProbeWaitAge(int ageMs) { m_iStuckRouteCompletionWaitStartedAtMs = System.GetTickCount() - ageMs; }
	bool AICF_ProbeGenerationFence()
	{
		m_iSpawnGeneration++;
		bool fenced = !IsStuckRouteContextCurrent() && IsStuckRouteWaypoint() && GetStuckRouteCompletionWaitAgeMs() == 0;
		m_iSpawnGeneration--;
		return fenced;
	}
}

modded class AICF_MatchController
{
	protected bool m_bAICFStuckWaitStarted;
	protected int m_iAICFStuckWaitChecks;
	protected int m_iAICFStuckWaitFailures;
	protected int m_iAICFStuckWaitPolls;
	protected int m_iAICFStuckWaitClock;
	protected int m_iAICFStuckWaitIntent;
	protected int m_iAICFStuckWaitGeneration;
	protected SCR_AIGroup m_AICFStuckWaitGroup;
	protected AIWaypoint m_AICFStuckWaitWaypoint;

	protected void AICF_CheckStuckWait(string name, bool passed)
	{
		m_iAICFStuckWaitChecks++;
		if (!passed) m_iAICFStuckWaitFailures++;
		Print(string.Format("[AICF][STUCK_WAIT_PROBE_CHECK] name=%1 passed=%2", name, passed));
	}

	override protected void TryLogRosterReady()
	{
		super.TryLogRosterReady();
		string enabled;
		if (!m_bRosterReady || m_bAICFStuckWaitStarted ||
			!System.GetCLIParam("aicfStuckWaitProbe", enabled) || enabled != "1")
			return;
		m_bAICFStuckWaitStarted = true;
		GetGame().GetCallqueue().CallLater(AICF_BeginStuckWaitProbe, 1, false);
	}

	protected void AICF_BeginStuckWaitProbe()
	{
		AICF_GroupSlot slot = m_USState.GetSlot(0);
		slot.SetDesiredSize(1);
		bool setup = !slot.IsRecruitingInfantry() &&
			m_OrderPlanner.AssignPlayerOrder(slot, m_USFaction, slot.GetTargetBase()) &&
			m_OrderPlanner.RebuildCurrentOrder(slot, m_USFaction, "PROBE_WAIT_SETUP", true);
		AICF_CheckStuckWait("SETUP", setup);
		if (!setup)
		{
			AICF_FinishStuckWaitProbe();
			return;
		}
		m_AICFStuckWaitGroup = slot.GetGroup();
		m_iAICFStuckWaitIntent = slot.GetStrategicIntentRevision();
		m_iAICFStuckWaitGeneration = slot.GetSpawnGeneration();
		AIWaypoint waypoint = slot.GetWaypoint();
		vector origin = waypoint.GetOrigin();
		waypoint.SetOrigin(origin + "100 0 0");
		AICF_CheckStuckWait("DISTANT_LEADER_NO_ADVANCE", !m_OrderPlanner.TryAdvanceStuckRoute(slot, m_USFaction) && slot.GetStuckRouteCompletionWaitAgeMs() == 0);
		waypoint.SetOrigin(origin);
		AICF_CheckStuckWait("NO_CALLBACK_NO_EARLY_ADVANCE", !m_OrderPlanner.TryAdvanceStuckRoute(slot, m_USFaction) && slot.GetWaypoint() == waypoint);
		slot.AICF_ProbeWaitAge(10000);
		AICF_CheckStuckWait("REPEATED_ARRIVAL_DOES_NOT_REARM", !slot.BeginStuckRouteCompletionWait() && slot.GetStuckRouteCompletionWaitAgeMs() >= 10000);
		waypoint.SetOrigin(origin + "100 0 0");
		m_OrderPlanner.TryAdvanceStuckRoute(slot, m_USFaction);
		waypoint.SetOrigin(origin);
		m_OrderPlanner.TryAdvanceStuckRoute(slot, m_USFaction);
		AICF_CheckStuckWait("RADIUS_REENTRY_KEEPS_DEADLINE", slot.GetStuckRouteCompletionWaitAgeMs() >= 10000);
		AICF_CheckStuckWait("GENERATION_FENCE", slot.AICF_ProbeGenerationFence());
		slot.RecordRuntimeWaypointReplacement();
		AICF_CheckStuckWait("ASSIGNMENT_FENCE", !slot.IsStuckRouteContextCurrent() && slot.IsStuckRouteWaypoint() && !m_OrderPlanner.TryAdvanceStuckRoute(slot, m_USFaction));
		slot.MarkStuckRouteWaypoint();
		slot.AICF_ProbeCompletion(waypoint);
		waypoint.SetOrigin(origin + "100 0 0");
		AICF_CheckStuckWait("CALLBACK_OUTSIDE_RADIUS_REJECTED", !m_OrderPlanner.TryAdvanceStuckRoute(slot, m_USFaction));
		waypoint.SetOrigin(origin);
		AICF_CheckStuckWait("CALLBACK_ADVANCES_IMMEDIATELY", m_OrderPlanner.TryAdvanceStuckRoute(slot, m_USFaction) && !slot.IsStuckRouteWaypoint());
		AICF_CheckStuckWait("CLEANUP_RESETS_WAIT", slot.GetStuckRouteCompletionWaitAgeMs() == 0);
		AICF_CheckStuckWait("SECOND_LEG", m_OrderPlanner.RebuildCurrentOrder(slot, m_USFaction, "PROBE_REAL_DEADLINE", true));
		m_AICFStuckWaitWaypoint = slot.GetWaypoint();
		m_OrderPlanner.TryAdvanceStuckRoute(slot, m_USFaction);
		MonitorGroupProgress(m_USState, slot, m_USFaction);
		m_iAICFStuckWaitClock = slot.AICF_ProbeProgressClock();
		GetGame().GetCallqueue().CallLater(AICF_PollStuckWaitProbe, 5000, true);
	}

	protected void AICF_PollStuckWaitProbe()
	{
		m_iAICFStuckWaitPolls++;
		AICF_GroupSlot slot = m_USState.GetSlot(0);
		if (m_iAICFStuckWaitPolls == 2)
		{
			AICF_CheckStuckWait("WAIT_PRESENT_AFTER_10S", slot.IsStuckRouteWaypoint() && slot.GetStuckRouteCompletionWaitAgeMs() >= 10000);
			AICF_CheckStuckWait("WATCHDOG_CLOCK_NOT_REFRESHED", slot.AICF_ProbeProgressClock() == m_iAICFStuckWaitClock);
		}
		if (slot.GetWaypoint() != m_AICFStuckWaitWaypoint || m_iAICFStuckWaitPolls >= 9)
		{
			AICF_CheckStuckWait("DEADLINE_RESUMED_ORDER", !slot.IsStuckRouteWaypoint() && slot.GetWaypoint() != m_AICFStuckWaitWaypoint && m_iAICFStuckWaitPolls >= 6 && m_iAICFStuckWaitPolls <= 7);
			AICF_CheckStuckWait("EXACT_TIMEOUT_WITHOUT_CALLBACK", m_OrderPlanner.m_sAICFProbeRebuildReason == "STUCK_ROUTE_COMPLETION_TIMEOUT" && m_OrderPlanner.m_bAICFProbeTimeoutWithoutCallback);
			AICF_CheckStuckWait("GROUP_GENERATION_INTENT_PRESERVED", slot.GetGroup() == m_AICFStuckWaitGroup && slot.GetSpawnGeneration() == m_iAICFStuckWaitGeneration && slot.GetStrategicIntentRevision() == m_iAICFStuckWaitIntent);
			AICF_FinishStuckWaitProbe();
		}
	}

	protected void AICF_FinishStuckWaitProbe()
	{
		Print(string.Format("[AICF][STUCK_WAIT_PROBE_FINISHED] checks=%1 failures=%2", m_iAICFStuckWaitChecks, m_iAICFStuckWaitFailures));
		GetGame().GetCallqueue().Remove(AICF_PollStuckWaitProbe);
		GetGame().RequestClose();
	}

	override protected void Stop(bool cleanupEntities)
	{
		if (GetGame())
		{
			GetGame().GetCallqueue().Remove(AICF_BeginStuckWaitProbe);
			GetGame().GetCallqueue().Remove(AICF_PollStuckWaitProbe);
		}
		super.Stop(cleanupEntities);
	}
}
