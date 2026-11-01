// Только индекс внутри одного синхронного RefreshCensus. Не snapshot допуска:
// caller по-прежнему проверяет identity, custody, ledger и native counters.
class AICF_AIQuotaBodyLookup
{
	protected ref map<IEntity, AICF_AIQuotaBody> m_mBodies = new map<IEntity, AICF_AIQuotaBody>();

	void Capture(array<ref AICF_AIQuotaBody> bodies)
	{
		m_mBodies.Clear();
		foreach (AICF_AIQuotaBody body : bodies)
		{
			if (!body || body.m_bReleased || !body.m_Entity || m_mBodies.Contains(body.m_Entity)) continue;
			m_mBodies.Set(body.m_Entity, body);
		}
	}

	AICF_AIQuotaBody Find(IEntity entity, array<ref AICF_AIQuotaBody> bodies)
	{
		if (!entity) return null;
		AICF_AIQuotaBody found = m_mBodies.Get(entity);
		// Retirement/removal during this refresh must not keep an old record.
		if (found && !found.m_bReleased && found.m_Entity == entity && bodies.Contains(found)) return found;
		// Binding can append a newborn after Capture. Do not cache absence.
		foreach (AICF_AIQuotaBody body : bodies)
		{
			if (!body || body.m_bReleased || body.m_Entity != entity) continue;
			m_mBodies.Set(entity, body);
			return body;
		}
		return null;
	}
}
