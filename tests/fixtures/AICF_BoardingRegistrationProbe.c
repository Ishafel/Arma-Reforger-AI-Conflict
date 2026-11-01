// Изолированный read-only probe границы dispatch до первого GetIn action.
class AICF_BoardingRegistrationEvidence
{
	static Vehicle s_Vehicle;
	static vector s_Origin;
	static bool s_bBoarded;
	static bool s_bClosed;
	static string s_sOperation;
	static bool s_bAssignmentRejected;
	static AICF_TransportTrip s_TacticalTrip;
	static Vehicle s_TacticalVehicle;
	static EntityID s_TacticalVehicleId;
	static vector s_ResumeOrigin;
	static bool s_bTacticalResumed;
	static ref array<string> s_aTacticalOperations = {};
}

modded class AICF_VehicleTaskHandoff
{
	override bool RegisterTacticalVehicle(AICF_TransportTrip trip, AICF_GroupSlot slot)
	{
		bool actual = super.RegisterTacticalVehicle(trip, slot);
		Print(string.Format("[AICF][TACTICAL_REGISTRATION_CHECK] operation=%1 registered=%2", trip.GetOperationId(), actual));
		if (actual && !AICF_BoardingRegistrationEvidence.s_aTacticalOperations.Contains(trip.GetOperationId()))
		{
			AICF_BoardingRegistrationEvidence.s_aTacticalOperations.Insert(trip.GetOperationId());
		}
		return actual;
	}

	override bool AttachVehicleUtility(AICF_TransportTrip trip)
	{
		AICF_StrategicAssignmentSnapshot assignment = trip.GetAssignment();
		AICF_GroupSlot slot = trip.GetVehiclePresetSlot();
		if (assignment && slot)
		{
			Print(string.Format("[AICF][ASSIGNMENT_SCOPE_PROBE] operation=%1 kind=%2 same_base=%3 snapshot_position=%4 slot_position=%5 same_revision=%6 same_intent=%7 same_group=%8 same_generation=%9", trip.GetOperationId(), assignment.GetTargetKind(), assignment.GetTargetBase() == slot.GetTargetBase(), assignment.GetTargetPosition(), slot.GetTargetPosition(), assignment.GetAssignmentRevision() == slot.GetStrategicAssignmentRevision(), assignment.GetStrategicIntentRevision() == slot.GetStrategicIntentRevision(), assignment.GetGroup() == slot.GetGroup(), trip.GetGroupGeneration() == slot.GetSpawnGeneration()));
		}
		bool actual = super.AttachVehicleUtility(trip);
		if (actual && trip.GetPhase() == AICF_ETransportTripPhase.TRANSIT &&
			AICF_BoardingRegistrationEvidence.s_aTacticalOperations.Contains(trip.GetOperationId()))
		{
			AICF_BoardingRegistrationEvidence.s_TacticalTrip = trip;
			AICF_BoardingRegistrationEvidence.s_TacticalVehicle = trip.GetLease().GetVehicle();
			AICF_BoardingRegistrationEvidence.s_TacticalVehicleId = trip.GetLease().GetVehicle().GetID();
			AICF_BoardingRegistrationEvidence.s_bTacticalResumed = true;
			AICF_BoardingRegistrationEvidence.s_ResumeOrigin = trip.GetLease().GetVehicle().GetOrigin();
			Print("[AICF][TACTICAL_REGISTRATION_RESUMED] same_vehicle=1 operation=" + trip.GetOperationId());
		}
		if (!actual && assignment && slot &&
			(assignment.GetTargetPosition() != slot.GetTargetPosition() ||
			assignment.GetAssignmentRevision() != slot.GetStrategicAssignmentRevision() ||
			assignment.GetStrategicIntentRevision() != slot.GetStrategicIntentRevision()))
		{
			AICF_BoardingRegistrationEvidence.s_bAssignmentRejected = true;
			Print("[AICF][ASSIGNMENT_SCOPE_REPRO] actual_attach_rejected=1");
		}
		return actual;
	}
}

modded class AICF_VehicleBoardingFlow
{
	override AICF_TripOutcome Begin(AICF_TransportTrip trip, string causationId)
	{
		SCR_AIGroup group = trip.GetAssignment().GetGroup();
		Vehicle vehicle = trip.GetLease().GetVehicle();
		SCR_AIGroupUtilityComponent utility = group.GetGroupUtilityComponent();
		SCR_AIGroupVehicle record = utility.m_VehicleMgr.FindVehicle(vehicle);
		Print(string.Format("[AICF][BOARDING_REGISTRATION_CHECK] operation=%1 registered_before_actions=%2 vehicle=%3 group=%4", trip.GetOperationId(), record != null, vehicle.GetID(), group.GetID()));
		return super.Begin(trip, causationId);
	}

	override AICF_TripOutcome Tick(AICF_TransportTrip trip, string causationId)
	{
		AICF_TripOutcome actual = super.Tick(trip, causationId);
		if (actual.GetReason() == "PRESET_EXACT_OCCUPANTS_CONFIRMED")
		{
			AICF_BoardingRegistrationEvidence.s_bBoarded = true;
			AICF_BoardingRegistrationEvidence.s_Vehicle = trip.GetLease().GetVehicle();
			AICF_BoardingRegistrationEvidence.s_Origin = trip.GetLease().GetVehicle().GetOrigin();
			AICF_BoardingRegistrationEvidence.s_sOperation = trip.GetOperationId();
			Print("[AICF][BOARDING_REGISTRATION_OCCUPANTS] exact_production_readback=1 operation=" + trip.GetOperationId());
		}
		return actual;
	}
}

modded class AICF_MatchController
{
	override protected void Update()
	{
		super.Update();
		string waitValue;
		if (System.GetCLIParam("aicfProbeWaitForAssignmentFailure", waitValue))
		{
			if (AICF_BoardingRegistrationEvidence.s_bAssignmentRejected && !AICF_BoardingRegistrationEvidence.s_bClosed)
			{
				AICF_BoardingRegistrationEvidence.s_bClosed = true;
				GetGame().RequestClose();
			}
			return;
		}
		if (!AICF_BoardingRegistrationEvidence.s_bBoarded || AICF_BoardingRegistrationEvidence.s_bClosed) return;
		Vehicle observedVehicle = AICF_BoardingRegistrationEvidence.s_Vehicle;
		vector observedOrigin = AICF_BoardingRegistrationEvidence.s_Origin;
		string observedOperation = AICF_BoardingRegistrationEvidence.s_sOperation;
		if (System.IsCLIParam("aicfRequireTacticalRegistration"))
		{
			if (!AICF_BoardingRegistrationEvidence.s_bTacticalResumed || !AICF_BoardingRegistrationEvidence.s_TacticalTrip ||
				AICF_BoardingRegistrationEvidence.s_TacticalTrip.IsTerminal() || !AICF_BoardingRegistrationEvidence.s_TacticalVehicle ||
				AICF_BoardingRegistrationEvidence.s_TacticalVehicle.GetID() != AICF_BoardingRegistrationEvidence.s_TacticalVehicleId) return;
			observedVehicle = AICF_BoardingRegistrationEvidence.s_TacticalVehicle;
			observedOrigin = AICF_BoardingRegistrationEvidence.s_ResumeOrigin;
			observedOperation = AICF_BoardingRegistrationEvidence.s_TacticalTrip.GetOperationId();
		}
		if (!observedVehicle) return;
		float distance = vector.DistanceXZ(observedOrigin, observedVehicle.GetOrigin());
		float requiredDistance = 10;
		string distanceValue;
		if (System.GetCLIParam("aicfBoardingRegistrationDistanceM", distanceValue))
			requiredDistance = Math.Clamp(distanceValue.ToFloat(), 10, 1000);
		if (distance < requiredDistance) return;
		AICF_BoardingRegistrationEvidence.s_bClosed = true;
		bool observeOnly = System.IsCLIParam("aicfBoardingRegistrationObserveOnly");
		Print(string.Format("[AICF][BOARDING_REGISTRATION_DONE] physical_distance_m=%1 normal_shutdown_requested=%5 operation=%2 vehicle=%3 required_distance_m=%4", distance, observedOperation, observedVehicle.GetID(), requiredDistance, !observeOnly));
		if (!observeOnly) GetGame().RequestClose();
	}
}
