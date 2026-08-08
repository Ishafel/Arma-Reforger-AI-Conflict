// Shared runtime queries for managed groups. SCR_AIGroup is a controller entity;
// its own origin is not guaranteed to follow the soldiers after deployment.
class AICF_GroupRuntime
{
	static IEntity ResolveAliveLeader(SCR_AIGroup group)
	{
		if (!group)
			return null;

		IEntity leader = group.GetLeaderEntity();
		if (IsAliveCharacter(leader))
			return leader;

		// Leader promotion is asynchronous. Use a surviving member until the group
		// appoints its next leader, then callers automatically switch on the next tick.
		array<AIAgent> agents = {};
		group.GetAgents(agents);
		foreach (AIAgent agent : agents)
		{
			if (!agent)
				continue;

			IEntity controlledEntity = agent.GetControlledEntity();
			if (IsAliveCharacter(controlledEntity))
				return controlledEntity;
		}

		return null;
	}

	static int CountAliveAgents(SCR_AIGroup group)
	{
		if (!group)
			return 0;

		array<AIAgent> agents = {};
		group.GetAgents(agents);
		int alive;
		foreach (AIAgent agent : agents)
		{
			if (agent && IsAliveCharacter(agent.GetControlledEntity()))
				alive++;
		}

		return alive;
	}

	static bool IsAliveCharacter(IEntity entity)
	{
		ChimeraCharacter character = ChimeraCharacter.Cast(entity);
		if (!character)
			return false;

		CharacterControllerComponent controller = character.GetCharacterController();
		return controller && controller.GetLifeState() == ECharacterLifeState.ALIVE;
	}
}
