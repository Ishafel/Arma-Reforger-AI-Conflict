// One durable request per destroyed managed slot. Readiness is accumulated with
// the current logistics pace and survives temporary route/supply outages.
class AICF_ReinforcementRequest
{
	protected FactionKey m_sFactionKey;
	protected int m_iSlotId;
	protected int m_iRequestId;
	protected int m_iAttemptToken;
	protected int m_iAttemptSlotGeneration;
	protected int m_iStartedAtMs;
	protected int m_iLastProgressAtMs;
	protected int m_iProgressMs;
	protected int m_iRetryAtMs;
	protected AICF_ESupplyNetworkTier m_LastTier = AICF_ESupplyNetworkTier.BLOCKED;
	protected bool m_bTierObserved;
	protected SCR_CampaignMilitaryBaseComponent m_SavedTargetBase;

	void AICF_ReinforcementRequest(
		FactionKey factionKey,
		int slotId,
		int requestId,
		SCR_CampaignMilitaryBaseComponent savedTargetBase)
	{
		m_sFactionKey = factionKey;
		m_iSlotId = slotId;
		m_iRequestId = requestId;
		m_SavedTargetBase = savedTargetBase;
		m_iStartedAtMs = System.GetTickCount();
		m_iLastProgressAtMs = m_iStartedAtMs;
	}

	FactionKey GetFactionKey() { return m_sFactionKey; }
	int GetSlotId() { return m_iSlotId; }
	int GetRequestId() { return m_iRequestId; }
	int GetAttemptToken() { return m_iAttemptToken; }
	int GetAttemptSlotGeneration() { return m_iAttemptSlotGeneration; }
	int GetProgressMs() { return m_iProgressMs; }
	int GetRetryAtMs() { return m_iRetryAtMs; }
	AICF_ESupplyNetworkTier GetLastTier() { return m_LastTier; }
	bool HasTierObservation() { return m_bTierObserved; }
	SCR_CampaignMilitaryBaseComponent GetSavedTargetBase() { return m_SavedTargetBase; }

	int GetAgeMs()
	{
		return System.GetTickCount(m_iStartedAtMs);
	}

	bool Advance(AICF_ESupplyNetworkTier tier, int pacePercent, int requiredProgressMs)
	{
		int nowMs = System.GetTickCount();
		int elapsedMs = System.GetTickCount(m_iLastProgressAtMs);
		m_iLastProgressAtMs = nowMs;
		if (elapsedMs < 0)
			elapsedMs = 0;
		if (elapsedMs > 5000)
			elapsedMs = 5000;

		bool tierChanged = !m_bTierObserved || m_LastTier != tier;
		m_bTierObserved = true;
		m_LastTier = tier;
		if (pacePercent > 0 && requiredProgressMs > 0 && m_iProgressMs < requiredProgressMs)
		{
			m_iProgressMs += elapsedMs * pacePercent / 100;
			if (m_iProgressMs > requiredProgressMs)
				m_iProgressMs = requiredProgressMs;
		}
		else if (requiredProgressMs <= 0)
		{
			m_iProgressMs = 1;
		}

		return tierChanged;
	}

	bool IsReady(int requiredProgressMs)
	{
		return requiredProgressMs <= 0 || m_iProgressMs >= requiredProgressMs;
	}

	bool CanAttempt(int requiredProgressMs, int nowMs)
	{
		return IsReady(requiredProgressMs) && nowMs >= m_iRetryAtMs;
	}

	int BeginAttempt(int slotGeneration)
	{
		m_iAttemptToken++;
		m_iAttemptSlotGeneration = slotGeneration;
		return m_iAttemptToken;
	}

	void ScheduleRetry(int retryIntervalMs)
	{
		if (retryIntervalMs < 0)
			retryIntervalMs = 0;
		m_iRetryAtMs = System.GetTickCount() + retryIntervalMs;
	}
}
