// Узкая коррекция последнего неподвижного manual rally участника. Это не arrival
// proof: последующий обычный MeasureStagingReadiness заново подтверждает весь roster.
class AICF_VehicleManualStagingRecovery
{
	static const float MAX_SHIFT_METERS = 8.0;
	protected AICF_TransportTrip m_Trip;
	protected AICF_StrategicAssignmentSnapshot m_Assignment;
	protected AICF_StrategicAssignmentSnapshot m_CurrentAssignment;
	protected AICF_VehicleRequestState m_Request;
	protected AICF_VehicleSpawnPlan m_Plan;
	protected AICF_VehicleStagingActionToken m_Token;
	protected AICF_VehicleSpawner m_Spawner;
	protected SCR_AIGroup m_Group;
	protected ChimeraCharacter m_Actor;
	protected AIWaypoint m_Waypoint;
	protected EntityID m_GroupId;
	protected string m_sGroupRpl;
	protected int m_iRequestGeneration;
	protected int m_iPresetRevision;
	protected vector m_vOrigin;
	protected vector m_vStaging;
	protected vector m_vWaypoint;
	protected float m_fRadius;
	protected bool m_bConsumed;

	void AICF_VehicleManualStagingRecovery(AICF_TransportTrip trip, AICF_StrategicAssignmentSnapshot current,
		AICF_VehicleRequestState request, AICF_VehicleSpawnPlan plan, AICF_VehicleStagingActionToken token,
		AICF_VehicleSpawner spawner, float radius)
	{
		m_Trip = trip; m_CurrentAssignment = current; m_Request = request; m_Plan = plan; m_Token = token;
		m_Spawner = spawner; m_fRadius = radius;
		if (!trip || !plan || !token || !request) return;
		m_Assignment = trip.GetAssignment();
		if (!m_Assignment) return;
		m_Group = m_Assignment.GetGroup();
		m_Actor = ChimeraCharacter.Cast(token.GetReservedEntity());
		m_Waypoint = plan.GetApproachWaypoint();
		if (!m_Group || !m_Actor || !m_Waypoint) return;
		m_GroupId = m_Group.GetID(); m_sGroupRpl = AICF_VehicleBehaviorBinding.ComponentRpl(m_Group);
		m_iRequestGeneration = request.GetRequestGeneration(); m_iPresetRevision = trip.GetVehiclePresetRevision();
		m_vOrigin = m_Actor.GetOrigin(); m_vStaging = plan.GetStagingPosition(); m_vWaypoint = m_Waypoint.GetOrigin();
	}

	bool IsCurrent(int nowMs)
	{
		if (m_bConsumed || !Replication.IsServer() || !m_Trip || !m_Trip.IsValid() || m_Trip.IsTerminal() ||
			m_Trip.GetPhase() != AICF_ETransportTripPhase.APPROACHING_SITE || m_Trip.GetLease() ||
			!m_Trip.GetVehiclePreset() || !m_Trip.IsVehiclePresetCurrent() || m_Trip.IsVehiclePresetDebited()) return false;
		if (!m_Assignment || m_Trip.GetAssignment() != m_Assignment || !m_CurrentAssignment || !m_CurrentAssignment.IsValid() ||
			!m_CurrentAssignment.MatchesCurrent(m_Trip.GetFactionKey(), m_Trip.GetSlotId(), m_Trip.GetGroupGeneration(), m_Group) ||
			m_CurrentAssignment.GetAssignmentRevision() != m_Assignment.GetAssignmentRevision() ||
			m_CurrentAssignment.GetStrategicIntentRevision() != m_Assignment.GetStrategicIntentRevision() ||
			m_CurrentAssignment.GetBaseRevision() != m_Assignment.GetBaseRevision()) return false;
		AICF_GroupSlot slot = m_Trip.GetVehiclePresetSlot();
		if (!slot || !slot.IsCombatReady() || slot.GetGroup() != m_Group || slot.GetSpawnGeneration() != m_Trip.GetGroupGeneration() ||
			slot.GetSlotId() != m_Trip.GetSlotId() || slot.GetStrategicAssignmentRevision() != m_Assignment.GetAssignmentRevision() ||
			slot.GetStrategicIntentRevision() != m_Assignment.GetStrategicIntentRevision() ||
			m_Trip.GetVehiclePresetRevision() != m_iPresetRevision) return false;
		if (!m_Request || m_Trip.GetRequestState() != m_Request || m_Request.GetRequestGeneration() != m_iRequestGeneration ||
			m_Request.GetSpawnPlan() != m_Plan || !m_Plan || !m_Plan.IsValid() || !m_Plan.RequiresManualRally() ||
			!m_Spawner || !m_Spawner.IsSiteReservationCurrent(m_Plan.GetReservation(), m_Trip, m_Request, nowMs)) return false;
		if (m_Trip.IsDeadlineReached(nowMs) || m_Request.IsDeadlineReached(nowMs) || nowMs >= m_Plan.GetApproachDeadlineMs() ||
			m_Plan.GetStagingPosition() != m_vStaging || m_Plan.GetApproachWaypoint() != m_Waypoint || !m_Waypoint ||
			m_Waypoint.GetOrigin() != m_vWaypoint || m_Group.GetCurrentWaypoint() != m_Waypoint) return false;
		if (!m_Group || m_Group.GetID() != m_GroupId || !AICF_VehicleBoardingMutationFence.IsAuthoritativeReplicatedEntity(m_Group) ||
			AICF_VehicleBehaviorBinding.ComponentRpl(m_Group) != m_sGroupRpl ||
			!m_Group.GetFaction() || m_Group.GetFaction().GetFactionKey() != m_Trip.GetFactionKey()) return false;
		array<AIWaypoint> waypoints = {}; m_Group.GetWaypoints(waypoints);
		SCR_AIGroupUtilityComponent groupUtility = m_Group.GetGroupUtilityComponent();
		if (!waypoints.Contains(m_Waypoint) || !groupUtility) return false;
		SCR_AIActivityBase activity = SCR_AIActivityBase.Cast(groupUtility.GetCurrentAction());
		if (!activity || activity.m_RelatedWaypoint != m_Waypoint || groupUtility.GetExecutedAction() != activity ||
			activity.GetActionState() != EAIActionState.RUNNING) return false;
		if (!m_Token || m_Plan.FindStagingActionToken(m_Actor) != m_Token || !m_Token.Matches(m_Trip, m_iRequestGeneration, m_Group) ||
			m_Token.GetRetryCount() != 1 || !m_Plan.WasStagingMemberRetried(m_Actor) || !m_Plan.IsStagingMemberExhausted(m_Actor) ||
			m_Plan.WasStagingMemberRelocated(m_Actor) || m_Token.GetTargetPosition() != m_vStaging ||
			!m_Token.HasNoPhysicalMotion(nowMs, 5000)) return false;
		if (nowMs - m_Token.GetLastProgressAtMs() < 12000 || !m_Token.IsActionOwnedByUtility() ||
			!m_Token.GetAction() || m_Token.GetAction().GetActionState() != EAIActionState.RUNNING) return false;
		SCR_AIUtilityComponent utility = SCR_AIUtilityComponent.Cast(m_Token.GetAgent().FindComponent(SCR_AIUtilityComponent));
		if (!utility || utility.m_OwnerEntity != m_Actor || utility.GetCurrentAction() != m_Token.GetAction() ||
			utility.GetExecutedAction() != m_Token.GetAction() || vector.Distance(m_Actor.GetOrigin(), m_vOrigin) > 0.5) return false;
		return IsExactFootRoster(slot);
	}

	protected bool IsFitFootActor(ChimeraCharacter actor, AIAgent agent)
	{
		if (!actor || !agent || !AICF_VehicleBoardingMutationFence.IsAuthoritativeAIEntity(actor) ||
			!SCR_AIDamageHandling.IsConscious(actor) || agent.GetParentGroup() != m_Group || agent.GetControlledEntity() != actor ||
			!actor.GetAIControlComponent() || actor.GetAIControlComponent().GetControlAIAgent() != agent) return false;
		CharacterControllerComponent controller = actor.GetCharacterController();
		if (!controller || controller.IsPlayerControlled()) return false;
		FactionAffiliationComponent faction = FactionAffiliationComponent.Cast(actor.FindComponent(FactionAffiliationComponent));
		CompartmentAccessComponent access = actor.GetCompartmentAccessComponent();
		return faction && faction.GetAffiliatedFaction() == m_Group.GetFaction() && access && !actor.IsInVehicle() &&
			!CompartmentAccessComponent.GetVehicleIn(actor) && !access.IsInCompartment() && !access.IsGettingIn() &&
			!access.IsGettingOut() && !access.IsSwitchingSeatsAnim();
	}

	protected bool IsExactFootRoster(AICF_GroupSlot slot)
	{
		array<AIAgent> agents = {}; m_Group.GetAgents(agents);
		int captured;
		int assigned;
		foreach (int memberPosition : m_Trip.GetVehiclePreset().m_aMembers)
			if (memberPosition >= 0) assigned++;
		int outside;
		bool found;
		for (int position; position < AICF_Stage1Config.MAX_GROUP_SIZE; position++)
		{
			ChimeraCharacter actor = ChimeraCharacter.Cast(m_Trip.VehiclePresetMember(position));
			if (!actor) continue;
			captured++;
			if (slot.GetRosterMember(position) != actor || !actor.GetAIControlComponent()) return false;
			AIAgent agent = actor.GetAIControlComponent().GetControlAIAgent();
			if (!agents.Contains(agent) || !IsFitFootActor(actor, agent)) return false;
			if (actor == m_Actor) found = true;
			if (!AICF_VehicleSpawner.IsStagingGeometryReady(vector.DistanceXZ(actor.GetOrigin(), m_vStaging), m_fRadius,
				vector.DistanceXZ(actor.GetOrigin(), m_Plan.GetSpawnPosition()), 90, true)) outside++;
		}
		return found && captured > 1 && captured == assigned && captured == slot.GetDesiredSize() && captured == agents.Count() && outside == 1 &&
			!AICF_VehicleSpawner.IsStagingGeometryReady(vector.DistanceXZ(m_Actor.GetOrigin(), m_vStaging), m_fRadius,
				vector.DistanceXZ(m_Actor.GetOrigin(), m_Plan.GetSpawnPosition()), 90, true);
	}

	protected bool SafeDestination(vector query, out vector destination)
	{
		return AICF_InfantrySpawnPlacement.IsUsable(m_Group, query, destination) &&
			IsBoundedStep(vector.Distance(m_vOrigin, destination), Math.AbsFloat(m_vOrigin[1] - destination[1]),
				vector.DistanceSqXZ(query, destination), vector.DistanceXZ(m_vOrigin, m_vStaging),
				vector.DistanceXZ(destination, m_vStaging), AICF_VehicleSpawner.IsStagingGeometryReady(
					vector.DistanceXZ(destination, m_vStaging), m_fRadius, vector.DistanceXZ(destination, m_Plan.GetSpawnPosition()), 90, true));
	}

	static bool IsBoundedStep(float shift, float vertical, float projectionSq, float before, float after, bool rallyReady)
	{
		return shift >= 2 && shift <= MAX_SHIFT_METERS && vertical >= 0 && vertical <= 1 &&
			projectionSq >= 0 && projectionSq <= 0.25 && after >= 0 && after < before - 1 && rallyReady;
	}

	protected bool FindDestination(out vector destination)
	{
		vector forward = m_vStaging - m_vOrigin; forward[1] = 0;
		if (forward.Length() <= 1) return false;
		forward.Normalize();
		vector right = Vector(-forward[2], 0, forward[0]);
		for (int ring; ring < 3; ring++)
		{
			for (int direction; direction < 3; direction++)
			{
				float angle;
				if (direction == 1) angle = 45 * Math.DEG2RAD;
				if (direction == 2) angle = -45 * Math.DEG2RAD;
				vector query = m_vOrigin + (forward * Math.Cos(angle) + right * Math.Sin(angle)) * (3 + ring * 2);
				if (SafeDestination(query, destination)) return true;
			}
		}
		return false;
	}

	bool TryRecover()
	{
		if (!IsCurrent(System.GetTickCount())) return false;
		vector destination;
		if (!FindDestination(destination)) return false;
		vector checked;
		if (!IsCurrent(System.GetTickCount()) || !SafeDestination(destination, checked) ||
			vector.Distance(checked, destination) > 0.1 || !(vector.Distance(m_Actor.GetOrigin(), destination) <= MAX_SHIFT_METERS)) return false;
		// Existing one-shot marker is consumed before asynchronous Teleport. No new retry.
		m_bConsumed = true;
		m_Plan.MarkStagingMemberRelocated(m_Actor);
		vector transform[4]; m_Actor.GetWorldTransform(transform); transform[3] = destination;
		m_Actor.Teleport(transform);
		string details = string.Format("operation_id=%1 member=%2 request_generation=%3 action_token=%4 before=%5 destination=%6 shift_m=%7",
			m_Trip.GetOperationId(), m_Actor.GetID(), m_iRequestGeneration, m_Token.GetFence().GetToken(), m_vOrigin, destination,
			vector.Distance(m_vOrigin, destination));
		details += string.Format(" retry=%1 approach_deadline_ms=%2 trip_deadline_ms=%3 paid=0 movement_confirmation=PENDING",
			m_Token.GetRetryCount(), m_Plan.GetApproachDeadlineMs(), m_Trip.GetAbsoluteDeadlineMs());
		AICF_Stage3Diagnostics.Info("VEHICLE_MANUAL_STAGING_SHORT_SUBMITTED", details);
		return true;
	}
}
