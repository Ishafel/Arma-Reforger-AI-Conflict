// Keep dead characters for the lifetime of the match while leaving the stock
// garbage policy unchanged for vehicles, dropped items, and other entities.
modded class SCR_GarbageSystem
{
	//------------------------------------------------------------------------------------------------
	protected bool AICF_IsPersistentCorpse(IEntity entity)
	{
		SCR_ChimeraCharacter character = SCR_ChimeraCharacter.Cast(entity);
		if (!character)
			return false;

		CharacterControllerComponent controller = character.GetCharacterController();
		return controller && controller.GetLifeState() == ECharacterLifeState.DEAD;
	}

	//------------------------------------------------------------------------------------------------
	override protected float OnInsertRequested(IEntity entity, float lifetime)
	{
		if (AICF_IsPersistentCorpse(entity))
			return -1;

		return super.OnInsertRequested(entity, lifetime);
	}

	//------------------------------------------------------------------------------------------------
	override protected bool OnBeforeDelete(IEntity entity)
	{
		if (AICF_IsPersistentCorpse(entity))
			return false;

		return super.OnBeforeDelete(entity);
	}
}
