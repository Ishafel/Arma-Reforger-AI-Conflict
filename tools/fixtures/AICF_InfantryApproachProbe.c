// Только изолированный stage: геометрия production + реальные приказы и движение.
modded class AICF_GroupSlot
{
	// Синхронные fault injections; все поля восстанавливаются до возврата в engine.
	bool AICF_ProbeApproachCompletion(AICF_OrderPlanner planner, SCR_CampaignFaction faction)
	{
		AIWaypoint waypoint = GetWaypoint();
		IEntity leader = AICF_GroupRuntime.ResolveAliveLeader(GetGroup());
		if (!waypoint || !leader || !IsApproachRouteWaypoint())
			return false;
		vector oldPosition = waypoint.GetOrigin();
		AIWaypoint oldTerminal = m_OwnedWaypointTerminalWaypoint;
		string oldOutcome = m_sOwnedWaypointTerminalOutcome;
		int oldGeneration = m_iOwnedWaypointTerminalGeneration;
		m_OwnedWaypointTerminalWaypoint = waypoint;
		m_iOwnedWaypointTerminalGeneration = m_iSpawnGeneration;
		waypoint.SetOrigin(leader.GetOrigin());
		m_sOwnedWaypointTerminalOutcome = "GROUP_CALLBACK_REMOVED";
		bool removedRejected = !planner.HasCompletedApproachLeg(this, faction);
		m_sOwnedWaypointTerminalOutcome = "GROUP_CALLBACK_COMPLETED";
		bool nearAccepted = planner.HasCompletedApproachLeg(this, faction);
		waypoint.SetOrigin(leader.GetOrigin() + "100 0 0");
		bool farRejected = !planner.HasCompletedApproachLeg(this, faction);
		waypoint.SetOrigin(leader.GetOrigin());
		m_iSpawnGeneration++;
		bool generationRejected = !planner.HasCompletedApproachLeg(this, faction);
		m_iSpawnGeneration--;
		m_iStrategicAssignmentRevision++;
		bool revisionRejected = !planner.HasCompletedApproachLeg(this, faction);
		m_iStrategicAssignmentRevision--;
		waypoint.SetOrigin(oldPosition);
		m_OwnedWaypointTerminalWaypoint = oldTerminal;
		m_sOwnedWaypointTerminalOutcome = oldOutcome;
		m_iOwnedWaypointTerminalGeneration = oldGeneration;
		Print(string.Format("[AICF][APPROACH_COMPLETION_FENCES] removed=%1 near=%2 far=%3 generation=%4 revision=%5", removedRejected, nearAccepted, farRejected, generationRejected, revisionRejected));
		return removedRejected && nearAccepted && farRejected && generationRejected && revisionRejected;
	}
}

modded class AICF_OrderPlanner
{
	bool m_bAICFApproachLegAdvanced;

	override bool RebuildCurrentOrder(AICF_GroupSlot slot, SCR_CampaignFaction faction,
		string reason, bool recoverStuckRoute = false)
	{
		bool wasTravelLeg = slot && slot.IsApproachRouteWaypoint() && slot.GetSlotId() < 2 &&
			faction && faction.GetFactionKey() == "US" &&
			!slot.GetApproachRoute(slot.GetTargetBase()).IsWaitingForTile();
		bool result = super.RebuildCurrentOrder(slot, faction, reason, recoverStuckRoute);
		if (result && wasTravelLeg && (reason == "INFANTRY_APPROACH_LEG_ARRIVED" || reason == "INFANTRY_APPROACH_COMPLETION_TIMEOUT"))
			m_bAICFApproachLegAdvanced = true;
		return result;
	}
}

modded class AICF_MatchController
{
	protected bool m_bAICFApproachProbeStarted;
	protected int m_iAICFApproachProbePolls;
	protected int m_iAICFApproachChecks;
	protected int m_iAICFApproachFailures;
	protected SCR_CampaignMilitaryBaseComponent m_AICFApproachTarget;
	protected ref array<vector> m_aAICFApproachStarts = {};
	protected ref array<EntityID> m_aAICFApproachWaypoints = {};
	protected bool m_bAICFApproachOrdersChecked;
	protected bool m_bAICFApproachAdvanced;

	protected void AICF_ApproachCheck(string name, bool passed)
	{
		m_iAICFApproachChecks++;
		if (!passed)
			m_iAICFApproachFailures++;
		Print(string.Format("[AICF][APPROACH_PROBE_CHECK] name=%1 passed=%2", name, passed));
	}

	override protected void TryLogRosterReady()
	{
		super.TryLogRosterReady();
		string enabled;
		if (!m_bRosterReady || m_bAICFApproachProbeStarted ||
			!System.GetCLIParam("aicfApproachProbe", enabled) || enabled != "1")
			return;
		m_bAICFApproachProbeStarted = true;
		for (int i; i < m_USState.GetSlotCount(); i++)
		{
			m_USState.GetSlot(i).SetDesiredSize(1);
			m_USSRState.GetSlot(i).SetDesiredSize(1);
		}
		GetGame().GetCallqueue().CallLater(AICF_StartApproachProbe, 1, false);
	}

	protected void AICF_StartApproachProbe()
	{
		m_AICFApproachTarget = m_USState.GetSlot(0).GetStrategicIntentTargetBase();
		if (!m_AICFApproachTarget)
			m_AICFApproachTarget = m_USState.GetSlot(0).GetTargetBase();
		if (!m_AICFApproachTarget)
		{
			string selectionMode;
			m_AICFApproachTarget = m_TargetSelector.SelectAttackTarget(m_ObjectiveGraph, m_USFaction, selectionMode);
		}
		AICF_ApproachCheck("TARGET", m_AICFApproachTarget && m_AICFApproachTarget.GetType() != SCR_ECampaignBaseType.RELAY);
		if (!m_AICFApproachTarget)
		{
			AICF_FinishApproachProbe();
			return;
		}
		AICF_InfantryApproachRoute route = new AICF_InfantryApproachRoute(m_AICFApproachTarget, "0 0 0");
		array<vector> candidates = {};
		bool separated = true;
		for (int slotId; slotId < 10; slotId++)
		{
			vector candidate;
			bool built = route.BuildCandidate("0 0 0", "1000 0 0", slotId, candidate);
			AICF_ApproachCheck(string.Format("LANE_%1", slotId), built && Math.AbsFloat(candidate[0] - 220) < 0.1);
			foreach (vector previous : candidates)
			{
				if (vector.DistanceXZ(candidate, previous) < 39.9)
					separated = false;
			}
			candidates.Insert(candidate);
		}
		AICF_ApproachCheck("TEN_DISTINCT_LANES", separated);
		vector second;
		AICF_ApproachCheck("ADVANCE_SAME_LANE", route.BuildCandidate(candidates[0], "1000 0 0", 0, second) &&
			Math.AbsFloat(second[0] - 440) < 0.1 && Math.AbsFloat(second[2] - candidates[0][2]) < 0.1);
		AICF_ApproachCheck("FINAL_APPROACH", !route.BuildCandidate("800 0 0", "1000 0 0", 0, second));
		AICF_ApproachCheck("ROTATED_AXIS", route.BuildCandidate("0 0 0", "0 0 1000", 0, second) &&
			Math.AbsFloat(second[2] - 220) < 0.1 && Math.AbsFloat(second[0] - 40) < 0.1);
		route.Disable();
		AICF_ApproachCheck("RECOVERY_DISABLE", !route.BuildCandidate("0 0 0", "1000 0 0", 0, second));
		for (int i; i < 2; i++)
		{
			AICF_GroupSlot slot = m_USState.GetSlot(i);
			IEntity leader = AICF_GroupRuntime.ResolveAliveLeader(slot.GetGroup());
			m_aAICFApproachStarts.Insert(leader.GetOrigin());
			SupersedePendingOrderRecovery(slot, m_USFaction, "APPROACH_PROBE_PLAYER_ORDER");
			m_OrderPlanner.AssignPlayerOrder(slot, m_USFaction, m_AICFApproachTarget);
			m_aAICFApproachWaypoints.Insert(EntityID.INVALID);
		}
		GetGame().GetCallqueue().CallLater(AICF_PollApproachProbe, 5000, true);
	}

	protected void AICF_PollApproachProbe()
	{
		m_iAICFApproachProbePolls++;
		AICF_GroupSlot first = m_USState.GetSlot(0);
		AICF_GroupSlot second = m_USState.GetSlot(1);
		if (!m_bAICFApproachOrdersChecked)
		{
			if (!first.HasPlayerStrategicIntent())
				m_OrderPlanner.AssignPlayerOrder(first, m_USFaction, m_AICFApproachTarget);
			if (!second.HasPlayerStrategicIntent())
				m_OrderPlanner.AssignPlayerOrder(second, m_USFaction, m_AICFApproachTarget);
			if (first.IsApproachRouteWaypoint() && second.IsApproachRouteWaypoint() &&
				!first.GetApproachRoute(m_AICFApproachTarget).IsWaitingForTile() &&
				!second.GetApproachRoute(m_AICFApproachTarget).IsWaitingForTile())
			{
				m_bAICFApproachOrdersChecked = true;
				AICF_ApproachCheck("COMPLETION_FENCES", first.AICF_ProbeApproachCompletion(m_OrderPlanner, m_USFaction));
				m_OrderPlanner.m_bAICFApproachLegAdvanced = false;
				AICF_ApproachCheck("REAL_DISTINCT_ENDPOINTS", vector.DistanceXZ(first.GetWaypoint().GetOrigin(), second.GetWaypoint().GetOrigin()) > 20);
				AICF_ApproachCheck("SHARED_BASE_PRESERVED", first.GetTargetBase() == m_AICFApproachTarget && second.GetTargetBase() == m_AICFApproachTarget);
				AICF_ApproachCheck("CURRENT_IDENTITY", first.IsStuckRouteContextCurrent() && second.IsStuckRouteContextCurrent());
				m_aAICFApproachWaypoints[0] = first.GetWaypoint().GetID();
				m_aAICFApproachWaypoints[1] = second.GetWaypoint().GetID();
			}
		}
		if (m_bAICFApproachOrdersChecked)
		{
			for (int i; i < 2; i++)
			{
				AICF_GroupSlot slot = m_USState.GetSlot(i);
				if (slot.IsApproachRouteWaypoint() && slot.GetWaypoint().GetID() != m_aAICFApproachWaypoints[i])
					m_bAICFApproachAdvanced = true;
			}
		}
		if (m_iAICFApproachProbePolls >= 36)
		{
			AICF_ApproachCheck("ROUTES_ASSIGNED", m_bAICFApproachOrdersChecked);
			AICF_ApproachCheck("NEXT_LEG_OBSERVED", m_bAICFApproachAdvanced && m_OrderPlanner.m_bAICFApproachLegAdvanced);
			for (int j; j < 2; j++)
			{
				IEntity leader = AICF_GroupRuntime.ResolveAliveLeader(m_USState.GetSlot(j).GetGroup());
				AICF_ApproachCheck(string.Format("REAL_MOVEMENT_%1", j), leader && vector.DistanceXZ(leader.GetOrigin(), m_aAICFApproachStarts[j]) > 30);
			}
			AICF_FinishApproachProbe();
		}
	}

	protected void AICF_FinishApproachProbe()
	{
		Print(string.Format("[AICF][APPROACH_PROBE_FINISHED] checks=%1 failures=%2", m_iAICFApproachChecks, m_iAICFApproachFailures));
		GetGame().GetCallqueue().Remove(AICF_PollApproachProbe);
		GetGame().RequestClose();
	}

	override protected void Stop(bool cleanupEntities)
	{
		if (GetGame())
		{
			GetGame().GetCallqueue().Remove(AICF_StartApproachProbe);
			GetGame().GetCallqueue().Remove(AICF_PollApproachProbe);
		}
		super.Stop(cleanupEntities);
	}
}
