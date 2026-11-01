// Opt-in synchronous read-only observer. Context exists only during one CheckApproach call.
class AICF_RecruitmentNavReadContext
{
	AICF_InfantryRecruitmentService m_Service;
	AICF_InfantryRecruitmentOrder m_Order;
	AICF_GroupSlot m_Slot;
	SCR_AIGroup m_Group;
	ChimeraCharacter m_Leader;
	AIAgent m_Agent;
	AIBaseMovementComponent m_Movement;
	SCR_AIUtilityComponent m_Utility;
	SCR_AIGroupUtilityComponent m_GroupUtility;
	CharacterControllerComponent m_Controller;
	AIActionBase m_CurrentAction;
	AIActionBase m_ExecutedAction;
	AIActionBase m_GroupAction;
	SCR_AIActivityBase m_Activity;
	AIWaypoint m_ActivityWaypoint;
	AIWaypoint m_Waypoint;
	AIPathfindingComponent m_GroupPath;
	AIPathfindingComponent m_ActorPath;
	NavmeshWorldComponent m_GroupNav;
	NavmeshWorldComponent m_ActorNav;
	EntityID m_GroupId;
	EntityID m_LeaderId;
	EntityID m_WaypointId;
	string m_sGroupRpl;
	string m_sLeaderRpl;
	int m_iGeneration;
	int m_iAssignment;
	int m_iIntent;
	int m_iGraph;
	int m_iToken;
	int m_iRepair;
	int m_iCurrentState;
	int m_iExecutedState;
	int m_iGroupState;
	int m_iActorLOD;
	int m_iGroupLOD;
	int m_iMaxLOD;
	bool m_bFailed;
	vector m_vActor;
	vector m_vEndpoint;
	vector m_vService;
	float m_fRadius;
	float m_fYPrecision;
	EAIWaypointCompletionType m_CompletionType;

	bool Current()
	{
		if (!Replication.IsServer() || !m_Service || !m_Service.AICF_RNDContains(m_Order)) return false;
		if (!m_Order || m_Order.m_RecruitmentService != m_Service || m_Order.m_Slot != m_Slot || !m_Order.IsCurrent(m_Slot)) return false;
		if (!m_Slot || !m_Slot.AICF_RNDMatchesOrder(m_Order) || m_Order.m_bAwaitingSupplyPlan || m_Order.m_iArrivedAtMs > 0 || m_Order.m_Donor) return false;
		if (m_Order.m_Group != m_Group || !m_Group || m_Group.GetID() != m_GroupId || m_Order.m_GroupId != m_GroupId) return false;
		if (m_Order.m_iGeneration != m_iGeneration || m_Slot.GetSpawnGeneration() != m_iGeneration) return false;
		if (m_Order.m_iAssignment != m_iAssignment || m_Order.m_iIntent != m_iIntent || m_Order.m_iGraphRevision != m_iGraph) return false;
		if (m_Order.m_iToken != m_iToken || m_Order.m_iApproachRepairs != m_iRepair || m_Order.m_bMovementFailed != m_bFailed) return false;
		if (m_Order.m_vPosition != m_vService || m_Order.m_Waypoint != m_Waypoint || !m_Waypoint || m_Waypoint.GetID() != m_WaypointId) return false;
		if (m_Slot.GetWaypoint() != m_Waypoint || m_Group.GetCurrentWaypoint() != m_Waypoint) return false;
		array<AIWaypoint> waypoints = {};
		m_Group.GetWaypoints(waypoints);
		if (!waypoints.Contains(m_Waypoint) || m_Waypoint.GetOrigin() != m_vEndpoint) return false;
		if (m_Waypoint.GetCompletionRadius() != m_fRadius || m_Waypoint.GetCompletionYPrecision() != m_fYPrecision || m_Waypoint.GetCompletionType() != m_CompletionType) return false;
		if (!AICF_VehicleBoardingMutationFence.IsAuthoritativeReplicatedEntity(m_Group) || !AICF_VehicleBoardingMutationFence.IsAuthoritativeAIEntity(m_Leader)) return false;
		if (AICF_GroupRuntime.ResolveAliveLeader(m_Group) != m_Leader || !AICF_GroupRuntime.IsAliveCharacter(m_Leader) || m_Leader.GetID() != m_LeaderId || m_Leader.GetOrigin() != m_vActor) return false;
		if (Replication.FindItemId(m_Group).ToString() != m_sGroupRpl || Replication.FindItemId(m_Leader).ToString() != m_sLeaderRpl) return false;
		if (!m_Leader.GetAIControlComponent() || m_Leader.GetAIControlComponent().GetControlAIAgent() != m_Agent || !m_Agent) return false;
		if (m_Agent.GetControlledEntity() != m_Leader || m_Agent.GetParentGroup() != m_Group || m_Agent.GetMovementComponent() != m_Movement || !m_Movement) return false;
		if (m_Agent.GetLOD() != m_iActorLOD || m_Group.GetLOD() != m_iGroupLOD || AIAgent.GetMaxLOD() != m_iMaxLOD) return false;
		if (!AICharacterMovementComponent.Cast(m_Movement) || m_Movement.GetPathfindingComponent() != m_ActorPath) return false;
		if (m_Group.FindComponent(AIPathfindingComponent) != m_GroupPath) return false;
		if (m_GroupPath && m_GroupPath.GetNavmeshComponent() != m_GroupNav) return false;
		if (m_ActorPath && m_ActorPath.GetNavmeshComponent() != m_ActorNav) return false;
		if (!m_Utility || m_Agent.FindComponent(SCR_AIUtilityComponent) != m_Utility || m_Utility.m_OwnerEntity != m_Leader) return false;
		if (m_Leader.GetCharacterController() != m_Controller) return false;
		if (m_Utility.GetCurrentAction() != m_CurrentAction || m_Utility.GetExecutedAction() != m_ExecutedAction) return false;
		if (m_CurrentAction && m_CurrentAction.GetActionState() != m_iCurrentState) return false;
		if (m_ExecutedAction && m_ExecutedAction.GetActionState() != m_iExecutedState) return false;
		if (!m_GroupUtility || m_Group.GetGroupUtilityComponent() != m_GroupUtility || m_GroupUtility.GetCurrentAction() != m_GroupAction) return false;
		if (m_GroupAction && m_GroupAction.GetActionState() != m_iGroupState) return false;
		SCR_AIBehaviorBase behavior = SCR_AIBehaviorBase.Cast(m_CurrentAction);
		SCR_AIActivityBase activity;
		if (behavior) activity = behavior.GetGroupActivityContext();
		if (activity != m_Activity || (activity && activity.m_RelatedWaypoint != m_ActivityWaypoint)) return false;
		CompartmentAccessComponent access = m_Leader.GetCompartmentAccessComponent();
		return access && !m_Leader.IsInVehicle() && !access.IsInCompartment() && !access.IsGettingIn() && !access.IsGettingOut() && !access.IsSwitchingSeatsAnim() && !CompartmentAccessComponent.GetVehicleIn(m_Leader);
	}
}

// Temporary query helper; references are discarded before Capture returns.
class AICF_RecruitmentPhysicalReadProbe
{
	static const int MAXIMUM_QUERIES = 32;
	static const int MAXIMUM_NATIVE_NODES = 3;
	IEntity m_ExcludedLeader;
	int m_iQueries;

	bool AcceptEntity(IEntity entity)
	{
		return entity && entity != m_ExcludedLeader && entity.GetRootParent() != m_ExcludedLeader;
	}

	void Query(BaseWorld world, vector start, vector end, int mask, string maskName,
		string kind, int nativeIndex, array<string> samples)
	{
		if (!world || m_iQueries >= MAXIMUM_QUERIES) return;
		ref TraceParam trace;
		if (kind == "RAY") trace = new TraceParam();
		else
		{
			TraceBox body = new TraceBox();
			body.Mins = "-0.3 0 -0.3";
			body.Maxs = "0.3 1.6 0.3";
			trace = body;
		}
		trace.Start = start; trace.End = end;
		trace.Flags = TraceFlags.ENTS | TraceFlags.WORLD | TraceFlags.OCEAN;
		trace.LayerMask = mask;
		trace.Exclude = m_ExcludedLeader;
		float coefficient;
		bool hit;
		if (kind == "BODY_POSITION")
		{
			coefficient = world.TracePosition(trace, AcceptEntity);
			hit = coefficient < 0;
		}
		else
		{
			coefficient = world.TraceMove(trace, AcceptEntity);
			hit = coefficient >= 0 && coefficient < 1;
		}
		m_iQueries++;
		IEntity blocker = trace.TraceEnt;
		IEntity root;
		string entityId = "NONE";
		string rootId = "NONE";
		string prefab = "NONE";
		string rootPrefab = "NONE";
		string entityType = "WORLD_OR_NONE";
		string rpl = "NONE";
		if (blocker)
		{
			entityId = blocker.GetID().ToString(); entityType = blocker.Type().ToString();
			if (blocker.GetPrefabData()) prefab = blocker.GetPrefabData().GetPrefabName();
			RplComponent replication = RplComponent.Cast(blocker.FindComponent(RplComponent));
			if (replication) rpl = replication.Id().ToString();
			root = blocker.GetRootParent();
			if (!root) root = blocker;
			rootId = root.GetID().ToString();
			if (root.GetPrefabData()) rootPrefab = root.GetPrefabData().GetPrefabName();
		}
		string contact = "NONE";
		string sweepOrigin = "NONE";
		float travelled = -1;
		if (hit && kind != "BODY_POSITION")
		{
			vector atHit = start + (end - start) * coefficient;
			travelled = vector.Distance(start, atHit);
			if (kind == "RAY") contact = atHit.ToString();
			else sweepOrigin = atHit.ToString();
		}
		string sample = string.Format(" physical_query=%1 physical_kind=%2 native_node_index=%3 mask=%4 mask_value=%5 start=%6 end=%7 hit=%8 native_result=%9",
			m_iQueries, kind, nativeIndex, maskName, mask, start, end, hit, coefficient);
		sample += string.Format(" hit_entity=%1 hit_type=%2 hit_prefab=%3 hit_root=%4 hit_root_prefab=%5 hit_rpl=%6 ray_surface_contact=%7 sweep_origin_at_hit=%8 travelled_m=%9",
			entityId, entityType, prefab, rootId, rootPrefab, rpl, contact, sweepOrigin, travelled);
		sample += string.Format(" normal=%1 native_plane_dist=%2 collider_index=%3 collider_node=%4 collider_name=%5 material=%6 excluded_leader=%7 excluded_descendants=1 surrogate_body=1 exact_character_capsule=0",
			trace.TraceNorm, trace.TraceDist, trace.ColliderIndex, trace.NodeIndex, trace.ColliderName, trace.TraceMaterial, m_ExcludedLeader.GetID());
		samples.Insert(sample);
	}

	static string Collect(AICF_RecruitmentNavReadContext c, array<vector> points, array<string> samples, int now)
	{
		if (!c.m_bFailed && now - c.m_Order.m_iApproachProgressAtMs < 3000) return " physical_sampled=0";
		BaseWorld world = c.m_Group.GetWorld();
		if (!world) return " physical_sampled=0 physical_reason=NO_WORLD";
		AICF_RecruitmentPhysicalReadProbe probe = new AICF_RecruitmentPhysicalReadProbe();
		probe.m_ExcludedLeader = c.m_Leader;
		array<int> masks = { EPhysicsLayerPresets.CharacterAI, EPhysicsLayerPresets.Projectile };
		array<string> names = { "CharacterAI", "Projectile" };
		for (int maskIndex; maskIndex < 2; maskIndex++)
			probe.Query(world, c.m_vActor + "0 0.15 0", c.m_vActor + "0 0.15 0", masks[maskIndex], names[maskIndex], "BODY_POSITION", -1, samples);
		int closest = -1;
		float nearest = float.MAX;
		for (int index; index < points.Count(); index++)
		{
			float distance = vector.DistanceSqXZ(c.m_vActor, points[index]);
			if (distance < nearest) { closest = index; nearest = distance; }
		}
		vector previous = c.m_vActor;
		int nodes;
		array<float> heights = { 0.25, 0.9, 1.5 };
		for (int node = closest; node >= 0 && node < points.Count() && nodes < MAXIMUM_NATIVE_NODES; node++)
		{
			vector target = points[node];
			for (int comparison; comparison < 2; comparison++)
			{
				probe.Query(world, target + "0 0.15 0", target + "0 0.15 0", masks[comparison], names[comparison], "BODY_POSITION", node, samples);
				probe.Query(world, previous + "0 0.15 0", target + "0 0.15 0", masks[comparison], names[comparison], "BODY_SWEEP", node, samples);
				foreach (float height : heights)
				{
					vector lift = Vector(0, height, 0);
					probe.Query(world, previous + lift, target + lift, masks[comparison], names[comparison], "RAY", node, samples);
				}
			}
			previous = target; nodes++;
		}
		string motion = " controller=NONE";
		if (c.m_Controller)
			motion = string.Format(" controller=%1 velocity=%2 movement_velocity=%3 movement_input=%4 stance=%5", c.m_Controller, c.m_Controller.GetVelocity(), c.m_Controller.GetMovementVelocity(), c.m_Controller.GetMovementInput(), c.m_Controller.GetStance());
		return string.Format(" physical_sampled=1 physical_queries=%1 physical_query_limit=32 physical_native_nodes=%2 physical_node_limit=3 physical_truncated=0 body_mins=<-0.3,0,-0.3> body_maxs=<0.3,1.6,0.3> body_y_offset_m=0.15", probe.m_iQueries, nodes) + motion;
	}
}

class AICF_RecruitmentNavDomainProbe
{
	static bool Enabled()
	{
		string value;
		return System.GetCLIParam("aicfRecruitmentNavDomainProbe", value) && value == "1";
	}

	static string Sample(AIPathfindingComponent path, NavmeshWorldComponent nav, vector query, string domain, string point)
	{
		bool loaded, valid, requested, found;
		vector projection;
		string projected = "NONE";
		float delta = -1;
		float height = -1;
		if (nav)
		{
			loaded = nav.IsTileLoaded(query);
			valid = nav.IsTileValid(query);
			requested = nav.IsTileRequested(query);
		}
		if (path && nav) found = path.GetClosestPositionOnNavmesh(query, "4 4 4", projection);
		if (found) { projected = projection.ToString(); delta = vector.DistanceXZ(query, projection); height = projection[1] - query[1]; }
		return string.Format(" domain=%1 point=%2 query=%3 tile_loaded=%4 tile_valid=%5 tile_requested=%6 projection_found=%7 projection=%8 delta_xz_m=%9",
			domain, point, query, loaded, valid, requested, found, projected, delta) + string.Format(" height_delta_m=%1", height);
	}

	static void Capture(AICF_InfantryRecruitmentService service, AICF_InfantryRecruitmentOrder order, string phase, string outcome, int now)
	{
		if (!Enabled() || !Replication.IsServer() || !order || !order.IsCurrent(order.m_Slot)) return;
		AICF_RecruitmentNavReadContext c = new AICF_RecruitmentNavReadContext();
		c.m_Service = service; c.m_Order = order; c.m_Slot = order.m_Slot; c.m_Group = order.m_Group; c.m_Waypoint = order.m_Waypoint;
		c.m_Leader = ChimeraCharacter.Cast(AICF_GroupRuntime.ResolveAliveLeader(c.m_Group));
		if (!c.m_Group || !c.m_Waypoint || !c.m_Leader || !c.m_Leader.GetAIControlComponent()) return;
		c.m_Agent = c.m_Leader.GetAIControlComponent().GetControlAIAgent();
		if (!c.m_Agent) return;
		c.m_Movement = c.m_Agent.GetMovementComponent();
		if (!c.m_Movement) return;
		c.m_iActorLOD = c.m_Agent.GetLOD(); c.m_iGroupLOD = c.m_Group.GetLOD(); c.m_iMaxLOD = AIAgent.GetMaxLOD();
		c.m_Utility = SCR_AIUtilityComponent.Cast(c.m_Agent.FindComponent(SCR_AIUtilityComponent));
		c.m_Controller = c.m_Leader.GetCharacterController();
		c.m_GroupUtility = c.m_Group.GetGroupUtilityComponent();
		if (!c.m_Utility || !c.m_GroupUtility) return;
		c.m_CurrentAction = c.m_Utility.GetCurrentAction(); c.m_ExecutedAction = c.m_Utility.GetExecutedAction(); c.m_GroupAction = c.m_GroupUtility.GetCurrentAction();
		if (c.m_CurrentAction) c.m_iCurrentState = c.m_CurrentAction.GetActionState();
		if (c.m_ExecutedAction) c.m_iExecutedState = c.m_ExecutedAction.GetActionState();
		if (c.m_GroupAction) c.m_iGroupState = c.m_GroupAction.GetActionState();
		SCR_AIBehaviorBase behavior = SCR_AIBehaviorBase.Cast(c.m_CurrentAction);
		if (behavior) c.m_Activity = behavior.GetGroupActivityContext();
		if (c.m_Activity) c.m_ActivityWaypoint = c.m_Activity.m_RelatedWaypoint;
		c.m_GroupPath = AIPathfindingComponent.Cast(c.m_Group.FindComponent(AIPathfindingComponent));
		c.m_ActorPath = c.m_Movement.GetPathfindingComponent();
		if (c.m_GroupPath) c.m_GroupNav = c.m_GroupPath.GetNavmeshComponent();
		if (c.m_ActorPath) c.m_ActorNav = c.m_ActorPath.GetNavmeshComponent();
		c.m_GroupId = c.m_Group.GetID(); c.m_LeaderId = c.m_Leader.GetID(); c.m_WaypointId = c.m_Waypoint.GetID();
		c.m_sGroupRpl = Replication.FindItemId(c.m_Group).ToString(); c.m_sLeaderRpl = Replication.FindItemId(c.m_Leader).ToString();
		c.m_iGeneration = order.m_iGeneration; c.m_iAssignment = order.m_iAssignment; c.m_iIntent = order.m_iIntent; c.m_iGraph = order.m_iGraphRevision;
		c.m_iToken = order.m_iToken; c.m_iRepair = order.m_iApproachRepairs; c.m_bFailed = order.m_bMovementFailed;
		c.m_vActor = c.m_Leader.GetOrigin(); c.m_vEndpoint = c.m_Waypoint.GetOrigin(); c.m_vService = order.m_vPosition;
		c.m_fRadius = c.m_Waypoint.GetCompletionRadius(); c.m_fYPrecision = c.m_Waypoint.GetCompletionYPrecision(); c.m_CompletionType = c.m_Waypoint.GetCompletionType();
		if (!c.Current()) return;
		array<vector> points = {};
		c.m_Movement.GetCurrentPath(points);
		int count = points.Count();
		if (count > 512) return;
		bool completed = c.m_Movement.HasCompletedRequest(false);
		bool completedWithTime = c.m_Movement.HasCompletedRequest(true);
		bool within = c.m_Waypoint.IsWithinCompletionRadius(c.m_Group);
		array<string> samples = {};
		samples.Insert(Sample(c.m_GroupPath, c.m_GroupNav, c.m_vActor, "GROUP", "ACTOR"));
		samples.Insert(Sample(c.m_ActorPath, c.m_ActorNav, c.m_vActor, "ACTOR", "ACTOR"));
		samples.Insert(Sample(c.m_GroupPath, c.m_GroupNav, c.m_vEndpoint, "GROUP", "ENDPOINT"));
		samples.Insert(Sample(c.m_ActorPath, c.m_ActorNav, c.m_vEndpoint, "ACTOR", "ENDPOINT"));
		samples.Insert(Sample(c.m_GroupPath, c.m_GroupNav, c.m_vService, "GROUP", "SERVICE"));
		samples.Insert(Sample(c.m_ActorPath, c.m_ActorNav, c.m_vService, "ACTOR", "SERVICE"));
		string lastText = "NONE";
		float lastDistance = -1;
		if (count > 0)
		{
			vector last = points[count - 1]; lastText = last.ToString(); lastDistance = vector.DistanceXZ(last, c.m_vEndpoint);
			samples.Insert(Sample(c.m_GroupPath, c.m_GroupNav, last, "GROUP", "LAST"));
			samples.Insert(Sample(c.m_ActorPath, c.m_ActorNav, last, "ACTOR", "LAST"));
		}
		string physical = AICF_RecruitmentPhysicalReadProbe.Collect(c, points, samples, now);
		array<vector> repeated = {};
		c.m_Movement.GetCurrentPath(repeated);
		if (count != repeated.Count()) return;
		for (int index; index < count; index++) if (points[index] != repeated[index]) return;
		if (!c.Current() || c.m_Movement.HasCompletedRequest(false) != completed || c.m_Movement.HasCompletedRequest(true) != completedWithTime || c.m_Waypoint.IsWithinCompletionRadius(c.m_Group) != within) return;
		string context = string.Format("[AICF][RECRUITMENT_PATH_DOMAIN] faction=%1 slot=%2 generation=%3 token=%4 assignment_revision=%5 intent_revision=%6 graph_revision=%7 repair=%8 t_ms=%9",
			order.m_Faction.GetFactionKey(), c.m_Slot.GetSlotId(), c.m_iGeneration, c.m_iToken, c.m_iAssignment, c.m_iIntent, c.m_iGraph, c.m_iRepair, now);
		context += string.Format(" phase=%1 outcome=%2 group=%3 actor=%4 actor_pos=%5 waypoint=%6 endpoint=%7 service=%8 movement_failed=%9",
			phase, outcome, c.m_GroupId, c.m_LeaderId, c.m_vActor, c.m_WaypointId, c.m_vEndpoint, c.m_vService, c.m_bFailed);
		context += string.Format(" current_action=%1 executed_action=%2 same_action=%3 group_action=%4 activity=%5 activity_waypoint=%6 activity_matches_waypoint=%7 conscious=%8",
			AICF_GroupTrace.Action(c.m_CurrentAction), AICF_GroupTrace.Action(c.m_ExecutedAction), c.m_CurrentAction == c.m_ExecutedAction, AICF_GroupTrace.Action(c.m_GroupAction), c.m_Activity, c.m_ActivityWaypoint, c.m_ActivityWaypoint == c.m_Waypoint, SCR_AIDamageHandling.IsConscious(c.m_Leader));
		context += string.Format(" current_action_ref=%1 executed_action_ref=%2 group_action_ref=%3 activity_is_group_current=%4 actor_lod=%5 group_lod=%6 max_lod=%7",
			c.m_CurrentAction, c.m_ExecutedAction, c.m_GroupAction, c.m_Activity == c.m_GroupAction, c.m_iActorLOD, c.m_iGroupLOD, c.m_iMaxLOD);
		context += string.Format(" group_path=%1 actor_path=%2 group_nav=%3 actor_nav=%4 same_nav=%5 movement_component=%6 agent=%7 utility=%8",
			c.m_GroupPath, c.m_ActorPath, c.m_GroupNav, c.m_ActorNav, c.m_GroupNav == c.m_ActorNav, c.m_Movement, c.m_Agent, c.m_Utility);
		context += string.Format(" native_points=%1 emitted_points=%2 last=%3 last_endpoint_xz_m=%4 completed_without_time=%5 completed_with_time=%6 waypoint_within_radius=%7 completion_radius_m=%8 completion_type=%9",
			count, count, lastText, lastDistance, completed, completedWithTime, within, c.m_fRadius, c.m_CompletionType);
		context += string.Format(" completion_y_precision_m=%1 distance_service_m=%2 distance_endpoint_m=%3 progress_age_ms=%4", c.m_fYPrecision, vector.DistanceXZ(c.m_vActor, c.m_vService), vector.DistanceXZ(c.m_vActor, c.m_vEndpoint), now - order.m_iApproachProgressAtMs);
		context += " path_status=COMPLETE_REPEATED_NATIVE point_limit=512 truncated=0 exact_current_order=1 readonly=1 cause_proven=0 reachability_proven=0";
		context += physical;
		foreach (string sample : samples) Print(context + sample);
		for (int nativeIndex; nativeIndex < count; nativeIndex++) Print(context + string.Format(" native_index=%1 point=%2", nativeIndex, points[nativeIndex]));
	}
}

modded class AICF_GroupSlot
{
	bool AICF_RNDMatchesOrder(AICF_InfantryRecruitmentOrder order) { return m_RecruitmentOrder == order; }
}

modded class AICF_InfantryRecruitmentService
{
	bool AICF_RNDContains(AICF_InfantryRecruitmentOrder order)
	{
		return !m_bStopped && order && m_aOrders.Contains(order) && m_Graph && m_Graph.GetRevision() == order.m_iGraphRevision && m_Campaign && m_Campaign.IsMaster() && m_Campaign.IsRunning();
	}

	override protected string CheckApproach(AICF_InfantryRecruitmentOrder order, vector position, int now)
	{
		AICF_RecruitmentNavDomainProbe.Capture(this, order, "PRE_CHECK", "PENDING", now);
		AIWaypoint priorWaypoint;
		int priorRepair;
		if (order) { priorWaypoint = order.m_Waypoint; priorRepair = order.m_iApproachRepairs; }
		string result = super.CheckApproach(order, position, now);
		if (order && (order.m_Waypoint != priorWaypoint || order.m_iApproachRepairs != priorRepair || !result.IsEmpty()))
			AICF_RecruitmentNavDomainProbe.Capture(this, order, "POST_CHECK", result, now);
		return result;
	}
}
