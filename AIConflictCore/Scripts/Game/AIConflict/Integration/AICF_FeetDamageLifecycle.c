// Private inventory models and characters leaving replication have no damage
// RPC recipients. Their footwear changes must not create replicated effects.
modded class SCR_FeetLoadoutSlotInfo
{
	protected bool AICF_CanUpdateFeetDamage()
	{
		if (!Replication.IsServer())
			return true;
		SCR_ChimeraCharacter character = SCR_ChimeraCharacter.Cast(GetOwner());
		if (!character)
			return true;
		if (character.IsDeleted() || character.GetWorld() != GetGame().GetWorld())
			return false;
		SCR_CharacterDamageManagerComponent damage = SCR_CharacterDamageManagerComponent.Cast(character.GetDamageManager());
		return !damage || Replication.FindItemId(damage).IsValid();
	}

	override void OnAttachedEntity(IEntity entity)
	{
		if (AICF_CanUpdateFeetDamage())
			super.OnAttachedEntity(entity);
	}

	override void OnDetachedEntity(IEntity entity)
	{
		if (AICF_CanUpdateFeetDamage())
			super.OnDetachedEntity(entity);
	}

	void ~SCR_FeetLoadoutSlotInfo()
	{
		if (GetGame())
			GetGame().GetCallqueue().Remove(OnSlotPostInit);
	}
}
