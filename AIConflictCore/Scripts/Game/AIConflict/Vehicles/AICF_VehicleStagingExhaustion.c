// Plan-owned immutable original roster. Read-only proof of an exhausted manual
// approach; it never grants physical recovery or changes taskless accounting.
class AICF_VehicleStagingExhaustion
{
	protected AICF_TransportTrip m_Trip;
	protected AICF_VehicleRequestState m_Request;
	protected AICF_VehicleSpawnPlan m_Plan;
	protected AICF_GroupSlot m_Slot;
	protected SCR_AIGroup m_Group;
	protected EntityID m_GroupId;
	protected string m_sGroupRpl;
	protected int m_iGroupGeneration;
	protected int m_iTripGeneration;
	protected int m_iRequestGeneration;
	protected int m_iPresetRevision;
	protected bool m_bCaptured;
	protected ref array<IEntity> m_aMembers = {};
	protected ref array<EntityID> m_aMemberIds = {};
	protected ref array<string> m_aMemberRpls = {};

	void AICF_VehicleStagingExhaustion(AICF_TransportTrip trip, AICF_VehicleSpawnPlan plan)
	{
		if (!Replication.IsServer() || !trip || !plan || !plan.RequiresManualRally() || !trip.GetVehiclePreset() || !trip.IsVehiclePresetCurrent()) return;
		m_Trip = trip; m_Request = trip.GetRequestState(); m_Plan = plan; m_Slot = trip.GetVehiclePresetSlot();
		if (!m_Request || m_Request.GetSpawnPlan() != plan || !m_Slot || !trip.GetAssignment()) return;
		m_Group = trip.GetAssignment().GetGroup();
		if (!m_Group) return;
		m_GroupId = m_Group.GetID(); m_sGroupRpl = AICF_VehicleBehaviorBinding.ComponentRpl(m_Group);
		m_iGroupGeneration = trip.GetGroupGeneration(); m_iTripGeneration = trip.GetTripGeneration();
		m_iRequestGeneration = m_Request.GetRequestGeneration(); m_iPresetRevision = trip.GetVehiclePresetRevision();
		for (int position; position < AICF_Stage1Config.MAX_GROUP_SIZE; position++)
		{
			IEntity actor = trip.VehiclePresetMember(position);
			m_aMembers.Insert(actor);
			EntityID id = EntityID.INVALID;
			if (actor) id = actor.GetID();
			m_aMemberIds.Insert(id);
			m_aMemberRpls.Insert(AICF_VehicleBehaviorBinding.ComponentRpl(actor));
		}
		m_bCaptured = IsRosterCurrent();
	}

	bool IsCaptured() { return m_bCaptured; }

	protected bool IsCurrent(AICF_StrategicAssignmentSnapshot current, AICF_VehicleSpawner spawner, int nowMs)
	{
		if (!m_bCaptured || !Replication.IsServer() || !m_Trip || !m_Trip.IsValid() || m_Trip.IsTerminal() ||
			m_Trip.GetPhase() != AICF_ETransportTripPhase.APPROACHING_SITE || m_Trip.GetLease() || m_Trip.IsVehiclePresetDebited() ||
			!m_Trip.GetVehiclePreset() || !m_Trip.IsVehiclePresetCurrent()) return false;
		AICF_StrategicAssignmentSnapshot observed = m_Trip.GetAssignment();
		if (!observed || !current || !current.IsValid() || !current.MatchesCurrent(m_Trip.GetFactionKey(), m_Trip.GetSlotId(), m_iGroupGeneration, m_Group) ||
			current.GetAssignmentRevision() != observed.GetAssignmentRevision() || current.GetBaseRevision() != observed.GetBaseRevision() ||
			current.GetStrategicIntentRevision() != observed.GetStrategicIntentRevision()) return false;
		if (m_Trip.GetTripGeneration() != m_iTripGeneration || m_Trip.GetGroupGeneration() != m_iGroupGeneration ||
			m_Trip.GetVehiclePresetRevision() != m_iPresetRevision || m_Trip.GetRequestState() != m_Request ||
			!m_Request || m_Request.GetRequestGeneration() != m_iRequestGeneration || m_Request.GetSpawnPlan() != m_Plan) return false;
		if (!m_Slot || m_Trip.GetVehiclePresetSlot() != m_Slot || !m_Slot.IsCombatReady() || m_Slot.GetGroup() != m_Group ||
			m_Slot.GetSpawnGeneration() != m_iGroupGeneration || m_Slot.GetSlotId() != m_Trip.GetSlotId() ||
			m_Slot.GetStrategicAssignmentRevision() != observed.GetAssignmentRevision() ||
			m_Slot.GetStrategicIntentRevision() != observed.GetStrategicIntentRevision()) return false;
		if (!m_Plan || !m_Plan.IsValid() || !m_Plan.RequiresManualRally() || !spawner ||
			!spawner.IsSiteReservationCurrentReadOnly(m_Plan.GetReservation(), m_Trip, m_Request, nowMs) ||
			m_Trip.IsDeadlineReached(nowMs) || m_Request.IsDeadlineReached(nowMs) || nowMs >= m_Plan.GetApproachDeadlineMs()) return false;
		return IsRosterCurrent();
	}

	protected bool IsRosterCurrent()
	{
		if (!m_Group || !m_Slot || !m_Trip || m_Group.GetID() != m_GroupId ||
			!AICF_VehicleBoardingMutationFence.IsAuthoritativeReplicatedEntity(m_Group) ||
			AICF_VehicleBehaviorBinding.ComponentRpl(m_Group) != m_sGroupRpl ||
			!m_Group.GetFaction() || m_Group.GetFaction().GetFactionKey() != m_Trip.GetFactionKey()) return false;
		if (m_aMembers.Count() != AICF_Stage1Config.MAX_GROUP_SIZE || m_aMemberIds.Count() != m_aMembers.Count() ||
			m_aMemberRpls.Count() != m_aMembers.Count()) return false;
		array<AIAgent> agents = {}; m_Group.GetAgents(agents);
		int captured;
		int assigned;
		foreach (int memberPosition : m_Trip.GetVehiclePreset().m_aMembers)
			if (memberPosition >= 0) assigned++;
		for (int position; position < m_aMembers.Count(); position++)
		{
			ChimeraCharacter actor = ChimeraCharacter.Cast(m_aMembers[position]);
			if (!actor)
			{
				if (m_aMemberIds[position] != EntityID.INVALID || m_Slot.GetRosterMember(position) || m_Trip.VehiclePresetMember(position)) return false;
				continue;
			}
			captured++;
			if (m_Slot.GetRosterMember(position) != actor || m_Trip.VehiclePresetMember(position) != actor || actor.GetID() != m_aMemberIds[position] ||
				AICF_VehicleBehaviorBinding.ComponentRpl(actor) != m_aMemberRpls[position] || !actor.GetAIControlComponent()) return false;
			AIAgent agent = actor.GetAIControlComponent().GetControlAIAgent();
			if (!agent || !agents.Contains(agent) || agent.GetParentGroup() != m_Group || agent.GetControlledEntity() != actor ||
				!AICF_VehicleBoardingMutationFence.IsAuthoritativeAIEntity(actor) || !SCR_AIDamageHandling.IsConscious(actor)) return false;
			CharacterControllerComponent controller = actor.GetCharacterController();
			CompartmentAccessComponent access = actor.GetCompartmentAccessComponent();
			FactionAffiliationComponent faction = FactionAffiliationComponent.Cast(actor.FindComponent(FactionAffiliationComponent));
			if (!controller || controller.IsPlayerControlled() || !faction || faction.GetAffiliatedFaction() != m_Group.GetFaction() || !access ||
				actor.IsInVehicle() || CompartmentAccessComponent.GetVehicleIn(actor) || access.IsInCompartment() || access.IsGettingIn() ||
				access.IsGettingOut() || access.IsSwitchingSeatsAnim()) return false;
		}
		return captured > 0 && captured == assigned && captured == m_Slot.GetDesiredSize() && captured == agents.Count();
	}

	protected bool HasCurrentActorTask()
	{
		for (int index; index < m_Plan.GetStagingActionTokenCount(); index++)
		{
			AICF_VehicleStagingActionToken token = m_Plan.GetStagingActionToken(index);
			if (!token || !token.Matches(m_Trip, m_iRequestGeneration, m_Group)) continue;
			SCR_AIMoveIndividuallyBehavior action = token.GetAction();
			if (!action || !token.IsActionOwnedByUtility() || action.GetRemoveAction() || action.GetActionIsSuspended() ||
				action.GetActionState() == EAIActionState.FAILED || action.GetActionState() == EAIActionState.COMPLETED) continue;
			SCR_AIUtilityComponent utility = SCR_AIUtilityComponent.Cast(token.GetAgent().FindComponent(SCR_AIUtilityComponent));
			if (utility && utility.m_OwnerEntity == token.GetReservedEntity() && utility.GetCurrentAction() == action && utility.GetExecutedAction() == action)
				return true;
		}
		return false;
	}

	bool ShouldEnd(AICF_StrategicAssignmentSnapshot current, AICF_VehicleSpawner spawner, int nowMs, float radius)
	{
		if (!(radius > 0) || !IsCurrent(current, spawner, nowMs) || m_Plan.GetApproachWaypointReissueCount() != 1 || HasCurrentActorTask()) return false;
		array<AIWaypoint> queue = {}; m_Group.GetWaypoints(queue);
		// Any actual waypoint prevents a fallback, including a foreign current task.
		if (!queue.IsEmpty() || m_Group.GetCurrentWaypoint()) return false;
		int outside;
		foreach (IEntity actor : m_aMembers)
		{
			if (!actor) continue;
			if (AICF_VehicleSpawner.IsStagingGeometryReady(vector.DistanceXZ(actor.GetOrigin(), m_Plan.GetStagingPosition()), radius,
				vector.DistanceXZ(actor.GetOrigin(), m_Plan.GetSpawnPosition()), 90, true)) continue;
			outside++;
			if (!m_Plan.WasStagingMemberRetried(actor) || !m_Plan.IsStagingMemberExhausted(actor) || m_Plan.FindStagingActionToken(actor)) return false;
		}
		return outside > 0;
	}
}
