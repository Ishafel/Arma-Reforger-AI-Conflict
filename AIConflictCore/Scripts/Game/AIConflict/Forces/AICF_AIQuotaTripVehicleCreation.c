// All catalog candidates in one acquisition attempt share this pure receipt.
class AICF_AIQuotaTripFactoryAttempt
{
	AICF_VehicleRequestState m_Request;
	int m_iRequestGeneration;
	int m_iAttemptAtMs;
	int m_iAttemptBefore;
	int m_iTotalBefore;
	bool m_bCommitted;

	bool Commit(int nowMs)
	{
		if (!m_Request || m_Request.GetRequestGeneration() != m_iRequestGeneration) return false;
		if (nowMs < m_iAttemptAtMs || m_Request.IsDeadlineReached(nowMs)) return false;
		if (m_bCommitted)
			return m_Request.GetAttemptCount() == m_iAttemptBefore + 1 && m_Request.GetTotalAttemptCount() == m_iTotalBefore + 1;
		if (m_Request.GetAttemptCount() != m_iAttemptBefore || m_Request.GetTotalAttemptCount() != m_iTotalBefore) return false;
		if (!m_Request.BeginAttempt(nowMs)) return false;
		m_bCommitted = true;
		return true;
	}
}

class AICF_AIQuotaTripVehicleCreation : AICF_AIQuotaVehicleCreationGuard
{
	protected AICF_TransportTrip m_Trip;
	protected AICF_VehicleRequestState m_Request;
	protected AICF_VehicleSpawnPlan m_Plan;
	protected AICF_VehicleSpawner m_Spawner;
	protected AICF_ConflictAdapter m_ConflictAdapter;
	protected AICF_StrategicAssignmentSnapshot m_Assignment;
	protected AICF_GroupSlot m_ManagedSlot;
	protected AICF_VehicleSpawnSiteSelection m_Selection;
	protected AICF_VehicleSpawnSiteReservation m_Site;
	protected ref AICF_AIQuotaCatalogIdentity m_BaseIdentity;
	protected SCR_CampaignMilitaryBaseComponent m_Base;
	protected SCR_CampaignFaction m_BaseFaction;
	protected vector m_vSpawnPosition;
	protected int m_iAssignmentRevision;
	protected int m_iIntentRevision;
	protected int m_iPresetRevision;
	protected int m_iRequestGeneration;
	protected int m_iTripGeneration;
	protected int m_iGroupGeneration;
	protected int m_iTransitions;
	protected int m_iDeadline;
	protected ref AICF_AIQuotaTripFactoryAttempt m_Attempt;

	static AICF_AIQuotaTripVehicleCreation Capture(AICF_TransportTrip trip, AICF_VehicleRequestState request,
		AICF_VehicleSpawner spawner, AICF_ConflictAdapter conflictAdapter)
	{
		if (!trip || !request || !spawner || !conflictAdapter || trip.GetRequestState() != request || !trip.GetAssignment()) return null;
		SCR_AIGroup group = trip.GetAssignment().GetGroup();
		if (!group) return null;
		AICF_AIWorldQuotaService service = AICF_AIWorldQuotaService.GetAuthorityForWorld(group.GetWorld());
		if (!service || !service.GetQuotaController()) return null;
		AICF_AIQuotaTripVehicleCreation guard = new AICF_AIQuotaTripVehicleCreation();
		guard.m_Trip = trip;
		guard.m_Request = request;
		guard.m_Plan = request.GetSpawnPlan();
		guard.m_Spawner = spawner;
		guard.m_ConflictAdapter = conflictAdapter;
		guard.m_Assignment = trip.GetAssignment();
		guard.m_ManagedSlot = service.GetQuotaController().FindManagedInfantrySlot(group);
		guard.m_iAssignmentRevision = guard.m_Assignment.GetAssignmentRevision();
		guard.m_iIntentRevision = guard.m_Assignment.GetStrategicIntentRevision();
		guard.m_iPresetRevision = trip.GetVehiclePresetRevision();
		if (!guard.m_Plan || !guard.m_Plan.IsValid() || !guard.m_ManagedSlot) return null;
		guard.m_Selection = guard.m_Plan.GetSelection();
		guard.m_Site = guard.m_Plan.GetReservation();
		if (!guard.m_Selection || !guard.m_Selection.m_Base || !guard.m_Site) return null;
		guard.m_Base = guard.m_Selection.m_Base;
		guard.m_BaseFaction = SCR_CampaignFaction.Cast(guard.m_Base.GetFaction());
		guard.m_BaseIdentity = AICF_AIQuotaCatalogIdentity.Capture(guard.m_Base.GetOwner(), false);
		guard.m_vSpawnPosition = guard.m_Selection.m_vPosition;
		guard.m_iRequestGeneration = request.GetRequestGeneration();
		guard.m_iTripGeneration = trip.GetTripGeneration();
		guard.m_iGroupGeneration = trip.GetGroupGeneration();
		guard.m_iTransitions = trip.GetTransitionCount();
		guard.m_iDeadline = request.GetAbsoluteDeadlineMs();
		if (!guard.Initialize(service, AICF_EAIQuotaBucket.FLEET, 1, trip.GetFactionKey(), group)) return null;
		return guard;
	}

	override bool IsDomainCurrent()
	{
		if (!m_Controller || !m_Trip || !m_Controller.IsAIQuotaVehicleTripCurrent(m_Trip)) return false;
		if (!m_Assignment || m_Trip.GetAssignment() != m_Assignment || !m_Assignment.IsValid() ||
			m_Assignment.GetAssignmentRevision() != m_iAssignmentRevision || m_Assignment.GetStrategicIntentRevision() != m_iIntentRevision) return false;
		if (!m_ManagedSlot || m_Controller.FindManagedInfantrySlot(m_Assignment.GetGroup()) != m_ManagedSlot ||
			m_ManagedSlot.GetStrategicAssignmentRevision() != m_iAssignmentRevision ||
			m_ManagedSlot.GetStrategicIntentRevision() != m_iIntentRevision || m_ManagedSlot.GetVehiclePresetRevision() != m_iPresetRevision) return false;
		if (m_Trip.GetVehiclePresetRevision() != m_iPresetRevision || (m_Trip.GetVehiclePresetSlot() && !m_Trip.IsVehiclePresetCurrent())) return false;
		if (m_Trip.GetRequestState() != m_Request || !m_Request || m_Request.GetRequestGeneration() != m_iRequestGeneration) return false;
		if (m_Trip.GetPhase() != AICF_ETransportTripPhase.SPAWN_COMMIT || m_Trip.GetTransitionCount() != m_iTransitions) return false;
		if (m_Request.GetAbsoluteDeadlineMs() != m_iDeadline || m_iDeadline <= 0 || m_Request.IsDeadlineReached(System.GetTickCount())) return false;
		if (!m_Plan || m_Plan.GetSelection() != m_Selection || m_Plan.GetReservation() != m_Site || !m_Spawner ||
			!m_Spawner.IsSiteReservationCurrentReadOnly(m_Site, m_Trip, m_Request, System.GetTickCount())) return false;
		if (!m_Selection || m_Selection.m_Base != m_Base || m_Selection.m_vPosition != m_vSpawnPosition || !m_Base ||
			!m_BaseIdentity || !m_BaseIdentity.IsCurrent() || m_Base.GetOwner() != m_BaseIdentity.m_Entity ||
			!m_BaseFaction || m_Base.GetFaction() != m_BaseFaction || m_BaseFaction.GetFactionKey() != m_sFaction) return false;
		if (!m_ConflictAdapter || !m_ConflictAdapter.GetSpawnRejectionReason(m_Base, m_BaseFaction).IsEmpty()) return false;
		return m_Trip.GetTripGeneration() == m_iTripGeneration && m_Trip.GetGroupGeneration() == m_iGroupGeneration &&
			m_Trip.GetFactionKey() == m_sFaction && m_Request.GetSpawnPlan() == m_Plan && m_Plan && m_Plan.IsValid();
	}

	bool PrepareAttempt(int nowMs)
	{
		if (!IsCurrent() || !m_Request.CanAttempt(nowMs)) return false;
		m_Attempt = new AICF_AIQuotaTripFactoryAttempt();
		m_Attempt.m_Request = m_Request;
		m_Attempt.m_iRequestGeneration = m_iRequestGeneration;
		m_Attempt.m_iAttemptAtMs = nowMs;
		m_Attempt.m_iAttemptBefore = m_Request.GetAttemptCount();
		m_Attempt.m_iTotalBefore = m_Request.GetTotalAttemptCount();
		return true;
	}

	AICF_AIQuotaTripFactoryAttempt GetAttempt() { return m_Attempt; }
	void AdoptAttempt(AICF_AIQuotaTripFactoryAttempt attempt) { m_Attempt = attempt; }

	override bool CommitNativeAttempt(int nowMs)
	{
		// Project value reads only. All native/virtual domain reads happen in
		// MarkExternalNativeCallStarted before this synchronous final commit.
		if (m_bCancelled || m_bQuarantined || m_bBound || !m_Trip || !m_Assignment || !m_Request || !m_Plan || !m_ManagedSlot) return false;
		if (m_Trip.GetTripGeneration() != m_iTripGeneration || m_Trip.GetGroupGeneration() != m_iGroupGeneration ||
			m_ManagedSlot.GetGroup() != m_Assignment.GetGroup() || m_ManagedSlot.GetSpawnGeneration() != m_iGroupGeneration) return false;
		if (m_Trip.GetPhase() != AICF_ETransportTripPhase.SPAWN_COMMIT || m_Trip.GetTransitionCount() != m_iTransitions ||
			m_Trip.GetAssignment() != m_Assignment || m_Trip.GetRequestState() != m_Request || m_Request.GetSpawnPlan() != m_Plan) return false;
		if (m_Request.GetAbsoluteDeadlineMs() != m_iDeadline || nowMs >= m_iDeadline ||
			m_ManagedSlot.GetStrategicAssignmentRevision() != m_iAssignmentRevision || m_ManagedSlot.GetStrategicIntentRevision() != m_iIntentRevision ||
			m_ManagedSlot.GetVehiclePresetRevision() != m_iPresetRevision) return false;
		if (!m_Spawner || m_Plan.GetSelection() != m_Selection || m_Plan.GetReservation() != m_Site ||
			!m_Spawner.IsSiteReservationCurrentReadOnly(m_Site, m_Trip, m_Request, nowMs)) return false;
		return m_Attempt && m_Attempt.Commit(nowMs);
	}
}
