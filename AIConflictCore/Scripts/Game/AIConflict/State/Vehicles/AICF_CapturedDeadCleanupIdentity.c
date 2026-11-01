// CAPTURED_DEAD_CLEANUP: только исходный actor из admission, не текущий roster.
// Разрешает cleanup native detach; не разрешает release/delete или gameplay.
class AICF_CapturedDeadCleanupIdentity
{
	// Diagnostic no-op: original decision is already evaluated; no native reads.
	protected void AICF_ObserveCapturedCleanupDecision(AICF_VehicleCleanupJob job, string methodName, string stage, bool actual)
	{
	}

	protected ref AICF_TripRetirementIdentity m_Admission;
	protected ref AICF_TransportTrip m_Trip;
	protected AICF_VehicleCleanupJob m_Job;
	protected ref AICF_FactionFleet m_Fleet;
	protected ref AICF_VehicleLease m_Lease;
	protected ref AICF_VehicleCleanupFence m_Fence;
	protected AICF_VehicleCleanupFence m_OriginalFence;
	protected BaseWorld m_World;
	protected Vehicle m_Vehicle;
	protected RplComponent m_VehicleRpl;
	protected ref AICF_StrategicAssignmentSnapshot m_Assignment;
	protected AICF_GroupSlot m_Slot;
	protected int m_iAssignmentRevision;
	protected int m_iIntentRevision;
	protected int m_iBaseRevision;
	protected string m_sOperation;
	protected int m_iTransition;
	protected string m_sCausation;

	static AICF_CapturedDeadCleanupIdentity Capture(AICF_TripRetirementIdentity admission, AICF_TransportTrip trip, AICF_VehicleCleanupJob job)
	{
		if (!admission || !admission.MatchesTerminalCleanupAdmission(trip) || !job || !job.m_Lease ||
			trip.GetLease() != job.m_Lease || !job.m_Fence || !GetGame() || !GetGame().GetWorld()) return null;
		AICF_CapturedDeadCleanupIdentity identity = new AICF_CapturedDeadCleanupIdentity();
		identity.m_Admission = admission;
		identity.m_Trip = trip;
		identity.m_Job = job;
		identity.m_Fleet = job.m_Fleet;
		identity.m_Lease = job.m_Lease;
		identity.m_OriginalFence = job.m_Fence;
		identity.m_Fence = new AICF_VehicleCleanupFence();
		if (!identity.m_Fence.InitializeFromLease(job.m_Lease, job.m_sActionToken)) return null;
		identity.m_World = GetGame().GetWorld();
		identity.m_Vehicle = job.m_Lease.GetVehicle();
		if (!identity.m_Vehicle) return null;
		identity.m_VehicleRpl = RplComponent.Cast(identity.m_Vehicle.FindComponent(RplComponent));
		if (!admission.MatchesTerminalCleanupAdmission(trip) || !trip.GetAssignment()) return null;
		identity.m_Assignment = trip.GetAssignment();
		identity.m_Slot = trip.GetVehiclePresetSlot();
		identity.m_iAssignmentRevision = identity.m_Assignment.GetAssignmentRevision();
		identity.m_iIntentRevision = identity.m_Assignment.GetStrategicIntentRevision();
		identity.m_iBaseRevision = identity.m_Assignment.GetBaseRevision();
		identity.m_sOperation = trip.GetOperationId();
		identity.m_iTransition = trip.GetTransitionCount();
		identity.m_sCausation = trip.GetCausationId();
		if (!identity.IsCurrent(job, identity.m_Vehicle)) return null;
		return identity;
	}

	bool IsCurrent(AICF_VehicleCleanupJob job, Vehicle vehicle)
	{
		if (!Replication.IsServer() || !GetGame() || !m_World || GetGame().GetWorld() != m_World)
		{
			AICF_ObserveCapturedCleanupDecision(job, "IsCurrent", "CURRENT_01", false);
			return false;
		}
		if (!job || job != m_Job || job.m_bCompleted || job.m_bReleaseComplete || !m_Admission || !m_Trip)
		{
			AICF_ObserveCapturedCleanupDecision(job, "IsCurrent", "CURRENT_02", false);
			return false;
		}
		if (!m_Assignment || !m_Admission.MatchesTerminalCleanupAdmission(m_Trip) || m_Trip.GetAssignment() != m_Assignment)
		{
			AICF_ObserveCapturedCleanupDecision(job, "IsCurrent", "CURRENT_03", false);
			return false;
		}
		if (m_Assignment.GetAssignmentRevision() != m_iAssignmentRevision || m_Assignment.GetStrategicIntentRevision() != m_iIntentRevision ||
			m_Assignment.GetBaseRevision() != m_iBaseRevision)
		{
			AICF_ObserveCapturedCleanupDecision(job, "IsCurrent", "CURRENT_04", false);
			return false;
		}
		if (m_Trip.GetTransitionCount() != m_iTransition || m_Trip.GetCausationId() != m_sCausation ||
			m_Trip.GetOperationId() != m_sOperation || job.m_sOperationId != m_sOperation)
		{
			AICF_ObserveCapturedCleanupDecision(job, "IsCurrent", "CURRENT_05", false);
			return false;
		}
		// Cleanup продолжает владеть работой после снятия lease с terminal trip.
		if (m_Trip.GetLease() && m_Trip.GetLease() != m_Lease)
		{
			AICF_ObserveCapturedCleanupDecision(job, "IsCurrent", "CURRENT_06", false);
			return false;
		}
		if (!m_Lease || !m_Fleet || job.m_Fleet != m_Fleet || job.m_Lease != m_Lease || job.m_Group != m_Assignment.GetGroup())
		{
			AICF_ObserveCapturedCleanupDecision(job, "IsCurrent", "CURRENT_07", false);
			return false;
		}
		if (m_Fleet.GetFactionKey() != m_Trip.GetFactionKey() ||
			!m_Lease.MatchesTripIdentity(m_Trip.GetFactionKey(), m_Trip.GetSlotId(), m_Trip.GetGroupGeneration(), m_Trip.GetTripGeneration()))
		{
			AICF_ObserveCapturedCleanupDecision(job, "IsCurrent", "CURRENT_08", false);
			return false;
		}
		if (job.m_Fence != m_OriginalFence || !m_Fence || !m_Fence.MatchesFence(job.m_Fence) || !m_Fence.MatchesLease(m_Lease))
		{
			AICF_ObserveCapturedCleanupDecision(job, "IsCurrent", "CURRENT_09", false);
			return false;
		}
		if (job.m_sActionToken != m_Fence.GetActionToken() || !m_Lease.IsCapActive() ||
			m_Fleet.FindLeaseForSlot(m_Fence.GetSlotId(), m_Fence.GetGroupGeneration()) != m_Lease)
		{
			AICF_ObserveCapturedCleanupDecision(job, "IsCurrent", "CURRENT_10", false);
			return false;
		}
		if (m_Lease.GetState() != AICF_EVehicleLeaseState.ACTIVE && m_Lease.GetState() != AICF_EVehicleLeaseState.FAILED_CLOSED)
		{
			AICF_ObserveCapturedCleanupDecision(job, "IsCurrent", "CURRENT_11", false);
			return false;
		}
		if (!vehicle || vehicle != m_Vehicle || vehicle.GetWorld() != m_World || m_Lease.GetVehicle() != vehicle)
		{
			AICF_ObserveCapturedCleanupDecision(job, "IsCurrent", "CURRENT_12", false);
			return false;
		}
		if (vehicle.GetID() != m_Fence.GetEntityId() || m_World.FindEntityByID(m_Fence.GetEntityId()) != vehicle ||
			vehicle.FindComponent(RplComponent) != m_VehicleRpl)
		{
			AICF_ObserveCapturedCleanupDecision(job, "IsCurrent", "CURRENT_13", false);
			return false;
		}
		if (!AICF_VehicleBoardingMutationFence.IsAuthoritativeReplicatedEntity(vehicle) || !m_VehicleRpl ||
			m_VehicleRpl.Id().ToString() != m_Fence.GetRplId())
		{
			AICF_ObserveCapturedCleanupDecision(job, "IsCurrent", "CURRENT_14", false);
			return false;
		}
		if (!Replication.IsServer() || !GetGame() || GetGame().GetWorld() != m_World || job != m_Job)
		{
			AICF_ObserveCapturedCleanupDecision(job, "IsCurrent", "CURRENT_15", false);
			return false;
		}
		if (job.m_bCompleted || job.m_bReleaseComplete || job.m_Lease != m_Lease || job.m_Fleet != m_Fleet || job.m_Fence != m_OriginalFence)
		{
			AICF_ObserveCapturedCleanupDecision(job, "IsCurrent", "CURRENT_16", false);
			return false;
		}
		bool actualCurrent = MatchesOwnerContext(job);
		AICF_ObserveCapturedCleanupDecision(job, "IsCurrent", "CURRENT_FINAL", actualCurrent);
		return actualCurrent;
	}

	// Только project-owned getters: повтор после последнего native seat getter.
	bool MatchesOwnerContext(AICF_VehicleCleanupJob job)
	{
		if (!job || job != m_Job || job.m_bCompleted || job.m_bReleaseComplete || !job.m_Group)
		{
			AICF_ObserveCapturedCleanupDecision(job, "MatchesOwnerContext", "OWNER_01", false);
			return false;
		}
		if (!m_Trip || !m_Assignment)
		{
			AICF_ObserveCapturedCleanupDecision(job, "MatchesOwnerContext", "OWNER_02", false);
			return false;
		}
		if (!m_Trip.IsTerminal() || !m_Trip.OwnsRetirementIdentity(m_Admission) || m_Trip.GetAssignment() != m_Assignment)
		{
			AICF_ObserveCapturedCleanupDecision(job, "MatchesOwnerContext", "OWNER_03", false);
			return false;
		}
		if (m_Trip.GetTransitionCount() != m_iTransition || m_Trip.GetCausationId() != m_sCausation ||
			m_Trip.GetOperationId() != m_sOperation || job.m_sOperationId != m_sOperation)
		{
			AICF_ObserveCapturedCleanupDecision(job, "MatchesOwnerContext", "OWNER_04", false);
			return false;
		}
		if (m_Assignment.GetAssignmentRevision() != m_iAssignmentRevision || m_Assignment.GetStrategicIntentRevision() != m_iIntentRevision ||
			m_Assignment.GetBaseRevision() != m_iBaseRevision)
		{
			AICF_ObserveCapturedCleanupDecision(job, "MatchesOwnerContext", "OWNER_05", false);
			return false;
		}
		AICF_GroupSlot slot = m_Trip.GetVehiclePresetSlot();
		if (!slot || slot != m_Slot || slot.GetSlotId() != m_Trip.GetSlotId() || slot.GetGroup() != job.m_Group ||
			slot.GetSpawnGeneration() != m_Trip.GetGroupGeneration() || job.m_Group != m_Assignment.GetGroup())
		{
			AICF_ObserveCapturedCleanupDecision(job, "MatchesOwnerContext", "OWNER_06", false);
			return false;
		}
		if (m_Trip.GetLease() && m_Trip.GetLease() != m_Lease)
		{
			AICF_ObserveCapturedCleanupDecision(job, "MatchesOwnerContext", "OWNER_07", false);
			return false;
		}
		if (!m_Lease || !m_Fleet || job.m_Lease != m_Lease || job.m_Fleet != m_Fleet || job.m_Fence != m_OriginalFence)
		{
			AICF_ObserveCapturedCleanupDecision(job, "MatchesOwnerContext", "OWNER_08", false);
			return false;
		}
		if (!m_Fence.MatchesFence(job.m_Fence) || !m_Fence.MatchesLease(m_Lease) || !m_Lease.IsCapActive())
		{
			AICF_ObserveCapturedCleanupDecision(job, "MatchesOwnerContext", "OWNER_09", false);
			return false;
		}
		if (m_Lease.GetState() != AICF_EVehicleLeaseState.ACTIVE && m_Lease.GetState() != AICF_EVehicleLeaseState.FAILED_CLOSED)
		{
			AICF_ObserveCapturedCleanupDecision(job, "MatchesOwnerContext", "OWNER_10", false);
			return false;
		}
		if (job.m_sActionToken != m_Fence.GetActionToken() || m_Lease.GetVehicle() != m_Vehicle)
		{
			AICF_ObserveCapturedCleanupDecision(job, "MatchesOwnerContext", "OWNER_11", false);
			return false;
		}
		bool actualOwner = m_Fleet.FindLeaseForSlot(m_Fence.GetSlotId(), m_Fence.GetGroupGeneration()) == m_Lease;
		AICF_ObserveCapturedCleanupDecision(job, "MatchesOwnerContext", "OWNER_FINAL", actualOwner);
		return actualOwner;
	}

	int GetCapturedMemberCount() { return m_Admission.GetCapturedMemberCount(); }
	string GetCapturedMemberIdString(int index) { return m_Admission.GetCapturedMemberIdString(index); }

	ChimeraCharacter Resolve(AICF_VehicleCleanupJob job, Vehicle vehicle, int index)
	{
		if (!IsCurrent(job, vehicle)) return null;
		ChimeraCharacter character = m_Admission.ResolveTerminalDeadActor(index);
		if (!character || !IsCurrent(job, vehicle)) return null;
		return character;
	}

	// Последний native seat getter обязан предшествовать последнему actor proof.
	// После ResolveTerminalDeadActorForCommit разрешены только project-owned getters.
	ChimeraCharacter ResolveForCommit(AICF_VehicleCleanupJob job, Vehicle vehicle, int index,
		ChimeraCharacter expectedActor, CompartmentAccessComponent expectedAccess, BaseCompartmentSlot expectedSeat)
	{
		if (!IsCurrent(job, vehicle) || !expectedActor || !expectedAccess || !expectedSeat) return null;
		ChimeraCharacter character = m_Admission.ResolveTerminalDeadActorForCommit(index, expectedActor, vehicle, expectedAccess, expectedSeat);
		if (!character || character != expectedActor || !expectedAccess || !expectedSeat || !MatchesOwnerContext(job)) return null;
		return character;
	}
}
