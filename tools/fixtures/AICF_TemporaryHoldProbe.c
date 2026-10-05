// Только isolated stage: -aicfTemporaryHoldProbe 1. Реальные группа, waypoint и BT.
// Инъекции: completed/removed waypoint и отказ strategic replan; deadline не ускоряется.
modded class AICF_GroupSlot
{
	override int GetDeploymentSize()
	{
		string enabled;
		if (GetSlotId() == 8 && System.GetCLIParam("aicfTemporaryHoldProbe", enabled) && enabled == "1")
			return GetDesiredSize();
		return super.GetDeploymentSize();
	}
}

modded class AICF_MatchController
{
	protected AICF_GroupSlot m_AICFHoldProbeSlot;
	protected int m_iAICFHoldProbeAt;
	protected int m_iAICFHoldProbePhase;
	protected int m_iAICFHoldProbeFailures;
	protected int m_iAICFHoldProbeAssignment;
	protected int m_iAICFHoldProbeGeneration;
	protected bool m_bAICFHoldProbeDone;
	protected AIWaypoint m_AICFHoldProbeWaypoint;

	protected void AICF_HoldCheck(string name, bool passed)
	{
		if (!passed)
			m_iAICFHoldProbeFailures++;
		Print(string.Format("[AICF][TEMPORARY_HOLD_PROBE] case=%1 passed=%2", name, passed));
	}

	override protected bool AssignFactionStrategicOrder(AICF_GroupSlot slot, SCR_CampaignFaction faction,
		string reason, SCR_CampaignMilitaryBaseComponent excludedTarget = null, bool waypointSuspendedByVehicle = false)
	{
		if (slot && slot == m_AICFHoldProbeSlot)
			return false;
		return super.AssignFactionStrategicOrder(slot, faction, reason, excludedTarget, waypointSuspendedByVehicle);
	}

	protected void AICF_HoldChangeAssignment(AIWaypoint waypoint)
	{
		m_AICFHoldProbeSlot.TouchCommanderConfiguration();
	}

	override protected void Update()
	{
		super.Update();
		string enabled;
		if (!System.GetCLIParam("aicfTemporaryHoldProbe", enabled) || enabled != "1" ||
			!m_bRosterReady || m_bStopped || m_bAICFHoldProbeDone)
			return;
		AICF_GroupSlot slot = m_USState.GetSlot(8);
		SCR_AIGroup group = slot.GetGroup();
		IEntity leader = AICF_GroupRuntime.ResolveAliveLeader(group);
		if (!slot.IsCombatReady() || !leader)
			return;
		if (!m_iAICFHoldProbeAt)
		{
			m_AICFHoldProbeSlot = slot;
			m_iAICFHoldProbeAt = System.GetTickCount();
			bool issued = m_OrderPlanner.HoldPositionForTemporaryRouteReplan(slot, m_USFaction, slot.GetTargetBase(), leader.GetOrigin());
			AICF_HoldCheck("INITIAL_HOLD", issued);
			if (!issued)
			{
				GetGame().RequestClose();
				return;
			}
			m_iAICFHoldProbeAssignment = slot.GetStrategicAssignmentRevision();
			m_iAICFHoldProbeGeneration = slot.GetSpawnGeneration();
			AIWaypoint completed = slot.GetWaypoint();
			group.CompleteWaypoint(completed);
			AICF_HoldCheck("COMPLETION_REMOVED", group.GetCurrentWaypoint() != completed);
			ResumeAfterFalseCompletionHold(slot, m_USFaction);
			AICF_HoldCheck("REJECTED_REPLAN_EXECUTABLE", slot.GetWaypoint() && group.GetCurrentWaypoint() == slot.GetWaypoint() && slot.GetWaypoint() != completed);
			m_AICFHoldProbeWaypoint = slot.GetWaypoint();
			return;
		}
		int elapsed = System.GetTickCount(m_iAICFHoldProbeAt);
		if (m_iAICFHoldProbePhase == 0 && elapsed >= 10000)
		{
			int holdAge = slot.GetTemporaryRouteReplanHoldAgeMs();
			int episodeAge = slot.GetRouteRecoveryEpisode().GetAgeMs();
			AIWaypoint removed = slot.GetWaypoint();
			group.RemoveWaypoint(removed);
			ProcessFactionReliability(m_USState, m_USFaction);
			AICF_HoldCheck("RELIABILITY_REPAIRS_REMOVAL", slot.GetWaypoint() && slot.GetWaypoint() != removed && group.GetCurrentWaypoint() == slot.GetWaypoint());
			AICF_HoldCheck("DEADLINES_PRESERVED", slot.GetTemporaryRouteReplanHoldAgeMs() >= holdAge && slot.GetRouteRecoveryEpisode().GetAgeMs() >= episodeAge);
			AICF_HoldCheck("IDENTITY_PRESERVED", slot.GetSpawnGeneration() == m_iAICFHoldProbeGeneration && slot.GetStrategicAssignmentRevision() == m_iAICFHoldProbeAssignment);
			m_AICFHoldProbeWaypoint = slot.GetWaypoint();
			m_iAICFHoldProbePhase = 1;
		}
		if (m_iAICFHoldProbePhase == 1 && elapsed >= 75000)
		{
			SCR_AIGroupUtilityComponent utility = group.GetGroupUtilityComponent();
			SCR_AIDefendActivity defend = SCR_AIDefendActivity.Cast(utility.GetExecutedAction());
			AICF_HoldCheck("NATIVE_HOLD_60S", defend && defend.GetActionState() == EAIActionState.RUNNING && defend.m_RelatedWaypoint == slot.GetWaypoint() && group.GetCurrentWaypoint() == slot.GetWaypoint());
			AICF_HoldCheck("NO_REPAIR_CHURN", slot.GetWaypoint() == m_AICFHoldProbeWaypoint && !slot.HasPendingOrderRecovery() && slot.GetStrategicAssignmentRevision() == m_iAICFHoldProbeAssignment);
			AICF_HoldCheck("NO_TASK_DEADLINE", !slot.HasReportedMeaningfulTaskLoss());
			AIWaypoint before = slot.GetWaypoint();
			group.CompleteWaypoint(before);
			group.GetOnWaypointAdded().Insert(AICF_HoldChangeAssignment);
			ResumeAfterFalseCompletionHold(slot, m_USFaction);
			group.GetOnWaypointAdded().Remove(AICF_HoldChangeAssignment);
			AICF_HoldCheck("STALE_ASSIGNMENT_REJECTED", slot.GetWaypoint() == before && !slot.IsTemporaryRouteReplanHold() && !group.GetCurrentWaypoint());
			bool resumed = m_OrderPlanner.HoldPositionForTemporaryRouteReplan(slot, m_USFaction, slot.GetTargetBase(), leader.GetOrigin());
			AICF_HoldCheck("HOLD_AFTER_CONTEXT_TEST", resumed && slot.GetRouteRecoveryEpisode().GetAgeMs() >= elapsed - 1000);
			m_iAICFHoldProbePhase = 2;
		}
		if (m_iAICFHoldProbePhase == 2 && elapsed >= 185000)
		{
			AICF_HoldCheck("ABSOLUTE_EPISODE_DEADLINE", slot.GetRouteRecoveryEpisode().IsBlocked(slot) && slot.GetRouteRecoveryEpisode().GetAgeMs() >= 180000);
			AICF_HoldCheck("EXHAUSTED_EXECUTABLE_HOLD", slot.IsPersistentStuckFieldHold() && slot.GetWaypoint() && group.GetCurrentWaypoint() == slot.GetWaypoint() && !slot.HasPendingOrderRecovery());
			m_bAICFHoldProbeDone = true;
			Print(string.Format("[AICF][TEMPORARY_HOLD_PROBE_DONE] failures=%1", m_iAICFHoldProbeFailures));
			GetGame().RequestClose();
		}
	}
}
