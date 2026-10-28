// Снимок одного native failure до OnMoveFailed. Приказ и бюджет остаются у service.
class AICF_RecruitmentMoveFailure
{
	protected AICF_InfantryRecruitmentOrder m_Order;
	protected SCR_AIGroup m_Group;
	protected ChimeraCharacter m_Actor;
	protected AIAgent m_Agent;
	protected AIWaypoint m_Waypoint;
	protected EntityID m_GroupId;
	protected EntityID m_ActorId;
	protected EntityID m_WaypointId;
	protected string m_sGroupRpl;
	protected string m_sActorRpl;
	protected int m_iGeneration;
	protected int m_iAssignment;
	protected int m_iIntent;
	protected int m_iGraph;
	protected int m_iToken;
	protected int m_iRepair;
	protected int m_iCapturedAt;
	protected int m_iResult;
	protected vector m_vOrigin;
	protected vector m_vEndpoint;
	protected vector m_vService;
	protected float m_fRadius;
	protected bool m_bConsumed;

	static AICF_RecruitmentMoveFailure Capture(AICF_InfantryRecruitmentOrder order,
		SCR_AIGroupUtilityComponent utility, int result, int handler, bool related)
	{
		if (!order || !order.m_RecruitmentService || !order.m_RecruitmentService.IsCurrentOrder(order) ||
			!related || handler != AIGroupMovementComponent.DEFAULT_HANDLER_ID ||
			(result != EMoveError.STUCK && result != EMoveError.UNREACHABLE) || order.m_MoveFailure)
			return null;
		SCR_AIGroup group = order.m_Group;
		if (!group || group.GetGroupUtilityComponent() != utility || !utility) return null;
		SCR_AIActivityBase activity = SCR_AIActivityBase.Cast(utility.GetCurrentAction());
		AIActionBase action = utility.GetExecutedAction();
		if (!activity || action != activity || activity.m_RelatedWaypoint != order.m_Waypoint ||
			action.GetActionState() == EAIActionState.FAILED || action.GetActionState() == EAIActionState.COMPLETED)
			return null;
		array<AIAgent> agents = {};
		group.GetAgents(agents);
		// Group callback не указывает бойца. Только единственный участник однозначен.
		if (agents.Count() != 1 || !agents[0]) return null;
		ChimeraCharacter actor = ChimeraCharacter.Cast(agents[0].GetControlledEntity());
		if (!actor) return null;
		AICF_RecruitmentMoveFailure receipt = new AICF_RecruitmentMoveFailure();
		receipt.m_Order = order; receipt.m_Group = group; receipt.m_Actor = actor; receipt.m_Agent = agents[0];
		receipt.m_Waypoint = order.m_Waypoint;
		if (!receipt.m_Waypoint) return null;
		receipt.m_GroupId = group.GetID(); receipt.m_ActorId = actor.GetID();
		receipt.m_WaypointId = receipt.m_Waypoint.GetID();
		receipt.m_sGroupRpl = Replication.FindItemId(group).ToString();
		receipt.m_sActorRpl = Replication.FindItemId(actor).ToString();
		receipt.m_iGeneration = order.m_iGeneration; receipt.m_iAssignment = order.m_iAssignment;
		receipt.m_iIntent = order.m_iIntent; receipt.m_iGraph = order.m_iGraphRevision;
		receipt.m_iToken = order.m_iToken; receipt.m_iRepair = order.m_iApproachRepairs;
		receipt.m_iCapturedAt = System.GetTickCount(); receipt.m_iResult = result;
		receipt.m_vOrigin = actor.GetOrigin(); receipt.m_vEndpoint = receipt.m_Waypoint.GetOrigin();
		receipt.m_vService = order.m_vPosition; receipt.m_fRadius = receipt.m_Waypoint.GetCompletionRadius();
		if (!receipt.IsCurrent(order, receipt.m_iRepair)) return null;
		return receipt;
	}

	bool IsCurrent(AICF_InfantryRecruitmentOrder order, int repair)
	{
		if (m_bConsumed || order != m_Order || !order || !order.m_RecruitmentService ||
			!order.m_RecruitmentService.IsCurrentOrder(order) || !order.IsCurrent(order.m_Slot) ||
			!order.m_Slot.MatchesRecruitmentOrder(order) || order.m_bAwaitingSupplyPlan || order.m_bDemandReleased ||
			order.m_Donor || order.m_iArrivedAtMs > 0 || !order.HasSafeBarracks()) return false;
		int age = System.GetTickCount() - m_iCapturedAt;
		if (age < 0 || age > 5000 || m_iRepair < 0 || m_iRepair >= 2 || repair != order.m_iApproachRepairs ||
			order.m_iGeneration != m_iGeneration || order.m_iAssignment != m_iAssignment ||
			order.m_iIntent != m_iIntent || order.m_iGraphRevision != m_iGraph || order.m_iToken != m_iToken)
			return false;
		if (
			order.m_Group != m_Group || !m_Group || m_Group.GetID() != m_GroupId ||
			order.m_vPosition != m_vService || order.m_Waypoint != m_Waypoint || !m_Waypoint ||
			m_Waypoint.GetID() != m_WaypointId || m_Waypoint.GetOrigin() != m_vEndpoint ||
			m_Waypoint.GetCompletionRadius() != m_fRadius || m_Group.GetCurrentWaypoint() != m_Waypoint)
			return false;
		if (m_iResult != EMoveError.STUCK && m_iResult != EMoveError.UNREACHABLE) return false;
		array<AIWaypoint> queue = {}; m_Group.GetWaypoints(queue);
		array<AIAgent> agents = {}; m_Group.GetAgents(agents);
		if (!queue.Contains(m_Waypoint) || agents.Count() != 1 || agents[0] != m_Agent ||
			!m_Agent || m_Agent.GetParentGroup() != m_Group || m_Agent.GetControlledEntity() != m_Actor ||
			!m_Actor || m_Actor.GetID() != m_ActorId || !m_Actor.GetAIControlComponent() ||
			m_Actor.GetAIControlComponent().GetControlAIAgent() != m_Agent)
			return false;
		if (
			!AICF_VehicleBoardingMutationFence.IsAuthoritativeAIEntity(m_Actor) ||
			!AICF_VehicleBoardingMutationFence.IsAuthoritativeReplicatedEntity(m_Group) ||
			Replication.FindItemId(m_Group).ToString() != m_sGroupRpl ||
			Replication.FindItemId(m_Actor).ToString() != m_sActorRpl ||
			vector.Distance(m_Actor.GetOrigin(), m_vOrigin) > 0.5) return false;
		CharacterControllerComponent controller = m_Actor.GetCharacterController();
		FactionAffiliationComponent faction = FactionAffiliationComponent.Cast(m_Actor.FindComponent(FactionAffiliationComponent));
		CompartmentAccessComponent access = m_Actor.GetCompartmentAccessComponent();
		return controller && !controller.IsPlayerControlled() && faction && faction.GetAffiliatedFaction() == order.m_Faction &&
			access && !m_Actor.IsInVehicle() && !CompartmentAccessComponent.GetVehicleIn(m_Actor) &&
			!access.IsInCompartment() && !access.IsGettingIn() && !access.IsGettingOut() && !access.IsSwitchingSeatsAnim();
	}

	bool IsCapturedCurrent(AICF_InfantryRecruitmentOrder order)
	{
		return IsCurrent(order, m_iRepair);
	}

	// Вызывается внутри уже списанного обычного ремонта. Native action больше не читается.
	bool TryRecover(AICF_InfantryRecruitmentOrder order)
	{
		if (!IsCurrent(order, m_iRepair + 1) || order.m_MoveFailure != this || !order.m_bMovementFailed) return false;
		vector destination;
		if (!FindDestination(destination)) return false;
		vector checked;
		if (!IsCurrent(order, m_iRepair + 1) || !SafeDestination(destination, checked) ||
			vector.Distance(checked, destination) > 0.1 || vector.Distance(m_Actor.GetOrigin(), destination) > 8)
			return false;
		// До asynchronous Teleport: повторное событие не получает второй перенос.
		m_bConsumed = true;
		vector transform[4];
		m_Actor.GetWorldTransform(transform);
		transform[3] = destination;
		m_Actor.Teleport(transform);
		order.Log("INFANTRY_RECRUITMENT_SHORT_SUBMITTED", string.Format(
			"actor=%1 before=%2 destination=%3 shift_m=%4 result=%5 repair=%6 movement_confirmation=PENDING paid=0",
			m_ActorId, m_vOrigin, destination, vector.Distance(m_vOrigin, destination), SCR_Enum.GetEnumName(EMoveError, m_iResult), order.m_iApproachRepairs));
		return true;
	}

	protected bool SafeDestination(vector query, out vector destination)
	{
		return AICF_InfantrySpawnPlacement.IsUsable(m_Group, query, destination) &&
			vector.DistanceSqXZ(query, destination) <= 0.25 &&
			vector.Distance(m_vOrigin, destination) >= 2 && vector.Distance(m_vOrigin, destination) <= 8 &&
			Math.AbsFloat(m_vOrigin[1] - destination[1]) <= 1 &&
			vector.DistanceXZ(destination, m_vService) > AICF_InfantryRecruitmentConfig.ARRIVAL_METERS + 5 &&
			vector.DistanceXZ(destination, m_vService) < vector.DistanceXZ(m_vOrigin, m_vService) - 1;
	}

	protected bool FindDestination(out vector destination)
	{
		vector forward = m_vService - m_vOrigin; forward[1] = 0;
		if (forward.Length() <= AICF_InfantryRecruitmentConfig.ARRIVAL_METERS + 8) return false;
		forward.Normalize();
		vector right = Vector(-forward[2], 0, forward[0]);
		// Forward, forward-left, forward-right: нет угадывания failed path segment.
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
}
