// Request metadata begins before policy activation. This is not a native queue
// enumeration: an observer-range eviction has no public completion callback.
class AICF_AIQuotaNativeQueueRecord
{
	ref AICF_AIQuotaGroupQueue m_Identity;
	int m_iSequence;
	int m_iSlotsWanted;
	float m_fObserverRange;
	bool m_bEnqueueReturned;
	ref array<ResourceName> m_aPrefabs = {};

	bool IsCurrent()
	{
		if (!m_Identity || !m_Identity.IsCurrent() || !m_Identity.m_Group.m_aUnitPrefabSlots ||
			m_Identity.m_Group.m_aUnitPrefabSlots.Count() != m_aPrefabs.Count()) return false;
		for (int index; index < m_aPrefabs.Count(); index++)
			if (m_Identity.m_Group.m_aUnitPrefabSlots[index] != m_aPrefabs[index]) return false;
		return true;
	}

	bool IsDefinitelyGone()
	{
		return m_Identity && GetGame() && m_Identity.m_World == GetGame().GetWorld() &&
			!m_Identity.m_Group && !m_Identity.m_World.FindEntityByID(m_Identity.m_GroupId);
	}
}

class AICF_AIQuotaNativeQueueJournal
{
	protected SCR_AIWorld m_World;
	protected BaseWorld m_MainWorld;
	protected int m_iSequence;
	protected bool m_bUnknown;
	protected ref array<ref AICF_AIQuotaNativeQueueRecord> m_aRecords = {};

	void AICF_AIQuotaNativeQueueJournal(SCR_AIWorld world)
	{
		m_World = world;
		if (world) m_MainWorld = world.GetWorld();
	}

	bool IsCurrent()
	{
		return Replication.IsServer() && GetGame() && m_World && m_MainWorld &&
			GetGame().GetAIWorld() == m_World && GetGame().GetWorld() == m_MainWorld && m_World.GetWorld() == m_MainWorld;
	}

	protected bool Reconcile()
	{
		if (!IsCurrent() || m_bUnknown) return false;
		for (int index = m_aRecords.Count() - 1; index >= 0; index--)
		{
			AICF_AIQuotaNativeQueueRecord record = m_aRecords[index];
			if (!record) return false;
			if (record.IsDefinitelyGone()) { m_aRecords.Remove(index); continue; }
			if (!record.IsCurrent()) return false;
			if (record.m_Identity.m_Group.IsExpandComplete()) m_aRecords.Remove(index);
		}
		return true;
	}

	AICF_AIQuotaNativeQueueRecord BeforeRequest(SCR_AIGroup group, int slotsWanted, float observerRange)
	{
		if (!Reconcile() || !group || group.GetWorld() != m_MainWorld)
		{
			m_bUnknown = true;
			return null;
		}
		int requested = AICF_AIQuotaGroupQueue.RequestedUnits(group, slotsWanted);
		if (requested < 0) { m_bUnknown = true; return null; }
		if (requested == 0) return null;
		AICF_AIQuotaNativeQueueRecord record;
		foreach (AICF_AIQuotaNativeQueueRecord existing : m_aRecords)
			if (existing.m_Identity.m_Group == group) record = existing;
		if (!record)
		{
			if (m_iSequence == int.MAX || m_aRecords.Count() >= AICF_AIQuotaMath.MAX_TOTAL_UNITS)
			{
				m_bUnknown = true;
				return null;
			}
			AICF_AIQuotaGroupQueue identity = AICF_AIQuotaGroupQueue.Capture(group, null, m_iSequence + 1);
			if (!identity) { m_bUnknown = true; return null; }
			record = new AICF_AIQuotaNativeQueueRecord();
			record.m_Identity = identity;
			m_iSequence++;
			record.m_iSequence = m_iSequence;
			m_aRecords.Insert(record);
		}
		record.m_iSlotsWanted = slotsWanted;
		record.m_fObserverRange = observerRange;
		record.m_bEnqueueReturned = false;
		record.m_aPrefabs.Clear();
		record.m_aPrefabs.Copy(group.m_aUnitPrefabSlots);
		return record;
	}

	void AfterRequest(AICF_AIQuotaNativeQueueRecord record)
	{
		if (!IsCurrent() || !record || !m_aRecords.Contains(record)) return;
		if (!record.IsCurrent()) { m_bUnknown = true; return; }
		record.m_bEnqueueReturned = true;
	}

	void Cancel(SCR_AIGroup group)
	{
		if (!IsCurrent() || !group) return;
		for (int index = m_aRecords.Count() - 1; index >= 0; index--)
		{
			AICF_AIQuotaNativeQueueRecord record = m_aRecords[index];
			if (record && record.m_Identity && record.m_Identity.m_Group == group && record.m_Identity.IsCurrent())
				m_aRecords.Remove(index);
		}
	}

	bool Snapshot(array<ref AICF_AIQuotaNativeQueueRecord> records)
	{
		if (!records) return false;
		records.Clear();
		if (!Reconcile()) return false;
		foreach (AICF_AIQuotaNativeQueueRecord record : m_aRecords)
		{
			if (!record || !record.IsCurrent() || !record.m_bEnqueueReturned) return false;
			records.Insert(record);
		}
		return IsCurrent();
	}
}

modded class SCR_AIWorld
{
	protected ref AICF_AIQuotaNativeQueueJournal m_AICFNativeQuotaJournal;

	AICF_AIQuotaNativeQueueJournal AICF_GetNativeQuotaJournal()
	{
		if (!Replication.IsServer() || !GetGame() || GetGame().GetAIWorld() != this || GetWorld() != GetGame().GetWorld()) return null;
		if (!m_AICFNativeQuotaJournal) m_AICFNativeQuotaJournal = new AICF_AIQuotaNativeQueueJournal(this);
		return m_AICFNativeQuotaJournal;
	}
}
