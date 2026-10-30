// Физический deployment/возврат в ту же машину. Переходами, waypoint и
// временной передачей infantry order владеет TransportTripController.
class AICF_VehicleBehaviorFlow
{
	static const int DEPLOYMENT_TIMEOUT_MS = 40000;
	protected ref AICF_Stage3Config m_Config;

	void AICF_VehicleBehaviorFlow(AICF_Stage3Config config)
	{
		m_Config = config;
		if (!m_Config) m_Config = new AICF_Stage3Config();
	}

	static bool IsCrewSeat(BaseCompartmentSlot seat)
	{
		return seat && (seat.GetType() == ECompartmentType.PILOT ||
			seat.GetType() == ECompartmentType.TURRET || seat.GetAttachedTurret());
	}

	static bool IsFit(IEntity member)
	{
		return AICF_VehicleBoardingMutationFence.IsAuthoritativeAIEntity(member) && SCR_AIDamageHandling.IsConscious(member);
	}

	bool IsCurrent(AICF_TransportTrip trip)
	{
		if (!Replication.IsServer() || !trip || !trip.IsValid() || trip.IsTerminal() ||
			!trip.GetVehiclePreset() || !trip.IsVehiclePresetCurrent() || !trip.GetLease()) return false;
		AICF_VehicleLease lease = trip.GetLease();
		if (!lease.HasPhysicalAsset() || !lease.MatchesTripIdentity(trip.GetFactionKey(), trip.GetSlotId(), trip.GetGroupGeneration(), trip.GetTripGeneration())) return false;
		Vehicle vehicle = lease.GetVehicle();
		return AICF_VehicleBoardingMutationFence.IsAuthoritativeReplicatedEntity(vehicle) &&
			AICF_VehicleBoardingMutationFence.IsAuthoritativeReplicatedEntity(trip.GetAssignment().GetGroup()) &&
			lease.MatchesEntityIdentity(vehicle, vehicle.GetID(), AICF_VehicleBehaviorBinding.ComponentRpl(vehicle));
	}

	AICF_TripOutcome InspectTransit(AICF_TransportTrip trip, string causationId)
	{
		if (!trip || !trip.GetVehiclePreset()) return null;
		if (!IsCurrent(trip)) return AICF_TripOutcome.FallbackToFoot("BEHAVIOR_CONTEXT_LOST", causationId);
		AICF_VehicleBehaviorPolicy.Register(trip);
		bool combat = AICF_VehicleBehaviorPolicy.HasLocalCombat(trip);
		AICF_VehicleBehaviorPolicy.Update(trip, combat, trip.GetBehaviorState().m_aDeployed);
		if (!combat) trip.GetBehaviorState().m_iCombatOverrideIntent = -1;
		if (combat && !AICF_VehicleBehaviorPolicy.HasTransportCombatOverride(trip))
			return AICF_TripOutcome.StartTacticalDeployment("LOCAL_THREAT", causationId);
		array<BaseCompartmentSlot> seats = {};
		if (!CollectBehaviorSeats(trip, seats, "TRANSIT")) return AICF_TripOutcome.FallbackToFoot("BEHAVIOR_TOPOLOGY_CHANGED", causationId);
		for (int index; index < seats.Count(); index++)
		{
			BaseCompartmentSlot seat = seats[index];
			if (!IsCrewSeat(seat)) continue;
			if (seat.GetType() != ECompartmentType.PILOT && trip.GetVehiclePreset().m_aMembers[index] < 0) continue;
			if (!IsFit(seat.GetOccupant()))
			{
				AICF_TransportNativeGate gate = trip.GetMovementState().GetNativeGate();
				IEntity assignedMember = trip.VehiclePresetMember(trip.GetVehiclePreset().m_aMembers[index]);
				if (gate && gate.OwnsTemporaryCrewExit(trip, seat, assignedMember))
				{
					gate.Log("TRANSPORT_NATIVE_GATE_CREW_EXIT_OWNED");
					continue;
				}
				return AICF_TripOutcome.StartTacticalDeployment("CREW_UNAVAILABLE", causationId);
			}
		}
		return null;
	}

	bool Begin(AICF_TransportTrip trip, string reason)
	{
		if (!IsCurrent(trip) || trip.GetPhase() != AICF_ETransportTripPhase.TACTICAL_DEPLOYMENT) return false;
		CancelEntries(trip);
		AICF_VehicleBehaviorState state = trip.GetBehaviorState();
		if (trip.GetVehiclePreset().m_iBehavior == AICF_EVehicleBehavior.TRANSPORT)
		{
			if (!state.m_Parking) state.m_Parking = new AICF_VehicleParking(trip);
			if (!state.m_Parking.EnsureApplied(trip)) return false;
		}
		state.m_bThreatDeployment = reason == "LOCAL_THREAT" || reason == "CREW_UNAVAILABLE";
		state.m_bSawCombat = AICF_VehicleBehaviorPolicy.HasLocalCombat(trip);
		state.m_bReboarding = false;
		state.m_bParkedAfterCombat = false;
		state.m_iDeploymentIntent = trip.GetAssignment().GetStrategicIntentRevision();
		state.m_iSettledPolls = 0;
		state.m_iLastReport = 0;
		state.m_iDeploymentDeadline = System.GetTickCount() + DEPLOYMENT_TIMEOUT_MS;
		state.m_iCrewRestoreDeadline = 0;
		state.m_iCombatOverrideIntent = -1;
		state.m_aDeployed.Clear();
		array<BaseCompartmentSlot> seats = {};
		AICF_VehiclePreset preset = trip.GetVehiclePreset();
		if (!CollectBehaviorSeats(trip, seats, "BEGIN_DEPLOYMENT")) return false;
		for (int index; index < seats.Count(); index++)
		{
			if (preset.m_iBehavior == AICF_EVehicleBehavior.COMBAT && IsCrewSeat(seats[index])) continue;
			IEntity actor = trip.VehiclePresetMember(preset.m_aMembers[index]);
			if (IsFit(actor)) state.m_aDeployed.Insert(actor);
		}
		AICF_VehicleBehaviorPolicy.Update(trip, state.m_bSawCombat, state.m_aDeployed);
		Report(trip, "DEPLOYMENT_BEGIN", reason);
		return true;
	}

	AICF_TripOutcome Tick(AICF_TransportTrip trip, string causationId)
	{
		if (!IsCurrent(trip)) return AICF_TripOutcome.FallbackToFoot("BEHAVIOR_CONTEXT_LOST", causationId);
		AICF_VehicleBehaviorState state = trip.GetBehaviorState();
		Vehicle vehicle = trip.GetLease().GetVehicle();
		SCR_AIVehicleUsageComponent usage = SCR_AIVehicleUsageComponent.Cast(vehicle.FindComponent(SCR_AIVehicleUsageComponent));
		if (!usage || usage.GetDamageState() == EDamageState.DESTROYED) return AICF_TripOutcome.FallbackToFoot("BEHAVIOR_VEHICLE_DESTROYED", causationId);
		if (SCR_AIVehicleUsability.VehicleIsOnFire(vehicle)) return AICF_TripOutcome.FallbackToFoot("BEHAVIOR_VEHICLE_ON_FIRE", causationId);
		bool combat = AICF_VehicleBehaviorPolicy.HasLocalCombat(trip);
		state.m_bSawCombat = state.m_bSawCombat || combat;
		AICF_VehicleBehaviorPolicy.Update(trip, combat, state.m_aDeployed);
		if (state.m_bReboarding) return PollReboarding(trip, causationId, combat);
		if (!DeployActors(trip))
		{
			if (System.GetTickCount() >= state.m_iDeploymentDeadline) return AICF_TripOutcome.FallbackToFoot("TACTICAL_DISEMBARK_TIMEOUT", causationId);
			return AICF_TripOutcome.Wait("TACTICAL_DISEMBARK_PENDING", causationId);
		}
		if (System.GetTickCount() - state.m_iLastReport >= 10000)
		{
			state.m_iLastReport = System.GetTickCount();
			Report(trip, "DEPLOYMENT_WAIT", "LOCAL_COMBAT_OR_FOOT_ORDER", combat);
		}
		// Боевая машина не восстанавливает потерянный экипаж в ongoing бою.
		if (combat && trip.GetVehiclePreset().m_iBehavior == AICF_EVehicleBehavior.COMBAT)
			return AICF_TripOutcome.Wait("COMBAT_CREW_RECOVERY_DEFERRED", causationId);
		// Транспорт назначает замену сразу; посадка ждёт calm либо distant order.
		string failure;
		if (!RestoreCrewAssignments(trip, failure))
		{
			if (failure == "CREW_RESTORE_PENDING") return AICF_TripOutcome.Wait(failure, causationId);
			return AICF_TripOutcome.FallbackToFoot(failure, causationId);
		}
		bool distantOrder = HasDistantOrder(trip);
		if (combat && !distantOrder) return AICF_TripOutcome.Wait("TRANSPORT_FOOT_COMBAT", causationId);
		if (!state.m_bThreatDeployment && !distantOrder) return AICF_TripOutcome.Wait("TACTICAL_FOOT_ORDER", causationId);
		if (!SCR_AIVehicleUsability.VehicleCanMove(vehicle)) return AICF_TripOutcome.FallbackToFoot("BEHAVIOR_MOVEMENT_UNAVAILABLE", causationId);
		CancelEntries(trip);
		state.m_bReboarding = true;
		int livingAssigned;
		foreach (int position : trip.GetVehiclePreset().m_aMembers)
			if (IsFit(trip.VehiclePresetMember(position))) livingAssigned++;
		state.m_iReboardDeadline = System.GetTickCount() + m_Config.GetBoardingTimeoutMs() * Math.Max(3, livingAssigned);
		state.m_iNextReboardTraceAt = 0;
		state.m_iSettledPolls = 0;
		state.m_aDeployed.Clear();
		AICF_VehicleBehaviorPolicy.Update(trip, combat, state.m_aDeployed);
		Report(trip, "REBOARD_BEGIN", "SAME_VEHICLE_NO_PURCHASE");
		return AICF_TripOutcome.Wait("TACTICAL_REBOARD_BEGIN", causationId);
	}

	protected bool HasDistantOrder(AICF_TransportTrip trip)
	{
		IEntity leader = AICF_GroupRuntime.ResolveAliveLeader(trip.GetAssignment().GetGroup());
		return leader && trip.GetAssignment().GetStrategicIntentRevision() != trip.GetBehaviorState().m_iDeploymentIntent &&
			vector.DistanceXZ(leader.GetOrigin(), trip.GetAssignment().GetTargetPosition()) > trip.GetVehiclePreset().m_iReturnDistance;
	}

	protected AICF_VehicleBehaviorAction FindAction(AICF_TransportTrip trip, IEntity actor)
	{
		foreach (AICF_VehicleBehaviorAction token : trip.GetBehaviorState().m_aActions)
			if (token && token.m_Actor == actor) return token;
		return null;
	}

	protected bool DeployActors(AICF_TransportTrip trip)
	{
		if (trip.GetVehiclePreset().m_iBehavior == AICF_EVehicleBehavior.TRANSPORT &&
			(!trip.GetBehaviorState().m_Parking || !trip.GetBehaviorState().m_Parking.IsReadyForExit(trip))) return false;
		bool outside = true;
		foreach (IEntity actor : trip.GetBehaviorState().m_aDeployed)
		{
			if (!IsFit(actor)) continue;
			if (!AICF_VehicleShortRecovery.IsCurrentMember(trip, actor)) { outside = false; continue; }
			ChimeraCharacter character = ChimeraCharacter.Cast(actor);
			CompartmentAccessComponent access = character.GetCompartmentAccessComponent();
			if (!access) { outside = false; continue; }
			if (!access.IsInCompartment() && !access.IsGettingIn() && !access.IsGettingOut() && !character.IsInVehicle() &&
				!CompartmentAccessComponent.GetVehicleIn(actor)) continue;
			outside = false;
			EnsureExit(trip, actor, access.GetCompartment());
			// Native GetOut просит остановку; correction ждёт реальный slow car.
			AICF_VehicleShortRecovery.TryExit(trip, actor, "TACTICAL_DEPLOYMENT");
		}
		return outside;
	}

	protected void EnsureExit(AICF_TransportTrip trip, IEntity actor, BaseCompartmentSlot seat)
	{
		if (!AICF_VehicleShortRecovery.IsCurrentMember(trip, actor)) return;
		if (seat && seat.GetType() == ECompartmentType.PILOT && trip.GetVehiclePreset().m_iBehavior == AICF_EVehicleBehavior.TRANSPORT &&
			(!trip.GetBehaviorState().m_Parking || !trip.GetBehaviorState().m_Parking.IsReadyForExit(trip))) return;
		AICF_VehicleBehaviorAction token = FindAction(trip, actor);
		if (token && token.m_Exit) return;
		if (token) token.Retire();
		else token = new AICF_VehicleBehaviorAction(trip, actor, seat);
		SCR_AIUtilityComponent utility = token.ResolveOwner();
		if (!utility || !IsCurrent(trip) || !AICF_VehicleShortRecovery.IsCurrentMember(trip, actor)) return;
		token.m_Exit = new SCR_AIGetOutVehicle(utility, null, trip.GetLease().GetVehicle(),
			priority: SCR_AIActionBase.PRIORITY_BEHAVIOR_GET_OUT_VEHICLE_HIGH_PRIORITY,
			priorityLevel: SCR_AIActionBase.PRIORITY_LEVEL_NORMAL);
		if (!trip.GetBehaviorState().m_aActions.Contains(token)) trip.GetBehaviorState().m_aActions.Insert(token);
		utility.AddAction(token.m_Exit);
	}

	protected bool RestoreCrewAssignments(AICF_TransportTrip trip, out string reason)
	{
		reason = "NO_SURVIVING_MOBILE_CREW";
		AICF_VehiclePreset preset = trip.GetVehiclePreset();
		array<BaseCompartmentSlot> seats = {};
		if (!CollectBehaviorSeats(trip, seats, "RESTORE_CREW")) { reason = "BEHAVIOR_TOPOLOGY_CHANGED"; return false; }
		// Pilot первым; исправный стрелок не отбирается при наличии десанта.
		for (int pass; pass < 2; pass++)
			for (int index; index < seats.Count(); index++)
			{
				BaseCompartmentSlot seat = seats[index];
				if (!IsCrewSeat(seat)) continue;
				if ((pass == 0) != (seat.GetType() == ECompartmentType.PILOT)) continue;
				// Native транспорт мог уже посадить своего живого replacement.
				// Принимаем только exact captured actor, не вытесняем его повторно.
				IEntity occupant = seat.GetOccupant();
				if (IsFit(occupant) && AICF_VehicleShortRecovery.IsCurrentMember(trip, occupant))
				{
					int actualPosition = FindCapturedPosition(trip, occupant);
					if (actualPosition < 0) { reason = "CREW_OCCUPANT_IDENTITY_CHANGED"; return false; }
					if (preset.m_aMembers[index] != actualPosition)
					{
						if (!trip.ReassignVehiclePresetSeat(index, actualPosition)) { reason = "CREW_REASSIGN_IDENTITY_CHANGED"; return false; }
						Report(trip, "CREW_REASSIGNED", string.Format("seat=%1 member=%2 native_adopted=1", AICF_VehiclePreset.SeatKey(seat), occupant.GetID()));
					}
					continue;
				}
				IEntity assigned = trip.VehiclePresetMember(preset.m_aMembers[index]);
				if (IsFit(assigned)) continue;
				int replacement = FindFreeSurvivor(trip, seats);
				if (replacement < 0 && seat.GetType() == ECompartmentType.PILOT)
					replacement = FindFreeSurvivor(trip, seats, true);
				if (replacement < 0)
				{
					if (seat.GetType() == ECompartmentType.PILOT) return false;
					if (!trip.ReassignVehiclePresetSeat(index, -1)) { reason = "CREW_REASSIGN_IDENTITY_CHANGED"; return false; }
					continue;
				}
				if (seat.GetOccupant())
				{
					AICF_VehicleShortRecovery.TryClearIncapacitatedSeat(trip, seat, "CREW_REPLACEMENT");
					return WaitForCrewClear(trip, reason);
				}
				IEntity actor = trip.VehiclePresetMember(replacement);
				CompartmentAccessComponent access = ChimeraCharacter.Cast(actor).GetCompartmentAccessComponent();
				if (!access) { reason = "CREW_ACCESS_MISSING"; return false; }
				if (access.IsInCompartment() || access.IsGettingIn() || access.IsGettingOut() || CompartmentAccessComponent.GetVehicleIn(actor))
				{
					EnsureExit(trip, actor, access.GetCompartment());
					AICF_VehicleShortRecovery.TryExit(trip, actor, "CREW_ROLE_CHANGE");
					return WaitForCrewClear(trip, reason);
				}
				if (!trip.ReassignVehiclePresetSeat(index, replacement)) { reason = "CREW_REASSIGN_IDENTITY_CHANGED"; return false; }
				Report(trip, "CREW_REASSIGNED", string.Format("seat=%1 member=%2", AICF_VehiclePreset.SeatKey(seat), actor.GetID()));
			}
		return FillSurvivorCargo(trip, seats, reason);
	}

	// Проснувшийся original member сохраняется в составе, даже если его crew role
	// уже получил другой actor. Рецепт библиотеки и здоровье не меняются.
	protected bool FillSurvivorCargo(AICF_TransportTrip trip, array<BaseCompartmentSlot> seats, out string reason)
	{
		reason = "FIT_ROSTER_IDENTITY_CHANGED";
		if (!IsCurrent(trip)) return false;
		AICF_VehiclePreset preset = trip.GetVehiclePreset();
		for (int position; position < AICF_Stage1Config.MAX_GROUP_SIZE; position++)
		{
			IEntity actor = trip.VehiclePresetMember(position);
			if (!IsFit(actor)) continue;
			if (FindCapturedPosition(trip, actor) != position) return false;
			if (preset.m_aMembers.Contains(position)) continue;
			int cargo = -1;
			for (int index; index < seats.Count(); index++)
			{
				BaseCompartmentSlot seat = seats[index];
				if (seat.GetType() != ECompartmentType.CARGO || IsCrewSeat(seat) || seat.GetOccupant() ||
					IsFit(trip.VehiclePresetMember(preset.m_aMembers[index]))) continue;
				cargo = index;
				break;
			}
			if (cargo < 0) { reason = "FIT_SURVIVOR_WITHOUT_CARGO"; return false; }
			if (!trip.ReassignVehiclePresetSeat(cargo, position)) return false;
			trip.GetBehaviorState().m_iSettledPolls = 0;
			Report(trip, "SURVIVOR_CARGO_ASSIGNED", string.Format("member=%1 seat=%2", actor.GetID(), AICF_VehiclePreset.SeatKey(seats[cargo])));
		}
		return CheckFitGroupAssignments(trip, seats, false, reason);
	}

	static bool HasUniqueSeatAssignment(array<int> members, int position)
	{
		if (position < 0) return false;
		int count;
		foreach (int assigned : members)
			if (assigned == position) count++;
		return count == 1;
	}

	// Проверяется actual current group, а не только оставшиеся effective seats.
	protected bool CheckFitGroupAssignments(AICF_TransportTrip trip, array<BaseCompartmentSlot> seats,
		bool requireSettled, out string reason)
	{
		reason = "FIT_ROSTER_IDENTITY_CHANGED";
		if (!IsCurrent(trip)) return false;
		SCR_AIGroup group = trip.GetAssignment().GetGroup();
		array<AIAgent> agents = {};
		group.GetAgents(agents);
		int fit;
		foreach (AIAgent agent : agents)
		{
			if (!agent) return false;
			IEntity actor = agent.GetControlledEntity();
			if (!AICF_GroupRuntime.IsAliveCharacter(actor) || !SCR_AIDamageHandling.IsConscious(actor)) continue;
			if (agent.GetParentGroup() != group || !IsFit(actor)) return false;
			int position = FindCapturedPosition(trip, actor);
			if (position < 0 || !HasUniqueSeatAssignment(trip.GetVehiclePreset().m_aMembers, position)) return false;
			fit++;
			if (!requireSettled) continue;
			int index = trip.GetVehiclePreset().m_aMembers.Find(position);
			BaseCompartmentSlot seat = seats[index];
			CompartmentAccessComponent access = ChimeraCharacter.Cast(actor).GetCompartmentAccessComponent();
			if (seat.GetOccupant() != actor || !access || !access.IsInCompartment() || access.GetCompartment() != seat ||
				access.IsGettingIn() || access.IsGettingOut() || access.IsSwitchingSeatsAnim() ||
				CompartmentAccessComponent.GetVehicleIn(actor) != trip.GetLease().GetVehicle())
			{
				reason = "TACTICAL_REBOARD_PENDING";
				return false;
			}
		}
		if (fit == 0) return false;
		reason = "READY";
		return true;
	}

	protected int FindCapturedPosition(AICF_TransportTrip trip, IEntity actor)
	{
		if (!AICF_VehicleShortRecovery.IsCurrentMember(trip, actor)) return -1;
		AICF_GroupSlot slot = trip.GetVehiclePresetSlot();
		if (!slot) return -1;
		for (int position; position < AICF_Stage1Config.MAX_GROUP_SIZE; position++)
			if (trip.VehiclePresetMember(position) == actor && slot.GetRosterMember(position) == actor) return position;
		return -1;
	}

	protected bool WaitForCrewClear(AICF_TransportTrip trip, out string reason)
	{
		AICF_VehicleBehaviorState state = trip.GetBehaviorState();
		if (state.m_iCrewRestoreDeadline <= 0) state.m_iCrewRestoreDeadline = System.GetTickCount() + m_Config.GetBoardingTimeoutMs();
		reason = "CREW_RESTORE_PENDING";
		if (System.GetTickCount() >= state.m_iCrewRestoreDeadline) reason = "CREW_RESTORE_TIMEOUT";
		return false;
	}

	protected int FindFreeSurvivor(AICF_TransportTrip trip, array<BaseCompartmentSlot> seats, bool allowTurret = false)
	{
		AICF_VehiclePreset preset = trip.GetVehiclePreset();
		for (int position; position < AICF_Stage1Config.MAX_GROUP_SIZE; position++)
		{
			IEntity actor = trip.VehiclePresetMember(position);
			if (!IsFit(actor) || !AICF_VehicleShortRecovery.IsCurrentMember(trip, actor)) continue;
			int seatIndex = preset.m_aMembers.Find(position);
			if (seatIndex >= 0 && IsCrewSeat(seats[seatIndex]))
			{
				if (!allowTurret || seats[seatIndex].GetType() == ECompartmentType.PILOT) continue;
			}
			return position;
		}
		return -1;
	}

	static bool HasMobilePilot(AICF_TransportTrip trip, array<BaseCompartmentSlot> seats, bool requireSettled)
	{
		AICF_VehiclePreset preset = trip.GetVehiclePreset();
		for (int index; index < seats.Count(); index++)
		{
			BaseCompartmentSlot seat = seats[index];
			if (seat.GetType() != ECompartmentType.PILOT) continue;
			IEntity actor = trip.VehiclePresetMember(preset.m_aMembers[index]);
			if (!IsFit(actor) || !AICF_VehicleShortRecovery.IsCurrentMember(trip, actor)) return false;
			if (!requireSettled) return true;
			CompartmentAccessComponent access = ChimeraCharacter.Cast(actor).GetCompartmentAccessComponent();
			return seat.GetOccupant() == actor && access && access.IsInCompartment() && access.GetCompartment() == seat &&
				!access.IsGettingIn() && !access.IsGettingOut() && !access.IsSwitchingSeatsAnim() &&
				CompartmentAccessComponent.GetVehicleIn(actor) == trip.GetLease().GetVehicle();
		}
		return false;
	}

	protected bool ReconcileReboardingActions(AICF_TransportTrip trip, array<BaseCompartmentSlot> seats)
	{
		AICF_VehicleBehaviorState state = trip.GetBehaviorState();
		AICF_VehiclePreset preset = trip.GetVehiclePreset();
		for (int index; index < state.m_aActions.Count(); index++)
		{
			AICF_VehicleBehaviorAction token = state.m_aActions[index];
			if (!token || !IsFit(token.m_Actor)) continue;
			int position = FindCapturedPosition(trip, token.m_Actor);
			if (position < 0) return false;
			int seatIndex = preset.m_aMembers.Find(position);
			if (seatIndex < 0) return false;
			if (token.m_Seat == seats[seatIndex]) continue;
			token.Retire();
			if (!IsCurrent(trip) || !AICF_VehicleShortRecovery.IsCurrentMember(trip, token.m_Actor)) return false;
			AICF_VehicleBehaviorAction rebound = new AICF_VehicleBehaviorAction(trip, token.m_Actor, seats[seatIndex]);
			// Remap роли не создаёт новый retry/short-recovery budget.
			rebound.m_iRetries = token.m_iRetries;
			rebound.m_iShortAttempts = token.m_iShortAttempts;
			rebound.m_iLastProgressAt = token.m_iLastProgressAt;
			rebound.m_vLastProgressPosition = token.m_vLastProgressPosition;
			state.m_aActions[index] = rebound;
			state.m_iSettledPolls = 0;
		}
		return true;
	}

	protected AICF_TripOutcome PollReboarding(AICF_TransportTrip trip, string causationId, bool combat)
	{
		AICF_VehicleBehaviorState state = trip.GetBehaviorState();
		if (combat && (trip.GetVehiclePreset().m_iBehavior == AICF_EVehicleBehavior.COMBAT || !HasDistantOrder(trip)))
		{
			CancelEntries(trip);
			Begin(trip, "LOCAL_THREAT");
			return AICF_TripOutcome.Wait("COMBAT_REBOARD_CANCELLED", causationId);
		}
		if (System.GetTickCount() >= state.m_iReboardDeadline)
		{
			ReportReboarding(trip, "DEADLINE");
			return AICF_TripOutcome.FallbackToFoot("TACTICAL_REBOARD_TIMEOUT", causationId);
		}
		array<BaseCompartmentSlot> seats = {};
		AICF_VehiclePreset preset = trip.GetVehiclePreset();
		if (!CollectBehaviorSeats(trip, seats, "POLL_REBOARDING")) return AICF_TripOutcome.FallbackToFoot("BEHAVIOR_TOPOLOGY_CHANGED", causationId);
		if (state.m_ReboardSeatConflict)
		{
			AICF_ReboardSeatConflictScope pendingConflict = state.m_ReboardSeatConflict;
			int exitReadback = pendingConflict.ObserveExitedReservation();
			if (!IsCurrent(trip) || trip.GetBehaviorState() != state || state.m_ReboardSeatConflict != pendingConflict)
				return AICF_TripOutcome.Wait("TACTICAL_OWN_SEAT_SCOPE_CHANGED", causationId);
			if (exitReadback < 0) return AICF_TripOutcome.FallbackToFoot("TACTICAL_OWN_RESERVATION_IDENTITY_CHANGED", causationId);
			if (exitReadback == 0) return AICF_TripOutcome.Wait("TACTICAL_OWN_SEAT_EXIT_READBACK_PENDING", causationId);
			state.m_ReboardSeatConflict = null;
		}
		string fitReason;
		if (!HasMobilePilot(trip, seats, false) && !RestoreCrewAssignments(trip, fitReason))
		{
			state.m_iSettledPolls = 0;
			if (fitReason == "CREW_RESTORE_PENDING") return AICF_TripOutcome.Wait(fitReason, causationId);
			return AICF_TripOutcome.FallbackToFoot(fitReason, causationId);
		}
		if (!FillSurvivorCargo(trip, seats, fitReason)) return AICF_TripOutcome.FallbackToFoot(fitReason, causationId);
		if (!ReconcileReboardingActions(trip, seats)) return AICF_TripOutcome.FallbackToFoot("REBOARD_ROLE_IDENTITY_CHANGED", causationId);
		if (System.GetTickCount() >= state.m_iNextReboardTraceAt)
		{
			state.m_iNextReboardTraceAt = System.GetTickCount() + 5000;
			ReportReboarding(trip, "POLL");
		}
		bool settled = true;
		for (int index; index < seats.Count(); index++)
		{
			IEntity actor = trip.VehiclePresetMember(preset.m_aMembers[index]);
			if (!IsFit(actor)) continue;
			BaseCompartmentSlot seat = seats[index];
			CompartmentAccessComponent access = ChimeraCharacter.Cast(actor).GetCompartmentAccessComponent();
			if (seat.GetOccupant() == actor && access && access.IsInCompartment() && access.GetCompartment() == seat &&
				!access.IsGettingIn() && !access.IsGettingOut() && !access.IsSwitchingSeatsAnim() &&
				CompartmentAccessComponent.GetVehicleIn(actor) == trip.GetLease().GetVehicle()) continue;
			settled = false;
			if (seat.GetOccupant() && seat.GetOccupant() != actor)
			{
				ReportSeatConflict(trip, actor, seat, "BEFORE_DECISION");
				if (AICF_VehicleShortRecovery.IsIncapacitatedSeatCurrent(trip, seat))
				{
					AICF_VehicleShortRecovery.TryClearIncapacitatedSeat(trip, seat, "REBOARD_INCAPACITATED_BLOCKER");
					continue;
				}
				AICF_TripOutcome ownClearance = RecoverOwnSeatConflict(trip, seats, actor, seat, causationId);
				if (ownClearance) return ownClearance;
				return AICF_TripOutcome.FallbackToFoot("TACTICAL_SEAT_OCCUPIED", causationId);
			}
			EnsureEntry(trip, actor, seat);
		}
		if (!settled) { state.m_iSettledPolls = 0; return AICF_TripOutcome.Wait("TACTICAL_REBOARD_PENDING", causationId); }
		state.m_iSettledPolls++;
		if (state.m_iSettledPolls < 2) return AICF_TripOutcome.Wait("TACTICAL_SETTLED_CONFIRMATION", causationId);
		CancelEntries(trip);
		if (!HasMobilePilot(trip, seats, true)) return AICF_TripOutcome.FallbackToFoot("REBOARD_PILOT_UNAVAILABLE", causationId);
		if (!CheckFitGroupAssignments(trip, seats, true, fitReason))
		{
			state.m_iSettledPolls = 0;
			if (fitReason == "TACTICAL_REBOARD_PENDING") return AICF_TripOutcome.Wait(fitReason, causationId);
			return AICF_TripOutcome.FallbackToFoot(fitReason, causationId);
		}
		if (state.m_Parking && !state.m_Parking.ReleaseForResume(trip)) return AICF_TripOutcome.Wait("TACTICAL_NATIVE_PILOT_PARKING_RELEASE_PENDING", causationId);
		state.m_bParkedAfterCombat = true;
		if (combat && trip.GetVehiclePreset().m_iBehavior == AICF_EVehicleBehavior.TRANSPORT && HasDistantOrder(trip))
			state.m_iCombatOverrideIntent = trip.GetAssignment().GetStrategicIntentRevision();
		Report(trip, "REBOARD_COMPLETE", "EXACT_SURVIVORS_SAME_LEASE");
		return AICF_TripOutcome.ResumeTacticalMovement("TACTICAL_EXACT_SEATS_CONFIRMED", causationId);
	}

	protected AICF_TripOutcome RecoverOwnSeatConflict(AICF_TransportTrip trip, array<BaseCompartmentSlot> seats,
		IEntity expected, BaseCompartmentSlot blocked, string causationId)
	{
		AICF_ReboardSeatConflictScope scope = AICF_ReboardSeatConflictScope.Capture(trip, seats, blocked, expected);
		if (!scope) return null;
		if (trip.GetBehaviorState().m_ReboardSeatConflict) return AICF_TripOutcome.Wait("TACTICAL_OWN_SEAT_SCOPE_CHANGED", causationId);
		CompartmentAccessComponent access = ChimeraCharacter.Cast(scope.m_Occupant).GetCompartmentAccessComponent();
		if (access.IsGettingOut() || access.IsGettingIn() || access.IsSwitchingSeatsAnim())
			return AICF_TripOutcome.Wait("TACTICAL_OWN_SEAT_NATIVE_TRANSITION_PENDING", causationId);
		if (!scope.CanBeginOwnedExit()) return AICF_TripOutcome.Wait("TACTICAL_OWN_SEAT_NATIVE_TRANSITION_PENDING", causationId);
		AICF_VehicleBehaviorAction token = FindAction(trip, scope.m_Occupant);
		if (!token)
		{
			token = new AICF_VehicleBehaviorAction(trip, scope.m_Occupant, scope.m_AssignedSeat);
			trip.GetBehaviorState().m_aActions.Insert(token);
		}
		if (!token.ResolveOwner() || token.m_Seat != scope.m_AssignedSeat || token.m_iShortAttempts >= 3) return null;
		token.Retire();
		if (!IsCurrent(trip) || !scope.IsCurrent() || !token.ResolveOwner() ||
			!trip.GetBehaviorState().m_aActions.Contains(token) || trip.GetBehaviorState().m_ReboardSeatConflict)
			return AICF_TripOutcome.Wait("TACTICAL_OWN_SEAT_SCOPE_CHANGED", causationId);
		if (access.IsGettingOut() || access.IsGettingIn() || access.IsSwitchingSeatsAnim())
			return AICF_TripOutcome.Wait("TACTICAL_OWN_SEAT_NATIVE_TRANSITION_PENDING", causationId);
		token.m_iShortAttempts++;
		bool accepted = AICF_VehicleShortRecovery.TryExit(trip, scope.m_Occupant, "REBOARD_OWN_WRONG_SEAT", scope);
		if (!IsCurrent(trip) || !scope.IsContextCurrent() || trip.GetBehaviorState().m_ReboardSeatConflict)
			return AICF_TripOutcome.Wait("TACTICAL_OWN_SEAT_SCOPE_CHANGED", causationId);
		if (accepted) trip.GetBehaviorState().m_ReboardSeatConflict = scope;
		AICF_Stage3Diagnostics.Info("VEHICLE_REBOARD_OWN_SEAT_CLEARANCE", string.Format(
			"operation_id=%1 occupant=%2 wrong_seat=%3 assigned_seat=%4 short_attempt=%5 maximum_shared_short_attempts=3 accepted=%6 reboard_deadline_ms=%7 remaining_ms=%8 mapping_unchanged=1 captured_own_reservation=%9",
			trip.GetOperationId(), AICF_GroupTrace.Entity(scope.m_Occupant), AICF_VehiclePreset.SeatKey(blocked),
			AICF_VehiclePreset.SeatKey(scope.m_AssignedSeat), token.m_iShortAttempts, accepted,
			trip.GetBehaviorState().m_iReboardDeadline, trip.GetBehaviorState().m_iReboardDeadline - System.GetTickCount(), scope.m_bCapturedOwnReservation));
		ReportSeatConflict(trip, expected, blocked, "AFTER_SHORT_EXIT");
		return AICF_TripOutcome.Wait("TACTICAL_OWN_SEAT_CLEARANCE_PENDING", causationId);
	}

	protected void ReportSeatConflict(AICF_TransportTrip trip, IEntity expected, BaseCompartmentSlot seat, string trigger)
	{
		if (!IsCurrent(trip) || !expected || !seat) return;
		IEntity occupant = seat.GetOccupant();
		CompartmentAccessComponent access;
		SCR_AIUtilityComponent utility;
		ChimeraCharacter character = ChimeraCharacter.Cast(occupant);
		if (character)
		{
			access = character.GetCompartmentAccessComponent();
			if (character.GetAIControlComponent())
			{
				AIAgent agent = character.GetAIControlComponent().GetControlAIAgent();
				if (agent) utility = SCR_AIUtilityComponent.Cast(agent.FindComponent(SCR_AIUtilityComponent));
			}
		}
		string details = string.Format("operation_id=%1 trigger=%2 seat=%3 expected=%4 expected_rpl=%5 occupant=%6 occupant_rpl=%7 captured_position=%8",
			trip.GetOperationId(), trigger, AICF_VehiclePreset.SeatKey(seat), AICF_GroupTrace.Entity(expected),
			AICF_VehicleBehaviorBinding.ComponentRpl(expected), AICF_GroupTrace.Entity(occupant),
			AICF_VehicleBehaviorBinding.ComponentRpl(occupant), FindCapturedPosition(trip, occupant));
		details += string.Format(" assignment_revision=%1 slot_assignment_revision=%2 intent_revision=%3 preset_revision=%4 reboard_deadline_ms=%5 remaining_ms=%6",
			trip.GetAssignment().GetAssignmentRevision(), trip.GetVehiclePresetSlot().GetStrategicAssignmentRevision(),
			trip.GetAssignment().GetStrategicIntentRevision(), trip.GetVehiclePresetRevision(),
			trip.GetBehaviorState().m_iReboardDeadline, trip.GetBehaviorState().m_iReboardDeadline - System.GetTickCount());
		if (access)
		{
			string actualSeat = "NONE";
			if (access.GetCompartment()) actualSeat = AICF_VehiclePreset.SeatKey(access.GetCompartment());
			details += string.Format(" linked_vehicle=%1 actual_seat=%2 getting_in=%3 getting_out=%4 switching=%5",
				AICF_GroupTrace.Entity(CompartmentAccessComponent.GetVehicleIn(occupant)), actualSeat,
				access.IsGettingIn(), access.IsGettingOut(), access.IsSwitchingSeatsAnim());
		}
		if (utility) details += string.Format(" selected_action=%1 executed_action=%2", AICF_GroupTrace.Action(utility.GetCurrentAction()), AICF_GroupTrace.Action(utility.GetCurrentBehavior()));
		details += " readonly=1 first_conflict_branch=1";
		details += string.Format(" expected_fit=%1 occupant_fit=%2 slow_vehicle=%3 group_generation=%4 trip_generation=%5 lease_generation=%6 vehicle_generation=%7",
			IsFit(expected), IsFit(occupant), AICF_VehicleShortRecovery.IsSlowVehicle(trip.GetLease().GetVehicle()),
			trip.GetGroupGeneration(), trip.GetTripGeneration(), trip.GetLease().GetLeaseGeneration(), trip.GetLease().GetVehicleGeneration());
		AICF_Stage3Diagnostics.Info("VEHICLE_REBOARD_SEAT_CONFLICT", details);
	}

	// Диагностика не вызывает EntryBlocked: чтение не меняет progress clock.
	protected void ReportReboarding(AICF_TransportTrip trip, string trigger)
	{
		if (!IsCurrent(trip)) return;
		array<BaseCompartmentSlot> seats = {};
		if (!AICF_VehiclePreset.CollectCaptured(trip.GetLease().GetVehicle(), trip.GetVehiclePreset(), seats)) return;
		Vehicle vehicle = trip.GetLease().GetVehicle();
		float speed = -1;
		Physics physics = vehicle.GetPhysics();
		if (physics) speed = physics.GetVelocity().Length();
		for (int index; index < seats.Count(); index++)
		{
			IEntity actor = trip.VehiclePresetMember(trip.GetVehiclePreset().m_aMembers[index]);
			if (!AICF_VehicleShortRecovery.IsCurrentMember(trip, actor)) continue;
			BaseCompartmentSlot seat = seats[index];
			CompartmentAccessComponent access = ChimeraCharacter.Cast(actor).GetCompartmentAccessComponent();
			AICF_VehicleBehaviorAction token = FindAction(trip, actor);
			string detail = string.Format("operation_id=%1 trigger=%2 member=%3 seat=%4 can_short_seat=%5 distance_m=%6 height_m=%7 speed_mps=%8 remaining_ms=%9",
				trip.GetOperationId(), trigger, AICF_GroupTrace.Entity(actor), AICF_VehiclePreset.SeatKey(seat),
				AICF_VehicleShortRecovery.CanSeat(trip, actor, seat), vector.Distance(actor.GetOrigin(), seat.GetPosition()),
				Math.AbsFloat(actor.GetOrigin()[1] - seat.GetPosition()[1]), speed, trip.GetBehaviorState().m_iReboardDeadline - System.GetTickCount());
			detail += string.Format(" vehicle=%1 vehicle_rpl=%2 member_rpl=%3 group_generation=%4 trip_generation=%5 lease_generation=%6 vehicle_generation=%7 preset_revision=%8 readonly=1",
				AICF_GroupTrace.Entity(vehicle), AICF_VehicleBehaviorBinding.ComponentRpl(vehicle), AICF_VehicleBehaviorBinding.ComponentRpl(actor),
				trip.GetGroupGeneration(), trip.GetTripGeneration(), trip.GetLease().GetLeaseGeneration(), trip.GetLease().GetVehicleGeneration(), trip.GetVehiclePresetRevision());
			detail += string.Format(" accessible=%1 locked=%2 reserved=%3 reserved_self=%4 occupant=%5 access=%6 in_vehicle=%7 can_move=%8 on_fire=%9",
				seat.IsCompartmentAccessible(), seat.IsGetInLockedFor(actor), seat.IsReserved(), seat.IsReservedBy(actor),
				AICF_GroupTrace.Entity(seat.GetOccupant()), access != null, AICF_GroupTrace.Entity(CompartmentAccessComponent.GetVehicleIn(actor)),
				SCR_AIVehicleUsability.VehicleCanMove(vehicle), SCR_AIVehicleUsability.VehicleIsOnFire(vehicle));
			if (access)
			{
				string actualSeat = "NONE";
				if (access.GetCompartment()) actualSeat = AICF_VehiclePreset.SeatKey(access.GetCompartment());
				detail += string.Format(" in_compartment=%1 getting_in=%2 getting_out=%3 switching=%4 actual_seat=%5",
					access.IsInCompartment(), access.IsGettingIn(), access.IsGettingOut(), access.IsSwitchingSeatsAnim(), actualSeat);
				detail += string.Format(" native_target_accessible=%1 native_can_get_in=%2",
					access.IsTargetVehicleAccessible(vehicle), access.CanGetInVehicle(vehicle));
			}
			if (token)
			{
				SCR_AIUtilityComponent utility = token.ResolveOwner();
				detail += string.Format(" entry=%1 owned=%2 short_attempts=%3 retries=%4 progress_age_ms=%5 progress_pos=%6 token_owner=%7",
					AICF_GroupTrace.Action(token.m_Entry), token.OwnsAction(utility, token.m_Entry), token.m_iShortAttempts,
					token.m_iRetries, System.GetTickCount() - token.m_iLastProgressAt, token.m_vLastProgressPosition, utility != null);
				if (utility) detail += " selected_action=" + AICF_GroupTrace.Action(utility.GetCurrentAction()) + " executed_action=" + AICF_GroupTrace.Action(utility.GetExecutedAction());
			}
			AICF_Stage3Diagnostics.Info("VEHICLE_REBOARD_ENTRY", detail);
		}
	}

	protected void EnsureEntry(AICF_TransportTrip trip, IEntity actor, BaseCompartmentSlot seat)
	{
		if (!AICF_VehicleShortRecovery.IsCurrentMember(trip, actor) || !seat || seat.GetOccupant() ||
			(seat.IsReserved() && !seat.IsReservedBy(actor))) return;
		AICF_VehicleBehaviorAction token = FindAction(trip, actor);
		if (token)
		{
			if (token.m_Seat != seat || !token.ResolveOwner()) return;
			bool blocked = token.EntryBlocked();
			if (blocked && token.m_iShortAttempts < 3 && AICF_VehicleShortRecovery.CanSeat(trip, actor, seat))
			{
				token.Retire();
				if (!AICF_VehicleShortRecovery.IsCurrentMember(trip, actor)) return;
				token.m_iShortAttempts++;
				AICF_VehicleShortRecovery.TrySeat(trip, actor, seat, "TACTICAL_NATIVE_ENTRY_BLOCKED");
			}
			else if (blocked && token.m_iRetries < 2 && token.CanRestartEntry())
			{
				// Реальный RUNNING stall за пределами short entry тоже получает
				// прежние два native retries; distant actor перемещается только AI.
				int progressAge = System.GetTickCount() - token.m_iLastProgressAt;
				token.Retire();
				if (!AICF_VehicleShortRecovery.IsCurrentMember(trip, actor) || !token.CanRestartEntry()) return;
				token.m_iRetries++;
				IssueEntry(trip, token, actor, seat);
				AICF_Stage3Diagnostics.Info("VEHICLE_REBOARD_NATIVE_RETRY", string.Format(
					"operation_id=%1 member=%2 seat=%3 native_retry=%4 maximum_retries=2 distance_m=%5 progress_age_ms=%6 remaining_ms=%7 physical_relocation=0",
					trip.GetOperationId(), AICF_GroupTrace.Entity(actor), AICF_VehiclePreset.SeatKey(seat), token.m_iRetries,
					vector.Distance(actor.GetOrigin(), seat.GetPosition()), progressAge, trip.GetBehaviorState().m_iReboardDeadline - System.GetTickCount()));
			}
			return;
		}
		token = new AICF_VehicleBehaviorAction(trip, actor, seat);
		trip.GetBehaviorState().m_aActions.Insert(token);
		IssueEntry(trip, token, actor, seat);
	}

	protected void IssueEntry(AICF_TransportTrip trip, AICF_VehicleBehaviorAction token, IEntity actor, BaseCompartmentSlot seat)
	{
		SCR_AIUtilityComponent utility = token.ResolveOwner();
		if (!utility || !AICF_VehicleShortRecovery.IsCurrentMember(trip, actor) ||
			seat.GetVehicle() != trip.GetLease().GetVehicle() || seat.GetOccupant() ||
			(seat.IsReserved() && !seat.IsReservedBy(actor))) return;
		seat.SetReserved(actor);
		if (!AICF_VehicleShortRecovery.IsCurrentMember(trip, actor) || !seat.IsReservedBy(actor) || seat.GetOccupant()) return;
		token.m_Entry = new SCR_AIGetInVehicle(utility, null, trip.GetLease().GetVehicle(), seat,
			SCR_AICompartmentHandling.CompartmentClassToType(seat.Type()),
			SCR_AIActionBase.PRIORITY_BEHAVIOR_GET_IN_VEHICLE, SCR_AIActionBase.PRIORITY_LEVEL_NORMAL);
		utility.AddAction(token.m_Entry);
		if (token.OwnsAction(utility, token.m_Entry))
		{
			token.m_vLastProgressPosition = actor.GetOrigin();
			token.m_iLastProgressAt = System.GetTickCount();
		}
	}

	void CancelEntries(AICF_TransportTrip trip)
	{
		if (!trip) return;
		AICF_VehicleBehaviorState state = trip.GetBehaviorState();
		state.m_ReboardSeatConflict = null;
		foreach (AICF_VehicleBehaviorAction token : state.m_aActions)
			if (token) token.Retire();
		state.m_aActions.Clear();
	}

	void Stop(AICF_TransportTrip trip)
	{
		CancelEntries(trip);
		if (trip && trip.GetBehaviorState().m_Parking)
		{
			trip.GetBehaviorState().m_Parking.Stop(trip);
			trip.GetBehaviorState().m_Parking = null;
		}
		AICF_VehicleBehaviorPolicy.Unregister(trip);
	}

	protected void Report(AICF_TransportTrip trip, string eventName, string reason, bool localCombat = false)
	{
		AICF_Stage3Diagnostics.Info("VEHICLE_BEHAVIOR_" + eventName, string.Format(
			"operation_id=%1 behavior=%2 return_distance_m=%3 vehicle=%4 deployed=%5 reboarding=%6 reason=[%7]",
			trip.GetOperationId(), trip.GetVehiclePreset().m_iBehavior, trip.GetVehiclePreset().m_iReturnDistance,
			AICF_GroupTrace.Entity(trip.GetLease().GetVehicle()), trip.GetBehaviorState().m_aDeployed.Count(), trip.GetBehaviorState().m_bReboarding, reason));
		if (eventName == "DEPLOYMENT_WAIT") AICF_VehicleCombatTrace.Snapshot(trip, localCombat);
	}

	protected bool CollectBehaviorSeats(AICF_TransportTrip trip, array<BaseCompartmentSlot> seats, string context)
	{
		if (AICF_VehiclePreset.CollectCaptured(trip.GetLease().GetVehicle(), trip.GetVehiclePreset(), seats)) return true;
		ReportTopologyMismatch(trip, context);
		return false;
	}

	// Только bounded raw readback в actual failure boundary. GetAllocatedCompartments
	// возвращает alias native array: его не очищаем, не сортируем и не дополняем.
	protected void ReportTopologyMismatch(AICF_TransportTrip trip, string context)
	{
		if (!IsCurrent(trip)) return;
		Vehicle vehicle = trip.GetLease().GetVehicle();
		SCR_AIGroup group = trip.GetAssignment().GetGroup();
		AICF_VehiclePreset preset = trip.GetVehiclePreset();
		array<BaseCompartmentSlot> allocated;
		group.GetAllocatedCompartments(allocated);
		array<BaseCompartmentSlot> accessible = {};
		AICF_VehiclePreset.Collect(vehicle, accessible);
		string expected;
		for (int index; index < preset.m_aSeats.Count() && index < AICF_VehiclePreset.MAX_SEATS; index++)
			expected += string.Format("%1:%2,", index, preset.m_aSeats[index]);
		SCR_AIGroupUtilityComponent utility = group.GetGroupUtilityComponent();
		SCR_AIGetInActivity getIn;
		if (utility) getIn = SCR_AIGetInActivity.Cast(utility.GetCurrentAction());
		IEntity getInVehicle;
		if (getIn && getIn.m_Vehicle) getInVehicle = getIn.m_Vehicle.m_Value;
		int allocatedCount;
		if (allocated) allocatedCount = allocated.Count();
		AICF_Stage3Diagnostics.Warning("VEHICLE_BEHAVIOR_TOPOLOGY_MISMATCH", string.Format(
			"operation_id=%1 context=%2 vehicle=%3 expected_count=%4 accessible_count=%5 allocated_count=%6 current_get_in_vehicle=%7 expected=[%8] readonly=1",
			trip.GetOperationId(), context, AICF_GroupTrace.Entity(vehicle), preset.m_aSeats.Count(), accessible.Count(), allocatedCount,
			AICF_GroupTrace.Entity(getInVehicle), expected));
		array<IEntity> hierarchy = {vehicle};
		int emitted;
		for (int node; node < hierarchy.Count() && node < 256; node++)
		{
			IEntity entity = hierarchy[node];
			if (!entity) continue;
			IEntity child = entity.GetChildren();
			while (child && hierarchy.Count() < 256) { hierarchy.Insert(child); child = child.GetSibling(); }
			BaseCompartmentManagerComponent manager = BaseCompartmentManagerComponent.Cast(entity.FindComponent(BaseCompartmentManagerComponent));
			if (!manager) continue;
			array<BaseCompartmentSlot> raw = {};
			manager.GetCompartments(raw);
			AICF_Stage3Diagnostics.Warning("VEHICLE_BEHAVIOR_TOPOLOGY_MANAGER", string.Format(
				"operation_id=%1 hierarchy_node=%2 owner=%3 raw_count=%4 remaining_rows=%5 readonly=1",
				trip.GetOperationId(), node, AICF_GroupTrace.Entity(entity), raw.Count(), AICF_VehiclePreset.MAX_SEATS - emitted));
			foreach (BaseCompartmentSlot seat : raw)
			{
				if (emitted >= AICF_VehiclePreset.MAX_SEATS) break;
				emitted++;
				if (!seat) continue;
				string key = AICF_VehiclePreset.SeatKey(seat);
				int captured = preset.m_aSeats.Find(key);
				IEntity member;
				if (captured >= 0) member = trip.VehiclePresetMember(preset.m_aMembers[captured]);
				bool nativeAllocated = allocated && allocated.Contains(seat);
				string details = string.Format("operation_id=%1 key=%2 expected_index=%3 owner=%4 owner_rpl=%5 exact_vehicle=%6 accessible=%7 occupant=%8",
					trip.GetOperationId(), key, captured, AICF_GroupTrace.Entity(seat.GetOwner()), AICF_VehicleBehaviorBinding.ComponentRpl(seat.GetOwner()),
					seat.GetVehicle() == vehicle, seat.IsCompartmentAccessible(), AICF_GroupTrace.Entity(seat.GetOccupant()));
				details += string.Format(" allocated_to_group=%1 reserved=%2 reserved_by_expected=%3 get_in_locked=%4 live_manager_id=%5 native_slot_id=%6 seat_type=%7 readonly=1",
					nativeAllocated, seat.IsReserved(), member && seat.IsReservedBy(member), seat.IsGetInLocked(), seat.GetCompartmentMgrID(), seat.GetCompartmentSlotID(), seat.GetType());
				AICF_Stage3Diagnostics.Warning("VEHICLE_BEHAVIOR_TOPOLOGY_SEAT", details);
			}
		}
	}
}
