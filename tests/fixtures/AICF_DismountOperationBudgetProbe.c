// Opt-in controlled fault injection; copied only to the isolated VehicleScenario
// stage. It calls real force-exit boundaries (never primes state counters), then
// places one fully disembarked exact AI in the hull to exercise physical recovery.
class AICF_VSDismountBudgetProbe
{
	static ref AICF_VSDismountBudgetProbe s_Current;
	AICF_TransportTrip m_Trip;
	SCR_AIGroup m_Group;
	Vehicle m_Vehicle;
	EntityID m_GroupId;
	EntityID m_VehicleId;
	string m_sOperation;
	string m_sVehicleRpl;
	int m_iGroupGeneration;
	int m_iTripGeneration;
	int m_iLeaseGeneration;
	int m_iVehicleGeneration;
	int m_iNormalDeadlineMs;
	int m_iTerminalDeadlineMs;
	int m_iNativeCalls;
	int m_iAcceptedCalls;
	int m_iProvenBatches;
	bool m_bGhostInjected;
	bool m_bPhysicalMoved;
	bool m_bRelocationPending;
	vector m_vRelocationStart;
	IEntity m_Ghost;
	ref array<EntityID> m_aMemberIds = {};

	static bool Enabled()
	{
		string value;
		return System.GetCLIParam("aicfVehicleScenarioDismountBudget", value) && value == "1";
	}

	static void Bind(AICF_TransportTrip trip, array<EntityID> memberIds)
	{
		if (!Enabled() || s_Current || !Replication.IsServer() || !trip || !trip.IsVehiclePresetCurrent()) return;
		if (!trip.GetLease() || !trip.GetLease().GetVehicle() || memberIds.Count() != 5) return;
		AICF_VSDismountBudgetProbe probe = new AICF_VSDismountBudgetProbe();
		probe.m_Trip = trip;
		probe.m_Group = trip.GetAssignment().GetGroup();
		probe.m_GroupId = probe.m_Group.GetID();
		probe.m_Vehicle = trip.GetLease().GetVehicle();
		probe.m_VehicleId = probe.m_Vehicle.GetID();
		probe.m_sVehicleRpl = Replication.FindItemId(probe.m_Vehicle).ToString();
		probe.m_sOperation = trip.GetOperationId();
		probe.m_iGroupGeneration = trip.GetGroupGeneration();
		probe.m_iTripGeneration = trip.GetTripGeneration();
		probe.m_iLeaseGeneration = trip.GetLease().GetLeaseGeneration();
		probe.m_iVehicleGeneration = trip.GetLease().GetVehicleGeneration();
		foreach (EntityID id : memberIds) probe.m_aMemberIds.Insert(id);
		probe.m_Ghost = trip.VehiclePresetMember(4);
		s_Current = probe;
		probe.Log("BOUND", "fixture_accelerated_force_batches=1 direct_counter_priming=0");
	}

	bool Matches(AICF_TransportTrip trip)
	{
		if (!Replication.IsServer() || !trip || trip != m_Trip || !trip.IsVehiclePresetCurrent()) return false;
		if (trip.GetOperationId() != m_sOperation || trip.GetGroupGeneration() != m_iGroupGeneration || trip.GetTripGeneration() != m_iTripGeneration) return false;
		if (!m_Group || m_Group.GetID() != m_GroupId || trip.GetAssignment().GetGroup() != m_Group) return false;
		if (!m_Vehicle || m_Vehicle.GetID() != m_VehicleId || !trip.GetLease() || trip.GetLease().GetVehicle() != m_Vehicle) return false;
		if (Replication.FindItemId(m_Vehicle).ToString() != m_sVehicleRpl) return false;
		if (trip.GetLease().GetLeaseGeneration() != m_iLeaseGeneration || trip.GetLease().GetVehicleGeneration() != m_iVehicleGeneration) return false;
		for (int position; position < m_aMemberIds.Count(); position++)
		{
			IEntity member = trip.VehiclePresetMember(position);
			if (!member || member.GetID() != m_aMemberIds[position] || !AICF_GroupRuntime.IsAliveCharacter(member)) return false;
			if (!AICF_VehicleBoardingMutationFence.IsAuthoritativeAIEntity(member)) return false;
		}
		return true;
	}

	bool IsDismountCurrent(AICF_TransportTrip trip)
	{
		if (!Matches(trip) || trip.GetPhase() != AICF_ETransportTripPhase.DISMOUNT) return false;
		AICF_VehicleDismountState state = trip.GetDismountState();
		if (m_iNormalDeadlineMs == 0)
		{
			m_iNormalDeadlineMs = state.GetNormalDeadlineMs();
			m_iTerminalDeadlineMs = state.GetTerminalDeadlineMs();
		}
		return state.GetNormalDeadlineMs() == m_iNormalDeadlineMs && state.GetTerminalDeadlineMs() == m_iTerminalDeadlineMs;
	}

	bool ForceThisMember(ChimeraCharacter character, int attempt)
	{
		int position = m_aMemberIds.Find(character.GetID());
		if (attempt == 1) return position == 0;
		if (attempt == 2) return position == 1;
		return attempt == 3 && position >= 2;
	}

	void Log(string eventName, string details)
	{
		Print(string.Format("[AICF][VEHICLE_SCENARIO_DISMOUNT_BUDGET] scenario=ural5 operation_id=%1 event=%2 normal_deadline_ms=%3 terminal_deadline_ms=%4 %5",
			m_sOperation, eventName, m_iNormalDeadlineMs, m_iTerminalDeadlineMs, details));
	}

	bool Passed()
	{
		return m_iProvenBatches == 3 && m_iNativeCalls >= 3 && m_bGhostInjected && m_bPhysicalMoved;
	}
}

modded class AICF_VehicleDismountFlow
{
	override AICF_TripOutcome ProcessNormalDismount(AICF_TransportTrip trip, string causationId)
	{
		AICF_VSDismountBudgetProbe probe = AICF_VSDismountBudgetProbe.s_Current;
		if (probe && probe.IsDismountCurrent(trip))
		{
			AICF_VehicleDismountState state = trip.GetDismountState();
			// Accelerate only the schedule of the three existing real mutations.
			// Partition1/1/3 keeps an actual logical occupant for each batch.
			for (int batch; batch < 3 && state.GetForceExitAttempts() < 3; batch++)
			{
				AICF_DismountClearanceSample before = InspectCurrentManagedClearance(trip);
				if (!probe.IsDismountCurrent(trip) || before.m_iLogicalOccupants <= 0) break;
				int oldAttempts = state.GetForceExitAttempts();
				int oldCalls = probe.m_iNativeCalls;
				int oldAccepted = probe.m_iAcceptedCalls;
				super.TryForceExactManagedExit(trip, before);
				int charged = state.GetForceExitAttempts() - oldAttempts;
				int nativeCalls = probe.m_iNativeCalls - oldCalls;
				if (charged == 1 && nativeCalls > 0 && probe.m_iAcceptedCalls > oldAccepted) probe.m_iProvenBatches++;
				probe.Log("FORCE_BATCH", string.Format("charged=%1 force_exit_attempts=%2 native_calls=%3 accepted_calls=%4 logical_before=%5 physical_relocation_attempts=%6",
					charged, state.GetForceExitAttempts(), nativeCalls, probe.m_iAcceptedCalls - oldAccepted,
					before.m_iLogicalOccupants, state.GetPhysicalRelocationAttempts()));
				if (charged != 1 || nativeCalls <= 0) break;
			}
			AICF_DismountClearanceSample egress = InspectCurrentManagedClearance(trip);
			if (probe.m_iProvenBatches == 3 && !probe.m_bGhostInjected && egress.m_iLogicalOccupants == 0 && egress.m_iTransitions == 0)
			{
				ChimeraCharacter ghost = ChimeraCharacter.Cast(probe.m_Ghost);
				Vehicle vehicle = trip.GetLease().GetVehicle();
				if (ghost && probe.IsDismountCurrent(trip) && !CompartmentAccessComponent.GetVehicleIn(ghost))
				{
					vector low, high;
					vehicle.GetBounds(low, high);
					vector transform[4];
					ghost.GetWorldTransform(transform);
					vector vehicleTransform[4];
					vehicle.GetWorldTransform(vehicleTransform);
					vector localCenter = (low + high) * 0.5;
					transform[3] = localCenter.Multiply4(vehicleTransform);
					ghost.Teleport(transform);
					AICF_DismountClearanceSample injected = InspectCurrentManagedClearance(trip);
					probe.m_bGhostInjected = injected.m_iLogicalOccupants == 0 && injected.m_iTransitions == 0 && IsPhysicalOnlyBlocker(ghost, vehicle, low, high);
					probe.Log("PHYSICAL_GHOST", string.Format("force_exit_attempts=%1 logical_occupants=%2 transitions=%3 inside_bounds=%4 exact_ghost=%5 native_calls=%6 physical_relocation_attempts=%7",
						state.GetForceExitAttempts(), injected.m_iLogicalOccupants, injected.m_iTransitions, injected.m_iInsideBounds,
						probe.m_bGhostInjected, probe.m_iNativeCalls, state.GetPhysicalRelocationAttempts()));
				}
			}
			// Teleport/native transform readback may become visible next tick.
			// Keep the actual charged operation receipt; require exact outside
			// state and physical displacement before declaring recovery proven.
			if (probe.m_bRelocationPending && probe.IsDismountCurrent(trip))
			{
				ChimeraCharacter moved = ChimeraCharacter.Cast(probe.m_Ghost);
				CompartmentAccessComponent access = moved.GetCompartmentAccessComponent();
				AICF_DismountClearanceSample observed = InspectCurrentManagedClearance(trip);
				bool nativeOutside = !moved.IsInVehicle() && access && !access.IsInCompartment();
				nativeOutside = nativeOutside && !CompartmentAccessComponent.GetVehicleIn(moved);
				bool noTransition = access && !access.IsGettingIn() && !access.IsGettingOut() && !access.IsSwitchingSeatsAnim();
				vector low, high;
				probe.m_Vehicle.GetBounds(low, high);
				float displacement = vector.Distance(moved.GetOrigin(), probe.m_vRelocationStart);
				bool physicallyClear = displacement > 1 && !IsPhysicalOnlyBlocker(moved, probe.m_Vehicle, low, high);
				if (nativeOutside && noTransition && physicallyClear && observed.m_iLogicalOccupants == 0 && observed.m_iTransitions == 0)
				{
					probe.m_bPhysicalMoved = true;
					probe.m_bRelocationPending = false;
					probe.Log("NORMAL_RELOCATION", string.Format("force_exit_attempts=%1 physical_relocation_attempts=%2 ghost_displacement_m=%3 ghost_moved_clear=1 unchanged_deadlines=1 terminal_only=0 native_calls=%4 delayed_exact_readback=1",
						state.GetForceExitAttempts(), state.GetPhysicalRelocationAttempts(), displacement, probe.m_iNativeCalls));
				}
			}
		}
		return super.ProcessNormalDismount(trip, causationId);
	}

	override protected bool ForceOneManagedMemberOut(AICF_TransportTrip trip, ChimeraCharacter character, int forceAttempt)
	{
		AICF_VSDismountBudgetProbe probe = AICF_VSDismountBudgetProbe.s_Current;
		if (!probe || !probe.IsDismountCurrent(trip)) return super.ForceOneManagedMemberOut(trip, character, forceAttempt);
		if (!probe.ForceThisMember(character, forceAttempt)) return false;
		probe.m_iNativeCalls++;
		bool accepted = super.ForceOneManagedMemberOut(trip, character, forceAttempt);
		if (accepted) probe.m_iAcceptedCalls++;
		return accepted;
	}

	override protected void TryRelocateExactManagedMembers(AICF_TransportTrip trip, AICF_DismountClearanceSample sample, bool terminalOnly)
	{
		AICF_VSDismountBudgetProbe probe = AICF_VSDismountBudgetProbe.s_Current;
		bool observing = probe && probe.IsDismountCurrent(trip) && probe.m_bGhostInjected && !terminalOnly;
		int oldAttempts;
		vector oldPosition;
		if (observing)
		{
			oldAttempts = trip.GetDismountState().GetPhysicalRelocationAttempts();
			oldPosition = probe.m_Ghost.GetOrigin();
		}
		super.TryRelocateExactManagedMembers(trip, sample, terminalOnly);
		if (!observing || !probe.IsDismountCurrent(trip)) return;
		AICF_VehicleDismountState state = trip.GetDismountState();
		if (state.GetPhysicalRelocationAttempts() <= oldAttempts) return;
		probe.m_vRelocationStart = oldPosition;
		probe.m_bRelocationPending = true;
		probe.Log("RELOCATION_COMMITTED", string.Format("force_exit_attempts=%1 physical_relocation_attempts=%2 unchanged_deadlines=1 terminal_only=0 native_calls=%3 physical_success_not_yet_proven=1",
			state.GetForceExitAttempts(), state.GetPhysicalRelocationAttempts(), probe.m_iNativeCalls));
	}
}

modded class AICF_MatchController
{
	override protected void Update()
	{
		super.Update();
		if (!AICF_VSDismountBudgetProbe.Enabled() || m_bVSDone || m_sVSCase != "ural5") return;
		if (m_iVSStep >= 1 && m_VSVehicles && m_VSState)
			AICF_VSDismountBudgetProbe.Bind(m_VSVehicles.GetTrip(m_VSState.GetSlot(0)), m_aVSMembers);
	}

	override protected void AICF_VSFinish()
	{
		if (AICF_VSDismountBudgetProbe.Enabled())
		{
			AICF_VSDismountBudgetProbe probe = AICF_VSDismountBudgetProbe.s_Current;
			AICF_VSCheck("DISMOUNT_OPERATION_BUDGET_CAUSAL", probe && probe.Passed());
			AICF_VSDismountBudgetProbe.s_Current = null;
		}
		super.AICF_VSFinish();
	}
}
