// Applies stock combat skill only to materialised managed combat rosters.
class AICF_ManagedAICombatPolicy
{
	static int Apply(SCR_AIGroup group)
	{
		if (!Replication.IsServer() || !group)
			return 0;

		array<AIAgent> agents = {};
		group.GetAgents(agents);
		int configuredAgents;
		foreach (AIAgent agent : agents)
		{
			if (!agent)
				continue;

			IEntity character = agent.GetControlledEntity();
			if (!AICF_GroupRuntime.IsAliveCharacter(character))
				continue;

			SCR_AICombatComponent combat = SCR_AICombatComponent.Cast(
				character.FindComponent(SCR_AICombatComponent));
			if (!combat)
				continue;

			if (combat.GetAISkill() != EAISkill.VETERAN)
				combat.SetAISkill(EAISkill.VETERAN);
			if (combat.GetAISkill() == EAISkill.VETERAN)
				configuredAgents++;
		}

		return configuredAgents;
	}
}
