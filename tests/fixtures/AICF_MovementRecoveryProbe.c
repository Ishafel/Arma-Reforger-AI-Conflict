// Только isolated stage. Native ошибка инъецируется на существующем Move activity.
modded class AICF_InfantryApproachRoute
{
	int ProbeTileWaitAge()
	{
		return System.GetTickCount(m_iTileWaitStartedAtMs);
	}
}

modded class AICF_OrderPlanner
{
	override bool RebuildCurrentOrder(AICF_GroupSlot slot, SCR_CampaignFaction faction, string reason, bool recoverStuckRoute = false)
	{
		int waitAge;
		int intent;
		SCR_AIGroup group;
		if (reason == "INFANTRY_APPROACH_TILE_READY")
		{
			waitAge = slot.GetApproachRoute(slot.GetTargetBase()).ProbeTileWaitAge();
			intent = slot.GetStrategicIntentRevision();
			group = slot.GetGroup();
		}
		bool result = super.RebuildCurrentOrder(slot, faction, reason, recoverStuckRoute);
		if (reason == "INFANTRY_APPROACH_TILE_READY")
			Print(string.Format("[AICF][TILE_RESUME_PROBE] faction=%1 numeric_slot=%2 success=%3 wait_ms=%4 intent_preserved=%5 group_preserved=%6",
				faction.GetFactionKey(), slot.GetSlotId(), result, waitAge, intent == slot.GetStrategicIntentRevision(), group == slot.GetGroup()));
		return result;
	}
}

modded class AICF_FIAPatrolService
{
	protected int m_iProbeStart;
	protected int m_iProbeSample;
	protected int m_iProbeStep;
	protected int m_iProbeChecks;
	protected int m_iProbeFailures;
	protected EntityID m_ProbeOldWaypoint;

	protected void ProbeCheck(string name, bool passed)
	{
		m_iProbeChecks++;
		if (!passed) m_iProbeFailures++;
		Print(string.Format("[AICF][MOVEMENT_PROBE_CHECK] name=%1 passed=%2", name, passed));
	}

	override void Update(bool graphReady)
	{
		super.Update(graphReady);
		string mode;
		if (!Replication.IsServer() || !System.GetCLIParam("aicfMovementRecoveryProbe", mode)) return;
		int now = System.GetTickCount();
		if (!m_iProbeStart) m_iProbeStart = now;
		if (now >= m_iProbeSample)
		{
			m_iProbeSample = now + 10000;
			foreach (AICF_FIAPatrol sample : m_aPatrols)
			{
				if (sample.m_Vehicle) sample.Log("FIA_PATROL_SAMPLE", string.Format("ready=%1 seated=%2 retired=%3 position=%4 distance_m=%5 retries=%6",
					sample.m_bReady, sample.Seated(), sample.m_bRetired, sample.m_Vehicle.GetOrigin(), vector.DistanceXZ(sample.m_Vehicle.GetOrigin(), sample.m_vEndpoint), sample.m_iRouteRetries));
			}
		}
		if (mode == "1" && !m_aPatrols.IsEmpty()) ProbeFailure(m_aPatrols[0], graphReady);
		int duration = 480000;
		if (mode == "1") duration = 360000;
		if (now - m_iProbeStart >= duration)
		{
			if (mode == "1") ProbeCheck("BOUNDED_LIFECYCLE_FINISHED", m_iProbeStep == 5);
			Stop();
			Print(string.Format("[AICF][MOVEMENT_PROBE_FINISHED] mode=%1 checks=%2 failures=%3 step=%4", mode, m_iProbeChecks, m_iProbeFailures, m_iProbeStep));
			GetGame().RequestClose();
		}
	}

	protected void ProbeFailure(AICF_FIAPatrol p, bool graphReady)
	{
		if (m_iProbeStep == 5 || !graphReady) return;
		if (p.m_bRetired)
		{
			ProbeCheck("RETIRED_ENTITIES_RETAINED", m_iProbeStep == 4 && !p.m_Waypoint && p.GroupIdentity() && p.VehicleIdentity() && p.CrewIdentity());
			ProbeCheck("RETIRED_OWNER_REJECTED", !ReportFailedMovement(p.m_Group, p.m_Waypoint));
			m_iProbeStep = 5;
			return;
		}
		if (!p.m_bReady || !p.Seated() || !p.m_Waypoint || p.m_Group.GetCurrentWaypoint() != p.m_Waypoint) return;
		SCR_AIGroupUtilityComponent utility = p.m_Group.GetGroupUtilityComponent();
		SCR_AIActivityBase activity = SCR_AIActivityBase.Cast(utility.GetCurrentAction());
		if (!activity || activity.m_RelatedWaypoint != p.m_Waypoint) return;
		if (m_iProbeStep == 0)
		{
			AICF_FIAPatrolMoveFailure token = new AICF_FIAPatrolMoveFailure(p);
			ProbeCheck("CURRENT_TOKEN", token.IsCurrent(p));
			p.m_iGeneration++;
			ProbeCheck("STALE_GENERATION", !token.IsCurrent(p));
			p.m_iGeneration--;
			p.m_iLeg++;
			ProbeCheck("STALE_LEG", !token.IsCurrent(p));
			p.m_iLeg--;
			p.m_iGraphRevision++;
			ProbeCheck("STALE_GRAPH", !token.IsCurrent(p) && !ReportFailedMovement(p.m_Group, p.m_Waypoint));
			p.m_iGraphRevision--;
			ProbeCheck("FOREIGN_VEHICLE", !ReportFailedMovement(p.m_Group, p.m_Waypoint, p.m_Driver));
			ProbeCheck("NOT_WAYPOINT_RELATED", !AICF_FIAPatrolMovementPolicy.Handle(p.m_Group, utility, EMoveError.UNKNOWN, -1, false, p.m_vEndpoint));
			ProbeCheck("OTHER_MOVE_RESULT", !AICF_FIAPatrolMovementPolicy.Handle(p.m_Group, utility, EMoveError.STOPPED, -1, true, p.m_vEndpoint));
			ProbeCheck("UNKNOWN_HANDLER", !AICF_FIAPatrolMovementPolicy.Handle(p.m_Group, utility, EMoveError.UNKNOWN, 999999, true, p.m_vEndpoint));
			ProbeCheck("REPORT_CURRENT", ReportFailedMovement(p.m_Group, p.m_Waypoint) == p);
			m_ProbeOldWaypoint = p.m_WaypointId;
			m_Handoff.MoveFIAPatrol(p, p.m_vEndpoint);
			ProbeCheck("STALE_WAYPOINT", !token.IsCurrent(p));
			m_iProbeStep = 1;
			return;
		}
		if (p.m_WaypointId == m_ProbeOldWaypoint || p.m_MoveFailure) return;
		if (m_iProbeStep == 1) ProbeCheck("STALE_CALLBACK_NO_RETRY", p.m_iRouteRetries == 0);
		m_ProbeOldWaypoint = p.m_WaypointId;
		bool handled = AICF_FIAPatrolMovementPolicy.Handle(p.m_Group, utility, EMoveError.UNKNOWN, -1, true, p.m_vEndpoint);
		ProbeCheck(string.Format("NATIVE_UNKNOWN_HANDLED_%1", m_iProbeStep), handled && p.m_MoveFailure);
		m_iProbeStep++;
	}
}
