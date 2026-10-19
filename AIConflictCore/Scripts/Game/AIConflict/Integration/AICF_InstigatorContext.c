// 1.8.0.13 defines OTHER but its native context classifier rejects that type.
// Keep the original source and let native code classify identifiable characters.
modded class SCR_InstigatorContextData
{
	override void CreateInstigatorData(int victimPlayerID, IEntity victimEntity, IEntity killerEntity, notnull Instigator instigator, bool isDeleted = false)
	{
		if (instigator.GetInstigatorType() != InstigatorType.INSTIGATOR_OTHER || isDeleted || !ChimeraCharacter.Cast(victimEntity))
		{
			super.CreateInstigatorData(victimPlayerID, victimEntity, killerEntity, instigator, isDeleted);
			return;
		}

		IEntity source = killerEntity;
		if (!source) source = instigator.GetInstigatorEntity();
		if (ChimeraCharacter.Cast(source))
		{
			Instigator characterInstigator = Instigator.CreateInstigator(source);
			InstigatorType type = characterInstigator.GetInstigatorType();
			if (type == InstigatorType.INSTIGATOR_AI || type == InstigatorType.INSTIGATOR_PLAYER)
			{
				// Native suicide/faction/disguise and player penalty rules remain the owner.
				super.CreateInstigatorData(victimPlayerID, victimEntity, source, characterInstigator, false);
				m_Instigator = instigator;
				return;
			}
		}

		// An unowned object is an environmental source, not an AI or player killer.
		// Reset derived fields as CreateInstigatorData may also reuse a context.
		m_iVictimPlayerID = victimPlayerID;
		if (m_iVictimPlayerID <= 0)
			m_iVictimPlayerID = SCR_PossessingManagerComponent.GetPlayerIdFromControlledEntity(victimEntity);
		m_VictimEntity = victimEntity;
		m_KillerEntity = source;
		m_Instigator = instigator;
		m_iKillerPlayerID = instigator.GetInstigatorPlayerID();
		m_eKillerControlType = SCR_ECharacterControlType.UNKNOWN;
		m_eVictimControlType = SCR_ECharacterControlType.AI;
		if (m_iVictimPlayerID > 0)
			m_eVictimControlType = SCR_CharacterHelper.GetCharacterControlType(victimEntity);
		m_eKillerDisguiseType = SCR_ECharacterDisguiseType.DEFAULT_FACTION;
		m_eVictimDisguiseType = SCR_ECharacterDisguiseType.DEFAULT_FACTION;
		SetPerceivedFaction(m_iVictimPlayerID, victimEntity, source, instigator, false);
		m_eVictimKillerRelation = SCR_ECharacterDeathStatusRelations.OTHER_DEATH;
	}
}
