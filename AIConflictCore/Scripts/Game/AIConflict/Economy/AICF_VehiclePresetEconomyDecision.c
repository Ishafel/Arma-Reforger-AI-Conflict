enum AICF_EVehiclePresetEconomyDecision
{
	INVALID_CONTEXT,
	AFFORDABLE,
	INSUFFICIENT_AVAILABLE_SUPPLIES
}

// Immutable quote at the authoritative pre-asset boundary. This is neither a
// purchase nor a completion receipt; the controller still owns foot handoff.
class AICF_VehiclePresetEconomyDecision
{
	static AICF_EVehiclePresetEconomyDecision ClassifyQuote(int cost, int stock, int obligations)
	{
		if (cost <= 0 || stock < 0 || obligations < 0) return AICF_EVehiclePresetEconomyDecision.INVALID_CONTEXT;
		if (stock - obligations < cost) return AICF_EVehiclePresetEconomyDecision.INSUFFICIENT_AVAILABLE_SUPPLIES;
		return AICF_EVehiclePresetEconomyDecision.AFFORDABLE;
	}

	protected AICF_TransportTrip m_Trip;
	protected AICF_FactionFleet m_Fleet;
	protected AICF_VehicleLease m_ReservedLease;
	protected bool m_bHadReservedLease;
	protected int m_iReservedLeaseGeneration;
	protected AICF_VehicleRequestState m_Request;
	protected AICF_VehicleSpawnPlan m_Plan;
	protected SCR_CampaignMilitaryBaseComponent m_Base;
	protected IEntity m_BaseOwner;
	protected EntityID m_BaseId;
	protected string m_sBaseRpl;
	protected string m_sOperation;
	protected ResourceName m_Prefab;
	protected int m_iGroupGeneration;
	protected int m_iTripGeneration;
	protected int m_iRequestGeneration;
	protected int m_iPresetRevision;
	protected int m_iStock;
	protected int m_iObligations;
	protected int m_iCost;
	protected AICF_EVehiclePresetEconomyDecision m_Status;

	void AICF_VehiclePresetEconomyDecision(AICF_TransportTrip trip,
		SCR_CampaignMilitaryBaseComponent base, int cost, int stock, int obligations,
		AICF_EVehiclePresetEconomyDecision status, AICF_FactionFleet fleet = null)
	{
		m_Trip = trip;
		m_Fleet = fleet;
		m_Base = base;
		m_iCost = cost;
		m_iStock = stock;
		m_iObligations = obligations;
		m_Status = status;
		if (!trip || !base || !trip.GetVehiclePreset() || !trip.GetRequestState()) return;
		m_ReservedLease = trip.GetLease();
		m_bHadReservedLease = m_ReservedLease != null;
		if (m_ReservedLease) m_iReservedLeaseGeneration = m_ReservedLease.GetLeaseGeneration();
		m_Request = trip.GetRequestState();
		m_Plan = m_Request.GetSpawnPlan();
		m_sOperation = trip.GetOperationId();
		m_iGroupGeneration = trip.GetGroupGeneration();
		m_iTripGeneration = trip.GetTripGeneration();
		m_iRequestGeneration = m_Request.GetRequestGeneration();
		m_iPresetRevision = trip.GetVehiclePresetRevision();
		m_Prefab = trip.GetVehiclePreset().m_sPrefab;
		m_BaseOwner = base.GetOwner();
		if (!m_BaseOwner) return;
		m_BaseId = m_BaseOwner.GetID();
		m_sBaseRpl = AICF_VehicleBehaviorBinding.ComponentRpl(m_BaseOwner);
	}

	AICF_EVehiclePresetEconomyDecision GetStatus() { return m_Status; }
	SCR_CampaignMilitaryBaseComponent GetBase() { return m_Base; }
	int GetStock() { return m_iStock; }
	int GetObligations() { return m_iObligations; }
	int GetCost() { return m_iCost; }
	AICF_FactionFleet GetFleet() { return m_Fleet; }
	bool HadReservedLease() { return m_bHadReservedLease; }
	int GetReservedLeaseGeneration() { return m_iReservedLeaseGeneration; }

	static bool IsOwnUnusedReservation(AICF_TransportTrip trip, AICF_FactionFleet fleet,
		AICF_VehicleLease lease, int leaseGeneration)
	{
		if (!Replication.IsServer() || !trip || !fleet || !lease || leaseGeneration <= 0) return false;
		if (fleet.GetFactionKey() != trip.GetFactionKey() || !fleet.OwnsVehicleLease(lease) ||
			fleet.FindLeaseForSlot(trip.GetSlotId()) != lease) return false;
		if (!lease.MatchesTripIdentity(trip.GetFactionKey(), trip.GetSlotId(), trip.GetGroupGeneration(), trip.GetTripGeneration()) ||
			lease.GetLeaseGeneration() != leaseGeneration || lease.GetState() != AICF_EVehicleLeaseState.RESERVED) return false;
		if (lease.HasPhysicalAsset() || lease.GetVehicle() || lease.GetVehicleGeneration() != 0 ||
			lease.GetEntityId() != EntityID.INVALID || lease.GetCleanupSnapshot()) return false;
		return lease.GetEntityIdString().IsEmpty() && lease.GetRplId().IsEmpty() &&
			lease.GetPrefab().IsEmpty() && lease.GetVehicleLifecycleId().IsEmpty();
	}

	bool HasCurrentLeaseFence(AICF_TransportTrip trip)
	{
		if (!m_Fleet || m_Fleet.GetFactionKey() != trip.GetFactionKey()) return false;
		if (!m_bHadReservedLease)
			return !trip.HasLease() && !m_Fleet.FindLeaseForSlot(trip.GetSlotId());
		return m_ReservedLease && trip.GetLease() == m_ReservedLease &&
			IsOwnUnusedReservation(trip, m_Fleet, m_ReservedLease, m_iReservedLeaseGeneration);
	}

	bool MatchesIdentity(AICF_TransportTrip trip)
	{
		if (!Replication.IsServer() || !trip || trip != m_Trip || !trip.IsValid() || !trip.IsVehiclePresetCurrent()) return false;
		if (trip.GetOperationId() != m_sOperation || trip.GetGroupGeneration() != m_iGroupGeneration ||
			trip.GetTripGeneration() != m_iTripGeneration || trip.GetVehiclePresetRevision() != m_iPresetRevision) return false;
		if (!trip.GetVehiclePreset() || trip.GetVehiclePreset().m_sPrefab != m_Prefab || !m_Request ||
			m_iRequestGeneration <= 0 || trip.GetRequestState() != m_Request) return false;
		return m_Request.GetRequestGeneration() == m_iRequestGeneration && m_Status == ClassifyQuote(m_iCost, m_iStock, m_iObligations);
	}

	bool IsCurrentPreflight(AICF_TransportTrip trip)
	{
		if (!MatchesIdentity(trip) || trip.GetPhase() != AICF_ETransportTripPhase.SPAWN_COMMIT ||
			trip.IsVehiclePresetDebited() || !HasCurrentLeaseFence(trip)) return false;
		if (m_Request.GetSpawnPlan() != m_Plan || !m_Plan || !m_Plan.GetSelection() || m_Plan.GetSelection().m_Base != m_Base) return false;
		if (!m_Base || m_Base.GetOwner() != m_BaseOwner || !m_BaseOwner || m_BaseOwner.GetID() != m_BaseId) return false;
		if (!GetGame() || !GetGame().GetWorld() || GetGame().GetWorld().FindEntityByID(m_BaseId) != m_BaseOwner ||
			!AICF_VehicleBoardingMutationFence.IsAuthoritativeReplicatedEntity(m_BaseOwner)) return false;
		return !m_sBaseRpl.IsEmpty() && m_sBaseRpl != "NONE" &&
			AICF_VehicleBehaviorBinding.ComponentRpl(m_BaseOwner) == m_sBaseRpl && m_Base.GetSupplies() == m_iStock;
	}

	string Details()
	{
		string fields = string.Format("operation_id=%1 group_generation=%2 trip_generation=%3 request_generation=%4 preset_revision=%5 base=%6 base_entity=%7 base_rpl=%8 prefab=%9",
			m_sOperation, m_iGroupGeneration, m_iTripGeneration, m_iRequestGeneration, m_iPresetRevision,
			AICF_Stage1Diagnostics.BaseKey(m_Base), m_BaseId, m_sBaseRpl, m_Prefab);
		return fields + string.Format(" quote=%1 stock=%2 obligations=%3 available=%4 decision=%5 cap_reserved=%6 captured_lease_generation=%7 main_world_spawned=0 vehicle_debit=0",
			m_iCost, m_iStock, m_iObligations, m_iStock - m_iObligations, m_Status, m_bHadReservedLease, m_iReservedLeaseGeneration);
	}
}
