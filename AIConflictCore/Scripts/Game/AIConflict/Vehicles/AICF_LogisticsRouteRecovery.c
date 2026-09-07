// Синхронный state одного leg: waypoint не является физическим progress.
class AICF_LogisticsRouteRecovery
{
	ref AICF_LogisticsDriverInteraction m_Context;
	vector m_vRoute;
	vector m_vAttemptPosition;
	vector m_vDiagnosticPosition;
	float m_fAttemptDistance;
	bool m_bIntermediate;
	bool m_bActive;
	int m_iStartedAtMs;
	int m_iAttemptAtMs;
	int m_iWaitAtStartMs;
	int m_iWaitAtAttemptMs;
	int m_iNextDiagnosticMs;

	void Begin(AICF_LogisticsWorker w, int now)
	{
		m_Context = new AICF_LogisticsDriverInteraction();
		m_Context.Capture(w, now);
		m_vRoute = w.m_vEndpoint;
		m_vDiagnosticPosition = w.m_Vehicle.GetOrigin();
		vector initialRoad;
		if (!w.m_bExitedSpawn && vector.DistanceXZ(w.m_Vehicle.GetOrigin(), w.m_vEndpoint) > 30 && RoadEndpoint(w, 0, initialRoad))
		{
			m_vRoute = initialRoad;
			m_bIntermediate = true;
		}
	}

	// Закреплённый RoadNetworkManager разрешает curved reachable road query.
	// Луч/свободный прямой коридор здесь не используется как admission veto.
	bool RoadEndpoint(AICF_LogisticsWorker w, int alternative, out vector endpoint)
	{
		SCR_AIWorld ai = SCR_AIWorld.Cast(GetGame().GetAIWorld());
		if (!ai || !ai.GetRoadNetworkManager() || !w.VehicleIdentity()) return false;
		vector origin = w.m_Vehicle.GetOrigin();
		vector direction = w.m_vEndpoint - origin;
		direction[1] = 0;
		if (direction.Length() < 1) return false;
		direction.Normalize();
		vector side = Vector(direction[2], 0, -direction[0]);
		vector goal = origin + direction * 45;
		if (alternative == 1) goal = origin + direction * 20 + side * 45;
		if (alternative >= 2) goal = origin + direction * 20 - side * 45;
		if (!ai.GetRoadNetworkManager().GetReachableWaypointInRoad(origin, goal, 30, endpoint)) return false;
		return vector.DistanceXZ(endpoint, goal) <= 30 && vector.DistanceXZ(endpoint, origin) >= 12 &&
			vector.DistanceXZ(endpoint, origin) <= 90 && (alternative == 0 || vector.DistanceXZ(endpoint, m_vRoute) >= 10);
	}

	bool CanRenew(AICF_LogisticsWorker w, int now)
	{
		return m_bActive && m_Context && m_Context.Matches(w) && w.Ready() && !w.HasForeignOccupant() &&
			RecoveryAge(w, now) < AICF_LogisticsConfig.RECOVERY_BUDGET_MS && w.LegAgeMs(now) < AICF_LogisticsConfig.LEG_TIMEOUT_MS;
	}

	int RecoveryAge(AICF_LogisticsWorker w, int now)
	{
		return now - m_iStartedAtMs - (w.m_iDriverWaitMs - m_iWaitAtStartMs);
	}

	AICF_TripOutcome Poll(AICF_LogisticsWorker w, int now)
	{
		if (!m_Context || !m_Context.Matches(w)) return AICF_TripOutcome.TerminalFailClosed("ROUTE_RECOVERY_IDENTITY_CHANGED", "LOGISTICS");
		if (m_bActive && RecoveryAge(w, now) >= AICF_LogisticsConfig.RECOVERY_BUDGET_MS)
			return AICF_TripOutcome.TerminalFailClosed("BOUNDED_ROUTE_RECOVERY_EXHAUSTED", m_Context.m_sToken);
		vector position = w.m_Vehicle.GetOrigin();
		if (vector.DistanceXZ(position, w.m_aSpawnTransform[3]) > 90) w.m_bExitedSpawn = true;
		if (now >= m_iNextDiagnosticMs)
		{
			m_iNextDiagnosticMs = now + 10000;
			AICF_LogisticsDriverInteraction.Diagnose(w, "MOTION_SAMPLE", now);
			m_vDiagnosticPosition = position;
		}
		if (m_bActive && vector.DistanceXZ(position, m_vAttemptPosition) >= 6 &&
			vector.DistanceXZ(position, m_vRoute) + 3 <= m_fAttemptDistance)
		{
			m_bActive = false;
			w.Log("LOGISTICS_RECOVERY_SUCCEEDED", string.Format("reason=PHYSICAL_MOTION_AND_ROUTE_PROGRESS attempt=%1 displacement_m=%2 route_endpoint=%3 route_progress_m=%4", w.m_iRouteRetries, vector.DistanceXZ(position, m_vAttemptPosition), m_vRoute, m_fAttemptDistance - vector.DistanceXZ(position, m_vRoute)));
		}
		if (m_bIntermediate && !m_bActive && vector.DistanceXZ(position, m_vRoute) <= 8)
		{
			m_bIntermediate = false;
			m_vRoute = w.m_vEndpoint;
			return AICF_TripOutcome.Retry("INTERMEDIATE_REACHED_CONTINUE_LEG", m_Context.m_sToken, now);
		}
		if (w.ProgressAgeMs(now) < AICF_LogisticsConfig.MOTION_STALL_MS) return AICF_TripOutcome.Wait("PHYSICAL_PROGRESS", m_Context.m_sToken);
		if (m_bActive && now - m_iAttemptAtMs - (w.m_iDriverWaitMs - m_iWaitAtAttemptMs) < AICF_LogisticsConfig.RECOVERY_ATTEMPT_MS)
			return AICF_TripOutcome.Wait("RECOVERY_OBSERVING_MOTION", m_Context.m_sToken);
		if (w.m_iRouteRetries >= AICF_LogisticsConfig.MAX_ROUTE_RETRIES)
			return AICF_TripOutcome.TerminalFailClosed("BOUNDED_ROUTE_RECOVERY_EXHAUSTED", m_Context.m_sToken);
		if (!m_iStartedAtMs)
		{
			m_iStartedAtMs = now;
			m_iWaitAtStartMs = w.m_iDriverWaitMs;
		}
		w.m_iRouteRetries++;
		m_bActive = true;
		m_iAttemptAtMs = now;
		m_iWaitAtAttemptMs = w.m_iDriverWaitMs;
		// Сначала новый native path к тому же endpoint; затем иной reachable выезд.
		if (w.m_iRouteRetries == 2)
		{
			vector alternate;
			if (RoadEndpoint(w, 1, alternate) || RoadEndpoint(w, 2, alternate))
			{
				m_vRoute = alternate;
				m_bIntermediate = true;
			}
		}
		m_vAttemptPosition = position;
		m_fAttemptDistance = vector.DistanceXZ(position, m_vRoute);
		w.Log("LOGISTICS_RECOVERY_ATTEMPT", string.Format("reason=NO_PHYSICAL_PROGRESS attempt=%1 route_endpoint=%2 position=%3 progress_age_ms=%4 elapsed_ms=%5", w.m_iRouteRetries, m_vRoute, position, w.ProgressAgeMs(now), now - m_iStartedAtMs));
		return AICF_TripOutcome.Retry("REBUILD_LOGISTICS_ROUTE", m_Context.m_sToken, now);
	}
}
