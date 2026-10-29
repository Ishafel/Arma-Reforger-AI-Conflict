// Test-only: настоящий managed slot0, штатный найм/экономика/vehicle lifecycle.
// Отсутствуют fake roster, instant buildings, truce, invulnerability и teleport.
// Пассивный observer: контекст native damage фиксируется без изменения повреждений.
class AICF_VehicleSoakSurvivorProof
{
	static bool AcceptOriginal(bool capturedExact, bool alive, bool currentExact, bool foreignCurrent)
	{
		return capturedExact && !foreignCurrent && (!alive || currentExact);
	}
}

class AICF_VehicleSoakDamageObserver
{
	protected AICF_GroupSlot m_Slot;
	protected SCR_AIGroup m_Group;
	protected ChimeraCharacter m_Actor;
	protected SCR_DamageManagerComponent m_Damage;
	protected EntityID m_ActorId;
	protected EntityID m_GroupId;
	protected string m_sActorRpl;
	protected string m_sGroupRpl;
	protected string m_sFaction;
	protected int m_iPosition;
	protected int m_iGeneration;
	protected int m_iStartedAt;
	protected bool m_bBound;

	static bool MatchesIdentity(bool exactActor, bool exactGroup, bool exactSlot, bool exactRpl, bool exactGeneration)
	{
		return exactActor && exactGroup && exactSlot && exactRpl && exactGeneration;
	}

	bool Bind(AICF_GroupSlot slot, int position, string faction, int startedAt)
	{
		if (m_bBound || !slot || !slot.GetGroup()) return false;
		ChimeraCharacter actor = ChimeraCharacter.Cast(slot.GetRosterMember(position));
		if (!actor) return false;
		SCR_DamageManagerComponent damage = SCR_DamageManagerComponent.Cast(actor.GetDamageManager());
		if (!damage) return false;
		m_Slot = slot; m_Group = slot.GetGroup(); m_Actor = actor; m_Damage = damage;
		m_ActorId = actor.GetID(); m_GroupId = m_Group.GetID();
		m_sActorRpl = Replication.FindItemId(actor).ToString();
		m_sGroupRpl = Replication.FindItemId(m_Group).ToString();
		m_sFaction = faction; m_iPosition = position; m_iGeneration = slot.GetSpawnGeneration();
		m_iStartedAt = startedAt; m_bBound = true;
		m_Damage.GetOnDamage().Insert(OnDamage);
		return true;
	}

	bool IsCurrent()
	{
		if (!m_bBound || !m_Slot || !m_Actor || !m_Group || !m_Damage) return false;
		return MatchesIdentity(m_Actor.GetID() == m_ActorId && m_Actor.GetDamageManager() == m_Damage,
			m_Group.GetID() == m_GroupId && m_Slot.GetGroup() == m_Group,
			m_Slot.GetRosterMember(m_iPosition) == m_Actor,
			Replication.FindItemId(m_Actor).ToString() == m_sActorRpl && Replication.FindItemId(m_Group).ToString() == m_sGroupRpl,
			m_Slot.GetSpawnGeneration() == m_iGeneration);
	}

	void Stop()
	{
		if (m_bBound && m_Damage) m_Damage.GetOnDamage().Remove(OnDamage);
		m_bBound = false;
	}

	void Poll()
	{
		if (!IsCurrent()) Stop();
	}

	protected void OnDamage(BaseDamageContext context)
	{
		if (!IsCurrent()) { Stop(); return; }
		if (!context || context.hitEntity != m_Actor) return;
		IEntity instigator;
		string instigatorType = "UNAVAILABLE";
		int instigatorPlayer;
		if (context.instigator)
		{
			instigator = context.instigator.GetInstigatorEntity();
			instigatorType = SCR_Enum.GetEnumName(InstigatorType, context.instigator.GetInstigatorType());
			instigatorPlayer = context.instigator.GetInstigatorPlayerID();
		}
		CharacterControllerComponent controller = m_Actor.GetCharacterController();
		string life = "UNAVAILABLE";
		if (controller) life = SCR_Enum.GetEnumName(ECharacterLifeState, controller.GetLifeState());
		AIAgent agent;
		SCR_AIGroup nativeGroup;
		if (m_Actor.GetAIControlComponent()) agent = m_Actor.GetAIControlComponent().GetControlAIAgent();
		if (agent) nativeGroup = SCR_AIGroup.Cast(agent.GetParentGroup());
		int now = System.GetTickCount();
		string details = string.Format("[AICF][VEHICLE_SOAK_DAMAGE] faction=%1 numeric_slot=0 t_ms=%2 elapsed_ms=%3 position=%4 actor=%5 actor_rpl=%6 actor_position=%7 life=%8 generation=%9",
			m_sFaction, now, now - m_iStartedAt, m_iPosition, m_ActorId, m_sActorRpl, m_Actor.GetOrigin(), life, m_iGeneration);
		details += string.Format(" damage_type=%1 damage_value=%2 source=%3 hit_entity=%4 hit_position=%5 impact_velocity=%6",
			SCR_Enum.GetEnumName(EDamageType, context.damageType), context.damageValue, AICF_GroupTrace.Entity(context.damageSource),
			AICF_GroupTrace.Entity(context.hitEntity), context.hitPosition, context.impactVelocity);
		details += string.Format(" instigator_type=%1 instigator_entity=%2 instigator_player=%3 native_group=%4 vehicle=%5 in_vehicle=%6 cause=UNDETERMINED observer_only=1",
			instigatorType, AICF_GroupTrace.Entity(instigator), instigatorPlayer, AICF_GroupTrace.Entity(nativeGroup),
			AICF_GroupTrace.Entity(CompartmentAccessComponent.GetVehicleIn(m_Actor)), m_Actor.IsInVehicle());
		Print(details);
	}
}

modded class AICF_MatchController
{
	protected bool m_bVehicleSoakStarted;
	protected bool m_bVehicleSoakDone;
	protected bool m_bVehicleSoakRecruitAccepted;
	protected bool m_bVehicleSoakArrived;
	protected bool m_bVehicleSoakFootArrived;
	protected bool m_bVehicleSoakMountedCombatWait;
	protected bool m_bVehicleSoakCombatInterrupted;
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
	protected int m_iVehicleSoakFootClearPolls;
	protected EntityID m_VehicleSoakGroupId;
	protected EntityID m_VehicleSoakVehicleId;
	protected Vehicle m_VehicleSoakVehicle;
	protected ref AICF_TransportTrip m_VehicleSoakObservedTrip;
	protected ref AICF_VehiclePreset m_VehicleSoakRecipe;
	protected ref array<EntityID> m_aVehicleSoakMembers = {};
	protected ref array<ref AICF_VehicleSoakDamageObserver> m_aVehicleSoakDamageObservers = {};
	protected ref array<string> m_aVehicleSoakFailureReasons = {};
	protected vector m_vVehicleSoakHome;
	protected vector m_vVehicleSoakStart;
	protected vector m_vVehicleSoakLastVehiclePosition;
	protected vector m_vVehicleSoakTarget;
	protected float m_fVehicleSoakMaxDisplacement;

	protected void AICF_VehicleSoakStopDamageObservers()
	{
		foreach (AICF_VehicleSoakDamageObserver observer : m_aVehicleSoakDamageObservers)
			observer.Stop();
		m_aVehicleSoakDamageObservers.Clear();
	}

	override protected void Stop(bool cleanupEntities)
	{
		AICF_VehicleSoakStopDamageObservers();
		super.Stop(cleanupEntities);
	}

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

	protected bool AICF_VehicleSoakSurvivorsCurrent(AICF_GroupSlot slot, AICF_TransportTrip trip, out int fitCount)
	{
		fitCount = 0;
		if (!slot || !slot.GetGroup() || slot.GetGroup().GetID() != m_VehicleSoakGroupId ||
			slot.GetSpawnGeneration() != m_iVehicleSoakGeneration || m_aVehicleSoakMembers.Count() != 5 ||
			!trip || trip != m_VehicleSoakObservedTrip || trip != m_VehicleCoordinator.GetTrip(slot) ||
			!trip.IsVehiclePresetCurrent()) return false;
		int aliveOriginals;
		for (int position; position < 5; position++)
		{
			ChimeraCharacter actor = ChimeraCharacter.Cast(trip.VehiclePresetMember(position));
			if (!actor || !actor.GetCharacterController()) return false;
			IEntity current = slot.GetRosterMember(position);
			bool alive = AICF_GroupRuntime.IsAliveCharacter(actor);
			if (!AICF_VehicleSoakSurvivorProof.AcceptOriginal(actor && actor.GetID() == m_aVehicleSoakMembers[position],
				alive, current == actor, current && current != actor)) return false;
			if (alive) aliveOriginals++;
			if (alive && SCR_AIDamageHandling.IsConscious(actor)) fitCount++;
		}
		return fitCount > 0 && AICF_GroupRuntime.CountAliveAgents(slot.GetGroup()) == aliveOriginals;
	}

	protected bool AICF_VehicleSoakRetainedFoot(AICF_GroupSlot slot, AICF_TransportTrip trip)
	{
		int fitCount;
		if (!AICF_VehicleSoakSurvivorsCurrent(slot, trip, fitCount) || !trip || trip.IsTerminal() ||
			trip != m_VehicleCoordinator.GetTrip(slot) || trip != m_VehicleSoakObservedTrip ||
			trip.GetPhase() != AICF_ETransportTripPhase.TACTICAL_DEPLOYMENT ||
			!trip.IsVehiclePresetCurrent() || !trip.GetHandoffState().IsOrderRestored() ||
			trip.GetBehaviorState().m_bReboarding || !trip.GetLease() || !trip.GetLease().HasPhysicalAsset()) return false;
		Vehicle vehicle = trip.GetLease().GetVehicle();
		if (!vehicle || vehicle != m_VehicleSoakVehicle || vehicle.GetID() != m_VehicleSoakVehicleId ||
			!trip.GetLease().MatchesTripIdentity(trip.GetFactionKey(), trip.GetSlotId(), trip.GetGroupGeneration(), trip.GetTripGeneration())) return false;
		vector boundsMin, boundsMax;
		vehicle.GetBounds(boundsMin, boundsMax);
		for (int position; position < 5; position++)
		{
			// Включая исходный incapacitated/dead actor: slot скрывает его,
			// но отсутствие боеспособности не доказывает physical detach.
			ChimeraCharacter actor = ChimeraCharacter.Cast(trip.VehiclePresetMember(position));
			if (!actor) return false;
			CompartmentAccessComponent access = actor.GetCompartmentAccessComponent();
			if (!access || actor.IsInVehicle() || CompartmentAccessComponent.GetVehicleIn(actor) ||
				access.IsInCompartment() || access.IsGettingIn() || access.IsGettingOut() || access.IsSwitchingSeatsAnim()) return false;
			vector local = vehicle.CoordToLocal(actor.GetOrigin());
			bool inside = local[0] >= boundsMin[0] - 1.5 && local[0] <= boundsMax[0] + 1.5 &&
				local[1] >= boundsMin[1] - 1.5 && local[1] <= boundsMax[1] + 1.5 &&
				local[2] >= boundsMin[2] - 1.5 && local[2] <= boundsMax[2] + 1.5;
			if (inside) return false;
		}
		return true;
	}

	protected float AICF_VehicleSoakFarthest(AICF_TransportTrip trip)
	{
		float farthest;
		for (int position; position < 5; position++)
		{
			IEntity actor = trip.VehiclePresetMember(position);
			if (AICF_GroupRuntime.IsAliveCharacter(actor) && SCR_AIDamageHandling.IsConscious(actor))
				farthest = Math.Max(farthest, vector.DistanceXZ(actor.GetOrigin(), m_vVehicleSoakTarget));
		}
		return farthest;
	}

	protected void AICF_VehicleSoakMemberFootReceipts(AICF_GroupSlot slot, AICF_TransportTrip trip, string eventName)
	{
		for (int position; position < 5; position++)
		{
			ChimeraCharacter actor = ChimeraCharacter.Cast(trip.VehiclePresetMember(position));
			bool conscious = SCR_AIDamageHandling.IsConscious(actor);
			bool fit = AICF_GroupRuntime.IsAliveCharacter(actor) && conscious;
			string life = SCR_Enum.GetEnumName(ECharacterLifeState, actor.GetCharacterController().GetLifeState());
			string details = string.Format("operation_id=%1 position=%2 member=%3 life=%4 conscious=%5 fit=%6 current_member=%7 target_distance_m=%8",
				m_sVehicleSoakTrip, position, AICF_GroupTrace.Entity(actor), life, conscious, fit,
				slot.GetRosterMember(position) == actor, vector.DistanceXZ(actor.GetOrigin(), m_vVehicleSoakTarget));
			AICF_VehicleSoakLog(eventName, details + " captured=1 on_foot=1 transition=0 inside_bounds=0 survivor_proof=1");
		}
	}

	protected void AICF_VehicleSoakArrival(AICF_GroupSlot slot, string category)
	{
		float distance = vector.DistanceXZ(m_vVehicleSoakLastVehiclePosition, m_vVehicleSoakTarget);
		bool physicalDrive = m_fVehicleSoakMaxDisplacement >= 100 && m_iVehicleSoakMovingPolls >= 5 &&
			slot.GetStrategicIntentRevision() == m_iVehicleSoakGoalRevision;
		if (category == "COMBAT_INTERRUPTED_COMBINED_ROUTE")
			m_bVehicleSoakArrived = physicalDrive && m_bVehicleSoakCombatInterrupted && AICF_VehicleSoakFarthest(m_VehicleSoakObservedTrip) <= 35;
		else
			m_bVehicleSoakArrived = physicalDrive && distance <= m_Stage3Config.GetDismountDistanceMeters() + 10;
		AICF_VehicleSoakLog("ARRIVAL", string.Format("operation_id=%1 passed=%2 vehicle=%3 last_position=%4 distance_m=%5 displacement_m=%6 moving_polls=%7 intent_revision=%8 category=%9",
			m_sVehicleSoakTrip, m_bVehicleSoakArrived, m_VehicleSoakVehicleId, m_vVehicleSoakLastVehiclePosition, distance,
			m_fVehicleSoakMaxDisplacement, m_iVehicleSoakMovingPolls, m_iVehicleSoakGoalRevision, category));
		if (!m_bVehicleSoakArrived) AICF_VehicleSoakFail("PHYSICAL_ROUTE_NOT_CONFIRMED");
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
			candidate.m_iBehavior = AICF_EVehicleBehavior.TRANSPORT;
			candidate.m_iReturnDistance = 300;
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

	protected void AICF_VehicleSoakHeartbeat(AICF_GroupSlot slot, SCR_CampaignFaction faction, AICF_TransportTrip trip, int now)
	{
		if (!slot || !faction || now < m_iVehicleSoakNextSample) return;
		int alive = AICF_GroupRuntime.CountAliveAgents(slot.GetGroup());
		m_iVehicleSoakNextSample = now + 10000;
		string phase = "NONE";
		string reason = "NONE";
		string retirement = "NONE";
		if (trip)
		{
			phase = AICF_Stage3Diagnostics.TripPhaseToString(trip.GetPhase());
			reason = trip.GetLastTransitionReason();
			AICF_TripRetirementObservation evidence = trip.ObserveCapturedDeadRetirement();
			if (evidence) retirement = typename.EnumToString(AICF_ETripRetirementObservation, evidence.GetStatus());
		}
		string details = string.Format("stage=%1 alive=%2 desired=%3 generation=%4 group=%5 trip_phase=%6 trip_reason=%7 recruiting=%8 route_complete=%9",
			m_iVehicleSoakPhase, alive, slot.GetDesiredSize(), slot.GetSpawnGeneration(), AICF_GroupTrace.Entity(slot.GetGroup()),
			phase, reason, slot.IsRecruitingInfantry(), m_bVehicleSoakFootArrived);
		details += string.Format(" group_present=%1 slot_state=%2 native_capacity_wait=%3 retirement_status=%4",
			slot.GetGroup() != null, typename.EnumToString(AICF_EGroupSlotState, slot.GetState()), slot.HasNativeSpawnCapacityReport(), retirement);
		AICF_VehicleSoakLog("HEARTBEAT", details);
		if (slot.GetGroup()) AICF_GroupTrace.Snapshot(slot, faction.GetFactionKey(), trip, 0, "VEHICLE_SOAK_READBACK");
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
		AICF_VehicleSoakHeartbeat(slot, faction, trip, now);
		if (slot.GetGroup().GetID() != m_VehicleSoakGroupId || slot.GetSpawnGeneration() != m_iVehicleSoakGeneration)
		{
			AICF_VehicleSoakStopDamageObservers();
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
			AICF_VehicleSoakStopDamageObservers();
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
			AICF_VehicleSoakStopDamageObservers();
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
				m_iVehicleSoakDuration, m_iVehicleSoakInitialAlive, AICF_GroupTrace.Entity(slot.GetGroup()), m_iVehicleSoakGeneration, configured, held, m_iVehicleSoakMountedHold) +
				" proof_version=3 teleport_scope=FIXTURE_ONLY production_short_recovery=1 behavior=TRANSPORT return_distance_m=300 casualty_policy=CAPTURED_ORIGINAL_SURVIVORS");
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
			AICF_TransportTrip waitingTrip;
			if (m_VehicleCoordinator) waitingTrip = m_VehicleCoordinator.GetTrip(slot);
			AICF_VehicleSoakHeartbeat(slot, faction, waitingTrip, now);
			AICF_VehicleSoakFail("GROUP_UNAVAILABLE");
			return;
		}
		AICF_VehicleSoakObserve(slot, faction, now);
		foreach (AICF_VehicleSoakDamageObserver damageObserver : m_aVehicleSoakDamageObservers)
			damageObserver.Poll();
		if (m_iVehicleSoakPhase == 9)
			return;
		if (m_iVehicleSoakPhase == 0)
		{
			if (AICF_VehicleSoakRosterExact(slot) && !slot.IsRecruitingInfantry() && m_bVehicleSoakRecruitAccepted)
			{
				for (int member; member < 5; member++)
					m_aVehicleSoakMembers.Insert(slot.GetRosterMember(member).GetID());
				for (int damageMember; damageMember < 5; damageMember++)
				{
					AICF_VehicleSoakDamageObserver observer = new AICF_VehicleSoakDamageObserver();
					if (!observer.Bind(slot, damageMember, m_sVehicleSoakFaction, m_iVehicleSoakStart))
						AICF_VehicleSoakFail("DAMAGE_OBSERVER_BIND_FAILED");
					m_aVehicleSoakDamageObservers.Insert(observer);
				}
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
			if (!AICF_VehicleSoakRosterExact(slot))
			{
				AICF_VehicleSoakFail("ROSTER_CHANGED_DURING_MOUNTED_HOLD");
				m_iVehicleSoakPhase = 9;
				return;
			}
			if (!AICF_VehicleSoakExactSeats(slot, m_VehicleSoakVehicle))
			{
				bool ownDeployment = trip.GetPhase() == AICF_ETransportTripPhase.TACTICAL_DEPLOYMENT &&
					trip.GetBehaviorState().m_bThreatDeployment && trip.GetLastTransitionReason() == "LOCAL_THREAT";
				if (ownDeployment || m_bVehicleSoakMountedCombatWait)
				{
					if (!m_bVehicleSoakMountedCombatWait)
						AICF_VehicleSoakLog("MOUNTED_COMBAT_WAIT", "operation_id=" + m_sVehicleSoakTrip + " reason=LOCAL_THREAT proof=OWN_TACTICAL_DEPLOYMENT");
					m_bVehicleSoakMountedCombatWait = true;
					m_iVehicleSoakBoardedAt = now;
					return;
				}
				AICF_VehicleSoakFail("MOUNTED_HOLD_INTERRUPTED");
				m_iVehicleSoakPhase = 9;
				return;
			}
			if (AICF_VehicleBehaviorPolicy.HasLocalCombat(trip))
			{
				m_iVehicleSoakBoardedAt = now;
				return;
			}
			if (m_bVehicleSoakMountedCombatWait)
			{
				m_bVehicleSoakMountedCombatWait = false;
				m_iVehicleSoakBoardedAt = now;
				AICF_VehicleSoakLog("MOUNTED_HOLD_RESUMED", "operation_id=" + m_sVehicleSoakTrip + " exact_settled=1 calm=1");
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
			if (trip.GetPhase() == AICF_ETransportTripPhase.TACTICAL_DEPLOYMENT && trip.GetLastTransitionReason() == "LOCAL_THREAT")
			{
				if (!m_bVehicleSoakCombatInterrupted)
					AICF_VehicleSoakLog("COMBAT_INTERRUPTION", string.Format("operation_id=%1 reason=LOCAL_THREAT phase=TACTICAL_DEPLOYMENT vehicle=%2 intent_revision=%3",
						m_sVehicleSoakTrip, AICF_GroupTrace.Entity(m_VehicleSoakVehicle), m_iVehicleSoakGoalRevision));
				m_bVehicleSoakCombatInterrupted = true;
			}
			else AICF_VehicleSoakArrival(slot, "DIRECT_VEHICLE_ARRIVAL");
			m_iVehicleSoakPhase = 5;
		}
		if (m_iVehicleSoakPhase == 5)
		{
			int retainedFit;
			if (!AICF_VehicleSoakSurvivorsCurrent(slot, trip, retainedFit) || slot.GetStrategicIntentRevision() != m_iVehicleSoakGoalRevision)
			{
				AICF_VehicleSoakFail("RETAINED_DISMOUNT_NOT_CONFIRMED");
				m_iVehicleSoakPhase = 9;
				return;
			}
			if (trip.GetPhase() == AICF_ETransportTripPhase.TRANSIT)
			{
				m_iVehicleSoakPhase = 4;
				return;
			}
			if (!AICF_VehicleSoakRetainedFoot(slot, trip))
			{
				m_iVehicleSoakFootClearPolls = 0;
				return;
			}
			m_iVehicleSoakFootClearPolls++;
			if (m_iVehicleSoakFootClearPolls < 2) return;
			if (!m_bVehicleSoakArrived)
			{
				if (!m_bVehicleSoakCombatInterrupted || AICF_VehicleSoakFarthest(trip) > 35) return;
				AICF_VehicleSoakArrival(slot, "COMBAT_INTERRUPTED_COMBINED_ROUTE");
				if (!m_bVehicleSoakArrived) { m_iVehicleSoakPhase = 9; return; }
			}
			m_iVehicleSoakPhase = 6;
			AICF_VehicleSoakMemberFootReceipts(slot, trip, "MEMBER_FOOT");
			AICF_VehicleSoakLog("TRIP_RETAINED_FOOT", string.Format("operation_id=%1 vehicle=%2 phase=TACTICAL_DEPLOYMENT alive=%3 in_vehicle=0 logical_occupants=0 transitions=0 inside_bounds=0 order_restored=1 same_lease=1 reboarding=0 clear_polls=%4 proof_version=3 fit_count=%5 originals=5 survivor_proof=1",
				m_sVehicleSoakTrip, AICF_GroupTrace.Entity(m_VehicleSoakVehicle), AICF_GroupRuntime.CountAliveAgents(slot.GetGroup()), m_iVehicleSoakFootClearPolls, retainedFit));
		}
		if (m_iVehicleSoakPhase == 6)
		{
			int arrivalFit;
			if (!AICF_VehicleSoakSurvivorsCurrent(slot, trip, arrivalFit) || slot.GetStrategicIntentRevision() != m_iVehicleSoakGoalRevision)
			{
				AICF_VehicleSoakFail("ROSTER_CHANGED_BEFORE_FOOT_ARRIVAL");
				m_iVehicleSoakPhase = 9;
				return;
			}
			if (!AICF_VehicleSoakRetainedFoot(slot, trip)) return;
			float farthest = AICF_VehicleSoakFarthest(trip);
			if (farthest <= 35)
			{
				m_bVehicleSoakFootArrived = true;
				m_iVehicleSoakPhase = 7;
				AICF_VehicleSoakMemberFootReceipts(slot, trip, "MEMBER_ARRIVAL");
				AICF_VehicleSoakLog("FOOT_ARRIVAL", string.Format("operation_id=%1 alive=%2 farthest_m=%3 position=%4 intent_revision=%5 fit_count=%6 originals=5 survivor_proof=1", m_sVehicleSoakTrip, AICF_GroupRuntime.CountAliveAgents(slot.GetGroup()), farthest, m_vVehicleSoakTarget, m_iVehicleSoakGoalRevision, arrivalFit));
			}
		}
	}
}
