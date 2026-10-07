// Только в изолированном stage. Моделирует отказ обычного коридора, оставляя
// production deadline, геометрию, оплату, завершение и пополнение без подмены.
modded class AICF_ConstructionPlanner
{
	protected bool m_bAICFForcedContractsRun;

	protected bool AICF_ForcedProbeEnabled()
	{
		string value;
		return System.GetCLIParam("aicfForcedSmallProbe", value) && value == "1";
	}

	override protected bool AccessClear(AICF_ConstructionOrder order)
	{
		if (AICF_ForcedProbeEnabled() && order.m_eType == AICF_EConstructionType.SMALL_BARRACKS && !order.m_bForcedSmallBarracks)
		{
			order.m_sReason = "PROBE_NORMAL_ACCESS_BLOCKED";
			return false;
		}
		return super.AccessClear(order);
	}

	override void Update()
	{
		if (AICF_ForcedProbeEnabled() && !m_bAICFForcedContractsRun)
		{
			m_bAICFForcedContractsRun = true;
			int passed;
			AICF_ConstructionOrder order = new AICF_ConstructionOrder();
			order.m_eType = AICF_EConstructionType.SMALL_BARRACKS;
			order.m_iStartedAt = 1000;
			if (!AICF_SmallBarracksFallback.IsDue(order, 120999)) passed++;
			if (AICF_SmallBarracksFallback.IsDue(order, 121000)) passed++;
			order.m_bAccepted = true;
			if (!AICF_SmallBarracksFallback.IsDue(order, 121000)) passed++;
			order.m_bAccepted = false;
			order.m_bCancelled = true;
			if (!AICF_SmallBarracksFallback.IsDue(order, 121000)) passed++;
			order.m_bCancelled = false;
			order.m_bCommitStarted = true;
			if (!AICF_SmallBarracksFallback.IsDue(order, 121000)) passed++;
			order.m_bCommitStarted = false;
			order.m_eType = AICF_EConstructionType.LARGE_BARRACKS;
			if (!AICF_SmallBarracksFallback.IsDue(order, 121000)) passed++;
			order.m_eType = AICF_EConstructionType.SMALL_BARRACKS;
			AICF_SmallBarracksFallback.Activate(order, 121000, 256);
			if (!order.m_bForcedSmallBarracks) passed++; // Нет живой identity.
			Print(string.Format("[AICF][FORCED_SMALL_CONTRACTS] passed=%1 total=7", passed));
			if (passed != 7)
				Print("[AICF][FORCED_SMALL_CONTRACTS] FAILED", LogLevel.ERROR);
		}
		super.Update();
	}
}
