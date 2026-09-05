// Отдельная частота строительства; infantry commander и worker сохраняют cadence.
class AICF_ConstructionConfig
{
	int m_iDecisionMs = 60000;
	int m_iCooldownMs = 60000;
	int m_iDeadlineMs = 120000;
	int m_iCandidatesPerTick = 4;
	int m_iMetadataEntriesPerTick = 24;
	int m_iSliceMs = 8;
	int m_iQueriesPerTick = 96;
	int m_iAttempts = 96;
	int m_iReserveGroups = 1;
	float m_fMargin = 1;
	float m_fHeightDelta = 0.8;
	float m_fTerrainStep = 3;

	void AICF_ConstructionConfig()
	{
		string value;
		if (System.GetCLIParam("aicfConstructionDecisionMs", value))
			m_iDecisionMs = Math.ClampInt(value.ToInt(), 60000, 3600000);
		if (System.GetCLIParam("aicfConstructionCooldownMs", value))
			m_iCooldownMs = Math.ClampInt(value.ToInt(), 60000, 3600000);
		if (System.GetCLIParam("aicfConstructionDeadlineMs", value))
			m_iDeadlineMs = Math.ClampInt(value.ToInt(), 1000, 120000);
		if (System.GetCLIParam("aicfConstructionCandidatesPerTick", value))
			m_iCandidatesPerTick = Math.ClampInt(value.ToInt(), 1, 4);
		if (System.GetCLIParam("aicfConstructionMetadataEntriesPerTick", value))
			m_iMetadataEntriesPerTick = Math.ClampInt(value.ToInt(), 1, 32);
		if (System.GetCLIParam("aicfConstructionSliceMs", value))
			m_iSliceMs = Math.ClampInt(value.ToInt(), 1, 16);
		if (System.GetCLIParam("aicfConstructionQueriesPerTick", value))
			m_iQueriesPerTick = Math.ClampInt(value.ToInt(), 16, 128);
		if (System.GetCLIParam("aicfConstructionAttempts", value))
			m_iAttempts = Math.ClampInt(value.ToInt(), 4, 128);
		if (System.GetCLIParam("aicfConstructionReserveGroups", value))
			m_iReserveGroups = Math.ClampInt(value.ToInt(), 0, 100);
	}
}
