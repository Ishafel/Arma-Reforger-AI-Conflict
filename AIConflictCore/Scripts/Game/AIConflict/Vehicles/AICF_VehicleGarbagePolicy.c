// Lease-owned permission; static registry and native entities are weak references.
// Native garbage may not bypass cap-held CleanupManager clearance ownership.
class AICF_VehicleGarbagePermission
{
	protected AICF_FactionFleet m_Fleet;
	protected AICF_VehicleLease m_Lease;
	protected Vehicle m_Vehicle;
	protected EntityID m_EntityId;
	protected string m_sRpl;
	protected FactionKey m_sFaction;
	protected int m_iSlot;
	protected int m_iGroup;
	protected int m_iTrip;
	protected int m_iLease;
	protected int m_iVehicle;
	protected bool m_bStopped;
	protected bool m_bEndedTracking;

	void AICF_VehicleGarbagePermission(AICF_FactionFleet fleet, AICF_VehicleLease lease)
	{
		m_Fleet = fleet; m_Lease = lease;
		m_Vehicle = lease.GetVehicle(); m_EntityId = lease.GetEntityId(); m_sRpl = lease.GetRplId();
		m_sFaction = lease.GetFactionKey(); m_iSlot = lease.GetSlotId();
		m_iGroup = lease.GetGroupGeneration(); m_iTrip = lease.GetTripGeneration();
		m_iLease = lease.GetLeaseGeneration(); m_iVehicle = lease.GetVehicleGeneration();
	}

	bool MatchesCapturedEntity(IEntity entity)
	{
		if (!Replication.IsServer() || !GetGame() || !entity || entity != m_Vehicle ||
			!entity.GetWorld() || entity.GetID() != m_EntityId || m_EntityId == EntityID.INVALID)
			return false;
		if (!AICF_VehicleBoardingMutationFence.IsAuthoritativeReplicatedEntity(entity) ||
			AICF_VehicleBehaviorBinding.ComponentRpl(entity) != m_sRpl || m_sRpl.IsEmpty() || m_sRpl == "NONE")
			return false;
		return true;
	}

	bool MatchesPhysicalIdentity(IEntity entity)
	{
		if (!MatchesCapturedEntity(entity) || !m_Lease || !m_Fleet) return false;
		return m_Lease.GetVehicle() == m_Vehicle && m_Lease.MatchesIdentity(m_sFaction, m_iSlot, m_iGroup,
			m_iLease, m_iVehicle, m_EntityId, m_sRpl) &&
			m_Lease.MatchesTripIdentity(m_sFaction, m_iSlot, m_iGroup, m_iTrip);
	}

	bool CanProtect(IEntity entity)
	{
		return !m_bStopped && MatchesPhysicalIdentity(entity) && m_Fleet.OwnsVehicleLease(m_Lease) && m_Lease.IsCapActive();
	}

	bool EndedTracking() { return m_bEndedTracking; }

	void RecordVeto(string phase)
	{
		m_bEndedTracking = true;
		string details = BuildIdentity();
		details += " phase=" + phase;
		details += " " + AICF_VehicleGarbagePolicy.Describe(m_Vehicle);
		AICF_Stage3Diagnostics.Info("VEHICLE_GARBAGE_VETO", details);
	}

	protected string BuildIdentity()
	{
		return string.Format("faction=%1 slot=%2 group_generation=%3 trip_generation=%4 lease_generation=%5 vehicle_generation=%6 entity=%7 rpl=%8",
			m_sFaction, m_iSlot, m_iGroup, m_iTrip, m_iLease, m_iVehicle, m_EntityId, m_sRpl);
	}

	void Stop()
	{
		if (m_bStopped) return;
		m_bStopped = true;
		// Remove before native Insert, so this permission cannot veto restoration.
		AICF_VehicleGarbagePolicy.Unregister(this);
		if (!m_bEndedTracking || !MatchesCapturedEntity(m_Vehicle) || AICF_VehicleGarbagePolicy.Find(m_Vehicle)) return;
		SCR_GarbageSystem garbage = SCR_GarbageSystem.GetByEntityWorld(m_Vehicle);
		if (!garbage || garbage.IsInserted(m_Vehicle)) return;
		bool requested = garbage.Insert(m_Vehicle);
		string details = BuildIdentity();
		details += string.Format(" native_insert_requested=%1 native_inserted=%2 lifetime_policy=DEFAULT", requested, garbage.IsInserted(m_Vehicle));
		AICF_Stage3Diagnostics.Info("VEHICLE_GARBAGE_RESTORE", details);
	}

	void ~AICF_VehicleGarbagePermission() { Stop(); }
}

class AICF_VehicleGarbagePolicy
{
	// No ref element: the lease owns permission lifetime, registry owns nothing.
	protected static ref array<AICF_VehicleGarbagePermission> s_aPermissions = {};

	static AICF_VehicleGarbagePermission Register(AICF_FactionFleet fleet, AICF_VehicleLease lease)
	{
		if (!Replication.IsServer() || !fleet || !lease || !fleet.OwnsVehicleLease(lease) || !lease.GetVehicle()) return null;
		AICF_VehicleGarbagePermission permission = new AICF_VehicleGarbagePermission(fleet, lease);
		if (!permission.CanProtect(lease.GetVehicle())) return null;
		s_aPermissions.Insert(permission);
		return permission;
	}

	static void Unregister(AICF_VehicleGarbagePermission permission)
	{
		if (s_aPermissions) s_aPermissions.RemoveItem(permission);
	}

	static AICF_VehicleGarbagePermission Find(IEntity entity)
	{
		if (!Replication.IsServer() || !entity || !s_aPermissions) return null;
		foreach (AICF_VehicleGarbagePermission permission : s_aPermissions)
			if (permission && permission.CanProtect(entity)) return permission;
		return null;
	}

	static string Describe(Vehicle vehicle)
	{
		if (!vehicle || !vehicle.GetWorld()) return "gc_available=0";
		SCR_GarbageSystem garbage = SCR_GarbageSystem.GetByEntityWorld(vehicle);
		if (!garbage) return "gc_available=0";
		AICF_VehicleGarbagePermission permission = Find(vehicle);
		return string.Format("gc_available=1 gc_inserted=%1 gc_lifetime_s=%2 gc_remaining_s=%3 gc_permission_current=%4 gc_veto_ended_tracking=%5",
			garbage.IsInserted(vehicle), garbage.GetLifetime(vehicle), garbage.GetRemainingLifetime(vehicle),
			permission != null, permission && permission.EndedTracking());
	}
}

modded class SCR_GarbageSystem
{
	override protected float OnInsertRequested(IEntity entity, float lifetime)
	{
		AICF_VehicleGarbagePermission permission = AICF_VehicleGarbagePolicy.Find(entity);
		if (permission)
		{
			// Short circuit before stock subscribes to native resource invokers.
			permission.RecordVeto("INSERT");
			return -1;
		}
		return super.OnInsertRequested(entity, lifetime);
	}

	override protected bool OnBeforeDelete(IEntity entity)
	{
		AICF_VehicleGarbagePermission permission = AICF_VehicleGarbagePolicy.Find(entity);
		if (permission)
		{
			permission.RecordVeto("DELETE");
			return false;
		}
		return super.OnBeforeDelete(entity);
	}
}
