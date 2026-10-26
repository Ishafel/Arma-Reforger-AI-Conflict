// FAULT_INJECTED behavioral gate. Только isolated stage, не NORMAL WAR.
// Меняется только оценка exact штатной activity; её actions/selection нативные.
class AICF_TCRGate
{
	static ref AICF_TCRGate s_Owned;
	ref AICF_TransportTrip m_Trip;
	AICF_GroupSlot m_Slot;
	SCR_AIGroup m_Group;
	EntityID m_GroupId;
	string m_sGroupRpl;
	int m_iGroupGeneration;
	ref AICF_VehicleLease m_Lease;
	Vehicle m_Vehicle;
	EntityID m_VehicleId;
	string m_sVehicleRpl;
	int m_iLeaseGeneration;
	int m_iVehicleGeneration;
	string m_sOperation;
	int m_iTripGeneration;
	int m_iDeadline;
	int m_iAssignment;
	int m_iIntent;
	int m_iPresetRevision;
	vector m_vTarget;
	SCR_AIGroupUtilityComponent m_Utility;
	ref SCR_AIVehicleCombatActivity m_Activity;
	ref array<IEntity> m_aActors = {};
	ref array<EntityID> m_aActorIds = {};
	ref array<string> m_aActorRpls = {};

	static void Clear() { s_Owned = null; }
	static string ComponentRpl(IEntity entity)
	{
		if (!entity) return "NONE";
		RplComponent rpl = RplComponent.Cast(entity.FindComponent(RplComponent));
		if (!rpl) return "NONE";
		return rpl.Id().ToString();
	}

	static bool Scores(SCR_AIVehicleCombatActivity activity, SCR_AIGroupUtilityComponent utility)
	{
		string enabled;
		return Replication.IsServer() && System.GetCLIParam("aicfTransportCombatResume", enabled) && enabled == "1" &&
			s_Owned && s_Owned.m_Activity == activity && s_Owned.m_Utility == utility && s_Owned.Matches();
	}

	bool Matches()
	{
		string reason = RejectionReason();
		return reason.IsEmpty();
	}
	string RejectionReason()
	{
		if (!m_Trip || !m_Trip.IsValid() || m_Trip.IsTerminal()) return "TRIP_INVALID_OR_TERMINAL";
		if (m_Trip.GetPhase() != AICF_ETransportTripPhase.TRANSIT) return "TRIP_PHASE";
		if (m_Trip.GetOperationId() != m_sOperation) return "OPERATION_ID";
		if (m_Trip.GetTripGeneration() != m_iTripGeneration) return "TRIP_GENERATION";
		if (m_Trip.GetGroupGeneration() != m_iGroupGeneration) return "GROUP_GENERATION";
		if (m_Trip.GetAbsoluteDeadlineMs() != m_iDeadline) return "ABSOLUTE_DEADLINE_CHANGED";
		if (m_Trip.IsDeadlineReached(System.GetTickCount())) return "ABSOLUTE_DEADLINE_EXPIRED";
		if (!m_Trip.IsVehiclePresetCurrent()) return "PRESET_NOT_CURRENT";
		if (!m_Slot || m_Slot.GetGroup() != m_Group) return "SLOT_GROUP";
		if (m_Slot.GetSpawnGeneration() != m_iGroupGeneration) return "SLOT_GENERATION";
		if (m_Slot.GetStrategicIntentRevision() != m_iIntent) return "INTENT_REVISION";
		if (m_Slot.GetVehiclePresetRevision() != m_iPresetRevision) return "PRESET_REVISION";
		if (m_Trip.GetAssignment().GetAssignmentRevision() != m_iAssignment) return "ASSIGNMENT_REVISION";
		if (m_Trip.GetAssignment().GetTargetPosition() != m_vTarget) return "ASSIGNMENT_TARGET";
		if (m_Trip.GetAssignment().GetGroup() != m_Group || m_Slot.GetTargetPosition() != m_vTarget) return "SLOT_TARGET_OR_GROUP";
		if (!AICF_VehicleBoardingMutationFence.IsAuthoritativeReplicatedEntity(m_Group)) return "GROUP_AUTHORITY";
		if (m_Group.GetID() != m_GroupId) return "GROUP_ENTITY_ID";
		if (ComponentRpl(m_Group) != m_sGroupRpl) return "GROUP_COMPONENT_RPL";
		if (m_Group.GetGroupUtilityComponent() != m_Utility || !m_Utility || m_Utility.m_Owner != m_Group || !m_Utility.m_VehicleMgr) return "UTILITY_OWNER";
		if (!m_Activity || m_Activity.m_Utility != m_Utility) return "ACTIVITY_OWNER";
		if (m_Activity.GetRemoveAction() || m_Activity.GetActionIsSuspended()) return "ACTIVITY_REMOVED_OR_SUSPENDED";
		EAIActionState state = m_Activity.GetActionState();
		if (state == EAIActionState.FAILED || state == EAIActionState.COMPLETED) return "ACTIVITY_TERMINAL";
		array<ref AIActionBase> actions = {};
		m_Utility.GetActions(actions);
		if (!actions.Contains(m_Activity)) return "ACTIVITY_ORPHAN";
		if (m_Trip.GetLease() != m_Lease || !m_Lease || !m_Lease.HasPhysicalAsset()) return "LEASE_NOT_CURRENT";
		if (m_Lease.GetLeaseGeneration() != m_iLeaseGeneration) return "LEASE_GENERATION";
		if (m_Lease.GetVehicleGeneration() != m_iVehicleGeneration) return "VEHICLE_GENERATION";
		if (!m_Lease.MatchesTripIdentity(m_Trip.GetFactionKey(), m_Trip.GetSlotId(), m_iGroupGeneration, m_iTripGeneration)) return "LEASE_TRIP_IDENTITY";
		if (!AICF_VehicleBoardingMutationFence.IsAuthoritativeReplicatedEntity(m_Vehicle)) return "VEHICLE_AUTHORITY";
		if (m_Vehicle.GetID() != m_VehicleId) return "VEHICLE_ENTITY_ID";
		if (ComponentRpl(m_Vehicle) != m_sVehicleRpl) return "VEHICLE_COMPONENT_RPL_CHANGED";
		if (!m_Lease.MatchesEntityIdentity(m_Vehicle, m_VehicleId, m_sVehicleRpl)) return "LEASE_ENTITY_COMPONENT_RPL";
		SCR_AIGroupVehicle registered = m_Utility.m_VehicleMgr.FindVehicle(m_Vehicle);
		SCR_AIVehicleUsageComponent usage = SCR_AIVehicleUsageComponent.Cast(m_Vehicle.FindComponent(SCR_AIVehicleUsageComponent));
		if (!registered || registered.GetEntity() != m_Vehicle || registered.GetVehicleUsageComponent() != usage ||
			registered.IsStatic()) return "REGISTERED_VEHICLE";
		if (!SCR_AIVehicleUsability.VehicleCanMove(m_Vehicle) || SCR_AIVehicleUsability.VehicleIsOnFire(m_Vehicle)) return "VEHICLE_DAMAGE";
		if (m_aActors.Count() != 5 || AICF_GroupRuntime.CountAliveAgents(m_Group) != 5) return "ROSTER_COUNT";
		array<AIAgent> agents = {};
		m_Group.GetAgents(agents);
		for (int index; index < 5; index++)
		{
			IEntity actor = m_aActors[index];
			if (!AICF_VehicleBoardingMutationFence.IsAuthoritativeAIEntity(actor) || actor.GetID() != m_aActorIds[index] ||
				ComponentRpl(actor) != m_aActorRpls[index] || m_Slot.GetRosterMember(index) != actor) return "ACTOR_IDENTITY_" + index.ToString();
			bool belongs;
			foreach (AIAgent agent : agents)
				if (agent && agent.GetParentGroup() == m_Group && agent.GetControlledEntity() == actor) belongs = true;
			if (!belongs) return "ACTOR_PARENT_GROUP_" + index.ToString();
		}
		return string.Empty;
	}
	void LogReject(string context, int observedAt)
	{
		string itemRpl = "NONE";
		string leaseRpl = "NONE";
		int deadline;
		if (m_Vehicle) itemRpl = Replication.FindItemId(m_Vehicle).ToString();
		if (m_Lease) leaseRpl = m_Lease.GetRplId();
		if (m_Trip) deadline = m_Trip.GetAbsoluteDeadlineMs();
		string details = string.Format("[AICF][TCR_REJECT] t_ms=%1 step=-1 context=%2 reason=%3 operation_id=%4 expected_component_rpl=%5 current_component_rpl=%6 vehicle_item_rpl=%7 lease_component_rpl=%8",
			observedAt, context, RejectionReason(), m_sOperation, m_sVehicleRpl, ComponentRpl(m_Vehicle), itemRpl, leaseRpl);
		details += string.Format(" expected_deadline_ms=%1 current_deadline_ms=%2 expected_assignment=%3 expected_intent=%4 group=%5 vehicle=%6",
			m_iDeadline, deadline, m_iAssignment, m_iIntent, m_GroupId, m_VehicleId);
		Print(details);
	}
}

modded class SCR_AIVehicleCombatActivity
{
	override float CustomEvaluate()
	{
		if (AICF_TCRGate.Scores(this, m_Utility)) return GetPriority();
		return super.CustomEvaluate();
	}
}

modded class AICF_MatchController
{
	protected ref AICF_FactionState m_TCRState;
	protected ref AICF_VehicleCoordinator m_TCRVehicles;
	protected SCR_AIGroup m_TCRGroup;
	protected ref AICF_VehiclePreset m_TCRRecipe;
	protected ref AICF_TransportTrip m_TCRTrip;
	protected ref AICF_TCRGate m_TCRWitness;
	protected Vehicle m_TCRVehicle;
	protected EntityID m_TCRVehicleId;
	protected ref array<EntityID> m_aTCRActors = {};
	protected vector m_vTCRHome;
	protected vector m_vTCRStart;
	protected vector m_vTCRGoal;
	protected vector m_vTCRStopped;
	protected vector m_vTCRRelease;
	protected int m_iTCRStarted;
	protected int m_iTCRObservedAt;
	protected int m_iTCRStep;
	protected int m_iTCRAt;
	protected int m_iTCRHoldAt;
	protected int m_iTCRStationaryAt;
	protected int m_iTCRReleaseAt;
	protected int m_iTCRMotionClock;
	protected int m_iTCRProgressClock;
	protected int m_iTCRSample;
	protected int m_iTCRMoving;
	protected int m_iTCRChecks;
	protected int m_iTCRFailures;
	protected int m_iTCRIntent;
	protected bool m_bTCRDone;
	protected bool m_bTCRResumed;
	protected bool m_bTCRWasRunning;

	protected void AICF_TCRLog(string eventName, string details)
	{
		// Один настоящий timestamp принятия решения для всех receipts этого tick.
		Print(string.Format("[AICF][TCR_%1] t_ms=%2 step=%3 %4", eventName, m_iTCRObservedAt, m_iTCRStep, details));
	}
	protected bool AICF_TCRCheck(string rule, bool passed)
	{
		m_iTCRChecks++;
		if (!passed) m_iTCRFailures++;
		AICF_TCRLog("CHECK", string.Format("rule=%1 passed=%2", rule, passed));
		return passed;
	}
	protected void AICF_TCRFail(string rule)
	{
		AICF_TCRCheck(rule, false);
		AICF_TCRFinish();
	}
	protected bool AICF_TCRRoster(AICF_GroupSlot slot)
	{
		if (!slot || slot.GetGroup() != m_TCRGroup || AICF_GroupRuntime.CountAliveAgents(m_TCRGroup) != 5 || m_aTCRActors.Count() != 5) return false;
		for (int position; position < 5; position++)
		{
			IEntity actor = slot.GetRosterMember(position);
			if (!AICF_VehicleBoardingMutationFence.IsAuthoritativeAIEntity(actor) || actor.GetID() != m_aTCRActors[position]) return false;
		}
		return true;
	}
	protected bool AICF_TCRSeats(AICF_GroupSlot slot)
	{
		if (!AICF_TCRRoster(slot) || !m_TCRVehicle || m_TCRVehicle.GetID() != m_TCRVehicleId) return false;
		array<BaseCompartmentSlot> seats = {};
		AICF_VehiclePreset.Collect(m_TCRVehicle, seats);
		if (!m_TCRRecipe || !m_TCRRecipe.MatchesSeats(seats)) return false;
		int exact;
		for (int seat; seat < seats.Count(); seat++)
		{
			int position = m_TCRRecipe.m_aMembers[seat];
			if (position < 0) continue;
			ChimeraCharacter actor = ChimeraCharacter.Cast(slot.GetRosterMember(position));
			CompartmentAccessComponent access = actor.GetCompartmentAccessComponent();
			if (!access || !actor.IsInVehicle() || !access.IsInCompartment() || access.IsGettingIn() || access.IsGettingOut() ||
				access.IsSwitchingSeatsAnim() || access.GetCompartment() != seats[seat] || seats[seat].GetOccupant() != actor ||
				CompartmentAccessComponent.GetVehicleIn(actor) != m_TCRVehicle) return false;
			exact++;
		}
		return exact == 5;
	}
	protected bool AICF_TCRTarget(out vector target)
	{
		AIPathfindingComponent path = AIPathfindingComponent.Cast(m_TCRGroup.FindComponent(AIPathfindingComponent));
		SCR_AIWorld world = SCR_AIWorld.Cast(GetGame().GetAIWorld());
		if (!path || !path.GetNavmeshComponent() || !world || !world.GetRoadNetworkManager()) return false;
		vector origin = m_TCRVehicle.GetOrigin();
		array<BaseRoad> roads = {};
		world.GetRoadNetworkManager().GetRoadsInAABB(origin - "1200 1200 1200", origin + "1200 1200 1200", roads);
		float best = 1100;
		bool found;
		foreach (BaseRoad road : roads)
		{
			if (!road) continue;
			array<vector> points = {};
			road.GetPoints(points);
			foreach (vector query : points)
			{
				float distance = vector.DistanceXZ(origin, query);
				if (distance < 650 || distance >= best) continue;
				bool nearBase;
				for (int index; index < m_ObjectiveGraph.GetNodeCount(); index++)
				{
					SCR_CampaignMilitaryBaseComponent base = m_ObjectiveGraph.GetNode(index).GetBase();
					if (base && base.GetOwner() && base != m_USSRFaction.GetMainBase() && vector.DistanceXZ(query, base.GetOwner().GetOrigin()) < 350) { nearBase = true; break; }
				}
				if (nearBase) continue;
				path.GetNavmeshComponent().LoadTileIn(query);
				vector nav;
				if (!path.GetClosestPositionOnNavmesh(query, "20 10 20", nav) || vector.DistanceXZ(origin, nav) < 650 ||
					ChimeraWorldUtils.TryGetWaterSurfaceSimple(GetGame().GetWorld(), nav - "0 0.5 0")) continue;
				target = nav;
				best = distance;
				found = true;
			}
		}
		return found;
	}
	protected void AICF_TCRSetup()
	{
		AICF_TCRGate.Clear();
		SCR_FactionManager manager = SCR_FactionManager.Cast(GetGame().GetFactionManager());
		array<Faction> factions = {};
		manager.GetFactionsList(factions);
		bool truce = true;
		for (int first; first < factions.Count(); first++)
			for (int second = first + 1; second < factions.Count(); second++)
			{
				SCR_Faction a = SCR_Faction.Cast(factions[first]);
				SCR_Faction b = SCR_Faction.Cast(factions[second]);
				if (!a || !b) continue;
				manager.SetFactionsFriendly(a, b);
				if (!a.IsFactionFriendly(b) || !b.IsFactionFriendly(a)) truce = false;
			}
		if (!AICF_TCRCheck("TRUCE", truce)) { AICF_TCRFinish(); return; }
		m_TCRState = new AICF_FactionState(m_USSRFaction.GetFactionKey(), m_Config);
		AICF_GroupSlot slot = m_TCRState.GetSlot(0);
		slot.SetDesiredSize(5);
		slot.SetUnitType(AICF_EGroupUnitType.MOTORIZED_LIGHT);
		m_TCRGroup = m_GroupSpawner.SpawnGroup(m_USSRFaction, m_USSRFaction.GetMainBase(), 0, 5, false);
		if (!AICF_TCRCheck("GROUP_CREATED", m_TCRGroup != null)) { AICF_TCRFinish(); return; }
		m_TCRGroup.SetLifecyclePolicy(SCR_EAIGroupLifecyclePolicy.Manual);
		m_TCRGroup.SetDeleteWhenEmpty(false);
		slot.BeginInitialSpawn();
		slot.BindSpawnedGroup(m_TCRGroup);
		m_GroupSpawner.BeginRosterSpawn(m_TCRGroup, 5);
		m_TCRVehicles = new AICF_VehicleCoordinator(m_Stage3Config, m_Campaign, m_ConflictAdapter, m_OrderPlanner,
			m_GroupCohesionPolicy, m_ObjectiveGraph, m_TargetSelector, m_ContentProfile);
		m_vTCRHome = m_USSRFaction.GetMainBase().GetSpawnPoint().GetOrigin();
	}
	protected bool AICF_TCRReady(AICF_GroupSlot slot)
	{
		if (m_TCRGroup.GetAgentsCount() != 5) return false;
		SCR_AIGroupUtilityComponent utility = m_TCRGroup.GetGroupUtilityComponent();
		if (!utility) { AICF_TCRFail("UTILITY"); return false; }
		utility.SetCombatMode(EAIGroupCombatMode.HOLD_FIRE);
		slot.MarkReady();
		slot.RecordDeploymentMembers();
		for (int index; index < 5; index++)
		{
			ChimeraCharacter actor = ChimeraCharacter.Cast(slot.GetRosterMember(index));
			if (!actor || !actor.GetDamageManager()) { AICF_TCRFail("ACTOR_DAMAGE_MANAGER"); return false; }
			actor.GetDamageManager().EnableDamageHandling(false);
			if (actor.GetDamageManager().IsDamageHandlingEnabled()) { AICF_TCRFail("ACTOR_PROTECTION"); return false; }
			m_aTCRActors.Insert(actor.GetID());
			AICF_TCRLog("MEMBER", string.Format("position=%1 actor=%2 rpl=%3", index, actor.GetID(), AICF_TCRGate.ComponentRpl(actor)));
		}
		AICF_TCRLog("ENV", "mode=FAULT_INJECTED truce=1 hold_fire=1 damage_protection=1 normal_war=0 teleport=0 setup=ISOLATED_ROSTER");
		AICF_VehiclePresetCatalog catalog = new AICF_VehiclePresetCatalog(m_USSRFaction);
		string reason;
		ResourceName prefab = "{259EE7B78C51B624}Prefabs/Vehicles/Wheeled/UAZ469/UAZ469.et";
		if (!catalog.Inspect(prefab, reason)) { catalog.Clear(); AICF_TCRFail("CATALOG"); return false; }
		m_TCRRecipe = catalog.Draft(prefab);
		int member;
		for (int mandatory; mandatory < catalog.Seats().Count(); mandatory++)
			if (catalog.Seats()[mandatory].GetType() == ECompartmentType.PILOT || catalog.Seats()[mandatory].GetType() == ECompartmentType.TURRET) m_TCRRecipe.Assign(mandatory, member++);
		for (int seat; seat < catalog.Seats().Count() && member < 5; seat++)
			if (m_TCRRecipe.m_aMembers[seat] < 0) m_TCRRecipe.Assign(seat, member++);
		bool valid = m_TCRRecipe.ValidateRoster(slot, catalog.Seats(), reason) && slot.SetVehiclePreset(m_TCRRecipe, slot.GetVehiclePresetRevision());
		catalog.Clear();
		if (!AICF_TCRCheck("RECIPE", valid)) { AICF_TCRFinish(); return false; }
		if (!AICF_TCRCheck("INITIAL_ORDER", m_OrderPlanner.AssignPlayerPointOrder(slot, m_USSRFaction, m_vTCRHome, false))) { AICF_TCRFinish(); return false; }
		return true;
	}
	protected bool AICF_TCRArm(AICF_GroupSlot slot, AICF_TransportTrip trip)
	{
		AICF_VehicleMovementState movement = trip.GetMovementState();
		if (!movement.IsRouteWaypointBound() || movement.GetCrewRecoveryToken() || movement.GetNativeGate() || !AICF_TCRSeats(slot)) return false;
		SCR_AIGroupUtilityComponent utility = m_TCRGroup.GetGroupUtilityComponent();
		SCR_AIVehicleCombatActivity activity = SCR_AIVehicleCombatActivity.Cast(utility.FindActionOfType(SCR_AIVehicleCombatActivity));
		if (!activity) { AICF_TCRFail("NATIVE_ACTIVITY_MISSING"); return false; }
		AICF_TCRGate gate = new AICF_TCRGate();
		gate.m_Trip = trip;
		gate.m_Slot = slot;
		gate.m_Group = m_TCRGroup;
		gate.m_GroupId = m_TCRGroup.GetID();
		gate.m_sGroupRpl = AICF_TCRGate.ComponentRpl(m_TCRGroup);
		gate.m_iGroupGeneration = trip.GetGroupGeneration();
		gate.m_Lease = trip.GetLease();
		gate.m_Vehicle = m_TCRVehicle;
		gate.m_VehicleId = m_TCRVehicleId;
		gate.m_sVehicleRpl = AICF_TCRGate.ComponentRpl(m_TCRVehicle);
		gate.m_iLeaseGeneration = trip.GetLease().GetLeaseGeneration();
		gate.m_iVehicleGeneration = trip.GetLease().GetVehicleGeneration();
		gate.m_sOperation = trip.GetOperationId();
		gate.m_iTripGeneration = trip.GetTripGeneration();
		gate.m_iDeadline = trip.GetAbsoluteDeadlineMs();
		gate.m_iAssignment = trip.GetAssignment().GetAssignmentRevision();
		gate.m_iIntent = slot.GetStrategicIntentRevision();
		gate.m_iPresetRevision = slot.GetVehiclePresetRevision();
		gate.m_vTarget = trip.GetAssignment().GetTargetPosition();
		gate.m_Utility = utility;
		gate.m_Activity = activity;
		for (int index; index < 5; index++)
		{
			IEntity actor = slot.GetRosterMember(index);
			gate.m_aActors.Insert(actor);
			gate.m_aActorIds.Insert(actor.GetID());
			gate.m_aActorRpls.Insert(AICF_TCRGate.ComponentRpl(actor));
		}
		if (!gate.Matches()) { gate.LogReject("ARM", System.GetTickCount()); AICF_TCRFail("ARM_IDENTITY"); return false; }
		m_TCRWitness = gate;
		AICF_TCRGate.s_Owned = gate;
		m_iTCRAt = System.GetTickCount();
		string details = string.Format("operation_id=%1 group=%2 vehicle=%3 lease_generation=%4 vehicle_generation=%5 deadline_ms=%6 assignment_revision=%7 intent_revision=%8 target=%9",
			gate.m_sOperation, gate.m_GroupId, gate.m_VehicleId, gate.m_iLeaseGeneration, gate.m_iVehicleGeneration, gate.m_iDeadline, gate.m_iAssignment, gate.m_iIntent, gate.m_vTarget);
		details += string.Format(" group_rpl=%1 vehicle_rpl=%2 group_generation=%3 trip_generation=%4 warm_displacement_m=%5 warm_moving_polls=%6",
			gate.m_sGroupRpl, gate.m_sVehicleRpl, gate.m_iGroupGeneration, gate.m_iTripGeneration, vector.DistanceXZ(m_vTCRStart, m_TCRVehicle.GetOrigin()), m_iTCRMoving);
		AICF_TCRLog("ARM", details);
		return true;
	}
	override protected void Update()
	{
		super.Update();
		string enabled;
		if (!Replication.IsServer() || m_bTCRDone || !System.GetCLIParam("aicfTransportCombatResume", enabled) || enabled != "1") return;
		int now = System.GetTickCount();
		m_iTCRObservedAt = now;
		if (!m_iTCRStarted) m_iTCRStarted = now;
		if (m_bStopped || !m_Campaign) { AICF_TCRFail("MATCH_STOPPED"); return; }
		if (m_aTCRActors.Count() != 5 && now - m_iTCRStarted >= 180000) { AICF_TCRFail("SETUP_TIMEOUT"); return; }
		if (now - m_iTCRStarted > 600000) { AICF_TCRFail("TIMEOUT"); return; }
		if (!m_Campaign.IsRunning())
		{
			if (m_bTCRWasRunning || m_bCampaignWasRunning || m_Campaign.GetState() != SCR_EGameModeState.PREGAME) AICF_TCRFail("MATCH_STOPPED");
			return;
		}
		m_bTCRWasRunning = true;
		if (!m_bRosterReady) return;
		if (!m_TCRState) { AICF_TCRSetup(); return; }
		AICF_GroupSlot slot = m_TCRState.GetSlot(0);
		if (!slot.IsCombatReady() && !AICF_TCRReady(slot)) return;
		if (m_bTCRDone) return;
		m_TCRVehicles.Update(null, null, m_TCRState, m_USSRFaction, m_iStrategicBaseRevision, true);
		AICF_TransportTrip active = m_TCRVehicles.GetTrip(slot);
		if (!m_TCRTrip && active) m_TCRTrip = active;
		if (!m_TCRTrip) return;
		AICF_TransportTrip trip = m_TCRTrip;
		if (active && active != trip) { AICF_TCRFail("TRIP_REPLACED"); return; }
		if (!AICF_TCRRoster(slot) || (trip.IsTerminal() && trip.GetPhase() != AICF_ETransportTripPhase.COMPLETE)) { AICF_TCRFail("ROSTER_OR_TERMINAL"); return; }
		if (trip.GetLease() && trip.GetLease().GetVehicle())
		{
			DamageManagerComponent damage = DamageManagerComponent.Cast(trip.GetLease().GetVehicle().FindComponent(DamageManagerComponent));
			if (!damage) { AICF_TCRFail("VEHICLE_DAMAGE_MANAGER"); return; }
			damage.EnableDamageHandling(false);
			if (damage.IsDamageHandlingEnabled()) { AICF_TCRFail("VEHICLE_PROTECTION"); return; }
		}
		if (m_iTCRStep == 0)
		{
			if (trip.GetPhase() != AICF_ETransportTripPhase.TRANSIT) return;
			m_TCRVehicle = trip.GetLease().GetVehicle();
			m_TCRVehicleId = m_TCRVehicle.GetID();
			m_vTCRStart = m_TCRVehicle.GetOrigin();
			if (!AICF_TCRCheck("EXACT_BOARDING", AICF_TCRSeats(slot))) { AICF_TCRFinish(); return; }
			m_iTCRAt = now;
			m_iTCRStep = 1;
		}
		if (m_iTCRStep == 1)
		{
			if (!AICF_TCRSeats(slot)) { AICF_TCRFail("PARKED_CREW"); return; }
			if (now - m_iTCRAt < 10000) return;
			vector target;
			if (!AICF_TCRTarget(target)) { if (now - m_iTCRAt > 40000) AICF_TCRFail("TARGET_PRECONDITION"); return; }
			if (!m_OrderPlanner.AssignPlayerPointOrder(slot, m_USSRFaction, target, m_TCRVehicles.IsInfantryOrderSuspended(slot))) { AICF_TCRFail("REMOTE_ORDER"); return; }
			m_vTCRGoal = target;
			m_iTCRIntent = slot.GetStrategicIntentRevision();
			AICF_TCRLog("GOAL", string.Format("target=%1 initial_distance_m=%2 intent_revision=%3 dismount_radius_m=%4", target, vector.DistanceXZ(m_vTCRStart, target), m_iTCRIntent, m_Stage3Config.GetDismountDistanceMeters()));
			m_iTCRStep = 2;
			return;
		}
		if (slot.GetStrategicIntentRevision() != m_iTCRIntent) { AICF_TCRFail("UNEXPECTED_RETARGET"); return; }
		if (m_iTCRStep <= 6 && (!m_TCRVehicle || m_TCRVehicle.GetID() != m_TCRVehicleId)) { AICF_TCRFail("VEHICLE_IDENTITY"); return; }
		if (m_TCRWitness && m_iTCRStep <= 6 && trip.GetPhase() == AICF_ETransportTripPhase.TRANSIT && !m_TCRWitness.Matches())
		{
			m_TCRWitness.LogReject("ACTIVE", now);
			AICF_TCRFail("WITNESS_IDENTITY");
			return;
		}
		float speed;
		if (m_TCRVehicle && m_TCRVehicle.GetPhysics()) speed = m_TCRVehicle.GetPhysics().GetVelocity().Length();
		AICF_VehicleMovementState movement = trip.GetMovementState();
		if (movement.GetMobilityRecoveryAttempts() > 0) { AICF_TCRFail("MOBILITY_RECOVERY"); return; }
		if (m_iTCRStep == 2)
		{
			if (speed >= 1) m_iTCRMoving++;
			if (vector.DistanceXZ(m_vTCRStart, m_TCRVehicle.GetOrigin()) < 25 || m_iTCRMoving < 5) return;
			if (!AICF_TCRArm(slot, trip)) return;
			m_iTCRStep = 3;
			return;
		}
		if (m_iTCRStep == 3)
		{
			if (!AICF_TransportCombatHold.IsCurrentCombat(trip) || m_TCRWitness.m_Utility.GetCurrentAction() != m_TCRWitness.m_Activity ||
				movement.GetNativeCombatActivity() != m_TCRWitness.m_Activity)
			{
				if (now - m_iTCRAt > 15000) AICF_TCRFail("NATIVE_SELECTION_TIMEOUT");
				return;
			}
			m_iTCRHoldAt = now;
			m_iTCRMotionClock = movement.GetLastPhysicalMotionAtMs();
			m_iTCRProgressClock = movement.GetLastRouteProgressAtMs();
			m_vTCRStopped = m_TCRVehicle.GetOrigin();
			AICF_TCRLog("SELECTED", string.Format("operation_id=%1 motion_clock=%2 progress_clock=%3 deadline_ms=%4", trip.GetOperationId(), m_iTCRMotionClock, m_iTCRProgressClock, trip.GetAbsoluteDeadlineMs()));
			m_iTCRStep = 4;
		}
		if (m_iTCRStep == 4)
		{
			if (!AICF_TransportCombatHold.IsCurrentCombat(trip) || m_TCRWitness.m_Utility.GetCurrentAction() != m_TCRWitness.m_Activity ||
				movement.GetLastPhysicalMotionAtMs() != m_iTCRMotionClock || movement.GetLastRouteProgressAtMs() != m_iTCRProgressClock)
			{ AICF_TCRFail("CONTINUOUS_HOLD_OR_CLOCKS"); return; }
			if (speed > 0.5 || vector.DistanceXZ(m_vTCRStopped, m_TCRVehicle.GetOrigin()) > 0.5)
			{
				m_iTCRStationaryAt = 0;
				m_vTCRStopped = m_TCRVehicle.GetOrigin();
			}
			else if (!m_iTCRStationaryAt) m_iTCRStationaryAt = now;
			if (now >= m_iTCRSample)
			{
				m_iTCRSample = now + 5000;
				int stationaryMs;
				if (m_iTCRStationaryAt) stationaryMs = now - m_iTCRStationaryAt;
				AICF_TCRLog("HOLD", string.Format("operation_id=%1 hold_ms=%2 stationary_ms=%3 motion_clock=%4 progress_clock=%5 deadline_ms=%6 exact_current=1",
					trip.GetOperationId(), now - m_iTCRHoldAt, stationaryMs, m_iTCRMotionClock, m_iTCRProgressClock, trip.GetAbsoluteDeadlineMs()));
			}
			if (now - m_iTCRHoldAt < 90000 || !m_iTCRStationaryAt || now - m_iTCRStationaryAt < 60000)
			{
				if (now - m_iTCRHoldAt > 120000) AICF_TCRFail("STATIONARY_HOLD_TIMEOUT");
				return;
			}
			AICF_TCRGate.Clear();
			m_iTCRReleaseAt = now;
			m_vTCRRelease = m_TCRVehicle.GetOrigin();
			m_iTCRMoving = 0;
			AICF_TCRLog("RELEASE", string.Format("operation_id=%1 hold_ms=%2 stationary_ms=%3 motion_clock=%4 progress_clock=%5 deadline_ms=%6", trip.GetOperationId(), now - m_iTCRHoldAt, now - m_iTCRStationaryAt, m_iTCRMotionClock, m_iTCRProgressClock, trip.GetAbsoluteDeadlineMs()));
			m_iTCRStep = 5;
			return;
		}
		if (m_iTCRStep == 5)
		{
			if (m_TCRWitness.m_Utility.GetCurrentAction() == m_TCRWitness.m_Activity)
			{
				if (now - m_iTCRReleaseAt > 10000) AICF_TCRFail("DESELECTION_TIMEOUT");
				return;
			}
			AICF_TCRLog("DESELECTED", string.Format("operation_id=%1 elapsed_ms=%2 policy_current=%3", trip.GetOperationId(), now - m_iTCRReleaseAt, AICF_TransportCombatHold.IsCurrentCombat(trip)));
			m_iTCRStep = 6;
		}
		if (m_iTCRStep == 6)
		{
			float resumed = vector.DistanceXZ(m_vTCRRelease, m_TCRVehicle.GetOrigin());
			if (speed >= 1) m_iTCRMoving++;
			SCR_AIMoveActivity nativeMove = SCR_AIMoveActivity.Cast(m_TCRGroup.GetGroupUtilityComponent().GetCurrentAction());
			bool nativeRoute = nativeMove && nativeMove.m_Utility == m_TCRGroup.GetGroupUtilityComponent() &&
				nativeMove.m_RelatedWaypoint == movement.GetRouteWaypoint() && movement.IsRouteWaypointBound() &&
				m_TCRGroup.GetCurrentWaypoint() == movement.GetRouteWaypoint();
			if (!m_bTCRResumed && resumed >= 100 && m_iTCRMoving >= 5 && AICF_TCRSeats(slot) && nativeRoute)
			{
				m_bTCRResumed = true;
				AICF_TCRLog("RESUMED", string.Format("operation_id=%1 vehicle=%2 displacement_m=%3 moving_polls=%4 exact_seats=5 deadline_ms=%5 intent_revision=%6 native_route=1", trip.GetOperationId(), m_TCRVehicleId, resumed, m_iTCRMoving, trip.GetAbsoluteDeadlineMs(), m_iTCRIntent));
			}
			if (!m_bTCRResumed && now - m_iTCRReleaseAt > 150000) { AICF_TCRFail("RESUME_TIMEOUT"); return; }
			if (trip.GetPhase() == AICF_ETransportTripPhase.TRANSIT) return;
			float distance = vector.DistanceXZ(m_TCRVehicle.GetOrigin(), m_vTCRGoal);
			if (!AICF_TCRCheck("ARRIVAL", m_bTCRResumed && distance <= m_Stage3Config.GetDismountDistanceMeters() + 10)) { AICF_TCRFinish(); return; }
			AICF_TCRLog("ARRIVAL", string.Format("operation_id=%1 vehicle=%2 distance_m=%3 target=%4 intent_revision=%5", trip.GetOperationId(), m_TCRVehicleId, distance, m_vTCRGoal, m_iTCRIntent));
			m_iTCRStep = 7;
		}
		if (m_iTCRStep == 7)
		{
			if (trip.GetPhase() != AICF_ETransportTripPhase.COMPLETE) return;
			if (!AICF_TCRCheck("NORMAL_COMPLETE", AICF_GroupRuntime.CountAliveAgentsInAnyVehicle(m_TCRGroup) == 0 &&
				!m_TCRVehicles.IsInfantryOrderSuspended(slot) && slot.GetWaypoint() && m_TCRGroup.GetCurrentWaypoint() == slot.GetWaypoint())) { AICF_TCRFinish(); return; }
			m_iTCRStep = 8;
		}
		if (m_iTCRStep == 8)
		{
			float farthest;
			for (int actorIndex; actorIndex < 5; actorIndex++) farthest = Math.Max(farthest, vector.DistanceXZ(slot.GetRosterMember(actorIndex).GetOrigin(), m_vTCRGoal));
			if (farthest > 35) return;
			AICF_TCRLog("FOOT", string.Format("operation_id=%1 farthest_m=%2 alive=5 target=%3 intent_revision=%4", trip.GetOperationId(), farthest, m_vTCRGoal, m_iTCRIntent));
			AICF_TCRCheck("FOOT_ARRIVAL", true);
			AICF_TCRFinish();
		}
	}
	protected void AICF_TCRFinish()
	{
		AICF_TCRGate.Clear();
		if (m_bTCRDone) return;
		m_bTCRDone = true;
		if (m_TCRVehicles)
		{
			m_TCRVehicles.StopWithFactionContexts(true, null, null, m_TCRState, m_USSRFaction);
			m_TCRVehicles = null;
		}
		if (m_TCRGroup)
		{
			m_OrderPlanner.ClearOrder(m_TCRState.GetSlot(0));
			m_TCRGroup.DespawnMembers();
			RplComponent.DeleteRplEntity(m_TCRGroup, false);
			m_TCRGroup = null;
		}
		AICF_TCRLog("DONE", string.Format("checks=%1 failures=%2 gate_cleared=%3 mode=FAULT_INJECTED", m_iTCRChecks, m_iTCRFailures, AICF_TCRGate.s_Owned == null));
		GetGame().RequestClose();
	}
	override protected void Stop(bool cleanupEntities)
	{
		AICF_TCRGate.Clear();
		string enabled;
		if (Replication.IsServer() && !m_bTCRDone && System.GetCLIParam("aicfTransportCombatResume", enabled) && enabled == "1")
		{
			m_iTCRObservedAt = System.GetTickCount();
			AICF_TCRFail("MATCH_STOPPED");
		}
		super.Stop(cleanupEntities);
	}
}
