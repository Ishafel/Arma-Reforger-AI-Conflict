// Только представление текущего плеча. Дорожный API не предоставляет здесь
// длину пути: запас на объезд не превращает расстояние по прямой в road ETA.
// Нет таймера обратного отсчёта или кеша, способного пережить смену job/машины.
class AICF_LogisticsArrivalEstimate
{
	static string TravelTime(float distance, float speed)
	{
		if (!(distance >= 0 && distance <= 1000000 && speed >= 0 && speed <= 150))
			return "{AICF:AICF_UI_estimating_arrival_118e10d0}";
		if (speed < 0.5) return "{AICF:AICF_UI_estimating_arrival_vehicle_stopped_5ece1f0c}";
		float seconds = distance * 1.3 / speed;
		if (seconds < 60) return "{AICF:AICF_UI_under_a_minute_6ffb7fab}";
		int minutes = Math.Ceil(seconds / 60);
		return AICF_Localization.Format("{AICF:AICF_UI_min_de0bd758}", string.Format("%1", minutes));
	}

	static string Text(AICF_LogisticsWorker w)
	{
		if (!w || w.m_bStopped || w.m_bCargoFault || w.m_bCleanupQueued ||
			w.m_ePhase == AICF_ELogisticsPhase.RETIRING || w.m_ePhase == AICF_ELogisticsPhase.FAILED_CLOSED)
			return string.Empty;
		if (w.m_ePhase == AICF_ELogisticsPhase.SPAWN_PENDING)
			return "{AICF:AICF_UI_Arrival_estimate_after_vehicle_preparation_9ecf3d6f}";
		bool returning = w.m_ePhase == AICF_ELogisticsPhase.RETURN_HOME;
		if (!returning && (!w.m_Job || w.m_Job.m_bCancelled || w.m_Job.m_iGeneration != w.m_iGeneration))
			return string.Empty;
		if (!w.m_Faction || !w.VehicleIdentity()) return "{AICF:AICF_UI_Arrival_estimate_data_unavailable_dc6722a2}";
		if (w.m_DriverInteraction || (w.m_RouteRecovery && w.m_RouteRecovery.m_bActive))
			return "{AICF:AICF_UI_Arrival_estimate_pending_recovering_moveme_1bf3152e}";
		if (w.m_ePhase == AICF_ELogisticsPhase.LOADING) return "{AICF:AICF_UI_At_source_loading_9c33f5c9}";
		if (w.m_ePhase == AICF_ELogisticsPhase.UNLOADING) return "{AICF:AICF_UI_At_destination_unloading_bca7970f}";
		string target;
		switch (w.m_ePhase)
		{
			case AICF_ELogisticsPhase.TO_SOURCE: target = "{AICF:AICF_UI_To_source_cccd21b8}"; break;
			case AICF_ELogisticsPhase.TO_DESTINATION: target = "{AICF:AICF_UI_To_destination_5eafb48e}"; break;
			case AICF_ELogisticsPhase.RETURN_HOME: target = "{AICF:AICF_UI_To_home_base_bf0d4d69}"; break;
			default: return string.Empty;
		}
		Physics physics = w.m_Vehicle.GetPhysics();
		if (!physics) return target + "{AICF:AICF_UI_estimating_arrival_1a909b3e}";
		vector velocity = physics.GetVelocity();
		velocity[1] = 0;
		return target + ": " + TravelTime(vector.DistanceXZ(w.m_Vehicle.GetOrigin(), w.m_vEndpoint), velocity.Length());
	}
}
