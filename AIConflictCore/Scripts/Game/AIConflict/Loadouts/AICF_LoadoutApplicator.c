class AICF_LoadoutApplicator
{
	static bool Apply(IEntity character, SCR_AIGroup group, SCR_CampaignFaction faction, AICF_LoadoutBinding binding, out string reason)
	{
		reason = "CHARACTER_IDENTITY_CHANGED";
		if (!Replication.IsServer() || !character || !group || !faction || !binding)
			return false;
		AIControlComponent control = AIControlComponent.Cast(character.FindComponent(AIControlComponent));
		CharacterControllerComponent controller = CharacterControllerComponent.Cast(character.FindComponent(CharacterControllerComponent));
		RplComponent rpl = RplComponent.Cast(character.FindComponent(RplComponent));
		if (!control || !control.GetControlAIAgent() || control.GetControlAIAgent().GetParentGroup() != group ||
			!controller || controller.IsPlayerControlled() || !rpl || !rpl.IsMaster() ||
			!AICF_GroupRuntime.IsAliveCharacter(character) || SCR_Faction.GetEntityFaction(character) != faction)
			return false;
		EntityID identity = character.GetID();
		AICF_LoadoutCatalog catalog = new AICF_LoadoutCatalog(faction);
		// Проверяем тот же concrete prefab после randomizer в изолированном мире.
		AICF_LoadoutRecipe probe = new AICF_LoadoutRecipe();
		probe.m_sCharacter = SCR_ResourceNameUtils.GetPrefabName(character);
		probe.m_sFaction = binding.m_Recipe.m_sFaction;
		AICF_LoadoutDraft draft = new AICF_LoadoutDraft();
		if (!draft.Build(probe, catalog, reason))
			return false;
		int ignored;
		bool compatible = AICF_LoadoutInventory.Restore(draft.GetCharacter(), binding.m_sInventory) &&
			AICF_LoadoutInventory.Signature(draft.GetCharacter(), catalog, ignored) == binding.m_sSignature;
		draft.Clear();
		reason = "RANDOMIZER_INVENTORY_INCOMPATIBLE";
		if (!compatible || !character || character.GetID() != identity || controller.IsPlayerControlled() ||
			!AICF_GroupRuntime.IsAliveCharacter(character) || !rpl.IsMaster() ||
			!control.GetControlAIAgent() || control.GetControlAIAgent().GetParentGroup() != group ||
			SCR_Faction.GetEntityFaction(character) != faction)
			return false;
		reason = "INVENTORY_APPLY_FAILED";
		if (!AICF_LoadoutInventory.Restore(character, binding.m_sInventory) ||
			AICF_LoadoutInventory.Signature(character, catalog, ignored) != binding.m_sSignature)
			return false;
		// Отряд ещё не передан gameplay. При ошибке вызывающий владелец уничтожает
		// именно этот donor/deployment и выполняет штатный economy rollback.
		AICF_Stage4Diagnostics.Info("LOADOUT_APPLIED", string.Format(
			"faction=%1 character=%2 revision=%3 cost=%4", faction.GetFactionKey(), identity, binding.m_iRevision, binding.m_iCost));
		reason = string.Empty;
		return true;
	}
}
