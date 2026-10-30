// TEST ONLY. Opt-in decision receipts; no action, clock, physics or order writes.
// Every override returns one original delegate result. No static Capture override.
class AICF_RecruitmentDecisionSnapshot
{
	AICF_InfantryRecruitmentOrder m_Order;
	AICF_GroupSlot m_Slot;
	SCR_AIGroup m_Group;
	BaseWorld m_World;
	RplComponent m_Rpl;
	SCR_AIGroupUtilityComponent m_Utility;
	AIActionBase m_Current;
	AIActionBase m_Executed;
	AIWaypoint m_Related;
	AIWaypoint m_Waypoint;
	ChimeraCharacter m_Actor;
	AIAgent m_Agent;
	RplComponent m_ActorRpl;
	EntityID m_GroupId;
	EntityID m_ActorId;
	string m_sRpl;
	string m_sActorRpl;
	int m_iToken;
	int m_iGeneration;
	int m_iAssignment;
	int m_iIntent;
	int m_iGraph;
	int m_iRepair;
	int m_iCurrentState = -1;
	int m_iExecutedState = -1;
	int m_iCapturedAt;
	bool m_bHadActor;
	bool m_bHadCurrent;
	bool m_bHadExecuted;
	bool m_bStable;
	string m_sFields = "capture_incomplete=1 stable_read=0 unknown_is_not_permission=1";

	bool SameOwner(AICF_RecruitmentDecisionSnapshot other)
	{
		if (!other || m_Order != other.m_Order || m_Slot != other.m_Slot || m_Group != other.m_Group || m_World != other.m_World) return false;
		if (m_Rpl != other.m_Rpl || m_GroupId != other.m_GroupId || m_sRpl != other.m_sRpl) return false;
		if (m_iToken != other.m_iToken || m_iGeneration != other.m_iGeneration || m_iAssignment != other.m_iAssignment) return false;
		if (m_iIntent != other.m_iIntent || m_iGraph != other.m_iGraph || m_iRepair != other.m_iRepair) return false;
		if (m_Waypoint != other.m_Waypoint || m_Actor != other.m_Actor || m_ActorId != other.m_ActorId) return false;
		return m_Agent == other.m_Agent && m_ActorRpl == other.m_ActorRpl && m_sActorRpl == other.m_sActorRpl && m_bHadActor == other.m_bHadActor;
	}

	bool Stable()
	{
		if (!Replication.IsServer() || !GetGame() || !m_World || GetGame().GetWorld() != m_World) return false;
		if (!m_Group || m_Group.GetWorld() != m_World) return false;
		if (!m_Group || m_Group.GetID() != m_GroupId || !m_Group) return false;
		if (m_World.FindEntityByID(m_GroupId) != m_Group || !m_Group) return false;
		if (!m_Rpl || m_Group.FindComponent(RplComponent) != m_Rpl || !m_Rpl) return false;
		if (!m_Rpl.Id().IsValid() || m_Rpl.Id().ToString() != m_sRpl) return false;
		if (!m_Utility || m_Group.GetGroupUtilityComponent() != m_Utility) return false;
		if (m_Utility.GetCurrentAction() != m_Current || !m_Utility) return false;
		if (m_Utility.GetExecutedAction() != m_Executed) return false;
		if ((m_bHadCurrent && !m_Current) || (m_bHadExecuted && !m_Executed) || (m_bHadActor && !m_Actor)) return false;
		if (m_Current && m_Current.GetActionState() != m_iCurrentState) return false;
		if (m_Executed && m_Executed.GetActionState() != m_iExecutedState) return false;
		SCR_AIActivityBase activity = SCR_AIActivityBase.Cast(m_Current);
		if (activity && activity.m_RelatedWaypoint != m_Related) return false;
		if (!m_Order || !m_Slot || m_Order.m_Slot != m_Slot || m_Slot.AICF_RDOrder() != m_Order) return false;
		if (m_Slot.GetGroup() != m_Group || m_Slot.GetSpawnGeneration() != m_iGeneration || m_Order.m_Group != m_Group) return false;
		if (m_Order.m_iToken != m_iToken || m_Order.m_iGeneration != m_iGeneration || m_Order.m_iAssignment != m_iAssignment) return false;
		if (m_Order.m_iIntent != m_iIntent || m_Order.m_iGraphRevision != m_iGraph || m_Order.m_iApproachRepairs != m_iRepair || m_Order.m_Waypoint != m_Waypoint) return false;
		if (m_Actor)
		{
			if (!m_Agent || m_Actor.GetWorld() != m_World || !m_Actor) return false;
			if (m_Actor.GetID() != m_ActorId || !m_Actor) return false;
			if (m_World.FindEntityByID(m_ActorId) != m_Actor || !m_Actor) return false;
			if (!m_ActorRpl || m_Actor.FindComponent(RplComponent) != m_ActorRpl || !m_ActorRpl) return false;
			if (!m_ActorRpl.Id().IsValid() || m_ActorRpl.Id().ToString() != m_sActorRpl) return false;
			if (!m_Agent || m_Agent.GetControlledEntity() != m_Actor || !m_Agent || !m_Actor) return false;
			if (m_Agent.GetParentGroup() != m_Group || !m_Actor) return false;
			AIControlComponent control = m_Actor.GetAIControlComponent();
			if (!control || control.GetControlAIAgent() != m_Agent) return false;
		}
		return m_Group && m_Utility && (!m_bHadActor || m_Actor);
	}
}

class AICF_RecruitmentDecisionProbe
{
	static const int MAXIMUM_EVENTS = 256;
	protected static int s_iEvents;
	protected static bool s_bOverflow;

	static bool Enabled()
	{
		string value;
		return Replication.IsServer() && System.GetCLIParam("aicfRecruitmentDecisionProbe", value) && value == "1";
	}

	static AICF_RecruitmentDecisionSnapshot Read(AICF_InfantryRecruitmentOrder order, SCR_AIGroup group, SCR_AIGroupUtilityComponent utility)
	{
		if (!Enabled() || s_iEvents >= MAXIMUM_EVENTS || !GetGame() || !order || !group) return null;
		if (!utility && group) utility = group.GetGroupUtilityComponent();
		AICF_RecruitmentDecisionSnapshot c = new AICF_RecruitmentDecisionSnapshot();
		c.m_iCapturedAt = System.GetTickCount();
		c.m_Order = order; c.m_Group = group; c.m_Utility = utility;
		c.m_World = GetGame().GetWorld();
		if (order)
		{
			c.m_Slot = order.m_Slot; c.m_Waypoint = order.m_Waypoint;
			c.m_iToken = order.m_iToken; c.m_iGeneration = order.m_iGeneration;
			c.m_iAssignment = order.m_iAssignment; c.m_iIntent = order.m_iIntent;
			c.m_iGraph = order.m_iGraphRevision; c.m_iRepair = order.m_iApproachRepairs;
		}
		bool sameWorld, master, currentWaypoint, queuedWaypoint, actorMaster, actorWorldSame, actorPlayer;
		int actorLife = -1;
		int agentsCount = -1;
		string actorPosition = "UNKNOWN";
		if (group)
		{
			c.m_GroupId = group.GetID();
			if (!group) return c;
			sameWorld = group.GetWorld() == c.m_World;
			if (!group) return c;
			c.m_Rpl = RplComponent.Cast(group.FindComponent(RplComponent));
			if (c.m_Rpl) { c.m_sRpl = c.m_Rpl.Id().ToString(); master = c.m_Rpl.IsMaster(); }
			if (!group) return c;
			currentWaypoint = group.GetCurrentWaypoint() == c.m_Waypoint && c.m_Waypoint != null;
			if (!group) return c;
			array<AIWaypoint> queue = {}; group.GetWaypoints(queue);
			if (!group) return c;
			queuedWaypoint = c.m_Waypoint && queue.Contains(c.m_Waypoint);
			array<AIAgent> agents = {}; group.GetAgents(agents); agentsCount = agents.Count();
			if (agentsCount == 1 && agents[0])
			{
				c.m_Agent = agents[0]; c.m_Actor = ChimeraCharacter.Cast(c.m_Agent.GetControlledEntity());
				c.m_bHadActor = c.m_Actor != null;
				if (c.m_Actor)
				{
					c.m_ActorId = c.m_Actor.GetID();
					if (!c.m_Actor) return c;
					actorPosition = c.m_Actor.GetOrigin().ToString();
					if (!c.m_Actor) return c;
					actorWorldSame = c.m_Actor.GetWorld() == c.m_World;
					if (!c.m_Actor) return c;
					c.m_ActorRpl = RplComponent.Cast(c.m_Actor.FindComponent(RplComponent));
					if (c.m_ActorRpl)
					{
						c.m_sActorRpl = c.m_ActorRpl.Id().ToString();
						if (!c.m_ActorRpl) return c;
						actorMaster = c.m_ActorRpl.IsMaster();
					}
					if (!c.m_Actor) return c;
					CharacterControllerComponent character = c.m_Actor.GetCharacterController();
					if (character)
					{
						actorLife = character.GetLifeState();
						if (!character) return c;
						actorPlayer = character.IsPlayerControlled();
					}
				}
			}
		}
		if (utility)
		{
			c.m_Current = utility.GetCurrentAction();
			if (!utility) return c;
			c.m_Executed = utility.GetExecutedAction();
			c.m_bHadCurrent = c.m_Current != null; c.m_bHadExecuted = c.m_Executed != null;
			if (c.m_Current) c.m_iCurrentState = c.m_Current.GetActionState();
			if (c.m_Executed) c.m_iExecutedState = c.m_Executed.GetActionState();
			SCR_AIActivityBase activity = SCR_AIActivityBase.Cast(c.m_Current);
			if (activity) c.m_Related = activity.m_RelatedWaypoint;
		}
		string identity = string.Format("sample_tick=%1 group=%2 group_rpl=%3 world_same=%4 group_master=%5 token=%6 generation=%7 assignment=%8 intent=%9",
			c.m_iCapturedAt, c.m_GroupId, c.m_sRpl, sameWorld, master, c.m_iToken, c.m_iGeneration, c.m_iAssignment, c.m_iIntent);
		identity += string.Format(" graph=%1 repair=%2 order=%3 slot=%4 actor=%5 actor_rpl=%6 actor_position=%7 agent=%8 agents=%9",
			c.m_iGraph, c.m_iRepair, order, c.m_Slot, c.m_ActorId, c.m_sActorRpl, actorPosition, c.m_Agent, agentsCount);
		identity += string.Format(" current=%1 executed=%2 same_action=%3 current_state=%4 executed_state=%5 related_waypoint=%6 order_waypoint=%7 current_waypoint_exact=%8 queued_waypoint_exact=%9",
			c.m_Current, c.m_Executed, c.m_Current == c.m_Executed, c.m_iCurrentState, c.m_iExecutedState, c.m_Related, c.m_Waypoint, currentWaypoint, queuedWaypoint);
		identity += string.Format(" actor_world_same=%1 actor_master=%2 actor_life=%3 actor_player=%4 auxiliary_flags_snapshot_only=1", actorWorldSame, actorMaster, actorLife, actorPlayer);
		bool receipt;
		if (order) receipt = order.m_MoveFailure != null;
		c.m_bStable = c.Stable();
		c.m_sFields = identity + string.Format(" receipt_present=%1 stable_read=%2 unknown_is_not_permission=1", receipt, c.m_bStable);
		return c;
	}

	static void Record(string eventName, bool result, AICF_RecruitmentDecisionSnapshot before, AICF_InfantryRecruitmentOrder order, SCR_AIGroup group, SCR_AIGroupUtilityComponent utility, string details)
	{
		if (!Enabled()) return;
		if (s_iEvents >= MAXIMUM_EVENTS)
		{
			if (!s_bOverflow) { s_bOverflow = true; Print("[AICF][RECRUITMENT_DECISION] event=OVERFLOW limit=256 diagnostic_only=1 completeness=UNPROVEN"); }
			return;
		}
		if (!before) return;
		AICF_RecruitmentDecisionSnapshot after = Read(order, group, utility);
		string afterFields = "post=UNAVAILABLE";
		if (after) afterFields = after.m_sFields;
		bool postStable;
		if (after) postStable = after.m_bStable;
		bool sameOwner = before.SameOwner(after);
		string continuity = string.Format(" same_owner_context=%1 pre_stable=%2 post_stable=%3 callback_scope_changed_or_unknown=%4", sameOwner, before.m_bStable, postStable, !sameOwner || !before.m_bStable || !postStable);
		s_iEvents++;
		// Cached PRE strings survive callbacks; POST is a fresh guarded read.
		Print(string.Format("[AICF][RECRUITMENT_DECISION] event=%1 result=%2 sequence=%3 AFTER_DECISION=1 diagnostic_only=1 cause_proven=0", eventName, result, s_iEvents) + details + continuity + " PRE{" + before.m_sFields + "} POST{" + afterFields + "}");
	}
}

modded class AICF_GroupSlot
{
	AICF_InfantryRecruitmentOrder AICF_RDOrder() { return m_RecruitmentOrder; }
}

modded class SCR_AIProcessFailedMovementResult
{
	override ENodeResult EOnTaskSimulate(AIAgent owner, float dt)
	{
		AICF_InfantryRecruitmentOrder order;
		int moveResult = -1;
		int handler = -1;
		bool related = false;
		vector location = "0 0 0";
		bool hasMove = false;
		bool hasHandler = false;
		bool hasRelated = false;
		bool hasLocation = false;
		if (AICF_RecruitmentDecisionProbe.Enabled())
		{
			AICF_MatchController controller = AICF_MatchController.GetActiveController();
			AICF_GroupSlot slot;
			if (controller) slot = controller.FindManagedInfantrySlot(m_Group);
			if (slot) order = slot.AICF_RDOrder();
			hasMove = GetVariableIn(PORT_MOVE_RESULT, moveResult);
			hasHandler = GetVariableIn(PORT_HANDLER_ID, handler);
			hasRelated = GetVariableIn(PORT_IS_WAYPOINT_RELATED, related);
			hasLocation = GetVariableIn(PORT_MOVE_LOCATION, location);
		}
		AICF_RecruitmentDecisionSnapshot before = AICF_RecruitmentDecisionProbe.Read(order, m_Group, m_GroupUtilityComponent);
		string details = string.Format(" native_result=%1 handler=%2 related=%3 location=%4 has_result=%5 has_handler=%6 has_related=%7 has_location=%8", moveResult, handler, related, location, hasMove, hasHandler, hasRelated, hasLocation);
		ENodeResult nodeResult = super.EOnTaskSimulate(owner, dt);
		details += string.Format(" node_result=%1 internal_handler_result=UNKNOWN result_is_node_fail_only=1", nodeResult);
		AICF_RecruitmentDecisionProbe.Record("NATIVE_NODE_TASK", nodeResult == ENodeResult.FAIL, before, order, m_Group, m_GroupUtilityComponent, details);
		return nodeResult;
	}
}

modded class AICF_RecruitmentMoveFailure
{
	override bool IsCurrent(AICF_InfantryRecruitmentOrder order, int repair)
	{
		AICF_RecruitmentDecisionSnapshot before = AICF_RecruitmentDecisionProbe.Read(order, m_Group, null);
		string details;
		if (before) details = string.Format(" requested_repair=%1 captured_repair=%2 captured_age_ms=%3 captured_token=%4 consumed=%5", repair, m_iRepair, System.GetTickCount() - m_iCapturedAt, m_iToken, m_bConsumed);
		if (before) details += string.Format(" captured_generation=%1 captured_assignment=%2 captured_intent=%3 captured_graph=%4 captured_actor=%5 captured_waypoint=%6 captured_origin=%7", m_iGeneration, m_iAssignment, m_iIntent, m_iGraph, m_ActorId, m_WaypointId, m_vOrigin);
		bool result = super.IsCurrent(order, repair);
		AICF_RecruitmentDecisionProbe.Record("RECEIPT_CURRENT", result, before, order, m_Group, null, details);
		return result;
	}

	override bool TryRecover(AICF_InfantryRecruitmentOrder order)
	{
		AICF_RecruitmentDecisionSnapshot before = AICF_RecruitmentDecisionProbe.Read(order, m_Group, null);
		string details;
		if (before) details = string.Format(" captured_repair=%1 captured_age_ms=%2 consumed=%3", m_iRepair, System.GetTickCount() - m_iCapturedAt, m_bConsumed);
		bool result = super.TryRecover(order);
		AICF_RecruitmentDecisionProbe.Record("SHORT_ATTEMPT", result, before, order, m_Group, null, details);
		return result;
	}

	override protected bool FindDestination(out vector destination)
	{
		AICF_InfantryRecruitmentOrder order = m_Order;
		AICF_RecruitmentDecisionSnapshot before = AICF_RecruitmentDecisionProbe.Read(order, m_Group, null);
		bool result = super.FindDestination(destination);
		string details = string.Format(" output=%1 output_valid_only_if_result=1", destination);
		AICF_RecruitmentDecisionProbe.Record("FIND_DESTINATION", result, before, order, m_Group, null, details);
		return result;
	}

	override protected bool SafeDestination(vector query, out vector destination)
	{
		AICF_InfantryRecruitmentOrder order = m_Order;
		AICF_RecruitmentDecisionSnapshot before = AICF_RecruitmentDecisionProbe.Read(order, m_Group, null);
		vector origin = m_vOrigin;
		vector service = m_vService;
		bool result = super.SafeDestination(query, destination);
		string details = string.Format(" query=%1 output=%2 step_m=%3 height_delta_m=%4 query_projection_xz_m=%5 service_delta_m=%6 output_valid_only_if_result=1", query, destination,
			vector.Distance(origin, destination), Math.AbsFloat(origin[1] - destination[1]), vector.DistanceXZ(query, destination), vector.DistanceXZ(origin, service) - vector.DistanceXZ(destination, service));
		AICF_RecruitmentDecisionProbe.Record("SAFE_DESTINATION", result, before, order, m_Group, null, details);
		return result;
	}
}
