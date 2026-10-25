// Только isolated stage; terminal native readback, без GUI automation.
modded class AICF_GroupSlot
{
	void AICF_VPAdvanceGeneration() { m_iSpawnGeneration++; }
}

modded class AICF_VehicleCoordinator
{
	bool AICF_VPAdmissionPolicy(AICF_GroupSlot slot, SCR_CampaignFaction faction, bool manual)
	{
		AICF_StrategicAssignmentSnapshot assignment;
		if (!m_OrderPlanner.TryCreateAssignmentSnapshot(slot, faction, m_iObservedBaseRevision, assignment)) return false;
		AICF_EGroupUnitType previous = slot.GetUnitType();
		slot.SetUnitType(AICF_EGroupUnitType.INFANTRY);
		string reason;
		IsAdmissionEligible(slot, assignment, m_Fleets.FindFleet(faction.GetFactionKey()), reason);
		slot.SetUnitType(previous);
		if (manual) return reason != "COMMANDER_INFANTRY_PROFILE";
		return reason == "COMMANDER_INFANTRY_PROFILE";
	}
	bool AICF_VPFleetCap(AICF_GroupSlot slot, SCR_CampaignFaction faction)
	{
		AICF_StrategicAssignmentSnapshot source;
		if (!m_OrderPlanner.TryCreateAssignmentSnapshot(slot, faction, m_iObservedBaseRevision, source)) return false;
		AICF_FactionFleet fleet = m_Fleets.FindFleet(faction.GetFactionKey());
		array<ref AICF_VehicleLease> leases = {};
		bool reserved = true;
		for (int i; i < fleet.GetMaximumActiveOrReserved(); i++)
		{
			AICF_StrategicAssignmentSnapshot assignment = new AICF_StrategicAssignmentSnapshot(faction.GetFactionKey(), i + 1, "probe-" + i.ToString(), source.GetGroupGeneration(), source.GetGroup(),
				source.GetRole(), source.GetUnitType(), source.GetPosture(), source.GetTargetKind(), source.GetTargetBase(), source.GetTargetPosition(), source.GetAssignmentRevision(),
				source.GetStrategicIntentRevision(), source.GetBaseRevision(), source.GetMeaningfulInfantryWaypoint());
			AICF_VehicleLease lease;
			if (!fleet.TryReserveLease(assignment, 10000 + i, lease)) reserved = false;
			else leases.Insert(lease);
		}
		string reason;
		bool blocked = !CanConfigureVehiclePreset(slot, faction, reason) && reason == "FLEET_CAP";
		foreach (AICF_VehicleLease held : leases)
			if (!fleet.ReleaseEmptyReservation(held)) reserved = false;
		return reserved && blocked && CanConfigureVehiclePreset(slot, faction, reason) && fleet.GetCappedActiveOrReservedCount() == 0;
	}
	bool AICF_VPStaleCallback(AICF_GroupSlot slot, SCR_CampaignFaction faction, AICF_TransportTrip oldTrip)
	{
		AICF_StrategicAssignmentSnapshot current;
		if (!m_OrderPlanner.TryCreateAssignmentSnapshot(slot, faction, m_iObservedBaseRevision, current)) return false;
		AICF_FactionFleet fleet = m_Fleets.FindFleet(faction.GetFactionKey());
		AICF_TripOutcome outcome = m_TripController.Tick(oldTrip, current, slot, fleet, faction);
		return outcome && outcome.GetReason() == "STALE_CONTROLLER_CALLBACK_CANCELLED";
	}
}

modded class AICF_MatchController
{
	protected int m_iVehicleProbeStarted;
	protected int m_iVehicleProbeStep;
	protected int m_iVehicleProbeChecks;
	protected int m_iVehicleProbeFailures;
	protected bool m_bVehicleProbeFinished;
	protected ref AICF_VehiclePreset m_VehicleProbePreset;
	protected int m_iVehicleProbeLoadoutRevision;
	protected int m_iVehicleProbeSize;
	protected int m_iVehicleProbeAppliedRevision;
	protected int m_iVehicleProbeConfirmedAt;
	protected ref AICF_FactionState m_VehicleProbeState;
	protected ref AICF_VehicleCoordinator m_VehicleProbeCoordinator;
	protected SCR_AIGroup m_VehicleProbeGroup;
	protected SCR_AIGroup m_VehicleProbeOldGroup;
	protected bool m_bVehicleProbeReplacing;
	protected Vehicle m_VehicleProbeHumanVehicle;
	protected EntityID m_VehicleProbeHumanVehicleId;
	protected SCR_PlayerController m_VehicleProbeHumanPlayer;
	protected IEntity m_VehicleProbeHumanDriver;
	protected int m_iVehicleProbeHumanAt;
	protected int m_iVehicleProbeExpectedMembers = 1;

	protected void AICF_VPBudget(AICF_GroupSlot slot)
	{
		array<SCR_CampaignMilitaryBaseComponent> bases = {};
		array<int> supplies = {};
		for (int i; i < m_ObjectiveGraph.GetNodeCount(); i++)
		{
			AICF_ObjectiveNode node = m_ObjectiveGraph.GetNode(i);
			if (!node || !node.GetBase() || node.GetBase().GetFaction() != m_USSRFaction) continue;
			SCR_CampaignMilitaryBaseComponent base = node.GetBase();
			bases.Insert(base);
			supplies.Insert(base.GetSupplies());
			base.AddSupplies(-base.GetSupplies());
		}
		string reason;
		AICF_VPCheck("SUPPLIES_REJECTED", !m_EconomySystem.CanAffordVehiclePreset(m_USSRFaction, m_VehicleProbePreset.m_sPrefab, reason) && reason == "INSUFFICIENT_SUPPLIES");
		bool restored = true;
		for (int b; b < bases.Count(); b++)
		{
			bases[b].AddSupplies(supplies[b]);
			if (bases[b].GetSupplies() != supplies[b]) restored = false;
		}
		AICF_VPCheck("SUPPLIES_RESTORED_NO_BINDING", restored && !slot.HasVehiclePreset());
	}

	protected void AICF_VPValidation(AICF_GroupSlot slot, AICF_VehiclePresetCatalog catalog)
	{
		string reason;
		AICF_VehiclePreset candidate = AICF_VehiclePreset.Decode(m_VehicleProbePreset.Encode());
		int driver = candidate.m_aMembers.Find(0);
		int other = 0;
		if (driver == 0) other = 1;
		AICF_VPCheck("UNASSIGN", candidate.Assign(driver, -1) && candidate.m_aMembers[driver] == -1);
		AICF_VPCheck("MANDATORY_CREW_REJECTED", !candidate.ValidateRoster(slot, catalog.Seats(), reason) && reason == "MANDATORY_CREW_MISSING");
		candidate = AICF_VehiclePreset.Decode(m_VehicleProbePreset.Encode());
		candidate.m_aMembers[other] = 0;
		AICF_VPCheck("DUPLICATE_MEMBER_REJECTED", !candidate.ValidateShape(reason) && reason == "DUPLICATE_MEMBER");
		candidate = AICF_VehiclePreset.Decode(m_VehicleProbePreset.Encode());
		string seat = candidate.m_aSeats[other];
		candidate.m_aSeats[other] = candidate.m_aSeats[driver];
		AICF_VPCheck("DUPLICATE_SEAT_REJECTED", !candidate.ValidateShape(reason) && reason == "DUPLICATE_SEAT");
		candidate.m_aSeats[other] = "999:999:2";
		AICF_VPCheck("MISSING_SEAT_REJECTED", !candidate.ValidateRoster(slot, catalog.Seats(), reason) && reason == "SEAT_TOPOLOGY_CHANGED");
		candidate = AICF_VehiclePreset.Decode(m_VehicleProbePreset.Encode());
		candidate.Assign(other, 9);
		AICF_VPCheck("INACTIVE_MEMBER_REJECTED", !candidate.ValidateRoster(slot, catalog.Seats(), reason) && reason == "MEMBER_INACTIVE");
		candidate = AICF_VehiclePreset.Decode(m_VehicleProbePreset.Encode());
		AICF_VPCheck("REASSIGN_UNIQUE", candidate.Assign(other, 0) && candidate.m_aMembers[driver] == -1 && candidate.m_aMembers[other] == 0);
		candidate = AICF_VehiclePreset.Decode(m_VehicleProbePreset.Encode());
		slot.SetDesiredSize(10);
		AICF_VPCheck("CAPACITY_REJECTED", !candidate.ValidateRoster(slot, catalog.Seats(), reason) && reason == "INSUFFICIENT_SEATS");
		slot.SetDesiredSize(m_iVehicleProbeExpectedMembers);
		AICF_VPCheck("VALID_AFTER_NEGATIVES", candidate.ValidateRoster(slot, catalog.Seats(), reason));
		AICF_VPCheck("NEGATIVES_KEEP_BINDING", !slot.HasVehiclePreset());
	}

	protected void AICF_VPCheck(string rule, bool passed)
	{
		m_iVehicleProbeChecks++;
		if (!passed) m_iVehicleProbeFailures++;
		Print(string.Format("[AICF][VEHICLE_PRESET_CHECK] rule=%1 passed=%2", rule, passed));
	}

	protected void AICF_VPChangeModel(AICF_GroupSlot slot, AICF_TransportTrip trip)
	{
		AICF_VehiclePresetCatalog catalog = new AICF_VehiclePresetCatalog(m_USSRFaction);
		JsonLoadContext list = new JsonLoadContext();
		array<ResourceName> prefabs = {};
		list.LoadFromString(catalog.List());
		list.ReadValue("prefabs", prefabs);
		bool changed;
		string reason;
		foreach (ResourceName prefab : prefabs)
		{
			if (prefab == m_VehicleProbePreset.m_sPrefab || !prefab.Contains("UAZ469") || !catalog.Inspect(prefab, reason) || catalog.Seats().Count() < slot.GetDesiredSize()) continue;
			AICF_VehiclePreset replacement = catalog.Draft(prefab);
			int driver;
			for (int seat; seat < catalog.Seats().Count(); seat++)
				if (catalog.Seats()[seat].GetType() == ECompartmentType.PILOT) driver = seat;
			replacement.Assign(driver, 0);
			int member = 1;
			for (int index; index < catalog.Seats().Count() && member < slot.GetDesiredSize(); index++)
				if (index != driver) { replacement.Assign(index, member); member++; }
			if (!replacement.ValidateRoster(slot, catalog.Seats(), reason)) continue;
			changed = slot.SetVehiclePreset(replacement, slot.GetVehiclePresetRevision()) && AICF_VehiclePreset.Decode(slot.GetVehiclePresetData()).m_sPrefab == prefab;
			break;
		}
		AICF_VPCheck("MODEL_CHANGE_CANCELS_OLD_TRIP", changed && !trip.IsVehiclePresetCurrent());
		catalog.Clear();
	}

	protected void AICF_VPModels(SCR_CampaignFaction faction)
	{
		string requestedModel;
		System.GetCLIParam("aicfVehiclePresetModel", requestedModel);
		AICF_VehiclePresetCatalog catalog = new AICF_VehiclePresetCatalog(faction);
		JsonLoadContext json = new JsonLoadContext();
		array<ResourceName> prefabs = {};
		json.LoadFromString(catalog.List());
		json.ReadValue("prefabs", prefabs);
		int inspected;
		string prefix = AICF_ContentProfile.GetActive().GetStableFactionKey(faction.GetFactionKey()) + "_";
		foreach (ResourceName prefab : prefabs)
		{
			string civilianFlag;
			bool civilian = System.GetCLIParam("aicfVehiclePresetCivilian", civilianFlag);
			if (catalog.Source(prefab) == AICF_EVehicleCatalogSource.CIVILIAN && !civilian) continue;
			if (civilian && catalog.Source(prefab) != AICF_EVehicleCatalogSource.CIVILIAN) continue;
			if (!prefab.Contains("UAZ") && !prefab.Contains("M1025") && (requestedModel.IsEmpty() || !prefab.Contains(requestedModel))) continue;
			string reason;
			bool valid = catalog.Inspect(prefab, reason);
			Print(string.Format("[AICF][VEHICLE_PRESET_MODEL] prefab=%1 valid=%2 seats=%3 reason=%4", prefab, valid, catalog.Seats().Count(), reason));
			if (!valid) continue;
			inspected++;
			AICF_VehiclePreset preset = catalog.Draft(prefab);
			AICF_VPCheck(prefix + "MODEL_ROUNDTRIP_" + inspected.ToString(), AICF_VehiclePreset.Decode(preset.Encode()) && preset.MatchesSeats(catalog.Seats()));
			int turret;
			foreach (BaseCompartmentSlot seat : catalog.Seats())
			{
				Print(string.Format("[AICF][VEHICLE_PRESET_NATIVE_SEAT] prefab=%1 seat=%2 type=%3 name=%4", prefab, AICF_VehiclePreset.SeatKey(seat), seat.GetType(), seat.GetCompartmentName(true)));
				if (seat.GetType() == ECompartmentType.TURRET) turret++;
			}
			AICF_VPCheck(prefix + "NATIVE_SEATS_" + inspected.ToString(), preset.m_aSeats.Count() == catalog.Seats().Count());
			if (prefab.Contains("PKM")) AICF_VPCheck("ARMED_UAZ_TURRET", turret > 0);
			else if (prefab.Contains("UAZ")) AICF_VPCheck(prefix + "UNARMED_UAZ_NO_TURRET_" + inspected.ToString(), turret == 0);
			string crew;
			System.GetCLIParam("aicfVehiclePresetCrew", crew);
			if (!requestedModel.IsEmpty() && prefab.Contains(requestedModel) && faction == m_USSRFaction) m_VehicleProbePreset = preset;
			else if (requestedModel.IsEmpty() && prefab.Contains("UAZ") && faction == m_USSRFaction)
			{
				if (civilian && prefab.Contains("UAZ469_uncovered_CIV_blue.et")) m_VehicleProbePreset = preset;
				else if (!civilian && crew == "armed" && prefab.Contains("PKM")) m_VehicleProbePreset = preset;
				else if (crew != "armed" && !m_VehicleProbePreset && turret == 0) m_VehicleProbePreset = preset;
			}
		}
		catalog.Clear();
		AICF_VPCheck(prefix + "CATALOG_INSPECTED", inspected > 0);
	}

	override protected void Update()
	{
		super.Update();
		string flag;
		if (!System.GetCLIParam("aicfVehiclePresetProbe", flag) || flag != "1" || !m_bRosterReady || m_bVehicleProbeFinished) return;
		int now = System.GetTickCount();
		AICF_GroupSlot slot = m_USSRState.GetSlot(0);
		string humanFlag;
		if (System.GetCLIParam("aicfVehiclePresetHuman", humanFlag))
		{
			array<int> connectedPlayers = {};
			GetGame().GetPlayerManager().GetPlayers(connectedPlayers);
			foreach (int connectedId : connectedPlayers)
			{
				SCR_PlayerController connectedPlayer = SCR_PlayerController.Cast(GetGame().GetPlayerManager().GetPlayerController(connectedId));
				if (connectedPlayer && !connectedPlayer.GetControlledEntity()) connectedPlayer.SetInitialMainEntity(m_USSRState.GetSlot(9).GetRosterMember(0));
			}
		}
		if (m_VehicleProbeState && m_bVehicleProbeReplacing)
		{
			if (now - m_iVehicleProbeStarted > 300000) { AICF_VPCheck("REPLACEMENT_TIMEOUT", false); AICF_VPClose(); return; }
			slot = m_VehicleProbeState.GetSlot(0);
			if (m_VehicleProbeGroup.GetAgentsCount() != m_iVehicleProbeExpectedMembers) return;
			slot.MarkReady();
			slot.RecordDeploymentMembers();
			AICF_VehiclePresetCatalog replacementCatalog = new AICF_VehiclePresetCatalog(m_USSRFaction);
			string replacementReason;
			bool inspected = replacementCatalog.Inspect(m_VehicleProbePreset.m_sPrefab, replacementReason);
			AICF_VPCheck("NEW_GROUP_REVALIDATED", inspected && slot.GetGroup() != m_VehicleProbeOldGroup && AICF_VehiclePreset.Decode(slot.GetVehiclePresetData()).ValidateRoster(slot, replacementCatalog.Seats(), replacementReason));
			replacementCatalog.Clear();
			AICF_VPClose();
			return;
		}
		if (m_iVehicleProbeHumanAt > 0)
		{
			if (now - m_iVehicleProbeHumanAt < 15000) return;
			slot = m_VehicleProbeState.GetSlot(0);
			m_VehicleProbeCoordinator.Update(null, null, m_VehicleProbeState, m_USSRFaction, m_iStrategicBaseRevision, true);
			AICF_VPCheck("HUMAN_VEHICLE_RETAINED", m_VehicleProbeHumanVehicle && m_VehicleProbeHumanVehicle.GetID() == m_VehicleProbeHumanVehicleId);
			AICF_VPCheck("HUMAN_NOT_EJECTED", m_VehicleProbeHumanPlayer.GetControlledEntity() == m_VehicleProbeHumanDriver && CompartmentAccessComponent.GetVehicleIn(m_VehicleProbeHumanDriver) == m_VehicleProbeHumanVehicle);
			// До удаления собственных actors возвращаем игрока на существующего
			// бойца production roster; занятую машину сама fixture не удаляет.
			m_VehicleProbeHumanPlayer.SetInitialMainEntity(m_USSRState.GetSlot(9).GetRosterMember(0));
			m_VehicleProbeHumanPlayer.AICF_VPHumanFinished();
			AICF_VPClose();
			return;
		}
		string reload;
		if (System.GetCLIParam("aicfVehiclePresetReload", reload))
		{
			AICF_VPModels(m_USFaction);
			AICF_VPModels(m_USSRFaction);
			AICF_VehiclePresetStore savedStore = new AICF_VehiclePresetStore();
			AICF_VehiclePreset persisted = savedStore.Get(1);
			// Восстанавливаем ожидаемый исходный план fixture, а не сравниваем
			// сохранённую рассадку с пустым Catalog.Draft.
			string reloadCrew;
			int expectedMembers = 4;
			if (System.GetCLIParam("aicfVehiclePresetCrew", reloadCrew) && reloadCrew == "armed") expectedMembers = 3;
			int expectedDriver;
			for (int seatIndex; seatIndex < m_VehicleProbePreset.m_aSeats.Count(); seatIndex++)
			{
				array<string> parts = {};
				m_VehicleProbePreset.m_aSeats[seatIndex].Split(":", parts, false);
				if (parts[2].ToInt() == ECompartmentType.PILOT) expectedDriver = seatIndex;
			}
			m_VehicleProbePreset.Assign(expectedDriver, 0);
			int expectedMember = 1;
			for (int position; position < m_VehicleProbePreset.m_aSeats.Count() && expectedMember < expectedMembers; position++)
				if (position != expectedDriver) { m_VehicleProbePreset.Assign(position, expectedMember); expectedMember++; }
			AICF_VPCheck("RESTART_MODEL_SEATS", persisted && persisted.SameAssignment(m_VehicleProbePreset) && persisted.m_sName == "Persisted vehicle");
			AICF_VPCheck("RESTART_TOMBSTONE", !savedStore.Get(0));
			AICF_LoadoutStore oldStore = new AICF_LoadoutStore();
			AICF_LoadoutRecipe oldRecipe = oldStore.Get(0);
			AICF_VPCheck("RESTART_LEGACY", oldRecipe && oldRecipe.m_sName == "Vehicle regression member");
			AICF_VPCheck("RESTART_NO_ASSIGNMENT", !slot.HasVehiclePreset());
			AICF_VPCheck("RESTART_NO_VEHICLE", m_VehicleCoordinator.GetSpawnedCount(m_USSRFaction.GetFactionKey()) == 0);
			AICF_VPClose();
			return;
		}
		string crew;
		if (System.GetCLIParam("aicfVehiclePresetCrew", crew))
		{
			m_iVehicleProbeExpectedMembers = 4;
			if (crew == "armed") m_iVehicleProbeExpectedMembers = 3;
			if (crew == "driver") m_iVehicleProbeExpectedMembers = 1;
			if (!m_VehicleProbeState)
			{
				m_VehicleProbeState = new AICF_FactionState(m_USSRFaction.GetFactionKey(), m_Config);
				m_VehicleProbeGroup = m_GroupSpawner.SpawnGroup(m_USSRFaction, m_USSRFaction.GetMainBase(), 0, m_iVehicleProbeExpectedMembers, false);
				AICF_VPCheck("CREW_GROUP_REQUESTED", m_VehicleProbeGroup != null);
				if (!m_VehicleProbeGroup) { AICF_VPClose(); return; }
				m_VehicleProbeGroup.SetLifecyclePolicy(SCR_EAIGroupLifecyclePolicy.Manual);
				m_VehicleProbeGroup.SetDeleteWhenEmpty(false);
				slot = m_VehicleProbeState.GetSlot(0);
				slot.SetDesiredSize(m_iVehicleProbeExpectedMembers);
				slot.SetUnitType(AICF_EGroupUnitType.MOTORIZED_LIGHT);
				slot.BeginInitialSpawn();
				slot.BindSpawnedGroup(m_VehicleProbeGroup);
				m_VehicleProbeGroup.AICF_TrackMemberPositions();
				m_GroupSpawner.BeginRosterSpawn(m_VehicleProbeGroup, m_iVehicleProbeExpectedMembers);
				m_VehicleProbeCoordinator = new AICF_VehicleCoordinator(m_Stage3Config, m_Campaign, m_ConflictAdapter, m_OrderPlanner, m_GroupCohesionPolicy, m_ObjectiveGraph, m_TargetSelector, m_ContentProfile);
				return;
			}
			slot = m_VehicleProbeState.GetSlot(0);
			if (!slot.IsCombatReady())
			{
				if (m_VehicleProbeGroup.GetAgentsCount() != m_iVehicleProbeExpectedMembers) return;
				slot.MarkReady();
				slot.RecordDeploymentMembers();
			}
			m_VehicleProbeCoordinator.Update(null, null, m_VehicleProbeState, m_USSRFaction, m_iStrategicBaseRevision, true);
		}
		if (!m_iVehicleProbeStarted)
		{
			m_iVehicleProbeStarted = now;
			AICF_VPModels(m_USFaction);
			AICF_VPModels(m_USSRFaction);
			AICF_VPCheck("SELECT_UAZ", m_VehicleProbePreset != null);
			string civilianModel;
			if (System.GetCLIParam("aicfVehiclePresetCivilian", civilianModel))
			{
				AICF_VehiclePresetCatalog civilianCatalog = new AICF_VehiclePresetCatalog(m_USSRFaction);
				AICF_VPCheck("CIVILIAN_MODEL", m_VehicleProbePreset && civilianCatalog.Source(m_VehicleProbePreset.m_sPrefab) == AICF_EVehicleCatalogSource.CIVILIAN && m_VehicleProbePreset.m_sFaction == "USSR" && civilianCatalog.Cost(m_VehicleProbePreset.m_sPrefab) > 0);
			}
			if (!m_VehicleProbePreset) { AICF_VPClose(); return; }
			m_VehicleProbePreset.m_sName = "Probe vehicle";
			// Явный выбор состава fixture, до настройки техники; production
			// никогда не уменьшает desired size ради выбранной машины.
			slot.SetDesiredSize(m_iVehicleProbeExpectedMembers);
			m_iVehicleProbeLoadoutRevision = slot.GetLoadoutRevision();
			m_iVehicleProbeSize = slot.GetDesiredSize();
			m_OrderPlanner.AssignPlayerPointOrder(slot, m_USSRFaction, AICF_GroupRuntime.ResolveAliveLeader(slot.GetGroup()).GetOrigin() + "400 0 0", false);
			AICF_VehiclePresetCatalog catalog = new AICF_VehiclePresetCatalog(m_USSRFaction);
			string reason;
			catalog.Inspect(m_VehicleProbePreset.m_sPrefab, reason);
			int driver;
			for (int i; i < catalog.Seats().Count(); i++)
				if (catalog.Seats()[i].GetType() == ECompartmentType.PILOT) driver = i;
			m_VehicleProbePreset.Assign(driver, 0);
			int member = 1;
			for (int seatIndex; seatIndex < catalog.Seats().Count() && member < m_iVehicleProbeExpectedMembers; seatIndex++)
				if (seatIndex != driver) { m_VehicleProbePreset.Assign(seatIndex, member); member++; }
			AICF_VPCheck("VALID_ROSTER", m_VehicleProbePreset.ValidateRoster(slot, catalog.Seats(), reason));
			Print("[AICF][VEHICLE_PRESET_ROSTER_REASON] reason=" + reason);
			AICF_VPValidation(slot, catalog);
			catalog.Clear();
			AICF_VPCheck("UNAVAILABLE_PREFAB_REJECTED", !catalog.Inspect("{0000000000000000}Prefabs/Unavailable.et", reason) && reason == "PREFAB_UNAVAILABLE");
			if (m_VehicleProbeCoordinator) AICF_VPCheck("FLEET_CAP_REJECTED_ATOMIC", m_VehicleProbeCoordinator.AICF_VPFleetCap(slot, m_USSRFaction));
			AICF_VPBudget(slot);
			AICF_VehiclePresetStore store = new AICF_VehiclePresetStore();
			string role;
			AICF_LoadoutRecipe oldRecipe = AICF_LoadoutService.DefaultRecipe(m_USSRFaction, m_GroupSpawner.ResolveRecruitPrefab(m_USSRFaction, 0, role), 0);
			oldRecipe.m_sName = "Vehicle regression member";
			AICF_LoadoutStore oldStore = new AICF_LoadoutStore();
			AICF_VPCheck("LEGACY_SEED", oldStore.Save(oldRecipe, reason));
			AICF_VPCheck("STORE_SAVE", store.Save(m_VehicleProbePreset, reason));
			AICF_VPCheck("STORE_LOAD", store.Get(0) && store.Get(0).SameAssignment(m_VehicleProbePreset));
			AICF_VehiclePresetStore reopened = new AICF_VehiclePresetStore();
			AICF_VPCheck("STORE_REOPEN", reopened.Get(0) && reopened.Get(0).SameAssignment(m_VehicleProbePreset));
			AICF_VPCheck("APPLY", slot.SetVehiclePreset(m_VehicleProbePreset, slot.GetVehiclePresetRevision()));
			if (m_VehicleProbeCoordinator) AICF_VPCheck("MANUAL_OVERRIDES_AUTOMATIC_POLICY", m_VehicleProbeCoordinator.AICF_VPAdmissionPolicy(slot, m_USSRFaction, true));
			m_iVehicleProbeAppliedRevision = slot.GetVehiclePresetRevision();
			AICF_VPCheck("REPEAT_APPLY", slot.SetVehiclePreset(m_VehicleProbePreset, m_iVehicleProbeAppliedRevision) && slot.GetVehiclePresetRevision() == m_iVehicleProbeAppliedRevision);
			AICF_VehiclePreset updated = AICF_VehiclePreset.Decode(m_VehicleProbePreset.Encode());
			updated.Assign(driver, 9);
			AICF_VPCheck("STORE_UPDATE_DRAFT", store.Save(updated, reason) && store.Get(0).SameAssignment(updated));
			AICF_VPCheck("UPDATE_KEEPS_ACTIVE", slot.GetVehiclePresetRevision() == m_iVehicleProbeAppliedRevision && AICF_VehiclePreset.Decode(slot.GetVehiclePresetData()).SameAssignment(m_VehicleProbePreset));
			AICF_VehiclePreset persisted = AICF_VehiclePreset.Decode(m_VehicleProbePreset.Encode());
			persisted.m_sName = "Persisted vehicle";
			AICF_VPCheck("STORE_PERSIST_FOR_RESTART", store.Save(persisted, reason));
			AICF_VPCheck("STORE_DELETE", store.Delete(0, reason));
			AICF_VPCheck("DELETE_KEEPS_BINDING", slot.HasVehiclePreset() && !store.Get(0));
			AICF_VPCheck("LEGACY_UNCHANGED", oldStore.Get(0) && oldStore.Get(0).Encode() == oldRecipe.Encode());
			return;
		}
		if (now - m_iVehicleProbeStarted > 300000) { AICF_VPCheck("PHYSICAL_TIMEOUT", false); AICF_VPClose(); return; }
		AICF_VehicleCoordinator coordinator = m_VehicleCoordinator;
		if (m_VehicleProbeCoordinator) coordinator = m_VehicleProbeCoordinator;
		AICF_TransportTrip trip = coordinator.GetTrip(slot);
		if (!trip || !trip.GetLease() || !trip.GetLease().GetVehicle()) return;
		if (trip.GetPhase() != AICF_ETransportTripPhase.TRANSIT) return;
		Vehicle vehicle = trip.GetLease().GetVehicle();
		if (!m_iVehicleProbeConfirmedAt)
		{
			m_iVehicleProbeConfirmedAt = now;
			array<BaseCompartmentSlot> seats = {};
			AICF_VehiclePreset.Collect(vehicle, seats);
			AICF_VPCheck("EXACT_MODEL", trip.GetLease().GetPrefab() == m_VehicleProbePreset.m_sPrefab);
			for (int i; i < seats.Count(); i++)
				if (m_VehicleProbePreset.m_aMembers[i] >= 0)
					AICF_VPCheck("EXACT_OCCUPANT_" + i.ToString(), seats[i].GetOccupant() == slot.GetRosterMember(m_VehicleProbePreset.m_aMembers[i]));
			AICF_VPCheck("PAID", trip.IsVehiclePresetDebited());
			AICF_VPCheck("UNCHANGED_LOADOUT_SIZE", m_iVehicleProbeLoadoutRevision == slot.GetLoadoutRevision() && m_iVehicleProbeSize == slot.GetDesiredSize());
			return;
		}
		if (now - m_iVehicleProbeConfirmedAt >= 5000)
		{
			int revision = slot.GetVehiclePresetRevision();
			int spawned = coordinator.GetSpawnedCount(m_USSRFaction.GetFactionKey());
			int supplies = m_EconomySystem.GetTotalSupplies(m_USSRFaction);
			AICF_VPCheck("REPEAT_LIVE_APPLY", slot.SetVehiclePreset(m_VehicleProbePreset, revision) && slot.GetVehiclePresetRevision() == revision);
			AICF_VPCheck("REPEAT_NO_SPAWN_DEBIT", coordinator.GetSpawnedCount(m_USSRFaction.GetFactionKey()) == spawned && m_EconomySystem.GetTotalSupplies(m_USSRFaction) == supplies);
			AICF_EGroupUnitType profile = slot.GetUnitType();
			AICF_VPCheck("MANUAL_PRIORITY", trip.GetLease().GetPrefab() == m_VehicleProbePreset.m_sPrefab);
			string human;
			if (System.GetCLIParam("aicfVehiclePresetHuman", human))
			{
				array<int> players = {};
				GetGame().GetPlayerManager().GetPlayers(players);
				if (players.IsEmpty()) return;
				m_VehicleProbeHumanPlayer = SCR_PlayerController.Cast(GetGame().GetPlayerManager().GetPlayerController(players[0]));
				m_VehicleProbeHumanDriver = slot.GetRosterMember(0);
				m_VehicleProbeHumanVehicle = vehicle;
				m_VehicleProbeHumanVehicleId = vehicle.GetID();
				m_VehicleProbeHumanPlayer.SetInitialMainEntity(m_VehicleProbeHumanDriver);
				AICF_VPCheck("HUMAN_TAKEOVER", m_VehicleProbeHumanPlayer.GetControlledEntity() == m_VehicleProbeHumanDriver && CompartmentAccessComponent.GetVehicleIn(m_VehicleProbeHumanDriver) == vehicle);
			}
			else AICF_VPChangeModel(slot, trip);
			AICF_VPCheck("RESET", slot.SetVehiclePreset(null, slot.GetVehiclePresetRevision()) && !slot.HasVehiclePreset());
			if (m_VehicleProbeCoordinator) AICF_VPCheck("AUTOMATIC_POLICY_RESTORED", m_VehicleProbeCoordinator.AICF_VPAdmissionPolicy(slot, m_USSRFaction, false));
			AICF_VPCheck("OLD_REQUEST_CANCELLED", !trip.IsVehiclePresetCurrent());
			slot.AICF_VPAdvanceGeneration();
			AICF_VPCheck("OLD_GENERATION_CALLBACK", coordinator.AICF_VPStaleCallback(slot, m_USSRFaction, trip));
			AICF_VPCheck("RESET_KEEPS_PROFILE", !slot.HasVehiclePreset() && slot.GetUnitType() == profile);
			if (m_VehicleProbeHumanPlayer) { m_iVehicleProbeHumanAt = now; return; }
			// Реальная новая controller entity и roster; настройка остаётся в slot.
			slot.SetVehiclePreset(m_VehicleProbePreset, slot.GetVehiclePresetRevision());
			slot.SetRoleAndIndex(AICF_EGroupRole.DEFEND, 7);
			AICF_VPCheck("ROLE_CHANGE_KEEPS_BINDING", AICF_VehiclePreset.Decode(slot.GetVehiclePresetData()).SameAssignment(m_VehicleProbePreset));
			m_VehicleProbeOldGroup = m_VehicleProbeGroup;
			slot.Reset();
			m_VehicleProbeGroup = m_GroupSpawner.SpawnGroup(m_USSRFaction, m_USSRFaction.GetMainBase(), 0, m_iVehicleProbeExpectedMembers, false);
			AICF_VPCheck("REPLACEMENT_GROUP_REQUESTED", m_VehicleProbeGroup && m_VehicleProbeGroup != m_VehicleProbeOldGroup && slot.HasVehiclePreset());
			if (!m_VehicleProbeGroup) { AICF_VPClose(); return; }
			m_VehicleProbeGroup.SetLifecyclePolicy(SCR_EAIGroupLifecyclePolicy.Manual);
			m_VehicleProbeGroup.SetDeleteWhenEmpty(false);
			slot.BeginInitialSpawn();
			slot.BindSpawnedGroup(m_VehicleProbeGroup);
			m_GroupSpawner.BeginRosterSpawn(m_VehicleProbeGroup, m_iVehicleProbeExpectedMembers);
			AICF_VPCheck("REPLACEMENT_REJECTS_OLD_TRIP", !trip.IsVehiclePresetCurrent());
			m_bVehicleProbeReplacing = true;
			return;
		}
	}

	protected void AICF_VPClose()
	{
		m_bVehicleProbeFinished = true;
		if (m_VehicleProbeCoordinator) m_VehicleProbeCoordinator.StopWithFactionContexts(true, null, null, m_VehicleProbeState, m_USSRFaction);
		if (m_VehicleProbeGroup)
		{
			array<AIAgent> agents = {};
			m_VehicleProbeGroup.GetAgents(agents);
			foreach (AIAgent agent : agents)
				if (agent.GetControlledEntity() && AICF_VehicleBoardingMutationFence.IsAuthoritativeAIEntity(agent.GetControlledEntity())) SCR_EntityHelper.DeleteEntityAndChildren(agent.GetControlledEntity());
			SCR_EntityHelper.DeleteEntityAndChildren(m_VehicleProbeGroup);
		}
		m_VehicleProbeCoordinator = null;
		if (m_VehicleProbeOldGroup)
		{
			array<AIAgent> oldAgents = {};
			m_VehicleProbeOldGroup.GetAgents(oldAgents);
			foreach (AIAgent oldAgent : oldAgents)
				if (AICF_VehicleBoardingMutationFence.IsAuthoritativeAIEntity(oldAgent.GetControlledEntity())) SCR_EntityHelper.DeleteEntityAndChildren(oldAgent.GetControlledEntity());
			SCR_EntityHelper.DeleteEntityAndChildren(m_VehicleProbeOldGroup);
			m_VehicleProbeOldGroup = null;
		}
		m_VehicleProbeGroup = null;
		m_VehicleProbeState = null;
		Print(string.Format("[AICF][VEHICLE_PRESET_DONE] checks=%1 failures=%2", m_iVehicleProbeChecks, m_iVehicleProbeFailures));
		if (m_VehicleProbeHumanPlayer) SCR_GameModeCampaign.Cast(m_Campaign).AICF_VPRequestClose(10000);
		else GetGame().RequestClose();
	}
}

modded class SCR_PlayerController
{
	void AICF_VPHumanFinished() { Rpc(RpcDo_AICFVPHumanFinished); }
	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	protected void RpcDo_AICFVPHumanFinished() { SCR_GameModeCampaign.Cast(GetGame().GetGameMode()).AICF_VPRequestClose(2000); }
}

modded class SCR_GameModeCampaign
{
	void AICF_VPRequestClose(int delay) { GetGame().GetCallqueue().CallLater(AICF_VPFinishClose, delay, false); }
	protected void AICF_VPFinishClose() { GetGame().RequestClose(); }
	override void OnGameEnd()
	{
		GetGame().GetCallqueue().Remove(AICF_VPFinishClose);
		super.OnGameEnd();
	}
}
