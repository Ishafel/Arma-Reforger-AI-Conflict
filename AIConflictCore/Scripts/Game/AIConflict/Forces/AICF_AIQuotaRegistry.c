// World-scoped registry хранит ownership физических старых incarnations.
// Session меняет право на новые dispatch, но не превращает старые own bodies
// в STOCK и не освобождает их бюджет без native identity readback.
class AICF_AIQuotaRegistry
{
	protected AICF_AIQuotaWorldState m_WorldState;
	protected AICF_MatchController m_Controller;
	protected int m_iEpoch;
	protected int m_iNextSerial = 1;
	protected ref array<ref AICF_AIQuotaGroupOwner> m_aGroups = {};

	void AICF_AIQuotaRegistry(AICF_AIQuotaWorldState worldState)
	{
		m_WorldState = worldState;
	}

	bool BeginSession(AICF_MatchController controller, int epoch)
	{
		if (!m_WorldState || !m_WorldState.IsSessionCurrent(controller, epoch)) return false;
		m_Controller = controller;
		m_iEpoch = epoch;
		return true;
	}

	protected AICF_AIQuotaGroupOwner BeginRecord(SCR_AIGroup group, AICF_EAIQuotaGroupOwner kind)
	{
		if (!m_WorldState || !m_WorldState.IsSessionCurrent(m_Controller, m_iEpoch) ||
			!group || m_iNextSerial <= 0 || m_iNextSerial == int.MAX)
			return null;
		return AICF_AIQuotaGroupOwner.Capture(m_Controller, m_WorldState, m_iEpoch, m_iNextSerial, group, kind);
	}

	protected AICF_AIQuotaGroupOwner Commit(AICF_AIQuotaGroupOwner record)
	{
		if (!record || !record.CanDispatch() || record.m_iSerial != m_iNextSerial ||
			record.m_Controller != m_Controller || record.m_iEpoch != m_iEpoch || record.m_WorldState != m_WorldState)
			return null;
		foreach (AICF_AIQuotaGroupOwner existing : m_aGroups)
		{
			if (!existing || existing.m_Group != record.m_Group) continue;
			// Не заменяем живое ownership новым контекстом с тем же group pointer.
			if (existing.m_Kind != record.m_Kind || existing.m_iEpoch != record.m_iEpoch ||
				existing.m_iGeneration != record.m_iGeneration || existing.m_iRequestToken != record.m_iRequestToken ||
				existing.m_FactionState != record.m_FactionState || existing.m_Slot != record.m_Slot ||
				existing.m_Recruit != record.m_Recruit || existing.m_Builder != record.m_Builder ||
				existing.m_Logistics != record.m_Logistics || existing.m_Garrison != record.m_Garrison ||
				existing.m_Patrol != record.m_Patrol || !existing.CanDispatch())
				return null;
			return existing;
		}
		PruneDeleted();
		if (m_aGroups.Count() >= AICF_AIQuotaMath.MAX_TOTAL_UNITS) return null;
		m_aGroups.Insert(record);
		m_iNextSerial++;
		return record;
	}

	protected void PruneDeleted()
	{
		for (int index = m_aGroups.Count() - 1; index >= 0; index--)
		{
			AICF_AIQuotaGroupOwner record = m_aGroups[index];
			if (!record || !record.m_Group) m_aGroups.Remove(index);
		}
	}

	AICF_AIQuotaGroupOwner RegisterSlot(AICF_FactionState state, AICF_GroupSlot slot)
	{
		if (!state || !slot) return null;
		AICF_AIQuotaGroupOwner record = BeginRecord(slot.GetGroup(), AICF_EAIQuotaGroupOwner.SLOT);
		if (!record) return null;
		record.m_FactionState = state;
		record.m_Slot = slot;
		record.m_iGeneration = slot.GetSpawnGeneration();
		return Commit(record);
	}

	AICF_AIQuotaGroupOwner RegisterDonor(AICF_InfantryRecruitmentOrder order)
	{
		if (!order) return null;
		AICF_AIQuotaGroupOwner record = BeginRecord(order.m_Donor, AICF_EAIQuotaGroupOwner.DONOR);
		if (!record) return null;
		record.m_Recruit = order;
		record.m_iGeneration = order.m_iGeneration;
		record.m_iRequestToken = order.m_iToken;
		return Commit(record);
	}

	AICF_AIQuotaGroupOwner RegisterBuilder(AICF_BaseBuilder builder)
	{
		if (!builder) return null;
		AICF_AIQuotaGroupOwner record = BeginRecord(builder.m_Group, AICF_EAIQuotaGroupOwner.BUILDER);
		if (!record) return null;
		record.m_Builder = builder;
		record.m_iGeneration = builder.m_iGeneration;
		return Commit(record);
	}

	AICF_AIQuotaGroupOwner RegisterLogistics(AICF_LogisticsWorker worker)
	{
		if (!worker) return null;
		AICF_AIQuotaGroupOwner record = BeginRecord(worker.m_Group, AICF_EAIQuotaGroupOwner.LOGISTICS);
		if (!record) return null;
		record.m_Logistics = worker;
		record.m_iGeneration = worker.m_iGeneration;
		return Commit(record);
	}

	AICF_AIQuotaGroupOwner RegisterGarrison(AICF_FIAGarrison garrison, bool desant)
	{
		if (!garrison) return null;
		SCR_AIGroup group = garrison.m_Group;
		AICF_EAIQuotaGroupOwner kind = AICF_EAIQuotaGroupOwner.FIA_GARRISON;
		if (desant)
		{
			group = garrison.m_DesantGroup;
			kind = AICF_EAIQuotaGroupOwner.FIA_DESANT;
		}
		AICF_AIQuotaGroupOwner record = BeginRecord(group, kind);
		if (!record) return null;
		record.m_Garrison = garrison;
		record.m_iGeneration = garrison.m_iGeneration;
		return Commit(record);
	}

	AICF_AIQuotaGroupOwner RegisterPatrol(AICF_FIAPatrol patrol)
	{
		if (!patrol) return null;
		AICF_AIQuotaGroupOwner record = BeginRecord(patrol.m_Group, AICF_EAIQuotaGroupOwner.FIA_PATROL);
		if (!record) return null;
		record.m_Patrol = patrol;
		record.m_iGeneration = patrol.m_iGeneration;
		return Commit(record);
	}

	AICF_AIQuotaGroupOwner Find(SCR_AIGroup group)
	{
		if (!group || !m_WorldState || !m_WorldState.IsCurrentWorld()) return null;
		foreach (AICF_AIQuotaGroupOwner record : m_aGroups)
			if (record && record.m_Group == group) return record;
		return null;
	}

	bool ClassifyGroup(SCR_AIGroup group, out AICF_EAIQuotaBucket bucket)
	{
		bucket = AICF_EAIQuotaBucket.STOCK;
		if (!m_WorldState || !m_WorldState.IsCurrentWorld()) return false;
		AICF_AIQuotaGroupOwner record = Find(group);
		if (!record) return true;
		// Stale registered identity НЕ переименовывается в STOCK.
		if (!record.MatchesIdentity()) return false;
		bucket = record.GetBucket();
		return bucket >= 0 && bucket < AICF_EAIQuotaBucket.COUNT;
	}
}
