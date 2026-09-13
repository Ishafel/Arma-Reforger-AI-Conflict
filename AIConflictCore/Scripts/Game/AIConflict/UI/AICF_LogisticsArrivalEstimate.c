// Только представление текущего плеча. Дорожный API не предоставляет здесь
// длину пути: запас на объезд не превращает расстояние по прямой в road ETA.
// Нет таймера обратного отсчёта или кеша, способного пережить смену job/машины.
class AICF_LogisticsArrivalEstimate
{
	static string TravelTime(float distance, float speed)
	{
		if (!(distance >= 0 && distance <= 1000000 && speed >= 0 && speed <= 150))
			return "время уточняется";
		if (speed < 0.5) return "время уточняется — машина стоит";
		float seconds = distance * 1.3 / speed;
		if (seconds < 60) return "≈ меньше минуты";
		int minutes = Math.Ceil(seconds / 60);
		return string.Format("≈ %1 мин", minutes);
	}

	static string Text(AICF_LogisticsWorker w)
	{
		if (!w || w.m_bStopped || w.m_bCargoFault || w.m_bCleanupQueued ||
			w.m_ePhase == AICF_ELogisticsPhase.RETIRING || w.m_ePhase == AICF_ELogisticsPhase.FAILED_CLOSED)
			return string.Empty;
		if (w.m_ePhase == AICF_ELogisticsPhase.SPAWN_PENDING)
			return "Время прибытия: после подготовки машины";
		bool returning = w.m_ePhase == AICF_ELogisticsPhase.RETURN_HOME;
		if (!returning && (!w.m_Job || w.m_Job.m_bCancelled || w.m_Job.m_iGeneration != w.m_iGeneration))
			return string.Empty;
		if (!w.m_Faction || !w.VehicleIdentity()) return "Время прибытия: данные недоступны";
		if (w.m_DriverInteraction || (w.m_RouteRecovery && w.m_RouteRecovery.m_bActive))
			return "Время прибытия уточняется — восстановление движения";
		if (w.m_ePhase == AICF_ELogisticsPhase.LOADING) return "У источника — погрузка";
		if (w.m_ePhase == AICF_ELogisticsPhase.UNLOADING) return "У получателя — разгрузка";
		string target;
		switch (w.m_ePhase)
		{
			case AICF_ELogisticsPhase.TO_SOURCE: target = "До источника"; break;
			case AICF_ELogisticsPhase.TO_DESTINATION: target = "До получателя"; break;
			case AICF_ELogisticsPhase.RETURN_HOME: target = "До базы приписки"; break;
			default: return string.Empty;
		}
		Physics physics = w.m_Vehicle.GetPhysics();
		if (!physics) return target + ": время уточняется";
		vector velocity = physics.GetVelocity();
		velocity[1] = 0;
		return target + ": " + TravelTime(vector.DistanceXZ(w.m_Vehicle.GetOrigin(), w.m_vEndpoint), velocity.Length());
	}
}
