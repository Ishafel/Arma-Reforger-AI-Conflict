// Два COMBAT units резервируются до новой worker generation: native vehicle
// и ровно один driver. Generation/lease принимаются только явными receipts.
class AICF_AIQuotaLogisticsCreationGuard : AICF_AIQuotaVehicleCreationGuard
{
	protected AICF_LogisticsWorker m_Worker;
	protected AICF_FactionFleet m_ExpectedFleet;
	protected AICF_VehicleLease m_AcceptedLease;
	protected SCR_CampaignFaction m_Faction;
	protected int m_iSlot;
	protected int m_iOrdinal;
	protected int m_iGeneration;
	protected AICF_ELogisticsPhase m_InitialPhase;
	protected int m_iSpawnPhaseAt;
	protected bool m_bGenerationAccepted;
	protected bool m_bSpawnStarted;
	protected ref AICF_AIQuotaCatalogIdentity m_Depot;
	protected ref AICF_AIQuotaCatalogIdentity m_Home;
	protected SCR_CampaignMilitaryBaseComponent m_HomeComponent;
	protected SCR_CatalogEntitySpawnerComponent m_Production;
	protected SCR_EntityCatalogEntry m_Entry;
	protected ResourceName m_Prefab;
	protected AICF_VehicleSpawnSiteReservation m_Site;
	protected SCR_EntitySpawnerSlotComponent m_SlotComponent;
	protected ref AICF_AIQuotaCatalogIdentity m_SlotOwner;
	protected vector m_aSpawn[4];
	protected vector m_aSlot[4];
	protected bool m_bSiteCaptured;
	protected AICF_AIQuotaGroupOwner m_DriverOwner;

	static AICF_AIQuotaLogisticsCreationGuard Capture(AICF_AIWorldQuotaService service,
		AICF_LogisticsWorker worker, AICF_FactionFleet fleet)
	{
		if (!service || !worker || !fleet || !worker.m_Faction || !worker.m_Depot || !worker.m_Home ||
			!worker.m_Home.GetOwner() || !worker.m_Production || !worker.m_Production.GetOwner() || !worker.m_Entry ||
			worker.m_Lease || worker.m_Group || worker.m_Vehicle || worker.m_QuotaCreationGuard || worker.m_bQuotaSpawnClosed) return null;
		AICF_AIQuotaLogisticsCreationGuard guard = new AICF_AIQuotaLogisticsCreationGuard();
		guard.m_Worker = worker;
		guard.m_ExpectedFleet = fleet;
		guard.m_Faction = worker.m_Faction;
		guard.m_iSlot = worker.m_iSlot;
		guard.m_iOrdinal = worker.m_iOrdinal;
		guard.m_iGeneration = worker.m_iGeneration;
		guard.m_InitialPhase = worker.m_ePhase;
		guard.m_Depot = AICF_AIQuotaCatalogIdentity.Capture(worker.m_Depot, false);
		guard.m_Home = AICF_AIQuotaCatalogIdentity.Capture(worker.m_Home.GetOwner(), false);
		guard.m_HomeComponent = worker.m_Home;
		guard.m_Production = worker.m_Production;
		guard.m_Entry = worker.m_Entry;
		guard.m_Prefab = worker.m_Entry.GetPrefab();
		worker.m_QuotaCreationGuard = guard;
		if (guard.Initialize(service, AICF_EAIQuotaBucket.COMBAT, 2, worker.m_Faction.GetFactionKey(), worker.m_Production.GetOwner())) return guard;
		guard.CancelAvailable();
		if (worker.m_QuotaCreationGuard == guard) worker.m_QuotaCreationGuard = null;
		return null;
	}

	protected bool StableWorker()
	{
		if (!m_Worker || m_Worker.m_QuotaCreationGuard != this || m_Worker.m_bStopped || m_Worker.m_bQuotaSpawnClosed ||
			!m_Controller || !m_Controller.OwnsAIQuotaLogisticsWorker(m_Worker)) return false;
		if (m_Worker.m_iSlot != m_iSlot || m_Worker.m_iOrdinal != m_iOrdinal || m_Worker.m_Faction != m_Faction ||
			m_Worker.m_Home != m_HomeComponent || m_Worker.m_Production != m_Production || m_Worker.m_Entry != m_Entry) return false;
		if (!m_Depot || !m_Depot.IsCurrent() || !m_Home || !m_Home.IsCurrent() ||
			m_Worker.m_Depot != m_Depot.m_Entity || m_Worker.m_DepotId != m_Depot.m_Id ||
			m_Worker.m_HomeId != m_Home.m_Id || !m_Entry || m_Entry.GetPrefab() != m_Prefab || m_Prefab.IsEmpty()) return false;
		return AICF_LogisticsDepotRegistry.Live(m_Worker);
	}

	bool AcceptGenerationAdvance()
	{
		if (m_bGenerationAccepted || !StableWorker() || m_iGeneration == int.MAX ||
			m_Worker.m_iGeneration != m_iGeneration + 1 || m_Worker.m_ePhase != m_InitialPhase ||
			m_Worker.m_Lease || m_Worker.m_Group || m_Worker.m_Vehicle) return false;
		m_iGeneration = m_Worker.m_iGeneration;
		m_bGenerationAccepted = true;
		return true;
	}

	bool AcceptPendingLease(AICF_VehicleLease lease)
	{
		if (!m_bGenerationAccepted || m_AcceptedLease || !StableWorker() || m_Worker.m_iGeneration != m_iGeneration ||
			!lease || m_Worker.m_Lease != lease || m_Worker.m_Fleet != m_ExpectedFleet || !m_ExpectedFleet.OwnsVehicleLease(lease) ||
			!lease.MatchesTripIdentity(m_Faction.GetFactionKey(), m_iSlot, m_iGeneration, m_iGeneration)) return false;
		m_AcceptedLease = lease;
		return AttachPendingLease(m_ExpectedFleet, lease);
	}

	bool AcceptSpawnPhase()
	{
		if (m_bSpawnStarted || !m_bGenerationAccepted || !m_AcceptedLease || !StableWorker() ||
			m_Worker.m_iGeneration != m_iGeneration || m_Worker.m_ePhase != AICF_ELogisticsPhase.SPAWN_PENDING ||
			m_Worker.m_Lease != m_AcceptedLease || m_Worker.m_bCleanupQueued) return false;
		m_iSpawnPhaseAt = m_Worker.m_iPhaseAtMs;
		m_bSpawnStarted = true;
		return IsCurrent();
	}

	bool CaptureSite()
	{
		if (!IsCurrent() || !m_bSpawnStarted || m_bSiteCaptured || !m_Worker.m_Site || !m_Worker.m_SpawnSlot ||
			!m_Worker.m_SpawnSlot.GetOwner()) return false;
		m_Site = m_Worker.m_Site;
		m_SlotComponent = m_Worker.m_SpawnSlot;
		m_SlotOwner = AICF_AIQuotaCatalogIdentity.Capture(m_SlotComponent.GetOwner(), false);
		for (int index; index < 4; index++)
		{
			m_aSpawn[index] = m_Worker.m_aSpawnTransform[index];
			m_aSlot[index] = m_Worker.m_aSlotTransform[index];
		}
		m_bSiteCaptured = true;
		return IsCurrent();
	}

	override bool IsDomainCurrent()
	{
		if (!StableWorker() || m_Worker.m_iGeneration != m_iGeneration || m_Worker.m_Group || m_Worker.m_Driver) return false;
		if (m_bSpawnStarted)
		{
			if (m_Worker.m_ePhase != AICF_ELogisticsPhase.SPAWN_PENDING || m_Worker.m_iPhaseAtMs != m_iSpawnPhaseAt || m_Worker.m_bCleanupQueued) return false;
		}
		else if (m_Worker.m_ePhase != m_InitialPhase) return false;
		if (m_Worker.m_Lease != m_AcceptedLease) return false;
		if (m_AcceptedLease && m_Worker.m_Fleet != m_ExpectedFleet) return false;
		if (m_Worker.m_Vehicle && (!m_Newborn || !m_Newborn.IsCurrent() || m_Worker.m_Vehicle != m_Newborn.m_Entity)) return false;
		if (!m_bSiteCaptured) return true;
		if (!m_Site || m_Worker.m_Site != m_Site || m_Site.IsExpired(System.GetTickCount()) ||
			m_Worker.m_SpawnSlot != m_SlotComponent || !m_SlotOwner || !m_SlotOwner.IsCurrent() ||
			m_SlotComponent.GetOwner() != m_SlotOwner.m_Entity || m_Worker.m_SpawnSlotId != m_SlotOwner.m_Id) return false;
		for (int index; index < 4; index++)
			if (m_Worker.m_aSpawnTransform[index] != m_aSpawn[index] || m_Worker.m_aSlotTransform[index] != m_aSlot[index]) return false;
		return m_SlotOwner.m_Entity.GetOrigin() == m_aSlot[3] && m_Production.AICF_AllowsLogisticsEntry(m_Entry, m_SlotComponent);
	}

	bool AdoptDriver(SCR_AIGroup group, AICF_AIQuotaGroupOwner owner)
	{
		if (!m_Worker || m_Worker.m_QuotaCreationGuard != this || m_Worker.m_bQuotaSpawnClosed ||
			!owner || owner.m_Kind != AICF_EAIQuotaGroupOwner.LOGISTICS || owner.m_Logistics != m_Worker ||
			owner.m_iGeneration != m_iGeneration || m_Worker.m_Group != group || !owner.CanDispatch()) return false;
		if (!AdoptRemainingGroup(group, owner)) return false;
		m_DriverOwner = owner;
		return true;
	}

	void CloseSpawn()
	{
		// Caller closes the domain flag before callbacks. Only this exact
		// registered driver's remaining requests can be cancelled here.
		if (m_Service && m_DriverOwner && m_Worker && m_Worker.m_QuotaCreationGuard == this &&
			m_Worker.m_bQuotaSpawnClosed && m_Worker.m_iGeneration == m_iGeneration &&
			m_DriverOwner.m_Logistics == m_Worker && m_DriverOwner.MatchesIdentity() &&
			m_Service.GetOwners().Find(m_DriverOwner.m_Group) == m_DriverOwner)
			m_Service.CancelGroupRequest(m_DriverOwner.m_Group);
		CancelAvailable();
	}
}
