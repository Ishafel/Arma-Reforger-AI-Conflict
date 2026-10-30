// Controlled historical Pilot OpenGate regression. Only fresh empty group birth
// and local delivery-site selection differ; native readiness/actions/path stay real.
class AICF_GateCrewProbe
{
	static AICF_MatchController s_Owner;
	static SCR_AIGroup s_Group;
	static EntityID s_GroupId;
	static string s_GroupRpl;
	static AICF_GroupSlot s_Slot;
	static AICF_TransportTrip s_Trip;
	static IEntity s_DoorEntity;
	static EntityID s_DoorId;
	static BaseDoorComponent s_Door;
	static bool s_CloseRequested;
	static bool s_Closed;
	static AICF_TransportNativeGate s_Gate;
	static BaseCompartmentSlot s_OriginalSeat;
	static bool s_Started;
	static int s_EmptyAt;
	static int s_OwnedPolls;
	static int s_CompletedAt;
	static int s_ReturnedAt;
	static int s_WindowStart;
	static int s_WindowDeadline;
	static vector s_ReturnPosition;
	static bool s_ActualPilot;
	static bool s_Invalid;
	static int s_MainWorldSpawns;
	static string Mode()
	{
		string value;
		if (!Replication.IsServer() || !System.GetCLIParam("aicfNativeGateCrewProbe", value)) return string.Empty;
		if (value != "BEFORE" && value != "AFTER") return string.Empty;
		return value;
	}
	static vector Pad()
	{
		vector position = "1157.67 1.066 3266.46";
		position[1] = GetGame().GetWorld().GetSurfaceY(position[0], position[2]) + 0.05;
		return position;
	}
	static bool GroupCurrent(SCR_AIGroup group)
	{
		return group && group == s_Group && group.GetID() == s_GroupId && AICF_VehicleBehaviorBinding.ComponentRpl(group) == s_GroupRpl;
	}
	static void Log(string phase, string details)
	{
		Print(string.Format("[AICF][GATE_CREW_PROBE] event=%1 mode=%2 classification=CONTROLLED_GATE_SITE_ONLY %3", phase, Mode(), details));
	}
	static float Health(IEntity entity)
	{
		DamageManagerComponent damage;
		if (entity) damage = DamageManagerComponent.Cast(entity.FindComponent(DamageManagerComponent));
		if (!damage || !damage.GetDefaultHitZone()) return -1;
		return damage.GetDefaultHitZone().GetHealthScaled();
	}
	static void ObserveGate(AICF_TransportNativeGate gate, string eventName)
	{
		if (Mode().IsEmpty() || !s_Trip || !s_DoorEntity || !gate.m_SmartOwner || gate.m_Trip != s_Trip || gate.m_SmartOwner != s_DoorEntity ||
			gate.m_SmartOwner.GetID() != s_DoorId || !gate.m_Seat || gate.m_Seat.GetType() != ECompartmentType.PILOT) return;
		if (eventName == "TRANSPORT_NATIVE_GATE_STARTED")
		{
			if (s_Started) { s_Invalid = true; Log("REPLACED_GATE", "same_original_token=0"); return; }
			s_Started = true;
			s_Gate = gate;
			s_OriginalSeat = gate.m_Seat;
			s_WindowStart = gate.m_iStarted; s_WindowDeadline = gate.m_iDeadline;
			s_ActualPilot = gate.m_Member == s_Trip.VehiclePresetMember(0);
			string details = string.Format("operation=%1 member=%2 seat=%3 gate=%4 window_start=%5 window_deadline=%6 actual_original_pilot=%7 closed_setup=%8 vehicle=%9", s_Trip.GetOperationId(), gate.m_Id, gate.m_sSeatKey, s_DoorId, s_WindowStart, s_WindowDeadline, s_ActualPilot, s_Closed, gate.m_VehicleId);
			details += string.Format(" vehicle_rpl=%1", AICF_VehicleBehaviorBinding.ComponentRpl(gate.m_Vehicle));
			Log("NATIVE_PILOT_CHAIN", details);
		}
		if (gate != s_Gate) return;
		if (eventName == "TRANSPORT_NATIVE_GATE_CREW_EXIT_OWNED") s_OwnedPolls++;
		if (eventName == "TRANSPORT_NATIVE_GATE_RETURNED")
		{
			s_CompletedAt = gate.m_iReturnCompletedAtMs; s_ReturnedAt = gate.m_iReturnedAtMs;
			s_ReturnPosition = gate.m_vReturnPosition;
			Log("NATIVE_COMPLETED_RETURN", string.Format("completion=%1 returned=%2 observed_exit=%3 actual_callback=%4 owned_polls=%5 window_start=%6 window_deadline=%7", s_CompletedAt, s_ReturnedAt, gate.m_bObservedExit, gate.m_bReturnCompleted, s_OwnedPolls, gate.m_iStarted, gate.m_iDeadline));
		}
	}
}
modded class AICF_TransportNativeGate
{
	override void Log(string eventName)
	{
		if (!AICF_GateCrewProbe.Mode().IsEmpty()) AICF_GateCrewProbe.ObserveGate(this, eventName);
		super.Log(eventName);
	}
}
modded class AICF_GroupSpawner
{
	override SCR_AIGroup SpawnGroup(SCR_CampaignFaction faction, SCR_CampaignMilitaryBaseComponent spawnBase, int slotId, int desiredSize, bool managedDeployment = true)
	{
		SCR_AIGroup group = super.SpawnGroup(faction, spawnBase, slotId, desiredSize, managedDeployment);
		if (AICF_GateCrewProbe.Mode().IsEmpty() || !managedDeployment || !faction || faction.GetFactionKey() != "USSR" || slotId != 0 || desiredSize != 1) return group;
		if (!AICF_GateCrewProbe.s_Owner || AICF_MatchController.GetActiveController() != AICF_GateCrewProbe.s_Owner || AICF_GateCrewProbe.s_Group) return group;
		if (!group || group.GetAgentsCount() != 0 || group.GetIntendedActiveCount() != 0) return group;
		group.SetOrigin(AICF_GateCrewProbe.Pad() + "-20 0 0");
		AICF_GateCrewProbe.s_Group = group; AICF_GateCrewProbe.s_GroupId = group.GetID();
		AICF_GateCrewProbe.s_GroupRpl = AICF_VehicleBehaviorBinding.ComponentRpl(group);
		AICF_GateCrewProbe.Log("EMPTY_BIRTH", string.Format("group=%1 agents=%2 intended=%3 position=%4 native_request_pending=1 actor_teleports=0", AICF_GateCrewProbe.s_GroupId, group.GetAgentsCount(), group.GetIntendedActiveCount(), group.GetOrigin()));
		return group;
	}
}
modded class AICF_VehicleSpawner
{
	override protected Vehicle SpawnSelectedPrefab(ResourceName prefab, vector position, EntitySpawnParams spawnParams = null)
	{
		Vehicle created = super.SpawnSelectedPrefab(prefab, position, spawnParams);
		if (!AICF_GateCrewProbe.Mode().IsEmpty() && created && created.GetWorld() == GetGame().GetWorld())
		{
			AICF_GateCrewProbe.s_MainWorldSpawns++;
			EntityID createdId = created.GetID();
			AICF_GateCrewProbe.Log("NATIVE_CAR_SPAWN", string.Format("vehicle=%1 vehicle_rpl=%2 native_spawns=%3 prefab=%4", createdId, AICF_VehicleBehaviorBinding.ComponentRpl(created), AICF_GateCrewProbe.s_MainWorldSpawns, prefab));
		}
		return created;
	}
	override bool TrySelectSiteForAcquisition(SCR_GameModeCampaign campaign, SCR_CampaignFaction faction, SCR_AIGroup group, AICF_ConflictAdapter conflictAdapter, vector preferredPosition, float maximumSpawnDistanceMeters, float maximumBoardingDistanceMeters, string identityContext, int requestGeneration, int attempt, bool preflightOnly, out AICF_VehicleSpawnSiteSelection selection)
	{
		if (AICF_GateCrewProbe.Mode().IsEmpty() || !AICF_GateCrewProbe.GroupCurrent(group)) return super.TrySelectSiteForAcquisition(campaign, faction, group, conflictAdapter, preferredPosition, maximumSpawnDistanceMeters, maximumBoardingDistanceMeters, identityContext, requestGeneration, attempt, preflightOnly, selection);
		if (!faction || !faction.GetMainBase() || !faction.GetMainBase().GetSpawnPoint() || AICF_MatchController.GetActiveController() != AICF_GateCrewProbe.s_Owner) return false;
		bool selected = super.TrySelectSiteForAcquisition(campaign, faction, group, conflictAdapter, faction.GetMainBase().GetSpawnPoint().GetOrigin(), maximumSpawnDistanceMeters, 0, identityContext, requestGeneration, attempt, preflightOnly, selection);
		if (!selected || !selection || !selection.IsSelected() || !AICF_GateCrewProbe.GroupCurrent(group)) return false;
		if (AICF_MatchController.GetActiveController() != AICF_GateCrewProbe.s_Owner || AICF_GateCrewProbe.s_Owner.FindManagedInfantrySlot(group) != AICF_GateCrewProbe.s_Slot) return false;
		selection.m_vPosition = AICF_GateCrewProbe.Pad();
		AICF_GateCrewProbe.Log("SITE", string.Format("group=%1 selected_base=%2 pad=%3 commit_unchanged=1", AICF_GateCrewProbe.s_GroupId, selection.m_Base, selection.m_vPosition));
		return true;
	}
}
modded class AICF_VehicleCoordinator
{
	override void Update(AICF_FactionState usState, SCR_CampaignFaction usFaction, AICF_FactionState ussrState, SCR_CampaignFaction ussrFaction, int baseRevision, bool dispatchTrips)
	{
		AICF_TransportTrip trip = AICF_GateCrewProbe.s_Trip;
		AICF_TransportNativeGate gate = AICF_GateCrewProbe.s_Gate;
		if (trip && gate && trip.GetMovementState().GetNativeGate() == gate && gate.Matches(trip) && !gate.IsExpired(System.GetTickCount()))
		{
			ChimeraCharacter actor = ChimeraCharacter.Cast(gate.m_Member);
			if (actor && actor.GetCompartmentAccessComponent() && !gate.m_Seat.GetOccupant() && !AICF_GateCrewProbe.s_EmptyAt)
			{
				AICF_GateCrewProbe.s_EmptyAt = System.GetTickCount();
				EntityID actorId = actor.GetID();
				string details = string.Format("operation=%1 member=%2 current_native=%3 window_start=%4 window_deadline=%5 character_in_vehicle=%6 in_compartment=%7 getting_out=%8 linked_vehicle=%9", trip.GetOperationId(), actorId, AICF_GroupTrace.Action(gate.m_Utility.GetCurrentBehavior()), gate.m_iStarted, gate.m_iDeadline, actor.IsInVehicle(), actor.GetCompartmentAccessComponent().IsInCompartment(), actor.GetCompartmentAccessComponent().IsGettingOut(), AICF_GroupTrace.Entity(CompartmentAccessComponent.GetVehicleIn(actor)));
				details += string.Format(" actor_health=%1 vehicle_health=%2 conscious=%3", AICF_GateCrewProbe.Health(actor), AICF_GateCrewProbe.Health(gate.m_Vehicle), SCR_AIDamageHandling.IsConscious(actor));
				AICF_GateCrewProbe.Log("ACTUAL_EMPTY_BEFORE_TICK", details);
			}
		}
		super.Update(usState, usFaction, ussrState, ussrFaction, baseRevision, dispatchTrips);
	}
}
modded class AICF_MatchController
{
	protected int m_iGCPStarted;
	protected bool m_bGCPConfigured;
	protected bool m_bGCPDone;
	protected bool m_bGCPTruce;
	protected IEntity m_GCPActor;
	protected EntityID m_GCPActorId;
	protected string m_sGCPActorRpl;
	protected Vehicle m_GCPCar;
	protected EntityID m_GCPCarId;
	protected string m_sGCPCarRpl;
	protected float m_fGCPActorHealth;
	protected float m_fGCPCarHealth;
	protected int m_iGCPDeadline;
	protected int m_iGCPAttempts;
	protected int m_iGCPMoving;
	protected int m_iGCPNextMotion;
	protected IEntity m_GCPBestGate;
	protected float m_fGCPBestDistance = 100;
	protected bool AICF_GCPCollect(IEntity entity)
	{
		if (!entity || entity.GetWorld() != GetGame().GetWorld()) return true;
		BaseDoorComponent door = BaseDoorComponent.Cast(entity.FindComponent(BaseDoorComponent));
		SCR_AISmartActionComponent smart = SCR_AISmartActionComponent.Cast(entity.FindComponent(SCR_AISmartActionComponent));
		if (!door || !smart) return true;
		array<string> tags = {}; smart.GetTags(tags);
		if (!tags.Contains(SCR_AIGoalReaction_OpenNavlinkDoor.SMART_ACTION_TAG)) return true;
		float distance = vector.Distance(entity.GetOrigin(), "1192.29 2.17984 3320.04");
		if (distance < m_fGCPBestDistance) { m_fGCPBestDistance = distance; m_GCPBestGate = entity; }
		return true;
	}
	protected bool AICF_GCPHealth(IEntity entity, out float health)
	{
		DamageManagerComponent damage;
		if (entity) damage = DamageManagerComponent.Cast(entity.FindComponent(DamageManagerComponent));
		if (!damage || !damage.GetDefaultHitZone()) return false;
		health = damage.GetDefaultHitZone().GetHealthScaled(); return health >= 0 && health <= 1;
	}
	protected void AICF_GCPTruce()
	{
		if (m_bGCPTruce) return;
		SCR_FactionManager manager = SCR_FactionManager.Cast(GetGame().GetFactionManager());
		if (!manager) return;
		array<Faction> factions = {}; manager.GetFactionsList(factions);
		if (factions.Count() < 2) return;
		for (int first; first < factions.Count(); first++)
		{
			for (int second = first + 1; second < factions.Count(); second++)
			{
				SCR_Faction a = SCR_Faction.Cast(factions[first]); SCR_Faction b = SCR_Faction.Cast(factions[second]);
				if (!a || !b) return;
				manager.SetFactionsFriendly(a, b);
				if (!a.IsFactionFriendly(b) || !b.IsFactionFriendly(a)) return;
			}
		}
		m_bGCPTruce = true;
		AICF_GateCrewProbe.Log("TRUCE", "native_all_pairs_friendly=1 health_writes=0 normal_war=0");
	}
	protected void AICF_GCPFinish(bool passed, string reason)
	{
		if (m_bGCPDone) return;
		m_bGCPDone = true;
		string details = string.Format("passed=%1 reason=%2 actual_empty=%3 owned_polls=%4 completion=%5 returned=%6 moving_polls=%7 deadline=%8", passed, reason, AICF_GateCrewProbe.s_EmptyAt, AICF_GateCrewProbe.s_OwnedPolls, AICF_GateCrewProbe.s_CompletedAt, AICF_GateCrewProbe.s_ReturnedAt, m_iGCPMoving, m_iGCPDeadline);
		if (m_GCPCar)
		{
			EntityID carId = m_GCPCar.GetID();
			details += string.Format(" vehicle=%1 vehicle_rpl=%2 measured_motion_m=%3 vehicle_health=%4 captured_vehicle_health=%5", carId, AICF_VehicleBehaviorBinding.ComponentRpl(m_GCPCar), vector.DistanceXZ(AICF_GateCrewProbe.s_ReturnPosition, m_GCPCar.GetOrigin()), AICF_GateCrewProbe.Health(m_GCPCar), m_fGCPCarHealth);
		}
		if (m_GCPActor)
		{
			EntityID memberId = m_GCPActor.GetID();
			details += string.Format(" member=%1 member_rpl=%2 actor_health=%3 captured_actor_health=%4", memberId, AICF_VehicleBehaviorBinding.ComponentRpl(m_GCPActor), AICF_GateCrewProbe.Health(m_GCPActor), m_fGCPActorHealth);
		}
		if (AICF_GateCrewProbe.s_Trip) details += string.Format(" current_deadline=%1 attempts=%2 captured_attempts=%3", AICF_GateCrewProbe.s_Trip.GetAbsoluteDeadlineMs(), AICF_GateCrewProbe.s_Trip.GetMovementState().GetMobilityRecoveryAttempts(), m_iGCPAttempts);
		bool exactPilot;
		ChimeraCharacter pilot = ChimeraCharacter.Cast(m_GCPActor);
		if (pilot && pilot.GetCompartmentAccessComponent() && AICF_GateCrewProbe.s_OriginalSeat)
			exactPilot = AICF_GateCrewProbe.s_OriginalSeat.GetOccupant() == pilot && pilot.GetCompartmentAccessComponent().GetCompartment() == AICF_GateCrewProbe.s_OriginalSeat;
		details += string.Format(" native_compartment_exact=%1 native_spawns=%2", exactPilot, AICF_GateCrewProbe.s_MainWorldSpawns);
		AICF_GateCrewProbe.Log("DONE", details);
		GetGame().RequestClose();
	}
	override protected bool SpawnInitialRoster(AICF_FactionState factionState, SCR_CampaignFaction faction)
	{
		if (!AICF_GateCrewProbe.Mode().IsEmpty())
		{
			if (!AICF_GateCrewProbe.s_Owner) AICF_GateCrewProbe.s_Owner = this;
			if (AICF_GateCrewProbe.s_Owner != this) return super.SpawnInitialRoster(factionState, faction);
			if (!m_iGCPStarted) m_iGCPStarted = System.GetTickCount();
			AICF_GCPTruce();
			if (factionState == m_USSRState) { factionState.GetSlot(0).SetUnitType(AICF_EGroupUnitType.MOTORIZED_LIGHT); factionState.GetSlot(0).SetDesiredSize(1); }
		}
		return super.SpawnInitialRoster(factionState, faction);
	}
	override protected void ProcessFaction(AICF_FactionState factionState, SCR_CampaignFaction faction)
	{
		super.ProcessFaction(factionState, faction);
		if (AICF_GateCrewProbe.Mode().IsEmpty() || !m_bRosterReady || m_bGCPConfigured || factionState != m_USSRState || !AICF_GateCrewProbe.GroupCurrent(factionState.GetSlot(0).GetGroup())) return;
		AICF_GroupSlot slot = factionState.GetSlot(0);
		if (!m_VehicleCoordinator) return;
		AICF_TransportTrip existing = m_VehicleCoordinator.GetTrip(slot);
		if (existing) AICF_GateCrewProbe.Log("EXISTING_BEFORE_INTENT", string.Format("operation=%1 state=%2 native_spawns=%3", existing.GetOperationId(), existing.GetPhase(), AICF_GateCrewProbe.s_MainWorldSpawns));
		int actual, wrong, dead;
		if (!slot.IsCombatReady() || !AICF_GroupRuntime.HasExactFactionRoster(slot.GetGroup(), "USSR", 1, actual, wrong, dead)) return;
		if (!m_GCPBestGate) GetGame().GetWorld().QueryEntitiesBySphere("1192.29 2.17984 3320.04", 40, AICF_GCPCollect, null, EQueryEntitiesFlags.ALL);
		if (!m_GCPBestGate) return;
		AICF_GateCrewProbe.s_DoorEntity = m_GCPBestGate; AICF_GateCrewProbe.s_DoorId = m_GCPBestGate.GetID();
		AICF_GateCrewProbe.s_Door = BaseDoorComponent.Cast(m_GCPBestGate.FindComponent(BaseDoorComponent));
		if (!AICF_GateCrewProbe.s_CloseRequested)
		{
			RplComponent rpl = RplComponent.Cast(m_GCPBestGate.FindComponent(RplComponent));
			if (!m_Campaign.IsMaster() || (rpl && (!rpl.IsMaster() || rpl.IsProxy()))) { AICF_GCPFinish(false, "DOOR_AUTHORITY"); return; }
			AICF_GateCrewProbe.s_Door.SetControlValue(0, RplId.Invalid());
			AICF_GateCrewProbe.s_CloseRequested = true;
			AICF_GateCrewProbe.Log("CLOSE_REQUESTED", string.Format("gate=%1 gate_rpl=%2 native_master_world=1", AICF_GateCrewProbe.s_DoorId, AICF_VehicleBehaviorBinding.ComponentRpl(m_GCPBestGate)));
		}
		if (Math.AbsFloat(AICF_GateCrewProbe.s_Door.GetNormalizedDoorState()) > 0.01 || Math.AbsFloat(AICF_GateCrewProbe.s_Door.GetControlValue()) > 0.01) return;
		AICF_GateCrewProbe.s_Closed = true;
		AICF_GCPTruce();
		if (!m_bGCPTruce) return;
		AICF_VehiclePresetCatalog catalog = new AICF_VehiclePresetCatalog(faction);
		ResourceName prefab = "{9B3A89DD33FF0483}Prefabs/Vehicles/Wheeled/UAZ469/UAZ469_uncovered_CIV_blue.et";
		string reason;
		if (!catalog.Inspect(prefab, reason)) { catalog.Clear(); AICF_GCPFinish(false, "CATALOG"); return; }
		AICF_VehiclePreset recipe = catalog.Draft(prefab);
		for (int index; index < catalog.Seats().Count(); index++) if (catalog.Seats()[index].GetType() == ECompartmentType.PILOT) recipe.Assign(index, 0);
		bool valid = recipe.ValidateRoster(slot, catalog.Seats(), reason) && PreflightVehiclePreset(slot, faction, recipe, reason);
		catalog.Clear();
		if (!valid) { AICF_GCPFinish(false, "PRESET"); return; }
		if (!m_OrderPlanner.AssignPlayerPointOrder(slot, faction, "1612.34 0.906388 3266.6", false)) return;
		if (!slot.SetVehiclePreset(recipe, slot.GetVehiclePresetRevision())) { AICF_GCPFinish(false, "PRESET_APPLY"); return; }
		m_GCPActor = slot.GetRosterMember(0); m_GCPActorId = m_GCPActor.GetID(); m_sGCPActorRpl = AICF_VehicleBehaviorBinding.ComponentRpl(m_GCPActor);
		if (!AICF_GCPHealth(m_GCPActor, m_fGCPActorHealth)) { AICF_GCPFinish(false, "ACTOR_HEALTH"); return; }
		AICF_GateCrewProbe.s_Slot = slot; m_bGCPConfigured = true;
		AICF_GateCrewProbe.Log("ACTUAL_READY_CLOSED", string.Format("group=%1 actors=%2 actor=%3 actor_rpl=%4 gate=%5 state=%6 control=%7 fake_ready=0", AICF_GateCrewProbe.s_GroupId, actual, m_GCPActorId, m_sGCPActorRpl, AICF_GateCrewProbe.s_DoorId, AICF_GateCrewProbe.s_Door.GetNormalizedDoorState(), AICF_GateCrewProbe.s_Door.GetControlValue()));
	}
	override protected void Update()
	{
		super.Update();
		if (AICF_GateCrewProbe.Mode().IsEmpty() || m_bGCPDone || m_bStopped || AICF_MatchController.GetActiveController() != this) return;
		if (!m_iGCPStarted) return;
		if (System.GetTickCount() - m_iGCPStarted > 300000) { AICF_GCPFinish(false, "ORIGINAL_300S_BOUND"); return; }
		if (AICF_GateCrewProbe.s_Invalid) { AICF_GCPFinish(false, "ORIGINAL_GATE_REPLACED"); return; }
		if (!m_bGCPConfigured) return;
		AICF_GroupSlot slot = m_USSRState.GetSlot(0);
		AICF_TransportTrip trip = m_VehicleCoordinator.GetTrip(slot);
		if (!trip) return;
		if (!AICF_GateCrewProbe.s_Trip) { AICF_GateCrewProbe.s_Trip = trip; m_iGCPDeadline = trip.GetAbsoluteDeadlineMs(); }
		if (trip != AICF_GateCrewProbe.s_Trip || !AICF_GateCrewProbe.GroupCurrent(slot.GetGroup())) { AICF_GCPFinish(false, "CURRENT_TRIP_CHANGED"); return; }
		Vehicle car;
		if (trip.GetLease()) car = trip.GetLease().GetVehicle();
		if (car && !m_GCPCar) { m_GCPCar = car; m_GCPCarId = car.GetID(); m_sGCPCarRpl = AICF_VehicleBehaviorBinding.ComponentRpl(car); if (!AICF_GCPHealth(car, m_fGCPCarHealth)) { AICF_GCPFinish(false, "CAR_HEALTH"); return; } m_iGCPAttempts = trip.GetMovementState().GetMobilityRecoveryAttempts(); }
		if (AICF_GateCrewProbe.Mode() == "BEFORE" && AICF_GateCrewProbe.s_ActualPilot && AICF_GateCrewProbe.s_EmptyAt > 0 && trip.GetPhase() == AICF_ETransportTripPhase.TACTICAL_DEPLOYMENT && trip.GetLastTransitionReason() == "CREW_UNAVAILABLE")
		{
			AICF_GCPFinish(true, "FALSE_CREW_UNAVAILABLE_TACTICAL"); return;
		}
		if (trip.IsTerminal())
		{
			bool reproduced = AICF_GateCrewProbe.Mode() == "BEFORE" && AICF_GateCrewProbe.s_ActualPilot && AICF_GateCrewProbe.s_EmptyAt > 0 && trip.GetTerminalReason() == "TACTICAL_BEGIN_REJECTED";
			AICF_GCPFinish(reproduced, "TERMINAL_" + trip.GetTerminalReason()); return;
		}
		if (!car || !AICF_GateCrewProbe.s_ReturnedAt) return;
		float actorHealth, carHealth;
		if (slot.GetRosterMember(0) != m_GCPActor || m_GCPActor.GetID() != m_GCPActorId || car != m_GCPCar || car.GetID() != m_GCPCarId || AICF_VehicleBehaviorBinding.ComponentRpl(m_GCPActor) != m_sGCPActorRpl || AICF_VehicleBehaviorBinding.ComponentRpl(car) != m_sGCPCarRpl) { AICF_GCPFinish(false, "ORIGINAL_ENTITY_RPL_CHANGED"); return; }
		if (!AICF_GCPHealth(m_GCPActor, actorHealth) || !AICF_GCPHealth(car, carHealth) || actorHealth != m_fGCPActorHealth || carHealth != m_fGCPCarHealth || trip.GetAbsoluteDeadlineMs() != m_iGCPDeadline || trip.GetMovementState().GetMobilityRecoveryAttempts() != m_iGCPAttempts) { AICF_GCPFinish(false, "HEALTH_DEADLINE_OR_BUDGET_CHANGED"); return; }
		if (System.GetTickCount() < m_iGCPNextMotion) return;
		m_iGCPNextMotion = System.GetTickCount() + 500;
		if (car.GetPhysics() && car.GetPhysics().GetVelocity().Length() >= 0.5) m_iGCPMoving++;
		if (AICF_GateCrewProbe.Mode() != "AFTER" || AICF_GateCrewProbe.s_EmptyAt <= 0 || AICF_GateCrewProbe.s_OwnedPolls <= 0) return;
		if (AICF_GateCrewProbe.s_CompletedAt <= 0 || AICF_GateCrewProbe.s_ReturnedAt < AICF_GateCrewProbe.s_CompletedAt) return;
		if (CompartmentAccessComponent.GetVehicleIn(m_GCPActor) != car || !AICF_VehicleNativeSubgroup.Settled(m_GCPActor, car)) return;
		if (!AICF_GateCrewProbe.s_OriginalSeat || AICF_GateCrewProbe.s_OriginalSeat.GetVehicle() != car || AICF_GateCrewProbe.s_OriginalSeat.GetOccupant() != m_GCPActor) return;
		if (m_iGCPMoving < 3 || vector.DistanceXZ(AICF_GateCrewProbe.s_ReturnPosition, car.GetOrigin()) < 10) return;
		AICF_GCPFinish(true, "ACTUAL_PILOT_NATIVE_RETURN_AND_10M");
	}
	override protected void Stop(bool cleanupEntities)
	{
		if (AICF_GateCrewProbe.s_Owner == this)
		{
			AICF_GateCrewProbe.s_Trip = null; AICF_GateCrewProbe.s_Slot = null;
			AICF_GateCrewProbe.s_Gate = null; AICF_GateCrewProbe.s_Group = null;
			AICF_GateCrewProbe.s_OriginalSeat = null;
			AICF_GateCrewProbe.s_Door = null; AICF_GateCrewProbe.s_DoorEntity = null;
			AICF_GateCrewProbe.s_Owner = null;
		}
		super.Stop(cleanupEntities);
	}
}
