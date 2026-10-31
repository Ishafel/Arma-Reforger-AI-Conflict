// Immutable physical staging receipt. No quota/economy decisions or actions.
class AICF_AcquisitionStagedBody
{
	protected AIAgent m_Agent;
	protected EntityID m_AgentId;
	protected IEntity m_Actor;
	protected EntityID m_ActorId;
	protected string m_sActorRpl;

	static AICF_AcquisitionStagedBody Capture(AIAgent agent)
	{
		if (!agent || !agent.GetControlledEntity()) return null;
		AICF_AcquisitionStagedBody body = new AICF_AcquisitionStagedBody();
		body.m_Agent = agent;
		body.m_AgentId = agent.GetID();
		body.m_Actor = agent.GetControlledEntity();
		body.m_ActorId = body.m_Actor.GetID();
		body.m_sActorRpl = AICF_VehicleBehaviorBinding.ComponentRpl(body.m_Actor);
		return body;
	}

	AIAgent GetAgent() { return m_Agent; }

	bool IsCurrent(SCR_AIGroup group, vector staging, vector spawn, float radius, float envelope, bool manual)
	{
		if (!group || !m_Agent || m_Agent.GetID() != m_AgentId || m_Agent.GetWorld() != group.GetWorld() || m_Agent.GetParentGroup() != group ||
			m_Agent.GetControlledEntity() != m_Actor || !m_Actor) return false;
		if (!AICF_VehicleBoardingMutationFence.IsAuthoritativeAIEntity(m_Actor) ||
			m_Actor.GetID() != m_ActorId || !m_Actor.GetWorld() || m_Actor.GetWorld() != group.GetWorld() ||
			m_Actor.GetWorld().FindEntityByID(m_ActorId) != m_Actor ||
			AICF_VehicleBehaviorBinding.ComponentRpl(m_Actor) != m_sActorRpl ||
			!SCR_AIDamageHandling.IsConscious(m_Actor)) return false;
		ChimeraCharacter character = ChimeraCharacter.Cast(m_Actor);
		if (!character || !character.GetAIControlComponent() ||
			character.GetAIControlComponent().GetControlAIAgent() != m_Agent) return false;
		CompartmentAccessComponent access = character.GetCompartmentAccessComponent();
		if (!access || character.IsInVehicle() || access.IsInCompartment() || access.IsGettingIn() || access.IsGettingOut() ||
			access.IsSwitchingSeatsAnim() || CompartmentAccessComponent.GetVehicleIn(m_Actor)) return false;
		if (!AICF_VehicleSpawner.IsStagingGeometryReady(vector.DistanceXZ(m_Actor.GetOrigin(), staging), radius,
			vector.DistanceXZ(m_Actor.GetOrigin(), spawn), envelope, manual)) return false;
		SCR_AIUtilityComponent utility = SCR_AIUtilityComponent.Cast(m_Agent.FindComponent(SCR_AIUtilityComponent));
		if (!utility || utility.m_OwnerEntity != m_Actor || !utility.m_ThreatSystem || !utility.m_CombatComponent) return false;
		BaseTarget target = utility.m_CombatComponent.GetCurrentTarget();
		if (target && target.GetTimeSinceSeen() <= 30) return false;
		if (utility.m_ThreatSystem.GetState() >= EAIThreatState.ALERTED &&
			utility.m_ThreatSystem.GetThreatMeasureWithoutInjuryFactor() > 0.01) return false;
		SCR_AIBehaviorBase behavior = utility.GetCurrentBehavior();
		return !SCR_AIAttackBehavior.Cast(behavior) && !SCR_AISuppressBehavior.Cast(behavior);
	}
}

class AICF_AcquisitionStagingReceipt
{
	protected AICF_TransportTrip m_Trip;
	protected AICF_StrategicAssignmentSnapshot m_Assignment;
	protected AICF_VehicleRequestState m_Request;
	protected AICF_VehicleSpawnPlan m_Plan;
	protected AICF_VehicleSpawnSiteSelection m_Selection;
	protected SCR_CampaignMilitaryBaseComponent m_Base;
	protected AICF_VehicleSpawnSiteReservation m_Reservation;
	protected AICF_VehicleSpawner m_Spawner;
	protected AICF_VehiclePreset m_Preset;
	protected AICF_GroupSlot m_PresetSlot;
	protected int m_iPresetRevision;
	protected SCR_AIGroup m_Group;
	protected EntityID m_GroupId;
	protected string m_sGroupRpl;
	protected BaseWorld m_World;
	protected int m_iGroupGeneration;
	protected int m_iTripGeneration;
	protected int m_iAssignmentRevision;
	protected int m_iIntentRevision;
	protected int m_iBaseRevision;
	protected int m_iRequestGeneration;
	protected string m_sReservationToken;
	protected string m_sIdentity;
	protected int m_iTripDeadline;
	protected int m_iRequestDeadline;
	protected int m_iSiteDeadline;
	protected int m_iCapturedAtMs;
	protected int m_iExpiresAtMs;
	protected vector m_vStaging;
	protected vector m_vSpawn;
	protected float m_fRadius;
	protected float m_fEnvelope;
	protected bool m_bManual;
	protected ref array<ref AICF_AcquisitionStagedBody> m_aBodies = {};

	static AICF_AcquisitionStagingReceipt Capture(AICF_TransportTrip trip, AICF_VehicleSpawner spawner,
		float radius, float envelope, int nowMs)
	{
		if (!trip || !trip.GetAssignment() || !trip.GetRequestState() || !spawner) return null;
		AICF_StrategicAssignmentSnapshot assignment = trip.GetAssignment();
		AICF_VehicleRequestState request = trip.GetRequestState();
		AICF_VehicleSpawnPlan plan = request.GetSpawnPlan();
		if (!plan || !plan.IsValid() || plan.GetAliveCount() <= 0 || plan.GetStagedCount() != plan.GetAliveCount()) return null;
		AICF_AcquisitionStagingReceipt receipt = new AICF_AcquisitionStagingReceipt();
		receipt.m_Trip = trip;
		receipt.m_Assignment = assignment;
		receipt.m_Request = request;
		receipt.m_Plan = plan;
		receipt.m_Selection = plan.GetSelection();
		receipt.m_Base = receipt.m_Selection.m_Base;
		receipt.m_Reservation = plan.GetReservation();
		receipt.m_Spawner = spawner;
		receipt.m_Preset = trip.GetVehiclePreset();
		receipt.m_PresetSlot = trip.GetVehiclePresetSlot();
		receipt.m_iPresetRevision = trip.GetVehiclePresetRevision();
		receipt.m_Group = receipt.m_Assignment.GetGroup();
		if (!receipt.m_Group || !receipt.m_Reservation) return null;
		receipt.m_GroupId = receipt.m_Group.GetID();
		receipt.m_sGroupRpl = AICF_VehicleBehaviorBinding.ComponentRpl(receipt.m_Group);
		receipt.m_World = receipt.m_Group.GetWorld();
		receipt.m_iGroupGeneration = trip.GetGroupGeneration();
		receipt.m_iTripGeneration = trip.GetTripGeneration();
		receipt.m_sIdentity = string.Format("faction=%1 slot=%2 numeric_slot=%3 group_generation=%4 trip_generation=%5 operation_id=%6",
			trip.GetFactionKey(), trip.GetSlotKey(), trip.GetSlotId(), receipt.m_iGroupGeneration, receipt.m_iTripGeneration, trip.GetOperationId());
		receipt.m_iAssignmentRevision = receipt.m_Assignment.GetAssignmentRevision();
		receipt.m_iIntentRevision = receipt.m_Assignment.GetStrategicIntentRevision();
		receipt.m_iBaseRevision = receipt.m_Assignment.GetBaseRevision();
		receipt.m_iRequestGeneration = receipt.m_Request.GetRequestGeneration();
		receipt.m_sReservationToken = receipt.m_Reservation.GetOwnerToken();
		receipt.m_iCapturedAtMs = nowMs;
		receipt.m_iTripDeadline = trip.AcquisitionHoldDeadlineAtMs(nowMs);
		receipt.m_iRequestDeadline = receipt.m_Request.GetAbsoluteDeadlineMs();
		receipt.m_iSiteDeadline = receipt.m_Reservation.GetExpiresAtMs();
		receipt.m_iExpiresAtMs = Math.Min(receipt.m_iTripDeadline, receipt.m_iRequestDeadline);
		receipt.m_iExpiresAtMs = Math.Min(receipt.m_iExpiresAtMs, receipt.m_iSiteDeadline);
		receipt.m_vStaging = plan.GetStagingPosition();
		receipt.m_vSpawn = plan.GetSpawnPosition();
		receipt.m_fRadius = radius;
		receipt.m_fEnvelope = envelope;
		receipt.m_bManual = plan.RequiresManualRally();
		array<AIAgent> agents = {};
		receipt.m_Group.GetAgents(agents);
		if (agents.Count() != plan.GetAliveCount() || agents.IsEmpty() || agents.Count() > AICF_Stage1Config.MAX_GROUP_SIZE) return null;
		array<AIAgent> unique = {};
		foreach (AIAgent agent : agents)
		{
			if (!agent || unique.Contains(agent)) return null;
			unique.Insert(agent);
			AICF_AcquisitionStagedBody body = AICF_AcquisitionStagedBody.Capture(agent);
			if (!body) return null;
			receipt.m_aBodies.Insert(body);
		}
		if (!receipt.IsCurrent(trip, nowMs)) return null;
		return receipt;
	}

	SCR_AIGroup GetGroup() { return m_Group; }
	AICF_VehicleSpawnPlan GetPlan() { return m_Plan; }
	BaseWorld GetWorld() { return m_World; }
	vector GetStagingPosition() { return m_vStaging; }
	float GetRadius() { return m_fRadius; }
	int GetExpiresAtMs() { return m_iExpiresAtMs; }
	int GetBaseRevision() { return m_iBaseRevision; }
	string GetIdentityDetails() { return m_sIdentity; }

	bool IsGroupIdentityCurrent()
	{
		return AICF_VehicleBoardingMutationFence.IsAuthoritativeReplicatedEntity(m_Group) && m_World && m_Group.GetWorld() == m_World && m_Group.GetID() == m_GroupId &&
			m_World.FindEntityByID(m_GroupId) == m_Group && AICF_VehicleBehaviorBinding.ComponentRpl(m_Group) == m_sGroupRpl;
	}

	bool IsGroupAbsent()
	{
		return !m_Group && m_World && !m_World.FindEntityByID(m_GroupId);
	}

	bool IsCurrent(AICF_TransportTrip trip, int nowMs)
	{
		if (!Replication.IsServer() || !GetGame() || !trip || trip != m_Trip || !trip.IsValid() || trip.IsTerminal() ||
			trip.GetPhase() != AICF_ETransportTripPhase.SPAWN_COMMIT || trip.IsDeadlineReached(nowMs)) return false;
		if (!m_Assignment || trip.GetAssignment() != m_Assignment || !m_Request || trip.GetRequestState() != m_Request ||
			!m_Plan || m_Request.GetSpawnPlan() != m_Plan || !m_Spawner || !m_Reservation) return false;
		if (nowMs >= m_iExpiresAtMs || m_iExpiresAtMs <= 0 || m_Request.IsDeadlineReached(nowMs) ||
			trip.AcquisitionHoldDeadlineAtMs(nowMs) != m_iTripDeadline || m_Request.GetAbsoluteDeadlineMs() != m_iRequestDeadline) return false;
		if (!AICF_VehicleBoardingMutationFence.IsAuthoritativeReplicatedEntity(m_Group)) return false;
		if (trip.GetGroupGeneration() != m_iGroupGeneration || trip.GetTripGeneration() != m_iTripGeneration ||
			m_Assignment.GetGroup() != m_Group || !IsGroupIdentityCurrent() || GetGame().GetWorld() != m_World) return false;
		if (m_Assignment.GetAssignmentRevision() != m_iAssignmentRevision || m_Assignment.GetStrategicIntentRevision() != m_iIntentRevision ||
			m_Assignment.GetBaseRevision() != m_iBaseRevision || m_Request.GetRequestGeneration() != m_iRequestGeneration) return false;
		if (m_Reservation.GetOwnerToken() != m_sReservationToken) return false;
		if (!m_Plan.IsValid() || m_Plan.GetReservation() != m_Reservation || m_Plan.GetStagingPosition() != m_vStaging ||
			m_Plan.GetSpawnPosition() != m_vSpawn || m_Plan.RequiresManualRally() != m_bManual ||
			!m_Spawner.IsSiteReservationCurrentReadOnly(m_Reservation, trip, m_Request, nowMs)) return false;
		AICF_GroupSlot slot = trip.GetVehiclePresetSlot();
		if (trip.GetVehiclePreset() && (!trip.IsVehiclePresetCurrent() || !slot || slot.GetGroup() != m_Group ||
			slot.GetSpawnGeneration() != m_iGroupGeneration || slot.GetStrategicAssignmentRevision() != m_iAssignmentRevision ||
			slot.GetStrategicIntentRevision() != m_iIntentRevision)) return false;
		AICF_VehicleLease lease = trip.GetLease();
		if (lease && (lease.HasPhysicalAsset() || lease.GetState() != AICF_EVehicleLeaseState.RESERVED ||
			!lease.MatchesTripIdentity(trip.GetFactionKey(), trip.GetSlotId(), m_iGroupGeneration, m_iTripGeneration))) return false;
		SCR_AIGroupUtilityComponent utility = m_Group.GetGroupUtilityComponent();
		if (!utility || utility.m_Owner != m_Group || !utility.m_Perception) return false;
		SCR_AIGroupTargetCluster cluster = utility.m_Perception.m_MostDangerousCluster;
		if (cluster && cluster.m_State.GetTimeSinceLastNewInformation() <= 30) return false;
		array<AIAgent> agents = {};
		m_Group.GetAgents(agents);
		if (agents.Count() != m_aBodies.Count() || agents.IsEmpty()) return false;
		array<AIAgent> seen = {};
		foreach (AICF_AcquisitionStagedBody body : m_aBodies)
		{
			if (!body || !body.IsCurrent(m_Group, m_vStaging, m_vSpawn, m_fRadius, m_fEnvelope, m_bManual) ||
				!agents.Contains(body.GetAgent()) || seen.Contains(body.GetAgent())) return false;
			seen.Insert(body.GetAgent());
		}
		return IsGroupIdentityCurrent() && HasTaskFields(trip, nowMs);
	}

	bool HasSourceIdentity(AICF_TransportTrip trip)
	{
		if (!trip || trip != m_Trip || !m_Assignment || trip.GetAssignment() != m_Assignment || !m_Request ||
			trip.GetRequestState() != m_Request || !m_Plan || m_Request.GetSpawnPlan() != m_Plan || !m_Group) return false;
		if (trip.IsTerminal() || trip.GetPhase() != AICF_ETransportTripPhase.SPAWN_COMMIT) return false;
		AICF_VehicleLease currentLease = trip.GetLease();
		if (currentLease && (currentLease.HasPhysicalAsset() || currentLease.GetState() != AICF_EVehicleLeaseState.RESERVED ||
			!currentLease.MatchesTripIdentity(trip.GetFactionKey(), trip.GetSlotId(), m_iGroupGeneration, m_iTripGeneration))) return false;
		if (!m_Plan.IsValid() || !m_Selection || m_Plan.GetSelection() != m_Selection || !m_Base || m_Selection.m_Base != m_Base) return false;
		if (trip.GetVehiclePreset() != m_Preset || trip.GetVehiclePresetSlot() != m_PresetSlot ||
			trip.GetVehiclePresetRevision() != m_iPresetRevision) return false;
		if (m_Preset && (!m_PresetSlot || m_PresetSlot.GetVehiclePresetRevision() != m_iPresetRevision ||
			m_PresetSlot.GetGroup() != m_Group || m_PresetSlot.GetSpawnGeneration() != m_iGroupGeneration ||
			m_PresetSlot.GetStrategicAssignmentRevision() != m_iAssignmentRevision || m_PresetSlot.GetStrategicIntentRevision() != m_iIntentRevision)) return false;
		if (trip.GetGroupGeneration() != m_iGroupGeneration || trip.GetTripGeneration() != m_iTripGeneration ||
			m_Request.GetRequestGeneration() != m_iRequestGeneration || m_Assignment.GetGroup() != m_Group) return false;
		if (m_Assignment.GetAssignmentRevision() != m_iAssignmentRevision ||
			m_Assignment.GetStrategicIntentRevision() != m_iIntentRevision || m_Assignment.GetBaseRevision() != m_iBaseRevision) return false;
		if (trip.AcquisitionHoldDeadlineAtMs(m_iCapturedAtMs) != m_iTripDeadline ||
			m_Request.GetAbsoluteDeadlineMs() != m_iRequestDeadline || !m_Reservation || m_Plan.GetReservation() != m_Reservation) return false;
		if (m_Reservation.GetOwnerToken() != m_sReservationToken || m_Reservation.GetExpiresAtMs() != m_iSiteDeadline || m_Reservation.GetBase() != m_Base ||
			!m_Reservation.MatchesTrip(trip, m_iRequestGeneration)) return false;
		if (m_Plan.GetStagingPosition() != m_vStaging || m_Plan.GetSpawnPosition() != m_vSpawn ||
			m_Plan.RequiresManualRally() != m_bManual || m_Plan.GetAliveCount() != m_aBodies.Count()) return false;
		return m_Plan.GetStagedCount() == m_aBodies.Count();
	}

	bool HasTaskFields(AICF_TransportTrip trip, int nowMs)
	{
		return HasSourceIdentity(trip) && m_iExpiresAtMs > 0 && nowMs < m_iExpiresAtMs &&
			!trip.IsDeadlineReached(nowMs) && !m_Request.IsDeadlineReached(nowMs);
	}

	// Ownership-only retirement fence: phase, lease, health and staging may end.
	bool OwnsCleanup(AICF_TransportTrip trip, AICF_VehicleSpawnPlan plan)
	{
		if (!trip || trip != m_Trip || !trip.GetAssignment() || !m_Request || trip.GetRequestState() != m_Request || !plan || plan != m_Plan) return false;
		if (m_Request.GetSpawnPlan() != plan || trip.GetGroupGeneration() != m_iGroupGeneration ||
			trip.GetTripGeneration() != m_iTripGeneration || m_Request.GetRequestGeneration() != m_iRequestGeneration) return false;
		return m_Reservation && plan.GetReservation() == m_Reservation && m_Reservation.GetOwnerToken() == m_sReservationToken && m_Reservation.GetBase() == m_Base &&
			m_Reservation.MatchesTrip(trip, m_iRequestGeneration);
	}
}

// Immutable owned intent. Failed cleanup must finish the original End before
// any subsequent acquisition/economy decision, even while staging remains valid.
class AICF_AcquisitionHoldRetirement
{
	protected AICF_TransportTrip m_Trip;
	protected AICF_StrategicAssignmentSnapshot m_Assignment;
	protected AICF_VehicleRequestState m_Request;
	protected AICF_VehicleSpawnPlan m_Plan;
	protected AICF_VehicleAcquisitionHold m_Hold;
	protected AICF_GroupSlot m_Slot;
	protected int m_iGroupGeneration;
	protected int m_iTripGeneration;
	protected int m_iRequestGeneration;
	protected int m_iAssignmentRevision;
	protected int m_iIntentRevision;
	protected int m_iBaseRevision;
	protected int m_iPresetRevision;
	protected AICF_ETransportTripPhase m_SourcePhase;
	protected AICF_ETransportTripPhase m_TerminalPhase;
	protected string m_sReason;
	protected string m_sCausation;
	protected bool m_bRestore;

	static AICF_AcquisitionHoldRetirement Capture(AICF_TransportTrip trip, AICF_GroupSlot slot,
		AICF_VehicleSpawnPlan plan, AICF_VehicleAcquisitionHold hold, AICF_ETransportTripPhase terminalPhase,
		string reason, string causation, bool restore)
	{
		if (!trip || !trip.GetAssignment() || !plan || !hold || plan.GetAcquisitionHold() != hold ||
			!hold.GetReceipt() || !hold.GetReceipt().OwnsCleanup(trip, plan)) return null;
		AICF_AcquisitionHoldRetirement intent = new AICF_AcquisitionHoldRetirement();
		intent.m_Trip = trip;
		intent.m_Assignment = trip.GetAssignment();
		intent.m_Request = trip.GetRequestState();
		intent.m_Plan = plan;
		intent.m_Hold = hold;
		intent.m_Slot = slot;
		intent.m_iGroupGeneration = trip.GetGroupGeneration();
		intent.m_iTripGeneration = trip.GetTripGeneration();
		intent.m_iRequestGeneration = intent.m_Request.GetRequestGeneration();
		intent.m_iAssignmentRevision = intent.m_Assignment.GetAssignmentRevision();
		intent.m_iIntentRevision = intent.m_Assignment.GetStrategicIntentRevision();
		intent.m_iBaseRevision = intent.m_Assignment.GetBaseRevision();
		intent.m_iPresetRevision = trip.GetVehiclePresetRevision();
		intent.m_SourcePhase = trip.GetPhase();
		intent.m_TerminalPhase = terminalPhase;
		intent.m_sReason = reason;
		intent.m_sCausation = causation;
		intent.m_bRestore = restore;
		return intent;
	}

	AICF_ETransportTripPhase GetTerminalPhase() { return m_TerminalPhase; }
	string GetReason() { return m_sReason; }
	string GetCausationId() { return m_sCausation; }
	bool RestoreCurrentOrder() { return m_bRestore; }
	int GetBaseRevision() { return m_iBaseRevision; }

	bool IsEpochCurrent(AICF_TransportTrip trip, AICF_GroupSlot slot, bool released)
	{
		if (!trip || trip != m_Trip || slot != m_Slot || !m_Assignment || trip.GetAssignment() != m_Assignment ||
			!m_Request || trip.GetRequestState() != m_Request || !m_Plan) return false;
		if (trip.IsTerminal() || trip.GetPhase() != m_SourcePhase || trip.GetGroupGeneration() != m_iGroupGeneration ||
			trip.GetTripGeneration() != m_iTripGeneration || m_Request.GetRequestGeneration() != m_iRequestGeneration) return false;
		if (m_Assignment.GetAssignmentRevision() != m_iAssignmentRevision || m_Assignment.GetStrategicIntentRevision() != m_iIntentRevision ||
			m_Assignment.GetBaseRevision() != m_iBaseRevision || trip.GetVehiclePresetRevision() != m_iPresetRevision) return false;
		if (slot && (slot.GetGroup() != m_Assignment.GetGroup() || slot.GetSpawnGeneration() != m_iGroupGeneration ||
			slot.GetStrategicAssignmentRevision() != m_iAssignmentRevision || slot.GetStrategicIntentRevision() != m_iIntentRevision)) return false;
		if (trip.GetVehiclePreset() && (!slot || slot.GetVehiclePresetRevision() != m_iPresetRevision)) return false;
		if (released) return m_Request.GetSpawnPlan() == null && m_Plan.GetAcquisitionHold() == null;
		return m_Request.GetSpawnPlan() == m_Plan && m_Plan.GetAcquisitionHold() == m_Hold;
	}
}

// Plan owns this token. It never mutates engine state; Handoff owns the queue.
class AICF_VehicleAcquisitionHold
{
	protected ref AICF_AcquisitionStagingReceipt m_Receipt;
	protected ref AICF_AcquisitionHoldRetirement m_Retirement;
	protected AIWaypoint m_Waypoint;
	protected EntityID m_WaypointId;
	protected string m_sWaypointRpl;
	protected vector m_vWaypointOrigin;

	static bool IsWaitOutcome(AICF_TripOutcome outcome)
	{
		if (!outcome || outcome.GetKind() != AICF_ETripOutcomeKind.WAIT) return false;
		return outcome.GetReason() == "VEHICLE_QUOTA_PENDING" || outcome.GetReason() == "SPAWN_COMMIT_CAP_WAIT" ||
			outcome.GetReason() == "SPAWN_BACKOFF";
	}

	static AICF_VehicleAcquisitionHold Capture(AICF_AcquisitionStagingReceipt receipt, AIWaypoint waypoint)
	{
		SCR_DefendWaypoint defend = SCR_DefendWaypoint.Cast(waypoint);
		if (!receipt || !defend || !defend.GetCurrentDefendPreset() ||
			!AICF_VehicleBoardingMutationFence.IsAuthoritativeReplicatedEntity(waypoint)) return null;
		vector actualOrigin = waypoint.GetOrigin();
		vector staging = receipt.GetStagingPosition();
		if (actualOrigin[0] != staging[0] || actualOrigin[2] != staging[2]) return null;
		AICF_VehicleAcquisitionHold hold = new AICF_VehicleAcquisitionHold();
		hold.m_Receipt = receipt;
		hold.m_Waypoint = waypoint;
		hold.m_WaypointId = waypoint.GetID();
		hold.m_sWaypointRpl = AICF_VehicleBehaviorBinding.ComponentRpl(waypoint);
		hold.m_vWaypointOrigin = actualOrigin;
		return hold;
	}

	AIWaypoint GetWaypoint() { return m_Waypoint; }
	AICF_AcquisitionStagingReceipt GetReceipt() { return m_Receipt; }
	AICF_AcquisitionHoldRetirement GetRetirement() { return m_Retirement; }

	bool BeginRetirement(AICF_TransportTrip trip, AICF_GroupSlot slot, AICF_VehicleSpawnPlan plan,
		AICF_ETransportTripPhase phase, string reason, string causation, bool restore)
	{
		if (m_Retirement) return m_Retirement.IsEpochCurrent(trip, slot, false);
		m_Retirement = AICF_AcquisitionHoldRetirement.Capture(trip, slot, plan, this, phase, reason, causation, restore);
		return m_Retirement != null;
	}

	bool HasOwnedEntity()
	{
		if (!m_Receipt || !m_Receipt.GetWorld()) return false;
		if (!m_Waypoint) return !m_Receipt.GetWorld().FindEntityByID(m_WaypointId);
		return AICF_VehicleBoardingMutationFence.IsAuthoritativeReplicatedEntity(m_Waypoint) &&
			m_Waypoint.GetWorld() == m_Receipt.GetWorld() && m_Waypoint.GetID() == m_WaypointId &&
			m_Receipt.GetWorld().FindEntityByID(m_WaypointId) == m_Waypoint &&
			AICF_VehicleBehaviorBinding.ComponentRpl(m_Waypoint) == m_sWaypointRpl;
	}

	bool IsCurrent(AICF_TransportTrip trip, int nowMs)
	{
		if (m_Retirement) return false;
		if (!m_Receipt || !m_Receipt.IsCurrent(trip, nowMs) || m_Receipt.GetPlan().GetAcquisitionHold() != this) return false;
		if (!m_Waypoint || !HasOwnedEntity() || m_Waypoint.GetOrigin() != m_vWaypointOrigin) return false;
		return m_Receipt.GetPlan().GetAcquisitionHold() == this && m_Receipt.HasTaskFields(trip, nowMs);
	}

	bool HasCurrentTask(AICF_TransportTrip trip, int currentBaseRevision, int nowMs)
	{
		if (!IsCurrent(trip, nowMs) || currentBaseRevision != m_Receipt.GetBaseRevision()) return false;
		SCR_AIGroup group = m_Receipt.GetGroup();
		array<AIWaypoint> queue = {};
		group.GetWaypoints(queue);
		if (!queue.Contains(m_Waypoint) || group.GetCurrentWaypoint() != m_Waypoint) return false;
		SCR_AIGroupUtilityComponent utility = group.GetGroupUtilityComponent();
		if (!utility) return false;
		SCR_AIDefendActivity activity = SCR_AIDefendActivity.Cast(utility.GetCurrentAction());
		return activity && activity.m_RelatedWaypoint == m_Waypoint && utility.GetExecutedAction() == activity &&
			activity.GetActionState() == EAIActionState.RUNNING && !activity.GetRemoveAction() && !activity.GetActionIsSuspended() &&
			IsCurrent(trip, nowMs) && m_Receipt.HasTaskFields(trip, System.GetTickCount());
	}
}
