modded class SCR_ChimeraCharacter
{
	override void EOnInit(IEntity owner)
	{
		super.EOnInit(owner);
		AICF_BarracksCombatSafety.TrackCharacter(this);
	}
}

modded class SCR_DamageManagerComponent
{
	override protected void OnDamage(notnull BaseDamageContext damageContext)
	{
		super.OnDamage(damageContext);
		if (damageContext.damageValue <= 0 || !GetOwner() || GetOwner().GetWorld() != GetGame().GetWorld())
			return;
		// Кровотечение, столкновения и лечение не продлевают бой после последнего попадания.
		switch (damageContext.damageType)
		{
			case EDamageType.KINETIC:
			case EDamageType.FRAGMENTATION:
			case EDamageType.PROCESSED_FRAGMENTATION:
			case EDamageType.EXPLOSIVE:
			case EDamageType.MELEE:
			case EDamageType.INCENDIARY:
				vector position = damageContext.hitPosition;
				if (position == vector.Zero)
					position = GetOwner().GetOrigin();
				AICF_BarracksCombatSafety.RecordCombat(position, "DAMAGE");
		}
	}
}
