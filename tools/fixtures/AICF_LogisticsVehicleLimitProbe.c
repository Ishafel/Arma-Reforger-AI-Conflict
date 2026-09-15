// Только isolated AIConflictArland/Scripts/Game/AIConflict/Tests, после Core.
// AICF_LogisticsRuntimeProbe.c подготавливает настоящий stock depot в Core.
// Проверяет identity/lease admission, не выдаёт пустые reservations за перевозку.
modded class AICF_FactionFleet
{
	AICF_VehicleLease AICF_ProbeInfantryReservation()
	{
		AICF_VehicleLease lease = new AICF_VehicleLease(m_sFactionKey, 0, 1, 1, ++m_iNextLeaseGeneration);
		m_aLeases.Insert(lease);
		return lease;
	}
}

class AICF_LogisticsVehicleLimitContracts
{
	protected int m_iCases;
	protected int m_iFailures;

	protected void Check(string scenario, bool passed)
	{
		m_iCases++;
		if (!passed) m_iFailures++;
		Print(string.Format("[AICF][VEHICLE_LIMIT_CONTRACT] test_only=1 case=%1 passed=%2", scenario, passed));
	}

	void Run(AICF_LogisticsDepotRegistry registry, AICF_LogisticsWorker seed, AICF_LogisticsConfig config)
	{
		int originalSlot = seed.m_iSlot;
		int originalGeneration = seed.m_iGeneration;
		Check("NULL_DEPOT", !registry.AddManualWorker(null));
		AICF_LogisticsWorker invalid = new AICF_LogisticsWorker();
		Check("UNREGISTERED_DEPOT", !registry.AddManualWorker(invalid));
		AICF_FactionFleet fleet = new AICF_FactionFleet(seed.m_Faction.GetFactionKey(), 1);
		AICF_VehicleLease infantry = fleet.AICF_ProbeInfantryReservation();
		Check("INFANTRY_CAP_FULL", fleet.GetCappedActiveOrReservedCount() == fleet.GetMaximumActiveOrReserved());
		array<ref AICF_LogisticsWorker> workers = {};
		int previousSlot = seed.m_iSlot;
		int previousOrdinal = seed.m_iOrdinal;
		for (int i; i < 12; i++)
		{
			AICF_LogisticsWorker w = registry.AddManualWorker(seed);
			Check("NEW_WORKER_" + i, w && w.m_iSlot > previousSlot && w.m_iOrdinal > previousOrdinal &&
				w.m_Depot == seed.m_Depot && w.m_DepotId == seed.m_DepotId && w.m_ExitHistory == seed.m_ExitHistory);
			if (!w) break;
			previousSlot = w.m_iSlot;
			previousOrdinal = w.m_iOrdinal;
			workers.Insert(w);
			w.m_iGeneration = 1;
			Check("LEASE_ABOVE_CAP_" + i, fleet.TryReserveLogistics(w));
		}
		Check("TWELVE_LOGISTICS_PLUS_INFANTRY", workers.Count() == 12 && fleet.GetActiveOrReservedCount() == 13);
		Check("LOGISTICS_EXCLUDED_FROM_INFANTRY_CAP", fleet.GetCappedActiveOrReservedCount() == 1);
		Check("ORIGINAL_IDENTITY_UNCHANGED", seed.m_iSlot == originalSlot && seed.m_iGeneration == originalGeneration);
		if (!workers.IsEmpty())
		{
			invalid.m_iSlot = workers[0].m_iSlot;
			invalid.m_iGeneration = 1;
			invalid.m_Faction = seed.m_Faction;
			Check("DUPLICATE_SLOT_REJECTED", !fleet.TryReserveLogistics(invalid));
			AICF_FactionFleet foreign = new AICF_FactionFleet("AICF_TEST_FOREIGN", 0);
			Check("FOREIGN_FACTION_REJECTED", !foreign.TryReserveLogistics(invalid));
			invalid.m_iSlot = 1;
			Check("INFANTRY_SLOT_REJECTED", !fleet.TryReserveLogistics(invalid));
			invalid.m_Faction = null;
			Check("NULL_FACTION_REJECTED", !fleet.TryReserveLogistics(invalid));
		}
		for (int scan; scan < 64; scan++)
		{
			registry.MarkDirty();
			registry.Update(config, System.GetTickCount());
		}
		int sameDepot;
		foreach (AICF_LogisticsWorker registered : registry.m_aWorkers)
		{
			if (registered.m_Depot == seed.m_Depot && registered.m_DepotId == seed.m_DepotId && registered.m_Faction == seed.m_Faction) sameDepot++;
		}
		Check("RECONCILE_DOES_NOT_ADD_SLOTS", sameDepot == workers.Count() + 1);
		bool released = true;
		foreach (AICF_LogisticsWorker worker : workers)
		{
			if (!fleet.ReleaseEmptyReservation(worker.m_Lease)) released = false;
			worker.m_Lease = null;
			worker.m_Fleet = null;
		}
		Check("RELEASE_ALL_LOGISTICS", released && fleet.GetActiveOrReservedCount() == 1);
		Check("RELEASE_INFANTRY", fleet.ReleaseEmptyReservation(infantry) && fleet.GetActiveOrReservedCount() == 0);
		registry.Stop();
		Check("STOP_PREVENTS_EXPANSION", !registry.AddManualWorker(seed));
		Print(string.Format("[AICF][VEHICLE_LIMIT_CONTRACTS_FINISHED] test_only=1 cases=%1 failures=%2 workers=%3 physical_delivery=NOT_RUN", m_iCases, m_iFailures, workers.Count()));
	}
}

modded class AICF_LogisticsService
{
	protected bool m_bAICFVehicleLimitProbed;

	override void Update(SCR_CampaignFaction us, SCR_CampaignFaction ussr, bool graphReady)
	{
		super.Update(us, ussr, graphReady);
		string enabled;
		if (!Replication.IsServer() || m_bAICFVehicleLimitProbed || !graphReady ||
			!System.GetCLIParam("aicfVehicleLimitProbe", enabled) || enabled != "1") return;
		AICF_LogisticsWorker seed;
		foreach (AICF_LogisticsWorker w : m_Registry.m_aWorkers)
		{
			if (w.m_bEligible && AICF_LogisticsDepotRegistry.Live(w) && !w.m_Lease)
			{
				seed = w;
				break;
			}
		}
		if (!seed) return;
		m_bAICFVehicleLimitProbed = true;
		AICF_LogisticsVehicleLimitContracts contracts = new AICF_LogisticsVehicleLimitContracts();
		contracts.Run(m_Registry, seed, m_Config);
		Stop();
		GetGame().RequestClose();
	}
}
