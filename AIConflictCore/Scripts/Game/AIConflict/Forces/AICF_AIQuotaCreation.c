// Producer adapter supplies an immutable request/slot/quote guard. IsCurrent
// must NOT depend on free AI quota: an exhausted bucket is not an identity loss.
class AICF_AIQuotaCreationGuard
{
	bool IsCurrent() { return false; }
	FactionKey GetFactionKey() { return string.Empty; }
	AICF_EAIQuotaBucket GetBucket() { return AICF_EAIQuotaBucket.STOCK; }
	bool ExpectsVehicle() { return false; }
	bool OwnsVehicleLease(AICF_VehicleLease lease, IEntity entity) { return false; }
	// Optional owned request budget commit: pure captured state only, with no
	// engine reads/callbacks/logging. It runs at the final pre-factory boundary.
	bool CommitNativeAttempt() { return true; }
}

class AICF_AIQuotaCreation
{
	AICF_AIWorldQuotaService m_Service;
	ref AICF_AIQuotaReservation m_Reservation;
	ref AICF_AIQuotaDispatch m_Ticket;
	ref AICF_AIQuotaCreationGuard m_Guard;
	int m_iSerial;
	ResourceName m_Prefab;
	IEntity m_Producer;
	EntityID m_ProducerId;
	BaseWorld m_World;
	RplComponent m_ProducerRpl;
	string m_sProducerRpl;
	AICF_EAIQuotaBirthStatus m_Status;
	IEntity m_Entity;
	ref AICF_AIQuotaBody m_Body;
	bool m_bHeaderOnly;
	int m_iHeaderUnits;
	int m_iNativeActiveBefore;
	int m_iNativeIntendedBefore;
	bool m_bFactoryObserved;
	bool m_bNativeCallStarted;
	bool m_bFinished;
	string m_sRejectReason;

	bool IsProducerCurrent()
	{
		return IsProducerIdentityCurrent() && m_Guard && m_Guard.IsCurrent() && IsProducerIdentityCurrent();
	}

	bool IsProducerIdentityCurrent()
	{
		if (!Replication.IsServer() || !GetGame() || !m_Producer || !m_World || GetGame().GetWorld() != m_World ||
			m_Producer.GetWorld() != m_World || m_Producer.GetID() != m_ProducerId ||
			m_World.FindEntityByID(m_ProducerId) != m_Producer) return false;
		if (!m_ProducerRpl) return !m_Producer.FindComponent(RplComponent);
		return m_Producer.FindComponent(RplComponent) == m_ProducerRpl && m_ProducerRpl.IsMaster() &&
			m_ProducerRpl.Id().IsValid() && m_ProducerRpl.Id().ToString() == m_sProducerRpl;
	}

	bool IsHeaderValid()
	{
		if (!m_bHeaderOnly || !m_Entity || !m_Body || !m_Body.MatchesIdentity()) return false;
		SCR_AIGroup group = SCR_AIGroup.Cast(m_Entity);
		if (!group || group.CountTowardsAIWorldLimit() || group.GetAgentsCount() != 0 || group.GetPlayerCount() != 0 ||
			!group.m_aUnitPrefabSlots || group.m_aUnitPrefabSlots.Count() != m_iHeaderUnits || m_iHeaderUnits <= 0) return false;
		foreach (ResourceName prefab : group.m_aUnitPrefabSlots)
			if (prefab.IsEmpty()) return false;
		return true;
	}
}
