// Ownership выводится только из живого typed fleet registry. Фракция водителя,
// prefab машины и имя группы не определяют bucket.
class AICF_AIQuotaVehicleOwner
{
	AICF_AIWorldQuotaService m_Service;
	AICF_MatchController m_Controller;
	int m_iEpoch;
	ref AICF_AIQuotaVehicleEntity m_Identity;
	AICF_FactionFleet m_Fleet;
	AICF_VehicleLease m_Lease;
	AICF_EAIQuotaBucket m_Role;
	AICF_EAIQuotaBucket m_Bucket;
	string m_sLease;
	bool m_bCensusOnly;
	int m_iBootstrapSerial;

	static string Fingerprint(AICF_VehicleLease lease)
	{
		if (!lease) return string.Empty;
		return string.Format("%1/%2/%3/%4/%5/%6/%7/%8", lease.GetFactionKey(), lease.GetSlotId(),
			lease.GetGroupGeneration(), lease.GetTripGeneration(), lease.GetLeaseGeneration(),
			lease.GetVehicleGeneration(), lease.GetVehicleLifecycleId(), lease.GetState());
	}

	static bool Resolve(AICF_MatchController controller, IEntity entity, out AICF_FactionFleet foundFleet,
		out AICF_VehicleLease foundLease, out AICF_EAIQuotaBucket foundRole, out AICF_EAIQuotaBucket bucket)
	{
		foundFleet = null;
		foundLease = null;
		foundRole = AICF_EAIQuotaBucket.STOCK_VEHICLES;
		bucket = AICF_EAIQuotaBucket.STOCK_VEHICLES;
		Vehicle vehicle = Vehicle.Cast(entity);
		if (!controller || !vehicle || !AICF_AIQuotaVehicleEntity.Capture(vehicle)) return false;
		array<AICF_FactionFleet> fleets = {};
		array<AICF_EAIQuotaBucket> roles = {};
		if (!controller.CollectAIQuotaFleets(fleets, roles) || fleets.Count() != roles.Count() || fleets.Count() > AICF_AIQuotaMath.MAX_TOTAL_UNITS) return false;
		int examined;
		for (int index; index < fleets.Count(); index++)
		{
			AICF_FactionFleet fleet = fleets[index];
			AICF_EAIQuotaBucket role = roles[index];
			if (!fleet || (role != AICF_EAIQuotaBucket.FLEET && role != AICF_EAIQuotaBucket.FIA_EXTRA)) return false;
			int leaseCount = fleet.GetLeaseCount();
			if (leaseCount < 0 || leaseCount > AICF_AIQuotaMath.MAX_TOTAL_UNITS - examined) return false;
			examined += leaseCount;
			for (int leaseIndex; leaseIndex < leaseCount; leaseIndex++)
			{
				AICF_VehicleLease lease = fleet.GetLease(leaseIndex);
				if (!lease) return false;
				if (lease.GetVehicle() != vehicle)
				{
					// A stale physical pointer with the same immutable identity is
					// ambiguous custody, not proof of a free STOCK vehicle.
					RplComponent rpl = RplComponent.Cast(vehicle.FindComponent(RplComponent));
					if (rpl && lease.GetEntityId() == vehicle.GetID() && lease.GetRplId() == rpl.Id().ToString()) return false;
					continue;
				}
				if (foundLease || !lease.IsCapActive() || lease.GetFactionKey() != fleet.GetFactionKey()) return false;
				RplComponent vehicleRpl = RplComponent.Cast(vehicle.FindComponent(RplComponent));
				if (!vehicleRpl || !lease.MatchesEntityIdentity(vehicle, vehicle.GetID(), vehicleRpl.Id().ToString())) return false;
				if (lease.GetGroupGeneration() <= 0 || lease.GetTripGeneration() <= 0 || lease.GetLeaseGeneration() <= 0 ||
					lease.GetVehicleGeneration() <= 0 || lease.GetVehicleLifecycleId().IsEmpty() || lease.GetFactionKey().IsEmpty()) return false;
				foundFleet = fleet;
				foundLease = lease;
				foundRole = role;
				bucket = role;
				if (role == AICF_EAIQuotaBucket.FLEET && lease.GetSlotId() >= AICF_LogisticsConfig.SERVICE_SLOT_FIRST)
					bucket = AICF_EAIQuotaBucket.COMBAT;
			}
			if (fleet.GetLeaseCount() != leaseCount) return false;
		}
		return true;
	}

	static AICF_AIQuotaVehicleOwner Capture(AICF_AIWorldQuotaService service, IEntity vehicle)
	{
		if (!service || !service.IsOwnerCurrent() || !service.IsConfigured()) return null;
		AICF_AIQuotaVehicleOwner owner = new AICF_AIQuotaVehicleOwner();
		owner.m_Service = service;
		owner.m_Controller = service.GetQuotaController();
		owner.m_iEpoch = service.GetEpoch();
		owner.m_Identity = AICF_AIQuotaVehicleEntity.Capture(vehicle);
		if (!owner.m_Identity || !Resolve(owner.m_Controller, vehicle, owner.m_Fleet, owner.m_Lease, owner.m_Role, owner.m_Bucket)) return null;
		owner.m_sLease = Fingerprint(owner.m_Lease);
		if (!owner.IsCurrent()) return null;
		return owner;
	}

	static AICF_AIQuotaVehicleOwner CaptureForCensus(AICF_AIWorldQuotaService service, IEntity vehicle)
	{
		if (!service) return null;
		AICF_MatchController controller = service.GetQuotaVehicleBootstrapController();
		if (!service.IsQuotaVehicleBootstrapCurrent(controller, service.GetQuotaVehicleBootstrapSerial())) return null;
		AICF_AIQuotaVehicleOwner owner = new AICF_AIQuotaVehicleOwner();
		owner.m_Service = service;
		owner.m_Controller = controller;
		owner.m_bCensusOnly = true;
		owner.m_iBootstrapSerial = service.GetQuotaVehicleBootstrapSerial();
		owner.m_Identity = AICF_AIQuotaVehicleEntity.Capture(vehicle);
		if (!owner.m_Identity || !Resolve(controller, vehicle, owner.m_Fleet, owner.m_Lease, owner.m_Role, owner.m_Bucket)) return null;
		owner.m_sLease = Fingerprint(owner.m_Lease);
		if (!owner.IsCurrent()) return null;
		return owner;
	}

	bool IsCurrent()
	{
		if (!m_Service || !m_Identity || !m_Identity.IsCurrent()) return false;
		if (m_bCensusOnly)
		{
			if (!m_Service.IsQuotaVehicleBootstrapCurrent(m_Controller, m_iBootstrapSerial)) return false;
		}
		else
		{
			if (!m_Service.IsOwnerCurrent() || !m_Service.IsConfigured()) return false;
			if (m_Service.GetQuotaController() != m_Controller || m_Service.GetEpoch() != m_iEpoch) return false;
		}
		AICF_FactionFleet fleet;
		AICF_VehicleLease lease;
		AICF_EAIQuotaBucket role;
		AICF_EAIQuotaBucket bucket;
		if (!Resolve(m_Controller, m_Identity.m_Entity, fleet, lease, role, bucket)) return false;
		return m_Identity.IsCurrent() && fleet == m_Fleet && lease == m_Lease && role == m_Role && bucket == m_Bucket && Fingerprint(lease) == m_sLease;
	}

	bool CanEnter()
	{
		if (m_bCensusOnly || !IsCurrent()) return false;
		return !m_Lease || m_Lease.GetState() == AICF_EVehicleLeaseState.ACTIVE;
	}
}
