// Server-owned reliability and load settings for Stage 2. Every value can be
// changed through CLI startup parameters without recompiling the addon.
class AICF_Stage2Config
{
	static const int DEFAULT_RELIABILITY_INTERVAL_MS = 5000;
	static const int DEFAULT_ORDER_RECOVERY_RETRY_MS = 5000;
	static const int DEFAULT_STUCK_TIMEOUT_MS = 120000;
	static const float DEFAULT_STUCK_PROGRESS_METERS = 25.0;
	static const int DEFAULT_MAX_STUCK_RECOVERIES = 3;
	static const int DEFAULT_OBJECTIVE_HOLD_TIMEOUT_MS = 300000;
	static const int DEFAULT_MAX_CONCURRENT_REPLACEMENT_SPAWNS = 1;

	protected int m_iReliabilityIntervalMs;
	protected int m_iOrderRecoveryRetryMs;
	protected int m_iStuckTimeoutMs;
	protected float m_fStuckProgressMeters;
	protected int m_iMaxStuckRecoveries;
	protected int m_iObjectiveHoldTimeoutMs;
	protected int m_iMaxConcurrentReplacementSpawns;
	protected bool m_bStuckWatchdogEnabled;
	protected FactionKey m_sTestDropOrderFaction;
	protected int m_iTestDropOrderSlot = -1;
	protected int m_iTestDropOrderAtMs = 30000;

	void AICF_Stage2Config()
	{
		m_iReliabilityIntervalMs = DEFAULT_RELIABILITY_INTERVAL_MS;
		m_iOrderRecoveryRetryMs = DEFAULT_ORDER_RECOVERY_RETRY_MS;
		m_iStuckTimeoutMs = DEFAULT_STUCK_TIMEOUT_MS;
		m_fStuckProgressMeters = DEFAULT_STUCK_PROGRESS_METERS;
		m_iMaxStuckRecoveries = DEFAULT_MAX_STUCK_RECOVERIES;
		m_iObjectiveHoldTimeoutMs = DEFAULT_OBJECTIVE_HOLD_TIMEOUT_MS;
		m_iMaxConcurrentReplacementSpawns = DEFAULT_MAX_CONCURRENT_REPLACEMENT_SPAWNS;
		m_bStuckWatchdogEnabled = true;
		ApplyCLIOverrides();
	}

	int GetReliabilityIntervalMs()
	{
		return m_iReliabilityIntervalMs;
	}

	int GetOrderRecoveryRetryMs()
	{
		return m_iOrderRecoveryRetryMs;
	}

	int GetStuckTimeoutMs()
	{
		return m_iStuckTimeoutMs;
	}

	float GetStuckProgressMeters()
	{
		return m_fStuckProgressMeters;
	}

	int GetMaxStuckRecoveries()
	{
		return m_iMaxStuckRecoveries;
	}

	int GetObjectiveHoldTimeoutMs()
	{
		return m_iObjectiveHoldTimeoutMs;
	}

	int GetMaxConcurrentReplacementSpawns()
	{
		return m_iMaxConcurrentReplacementSpawns;
	}

	bool GetStuckWatchdogEnabled()
	{
		return m_bStuckWatchdogEnabled;
	}

	bool HasTestDropOrder()
	{
		return (m_sTestDropOrderFaction == "US" || m_sTestDropOrderFaction == "USSR") &&
			m_iTestDropOrderSlot >= 0 &&
			m_iTestDropOrderSlot < AICF_Stage1Config.GROUP_SLOTS_PER_FACTION;
	}

	FactionKey GetTestDropOrderFaction()
	{
		return m_sTestDropOrderFaction;
	}

	int GetTestDropOrderSlot()
	{
		return m_iTestDropOrderSlot;
	}

	int GetTestDropOrderAtMs()
	{
		return m_iTestDropOrderAtMs;
	}

	protected void ApplyCLIOverrides()
	{
		string value;
		if (System.GetCLIParam("aicfReliabilityIntervalMs", value))
			m_iReliabilityIntervalMs = ClampInt(value.ToInt(), 1000, 60000);
		if (System.GetCLIParam("aicfOrderRecoveryRetryMs", value))
			m_iOrderRecoveryRetryMs = ClampInt(value.ToInt(), 1000, 60000);
		if (System.GetCLIParam("aicfStuckTimeoutMs", value))
			m_iStuckTimeoutMs = ClampInt(value.ToInt(), 30000, 3600000);
		if (System.GetCLIParam("aicfStuckProgressMeters", value))
			m_fStuckProgressMeters = ClampFloat(value.ToFloat(), 1.0, 500.0);
		if (System.GetCLIParam("aicfMaxStuckRecoveries", value))
			m_iMaxStuckRecoveries = ClampInt(value.ToInt(), 1, 100);
		if (System.GetCLIParam("aicfObjectiveHoldTimeoutMs", value))
			m_iObjectiveHoldTimeoutMs = ClampInt(value.ToInt(), 30000, 1800000);
		if (System.GetCLIParam("aicfMaxConcurrentSpawns", value))
			m_iMaxConcurrentReplacementSpawns = ClampInt(value.ToInt(), 1, 8);
		if (System.GetCLIParam("aicfStuckWatchdog", value))
			m_bStuckWatchdogEnabled = value.ToInt() > 0;
		if (System.GetCLIParam("aicfTestDropOrderFaction", value))
		{
			if (value == "US" || value == "USSR")
				m_sTestDropOrderFaction = value;
		}
		if (System.GetCLIParam("aicfTestDropOrderSlot", value))
			m_iTestDropOrderSlot = ClampInt(value.ToInt(), -1, AICF_Stage1Config.GROUP_SLOTS_PER_FACTION - 1);
		if (System.GetCLIParam("aicfTestDropOrderAtMs", value))
			m_iTestDropOrderAtMs = ClampInt(value.ToInt(), 5000, 3600000);
	}

	protected int ClampInt(int value, int minimum, int maximum)
	{
		if (value < minimum)
			return minimum;
		if (value > maximum)
			return maximum;

		return value;
	}

	protected float ClampFloat(float value, float minimum, float maximum)
	{
		if (value < minimum)
			return minimum;
		if (value > maximum)
			return maximum;

		return value;
	}
}
