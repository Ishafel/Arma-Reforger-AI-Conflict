class AICF_EquipmentPlanFailure
{
	string m_sKey;
	int m_iFailedAt;
}

// Только строки и время: не удерживает character, inventory или private world.
class AICF_EquipmentPlanFailures
{
	static const int RETRY_MS = 120000;
	static const int MAX_ENTRIES = 256;
	protected ref array<ref AICF_EquipmentPlanFailure> m_aEntries = {};

	bool CanAttempt(string key, int now)
	{
		for (int index = m_aEntries.Count() - 1; index >= 0; index--)
		{
			int age = now - m_aEntries[index].m_iFailedAt;
			if (age >= RETRY_MS) m_aEntries.Remove(index);
		}
		foreach (AICF_EquipmentPlanFailure entry : m_aEntries)
			if (entry.m_sKey == key) return false;
		// Не вытесняем ещё действующий отказ и не разрешаем ранний retry.
		return m_aEntries.Count() < MAX_ENTRIES;
	}

	void Record(string key, int now)
	{
		foreach (AICF_EquipmentPlanFailure entry : m_aEntries)
		{
			if (entry.m_sKey != key) continue;
			entry.m_iFailedAt = now;
			return;
		}
		if (m_aEntries.Count() >= MAX_ENTRIES) return;
		AICF_EquipmentPlanFailure failure = new AICF_EquipmentPlanFailure();
		failure.m_sKey = key;
		failure.m_iFailedAt = now;
		m_aEntries.Insert(failure);
	}

	void Forget(string key)
	{
		for (int index = m_aEntries.Count() - 1; index >= 0; index--)
			if (m_aEntries[index].m_sKey == key) m_aEntries.Remove(index);
	}

	int Count() { return m_aEntries.Count(); }
	void Clear() { m_aEntries.Clear(); }
}
