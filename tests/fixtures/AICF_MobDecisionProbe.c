// TEST ONLY. Single-delegate observation of existing authority MOB decisions.
// Snapshots are sequential BEFORE/AFTER reads, never an atomic gameplay proof.
class AICF_MobDecisionSnapshot
{
	ref array<string> m_Lines = {};
	string m_sKey;
}

class AICF_MobDecisionFrame
{
	AICF_GroupSlot m_Slot;
	SCR_AIGroup m_Group;
	SCR_CampaignMilitaryBaseComponent m_MainBase;
	BaseWorld m_World;
	RplComponent m_Rpl;
	string m_sRpl;
	IEntity m_MobOwner;
	EntityID m_MobId;
	AIWaypoint m_Waypoint;
	EntityID m_GroupId;
	int m_iGeneration;
	int m_iAssignment;
	int m_iIntent;
	int m_iSerial;
	ref AICF_MobDecisionSnapshot m_Before;

	bool IsCurrent()
	{
		if (!GetGame() || !m_World || GetGame().GetWorld() != m_World) return false;
		if (!m_Slot || !m_Group || m_Slot.GetGroup() != m_Group || m_Group.GetID() != m_GroupId) return false;
		if (m_Group.GetWorld() != m_World || m_World.FindEntityByID(m_GroupId) != m_Group) return false;
		if (!m_Rpl || m_Group.FindComponent(RplComponent) != m_Rpl || !m_Rpl.Id().IsValid() || m_Rpl.Id().ToString() != m_sRpl) return false;
		if (!m_MainBase || m_MainBase.GetOwner() != m_MobOwner || !m_MobOwner || m_MobOwner.GetID() != m_MobId) return false;
		return m_Slot.GetSpawnGeneration() == m_iGeneration && m_Slot.GetStrategicAssignmentRevision() == m_iAssignment &&
			m_Slot.GetStrategicIntentRevision() == m_iIntent && m_Slot.GetWaypoint() == m_Waypoint;
	}
}

class AICF_MobDecisionProbe
{
	static const int MAX_RECORDS = 32;
	static const int MAX_MEMBERS = 32;
	static ref AICF_MobDecisionFrame s_Frame;
	static ref array<string> s_Keys = {};
	static int s_iSerial;
	static bool s_bOverflow;
	static bool s_bScopeLoss;

	static bool Enabled()
	{
		string value;
		return GetGame() && Replication.IsServer() && System.GetCLIParam("aicfMobDecisionProbe", value) && value == "1";
	}

	static AICF_MobDecisionFrame Begin(AICF_GroupSlot slot, SCR_CampaignMilitaryBaseComponent mainBase)
	{
		if (!Enabled() || !slot) return null;
		if (slot.GetDecisionAuthority() != AICF_EStrategicDecisionAuthority.PLAYER_COMMAND ||
			slot.GetTargetKind() != AICF_EOrderTargetKind.POSITION) return null;
		if (s_Keys.Count() >= MAX_RECORDS)
		{
			if (!s_bOverflow) { s_bOverflow = true; Print("[AICF][MOB_DECISION] event=OVERFLOW record_limit=32 observations_incomplete=1 observation_only=1"); }
			return null;
		}
		AICF_MobDecisionFrame frame = new AICF_MobDecisionFrame();
		frame.m_Slot = slot; frame.m_Group = slot.GetGroup(); frame.m_MainBase = mainBase; frame.m_Waypoint = slot.GetWaypoint();
		frame.m_World = GetGame().GetWorld();
		if (frame.m_Group)
		{
			frame.m_GroupId = frame.m_Group.GetID();
			frame.m_Rpl = RplComponent.Cast(frame.m_Group.FindComponent(RplComponent));
			if (frame.m_Rpl) frame.m_sRpl = frame.m_Rpl.Id().ToString();
		}
		if (mainBase) frame.m_MobOwner = mainBase.GetOwner();
		if (frame.m_MobOwner) frame.m_MobId = frame.m_MobOwner.GetID();
		frame.m_iGeneration = slot.GetSpawnGeneration(); frame.m_iAssignment = slot.GetStrategicAssignmentRevision();
		frame.m_iIntent = slot.GetStrategicIntentRevision(); frame.m_iSerial = ++s_iSerial;
		s_Frame = frame;
		frame.m_Before = Read(frame);
		return frame;
	}

	static int Cause(AIActionBase action)
	{
		SCR_AIActionBase nativeAction = SCR_AIActionBase.Cast(action);
		if (!nativeAction) return -1;
		return nativeAction.GetCause();
	}

	static AICF_MobDecisionSnapshot Read(AICF_MobDecisionFrame frame)
	{
		AICF_MobDecisionSnapshot snap = new AICF_MobDecisionSnapshot();
		if (!frame || !frame.m_Slot) { snap.m_Lines.Insert("snapshot_incomplete=1 slot_present=0"); return snap; }
		AICF_GroupSlot slot = frame.m_Slot;
		SCR_AIGroup group = frame.m_Group;
		Faction faction;
		string factionKey = "UNKNOWN";
		if (group) faction = group.GetFaction();
		if (faction) factionKey = faction.GetFactionKey();
		string identity = string.Format("numeric_slot=%1 group=%2 generation=%3 assignment=%4 intent=%5", slot.GetSlotId(),
			AICF_GroupTrace.Entity(group), frame.m_iGeneration, frame.m_iAssignment, frame.m_iIntent);
		identity += " faction=" + factionKey;
		string flags = string.Format("pending=%1 failed=%2 approach=%3 combat_ready=%4 player_intent=%5 authority=%6",
			slot.HasPendingOrderRecovery(), slot.HasFailedMovement(), slot.IsApproachRouteWaypoint(), slot.IsCombatReady(),
			slot.HasPlayerStrategicIntent(), slot.GetDecisionAuthority());
		snap.m_Lines.Insert(identity + " " + flags + string.Format(" captured_tick=%1 non_atomic=1", System.GetTickCount()));
		snap.m_sKey = identity + "/" + flags;
		snap.m_Lines.Insert(string.Format("target_kind=%1 player_target_kind=%2 target=%3 player_target=%4 posture=%5 intent_posture=%6",
			slot.GetTargetKind(), slot.GetPlayerStrategicIntentTargetKind(), slot.GetTargetPosition(),
			slot.GetPlayerStrategicIntentTargetPosition(), slot.GetOperationalPosture(), slot.GetStrategicIntentPosture()));
		IEntity mobOwner;
		if (frame.m_MainBase) mobOwner = frame.m_MainBase.GetOwner();
		snap.m_Lines.Insert(string.Format("mob_owner=%1 mob_origin=%2 sampled_main_base=%3 owned_waypoint=%4",
			AICF_GroupTrace.Entity(mobOwner), AICF_GroupTrace.Position(mobOwner), frame.m_MainBase != null, AICF_GroupTrace.Entity(slot.GetWaypoint())));
		snap.m_sKey += "/" + AICF_GroupTrace.Entity(mobOwner) + "/" + AICF_GroupTrace.Position(mobOwner) + "/" + slot.GetTargetPosition().ToString();
		if (!group) { snap.m_Lines.Insert("snapshot_incomplete=1 group_present=0"); return snap; }
		AIWaypoint current = group.GetCurrentWaypoint();
		AIWaypoint ownedWaypoint = slot.GetWaypoint();
		array<AIWaypoint> queue = {}; group.GetWaypoints(queue);
		float radius = -1;
		if (ownedWaypoint) radius = ownedWaypoint.GetCompletionRadius();
		snap.m_Lines.Insert(string.Format("current_waypoint=%1 owned_waypoint=%2 current_exact=%3 queued=%4 queue_count=%5 waypoint_pos=%6 completion_radius_m=%7",
			AICF_GroupTrace.Entity(current), AICF_GroupTrace.Entity(ownedWaypoint), current == ownedWaypoint && ownedWaypoint != null,
			queue.Contains(ownedWaypoint), queue.Count(), AICF_GroupTrace.Position(ownedWaypoint), radius));
		snap.m_sKey += "/" + AICF_GroupTrace.Entity(current) + "/" + AICF_GroupTrace.Entity(ownedWaypoint);
		SCR_AIGroupUtilityComponent groupUtility = group.GetGroupUtilityComponent();
		if (groupUtility)
		{
			AIActionBase selected = groupUtility.GetCurrentAction();
			AIActionBase executed = groupUtility.GetExecutedAction();
			SCR_AIActivityBase activity = SCR_AIActivityBase.Cast(executed);
			AIWaypoint related;
			if (activity) related = activity.m_RelatedWaypoint;
			string actions = string.Format("group_selected=%1 group_executed=%2 same_action=%3 executed_related=%4 related_exact=%5 group_threat_sample=%6",
				AICF_GroupTrace.Action(selected), AICF_GroupTrace.Action(executed), selected == executed,
				AICF_GroupTrace.Entity(related), related == ownedWaypoint && ownedWaypoint != null, groupUtility.GetThreatMeasure());
			snap.m_Lines.Insert(actions); snap.m_sKey += "/" + AICF_GroupTrace.Action(selected) + "/" + AICF_GroupTrace.Action(executed);
		}
		array<AIAgent> agents = {}; group.GetAgents(agents);
		IEntity leader = AICF_GroupRuntime.ResolveAliveLeader(group);
		if (leader && ownedWaypoint) snap.m_Lines.Insert(string.Format("sampled_leader=%1 leader_waypoint_distance_m=%2 formation_result=NOT_REEVALUATED",
			AICF_GroupTrace.Entity(leader), vector.DistanceXZ(leader.GetOrigin(), ownedWaypoint.GetOrigin())));
		snap.m_Lines.Insert(string.Format("roster_count=%1 roster_examined=%2 roster_complete=%3 formation_result=NOT_REEVALUATED sector_flags=UNOBSERVED",
			agents.Count(), Math.Min(agents.Count(), MAX_MEMBERS), agents.Count() <= MAX_MEMBERS));
		for (int i; i < agents.Count() && i < MAX_MEMBERS; i++)
		{
			AIAgent agent = agents[i];
			IEntity actor;
			if (agent) actor = agent.GetControlledEntity();
			string member = string.Format("member_index=%1 agent=%2 actor=%3 alive=%4", i, AICF_GroupTrace.Entity(agent),
				AICF_GroupTrace.Entity(actor), AICF_GroupRuntime.IsAliveCharacter(actor));
			if (agent) member += string.Format(" parent_exact=%1", agent.GetParentGroup() == group);
			ChimeraCharacter character = ChimeraCharacter.Cast(actor);
			if (character)
			{
				CharacterControllerComponent controller = character.GetCharacterController();
				CompartmentAccessComponent access = character.GetCompartmentAccessComponent();
				member += string.Format(" controller_present=%1 access_present=%2", controller != null, access != null);
				if (access) member += string.Format(" in_compartment=%1 getting_in=%2 getting_out=%3 switching_seats=%4",
					access.IsInCompartment(), access.IsGettingIn(), access.IsGettingOut(), access.IsSwitchingSeatsAnim());
			}
			if (actor)
			{
				member += " position=" + AICF_GroupTrace.Position(actor);
				if (ownedWaypoint) member += string.Format(" waypoint_distance_m=%1 completion_radius_m=%2", vector.DistanceXZ(actor.GetOrigin(), ownedWaypoint.GetOrigin()), radius);
			}
			SCR_AIUtilityComponent utility;
			if (agent) utility = SCR_AIUtilityComponent.Cast(agent.FindComponent(SCR_AIUtilityComponent));
			if (utility)
			{
				AIActionBase selectedMember = utility.GetCurrentAction();
				AIActionBase executedMember = utility.GetExecutedAction();
				string memberActions = string.Format(" selected=%1 executed=%2 selected_cause=%3 executed_cause=%4",
					AICF_GroupTrace.Action(selectedMember), AICF_GroupTrace.Action(executedMember), Cause(selectedMember), Cause(executedMember));
				member += memberActions; snap.m_sKey += "/" + AICF_GroupTrace.Entity(actor) + memberActions;
			}
			snap.m_Lines.Insert(member);
		}
		return snap;
	}

	static bool Want(string key)
	{
		if (s_Keys.Contains(key)) return false;
		if (s_Keys.Count() >= MAX_RECORDS)
		{
			if (!s_bOverflow)
			{
				s_bOverflow = true;
				Print("[AICF][MOB_DECISION] event=OVERFLOW record_limit=32 observations_incomplete=1 observation_only=1");
			}
			return false;
		}
		s_Keys.Insert(key); return true;
	}

	static void Write(AICF_MobDecisionFrame frame, string stage, string actual, string inputs)
	{
		if (!Enabled() || !frame) return;
		if (s_Frame != frame) { ScopeLoss(); return; }
		AICF_MobDecisionSnapshot after = Read(frame);
		if (s_Frame != frame) { ScopeLoss(); return; }
		bool scopeCurrent = frame.IsCurrent();
		string key = stage + "/" + actual + "/" + inputs + "/" + frame.m_Before.m_sKey + "/" + after.m_sKey;
		if (s_Frame != frame) { ScopeLoss(); return; }
		if (!Want(key)) return;
		string prefix = string.Format("[AICF][MOB_DECISION] record=%1 serial=%2 stage=%3 ", s_Keys.Count(), frame.m_iSerial, stage);
		Print(prefix + string.Format("event=DECISION actual=%1 scope_current_after=%2 observation_only=1 non_atomic=1 ", actual, scopeCurrent) + inputs);
		foreach (string beforeLine : frame.m_Before.m_Lines) Print(prefix + "event=SNAPSHOT phase=BEFORE_PARENT " + beforeLine);
		foreach (string afterLine : after.m_Lines) Print(prefix + "event=SNAPSHOT phase=AFTER " + afterLine);
	}

	static void ScopeLoss()
	{
		if (!s_bScopeLoss) { s_bScopeLoss = true; Print("[AICF][MOB_DECISION] event=SCOPE_LOSS observations_incomplete=1 successor_untouched=1"); }
	}

	static void End(AICF_MobDecisionFrame frame, AICF_MobDecisionFrame previous)
	{
		if (frame && s_Frame == frame) s_Frame = previous;
	}

	static void Stop() { s_Frame = null; s_Keys.Clear(); s_iSerial = 0; s_bOverflow = false; s_bScopeLoss = false; }
}

modded class AICF_MatchController
{
	override protected string ResolveAllowedIdleReason(AICF_GroupSlot slot, SCR_CampaignMilitaryBaseComponent mainBase, bool vehicleLifecycle, bool safeVehicleSpawnWait)
	{
		AICF_MobDecisionFrame previous = AICF_MobDecisionProbe.s_Frame;
		AICF_MobDecisionFrame frame = AICF_MobDecisionProbe.Begin(slot, mainBase);
		string actual = super.ResolveAllowedIdleReason(slot, mainBase, vehicleLifecycle, safeVehicleSpawnWait);
		if (actual == "NONE")
			AICF_MobDecisionProbe.Write(frame, "ALLOWED_IDLE", actual, string.Format("input_vehicle_lifecycle=%1 input_safe_spawn_wait=%2", vehicleLifecycle, safeVehicleSpawnWait));
		AICF_MobDecisionProbe.End(frame, previous);
		return actual;
	}

	override protected bool TryApplyHiddenMobEgressRecovery(AICF_GroupSlot slot, SCR_CampaignFaction faction, SCR_CampaignMilitaryBaseComponent mainBase,
		out vector recoveryDestination, out float nearestPlayerMeters, out int relocatedMembers, out string playerClearance,
		out string losClearance, out string combatClearance, out string rejectionReason)
	{
		AICF_MobDecisionFrame previous = AICF_MobDecisionProbe.s_Frame;
		AICF_MobDecisionFrame frame = AICF_MobDecisionProbe.Begin(slot, mainBase);
		bool actual = super.TryApplyHiddenMobEgressRecovery(slot, faction, mainBase, recoveryDestination, nearestPlayerMeters,
			relocatedMembers, playerClearance, losClearance, combatClearance, rejectionReason);
		string outputs = string.Format("destination_return=%1 nearest_player_return=%2 relocated_return=%3 player_clearance_return=%4 los_clearance_return=%5 combat_clearance_return=%6 rejection_return=%7",
			recoveryDestination, nearestPlayerMeters, relocatedMembers, playerClearance, losClearance, combatClearance, rejectionReason);
		AICF_MobDecisionProbe.Write(frame, "HIDDEN_RECOVERY", actual.ToString(), outputs);
		AICF_MobDecisionProbe.End(frame, previous);
		return actual;
	}

	override protected bool TryFindDistinctMobEgressDestination(BaseWorld world, vector memberOrigin, vector targetPosition, vector mobOrigin,
		array<vector> reservedDestinations, out vector destination)
	{
		AICF_MobDecisionFrame frame = AICF_MobDecisionProbe.s_Frame;
		bool actual = super.TryFindDistinctMobEgressDestination(world, memberOrigin, targetPosition, mobOrigin, reservedDestinations, destination);
		string inputs = string.Format("input_member_origin=%1 input_target=%2 input_mob_origin=%3 destination_return=%4 candidate_rejections=UNOBSERVED extra_queries=0",
			memberOrigin, targetPosition, mobOrigin, destination);
		AICF_MobDecisionProbe.Write(frame, "DISTINCT_DESTINATION", actual.ToString(), inputs);
		return actual;
	}
}

modded class ArmaReforgerScripted
{
	override void OnGameEnd()
	{
		AICF_MobDecisionProbe.Stop();
		super.OnGameEnd();
	}
}
