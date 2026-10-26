// Только чтение authority-состояния. Никаких actions, reservations или timers.
class AICF_GroupTraceCursor
{
	string m_sKey;
	int m_iNextMs;
	int m_iSequence;
}

class AICF_GroupTrace
{
	protected static const int NATIVE_PATH_POINT_LIMIT = 128;
	protected ref array<ref AICF_GroupTraceCursor> m_aCursors = {};
	protected bool m_bAllGroups;
	protected int m_iIntervalMs = 5000;
	protected int m_iSlot = -1;
	protected string m_sFaction;

	void AICF_GroupTrace()
	{
		string value;
		m_bAllGroups = System.GetCLIParam("aicfGroupTrace", value) && value == "1";
		if (System.GetCLIParam("aicfGroupTraceIntervalMs", value))
			m_iIntervalMs = Math.Clamp(value.ToInt(), 1000, 60000);
		if (System.GetCLIParam("aicfGroupTraceSlot", value))
			m_iSlot = value.ToInt();
		System.GetCLIParam("aicfGroupTraceFaction", m_sFaction);
	}

	void Stop()
	{
		m_aCursors.Clear();
	}

	void Update(AICF_FactionState state, AICF_VehicleCoordinator vehicles)
	{
		if (!Replication.IsServer() || !state || !vehicles) return;
		if (!m_sFaction.IsEmpty() && m_sFaction != state.GetFactionKey()) return;
		int now = System.GetTickCount();
		for (int index; index < state.GetSlotCount(); index++)
		{
			AICF_GroupSlot slot = state.GetSlot(index);
			if (!slot || !slot.IsCombatReady() || !slot.GetGroup()) continue;
			if (!m_bAllGroups && !slot.HasVehiclePreset()) continue;
			if (m_iSlot >= 0 && slot.GetSlotId() != m_iSlot) continue;
			string key = string.Format("%1:%2", state.GetFactionKey(), slot.GetSlotId());
			AICF_GroupTraceCursor cursor;
			foreach (AICF_GroupTraceCursor candidate : m_aCursors)
				if (candidate.m_sKey == key) { cursor = candidate; break; }
			if (!cursor)
			{
				cursor = new AICF_GroupTraceCursor();
				cursor.m_sKey = key;
				m_aCursors.Insert(cursor);
			}
			if (now < cursor.m_iNextMs) continue;
			cursor.m_iNextMs = now + m_iIntervalMs;
			cursor.m_iSequence++;
			Snapshot(slot, state.GetFactionKey(), vehicles.GetTrip(slot), cursor.m_iSequence, "PERIODIC_READBACK");
		}
	}

	static string Entity(IEntity entity)
	{
		if (!entity) return "NONE";
		string value = entity.GetID().ToString();
		value.Replace(" ", "");
		return value;
	}

	static string Position(IEntity entity)
	{
		if (!entity) return "NONE";
		vector position = entity.GetOrigin();
		return string.Format("<%1,%2,%3>", position[0], position[1], position[2]);
	}

	static string Action(AIActionBase action)
	{
		if (!action) return "NONE";
		return action.Type().ToString() + ":" + typename.EnumToString(EAIActionState, action.GetActionState());
	}

	static void Snapshot(AICF_GroupSlot slot, FactionKey faction, AICF_TransportTrip trip, int sequence, string trigger)
	{
		if (!Replication.IsServer() || !slot || !slot.GetGroup()) return;
		SCR_AIGroup group = slot.GetGroup();
		IEntity leader = AICF_GroupRuntime.ResolveAliveLeader(group);
		AIWaypoint current = group.GetCurrentWaypoint();
		array<AIWaypoint> queue = {};
		group.GetWaypoints(queue);
		string identity = string.Format("faction=%1 numeric_slot=%2 stable_slot=%3 group=%4 group_generation=%5 trace_seq=%6",
			faction, slot.GetSlotId(), slot.GetStableSlotKey(), Entity(group), slot.GetSpawnGeneration(), sequence);
		string details = identity + string.Format(" trigger=%1 callsign=%2 assignment_revision=%3 intent_revision=%4 vehicle_revision=%5",
			trigger, slot.GetCallsignId(), slot.GetStrategicAssignmentRevision(), slot.GetStrategicIntentRevision(), slot.GetVehiclePresetRevision());
		details += string.Format(" role=%1 unit_type=%2 posture=%3 decision_authority=%4 target_kind=%5 target_pos=%6 leader=%7 leader_pos=%8",
			typename.EnumToString(AICF_EGroupRole, slot.GetRole()), typename.EnumToString(AICF_EGroupUnitType, slot.GetUnitType()),
			slot.GetOperationalPosture(), typename.EnumToString(AICF_EStrategicDecisionAuthority, slot.GetDecisionAuthority()), typename.EnumToString(AICF_EOrderTargetKind, slot.GetTargetKind()),
			slot.GetTargetPosition(), Entity(leader), Position(leader));
		details += string.Format(" alive=%1 inside=%2 current_wp=%3 owned_wp=%4 queue_count=%5 wp_pos=%6",
			AICF_GroupRuntime.CountAliveAgents(group), AICF_GroupRuntime.CountAliveAgentsInAnyVehicle(group),
			Entity(current), Entity(slot.GetWaypoint()), queue.Count(), Position(current));
		SCR_AIGroupUtilityComponent utility = group.GetGroupUtilityComponent();
		if (utility) details += " group_action=" + Action(utility.GetCurrentAction()) + " combat_mode=" + typename.EnumToString(EAIGroupCombatMode, utility.GetCombatModeActual());
		Vehicle vehicle;
		if (trip)
		{
			details += string.Format(" operation_id=%1 phase=%2 reason=%3 phase_age_ms=%4 deadline_remaining_ms=%5",
				trip.GetOperationId(), AICF_Stage3Diagnostics.TripPhaseToString(trip.GetPhase()), trip.GetLastTransitionReason(),
				System.GetTickCount() - trip.GetPhaseStartedAtMs(), trip.GetAbsoluteDeadlineMs() - System.GetTickCount());
			if (trip.GetLease()) vehicle = trip.GetLease().GetVehicle();
			AICF_VehicleMovementState movement = trip.GetMovementState();
			details += string.Format(" route_generation=%1 route_endpoint=%2 route_wp=%3 mobility_recoveries=%4 crew_recoveries=%5",
				movement.GetRouteGeneration(), movement.GetRouteEndpoint(), Entity(movement.GetRouteWaypoint()),
				movement.GetMobilityRecoveryAttempts(), movement.GetCrewRecoveryAttempts());
		}
		float speed;
		if (vehicle && vehicle.GetPhysics()) speed = vehicle.GetPhysics().GetVelocity().Length();
		details += string.Format(" vehicle=%1 vehicle_pos=%2 speed_mps=%3", Entity(vehicle), Position(vehicle), speed);
		if (leader) details += string.Format(" leader_target_distance_m=%1", vector.DistanceXZ(leader.GetOrigin(), slot.GetTargetPosition()));
		if (vehicle) details += string.Format(" vehicle_target_distance_m=%1", vector.DistanceXZ(vehicle.GetOrigin(), slot.GetTargetPosition()));
		AICF_Stage3Diagnostics.Info("GROUP_TRACE", details);
		array<BaseCompartmentSlot> seats = {};
		if (vehicle) AICF_VehiclePreset.Collect(vehicle, seats);
		for (int position; position < slot.GetDeploymentSize(); position++)
			Member(slot, trip, seats, identity, position);
		// Deployment size описывает исходный spawn; платный набор добавляет живых
		// участников после него. Физический состав читаем отдельно, не выдавая
		// порядок native agents за постоянные позиции roster/preset.
		array<AIAgent> physicalAgents = {};
		group.GetAgents(physicalAgents);
		for (int physicalIndex; physicalIndex < physicalAgents.Count(); physicalIndex++)
			PhysicalMember(slot, group, physicalAgents[physicalIndex], identity, physicalIndex);
		NativeVehiclePath(slot, group, trip, identity);
	}

	protected static bool IsNativePathTripCurrent(AICF_GroupSlot slot, SCR_AIGroup group, AICF_TransportTrip trip)
	{
		if (!Replication.IsServer() || !slot || !slot.IsCombatReady() || slot.GetGroup() != group ||
			!AICF_VehicleBoardingMutationFence.IsAuthoritativeReplicatedEntity(group) ||
			!trip || !trip.IsValid() || trip.IsTerminal() || trip.GetPhase() != AICF_ETransportTripPhase.TRANSIT)
			return false;
		AICF_StrategicAssignmentSnapshot assignment = trip.GetAssignment();
		if (assignment.GetGroup() != group || assignment.GetSlotId() != slot.GetSlotId() ||
			assignment.GetGroupGeneration() != slot.GetSpawnGeneration() || !group.GetFaction() ||
			assignment.GetFactionKey() != group.GetFaction().GetFactionKey() ||
			assignment.GetAssignmentRevision() != slot.GetStrategicAssignmentRevision() ||
			assignment.GetStrategicIntentRevision() != slot.GetStrategicIntentRevision() ||
			assignment.GetTargetKind() != slot.GetTargetKind()) return false;
		if (assignment.GetTargetKind() == AICF_EOrderTargetKind.POSITION && assignment.GetTargetPosition() != slot.GetTargetPosition()) return false;
		if (assignment.GetTargetKind() == AICF_EOrderTargetKind.BASE && assignment.GetTargetBase() != slot.GetTargetBase()) return false;
		AICF_VehicleLease lease = trip.GetLease();
		if (!lease || lease.GetState() != AICF_EVehicleLeaseState.ACTIVE || !lease.HasPhysicalAsset() ||
			!lease.MatchesTripIdentity(trip.GetFactionKey(), slot.GetSlotId(), slot.GetSpawnGeneration(), trip.GetTripGeneration())) return false;
		Vehicle vehicle = lease.GetVehicle();
		if (!AICF_VehicleBoardingMutationFence.IsAuthoritativeReplicatedEntity(vehicle)) return false;
		RplComponent vehicleRpl = RplComponent.Cast(vehicle.FindComponent(RplComponent));
		if (!vehicleRpl || !lease.MatchesEntityIdentity(vehicle, vehicle.GetID(), vehicleRpl.Id().ToString())) return false;
		AICF_VehicleMovementState movement = trip.GetMovementState();
		AIWaypoint route = movement.GetRouteWaypoint();
		SCR_AIGroupUtilityComponent utility = group.GetGroupUtilityComponent();
		if (!route || !movement.IsRouteWaypointBound() || movement.GetRouteGeneration() <= 0 ||
			movement.GetAssignmentRevision() != assignment.GetAssignmentRevision() || group.GetCurrentWaypoint() != route ||
			!utility || utility.m_Owner != group || !utility.m_VehicleMgr) return false;
		array<AIWaypoint> queue = {};
		group.GetWaypoints(queue);
		if (!queue.Contains(route)) return false;
		SCR_AIMoveActivity move = SCR_AIMoveActivity.Cast(utility.GetCurrentAction());
		if (!move || move.m_Utility != utility || move.m_RelatedWaypoint != route ||
			move.GetRemoveAction() || move.GetActionIsSuspended()) return false;
		EAIActionState state = move.GetActionState();
		return state == EAIActionState.RUNNING || state == EAIActionState.EVALUATED;
	}

	protected static bool IsNativePathDriverCurrent(SCR_AIGroup group, Vehicle vehicle, ChimeraCharacter driver,
		AIAgent driverAgent, PilotCompartmentSlot pilot, AIBaseMovementComponent movementComponent, out string reason)
	{
		reason = "CURRENT";
		if (!group || !vehicle) { reason = "GROUP_OR_VEHICLE_MISSING"; return false; }
		if (!AICF_VehicleBoardingMutationFence.IsAuthoritativeAIEntity(driver)) { reason = "DRIVER_NOT_AUTHORITY_AI"; return false; }
		if (!SCR_AIDamageHandling.IsConscious(driver)) { reason = "DRIVER_NOT_CONSCIOUS"; return false; }
		if (!driver.GetAIControlComponent()) { reason = "DRIVER_CONTROL_MISSING"; return false; }
		if (!driverAgent) { reason = "DRIVER_AGENT_MISSING"; return false; }
		if (driver.GetAIControlComponent().GetControlAIAgent() != driverAgent) { reason = "DRIVER_CONTROL_AGENT_CHANGED"; return false; }
		if (driverAgent.GetControlledEntity() != driver) { reason = "DRIVER_AGENT_ACTOR_MISMATCH"; return false; }
		if (driverAgent.GetParentGroup() != group) { reason = "DRIVER_AGENT_GROUP_MISMATCH"; return false; }
		if (!pilot) { reason = "ACTUAL_PILOT_COMPARTMENT_MISSING"; return false; }
		if (pilot.GetVehicle() != vehicle) { reason = "PILOT_VEHICLE_MISMATCH"; return false; }
		if (pilot.GetOccupant() != driver) { reason = "PILOT_OCCUPANT_MISMATCH"; return false; }
		CompartmentAccessComponent access = driver.GetCompartmentAccessComponent();
		if (!access) { reason = "DRIVER_ACCESS_MISSING"; return false; }
		if (!driver.IsInVehicle()) { reason = "DRIVER_NOT_IN_VEHICLE"; return false; }
		if (!access.IsInCompartment()) { reason = "DRIVER_NOT_IN_COMPARTMENT"; return false; }
		if (access.IsGettingIn()) { reason = "DRIVER_GETTING_IN"; return false; }
		if (access.IsGettingOut()) { reason = "DRIVER_GETTING_OUT"; return false; }
		if (access.IsSwitchingSeatsAnim()) { reason = "DRIVER_SWITCHING_SEATS"; return false; }
		if (access.GetCompartment() != pilot) { reason = "DRIVER_ACTUAL_COMPARTMENT_CHANGED"; return false; }
		if (CompartmentAccessComponent.GetVehicleIn(driver) != vehicle) { reason = "DRIVER_LINKED_VEHICLE_MISMATCH"; return false; }
		array<AIAgent> agents = {};
		group.GetAgents(agents);
		if (!agents.Contains(driverAgent)) { reason = "DRIVER_AGENT_NOT_IN_GROUP"; return false; }
		SCR_AIGroupUtilityComponent utility = group.GetGroupUtilityComponent();
		if (!utility || !utility.m_VehicleMgr) { reason = "VEHICLE_MANAGER_MISSING"; return false; }
		SCR_AIGroupVehicle registered = utility.m_VehicleMgr.FindVehicle(vehicle);
		if (!registered || registered.GetEntity() != vehicle) { reason = "REGISTERED_VEHICLE_MISMATCH"; return false; }
		if (registered.GetDriver() != driver) { reason = "REGISTERED_DRIVER_CHANGED"; return false; }
		if (!movementComponent) { reason = "DRIVER_MOVEMENT_COMPONENT_MISSING"; return false; }
		if (driverAgent.GetMovementComponent() != movementComponent) { reason = "DRIVER_MOVEMENT_COMPONENT_CHANGED"; return false; }
		return true;
	}

	protected static void NativePathUnavailable(string identity, string reason, bool queried = false)
	{
		AICF_Stage3Diagnostics.Info("NATIVE_VEHICLE_PATH_TRACE", identity + " path_status=" + reason +
			string.Format(" queried=%1 native_point_count=-1 emitted_points=0 point_limit=%2 truncated=0 first=NONE last=NONE raw_native_order=1 remaining_distance_proven=0", queried, NATIVE_PATH_POINT_LIMIT));
	}

	protected static void NativeVehiclePath(AICF_GroupSlot slot, SCR_AIGroup group, AICF_TransportTrip trip, string identity)
	{
		if (!Replication.IsServer() || !slot || !group) return;
		identity += string.Format(" path_snapshot_ms=%1", System.GetTickCount());
		if (!IsNativePathTripCurrent(slot, group, trip)) { NativePathUnavailable(identity, "NO_CURRENT_NATIVE_ROUTE"); return; }
		AICF_VehicleLease lease = trip.GetLease();
		Vehicle vehicle = lease.GetVehicle();
		EntityID vehicleId = vehicle.GetID();
		string vehicleRpl = RplComponent.Cast(vehicle.FindComponent(RplComponent)).Id().ToString();
		int leaseGeneration = lease.GetLeaseGeneration();
		int vehicleGeneration = lease.GetVehicleGeneration();
		AICF_VehicleMovementState movement = trip.GetMovementState();
		AIWaypoint route = movement.GetRouteWaypoint();
		int routeGeneration = movement.GetRouteGeneration();
		int assignmentRevision = trip.GetAssignment().GetAssignmentRevision();
		SCR_AIGroupUtilityComponent utility = group.GetGroupUtilityComponent();
		SCR_AIMoveActivity move = SCR_AIMoveActivity.Cast(utility.GetCurrentAction());
		identity += string.Format(" operation_id=%1 trip_generation=%2 lease_generation=%3 vehicle_generation=%4 vehicle=%5 vehicle_rpl=%6 car_pos=%7",
			trip.GetOperationId(), trip.GetTripGeneration(), leaseGeneration, vehicleGeneration, Entity(vehicle), vehicleRpl, Position(vehicle));
		identity += string.Format(" route_generation=%1 route_waypoint=%2 route_endpoint=%3 assignment_revision=%4 intent_revision=%5 target_pos=%6 native_move=%7 native_move_waypoint=%8",
			routeGeneration, Entity(route), movement.GetRouteEndpoint(), assignmentRevision, slot.GetStrategicIntentRevision(), trip.GetAssignment().GetTargetPosition(), Action(move), Entity(move.m_RelatedWaypoint));
		SCR_AIGroupVehicle registered = utility.m_VehicleMgr.FindVehicle(vehicle);
		ChimeraCharacter driver;
		if (registered) driver = ChimeraCharacter.Cast(registered.GetDriver());
		AIAgent driverAgent;
		if (driver && driver.GetAIControlComponent()) driverAgent = driver.GetAIControlComponent().GetControlAIAgent();
		PilotCompartmentSlot pilot;
		if (driver && driver.GetCompartmentAccessComponent()) pilot = PilotCompartmentSlot.Cast(driver.GetCompartmentAccessComponent().GetCompartment());
		AIBaseMovementComponent movementComponent;
		if (driverAgent) movementComponent = driverAgent.GetMovementComponent();
		AICarMovementComponent carMovement = AICarMovementComponent.Cast(movementComponent);
		AIAgent componentAgent;
		string componentType = "NONE";
		string componentReference = "NONE";
		if (movementComponent)
		{
			componentAgent = movementComponent.GetAIAgent();
			componentType = movementComponent.Type().ToString();
			componentReference = movementComponent.ToString();
		}
		identity += string.Format(" driver_candidate=%1 driver_agent=%2 movement_component=%3 movement_component_ref=%4 native_associated_agent=%5 car_cast=%6 movement_agent_is_driver=%7",
			Entity(driver), Entity(driverAgent), componentType, componentReference, Entity(componentAgent), carMovement != null, componentAgent && componentAgent == driverAgent);
		if (driver && driver.GetAIControlComponent()) identity += string.Format(" driver_control_ref=%1 driver_control_associated_agent=%2 control_agent=%3",
			driver.GetAIControlComponent().ToString(), Entity(driver.GetAIControlComponent().GetAIAgent()), Entity(driver.GetAIControlComponent().GetControlAIAgent()));
		if (driverAgent) identity += string.Format(" driver_agent_controlled=%1 driver_agent_parent=%2", Entity(driverAgent.GetControlledEntity()), Entity(driverAgent.GetParentGroup()));
		if (componentAgent) identity += string.Format(" movement_agent_controlled=%1 movement_agent_parent=%2", Entity(componentAgent.GetControlledEntity()), Entity(componentAgent.GetParentGroup()));
		if (driver && driver.GetCompartmentAccessComponent())
		{
			CompartmentAccessComponent access = driver.GetCompartmentAccessComponent();
			string actualSeat = "NONE";
			if (access.GetCompartment()) actualSeat = AICF_VehiclePreset.SeatKey(access.GetCompartment());
			identity += string.Format(" actual_seat=%1 actual_pilot=%2 character_in_vehicle=%3 in_compartment=%4 getting_in=%5 getting_out=%6 switching_seats=%7 linked_vehicle=%8",
				actualSeat, pilot != null, driver.IsInVehicle(), access.IsInCompartment(), access.IsGettingIn(), access.IsGettingOut(), access.IsSwitchingSeatsAnim(), Entity(CompartmentAccessComponent.GetVehicleIn(driver)));
		}
		string driverReason;
		if (!IsNativePathDriverCurrent(group, vehicle, driver, driverAgent, pilot, movementComponent, driverReason)) { NativePathUnavailable(identity, driverReason); return; }
		EntityID driverId = driver.GetID();
		string driverRpl = RplComponent.Cast(driver.FindComponent(RplComponent)).Id().ToString();
		identity += string.Format(" driver=%1 driver_rpl=%2 pilot_seat=%3 source=DRIVER_MOVEMENT_COMPONENT",
			Entity(driver), driverRpl, AICF_VehiclePreset.SeatKey(pilot));
		array<vector> points = {};
		// API принадлежит AIBaseMovementComponent. Тип/native agent/car cast наблюдаем,
		// не навязывая native getter неописанный контракт owner == driverAgent.
		movementComponent.GetCurrentPath(points);
		// Native getter не даёт generation/index и не обещает remaining-distance.
		// Не публикуем corners под новой lease/route/driver identity после чтения.
		if (!IsNativePathTripCurrent(slot, group, trip) || trip.GetLease() != lease ||
			lease.GetLeaseGeneration() != leaseGeneration || lease.GetVehicleGeneration() != vehicleGeneration ||
			!lease.MatchesEntityIdentity(vehicle, vehicleId, vehicleRpl) || movement.GetRouteWaypoint() != route ||
			movement.GetRouteGeneration() != routeGeneration || trip.GetAssignment().GetAssignmentRevision() != assignmentRevision ||
			utility.GetCurrentAction() != move || !IsNativePathDriverCurrent(group, vehicle, driver, driverAgent, pilot, movementComponent, driverReason) ||
			movementComponent.GetAIAgent() != componentAgent ||
			driver.GetID() != driverId || RplComponent.Cast(driver.FindComponent(RplComponent)).Id().ToString() != driverRpl)
		{ NativePathUnavailable(identity + " post_read_driver_reason=" + driverReason, "CONTEXT_CHANGED_DURING_READ_DISCARDED", true); return; }
		int count = points.Count();
		int emitted = Math.Min(count, NATIVE_PATH_POINT_LIMIT);
		string first = "NONE";
		string last = "NONE";
		if (count > 0) { first = points[0].ToString(); last = points[count - 1].ToString(); }
		string status = "RAW_NATIVE_PATH";
		if (count == 0) status = "EMPTY_NATIVE_PATH";
		AICF_Stage3Diagnostics.Info("NATIVE_VEHICLE_PATH_TRACE", identity + string.Format(
			" path_status=%1 queried=1 native_point_count=%2 emitted_points=%3 point_limit=%4 truncated=%5 first=%6 last=%7 raw_native_order=1 remaining_distance_proven=0",
			status, count, emitted, NATIVE_PATH_POINT_LIMIT, count > emitted, first, last));
		for (int index; index < emitted; index++)
			AICF_Stage3Diagnostics.Info("NATIVE_VEHICLE_PATH_POINT", identity + string.Format(" native_index=%1 point=%2", index, points[index]));
	}

	protected static void PhysicalMember(AICF_GroupSlot slot, SCR_AIGroup group, AIAgent agent, string identity, int physicalIndex)
	{
		IEntity entity;
		if (agent) entity = agent.GetControlledEntity();
		int rosterPosition = -1;
		if (entity)
		{
			for (int position; position < AICF_Stage1Config.MAX_GROUP_SIZE; position++)
			{
				if (slot.GetRosterMember(position) != entity) continue;
				rosterPosition = position;
				break;
			}
		}
		string memberRpl = "NONE";
		RplComponent rpl;
		if (entity) rpl = RplComponent.Cast(entity.FindComponent(RplComponent));
		if (rpl) memberRpl = rpl.Id().ToString();
		ChimeraCharacter character = ChimeraCharacter.Cast(entity);
		string life = "NO_CHARACTER";
		if (character && character.GetCharacterController())
			life = typename.EnumToString(ECharacterLifeState, character.GetCharacterController().GetLifeState());
		AIGroup parent;
		if (agent) parent = agent.GetParentGroup();
		bool controlAgentExact;
		if (character && character.GetAIControlComponent())
			controlAgentExact = character.GetAIControlComponent().GetControlAIAgent() == agent;
		string details = identity + string.Format(" physical_index=%1 roster_position=%2 agent=%3 parent_group=%4 parent_group_exact=%5 control_agent_exact=%6 member=%7 member_rpl=%8",
			physicalIndex, rosterPosition, Entity(agent), Entity(parent), parent == group, controlAgentExact, Entity(entity), memberRpl);
		details += string.Format(" pos=%1 life=%2 alive=%3", Position(entity), life, AICF_GroupRuntime.IsAliveCharacter(entity));
		AIWaypoint waypoint = slot.GetWaypoint();
		if (entity)
		{
			details += string.Format(" target_distance_m=%1", vector.DistanceXZ(entity.GetOrigin(), slot.GetTargetPosition()));
			if (waypoint) details += string.Format(" waypoint=%1 waypoint_distance_m=%2 completion_radius_m=%3",
				Entity(waypoint), vector.DistanceXZ(entity.GetOrigin(), waypoint.GetOrigin()), waypoint.GetCompletionRadius());
		}
		if (character)
		{
			CompartmentAccessComponent access = character.GetCompartmentAccessComponent();
			details += string.Format(" linked_vehicle=%1 character_in_vehicle=%2", Entity(CompartmentAccessComponent.GetVehicleIn(character)), character.IsInVehicle());
			if (access) details += string.Format(" in_compartment=%1 getting_in=%2 getting_out=%3 switching_seats=%4",
				access.IsInCompartment(), access.IsGettingIn(), access.IsGettingOut(), access.IsSwitchingSeatsAnim());
		}
		SCR_AIUtilityComponent utility;
		if (agent) utility = SCR_AIUtilityComponent.Cast(agent.FindComponent(SCR_AIUtilityComponent));
		if (utility)
		{
			details += " selected_action=" + Action(utility.GetCurrentAction()) + " executed_action=" + Action(utility.GetExecutedAction());
			SCR_AIBehaviorBase behavior = SCR_AIBehaviorBase.Cast(utility.GetCurrentAction());
			SCR_AIActivityBase activity;
			if (behavior) activity = behavior.GetGroupActivityContext();
			if (activity)
			{
				SCR_AIGroupUtilityComponent groupUtility = group.GetGroupUtilityComponent();
				details += string.Format(" selected_group_activity=%1 activity_waypoint=%2 group_activity_current=%3",
					Action(activity), Entity(activity.m_RelatedWaypoint), groupUtility && groupUtility.GetCurrentAction() == activity);
			}
			SCR_AIDefendBehavior defend = SCR_AIDefendBehavior.Cast(behavior);
			if (defend && defend.m_RelatedWaypoint)
				details += " defend_waypoint=" + Entity(defend.m_RelatedWaypoint.m_Value);
		}
		AICF_Stage3Diagnostics.Info("PHYSICAL_MEMBER_TRACE", details);
	}

	protected static void Member(AICF_GroupSlot slot, AICF_TransportTrip trip, array<BaseCompartmentSlot> seats, string identity, int position)
	{
		IEntity entity = slot.GetRosterMember(position);
		ChimeraCharacter character = ChimeraCharacter.Cast(entity);
		string life = "NO_CHARACTER";
		if (character && character.GetCharacterController())
			life = typename.EnumToString(ECharacterLifeState, character.GetCharacterController().GetLifeState());
		string details = identity + string.Format(" position=%1 member=%2 pos=%3 life=%4",
			position, Entity(entity), Position(entity), life);
		if (!character) { AICF_Stage3Diagnostics.Info("MEMBER_TRACE", details); return; }
		IEntity linked = CompartmentAccessComponent.GetVehicleIn(character);
		CompartmentAccessComponent access = character.GetCompartmentAccessComponent();
		string actualSeat = "NONE";
		if (access && access.GetCompartment()) actualSeat = AICF_VehiclePreset.SeatKey(access.GetCompartment());
		details += string.Format(" linked_vehicle=%1 actual_seat=%2 character_in_vehicle=%3", Entity(linked), actualSeat, character.IsInVehicle());
		if (access) details += string.Format(" in_compartment=%1 getting_in=%2 getting_out=%3",
			access.IsInCompartment(), access.IsGettingIn(), access.IsGettingOut());
		AIAgent agent;
		if (character.GetAIControlComponent()) agent = character.GetAIControlComponent().GetControlAIAgent();
		SCR_AIUtilityComponent utility;
		if (agent) utility = SCR_AIUtilityComponent.Cast(agent.FindComponent(SCR_AIUtilityComponent));
		if (utility) details += " selected_action=" + Action(utility.GetCurrentAction()) + " executed_action=" + Action(utility.GetExecutedAction());
		if (trip && trip.GetVehiclePreset())
		{
			AICF_VehiclePreset preset = trip.GetVehiclePreset();
			for (int seat; seat < seats.Count() && seat < preset.m_aMembers.Count(); seat++)
			{
				if (preset.m_aMembers[seat] != position) continue;
				details += string.Format(" expected_seat=%1 occupant=%2 reserved_by_member=%3 get_in_locked=%4",
					AICF_VehiclePreset.SeatKey(seats[seat]), Entity(seats[seat].GetOccupant()), seats[seat].IsReservedBy(entity), seats[seat].IsGetInLockedFor(entity));
			}
			AICF_VehicleBoardingActionToken token;
			AICF_VehicleBoardingState boarding = trip.GetBoardingState();
			if (agent && boarding && boarding.GetTokens()) token = boarding.GetTokens().FindByAgent(agent);
			if (token) details += string.Format(" boarding_action=%1 boarding_age_ms=%2 boarding_queued=%3 tracked_action_current=%4 tracked_action_owned=%5",
				typename.EnumToString(EAIActionState, token.GetActionState()), System.GetTickCount() - token.GetIssuedAtMs(), token.WasPresetQueued(),
				token.IsTrackedActionCurrent(), token.IsTrackedActionOwnedByUtility());
		}
		AICF_Stage3Diagnostics.Info("MEMBER_TRACE", details);
	}
}
