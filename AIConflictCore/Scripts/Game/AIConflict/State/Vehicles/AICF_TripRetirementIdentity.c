// Отдельная identity для retirement после native удаления controller группы.
// Она не разрешает новый gameplay/order/boarding и не заменяет IsValid().
class AICF_TripRetirementMember
{
	ChimeraCharacter m_Character;
	EntityID m_Id;
	string m_sIdCanonical;
	string m_sRpl;
	bool m_bNativeDeadObserved;
	BaseWorld m_World;
	RplComponent m_Rpl;
	CharacterControllerComponent m_Controller;
	bool m_bAdmissionAlive;
}

// Одноразовый bookkeeping token: не выбирает приказ и не меняет gameplay.
class AICF_CleanupAssignmentRebind
{
	protected AICF_TransportTrip m_Trip;
	protected AICF_TripRetirementIdentity m_Identity;
	protected AICF_GroupSlot m_Slot;
	protected ref AICF_StrategicAssignmentSnapshot m_Previous;
	protected ref AICF_StrategicAssignmentSnapshot m_Accepted;
	protected ref AICF_VehicleLease m_Lease;
	protected int m_iPreviousRevision;
	protected int m_iPreviousIntent;
	protected int m_iPreviousBase;
	protected int m_iAcceptedRevision;
	protected int m_iAcceptedIntent;
	protected int m_iAcceptedBase;
	protected int m_iTripGeneration;
	protected int m_iLeaseGeneration;
	protected int m_iVehicleGeneration;
	protected string m_sLifecycle;
	protected string m_sOperation;
	protected bool m_bCommitted;

	static AICF_CleanupAssignmentRebind Capture(AICF_TransportTrip trip, AICF_TripRetirementIdentity identity,
		AICF_StrategicAssignmentSnapshot accepted, AICF_GroupSlot slot)
	{
		if (!Replication.IsServer() || !trip || !identity || !accepted || !slot || trip.IsTerminal()) return null;
		AICF_StrategicAssignmentSnapshot previous = trip.GetAssignment();
		if (!identity.MatchesCleanupAssignmentFence(previous) || !trip.OwnsRetirementIdentity(identity)) return null;
		AICF_CleanupAssignmentRebind token = new AICF_CleanupAssignmentRebind();
		token.m_Trip = trip;
		token.m_Identity = identity;
		token.m_Slot = slot;
		token.m_Previous = previous;
		token.m_Accepted = accepted;
		token.m_iPreviousRevision = previous.GetAssignmentRevision();
		token.m_iPreviousIntent = previous.GetStrategicIntentRevision();
		token.m_iPreviousBase = previous.GetBaseRevision();
		token.m_iAcceptedRevision = accepted.GetAssignmentRevision();
		token.m_iAcceptedIntent = accepted.GetStrategicIntentRevision();
		token.m_iAcceptedBase = accepted.GetBaseRevision();
		token.m_iTripGeneration = trip.GetTripGeneration();
		token.m_sOperation = trip.GetOperationId();
		token.m_Lease = trip.GetLease();
		if (token.m_Lease)
		{
			token.m_iLeaseGeneration = token.m_Lease.GetLeaseGeneration();
			token.m_iVehicleGeneration = token.m_Lease.GetVehicleGeneration();
			token.m_sLifecycle = token.m_Lease.GetVehicleLifecycleId();
		}
		if (!token.MatchesContext(trip, identity, slot, false)) return null;
		return token;
	}

	// Только actual Trip.CommitRetarget может отметить exact candidate принятым.
	void RecordCommitted(AICF_TransportTrip trip, AICF_StrategicAssignmentSnapshot accepted)
	{
		if (!trip || !accepted) return;
		if (trip == m_Trip && accepted == m_Accepted && trip.GetAssignment() == m_Accepted)
			m_bCommitted = true;
	}

	AICF_StrategicAssignmentSnapshot GetPrevious() { return m_Previous; }
	AICF_StrategicAssignmentSnapshot GetAccepted() { return m_Accepted; }
	AICF_GroupSlot GetSlot() { return m_Slot; }

	// Все getters ниже — script state, без native callbacks.
	bool MatchesContext(AICF_TransportTrip trip, AICF_TripRetirementIdentity identity, AICF_GroupSlot slot, bool committed)
	{
		if (!trip || trip != m_Trip || identity != m_Identity || slot != m_Slot || !slot || trip.IsTerminal()) return false;
		if (committed != m_bCommitted || !trip.OwnsRetirementIdentity(identity) || trip.GetTripGeneration() != m_iTripGeneration || trip.GetOperationId() != m_sOperation) return false;
		if (!m_Previous || !m_Accepted || !identity.MatchesCleanupAssignmentFence(m_Previous)) return false;
		if (!committed && trip.GetAssignment() != m_Previous) return false;
		if (committed && trip.GetAssignment() != m_Accepted) return false;
		if (m_Previous.GetAssignmentRevision() != m_iPreviousRevision || m_Previous.GetStrategicIntentRevision() != m_iPreviousIntent || m_Previous.GetBaseRevision() != m_iPreviousBase) return false;
		if (m_Accepted.GetAssignmentRevision() != m_iAcceptedRevision || m_Accepted.GetStrategicIntentRevision() != m_iAcceptedIntent || m_Accepted.GetBaseRevision() != m_iAcceptedBase) return false;
		if (!m_Accepted.MatchesCurrent(m_Previous.GetFactionKey(), m_Previous.GetSlotId(), m_Previous.GetGroupGeneration(), m_Previous.GetGroup())) return false;
		if (slot.GetSlotId() != m_Accepted.GetSlotId() || slot.GetGroup() != m_Accepted.GetGroup() || slot.GetSpawnGeneration() != m_Accepted.GetGroupGeneration()) return false;
		if (slot.GetStrategicAssignmentRevision() != m_iAcceptedRevision || slot.GetStrategicIntentRevision() != m_iAcceptedIntent) return false;
		if (slot.GetTargetKind() != m_Accepted.GetTargetKind() || slot.GetTargetBase() != m_Accepted.GetTargetBase()) return false;
		if (slot.GetTargetKind() == AICF_EOrderTargetKind.POSITION && slot.GetTargetPosition() != m_Accepted.GetTargetPosition()) return false;
		if (trip.GetLease() != m_Lease) return false;
		if (m_Lease && (m_Lease.GetLeaseGeneration() != m_iLeaseGeneration || m_Lease.GetVehicleGeneration() != m_iVehicleGeneration || m_Lease.GetVehicleLifecycleId() != m_sLifecycle)) return false;
		return true;
	}
}

class AICF_TripRetirementIdentity
{
	// Diagnostic no-op: original decision is already evaluated; no native reads.
	protected void AICF_ObserveTerminalAdmissionDecision(AICF_TransportTrip trip, string stage, bool actual)
	{
	}

	protected bool m_bCaptured;
	protected FactionKey m_sFaction;
	protected int m_iSlot;
	protected int m_iGroupGeneration;
	protected int m_iTripGeneration;
	protected string m_sOperation;
	protected EntityID m_GroupId;
	protected RplId m_GroupRpl = RplId.Invalid();
	protected SCR_AIGroup m_CapturedGroup;
	protected BaseWorld m_AdmissionWorld;
	protected RplComponent m_AdmissionGroupRpl;
	protected ref AICF_StrategicAssignmentSnapshot m_AdmissionAssignment;
	protected int m_iAdmissionAssignmentRevision;
	protected int m_iAdmissionIntentRevision;
	protected int m_iAdmissionBaseRevision;
	protected ref AICF_StrategicAssignmentSnapshot m_CleanupAssignment;
	protected int m_iCleanupAssignmentRevision;
	protected int m_iCleanupIntentRevision;
	protected int m_iCleanupBaseRevision;
	protected ref array<ref AICF_TripRetirementMember> m_aMembers = {};
	protected bool m_bRosterTrusted = true;
	protected AICF_FactionFleet m_CleanupFleet;
	protected AICF_VehicleLease m_CleanupLease;
	protected int m_iCleanupLeaseGeneration;
	protected int m_iCleanupVehicleGeneration;
	protected EntityID m_CleanupVehicleId;
	protected string m_sCleanupVehicleRpl;
	protected bool m_bAllDeadObserved;
	protected int m_iAllDeadObservedAt;
	protected int m_iObservationDeadlineAt;
	protected int m_iCleanupTerminalAt;
	protected int m_iAuthorizedTerminalTransition;
	protected string m_sAuthorizedTerminalCausation;
	protected bool m_bCleanupContextCaptured;
	protected bool m_bPoolReleaseReceipt;
	protected bool m_bRetirementReleaseReceipt;
	protected bool m_bAbsenceReceipt;
	protected bool m_bAcknowledgementReceipt;
	protected int m_iReleaseReceiptAt;
	protected int m_iAbsenceReceiptAt;
	protected int m_iAcknowledgementAt;

	void AICF_TripRetirementIdentity(AICF_StrategicAssignmentSnapshot assignment, int tripGeneration, string operation)
	{
		if (!Replication.IsServer() || !GetGame() || !GetGame().GetWorld() || !assignment || !assignment.IsValid() || tripGeneration <= 0 || operation.IsEmpty()) return;
		SCR_AIGroup group = assignment.GetGroup();
		if (!AICF_VehicleBoardingMutationFence.IsAuthoritativeReplicatedEntity(group)) return;
		RplComponent rpl = RplComponent.Cast(group.FindComponent(RplComponent));
		if (!rpl || !rpl.Id().IsValid()) return;
		m_sFaction = assignment.GetFactionKey();
		m_iSlot = assignment.GetSlotId();
		m_iGroupGeneration = assignment.GetGroupGeneration();
		m_iTripGeneration = tripGeneration;
		m_sOperation = operation;
		m_GroupId = group.GetID();
		m_GroupRpl = rpl.Id();
		m_CapturedGroup = group;
		m_AdmissionWorld = GetGame().GetWorld();
		m_AdmissionGroupRpl = rpl;
		m_AdmissionAssignment = assignment;
		m_iAdmissionAssignmentRevision = assignment.GetAssignmentRevision();
		m_iAdmissionIntentRevision = assignment.GetStrategicIntentRevision();
		m_iAdmissionBaseRevision = assignment.GetBaseRevision();
		m_CleanupAssignment = assignment;
		m_iCleanupAssignmentRevision = m_iAdmissionAssignmentRevision;
		m_iCleanupIntentRevision = m_iAdmissionIntentRevision;
		m_iCleanupBaseRevision = m_iAdmissionBaseRevision;
		m_bCaptured = true;
		array<AIAgent> agents = {};
		group.GetAgents(agents);
		foreach (AIAgent agent : agents)
		{
			if (agent && agent.GetParentGroup() == group) CaptureMember(agent.GetControlledEntity());
		}
	}

	protected void CaptureMember(IEntity entity)
	{
		ChimeraCharacter character = ChimeraCharacter.Cast(entity);
		if (!character) return;
		RplComponent rpl = RplComponent.Cast(character.FindComponent(RplComponent));
		FactionAffiliationComponent affiliation = FactionAffiliationComponent.Cast(character.FindComponent(FactionAffiliationComponent));
		Faction faction;
		if (affiliation) faction = affiliation.GetAffiliatedFaction();
		if (!AICF_VehicleBoardingMutationFence.IsAuthoritativeAIEntity(character) || !rpl || !rpl.Id().IsValid() ||
			!faction || faction.GetFactionKey() != m_sFaction || m_aMembers.Count() >= AICF_Stage1Config.MAX_GROUP_SIZE)
		{
			m_bRosterTrusted = false;
			return;
		}
		AICF_TripRetirementMember member = new AICF_TripRetirementMember();
		member.m_Character = character;
		member.m_Id = character.GetID();
		member.m_sIdCanonical = member.m_Id.ToString();
		member.m_sRpl = rpl.Id().ToString();
		member.m_World = character.GetWorld();
		member.m_Rpl = rpl;
		member.m_Controller = character.GetCharacterController();
		member.m_bAdmissionAlive = member.m_World == m_AdmissionWorld && member.m_Controller && member.m_Controller.GetLifeState() == ECharacterLifeState.ALIVE;
		m_aMembers.Insert(member);
		AICF_Stage3Diagnostics.Info("TRIP_RETIREMENT_MEMBER_CAPTURED", string.Format("faction=%1 slot=%2 group_generation=%3 trip_generation=%4 operation_id=%5 member=%6 actor_rpl=%7 scope=ADMISSION_LIVE_GROUP member_id_canonical=%8", m_sFaction, m_iSlot, m_iGroupGeneration, m_iTripGeneration, m_sOperation, member.m_Id, member.m_sRpl, member.m_sIdCanonical));
	}

	protected bool IsCapturedGroupAbsent()
	{
		return m_bCaptured && m_GroupId != EntityID.INVALID && m_GroupRpl.IsValid() && GetGame() && GetGame().GetWorld() &&
			!GetGame().GetWorld().FindEntityByID(m_GroupId) && !Replication.FindItem(m_GroupRpl);
	}

	protected bool BindCleanupLease(AICF_TransportTrip trip, AICF_FactionFleet fleet, AICF_VehicleLease lease)
	{
		if (!lease || trip.GetLease() != lease || !IsFleetAssetCurrent(trip, fleet)) return false;
		if (m_CleanupLease) return m_CleanupLease == lease && m_CleanupFleet == fleet && m_iCleanupLeaseGeneration == lease.GetLeaseGeneration() &&
			m_iCleanupVehicleGeneration == lease.GetVehicleGeneration() && m_CleanupVehicleId == lease.GetEntityId() && m_sCleanupVehicleRpl == lease.GetRplId();
		m_CleanupFleet = fleet;
		m_CleanupLease = lease;
		m_iCleanupLeaseGeneration = lease.GetLeaseGeneration();
		m_iCleanupVehicleGeneration = lease.GetVehicleGeneration();
		m_CleanupVehicleId = lease.GetEntityId();
		m_sCleanupVehicleRpl = lease.GetRplId();
		return true;
	}

	bool CaptureCleanup(AICF_TransportTrip trip, AICF_FactionFleet fleet, AICF_VehicleLease lease, int terminalAt)
	{
		if ((!MatchesAbsentGroup(trip) && !MatchesLiveCapturedGroup(trip)) || !trip.IsTerminal() || trip.GetPhase() != AICF_ETransportTripPhase.FAILED_CLOSED ||
			!m_bRosterTrusted || m_aMembers.IsEmpty() || !BindCleanupLease(trip, fleet, lease) || terminalAt <= 0) return false;
		if (m_iCleanupTerminalAt > 0 && m_iCleanupTerminalAt != terminalAt) return false;
		m_iCleanupTerminalAt = terminalAt;
		m_bCleanupContextCaptured = true;
		return true;
	}

	protected bool MatchesLiveCapturedGroup(AICF_TransportTrip trip)
	{
		if (!m_bCaptured || !trip || !trip.GetAssignment() || !m_CapturedGroup || !GetGame() || !GetGame().GetWorld() ||
			trip.GetAssignment().GetGroup() != m_CapturedGroup || m_CapturedGroup.GetID() != m_GroupId ||
			!AICF_VehicleBoardingMutationFence.IsAuthoritativeReplicatedEntity(m_CapturedGroup)) return false;
		RplComponent rpl = RplComponent.Cast(m_CapturedGroup.FindComponent(RplComponent));
		return rpl && rpl.Id() == m_GroupRpl && trip.MatchesIdentity(m_sFaction, m_iSlot, m_iGroupGeneration, m_iTripGeneration) && trip.GetOperationId() == m_sOperation;
	}

	bool RecordAllCapturedDead(AICF_TransportTrip trip, AICF_FactionFleet fleet, int observationBudget)
	{
		if (m_bAllDeadObserved) return trip && fleet == m_CleanupFleet && trip.GetOperationId() == m_sOperation && trip.MatchesIdentity(m_sFaction, m_iSlot, m_iGroupGeneration, m_iTripGeneration) && BindCleanupLease(trip, fleet, trip.GetLease());
		if (!Replication.IsServer() || !m_bRosterTrusted || m_aMembers.IsEmpty() || observationBudget <= 0 ||
			(!MatchesLiveCapturedGroup(trip) && !MatchesAbsentGroup(trip)) || !BindCleanupLease(trip, fleet, trip.GetLease())) return false;
		for (int index; index < m_aMembers.Count(); index++)
			if (!ResolveDeadCapturedIdentity(index)) return false;
		m_bAllDeadObserved = true;
		m_iAllDeadObservedAt = System.GetTickCount();
		m_iObservationDeadlineAt = m_iAllDeadObservedAt + observationBudget;
		foreach (AICF_TripRetirementMember member : m_aMembers)
		{
			member.m_bNativeDeadObserved = true;
			AICF_Stage3Diagnostics.Info("TRIP_RETIREMENT_DEAD_MEMBER", string.Format("faction=%1 slot=%2 group_generation=%3 trip_generation=%4 operation_id=%5 member=%6 actor_rpl=%7 life_state=DEAD native_readback=1", m_sFaction, m_iSlot, m_iGroupGeneration, m_iTripGeneration, m_sOperation, member.m_Id, member.m_sRpl));
		}
		AICF_Stage3Diagnostics.Info("TRIP_RETIREMENT_ALL_MEMBERS_DEAD", EvidenceDetails());
		return true;
	}

	bool HasCapturedDeadEvidence() { return m_bAllDeadObserved; }

	void RecordOrphanTerminalCommit(AICF_TransportTrip trip, AICF_FactionFleet fleet)
	{
		if (!m_bAllDeadObserved || !trip || trip.GetPhase() != AICF_ETransportTripPhase.FAILED_CLOSED ||
			(!MatchesAbsentGroup(trip) && !MatchesLiveCapturedGroup(trip)) || !AllObservedDeadRemainKnown() ||
			fleet != m_CleanupFleet || !BindCleanupLease(trip, fleet, trip.GetLease())) return;
		m_iAuthorizedTerminalTransition = trip.GetTransitionCount();
		m_sAuthorizedTerminalCausation = trip.GetCausationId();
	}

	protected bool AllObservedDeadRemainKnown()
	{
		foreach (AICF_TripRetirementMember member : m_aMembers)
		{
			if (!member.m_bNativeDeadObserved) return false;
			if (!GetGame().GetWorld().FindEntityByID(member.m_Id)) continue;
			int index = m_aMembers.Find(member);
			if (!ResolveDeadCapturedIdentity(index)) return false;
		}
		return true;
	}

	protected bool MatchesCleanupReceipt(AICF_VehicleCleanupJob job)
	{
		if (!Replication.IsServer() || !m_bCleanupContextCaptured || !m_bAllDeadObserved || m_iAuthorizedTerminalTransition <= 0 || !job ||
			job.m_bFailClosed || job.m_Fleet != m_CleanupFleet || job.m_Lease != m_CleanupLease || !m_CleanupLease) return false;
		if (job.m_sOperationId != m_sOperation || !job.m_Fence || !job.m_Fence.IsComplete() || !job.m_Fence.MatchesLease(m_CleanupLease)) return false;
		return m_CleanupLease.GetLeaseGeneration() == m_iCleanupLeaseGeneration && m_CleanupLease.GetVehicleGeneration() == m_iCleanupVehicleGeneration &&
			m_CleanupLease.GetEntityId() == m_CleanupVehicleId && m_CleanupLease.GetRplId() == m_sCleanupVehicleRpl;
	}

	void ObservePhysicalRelease(AICF_VehicleCleanupJob job, bool worldPool)
	{
		if (!MatchesCleanupReceipt(job) || !IsCapturedGroupAbsent() || !AllObservedDeadRemainKnown() || !job.m_bReleaseComplete ||
			!job.m_bClearanceSafe || !job.m_Scan || !job.m_Scan.m_bSafe || m_CleanupLease.IsCapActive() || m_CleanupLease.GetState() != AICF_EVehicleLeaseState.RELEASED) return;
		// Fleet уже передал physical reference новому asset и занулил lease.m_Vehicle.
		Vehicle vehicle;
		if (worldPool && job.m_WorldPoolAsset) vehicle = job.m_WorldPoolAsset.GetVehicle();
		else if (!worldPool && job.m_RetirementAsset) vehicle = job.m_RetirementAsset.GetVehicle();
		if (!vehicle || GetGame().GetWorld().FindEntityByID(m_CleanupVehicleId) != vehicle ||
			!AICF_VehicleBoardingMutationFence.IsAuthoritativeReplicatedEntity(vehicle)) return;
		RplComponent rpl = RplComponent.Cast(vehicle.FindComponent(RplComponent));
		if (!rpl || rpl.Id().ToString() != m_sCleanupVehicleRpl) return;
		if (worldPool)
		{
			bool found;
			for (int index; index < m_CleanupFleet.GetWorldPoolCount(); index++)
				if (m_CleanupFleet.GetWorldPoolAsset(index) == job.m_WorldPoolAsset) found = true;
			if (!found || !job.m_WorldPoolAsset || !job.m_WorldPoolAsset.MatchesIdentity(m_CleanupVehicleId, m_sCleanupVehicleRpl)) return;
			m_bPoolReleaseReceipt = true;
		}
		else
		{
			if (!job.m_RetirementAsset || !job.m_bQuarantined || job.m_RetirementAsset.GetEntityId() != m_CleanupVehicleId ||
				job.m_RetirementAsset.GetRplId() != m_sCleanupVehicleRpl) return;
			m_bRetirementReleaseReceipt = true;
		}
		if (m_iReleaseReceiptAt == 0) m_iReleaseReceiptAt = System.GetTickCount();
	}

	void ObserveAcknowledgement(AICF_VehicleCleanupJob job)
	{
		if (!MatchesCleanupReceipt(job) || !job.m_bControllerAcknowledged || !job.m_bReleaseComplete ||
			m_CleanupLease.IsCapActive() || m_CleanupLease.GetState() != AICF_EVehicleLeaseState.RELEASED) return;
		m_bAcknowledgementReceipt = true;
		if (m_iAcknowledgementAt == 0) m_iAcknowledgementAt = System.GetTickCount();
	}

	void ObserveConfirmedAbsence(AICF_VehicleCleanupJob job)
	{
		if (!MatchesCleanupReceipt(job) || !m_bRetirementReleaseReceipt || !job.m_bCompleted || !GetGame() || !GetGame().GetWorld() ||
			GetGame().GetWorld().FindEntityByID(m_CleanupVehicleId)) return;
		m_bAbsenceReceipt = true;
		if (m_iAbsenceReceiptAt == 0) m_iAbsenceReceiptAt = System.GetTickCount();
	}

	AICF_ETripRetirementObservation ClassifyObservation(AICF_TransportTrip trip)
	{
		if (!m_bAllDeadObserved || !trip || trip.GetPhase() != AICF_ETransportTripPhase.FAILED_CLOSED) return AICF_ETripRetirementObservation.NONE;
		if (m_iAuthorizedTerminalTransition <= 0 || trip.GetTransitionCount() != m_iAuthorizedTerminalTransition ||
			trip.GetCausationId() != m_sAuthorizedTerminalCausation) return AICF_ETripRetirementObservation.NONE;
		AICF_VehicleHandoffState handoff = trip.GetHandoffState();
		if (m_bCleanupContextCaptured && m_bAcknowledgementReceipt && (m_bPoolReleaseReceipt || (m_bRetirementReleaseReceipt && m_bAbsenceReceipt)))
		{
			if (!handoff.IsCleanupQueueAccepted() || !handoff.IsCleanupReleaseComplete() || !handoff.IsClearanceSafe() ||
				handoff.IsCleanupRetainedFailClosed() || !handoff.GetCleanupFailureReason().IsEmpty() || trip.GetLease()) return AICF_ETripRetirementObservation.REJECTED;
			int completedAt = Math.Max(m_iReleaseReceiptAt, m_iAcknowledgementAt);
			if (m_bRetirementReleaseReceipt) completedAt = Math.Max(completedAt, m_iAbsenceReceiptAt);
			if (completedAt > 0 && m_iCleanupTerminalAt > 0 && completedAt <= m_iCleanupTerminalAt) return AICF_ETripRetirementObservation.CONFIRMED;
			return AICF_ETripRetirementObservation.REJECTED;
		}
		if ((!MatchesAbsentGroup(trip) && !MatchesLiveCapturedGroup(trip)) || !AllObservedDeadRemainKnown() || !trip.IsTerminal() ||
			trip.GetPhase() != AICF_ETransportTripPhase.FAILED_CLOSED || !m_CleanupLease || !m_CleanupFleet) return AICF_ETripRetirementObservation.REJECTED;
		int deadline = m_iObservationDeadlineAt;
		if (m_iCleanupTerminalAt > 0) deadline = m_iCleanupTerminalAt;
		if (System.GetTickCount() > deadline || handoff.IsCleanupRetainedFailClosed() || !handoff.GetCleanupFailureReason().IsEmpty()) return AICF_ETripRetirementObservation.REJECTED;
		if (!MatchesAbsentGroup(trip)) return AICF_ETripRetirementObservation.PENDING;
		return AICF_ETripRetirementObservation.PENDING;
	}

	string EvidenceDetails()
	{
		string details = string.Format("faction=%1 slot=%2 group_generation=%3 trip_generation=%4 operation_id=%5 vehicle=%6 rpl_id=%7 lease_generation=%8 vehicle_generation=%9", m_sFaction, m_iSlot, m_iGroupGeneration, m_iTripGeneration, m_sOperation, m_CleanupVehicleId, m_sCleanupVehicleRpl, m_iCleanupLeaseGeneration, m_iCleanupVehicleGeneration);
		details += string.Format(" captured_count=%1 all_native_dead=%2 observed_dead_at_ms=%3 cleanup_terminal_at_ms=%4 observation_deadline_at_ms=%5 original_full_route=0", m_aMembers.Count(), m_bAllDeadObserved, m_iAllDeadObservedAt, m_iCleanupTerminalAt, m_iObservationDeadlineAt);
		string capAtRelease = "UNOBSERVED";
		if (m_bPoolReleaseReceipt || m_bRetirementReleaseReceipt) capAtRelease = "0";
		details += string.Format(" pool_offer_receipt=%1 quarantine_receipt=%2 native_absence_receipt=%3 release_ack_receipt=%4 release_at_ms=%5 absence_at_ms=%6 release_ack_at_ms=%7 own_cap_at_release=%8", m_bPoolReleaseReceipt, m_bRetirementReleaseReceipt, m_bAbsenceReceipt, m_bAcknowledgementReceipt, m_iReleaseReceiptAt, m_iAbsenceReceiptAt, m_iAcknowledgementAt, capAtRelease);
		return details;
	}

	bool MatchesCapturedActor(EntityID id, string rpl)
	{
		foreach (AICF_TripRetirementMember member : m_aMembers)
			if (member.m_Id == id && member.m_sRpl == rpl && member.m_bNativeDeadObserved) return true;
		return false;
	}

	int GetCapturedMemberCount() { return m_aMembers.Count(); }
	string GetCapturedMemberIdString(int index)
	{
		if (!m_aMembers.IsIndexValid(index)) return string.Empty;
		return m_aMembers[index].m_sIdCanonical;
	}

	bool MatchesCleanupAssignmentFence(AICF_StrategicAssignmentSnapshot assignment)
	{
		return m_bCaptured && assignment && assignment == m_CleanupAssignment &&
			assignment.GetAssignmentRevision() == m_iCleanupAssignmentRevision &&
			assignment.GetStrategicIntentRevision() == m_iCleanupIntentRevision &&
			assignment.GetBaseRevision() == m_iCleanupBaseRevision;
	}

	bool AcceptCleanupAssignmentRebind(AICF_TransportTrip trip, AICF_GroupSlot slot, AICF_CleanupAssignmentRebind token)
	{
		if (!Replication.IsServer() || !GetGame() || GetGame().GetWorld() != m_AdmissionWorld || !trip || !token) return false;
		if (!trip.OwnsCleanupAssignmentRebind(token) || !token.MatchesContext(trip, this, slot, true)) return false;
		if (!MatchesLiveCapturedGroup(trip) || m_CapturedGroup.GetWorld() != m_AdmissionWorld) return false;
		if (m_AdmissionWorld.FindEntityByID(m_GroupId) != m_CapturedGroup || m_CapturedGroup.FindComponent(RplComponent) != m_AdmissionGroupRpl) return false;
		if (!Replication.IsServer() || !GetGame() || GetGame().GetWorld() != m_AdmissionWorld) return false;
		// Native getters закончены: stale outer callback не переносит successor fence.
		if (!trip.OwnsCleanupAssignmentRebind(token) || !token.MatchesContext(trip, this, slot, true)) return false;
		AICF_StrategicAssignmentSnapshot accepted = token.GetAccepted();
		m_CleanupAssignment = accepted;
		m_iCleanupAssignmentRevision = accepted.GetAssignmentRevision();
		m_iCleanupIntentRevision = accepted.GetStrategicIntentRevision();
		m_iCleanupBaseRevision = accepted.GetBaseRevision();
		return true;
	}

	// Отдельный cleanup capability для исходного DEAD actor при ещё живой группе.
	// Orphan resolver ниже сохраняет прежний, более узкий контракт.
	bool MatchesTerminalCleanupAdmission(AICF_TransportTrip trip)
	{
		if (!Replication.IsServer() || !m_bRosterTrusted || m_aMembers.IsEmpty() || !GetGame())
		{
			AICF_ObserveTerminalAdmissionDecision(trip, "ADMISSION_01", false);
			return false;
		}
		if (!m_AdmissionWorld || GetGame().GetWorld() != m_AdmissionWorld || !trip || !trip.IsTerminal())
		{
			AICF_ObserveTerminalAdmissionDecision(trip, "ADMISSION_02", false);
			return false;
		}
		if (!trip.OwnsRetirementIdentity(this) || !MatchesCleanupAssignmentFence(trip.GetAssignment()) || !MatchesLiveCapturedGroup(trip))
		{
			AICF_ObserveTerminalAdmissionDecision(trip, "ADMISSION_03", false);
			return false;
		}
		if (m_AdmissionAssignment.GetAssignmentRevision() != m_iAdmissionAssignmentRevision ||
			m_AdmissionAssignment.GetStrategicIntentRevision() != m_iAdmissionIntentRevision ||
			m_AdmissionAssignment.GetBaseRevision() != m_iAdmissionBaseRevision)
		{
			AICF_ObserveTerminalAdmissionDecision(trip, "ADMISSION_04", false);
			return false;
		}
		if (m_CapturedGroup.GetWorld() != m_AdmissionWorld || m_AdmissionWorld.FindEntityByID(m_GroupId) != m_CapturedGroup ||
			m_CapturedGroup.FindComponent(RplComponent) != m_AdmissionGroupRpl)
		{
			AICF_ObserveTerminalAdmissionDecision(trip, "ADMISSION_05", false);
			return false;
		}
		AICF_GroupSlot slot = trip.GetVehiclePresetSlot();
		bool actualAdmission = slot && slot.GetSlotId() == m_iSlot && slot.GetGroup() == m_CapturedGroup && slot.GetSpawnGeneration() == m_iGroupGeneration;
		AICF_ObserveTerminalAdmissionDecision(trip, "ADMISSION_FINAL", actualAdmission);
		return actualAdmission;
	}

	ChimeraCharacter ResolveTerminalDeadActor(int index)
	{
		if (!Replication.IsServer() || !m_bRosterTrusted || !m_aMembers.IsIndexValid(index) ||
			!GetGame() || GetGame().GetWorld() != m_AdmissionWorld) return null;
		AICF_TripRetirementMember member = m_aMembers[index];
		ChimeraCharacter character = ResolveDeadCapturedIdentity(index);
		if (!character || !member.m_bAdmissionAlive || member.m_World != m_AdmissionWorld || character.GetWorld() != m_AdmissionWorld ||
			character.FindComponent(RplComponent) != member.m_Rpl || character.GetCharacterController() != member.m_Controller) return null;
		if (member.m_Controller.IsPlayerControlled()) return null;
		// Все native parent/component/life/world чтения ДО последнего human scan.
		if (!Replication.IsServer() || !GetGame() || GetGame().GetWorld() != m_AdmissionWorld ||
			ResolveDeadCapturedIdentity(index) != character || character.FindComponent(RplComponent) != member.m_Rpl ||
			character.GetCharacterController() != member.m_Controller) return null;
		if (character.GetWorld() != m_AdmissionWorld || member.m_Controller.GetLifeState() != ECharacterLifeState.DEAD ||
			!AICF_VehicleBoardingMutationFence.IsAuthoritativeReplicatedEntity(character)) return null;
		PlayerManager playerManager = GetGame().GetPlayerManager();
		if (!playerManager) return null;
		array<int> playerIds = {};
		int playerCount = playerManager.GetAllPlayers(playerIds);
		if (playerCount != playerIds.Count() || playerCount > 256) return null;
		foreach (int playerId : playerIds)
		{
			PlayerController playerController = playerManager.GetPlayerController(playerId);
			if (!playerController || playerController.GetControlledEntity() == character) return null;
		}
		// PC getter callbacks могут сменить world либо прямое player ownership.
		if (!GetGame() || GetGame().GetWorld() != m_AdmissionWorld || GetGame().GetPlayerManager() != playerManager) return null;
		if (member.m_Controller.IsPlayerControlled() || playerManager.GetPlayerIdFromControlledEntity(character) > 0 ||
			SCR_PossessingManagerComponent.GetPlayerIdFromMainEntity(character) > 0) return null;
		return character;
	}

	ChimeraCharacter ResolveTerminalDeadActorForCommit(int index, ChimeraCharacter expectedActor, Vehicle vehicle,
		CompartmentAccessComponent expectedAccess, BaseCompartmentSlot expectedSeat)
	{
		// 1. World/entity/Rpl/component/parent до exact native seat proof.
		if (!expectedActor || ResolveTerminalDeadActor(index) != expectedActor) return null;
		AICF_TripRetirementMember member = m_aMembers[index];
		// 2. Последние native access/seat/vehicle getters.
		if (!expectedAccess || !expectedSeat || expectedActor.GetCompartmentAccessComponent() != expectedAccess) return null;
		if (expectedAccess.GetCompartment() != expectedSeat || expectedSeat.GetVehicle() != vehicle ||
			expectedSeat.GetOccupant() != expectedActor) return null;
		// 3. Финальный DEAD/control/nonplayer proof. Ни parent, ни seat после него.
		if (!expectedActor || !member || !member.m_Controller) return null;
		CharacterControllerComponent controller = expectedActor.GetCharacterController();
		if (!controller || controller != member.m_Controller || controller.GetLifeState() != ECharacterLifeState.DEAD || controller.IsPlayerControlled()) return null;
		if (!Replication.IsServer() || !GetGame() || GetGame().GetWorld() != m_AdmissionWorld) return null;
		PlayerManager playerManager = GetGame().GetPlayerManager();
		if (!playerManager || playerManager.GetPlayerIdFromControlledEntity(expectedActor) > 0 ||
			SCR_PossessingManagerComponent.GetPlayerIdFromMainEntity(expectedActor) > 0) return null;
		array<int> playerIds = {};
		int playerCount = playerManager.GetAllPlayers(playerIds);
		if (playerCount != playerIds.Count() || playerCount > 256) return null;
		foreach (int playerId : playerIds)
		{
			PlayerController playerController = playerManager.GetPlayerController(playerId);
			if (!playerController || playerController.GetControlledEntity() == expectedActor) return null;
		}
		if (!Replication.IsServer() || !GetGame() || !expectedActor || !controller || !playerManager) return null;
		if (GetGame().GetWorld() != m_AdmissionWorld || GetGame().GetPlayerManager() != playerManager) return null;
		if (!expectedActor || !controller || !playerManager) return null;
		if (expectedActor.GetCharacterController() != controller) return null;
		if (!controller || controller.GetLifeState() != ECharacterLifeState.DEAD || controller.IsPlayerControlled() ||
			playerManager.GetPlayerIdFromControlledEntity(expectedActor) > 0 || SCR_PossessingManagerComponent.GetPlayerIdFromMainEntity(expectedActor) > 0) return null;
		return expectedActor;
	}

	ChimeraCharacter ResolveCapturedDeadMember(AICF_VehicleCleanupJob job, Vehicle vehicle, int index)
	{
		if (!Replication.IsServer() || !IsCapturedGroupAbsent() || !m_bRosterTrusted || !job || job.m_Group || !vehicle ||
			job.m_Fleet != m_CleanupFleet || !m_CleanupFleet || job.m_Lease != m_CleanupLease || !m_CleanupLease || !job.m_Fence ||
			!job.m_Fence.IsComplete() || !job.m_Fence.MatchesLease(m_CleanupLease) || job.m_sOperationId != m_sOperation) return null;
		if (m_CleanupLease.GetFactionKey() != m_sFaction || m_CleanupLease.GetSlotId() != m_iSlot ||
			m_CleanupLease.GetGroupGeneration() != m_iGroupGeneration || m_CleanupLease.GetTripGeneration() != m_iTripGeneration ||
			m_CleanupLease.GetLeaseGeneration() != m_iCleanupLeaseGeneration || m_CleanupLease.GetVehicleGeneration() != m_iCleanupVehicleGeneration ||
			m_CleanupFleet.FindLeaseForSlot(m_iSlot, m_iGroupGeneration) != m_CleanupLease || !m_CleanupLease.IsCapActive()) return null;
		if (!AICF_VehicleBoardingMutationFence.IsAuthoritativeReplicatedEntity(vehicle) || vehicle.GetID() != m_CleanupVehicleId ||
			GetGame().GetWorld().FindEntityByID(m_CleanupVehicleId) != vehicle || !m_CleanupLease.MatchesEntityIdentity(vehicle, m_CleanupVehicleId, m_sCleanupVehicleRpl)) return null;
		RplComponent vehicleRpl = RplComponent.Cast(vehicle.FindComponent(RplComponent));
		if (!vehicleRpl || vehicleRpl.Id().ToString() != m_sCleanupVehicleRpl || !m_aMembers.IsIndexValid(index)) return null;
		return ResolveDeadCapturedIdentity(index);
	}

	protected ChimeraCharacter ResolveDeadCapturedIdentity(int index)
	{
		if (!m_aMembers.IsIndexValid(index) || !GetGame() || !GetGame().GetWorld()) return null;
		AICF_TripRetirementMember member = m_aMembers[index];
		ChimeraCharacter character = member.m_Character;
		if (!character || character.GetID() != member.m_Id || GetGame().GetWorld().FindEntityByID(member.m_Id) != character ||
			!AICF_VehicleBoardingMutationFence.IsAuthoritativeReplicatedEntity(character)) return null;
		CharacterControllerComponent controller = character.GetCharacterController();
		RplComponent actorRpl = RplComponent.Cast(character.FindComponent(RplComponent));
		FactionAffiliationComponent affiliation = FactionAffiliationComponent.Cast(character.FindComponent(FactionAffiliationComponent));
		Faction faction;
		if (affiliation) faction = affiliation.GetAffiliatedFaction();
		if (!controller || controller.GetLifeState() != ECharacterLifeState.DEAD || !actorRpl || actorRpl.Id().ToString() != member.m_sRpl ||
			!faction || faction.GetFactionKey() != m_sFaction) return null;
		AIControlComponent control = AIControlComponent.Cast(character.FindComponent(AIControlComponent));
		AIAgent agent;
		if (control) agent = control.GetAIAgent();
		if (agent && agent.GetParentGroup() && agent.GetParentGroup() != m_CapturedGroup) return null;
		PlayerManager playerManager = GetGame().GetPlayerManager();
		if (!playerManager || playerManager.GetPlayerIdFromControlledEntity(character) > 0 || SCR_PossessingManagerComponent.GetPlayerIdFromMainEntity(character) > 0) return null;
		return character;
	}

	bool MatchesAbsentGroup(AICF_TransportTrip trip)
	{
		if (!Replication.IsServer() || !trip || !m_bCaptured || !trip.GetAssignment() ||
			m_GroupId == EntityID.INVALID || !m_GroupRpl.IsValid() || !GetGame() || !GetGame().GetWorld()) return false;
		if (trip.GetAssignment().GetGroup() || GetGame().GetWorld().FindEntityByID(m_GroupId) || Replication.FindItem(m_GroupRpl)) return false;
		return trip.MatchesIdentity(m_sFaction, m_iSlot, m_iGroupGeneration, m_iTripGeneration) &&
			trip.GetOperationId() == m_sOperation && !m_sOperation.IsEmpty();
	}

	static bool IsFleetAssetCurrent(AICF_TransportTrip trip, AICF_FactionFleet fleet)
	{
		if (!Replication.IsServer() || !trip || !fleet || fleet.GetFactionKey() != trip.GetFactionKey()) return false;
		AICF_VehicleLease lease = trip.GetLease();
		if (!lease) return true;
		if (!lease.MatchesTripIdentity(trip.GetFactionKey(), trip.GetSlotId(), trip.GetGroupGeneration(), trip.GetTripGeneration())) return false;
		if (lease.GetState() == AICF_EVehicleLeaseState.RELEASED)
			return trip.IsTerminal() && trip.GetHandoffState().IsCleanupQueueAccepted();
		if (fleet.FindLeaseForSlot(trip.GetSlotId(), trip.GetGroupGeneration()) != lease) return false;
		Vehicle vehicle = lease.GetVehicle();
		if (!vehicle)
		{
			if (lease.GetState() == AICF_EVehicleLeaseState.RESERVED) return lease.GetEntityId() == EntityID.INVALID;
			return lease.GetEntityId() != EntityID.INVALID && !lease.GetRplId().IsEmpty() && GetGame() && GetGame().GetWorld() &&
				!GetGame().GetWorld().FindEntityByID(lease.GetEntityId());
		}
		if (!AICF_VehicleBoardingMutationFence.IsAuthoritativeReplicatedEntity(vehicle) || !GetGame() || !GetGame().GetWorld() ||
			GetGame().GetWorld().FindEntityByID(lease.GetEntityId()) != vehicle) return false;
		RplComponent rpl = RplComponent.Cast(vehicle.FindComponent(RplComponent));
		return rpl && lease.MatchesEntityIdentity(vehicle, vehicle.GetID(), rpl.Id().ToString());
	}
}
