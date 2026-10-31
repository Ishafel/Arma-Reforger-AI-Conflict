// Isolated targeted native probe: собственный roster, real paid vehicle,
// native threat injection, одна настоящая потеря водителя. Не normal war/paid recruitment.
modded class AICF_VehicleCleanupManager
{
	string AICF_VBIncapGuards(AICF_VehicleLease lease, ChimeraCharacter character)
	{
		AICF_VehicleCleanupJob job = FindJobByLease(lease);
		if (!job) return "job=0";
		Vehicle vehicle;
		string rpl;
		string reason;
		bool vehicleResolved = ResolveExpectedVehicle(job, vehicle, rpl, reason);
		AIAgent agent;
		if (character && character.GetAIControlComponent()) agent = character.GetAIControlComponent().GetControlAIAgent();
		bool rosterCurrent = job.m_RecoveryRoster && job.m_RecoveryRoster.IsCurrent(job, vehicle, agent, character);
		bool eligible = IsCurrentDetachedHullMember(job, vehicle, agent, character);
		float speed = -1;
		if (vehicle && vehicle.GetPhysics()) speed = vehicle.GetPhysics().GetVelocity().Length();
		string details = string.Format("job=1 job_current=%1 roster_present=%2 roster_current=%3 vehicle_resolved=%4 vehicle_reason=%5 speed_mps=%6 eligible=%7 attempts=%8",
			IsJobCurrent(job), job.m_RecoveryRoster != null, rosterCurrent, vehicleResolved, reason, speed, eligible, job.m_iManagedExactRecoveryAttempts);
		details += string.Format(" deadline_tick=%1 now_tick=%2 completed=%3 fail_closed=%4 group=%5 character=%6 agent=%7",
			job.m_iProtectedClearanceDeadlineAtMs, System.GetTickCount(), job.m_bCompleted, job.m_bFailClosed,
			AICF_GroupTrace.Entity(job.m_Group), AICF_GroupTrace.Entity(character), AICF_GroupTrace.Entity(agent));
		if (job.m_Scan) details += string.Format(" scan_available=%1 protected_occupants=%2 player_transitions=%3 managed_logical=%4 managed_transitions=%5 nearby_players=%6 unknown_position=%7",
			job.m_Scan.m_bPlayerScanAvailable, job.m_Scan.m_iProtectedOccupants, job.m_Scan.m_iPlayerTransitions,
			job.m_Scan.m_iManagedLogicalOccupants, job.m_Scan.m_iManagedTransitions, job.m_Scan.m_iNearbyPlayers,
			job.m_Scan.m_sGlobalSamples.Contains("position_unknown"));
		if (job.m_RecoveryRoster) details += " " + job.m_RecoveryRoster.AICF_VBFenceDetails(job, vehicle, agent, character);
		return details + " readonly=1";
	}
	int AICF_VBExactAttempts(AICF_VehicleLease lease)
	{
		AICF_VehicleCleanupJob job = FindJobByLease(lease);
		if (!job) return -1;
		return job.m_iManagedExactRecoveryAttempts;
	}
	int AICF_VBDeadline(AICF_VehicleLease lease)
	{
		AICF_VehicleCleanupJob job = FindJobByLease(lease);
		if (!job) return -1;
		return job.m_iProtectedClearanceDeadlineAtMs;
	}
}

modded class AICF_VehicleCleanupRecoveryRoster
{
	string AICF_VBFenceDetails(AICF_VehicleCleanupJob job, Vehicle vehicle, AIAgent agent, ChimeraCharacter character)
	{
		bool slotGroup = m_Slot && m_Slot.GetGroup() == m_Group;
		bool slotGeneration = m_Slot && m_Slot.GetSpawnGeneration() == m_iGroupGeneration;
		bool groupEntity = m_Group && m_Group.GetID() == m_GroupId;
		bool groupRpl = m_Group && ComponentRpl(m_Group) == m_sGroupRpl;
		bool faction = m_Group && m_Group.GetFaction() && m_Group.GetFaction().GetFactionKey() == m_sFaction;
		bool vehicleEntity = vehicle && vehicle.GetID() == m_VehicleId;
		bool vehicleRpl = vehicle && ComponentRpl(vehicle) == m_sVehicleRpl;
		string details = string.Format("slot_group=%1 slot_generation=%2 group_entity=%3 group_rpl=%4 faction=%5 vehicle_entity=%6 vehicle_rpl=%7",
			slotGroup, slotGeneration, groupEntity, groupRpl, faction, vehicleEntity, vehicleRpl);
		details += string.Format(" fence_group_gen=%1 fence_trip_gen=%2 fence_lease_gen=%3 fence_vehicle_gen=%4",
			job.m_Fence.GetGroupGeneration() == m_iGroupGeneration, job.m_Fence.GetTripGeneration() == m_iTripGeneration,
			job.m_Fence.GetLeaseGeneration() == m_iLeaseGeneration, job.m_Fence.GetVehicleGeneration() == m_iVehicleGeneration);
		bool found;
		foreach (AICF_VehicleCleanupRecoveryMember member : m_aMembers)
		{
			if (member.m_Character != character) continue;
			found = true;
			details += string.Format(" actor_reference=1 actor_id=%1 actor_rpl=%2 agent_reference=%3 control_entity=%4 member_matches=%5",
				character && character.GetID() == member.m_Id, character && ComponentRpl(character) == member.m_sRpl,
				agent == member.m_Agent, agent && agent.GetControlledEntity() == character, member.Matches(agent, character));
		}
		if (!found) details += " actor_reference=0";
		return details;
	}
}

modded class AICF_VehicleCoordinator
{
	bool AICF_VBStartCleanup(AICF_GroupSlot slot, SCR_CampaignFaction faction)
	{
		AICF_TransportTrip trip = GetTrip(slot);
		AICF_FactionFleet fleet = m_Fleets.FindFleet(faction.GetFactionKey());
		if (!trip || !trip.GetLease() || !fleet) return false;
		m_TripController.AbortForStop(trip, slot, fleet, faction, true, "CONTROLLED_INCAP_HULL_CLEANUP");
		AICF_VehicleCleanupOutcome outcome = m_CleanupManager.BeginStopLease(trip, fleet, trip.GetLease(), System.GetTickCount());
		return outcome && outcome.IsAccepted();
	}
	void AICF_VBPollCleanup(SCR_CampaignFaction faction, int nowMs)
	{
		m_CleanupManager.UpdateFleet(m_Fleets.FindFleet(faction.GetFactionKey()), nowMs);
	}
	AICF_VehicleCleanupQuery AICF_VBCleanupReadback(AICF_VehicleLease lease)
	{
		return m_CleanupManager.QueryLease(lease);
	}
	int AICF_VBCleanupAttempts(AICF_VehicleLease lease) { return m_CleanupManager.AICF_VBExactAttempts(lease); }
	int AICF_VBCleanupDeadline(AICF_VehicleLease lease) { return m_CleanupManager.AICF_VBDeadline(lease); }
	string AICF_VBCleanupGuardReadback(AICF_VehicleLease lease, ChimeraCharacter character) { return m_CleanupManager.AICF_VBIncapGuards(lease, character); }
}

modded class AICF_MatchController
{
	protected string m_sVBScenario;
	protected string m_sVBFaction = "USSR";
	protected SCR_CampaignFaction m_VBFaction;
	protected ref AICF_FactionState m_VBState;
	protected ref AICF_VehicleCoordinator m_VBCoordinator;
	protected SCR_AIGroup m_VBGroup;
	protected EntityID m_VBGroupId;
	protected ref AICF_VehiclePreset m_VBRecipe;
	protected ref AICF_TransportTrip m_VBTrip;
	protected Vehicle m_VBVehicle;
	protected EntityID m_VBVehicleId;
	protected int m_iVBLeaseGeneration;
	protected int m_iVBOverrideIntent = -1;
	protected IEntity m_VBDeadDriver;
	protected EntityID m_VBDeadDriverId;
	protected bool m_bVBReboardPilotLossProbe;
	protected bool m_bVBReboardPilotLossInjected;
	protected bool m_bVBReboardKillReported;
	protected int m_iVBReboardKillSubmittedAt;
	protected int m_iVBReboardKillPolls;
	protected IEntity m_VBReboardVictim;
	protected EntityID m_VBReboardVictimId;
	protected IEntity m_VBReboardGunner;
	protected EntityID m_VBReboardGunnerId;
	protected int m_iVBOriginalReboardDeadline;
	protected float m_fVBReboardGunnerHealth;
	protected bool m_bVBNativeRouteLossProbe;
	protected bool m_bVBNativeRouteLossActive;
	protected int m_iVBRouteLossRemoved;
	protected int m_iVBRouteLossRebuilt;
	protected int m_iVBRouteLossRemovedAt;
	protected int m_iVBRouteLossGeneration;
	protected EntityID m_VBRouteLossWaypointId;
	protected int m_iVBRouteLossDeadline;
	protected int m_iVBRouteLossTripGeneration;
	protected int m_iVBRouteLossVehicleGeneration;
	protected AICF_VehicleLease m_VBRouteLossLease;
	protected int m_iVBRouteLossAttemptsBeforeExit = -1;
	protected IEntity m_VBInjuredMember;
	protected float m_fVBInjuredHealth;
	protected int m_iVBSize = 5;
	protected int m_iVBStep;
	protected int m_iVBStarted;
	protected int m_iVBPhaseStarted;
	protected int m_iVBChecks;
	protected int m_iVBFailures;
	protected bool m_bVBDone;
	protected bool m_bVBThreat;
	protected bool m_bVBObserver;
	protected ObserversSystem m_VBObservers;
	protected vector m_vVBHome;
	protected vector m_vVBMovementStart;
	protected ref array<IEntity> m_aVBMembers = {};
	protected ref array<EntityID> m_aVBMemberIds = {};
	protected ref array<IEntity> m_aVBCrew = {};
	protected ref array<BaseCompartmentSlot> m_aVBCrewSeats = {};
	protected static const int AICF_VB_OBSERVER_KEY = -1513180001;
	protected bool m_bVBCleanupIncapActive;
	protected bool m_bVBCleanupAliveMode;
	protected bool m_bVBCleanupOldMovementDisabled;
	protected int m_iVBCleanupIncapStep;
	protected int m_iVBCleanupDeadline;
	protected ref AICF_VehicleLease m_VBCleanupLease;
	protected vector m_vVBCleanupGhost;
	protected float m_fVBCleanupHealth;
	protected float m_fVBCleanupResilience;
	protected int m_iVBCleanupIncapStarted;
	protected int m_iVBCleanupGuardReported;

	protected void AICF_VBCheck(string name, bool passed, string details = "")
	{
		m_iVBChecks++;
		if (!passed) m_iVBFailures++;
		Print(string.Format("[AICF][VEHICLE_BEHAVIOR_CASE] case=%1 passed=%2 details=[scenario=%3 %4]",
			name, passed, m_sVBScenario, details));
	}

	protected bool AICF_VBInitialize()
	{
		string aliveCleanupFlag;
		bool aliveProbe = System.GetCLIParam("aicfCleanupAliveProbe", aliveCleanupFlag) && aliveCleanupFlag == "1";
		string routeLossFlag;
		m_bVBNativeRouteLossProbe = System.GetCLIParam("aicfNativeRouteLossProbe", routeLossFlag) && routeLossFlag == "1";
		string pilotLossFlag;
		m_bVBReboardPilotLossProbe = System.GetCLIParam("aicfReboardPilotLossProbe", pilotLossFlag) && pilotLossFlag == "1";
		if (m_bVBNativeRouteLossProbe)
		{
			string routeCleanupFlag;
			if (m_sVBScenario != "combat" || m_bVBReboardPilotLossProbe || aliveProbe ||
				(System.GetCLIParam("aicfCleanupIncapProbe", routeCleanupFlag) && routeCleanupFlag == "1")) return false;
		}
		if (m_bVBReboardPilotLossProbe)
		{
			string cleanupFlag;
			if (m_sVBScenario != "combat" || aliveProbe || (System.GetCLIParam("aicfCleanupIncapProbe", cleanupFlag) && cleanupFlag == "1")) return false;
			m_iVBSize = 3;
		}
		System.GetCLIParam("aicfVehicleBehaviorFaction", m_sVBFaction);
		if (m_sVBFaction != "US" && m_sVBFaction != "USSR") return false;
		m_VBFaction = m_USSRFaction;
		if (m_sVBFaction == "US") m_VBFaction = m_USFaction;
		if (!m_VBFaction || !m_VBFaction.GetMainBase() || !m_VBFaction.GetMainBase().GetSpawnPoint()) return false;
		AICF_VehicleBehaviorSettingsProbe settings = new AICF_VehicleBehaviorSettingsProbe();
		AICF_VBCheck("SETTINGS_NATIVE", settings.Run() == 0);
		SCR_FactionManager manager = SCR_FactionManager.Cast(GetGame().GetFactionManager());
		if (!manager) return false;
		array<Faction> factions = {};
		manager.GetFactionsList(factions);
		bool friendly = true;
		for (int first; first < factions.Count(); first++)
			for (int second = first + 1; second < factions.Count(); second++)
			{
				SCR_Faction a = SCR_Faction.Cast(factions[first]);
				SCR_Faction b = SCR_Faction.Cast(factions[second]);
				if (!a || !b) continue;
				manager.SetFactionsFriendly(a, b);
				if (!a.IsFactionFriendly(b) || !b.IsFactionFriendly(a)) friendly = false;
			}
		AICF_VBCheck("CONTROLLED_WORLD_TRUCE", friendly, "normal_war=0 damage_protection=0 paid_recruitment=0");
		if (!friendly) return false;
		if (m_sVBScenario == "combat" && m_sVBFaction == "USSR") m_iVBSize = 3;
		m_VBState = new AICF_FactionState(m_VBFaction.GetFactionKey(), m_Config);
		AICF_GroupSlot slot = m_VBState.GetSlot(0);
		slot.SetDesiredSize(m_iVBSize);
		slot.SetUnitType(AICF_EGroupUnitType.MOTORIZED_LIGHT);
		m_VBGroup = m_GroupSpawner.SpawnGroup(m_VBFaction, m_VBFaction.GetMainBase(), 0, m_iVBSize, false);
		AICF_VBCheck("GROUP_CREATED", m_VBGroup != null);
		if (!m_VBGroup) return false;
		m_VBGroupId = m_VBGroup.GetID();
		m_VBGroup.SetLifecyclePolicy(SCR_EAIGroupLifecyclePolicy.Manual);
		m_VBGroup.SetDeleteWhenEmpty(false);
		slot.BeginInitialSpawn();
		slot.BindSpawnedGroup(m_VBGroup);
		m_GroupSpawner.BeginRosterSpawn(m_VBGroup, m_iVBSize);
		m_VBCoordinator = new AICF_VehicleCoordinator(m_Stage3Config, m_Campaign, m_ConflictAdapter, m_OrderPlanner,
			m_GroupCohesionPolicy, m_ObjectiveGraph, m_TargetSelector, m_ContentProfile);
		m_vVBHome = m_VBFaction.GetMainBase().GetSpawnPoint().GetOrigin();
		return true;
	}

	protected bool AICF_VBPrepare(AICF_GroupSlot slot)
	{
		if (m_VBGroup.GetAgentsCount() != m_iVBSize) return false;
		SCR_AIGroupUtilityComponent utility = m_VBGroup.GetGroupUtilityComponent();
		if (!utility) return false;
		utility.SetCombatMode(EAIGroupCombatMode.HOLD_FIRE);
		slot.MarkReady();
		slot.RecordDeploymentMembers();
		AICF_VBCheck("ROSTER_READY_IDENTITIES", AICF_GroupRuntime.CountAliveAgents(m_VBGroup) == m_iVBSize);
		m_GroupCohesionPolicy.Apply(m_VBGroup);
		for (int member; member < m_iVBSize; member++)
		{
			IEntity actor = slot.GetRosterMember(member);
			if (!AICF_VehicleBoardingMutationFence.IsAuthoritativeAIEntity(actor)) return false;
			m_aVBMembers.Insert(actor);
			m_aVBMemberIds.Insert(actor.GetID());
		}
		AICF_VehiclePresetCatalog catalog = new AICF_VehiclePresetCatalog(m_VBFaction);
		JsonLoadContext list = new JsonLoadContext();
		array<ResourceName> prefabs = {};
		if (!list.LoadFromString(catalog.List()) || !list.ReadValue("prefabs", prefabs)) { catalog.Clear(); return false; }
		ResourceName selected;
		string reason;
		foreach (ResourceName prefab : prefabs)
		{
			if (catalog.Source(prefab) != AICF_EVehicleCatalogSource.FACTION) continue;
			if (m_sVBFaction == "USSR")
			{
				if (m_sVBScenario == "combat" && !prefab.EndsWith("/UAZ469_PKM.et")) continue;
				if (m_sVBScenario == "transport" && !prefab.EndsWith("/UAZ469.et")) continue;
			}
			else if (!prefab.Contains("/M1025_") || !prefab.Contains("M2HB")) continue;
			if (!catalog.Inspect(prefab, reason) || catalog.Cost(prefab) <= 0) continue;
			if (m_bVBReboardPilotLossProbe)
			{
				if (catalog.Seats().Count() < m_iVBSize) continue;
			}
			else if (catalog.Seats().Count() != m_iVBSize) continue;
			selected = prefab;
			break;
		}
		AICF_VBCheck("MODEL_NATIVE_TOPOLOGY", !selected.IsEmpty(), string.Format("faction=%1 size=%2 prefab=%3", m_sVBFaction, m_iVBSize, selected));
		if (selected.IsEmpty()) { catalog.Clear(); return false; }
		m_VBRecipe = catalog.Draft(selected);
		m_VBRecipe.m_iBehavior = AICF_EVehicleBehavior.TRANSPORT;
		if (m_sVBScenario == "combat") m_VBRecipe.m_iBehavior = AICF_EVehicleBehavior.COMBAT;
		m_VBRecipe.m_iReturnDistance = 300;
		int position;
		for (int mandatory; mandatory < catalog.Seats().Count(); mandatory++)
			if (AICF_VehicleBehaviorFlow.IsCrewSeat(catalog.Seats()[mandatory])) m_VBRecipe.Assign(mandatory, position++);
		for (int seat; seat < catalog.Seats().Count() && position < m_iVBSize; seat++)
			if (m_VBRecipe.m_aMembers[seat] < 0) m_VBRecipe.Assign(seat, position++);
		bool valid = m_VBRecipe.ValidateRoster(slot, catalog.Seats(), reason) && slot.SetVehiclePreset(m_VBRecipe, slot.GetVehiclePresetRevision());
		catalog.Clear();
		AICF_VBCheck("PRESET_APPLIED_MODE300", valid, reason);
		if (!valid) return false;
		AICF_VBCheck("INITIAL_POINT_ORDER", m_OrderPlanner.AssignPlayerPointOrder(slot, m_VBFaction, m_vVBHome, false));
		return true;
	}

	protected void AICF_VBThreat(bool enabled)
	{
		if (!m_VBGroup) return;
		array<AIAgent> agents = {};
		m_VBGroup.GetAgents(agents);
		foreach (AIAgent agent : agents)
		{
			if (!agent || !AICF_GroupRuntime.IsAliveCharacter(agent.GetControlledEntity())) continue;
			SCR_AIUtilityComponent utility = SCR_AIUtilityComponent.Cast(agent.FindComponent(SCR_AIUtilityComponent));
			if (!utility || utility.m_OwnerEntity != agent.GetControlledEntity() || !utility.m_ThreatSystem) continue;
			float suppression;
			if (enabled) suppression = 1;
			utility.m_ThreatSystem.SetThreatValues(suppression, 0, 0, 0);
			utility.m_ThreatSystem.Update(utility, 0.001);
		}
	}

	protected bool AICF_VBExact(AICF_TransportTrip trip)
	{
		if (!trip || !trip.GetLease() || !m_VBVehicle || trip.GetLease().GetVehicle() != m_VBVehicle || m_VBVehicle.GetID() != m_VBVehicleId) return false;
		array<BaseCompartmentSlot> seats = {};
		AICF_VehiclePreset.Collect(m_VBVehicle, seats);
		AICF_VehiclePreset recipe = trip.GetVehiclePreset();
		if (!recipe || !recipe.MatchesSeats(seats)) return false;
		int expected;
		int actual;
		for (int index; index < seats.Count(); index++)
		{
			IEntity actor = trip.VehiclePresetMember(recipe.m_aMembers[index]);
			if (!AICF_GroupRuntime.IsAliveCharacter(actor)) continue;
			expected++;
			ChimeraCharacter character = ChimeraCharacter.Cast(actor);
			CompartmentAccessComponent access = character.GetCompartmentAccessComponent();
			if (seats[index].GetOccupant() == actor && access && access.GetCompartment() == seats[index] &&
				CompartmentAccessComponent.GetVehicleIn(actor) == m_VBVehicle && character.IsInVehicle() &&
				!access.IsGettingIn() && !access.IsGettingOut() && !access.IsSwitchingSeatsAnim()) actual++;
		}
		int survivors = m_iVBSize;
		if (m_VBDeadDriver) survivors--;
		if (m_VBReboardVictim) survivors--;
		return expected == survivors && actual == survivors && AICF_GroupRuntime.CountAliveAgents(m_VBGroup) == survivors;
	}

	// Causal contract injection только в fixture: mutable allocation flag не
	// означает удаления compartment. Предыдущее значение возвращается сразу.
	protected bool AICF_VBNativeAllocation(AICF_TransportTrip trip)
	{
		array<BaseCompartmentSlot> original = {};
		AICF_VehiclePreset.Collect(m_VBVehicle, original);
		AICF_VehiclePreset preset = trip.GetVehiclePreset();
		if (original.IsEmpty() || !preset.MatchesSeats(original)) return false;
		BaseCompartmentSlot selected = original[0];
		IEntity occupant = selected.GetOccupant();
		bool accessibleBefore = selected.IsCompartmentAccessible();
		selected.SetCompartmentAccessible(false);
		array<BaseCompartmentSlot> oldFiltered = {};
		AICF_VehiclePreset.Collect(m_VBVehicle, oldFiltered);
		array<BaseCompartmentSlot> captured = {};
		bool resolved = AICF_VehiclePreset.CollectCaptured(m_VBVehicle, preset, captured);
		bool flagPreserved = !selected.IsCompartmentAccessible();
		bool sameOrder = captured.Count() == original.Count();
		for (int index; sameOrder && index < original.Count(); index++)
			if (captured[index] != original[index]) sameOrder = false;
		selected.SetCompartmentAccessible(accessibleBefore);
		bool unchanged = selected.GetOccupant() == occupant && selected.IsCompartmentAccessible() == accessibleBefore;
		AICF_VBCheck("NATIVE_ALLOCATION_NOT_TOPOLOGY_CHANGE", accessibleBefore && oldFiltered.Count() == original.Count() - 1 &&
			resolved && sameOrder && flagPreserved && unchanged,
			string.Format("raw_captured=%1 old_filtered=%2 temporarily_inaccessible=1 restored_flag=%3 occupant_unchanged=%4 controlled_injection=1 organic_soak_cause_proven=0",
			captured.Count(), oldFiltered.Count(), selected.IsCompartmentAccessible() == accessibleBefore, selected.GetOccupant() == occupant));
		return resolved && sameOrder && flagPreserved && unchanged;
	}

	protected bool AICF_VBDeployed(AICF_TransportTrip trip)
	{
		array<BaseCompartmentSlot> seats = {};
		AICF_VehiclePreset.Collect(m_VBVehicle, seats);
		AICF_VehiclePreset recipe = trip.GetVehiclePreset();
		if (!recipe.MatchesSeats(seats)) return false;
		int deployed;
		for (int index; index < seats.Count(); index++)
		{
			IEntity actor = trip.VehiclePresetMember(recipe.m_aMembers[index]);
			if (!AICF_GroupRuntime.IsAliveCharacter(actor)) continue;
			bool crew = AICF_VehicleBehaviorFlow.IsCrewSeat(seats[index]);
			if (m_sVBScenario == "combat" && crew) continue;
			deployed++;
			ChimeraCharacter character = ChimeraCharacter.Cast(actor);
			CompartmentAccessComponent access = character.GetCompartmentAccessComponent();
			if (!access || character.IsInVehicle() || access.IsInCompartment() || access.IsGettingIn() || access.IsGettingOut() ||
				CompartmentAccessComponent.GetVehicleIn(actor)) return false;
		}
		return deployed > 0;
	}

	protected bool AICF_VBCrewRetained()
	{
		for (int index; index < m_aVBCrew.Count(); index++)
		{
			IEntity actor = m_aVBCrew[index];
			BaseCompartmentSlot seat = m_aVBCrewSeats[index];
			if (!AICF_GroupRuntime.IsAliveCharacter(actor) || seat.GetOccupant() != actor || CompartmentAccessComponent.GetVehicleIn(actor) != m_VBVehicle) return false;
		}
		return !m_aVBCrew.IsEmpty();
	}

	protected IEntity AICF_VBPilot()
	{
		array<BaseCompartmentSlot> seats = {};
		AICF_VehiclePreset.Collect(m_VBVehicle, seats);
		foreach (BaseCompartmentSlot seat : seats)
			if (seat.GetType() == ECompartmentType.PILOT) return seat.GetOccupant();
		return null;
	}

	protected bool AICF_VBIdentities()
	{
		if (!m_VBGroup || m_VBGroup.GetID() != m_VBGroupId) return false;
		for (int index; index < m_aVBMembers.Count(); index++)
		{
			IEntity actor = m_aVBMembers[index];
			if (!actor || actor.GetID() != m_aVBMemberIds[index]) return false;
			if (actor == m_VBDeadDriver) continue;
			if (actor == m_VBReboardVictim && m_bVBReboardPilotLossInjected && !AICF_GroupRuntime.IsAliveCharacter(actor)) continue;
			if (!AICF_VehicleShortRecovery.IsCurrentMember(m_VBTrip, actor)) return false;
		}
		return true;
	}

	protected bool AICF_VBFarTarget(out vector target)
	{
		SCR_AIWorld world = SCR_AIWorld.Cast(GetGame().GetAIWorld());
		AIPathfindingComponent path = AIPathfindingComponent.Cast(m_VBGroup.FindComponent(AIPathfindingComponent));
		if (!world || !world.GetRoadNetworkManager() || !path || !path.GetNavmeshComponent()) return false;
		vector origin = m_VBVehicle.GetOrigin();
		array<BaseRoad> roads = {};
		world.GetRoadNetworkManager().GetRoadsInAABB(origin - "1000 1000 1000", origin + "1000 1000 1000", roads);
		float best = 900;
		bool found;
		foreach (BaseRoad road : roads)
		{
			if (!road) continue;
			array<vector> points = {};
			road.GetPoints(points);
			foreach (vector point : points)
			{
				float distance = vector.DistanceXZ(origin, point);
				if (distance <= 350 || distance >= best) continue;
				path.GetNavmeshComponent().LoadTileIn(point);
				vector nav;
				if (!path.GetClosestPositionOnNavmesh(point, "20 10 20", nav) ||
					ChimeraWorldUtils.TryGetWaterSurfaceSimple(GetGame().GetWorld(), nav - "0 0.5 0")) continue;
				target = nav;
				best = distance;
				found = true;
			}
		}
		return found;
	}

	protected void AICF_VBObserve()
	{
		if (!m_VBGroup || m_bVBDone) return;
		IEntity leader = AICF_GroupRuntime.ResolveAliveLeader(m_VBGroup);
		if (!leader) return;
		if (!m_VBObservers) m_VBObservers = ObserversSystem.Cast(GetGame().GetWorld().FindSystem(ObserversSystem));
		if (!m_VBObservers) return;
		m_VBObservers.InsertObserverSP(AICF_VB_OBSERVER_KEY, 0, 0, leader);
		m_bVBObserver = true;
	}

	protected void AICF_VBStopObserver()
	{
		if (m_VBObservers && m_bVBObserver) m_VBObservers.RemoveObserverSP(AICF_VB_OBSERVER_KEY);
		m_bVBObserver = false;
		m_VBObservers = null;
	}

	// Опциональная инъекция выполняется перед очередным production coordinator Tick.
	// Единственная новая мутация — настоящая потеря текущего replacement Pilot.
	protected void AICF_VBTryReboardPilotLoss()
	{
		if (!m_bVBReboardPilotLossProbe || m_iVBStep != 4 || m_sVBScenario != "combat") return;
		if (m_bVBReboardPilotLossInjected)
		{
			AICF_VBObserveReboardPilotDeath();
			return;
		}
		AICF_TransportTrip trip = m_VBCoordinator.GetTrip(m_VBState.GetSlot(0));
		if (trip != m_VBTrip || !trip || trip.IsTerminal() || !trip.IsVehiclePresetCurrent() ||
			trip.GetPhase() != AICF_ETransportTripPhase.TACTICAL_DEPLOYMENT || !trip.GetBehaviorState().m_bReboarding) return;
		AICF_VehicleBehaviorState state = trip.GetBehaviorState();
		if (!trip.GetLease() || trip.GetLease().GetVehicle() != m_VBVehicle || m_VBVehicle.GetID() != m_VBVehicleId ||
			trip.GetLease().GetLeaseGeneration() != m_iVBLeaseGeneration || state.m_iReboardDeadline <= System.GetTickCount() ||
			!m_VBDeadDriver || AICF_GroupRuntime.IsAliveCharacter(m_VBDeadDriver)) return;
		array<BaseCompartmentSlot> seats = {};
		AICF_VehiclePreset preset = trip.GetVehiclePreset();
		if (!AICF_VehiclePreset.CollectCaptured(m_VBVehicle, preset, seats)) return;
		IEntity replacement;
		BaseCompartmentSlot pilot;
		IEntity gunner;
		for (int index; index < seats.Count(); index++)
		{
			if (seats[index].GetType() == ECompartmentType.PILOT)
			{
				pilot = seats[index];
				replacement = trip.VehiclePresetMember(preset.m_aMembers[index]);
			}
			else if (AICF_VehicleBehaviorFlow.IsCrewSeat(seats[index])) gunner = seats[index].GetOccupant();
		}
		if (!pilot || !AICF_VehicleBehaviorFlow.IsFit(replacement) || !AICF_VehicleShortRecovery.IsCurrentMember(trip, replacement) ||
			!AICF_VehicleBehaviorFlow.IsFit(gunner) || !AICF_VehicleShortRecovery.IsCurrentMember(trip, gunner) ||
			!m_aVBMembers.Contains(replacement) || !m_aVBMembers.Contains(gunner) || replacement == gunner || replacement == m_VBDeadDriver) return;
		CompartmentAccessComponent access = ChimeraCharacter.Cast(replacement).GetCompartmentAccessComponent();
		if (!access || AICF_VehicleBehaviorFlow.HasMobilePilot(trip, seats, true)) return;
		SCR_CharacterDamageManagerComponent damage = SCR_CharacterDamageManagerComponent.Cast(replacement.FindComponent(SCR_CharacterDamageManagerComponent));
		SCR_CharacterDamageManagerComponent gunnerDamage = SCR_CharacterDamageManagerComponent.Cast(gunner.FindComponent(SCR_CharacterDamageManagerComponent));
		if (!damage || !gunnerDamage || AICF_GroupRuntime.CountAliveAgents(m_VBGroup) != 2) return;
		m_VBReboardVictim = replacement;
		m_VBReboardVictimId = replacement.GetID();
		m_VBReboardGunner = gunner;
		m_VBReboardGunnerId = gunner.GetID();
		m_iVBOriginalReboardDeadline = state.m_iReboardDeadline;
		m_fVBReboardGunnerHealth = gunnerDamage.GetHealthScaled();
		AICF_VBCheck("REBOARD_PILOT_LOSS_PHASE", true, string.Format(
			"replacement=%1 gunner=%2 reboarding=1 pilot_settled=0 getting_in=%3 getting_out=%4 deadline=%5 now_tick=%6 controlled_setup=1",
			m_VBReboardVictimId, m_VBReboardGunnerId, access.IsGettingIn(), access.IsGettingOut(), m_iVBOriginalReboardDeadline, System.GetTickCount()));
		damage.Kill(Instigator.CreateInstigatorGM());
		m_bVBReboardPilotLossInjected = true;
		m_iVBReboardKillSubmittedAt = System.GetTickCount();
	}

	// Kill обновляет native life state асинхронно. Эта ветвь вызывается только
	// следующим Update, не повторяет Kill и не меняет исходный deadline.
	protected void AICF_VBObserveReboardPilotDeath()
	{
		if (!m_bVBReboardPilotLossInjected || m_bVBReboardKillReported) return;
		m_iVBReboardKillPolls++;
		bool current = m_VBReboardVictim && m_VBReboardVictim.GetID() == m_VBReboardVictimId &&
			m_VBGroup && m_VBGroup.GetID() == m_VBGroupId && m_VBTrip && !m_VBTrip.IsTerminal() &&
			m_VBTrip.GetLease() && m_VBTrip.GetLease().GetVehicle() == m_VBVehicle &&
			m_VBTrip.GetLease().GetLeaseGeneration() == m_iVBLeaseGeneration &&
			m_VBTrip.GetBehaviorState().m_iReboardDeadline == m_iVBOriginalReboardDeadline;
		int nowMs = System.GetTickCount();
		if (current && (nowMs <= m_iVBReboardKillSubmittedAt || AICF_GroupRuntime.IsAliveCharacter(m_VBReboardVictim)) &&
			nowMs < m_iVBOriginalReboardDeadline) return;
		m_bVBReboardKillReported = true;
		AICF_VBCheck("REBOARD_PILOT_LOSS_NATIVE", current && nowMs > m_iVBReboardKillSubmittedAt &&
			!AICF_GroupRuntime.IsAliveCharacter(m_VBReboardVictim) && nowMs < m_iVBOriginalReboardDeadline,
			string.Format("member=%1 alive_after_kill=%2 original_dead=%3 survivor_alive=%4 elapsed_ms=%5 later_update=1 polls=%6 deadline=%7",
				m_VBReboardVictimId, AICF_GroupRuntime.IsAliveCharacter(m_VBReboardVictim), !AICF_GroupRuntime.IsAliveCharacter(m_VBDeadDriver),
				AICF_GroupRuntime.IsAliveCharacter(m_VBReboardGunner), nowMs - m_iVBReboardKillSubmittedAt, m_iVBReboardKillPolls, m_iVBOriginalReboardDeadline));
	}

	protected bool AICF_VBReboardPilotRecovered(AICF_TransportTrip trip)
	{
		if (!m_bVBReboardPilotLossInjected || !m_bVBReboardKillReported || !m_VBReboardGunner || m_VBReboardGunner.GetID() != m_VBReboardGunnerId ||
			!m_VBReboardVictim || m_VBReboardVictim.GetID() != m_VBReboardVictimId || AICF_GroupRuntime.IsAliveCharacter(m_VBReboardVictim) ||
			trip.GetBehaviorState().m_iReboardDeadline != m_iVBOriginalReboardDeadline || !AICF_VBExact(trip)) return false;
		array<BaseCompartmentSlot> seats = {};
		AICF_VehiclePreset preset = trip.GetVehiclePreset();
		if (!AICF_VehiclePreset.CollectCaptured(m_VBVehicle, preset, seats) || !AICF_VehicleBehaviorFlow.HasMobilePilot(trip, seats, true)) return false;
		for (int index; index < seats.Count(); index++)
		{
			if (seats[index].GetType() == ECompartmentType.PILOT)
			{
				if (seats[index].GetOccupant() != m_VBReboardGunner || trip.VehiclePresetMember(preset.m_aMembers[index]) != m_VBReboardGunner) return false;
			}
			else if (AICF_VehicleBehaviorFlow.IsCrewSeat(seats[index]) && (preset.m_aMembers[index] >= 0 || seats[index].GetOccupant())) return false;
		}
		SCR_CharacterDamageManagerComponent damage = SCR_CharacterDamageManagerComponent.Cast(m_VBReboardGunner.FindComponent(SCR_CharacterDamageManagerComponent));
		return damage && damage.GetHealthScaled() <= m_fVBReboardGunnerHealth + 0.001 &&
			AICF_GroupRuntime.CountAliveAgents(m_VBGroup) == 1 && AICF_VBIdentities();
	}

	protected void AICF_VBBeginNativeRouteLoss(AICF_TransportTrip trip, int nowMs)
	{
		AICF_VehicleMovementState state = trip.GetMovementState();
		bool ready = trip.GetPhase() == AICF_ETransportTripPhase.TRANSIT && AICF_VBExact(trip) &&
			state.GetMobilityRecoveryAttempts() == 0 && state.GetMaximumMobilityRecoveryAttempts() == 2 &&
			state.GetRouteGeneration() > 0 && state.IsRouteWaypointBound() && state.GetRouteWaypoint() &&
			vector.DistanceXZ(m_VBVehicle.GetOrigin(), state.GetTacticalTarget()) > m_Stage3Config.GetDismountDistanceMeters();
		AICF_VBCheck("NATIVE_ROUTE_LOSS_PRECONDITION", ready, string.Format("mobility_attempts=%1 maximum=2 physical_resume_m=%2 target_distance_m=%3",
			state.GetMobilityRecoveryAttempts(), vector.DistanceXZ(m_vVBMovementStart, m_VBVehicle.GetOrigin()),
			vector.DistanceXZ(m_VBVehicle.GetOrigin(), state.GetTacticalTarget())));
		if (!ready) { AICF_VBFinish(); return; }
		m_bVBNativeRouteLossActive = true;
		m_iVBRouteLossDeadline = trip.GetAbsoluteDeadlineMs();
		m_iVBRouteLossTripGeneration = trip.GetTripGeneration();
		m_iVBRouteLossVehicleGeneration = trip.GetLease().GetVehicleGeneration();
		m_VBRouteLossLease = trip.GetLease();
		AICF_VBNativeRouteLossUpdate(trip, nowMs);
	}

	protected static bool AICF_VBRouteLossExhaustionReceipt(int removals, int rebuilds, int beforeExit, int afterExit,
		bool expectedFallback, int latencyMs)
	{
		return removals == 3 && rebuilds == 2 && beforeExit == 2 && afterExit == 0 &&
			expectedFallback && latencyMs >= 0 && latencyMs < 30000;
	}

	protected void AICF_VBNativeRouteLossUpdate(AICF_TransportTrip trip, int nowMs)
	{
		if (!trip || trip != m_VBTrip || trip.GetTripGeneration() != m_iVBRouteLossTripGeneration ||
			trip.GetAbsoluteDeadlineMs() != m_iVBRouteLossDeadline || !trip.GetLease() ||
			trip.GetLease() != m_VBRouteLossLease ||
			trip.GetLease().GetLeaseGeneration() != m_iVBLeaseGeneration ||
			trip.GetLease().GetVehicleGeneration() != m_iVBRouteLossVehicleGeneration)
		{
			AICF_VBCheck("NATIVE_ROUTE_LOSS_IDENTITY", false);
			AICF_VBFinish();
			return;
		}
		AICF_VehicleMovementState state = trip.GetMovementState();
		if (m_iVBRouteLossRemoved > m_iVBRouteLossRebuilt)
		{
			int latency = nowMs - m_iVBRouteLossRemovedAt;
			if (m_iVBRouteLossRemoved == 3)
			{
				if (!trip.IsTerminal() && latency < 30000) return;
				bool expectedFallback = trip.GetPhase() == AICF_ETransportTripPhase.FALLBACK &&
					trip.GetTerminalReason() == "NATIVE_ROUTE_WAYPOINT_LOST_RECOVERY_EXHAUSTED";
				bool bounded = AICF_VBRouteLossExhaustionReceipt(m_iVBRouteLossRemoved, m_iVBRouteLossRebuilt,
					m_iVBRouteLossAttemptsBeforeExit, state.GetMobilityRecoveryAttempts(), expectedFallback, latency);
				AICF_VBCheck("NATIVE_ROUTE_LOSS_EXHAUSTED", bounded, string.Format("removals=3 rebuilds=%1 attempts=%2 latency_ms=%3 deadline=%4 terminal_reason=%5 before_exit_attempts=%6 after_exit_reset=%7",
					m_iVBRouteLossRebuilt, state.GetMobilityRecoveryAttempts(), latency, trip.GetAbsoluteDeadlineMs(), trip.GetTerminalReason(),
					m_iVBRouteLossAttemptsBeforeExit, state.GetMobilityRecoveryAttempts() == 0));
				AICF_VBFinish();
				return;
			}
			array<AIWaypoint> actual = {};
			m_VBGroup.GetWaypoints(actual);
			AIWaypoint route = state.GetRouteWaypoint();
			bool rebuilt = trip.GetPhase() == AICF_ETransportTripPhase.TRANSIT && route && actual.Contains(route) &&
				route.GetID() != m_VBRouteLossWaypointId && state.GetRouteGeneration() == m_iVBRouteLossGeneration + 1 &&
				state.IsRouteWaypointBound() && state.GetMobilityRecoveryAttempts() == m_iVBRouteLossRemoved &&
				state.GetPendingRecoveryReason() == "NATIVE_ROUTE_WAYPOINT_LOST" && !state.WasPendingUnstuckRelocated() && AICF_VBExact(trip);
			if (!rebuilt && !trip.IsTerminal() && latency < 30000) return;
			AICF_VBCheck("NATIVE_ROUTE_LOSS_REBUILD_" + m_iVBRouteLossRemoved.ToString(), rebuilt && latency < 30000,
				string.Format("removal=%1 route_generation=%2 actual_queued=%3 attempts=%4 latency_ms=%5 deadline=%6 relocated=%7",
					m_iVBRouteLossRemoved, state.GetRouteGeneration(), route && actual.Contains(route), state.GetMobilityRecoveryAttempts(),
					latency, trip.GetAbsoluteDeadlineMs(), state.WasPendingUnstuckRelocated()));
			if (m_iVBFailures > 0) { AICF_VBFinish(); return; }
			m_iVBRouteLossRebuilt++;
		}
		array<AIWaypoint> queued = {};
		m_VBGroup.GetWaypoints(queued);
		AIWaypoint expected = state.GetRouteWaypoint();
		if (trip.GetPhase() != AICF_ETransportTripPhase.TRANSIT || !expected || !queued.Contains(expected) ||
			!state.IsRouteWaypointBound() || !AICF_VBExact(trip) || !trip.IsVehiclePresetCurrent() ||
			vector.DistanceXZ(m_VBVehicle.GetOrigin(), state.GetTacticalTarget()) <= m_Stage3Config.GetDismountDistanceMeters())
		{
			AICF_VBCheck("NATIVE_ROUTE_LOSS_REMOVE_PRECONDITION", false);
			AICF_VBFinish();
			return;
		}
		m_iVBRouteLossGeneration = state.GetRouteGeneration();
		m_VBRouteLossWaypointId = expected.GetID();
		m_iVBRouteLossRemovedAt = nowMs;
		m_iVBRouteLossRemoved++;
		m_VBGroup.RemoveWaypoint(expected);
		queued.Clear();
		m_VBGroup.GetWaypoints(queued);
		if (m_iVBRouteLossRemoved == 3)
		{
			if (m_iVBRouteLossAttemptsBeforeExit != -1)
			{
				AICF_VBCheck("NATIVE_ROUTE_LOSS_DUPLICATE_RECEIPT", false);
				AICF_VBFinish();
				return;
			}
			m_iVBRouteLossAttemptsBeforeExit = state.GetMobilityRecoveryAttempts();
		}
		bool budgetReceipt = m_iVBRouteLossRemoved != 3 || (m_iVBRouteLossAttemptsBeforeExit == 2 && m_iVBRouteLossRebuilt == 2);
		AICF_VBCheck("NATIVE_ROUTE_LOSS_REMOVED_" + m_iVBRouteLossRemoved.ToString(), !queued.Contains(expected) &&
			state.GetRouteWaypoint() == expected && state.IsRouteWaypointBound() && budgetReceipt,
			string.Format("removal=%1 waypoint=%2 route_generation=%3 cached_bound=%4 actual_queued=%5 attempts=%6 deadline=%7 controlled_setup=1 before_exit_attempts=%8",
				m_iVBRouteLossRemoved, m_VBRouteLossWaypointId, state.GetRouteGeneration(), state.IsRouteWaypointBound(), queued.Contains(expected),
				state.GetMobilityRecoveryAttempts(), trip.GetAbsoluteDeadlineMs(), m_iVBRouteLossAttemptsBeforeExit));
		if (m_iVBFailures > 0) AICF_VBFinish();
	}

	override protected void Update()
	{
		super.Update();
		if (!Replication.IsServer() || m_bVBDone || !m_bRosterReady || !System.GetCLIParam("aicfVehicleBehaviorProbe", m_sVBScenario)) return;
		int now = System.GetTickCount();
		if (!m_iVBStarted) m_iVBStarted = now;
		if (m_sVBScenario != "transport" && m_sVBScenario != "combat") { AICF_VBCheck("SCENARIO_ARGUMENT", false); AICF_VBFinish(); return; }
		if (now - m_iVBStarted > 600000) { AICF_VBCheck("TIMEOUT", false, string.Format("step=%1", m_iVBStep)); AICF_VBFinish(); return; }
		if (m_bVBCleanupIncapActive)
		{
			if (m_bVBCleanupAliveMode) AICF_VBCleanupAliveUpdate(now);
			else AICF_VBCleanupIncapUpdate(now);
			return;
		}
		if (!m_VBState)
		{
			if (!AICF_VBInitialize()) { AICF_VBCheck("INITIALIZATION", false); AICF_VBFinish(); }
			return;
		}
		AICF_VBObserve();
		AICF_GroupSlot slot = m_VBState.GetSlot(0);
		if (!slot.IsCombatReady())
		{
			if (m_VBGroup.GetAgentsCount() != m_iVBSize) return;
			if (!AICF_VBPrepare(slot)) { AICF_VBCheck("PREPARE_ROSTER_MODEL", false); AICF_VBFinish(); return; }
		}
		AICF_VBThreat(m_bVBThreat);
		AICF_VBTryReboardPilotLoss();
		if (m_iVBFailures > 0) { AICF_VBFinish(); return; }
		if (m_sVBFaction == "US") m_VBCoordinator.Update(m_VBState, m_VBFaction, null, null, m_iStrategicBaseRevision, true);
		else m_VBCoordinator.Update(null, null, m_VBState, m_VBFaction, m_iStrategicBaseRevision, true);
		AICF_TransportTrip trip = m_VBCoordinator.GetTrip(slot);
		if (!trip) return;
		if (m_bVBNativeRouteLossActive) { AICF_VBNativeRouteLossUpdate(trip, now); return; }
		if (trip.IsTerminal()) { AICF_VBCheck("NONTERMINAL_EXPECTED", false, trip.GetTerminalReason()); AICF_VBFinish(); return; }
		if (!m_VBTrip) m_VBTrip = trip;
		if (trip != m_VBTrip) { AICF_VBCheck("ORIGINAL_TRIP_IDENTITY", false); AICF_VBFinish(); return; }
		if (m_iVBStep == 0)
		{
			if (trip.GetPhase() != AICF_ETransportTripPhase.TRANSIT || !trip.GetLease()) return;
			m_VBVehicle = trip.GetLease().GetVehicle();
			if (!m_VBVehicle) return;
			m_VBVehicleId = m_VBVehicle.GetID();
			m_iVBLeaseGeneration = trip.GetLease().GetLeaseGeneration();
			AICF_VBCheck("BOARDING_PHYSICAL", AICF_VBExact(trip));
			AICF_VBCheck("SINGLE_PAID_ASSET", trip.IsVehiclePresetDebited() && m_VBCoordinator.GetSpawnedCount(m_VBFaction.GetFactionKey()) == 1);
			if (!AICF_VBExact(trip)) { AICF_VBFinish(); return; }
			if (!AICF_VBNativeAllocation(trip)) { AICF_VBFinish(); return; }
			m_VBInjuredMember = m_aVBMembers[m_aVBMembers.Count() - 1];
			DamageManagerComponent injury = DamageManagerComponent.Cast(m_VBInjuredMember.FindComponent(DamageManagerComponent));
			if (!injury || !injury.GetDefaultHitZone()) { AICF_VBCheck("INJURY_PRECONDITION", false); AICF_VBFinish(); return; }
			HitZone zone = injury.GetDefaultHitZone();
			zone.SetHealthScaled(Math.Min(zone.GetHealthScaled(), 0.9));
			m_fVBInjuredHealth = zone.GetHealthScaled();
			AICF_VBCheck("CONTROLLED_NONLETHAL_DAMAGE", m_fVBInjuredHealth > 0 && m_fVBInjuredHealth <= 0.9);
			array<BaseCompartmentSlot> seats = {};
			AICF_VehiclePreset.Collect(m_VBVehicle, seats);
			foreach (BaseCompartmentSlot seat : seats)
			{
				if (!AICF_VehicleBehaviorFlow.IsCrewSeat(seat)) continue;
				m_aVBCrew.Insert(seat.GetOccupant());
				m_aVBCrewSeats.Insert(seat);
			}
			m_bVBThreat = true;
			m_iVBStep = 1;
			m_iVBPhaseStarted = now;
			return;
		}
		if (m_iVBStep == 1)
		{
			if (!AICF_VehicleBehaviorPolicy.HasLocalCombat(trip) || trip.GetPhase() != AICF_ETransportTripPhase.TACTICAL_DEPLOYMENT || !AICF_VBDeployed(trip)) return;
			AICF_VBCheck("NATIVE_THREAT_DETECTED", true, "actual_ThreatSystem_state=ALERTED_or_THREATENED");
			AICF_VBCheck("THREAT_DISMOUNT_PHYSICAL", true, string.Format("behavior=%1 deployed=%2", trip.GetVehiclePreset().m_iBehavior, trip.GetBehaviorState().m_aDeployed.Count()));
			if (m_sVBScenario == "combat") AICF_VBCheck("COMBAT_CREW_RETAINED", AICF_VBCrewRetained());
			m_iVBStep = 2;
			m_iVBPhaseStarted = now;
			return;
		}
		if (m_iVBStep == 2)
		{
			if (now - m_iVBPhaseStarted < 10000) return;
			for (int index; index < m_aVBCrewSeats.Count(); index++)
				if (m_aVBCrewSeats[index].GetType() == ECompartmentType.PILOT) m_VBDeadDriver = m_aVBCrew[index];
			if (!m_VBDeadDriver) { AICF_VBCheck("ORIGINAL_PILOT", false); AICF_VBFinish(); return; }
			m_VBDeadDriverId = m_VBDeadDriver.GetID();
			SCR_CharacterDamageManagerComponent damage = SCR_CharacterDamageManagerComponent.Cast(m_VBDeadDriver.FindComponent(SCR_CharacterDamageManagerComponent));
			if (!damage) { AICF_VBCheck("PILOT_DAMAGE_MANAGER", false); AICF_VBFinish(); return; }
			damage.Kill(Instigator.CreateInstigatorGM());
			m_iVBStep = 3;
			m_iVBPhaseStarted = now;
			return;
		}
		if (m_iVBStep == 3)
		{
			if (AICF_GroupRuntime.IsAliveCharacter(m_VBDeadDriver)) { AICF_VBCheck("NATIVE_DRIVER_LOSS", false); AICF_VBFinish(); return; }
			if (m_sVBScenario == "combat")
			{
				IEntity driver = AICF_VBPilot();
				if (AICF_GroupRuntime.IsAliveCharacter(driver)) { AICF_VBCheck("COMBAT_REPLACEMENT_DEFERRED", false); AICF_VBFinish(); return; }
			}
			if (now - m_iVBPhaseStarted < 15000) return;
			AICF_VBCheck("NATIVE_DRIVER_LOSS", true, string.Format("member=%1 dead=1", m_VBDeadDriverId));
			if (m_sVBScenario == "combat") AICF_VBCheck("COMBAT_REPLACEMENT_DEFERRED", true, "physical_pilot_unfit_for_15000ms ongoing_native_threat=1");
			m_bVBThreat = false;
			m_iVBStep = 4;
			return;
		}
		if (m_iVBStep == 4)
		{
			if (AICF_VehicleBehaviorPolicy.HasLocalCombat(trip) || !AICF_VBExact(trip) || trip.GetPhase() != AICF_ETransportTripPhase.TRANSIT) return;
			if (m_bVBReboardPilotLossProbe)
			{
				AICF_VBCheck("REBOARD_PILOT_LOSS_RECOVERED", AICF_VBReboardPilotRecovered(trip) && System.GetTickCount() < m_iVBOriginalReboardDeadline,
					string.Format("injected=%1 gunner=%2 pilot=%3 deadline=%4 unchanged_deadline=%5 alive=%6",
						m_bVBReboardPilotLossInjected, m_VBReboardGunnerId, AICF_GroupTrace.Entity(AICF_VBPilot()), m_iVBOriginalReboardDeadline,
						trip.GetBehaviorState().m_iReboardDeadline == m_iVBOriginalReboardDeadline, AICF_GroupRuntime.CountAliveAgents(m_VBGroup)));
				if (m_iVBFailures > 0) { AICF_VBFinish(); return; }
			}
			IEntity replacement = AICF_VBPilot();
			AICF_VBCheck("NATIVE_THREAT_CLEARED", true);
			AICF_VBCheck("DRIVER_REPLACED_PHYSICAL", replacement && replacement != m_VBDeadDriver && m_aVBMembers.Contains(replacement) && AICF_GroupRuntime.IsAliveCharacter(replacement), string.Format("old=%1 replacement=%2", m_VBDeadDriverId, AICF_GroupTrace.Entity(replacement)));
			AICF_VBCheck("RETURN_SAME_ASSET", m_VBVehicle.GetID() == m_VBVehicleId && trip.GetLease().GetLeaseGeneration() == m_iVBLeaseGeneration && m_VBCoordinator.GetSpawnedCount(m_VBFaction.GetFactionKey()) == 1);
			AICF_VBCheck("SURVIVOR_IDENTITIES", AICF_VBIdentities());
			AICF_VBCheck("NO_RESURRECTION", m_VBDeadDriver && m_VBDeadDriver.GetID() == m_VBDeadDriverId && !AICF_GroupRuntime.IsAliveCharacter(m_VBDeadDriver));
			DamageManagerComponent injury = DamageManagerComponent.Cast(m_VBInjuredMember.FindComponent(DamageManagerComponent));
			AICF_VBCheck("NO_HEAL_DELTAS", injury && injury.GetDefaultHitZone() && injury.GetDefaultHitZone().GetHealthScaled() <= m_fVBInjuredHealth + 0.001);
			m_vVBMovementStart = m_VBVehicle.GetOrigin();
			m_iVBStep = 5;
			if (m_sVBScenario == "transport")
			{
				// Второй пеший бой проверяет новый дальний приказ при продолжающейся
				// native угрозе, а не обычный маршрут уже после её снятия.
				m_bVBThreat = true;
				m_iVBStep = 7;
			}
			return;
		}
		if (m_iVBStep == 5)
		{
			vector target;
			if (!AICF_VBFarTarget(target)) return;
			bool order = m_OrderPlanner.AssignPlayerPointOrder(slot, m_VBFaction, target, m_VBCoordinator.IsInfantryOrderSuspended(slot));
			AICF_VBCheck("NEW_ORDER_OVER_N", order && vector.DistanceXZ(m_vVBMovementStart, target) > 300, string.Format("target=%1 N=300", target));
			if (!order) { AICF_VBFinish(); return; }
			m_iVBStep = 6;
			return;
		}
		if (m_iVBStep == 6 && vector.DistanceXZ(m_vVBMovementStart, m_VBVehicle.GetOrigin()) >= 10)
		{
			if (m_bVBReboardPilotLossProbe) AICF_VBCheck("REBOARD_PILOT_LOSS_MOVEMENT", AICF_VBReboardPilotRecovered(trip),
				string.Format("displacement_m=%1 driver=%2 expected_gunner=%3 original_dead=%4 replacement_dead=%5 health_preserved=1",
					vector.DistanceXZ(m_vVBMovementStart, m_VBVehicle.GetOrigin()), AICF_GroupTrace.Entity(AICF_VBPilot()), m_VBReboardGunnerId,
					!AICF_GroupRuntime.IsAliveCharacter(m_VBDeadDriver), !AICF_GroupRuntime.IsAliveCharacter(m_VBReboardVictim)));
			AICF_VBCheck("SAME_VEHICLE_PHYSICAL_RESUME", AICF_VBExact(trip) && m_VBVehicle.GetID() == m_VBVehicleId, string.Format("displacement_m=%1", vector.DistanceXZ(m_vVBMovementStart, m_VBVehicle.GetOrigin())));
			if (m_bVBNativeRouteLossProbe) { AICF_VBBeginNativeRouteLoss(trip, now); return; }
			AICF_VBFinish();
		}
		if (m_iVBStep == 7)
		{
			if (!AICF_VehicleBehaviorPolicy.HasLocalCombat(trip) ||
				trip.GetPhase() != AICF_ETransportTripPhase.TACTICAL_DEPLOYMENT || !AICF_VBDeployed(trip)) return;
			vector combatTarget;
			if (!AICF_VBFarTarget(combatTarget)) return;
			m_vVBMovementStart = m_VBVehicle.GetOrigin();
			AICF_VBCheck("TRANSPORT_SECOND_THREAT_DEPLOYMENT", true, "survivors_physically_outside ongoing_native_threat=1");
			int previousIntent = slot.GetStrategicIntentRevision();
			bool combatOrder = m_OrderPlanner.AssignPlayerPointOrder(slot, m_VBFaction, combatTarget, m_VBCoordinator.IsInfantryOrderSuspended(slot));
			m_iVBOverrideIntent = slot.GetStrategicIntentRevision();
			AICF_VBCheck("NEW_ORDER_OVER_N", combatOrder && m_iVBOverrideIntent > previousIntent &&
				vector.DistanceXZ(m_vVBMovementStart, combatTarget) > trip.GetVehiclePreset().m_iReturnDistance,
				string.Format("target=%1 N=%2 ongoing_native_threat=1 previous_intent=%3 new_intent=%4",
				combatTarget, trip.GetVehiclePreset().m_iReturnDistance, previousIntent, m_iVBOverrideIntent));
			if (!combatOrder) { AICF_VBFinish(); return; }
			m_iVBStep = 8;
			return;
		}
		if (m_iVBStep == 8 && vector.DistanceXZ(m_vVBMovementStart, m_VBVehicle.GetOrigin()) >= 10)
		{
			bool samePhysicalAsset = AICF_VBExact(trip) && m_VBVehicle.GetID() == m_VBVehicleId &&
				trip.GetLease().GetLeaseGeneration() == m_iVBLeaseGeneration && m_VBCoordinator.GetSpawnedCount(m_VBFaction.GetFactionKey()) == 1;
			bool combatOverride = m_bVBThreat && AICF_VehicleBehaviorPolicy.HasLocalCombat(trip) &&
				AICF_VehicleBehaviorPolicy.HasTransportCombatOverride(trip) &&
				trip.GetAssignment().GetStrategicIntentRevision() == m_iVBOverrideIntent;
			AICF_VBCheck("SAME_VEHICLE_PHYSICAL_RESUME", samePhysicalAsset,
				string.Format("displacement_m=%1 ongoing_native_threat=1", vector.DistanceXZ(m_vVBMovementStart, m_VBVehicle.GetOrigin())));
			AICF_VBCheck("DISTANT_ORDER_COMBAT_OVERRIDE", samePhysicalAsset && combatOverride &&
				trip.GetPhase() == AICF_ETransportTripPhase.TRANSIT && AICF_VBIdentities(),
				string.Format("displacement_m=%1 native_threat=%2 current_override=%3 intent=%4",
				vector.DistanceXZ(m_vVBMovementStart, m_VBVehicle.GetOrigin()),
				AICF_VehicleBehaviorPolicy.HasLocalCombat(trip), AICF_VehicleBehaviorPolicy.HasTransportCombatOverride(trip), m_iVBOverrideIntent));
			AICF_VBFinish();
		}
	}

	protected void AICF_VBFinish()
	{
		string cleanupFlag;
		string aliveFlag;
		bool aliveProbe = System.GetCLIParam("aicfCleanupAliveProbe", aliveFlag) && aliveFlag == "1";
		if (!m_bVBCleanupIncapActive && m_iVBFailures == 0 && (m_iVBStep == 6 || m_iVBStep == 8) &&
			((System.GetCLIParam("aicfCleanupIncapProbe", cleanupFlag) && cleanupFlag == "1") || aliveProbe))
		{
			m_bVBCleanupIncapActive = true;
			m_bVBCleanupAliveMode = aliveProbe;
			m_bVBThreat = false;
			m_iVBCleanupIncapStep = 0;
			m_iVBCleanupIncapStarted = System.GetTickCount();
			return;
		}
		m_bVBDone = true;
		AICF_VBStopObserver();
		if (m_VBCoordinator)
		{
			if (m_sVBFaction == "US") m_VBCoordinator.StopWithFactionContexts(true, m_VBState, m_VBFaction, null, null);
			else m_VBCoordinator.StopWithFactionContexts(true, null, null, m_VBState, m_VBFaction);
		}
		if (m_VBGroup)
		{
			m_OrderPlanner.ClearOrder(m_VBState.GetSlot(0));
			m_VBGroup.DespawnMembers();
			RplComponent.DeleteRplEntity(m_VBGroup, false);
		}
		Print(string.Format("[AICF][VEHICLE_BEHAVIOR_FINISHED] scenario=%1 checks=%2 failures=%3 faction=%4 scope=CONTROLLED_NATIVE_THREAT", m_sVBScenario, m_iVBChecks, m_iVBFailures, m_sVBFaction));
		GetGame().RequestClose();
	}

	protected bool AICF_VBCleanupHull(ChimeraCharacter character)
	{
		if (!character || !m_VBVehicle) return false;
		vector minimum;
		vector maximum;
		m_VBVehicle.GetBounds(minimum, maximum);
		vector local = m_VBVehicle.CoordToLocal(character.GetOrigin());
		return local[0] >= minimum[0] - 1.5 && local[0] <= maximum[0] + 1.5 &&
			local[1] >= minimum[1] - 1.5 && local[1] <= maximum[1] + 1.5 &&
			local[2] >= minimum[2] - 1.5 && local[2] <= maximum[2] + 1.5;
	}

	protected void AICF_VBCleanupIncapUpdate(int nowMs)
	{
		if (m_iVBFailures > 0) { AICF_VBFinish(); return; }
		if (nowMs - m_iVBCleanupIncapStarted >= 40000)
		{
			AICF_VBCheck("CLEANUP_INCAP_TIMEOUT", false, string.Format("step=%1 duration_ms=%2", m_iVBCleanupIncapStep, nowMs - m_iVBCleanupIncapStarted));
			AICF_VBFinish();
			return;
		}
		AICF_GroupSlot slot = m_VBState.GetSlot(0);
		ChimeraCharacter victim = ChimeraCharacter.Cast(m_VBInjuredMember);
		if (!victim) { AICF_VBCheck("CLEANUP_INCAP_MEMBER_PRESENT", false); AICF_VBFinish(); return; }
		CompartmentAccessComponent access = victim.GetCompartmentAccessComponent();
		SCR_CharacterDamageManagerComponent damage = SCR_CharacterDamageManagerComponent.Cast(victim.GetDamageManager());
		if (!access || !damage) { AICF_VBCheck("CLEANUP_INCAP_NATIVE_COMPONENTS", false); AICF_VBFinish(); return; }
		if (m_VBCleanupLease && (m_iVBCleanupIncapStep == 2 || nowMs - m_iVBCleanupGuardReported >= 5000))
		{
			m_iVBCleanupGuardReported = nowMs;
			Print("[AICF][VEHICLE_CLEANUP_INCAP_GUARD] " + m_VBCoordinator.AICF_VBCleanupGuardReadback(m_VBCleanupLease, victim));
		}
		if (m_VBCleanupLease && m_iVBCleanupIncapStep > 0)
		{
			AICF_VehicleCleanupQuery currentCleanup = m_VBCoordinator.AICF_VBCleanupReadback(m_VBCleanupLease);
			if (currentCleanup && currentCleanup.IsRetainedFailClosed())
			{
				AICF_VBCheck("CLEANUP_INCAP_NOT_RETAINED", false, "actual_job_retained=1 further_natural_motion_not_recovery_proof");
				AICF_VBFinish();
				return;
			}
		}
		if (m_iVBCleanupIncapStep == 0)
		{
			m_VBCleanupLease = m_VBTrip.GetLease();
			AICF_VBThreat(false);
			bool queued = m_VBCoordinator.AICF_VBStartCleanup(slot, m_VBFaction);
			AICF_VBCheck("CLEANUP_INCAP_RELEASE_QUEUED", queued);
			if (!queued) { AICF_VBFinish(); return; }
			m_iVBCleanupDeadline = m_VBCoordinator.AICF_VBCleanupDeadline(m_VBCleanupLease);
			m_iVBCleanupIncapStep = 1;
			return;
		}
		if (m_iVBCleanupIncapStep == 1)
		{
			if (!m_VBVehicle || !m_VBVehicle.GetPhysics() || m_VBVehicle.GetPhysics().GetVelocity().Length() > 0.25)
			{
				m_VBCoordinator.AICF_VBPollCleanup(m_VBFaction, nowMs);
				return;
			}
			bool outside = true;
			foreach (IEntity actor : m_aVBMembers)
			{
				if (!AICF_GroupRuntime.IsAliveCharacter(actor)) continue;
				ChimeraCharacter character = ChimeraCharacter.Cast(actor);
				CompartmentAccessComponent memberAccess = character.GetCompartmentAccessComponent();
				if (!memberAccess || character.IsInVehicle() || CompartmentAccessComponent.GetVehicleIn(actor) ||
					memberAccess.IsInCompartment() || memberAccess.IsGettingIn() || memberAccess.IsGettingOut())
				{
					outside = false;
					AICF_VehicleShortRecovery.TryExit(m_VBTrip, actor, "CONTROLLED_INCAP_PROBE_EGRESS");
				}
			}
			if (!outside) { m_VBCoordinator.AICF_VBPollCleanup(m_VBFaction, nowMs); return; }
			vector minimum;
			vector maximum;
			m_VBVehicle.GetBounds(minimum, maximum);
			vector ghost = m_VBVehicle.CoordToParent((minimum + maximum) * 0.5);
			float displacement = vector.Distance(victim.GetOrigin(), ghost);
			if (displacement > AICF_VehicleShortRecovery.MAX_ACTOR_DISPLACEMENT_METERS)
			{
				AICF_VBCheck("CLEANUP_INCAP_SETUP_SHORT", false, string.Format("distance_m=%1", displacement));
				AICF_VBFinish();
				return;
			}
			vector transform[4];
			victim.GetWorldTransform(transform);
			transform[3] = ghost;
			victim.Teleport(transform);
			damage.ForceUnconsciousness(0);
			m_iVBCleanupIncapStep = 2;
			return;
		}
		if (m_iVBCleanupIncapStep == 2)
		{
			bool incapacitated = victim.GetCharacterController() && victim.GetCharacterController().GetLifeState() == ECharacterLifeState.INCAPACITATED;
			bool detached = !victim.IsInVehicle() && !CompartmentAccessComponent.GetVehicleIn(victim) &&
				!access.IsInCompartment() && !access.IsGettingIn() && !access.IsGettingOut() && !access.IsSwitchingSeatsAnim();
			bool ghost = AICF_VBCleanupHull(victim);
			AICF_VBCheck("CLEANUP_INCAP_HULL_GHOST_ESTABLISHED", incapacitated && detached && ghost,
				string.Format("member=%1 position=%2 incapacitated=%3 detached=%4 inside_hull=%5 controlled_setup=1",
				victim.GetID(), victim.GetOrigin(), incapacitated, detached, ghost));
			if (!incapacitated || !detached || !ghost) { AICF_VBFinish(); return; }
			m_vVBCleanupGhost = victim.GetOrigin();
			m_fVBCleanupHealth = damage.GetHealthScaled();
			m_fVBCleanupResilience = damage.GetResilienceHitZone().GetHealthScaled();
			m_VBCoordinator.AICF_VBPollCleanup(m_VBFaction, nowMs);
			m_iVBCleanupIncapStep = 3;
			return;
		}
		if (m_iVBCleanupIncapStep == 3)
		{
			if (!AICF_VBCleanupHull(victim))
			{
				float displacement = vector.Distance(victim.GetOrigin(), m_vVBCleanupGhost);
				bool lifeUnchanged = victim.GetCharacterController() && victim.GetCharacterController().GetLifeState() == ECharacterLifeState.INCAPACITATED;
				bool healthUnchanged = damage.GetHealthScaled() <= m_fVBCleanupHealth + 0.001 &&
					damage.GetResilienceHitZone().GetHealthScaled() <= m_fVBCleanupResilience + 0.001;
				int attempts = m_VBCoordinator.AICF_VBCleanupAttempts(m_VBCleanupLease);
				AICF_VBCheck("CLEANUP_INCAP_HULL_PHYSICAL_CLEAR", displacement > 1 && displacement <= AICF_VehicleShortRecovery.MAX_ACTOR_DISPLACEMENT_METERS &&
					!victim.IsInVehicle() && !CompartmentAccessComponent.GetVehicleIn(victim), string.Format("later_native_readback=1 displacement_m=%1", displacement));
				AICF_VBCheck("CLEANUP_INCAP_LIFE_HEALTH_PRESERVED", lifeUnchanged && healthUnchanged);
				AICF_VBCheck("CLEANUP_INCAP_RECOVERY_BUDGET", attempts > 0 && attempts <= 3 &&
					m_VBCoordinator.AICF_VBCleanupDeadline(m_VBCleanupLease) == m_iVBCleanupDeadline,
					string.Format("attempts=%1 maximum=3 deadline=%2", attempts, m_iVBCleanupDeadline));
				if (m_iVBFailures > 0) { AICF_VBFinish(); return; }
				m_iVBCleanupIncapStep = 4;
			}
			m_VBCoordinator.AICF_VBPollCleanup(m_VBFaction, nowMs);
			return;
		}
		if (m_iVBCleanupIncapStep == 4)
		{
			m_VBCoordinator.AICF_VBPollCleanup(m_VBFaction, nowMs);
			AICF_VehicleCleanupQuery query = m_VBCoordinator.AICF_VBCleanupReadback(m_VBCleanupLease);
			if (!query || !query.IsReleaseComplete()) return;
			AICF_VBCheck("CLEANUP_INCAP_FULL_RELEASE", query.IsClearanceSafe() && !query.IsRetainedFailClosed());
			m_iVBCleanupIncapStep = 5;
			AICF_VBFinish();
		}
	}

	protected void AICF_VBCleanupAliveUpdate(int nowMs)
	{
		if (m_iVBFailures > 0) { AICF_VBFinish(); return; }
		if (nowMs - m_iVBCleanupIncapStarted >= 40000)
		{
			AICF_VBCheck("CLEANUP_ALIVE_TIMEOUT", false, string.Format("step=%1 duration_ms=%2", m_iVBCleanupIncapStep, nowMs - m_iVBCleanupIncapStarted));
			AICF_VBFinish();
			return;
		}
		AICF_GroupSlot slot = m_VBState.GetSlot(0);
		ChimeraCharacter victim = ChimeraCharacter.Cast(m_VBInjuredMember);
		if (!victim) { AICF_VBCheck("CLEANUP_ALIVE_MEMBER_PRESENT", false); AICF_VBFinish(); return; }
		CompartmentAccessComponent access = victim.GetCompartmentAccessComponent();
		SCR_CharacterDamageManagerComponent damage = SCR_CharacterDamageManagerComponent.Cast(victim.GetDamageManager());
		if (!access || !damage) { AICF_VBCheck("CLEANUP_ALIVE_NATIVE_COMPONENTS", false); AICF_VBFinish(); return; }
		if (m_VBCleanupLease && (m_iVBCleanupIncapStep == 2 || nowMs - m_iVBCleanupGuardReported >= 5000))
		{
			m_iVBCleanupGuardReported = nowMs;
			Print("[AICF][VEHICLE_CLEANUP_ALIVE_GUARD] " + m_VBCoordinator.AICF_VBCleanupGuardReadback(m_VBCleanupLease, victim));
		}
		if (m_VBCleanupLease && m_iVBCleanupIncapStep > 0)
		{
			AICF_VehicleCleanupQuery currentCleanup = m_VBCoordinator.AICF_VBCleanupReadback(m_VBCleanupLease);
			if (currentCleanup && currentCleanup.IsRetainedFailClosed())
			{
				AICF_VBCheck("CLEANUP_ALIVE_NOT_RETAINED", false, "actual_job_retained=1 further_natural_motion_not_recovery_proof");
				AICF_VBFinish();
				return;
			}
		}
		if (m_iVBCleanupIncapStep == 0)
		{
			m_VBCleanupLease = m_VBTrip.GetLease();
			AICF_VBThreat(false);
			bool queued = m_VBCoordinator.AICF_VBStartCleanup(slot, m_VBFaction);
			AICF_VBCheck("CLEANUP_ALIVE_RELEASE_QUEUED", queued);
			if (!queued) { AICF_VBFinish(); return; }
			m_iVBCleanupDeadline = m_VBCoordinator.AICF_VBCleanupDeadline(m_VBCleanupLease);
			m_iVBCleanupIncapStep = 1;
			return;
		}
		if (m_iVBCleanupIncapStep == 1)
		{
			if (!m_VBVehicle || !m_VBVehicle.GetPhysics() || m_VBVehicle.GetPhysics().GetVelocity().Length() > 0.25)
			{
				m_VBCoordinator.AICF_VBPollCleanup(m_VBFaction, nowMs);
				return;
			}
			bool outside = true;
			foreach (IEntity actor : m_aVBMembers)
			{
				if (!AICF_GroupRuntime.IsAliveCharacter(actor)) continue;
				ChimeraCharacter character = ChimeraCharacter.Cast(actor);
				CompartmentAccessComponent memberAccess = character.GetCompartmentAccessComponent();
				if (!memberAccess || character.IsInVehicle() || CompartmentAccessComponent.GetVehicleIn(actor) ||
					memberAccess.IsInCompartment() || memberAccess.IsGettingIn() || memberAccess.IsGettingOut())
				{
					outside = false;
					AICF_VehicleShortRecovery.TryExit(m_VBTrip, actor, "CONTROLLED_ALIVE_PROBE_EGRESS");
				}
			}
			if (!outside) { m_VBCoordinator.AICF_VBPollCleanup(m_VBFaction, nowMs); return; }
			vector minimum;
			vector maximum;
			m_VBVehicle.GetBounds(minimum, maximum);
			vector localGhost = (minimum + maximum) * 0.5;
			localGhost[0] = minimum[0] - 0.5;
			vector ghost = m_VBVehicle.CoordToParent(localGhost);
			ghost[1] = GetGame().GetWorld().GetSurfaceY(ghost[0], ghost[2]) + 0.05;
			float displacement = vector.Distance(victim.GetOrigin(), ghost);
			if (displacement > AICF_VehicleShortRecovery.MAX_ACTOR_DISPLACEMENT_METERS)
			{
				AICF_VBCheck("CLEANUP_ALIVE_SETUP_SHORT", false, string.Format("distance_m=%1", displacement));
				AICF_VBFinish();
				return;
			}
			vector transform[4];
			victim.GetWorldTransform(transform);
			transform[3] = ghost;
			m_bVBCleanupOldMovementDisabled = victim.GetCharacterController().GetDisableMovementControls();
			victim.GetCharacterController().SetDisableMovementControls(true);
			m_fVBCleanupHealth = damage.GetHealthScaled();
			m_fVBCleanupResilience = damage.GetResilienceHitZone().GetHealthScaled();
			victim.Teleport(transform);

			m_iVBCleanupIncapStep = 2;
			return;
		}
		if (m_iVBCleanupIncapStep == 2)
		{
			bool alive = victim.GetCharacterController() && victim.GetCharacterController().GetLifeState() == ECharacterLifeState.ALIVE;
			bool detached = !victim.IsInVehicle() && !CompartmentAccessComponent.GetVehicleIn(victim) &&
				!access.IsInCompartment() && !access.IsGettingIn() && !access.IsGettingOut() && !access.IsSwitchingSeatsAnim();
			bool ghost = AICF_VBCleanupHull(victim);
			AICF_VBCheck("CLEANUP_ALIVE_HULL_GHOST_ESTABLISHED", alive && detached && ghost && victim.GetCharacterController().GetDisableMovementControls(),
				string.Format("member=%1 position=%2 alive=%3 detached=%4 inside_hull=%5 controlled_setup=1",
				victim.GetID(), victim.GetOrigin(), alive, detached, ghost));
			if (!alive || !detached || !ghost) { AICF_VBFinish(); return; }
			m_vVBCleanupGhost = victim.GetOrigin();
			m_fVBCleanupHealth = damage.GetHealthScaled();
			m_fVBCleanupResilience = damage.GetResilienceHitZone().GetHealthScaled();
			m_VBCoordinator.AICF_VBPollCleanup(m_VBFaction, nowMs);
			m_iVBCleanupIncapStep = 3;
			return;
		}
		if (m_iVBCleanupIncapStep == 3)
		{
			if (!AICF_VBCleanupHull(victim))
			{
				float displacement = vector.Distance(victim.GetOrigin(), m_vVBCleanupGhost);
				bool lifeUnchanged = victim.GetCharacterController() && victim.GetCharacterController().GetLifeState() == ECharacterLifeState.ALIVE;
				bool healthUnchanged = Math.AbsFloat(damage.GetHealthScaled() - m_fVBCleanupHealth) <= 0.001 &&
					Math.AbsFloat(damage.GetResilienceHitZone().GetHealthScaled() - m_fVBCleanupResilience) <= 0.001;
				int attempts = m_VBCoordinator.AICF_VBCleanupAttempts(m_VBCleanupLease);
				AICF_VBCheck("CLEANUP_ALIVE_HULL_PHYSICAL_CLEAR", displacement > 1 && displacement <= AICF_VehicleShortRecovery.MAX_ACTOR_DISPLACEMENT_METERS &&
					!victim.IsInVehicle() && !CompartmentAccessComponent.GetVehicleIn(victim), string.Format("later_native_readback=1 displacement_m=%1", displacement));
				AICF_VBCheck("CLEANUP_ALIVE_LIFE_HEALTH_PRESERVED", lifeUnchanged && healthUnchanged);
				AICF_VBCheck("CLEANUP_ALIVE_RECOVERY_BUDGET", attempts > 0 && attempts <= 3 &&
					m_VBCoordinator.AICF_VBCleanupDeadline(m_VBCleanupLease) == m_iVBCleanupDeadline,
					string.Format("attempts=%1 maximum=3 deadline=%2", attempts, m_iVBCleanupDeadline));
				if (m_iVBFailures > 0) { AICF_VBFinish(); return; }
				m_iVBCleanupIncapStep = 4;
			}
			m_VBCoordinator.AICF_VBPollCleanup(m_VBFaction, nowMs);
			return;
		}
		if (m_iVBCleanupIncapStep == 4)
		{
			m_VBCoordinator.AICF_VBPollCleanup(m_VBFaction, nowMs);
			AICF_VehicleCleanupQuery query = m_VBCoordinator.AICF_VBCleanupReadback(m_VBCleanupLease);
			if (!query || !query.IsReleaseComplete()) return;
			AICF_VBCheck("CLEANUP_ALIVE_FULL_RELEASE", query.IsClearanceSafe() && !query.IsRetainedFailClosed());
			victim.GetCharacterController().SetDisableMovementControls(m_bVBCleanupOldMovementDisabled);
			m_iVBCleanupIncapStep = 5;
			AICF_VBFinish();
		}
	}

	override protected void Stop(bool cleanupEntities)
	{
		AICF_VBStopObserver();
		super.Stop(cleanupEntities);
	}
}
