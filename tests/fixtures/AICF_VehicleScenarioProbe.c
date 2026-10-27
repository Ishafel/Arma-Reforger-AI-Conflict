// Разрешает только доказанную штатную цепочку OpenGate → exact-seat return.
// Наблюдатель fixture не выдаёт actions и не меняет production deadlines.
class AICF_VSNativeGate
{
	IEntity m_Member;
	EntityID m_Id;
	BaseCompartmentSlot m_Seat;
	SCR_AIUtilityComponent m_Utility;
	ref SCR_AIGetInVehicle m_Return;
	int m_iStarted;
	string m_sOperation;

	static AICF_VSNativeGate Begin(AICF_TransportTrip trip, IEntity member, BaseCompartmentSlot seat)
	{
		ChimeraCharacter character = ChimeraCharacter.Cast(member);
		if (!character || !character.GetAIControlComponent()) return null;
		AIAgent agent = character.GetAIControlComponent().GetControlAIAgent();
		if (!agent || agent.GetParentGroup() != trip.GetAssignment().GetGroup()) return null;
		SCR_AIUtilityComponent utility = SCR_AIUtilityComponent.Cast(agent.FindComponent(SCR_AIUtilityComponent));
		if (!utility || utility.m_OwnerEntity != member) return null;
		array<ref AIActionBase> actions = {};
		utility.GetActions(actions);
		foreach (AIActionBase candidate : actions)
		{
			SCR_AIPerformActionBehavior perform = SCR_AIPerformActionBehavior.Cast(candidate);
			if (!AICF_LogisticsDriverInteraction.Live(perform) || perform.GetPriority() != SCR_AIActionBase.PRIORITY_BEHAVIOR_OPEN_NAVLINK_DOOR) continue;
			SCR_AIActivityBase activity = SCR_AIActivityBase.Cast(perform.GetRelatedGroupActivity());
			if (!activity || activity.m_Utility != trip.GetAssignment().GetGroup().GetGroupUtilityComponent()) continue;
			SCR_AISmartActionComponent smart = perform.m_SmartActionComponent.m_Value;
			if (!smart || !smart.GetOwner() || vector.Distance(seat.GetVehicle().GetOrigin(), smart.GetOwner().GetOrigin()) > 50) continue;
			array<string> tags = {};
			smart.GetTags(tags);
			if (!tags.Contains(SCR_AIGoalReaction_OpenNavlinkDoor.SMART_ACTION_TAG)) continue;
			SCR_AIGetInVehicle returnAction;
			SCR_AIGetOutVehicle exitAction;
			foreach (AIActionBase sibling : actions)
			{
				if (!AICF_LogisticsDriverInteraction.Live(sibling) || sibling.GetRelatedGroupActivity() != activity) continue;
				SCR_AIGetInVehicle getIn = SCR_AIGetInVehicle.Cast(sibling);
				if (getIn && getIn.m_Vehicle.m_Value == seat.GetOwner() && getIn.m_CompartmentToGetIn.m_Value == seat && getIn.GetPriority() == perform.GetPriority()) returnAction = getIn;
				SCR_AIGetOutVehicle getOut = SCR_AIGetOutVehicle.Cast(sibling);
				if (getOut && getOut.m_Vehicle.m_Value == seat.GetOwner() && getOut.GetPriority() == perform.GetPriority() + 100) exitAction = getOut;
			}
			if (!returnAction || (utility.GetCurrentBehavior() != perform && (!exitAction || utility.GetCurrentBehavior() != exitAction))) continue;
			AICF_VSNativeGate gate = new AICF_VSNativeGate();
			gate.m_Member = member;
			gate.m_Id = member.GetID();
			gate.m_Seat = seat;
			gate.m_Utility = utility;
			gate.m_Return = returnAction;
			gate.m_iStarted = System.GetTickCount();
			gate.m_sOperation = trip.GetOperationId();
			return gate;
		}
		return null;
	}
}

// Только политика выбора native observer; root keys/Start/Update не вызываются.
class AICF_VSObserverPolicy : AICF_ManagedObservers
{
	IEntity ResolveCurrent(SCR_GameModeCampaign campaign, AICF_FactionState state, AICF_GroupSlot slot, SCR_AIGroup group, EntityID groupId, Faction faction, FactionKey expected)
	{
		if (!Replication.IsServer() || !campaign || !campaign.IsMaster() || !campaign.IsRunning()) return null;
		if (!state || !slot || state.GetSlot(0) != slot || !slot.IsCombatReady()) return null;
		if (!group || group.GetID() != groupId || slot.GetGroup() != group || !faction || group.GetFaction() != faction) return null;
		if (!AICF_VehicleBoardingMutationFence.IsAuthoritativeReplicatedEntity(group)) return null;
		if ((expected != "US" && expected != "USSR") || state.GetFactionKey() != faction.GetFactionKey() || AICF_ContentProfile.GetActive().GetStableFactionKey(state.GetFactionKey()) != expected) return null;
		// ResolveMember проверяет native live leader, group master и faction бойца.
		return ResolveMember(group, expected);
	}
}

// Полная поездка к неизменной цели и пешее прибытие; только isolated stage.
modded class AICF_MatchController
{
	protected SCR_CampaignFaction m_VSFaction;
	protected FactionKey m_sVSFaction = "USSR";
	protected static const int AICF_VS_OBSERVER_KEY = -1513170001;
	protected ref AICF_VSObserverPolicy m_VSObserverPolicy = new AICF_VSObserverPolicy();
	protected ObserversSystem m_VSObservers;
	protected IEntity m_VSObservedMember;
	protected bool m_bVSObserverRequested;
	protected ref AICF_FactionState m_VSState;
	protected ref AICF_VehicleCoordinator m_VSVehicles;
	protected SCR_AIGroup m_VSGroup;
	protected EntityID m_VSGroupId;
	protected ref AICF_VehiclePreset m_VSRecipe;
	protected Vehicle m_VSVehicle;
	protected ref AICF_TransportTrip m_VSCompletedTrip;
	protected EntityID m_VSVehicleId;
	protected string m_sVSCase;
	protected string m_sVSPreviousTrip;
	protected int m_iVSSize = 5;
	protected int m_iVSStep;
	protected int m_iVSStarted;
	protected int m_iVSParked;
	protected int m_iVSTiles;
	protected int m_iVSChecks;
	protected int m_iVSFailures;
	protected int m_iVSLeg = 1;
	protected int m_iVSMoving;
	protected int m_iVSRecoveries;
	protected int m_iVSSample;
	protected int m_iVSGoalRevision;
	protected int m_iVSRepeatRequested;
	protected bool m_bVSRetargeted;
	protected bool m_bVSDone;
	protected bool m_bVSTruce;
	protected vector m_vVSStart;
	protected vector m_vVSGoal;
	protected vector m_vVSHome;
	protected vector m_vVSRepeatTarget;
	protected ref array<EntityID> m_aVSMembers = {};
	protected ref array<ref AICF_VSNativeGate> m_aVSGates = {};

	protected bool AICF_VSSelectFaction()
	{
		if (m_VSFaction || m_VSState) return false;
		string requested = "USSR";
		if (!System.GetCLIParam("aicfVehicleScenarioFaction", requested)) requested = "USSR";
		if (requested != "US" && requested != "USSR") return false;
		if (requested == "US" && m_sVSCase != "repeat5") return false;
		SCR_CampaignFaction selected = m_USSRFaction;
		if (requested == "US") selected = m_USFaction;
		if (!selected || !selected.GetMainBase() || AICF_ContentProfile.GetActive().GetStableFactionKey(selected.GetFactionKey()) != requested) return false;
		m_VSFaction = selected;
		m_sVSFaction = requested;
		return true;
	}

	protected bool AICF_VSUSModelCandidate(ResourceName prefab, int source, int seats, int cost)
	{
		return prefab.Contains("/M1025_") && prefab.Contains("M2HB") && source == AICF_EVehicleCatalogSource.FACTION && seats == 5 && cost > 0;
	}

	protected bool AICF_VSSelectUSModel(AICF_VehiclePresetCatalog catalog, out ResourceName selected)
	{
		JsonLoadContext list = new JsonLoadContext();
		array<ResourceName> prefabs = {};
		if (!list.LoadFromString(catalog.List()) || !list.ReadValue("prefabs", prefabs)) return false;
		foreach (ResourceName prefab : prefabs)
		{
			string reason;
			if (!prefab.Contains("/M1025_") || !prefab.Contains("M2HB") || catalog.Source(prefab) != AICF_EVehicleCatalogSource.FACTION) continue;
			if (!catalog.Inspect(prefab, reason) || !AICF_VSUSModelCandidate(prefab, catalog.Source(prefab), catalog.Seats().Count(), catalog.Cost(prefab))) continue;
			selected = prefab;
			return true;
		}
		return false;
	}

	protected void AICF_VSUpdateVehicles()
	{
		if (m_sVSFaction == "US") m_VSVehicles.Update(m_VSState, m_VSFaction, null, null, m_iStrategicBaseRevision, true);
		else m_VSVehicles.Update(null, null, m_VSState, m_VSFaction, m_iStrategicBaseRevision, true);
	}

	protected bool AICF_VSTransitCrew(AICF_GroupSlot slot, AICF_TransportTrip trip)
	{
		array<BaseCompartmentSlot> seats = {};
		AICF_VehiclePreset.Collect(m_VSVehicle, seats);
		if (!m_VSRecipe.MatchesSeats(seats)) return false;
		for (int index; index < m_VSRecipe.m_aMembers.Count(); index++)
		{
			int memberIndex = m_VSRecipe.m_aMembers[index];
			if (memberIndex < 0) continue;
			BaseCompartmentSlot seat;
			foreach (BaseCompartmentSlot physical : seats)
				if (AICF_VehiclePreset.SeatKey(physical) == m_VSRecipe.m_aSeats[index]) seat = physical;
			IEntity actor = slot.GetRosterMember(memberIndex);
			if (!seat || !AICF_GroupRuntime.IsAliveCharacter(actor) || actor.GetID() != m_aVSMembers[memberIndex]) return false;
			ChimeraCharacter character = ChimeraCharacter.Cast(actor);
			CompartmentAccessComponent access = character.GetCompartmentAccessComponent();
			bool settled = access && character.IsInVehicle() && access.GetCompartment() == seat && seat.GetOccupant() == actor && !access.IsGettingIn() && !access.IsGettingOut();
			AICF_VSNativeGate gate;
			foreach (AICF_VSNativeGate existing : m_aVSGates)
				if (existing.m_Member == actor) gate = existing;
			if (gate)
			{
				if (gate.m_sOperation != trip.GetOperationId() || gate.m_Id != actor.GetID() || gate.m_Seat != seat || gate.m_Utility.m_OwnerEntity != actor || System.GetTickCount() - gate.m_iStarted > 60000) return false;
				if (settled)
				{
					Print(string.Format("[AICF][VEHICLE_SCENARIO_GATE] scenario=%1 leg=%2 event=RETURNED member=%3 seat=%4 elapsed_ms=%5", m_sVSCase, m_iVSLeg, AICF_GroupTrace.Entity(actor), AICF_VehiclePreset.SeatKey(seat), System.GetTickCount() - gate.m_iStarted));
					m_aVSGates.RemoveItem(gate);
				}
				// Action state не доказывает физическую посадку/её отсутствие.
				// Native может повторить вход. Exact readback обязателен до60s.
			}
			else if (!settled)
			{
				gate = AICF_VSNativeGate.Begin(trip, actor, seat);
				if (!gate) return false;
				m_aVSGates.Insert(gate);
				Print(string.Format("[AICF][VEHICLE_SCENARIO_GATE] scenario=%1 leg=%2 event=STARTED member=%3 seat=%4 elapsed_ms=0", m_sVSCase, m_iVSLeg, AICF_GroupTrace.Entity(actor), AICF_VehiclePreset.SeatKey(seat)));
			}
		}
		return true;
	}

	protected void AICF_VSCheck(string rule, bool passed)
	{
		m_iVSChecks++;
		if (!passed) m_iVSFailures++;
		Print(string.Format("[AICF][VEHICLE_SCENARIO_CHECK] scenario=%1 leg=%2 rule=%3 passed=%4", m_sVSCase, m_iVSLeg, rule, passed));
	}

	protected bool AICF_VSExact(AICF_GroupSlot slot)
	{
		if (!m_VSVehicle || m_VSVehicle.GetID() != m_VSVehicleId) return false;
		array<BaseCompartmentSlot> seats = {};
		AICF_VehiclePreset.Collect(m_VSVehicle, seats);
		if (!m_VSRecipe.MatchesSeats(seats)) return false;
		int exact;
		for (int seat; seat < seats.Count(); seat++)
		{
			int position = m_VSRecipe.m_aMembers[seat];
			if (position < 0) continue;
			IEntity member = slot.GetRosterMember(position);
			ChimeraCharacter character = ChimeraCharacter.Cast(member);
			if (!character || member.GetID() != m_aVSMembers[position] || seats[seat].GetOccupant() != member ||
				CompartmentAccessComponent.GetVehicleIn(member) != m_VSVehicle || character.GetCompartmentAccessComponent().IsGettingOut()) return false;
			exact++;
		}
		return exact == m_iVSSize;
	}

	protected bool AICF_VSTarget(out vector target, bool alternate = false)
	{
		AIPathfindingComponent path = AIPathfindingComponent.Cast(m_VSGroup.FindComponent(AIPathfindingComponent));
		if (!path || !path.GetNavmeshComponent()) return false;
		SCR_AIWorld world = SCR_AIWorld.Cast(GetGame().GetAIWorld());
		if (!world || !world.GetRoadNetworkManager()) return false;
		if (!m_iVSTiles) m_iVSTiles = System.GetTickCount();
		vector origin = m_vVSHome;
		if (m_VSVehicle) origin = m_VSVehicle.GetOrigin();
		array<BaseRoad> roads = {};
		world.GetRoadNetworkManager().GetRoadsInAABB(origin - "1000 1000 1000", origin + "1000 1000 1000", roads);
		float best = 900;
		bool found;
		foreach (BaseRoad road : roads)
		{
			if (!road) continue;
			array<vector> points = {};
			road.GetPoints(points);
			foreach (vector query : points)
			{
				float distance = vector.DistanceXZ(origin, query);
				if (distance < 350 || distance >= best) continue;
				if (alternate && vector.DistanceXZ(m_vVSGoal, query) < 250) continue;
				bool nearBase;
				for (int index; index < m_ObjectiveGraph.GetNodeCount(); index++)
				{
					SCR_CampaignMilitaryBaseComponent base = m_ObjectiveGraph.GetNode(index).GetBase();
					if (!base || !base.GetOwner() || base == m_VSFaction.GetMainBase()) continue;
					if (vector.DistanceXZ(query, base.GetOwner().GetOrigin()) < 350) { nearBase = true; break; }
				}
				if (nearBase) continue;
				path.GetNavmeshComponent().LoadTileIn(query);
				vector nav;
				if (!path.GetClosestPositionOnNavmesh(query, "20 10 20", nav)) continue;
				if (ChimeraWorldUtils.TryGetWaterSurfaceSimple(GetGame().GetWorld(), nav - "0 0.5 0")) continue;
				target = nav;
				best = distance;
				found = true;
			}
		}
		return found;
	}
	protected bool AICF_VSOrder(AICF_GroupSlot slot, vector target)
	{
		bool accepted = m_OrderPlanner.AssignPlayerPointOrder(slot, m_VSFaction, target, m_VSVehicles.IsInfantryOrderSuspended(slot));
		if (!accepted) return false;
		m_vVSGoal = target;
		m_iVSGoalRevision = slot.GetStrategicIntentRevision();
		Print(string.Format("[AICF][VEHICLE_SCENARIO_GOAL] scenario=%1 leg=%2 target=%3 intent_revision=%4 vehicle_start=%5 dismount_radius_m=%6",
			m_sVSCase, m_iVSLeg, target, m_iVSGoalRevision, m_vVSStart, m_Stage3Config.GetDismountDistanceMeters()));
		return true;
	}

	// Opt-in causal fixtures may own a distinct verdict without weakening the
	// normal full-arrival scenario. The ordinary runner always returns false.
	protected bool AICF_VSAlternativeScenario(AICF_GroupSlot slot, AICF_TransportTrip trip)
	{
		return false;
	}

	// A fixture-owned FactionState needs its own owner servicing. Ordinary
	// scenarios keep the existing flow; opt-in probes may call the real owner.
	protected void AICF_VSServiceRecovery(AICF_GroupSlot slot)
	{
	}

	protected void AICF_VSOnRecoveryVerified(AICF_GroupSlot slot, AIWaypoint waypoint, int stableMs, int stablePolls, int requiredStableMs)
	{
	}

	protected bool AICF_VSApplyInitialCohesion(AICF_GroupSlot slot)
	{
		if (!Replication.IsServer() || !slot || !slot.IsCombatReady() || !m_VSGroup || slot.GetGroup() != m_VSGroup) return false;
		if (m_VSGroup.GetID() != m_VSGroupId || !m_GroupCohesionPolicy || m_VSGroup.GetFaction() != m_VSFaction) return false;
		if (!AICF_VehicleBoardingMutationFence.IsAuthoritativeReplicatedEntity(m_VSGroup)) return false;
		bool applied = m_GroupCohesionPolicy.Apply(m_VSGroup);
		AIFormationComponent formation = AIFormationComponent.Cast(m_VSGroup.FindComponent(AIFormationComponent));
		AIGroupMovementComponent movement = AIGroupMovementComponent.Cast(m_VSGroup.FindComponent(AIGroupMovementComponent));
		AIFormationDefinition selected;
		AIFormationDefinition defaultFormation;
		if (formation) selected = formation.GetFormation();
		if (movement) defaultFormation = movement.GetFormationDefinition(AIGroupMovementComponent.DEFAULT_HANDLER_ID);
		bool readback = applied && selected && selected.GetName() == AICF_GroupCohesionPolicy.FORMATION_NAME &&
			defaultFormation && defaultFormation.GetName() == AICF_GroupCohesionPolicy.FORMATION_NAME && movement.GetFormationDisplacement() == 0;
		bool current = slot.GetGroup() == m_VSGroup && m_VSGroup.GetID() == m_VSGroupId && m_VSGroup.GetFaction() == m_VSFaction;
		Print(string.Format("[AICF][VEHICLE_SCENARIO_COHESION] scenario=%1 group=%2 formation=Column displacement=0 native_readback=%3 identity_current=%4 setup_parity_only=1 causal_foot_fix_proven=0",
			m_sVSCase, AICF_GroupTrace.Entity(m_VSGroup), readback, current));
		return readback && current;
	}

	protected bool AICF_VSOwnRecoveryRoster(AICF_GroupSlot slot)
	{
		if (!Replication.IsServer() || !m_VSState || !slot || slot != m_VSState.GetSlot(0) || !slot.IsCombatReady()) return false;
		if (!m_VSGroup || m_VSGroup.GetID() != m_VSGroupId || slot.GetGroup() != m_VSGroup || m_VSGroup.GetFaction() != m_VSFaction) return false;
		if (!AICF_VehicleBoardingMutationFence.IsAuthoritativeReplicatedEntity(m_VSGroup) || m_aVSMembers.Count() != m_iVSSize) return false;
		for (int position; position < m_iVSSize; position++)
		{
			IEntity member = slot.GetRosterMember(position);
			if (!AICF_GroupRuntime.IsAliveCharacter(member) || member.GetID() != m_aVSMembers[position] || !AICF_VehicleBoardingMutationFence.IsAuthoritativeAIEntity(member)) return false;
			ChimeraCharacter character = ChimeraCharacter.Cast(member);
			if (!character || !character.GetAIControlComponent()) return false;
			AIAgent agent = character.GetAIControlComponent().GetControlAIAgent();
			if (!agent || agent.GetControlledEntity() != member || agent.GetParentGroup() != m_VSGroup) return false;
		}
		return true;
	}

	// The isolated scenario owns a separate FactionState/coordinator. Delegate
	// its exact current roster to the same production failure owner boundary.
	override protected bool ResolveVehicleSpawnApproachOwner(SCR_AIGroup group, out AICF_GroupSlot slot,
		out AICF_VehicleCoordinator coordinator, out SCR_CampaignFaction faction)
	{
		if (super.ResolveVehicleSpawnApproachOwner(group, slot, coordinator, faction)) return true;
		if (!Replication.IsServer() || !m_bStarted || !m_bRosterReady || m_bVSDone ||
			!m_Campaign || !m_Campaign.IsMaster() || !m_Campaign.IsRunning() ||
			!m_VSState || !m_VSVehicles || group != m_VSGroup) return false;
		AICF_GroupSlot ownSlot = m_VSState.GetSlot(0);
		if (!AICF_VSOwnRecoveryRoster(ownSlot)) return false;
		slot = ownSlot;
		coordinator = m_VSVehicles;
		faction = m_VSFaction;
		return true;
	}

	protected bool AICF_VSCanServiceRecovery(AICF_GroupSlot slot)
	{
		if (!AICF_VSOwnRecoveryRoster(slot) || !m_VSVehicles) return false;
		if (!slot.HasPendingOrderRecovery() || !slot.IsPendingOrderRecoveryContextCurrent()) return false;
		if (slot.GetPendingOrderRecoveryCause() != "VEHICLE_HANDOFF" || slot.PendingOrderRecoveryCountsAsReliabilityAttempt() || slot.PendingOrderRecoveryCountsAsStuckRecovery()) return false;
		if (m_VSVehicles.IsControllingMovement(slot) || m_VSVehicles.IsRestorePending(slot)) return false;
		return AICF_GroupRuntime.CountAliveAgentsInAnyVehicle(m_VSGroup) == 0;
	}

	protected void AICF_VSServiceOwnRecovery(AICF_GroupSlot slot)
	{
		if (!AICF_VSCanServiceRecovery(slot)) return;
		AIWaypoint expected = slot.GetPendingOrderRecoveryWaypoint();
		int stableMs = slot.GetPendingOrderRecoveryStableElapsedMs();
		int stablePolls = slot.GetPendingOrderRecoveryStablePolls();
		int requiredStableMs = Math.Max(ORDER_RECOVERY_MIN_STABLE_MS, m_Stage2Config.GetReliabilityIntervalMs() * ORDER_RECOVERY_STABLE_INTERVALS);
		// Custom state is outside the ordinary faction loop. Delegate once to its
		// real owner; never clear pending or manufacture durability observations.
		ProcessPendingOrderRecovery(m_VSState, slot, m_VSFaction);
		if (!AICF_VSOwnRecoveryRoster(slot) || slot.HasPendingOrderRecovery() || slot.GetWaypoint() != expected || m_VSGroup.GetCurrentWaypoint() != expected) return;
		if (stableMs < requiredStableMs || stablePolls < ORDER_RECOVERY_STABLE_POLLS || !m_OrderPlanner.GetOrderFailureReason(slot, m_VSFaction).IsEmpty()) return;
		Print(string.Format("[AICF][VEHICLE_SCENARIO_RECOVERY_VERIFIED] scenario=%1 leg=%2 group=%3 waypoint_id=%4 stable_ms=%5 stable_polls=%6 required_stable_ms=%7 owner=ProcessPendingOrderRecovery direct_clear=0",
			m_sVSCase, m_iVSLeg, AICF_GroupTrace.Entity(m_VSGroup), expected.GetID(), stableMs, stablePolls, requiredStableMs));
		AICF_VSOnRecoveryVerified(slot, expected, stableMs, stablePolls, requiredStableMs);
	}

	void AICF_VSStopObserver(string reason)
	{
		if (m_VSObservers && m_bVSObserverRequested)
		{
			m_VSObservers.RemoveObserverSP(AICF_VS_OBSERVER_KEY);
			Print(string.Format("[AICF][VEHICLE_SCENARIO_OBSERVER] scenario=%1 event=REMOVED key=%2 removal_request=1 key_readback_available=0 reason=%3", m_sVSCase, AICF_VS_OBSERVER_KEY, reason));
		}
		m_bVSObserverRequested = false;
		m_VSObservedMember = null;
		m_VSObservers = null;
	}

	protected void AICF_VSUpdateObserver()
	{
		AICF_GroupSlot slot;
		if (m_VSState) slot = m_VSState.GetSlot(0);
		IEntity member;
		if (!m_bStopped && !m_bVSDone && m_bRosterReady && m_VSObserverPolicy)
			member = m_VSObserverPolicy.ResolveCurrent(m_Campaign, m_VSState, slot, m_VSGroup, m_VSGroupId, m_VSFaction, m_sVSFaction);
		if (!member) { AICF_VSStopObserver("OWNER_NOT_CURRENT"); return; }
		if (!m_VSObservers) m_VSObservers = ObserversSystem.Cast(GetGame().GetWorld().FindSystem(ObserversSystem));
		if (!m_VSObservers || m_VSObserverPolicy.ResolveCurrent(m_Campaign, m_VSState, slot, m_VSGroup, m_VSGroupId, m_VSFaction, m_sVSFaction) != member)
		{
			AICF_VSStopObserver("NATIVE_SYSTEM_OR_IDENTITY");
			return;
		}
		// Как root ManagedObservers: каждый Update, локальное смещение 0/0.
		m_VSObservers.InsertObserverSP(AICF_VS_OBSERVER_KEY, 0, 0, member);
		if (!m_bVSObserverRequested || m_VSObservedMember != member)
			Print(string.Format("[AICF][VEHICLE_SCENARIO_OBSERVER] scenario=%1 event=REQUESTED key=%2 member=%3 registered_request=1 key_readback_available=0 setup_parity_only=1 causal_foot_fix_proven=0", m_sVSCase, AICF_VS_OBSERVER_KEY, AICF_GroupTrace.Entity(member)));
		m_bVSObserverRequested = true;
		m_VSObservedMember = member;
	}

	override protected void Stop(bool cleanupEntities)
	{
		AICF_VSStopObserver("STOP");
		super.Stop(cleanupEntities);
	}

	override protected void Update()
	{
		super.Update();
		AICF_VSUpdateObserver();
		if (!Replication.IsServer() || m_bVSDone || !m_bRosterReady || !System.GetCLIParam("aicfVehicleScenario", m_sVSCase)) return;
		int now = System.GetTickCount();
		if (!m_iVSStarted) m_iVSStarted = now;
		if (now - m_iVSStarted > 1200000) { AICF_VSCheck("TIMEOUT", false); AICF_VSFinish(); return; }
		if (!m_VSFaction && !AICF_VSSelectFaction()) { AICF_VSCheck("FACTION_SELECTION", false); AICF_VSFinish(); return; }
		if (!m_VSState)
		{
			string worldCombat;
			if (!System.GetCLIParam("aicfVehicleScenarioCombat", worldCombat) || worldCombat != "1")
			{
				SCR_FactionManager manager = SCR_FactionManager.Cast(GetGame().GetFactionManager());
				array<Faction> factions = {};
				manager.GetFactionsList(factions);
				m_bVSTruce = true;
				for (int first; first < factions.Count(); first++)
					for (int second = first + 1; second < factions.Count(); second++)
					{
						SCR_Faction a = SCR_Faction.Cast(factions[first]);
						SCR_Faction b = SCR_Faction.Cast(factions[second]);
						if (!a || !b) continue;
						manager.SetFactionsFriendly(a, b);
						if (!a.IsFactionFriendly(b) || !b.IsFactionFriendly(a)) m_bVSTruce = false;
					}
				AICF_VSCheck("TEST_WORLD_TRUCE", m_bVSTruce);
				if (!m_bVSTruce) { AICF_VSFinish(); return; }
			}
			if (m_sVSCase == "armed3") m_iVSSize = 3;
			if (m_sVSCase == "solo1") m_iVSSize = 1;
			m_VSState = new AICF_FactionState(m_VSFaction.GetFactionKey(), m_Config);
			AICF_GroupSlot fresh = m_VSState.GetSlot(0);
			fresh.SetDesiredSize(m_iVSSize);
			fresh.SetUnitType(AICF_EGroupUnitType.MOTORIZED_LIGHT);
			m_VSGroup = m_GroupSpawner.SpawnGroup(m_VSFaction, m_VSFaction.GetMainBase(), 0, m_iVSSize, false);
			AICF_VSCheck("GROUP_CREATED", m_VSGroup != null);
			if (!m_VSGroup) { AICF_VSFinish(); return; }
			m_VSGroupId = m_VSGroup.GetID();
			m_VSGroup.SetLifecyclePolicy(SCR_EAIGroupLifecyclePolicy.Manual);
			m_VSGroup.SetDeleteWhenEmpty(false);
			fresh.BeginInitialSpawn();
			fresh.BindSpawnedGroup(m_VSGroup);
			m_GroupSpawner.BeginRosterSpawn(m_VSGroup, m_iVSSize);
			m_VSVehicles = new AICF_VehicleCoordinator(m_Stage3Config, m_Campaign, m_ConflictAdapter, m_OrderPlanner,
				m_GroupCohesionPolicy, m_ObjectiveGraph, m_TargetSelector, m_ContentProfile);
			m_vVSHome = m_VSFaction.GetMainBase().GetSpawnPoint().GetOrigin();
			return;
		}
		AICF_GroupSlot slot = m_VSState.GetSlot(0);
		if (!slot.IsCombatReady())
		{
			if (m_VSGroup.GetAgentsCount() != m_iVSSize) return;
			string combat;
			bool combatCase = System.GetCLIParam("aicfVehicleScenarioCombat", combat) && combat == "1";
			SCR_AIGroupUtilityComponent groupUtility = m_VSGroup.GetGroupUtilityComponent();
			if (!groupUtility) { AICF_VSCheck("GROUP_UTILITY", false); AICF_VSFinish(); return; }
			if (!combatCase) groupUtility.SetCombatMode(EAIGroupCombatMode.HOLD_FIRE);
			slot.MarkReady();
			slot.RecordDeploymentMembers();
			bool initialCohesion = AICF_VSApplyInitialCohesion(slot);
			AICF_VSCheck("INITIAL_COHESION_POLICY", initialCohesion);
			if (!initialCohesion) { AICF_VSFinish(); return; }
			if (!combatCase)
				for (int protectedMember; protectedMember < m_iVSSize; protectedMember++)
				{
					ChimeraCharacter actor = ChimeraCharacter.Cast(slot.GetRosterMember(protectedMember));
					if (!actor || !actor.GetDamageManager()) { AICF_VSCheck("DAMAGE_MANAGER", false); AICF_VSFinish(); return; }
					actor.GetDamageManager().EnableDamageHandling(false);
					if (actor.GetDamageManager().IsDamageHandlingEnabled()) { AICF_VSCheck("DAMAGE_PROTECTION", false); AICF_VSFinish(); return; }
				}
			Print(string.Format("[AICF][VEHICLE_SCENARIO_ENV] scenario=%1 combat=%2 combat_mode=%3 damage_protection=%4 truce=%5",
				m_sVSCase, combatCase, typename.EnumToString(EAIGroupCombatMode, groupUtility.GetCombatModeExternal()), !combatCase, m_bVSTruce));
			ResourceName prefab = "{259EE7B78C51B624}Prefabs/Vehicles/Wheeled/UAZ469/UAZ469.et";
			if (m_sVSCase == "ural5") prefab = "{16C1F16C9B053801}Prefabs/Vehicles/Wheeled/Ural4320/Ural4320_transport.et";
			if (m_sVSCase == "solo1") prefab = "{16A674FE31B0921C}Prefabs/Vehicles/Wheeled/UAZ469/UAZ469_uncovered.et";
			AICF_VehiclePresetCatalog catalog = new AICF_VehiclePresetCatalog(m_VSFaction);
			if (m_sVSFaction == "US" && !AICF_VSSelectUSModel(catalog, prefab)) { AICF_VSCheck("CATALOG_US_EXACT5", false); catalog.Clear(); AICF_VSFinish(); return; }
			if (m_sVSCase == "armed3")
			{
				prefab = "";
				SCR_JsonLoadContext list = new SCR_JsonLoadContext();
				list.LoadFromString(catalog.List());
				array<ResourceName> prefabs = {};
				list.ReadValue("prefabs", prefabs);
				foreach (ResourceName armedPrefab : prefabs)
					if (armedPrefab.EndsWith("/UAZ469_PKM.et")) { prefab = armedPrefab; break; }
			}
			string reason;
			if (!catalog.Inspect(prefab, reason)) { AICF_VSCheck("CATALOG_INSPECTION", false); catalog.Clear(); AICF_VSFinish(); return; }
			m_VSRecipe = catalog.Draft(prefab);
			Print(string.Format("[AICF][VEHICLE_SCENARIO_SELECTION] scenario=%1 faction=%2 actual_faction=%3 prefab=%4 native_seats=%5 native_cost=%6", m_sVSCase, m_sVSFaction, m_VSFaction.GetFactionKey(), prefab, catalog.Seats().Count(), catalog.Cost(prefab)));
			int member;
			for (int mandatory; mandatory < catalog.Seats().Count(); mandatory++)
				if (catalog.Seats()[mandatory].GetType() == ECompartmentType.PILOT || catalog.Seats()[mandatory].GetType() == ECompartmentType.TURRET)
					m_VSRecipe.Assign(mandatory, member++);
			for (int seat; seat < catalog.Seats().Count() && member < m_iVSSize; seat++)
				if (m_VSRecipe.m_aMembers[seat] < 0) m_VSRecipe.Assign(seat, member++);
			bool valid = m_VSRecipe.ValidateRoster(slot, catalog.Seats(), reason) && slot.SetVehiclePreset(m_VSRecipe, slot.GetVehiclePresetRevision());
			catalog.Clear();
			AICF_VSCheck("EXACT_RECIPE", valid);
			if (!valid) { AICF_VSFinish(); return; }
			for (int actor; actor < m_iVSSize; actor++) m_aVSMembers.Insert(slot.GetRosterMember(actor).GetID());
			AICF_VSCheck("INITIAL_ORDER", m_OrderPlanner.AssignPlayerPointOrder(slot, m_VSFaction, m_vVSHome, false));
		}
		AICF_VSServiceOwnRecovery(slot);
		AICF_VSServiceRecovery(slot);
		AICF_VSUpdateVehicles();
		AICF_TransportTrip trip = m_VSVehicles.GetTrip(slot);
		// Реестр вправе удалить завершённый trip до пешего прибытия к точке.
		if (!trip && m_iVSStep == 4) trip = m_VSCompletedTrip;
		if (m_iVSRepeatRequested && (!trip || trip.GetOperationId() == m_sVSPreviousTrip) && now - m_iVSRepeatRequested > 30000)
		{
			AICF_VSCheck("REPEAT_TRIP_NOT_ADMITTED", false); AICF_VSFinish(); return;
		}
		if (!trip || trip.GetOperationId() == m_sVSPreviousTrip) return;
		string damageCombat;
		if ((!System.GetCLIParam("aicfVehicleScenarioCombat", damageCombat) || damageCombat != "1") && trip.GetLease() && trip.GetLease().GetVehicle())
		{
			DamageManagerComponent damage = DamageManagerComponent.Cast(trip.GetLease().GetVehicle().FindComponent(DamageManagerComponent));
			if (damage && damage.IsDamageHandlingEnabled()) damage.EnableDamageHandling(false);
		}
		if (AICF_VSAlternativeScenario(slot, trip)) return;
		if (trip.IsTerminal() && trip.GetPhase() != AICF_ETransportTripPhase.COMPLETE)
		{
			AICF_GroupTrace.Snapshot(slot, m_VSFaction.GetFactionKey(), trip, 0, "SCENARIO_TERMINAL_FAILURE");
			AICF_VSCheck(trip.GetTerminalReason(), false); AICF_VSFinish(); return;
		}
		if (m_iVSStep == 0)
		{
			if (trip.GetPhase() != AICF_ETransportTripPhase.TRANSIT) return;
			m_VSVehicle = trip.GetLease().GetVehicle();
			m_VSVehicleId = m_VSVehicle.GetID();
			m_vVSStart = m_VSVehicle.GetOrigin();
			AICF_VSCheck("EXACT_BOARDING", AICF_VSExact(slot));
			Print(string.Format("[AICF][VEHICLE_SCENARIO_BOARDING] scenario=%1 leg=%2 operation_id=%3 vehicle=%4 group=%5 crew=%6",
				m_sVSCase, m_iVSLeg, trip.GetOperationId(), AICF_GroupTrace.Entity(m_VSVehicle), AICF_GroupTrace.Entity(m_VSGroup), m_iVSSize));
			if (!AICF_VSExact(slot)) { AICF_VSFinish(); return; }
			m_iVSParked = now;
			m_iVSStep = 1;
			if (m_iVSLeg > 1) m_iVSStep = 2;
		}
		if (m_iVSStep == 1)
		{
			if (!AICF_VSExact(slot) || !trip.IsPresetWaitingForDestination()) { AICF_VSCheck("WAITING_MOUNTED", false); AICF_VSFinish(); return; }
			if (now - m_iVSParked < 30000) return;
			vector target;
			if (!AICF_VSTarget(target))
			{
				if (m_iVSTiles && now - m_iVSTiles > 30000) { AICF_VSCheck("TARGET_PRECONDITION", false); AICF_VSFinish(); }
				return;
			}
			AICF_VSCheck("WAITING_MOUNTED", true);
			AICF_VSCheck("REMOTE_ORDER", AICF_VSOrder(slot, target));
			m_iVSStep = 2;
			return;
		}
		// Cleanup вправе удалить прежнюю машину, пока состав идёт к точке пешком.
		if (m_iVSStep <= 2 && !m_VSVehicle) { AICF_VSCheck("VEHICLE_DISAPPEARED_IN_TRANSIT", false); AICF_VSFinish(); return; }
		float distance = -1;
		if (m_VSVehicle) distance = vector.DistanceXZ(m_VSVehicle.GetOrigin(), m_vVSGoal);
		if (now >= m_iVSSample)
		{
			m_iVSSample = now + 5000;
			Print(string.Format("[AICF][VEHICLE_SCENARIO_SAMPLE] scenario=%1 leg=%2 step=%3 phase=%4 vehicle=%5 pos=%6 goal_distance_m=%7 moving_polls=%8",
				m_sVSCase, m_iVSLeg, m_iVSStep, trip.GetPhase(), AICF_GroupTrace.Entity(m_VSVehicle), AICF_GroupTrace.Position(m_VSVehicle), distance, m_iVSMoving));
		}
		if (m_iVSStep == 2)
		{
			if (slot.GetStrategicIntentRevision() != m_iVSGoalRevision) { AICF_VSCheck("UNEXPECTED_RETARGET", false); AICF_VSFinish(); return; }
			if (trip.GetPhase() == AICF_ETransportTripPhase.TRANSIT)
			{
				m_iVSRecoveries = Math.Max(m_iVSRecoveries, trip.GetMovementState().GetMobilityRecoveryAttempts());
				if (!AICF_VSTransitCrew(slot, trip)) { AICF_VSCheck("PREMATURE_CREW_EXIT", false); AICF_VSFinish(); return; }
				if (m_VSVehicle.GetPhysics() && m_VSVehicle.GetPhysics().GetVelocity().Length() > 1) m_iVSMoving++;
				if (m_sVSCase == "retarget5" && !m_bVSRetargeted && vector.DistanceXZ(m_vVSStart, m_VSVehicle.GetOrigin()) > 80)
				{
					vector alternative;
					if (!AICF_VSTarget(alternative, true)) { AICF_VSCheck("ALTERNATE_TARGET", false); AICF_VSFinish(); return; }
					AICF_VSCheck("FAR_RETARGET", AICF_VSOrder(slot, alternative));
					m_bVSRetargeted = true;
				}
				return;
			}
			AICF_VSCheck("GATE_RETURNS_COMPLETE", m_aVSGates.IsEmpty());
			AICF_VSCheck("ARRIVAL_AT_ORIGINAL_GOAL", distance <= m_Stage3Config.GetDismountDistanceMeters() + 10);
			AICF_VSCheck("REAL_TRAVEL", vector.DistanceXZ(m_vVSStart, m_VSVehicle.GetOrigin()) >= 100 && m_iVSMoving >= 5 && m_iVSRecoveries == 0);
			Print(string.Format("[AICF][VEHICLE_SCENARIO_ARRIVAL] scenario=%1 leg=%2 vehicle=%3 distance_m=%4 displacement_m=%5 moving_polls=%6 intent_revision=%7 goal=%8",
				m_sVSCase, m_iVSLeg, AICF_GroupTrace.Entity(m_VSVehicle), distance, vector.DistanceXZ(m_vVSStart, m_VSVehicle.GetOrigin()), m_iVSMoving, m_iVSGoalRevision, m_vVSGoal));
			m_iVSStep = 3;
		}
		if (m_iVSStep == 3)
		{
			if (trip.GetPhase() != AICF_ETransportTripPhase.COMPLETE) return;
			AICF_VSCheck("NORMAL_DISMOUNT", AICF_GroupRuntime.CountAliveAgents(m_VSGroup) == m_iVSSize && AICF_GroupRuntime.CountAliveAgentsInAnyVehicle(m_VSGroup) == 0);
			AICF_VSCheck("FOOT_ORDER_RESTORED", !m_VSVehicles.IsInfantryOrderSuspended(slot) && slot.GetWaypoint() && m_VSGroup.GetCurrentWaypoint() == slot.GetWaypoint());
			m_VSCompletedTrip = trip;
			m_iVSStep = 4;
		}
		if (m_iVSStep == 4)
		{
			float farthest;
			for (int soldier; soldier < m_iVSSize; soldier++)
			{
				IEntity memberEntity = slot.GetRosterMember(soldier);
				if (!AICF_GroupRuntime.IsAliveCharacter(memberEntity) || memberEntity.GetID() != m_aVSMembers[soldier]) { AICF_VSCheck("ROSTER_CHANGED", false); AICF_VSFinish(); return; }
				farthest = Math.Max(farthest, vector.DistanceXZ(memberEntity.GetOrigin(), m_vVSGoal));
			}
			if (farthest > 35) return;
			// Повтор того же назначения идемпотентен: для нового trip нужен
			// другой настоящий destination, а не повторный admission старой цели.
			if (m_sVSCase == "repeat5" && m_iVSLeg == 1 && !AICF_VSTarget(m_vVSRepeatTarget, true)) return;
			AICF_VSCheck("ALL_MEMBERS_AT_POINT", true);
			Print(string.Format("[AICF][VEHICLE_SCENARIO_FOOT_ARRIVAL] scenario=%1 leg=%2 farthest_m=%3 alive=%4 goal=%5", m_sVSCase, m_iVSLeg, farthest, AICF_GroupRuntime.CountAliveAgents(m_VSGroup), m_vVSGoal));
			if (m_sVSCase == "repeat5" && m_iVSLeg == 1)
			{
				m_iVSLeg = 2;
				m_sVSPreviousTrip = trip.GetOperationId();
				m_VSCompletedTrip = null;
				AICF_VSCheck("REPEAT_ORDER", AICF_VSOrder(slot, m_vVSRepeatTarget));
				AICF_VSCheck("EXPLICIT_READMISSION", m_VSVehicles.RequestExplicitVehicleAdmission(slot, m_VSFaction, "SCENARIO_REPEAT_TRIP"));
				m_iVSRepeatRequested = now;
				m_iVSMoving = 0;
				m_iVSRecoveries = 0;
				m_iVSStep = 0;
				return;
			}
			AICF_VSCheck("PURCHASE_LIMIT", m_VSVehicles.GetSpawnedCount(m_VSFaction.GetFactionKey()) <= m_iVSLeg);
			AICF_VSFinish();
		}
	}

	protected void AICF_VSFinish()
	{
		AICF_VSStopObserver("FINISH");
		m_bVSDone = true;
		m_VSCompletedTrip = null;
		if (m_VSVehicles)
		{
			if (m_sVSFaction == "US") m_VSVehicles.StopWithFactionContexts(true, m_VSState, m_VSFaction, null, null);
			else m_VSVehicles.StopWithFactionContexts(true, null, null, m_VSState, m_VSFaction);
		}
		if (m_VSGroup)
		{
			m_OrderPlanner.ClearOrder(m_VSState.GetSlot(0));
			m_VSGroup.DespawnMembers();
			RplComponent.DeleteRplEntity(m_VSGroup, false);
		}
		Print(string.Format("[AICF][VEHICLE_SCENARIO_DONE] scenario=%1 checks=%2 failures=%3 legs=%4", m_sVSCase, m_iVSChecks, m_iVSFailures, m_iVSLeg));
		GetGame().RequestClose();
	}
}

modded class ArmaReforgerScripted
{
	override void OnGameEnd()
	{
		AICF_MatchController controller = AICF_MatchController.GetActiveController();
		if (controller) controller.AICF_VSStopObserver("ON_GAME_END");
		super.OnGameEnd();
	}
}
