// Полный explicit remaining basket ПЕРЕД native queue/intended reservation.
// Не читает GetSpawnQueueSize(): stock compatibility getter всегда возвращает0.
class AICF_AIQuotaGroupQueue
{
	SCR_AIGroup m_Group;
	EntityID m_GroupId;
	BaseWorld m_World;
	RplComponent m_Rpl;
	string m_sRpl;
	Faction m_Faction;
	AICF_AIQuotaGroupOwner m_Owner;
	int m_iSerial;
	bool m_bQuarantined;
	protected ref array<ref AICF_AIQuotaReservation> m_aBaskets = {};

	static AICF_AIQuotaGroupQueue Capture(SCR_AIGroup group, AICF_AIQuotaGroupOwner owner, int serial)
	{
		if (!Replication.IsServer() || !GetGame() || !group || serial <= 0 || group.GetWorld() != GetGame().GetWorld()) return null;
		RplComponent rpl = RplComponent.Cast(group.FindComponent(RplComponent));
		if (rpl && (!rpl.IsMaster() || !rpl.Id().IsValid())) return null;
		AICF_AIQuotaGroupQueue queue = new AICF_AIQuotaGroupQueue();
		queue.m_Group = group;
		queue.m_GroupId = group.GetID();
		queue.m_World = group.GetWorld();
		queue.m_Rpl = rpl;
		if (rpl) queue.m_sRpl = rpl.Id().ToString();
		queue.m_Faction = group.GetFaction();
		queue.m_Owner = owner;
		queue.m_iSerial = serial;
		if (!queue.IsCurrent()) return null;
		return queue;
	}

	AICF_EAIQuotaBucket GetBucket()
	{
		if (m_Owner) return m_Owner.GetBucket();
		return AICF_EAIQuotaBucket.STOCK;
	}

	bool IsCurrent()
	{
		if (!Replication.IsServer() || !GetGame() || !m_Group || !m_World || GetGame().GetWorld() != m_World ||
			m_Group.GetWorld() != m_World || m_Group.GetID() != m_GroupId || m_World.FindEntityByID(m_GroupId) != m_Group ||
			m_Group.GetFaction() != m_Faction || m_iSerial <= 0 || m_bQuarantined)
			return false;
		if (m_Rpl)
		{
			if (m_Group.FindComponent(RplComponent) != m_Rpl || !m_Rpl.IsMaster() || !m_Rpl.Id().IsValid() || m_Rpl.Id().ToString() != m_sRpl)
				return false;
		}
		else if (m_Group.FindComponent(RplComponent))
			return false;
		return !m_Owner || (m_Owner.m_Group == m_Group && m_Owner.CanDispatch());
	}

	// Native RequestSpawn(-1) initial1 затем auto-expands до full authored roster.
	// Поэтому резервируется full remaining, а не только первый queue member.
	static int RequestedUnits(SCR_AIGroup group, int slotsWanted)
	{
		if (!group || !group.m_aUnitPrefabSlots || slotsWanted < -1 || slotsWanted > AICF_AIQuotaMath.MAX_TOTAL_UNITS ||
			group.m_aUnitPrefabSlots.Count() > AICF_AIQuotaMath.MAX_TOTAL_UNITS)
			return -1;
		if (slotsWanted == 0) return 0;
		int capacity = group.m_aUnitPrefabSlots.Count() - group.GetDormantDeadCount();
		int actual = group.GetAgentsCount();
		if (capacity < 0 || actual < 0 || actual > capacity) return -1;
		int remaining = capacity - actual;
		if (slotsWanted == -1 && group.GetDormantAliveCount() == 0) return 0;
		if (remaining > AICF_AIQuotaMath.MAX_TOTAL_UNITS) return -1;
		return remaining;
	}

	int Available()
	{
		int available;
		foreach (AICF_AIQuotaReservation basket : m_aBaskets)
		{
			if (!basket || basket.m_iOwnerSerial != m_iSerial || basket.m_bCancelled) continue;
			int sum;
			if (!AICF_AIQuotaMath.AddBounded(available, basket.m_iAvailable, AICF_AIQuotaMath.MAX_TOTAL_UNITS, sum)) return -1;
			available = sum;
		}
		return available;
	}

	bool AdoptBasket(AICF_AIQuotaLedger ledger, AICF_AIQuotaReservation basket)
	{
		if (!IsCurrent() || !ledger || !basket || basket.m_Bucket != GetBucket() ||
			!ledger.HasAvailableReservation(basket, m_iSerial) || !m_aBaskets.IsEmpty()) return false;
		m_aBaskets.Insert(basket);
		return true;
	}

	bool HasBasket(AICF_AIQuotaReservation basket, int serial)
	{
		return IsCurrent() && basket && serial == m_iSerial && basket.m_iOwnerSerial == serial && m_aBaskets.Contains(basket);
	}

	bool EnsureAvailable(AICF_AIQuotaLedger ledger, int required)
	{
		if (!IsCurrent() || !ledger || required < 0 || required > AICF_AIQuotaMath.MAX_TOTAL_UNITS) return false;
		int available = Available();
		if (available < 0) return false;
		if (required <= available) return true;
		for (int index = m_aBaskets.Count() - 1; index >= 0; index--)
			if (!m_aBaskets[index] || m_aBaskets[index].m_bCancelled || m_aBaskets[index].m_iAvailable == 0) m_aBaskets.Remove(index);
		if (m_aBaskets.Count() >= AICF_AIQuotaMath.MAX_TOTAL_UNITS) return false;
		AICF_AIQuotaReservation extra = ledger.Reserve(GetBucket(), required - available, m_iSerial);
		if (!extra || !IsCurrent())
		{
			if (extra) ledger.CancelAvailable(extra, m_iSerial);
			return false;
		}
		m_aBaskets.Insert(extra);
		return true;
	}

	AICF_AIQuotaDispatch BeginDispatch(AICF_AIQuotaLedger ledger)
	{
		if (!IsCurrent() || !ledger) return null;
		foreach (AICF_AIQuotaReservation basket : m_aBaskets)
		{
			if (!basket || basket.m_bCancelled || basket.m_iAvailable <= 0) continue;
			return ledger.BeginDispatch(basket, m_iSerial);
		}
		return null;
	}

	void CancelAvailable(AICF_AIQuotaLedger ledger)
	{
		if (!ledger) return;
		foreach (AICF_AIQuotaReservation basket : m_aBaskets)
			if (basket) ledger.CancelAvailable(basket, m_iSerial);
	}
}
