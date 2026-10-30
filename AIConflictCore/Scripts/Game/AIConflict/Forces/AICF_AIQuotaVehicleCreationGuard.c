// Одно разрешение конкретного producer. Оно не передаётся через ambient state
// и сохраняет исходный bucket между factory return и фактическим fleet bind.
class AICF_AIQuotaVehicleCreationGuard : AICF_AIQuotaCreationGuard
{
	protected AICF_AIWorldQuotaService m_Service;
	protected AICF_MatchController m_Controller;
	protected int m_iEpoch;
	protected int m_iSerial;
	protected AICF_EAIQuotaBucket m_Bucket;
	protected FactionKey m_sFaction;
	protected ref AICF_AIQuotaReservation m_Reservation;
	protected ref AICF_AIQuotaCatalogIdentity m_Producer;
	protected AICF_FactionFleet m_Fleet;
	protected AICF_VehicleLease m_Lease;
	protected string m_sPendingLease;
	protected ref AICF_AIQuotaVehicleEntity m_Newborn;
	protected string m_sBoundLifecycle;
	protected int m_iBoundGeneration;
	protected int m_iBoundLeaseGeneration;
	protected bool m_bBound;
	protected bool m_bCancelled;
	protected bool m_bQuarantined;
	protected bool m_bReservationAdopted;
	protected bool m_bNativeNull;
	protected bool m_bLeaseAttached;

	// Derived producer captures its typed request before calling Initialize.
	bool Initialize(AICF_AIWorldQuotaService service, AICF_EAIQuotaBucket bucket,
		int units, FactionKey faction, IEntity producer, bool requireMaster = true)
	{
		if (m_Service || !service || !service.IsConfigured() || units <= 0 || faction.IsEmpty()) return false;
		m_Service = service;
		m_Controller = service.GetQuotaController();
		m_iEpoch = service.GetEpoch();
		m_iSerial = service.AllocateContextSerial();
		m_Bucket = bucket;
		m_sFaction = faction;
		m_Producer = AICF_AIQuotaCatalogIdentity.Capture(producer, requireMaster);
		if (!IsCurrent()) return false;
		m_Reservation = service.ReserveUnits(bucket, units, m_iSerial);
		if (!m_Reservation || !IsCurrent())
		{
			CancelAvailable();
			return false;
		}
		return true;
	}

	bool IsDomainCurrent() { return false; }
	// Pure commit, invoked after all admission/native reads and immediately
	// before the actual native factory. A quota veto must not spend attempts.
	override bool CommitNativeAttempt() { return true; }

	override bool IsCurrent()
	{
		if (m_bCancelled || m_bQuarantined || m_bBound || !m_Service || !m_Service.IsCurrentWorld()) return false;
		if (m_Service.GetQuotaController() != m_Controller || m_Service.GetEpoch() != m_iEpoch ||
			!m_Producer || !m_Producer.IsCurrent() || !IsDomainCurrent()) return false;
		if (!m_Lease) return !m_bLeaseAttached;
		return m_Fleet && m_Fleet.OwnsVehicleLease(m_Lease) && !m_Lease.HasPhysicalAsset() &&
			m_Lease.GetState() == AICF_EVehicleLeaseState.RESERVED &&
			AICF_AIQuotaVehicleOwner.Fingerprint(m_Lease) == m_sPendingLease;
	}

	override FactionKey GetFactionKey() { return m_sFaction; }
	override AICF_EAIQuotaBucket GetBucket() { return m_Bucket; }
	override bool ExpectsVehicle() { return true; }

	bool AttachPendingLease(AICF_FactionFleet fleet, AICF_VehicleLease lease)
	{
		if (m_bLeaseAttached) return m_Fleet == fleet && m_Lease == lease && IsCurrent();
		if (!IsCurrent() || !fleet || !lease || !fleet.OwnsVehicleLease(lease) ||
			lease.HasPhysicalAsset() || lease.GetState() != AICF_EVehicleLeaseState.RESERVED) return false;
		m_Fleet = fleet;
		m_Lease = lease;
		m_bLeaseAttached = true;
		m_sPendingLease = AICF_AIQuotaVehicleOwner.Fingerprint(lease);
		return IsCurrent();
	}

	AICF_AIQuotaCreation BeginFactory(ResourceName prefab)
	{
		m_bNativeNull = false;
		if (!IsCurrent() || !m_Lease || m_Newborn || !m_Reservation) return null;
		return m_Service.BeginExternalCreation(m_Reservation, m_iSerial, prefab, m_Producer.m_Entity, false, this);
	}

	AICF_AIWorldQuotaService GetService() { return m_Service; }
	AICF_FactionFleet GetFleet() { return m_Fleet; }
	bool IsQuarantined() { return m_bQuarantined; }
	bool WasNativeNull() { return m_bNativeNull; }
	bool HasFactoryBody() { return m_Newborn != null; }

	bool CaptureFactoryResult(AICF_AIQuotaCreation creation, IEntity raw)
	{
		if (!creation || creation.m_Service != m_Service || creation.m_Guard != this || creation.m_Entity != raw) return false;
		if (creation.m_Status == AICF_EAIQuotaBirthStatus.NATIVE_NULL)
		{
			m_bNativeNull = true;
			return false;
		}
		if (creation.m_Status != AICF_EAIQuotaBirthStatus.BOUND || !raw || !creation.m_Body ||
			!creation.m_Body.MatchesIdentity() || !IsCurrent())
		{
			m_bQuarantined = true;
			m_Service.RejectExternalCreation(creation, "OWNED_VEHICLE_NATIVE_RETURN_AMBIGUOUS");
			return false;
		}
		m_Newborn = AICF_AIQuotaVehicleEntity.Capture(raw);
		if (m_Newborn && m_Newborn.IsCurrent()) return true;
		m_bQuarantined = true;
		m_Service.RejectExternalCreation(creation, "OWNED_VEHICLE_NEWBORN_IDENTITY_MISSING");
		return false;
	}

	// Call only on the true branch of this producer's BindReservedLeaseVehicle.
	// Capture the actual accepted receipt; do not predict a +1 generation.
	bool RecordOwnLeaseBind(Vehicle vehicle)
	{
		if (m_bCancelled || m_bQuarantined || m_bBound || !m_Service || !m_Service.IsCurrentWorld() ||
			m_Service.GetEpoch() != m_iEpoch || !IsDomainCurrent() || !m_Newborn || !m_Newborn.IsCurrent()) return false;
		if (!m_Fleet || !m_Lease || !m_Fleet.OwnsVehicleLease(m_Lease) || !m_Lease.HasPhysicalAsset() ||
			m_Newborn.m_Entity != vehicle || !m_Lease.MatchesEntityIdentity(vehicle, m_Newborn.m_Id, m_Newborn.m_sRpl)) return false;
		m_iBoundGeneration = m_Lease.GetVehicleGeneration();
		m_iBoundLeaseGeneration = m_Lease.GetLeaseGeneration();
		m_sBoundLifecycle = m_Lease.GetVehicleLifecycleId();
		m_bBound = true;
		return OwnsVehicleLease(m_Lease, vehicle);
	}

	override bool OwnsVehicleLease(AICF_VehicleLease lease, IEntity entity)
	{
		if (!m_bBound || m_bQuarantined || !m_Service || !m_Service.IsCurrentWorld() ||
			m_Service.GetEpoch() != m_iEpoch || !m_Newborn || !m_Newborn.IsCurrent()) return false;
		return lease == m_Lease && entity == m_Newborn.m_Entity && m_Fleet && m_Fleet.OwnsVehicleLease(lease) &&
			lease.GetVehicleGeneration() == m_iBoundGeneration && lease.GetLeaseGeneration() == m_iBoundLeaseGeneration &&
			lease.GetVehicleLifecycleId() == m_sBoundLifecycle &&
			lease.MatchesEntityIdentity(Vehicle.Cast(entity), m_Newborn.m_Id, m_Newborn.m_sRpl);
	}

	bool AdoptRemainingGroup(SCR_AIGroup group, AICF_AIQuotaGroupOwner owner)
	{
		if (m_bReservationAdopted || m_bCancelled || m_bQuarantined || !m_bBound || !m_Service) return false;
		if (!m_Service.AdoptExternalGroupReservation(group, owner, m_Reservation, m_iSerial)) return false;
		m_bReservationAdopted = true;
		return true;
	}

	void CancelAvailable()
	{
		if (m_Service && m_Reservation && !m_bReservationAdopted) m_Service.CancelReservation(m_Reservation, m_iSerial);
		m_bCancelled = true;
	}

	void ~AICF_AIQuotaVehicleCreationGuard() { CancelAvailable(); }
}
