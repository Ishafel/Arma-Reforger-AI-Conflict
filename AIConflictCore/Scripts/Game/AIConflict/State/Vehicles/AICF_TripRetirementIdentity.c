// Отдельная identity для retirement после native удаления controller группы.
// Она не разрешает новый gameplay/order/boarding и не заменяет IsValid().
class AICF_TripRetirementMember
{
	ChimeraCharacter m_Character;
	EntityID m_Id;
	string m_sRpl;
	bool m_bNativeDeadObserved;
}

class AICF_TripRetirementIdentity
{
	protected bool m_bCaptured;
	protected FactionKey m_sFaction;
	protected int m_iSlot;
	protected int m_iGroupGeneration;
	protected int m_iTripGeneration;
	protected string m_sOperation;
	protected EntityID m_GroupId;
	protected RplId m_GroupRpl = RplId.Invalid();
	protected SCR_AIGroup m_CapturedGroup;
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
		if (!Replication.IsServer() || !assignment || !assignment.IsValid() || tripGeneration <= 0 || operation.IsEmpty()) return;
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
		member.m_sRpl = rpl.Id().ToString();
		m_aMembers.Insert(member);
		AICF_Stage3Diagnostics.Info("TRIP_RETIREMENT_MEMBER_CAPTURED", string.Format("faction=%1 slot=%2 group_generation=%3 trip_generation=%4 operation_id=%5 member=%6 actor_rpl=%7 scope=ADMISSION_LIVE_GROUP", m_sFaction, m_iSlot, m_iGroupGeneration, m_iTripGeneration, m_sOperation, member.m_Id, member.m_sRpl));
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
