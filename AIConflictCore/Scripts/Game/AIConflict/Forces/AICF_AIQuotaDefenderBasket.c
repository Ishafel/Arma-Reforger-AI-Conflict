// Stock defenders имеют отдельный direct factory и оплату за каждого бойца.
// Basket резервирует весь remaining refill, а не обходит group admission.
class AICF_AIQuotaDefenderBasket
{
	AICF_AIWorldQuotaService m_Service;
	SCR_DefenderSpawnerComponent m_Spawner;
	SCR_AIGroup m_Group;
	SCR_EntityCatalogEntry m_Entry;
	SCR_EntityCatalogSpawnerData m_Data;
	Faction m_Faction;
	int m_iEpoch;
	int m_iSerial;
	int m_iSupplyCost;
	ref AICF_AIQuotaCatalogIdentity m_Producer;
	ref AICF_AIQuotaCatalogIdentity m_GroupIdentity;
	ref array<ResourceName> m_Queue;
	protected ref array<ResourceName> m_aAuthored = {};
	protected ref array<ref AICF_AIQuotaReservation> m_aReservations = {};
	bool m_bCancelled;

	static AICF_AIQuotaDefenderBasket Capture(AICF_AIWorldQuotaService service, SCR_DefenderSpawnerComponent spawner,
		SCR_AIGroup group, SCR_EntityCatalogEntry entry, array<ResourceName> queue)
	{
		if (!service || !service.IsConfigured() || !spawner || !group || !entry || !queue || queue.IsEmpty() ||
			queue.Count() > AICF_AIQuotaMath.MAX_TOTAL_UNITS || !group.m_aUnitPrefabSlots ||
			group.m_aUnitPrefabSlots.IsEmpty() || group.m_aUnitPrefabSlots.Count() > AICF_AIQuotaMath.MAX_TOTAL_UNITS) return null;
		AICF_AIQuotaDefenderBasket basket = new AICF_AIQuotaDefenderBasket();
		basket.m_Service = service;
		basket.m_Spawner = spawner;
		basket.m_Group = group;
		basket.m_Entry = entry;
		basket.m_Data = SCR_EntityCatalogSpawnerData.Cast(entry.GetEntityDataOfType(SCR_EntityCatalogSpawnerData));
		if (!basket.m_Data) return null;
		basket.m_iSupplyCost = basket.m_Data.GetSupplyCost();
		basket.m_Queue = queue;
		basket.m_aAuthored.Copy(group.m_aUnitPrefabSlots);
		basket.m_Faction = spawner.GetFaction();
		basket.m_iEpoch = service.GetEpoch();
		basket.m_iSerial = service.AllocateContextSerial();
		basket.m_Producer = AICF_AIQuotaCatalogIdentity.Capture(spawner.GetOwner(), true);
		basket.m_GroupIdentity = AICF_AIQuotaCatalogIdentity.Capture(group, true);
		if (!basket.IsCurrent()) return null;
		return basket;
	}

	bool IsCurrent()
	{
		// Identity не зависит от доступной capacity/global quarantine: иначе
		// чужой quota отказ ошибочно разрешит rollback валидного newborn.
		if (m_bCancelled || !m_Service || !m_Service.IsCurrentWorld() || m_Service.GetEpoch() != m_iEpoch) return false;
		if (m_iSerial <= 0 || !m_Spawner || !m_Producer || !m_Producer.IsCurrent() ||
			!m_GroupIdentity || !m_GroupIdentity.IsCurrent()) return false;
		if (!m_Entry || !m_Data || m_Data.GetSupplyCost() != m_iSupplyCost || m_iSupplyCost < 0 || !m_Faction) return false;
		if (m_Entry.GetEntityDataOfType(SCR_EntityCatalogSpawnerData) != m_Data ||
			m_Spawner.GetOwner() != m_Producer.m_Entity || m_Spawner.GetFaction() != m_Faction) return false;
		if (m_Group.GetFaction() != m_Faction || !m_Spawner.AICF_QuotaDefenderOwns(this)) return false;
		if (!m_Group.m_aUnitPrefabSlots || m_Group.m_aUnitPrefabSlots.Count() != m_aAuthored.Count()) return false;
		for (int index; index < m_aAuthored.Count(); index++)
			if (m_aAuthored[index].IsEmpty() || m_Group.m_aUnitPrefabSlots[index] != m_aAuthored[index]) return false;
		return true;
	}

	int Available()
	{
		int result;
		foreach (AICF_AIQuotaReservation reservation : m_aReservations)
		{
			if (!reservation || reservation.m_bCancelled) continue;
			int sum;
			if (!AICF_AIQuotaMath.AddBounded(result, reservation.m_iAvailable, AICF_AIQuotaMath.MAX_TOTAL_UNITS, sum)) return -1;
			result = sum;
		}
		return result;
	}

	bool EnsureRemaining()
	{
		if (!IsCurrent() || !m_Service.IsConfigured() || !m_Queue || m_Queue.IsEmpty() || m_Queue.Count() > AICF_AIQuotaMath.MAX_TOTAL_UNITS) return false;
		int available = Available();
		if (available < 0) return false;
		if (available >= m_Queue.Count()) return true;
		for (int index = m_aReservations.Count() - 1; index >= 0; index--)
			if (!m_aReservations[index] || m_aReservations[index].m_bCancelled || m_aReservations[index].m_iAvailable == 0) m_aReservations.Remove(index);
		if (m_aReservations.Count() >= AICF_AIQuotaMath.MAX_TOTAL_UNITS) return false;
		AICF_AIQuotaReservation extra = m_Service.ReserveUnits(AICF_EAIQuotaBucket.STOCK, m_Queue.Count() - available, m_iSerial);
		if (!extra) return false;
		if (!IsCurrent())
		{
			m_Service.CancelReservation(extra, m_iSerial);
			return false;
		}
		m_aReservations.Insert(extra);
		return true;
	}

	AICF_AIQuotaReservation NextReservation()
	{
		if (!IsCurrent()) return null;
		foreach (AICF_AIQuotaReservation reservation : m_aReservations)
			if (reservation && !reservation.m_bCancelled && reservation.m_iAvailable > 0) return reservation;
		return null;
	}

	void Cancel()
	{
		if (m_bCancelled) return;
		m_bCancelled = true;
		foreach (AICF_AIQuotaReservation reservation : m_aReservations)
			if (reservation && m_Service) m_Service.CancelReservation(reservation, m_iSerial);
	}
}

class AICF_AIQuotaDefenderAttempt : AICF_AIQuotaCreationGuard
{
	AICF_AIQuotaDefenderBasket m_Basket;
	SCR_EntitySpawnerSlotComponent m_Slot;
	ref AICF_AIQuotaCatalogIdentity m_SlotOwner;
	ResourceName m_Prefab;
	ref array<ResourceName> m_aBeforeQueue = {};
	float m_fSupplies;
	int m_iFreeCredit;
	bool m_bBound;

	static AICF_AIQuotaDefenderAttempt Capture(AICF_AIQuotaDefenderBasket basket,
		SCR_EntitySpawnerSlotComponent slot, ResourceName prefab, float supplies, int freeCredit)
	{
		if (!basket || !basket.IsCurrent() || !slot || prefab.IsEmpty() || !basket.m_Queue || basket.m_Queue.IsEmpty()) return null;
		AICF_AIQuotaDefenderAttempt attempt = new AICF_AIQuotaDefenderAttempt();
		attempt.m_Basket = basket;
		attempt.m_Slot = slot;
		// Slot — read-only геометрический child. Authority подтверждена producer,
		// необязательный собственный Rpl child не выдумывается.
		attempt.m_SlotOwner = AICF_AIQuotaCatalogIdentity.Capture(slot.GetOwner(), false);
		attempt.m_Prefab = prefab;
		attempt.m_fSupplies = supplies;
		attempt.m_iFreeCredit = freeCredit;
		attempt.m_aBeforeQueue.Copy(basket.m_Queue);
		if (!attempt.IsCurrent()) return null;
		return attempt;
	}

	override FactionKey GetFactionKey()
	{
		if (!m_Basket || !m_Basket.m_Faction) return string.Empty;
		return m_Basket.m_Faction.GetFactionKey();
	}

	bool OwnerCurrent()
	{
		if (m_SlotOwner && m_SlotOwner.m_Rpl && !m_SlotOwner.m_Rpl.IsMaster()) return false;
		return m_Basket && m_Basket.IsCurrent() && m_Slot && m_SlotOwner && m_SlotOwner.IsCurrent() &&
			m_Slot.GetOwner() == m_SlotOwner.m_Entity && m_Basket.m_Spawner.AICF_QuotaDefenderHasSlot(m_Slot);
	}

	override bool IsCurrent()
	{
		if (!OwnerCurrent() || !m_Basket.m_Queue || m_Basket.m_Queue.Count() != m_aBeforeQueue.Count() ||
			m_aBeforeQueue.IsEmpty() || m_Basket.m_Queue[0] != m_Prefab ||
			m_Basket.m_Spawner.AICF_QuotaDefenderFreeCredit() != m_iFreeCredit ||
			m_Basket.m_Spawner.GetSpawnerSupplies() != m_fSupplies) return false;
		for (int index; index < m_aBeforeQueue.Count(); index++)
			if (m_Basket.m_Queue[index] != m_aBeforeQueue[index]) return false;
		return true;
	}

	bool CanConsumeFreeCredit()
	{
		if (!m_bBound || m_iFreeCredit <= 0 || !OwnerCurrent() ||
			m_Basket.m_Queue.Count() != m_aBeforeQueue.Count() - 1 ||
			m_Basket.m_Spawner.AICF_QuotaDefenderFreeCredit() != m_iFreeCredit) return false;
		for (int index; index < m_Basket.m_Queue.Count(); index++)
			if (m_Basket.m_Queue[index] != m_aBeforeQueue[index + 1]) return false;
		return true;
	}
}
