// Только чтение authority-состояния. Никаких actions, reservations или timers.
class AICF_GroupTraceCursor
{
	string m_sKey;
	int m_iNextMs;
	int m_iSequence;
}

class AICF_GroupTrace
{
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
