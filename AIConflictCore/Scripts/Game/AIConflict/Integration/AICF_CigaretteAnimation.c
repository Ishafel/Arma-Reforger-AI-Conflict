// Play/StopEmission меняют отдельные particle entities. Fixed callback может
// выполняться при заблокированной иерархии; обновляем косметику в обычном frame.
modded class SCR_CigaretAnimationComponent
{
	override protected void OnAddedToParent(IEntity child, IEntity parent)
	{
		super.OnAddedToParent(child, parent);
		ClearEventMask(child, EntityEvent.POSTFIXEDFRAME);
		SetEventMask(child, EntityEvent.FRAME);
	}

	override void EOnPostFixedFrame(IEntity owner, float timeSlice)
	{
		// Не допускаем старый dispatch, уже поставленный в fixed-step очередь.
	}

	override void EOnFrame(IEntity owner, float timeSlice)
	{
		if (!owner || !ChimeraCharacter.Cast(owner.GetParent()) || !m_CigarSmokeEmitter || !m_MouthSmokeEmitter) return;
		super.EOnPostFixedFrame(owner, timeSlice);
	}

	override protected void OnRemovedFromParent(IEntity child, IEntity parent)
	{
		ClearEventMask(child, EntityEvent.FRAME | EntityEvent.POSTFIXEDFRAME);
		super.OnRemovedFromParent(child, parent);
	}

	override void OnDelete(IEntity owner)
	{
		if (owner) ClearEventMask(owner, EntityEvent.FRAME | EntityEvent.POSTFIXEDFRAME);
		super.OnDelete(owner);
	}
}
