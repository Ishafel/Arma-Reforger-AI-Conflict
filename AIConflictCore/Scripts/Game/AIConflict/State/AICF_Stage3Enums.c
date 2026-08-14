enum AICF_EVehicleKind
{
	TRANSPORT,
	LIGHT_TRANSPORT,
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
