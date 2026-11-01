// Изолированный read-only probe границы dispatch до первого GetIn action.
class AICF_BoardingRegistrationEvidence
{
	static Vehicle s_Vehicle;
	static vector s_Origin;
	static bool s_bBoarded;
	static bool s_bClosed;
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
		if (actual.GetReason() == "PRESET_EXACT_OCCUPANTS_CONFIRMED" && !AICF_BoardingRegistrationEvidence.s_bBoarded)
		{
			AICF_BoardingRegistrationEvidence.s_bBoarded = true;
			AICF_BoardingRegistrationEvidence.s_Vehicle = trip.GetLease().GetVehicle();
			AICF_BoardingRegistrationEvidence.s_Origin = trip.GetLease().GetVehicle().GetOrigin();
			Print("[AICF][BOARDING_REGISTRATION_OCCUPANTS] exact_production_readback=1");
		}
		return actual;
	}
}

modded class AICF_MatchController
{
	override protected void Update()
	{
		super.Update();
		if (!AICF_BoardingRegistrationEvidence.s_bBoarded || AICF_BoardingRegistrationEvidence.s_bClosed ||
			!AICF_BoardingRegistrationEvidence.s_Vehicle) return;
		float distance = vector.DistanceXZ(AICF_BoardingRegistrationEvidence.s_Origin, AICF_BoardingRegistrationEvidence.s_Vehicle.GetOrigin());
		if (distance < 10) return;
		AICF_BoardingRegistrationEvidence.s_bClosed = true;
		Print(string.Format("[AICF][BOARDING_REGISTRATION_DONE] physical_distance_m=%1 normal_shutdown_requested=1", distance));
		GetGame().RequestClose();
	}
}
