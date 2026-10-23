modded class SCR_FeetLoadoutSlotInfo
{
	bool AICF_TestFeetAllowed()
	{
		return AICF_CanUpdateFeetDamage();
	}
}
modded class AICF_MatchController
{
	protected int m_iAICFFeetPhase;
	protected int m_iAICFFeetChecks;
	protected int m_iAICFFeetFailed;
	protected InventoryStorageSlot m_AICFFeetSlot;
	protected IEntity m_AICFBoots;
	protected SCR_CharacterDamageManagerComponent m_AICFFeetDamage;
	protected SCR_FeetLoadoutSlotInfo AICF_FindFeet(IEntity character)
	{
		set<BaseInventoryStorageComponent> roots = new set<BaseInventoryStorageComponent>();
		SCR_PlayerArsenalLoadout.FindStorageComponents(character, roots);
		foreach (BaseInventoryStorageComponent storage : roots)
			for (int i; i < storage.GetSlotsCount(); i++)
			{
				SCR_FeetLoadoutSlotInfo feet = SCR_FeetLoadoutSlotInfo.Cast(storage.GetSlot(i));
				if (feet) return feet;
			}
		return null;
	}
	protected void AICF_FeetCheck(string name, bool pass)
	{
		m_iAICFFeetChecks++;
		if (!pass) m_iAICFFeetFailed++;
		Print(string.Format("[AICF][FEET_CASE] case=%1 pass=%2", name, pass));
	}
	override protected void Update()
	{
		super.Update();
		if (!m_bRosterReady || !System.IsCLIParam("aicfFeetProbe") || m_iAICFFeetPhase > 2) return;
		if (m_iAICFFeetPhase == 0)
		{
			AICF_LoadoutRecipe recipe = new AICF_LoadoutRecipe();
			string role, reason;
			recipe.m_sCharacter = m_GroupSpawner.ResolveRecruitPrefab(m_USSRFaction, 0, role);
			recipe.m_sFaction = "USSR";
			recipe.m_sName = "Feet damage lifecycle probe";
			AICF_LoadoutCatalog catalog = new AICF_LoadoutCatalog(m_USSRFaction);
			AICF_EquipmentDraft draft = new AICF_EquipmentDraft();
			bool built = draft.Build(recipe, catalog, reason);
			AICF_FeetCheck("LOCAL_MODEL", built);
			if (built)
			{
				SCR_FeetLoadoutSlotInfo privateFeet = AICF_FindFeet(draft.GetCharacter());
				AICF_FeetCheck("LOCAL_DENIED", privateFeet && !privateFeet.AICF_TestFeetAllowed());
				SCR_CharacterDamageManagerComponent privateDamage = SCR_CharacterDamageManagerComponent.Cast(draft.GetCharacter().FindComponent(SCR_CharacterDamageManagerComponent));
				IEntity boots;
				if (privateFeet) boots = privateFeet.GetAttachedEntity();
				if (boots) privateFeet.DetachEntity();
				AICF_FeetCheck("LOCAL_NO_DAMAGE_EFFECT", privateDamage && !privateDamage.IsDamageEffectPresent(SCR_ExposedFeetDamageEffect));
				if (boots) privateFeet.AttachEntity(boots);
			}
			draft.Clear();
			AICF_FeetCheck("LOCAL_CLEARED", !draft.GetCharacter());
			AICF_GroupSlot slot = m_USState.GetSlot(0);
			IEntity live;
			if (slot) live = AICF_GroupRuntime.ResolveAliveLeader(slot.GetGroup());
			SCR_FeetLoadoutSlotInfo feet;
			if (live) feet = AICF_FindFeet(live);
			if (live) m_AICFFeetDamage = SCR_CharacterDamageManagerComponent.Cast(live.FindComponent(SCR_CharacterDamageManagerComponent));
			AICF_FeetCheck("LIVE_REGISTERED", m_AICFFeetDamage && Replication.FindItemId(m_AICFFeetDamage).IsValid());
			AICF_FeetCheck("LIVE_ALLOWED", feet && feet.AICF_TestFeetAllowed());
			if (feet) m_AICFBoots = feet.GetAttachedEntity();
			AICF_FeetCheck("LIVE_BOOTS_PRESENT", m_AICFBoots != null);
			if (!feet || !m_AICFBoots)
			{
				m_iAICFFeetPhase = 3;
				GetGame().RequestClose();
				return;
			}
			m_AICFFeetSlot = feet;
			feet.DetachEntity();
			AICF_FeetCheck("LIVE_BOOTS_DETACHED", !feet.GetAttachedEntity());
			m_iAICFFeetPhase = 1;
			return;
		}
		if (m_iAICFFeetPhase == 1)
		{
			AICF_FeetCheck("LIVE_EFFECT_ADDED", m_AICFFeetDamage && m_AICFFeetDamage.IsDamageEffectPresent(SCR_ExposedFeetDamageEffect));
			if (m_AICFFeetSlot && m_AICFBoots) m_AICFFeetSlot.AttachEntity(m_AICFBoots);
			m_iAICFFeetPhase = 2;
			return;
		}
		AICF_FeetCheck("LIVE_BOOTS_RESTORED", m_AICFFeetSlot && m_AICFFeetSlot.GetAttachedEntity() == m_AICFBoots);
		AICF_FeetCheck("LIVE_EFFECT_REMOVED", m_AICFFeetDamage && !m_AICFFeetDamage.IsDamageEffectPresent(SCR_ExposedFeetDamageEffect));
		m_AICFFeetSlot = null;
		m_AICFBoots = null;
		m_AICFFeetDamage = null;
		m_iAICFFeetPhase = 3;
		Print(string.Format("[AICF][FEET_DONE] checks=%1 failed=%2", m_iAICFFeetChecks, m_iAICFFeetFailed));
		GetGame().RequestClose();
	}
}
