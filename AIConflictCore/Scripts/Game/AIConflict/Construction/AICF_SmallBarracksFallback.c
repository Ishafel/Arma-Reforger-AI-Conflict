// Обязательная малая казарма после двух минут без площадки. Геометрия,
// identity и оплата остаются у search/adapter; маршрут строителя не требуется.
class AICF_SmallBarracksFallback
{
	static const int DELAY_MS = 120000;

	static bool IsDue(AICF_ConstructionOrder order, int now)
	{
		return order && order.m_eType == AICF_EConstructionType.SMALL_BARRACKS &&
			!order.m_bAccepted && !order.m_bCancelled && !order.m_bCommitStarted &&
			now - order.m_iStartedAt >= DELAY_MS;
	}

	static void Activate(AICF_ConstructionOrder order, int now, int attempts)
	{
		if (!Replication.IsServer() || !IsDue(order, now) || order.m_bForcedSmallBarracks ||
			!order.IdentityValid() || order.m_bSiteReserved || order.m_iStage == 4)
			return;
		order.LogSearch();
		order.m_bForcedSmallBarracks = true;
		AICF_ConstructionSiteSearch.ReleaseClaim(order.m_sToken);
		order.m_aPendingCandidates.Clear();
		order.m_Path = null;
		order.m_Navigation = new AICF_ConstructionNavigation();
		order.m_aPathStarts.Clear();
		// Повторяем и ранее отвергнутые коридором/маршрутом участки.
		order.m_iSearchOffset = 0;
		order.m_iAttempts = 0;
		order.m_iAttemptLimit = attempts;
		order.m_iStage = -1;
		if (order.m_Metadata && order.m_Metadata.m_bGeometryLoaded)
			order.m_iStage = 0;
		order.m_iDeadline = now + DELAY_MS;
		order.m_iResumeAt = 0;
		order.m_sReason = "SMALL_BARRACKS_TIMEOUT";
		order.Log("CONSTRUCTION_FORCED_SMALL_STARTED", "delay_ms=120000 worker_path_required=0 access_corridor_required=0 physical_clearance_required=1");
	}
}
