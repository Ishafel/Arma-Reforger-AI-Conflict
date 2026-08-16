// Single faction aggregate for cap, leases, accepted generation and world pool.
// There are deliberately no faction-specific parallel arrays.
class AICF_FactionFleet
{
	static const int HARD_MAX_ACTIVE_OR_RESERVED = 10;

	protected FactionKey m_sFactionKey;
	protected int m_iMaximumActiveOrReserved;
	protected int m_iNextLeaseGeneration;
	protected int m_iAcceptedVehicleGeneration;
	protected ref array<ref AICF_VehicleLease> m_aLeases = {};
	protected ref array<ref AICF_WorldPoolAsset> m_aWorldPool = {};

	void AICF_FactionFleet(FactionKey factionKey, int maximumActiveOrReserved = 10)
	{
		m_sFactionKey = factionKey;
		m_iMaximumActiveOrReserved = Math.Max(0, maximumActiveOrReserved);
		if (m_iMaximumActiveOrReserved > HARD_MAX_ACTIVE_OR_RESERVED)
			m_iMaximumActiveOrReserved = HARD_MAX_ACTIVE_OR_RESERVED;
	}

	FactionKey GetFactionKey() { return m_sFactionKey; }
	int GetMaximumActiveOrReserved() { return m_iMaximumActiveOrReserved; }
	int GetAcceptedVehicleGeneration() { return m_iAcceptedVehicleGeneration; }
	int GetWorldPoolCount() { return m_aWorldPool.Count(); }
	int GetLeaseCount() { return m_aLeases.Count(); }

	int GetActiveOrReservedCount()
	{
		int count;
		foreach (AICF_VehicleLease lease : m_aLeases)
		{
			if (lease && lease.IsCapActive())
				count++;
		}
		return count;
	}

	int GetReservedCount()
	{
		int count;
		foreach (AICF_VehicleLease lease : m_aLeases)
		{
			if (lease && lease.GetState() == AICF_EVehicleLeaseState.RESERVED)
				count++;
		}
		return count;
	}

	int GetActiveCount()
	{
		int count;
		foreach (AICF_VehicleLease lease : m_aLeases)
		{
			if (lease && lease.GetState() == AICF_EVehicleLeaseState.ACTIVE)
				count++;
		}
		return count;
	}

	int GetReleasePendingCount()
	{
		int count;
		foreach (AICF_VehicleLease lease : m_aLeases)
		{
			if (lease && lease.GetState() == AICF_EVehicleLeaseState.RELEASE_PENDING)
				count++;
		}
		return count;
	}

	int GetFailedClosedCount()
	{
		int count;
		foreach (AICF_VehicleLease lease : m_aLeases)
		{
			if (lease && lease.GetState() == AICF_EVehicleLeaseState.FAILED_CLOSED)
				count++;
		}
		return count;
	}

	int GetPhysicalLeaseCount()
	{
		int count;
		foreach (AICF_VehicleLease lease : m_aLeases)
		{
			if (lease && lease.GetVehicle())
				count++;
		}
		return count;
	}

	AICF_VehicleLease GetLease(int index)
	{
		if (!m_aLeases.IsIndexValid(index))
			return null;
		return m_aLeases[index];
	}

	AICF_WorldPoolAsset GetWorldPoolAsset(int index)
	{
		if (!m_aWorldPool.IsIndexValid(index))
			return null;
		return m_aWorldPool[index];
	}

	AICF_VehicleLease FindLeaseForSlot(int slotId, int groupGeneration = -1)
	{
		foreach (AICF_VehicleLease lease : m_aLeases)
		{
			if (!lease || lease.GetSlotId() != slotId)
				continue;
			if (groupGeneration < 0 || lease.GetGroupGeneration() == groupGeneration)
				return lease;
		}
		return null;
	}

	bool HasLeaseForSlot(int slotId)
	{
		return FindLeaseForSlot(slotId) != null;
	}

	bool CanReserveLease(AICF_StrategicAssignmentSnapshot assignment)
	{
		return assignment && assignment.IsValid() &&
			assignment.GetFactionKey() == m_sFactionKey &&
			!HasLeaseForSlot(assignment.GetSlotId()) &&
			GetActiveOrReservedCount() < m_iMaximumActiveOrReserved;
	}

	bool TryReserveLease(
		AICF_StrategicAssignmentSnapshot assignment,
		int tripGeneration,
		out AICF_VehicleLease lease)
	{
		lease = null;
		if (tripGeneration <= 0 || !CanReserveLease(assignment))
			return false;
		int leaseGeneration = m_iNextLeaseGeneration + 1;
		AICF_VehicleLease candidate = new AICF_VehicleLease(
			m_sFactionKey,
			assignment.GetSlotId(),
			assignment.GetGroupGeneration(),
			tripGeneration,
			leaseGeneration);
		if (!candidate)
			return false;
		m_aLeases.Insert(candidate);
		m_iNextLeaseGeneration = leaseGeneration;
		lease = candidate;
		return true;
	}

	// Rejected spawn/surface candidates never call this method and therefore do
	// not advance accepted vehicle generation.
	bool BindReservedLeaseVehicle(
		AICF_VehicleLease lease,
		Vehicle vehicle,
		string rplId,
		ResourceName prefab,
		AICF_EVehicleKind kind,
		int capacity,
		vector origin)
	{
		if (!OwnsLease(lease) || lease.GetState() != AICF_EVehicleLeaseState.RESERVED)
			return false;
		int acceptedGeneration = m_iAcceptedVehicleGeneration + 1;
		string lifecycleId = string.Format(
			"AICF-%1-%2-%3-%4",
			m_sFactionKey,
			lease.GetSlotId(),
			lease.GetLeaseGeneration(),
			acceptedGeneration);
		if (!lease.BindAcceptedVehicle(
			acceptedGeneration,
			lifecycleId,
			vehicle,
			rplId,
			prefab,
			kind,
			capacity,
			origin))
			return false;
		m_iAcceptedVehicleGeneration = acceptedGeneration;
		return true;
	}

	bool ReleaseLease(
		AICF_VehicleLease lease,
		bool clearanceSafe,
		string cleanupTrigger,
		vector origin,
		out AICF_WorldPoolAsset releasedAsset)
	{
		return ReleaseLeaseAt(
			lease,
			clearanceSafe,
			cleanupTrigger,
			origin,
			System.GetTickCount(),
			releasedAsset);
	}

	bool ReleaseLeaseAt(
		AICF_VehicleLease lease,
		bool clearanceSafe,
		string cleanupTrigger,
		vector origin,
		int releaseAtMs,
		out AICF_WorldPoolAsset releasedAsset)
	{
		releasedAsset = null;
		if (!OwnsLease(lease) ||
			!lease.BeginRelease(clearanceSafe, cleanupTrigger, origin, releaseAtMs))
			return false;
		AICF_WorldPoolAsset asset = new AICF_WorldPoolAsset(lease);
		if (!asset || !asset.IsValid())
		{
			lease.FailClosed();
			return false;
		}
		if (!lease.MarkReleased() ||
			!lease.RelinquishReleasedVehicleReference(asset.GetVehicle()))
		{
			lease.FailClosed();
			return false;
		}
		m_aLeases.RemoveItem(lease);
		m_aWorldPool.Insert(asset);
		releasedAsset = asset;
		return true;
	}

	// Exact retained-clearance transfer. This is intentionally separate from
	// normal release: only a Fleet-owned FAILED_CLOSED lease may enter it, after
	// CleanupManager has repeated every safety/identity check and proved five
	// seconds of continuous clearance. The cap remains held until this succeeds.
	bool ReleaseRetainedLeaseAt(
		AICF_VehicleLease lease,
		bool clearanceSafe,
		string cleanupTrigger,
		vector origin,
		int releaseAtMs,
		out AICF_WorldPoolAsset releasedAsset)
	{
		releasedAsset = null;
		if (!OwnsLease(lease) ||
			lease.GetState() != AICF_EVehicleLeaseState.FAILED_CLOSED ||
			!lease.BeginRetainedRelease(
				clearanceSafe,
				cleanupTrigger,
				origin,
				releaseAtMs))
		{
			return false;
		}
		AICF_WorldPoolAsset asset = new AICF_WorldPoolAsset(lease);
		if (!asset || !asset.IsValid())
		{
			lease.FailClosed();
			return false;
		}
		if (!lease.MarkReleased() ||
			!lease.RelinquishReleasedVehicleReference(asset.GetVehicle()))
		{
			lease.FailClosed();
			return false;
		}
		m_aLeases.RemoveItem(lease);
		m_aWorldPool.Insert(asset);
		releasedAsset = asset;
		return true;
	}

	// Retires an unusable/destroyed asset from the active lease registry without
	// advertising it as a reusable fleet asset. CleanupManager owns the returned
	// quarantine object and may delete it only after its independent safety gate.
	bool RetireLeaseAt(
		AICF_VehicleLease lease,
		bool clearanceSafe,
		string cleanupTrigger,
		vector origin,
		int releaseAtMs,
		out AICF_VehicleRetirementAsset retirementAsset)
	{
		retirementAsset = null;
		if (!OwnsLease(lease) ||
			!lease.BeginRelease(clearanceSafe, cleanupTrigger, origin, releaseAtMs))
		{
			return false;
		}
		AICF_VehicleRetirementAsset retiredAsset = new AICF_VehicleRetirementAsset(lease);
		if (!retiredAsset || !retiredAsset.IsValid())
		{
			lease.FailClosed();
			return false;
		}
		if (!lease.MarkReleased() ||
			!lease.RelinquishReleasedVehicleReference(retiredAsset.GetVehicle()))
		{
			lease.FailClosed();
			return false;
		}
		m_aLeases.RemoveItem(lease);
		retirementAsset = retiredAsset;
		return true;
	}

	// Unusable retained assets use the same exact FAILED_CLOSED recovery gate,
	// but transfer into quarantine rather than player-available world-pool stock.
	bool RetireRetainedLeaseAt(
		AICF_VehicleLease lease,
		bool clearanceSafe,
		string cleanupTrigger,
		vector origin,
		int releaseAtMs,
		out AICF_VehicleRetirementAsset retirementAsset)
	{
		retirementAsset = null;
		if (!OwnsLease(lease) ||
			lease.GetState() != AICF_EVehicleLeaseState.FAILED_CLOSED ||
			!lease.BeginRetainedRelease(
				clearanceSafe,
				cleanupTrigger,
				origin,
				releaseAtMs))
		{
			return false;
		}
		AICF_VehicleRetirementAsset retiredAsset = new AICF_VehicleRetirementAsset(lease);
		if (!retiredAsset || !retiredAsset.IsValid())
		{
			lease.FailClosed();
			return false;
		}
		if (!lease.MarkReleased() ||
			!lease.RelinquishReleasedVehicleReference(retiredAsset.GetVehicle()))
		{
			lease.FailClosed();
			return false;
		}
		m_aLeases.RemoveItem(lease);
		retirementAsset = retiredAsset;
		return true;
	}

	bool ReleaseEmptyReservation(AICF_VehicleLease lease)
	{
		if (!OwnsLease(lease) || lease.GetState() != AICF_EVehicleLeaseState.RESERVED)
			return false;
		if (!lease.CancelReservation())
			return false;
		m_aLeases.RemoveItem(lease);
		return true;
	}

	bool AddWorldPoolAsset(AICF_WorldPoolAsset asset)
	{
		if (!asset || !asset.IsValid() || asset.GetFactionKey() != m_sFactionKey ||
			m_aWorldPool.Find(asset) >= 0)
			return false;
		m_aWorldPool.Insert(asset);
		return true;
	}

	bool RemoveWorldPoolAsset(AICF_WorldPoolAsset expected)
	{
		if (!expected || m_aWorldPool.Find(expected) < 0)
			return false;
		m_aWorldPool.RemoveItem(expected);
		return true;
	}

	protected bool OwnsLease(AICF_VehicleLease lease)
	{
		return lease && lease.GetFactionKey() == m_sFactionKey &&
			m_aLeases.Find(lease) >= 0;
	}
}

// Registry keyed by faction identity. It replaces all US/USSR-specific runtime,
// cooldown, generation, cap and pool arrays with one concrete aggregate type.
class AICF_FleetRegistry
{
	protected ref array<ref AICF_FactionFleet> m_aFleets = {};

	AICF_FactionFleet FindFleet(FactionKey factionKey)
	{
		foreach (AICF_FactionFleet fleet : m_aFleets)
		{
			if (fleet && fleet.GetFactionKey() == factionKey)
				return fleet;
		}
		return null;
	}

	AICF_FactionFleet GetOrCreateFleet(FactionKey factionKey, int maximumActiveOrReserved = 10)
	{
		if (factionKey.IsEmpty())
			return null;
		AICF_FactionFleet fleet = FindFleet(factionKey);
		if (fleet)
			return fleet;
		fleet = new AICF_FactionFleet(factionKey, maximumActiveOrReserved);
		m_aFleets.Insert(fleet);
		return fleet;
	}

	int GetFleetCount() { return m_aFleets.Count(); }

	AICF_FactionFleet GetFleet(int index)
	{
		if (!m_aFleets.IsIndexValid(index))
			return null;
		return m_aFleets[index];
	}
}
