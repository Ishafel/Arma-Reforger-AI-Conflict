// BehaviorProbe/Settings scaffold; explicit controlled economy cases, no war proof.
class AICF_PresetEconomyProbe
{
	static SCR_AIGroup s_Group;
	static AICF_TransportTrip s_Trip;
	static bool s_Scope;
	static bool s_StockSet;
	static int s_Quote;
	static int s_StockBefore;
	static int s_StockAfter;
	static int s_LeaseCalls;
	static int s_Leases;
	static int s_EmptyReleases;
	static int s_CapPeak;
	static int s_SpawnCalls;
	static int s_MainWorldCars;
	static int s_PurchaseCalls;
	static int s_Paid;
	static int s_Operations;
	static int s_Checks;
	static int s_Failures;
	static string s_Operation;
	static SCR_CampaignMilitaryBaseComponent s_Base;
	static int s_DrainLeaseGeneration;
	static int s_DrainAt;
	static int s_DeclineAt;
	static int s_LastEmptyReleaseAt;
	static int s_DeclineOwnCap;
	static bool s_DeclineSameDrainedLease;
	static AICF_VehicleLease s_DrainedLease;
	static string s_DeclineDetails;
	static AICF_VehiclePresetEconomyDecision s_DeclineReceipt;
	static AICF_TransportTrip s_DeclineTrip;
	static AICF_VehicleRequestState s_DeclineRequest;
	static int s_DeclineGroupGeneration;
	static int s_DeclineTripGeneration;
	static int s_DeclineRequestGeneration;
	static int s_DeclinePresetRevision;
	static string s_DeclineOperation;
	static bool TerminalReceiptSame(bool capturedCurrent, bool sameTrip, bool sameReceipt, bool sameDetails, bool sameRequest,
		int group, int currentGroup, int trip, int currentTrip, int request, int currentRequest,
		int preset, int currentPreset, string operation, string currentOperation)
	{
		return capturedCurrent && sameTrip && sameReceipt && sameDetails && sameRequest && group == currentGroup && trip == currentTrip &&
			request > 0 && currentRequest == 0 && preset == currentPreset && operation == currentOperation;
	}
	static bool SameObservedTrip(AICF_TransportTrip original, AICF_TransportTrip current)
	{
		return !original || current == original;
	}
	static bool FootContextSame(bool ownerCurrent, bool tripCurrent, bool groupCurrent,
		int generation, int currentGeneration, int intent, int currentIntent,
		int kind, int currentKind, vector target, vector currentTarget, bool sameBase)
	{
		if (!ownerCurrent || !tripCurrent || !groupCurrent || !sameBase) return false;
		return generation == currentGeneration && intent == currentIntent && kind == currentKind && target == currentTarget;
	}
	static bool FootReconcileDue(bool eligible, int nowMs, int nextAtMs, int intervalMs)
	{
		return eligible && intervalMs > 0 && nowMs >= nextAtMs;
	}
	static bool Enabled()
	{
		string flag;
		return System.GetCLIParam("aicfPresetEconomyProbe", flag) && flag == "1";
	}
	static bool Affordable()
	{
		string mode;
		return System.GetCLIParam("aicfPresetEconomyCase", mode) && mode == "AFFORDABLE";
	}
	static bool DrainWindow()
	{
		string mode;
		return System.GetCLIParam("aicfPresetEconomyCase", mode) && mode == "DRAIN_AFTER_RESERVE";
	}
	static bool Owns(AICF_TransportTrip trip)
	{
		return Enabled() && s_Scope && trip && trip.GetAssignment() && trip.GetAssignment().GetGroup() == s_Group;
	}
	static void Check(string name, bool passed, string details = "")
	{
		s_Checks++;
		if (!passed) s_Failures++;
		Print(string.Format("[AICF][PRESET_ECONOMY_CHECK] name=%1 passed=%2 %3", name, passed, details));
	}
}

modded class AICF_TransportTripRegistry
{
	override AICF_TransportTrip Create(AICF_StrategicAssignmentSnapshot assignment, int nowMs,
		int absoluteDeadlineMs, string causationId)
	{
		AICF_TransportTrip trip = super.Create(assignment, nowMs, absoluteDeadlineMs, causationId);
		if (AICF_PresetEconomyProbe.Owns(trip))
		{
			AICF_PresetEconomyProbe.s_Operations++;
			AICF_PresetEconomyProbe.s_Trip = trip;
			AICF_PresetEconomyProbe.s_Operation = trip.GetOperationId();
			Print(string.Format("[AICF][PRESET_ECONOMY_TRIP_CREATED] operation_id=%1 group_generation=%2 trip_generation=%3 created=%4",
				trip.GetOperationId(), trip.GetGroupGeneration(), trip.GetTripGeneration(), AICF_PresetEconomyProbe.s_Operations));
		}
		return trip;
	}
}

modded class AICF_EconomySystem
{
	int AICF_EconomyProbeObligations(SCR_CampaignMilitaryBaseComponent base) { return LogisticsObligations(base); }
	override bool PurchaseVehiclePreset(AICF_TransportTrip trip, SCR_CampaignFaction faction,
		SCR_CampaignMilitaryBaseComponent base, ResourceName prefab, out int paid)
	{
		bool probeOwned = AICF_PresetEconomyProbe.Owns(trip);
		if (probeOwned) AICF_PresetEconomyProbe.s_PurchaseCalls++;
		bool result = super.PurchaseVehiclePreset(trip, faction, base, prefab, paid);
		if (probeOwned) AICF_PresetEconomyProbe.s_Paid += paid;
		return result;
	}
}

modded class AICF_FactionFleet
{
	override bool TryReserveLease(AICF_StrategicAssignmentSnapshot assignment, int tripGeneration, out AICF_VehicleLease lease)
	{
		bool probeOwned = AICF_PresetEconomyProbe.Enabled() && AICF_PresetEconomyProbe.s_Scope &&
			assignment && assignment.GetGroup() == AICF_PresetEconomyProbe.s_Group;
		if (probeOwned) AICF_PresetEconomyProbe.s_LeaseCalls++;
		bool result = super.TryReserveLease(assignment, tripGeneration, lease);
		if (probeOwned && result)
		{
			AICF_PresetEconomyProbe.s_Leases++;
			int active;
			foreach (AICF_VehicleLease current : m_aLeases)
				if (current && current.MatchesTripIdentity(assignment.GetFactionKey(), assignment.GetSlotId(), assignment.GetGroupGeneration(), tripGeneration) && current.IsCapActive()) active++;
			AICF_PresetEconomyProbe.s_CapPeak = Math.Max(AICF_PresetEconomyProbe.s_CapPeak, active);
			Print(string.Format("[AICF][PRESET_ECONOMY_LEASE_RESERVED] operation_id=%1 lease_generation=%2 current_cap=%3 has_asset=%4",
				AICF_PresetEconomyProbe.s_Operation, lease.GetLeaseGeneration(), active, lease.HasPhysicalAsset()));
			if (AICF_PresetEconomyProbe.DrainWindow() && !AICF_PresetEconomyProbe.s_DrainAt)
			{
				SCR_CampaignMilitaryBaseComponent base = AICF_PresetEconomyProbe.s_Base;
				int before = -1;
				if (base) { before = base.GetSupplies(); base.AddSupplies(-before); }
				AICF_PresetEconomyProbe.s_DrainAt = System.GetTickCount();
				AICF_PresetEconomyProbe.s_DrainedLease = lease;
				AICF_PresetEconomyProbe.s_DrainLeaseGeneration = lease.GetLeaseGeneration();
				AICF_PresetEconomyProbe.Check("AFTER_NATIVE_RESERVE_STOCK_DRAIN", base && before > 0 && base.GetSupplies() == 0 && active == 1);
				Print(string.Format("[AICF][PRESET_ECONOMY_STOCK_DRAIN] operation_id=%1 lease_generation=%2 before=%3 after=0 tick_ms=%4",
					AICF_PresetEconomyProbe.s_Operation, lease.GetLeaseGeneration(), before, AICF_PresetEconomyProbe.s_DrainAt));
			}
		}
		return result;
	}
	override bool ReleaseEmptyReservation(AICF_VehicleLease lease)
	{
		AICF_TransportTrip trip = AICF_PresetEconomyProbe.s_Trip;
		bool probeOwned = AICF_PresetEconomyProbe.Owns(trip) && lease &&
			lease.MatchesTripIdentity(trip.GetFactionKey(), trip.GetSlotId(), trip.GetGroupGeneration(), trip.GetTripGeneration());
		bool empty = lease && !lease.HasPhysicalAsset();
		bool result = super.ReleaseEmptyReservation(lease);
		if (probeOwned && result)
		{
			bool released = empty && !lease.IsCapActive() && !m_aLeases.Contains(lease);
			if (released) AICF_PresetEconomyProbe.s_EmptyReleases++;
			AICF_PresetEconomyProbe.s_LastEmptyReleaseAt = System.GetTickCount();
			AICF_PresetEconomyProbe.Check("EXACT_EMPTY_LEASE_RELEASE", released);
			Print(string.Format("[AICF][PRESET_ECONOMY_EMPTY_RELEASE] operation_id=%1 lease_generation=%2 empty=%3 cap_active=%4 registered=%5",
				trip.GetOperationId(), lease.GetLeaseGeneration(), empty, lease.IsCapActive(), m_aLeases.Contains(lease)));
		}
		return result;
	}
	void AICF_EconomyProbeCount(AICF_TransportTrip trip, out int active, out int physical)
	{
		active = 0; physical = 0;
		foreach (AICF_VehicleLease current : m_aLeases)
		{
			if (!current || !current.MatchesTripIdentity(trip.GetFactionKey(), trip.GetSlotId(), trip.GetGroupGeneration(), trip.GetTripGeneration())) continue;
			if (current.IsCapActive()) active++;
			if (current.HasPhysicalAsset()) physical++;
		}
	}
}

modded class AICF_VehicleSpawner
{
	override protected Vehicle SpawnSelectedPrefab(ResourceName prefab, vector position, EntitySpawnParams spawnParams = null, AICF_AIQuotaVehicleCreationGuard quotaGuard = null)
	{
		bool probeOwned = AICF_PresetEconomyProbe.Enabled() && AICF_PresetEconomyProbe.s_Scope;
		if (probeOwned) AICF_PresetEconomyProbe.s_SpawnCalls++;
		Vehicle vehicle = super.SpawnSelectedPrefab(prefab, position, spawnParams, quotaGuard);
		if (probeOwned && vehicle && vehicle.GetWorld() == GetGame().GetWorld()) AICF_PresetEconomyProbe.s_MainWorldCars++;
		return vehicle;
	}
}

modded class AICF_VehicleAcquisitionFlow
{
	override protected AICF_TripOutcome ProcessSpawnCommit(AICF_TransportTrip trip, AICF_FactionFleet fleet,
		SCR_CampaignFaction faction, AICF_VehicleRequestState requestState, int nowMs)
	{
		bool probeOwned = AICF_PresetEconomyProbe.Owns(trip);
		SCR_CampaignMilitaryBaseComponent base;
		if (probeOwned)
		{
			AICF_PresetEconomyProbe.s_Trip = trip;
			AICF_VehicleSpawnPlan plan = requestState.GetSpawnPlan();
			if (plan && plan.GetSelection()) base = plan.GetSelection().m_Base;
			if (base && !AICF_PresetEconomyProbe.s_StockSet)
			{
				AICF_EconomySystem economy = AICF_MatchController.GetActiveController().VehiclePresetEconomy();
				AICF_VehiclePresetCatalog catalog = new AICF_VehiclePresetCatalog(faction);
				int quote = catalog.Cost(trip.GetVehiclePreset().m_sPrefab);
				int obligations = economy.AICF_EconomyProbeObligations(base);
				int wanted;
				if (AICF_PresetEconomyProbe.Affordable() || AICF_PresetEconomyProbe.DrainWindow()) wanted = quote + obligations + 200;
				base.AddSupplies(wanted - base.GetSupplies());
				AICF_PresetEconomyProbe.s_Quote = quote;
				AICF_PresetEconomyProbe.s_StockSet = true;
				AICF_PresetEconomyProbe.s_Base = base;
				AICF_PresetEconomyProbe.Check("EXACT_CONTROLLED_STOCK", quote > 0 && obligations >= 0 && base.GetSupplies() == wanted,
					string.Format("quote=%1 obligations=%2 stock=%3 affordable=%4 base=%5", quote, obligations, base.GetSupplies(), AICF_PresetEconomyProbe.Affordable(), AICF_Stage1Diagnostics.BaseKey(base)));
			}
			if (base) AICF_PresetEconomyProbe.s_StockBefore = base.GetSupplies();
		}
		AICF_TripOutcome outcome = super.ProcessSpawnCommit(trip, fleet, faction, requestState, nowMs);
		if (probeOwned && base) AICF_PresetEconomyProbe.s_StockAfter = base.GetSupplies();
		if (probeOwned && trip.GetVehiclePresetEconomyDecline() && !AICF_PresetEconomyProbe.s_DeclineAt)
		{
			AICF_VehiclePresetEconomyDecision decision = trip.GetVehiclePresetEconomyDecline();
			AICF_PresetEconomyProbe.Check("LIVE_TYPED_RECEIPT_BEFORE_HANDOFF", decision.MatchesIdentity(trip));
			if (!decision.MatchesIdentity(trip)) return outcome;
			AICF_PresetEconomyProbe.s_DeclineReceipt = decision;
			AICF_PresetEconomyProbe.s_DeclineTrip = trip;
			AICF_PresetEconomyProbe.s_DeclineRequest = trip.GetRequestState();
			AICF_PresetEconomyProbe.s_DeclineGroupGeneration = trip.GetGroupGeneration();
			AICF_PresetEconomyProbe.s_DeclineTripGeneration = trip.GetTripGeneration();
			AICF_PresetEconomyProbe.s_DeclineRequestGeneration = trip.GetRequestState().GetRequestGeneration();
			AICF_PresetEconomyProbe.s_DeclinePresetRevision = trip.GetVehiclePresetRevision();
			AICF_PresetEconomyProbe.s_DeclineOperation = trip.GetOperationId();
			AICF_PresetEconomyProbe.s_DeclineAt = System.GetTickCount();
			AICF_PresetEconomyProbe.s_DeclineDetails = trip.GetVehiclePresetEconomyDecline().Details();
			int cap; int physical;
			fleet.AICF_EconomyProbeCount(trip, cap, physical);
			AICF_PresetEconomyProbe.s_DeclineOwnCap = cap;
			AICF_PresetEconomyProbe.s_DeclineSameDrainedLease = trip.GetLease() && trip.GetLease() == AICF_PresetEconomyProbe.s_DrainedLease;
			Print("[AICF][PRESET_ECONOMY_DECLINE_RETURNED] " + AICF_PresetEconomyProbe.s_DeclineDetails +
				string.Format(" own_cap_before=%1 physical_before=%2 tick_ms=%3", cap, physical, AICF_PresetEconomyProbe.s_DeclineAt));
		}
		return outcome;
	}
}

modded class AICF_VehicleCoordinator
{
	bool AICF_EconomyProbeHasFailure() { return m_Acceptance.HasFailureLatch(); }
	void AICF_EconomyProbeCount(AICF_TransportTrip trip, out int active, out int physical)
	{
		active = -1; physical = -1;
		AICF_FactionFleet fleet = m_Fleets.FindFleet(trip.GetFactionKey());
		if (fleet) fleet.AICF_EconomyProbeCount(trip, active, physical);
	}
}

modded class AICF_MatchController
{
	protected int m_iEconomyProbeStarted;
	protected int m_iEconomyProbeTerminalAt;
	protected bool m_bEconomyProbeFinished;
	protected bool m_bEconomyProbeGoal;
	protected vector m_vEconomyProbeWalkStart;
	protected IEntity m_EconomyProbeLeader;
	protected EntityID m_EconomyProbeLeaderId;
	protected AICF_TransportTrip m_EconomyProbeOriginal;
	protected int m_iEconomyFootNextReconcile;
	protected int m_iEconomyFootGeneration;
	protected int m_iEconomyFootIntent;
	protected AICF_EOrderTargetKind m_EconomyFootKind;
	protected vector m_vEconomyFootTarget;
	protected SCR_CampaignMilitaryBaseComponent m_EconomyFootBase;

	protected bool AICF_EconomyFootContextCurrent(AICF_GroupSlot slot)
	{
		bool owner = AICF_MatchController.GetActiveController() == this && m_bStarted && !m_bStopped;
		bool sameGroup = m_VBState && m_VBState.GetSlot(0) == slot && slot && slot.GetGroup() == m_VBGroup &&
			m_VBGroup && m_VBGroup.GetID() == m_VBGroupId && AICF_VehicleBoardingMutationFence.IsAuthoritativeReplicatedEntity(m_VBGroup);
		if (!owner || !sameGroup || !m_EconomyProbeOriginal || !m_EconomyProbeOriginal.IsTerminal()) return false;
		return AICF_PresetEconomyProbe.FootContextSame(owner, m_VBCoordinator.GetTrip(slot) == m_EconomyProbeOriginal, sameGroup,
			m_iEconomyFootGeneration, slot.GetSpawnGeneration(), m_iEconomyFootIntent, slot.GetStrategicIntentRevision(),
			m_EconomyFootKind, slot.GetTargetKind(), m_vEconomyFootTarget, slot.GetTargetPosition(), m_EconomyFootBase == slot.GetTargetBase());
	}
	protected bool AICF_EconomyMaintainFootOrder(AICF_GroupSlot slot, int nowMs)
	{
		if (!m_iEconomyProbeTerminalAt) return true;
		if (!AICF_EconomyFootContextCurrent(slot)) { AICF_PresetEconomyProbe.Check("UNCHANGED_FOOT_CONTEXT", false); return false; }
		int interval = m_Config.GetCommanderIntervalMs();
		if (!AICF_PresetEconomyProbe.FootReconcileDue(true, nowMs, m_iEconomyFootNextReconcile, interval)) return true;
		m_iEconomyFootNextReconcile = nowMs + interval;
		// The private VBState is absent from the normal MatchController faction
		// loop. Use its actual planning owner, preserving the committed intent.
		m_OrderPlanner.ReconcileStrategicOrder(slot, m_VBFaction, m_ObjectiveGraph, m_TargetSelector,
			"CONTROLLED_ECONOMY_FOOT_CONTINUATION", m_Config.GetRoleMinimumDwellMs(), m_Config.GetCommanderIntervalMs());
		if (!AICF_EconomyFootContextCurrent(slot)) { AICF_PresetEconomyProbe.Check("UNCHANGED_FOOT_CONTEXT", false); return false; }
		return true;
	}

	override protected void Update()
	{
		super.Update();
		if (!AICF_PresetEconomyProbe.Enabled() || !Replication.IsServer() || !m_bRosterReady || m_bEconomyProbeFinished) return;
		int now = System.GetTickCount();
		m_sVBScenario = "transport";
		if (!m_iEconomyProbeStarted)
		{
			m_iEconomyProbeStarted = now;
			if (!AICF_VBInitialize()) { AICF_PresetEconomyProbe.Check("SETUP", false); AICF_EconomyProbeFinish(); }
			return;
		}
		if (now - m_iEconomyProbeStarted > 180000 || m_iVBFailures || AICF_PresetEconomyProbe.s_Failures)
		{
			AICF_PresetEconomyProbe.Check("BOUNDED_FINISH", false); AICF_EconomyProbeFinish(); return;
		}
		AICF_GroupSlot slot = m_VBState.GetSlot(0);
		if (!slot.IsCombatReady())
		{
			if (m_VBGroup.GetAgentsCount() != 5) return;
			if (!AICF_VBPrepare(slot)) { AICF_PresetEconomyProbe.Check("ROSTER_PRESET", false); AICF_EconomyProbeFinish(); return; }
			AICF_PresetEconomyProbe.s_Group = m_VBGroup;
		}
		if (!m_bEconomyProbeGoal)
		{
			for (int direction; direction < 16; direction++)
			{
				float angle = direction * 22.5 * Math.DEG2RAD;
				vector point;
				if (!AICF_InfantrySpawnPlacement.IsUsable(m_VBGroup, m_vVBHome + Vector(Math.Cos(angle) * 120, 0, Math.Sin(angle) * 120), point)) continue;
				m_bEconomyProbeGoal = m_OrderPlanner.AssignPlayerPointOrder(slot, m_VBFaction, point, false);
				break;
			}
			// Native navmesh tile loads are asynchronous; the unchanged outer
			// setup deadline bounds this wait. No readiness or movement is faked.
			if (!m_bEconomyProbeGoal) return;
			AICF_PresetEconomyProbe.Check("REAL_FOOT_DESTINATION", true);
		}
		if (!AICF_EconomyMaintainFootOrder(slot, now)) { AICF_EconomyProbeFinish(); return; }
		AICF_PresetEconomyProbe.s_Scope = true;
		if (m_sVBFaction == "US") m_VBCoordinator.Update(m_VBState, m_VBFaction, null, null, m_iStrategicBaseRevision, true);
		else m_VBCoordinator.Update(null, null, m_VBState, m_VBFaction, m_iStrategicBaseRevision, true);
		AICF_PresetEconomyProbe.s_Scope = false;
		AICF_TransportTrip currentTrip = m_VBCoordinator.GetTrip(slot);
		if (!AICF_PresetEconomyProbe.SameObservedTrip(m_EconomyProbeOriginal, currentTrip))
		{
			AICF_PresetEconomyProbe.Check("NO_NEW_SAME_INTENT_TRIP_IN_ANY_PHASE", false,
				"actual_current_trip_different_or_absent=1");
			AICF_EconomyProbeFinish(); return;
		}
		AICF_TransportTrip trip = AICF_PresetEconomyProbe.s_Trip;
		if (!trip) return;
		if (AICF_PresetEconomyProbe.Affordable())
		{
			if (!trip.GetLease() || !trip.GetLease().GetVehicle() || !trip.IsVehiclePresetDebited()) return;
			int active;
			int physical;
			m_VBCoordinator.AICF_EconomyProbeCount(trip, active, physical);
			AICF_PresetEconomyProbe.Check("AFFORDABLE_REAL_PURCHASE", currentTrip == trip && active == 1 && physical == 1 && AICF_PresetEconomyProbe.s_CapPeak == 1 &&
				AICF_PresetEconomyProbe.s_Leases == AICF_PresetEconomyProbe.s_EmptyReleases + 1 &&
				AICF_PresetEconomyProbe.s_MainWorldCars == 1 && AICF_PresetEconomyProbe.s_PurchaseCalls == 1 &&
				AICF_PresetEconomyProbe.s_Paid == AICF_PresetEconomyProbe.s_Quote && !m_VBCoordinator.AICF_EconomyProbeHasFailure(),
				string.Format("current_cap=%1 current_physical=%2 cap_peak=%3 empty_releases=%4 accepted_lease_generation=%5 current_trip_exact=%6",
					active, physical, AICF_PresetEconomyProbe.s_CapPeak, AICF_PresetEconomyProbe.s_EmptyReleases, trip.GetLease().GetLeaseGeneration(), currentTrip == trip));
			AICF_EconomyProbeFinish(); return;
		}
		if (!trip.IsTerminal()) return;
		if (!m_iEconomyProbeTerminalAt)
		{
			AICF_VehiclePresetEconomyDecision declined = trip.GetVehiclePresetEconomyDecline();
			bool sameReceipt = AICF_PresetEconomyProbe.TerminalReceiptSame(AICF_PresetEconomyProbe.s_DeclineAt > 0,
				trip == AICF_PresetEconomyProbe.s_DeclineTrip, declined && declined == AICF_PresetEconomyProbe.s_DeclineReceipt,
				declined && declined.Details() == AICF_PresetEconomyProbe.s_DeclineDetails,
				trip.GetRequestState() == AICF_PresetEconomyProbe.s_DeclineRequest && !trip.GetRequestState().GetSpawnPlan(),
				AICF_PresetEconomyProbe.s_DeclineGroupGeneration, trip.GetGroupGeneration(),
				AICF_PresetEconomyProbe.s_DeclineTripGeneration, trip.GetTripGeneration(),
				AICF_PresetEconomyProbe.s_DeclineRequestGeneration, trip.GetRequestState().GetRequestGeneration(),
				AICF_PresetEconomyProbe.s_DeclinePresetRevision, trip.GetVehiclePresetRevision(),
				AICF_PresetEconomyProbe.s_DeclineOperation, trip.GetOperationId());
			if (!declined || !sameReceipt ||
				declined.GetStatus() != AICF_EVehiclePresetEconomyDecision.INSUFFICIENT_AVAILABLE_SUPPLIES)
			{
				AICF_PresetEconomyProbe.Check("EXACT_TYPED_TERMINAL_DECLINE", false); AICF_EconomyProbeFinish(); return;
			}
			m_iEconomyProbeTerminalAt = now;
			m_EconomyProbeOriginal = trip;
			m_iEconomyFootGeneration = slot.GetSpawnGeneration();
			m_iEconomyFootIntent = slot.GetStrategicIntentRevision();
			m_EconomyFootKind = slot.GetTargetKind();
			m_vEconomyFootTarget = slot.GetTargetPosition();
			m_EconomyFootBase = slot.GetTargetBase();
			m_iEconomyFootNextReconcile = now + m_Config.GetCommanderIntervalMs();
			m_EconomyProbeLeader = AICF_GroupRuntime.ResolveAliveLeader(m_VBGroup);
			if (m_EconomyProbeLeader) { m_EconomyProbeLeaderId = m_EconomyProbeLeader.GetID(); m_vEconomyProbeWalkStart = m_EconomyProbeLeader.GetOrigin(); }
			int expectedReserved;
			if (AICF_PresetEconomyProbe.DrainWindow()) expectedReserved = 1;
			int currentCap; int currentPhysical;
			m_VBCoordinator.AICF_EconomyProbeCount(trip, currentCap, currentPhysical);
			bool emptyOwned = trip.GetPhase() == AICF_ETransportTripPhase.FALLBACK &&
				!trip.HasLease() && !trip.IsVehiclePresetDebited() && currentCap == 0 && currentPhysical == 0 &&
				AICF_PresetEconomyProbe.s_LeaseCalls == expectedReserved;
			bool noTransaction =
				AICF_PresetEconomyProbe.s_SpawnCalls == 0 && AICF_PresetEconomyProbe.s_PurchaseCalls == 0 &&
				AICF_PresetEconomyProbe.s_StockBefore == AICF_PresetEconomyProbe.s_StockAfter && !m_VBCoordinator.AICF_EconomyProbeHasFailure();
			AICF_PresetEconomyProbe.Check("EXPECTED_ATOMIC_DECLINE", emptyOwned && noTransaction);
			if (AICF_PresetEconomyProbe.DrainWindow())
			{
				string exactEpoch = string.Format("captured_lease_generation=%1 ", AICF_PresetEconomyProbe.s_DrainLeaseGeneration);
				bool exactCapture = AICF_PresetEconomyProbe.s_DeclineOwnCap == 1 &&
					AICF_PresetEconomyProbe.s_DeclineSameDrainedLease && AICF_PresetEconomyProbe.s_DeclineDetails.Contains("cap_reserved=1 ") &&
					AICF_PresetEconomyProbe.s_DeclineDetails.Contains(exactEpoch);
				bool causalRelease = AICF_PresetEconomyProbe.s_DrainAt < AICF_PresetEconomyProbe.s_DeclineAt &&
					AICF_PresetEconomyProbe.s_DeclineAt <= AICF_PresetEconomyProbe.s_LastEmptyReleaseAt && AICF_PresetEconomyProbe.s_EmptyReleases == 1;
				AICF_PresetEconomyProbe.Check("EXACT_RESERVED_EMPTY_DRAIN_WINDOW", exactCapture && causalRelease,
					string.Format("captured_lease_generation=%1 own_cap_after=%2 actual_same_drained_lease=%3",
						AICF_PresetEconomyProbe.s_DrainLeaseGeneration, currentCap, AICF_PresetEconomyProbe.s_DeclineSameDrainedLease));
			}
			return;
		}
		if (now - m_iEconomyProbeTerminalAt < 20000) return;
		bool leaderExact = m_EconomyProbeLeader && m_EconomyProbeLeader.GetID() == m_EconomyProbeLeaderId &&
			AICF_GroupRuntime.ResolveAliveLeader(m_VBGroup) == m_EconomyProbeLeader && AICF_GroupRuntime.IsAliveCharacter(m_EconomyProbeLeader);
		float moved;
		if (leaderExact) moved = vector.DistanceXZ(m_vEconomyProbeWalkStart, m_EconomyProbeLeader.GetOrigin());
		bool currentOrder = trip.GetHandoffState().IsOrderRestored() && m_VBGroup.GetCurrentWaypoint() &&
			m_VBGroup.GetCurrentWaypoint() == slot.GetWaypoint();
		if ((!leaderExact || moved < 10 || !currentOrder) && now - m_iEconomyProbeTerminalAt < 60000) return;
		AICF_PresetEconomyProbe.Check("SAME_INTENT_ONE_SHOT_NATIVE_FOOT_WALK", leaderExact && moved >= 10 &&
			currentTrip == m_EconomyProbeOriginal &&
			AICF_PresetEconomyProbe.s_Operations == 1 && AICF_PresetEconomyProbe.s_Trip == m_EconomyProbeOriginal &&
			AICF_PresetEconomyProbe.s_Leases == AICF_PresetEconomyProbe.s_EmptyReleases && AICF_PresetEconomyProbe.s_MainWorldCars == 0 &&
			AICF_PresetEconomyProbe.s_Paid == 0 && trip.GetHandoffState().IsOrderRestored() &&
			m_VBGroup.GetCurrentWaypoint() && m_VBGroup.GetCurrentWaypoint() == slot.GetWaypoint(),
			string.Format("moved_m=%1 operations=%2 original_route_proven=0 barrier_ms=%3 trip_creation_boundary=REGISTRY_CREATE current_trip_exact=%4",
				moved, AICF_PresetEconomyProbe.s_Operations, now - m_iEconomyProbeTerminalAt, currentTrip == m_EconomyProbeOriginal));
		AICF_EconomyProbeFinish();
	}
	protected void AICF_EconomyProbeFinish()
	{
		if (m_bEconomyProbeFinished) return;
		m_bEconomyProbeFinished = true;
		Print(string.Format("[AICF][PRESET_ECONOMY_DONE] checks=%1 failures=%2 affordable=%3 original_route_proven=0 leases=%4 main_world_cars=%5 purchase_calls=%6 paid=%7 operations=%8",
			AICF_PresetEconomyProbe.s_Checks, AICF_PresetEconomyProbe.s_Failures, AICF_PresetEconomyProbe.Affordable(),
			AICF_PresetEconomyProbe.s_Leases, AICF_PresetEconomyProbe.s_MainWorldCars, AICF_PresetEconomyProbe.s_PurchaseCalls,
			AICF_PresetEconomyProbe.s_Paid, AICF_PresetEconomyProbe.s_Operations));
		AICF_VBFinish();
	}
}
