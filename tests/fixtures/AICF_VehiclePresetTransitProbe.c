// Отдельный native roster5: выдача у своей базы -> ожидание -> реальное движение -> высадка.
modded class AICF_MatchController
{
	protected ref AICF_FactionState m_PresetTransitState;
	protected ref AICF_VehicleCoordinator m_PresetTransitVehicles;
	protected SCR_AIGroup m_PresetTransitGroup;
	protected ref AICF_VehiclePreset m_PresetTransitRecipe;
	protected Vehicle m_PresetTransitVehicle;
	protected EntityID m_PresetTransitVehicleId;
	protected int m_iPresetTransitStarted;
	protected int m_iPresetTransitParked;
	protected int m_iPresetTransitStep;
	protected int m_iPresetTransitChecks;
	protected int m_iPresetTransitFailures;
	protected int m_iPresetTransitMovingPolls;
	protected int m_iPresetTransitTargetSearch;
	protected vector m_vPresetTransitStart;
	protected bool m_bPresetTransitDone;

	protected void AICF_PTCheck(string rule, bool passed)
	{
		m_iPresetTransitChecks++;
		if (!passed) m_iPresetTransitFailures++;
		Print(string.Format("[AICF][PRESET_TRANSIT_CHECK] rule=%1 passed=%2", rule, passed));
	}

	protected bool AICF_PTExact(AICF_GroupSlot slot, Vehicle vehicle)
	{
		if (!vehicle || vehicle != m_PresetTransitVehicle || vehicle.GetID() != m_PresetTransitVehicleId) return false;
		array<BaseCompartmentSlot> seats = {};
		AICF_VehiclePreset.Collect(vehicle, seats);
		if (!m_PresetTransitRecipe.MatchesSeats(seats)) return false;
		int exact;
		for (int i; i < seats.Count(); i++)
		{
			int member = m_PresetTransitRecipe.m_aMembers[i];
			if (member < 0) continue;
			IEntity actor = slot.GetRosterMember(member);
			ChimeraCharacter character = ChimeraCharacter.Cast(actor);
			if (!character || seats[i].GetOccupant() != actor || CompartmentAccessComponent.GetVehicleIn(actor) != vehicle ||
				character.GetCompartmentAccessComponent().IsGettingOut()) return false;
			exact++;
		}
		return exact == 5;
	}

	protected bool AICF_PTTarget(out vector target)
	{
		AIPathfindingComponent path = AIPathfindingComponent.Cast(m_PresetTransitGroup.FindComponent(AIPathfindingComponent));
		if (!path || !path.GetNavmeshComponent()) return false;
		NavmeshWorldComponent navmesh = path.GetNavmeshComponent();
		navmesh.LoadTileIn(m_PresetTransitVehicle.GetOrigin());
		if (!m_iPresetTransitTargetSearch)
		{
			m_iPresetTransitTargetSearch = System.GetTickCount();
			for (int tile; tile < m_ObjectiveGraph.GetNodeCount(); tile++)
			{
				SCR_CampaignMilitaryBaseComponent base = m_ObjectiveGraph.GetNode(tile).GetBase();
				if (base && base.GetOwner() && vector.DistanceXZ(base.GetOwner().GetOrigin(), m_PresetTransitVehicle.GetOrigin()) < 1200)
					navmesh.LoadTileIn(base.GetOwner().GetOrigin());
			}
			return false;
		}
		if (System.GetTickCount() - m_iPresetTransitTargetSearch < 2000) return false;
		vector origin;
		if (!path.GetClosestPositionOnNavmesh(m_PresetTransitVehicle.GetOrigin(), "30 10 30", origin)) return false;
		float best = 1200;
		bool found;
		for (int sample; sample < m_ObjectiveGraph.GetNodeCount(); sample++)
		{
			SCR_CampaignMilitaryBaseComponent candidateBase = m_ObjectiveGraph.GetNode(sample).GetBase();
			if (!candidateBase || !candidateBase.GetOwner()) continue;
			vector query = candidateBase.GetOwner().GetOrigin();
			float distance = vector.DistanceXZ(origin, query);
			if (distance < 300 || distance >= best) continue;
			vector candidate;
			if (!path.GetClosestPositionOnNavmesh(query, "40 15 40", candidate)) continue;
			float leg = vector.DistanceXZ(origin, candidate);
			if (leg < m_Stage3Config.GetDismountDistanceMeters() + 50 ||
				ChimeraWorldUtils.TryGetWaterSurfaceSimple(GetGame().GetWorld(), candidate - "0 0.5 0")) continue;
			target = candidate;
			best = distance;
			found = true;
		}
		return found;
	}

	override protected void Update()
	{
		super.Update();
		string flag;
		if (!Replication.IsServer() || !System.GetCLIParam("aicfVehiclePresetTransitProbe", flag) || !m_bRosterReady || m_bPresetTransitDone) return;
		int now = System.GetTickCount();
		if (!m_iPresetTransitStarted) m_iPresetTransitStarted = now;
		if (now - m_iPresetTransitStarted > 420000) { AICF_PTCheck("TIMEOUT", false); AICF_PTFinish(); return; }
		if (!m_PresetTransitState)
		{
			m_PresetTransitState = new AICF_FactionState(m_USSRFaction.GetFactionKey(), m_Config);
			AICF_GroupSlot fresh = m_PresetTransitState.GetSlot(0);
			fresh.SetUnitType(AICF_EGroupUnitType.MOTORIZED_LIGHT);
			fresh.SetDesiredSize(5);
			m_PresetTransitGroup = m_GroupSpawner.SpawnGroup(m_USSRFaction, m_USSRFaction.GetMainBase(), 0, 5, false);
			AICF_PTCheck("GROUP5_CREATED", m_PresetTransitGroup != null);
			if (!m_PresetTransitGroup) { AICF_PTFinish(); return; }
			m_PresetTransitGroup.SetLifecyclePolicy(SCR_EAIGroupLifecyclePolicy.Manual);
			m_PresetTransitGroup.SetDeleteWhenEmpty(false);
			fresh.BeginInitialSpawn();
			fresh.BindSpawnedGroup(m_PresetTransitGroup);
			m_GroupSpawner.BeginRosterSpawn(m_PresetTransitGroup, 5);
			m_PresetTransitVehicles = new AICF_VehicleCoordinator(m_Stage3Config, m_Campaign, m_ConflictAdapter, m_OrderPlanner,
				m_GroupCohesionPolicy, m_ObjectiveGraph, m_TargetSelector, m_ContentProfile);
			return;
		}
		AICF_GroupSlot slot = m_PresetTransitState.GetSlot(0);
		if (!slot.IsCombatReady())
		{
			if (m_PresetTransitGroup.GetAgentsCount() != 5) return;
			slot.MarkReady();
			slot.RecordDeploymentMembers();
			AICF_VehiclePresetCatalog catalog = new AICF_VehiclePresetCatalog(m_USSRFaction);
			string reason;
			bool inspected = catalog.Inspect("{16A674FE31B0921C}Prefabs/Vehicles/Wheeled/UAZ469/UAZ469_uncovered.et", reason);
			AICF_PTCheck("UAZ_FIVE_REAL_SEATS", inspected && catalog.Seats().Count() == 5);
			if (!inspected || catalog.Seats().Count() != 5) { catalog.Clear(); AICF_PTFinish(); return; }
			m_PresetTransitRecipe = catalog.Draft("{16A674FE31B0921C}Prefabs/Vehicles/Wheeled/UAZ469/UAZ469_uncovered.et");
			int driver;
			for (int seat; seat < catalog.Seats().Count(); seat++)
				if (catalog.Seats()[seat].GetType() == ECompartmentType.PILOT) driver = seat;
			m_PresetTransitRecipe.Assign(driver, 0);
			int member = 1;
			for (int other; other < catalog.Seats().Count(); other++)
				if (other != driver) m_PresetTransitRecipe.Assign(other, member++);
			AICF_PTCheck("COMPATIBLE_APPLY", m_PresetTransitRecipe.ValidateRoster(slot, catalog.Seats(), reason) && slot.SetVehiclePreset(m_PresetTransitRecipe, slot.GetVehiclePresetRevision()));
			catalog.Clear();
			AICF_PTCheck("NEARBY_INITIAL_ORDER", m_OrderPlanner.AssignPlayerPointOrder(slot, m_USSRFaction, m_USSRFaction.GetMainBase().GetSpawnPoint().GetOrigin(), false));
		}
		m_PresetTransitVehicles.Update(null, null, m_PresetTransitState, m_USSRFaction, m_iStrategicBaseRevision, true);
		AICF_TransportTrip trip = m_PresetTransitVehicles.GetTrip(slot);
		if (!trip || !trip.GetLease() || !trip.GetLease().GetVehicle()) return;
		Vehicle vehicle = trip.GetLease().GetVehicle();
		if (m_iPresetTransitStep == 0)
		{
			if (trip.GetPhase() == AICF_ETransportTripPhase.BOARDING) return;
			AICF_PTCheck("NO_IMMEDIATE_DISMOUNT", trip.GetPhase() == AICF_ETransportTripPhase.TRANSIT && trip.IsPresetWaitingForDestination());
			if (!trip.IsPresetWaitingForDestination()) { AICF_PTFinish(); return; }
			m_PresetTransitVehicle = vehicle;
			m_PresetTransitVehicleId = vehicle.GetID();
			m_vPresetTransitStart = vehicle.GetOrigin();
			m_iPresetTransitParked = now;
			AICF_PTCheck("PARKED_EXACT_FIVE", AICF_PTExact(slot, vehicle));
			AICF_PTCheck("IDLE_BUDGET_PAUSED", !trip.IsDeadlineReached(now + 3600000));
			m_iPresetTransitStep = 1;
			return;
		}
		if (m_iPresetTransitStep == 1)
		{
			if (!AICF_PTExact(slot, vehicle) || trip.GetPhase() != AICF_ETransportTripPhase.TRANSIT || !trip.IsPresetWaitingForDestination())
				{ AICF_PTCheck("CREW_LEFT_DURING_WAIT", false); AICF_PTFinish(); return; }
			if (now - m_iPresetTransitParked < 30000) return;
			AICF_PTCheck("FIVE_STAY_MOUNTED_30_SECONDS", AICF_PTExact(slot, vehicle) && trip.GetMovementState().GetRouteGeneration() == 0);
			AICF_PTCheck("WAIT_STATUS", m_PresetTransitVehicles.VehiclePresetStatus(slot) == "PRESET_WAITING_FOR_DESTINATION");
			m_iPresetTransitStep = 11;
			return;
		}
		if (m_iPresetTransitStep == 11)
		{
			vector destination;
			bool target = AICF_PTTarget(destination);
			if (!target && now - m_iPresetTransitTargetSearch < 30000) return;
			AICF_PTCheck("NAVMESH_DESTINATION", target);
			if (!target) { AICF_PTFinish(); return; }
			AICF_PTCheck("ONWARD_ORDER", m_OrderPlanner.AssignPlayerPointOrder(slot, m_USSRFaction, destination, m_PresetTransitVehicles.IsInfantryOrderSuspended(slot)));
			m_iPresetTransitStep = 2;
			return;
		}
		if (m_iPresetTransitStep == 2)
		{
			if (trip.GetPhase() != AICF_ETransportTripPhase.TRANSIT || !AICF_PTExact(slot, vehicle))
				{ AICF_PTCheck("ONWARD_TRANSIT_LOST", false); AICF_PTFinish(); return; }
			Physics physics = vehicle.GetPhysics();
			if (physics && physics.GetVelocity().Length() > 1) m_iPresetTransitMovingPolls++;
			float moved = vector.DistanceXZ(vehicle.GetOrigin(), m_vPresetTransitStart);
			if (moved < 25 || m_iPresetTransitMovingPolls < 5) return;
			AICF_PTCheck("SAME_VEHICLE_REAL_MOTION", vehicle.GetID() == m_PresetTransitVehicleId && trip.GetMovementState().GetMobilityRecoveryAttempts() == 0);
			Print(string.Format("[AICF][PRESET_TRANSIT_MOTION] displacement_m=%1 moving_polls=%2 same_vehicle=%3 exact_occupants=5",
				moved, m_iPresetTransitMovingPolls, vehicle.GetID() == m_PresetTransitVehicleId));
			AICF_PTCheck("ROUTE_BUDGET_RESTORED", !trip.IsPresetWaitingForDestination() && trip.IsDeadlineReached(trip.GetAbsoluteDeadlineMs()));
			AICF_PTCheck("ARRIVAL_RETARGET", m_OrderPlanner.AssignPlayerPointOrder(slot, m_USSRFaction, vehicle.GetOrigin(), m_PresetTransitVehicles.IsInfantryOrderSuspended(slot)));
			m_iPresetTransitStep = 3;
			return;
		}
		if (m_iPresetTransitStep == 3)
		{
			int inside = AICF_GroupRuntime.CountAliveAgentsInAnyVehicle(m_PresetTransitGroup);
			if (inside > 0 || m_PresetTransitVehicles.IsInfantryOrderSuspended(slot)) return;
			AICF_PTCheck("ARRIVAL_FULL_DISMOUNT", inside == 0 && AICF_GroupRuntime.CountAliveAgents(m_PresetTransitGroup) == 5 &&
				trip.GetPhase() == AICF_ETransportTripPhase.COMPLETE);
			AICF_PTCheck("ONE_PURCHASE", m_PresetTransitVehicles.GetSpawnedCount(m_USSRFaction.GetFactionKey()) == 1);
			AICF_PTFinish();
		}
	}

	protected void AICF_PTFinish()
	{
		m_bPresetTransitDone = true;
		if (m_PresetTransitVehicles) m_PresetTransitVehicles.StopWithFactionContexts(true, null, null, m_PresetTransitState, m_USSRFaction);
		m_PresetTransitVehicles = null;
		if (m_PresetTransitGroup)
		{
			m_OrderPlanner.ClearOrder(m_PresetTransitState.GetSlot(0));
			m_PresetTransitGroup.DespawnMembers();
			RplComponent.DeleteRplEntity(m_PresetTransitGroup, false);
		}
		Print(string.Format("[AICF][PRESET_TRANSIT_DONE] checks=%1 failures=%2", m_iPresetTransitChecks, m_iPresetTransitFailures));
		GetGame().RequestClose();
	}
}
