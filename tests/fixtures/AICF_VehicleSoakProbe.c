// Test-only: настоящий managed slot0, штатный найм/экономика/vehicle lifecycle.
// Отсутствуют fake roster, instant buildings, truce, invulnerability и teleport.
modded class AICF_MatchController
{
	protected bool m_bVehicleSoakStarted;
	protected bool m_bVehicleSoakDone;
	protected bool m_bVehicleSoakRecruitAccepted;
	protected bool m_bVehicleSoakArrived;
	protected bool m_bVehicleSoakFootArrived;
	protected string m_sVehicleSoakFaction;
	protected string m_sVehicleSoakTrip;
	protected int m_iVehicleSoakStart;
	protected int m_iVehicleSoakDuration = 1800000;
	protected int m_iVehicleSoakMountedHold = 10000;
	protected int m_iVehicleSoakNextSample;
	protected int m_iVehicleSoakNextCommand;
	protected int m_iVehicleSoakPhase;
	protected int m_iVehicleSoakGeneration;
	protected int m_iVehicleSoakFailures;
	protected int m_iVehicleSoakInitialAlive;
	protected int m_iVehicleSoakMovingPolls;
	protected int m_iVehicleSoakLastAlive = -1;
	protected int m_iVehicleSoakBoardedAt;
	protected int m_iVehicleSoakGoalRevision;
	protected EntityID m_VehicleSoakGroupId;
	protected EntityID m_VehicleSoakVehicleId;
	protected Vehicle m_VehicleSoakVehicle;
	protected ref AICF_TransportTrip m_VehicleSoakObservedTrip;
	protected ref AICF_VehiclePreset m_VehicleSoakRecipe;
	protected ref array<EntityID> m_aVehicleSoakMembers = {};
	protected ref array<string> m_aVehicleSoakFailureReasons = {};
	protected vector m_vVehicleSoakHome;
	protected vector m_vVehicleSoakStart;
	protected vector m_vVehicleSoakLastVehiclePosition;
	protected vector m_vVehicleSoakTarget;
	protected float m_fVehicleSoakMaxDisplacement;

	protected void AICF_VehicleSoakLog(string eventName, string details)
	{
		int now = System.GetTickCount();
		Print(string.Format("[AICF][VEHICLE_SOAK_%1] faction=%2 numeric_slot=0 t_ms=%3 elapsed_ms=%4 %5",
			eventName, m_sVehicleSoakFaction, now, now - m_iVehicleSoakStart, details));
	}

	protected void AICF_VehicleSoakFail(string reason)
	{
		if (m_aVehicleSoakFailureReasons.Contains(reason))
			return;
		m_aVehicleSoakFailureReasons.Insert(reason);
		m_iVehicleSoakFailures++;
		AICF_VehicleSoakLog("FAILURE", "reason=" + reason);
	}

	protected bool AICF_VehicleSoakRosterExact(AICF_GroupSlot slot)
	{
		if (!slot || !slot.GetGroup() || slot.GetGroup().GetID() != m_VehicleSoakGroupId ||
			slot.GetSpawnGeneration() != m_iVehicleSoakGeneration || AICF_GroupRuntime.CountAliveAgents(slot.GetGroup()) != 5)
			return false;
		for (int position; position < 5; position++)
		{
			IEntity actor = slot.GetRosterMember(position);
			if (!AICF_GroupRuntime.IsAliveCharacter(actor))
				return false;
			if (m_aVehicleSoakMembers.Count() == 5 && actor.GetID() != m_aVehicleSoakMembers[position])
				return false;
		}
		return true;
	}

	protected bool AICF_VehicleSoakExactSeats(AICF_GroupSlot slot, Vehicle vehicle)
	{
		if (!vehicle || vehicle.GetID() != m_VehicleSoakVehicleId || !AICF_VehicleSoakRosterExact(slot) || !m_VehicleSoakRecipe)
			return false;
		array<BaseCompartmentSlot> seats = {};
		AICF_VehiclePreset.Collect(vehicle, seats);
		if (seats.Count() != 5 || !m_VehicleSoakRecipe.MatchesSeats(seats))
			return false;
		for (int index; index < seats.Count(); index++)
		{
			int position = m_VehicleSoakRecipe.m_aMembers[index];
			if (position < 0 || position >= 5)
				return false;
			ChimeraCharacter actor = ChimeraCharacter.Cast(slot.GetRosterMember(position));
			if (!actor)
				return false;
			CompartmentAccessComponent access = actor.GetCompartmentAccessComponent();
			if (!access || !actor.IsInVehicle() || !access.IsInCompartment() || access.IsGettingIn() || access.IsGettingOut())
				return false;
			if (access.GetCompartment() != seats[index] || seats[index].GetOccupant() != actor || CompartmentAccessComponent.GetVehicleIn(actor) != vehicle)
				return false;
		}
		return true;
	}

	protected bool AICF_VehicleSoakSelect(AICF_FactionState state, SCR_CampaignFaction faction, AICF_GroupSlot slot)
	{
		AICF_VehiclePresetCatalog catalog = new AICF_VehiclePresetCatalog(faction);
		JsonLoadContext list = new JsonLoadContext();
		array<ResourceName> prefabs = {};
		if (!list.LoadFromString(catalog.List()) || !list.ReadValue("prefabs", prefabs))
		{
			catalog.Clear();
			AICF_VehicleSoakFail("CATALOG_READ_FAILED");
			return false;
		}
		foreach (ResourceName prefab : prefabs)
		{
			bool wanted;
			if (m_sVehicleSoakFaction == "US")
				wanted = prefab.Contains("/M1025_") && prefab.Contains("M2HB");
			else
				wanted = prefab.EndsWith("/UAZ469.et");
			if (!wanted || catalog.Source(prefab) != AICF_EVehicleCatalogSource.FACTION)
				continue;
			string reason;
			if (!catalog.Inspect(prefab, reason) || catalog.Seats().Count() != 5)
				continue;
			AICF_VehiclePreset candidate = catalog.Draft(prefab);
			int member;
			for (int mandatory; mandatory < catalog.Seats().Count(); mandatory++)
			{
				ECompartmentType type = catalog.Seats()[mandatory].GetType();
				if (type == ECompartmentType.PILOT || type == ECompartmentType.TURRET)
					candidate.Assign(mandatory, member++);
			}
			for (int seat; seat < catalog.Seats().Count(); seat++)
			{
				if (candidate.m_aMembers[seat] < 0)
					candidate.Assign(seat, member++);
			}
			if (member != 5 || !candidate.ValidateRoster(slot, catalog.Seats(), reason) || !PreflightVehiclePreset(slot, faction, candidate, reason))
			{
				AICF_VehicleSoakLog("VEHICLE_WAIT", "reason=" + reason + " prefab=" + prefab);
				catalog.Clear();
				return false;
			}
			bool configured = state.ApplyCommanderConfiguration(0, slot.GetRole(), AICF_EGroupUnitType.MOTORIZED_LIGHT, 5);
			if (configured)
				slot.TouchCommanderConfiguration();
			bool applied = configured && slot.SetVehiclePreset(candidate, slot.GetVehiclePresetRevision());
			if (!applied)
			{
				catalog.Clear();
				AICF_VehicleSoakFail("PRESET_APPLY_FAILED");
				return false;
			}
			m_VehicleSoakRecipe = candidate;
			AICF_VehicleSoakLog("VEHICLE", string.Format("prefab=%1 seats=5 assigned=5 revision=%2 cost=%3 authority_path=SERVER_FIXTURE_DOWNSTREAM",
				prefab, slot.GetVehiclePresetRevision(), catalog.Cost(prefab)));
			catalog.Clear();
			return true;
		}
		catalog.Clear();
		AICF_VehicleSoakFail("FACTION_FIVE_SEAT_MODEL_UNAVAILABLE");
		return false;
	}

	protected bool AICF_VehicleSoakFindTarget(AICF_GroupSlot slot, SCR_CampaignFaction faction, out vector target)
	{
		if (!m_VehicleSoakVehicle || !slot.GetGroup())
			return false;
		AIPathfindingComponent path = AIPathfindingComponent.Cast(slot.GetGroup().FindComponent(AIPathfindingComponent));
		SCR_AIWorld world = SCR_AIWorld.Cast(GetGame().GetAIWorld());
		if (!path || !path.GetNavmeshComponent() || !world || !world.GetRoadNetworkManager())
			return false;
		vector origin = m_VehicleSoakVehicle.GetOrigin();
		array<BaseRoad> roads = {};
		world.GetRoadNetworkManager().GetRoadsInAABB(origin - "1000 1000 1000", origin + "1000 1000 1000", roads);
		float best = 900;
		bool found;
		foreach (BaseRoad road : roads)
		{
			if (!road)
				continue;
			array<vector> points = {};
			road.GetPoints(points);
			foreach (vector query : points)
			{
				float distance = vector.DistanceXZ(origin, query);
				if (distance < 350 || distance >= best)
					continue;
				bool nearOtherBase;
				for (int index; index < m_ObjectiveGraph.GetNodeCount(); index++)
				{
					SCR_CampaignMilitaryBaseComponent base = m_ObjectiveGraph.GetNode(index).GetBase();
					if (!base || !base.GetOwner() || base == faction.GetMainBase())
						continue;
					if (vector.DistanceXZ(query, base.GetOwner().GetOrigin()) < 350)
					{
						nearOtherBase = true;
						break;
					}
				}
				if (nearOtherBase)
					continue;
				path.GetNavmeshComponent().LoadTileIn(query);
				vector nav;
				if (!path.GetClosestPositionOnNavmesh(query, "20 10 20", nav))
					continue;
				if (ChimeraWorldUtils.TryGetWaterSurfaceSimple(GetGame().GetWorld(), nav - "0 0.5 0"))
					continue;
				target = nav;
				best = distance;
				found = true;
			}
		}
		return found;
	}

	protected void AICF_VehicleSoakObserve(AICF_GroupSlot slot, SCR_CampaignFaction faction, int now)
	{
		int alive = AICF_GroupRuntime.CountAliveAgents(slot.GetGroup());
		if (m_iVehicleSoakLastAlive >= 0 && alive < m_iVehicleSoakLastAlive)
			AICF_VehicleSoakLog("CASUALTY", string.Format("alive=%1 previous_alive=%2 route_complete=%3 observed_change=ALIVE_COUNT_DECREASE cause=UNDETERMINED native_damage_overridden=0", alive, m_iVehicleSoakLastAlive, m_bVehicleSoakFootArrived));
		m_iVehicleSoakLastAlive = alive;
		AICF_TransportTrip trip;
		if (m_VehicleCoordinator)
			trip = m_VehicleCoordinator.GetTrip(slot);
		if (now >= m_iVehicleSoakNextSample)
		{
			m_iVehicleSoakNextSample = now + 10000;
			string phase = "NONE";
			string reason = "NONE";
			if (trip)
			{
				phase = AICF_Stage3Diagnostics.TripPhaseToString(trip.GetPhase());
				reason = trip.GetLastTransitionReason();
			}
			AICF_VehicleSoakLog("HEARTBEAT", string.Format("stage=%1 alive=%2 desired=%3 generation=%4 group=%5 trip_phase=%6 trip_reason=%7 recruiting=%8 route_complete=%9",
				m_iVehicleSoakPhase, alive, slot.GetDesiredSize(), slot.GetSpawnGeneration(), AICF_GroupTrace.Entity(slot.GetGroup()),
				phase, reason, slot.IsRecruitingInfantry(), m_bVehicleSoakFootArrived));
			AICF_GroupTrace.Snapshot(slot, faction.GetFactionKey(), trip, 0, "VEHICLE_SOAK_READBACK");
		}
		if (slot.GetGroup().GetID() != m_VehicleSoakGroupId || slot.GetSpawnGeneration() != m_iVehicleSoakGeneration)
		{
			AICF_VehicleSoakLog("GROUP_CHANGED", string.Format("group=%1 generation=%2 route_complete=%3", AICF_GroupTrace.Entity(slot.GetGroup()), slot.GetSpawnGeneration(), m_bVehicleSoakFootArrived));
			if (!m_bVehicleSoakFootArrived)
				AICF_VehicleSoakFail("GROUP_REPLACED_BEFORE_ARRIVAL");
			m_iVehicleSoakPhase = 9;
		}
	}

	override protected void Update()
	{
		super.Update();
		string requestedFaction;
		if (!Replication.IsServer() || m_bVehicleSoakDone || !System.GetCLIParam("aicfVehicleSoakFaction", requestedFaction))
			return;
		if (requestedFaction != "US" && requestedFaction != "USSR")
			return;
		int now = System.GetTickCount();
		if (m_bVehicleSoakStarted && now - m_iVehicleSoakStart >= m_iVehicleSoakDuration)
		{
			if (!m_bVehicleSoakFootArrived)
				AICF_VehicleSoakFail("ROUTE_NOT_COMPLETE_AT_DURATION");
			m_bVehicleSoakDone = true;
			AICF_VehicleSoakLog("DONE", string.Format("requested_duration_ms=%1 duration_satisfied=1 failures=%2 route_complete=%3 phase=%4",
				m_iVehicleSoakDuration, m_iVehicleSoakFailures, m_bVehicleSoakFootArrived, m_iVehicleSoakPhase));
			Stop(true);
			GetGame().RequestClose();
			return;
		}
		if (m_bVehicleSoakStarted && (m_bStopped || !m_Campaign || !m_Campaign.IsRunning()))
		{
			AICF_VehicleSoakFail("MATCH_STOPPED_BEFORE_DURATION");
			m_bVehicleSoakDone = true;
			AICF_VehicleSoakLog("ABORTED", "reason=MATCH_STOPPED_BEFORE_DURATION duration_satisfied=0");
			GetGame().RequestClose();
			return;
		}
		if (!m_bRosterReady)
			return;
		AICF_FactionState state;
		SCR_CampaignFaction faction;
		if (requestedFaction == "US")
		{
			state = m_USState;
			faction = m_USFaction;
		}
		else
		{
			state = m_USSRState;
			faction = m_USSRFaction;
		}
		if (!state || !faction || !state.GetSlot(0))
			return;
		AICF_GroupSlot slot = state.GetSlot(0);
		if (!m_bVehicleSoakStarted)
		{
			if (!slot.IsCombatReady() || !slot.GetGroup() || !faction.GetMainBase() || !faction.GetMainBase().GetSpawnPoint())
				return;
			m_sVehicleSoakFaction = requestedFaction;
			m_iVehicleSoakStart = now;
			string duration;
			if (System.GetCLIParam("aicfVehicleSoakDurationMs", duration))
				m_iVehicleSoakDuration = Math.Clamp(duration.ToInt(), 1000, 3600000);
			string mountedHold;
			if (System.GetCLIParam("aicfVehicleSoakMountedHoldMs", mountedHold))
				m_iVehicleSoakMountedHold = Math.Clamp(mountedHold.ToInt(), 10000, 180000);
			m_bVehicleSoakStarted = true;
			m_VehicleSoakGroupId = slot.GetGroup().GetID();
			m_iVehicleSoakGeneration = slot.GetSpawnGeneration();
			m_iVehicleSoakInitialAlive = AICF_GroupRuntime.CountAliveAgents(slot.GetGroup());
			m_vVehicleSoakHome = faction.GetMainBase().GetSpawnPoint().GetOrigin();
			bool configured = state.ApplyCommanderConfiguration(0, slot.GetRole(), AICF_EGroupUnitType.INFANTRY, 5);
			bool held = configured && m_OrderPlanner.AssignPlayerPointOrder(slot, faction, m_vVehicleSoakHome, false);
			AICF_VehicleSoakLog("START", string.Format("requested_duration_ms=%1 initial_alive=%2 desired=5 group=%3 generation=%4 config_accepted=%5 home_order_accepted=%6 truce=0 invulnerability=0 teleport=0 mounted_hold_ms=%7",
				m_iVehicleSoakDuration, m_iVehicleSoakInitialAlive, AICF_GroupTrace.Entity(slot.GetGroup()), m_iVehicleSoakGeneration, configured, held, m_iVehicleSoakMountedHold));
			if (!configured || !held)
				AICF_VehicleSoakFail("INITIAL_COMMAND_REJECTED");
			if (m_iVehicleSoakInitialAlive > 5)
			{
				AICF_VehicleSoakFail("INITIAL_ROSTER_EXCEEDS_FIVE");
				m_iVehicleSoakPhase = 9;
			}
		}
		if (!slot.GetGroup())
		{
			AICF_VehicleSoakFail("GROUP_UNAVAILABLE");
			return;
		}
		AICF_VehicleSoakObserve(slot, faction, now);
		if (m_iVehicleSoakPhase == 9)
			return;
		if (m_iVehicleSoakPhase == 0)
		{
			if (AICF_VehicleSoakRosterExact(slot) && !slot.IsRecruitingInfantry() && m_bVehicleSoakRecruitAccepted)
			{
				for (int member; member < 5; member++)
					m_aVehicleSoakMembers.Insert(slot.GetRosterMember(member).GetID());
				AICF_VehicleSoakLog("RECRUITED", string.Format("alive=5 desired=5 initial_alive=%1 group=%2 generation=%3", m_iVehicleSoakInitialAlive, AICF_GroupTrace.Entity(slot.GetGroup()), m_iVehicleSoakGeneration));
				for (int position; position < 5; position++)
					AICF_VehicleSoakLog("MEMBER", string.Format("position=%1 member=%2", position, AICF_GroupTrace.Entity(slot.GetRosterMember(position))));
				m_iVehicleSoakPhase = 1;
			}
			else if (now >= m_iVehicleSoakNextCommand && m_InfantryRecruitment)
			{
				m_iVehicleSoakNextCommand = now + 10000;
				string reason;
				bool accepted = m_InfantryRecruitment.RequestPlayerRecruitment(slot, faction, reason);
				if (accepted)
					m_bVehicleSoakRecruitAccepted = true;
				AICF_VehicleSoakLog("RECRUIT_REQUEST", string.Format("accepted=%1 reason=%2 alive=%3 desired=5 authority_path=SERVER_FIXTURE_DOWNSTREAM",
					accepted, reason, AICF_GroupRuntime.CountAliveAgents(slot.GetGroup())));
			}
			return;
		}
		if (m_iVehicleSoakPhase == 1)
		{
			if (now < m_iVehicleSoakNextCommand)
				return;
			m_iVehicleSoakNextCommand = now + 10000;
			if (AICF_VehicleSoakSelect(state, faction, slot))
				m_iVehicleSoakPhase = 2;
			return;
		}
		AICF_TransportTrip active;
		if (m_VehicleCoordinator)
			active = m_VehicleCoordinator.GetTrip(slot);
		if (active && m_sVehicleSoakTrip.IsEmpty())
		{
			m_sVehicleSoakTrip = active.GetOperationId();
			m_VehicleSoakObservedTrip = active;
			AICF_VehicleSoakLog("TRIP", "operation_id=" + m_sVehicleSoakTrip);
		}
		AICF_TransportTrip trip = m_VehicleSoakObservedTrip;
		if (!trip)
			return;
		if (active && active.GetOperationId() != m_sVehicleSoakTrip && !m_bVehicleSoakFootArrived)
		{
			AICF_VehicleSoakFail("TRIP_REPLACED_BEFORE_ARRIVAL");
			m_iVehicleSoakPhase = 9;
			return;
		}
		if (trip.IsTerminal() && trip.GetPhase() != AICF_ETransportTripPhase.COMPLETE)
		{
			AICF_VehicleSoakFail(trip.GetTerminalReason());
			AICF_GroupTrace.Snapshot(slot, faction.GetFactionKey(), trip, 0, "VEHICLE_SOAK_FAILURE");
			m_iVehicleSoakPhase = 9;
			return;
		}
		if (trip.GetLease() && trip.GetLease().GetVehicle())
		{
			Vehicle current = trip.GetLease().GetVehicle();
			if (m_iVehicleSoakPhase == 2 && trip.GetPhase() == AICF_ETransportTripPhase.TRANSIT)
			{
				m_VehicleSoakVehicle = current;
				m_VehicleSoakVehicleId = current.GetID();
				m_vVehicleSoakStart = current.GetOrigin();
				if (!AICF_VehicleSoakExactSeats(slot, current))
				{
					AICF_VehicleSoakFail("EXACT_BOARDING_NOT_CONFIRMED");
					m_iVehicleSoakPhase = 9;
					return;
				}
				m_iVehicleSoakBoardedAt = now;
				m_iVehicleSoakPhase = 3;
				AICF_VehicleSoakLog("BOARDED", string.Format("operation_id=%1 vehicle=%2 seats=5 occupants=5 start=%3", m_sVehicleSoakTrip, AICF_GroupTrace.Entity(current), m_vVehicleSoakStart));
			}
			if (m_VehicleSoakVehicle && current == m_VehicleSoakVehicle && current.GetID() == m_VehicleSoakVehicleId)
			{
				m_vVehicleSoakLastVehiclePosition = current.GetOrigin();
				m_fVehicleSoakMaxDisplacement = Math.Max(m_fVehicleSoakMaxDisplacement, vector.DistanceXZ(m_vVehicleSoakStart, current.GetOrigin()));
				if (current.GetPhysics() && current.GetPhysics().GetVelocity().Length() > 1)
					m_iVehicleSoakMovingPolls++;
			}
		}
		if (m_iVehicleSoakPhase == 3)
		{
			if (!AICF_VehicleSoakExactSeats(slot, m_VehicleSoakVehicle))
			{
				AICF_VehicleSoakFail("MOUNTED_HOLD_INTERRUPTED");
				m_iVehicleSoakPhase = 9;
				return;
			}
			if (now - m_iVehicleSoakBoardedAt < m_iVehicleSoakMountedHold)
				return;
			vector target;
			if (!AICF_VehicleSoakFindTarget(slot, faction, target))
				return;
			bool accepted = m_OrderPlanner.AssignPlayerPointOrder(slot, faction, target, m_VehicleCoordinator.IsInfantryOrderSuspended(slot));
			if (!accepted)
			{
				AICF_VehicleSoakFail("POINT_ORDER_REJECTED");
				return;
			}
			m_vVehicleSoakTarget = target;
			m_iVehicleSoakGoalRevision = slot.GetStrategicIntentRevision();
			m_iVehicleSoakPhase = 4;
			AICF_VehicleSoakLog("MOUNTED_HOLD", string.Format("operation_id=%1 configured_ms=%2 held_ms=%3 exact_settled=1 alive=5",
				m_sVehicleSoakTrip, m_iVehicleSoakMountedHold, now - m_iVehicleSoakBoardedAt));
			AICF_VehicleSoakLog("TARGET", string.Format("operation_id=%1 position=%2 intent_revision=%3 vehicle=%4 dismount_radius_m=%5",
				m_sVehicleSoakTrip, target, m_iVehicleSoakGoalRevision, AICF_GroupTrace.Entity(m_VehicleSoakVehicle), m_Stage3Config.GetDismountDistanceMeters()));
		}
		if (m_iVehicleSoakPhase == 4 && trip.GetPhase() != AICF_ETransportTripPhase.TRANSIT)
		{
			float distance = vector.DistanceXZ(m_vVehicleSoakLastVehiclePosition, m_vVehicleSoakTarget);
			m_bVehicleSoakArrived = distance <= m_Stage3Config.GetDismountDistanceMeters() + 10 &&
				m_fVehicleSoakMaxDisplacement >= 100 && m_iVehicleSoakMovingPolls >= 5 && slot.GetStrategicIntentRevision() == m_iVehicleSoakGoalRevision;
			AICF_VehicleSoakLog("ARRIVAL", string.Format("operation_id=%1 passed=%2 vehicle=%3 last_position=%4 distance_m=%5 displacement_m=%6 moving_polls=%7 intent_revision=%8",
				m_sVehicleSoakTrip, m_bVehicleSoakArrived, m_VehicleSoakVehicleId, m_vVehicleSoakLastVehiclePosition, distance,
				m_fVehicleSoakMaxDisplacement, m_iVehicleSoakMovingPolls, m_iVehicleSoakGoalRevision));
			if (!m_bVehicleSoakArrived)
				AICF_VehicleSoakFail("PREMATURE_TRANSIT_EXIT");
			m_iVehicleSoakPhase = 5;
		}
		if (m_iVehicleSoakPhase == 5 && trip.GetPhase() == AICF_ETransportTripPhase.COMPLETE)
		{
			if (!m_bVehicleSoakArrived || !AICF_VehicleSoakRosterExact(slot) || AICF_GroupRuntime.CountAliveAgentsInAnyVehicle(slot.GetGroup()) != 0)
			{
				AICF_VehicleSoakFail("NORMAL_DISMOUNT_NOT_CONFIRMED");
				m_iVehicleSoakPhase = 9;
				return;
			}
			m_iVehicleSoakPhase = 6;
			AICF_VehicleSoakLog("DISMOUNTED", "operation_id=" + m_sVehicleSoakTrip + " alive=5 in_vehicle=0 normal_complete=1");
		}
		if (m_iVehicleSoakPhase == 6)
		{
			if (!AICF_VehicleSoakRosterExact(slot))
			{
				AICF_VehicleSoakFail("ROSTER_CHANGED_BEFORE_FOOT_ARRIVAL");
				m_iVehicleSoakPhase = 9;
				return;
			}
			float farthest;
			for (int actor; actor < 5; actor++)
				farthest = Math.Max(farthest, vector.DistanceXZ(slot.GetRosterMember(actor).GetOrigin(), m_vVehicleSoakTarget));
			if (farthest <= 35)
			{
				m_bVehicleSoakFootArrived = true;
				m_iVehicleSoakPhase = 7;
				AICF_VehicleSoakLog("FOOT_ARRIVAL", string.Format("operation_id=%1 alive=5 farthest_m=%2 position=%3 intent_revision=%4", m_sVehicleSoakTrip, farthest, m_vVehicleSoakTarget, m_iVehicleSoakGoalRevision));
			}
		}
	}
}
