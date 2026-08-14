// Immutable identity captured before a lease drops its entity reference. This
// snapshot is the only acceptable input to destructive cleanup confirmation.
class AICF_VehicleCleanupSnapshot
{
	protected FactionKey m_sFactionKey;
	protected int m_iSlotId;
	protected int m_iGroupGeneration;
	protected int m_iTripGeneration;
	protected int m_iLeaseGeneration;
	protected string m_sVehicleLifecycleId;
	protected int m_iVehicleGeneration;
	protected EntityID m_LastEntityId = EntityID.INVALID;
	protected string m_sLastEntityId;
	protected string m_sLastRplId;
	protected vector m_vLastOrigin;
	protected ResourceName m_Prefab;
	protected int m_iReleaseAtMs;
	protected string m_sCleanupTrigger;

	void AICF_VehicleCleanupSnapshot(
		FactionKey factionKey,
		int slotId,
		int groupGeneration,
		int tripGeneration,
		int leaseGeneration,
		string vehicleLifecycleId,
		int vehicleGeneration,
		EntityID lastEntityId,
		string lastEntityIdString,
		string lastRplId,
		vector lastOrigin,
		ResourceName prefab,
		int releaseAtMs,
		string cleanupTrigger)
	{
		m_sFactionKey = factionKey;
		m_iSlotId = slotId;
		m_iGroupGeneration = groupGeneration;
		m_iTripGeneration = tripGeneration;
		m_iLeaseGeneration = leaseGeneration;
		m_sVehicleLifecycleId = vehicleLifecycleId;
		m_iVehicleGeneration = vehicleGeneration;
		m_LastEntityId = lastEntityId;
		m_sLastEntityId = lastEntityIdString;
		m_sLastRplId = lastRplId;
		m_vLastOrigin = lastOrigin;
		m_Prefab = prefab;
		m_iReleaseAtMs = releaseAtMs;
		m_sCleanupTrigger = cleanupTrigger;
	}

	FactionKey GetFactionKey() { return m_sFactionKey; }
	int GetSlotId() { return m_iSlotId; }
	int GetGroupGeneration() { return m_iGroupGeneration; }
	int GetTripGeneration() { return m_iTripGeneration; }
	int GetLeaseGeneration() { return m_iLeaseGeneration; }
	string GetVehicleLifecycleId() { return m_sVehicleLifecycleId; }
	int GetVehicleGeneration() { return m_iVehicleGeneration; }
	EntityID GetLastEntityId() { return m_LastEntityId; }
	string GetLastEntityIdString() { return m_sLastEntityId; }
	string GetLastRplId() { return m_sLastRplId; }
	vector GetLastOrigin() { return m_vLastOrigin; }
	ResourceName GetPrefab() { return m_Prefab; }
	int GetReleaseAtMs() { return m_iReleaseAtMs; }
	string GetCleanupTrigger() { return m_sCleanupTrigger; }

	bool IsComplete()
	{
		return !m_sFactionKey.IsEmpty() && m_iSlotId >= 0 &&
			m_iGroupGeneration > 0 && m_iTripGeneration > 0 &&
			m_iLeaseGeneration > 0 && !m_sVehicleLifecycleId.IsEmpty() &&
			m_iVehicleGeneration > 0 &&
			m_LastEntityId != EntityID.INVALID && !m_sLastEntityId.IsEmpty() &&
			!m_sLastRplId.IsEmpty() && m_sLastRplId != "NONE" && !m_Prefab.IsEmpty() &&
			m_iReleaseAtMs > 0 && !m_sCleanupTrigger.IsEmpty();
	}

	bool Matches(EntityID entityId, string rplId)
	{
		return IsComplete() && entityId == m_LastEntityId && rplId == m_sLastRplId;
	}
}

// Exclusive reservation of one faction cap unit and, after accepted binding,
// one physical vehicle. State/generation mutation is called only by Fleet.
class AICF_VehicleLease
{
	protected FactionKey m_sFactionKey;
	protected int m_iSlotId;
	protected int m_iGroupGeneration;
	protected int m_iTripGeneration;
	protected int m_iLeaseGeneration;
	protected int m_iVehicleGeneration;
	protected string m_sVehicleLifecycleId;
	protected AICF_EVehicleLeaseState m_State;
	protected Vehicle m_Vehicle;
	protected EntityID m_EntityId = EntityID.INVALID;
	protected string m_sEntityId;
	protected string m_sRplId;
	protected ResourceName m_Prefab;
	protected AICF_EVehicleKind m_Kind;
	protected int m_iCapacity;
	protected vector m_vLastKnownOrigin;
	protected ref AICF_VehicleCleanupSnapshot m_CleanupSnapshot;

	void AICF_VehicleLease(
		FactionKey factionKey,
		int slotId,
		int groupGeneration,
		int tripGeneration,
		int leaseGeneration)
	{
		m_sFactionKey = factionKey;
		m_iSlotId = slotId;
		m_iGroupGeneration = groupGeneration;
		m_iTripGeneration = tripGeneration;
		m_iLeaseGeneration = leaseGeneration;
		m_State = AICF_EVehicleLeaseState.RESERVED;
	}

	FactionKey GetFactionKey() { return m_sFactionKey; }
	int GetSlotId() { return m_iSlotId; }
	int GetGroupGeneration() { return m_iGroupGeneration; }
	int GetTripGeneration() { return m_iTripGeneration; }
	int GetLeaseGeneration() { return m_iLeaseGeneration; }
	int GetVehicleGeneration() { return m_iVehicleGeneration; }
	string GetVehicleLifecycleId() { return m_sVehicleLifecycleId; }
	AICF_EVehicleLeaseState GetState() { return m_State; }
	Vehicle GetVehicle() { return m_Vehicle; }
	EntityID GetEntityId() { return m_EntityId; }
	string GetEntityIdString() { return m_sEntityId; }
	string GetRplId() { return m_sRplId; }
	ResourceName GetPrefab() { return m_Prefab; }
	AICF_EVehicleKind GetKind() { return m_Kind; }
	int GetCapacity() { return m_iCapacity; }
	vector GetLastKnownOrigin() { return m_vLastKnownOrigin; }
	AICF_VehicleCleanupSnapshot GetCleanupSnapshot() { return m_CleanupSnapshot; }

	bool MatchesSlotIdentity(FactionKey factionKey, int slotId, int groupGeneration)
	{
		return factionKey == m_sFactionKey && slotId == m_iSlotId &&
			groupGeneration == m_iGroupGeneration;
	}

	bool MatchesTripIdentity(
		FactionKey factionKey,
		int slotId,
		int groupGeneration,
		int tripGeneration)
	{
		return MatchesSlotIdentity(factionKey, slotId, groupGeneration) &&
			tripGeneration == m_iTripGeneration;
	}

	bool MatchesIdentity(
		FactionKey factionKey,
		int slotId,
		int groupGeneration,
		int leaseGeneration,
		int vehicleGeneration,
		EntityID entityId,
		string rplId)
	{
		return MatchesSlotIdentity(factionKey, slotId, groupGeneration) &&
			leaseGeneration == m_iLeaseGeneration &&
			vehicleGeneration == m_iVehicleGeneration &&
			entityId == m_EntityId && rplId == m_sRplId;
	}

	bool MatchesEntityIdentity(Vehicle vehicle, EntityID entityId, string rplId)
	{
		return m_Vehicle && vehicle == m_Vehicle && entityId == m_EntityId &&
			vehicle.GetID() == m_EntityId && rplId == m_sRplId;
	}

	bool IsCapActive()
	{
		return m_State == AICF_EVehicleLeaseState.RESERVED ||
			m_State == AICF_EVehicleLeaseState.ACTIVE ||
			m_State == AICF_EVehicleLeaseState.RELEASE_PENDING ||
			m_State == AICF_EVehicleLeaseState.FAILED_CLOSED;
	}

	bool HasPhysicalAsset()
	{
		return m_State == AICF_EVehicleLeaseState.ACTIVE && m_Vehicle &&
			m_iVehicleGeneration > 0 && !m_sVehicleLifecycleId.IsEmpty() &&
			m_EntityId != EntityID.INVALID && !m_sRplId.IsEmpty();
	}

	// Fleet-only mutation. The caller advances its accepted generation only
	// after this method succeeds.
	bool BindAcceptedVehicle(
		int vehicleGeneration,
		string vehicleLifecycleId,
		Vehicle vehicle,
		string rplId,
		ResourceName prefab,
		AICF_EVehicleKind kind,
		int capacity,
		vector origin)
	{
		if (m_State != AICF_EVehicleLeaseState.RESERVED || vehicleGeneration <= 0 ||
			vehicleLifecycleId.IsEmpty() || !vehicle || rplId.IsEmpty() || rplId == "NONE" ||
			prefab.IsEmpty() || capacity < 1)
			return false;
		EntityID entityId = vehicle.GetID();
		if (entityId == EntityID.INVALID)
			return false;
		m_iVehicleGeneration = vehicleGeneration;
		m_sVehicleLifecycleId = vehicleLifecycleId;
		m_Vehicle = vehicle;
		m_EntityId = entityId;
		m_sEntityId = entityId.ToString();
		m_sRplId = rplId;
		m_Prefab = prefab;
		m_Kind = kind;
		m_iCapacity = capacity;
		m_vLastKnownOrigin = origin;
		m_State = AICF_EVehicleLeaseState.ACTIVE;
		return true;
	}

	bool BeginRelease(bool clearanceSafe, string trigger, vector origin, int releaseAtMs)
	{
		if (!clearanceSafe || m_State != AICF_EVehicleLeaseState.ACTIVE ||
			trigger.IsEmpty() || releaseAtMs <= 0 || !HasPhysicalAsset())
			return false;
		m_vLastKnownOrigin = origin;
		m_CleanupSnapshot = new AICF_VehicleCleanupSnapshot(
			m_sFactionKey,
			m_iSlotId,
			m_iGroupGeneration,
			m_iTripGeneration,
			m_iLeaseGeneration,
			m_sVehicleLifecycleId,
			m_iVehicleGeneration,
			m_EntityId,
			m_sEntityId,
			m_sRplId,
			origin,
			m_Prefab,
			releaseAtMs,
			trigger);
		if (!m_CleanupSnapshot.IsComplete())
		{
			m_CleanupSnapshot = null;
			return false;
		}
		m_State = AICF_EVehicleLeaseState.RELEASE_PENDING;
		return true;
	}

	bool MarkReleased()
	{
		if (m_State != AICF_EVehicleLeaseState.RELEASE_PENDING || !m_CleanupSnapshot)
			return false;
		m_State = AICF_EVehicleLeaseState.RELEASED;
		return true;
	}

	// Fleet-only ownership transfer. The immutable identity remains on the
	// released lease for audit, but the physical reference moves to exactly one
	// released-asset owner (world pool or retirement cleanup).
	bool RelinquishReleasedVehicleReference(Vehicle expectedVehicle)
	{
		if (m_State != AICF_EVehicleLeaseState.RELEASED || !expectedVehicle ||
			expectedVehicle != m_Vehicle || !m_CleanupSnapshot ||
			!m_CleanupSnapshot.Matches(m_EntityId, m_sRplId) ||
			expectedVehicle.GetID() != m_EntityId)
			return false;
		m_Vehicle = null;
		return true;
	}

	bool CancelReservation()
	{
		if (m_State != AICF_EVehicleLeaseState.RESERVED || m_Vehicle)
			return false;
		m_State = AICF_EVehicleLeaseState.RELEASED;
		return true;
	}

	void FailClosed()
	{
		m_State = AICF_EVehicleLeaseState.FAILED_CLOSED;
	}
}

// Physical asset outside the active/reserved AI cap. Cleanup may retain this
// object indefinitely when protected safety or identity confirmation fails.
class AICF_WorldPoolAsset
{
	protected FactionKey m_sFactionKey;
	protected string m_sVehicleLifecycleId;
	protected int m_iVehicleGeneration;
	protected Vehicle m_Vehicle;
	protected EntityID m_EntityId = EntityID.INVALID;
	protected string m_sRplId;
	protected ResourceName m_Prefab;
	protected AICF_EVehicleKind m_Kind;
	protected int m_iCapacity;
	protected vector m_vOrigin;
	protected ref AICF_VehicleCleanupSnapshot m_CleanupSnapshot;
	protected ref AICF_VehicleCleanupState m_CleanupState;

	void AICF_WorldPoolAsset(AICF_VehicleLease releasedLease)
	{
		if (!releasedLease)
			return;
		m_sFactionKey = releasedLease.GetFactionKey();
		m_sVehicleLifecycleId = releasedLease.GetVehicleLifecycleId();
		m_iVehicleGeneration = releasedLease.GetVehicleGeneration();
		m_Vehicle = releasedLease.GetVehicle();
		m_EntityId = releasedLease.GetEntityId();
		m_sRplId = releasedLease.GetRplId();
		m_Prefab = releasedLease.GetPrefab();
		m_Kind = releasedLease.GetKind();
		m_iCapacity = releasedLease.GetCapacity();
		m_vOrigin = releasedLease.GetLastKnownOrigin();
		m_CleanupSnapshot = releasedLease.GetCleanupSnapshot();
		m_CleanupState = new AICF_VehicleCleanupState();
	}

	FactionKey GetFactionKey() { return m_sFactionKey; }
	string GetVehicleLifecycleId() { return m_sVehicleLifecycleId; }
	int GetVehicleGeneration() { return m_iVehicleGeneration; }
	Vehicle GetVehicle() { return m_Vehicle; }
	EntityID GetEntityId() { return m_EntityId; }
	string GetRplId() { return m_sRplId; }
	ResourceName GetPrefab() { return m_Prefab; }
	AICF_EVehicleKind GetKind() { return m_Kind; }
	int GetCapacity() { return m_iCapacity; }
	vector GetOrigin() { return m_vOrigin; }
	AICF_VehicleCleanupSnapshot GetCleanupSnapshot() { return m_CleanupSnapshot; }
	AICF_VehicleCleanupState GetCleanupState() { return m_CleanupState; }

	bool IsValid()
	{
		return !m_sFactionKey.IsEmpty() && !m_sVehicleLifecycleId.IsEmpty() &&
			m_iVehicleGeneration > 0 && m_Vehicle && m_EntityId != EntityID.INVALID &&
			!m_sRplId.IsEmpty() && !m_Prefab.IsEmpty() && m_iCapacity > 0 &&
			m_CleanupSnapshot && m_CleanupSnapshot.IsComplete();
	}

	bool MatchesIdentity(EntityID entityId, string rplId)
	{
		return IsValid() && entityId == m_EntityId && rplId == m_sRplId &&
			m_Vehicle.GetID() == m_EntityId;
	}

	void ClearVehicleReferenceAfterDeleteRequest(EntityID expectedEntityId, string expectedRplId)
	{
		if (expectedEntityId != m_EntityId || expectedRplId != m_sRplId)
			return;
		m_Vehicle = null;
	}
}

// A destroyed or otherwise unusable physical vehicle is never represented by
// AICF_WorldPoolAsset. This quarantine owner retains only the live entity
// reference, the immutable pre-release identity and cleanup-local evidence.
// It has no capacity/kind API and therefore cannot be rebound as fleet stock.
class AICF_VehicleRetirementAsset
{
	protected Vehicle m_Vehicle;
	protected ref AICF_VehicleCleanupSnapshot m_CleanupSnapshot;
	protected ref AICF_VehicleCleanupState m_CleanupState;

	void AICF_VehicleRetirementAsset(AICF_VehicleLease releasingLease)
	{
		if (!releasingLease ||
			releasingLease.GetState() != AICF_EVehicleLeaseState.RELEASE_PENDING)
			return;
		m_Vehicle = releasingLease.GetVehicle();
		m_CleanupSnapshot = releasingLease.GetCleanupSnapshot();
		m_CleanupState = new AICF_VehicleCleanupState();
	}

	Vehicle GetVehicle() { return m_Vehicle; }
	AICF_VehicleCleanupSnapshot GetCleanupSnapshot() { return m_CleanupSnapshot; }
	AICF_VehicleCleanupState GetCleanupState() { return m_CleanupState; }

	FactionKey GetFactionKey()
	{
		if (!m_CleanupSnapshot)
			return "";
		return m_CleanupSnapshot.GetFactionKey();
	}

	string GetVehicleLifecycleId()
	{
		if (!m_CleanupSnapshot)
			return "";
		return m_CleanupSnapshot.GetVehicleLifecycleId();
	}

	int GetVehicleGeneration()
	{
		if (!m_CleanupSnapshot)
			return 0;
		return m_CleanupSnapshot.GetVehicleGeneration();
	}

	EntityID GetEntityId()
	{
		if (!m_CleanupSnapshot)
			return EntityID.INVALID;
		return m_CleanupSnapshot.GetLastEntityId();
	}

	string GetEntityIdString()
	{
		if (!m_CleanupSnapshot)
			return "";
		return m_CleanupSnapshot.GetLastEntityIdString();
	}

	string GetRplId()
	{
		if (!m_CleanupSnapshot)
			return "";
		return m_CleanupSnapshot.GetLastRplId();
	}

	ResourceName GetPrefab()
	{
		if (!m_CleanupSnapshot)
			return ResourceName.Empty;
		return m_CleanupSnapshot.GetPrefab();
	}

	vector GetOrigin()
	{
		if (!m_CleanupSnapshot)
			return vector.Zero;
		return m_CleanupSnapshot.GetLastOrigin();
	}

	bool IsValid()
	{
		return m_Vehicle && m_CleanupSnapshot && m_CleanupSnapshot.IsComplete() &&
			m_Vehicle.GetID() == m_CleanupSnapshot.GetLastEntityId();
	}

	bool MatchesLiveIdentity(EntityID entityId, string rplId)
	{
		return IsValid() && m_CleanupSnapshot.Matches(entityId, rplId) &&
			m_Vehicle.GetID() == entityId;
	}

	bool MatchesSnapshotIdentity(EntityID entityId, string rplId)
	{
		return m_CleanupSnapshot && m_CleanupSnapshot.Matches(entityId, rplId);
	}

	// CleanupManager calls this only after its repeated safety scan and
	// identity-safe delete request. Confirmation continues from the snapshot.
	bool ClearVehicleReferenceAfterDeleteRequest(EntityID expectedEntityId, string expectedRplId)
	{
		if (!MatchesLiveIdentity(expectedEntityId, expectedRplId))
			return false;
		m_Vehicle = null;
		return true;
	}
}
