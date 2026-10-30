// Дополнение client probe: реальный RPC, отдельный roster10, physical return и driver-only trip.
modded class AICF_VehicleCrewReturn
{
	int AICF_CrewReturnCount() { return m_aReturns.Count(); }
	void AICF_CrewTracePending(int fixtureDeadlineRemainingMs)
	{
		foreach (AICF_ReturningSoldier returning : m_aReturns)
		{
			IEntity member = returning.m_Member;
			SCR_AIGroup group = returning.m_Group;
			ChimeraCharacter character = ChimeraCharacter.Cast(member);
			AIAgent agent;
			CompartmentAccessComponent access;
			if (character)
			{
				access = character.GetCompartmentAccessComponent();
				if (character.GetAIControlComponent()) agent = character.GetAIControlComponent().GetControlAIAgent();
			}
			SCR_AIUtilityComponent utility;
			if (agent) utility = SCR_AIUtilityComponent.Cast(agent.FindComponent(SCR_AIUtilityComponent));
			string details = string.Format("member=%1 member_rpl=%2 alive=%3 conscious=%4 group=%5 agent=%6 current_owner_safe=%7 fixture_deadline_remaining_ms=%8",
				AICF_GroupTrace.Entity(member), AICF_VehicleBehaviorBinding.ComponentRpl(member), AICF_GroupRuntime.IsAliveCharacter(member),
				member && SCR_AIDamageHandling.IsConscious(member), AICF_GroupTrace.Entity(group), AICF_GroupTrace.Entity(agent), returning.Current(), fixtureDeadlineRemainingMs);
			if (member) details += string.Format(" position=%1", member.GetOrigin());
			if (agent) details += string.Format(" parent_group=%1 control_exact=%2", AICF_GroupTrace.Entity(agent.GetParentGroup()), agent.GetControlledEntity() == member);
			if (member && returning.m_Base && returning.m_Base.GetSpawnPoint()) details += string.Format(" desired_hq=%1 distance_m=%2", returning.m_Base.GetSpawnPoint().GetOrigin(), vector.Distance(member.GetOrigin(), returning.m_Base.GetSpawnPoint().GetOrigin()));
			if (group) details += string.Format(" current_wp=%1 owned_wp=%2", AICF_GroupTrace.Entity(group.GetCurrentWaypoint()), AICF_GroupTrace.Entity(returning.m_Waypoint));
			if (group && group.GetGroupUtilityComponent()) details += string.Format(" group_action=%1", AICF_GroupTrace.Action(group.GetGroupUtilityComponent().GetCurrentAction()));
			if (utility) details += string.Format(" current_behavior=%1", AICF_GroupTrace.Action(utility.GetCurrentBehavior()));
			SCR_DamageManagerComponent damage;
			if (member) damage = SCR_DamageManagerComponent.GetDamageManager(member);
			if (damage && damage.GetDefaultHitZone()) details += string.Format(" health_default_scaled=%1 health_state=%2", damage.GetDefaultHitZone().GetHealthScaled(), damage.GetDefaultHitZone().GetDamageState());
			if (access) details += string.Format(" linked_vehicle=%1 access_vehicle=%2 seat=%3 getting_in=%4 getting_out=%5 switching=%6", AICF_GroupTrace.Entity(CompartmentAccessComponent.GetVehicleIn(member)), AICF_GroupTrace.Entity(access.GetVehicleCompartmentManagerOwner()), access.GetCompartment(), access.IsGettingIn(), access.IsGettingOut(), access.IsSwitchingSeatsAnim());
			details += string.Format(" last_order_at_ms=%1 last_order_age_ms=%2 progress_deadline=UNKNOWN native_reads_only=1", returning.m_iLastOrder, System.GetTickCount() - returning.m_iLastOrder);
			Print("[AICF][CREW_PENDING_READBACK] " + details);
		}
	}
	override bool Commit(AICF_VehicleCrewPlan plan, SCR_CampaignFaction faction, AICF_VehiclePreset preset, out string reason)
	{
		IEntity kept = plan.m_Slot.GetRosterMember(plan.m_aKept[0]);
		string before, after;
		bool captured = AICF_LoadoutInventory.Capture(kept, before);
		bool accepted = super.Commit(plan, faction, preset, reason);
		if (accepted) Print(string.Format("[AICF][CREW_INVENTORY] unchanged=%1", captured && AICF_LoadoutInventory.Capture(kept, after) && before == after));
		return accepted;
	}
}

modded class AICF_MatchController
{
	protected ref AICF_FactionState m_CrewState;
	protected ref AICF_VehicleCoordinator m_CrewVehicles;
	protected SCR_AIGroup m_CrewGroup;
	protected IEntity m_CrewKept;
	protected int m_iCrewStarted;
	protected int m_iCrewChecks;
	protected int m_iCrewFailures;
	protected bool m_bCrewReduced;
	protected bool m_bCrewDone;
	protected int m_iCrewNextTrace;

	protected void AICF_CrewCheck(string rule, bool passed)
	{
		m_iCrewChecks++;
		if (!passed) m_iCrewFailures++;
		Print(string.Format("[AICF][CREW_CHECK] rule=%1 passed=%2", rule, passed));
	}

	override bool ResolveVehiclePresetContext(SCR_PlayerController player, int slotId, out AICF_GroupSlot slot, out SCR_CampaignFaction faction)
	{
		if (!super.ResolveVehiclePresetContext(player, slotId, slot, faction)) return false;
		string flag;
		if (!System.GetCLIParam("aicfVehicleCrewReturnProbe", flag)) return true;
		if (!m_CrewState || !m_CrewState.GetSlot(0).IsCombatReady() || slotId != 0 || faction != m_USSRFaction) return false;
		slot = m_CrewState.GetSlot(0);
		return true;
	}

	override protected void Update()
	{
		super.Update();
		string flag;
		if (!Replication.IsServer() || !System.GetCLIParam("aicfVehicleCrewReturnProbe", flag) || !m_bRosterReady || m_bCrewDone) return;
		if (!m_iCrewStarted) m_iCrewStarted = System.GetTickCount();
		if (System.GetTickCount() - m_iCrewStarted > 300000) { AICF_CrewCheck("TIMEOUT", false); AICF_CrewClose(); return; }
		if (!m_CrewState)
		{
			m_CrewState = new AICF_FactionState(m_USSRFaction.GetFactionKey(), m_Config);
			AICF_GroupSlot fresh = m_CrewState.GetSlot(0);
			fresh.SetUnitType(AICF_EGroupUnitType.MOTORIZED_LIGHT);
			fresh.SetDesiredSize(10);
			m_CrewGroup = m_GroupSpawner.SpawnGroup(m_USSRFaction, m_USSRFaction.GetMainBase(), 0, 10, false);
			AICF_CrewCheck("GROUP10_CREATED", m_CrewGroup != null);
			if (!m_CrewGroup) { AICF_CrewClose(); return; }
			m_CrewGroup.SetLifecyclePolicy(SCR_EAIGroupLifecyclePolicy.Manual);
			m_CrewGroup.SetDeleteWhenEmpty(false);
			fresh.BeginInitialSpawn();
			fresh.BindSpawnedGroup(m_CrewGroup);
			m_GroupSpawner.BeginRosterSpawn(m_CrewGroup, 10);
			m_CrewVehicles = new AICF_VehicleCoordinator(m_Stage3Config, m_Campaign, m_ConflictAdapter, m_OrderPlanner, m_GroupCohesionPolicy, m_ObjectiveGraph, m_TargetSelector, m_ContentProfile);
			return;
		}
		AICF_GroupSlot slot = m_CrewState.GetSlot(0);
		if (!slot.IsCombatReady())
		{
			if (m_CrewGroup.GetAgentsCount() != 10) return;
			slot.MarkReady();
			slot.RecordDeploymentMembers();
			m_CrewKept = slot.GetRosterMember(9);
			AICF_CrewCheck("TEN_TRACKED_NONPREFIX", m_CrewKept && slot.GetRosterMember(0));
			// Начальная сцена fixture: бойцы стоят в 40m от штатного spawn point.
			AIPathfindingComponent path = AIPathfindingComponent.Cast(m_CrewGroup.FindComponent(AIPathfindingComponent));
			vector origin = m_USSRFaction.GetMainBase().GetSpawnPoint().GetOrigin();
			path.GetNavmeshComponent().LoadTileIn(origin + "40 0 0");
			vector position;
			bool located = path.GetClosestPositionOnNavmesh(origin + "40 0 0", "12 5 12", position);
			AICF_CrewCheck("RETURN_ROUTE_START_OUTSIDE_BASE", located && vector.DistanceSqXZ(origin, position) > 225);
			if (!located) { AICF_CrewClose(); return; }
			for (int i; i < 10; i++) { slot.GetRosterMember(i).SetOrigin(position + Vector(0, 0, i * 0.7)); slot.GetRosterMember(i).Update(); }
			return;
		}
		if (!slot.HasVehiclePreset()) return;
		if (!m_bCrewReduced)
		{
			m_bCrewReduced = true;
			AICF_CrewCheck("ONLY_ASSIGNED_MEMBER_RETAINED", slot.GetDesiredSize() == 1 && m_CrewGroup.GetAgentsCount() == 1 && slot.GetRosterMember(0) == m_CrewKept && !slot.GetRosterMember(9));
			AICF_CrewCheck("ROLE_AND_TRACKING_REMAPPED", slot.GetEquipmentSourceIndex(0) == 9 && m_CrewGroup.AICF_GetSpawnMember(0) == m_CrewKept);
			AICF_CrewCheck("NINE_MANAGED_RETURNING", m_VehicleCrewReturn.CountAgents() == 9 && m_VehicleCrewReturn.AICF_CrewReturnCount() == 9);
			m_OrderPlanner.AssignPlayerPointOrder(slot, m_USSRFaction, m_CrewKept.GetOrigin() + "400 0 0", false);
		}
		m_CrewVehicles.Update(null, null, m_CrewState, m_USSRFaction, m_iStrategicBaseRevision, true);
		if (System.GetTickCount() >= m_iCrewNextTrace)
		{
			m_iCrewNextTrace = System.GetTickCount() + 10000;
			m_VehicleCrewReturn.AICF_CrewTracePending(300000 - (System.GetTickCount() - m_iCrewStarted));
		}
		AICF_TransportTrip trip = m_CrewVehicles.GetTrip(slot);
		Vehicle vehicle;
		if (trip && trip.GetLease()) vehicle = trip.GetLease().GetVehicle();
		if (vehicle && m_VehicleCrewReturn.AICF_CrewReturnCount() == 0 && CompartmentAccessComponent.GetVehicleIn(m_CrewKept) == vehicle)
		{
			array<BaseCompartmentSlot> seats = {};
			AICF_VehiclePreset.Collect(vehicle, seats);
			int occupants;
			bool driver;
			foreach (BaseCompartmentSlot seat : seats)
			{
				if (seat.GetOccupant()) occupants++;
				if (seat.GetType() == ECompartmentType.PILOT && seat.GetOccupant() == m_CrewKept) driver = true;
			}
			AICF_CrewCheck("UAZ_SINGLE_EXACT_DRIVER", occupants == 1 && driver && vehicle.GetPrefabData().GetPrefabName().Contains("UAZ469"));
			AICF_CrewCheck("RETURN_FINISHED_BEFORE_DONE", m_VehicleCrewReturn.CountAgents() == 0);
			AICF_CrewClose();
		}
	}

	protected void AICF_CrewClose()
	{
		m_bCrewDone = true;
		if (m_CrewVehicles) m_CrewVehicles.StopWithFactionContexts(true, null, null, m_CrewState, m_USSRFaction);
		m_CrewVehicles = null;
		if (m_CrewGroup)
		{
			m_OrderPlanner.ClearOrder(m_CrewState.GetSlot(0));
			m_CrewGroup.DespawnMembers();
			RplComponent.DeleteRplEntity(m_CrewGroup, false);
		}
		Print(string.Format("[AICF][CREW_DONE] checks=%1 failures=%2", m_iCrewChecks, m_iCrewFailures));
		SCR_GameModeCampaign.Cast(m_Campaign).AICF_VehicleProbeScheduleServerClose();
	}
}
