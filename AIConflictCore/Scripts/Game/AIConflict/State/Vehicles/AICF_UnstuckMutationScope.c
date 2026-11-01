// Неизменяемый владелец одной синхронной короткой коррекции, включая UNKNOWN hint.
class AICF_UnstuckMutationScope
{
	protected AICF_TransportTrip m_Trip;
	protected AICF_StrategicAssignmentSnapshot m_Assignment;
	protected AICF_VehicleLease m_Lease;
	protected AICF_MatchController m_Controller;
	protected SCR_AIGroup m_Group;
	protected Vehicle m_Vehicle;
	protected EntityID m_GroupId;
	protected EntityID m_VehicleId;
	protected RplComponent m_GroupRpl;
	protected RplComponent m_VehicleRpl;
	protected string m_sGroupRplId;
	protected string m_sVehicleRplId;
	protected string m_sScope;
	protected vector m_vOrigin;
	protected vector m_vGoal;
	protected int m_iCapturedAtMs;

	protected string Scope()
	{
		return string.Format("%1:%2:%3:%4:%5:%6:%7:%8", m_Trip.GetFactionKey(), m_Trip.GetSlotId(),
			m_Trip.GetGroupGeneration(), m_Trip.GetTripGeneration(), m_Lease.GetLeaseGeneration(), m_Lease.GetVehicleGeneration(),
			m_Assignment.GetAssignmentRevision(), m_Assignment.GetStrategicIntentRevision());
	}
	static AICF_UnstuckMutationScope Capture(AICF_TransportTrip trip, Vehicle vehicle)
	{
		if (!Replication.IsServer() || !trip || !trip.IsValid() || !trip.GetAssignment() || !trip.GetLease() || !vehicle) return null;
		AICF_UnstuckMutationScope scope = new AICF_UnstuckMutationScope();
		scope.m_Trip = trip;
		scope.m_Assignment = trip.GetAssignment();
		scope.m_Lease = trip.GetLease();
		scope.m_Group = scope.m_Assignment.GetGroup();
		scope.m_Vehicle = vehicle;
		if (!scope.m_Group || scope.m_Lease.GetVehicle() != vehicle) return null;
		scope.m_Controller = AICF_MatchController.GetActiveController();
		if (!scope.m_Controller) return null;
		scope.m_GroupId = scope.m_Group.GetID();
		scope.m_VehicleId = vehicle.GetID();
		scope.m_GroupRpl = RplComponent.Cast(scope.m_Group.FindComponent(RplComponent));
		scope.m_VehicleRpl = RplComponent.Cast(vehicle.FindComponent(RplComponent));
		if (!scope.m_GroupRpl || !scope.m_VehicleRpl) return null;
		scope.m_sGroupRplId = scope.m_GroupRpl.Id().ToString();
		scope.m_sVehicleRplId = scope.m_VehicleRpl.Id().ToString();
		scope.m_sScope = scope.Scope();
		scope.m_vOrigin = vehicle.GetOrigin();
		scope.m_vGoal = scope.m_Assignment.GetTargetPosition();
		scope.m_iCapturedAtMs = System.GetTickCount();
		if (!scope.IsCurrent()) return null;
		return scope;
	}
	bool IsCurrent()
	{
		int now = System.GetTickCount();
		if (!Replication.IsServer() || !m_Trip || !m_Trip.IsValid() || m_Trip.IsTerminal() || m_Trip.IsDeadlineReached(now)) return false;
		if (!m_Assignment || !m_Lease) return false;
		if (m_Trip.GetPhase() != AICF_ETransportTripPhase.TRANSIT || m_iCapturedAtMs <= 0 || now < m_iCapturedAtMs || now - m_iCapturedAtMs > 3000) return false;
		if (m_Trip.GetAssignment() != m_Assignment || m_Trip.GetLease() != m_Lease || !m_Group || !m_Vehicle || !m_Controller) return false;
		if (!m_GroupRpl || !m_VehicleRpl) return false;
		if (AICF_MatchController.GetActiveController() != m_Controller || m_Assignment.GetGroup() != m_Group || m_Lease.GetVehicle() != m_Vehicle) return false;
		AICF_GroupSlot slot = m_Controller.FindManagedInfantrySlot(m_Group);
		if (!slot || !slot.IsCombatReady() || slot.GetGroup() != m_Group || slot.GetSpawnGeneration() != m_Trip.GetGroupGeneration()) return false;
		if (slot.GetSlotId() != m_Trip.GetSlotId()) return false;
		if (m_Trip.GetVehiclePreset() && (!m_Trip.IsVehiclePresetCurrent() || m_Trip.GetVehiclePresetSlot() != slot)) return false;
		if (slot.GetStrategicAssignmentRevision() != m_Assignment.GetAssignmentRevision() || slot.GetStrategicIntentRevision() != m_Assignment.GetStrategicIntentRevision()) return false;
		if (slot.GetTargetKind() != m_Assignment.GetTargetKind() || slot.GetTargetBase() != m_Assignment.GetTargetBase()) return false;
		if (m_Assignment.GetTargetPosition() != m_vGoal || slot.GetTargetPosition() != m_vGoal || Scope() != m_sScope) return false;
		if (!m_Group.GetFaction() || m_Group.GetFaction().GetFactionKey() != m_Trip.GetFactionKey()) return false;
		if (!m_Lease.HasPhysicalAsset() || m_Lease.GetState() != AICF_EVehicleLeaseState.ACTIVE) return false;
		if (!m_Lease.MatchesTripIdentity(m_Trip.GetFactionKey(), m_Trip.GetSlotId(), m_Trip.GetGroupGeneration(), m_Trip.GetTripGeneration())) return false;
		if (!m_Lease.MatchesEntityIdentity(m_Vehicle, m_Vehicle.GetID(), m_VehicleRpl.Id().ToString())) return false;
		if (m_Group.GetID() != m_GroupId || m_Vehicle.GetID() != m_VehicleId || m_Vehicle.GetOrigin() != m_vOrigin) return false;
		if (RplComponent.Cast(m_Group.FindComponent(RplComponent)) != m_GroupRpl || RplComponent.Cast(m_Vehicle.FindComponent(RplComponent)) != m_VehicleRpl) return false;
		if (!AICF_VehicleBoardingMutationFence.IsAuthoritativeReplicatedEntity(m_Group) || !AICF_VehicleBoardingMutationFence.IsAuthoritativeReplicatedEntity(m_Vehicle)) return false;
		return m_GroupRpl.Id().ToString() == m_sGroupRplId && m_VehicleRpl.Id().ToString() == m_sVehicleRplId;
	}
}
