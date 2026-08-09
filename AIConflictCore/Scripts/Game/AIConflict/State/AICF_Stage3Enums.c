// Server-owned vehicle lifecycle. Vehicles accelerate a managed infantry slot;
// they never replace the slot or its ticket/reinforcement lifecycle.
enum AICF_EVehicleState
{
	NONE,
	REQUESTED,
	SPAWNING,
	WAITING_FOR_SITE,
	BOARDING,
	MOUNTED,
	MOVING,
	DISEMBARKING,
	DISMOUNTED,
	RECOVERING,
	INFANTRY_FALLBACK,
	ABANDONED,
	DESTROYED
}

enum AICF_EVehicleKind
{
	TRANSPORT,
	ARMED_LIGHT
}

// Initial boarding is deliberately role-ordered. A broad get-in waypoint can
// allocate every soldier to cargo before the pilot compartment is occupied.
enum AICF_EVehicleBoardingPhase
{
	NONE,
	APPROACH,
	DRIVER,
	GUNNER,
	PASSENGERS
}

// Crew recovery is sequential and keeps at most one exact role action active.
// Unlike a boolean, this phase remains explicit when an armed vehicle must
// recover both mandatory crew roles in succession.
enum AICF_EVehicleCrewRecoveryPhase
{
	NONE,
	DRIVER,
	GUNNER
}
