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

enum AICF_EStrategicDecisionAuthority
{
	NONE = 0,
	AI_COMMANDER,
	PLAYER_COMMAND,
	SYSTEM_HOLD
}

class AICF_StrategicDecisionAuthority
{
	static string ToString(AICF_EStrategicDecisionAuthority authority)
	{
		switch (authority)
		{
			case AICF_EStrategicDecisionAuthority.AI_COMMANDER:
				return "AI_COMMANDER";
			case AICF_EStrategicDecisionAuthority.PLAYER_COMMAND:
				return "PLAYER_COMMAND";
			case AICF_EStrategicDecisionAuthority.SYSTEM_HOLD:
				return "SYSTEM_HOLD";
		}

		return "NONE";
	}
}
