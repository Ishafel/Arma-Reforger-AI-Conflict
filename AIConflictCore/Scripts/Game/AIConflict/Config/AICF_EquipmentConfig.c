// Патроны, а не количество магазинов. Целевой запас берётся из актуального комплекта.
class AICF_EquipmentConfig
{
	static const int EVALUATE_MS = 10000;
	static const int QUIET_MS = 30000;
	static const int RETRY_MS = 60000;
	static const int CORPSE_TIMEOUT_MS = 45000;
	static const int VISIT_TIMEOUT_MS = 180000;
	static const float SEARCH_METERS = 50;
	static const float TRANSFER_METERS = 3;
	static const float SERVICE_METERS = 15;
	static const float MAX_SERVICE_METERS = 1500;
	int m_iLowPercent = 25;
	int m_iSatisfiedPercent = 75;

	void AICF_EquipmentConfig()
	{
		string value;
		if (System.GetCLIParam("aicfEquipmentLowPercent", value))
			m_iLowPercent = Math.ClampInt(value.ToInt(), 1, 50);
		if (System.GetCLIParam("aicfEquipmentSatisfiedPercent", value))
			m_iSatisfiedPercent = Math.ClampInt(value.ToInt(), m_iLowPercent + 1, 100);
	}

	bool Low(int rounds, int target, bool launcher, bool pending)
	{
		if (target <= 0) return false;
		if (launcher) return rounds < 1;
		if (pending) return rounds * 100 < target * m_iSatisfiedPercent;
		return rounds * 100 <= target * m_iLowPercent;
	}

	static bool InRange(vector a, vector b, float meters)
	{
		return vector.DistanceSq(a, b) <= meters * meters;
	}

	static bool Prefer(float distance, EntityID id, float bestDistance, EntityID bestId)
	{
		return distance < bestDistance || (distance == bestDistance && id.ToString().Compare(bestId.ToString()) < 0);
	}
}
