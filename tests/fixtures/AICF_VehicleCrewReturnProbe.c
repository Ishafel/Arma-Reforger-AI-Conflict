// Дополнение client probe: реальный RPC, canonical roster10, physical return и driver-only trip.
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
	protected SCR_AIGroup m_CrewGroup;
	protected IEntity m_CrewKept;
	protected int m_iCrewStarted;
	protected int m_iCrewChecks;
	protected int m_iCrewFailures;
	protected bool m_bCrewReduced;
	protected bool m_bCrewDone;
	protected bool m_bCrewPrepared;
	protected bool m_bCrewSpawnProfileRestored;
	protected int m_iCrewNextTrace;

	protected void AICF_CrewCheck(string rule, bool passed)
	{
		m_iCrewChecks++;
		if (!passed) m_iCrewFailures++;
		Print(string.Format("[AICF][CREW_CHECK] rule=%1 passed=%2", rule, passed));
	}

	override protected bool SpawnInitialRoster(AICF_FactionState factionState, SCR_CampaignFaction faction)
	{
		string flag;
		if (Replication.IsServer() && System.GetCLIParam("aicfVehicleCrewReturnProbe", flag) &&
			factionState && factionState == m_USSRState && faction && faction == m_USSRFaction)
		{
			AICF_GroupSlot slot = factionState.GetSlot(0);
			if (!slot || slot.GetState() != AICF_EGroupSlotState.EMPTY || slot.GetGroup()) return false;
			// Только исходные параметры сценария; super владеет generation, quota bind,
			// actual roster request и readiness. MOTORIZED_LIGHT разворачивает desired10;
			// INFANTRY разворачивает только MIN1 и здесь не подходит.
			slot.SetUnitType(AICF_EGroupUnitType.MOTORIZED_LIGHT);
			slot.SetDesiredSize(10);
		}
		return super.SpawnInitialRoster(factionState, faction);
	}

	override protected void ProcessFaction(AICF_FactionState factionState, SCR_CampaignFaction faction)
	{
		super.ProcessFaction(factionState, faction);
		string flag;
		if (!Replication.IsServer() || !System.GetCLIParam("aicfVehicleCrewReturnProbe", flag) ||
			m_bStopped || m_bCrewDone) return;
		if (!factionState || factionState != m_USSRState || !faction || faction != m_USSRFaction) return;
		AICF_GroupSlot slot = factionState.GetSlot(0);
		if (m_bCrewSpawnProfileRestored)
		{
			if (slot && slot.IsCombatReady()) AICF_CrewPrepareReducedOrder(slot);
			return;
		}
		if (!slot || !slot.IsCombatReady() || slot.IsReplacementDeployment() || slot.GetSpawnGeneration() != 1) return;
		if (slot.GetDesiredSize() != 10 || slot.GetUnitType() != AICF_EGroupUnitType.MOTORIZED_LIGHT) return;
		SCR_AIGroup group = slot.GetGroup();
		if (!group || group.GetAgentsCount() != 10 || !slot.GetRosterMember(9)) return;
		if (m_bStopped || m_bCrewDone || m_bCrewSpawnProfileRestored || factionState != m_USSRState || faction != m_USSRFaction) return;
		if (factionState.GetSlot(0) != slot || slot.GetGroup() != group || !slot.IsCombatReady() || slot.GetSpawnGeneration() != 1) return;
		if (slot.IsReplacementDeployment() || slot.GetDesiredSize() != 10 || slot.GetUnitType() != AICF_EGroupUnitType.MOTORIZED_LIGHT) return;
		// ProcessFaction уже выполнил native READY/CompleteReadyDeployment. Внешний
		// root.Update ещё не дошёл до coordinator: никакой automatic trip не начат.
		slot.SetUnitType(AICF_EGroupUnitType.INFANTRY);
		m_bCrewSpawnProfileRestored = true;
		Print("[AICF][CREW_PROFILE_RESTORED] generation=1 actual=10 unit_type=INFANTRY before_coordinator_tick=1");
	}

	protected void AICF_CrewPrepareReducedOrder(AICF_GroupSlot slot)
	{
		if (!m_bCrewPrepared || m_bCrewReduced || !slot.HasVehiclePreset() || slot.GetGroup() != m_CrewGroup) return;
		m_bCrewReduced = true;
		AICF_CrewCheck("ONLY_ASSIGNED_MEMBER_RETAINED", slot.GetDesiredSize() == 1 && m_CrewGroup.GetAgentsCount() == 1 && slot.GetRosterMember(0) == m_CrewKept && !slot.GetRosterMember(9));
		AICF_CrewCheck("ROLE_AND_TRACKING_REMAPPED", slot.GetEquipmentSourceIndex(0) == 9 && m_CrewGroup.AICF_GetSpawnMember(0) == m_CrewKept);
		AICF_CrewCheck("NINE_MANAGED_RETURNING", m_VehicleCrewReturn.CountAgents() == 9 && m_VehicleCrewReturn.AICF_CrewReturnCount() == 9);
		if (m_bStopped || m_bCrewDone || m_USSRState.GetSlot(0) != slot || slot.GetGroup() != m_CrewGroup || slot.GetRosterMember(0) != m_CrewKept) return;
		if (!m_OrderPlanner.AssignPlayerPointOrder(slot, m_USSRFaction, m_CrewKept.GetOrigin() + "400 0 0", false))
		{
			AICF_CrewCheck("FAR_ORDER_REJECTED", false);
			AICF_CrewClose();
		}
	}

	override protected void Update()
	{
		super.Update();
		string flag;
		if (!Replication.IsServer() || !System.GetCLIParam("aicfVehicleCrewReturnProbe", flag) || !m_bRosterReady || m_bCrewDone) return;
		if (!m_iCrewStarted) m_iCrewStarted = System.GetTickCount();
		if (System.GetTickCount() - m_iCrewStarted > 300000) { AICF_CrewCheck("TIMEOUT", false); AICF_CrewClose(); return; }
		if (!m_USSRState || !m_VehicleCoordinator || !m_USSRFaction) return;
		AICF_GroupSlot slot = m_USSRState.GetSlot(0);
		if (!slot || !slot.IsCombatReady()) return;
		if (!m_bCrewPrepared)
		{
			m_CrewGroup = slot.GetGroup();
			AICF_CrewCheck("GROUP10_CREATED", m_CrewGroup && m_CrewGroup.GetAgentsCount() == 10 && slot.GetDesiredSize() == 10 && m_bCrewSpawnProfileRestored);
			if (!m_CrewGroup || m_CrewGroup.GetAgentsCount() != 10) { AICF_CrewClose(); return; }
			Print(string.Format("[AICF][CREW_CANONICAL_BIND] group=%1 initial_generation=%2 desired=10 actual=%3 quota_slot_current=%4 managed_lookup_current=%5 coordinator_current=%6",
				AICF_GroupTrace.Entity(m_CrewGroup), slot.GetSpawnGeneration(), m_CrewGroup.GetAgentsCount(),
				IsAIQuotaSlotCurrent(m_USSRState, slot), FindManagedInfantrySlot(m_CrewGroup) == slot, m_VehicleCoordinator != null));
			m_CrewKept = slot.GetRosterMember(9);
			AICF_CrewCheck("TEN_TRACKED_NONPREFIX", m_CrewKept && slot.GetRosterMember(0));
			// Начальная сцена fixture: бойцы стоят в 40m от штатного spawn point.
			AIPathfindingComponent path = AIPathfindingComponent.Cast(m_CrewGroup.FindComponent(AIPathfindingComponent));
			vector origin = m_USSRFaction.GetMainBase().GetSpawnPoint().GetOrigin();
			path.GetNavmeshComponent().LoadTileIn(origin + "40 0 0");
			vector position;
			bool located = path.GetClosestPositionOnNavmesh(origin + "40 0 0", "12 5 12", position);
			if (!located) { AICF_CrewCheck("RETURN_ROUTE_START_OUTSIDE_BASE", false); AICF_CrewClose(); return; }
			for (int i; i < 10; i++) { slot.GetRosterMember(i).SetOrigin(position + Vector(0, 0, i * 0.7)); slot.GetRosterMember(i).Update(); }
			// Настоящий player point hold защищает confirmation от AI retarget;
			// waypoint не suspended, поэтому обычный CanResize остаётся проверяемым.
			bool holding = m_OrderPlanner.AssignPlayerPointOrder(slot, m_USSRFaction, position, false);
			bool resizeAllowed = CanResizeVehicleCrew(slot);
			AICF_CrewCheck("RETURN_ROUTE_START_OUTSIDE_BASE", vector.DistanceSqXZ(origin, position) > 225 && holding && resizeAllowed);
			if (!holding || !resizeAllowed) { AICF_CrewClose(); return; }
			Print(string.Format("[AICF][CREW_CONFIRMATION_HOLD] player_intent=%1 resize_allowed=%2 waypoint=%3",
				slot.HasPlayerStrategicIntent(), resizeAllowed, AICF_GroupTrace.Entity(slot.GetWaypoint())));
			m_bCrewPrepared = true;
			return;
		}
		if (slot.GetGroup() != m_CrewGroup) { AICF_CrewCheck("CANONICAL_GROUP_REPLACED", false); AICF_CrewClose(); return; }
		if (!slot.HasVehiclePreset()) return;
		if (System.GetTickCount() >= m_iCrewNextTrace)
		{
			m_iCrewNextTrace = System.GetTickCount() + 10000;
			m_VehicleCrewReturn.AICF_CrewTracePending(300000 - (System.GetTickCount() - m_iCrewStarted));
		}
		AICF_TransportTrip trip = m_VehicleCoordinator.GetTrip(slot);
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
		// Канонические slot/group/coordinator не принадлежат fixture. Штатный
		// scheduled shutdown выполняет их единственный lifecycle cleanup.
		Print(string.Format("[AICF][CREW_DONE] checks=%1 failures=%2", m_iCrewChecks, m_iCrewFailures));
		SCR_GameModeCampaign.Cast(m_Campaign).AICF_VehicleProbeScheduleServerClose();
	}
}
