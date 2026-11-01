// Opt-in read-only paired group/actor navmesh observations. No timers/actions.
class AICF_BoardingNavReadContext
{
	AICF_TransportTrip m_Trip;
	SCR_AIGroup m_Group;
	IEntity m_Member;
	AIAgent m_Agent;
	Vehicle m_Vehicle;
	AICF_VehicleLease m_Lease;
	RplComponent m_VehicleRpl;
	bool m_bRequireLease;
	int m_iLeaseGeneration;
	int m_iVehicleGeneration;
	string m_sVehicleComponentRpl;
	AIBaseMovementComponent m_Movement;
	AIPathfindingComponent m_GroupPath;
	AIPathfindingComponent m_ActorPath;
	NavmeshWorldComponent m_GroupNav;
	NavmeshWorldComponent m_ActorNav;
	EntityID m_GroupId;
	EntityID m_MemberId;
	EntityID m_VehicleId;
	string m_sGroupRpl;
	string m_sMemberRpl;
	string m_sVehicleRpl;
	int m_iPosition;
	int m_iGroupGeneration;
	int m_iTripGeneration;
	int m_iAssignment;
	int m_iIntent;
	int m_iPreset;
	vector m_vVehiclePosition;
	vector m_vVehicleAngles;

	bool Current()
	{
		if (!Replication.IsServer() || !m_Trip || !m_Trip.IsVehiclePresetCurrent()) return false;
		if (m_Trip.GetGroupGeneration() != m_iGroupGeneration || m_Trip.GetTripGeneration() != m_iTripGeneration) return false;
		if (m_Trip.GetAssignment().GetGroup() != m_Group || !m_Group || m_Group.GetID() != m_GroupId) return false;
		if (m_Trip.GetAssignment().GetAssignmentRevision() != m_iAssignment || m_Trip.GetAssignment().GetStrategicIntentRevision() != m_iIntent) return false;
		if (m_Trip.GetVehiclePresetRevision() != m_iPreset || m_Trip.VehiclePresetMember(m_iPosition) != m_Member) return false;
		if (!AICF_VehicleBoardingMutationFence.IsAuthoritativeAIEntity(m_Member) || m_Member.GetID() != m_MemberId) return false;
		if (!AICF_GroupRuntime.IsAliveCharacter(m_Member) || m_Trip.GetVehiclePresetSlot().GetRosterMember(m_iPosition) != m_Member) return false;
		if (!AICF_VehicleBoardingMutationFence.IsAuthoritativeReplicatedEntity(m_Group)) return false;
		if (Replication.FindItemId(m_Group).ToString() != m_sGroupRpl || Replication.FindItemId(m_Member).ToString() != m_sMemberRpl) return false;
		if (!m_Agent || m_Agent.GetControlledEntity() != m_Member || m_Agent.GetParentGroup() != m_Group) return false;
		if (m_Agent.GetMovementComponent() != m_Movement || !m_Movement || m_Movement.GetPathfindingComponent() != m_ActorPath) return false;
		if (m_Group.FindComponent(AIPathfindingComponent) != m_GroupPath) return false;
		if (m_GroupPath && m_GroupPath.GetNavmeshComponent() != m_GroupNav) return false;
		if (m_ActorPath && m_ActorPath.GetNavmeshComponent() != m_ActorNav) return false;
		if (m_Vehicle)
		{
			if (!AICF_VehicleBoardingMutationFence.IsAuthoritativeReplicatedEntity(m_Vehicle) || m_Vehicle.GetID() != m_VehicleId) return false;
			if (Replication.FindItemId(m_Vehicle).ToString() != m_sVehicleRpl) return false;
			if (m_Vehicle.GetOrigin() != m_vVehiclePosition || m_Vehicle.GetYawPitchRoll() != m_vVehicleAngles) return false;
		}
		// Candidate samples precede accepted bind; BOARDING must prove the live lease.
		if (m_bRequireLease)
		{
			if (!m_Lease || m_Trip.GetLease() != m_Lease || !m_Lease.HasPhysicalAsset()) return false;
			if (m_Lease.GetVehicle() != m_Vehicle || m_Lease.GetLeaseGeneration() != m_iLeaseGeneration) return false;
			if (m_Lease.GetVehicleGeneration() != m_iVehicleGeneration || !m_VehicleRpl) return false;
			if (m_Vehicle.FindComponent(RplComponent) != m_VehicleRpl || m_VehicleRpl.Id().ToString() != m_sVehicleComponentRpl) return false;
			if (!m_Lease.MatchesEntityIdentity(m_Vehicle, m_VehicleId, m_sVehicleComponentRpl)) return false;
		}
		return true;
	}
}

class AICF_BoardingNavDomainProbe
{
	static bool Enabled()
	{
		string value;
		return System.GetCLIParam("aicfBoardingNavDomainProbe", value) && value == "1";
	}

	static string Sample(AIPathfindingComponent path, NavmeshWorldComponent nav, vector query, string domain, string point)
	{
		bool loaded, valid, requested, found;
		vector projection;
		string projectionText = "NONE";
		float delta = -1;
		float height = -1;
		if (nav)
		{
			loaded = nav.IsTileLoaded(query);
			valid = nav.IsTileValid(query);
			requested = nav.IsTileRequested(query);
		}
		if (path && nav) found = path.GetClosestPositionOnNavmesh(query, "4 4 4", projection);
		if (found)
		{
			projectionText = projection.ToString();
			delta = vector.DistanceXZ(query, projection);
			height = projection[1] - query[1];
		}
		return string.Format(" domain=%1 point=%2 query=%3 tile_loaded=%4 tile_valid=%5 tile_requested=%6 projection_found=%7 projection=%8 delta_xz_m=%9",
			domain, point, query, loaded, valid, requested, found, projectionText, delta) + string.Format(" height_delta_m=%1", height);
	}

	static void Capture(AICF_TransportTrip trip, Vehicle vehicle, vector site, string phase)
	{
		if (!Enabled() || !Replication.IsServer() || !trip || !trip.IsVehiclePresetCurrent()) return;
		SCR_AIGroup group = trip.GetAssignment().GetGroup();
		if (!group || !AICF_VehicleBoardingMutationFence.IsAuthoritativeReplicatedEntity(group)) return;
		array<vector> queries = { site };
		array<string> names = { "SITE" };
		array<vector> grid = { "4 0 0", "-4 0 0", "0 0 4", "0 0 -4", "4 0 4", "4 0 -4", "-4 0 4", "-4 0 -4" };
		for (int gridIndex; gridIndex < grid.Count(); gridIndex++)
		{
			queries.Insert(site + grid[gridIndex]);
			names.Insert("PAD_" + gridIndex.ToString());
		}
		if (vehicle)
		{
			array<BaseCompartmentSlot> seats = {};
			array<string> doorKeys = {};
			AICF_VehiclePreset.Collect(vehicle, seats);
			foreach (BaseCompartmentSlot seat : seats)
			{
				array<int> doors = {};
				seat.GetAvailableDoorIndices(doors);
				foreach (int door : doors)
				{
					string key = string.Format("%1:%2", AICF_VehiclePreset.RelativeOwnerIndex(seat), door);
					if (doorKeys.Contains(key) || doorKeys.Count() >= 16) continue;
					doorKeys.Insert(key);
					BaseCompartmentManagerComponent manager = seat.GetManager();
					if (!manager) continue;
					CompartmentDoorInfo info = manager.GetDoorInfo(door);
					if (!info) continue;
					PointInfo entry = info.GetAIEntryPointInfo();
					if (!entry) entry = info.GetEntryPointInfo();
					if (!entry) continue;
					vector pose[4];
					entry.GetWorldTransform(pose);
					queries.Insert(pose[3]);
					names.Insert("DOOR_" + key);
				}
			}
		}
		for (int position; position < trip.GetVehiclePresetSlot().GetDeploymentSize() && position < 10; position++)
		{
			ChimeraCharacter member = ChimeraCharacter.Cast(trip.VehiclePresetMember(position));
			if (!member || !member.GetAIControlComponent()) continue;
			CompartmentAccessComponent access = member.GetCompartmentAccessComponent();
			if (phase == "BOARDING" && access && access.IsInCompartment() && !access.IsGettingIn() && !access.IsGettingOut()) continue;
			AICF_BoardingNavReadContext c = new AICF_BoardingNavReadContext();
			c.m_Trip = trip; c.m_Group = group; c.m_Member = member; c.m_Vehicle = vehicle;
			c.m_bRequireLease = phase == "BOARDING";
			if (c.m_bRequireLease)
			{
				c.m_Lease = trip.GetLease();
				if (!c.m_Lease || !vehicle) continue;
				c.m_iLeaseGeneration = c.m_Lease.GetLeaseGeneration();
				c.m_iVehicleGeneration = c.m_Lease.GetVehicleGeneration();
				c.m_VehicleRpl = RplComponent.Cast(vehicle.FindComponent(RplComponent));
				if (!c.m_VehicleRpl) continue;
				c.m_sVehicleComponentRpl = c.m_VehicleRpl.Id().ToString();
			}
			c.m_Agent = member.GetAIControlComponent().GetControlAIAgent();
			if (!c.m_Agent) continue;
			c.m_Movement = c.m_Agent.GetMovementComponent();
			if (!c.m_Movement) continue;
			c.m_GroupPath = AIPathfindingComponent.Cast(group.FindComponent(AIPathfindingComponent));
			c.m_ActorPath = c.m_Movement.GetPathfindingComponent();
			if (c.m_GroupPath) c.m_GroupNav = c.m_GroupPath.GetNavmeshComponent();
			if (c.m_ActorPath) c.m_ActorNav = c.m_ActorPath.GetNavmeshComponent();
			c.m_GroupId = group.GetID(); c.m_MemberId = member.GetID(); c.m_iPosition = position;
			c.m_sGroupRpl = Replication.FindItemId(group).ToString(); c.m_sMemberRpl = Replication.FindItemId(member).ToString();
			c.m_iGroupGeneration = trip.GetGroupGeneration(); c.m_iTripGeneration = trip.GetTripGeneration();
			c.m_iAssignment = trip.GetAssignment().GetAssignmentRevision(); c.m_iIntent = trip.GetAssignment().GetStrategicIntentRevision();
			c.m_iPreset = trip.GetVehiclePresetRevision();
			if (vehicle)
			{
				c.m_VehicleId = vehicle.GetID(); c.m_sVehicleRpl = Replication.FindItemId(vehicle).ToString();
				c.m_vVehiclePosition = vehicle.GetOrigin(); c.m_vVehicleAngles = vehicle.GetYawPitchRoll();
			}
			if (!c.Current()) continue;
			int agentLOD = c.m_Agent.GetLOD();
			int groupLOD = c.m_Group.GetLOD();
			int maxLOD = AIAgent.GetMaxLOD();
			array<string> samples = {};
			samples.Insert(Sample(c.m_GroupPath, c.m_GroupNav, member.GetOrigin(), "GROUP", "MEMBER"));
			samples.Insert(Sample(c.m_ActorPath, c.m_ActorNav, member.GetOrigin(), "ACTOR", "MEMBER"));
			for (int queryIndex; queryIndex < queries.Count(); queryIndex++)
			{
				samples.Insert(Sample(c.m_GroupPath, c.m_GroupNav, queries[queryIndex], "GROUP", names[queryIndex]));
				samples.Insert(Sample(c.m_ActorPath, c.m_ActorNav, queries[queryIndex], "ACTOR", names[queryIndex]));
			}
			if (agentLOD != c.m_Agent.GetLOD() || groupLOD != c.m_Group.GetLOD() || maxLOD != AIAgent.GetMaxLOD()) continue;
			if (!c.Current()) continue;
			string context = string.Format("[AICF][BOARDING_NAV_DOMAIN] operation_id=%1 faction=%2 numeric_slot=%3 position=%4 member=%5 phase=%6 t_ms=%7 group_generation=%8 trip_generation=%9",
				trip.GetOperationId(), trip.GetFactionKey(), trip.GetSlotId(), position, member.GetID(), phase, System.GetTickCount(), c.m_iGroupGeneration, c.m_iTripGeneration);
			context += string.Format(" group_path=%1 actor_path=%2 group_nav=%3 actor_nav=%4 same_path=%5 same_nav=%6 vehicle=%7",
				c.m_GroupPath, c.m_ActorPath, c.m_GroupNav, c.m_ActorNav, c.m_GroupPath == c.m_ActorPath, c.m_GroupNav == c.m_ActorNav, AICF_GroupTrace.Entity(vehicle));
			context += string.Format(" movement_component=%1 assignment_revision=%2 intent_revision=%3 preset_revision=%4", c.m_Movement.Type().ToString(), c.m_iAssignment, c.m_iIntent, c.m_iPreset);
			context += string.Format(" agent_lod=%1 group_lod=%2 max_lod=%3", agentLOD, groupLOD, maxLOD);
			context += string.Format(" bound_lease_required=%1 lease_generation=%2 vehicle_generation=%3 vehicle_entity_id=%4 vehicle_component_rpl=%5",
				c.m_bRequireLease, c.m_iLeaseGeneration, c.m_iVehicleGeneration, c.m_VehicleId, c.m_sVehicleComponentRpl);
			context += " readonly=1 cause_proven=0 reachability_proven=0 nav_search_half_extents=<4,4,4>";
			foreach (string sample : samples) Print(context + sample);
		}
	}
}

// Temporary read context only; no retained engine entities or callbacks.
class AICF_StagingPathReadContext : AICF_BoardingNavReadContext
{
	AICF_VehicleRequestState m_Request;
	AICF_VehicleSpawnPlan m_Plan;
	AICF_VehicleStagingActionToken m_Token;
	SCR_AIMoveIndividuallyBehavior m_Action;
	SCR_AIUtilityComponent m_Utility;
	int m_iRequestGeneration;
	int m_iApproachDeadline;
	vector m_vTarget;
	vector m_vActorPosition;

	override bool Current()
	{
		if (!super.Current() || m_Trip.GetPhase() != AICF_ETransportTripPhase.APPROACHING_SITE) return false;
		if (!m_Request || m_Request.GetRequestGeneration() != m_iRequestGeneration || m_Request.GetSpawnPlan() != m_Plan) return false;
		if (!m_Plan || m_Plan.GetStagingPosition() != m_vTarget || m_Plan.GetApproachDeadlineMs() != m_iApproachDeadline) return false;
		if (!m_Plan.GetReservation() || !m_Plan.GetReservation().MatchesTrip(m_Trip, m_iRequestGeneration)) return false;
		if (!m_Token || !m_Token.Matches(m_Trip, m_iRequestGeneration, m_Group) || m_Token.GetAction() != m_Action) return false;
		if (m_Token.GetAgent() != m_Agent || m_Token.GetReservedEntity() != m_Member || !m_Token.IsActionOwnedByUtility()) return false;
		if (!m_Utility || m_Agent.FindComponent(SCR_AIUtilityComponent) != m_Utility || m_Utility.m_OwnerEntity != m_Member) return false;
		if (m_Utility.GetCurrentAction() != m_Action || m_Utility.GetExecutedAction() != m_Action) return false;
		if (!m_Action || m_Action.GetActionState() != EAIActionState.RUNNING || m_Member.GetOrigin() != m_vActorPosition) return false;
		ChimeraCharacter character = ChimeraCharacter.Cast(m_Member);
		if (!character) return false;
		CompartmentAccessComponent access = character.GetCompartmentAccessComponent();
		if (!access || access.IsInCompartment() || access.IsGettingIn() || access.IsGettingOut() || access.IsSwitchingSeatsAnim()) return false;
		return !CompartmentAccessComponent.GetVehicleIn(character) && AICharacterMovementComponent.Cast(m_Movement) != null;
	}
}

class AICF_StagingPathDomainProbe
{
	static void CaptureStagingPath(AICF_TransportTrip trip, AICF_VehicleRequestState request,
		AICF_VehicleSpawnPlan plan, AICF_VehicleStagingActionToken token, string reason)
	{
		if (!AICF_BoardingNavDomainProbe.Enabled() || !Replication.IsServer() || !trip || !request || !plan || !token) return;
		if (reason != "ACTION_ACTIVE" && reason != "NO_PROGRESS_TIMEOUT") return;
		if (!trip.IsVehiclePresetCurrent() || !trip.GetAssignment()) return;
		AICF_StagingPathReadContext c = new AICF_StagingPathReadContext();
		c.m_Trip = trip; c.m_Request = request; c.m_Plan = plan; c.m_Token = token;
		c.m_Group = trip.GetAssignment().GetGroup(); c.m_Member = token.GetReservedEntity(); c.m_Agent = token.GetAgent(); c.m_Action = token.GetAction();
		if (!c.m_Group || !c.m_Member || !c.m_Agent) return;
		c.m_iPosition = -1;
		for (int position; position < trip.GetVehiclePresetSlot().GetDeploymentSize() && position < 10; position++)
			if (trip.VehiclePresetMember(position) == c.m_Member) { c.m_iPosition = position; break; }
		if (c.m_iPosition < 0) return;
		c.m_Movement = c.m_Agent.GetMovementComponent();
		if (!c.m_Movement) return;
		c.m_Utility = SCR_AIUtilityComponent.Cast(c.m_Agent.FindComponent(SCR_AIUtilityComponent));
		c.m_GroupPath = AIPathfindingComponent.Cast(c.m_Group.FindComponent(AIPathfindingComponent));
		c.m_ActorPath = c.m_Movement.GetPathfindingComponent();
		if (c.m_GroupPath) c.m_GroupNav = c.m_GroupPath.GetNavmeshComponent();
		if (c.m_ActorPath) c.m_ActorNav = c.m_ActorPath.GetNavmeshComponent();
		c.m_GroupId = c.m_Group.GetID(); c.m_MemberId = c.m_Member.GetID();
		c.m_sGroupRpl = Replication.FindItemId(c.m_Group).ToString(); c.m_sMemberRpl = Replication.FindItemId(c.m_Member).ToString();
		c.m_iGroupGeneration = trip.GetGroupGeneration(); c.m_iTripGeneration = trip.GetTripGeneration();
		c.m_iAssignment = trip.GetAssignment().GetAssignmentRevision(); c.m_iIntent = trip.GetAssignment().GetStrategicIntentRevision(); c.m_iPreset = trip.GetVehiclePresetRevision();
		c.m_iRequestGeneration = request.GetRequestGeneration(); c.m_iApproachDeadline = plan.GetApproachDeadlineMs();
		c.m_vTarget = plan.GetStagingPosition(); c.m_vActorPosition = c.m_Member.GetOrigin();
		if (!c.Current()) return;
		int agentLOD = c.m_Agent.GetLOD();
		int groupLOD = c.m_Group.GetLOD();
		int maxLOD = AIAgent.GetMaxLOD();
		array<vector> points = {};
		c.m_Movement.GetCurrentPath(points);
		int count = points.Count();
		if (count > 512) return;
		bool completed = c.m_Movement.HasCompletedRequest(false);
		bool completedWithTime = c.m_Movement.HasCompletedRequest(true);
		array<string> samples = {};
		samples.Insert(AICF_BoardingNavDomainProbe.Sample(c.m_GroupPath, c.m_GroupNav, c.m_vActorPosition, "GROUP", "ACTOR"));
		samples.Insert(AICF_BoardingNavDomainProbe.Sample(c.m_ActorPath, c.m_ActorNav, c.m_vActorPosition, "ACTOR", "ACTOR"));
		samples.Insert(AICF_BoardingNavDomainProbe.Sample(c.m_GroupPath, c.m_GroupNav, c.m_vTarget, "GROUP", "TARGET"));
		samples.Insert(AICF_BoardingNavDomainProbe.Sample(c.m_ActorPath, c.m_ActorNav, c.m_vTarget, "ACTOR", "TARGET"));
		string lastText = "NONE";
		float lastTargetDistance = -1;
		if (count > 0)
		{
			vector last = points[count - 1];
			lastText = last.ToString(); lastTargetDistance = vector.DistanceXZ(last, c.m_vTarget);
			samples.Insert(AICF_BoardingNavDomainProbe.Sample(c.m_GroupPath, c.m_GroupNav, last, "GROUP", "LAST"));
			samples.Insert(AICF_BoardingNavDomainProbe.Sample(c.m_ActorPath, c.m_ActorNav, last, "ACTOR", "LAST"));
		}
		array<vector> repeated = {};
		c.m_Movement.GetCurrentPath(repeated);
		if (count != repeated.Count()) return;
		for (int index; index < count; index++)
			if (points[index] != repeated[index]) return;
		if (agentLOD != c.m_Agent.GetLOD() || groupLOD != c.m_Group.GetLOD() || maxLOD != AIAgent.GetMaxLOD()) return;
		if (!c.Current() || c.m_Movement.HasCompletedRequest(false) != completed || c.m_Movement.HasCompletedRequest(true) != completedWithTime) return;
		string context = string.Format("[AICF][STAGING_PATH_DOMAIN] operation_id=%1 position=%2 member=%3 request_generation=%4 phase=APPROACHING_SITE reason=%5 t_ms=%6 actor_pos=%7 target=%8 deadline_ms=%9",
			trip.GetOperationId(), c.m_iPosition, c.m_MemberId, c.m_iRequestGeneration, reason, System.GetTickCount(), c.m_vActorPosition, c.m_vTarget, c.m_iApproachDeadline);
		context += string.Format(" group_path=%1 actor_path=%2 group_nav=%3 actor_nav=%4 same_nav=%5 action=%6 action_token=%7 action_current=1 action_owned=1 conscious=%8",
			c.m_GroupPath, c.m_ActorPath, c.m_GroupNav, c.m_ActorNav, c.m_GroupNav == c.m_ActorNav, c.m_Action, token.GetFence().GetToken(), SCR_AIDamageHandling.IsConscious(c.m_Member));
		context += string.Format(" agent_lod=%1 group_lod=%2 max_lod=%3", agentLOD, groupLOD, maxLOD);
		context += string.Format(" path_status=COMPLETE_REPEATED_NATIVE native_points=%1 emitted_points=%2 point_limit=512 truncated=0 last=%3 last_target_xz_m=%4 completed_without_time=%5 completed_with_time=%6 readonly=1 cause_proven=0 reachability_proven=0",
			count, count, lastText, lastTargetDistance, completed, completedWithTime);
		foreach (string sample : samples) Print(context + sample);
		for (int nativeIndex; nativeIndex < count; nativeIndex++) Print(context + string.Format(" native_index=%1 point=%2", nativeIndex, points[nativeIndex]));
	}
}

modded class AICF_VehicleAcquisitionFlow
{
	override protected bool SpawnCandidate(AICF_TransportTrip trip, SCR_CampaignFaction faction,
		AICF_EVehicleKind kind, array<ResourceName> candidates, AICF_VehicleSpawnSiteSelection selection,
		AICF_VehicleAcquisitionCandidate candidate, string identity)
	{
		if (selection) AICF_BoardingNavDomainProbe.Capture(trip, null, selection.m_vPosition, "PRE_SPAWN");
		bool result = super.SpawnCandidate(trip, faction, kind, candidates, selection, candidate, identity);
		if (result && candidate && candidate.m_Vehicle)
			AICF_BoardingNavDomainProbe.Capture(trip, candidate.m_Vehicle, selection.m_vPosition, "POST_SPAWN");
		return result;
	}

	override protected void ReportStagingMemberAction(AICF_TransportTrip trip, AICF_VehicleRequestState requestState,
		AICF_VehicleSpawnPlan plan, AICF_VehicleStagingActionToken token, string eventName, string reason, int nowMs)
	{
		super.ReportStagingMemberAction(trip, requestState, plan, token, eventName, reason, nowMs);
		AICF_StagingPathDomainProbe.CaptureStagingPath(trip, requestState, plan, token, reason);
	}
}

modded class AICF_VehicleBoardingFlow
{
	protected ref array<string> m_aAICFNavOperations = {};
	protected ref array<int> m_aAICFNavNextMs = {};

	override protected AICF_TripOutcome TickPreset(AICF_TransportTrip trip, AICF_VehicleLease lease,
		SCR_AIGroup group, AICF_VehicleBoardingState state, string causationId)
	{
		if (AICF_BoardingNavDomainProbe.Enabled() && trip && lease && lease.GetVehicle())
		{
			string operation = trip.GetOperationId();
			int index = m_aAICFNavOperations.Find(operation);
			if (index < 0)
			{
				if (m_aAICFNavOperations.Count() >= 32) { m_aAICFNavOperations.Remove(0); m_aAICFNavNextMs.Remove(0); }
				index = m_aAICFNavOperations.Insert(operation);
				m_aAICFNavNextMs.Insert(0);
			}
			int now = System.GetTickCount();
			if (now >= m_aAICFNavNextMs[index])
			{
				m_aAICFNavNextMs[index] = now + 5000;
				AICF_BoardingNavDomainProbe.Capture(trip, lease.GetVehicle(), lease.GetVehicle().GetOrigin(), "BOARDING");
			}
		}
		return super.TickPreset(trip, lease, group, state, causationId);
	}
}
