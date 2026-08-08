enum AICF_EGroupRole
{
	ATTACK = 0,
	DEFEND,
	RESERVE
}

// A slot survives group destruction; only the group bound to it changes.
enum AICF_EGroupSlotState
{
	EMPTY = 0,
	SPAWNING,
	READY,
	DESTROYED,
	WAITING
}

enum AICF_EDeploymentKind
{
	INITIAL = 0,
	REPLACEMENT
}
