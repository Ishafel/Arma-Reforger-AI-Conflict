enum AICF_EGroupRole
{
	ATTACK = 0,
	DEFEND,
	RESERVE
}

// Commander-selectable mobility profile. Armed light vehicles are supported;
// heavy armor remains deliberately outside the Stage 4 command surface.
enum AICF_EGroupUnitType
{
	INFANTRY = 0,
	MOTORIZED_LIGHT,
	MOTORIZED_TRUCK,
	MOTORIZED_ARMED_LIGHT
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
