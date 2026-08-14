// A trip describes one bounded attempt to accelerate an infantry assignment.
// Physical asset cleanup is deliberately not encoded in this phase enum.
enum AICF_ETransportTripPhase
{
	WAITING_FOR_SITE = 0,
	ACQUIRING,
	BOARDING,
	TRANSIT,
	DISMOUNT,
	HANDOFF,
	COMPLETE,
	FALLBACK,
	FAILED_CLOSED
}

// Phase flows return data. They never transition the trip or invoke another
// flow directly; the trip controller is the sole outcome interpreter.
enum AICF_ETripOutcomeKind
{
	WAIT = 0,
	RETRY,
	START_BOARDING,
	START_MOVEMENT,
	START_DISMOUNT,
	COMPLETE_TRIP,
	FALLBACK_TO_FOOT,
	RELEASE_LEASE,
	TERMINAL_FAIL_CLOSED
}

// Only FactionFleet may change lease state. A world-pool vehicle has no lease
// and therefore never appears in this enum.
enum AICF_EVehicleLeaseState
{
	RESERVED = 0,
	ACTIVE,
	RELEASE_PENDING,
	RELEASED,
	FAILED_CLOSED
}
