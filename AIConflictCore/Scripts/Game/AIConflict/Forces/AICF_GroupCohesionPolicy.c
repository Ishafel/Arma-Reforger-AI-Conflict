// Keeps managed fireteams in a compact stock formation without teleporting
// soldiers or overriding their combat reactions.
class AICF_GroupCohesionPolicy
{
	static const string FORMATION_NAME = "Column";
	static const int MAX_MOVE_HANDLERS = 16;

	bool Apply(SCR_AIGroup group)
	{
		if (!group)
			return false;

		bool applied;
		AIFormationComponent formation = AIFormationComponent.Cast(
			group.FindComponent(AIFormationComponent));
		if (formation && formation.SetFormation(FORMATION_NAME))
			applied = true;

		AIGroupMovementComponent movement = AIGroupMovementComponent.Cast(
			group.FindComponent(AIGroupMovementComponent));
		if (movement)
		{
			// Zero displacement keeps all handlers attached to the group's formation.
			movement.SetFormationDisplacement(0);
			for (int handlerId = 0; handlerId < MAX_MOVE_HANDLERS; handlerId++)
			{
				if (movement.GetMoveHandlerAgentCount(handlerId) == -1)
					break;

				movement.SetFormationDefinition(handlerId, FORMATION_NAME);
			}
			applied = true;
		}

		return applied;
	}
}
