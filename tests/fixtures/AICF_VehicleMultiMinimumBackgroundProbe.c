// Дополнительная controlled preparation. Не заменяет stress с production фоном.
class AICF_MultiBackgroundCensus
{
	static bool SnapshotComplete(int returned, int examined, int missing, bool groupsComplete,
		int before, int after, int intendedBefore, int intendedAfter)
	{
		if (returned < 0 || before < 0 || intendedBefore < 0 || missing != 0 || !groupsComplete) return false;
		return returned <= 4096 && examined == returned && before == after && intendedBefore == intendedAfter;
	}
}

class AICF_MultiCarActivationProof
{
	static bool WorldComplete(int returned, int examined, int missing)
	{
		return returned >= 0 && returned <= 4096 && examined == returned && missing == 0;
	}

	static bool Proven(bool scopeCurrent, bool driverCurrent, bool handlerCurrent, bool componentExact,
		int worldMatches, bool worldComplete, bool repeatedCurrent)
	{
		return scopeCurrent && driverCurrent && handlerCurrent && componentExact &&
			worldMatches == 1 && worldComplete && repeatedCurrent;
	}
}

modded class AICF_GroupTrace
{
	protected static string AICF_ActivationFlags(string prefix, AIAgent agent)
	{
		if (!agent) return " " + prefix + "_active=UNKNOWN";
		return string.Format(" %1_agent=%2 %1_controlled=%3 %1_parent=%4 %1_active=%5 %1_counted=%6 %1_lod=%7 %1_permanent_lod=%8 %1_max_lod=%9",
			prefix, Entity(agent), Entity(agent.GetControlledEntity()), Entity(agent.GetParentGroup()),
			agent.IsAIActivated(), agent.CountTowardsAIWorldLimit(), agent.GetLOD(), agent.GetPermanentLOD(), AIAgent.GetMaxLOD());
	}

	protected static void AICF_ActivationRow(string fields)
	{
		Print("[AICF][MULTI_BACKGROUND_VEHICLE] background_mode=MINIMUM_BACKGROUND_CAPACITY preparation_only=1 original_stress_covered=0 normal_campaign=0 " + fields + " readonly=1");
	}

	static void AICF_MultiVehicleActivation(AICF_MatchController controller, AICF_GroupSlot slot, SCR_AIGroup group,
		AICF_TransportTrip trip, array<AIAgent> worldAgents, bool worldComplete)
	{
		if (!Replication.IsServer() || !controller || !controller.AICF_BackgroundObserverCurrent()) return;
		if (!slot || !group || !trip || !trip.GetAssignment() || !trip.GetMovementState()) return;
		if (!trip.GetLease() || !trip.GetLease().HasPhysicalAsset()) return;
		AICF_VehicleLease lease = trip.GetLease(); Vehicle vehicle = lease.GetVehicle();
		if (!vehicle) return;
		string prefix = string.Format("t_ms=%1 operation_id=%2 faction=%3 numeric_slot=%4 group_generation=%5 trip_generation=%6 lease_generation=%7 vehicle_generation=%8 vehicle=%9",
			System.GetTickCount(), trip.GetOperationId(), trip.GetFactionKey(), slot.GetSlotId(), slot.GetSpawnGeneration(),
			trip.GetTripGeneration(), lease.GetLeaseGeneration(), lease.GetVehicleGeneration(), Entity(vehicle));
		prefix += string.Format(" vehicle_rpl=%1 group=%2 phase=%3 route_generation=%4 route_endpoint=%5 assignment_goal=%6 assignment_revision=%7 intent_revision=%8",
			AICF_VehicleBehaviorBinding.ComponentRpl(vehicle), Entity(group), trip.GetPhase(), trip.GetMovementState().GetRouteGeneration(),
			trip.GetMovementState().GetRouteEndpoint(), trip.GetAssignment().GetTargetPosition(), trip.GetAssignment().GetAssignmentRevision(), slot.GetStrategicIntentRevision());
		if (!IsNativePathTripCurrent(slot, group, trip))
		{
			AICF_ActivationRow(prefix + " resolution=UNPROVEN reason=NO_CURRENT_OWNED_ROUTE car_active=UNKNOWN pilot_active=UNKNOWN"); return;
		}
		SCR_AIGroupUtilityComponent utility = group.GetGroupUtilityComponent();
		SCR_AIGroupVehicle record = utility.m_VehicleMgr.FindVehicle(vehicle);
		SCR_AIVehicleUsageComponent usage = SCR_AIVehicleUsageComponent.Cast(vehicle.FindComponent(SCR_AIVehicleUsageComponent));
		PilotCompartmentSlot pilot;
		if (usage) pilot = usage.GetPilotCompartmentSlot();
		ChimeraCharacter driver;
		if (pilot) driver = ChimeraCharacter.Cast(pilot.GetOccupant());
		AIAgent driverAgent;
		if (driver && driver.GetAIControlComponent()) driverAgent = driver.GetAIControlComponent().GetControlAIAgent();
		AIBaseMovementComponent movement;
		if (driverAgent) movement = driverAgent.GetMovementComponent();
		AICarMovementComponent car = AICarMovementComponent.Cast(movement);
		AIAgent carAgent;
		if (car) carAgent = car.GetAIAgent();
		string driverReason;
		bool driverCurrent = IsNativePathDriverCurrent(group, vehicle, driver, driverAgent, pilot, movement, driverReason);
		int handler = -1; int pilotHandler = -1;
		if (record) handler = record.GetSubgroupHandleId();
		AIGroupMovementComponent groupMovement = AIGroupMovementComponent.Cast(group.FindComponent(AIGroupMovementComponent));
		if (groupMovement && driverAgent) pilotHandler = groupMovement.GetAgentMoveHandlerId(driverAgent);
		bool handlerCurrent = record && record.GetEntity() == vehicle && handler >= 0 &&
			handler != AIGroupMovementComponent.DEFAULT_HANDLER_ID && pilotHandler == handler;
		int matches; AIAgent matched;
		if (worldAgents)
			for (int index; index < Math.Min(worldAgents.Count(), 4096); index++)
				if (worldAgents[index] && worldAgents[index].GetControlledEntity() == vehicle) { matches++; matched = worldAgents[index]; }
		bool componentExact = carAgent && carAgent.GetControlledEntity() == vehicle && carAgent == matched;
		prefix += string.Format(" car_candidate=%1 pilot_candidate=%2 pilot_actor=%3 movement_component=%4",
			Entity(carAgent), Entity(driverAgent), Entity(driver), movement);
		prefix += string.Format(" handler=%1 pilot_handler=%2 handler_current=%3 driver_current=%4 driver_reason=%5 world_vehicle_matches=%6 world_complete=%7 component_exact=%8",
			handler, pilotHandler, handlerCurrent, driverCurrent, driverReason, matches, worldComplete, componentExact);
		if (!AICF_MultiCarActivationProof.Proven(true, driverCurrent, handlerCurrent, componentExact, matches, worldComplete, true))
		{
			AICF_ActivationRow(prefix + " resolution=UNPROVEN reason=AMBIGUOUS_OR_MISSING_NATIVE_AGENT car_active=UNKNOWN pilot_active=UNKNOWN"); return;
		}
		AIWaypoint route = trip.GetMovementState().GetRouteWaypoint(); int routeGeneration = trip.GetMovementState().GetRouteGeneration();
		string flags = AICF_ActivationFlags("car", carAgent) + AICF_ActivationFlags("pilot", driverAgent);
		flags += string.Format(" car_agent_rpl=%1 pilot_rpl=%2 pilot_agent_rpl=%3 pilot_control_agent=%4 pilot_control_associated_agent=%5",
			AICF_VehicleBehaviorBinding.ComponentRpl(carAgent), AICF_VehicleBehaviorBinding.ComponentRpl(driver),
			AICF_VehicleBehaviorBinding.ComponentRpl(driverAgent), Entity(driver.GetAIControlComponent().GetControlAIAgent()), Entity(driver.GetAIControlComponent().GetAIAgent()));
		AIControlComponent carControl = carAgent.GetControlComponent();
		if (carControl) flags += " car_control_associated_agent=" + Entity(carControl.GetAIAgent());
		SCR_AIMoveActivity move = SCR_AIMoveActivity.Cast(utility.GetCurrentAction());
		flags += " current_group_action=" + Action(utility.GetCurrentAction()) + " executed_group_action=" + Action(utility.GetExecutedAction());
		if (move && move.m_vPosition) flags += " native_move_goal=" + move.m_vPosition.m_Value.ToString();
		if (move && move.m_Entity) flags += " native_move_entity=" + Entity(move.m_Entity.m_Value);
		IEntity navlink = car.GetLastNavlinkEntity();
		flags += " last_navlink=" + Entity(navlink);
		if (navlink) flags += " last_navlink_type=" + navlink.Type().ToString() + " last_navlink_pos=" + Position(navlink);
		array<vector> points = {}; car.GetCurrentPath(points);
		flags += string.Format(" native_path_count=%1 native_path_order_raw=1 path_remaining_proven=0", points.Count());
		if (!points.IsEmpty()) flags += string.Format(" native_path_first=%1 native_path_last=%2", points[0], points[points.Count() - 1]);
		Physics physics = vehicle.GetPhysics();
		if (physics) flags += string.Format(" velocity=%1 speed_mps=%2", physics.GetVelocity(), physics.GetVelocity().Length());
		else flags += " velocity=UNKNOWN speed_mps=UNKNOWN";
		CarControllerComponent carController = CarControllerComponent.Cast(vehicle.FindComponent(CarControllerComponent));
		if (carController) flags += string.Format(" handbrake=%1 persistent_handbrake=%2", carController.GetHandBrake(), carController.GetPersistentHandBrake());
		else flags += " handbrake=UNKNOWN persistent_handbrake=UNKNOWN";
		flags += string.Format(" usage_damage_state=%1 fire=%2", usage.GetDamageState(), SCR_AIVehicleUsability.VehicleIsOnFire(vehicle));
		SCR_DamageManagerComponent damage = SCR_DamageManagerComponent.GetDamageManager(vehicle);
		if (damage && damage.GetDefaultHitZone()) flags += string.Format(" health_scaled=%1 movement_damage=%2", damage.GetDefaultHitZone().GetHealthScaled(), damage.GetMovementDamage());
		else flags += " health_scaled=UNKNOWN movement_damage=UNKNOWN";
		flags += string.Format(" gate_pending=%1 crew_recovery_pending=%2", trip.GetMovementState().GetNativeGate() != null, trip.GetMovementState().GetCrewRecoveryToken() != null);
		bool repeated = controller.AICF_BackgroundObserverCurrent() && IsNativePathTripCurrent(slot, group, trip);
		if (repeated) repeated = trip.GetLease() == lease && trip.GetMovementState().GetRouteWaypoint() == route &&
			trip.GetMovementState().GetRouteGeneration() == routeGeneration;
		if (repeated) repeated = IsNativePathDriverCurrent(group, vehicle, driver, driverAgent, pilot, movement, driverReason) && utility.m_VehicleMgr.FindVehicle(vehicle) == record;
		if (repeated) repeated = record.GetSubgroupHandleId() == handler && groupMovement.GetAgentMoveHandlerId(driverAgent) == pilotHandler;
		if (repeated) repeated = car.GetAIAgent() == carAgent && carAgent.GetControlledEntity() == vehicle;
		array<AIAgent> repeatedAgents = {};
		int repeatedMatches; int repeatedMissing; AIAgent repeatedMatched;
		AIWorld repeatedWorld = GetGame().GetAIWorld();
		if (repeatedWorld) repeatedWorld.GetAIAgents(repeatedAgents);
		for (int again; again < Math.Min(repeatedAgents.Count(), 4096); again++)
		{
			if (!repeatedAgents[again]) { repeatedMissing++; continue; }
			if (repeatedAgents[again].GetControlledEntity() == vehicle) { repeatedMatches++; repeatedMatched = repeatedAgents[again]; }
		}
		bool repeatedWorldComplete = repeatedWorld && AICF_MultiCarActivationProof.WorldComplete(repeatedAgents.Count(), Math.Min(repeatedAgents.Count(), 4096), repeatedMissing);
		prefix += string.Format(" repeated_world_matches=%1 repeated_world_complete=%2", repeatedMatches, repeatedWorldComplete);
		repeated = repeated && repeatedWorldComplete && repeatedMatches == 1 && repeatedMatched == carAgent;
		if (repeated) repeated = controller.AICF_BackgroundObserverCurrent() && IsNativePathTripCurrent(slot, group, trip) && car.GetAIAgent() == carAgent;
		if (!AICF_MultiCarActivationProof.Proven(true, driverCurrent, handlerCurrent, componentExact, matches, worldComplete, repeated))
		{
			AICF_ActivationRow(prefix + " resolution=UNPROVEN reason=CONTEXT_CHANGED_DURING_NATIVE_READ car_active=UNKNOWN pilot_active=UNKNOWN"); return;
		}
		AICF_ActivationRow(prefix + " resolution=PROVEN" + flags);
	}
}

modded class AICF_InfantryRecruitmentService
{
	bool AICF_MultiBackgroundHasOrder(AICF_GroupSlot slot)
	{
		return HasOrder(slot);
	}
}

class AICF_MultiBackgroundOriginal
{
	AICF_FactionState m_State;
	AICF_GroupSlot m_Slot;
	SCR_AIGroup m_Group;
	IEntity m_Actor;
	AIAgent m_Agent;
	EntityID m_GroupId;
	EntityID m_ActorId;
	string m_GroupRpl;
	string m_ActorRpl;
	string m_Faction;
	int m_Id;
	int m_Generation;

	bool Capture(AICF_FactionState state, string faction, int id, AICF_InfantryRecruitmentService recruitment)
	{
		if (!Replication.IsServer() || !state || id < 6 || id > 9 || !recruitment) return false;
		m_State = state; m_Faction = faction; m_Id = id; m_Slot = state.GetSlot(id);
		if (!m_Slot || !m_Slot.IsCombatReady() || m_Slot.HasActiveRecruitmentOrder() || recruitment.AICF_MultiBackgroundHasOrder(m_Slot)) return false;
		m_Group = m_Slot.GetGroup();
		if (!m_Group || m_Group.GetAgentsCount() != 1 || AICF_GroupRuntime.CountAliveAgents(m_Group) != 1) return false;
		array<AIAgent> agents = {}; m_Group.GetAgents(agents);
		if (agents.Count() != 1 || !agents[0]) return false;
		m_Agent = agents[0]; m_Actor = m_Agent.GetControlledEntity();
		if (!AICF_VehicleBoardingMutationFence.IsAuthoritativeReplicatedEntity(m_Group) ||
			!AICF_VehicleBoardingMutationFence.IsAuthoritativeAIEntity(m_Actor) ||
			m_Agent.GetParentGroup() != m_Group || !m_Group.GetFaction() || m_Group.GetFaction().GetFactionKey() != faction) return false;
		ChimeraCharacter character = ChimeraCharacter.Cast(m_Actor);
		if (!character || !character.GetAIControlComponent() || character.GetAIControlComponent().GetControlAIAgent() != m_Agent) return false;
		m_GroupId = m_Group.GetID(); m_ActorId = m_Actor.GetID(); m_Generation = m_Slot.GetSpawnGeneration();
		m_GroupRpl = AICF_VehicleBehaviorBinding.ComponentRpl(m_Group); m_ActorRpl = AICF_VehicleBehaviorBinding.ComponentRpl(m_Actor);
		return IsCurrent(false, recruitment);
	}

	bool IsCurrent(bool requireOne, AICF_InfantryRecruitmentService recruitment)
	{
		if (!Replication.IsServer() || !m_State || !m_Slot || m_State.GetSlot(m_Id) != m_Slot || !m_Slot.IsCombatReady()) return false;
		if (!m_Group || m_Slot.GetGroup() != m_Group || m_Slot.GetSpawnGeneration() != m_Generation ||
			m_Group.GetID() != m_GroupId || AICF_VehicleBehaviorBinding.ComponentRpl(m_Group) != m_GroupRpl) return false;
		if (!AICF_VehicleBoardingMutationFence.IsAuthoritativeReplicatedEntity(m_Group) || !m_Group.GetFaction() ||
			m_Group.GetFaction().GetFactionKey() != m_Faction || m_Slot.GetUnitType() != AICF_EGroupUnitType.INFANTRY) return false;
		if (!recruitment || m_Slot.HasActiveRecruitmentOrder() || recruitment.AICF_MultiBackgroundHasOrder(m_Slot) ||
			(requireOne && (m_Slot.GetDesiredSize() != 1 || m_Slot.IsRecruitingInfantry()))) return false;
		if (m_Group.GetAgentsCount() != 1 || AICF_GroupRuntime.CountAliveAgents(m_Group) != 1 || !m_Actor || m_Actor.GetID() != m_ActorId ||
			AICF_VehicleBehaviorBinding.ComponentRpl(m_Actor) != m_ActorRpl || !AICF_GroupRuntime.IsAliveCharacter(m_Actor) ||
			!AICF_VehicleBoardingMutationFence.IsAuthoritativeAIEntity(m_Actor)) return false;
		array<AIAgent> agents = {}; m_Group.GetAgents(agents);
		if (agents.Count() != 1 || agents[0] != m_Agent || m_Agent.GetControlledEntity() != m_Actor || m_Agent.GetParentGroup() != m_Group) return false;
		ChimeraCharacter character = ChimeraCharacter.Cast(m_Actor);
		return character && character.GetAIControlComponent() && character.GetAIControlComponent().GetControlAIAgent() == m_Agent;
	}
}

modded class AICF_MatchController
{
	protected ref array<ref AICF_MultiBackgroundOriginal> m_aAICFMinimumBackground = {};
	protected bool m_bAICFMinimumBackgroundReady;
	protected int m_iAICFBackgroundNextReadback;
	protected int m_iAICFBackgroundNextCensus;
	protected int m_iAICFBackgroundLastCohort = -1;
	protected int m_iAICFBackgroundNextVehicleObserver;
	protected AICF_FactionState m_AICFBackgroundCapturedUS;
	protected AICF_FactionState m_AICFBackgroundCapturedUSSR;

	protected bool AICF_MinimumBackgroundSelected()
	{
		string mode;
		return System.GetCLIParam("aicfVehicleMultiBackgroundMode", mode) && mode == "MINIMUM_BACKGROUND_CAPACITY";
	}

	bool AICF_BackgroundObserverCurrent()
	{
		return AICF_BackgroundOwnerCurrent() && AICF_BackgroundEpochCurrent() && m_bAICFMinimumBackgroundReady;
	}

	protected void AICF_BackgroundVehicleObserver()
	{
		if (!AICF_BackgroundObserverCurrent() || !GetGame() || !m_VehicleCoordinator) return;
		AIWorld world = GetGame().GetAIWorld();
		if (!world) return;
		array<AIAgent> agents = {}; world.GetAIAgents(agents);
		int missing;
		for (int member; member < Math.Min(agents.Count(), 4096); member++)
			if (!agents[member]) missing++;
		bool complete = AICF_MultiCarActivationProof.WorldComplete(agents.Count(), Math.Min(agents.Count(), 4096), missing);
		for (int index; index < Math.Min(m_aAICFMultiCases.Count(), 12); index++)
		{
			AICF_MultiSquadCase item = m_aAICFMultiCases[index];
			AICF_GroupSlot slot = AICF_MultiSlot(item);
			if (!AICF_BackgroundObserverCurrent()) return;
			if (!slot || !slot.GetGroup()) continue;
			AICF_GroupTrace.AICF_MultiVehicleActivation(this, slot, slot.GetGroup(), m_VehicleCoordinator.GetTrip(slot), agents, complete);
		}
	}

	protected bool AICF_BackgroundOwnerCurrent()
	{
		return Replication.IsServer() && m_bStarted && !m_bStopped && AICF_MatchController.GetActiveController() == this;
	}

	protected bool AICF_BackgroundEpochCurrent()
	{
		if (!m_AICFBackgroundCapturedUS && !m_AICFBackgroundCapturedUSSR) return true;
		return m_AICFBackgroundCapturedUS == m_USState && m_AICFBackgroundCapturedUSSR == m_USSRState;
	}

	protected bool AICF_BackgroundStateCurrent(AICF_MultiBackgroundOriginal captured)
	{
		if (!AICF_BackgroundOwnerCurrent() || !captured) return false;
		if (!m_AICFBackgroundCapturedUS || !m_AICFBackgroundCapturedUSSR || m_AICFBackgroundCapturedUS != m_USState ||
			m_AICFBackgroundCapturedUSSR != m_USSRState) return false;
		if (captured.m_Faction == "US") return captured.m_State == m_USState;
		if (captured.m_Faction == "USSR") return captured.m_State == m_USSRState;
		return false;
	}

	protected bool AICF_BackgroundAbortOwned(string reason)
	{
		if (!AICF_BackgroundOwnerCurrent() || !AICF_BackgroundEpochCurrent()) return false;
		AICF_MultiAbort(reason);
		return false;
	}

	protected bool AICF_BackgroundConfigureOne(AICF_MultiBackgroundOriginal captured)
	{
		if (!AICF_BackgroundStateCurrent(captured)) return false;
		bool beforeCurrent = captured.IsCurrent(false, m_InfantryRecruitment);
		if (!AICF_BackgroundStateCurrent(captured)) return false;
		if (!beforeCurrent) return AICF_BackgroundAbortOwned("MINIMUM_BACKGROUND_CONFIGURATION_REJECTED");
		bool applied = captured.m_State.ApplyCommanderConfiguration(captured.m_Id, captured.m_Slot.GetRole(), AICF_EGroupUnitType.INFANTRY, 1);
		if (!AICF_BackgroundStateCurrent(captured)) return false;
		if (applied && !AICF_BackgroundRefreshMuster(captured)) return false;
		bool afterCurrent = applied && captured.IsCurrent(true, m_InfantryRecruitment);
		if (!AICF_BackgroundStateCurrent(captured)) return false;
		if (!afterCurrent) return AICF_BackgroundAbortOwned("MINIMUM_BACKGROUND_CONFIGURATION_REJECTED");
		return true;
	}

	protected bool AICF_BackgroundRefreshMuster(AICF_MultiBackgroundOriginal captured)
	{
		if (!AICF_BackgroundStateCurrent(captured)) return false;
		if (!captured.m_Slot.IsWaitingForInfantryMuster()) return true;
		bool current = captured.IsCurrent(false, m_InfantryRecruitment);
		if (!AICF_BackgroundStateCurrent(captured)) return false;
		if (!current || captured.m_Slot.GetDesiredSize() != 1 || !m_OrderPlanner || !m_ObjectiveGraph || !m_TargetSelector)
			return AICF_BackgroundAbortOwned("MINIMUM_BACKGROUND_MUSTER_HANDOFF_INVALID");
		SCR_CampaignFaction faction;
		if (captured.m_Faction == "US") faction = m_USFaction;
		if (captured.m_Faction == "USSR") faction = m_USSRFaction;
		if (!faction || faction.GetFactionKey() != captured.m_Faction)
			return AICF_BackgroundAbortOwned("MINIMUM_BACKGROUND_MUSTER_FACTION_INVALID");
		if (!AICF_BackgroundStateCurrent(captured)) return false;
		m_OrderPlanner.RefreshInfantryMuster(captured.m_Slot, faction, m_ObjectiveGraph, m_TargetSelector);
		if (!AICF_BackgroundStateCurrent(captured)) return false;
		AICF_BackgroundLog("MUSTER_HANDOFF", string.Format("faction=%1 numeric_slot=%2 desired=%3 waiting_muster=%4 recruiting=%5 active_order=%6 queued_order=%7 waypoint=%8 owner=ORDER_PLANNER",
			captured.m_Faction, captured.m_Id, captured.m_Slot.GetDesiredSize(), captured.m_Slot.IsWaitingForInfantryMuster(),
			captured.m_Slot.IsRecruitingInfantry(), captured.m_Slot.HasActiveRecruitmentOrder(),
			m_InfantryRecruitment.AICF_MultiBackgroundHasOrder(captured.m_Slot), AICF_GroupTrace.Entity(captured.m_Slot.GetWaypoint())));
		if (!AICF_BackgroundStateCurrent(captured)) return false;
		if (captured.m_Slot.IsWaitingForInfantryMuster() || !captured.m_Slot.GetWaypoint())
			return AICF_BackgroundAbortOwned("MINIMUM_BACKGROUND_MUSTER_HANDOFF_INCOMPLETE");
		return true;
	}

	protected void AICF_BackgroundLog(string eventName, string fields)
	{
		if (!AICF_BackgroundOwnerCurrent()) return;
		Print("[AICF][MULTI_BACKGROUND_" + eventName + "] background_mode=MINIMUM_BACKGROUND_CAPACITY preparation_only=1 original_stress_covered=0 normal_campaign=0 " + fields);
	}

	override protected bool AICF_MultiInitialize()
	{
		if (!AICF_MinimumBackgroundSelected()) return super.AICF_MultiInitialize();
		if (!AICF_BackgroundOwnerCurrent() || !m_bRosterReady || !m_USState || !m_USSRState) return false;
		if (!m_AICFBackgroundCapturedUS && !m_AICFBackgroundCapturedUSSR)
		{
			m_AICFBackgroundCapturedUS = m_USState; m_AICFBackgroundCapturedUSSR = m_USSRState;
		}
		if (m_AICFBackgroundCapturedUS != m_USState || m_AICFBackgroundCapturedUSSR != m_USSRState) return false;
		if (!m_bAICFMinimumBackgroundReady)
		{
			if (m_sAICFMultiMode != "CAPACITY" || m_USState.GetSlotCount() != 10 || m_USSRState.GetSlotCount() != 10 || !m_InfantryRecruitment)
			{
				return AICF_BackgroundAbortOwned("MINIMUM_BACKGROUND_SETUP_CONTEXT_INVALID");
			}
			// Сначала весь preflight; частичное сокращение уже набранного roster запрещено.
			for (int side; side < 2; side++)
				for (int id = 6; id < 10; id++)
				{
					AICF_FactionState state = m_USState; string faction = "US";
					if (side == 1) { state = m_USSRState; faction = "USSR"; }
					if (!AICF_BackgroundOwnerCurrent()) return false;
					AICF_MultiBackgroundOriginal original = new AICF_MultiBackgroundOriginal();
					bool capturedOriginal = original.Capture(state, faction, id, m_InfantryRecruitment);
					if (!AICF_BackgroundStateCurrent(original)) return false;
					if (!capturedOriginal)
					{
						AICF_BackgroundLog("REJECTED", string.Format("faction=%1 numeric_slot=%2 reason=NOT_EXACT_UNRECRUITED_SINGLE_ORIGINAL", faction, id));
						return AICF_BackgroundAbortOwned("MINIMUM_BACKGROUND_ALREADY_RECRUITED_OR_IDENTITY_INVALID");
					}
					m_aAICFMinimumBackground.Insert(original);
				}
			if (m_aAICFMinimumBackground.Count() != 8) return AICF_BackgroundAbortOwned("MINIMUM_BACKGROUND_MISSING_ORIGINAL");
			foreach (AICF_MultiBackgroundOriginal captured : m_aAICFMinimumBackground)
			{
				if (!AICF_BackgroundConfigureOne(captured)) return false;
			}
			if (!AICF_BackgroundOwnerCurrent()) return false;
			m_bAICFMinimumBackgroundReady = true;
			if (!AICF_BackgroundReadback("SETUP")) return false;
			AICF_BackgroundCensus("SETUP");
		}
		if (!AICF_BackgroundOwnerCurrent()) return false;
		return super.AICF_MultiInitialize();
	}

	protected bool AICF_BackgroundReadback(string stage)
	{
		if (!AICF_BackgroundOwnerCurrent() || !m_USState || !m_USSRState || !m_InfantryRecruitment) return false;
		int missing;
		string originals;
		foreach (AICF_MultiBackgroundOriginal original : m_aAICFMinimumBackground)
		{
			if (!AICF_BackgroundStateCurrent(original)) return false;
			bool exact = original.IsCurrent(true, m_InfantryRecruitment);
			if (!exact) missing++;
			int desired = -1; if (original && original.m_Slot) desired = original.m_Slot.GetDesiredSize();
			if (original)
			{
				int physical = -1; int alive = -1; bool recruiting; bool queued;
				if (original.m_Group) { physical = original.m_Group.GetAgentsCount(); alive = AICF_GroupRuntime.CountAliveAgents(original.m_Group); }
				if (original.m_Slot) { recruiting = original.m_Slot.IsRecruitingInfantry(); queued = m_InfantryRecruitment.AICF_MultiBackgroundHasOrder(original.m_Slot); }
				string fields = string.Format("stage=%1 faction=%2 numeric_slot=%3 group=%4 actor=%5 actor_rpl=%6 same_original_alive_one=%7 desired=%8 generation=%9",
					stage, original.m_Faction, original.m_Id, original.m_GroupId, original.m_ActorId, original.m_ActorRpl, exact, desired, original.m_Generation);
				fields += string.Format(" physical_agents=%1 alive_agents=%2 recruiting=%3 queued_order=%4", physical, alive, recruiting, queued);
				AICF_BackgroundLog("MEMBER", fields);
				originals += string.Format("%1/%2:%3;", original.m_Faction, original.m_Id, original.m_ActorId);
			}
		}
		foreach (AICF_MultiBackgroundOriginal verified : m_aAICFMinimumBackground)
			if (!AICF_BackgroundStateCurrent(verified)) return false;
		AICF_BackgroundLog("READBACK", string.Format("stage=%1 t_ms=%2 background_originals=%3 missing=%4 total_managed_slots=%5 selected_groups=12 selected_crew=5",
			stage, System.GetTickCount(), m_aAICFMinimumBackground.Count(), missing, m_USState.GetSlotCount() + m_USSRState.GetSlotCount()) + " background_originals_id=[" + originals + "]");
		if (missing > 0 || m_aAICFMinimumBackground.Count() != 8) return AICF_BackgroundAbortOwned("MINIMUM_BACKGROUND_ORIGINAL_CHANGED");
		return true;
	}

	protected void AICF_BackgroundCensus(string trigger)
	{
		if (!AICF_BackgroundOwnerCurrent() || !AICF_BackgroundEpochCurrent() || !GetGame()) return;
		AIWorld world = GetGame().GetAIWorld();
		if (!world) return;
		int before = world.GetCurrentNumOfActiveAIs(); int intendedBefore = world.GetActiveGroupIntendedCount();
		array<AIAgent> agents = {}; world.GetAIAgents(agents);
		array<AIGroup> groups = {};
		bool groupsComplete = true;
		int counted; int characters; int vehicles; int controllers; int empty; int other; int missing;
		int usSelected; int usBackground; int ussrSelected; int ussrBackground; int foreignCharacters;
		int foreignUS; int foreignUSSR; int foreignFIA; int foreignCIV; int foreignUnknown;
		int limit = Math.Min(agents.Count(), 4096);
		for (int index; index < limit; index++)
		{
			AIAgent agent = agents[index];
			if (!agent) { missing++; continue; }
			AIGroup owner = agent.GetParentGroup();
			AIGroup groupAgent = AIGroup.Cast(agent);
			if (owner && !groups.Contains(owner))
			{
				if (groups.Count() < 4096) groups.Insert(owner); else groupsComplete = false;
			}
			if (groupAgent && !groups.Contains(groupAgent))
			{
				if (groups.Count() < 4096) groups.Insert(groupAgent); else groupsComplete = false;
			}
			if (!agent.IsAIActivated() || !agent.CountTowardsAIWorldLimit()) continue;
			counted++;
			IEntity entity = agent.GetControlledEntity();
			if (groupAgent) controllers++;
			else if (ChimeraCharacter.Cast(entity))
			{
				characters++;
				SCR_AIGroup nativeGroup = SCR_AIGroup.Cast(owner);
				AICF_GroupSlot slot = FindManagedInfantrySlot(nativeGroup);
				string faction;
				if (nativeGroup && nativeGroup.GetFaction()) faction = nativeGroup.GetFaction().GetFactionKey();
				if (!slot)
				{
					foreignCharacters++;
					if (faction == "US") foreignUS++;
					else if (faction == "USSR") foreignUSSR++;
					else if (faction == "FIA") foreignFIA++;
					else if (faction == "CIV") foreignCIV++;
					else foreignUnknown++;
				}
				else if (faction == "US" && slot.GetSlotId() < 6) usSelected++;
				else if (faction == "US") usBackground++;
				else if (faction == "USSR" && slot.GetSlotId() < 6) ussrSelected++;
				else if (faction == "USSR") ussrBackground++;
			}
			else if (Vehicle.Cast(entity)) vehicles++;
			else if (!entity) empty++;
			else other++;
		}
		int intendedSum; int activeGroups;
		foreach (AIGroup group : groups)
		{
			if (!group || !group.IsAIActivated()) continue;
			activeGroups++;
			int groupIntended = group.GetIntendedActiveCount();
			if (groupIntended < 0 || groupIntended > int.MAX - intendedSum) { groupsComplete = false; continue; }
			intendedSum += groupIntended;
		}
		int after = world.GetCurrentNumOfActiveAIs(); int intendedAfter = world.GetActiveGroupIntendedCount();
		bool complete = AICF_MultiBackgroundCensus.SnapshotComplete(agents.Count(), limit, missing, groupsComplete, before, after, intendedBefore, intendedAfter);
		string data = string.Format("trigger=%1 t_ms=%2 agents_returned=%3 agents_examined=%4 missing=%5 snapshot_complete=%6 native_active_before=%7 native_active_after=%8 counted_activated=%9",
			trigger, System.GetTickCount(), agents.Count(), limit, missing, complete, before, after, counted);
		data += string.Format(" native_intended_before=%1 native_intended_after=%2 hard_limit=%3 global_can_activate=%4 active_characters=%5 active_vehicles=%6 active_group_controllers=%7 active_null=%8 active_other=%9",
			intendedBefore, intendedAfter, world.GetLimitOfActiveAIs(), world.CanAIBeActivated(), characters, vehicles, controllers, empty, other);
		data += string.Format(" us_selected_characters=%1 us_background_characters=%2 ussr_selected_characters=%3 ussr_background_characters=%4 foreign_characters=%5 unique_groups=%6 active_groups=%7 active_group_intended_sum=%8 native_count_reconciled=%9",
			usSelected, usBackground, ussrSelected, ussrBackground, foreignCharacters, groups.Count(), activeGroups, intendedSum, complete && counted == after);
		data += string.Format(" foreign_us_characters=%1 foreign_ussr_characters=%2 foreign_fia_characters=%3 foreign_civ_characters=%4 foreign_unknown_characters=%5",
			foreignUS, foreignUSSR, foreignFIA, foreignCIV, foreignUnknown);
		AICF_BackgroundLog("CENSUS", data + string.Format(" groups_complete=%1 actor_rows_emitted=0 maximum_agents_examined=4096 maximum_groups_examined=4096 readonly=1", groupsComplete));
		m_iAICFBackgroundNextCensus = System.GetTickCount() + 30000;
	}

	override protected void Update()
	{
		bool selected = AICF_MinimumBackgroundSelected();
		if (selected && (!AICF_BackgroundOwnerCurrent() || !AICF_BackgroundEpochCurrent())) return;
		int now = System.GetTickCount();
		if (selected && m_bAICFMinimumBackgroundReady && !m_bAICFMultiDone && !m_bStopped)
		{
			bool finalSample = m_bAICFMultiStarted && now - m_iAICFMultiStart >= m_iAICFMultiDuration;
			if (finalSample || now >= m_iAICFBackgroundNextReadback)
			{
				string stage = "PERIODIC"; if (finalSample) stage = "FINAL";
				if (!AICF_BackgroundReadback(stage)) return;
				m_iAICFBackgroundNextReadback = now + 30000;
			}
		}
		super.Update();
		if (!selected || !AICF_BackgroundOwnerCurrent() || !m_bAICFMinimumBackgroundReady || m_bAICFMultiDone) return;
		if (now >= m_iAICFBackgroundNextVehicleObserver)
		{
			m_iAICFBackgroundNextVehicleObserver = now + 5000; AICF_BackgroundVehicleObserver();
		}
		if (m_iAICFBackgroundLastCohort != m_iAICFMultiCohort)
		{
			m_iAICFBackgroundLastCohort = m_iAICFMultiCohort; AICF_BackgroundCensus("COHORT");
		}
		else if (now >= m_iAICFBackgroundNextCensus && GetGame().GetAIWorld() && !GetGame().GetAIWorld().CanAIBeActivated())
			AICF_BackgroundCensus("CAPACITY_WAIT");
	}
}
