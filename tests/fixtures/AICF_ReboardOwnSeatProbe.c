// Controlled own-original wrong-seat fault. Native readiness, trip phases,
// purchase, lease, boarding, tactical dismount and route remain production.
class AICF_OwnSeatProbe
{
	static AICF_MatchController s_Owner;
	static AICF_GroupSlot s_Slot;
	static ref AICF_TransportTrip s_Trip;
	static SCR_AIGroup s_Group;
	static EntityID s_GroupId;
	static string s_GroupRpl;
	static Vehicle s_Car;
	static EntityID s_CarId;
	static string s_CarRpl;
	static AICF_VehicleLease s_Lease;
	static int s_LeaseGeneration;
	static int s_VehicleGeneration;
	static int s_TripGeneration;
	static int s_PresetRevision;
	static ref array<IEntity> s_Members = {};
	static ref array<EntityID> s_Ids = {};
	static ref array<string> s_IdTexts = {};
	static ref array<string> s_Rpls = {};
	static ref array<float> s_Health = {};
	static float s_CarHealth;
	static bool s_Threat;
	static bool s_FaultSubmitted;
	static bool s_FaultObserved;
	static bool s_Invalid;
	static int s_FaultCalls;
	static int s_Deadline;
	static int s_OriginalBudget;
	static int s_InitialShort;
	static int s_LastShort;
	static int s_Payments;
	static int s_Paid;
	static int s_Spawns;
	static bool s_ConflictObserved;
	static bool s_ExitObserved;
	static int s_MotionPolls;
	static int s_NextMotion;
	static vector s_ResumePosition;
	static bool s_Resumed;
	static bool s_InterruptSubmitted;
	static bool s_InterruptObserved;
	static bool s_SecondThreatHandled;
	static int s_FirstDeadline;
	static int s_SecondDeadline;
	static int s_FirstShort;
	static BaseCompartmentSlot s_PilotSeat;
	static BaseCompartmentSlot s_CargoSeat;
	static IEntity s_Pilot;
	static IEntity s_Cargo;
	static void ActorLedger(string phase)
	{
		array<BaseCompartmentSlot> seats = {};
		bool topology;
		if (s_Trip && s_Car) topology = AICF_VehiclePreset.CollectCaptured(s_Car, s_Trip.GetVehiclePreset(), seats);
		for (int index; index < s_Members.Count(); index++)
		{
			IEntity actor = s_Members[index];
			string expectedSeat = "UNKNOWN"; string actualSeat = "NONE";
			if (topology)
			{
				int seatIndex = s_Trip.GetVehiclePreset().m_aMembers.Find(index);
				if (seatIndex >= 0 && seatIndex < seats.Count()) expectedSeat = AICF_VehiclePreset.SeatKey(seats[seatIndex]);
			}
			ChimeraCharacter character = ChimeraCharacter.Cast(actor);
			if (character && character.GetCompartmentAccessComponent() && character.GetCompartmentAccessComponent().GetCompartment()) actualSeat = AICF_VehiclePreset.SeatKey(character.GetCompartmentAccessComponent().GetCompartment());
			string details = string.Format("phase=%1 position=%2 actor=%3 rpl=%4 health=%5 captured_health=%6 captured_id=%7 captured_rpl=%8", phase, index, AICF_GroupTrace.Entity(actor), AICF_VehicleBehaviorBinding.ComponentRpl(actor), Health(actor), s_Health[index], s_IdTexts[index], s_Rpls[index]);
			details += string.Format(" expected_seat=%1 actual_seat=%2", expectedSeat, actualSeat);
			Log("ACTOR", details);
		}
	}

	static string Mode()
	{
		string mode;
		if (!Replication.IsServer() || !System.GetCLIParam("aicfReboardOwnSeatProbe", mode)) return string.Empty;
		if (mode != "BEFORE" && mode != "AFTER") return string.Empty;
		return mode;
	}
	static bool InterruptCase()
	{
		string flag;
		return System.GetCLIParam("aicfReboardOwnSeatInterrupt", flag) && flag == "1";
	}
	static void Log(string eventName, string details)
	{
		Print(string.Format("[AICF][OWN_SEAT_PROBE] event=%1 mode=%2 scope=CONTROLLED_OWN_WRONG_SEAT %3", eventName, Mode(), details));
	}
	static float Health(IEntity entity)
	{
		DamageManagerComponent damage;
		if (entity) damage = DamageManagerComponent.Cast(entity.FindComponent(DamageManagerComponent));
		if (!damage || !damage.GetDefaultHitZone()) return -1;
		return damage.GetDefaultHitZone().GetHealthScaled();
	}
	static bool Current(AICF_TransportTrip trip)
	{
		if (!Replication.IsServer() || !s_Owner || AICF_MatchController.GetActiveController() != s_Owner || !s_Slot) return false;
		if (!trip || trip != s_Trip || trip.IsTerminal() || !s_Lease || trip.GetLease() != s_Lease ||
			trip.GetTripGeneration() != s_TripGeneration || trip.GetVehiclePresetRevision() != s_PresetRevision) return false;
		if (!s_Group || s_Slot.GetGroup() != s_Group || s_Group.GetID() != s_GroupId || AICF_VehicleBehaviorBinding.ComponentRpl(s_Group) != s_GroupRpl) return false;
		if (!s_Car || s_Lease.GetVehicle() != s_Car || s_Car.GetID() != s_CarId || AICF_VehicleBehaviorBinding.ComponentRpl(s_Car) != s_CarRpl) return false;
		if (s_Lease.GetLeaseGeneration() != s_LeaseGeneration || s_Lease.GetVehicleGeneration() != s_VehicleGeneration || Health(s_Car) != s_CarHealth) return false;
		if (AICF_GroupRuntime.CountAliveAgents(s_Group) != 5 || s_Members.Count() != 5 || !s_Slot.IsCombatReady()) return false;
		for (int index; index < 5; index++)
		{
			IEntity actor = s_Members[index];
			if (!actor || s_Slot.GetRosterMember(index) != actor || trip.VehiclePresetMember(index) != actor || actor.GetID() != s_Ids[index] ||
				AICF_VehicleBehaviorBinding.ComponentRpl(actor) != s_Rpls[index] || Health(actor) != s_Health[index] ||
				!AICF_VehicleShortRecovery.IsCurrentMember(trip, actor)) return false;
		}
		return true;
	}
	static bool Exact(AICF_TransportTrip trip)
	{
		if (!Current(trip)) return false;
		array<BaseCompartmentSlot> seats = {};
		if (!AICF_VehiclePreset.CollectCaptured(s_Car, trip.GetVehiclePreset(), seats)) return false;
		int settled;
		for (int index; index < seats.Count(); index++)
		{
			IEntity actor = trip.VehiclePresetMember(trip.GetVehiclePreset().m_aMembers[index]);
			if (!actor) continue;
			ChimeraCharacter character = ChimeraCharacter.Cast(actor);
			CompartmentAccessComponent access = character.GetCompartmentAccessComponent();
			if (seats[index].GetOccupant() != actor || !access || !access.IsInCompartment() || access.GetCompartment() != seats[index] ||
				!character.IsInVehicle() || CompartmentAccessComponent.GetVehicleIn(actor) != s_Car ||
				access.IsGettingIn() || access.IsGettingOut() || access.IsSwitchingSeatsAnim()) return false;
			settled++;
		}
		return settled == 5;
	}
	static void NativeThreat()
	{
		if (!s_Trip || !Current(s_Trip)) return;
		array<AIAgent> agents = {}; s_Group.GetAgents(agents);
		foreach (AIAgent agent : agents)
		{
			SCR_AIUtilityComponent utility;
			if (agent) utility = SCR_AIUtilityComponent.Cast(agent.FindComponent(SCR_AIUtilityComponent));
			if (!utility || utility.m_OwnerEntity != agent.GetControlledEntity() || !utility.m_ThreatSystem) { s_Invalid = true; return; }
			float threat;
			if (s_Threat) threat = 1;
			utility.m_ThreatSystem.SetThreatValues(threat, 0, 0, 0);
			utility.m_ThreatSystem.Update(utility, 0.001);
		}
	}
	static void CaptureConflict(AICF_TransportTrip trip)
	{
		if (!s_FaultSubmitted || !Current(trip) || !s_PilotSeat || s_PilotSeat.GetOccupant() != s_Cargo) return;
		CompartmentAccessComponent access = ChimeraCharacter.Cast(s_Cargo).GetCompartmentAccessComponent();
		if (!access || access.GetCompartment() != s_PilotSeat || CompartmentAccessComponent.GetVehicleIn(s_Cargo) != s_Car ||
			!access.IsInCompartment() || access.IsGettingIn() || access.IsGettingOut() || access.IsSwitchingSeatsAnim()) return;
		s_FaultObserved = true;
		s_ConflictObserved = true;
		EntityID pilotId = s_Pilot.GetID(); EntityID cargoId = s_Cargo.GetID();
		Log("ACTUAL_CONFLICT_BEFORE_SUPER", string.Format("operation=%1 expected=%2 occupant=%3 seat=%4 occupant_assigned=%5 deadline=%6 remaining=%7 actual_wrongseat=1 mapping_unchanged=1 health_unchanged=1",
			trip.GetOperationId(), AICF_GroupTrace.Entity(s_Pilot), AICF_GroupTrace.Entity(s_Cargo), AICF_VehiclePreset.SeatKey(s_PilotSeat), AICF_VehiclePreset.SeatKey(s_CargoSeat), s_Deadline, s_Deadline - System.GetTickCount()));
	}
}
modded class AICF_VehicleSpawner
{
	override protected Vehicle SpawnSelectedPrefab(ResourceName prefab, vector position, EntitySpawnParams spawnParams = null, AICF_AIQuotaVehicleCreationGuard quotaGuard = null)
	{
		Vehicle created = super.SpawnSelectedPrefab(prefab, position, spawnParams, quotaGuard);
		if (!AICF_OwnSeatProbe.Mode().IsEmpty() && created && created.GetWorld() == GetGame().GetWorld())
		{
			AICF_OwnSeatProbe.s_Spawns++;
			EntityID id = created.GetID();
			AICF_OwnSeatProbe.Log("NATIVE_SPAWN", string.Format("vehicle=%1 rpl=%2 count=%3 prefab=%4", AICF_GroupTrace.Entity(created), AICF_VehicleBehaviorBinding.ComponentRpl(created), AICF_OwnSeatProbe.s_Spawns, prefab));
		}
		return created;
	}
}
modded class AICF_TransportTrip
{
	override bool CommitBehaviorClockPause(bool paused, int nowMs)
	{
		int before = m_iAbsoluteDeadlineMs;
		int naturalPause;
		if (!paused) naturalPause = GetBehaviorState().PausedMs(nowMs);
		bool accepted = super.CommitBehaviorClockPause(paused, nowMs);
		if (!AICF_OwnSeatProbe.Mode().IsEmpty() && this == AICF_OwnSeatProbe.s_Trip)
		{
			bool exact = !accepted || m_iAbsoluteDeadlineMs == before + naturalPause;
			if (!exact) AICF_OwnSeatProbe.s_Invalid = true;
			AICF_OwnSeatProbe.Log("NATIVE_CLOCK_PAUSE", string.Format("operation=%1 paused=%2 accepted=%3 before=%4 after=%5 actual_natural_pause=%6 exact_native_clock=%7 clock_write=0", GetOperationId(), paused, accepted, before, m_iAbsoluteDeadlineMs, naturalPause, exact));
		}
		return accepted;
	}
}
modded class AICF_VehicleBehaviorFlow
{
	override protected AICF_TripOutcome PollReboarding(AICF_TransportTrip trip, string causationId, bool combat)
	{
		if (AICF_OwnSeatProbe.Mode().IsEmpty() || trip != AICF_OwnSeatProbe.s_Trip) return super.PollReboarding(trip, causationId, combat);
		if (!AICF_OwnSeatProbe.Current(trip)) { AICF_OwnSeatProbe.s_Invalid = true; return super.PollReboarding(trip, causationId, combat); }
		AICF_VehicleBehaviorState state = trip.GetBehaviorState();
		if (!AICF_OwnSeatProbe.s_Deadline) AICF_OwnSeatProbe.s_Deadline = state.m_iReboardDeadline;
		if (AICF_OwnSeatProbe.InterruptCase() && AICF_OwnSeatProbe.s_InterruptObserved && !AICF_OwnSeatProbe.s_SecondDeadline && state.m_iReboardDeadline != AICF_OwnSeatProbe.s_Deadline)
		{
			AICF_OwnSeatProbe.s_SecondDeadline = state.m_iReboardDeadline;
			AICF_OwnSeatProbe.s_Deadline = state.m_iReboardDeadline;
			AICF_OwnSeatProbe.Log("CORE_SECOND_REBOARD_WINDOW", string.Format("operation=%1 first_deadline=%2 second_deadline=%3 observed_at=%4 fixture_clock_write=0", trip.GetOperationId(), AICF_OwnSeatProbe.s_FirstDeadline, state.m_iReboardDeadline, System.GetTickCount()));
		}
		if (state.m_iReboardDeadline != AICF_OwnSeatProbe.s_Deadline || System.GetTickCount() >= AICF_OwnSeatProbe.s_Deadline)
		{
			AICF_OwnSeatProbe.s_Invalid = true;
			return super.PollReboarding(trip, causationId, combat);
		}
		if (!AICF_OwnSeatProbe.s_FaultSubmitted)
		{
			// FIRST Poll precedes own GetIn dispatch. Never clear another actor's reservation.
			BaseCompartmentSlot pilotSeat = AICF_OwnSeatProbe.s_PilotSeat;
			IEntity cargo = AICF_OwnSeatProbe.s_Cargo;
			if (!pilotSeat || pilotSeat.GetOccupant() || pilotSeat.IsReserved() || combat || !AICF_VehicleShortRecovery.CanSeat(trip, cargo, pilotSeat))
			{
				AICF_OwnSeatProbe.s_Invalid = true;
				AICF_OwnSeatProbe.Log("SETUP_REJECTED", "reason=FIRST_POLL_NOT_EMPTY_UNRESERVED_LOCAL_NATIVE_SEAT no_reservation_write=1");
				return super.PollReboarding(trip, causationId, combat);
			}
			AICF_OwnSeatProbe.s_FaultSubmitted = true;
			AICF_OwnSeatProbe.s_FaultCalls++;
			bool accepted = AICF_VehicleShortRecovery.TrySeat(trip, cargo, pilotSeat, "CONTROLLED_OWN_WRONG_SEAT_FAULT");
		AICF_OwnSeatProbe.Log("NATIVE_FAULT_SUBMITTED", string.Format("operation=%1 accepted=%2 calls=%3 actor_displacement_bound=25 original_deadline=%4 mapping_write=0 health_write=0 vehicle_pose_write=0 submitted_at=%5", trip.GetOperationId(), accepted, AICF_OwnSeatProbe.s_FaultCalls, AICF_OwnSeatProbe.s_Deadline, System.GetTickCount()));
			if (!accepted) AICF_OwnSeatProbe.s_Invalid = true;
		}
		AICF_OwnSeatProbe.CaptureConflict(trip);
		if (!AICF_OwnSeatProbe.s_FaultObserved && !AICF_OwnSeatProbe.s_Invalid)
			return AICF_TripOutcome.Wait("CONTROLLED_OWN_SEAT_FAULT_READBACK_PENDING", causationId);
		bool pendingBefore = state.m_ReboardSeatConflict != null;
		bool distantBefore = HasDistantOrder(trip);
		AICF_TripOutcome outcome = super.PollReboarding(trip, causationId, combat);
		if (!AICF_OwnSeatProbe.Current(trip)) return outcome;
		foreach (AICF_VehicleBehaviorAction token : state.m_aActions)
			if (token && token.m_Actor == AICF_OwnSeatProbe.s_Cargo) AICF_OwnSeatProbe.s_LastShort = token.m_iShortAttempts;
		if (AICF_OwnSeatProbe.s_FaultObserved && AICF_OwnSeatProbe.s_PilotSeat.GetOccupant() != AICF_OwnSeatProbe.s_Cargo)
			AICF_OwnSeatProbe.s_ExitObserved = true;
		if (AICF_OwnSeatProbe.InterruptCase() && !AICF_OwnSeatProbe.s_InterruptSubmitted && state.m_ReboardSeatConflict && AICF_OwnSeatProbe.s_LastShort > 0)
		{
			AICF_OwnSeatProbe.s_InterruptSubmitted = true;
			AICF_OwnSeatProbe.s_FirstDeadline = state.m_iReboardDeadline;
			AICF_OwnSeatProbe.s_FirstShort = AICF_OwnSeatProbe.s_LastShort;
			AICF_OwnSeatProbe.s_Threat = true;
			AICF_OwnSeatProbe.Log("INTERRUPT_NATIVE_THREAT_SUBMITTED", string.Format("operation=%1 first_deadline=%2 shared_short=%3 pending_receipt=1 pilot_unreserved=%4 actual_native_threat_next_tick=1", trip.GetOperationId(), state.m_iReboardDeadline, AICF_OwnSeatProbe.s_LastShort, !AICF_OwnSeatProbe.s_PilotSeat.IsReserved()));
		}
		if (AICF_OwnSeatProbe.InterruptCase() && AICF_OwnSeatProbe.s_InterruptSubmitted && !AICF_OwnSeatProbe.s_InterruptObserved && outcome && outcome.GetReason() == "COMBAT_REBOARD_CANCELLED")
		{
			AICF_OwnSeatProbe.s_InterruptObserved = true;
			AICF_OwnSeatProbe.Log("ACTUAL_LOCAL_THREAT_CANCELLED", string.Format("operation=%1 first_deadline=%2 pending_before=%3 pending_after=%4 combat_argument=%5 distant_before=%6 pilot_unreserved=%7 first_shared_short=%8 outcome=COMBAT_REBOARD_CANCELLED", trip.GetOperationId(), state.m_iReboardDeadline, pendingBefore, state.m_ReboardSeatConflict != null, combat, distantBefore, !AICF_OwnSeatProbe.s_PilotSeat.IsReserved(), AICF_OwnSeatProbe.s_LastShort));
		}
		return outcome;
	}
}
modded class AICF_EconomySystem
{
	override bool PurchaseVehiclePreset(AICF_TransportTrip trip, SCR_CampaignFaction faction, SCR_CampaignMilitaryBaseComponent base, ResourceName prefab, out int paid)
	{
		bool purchased = super.PurchaseVehiclePreset(trip, faction, base, prefab, paid);
		if (!AICF_OwnSeatProbe.Mode().IsEmpty() && trip && trip.GetVehiclePresetSlot() == AICF_OwnSeatProbe.s_Slot && purchased)
		{
			AICF_OwnSeatProbe.s_Payments++; AICF_OwnSeatProbe.s_Paid += paid;
			AICF_OwnSeatProbe.Log("NATIVE_PURCHASE", string.Format("operation=%1 purchased=%2 paid=%3 payments=%4", trip.GetOperationId(), purchased, paid, AICF_OwnSeatProbe.s_Payments));
		}
		return purchased;
	}
}
modded class AICF_VehicleCoordinator
{
	override void Update(AICF_FactionState usState, SCR_CampaignFaction usFaction, AICF_FactionState ussrState, SCR_CampaignFaction ussrFaction, int baseRevision, bool dispatchTrips)
	{
		if (!AICF_OwnSeatProbe.Mode().IsEmpty()) AICF_OwnSeatProbe.NativeThreat();
		super.Update(usState, usFaction, ussrState, ussrFaction, baseRevision, dispatchTrips);
	}
}
modded class AICF_MatchController
{
	protected int m_iOSStarted;
	protected bool m_bOSConfigured;
	protected bool m_bOSDone;
	protected bool m_bOSTruced;
	protected int m_iOSStep;
	protected bool AICF_OSFarPoint(vector origin, out vector target)
	{
		SCR_AIWorld ai = SCR_AIWorld.Cast(GetGame().GetAIWorld());
		AIPathfindingComponent path = AIPathfindingComponent.Cast(AICF_OwnSeatProbe.s_Group.FindComponent(AIPathfindingComponent));
		if (!ai || !ai.GetRoadNetworkManager() || !path || !path.GetNavmeshComponent()) return false;
		array<BaseRoad> roads = {};
		ai.GetRoadNetworkManager().GetRoadsInAABB(origin - "900 900 900", origin + "900 900 900", roads);
		float best = 850;
		bool found;
		foreach (BaseRoad road : roads)
		{
			if (!road) continue;
			array<vector> points = {}; road.GetPoints(points);
			foreach (vector point : points)
			{
				float distance = vector.DistanceXZ(origin, point);
				if (distance <= 350 || distance >= best) continue;
				path.GetNavmeshComponent().LoadTileIn(point);
				vector nav;
				if (!path.GetClosestPositionOnNavmesh(point, "20 10 20", nav) || ChimeraWorldUtils.TryGetWaterSurfaceSimple(GetGame().GetWorld(), nav - "0 0.5 0")) continue;
				target = nav; best = distance; found = true;
			}
		}
		return found;
	}
	protected void AICF_OSTruce()
	{
		if (m_bOSTruced) return;
		SCR_FactionManager manager = SCR_FactionManager.Cast(GetGame().GetFactionManager());
		if (!manager) return;
		array<Faction> factions = {}; manager.GetFactionsList(factions);
		if (factions.Count() < 2) return;
		for (int first; first < factions.Count(); first++)
			for (int second = first + 1; second < factions.Count(); second++)
			{
				SCR_Faction a = SCR_Faction.Cast(factions[first]); SCR_Faction b = SCR_Faction.Cast(factions[second]);
				if (!a || !b) return;
				manager.SetFactionsFriendly(a, b);
				if (!a.IsFactionFriendly(b) || !b.IsFactionFriendly(a)) return;
			}
		m_bOSTruced = true;
	}
	protected void AICF_OSFinish(bool passed, string reason)
	{
		if (m_bOSDone) return;
		m_bOSDone = true;
		int currentDeadline, currentAttempts;
		float displacement;
		if (AICF_OwnSeatProbe.s_Trip) { currentDeadline = AICF_OwnSeatProbe.s_Trip.GetBehaviorState().m_iReboardDeadline; currentAttempts = AICF_OwnSeatProbe.s_Trip.GetMovementState().GetMobilityRecoveryAttempts(); }
		if (AICF_OwnSeatProbe.s_Car && AICF_OwnSeatProbe.s_Resumed) displacement = vector.DistanceXZ(AICF_OwnSeatProbe.s_ResumePosition, AICF_OwnSeatProbe.s_Car.GetOrigin());
		string details = string.Format("passed=%1 reason=%2 fault_calls=%3 conflict=%4 exit_readback=%5 shared_short=%6 deadline=%7 current_deadline=%8 payments=%9", passed, reason, AICF_OwnSeatProbe.s_FaultCalls, AICF_OwnSeatProbe.s_ConflictObserved, AICF_OwnSeatProbe.s_ExitObserved, AICF_OwnSeatProbe.s_LastShort, AICF_OwnSeatProbe.s_Deadline, currentDeadline, AICF_OwnSeatProbe.s_Payments);
		details += string.Format(" paid=%1 moving_polls=%2 displacement_m=%3 current_identity=%4 exact5=%5 old_mobility_attempts=%6 current_mobility_attempts=%7", AICF_OwnSeatProbe.s_Paid, AICF_OwnSeatProbe.s_MotionPolls, displacement, AICF_OwnSeatProbe.Current(AICF_OwnSeatProbe.s_Trip), AICF_OwnSeatProbe.Exact(AICF_OwnSeatProbe.s_Trip), AICF_OwnSeatProbe.s_OriginalBudget, currentAttempts);
		details += string.Format(" physical_spawns=%1", AICF_OwnSeatProbe.s_Spawns);
		details += string.Format(" interrupt_case=%1 actual_interrupt=%2 first_deadline=%3 second_deadline=%4", AICF_OwnSeatProbe.InterruptCase(), AICF_OwnSeatProbe.s_InterruptObserved, AICF_OwnSeatProbe.s_FirstDeadline, AICF_OwnSeatProbe.s_SecondDeadline);
		details += " first_shared_short=" + AICF_OwnSeatProbe.s_FirstShort.ToString();
		if (AICF_OwnSeatProbe.s_Trip) details += string.Format(" operation=%1 group_gen=%2 trip_gen=%3", AICF_OwnSeatProbe.s_Trip.GetOperationId(), AICF_OwnSeatProbe.s_Trip.GetGroupGeneration(), AICF_OwnSeatProbe.s_Trip.GetTripGeneration());
		if (AICF_OwnSeatProbe.s_Car) details += string.Format(" car=%1 car_rpl=%2 car_health=%3 captured_car_health=%4", AICF_GroupTrace.Entity(AICF_OwnSeatProbe.s_Car), AICF_OwnSeatProbe.s_CarRpl, AICF_OwnSeatProbe.Health(AICF_OwnSeatProbe.s_Car), AICF_OwnSeatProbe.s_CarHealth);
		AICF_OwnSeatProbe.ActorLedger("FINAL");
		AICF_OwnSeatProbe.Log("DONE", details);
		GetGame().RequestClose();
	}
	override protected bool SpawnInitialRoster(AICF_FactionState factionState, SCR_CampaignFaction faction)
	{
		if (!AICF_OwnSeatProbe.Mode().IsEmpty())
		{
			if (!AICF_OwnSeatProbe.s_Owner) AICF_OwnSeatProbe.s_Owner = this;
			if (!m_iOSStarted) m_iOSStarted = System.GetTickCount();
			AICF_OSTruce();
			if (factionState == m_USSRState) { factionState.GetSlot(0).SetUnitType(AICF_EGroupUnitType.MOTORIZED_LIGHT); factionState.GetSlot(0).SetDesiredSize(5); }
		}
		return super.SpawnInitialRoster(factionState, faction);
	}
	override protected void ProcessFaction(AICF_FactionState factionState, SCR_CampaignFaction faction)
	{
		super.ProcessFaction(factionState, faction);
		if (m_bStopped || AICF_MatchController.GetActiveController() != this || AICF_OwnSeatProbe.s_Owner != this) return;
		if (AICF_OwnSeatProbe.Mode().IsEmpty() || !m_bRosterReady || m_bOSConfigured || factionState != m_USSRState) return;
		AICF_GroupSlot slot = factionState.GetSlot(0);
		int actual, wrong, dead;
		if (!slot.IsCombatReady() || !AICF_GroupRuntime.HasExactFactionRoster(slot.GetGroup(), "USSR", 5, actual, wrong, dead)) return;
		AICF_VehiclePresetCatalog catalog = new AICF_VehiclePresetCatalog(faction);
		ResourceName prefab = "{9B3A89DD33FF0483}Prefabs/Vehicles/Wheeled/UAZ469/UAZ469_uncovered_CIV_blue.et";
		string reason;
		if (!catalog.Inspect(prefab, reason)) { catalog.Clear(); return; }
		AICF_VehiclePreset recipe = catalog.Draft(prefab);
		recipe.m_iBehavior = AICF_EVehicleBehavior.TRANSPORT; recipe.m_iReturnDistance = 300;
		int position;
		for (int index; index < catalog.Seats().Count(); index++) if (catalog.Seats()[index].GetType() == ECompartmentType.PILOT) recipe.Assign(index, position++);
		for (int index; index < catalog.Seats().Count() && position < 5; index++) if (recipe.m_aMembers[index] < 0) recipe.Assign(index, position++);
		bool valid = recipe.ValidateRoster(slot, catalog.Seats(), reason) && PreflightVehiclePreset(slot, faction, recipe, reason);
		catalog.Clear();
		if (!valid || !faction.GetMainBase() || !faction.GetMainBase().GetSpawnPoint()) return;
		if (!m_OrderPlanner.AssignPlayerPointOrder(slot, faction, faction.GetMainBase().GetSpawnPoint().GetOrigin(), false)) return;
		if (!slot.SetVehiclePreset(recipe, slot.GetVehiclePresetRevision())) return;
		AICF_OwnSeatProbe.s_Slot = slot; AICF_OwnSeatProbe.s_Group = slot.GetGroup(); AICF_OwnSeatProbe.s_GroupId = slot.GetGroup().GetID(); AICF_OwnSeatProbe.s_GroupRpl = AICF_VehicleBehaviorBinding.ComponentRpl(slot.GetGroup());
		m_bOSConfigured = true;
		AICF_OwnSeatProbe.Log("READY", string.Format("group=%1 group_rpl=%2 actual=%3 aggregate_ready=%4 fake_ready=0 preset_revision=%5", AICF_OwnSeatProbe.s_GroupId, AICF_OwnSeatProbe.s_GroupRpl, actual, m_bRosterReady, slot.GetVehiclePresetRevision()));
	}
	override protected void Update()
	{
		super.Update();
		if (AICF_OwnSeatProbe.Mode().IsEmpty() || m_bOSDone || m_bStopped || AICF_MatchController.GetActiveController() != this || !m_iOSStarted) return;
		if (System.GetTickCount() - m_iOSStarted > 420000) { AICF_OSFinish(false, "ORIGINAL_420S_SETUP_BOUND"); return; }
		if (!m_bOSConfigured) return;
		AICF_TransportTrip trip = m_VehicleCoordinator.GetTrip(AICF_OwnSeatProbe.s_Slot);
		if (!trip) return;
		if (!AICF_OwnSeatProbe.s_Trip)
		{
			if (trip.GetPhase() != AICF_ETransportTripPhase.TRANSIT || !trip.GetLease() || !trip.GetLease().GetVehicle()) return;
			AICF_OwnSeatProbe.s_Trip = trip; AICF_OwnSeatProbe.s_Lease = trip.GetLease(); AICF_OwnSeatProbe.s_Car = trip.GetLease().GetVehicle();
			AICF_OwnSeatProbe.s_CarId = AICF_OwnSeatProbe.s_Car.GetID(); AICF_OwnSeatProbe.s_CarRpl = AICF_VehicleBehaviorBinding.ComponentRpl(AICF_OwnSeatProbe.s_Car);
			AICF_OwnSeatProbe.s_LeaseGeneration = trip.GetLease().GetLeaseGeneration(); AICF_OwnSeatProbe.s_VehicleGeneration = trip.GetLease().GetVehicleGeneration();
			AICF_OwnSeatProbe.s_TripGeneration = trip.GetTripGeneration(); AICF_OwnSeatProbe.s_PresetRevision = trip.GetVehiclePresetRevision();
			AICF_OwnSeatProbe.s_CarHealth = AICF_OwnSeatProbe.Health(AICF_OwnSeatProbe.s_Car);
			if (AICF_OwnSeatProbe.s_CarHealth <= 0) { AICF_OSFinish(false, "UNKNOWN_CAR_HEALTH"); return; }
			AICF_OwnSeatProbe.s_OriginalBudget = trip.GetMovementState().GetMobilityRecoveryAttempts();
			for (int index; index < 5; index++)
			{
				IEntity actor = AICF_OwnSeatProbe.s_Slot.GetRosterMember(index);
				if (!actor || AICF_OwnSeatProbe.Health(actor) <= 0 || AICF_VehicleBehaviorBinding.ComponentRpl(actor) == "NONE") { AICF_OSFinish(false, "UNKNOWN_ACTOR_IDENTITY_HEALTH"); return; }
				AICF_OwnSeatProbe.s_Members.Insert(actor); AICF_OwnSeatProbe.s_Ids.Insert(actor.GetID()); AICF_OwnSeatProbe.s_IdTexts.Insert(AICF_GroupTrace.Entity(actor)); AICF_OwnSeatProbe.s_Rpls.Insert(AICF_VehicleBehaviorBinding.ComponentRpl(actor)); AICF_OwnSeatProbe.s_Health.Insert(AICF_OwnSeatProbe.Health(actor));
			}
			array<BaseCompartmentSlot> seats = {};
			if (!AICF_VehiclePreset.CollectCaptured(AICF_OwnSeatProbe.s_Car, trip.GetVehiclePreset(), seats)) { AICF_OSFinish(false, "TOPOLOGY"); return; }
			foreach (BaseCompartmentSlot seat : seats)
			{
				if (seat.GetType() == ECompartmentType.PILOT) AICF_OwnSeatProbe.s_PilotSeat = seat;
				else if (!AICF_OwnSeatProbe.s_CargoSeat && seat.GetType() == ECompartmentType.CARGO) AICF_OwnSeatProbe.s_CargoSeat = seat;
			}
			if (!AICF_OwnSeatProbe.s_PilotSeat || !AICF_OwnSeatProbe.s_CargoSeat || !AICF_OwnSeatProbe.Exact(trip)) { AICF_OSFinish(false, "INITIAL_EXACT5"); return; }
			AICF_OwnSeatProbe.s_Pilot = AICF_OwnSeatProbe.s_PilotSeat.GetOccupant(); AICF_OwnSeatProbe.s_Cargo = AICF_OwnSeatProbe.s_CargoSeat.GetOccupant();
			AICF_OwnSeatProbe.Log("INITIAL_EXACT5", string.Format("operation=%1 vehicle=%2 vehicle_rpl=%3 lease_generation=%4 vehicle_generation=%5 payments=%6", trip.GetOperationId(), AICF_GroupTrace.Entity(AICF_OwnSeatProbe.s_Car), AICF_OwnSeatProbe.s_CarRpl, AICF_OwnSeatProbe.s_LeaseGeneration, AICF_OwnSeatProbe.s_VehicleGeneration, AICF_OwnSeatProbe.s_Payments));
			AICF_OwnSeatProbe.ActorLedger("INITIAL");
		}
		if (AICF_OwnSeatProbe.s_Invalid) { AICF_OSFinish(false, "SETUP_OR_IDENTITY_INVALID"); return; }
		if (trip.IsTerminal())
		{
			AICF_OSFinish(AICF_OwnSeatProbe.Mode() == "BEFORE" && AICF_OwnSeatProbe.s_ConflictObserved && trip.GetTerminalReason() == "TACTICAL_SEAT_OCCUPIED", "TERMINAL_" + trip.GetTerminalReason()); return;
		}
		if (!AICF_OwnSeatProbe.Current(trip)) { AICF_OSFinish(false, "IDENTITY_HEALTH_CHANGED"); return; }
		if (AICF_OwnSeatProbe.InterruptCase() && AICF_OwnSeatProbe.s_InterruptObserved && !AICF_OwnSeatProbe.s_SecondThreatHandled)
		{
			AICF_OwnSeatProbe.s_SecondThreatHandled = true;
			m_iOSStep = 1;
		}
		if (m_iOSStep == 0 && AICF_VehicleShortRecovery.IsSlowVehicle(AICF_OwnSeatProbe.s_Car)) { AICF_OwnSeatProbe.s_Threat = true; m_iOSStep = 1; }
		if (m_iOSStep == 1 && trip.GetPhase() == AICF_ETransportTripPhase.TACTICAL_DEPLOYMENT)
		{
			bool outside = true;
			foreach (IEntity actor : AICF_OwnSeatProbe.s_Members)
			{
				CompartmentAccessComponent access = ChimeraCharacter.Cast(actor).GetCompartmentAccessComponent();
				if (!access || access.IsInCompartment() || access.IsGettingIn() || access.IsGettingOut() || access.IsSwitchingSeatsAnim() || CompartmentAccessComponent.GetVehicleIn(actor)) outside = false;
			}
			if (outside)
			{
				if (AICF_OwnSeatProbe.InterruptCase() && !AICF_OwnSeatProbe.s_InterruptObserved)
				{
					AICF_OwnSeatProbe.s_Threat = false; m_iOSStep = 2;
					AICF_OwnSeatProbe.Log("FIRST_THREAT_CLEARED_NO_FAR_INTENT", "ordinary_reboard=1 distant_override=0");
					return;
				}
				vector target;
				if (!AICF_OSFarPoint(AICF_OwnSeatProbe.s_Car.GetOrigin(), target)) return;
				if (!m_OrderPlanner.AssignPlayerPointOrder(AICF_OwnSeatProbe.s_Slot, m_USSRFaction, target, m_VehicleCoordinator.IsInfantryOrderSuspended(AICF_OwnSeatProbe.s_Slot))) return;
				AICF_OwnSeatProbe.s_Threat = false; m_iOSStep = 2;
				AICF_OwnSeatProbe.Log("THREAT_CLEARED_FAR_ORDER", string.Format("target=%1 distance=%2 phase=%3", target, vector.DistanceXZ(AICF_OwnSeatProbe.s_Car.GetOrigin(), target), trip.GetPhase()));
			}
		}
		if (!AICF_OwnSeatProbe.s_FaultObserved || trip.GetPhase() != AICF_ETransportTripPhase.TRANSIT) return;
		if (!AICF_OwnSeatProbe.s_Resumed)
		{
			if (!AICF_OwnSeatProbe.Exact(trip)) { AICF_OSFinish(false, "RESUME_WITHOUT_EXACT5"); return; }
			AICF_OwnSeatProbe.s_Resumed = true; AICF_OwnSeatProbe.s_ResumePosition = AICF_OwnSeatProbe.s_Car.GetOrigin();
			AICF_OwnSeatProbe.Log("EXACT5_RESUMED", string.Format("operation=%1 current_deadline=%2 original_deadline=%3 shared_short=%4", trip.GetOperationId(), trip.GetBehaviorState().m_iReboardDeadline, AICF_OwnSeatProbe.s_Deadline, AICF_OwnSeatProbe.s_LastShort));
		}
		if (System.GetTickCount() < AICF_OwnSeatProbe.s_NextMotion) return;
		AICF_OwnSeatProbe.s_NextMotion = System.GetTickCount() + 500;
		if (AICF_OwnSeatProbe.s_Car.GetPhysics() && AICF_OwnSeatProbe.s_Car.GetPhysics().GetVelocity().Length() >= 0.5) AICF_OwnSeatProbe.s_MotionPolls++;
		bool interruptProof = !AICF_OwnSeatProbe.InterruptCase() || (AICF_OwnSeatProbe.s_InterruptObserved && AICF_OwnSeatProbe.s_SecondDeadline > 0);
		bool shortProof = AICF_OwnSeatProbe.s_LastShort > 0 && AICF_OwnSeatProbe.s_LastShort <= 3;
		if (AICF_OwnSeatProbe.InterruptCase()) shortProof = AICF_OwnSeatProbe.s_FirstShort > 0 && AICF_OwnSeatProbe.s_FirstShort <= 3 && AICF_OwnSeatProbe.s_LastShort >= 0 && AICF_OwnSeatProbe.s_LastShort <= 3;
		if (AICF_OwnSeatProbe.Mode() == "AFTER" && interruptProof && shortProof && AICF_OwnSeatProbe.s_ExitObserved && AICF_OwnSeatProbe.s_MotionPolls >= 3 &&
			vector.DistanceXZ(AICF_OwnSeatProbe.s_ResumePosition, AICF_OwnSeatProbe.s_Car.GetOrigin()) >= 10 && AICF_OwnSeatProbe.Exact(trip) &&
			trip.GetBehaviorState().m_iReboardDeadline == AICF_OwnSeatProbe.s_Deadline && trip.GetMovementState().GetMobilityRecoveryAttempts() == AICF_OwnSeatProbe.s_OriginalBudget && AICF_OwnSeatProbe.s_Payments == 1 && AICF_OwnSeatProbe.s_Paid > 0 && AICF_OwnSeatProbe.s_Spawns == 1)
			AICF_OSFinish(true, "OWN_ORIGINAL_RECOVERED_EXACT5_NATIVE_10M");
	}
	override protected void Stop(bool cleanupEntities)
	{
		if (AICF_OwnSeatProbe.s_Owner == this)
		{
			AICF_OwnSeatProbe.s_Trip = null; AICF_OwnSeatProbe.s_Slot = null; AICF_OwnSeatProbe.s_Group = null;
			AICF_OwnSeatProbe.s_Car = null; AICF_OwnSeatProbe.s_Lease = null; AICF_OwnSeatProbe.s_Pilot = null; AICF_OwnSeatProbe.s_Cargo = null;
			AICF_OwnSeatProbe.s_PilotSeat = null; AICF_OwnSeatProbe.s_CargoSeat = null; AICF_OwnSeatProbe.s_Owner = null;
			AICF_OwnSeatProbe.s_Members.Clear(); AICF_OwnSeatProbe.s_Ids.Clear(); AICF_OwnSeatProbe.s_IdTexts.Clear(); AICF_OwnSeatProbe.s_Rpls.Clear(); AICF_OwnSeatProbe.s_Health.Clear();
		}
		super.Stop(cleanupEntities);
	}
}
