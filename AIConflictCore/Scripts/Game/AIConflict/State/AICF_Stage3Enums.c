// Server-owned vehicle lifecycle. Vehicles accelerate a managed infantry slot;
// they never replace the slot or its ticket/reinforcement lifecycle.
enum AICF_EVehicleState
{
	NONE,
	REQUESTED,
	SPAWNING,
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
