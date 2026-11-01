// Только controlled after-stage; baseline old source этот observer не загружает.
modded class AICF_VehicleAcceptanceMonitor
{
	override void ObserveConfirmedCapturedDeathRetirement(AICF_TransportTrip trip, AICF_TripRetirementObservation evidence)
	{
		super.ObserveConfirmedCapturedDeathRetirement(trip, evidence);
		if (!trip || !evidence || !evidence.IsConfirmed() || !evidence.Matches(trip)) return;
		if (!AICF_OrphanTripNativeConfirmation.s_aConfirmedOperations.Contains(trip.GetOperationId()))
			AICF_OrphanTripNativeConfirmation.s_aConfirmedOperations.Insert(trip.GetOperationId());
	}
}
