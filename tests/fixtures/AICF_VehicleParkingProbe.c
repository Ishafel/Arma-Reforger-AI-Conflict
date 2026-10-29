// Native road slope, transport deployment,30s physical hold and real resume.
modded class AICF_VehicleParking
{
	bool AICF_PPStaleNoMutation(AICF_TransportTrip trip)
	{
		bool brake = m_Controller.GetPersistentHandBrake();
		string current = m_sVehicleRpl; m_sVehicleRpl = "STALE";
		bool accepted = EnsureApplied(trip); m_sVehicleRpl = current;
		return !accepted && m_Controller.GetPersistentHandBrake() == brake;
	}
	bool AICF_PPOwnsBrake() { return m_bOwnsBrake; }
}

modded class AICF_MatchController
{
	protected bool m_bPPFinished;
	protected bool m_bPPThreat;
	protected int m_iPPStarted;
	protected int m_iPPStep;
	protected int m_iPPHeldAt;
	protected int m_iPPHoldPolls;
	protected int m_iPPMotionPolls;
	protected int m_iPPChecks;
	protected int m_iPPFailures;
	protected vector m_vPPDriveStart;
	protected vector m_vPPParked;
	protected vector m_vPPResumed;
	protected float m_fPPCarHealth;
	protected ref array<float> m_aPPHealth = {};
	protected bool AICF_PPSeedOnly()
	{
		string value;
		return System.GetCLIParam("aicfVehicleParkingSeedOnly", value) && value == "1";
	}
	protected void AICF_PPCaptureHealth()
	{
		m_fPPCarHealth = DamageManagerComponent.Cast(m_VBVehicle.FindComponent(DamageManagerComponent)).GetDefaultHitZone().GetHealthScaled();
		foreach (IEntity member : m_aVBMembers) m_aPPHealth.Insert(DamageManagerComponent.Cast(member.FindComponent(DamageManagerComponent)).GetDefaultHitZone().GetHealthScaled());
	}
	protected bool AICF_PPHealthUnchanged()
	{
		bool health = DamageManagerComponent.Cast(m_VBVehicle.FindComponent(DamageManagerComponent)).GetDefaultHitZone().GetHealthScaled() == m_fPPCarHealth;
		for (int index; index < m_aVBMembers.Count(); index++) health = health && DamageManagerComponent.Cast(m_aVBMembers[index].FindComponent(DamageManagerComponent)).GetDefaultHitZone().GetHealthScaled() == m_aPPHealth[index];
		return health;
	}
	protected void AICF_PPCheck(string rule, bool passed, string details = "")
	{
		m_iPPChecks++;
		if (!passed) m_iPPFailures++;
		Print(string.Format("[AICF][PARKING_CASE] rule=%1 passed=%2 details=[%3]", rule, passed, details));
	}
	override protected void Update()
	{
		super.Update();
		string enabled;
		if (!System.GetCLIParam("aicfVehicleParkingProbe", enabled) || enabled != "1" || !Replication.IsServer() || !m_bRosterReady || m_bPPFinished) return;
		int now = System.GetTickCount();
		if (!m_iPPStarted)
		{
			m_iPPStarted = now; m_sVBScenario = "transport";
			if (!AICF_VBInitialize()) { AICF_PPCheck("INITIALIZE", false); AICF_PPFinish(); }
			return;
		}
		if (now - m_iPPStarted > 360000 || m_iPPFailures || m_iVBFailures) { AICF_PPCheck("BOUNDED_FINISH", false); AICF_PPFinish(); return; }
		AICF_GroupSlot slot = m_VBState.GetSlot(0);
		if (!slot.IsCombatReady())
		{
			if (m_VBGroup.GetAgentsCount() != 5) return;
			if (!AICF_VBPrepare(slot)) { AICF_PPCheck("PRESET", false); AICF_PPFinish(); return; }
		}
		AICF_VBObserve(); AICF_VBThreat(m_bPPThreat);
		if (m_sVBFaction == "US") m_VBCoordinator.Update(m_VBState, m_VBFaction, null, null, m_iStrategicBaseRevision, true);
		else m_VBCoordinator.Update(null, null, m_VBState, m_VBFaction, m_iStrategicBaseRevision, true);
		AICF_TransportTrip trip = m_VBCoordinator.GetTrip(slot);
		if (!trip || !trip.GetLease() || !trip.GetLease().HasPhysicalAsset()) return;
		if (trip.IsTerminal()) { AICF_PPCheck("NO_PREMATURE_FALLBACK", false, trip.GetTerminalReason()); AICF_PPFinish(); return; }
		m_VBTrip = trip; m_VBVehicle = trip.GetLease().GetVehicle();
		CarControllerComponent controller = CarControllerComponent.Cast(m_VBVehicle.FindComponent(CarControllerComponent));
		Physics physics = m_VBVehicle.GetPhysics();
		if (!controller || !physics) { AICF_PPCheck("CAR_PHYSICS", false); AICF_PPFinish(); return; }
		if (m_iPPStep == 0)
		{
			if (trip.GetPhase() != AICF_ETransportTripPhase.TRANSIT) return;
			m_VBVehicleId = m_VBVehicle.GetID(); m_iVBLeaseGeneration = trip.GetLease().GetLeaseGeneration();
			if (!AICF_VBExact(trip)) return;
			m_vPPDriveStart = m_VBVehicle.GetOrigin();
			vector target;
			if (!AICF_VBFarTarget(target)) return;
			if (AICF_PPSeedOnly())
			{
				AICF_PPCaptureHealth();
				controller.SetPersistentHandBrake(true);
				AICF_PPCheck("PREEXISTING_NATIVE_BRAKE_SEEDED", controller.GetPersistentHandBrake() && !trip.GetBehaviorState().m_Parking,
					"persistent_brake=1 parking_token=0 controlled_native_brake_seed=1");
			}
			AICF_PPCheck("INITIAL_REAL_ROUTE", m_OrderPlanner.AssignPlayerPointOrder(slot, m_VBFaction, target, m_VBCoordinator.IsInfantryOrderSuspended(slot)));
			m_iPPStep = 1; return;
		}
		if (m_iPPStep == 1)
		{
			if (AICF_PPSeedOnly())
			{
				if (physics.GetVelocity().Length() >= 0.5) m_iPPMotionPolls++;
				float distance = vector.DistanceXZ(m_vPPDriveStart, m_VBVehicle.GetOrigin());
				if (distance < 25 || m_iPPMotionPolls < 3) return;
				ChimeraCharacter pilot = ChimeraCharacter.Cast(AICF_VBPilot());
				AIAgent agent;
				if (pilot && pilot.GetAIControlComponent()) agent = pilot.GetAIControlComponent().GetControlAIAgent();
				AICarMovementComponent car;
				if (agent) car = AICarMovementComponent.Cast(agent.GetMovementComponent());
				bool nativeReady = car && car.GetAIAgent() && car.GetAIAgent().GetControlledEntity() == m_VBVehicle;
				AICF_PPCheck("NATIVE_PILOT_RELEASED_PREEXISTING", !controller.GetPersistentHandBrake() && !trip.GetBehaviorState().m_Parking && nativeReady && AICF_VBExact(trip),
					string.Format("persistent_brake=%1 parking_token=%2 moved_m=%3 velocity_polls=%4 native_pilot_ready=%5", controller.GetPersistentHandBrake(), trip.GetBehaviorState().m_Parking != null, distance, m_iPPMotionPolls, nativeReady));
				AICF_PPCheck("ORIGINAL_HEALTH_UNCHANGED", AICF_PPHealthUnchanged());
				AICF_PPFinish(); return;
			}
			vector angles = m_VBVehicle.GetYawPitchRoll();
			vector transform[4]; m_VBVehicle.GetWorldTransform(transform);
			vector forward = transform[2]; forward[1] = 0; forward.Normalize();
			vector before = m_VBVehicle.GetOrigin() - forward * 5;
			vector after = m_VBVehicle.GetOrigin() + forward * 5;
			float terrainGrade = Math.AbsFloat(m_VBVehicle.GetWorld().GetSurfaceY(after[0], after[2]) - m_VBVehicle.GetWorld().GetSurfaceY(before[0], before[2])) / 10;
			if (!AICF_VBExact(trip) || physics.GetVelocity().Length() < 2 || controller.GetPersistentHandBrake() ||
				vector.DistanceXZ(m_vPPDriveStart, m_VBVehicle.GetOrigin()) < 150 || terrainGrade < 0.1 || Math.AbsFloat(angles[1]) < 6 || transform[1][1] < 0.8) return;
			DamageManagerComponent carDamage = DamageManagerComponent.Cast(m_VBVehicle.FindComponent(DamageManagerComponent));
			if (!carDamage || !carDamage.GetDefaultHitZone()) return;
			AICF_PPCaptureHealth();
			m_bPPThreat = true;
			AICF_PPCheck("REAL_NATIVE_SLOPE", true, string.Format("pitch_deg=%1 speed_mps=%2 pre_brake=0 pos=%3 terrain_grade=%4", angles[1], physics.GetVelocity().Length(), m_VBVehicle.GetOrigin(), terrainGrade));
			m_iPPStep = 2; return;
		}
		if (m_iPPStep == 2)
		{
			if (trip.GetPhase() != AICF_ETransportTripPhase.TACTICAL_DEPLOYMENT || !AICF_VBDeployed(trip)) return;
			m_vPPParked = m_VBVehicle.GetOrigin(); m_iPPHeldAt = now;
			AICF_PPCheck("ALL_FIVE_OUTSIDE_PARKED", !AICF_VBPilot() && controller.GetPersistentHandBrake() && physics.GetVelocity().Length() <= 1 && trip.GetBehaviorState().m_Parking != null,
				string.Format("speed_mps=%1 pitch_deg=%2 owns_brake=%3", physics.GetVelocity().Length(), m_VBVehicle.GetYawPitchRoll()[1], trip.GetBehaviorState().m_Parking.AICF_PPOwnsBrake()));
			AICF_PPCheck("STALE_CAR_NO_MUTATION", trip.GetBehaviorState().m_Parking.AICF_PPStaleNoMutation(trip));
			m_iPPStep = 3; return;
		}
		if (m_iPPStep == 3)
		{
			float drift = vector.Distance(m_vPPParked, m_VBVehicle.GetOrigin());
			vector pose[4]; m_VBVehicle.GetWorldTransform(pose);
			bool held = controller.GetPersistentHandBrake() && !AICF_VBPilot() && physics.GetVelocity().Length() <= 1 && drift <= 1 && pose[1][1] > 0.8;
			if (!held) { AICF_PPCheck("NATIVE_PHYSICAL_HOLD", false, string.Format("drift_m=%1 speed=%2 brake=%3 up=%4", drift, physics.GetVelocity().Length(), controller.GetPersistentHandBrake(), pose[1][1])); AICF_PPFinish(); return; }
			m_iPPHoldPolls++;
			if (now - m_iPPHeldAt < 30000) return;
			AICF_PPCheck("THIRTY_SECOND_SLOPE_HOLD", m_iPPHoldPolls >= 20 && Math.AbsFloat(m_VBVehicle.GetYawPitchRoll()[1]) >= 3,
				string.Format("hold_ms=%1 polls=%2 drift_m=%3 pitch_deg=%4", now-m_iPPHeldAt, m_iPPHoldPolls, drift, m_VBVehicle.GetYawPitchRoll()[1]));
			m_bPPThreat = false; m_iPPStep = 4; return;
		}
		if (m_iPPStep == 4)
		{
			if (trip.GetPhase() != AICF_ETransportTripPhase.TRANSIT || !AICF_VBExact(trip)) return;
			AICF_PPCheck("OWN_BRAKE_RELEASED_NATIVE_PILOT", !controller.GetPersistentHandBrake() && AICF_VBPilot() && !trip.GetBehaviorState().m_Parking);
			m_vPPResumed = m_VBVehicle.GetOrigin(); m_iPPStep = 5; return;
		}
		if (m_iPPStep == 5)
		{
			if (physics.GetVelocity().Length() >= 0.5) m_iPPMotionPolls++;
			if (vector.DistanceXZ(m_vPPResumed, m_VBVehicle.GetOrigin()) < 25 || m_iPPMotionPolls < 3) return;
			AICF_PPCheck("REAL_RESUMED_MOVEMENT", AICF_VBIdentities() && m_VBVehicle.GetID() == m_VBVehicleId && trip.GetLease().GetLeaseGeneration() == m_iVBLeaseGeneration,
				string.Format("moved_m=%1 velocity_polls=%2", vector.DistanceXZ(m_vPPResumed, m_VBVehicle.GetOrigin()), m_iPPMotionPolls));
			AICF_PPCheck("ORIGINAL_HEALTH_UNCHANGED", AICF_PPHealthUnchanged());
			AICF_PPFinish();
		}
	}
	protected void AICF_PPFinish()
	{
		if (m_bPPFinished) return;
		m_bPPFinished = true; AICF_VBThreat(false);
		string classification = "CONTROLLED_NATIVE_SLOPE";
		if (AICF_PPSeedOnly()) classification = "CONTROLLED_PREEXISTING_NATIVE_BRAKE";
		Print(string.Format("[AICF][PARKING_FINISHED] checks=%1 failures=%2 classification=%3", m_iPPChecks, m_iPPFailures, classification));
		AICF_VBFinish();
	}
}
