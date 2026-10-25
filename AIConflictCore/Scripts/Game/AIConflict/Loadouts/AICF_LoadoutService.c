class AICF_LoadoutService
{
	protected ref AICF_LoadoutStore m_Store = new AICF_LoadoutStore();
	protected ref array<SCR_PlayerController> m_aViewers = {};
	protected AICF_MatchController m_Match;

	void AICF_LoadoutService(AICF_MatchController match) { m_Match = match; }

	// 0 read, 1 apply member, 2 load draft, 3 save member,
	// 4 delete, 5 position to all squads, 6 save squad, 7 apply squad,
	// 8 inspect, 9 one complete loadout to every member of the current squad.
	void Request(SCR_PlayerController player, int token, int slotId, int member, int revision, int operation, string payload,
		string targetFaction = "", int libraryRevision = -1)
	{
		if (!Replication.IsServer() || !player || !m_Match) return;
		AICF_GroupSlot slot;
		SCR_CampaignFaction faction;
		ResourceName source;
		string role;
		string reason = "MATCH_OR_FACTION_UNAVAILABLE";
		if (!m_Match.ResolveLoadoutContext(player, slotId, member, slot, faction, source, role, targetFaction))
		{
			player.AICF_LoadoutResult(token, false, reason);
			return;
		}
		AICF_LoadoutRecipe recipe = DefaultRecipe(faction, source, member);
		AICF_LoadoutRecipe libraryContext = recipe;
		AICF_LoadoutBinding current = slot.GetLoadout(member);
		bool accepted = operation == 0;
		bool libraryOperation = operation == 2 || operation == 3 || operation == 4 || operation == 6 || operation == 7 || operation == 8;
		if (operation < 0 || operation > 9) reason = "INVALID_REQUEST";
		else if (operation != 0 && revision != slot.GetLoadoutRevision()) reason = "REVISION_CONFLICT";
		else if (libraryOperation && libraryRevision != m_Store.GetRevision()) reason = "LIBRARY_CONFLICT";
		else if (operation == 1 || operation == 3 || operation == 5 || operation == 9)
		{
			AICF_LoadoutRecipe candidate = AICF_LoadoutRecipe.Decode(payload);
			reason = "INVALID_RECIPE";
			if (candidate && candidate.m_sProfile == recipe.m_sProfile && candidate.m_sFaction == recipe.m_sFaction &&
				KnownSource(player, slotId, candidate))
			{
				AICF_LoadoutBinding binding = Validate(candidate, faction, reason);
				if (binding && !AICF_LoadoutTransfer.Fits(binding, faction, source, reason)) binding = null;
				if (binding && revision != slot.GetLoadoutRevision())
				{
					reason = "REVISION_CONFLICT";
					binding = null;
				}
				if (binding && operation == 3) accepted = m_Store.Save(candidate, reason);
				else if (binding && operation == 1) accepted = slot.SetLoadout(member, binding, revision);
				else if (binding && operation == 5) accepted = ApplyRole(player, candidate, binding, member, reason);
				else if (binding && operation == 9) accepted = ApplyMembers(player, slotId, candidate, binding, reason);
			}
		}
		else if (operation == 2 || operation == 4 || operation == 7 || operation == 8)
		{
			int id = payload.ToInt(-1);
			int inspectMember = member;
			string canonical = id.ToString();
			if (operation == 8)
			{
				array<string> parts = {};
				payload.Split(":", parts, false);
				id = -1;
				if (parts.Count() == 2)
				{
					id = parts[0].ToInt(-1);
					inspectMember = parts[1].ToInt(-1);
				}
				canonical = id.ToString() + ":" + inspectMember.ToString();
			}
			AICF_LoadoutPreset preset;
			if (id >= 0 && canonical == payload && inspectMember >= 0 && inspectMember < AICF_Stage1Config.MAX_GROUP_SIZE) preset = m_Store.GetPreset(id);
			reason = "TEMPLATE_UNAVAILABLE";
			if (preset && preset.m_sProfile == recipe.m_sProfile && preset.m_sFaction == recipe.m_sFaction)
			{
				if (operation == 4) accepted = m_Store.Delete(id, reason);
				else if (operation == 7 && preset.m_sKind == "squad")
					accepted = ApplySquad(player, slotId, AICF_SquadLoadout.Decode(preset.m_sData), reason);
				else if (operation == 2 || operation == 8)
				{
					AICF_LoadoutRecipe loaded;
					ResourceName inspectSource = source;
					if (preset.m_sKind == "member" && inspectMember == member) loaded = m_Store.Get(id);
					else if (operation == 8)
					{
						AICF_SquadLoadout squad = AICF_SquadLoadout.Decode(preset.m_sData);
						AICF_GroupSlot inspectSlot;
						SCR_CampaignFaction inspectFaction;
						string inspectRole;
						if (squad && m_Match.ResolveLoadoutContext(player, slotId, inspectMember, inspectSlot, inspectFaction,
							inspectSource, inspectRole, recipe.m_sFaction, true) && inspectSlot == slot && inspectFaction == faction)
							loaded = AICF_LoadoutRecipe.Decode(squad.m_aRecipes[inspectMember]);
					}
					if (loaded && KnownSource(player, slotId, loaded))
					{
						AICF_LoadoutBinding inspected = Validate(loaded, faction, reason);
						if (inspected && AICF_LoadoutTransfer.Fits(inspected, faction, inspectSource, reason)) { recipe = loaded; accepted = true; }
					}
				}
			}
		}
		else if (operation == 6) accepted = SaveSquad(player, slotId, recipe.m_sFaction, payload, reason);
		current = slot.GetLoadout(member);
		if (current && !((operation == 2 || operation == 8) && accepted)) recipe = current.m_Recipe;
		int cost;
		if (current) cost = current.m_iCost;
		if (operation != 8) libraryContext = recipe;
		player.AICF_LoadoutView(slotId, member, slot.GetLoadoutRevision(), recipe.Encode(), m_Store.List(libraryContext, slot), cost);
		if (!m_aViewers.Contains(player)) m_aViewers.Insert(player);
		if (accepted)
		{
			array<string> statuses = {"READY", "APPLIED", "TEMPLATE_LOADED", "TEMPLATE_SAVED", "TEMPLATE_DELETED", "ROLE_APPLIED", "SQUAD_SAVED", "SQUAD_APPLIED", "TEMPLATE_INSPECTED", "SQUAD_APPLIED"};
			reason = statuses[operation];
		}
		player.AICF_LoadoutResult(token, accepted, reason);
		AICF_Stage4Diagnostics.Info("LOADOUT_REQUEST_RESULT", string.Format(
			"player=%1 faction=%2 slot=%3 member=%4 token=%5 revision=%6 accepted=%7 reason=%8",
			player.GetPlayerId(), faction.GetFactionKey(), slotId, member, token, slot.GetLoadoutRevision(), accepted, reason));
	}

	static AICF_LoadoutRecipe DefaultRecipe(SCR_CampaignFaction faction, ResourceName source, int member)
	{
		AICF_LoadoutRecipe recipe = new AICF_LoadoutRecipe();
		recipe.m_sProfile = AICF_ContentProfile.GetActive().GetProfileKey();
		recipe.m_sFaction = AICF_ContentProfile.GetActive().GetStableFactionKey(faction.GetFactionKey());
		recipe.m_sCharacter = source;
		recipe.m_sName = "{AICF:AICF_UI_Loadout_6c772ac0}" + (member + 1).ToString();
		return recipe;
	}

	protected bool KnownSource(SCR_PlayerController player, int slotId, AICF_LoadoutRecipe recipe)
	{
		if (!recipe || recipe.m_sProfile != AICF_ContentProfile.GetActive().GetProfileKey()) return false;
		for (int i; i < AICF_Stage1Config.MAX_GROUP_SIZE; i++)
		{
			AICF_GroupSlot slot;
			SCR_CampaignFaction faction;
			ResourceName source;
			string role;
			if (m_Match.ResolveLoadoutContext(player, slotId, i, slot, faction, source, role, recipe.m_sFaction, true) &&
				source == recipe.m_sCharacter) return true;
		}
		return false;
	}

	protected bool ApplyMembers(SCR_PlayerController player, int slotId, AICF_LoadoutRecipe recipe, AICF_LoadoutBinding binding, out string reason)
	{
		AICF_LoadoutBatch batch = new AICF_LoadoutBatch();
		reason = "INCOMPATIBLE_TARGET";
		for (int i; i < AICF_Stage1Config.MAX_GROUP_SIZE; i++)
		{
			AICF_GroupSlot slot;
			SCR_CampaignFaction faction;
			ResourceName source;
			string role;
			if (!m_Match.ResolveLoadoutContext(player, slotId, i, slot, faction, source, role, recipe.m_sFaction, true) ||
				!AICF_LoadoutTransfer.Fits(binding, faction, source, reason)) return false;
			batch.Add(slot, i, binding);
		}
		reason = "REVISION_CONFLICT";
		return batch.Commit();
	}

	protected bool ApplyRole(SCR_PlayerController player, AICF_LoadoutRecipe recipe, AICF_LoadoutBinding binding, int member, out string reason)
	{
		AICF_LoadoutBatch batch = new AICF_LoadoutBatch();
		reason = "INCOMPATIBLE_TARGET";
		for (int i; i < AICF_Stage1Config.GROUP_SLOTS_PER_FACTION; i++)
		{
			AICF_GroupSlot slot;
			SCR_CampaignFaction faction;
			ResourceName source;
			string role;
			if (!m_Match.ResolveLoadoutContext(player, i, member, slot, faction, source, role, recipe.m_sFaction, true) ||
				!AICF_LoadoutTransfer.Fits(binding, faction, source, reason)) return false;
			batch.Add(slot, member, binding);
		}
		reason = "REVISION_CONFLICT";
		return batch.Commit();
	}

	protected bool SaveSquad(SCR_PlayerController player, int slotId, string factionKey, string name, out string reason)
	{
		reason = "INVALID_RECIPE";
		if (!AICF_LoadoutPreset.ValidName(name)) return false;
		AICF_SquadLoadout squad = new AICF_SquadLoadout();
		squad.m_sName = name;
		squad.m_sProfile = AICF_ContentProfile.GetActive().GetProfileKey();
		squad.m_sFaction = factionKey;
		for (int i; i < AICF_Stage1Config.MAX_GROUP_SIZE; i++)
		{
			AICF_GroupSlot slot;
			SCR_CampaignFaction faction;
			ResourceName source;
			string role;
			if (!m_Match.ResolveLoadoutContext(player, slotId, i, slot, faction, source, role, factionKey, true)) return false;
			AICF_LoadoutBinding binding = slot.GetLoadout(i);
			AICF_LoadoutRecipe recipe = DefaultRecipe(faction, source, i);
			if (binding) recipe = binding.m_Recipe;
			if (!KnownSource(player, slotId, recipe)) return false;
			AICF_LoadoutBinding checked = Validate(recipe, faction, reason);
			if (!checked || !AICF_LoadoutTransfer.Fits(checked, faction, source, reason)) return false;
			squad.m_aRecipes.Insert(recipe.Encode());
		}
		AICF_LoadoutPreset preset = new AICF_LoadoutPreset();
		preset.m_sKind = "squad";
		preset.m_sName = name;
		preset.m_sProfile = squad.m_sProfile;
		preset.m_sFaction = factionKey;
		preset.m_sData = squad.Encode();
		return m_Store.SavePreset(preset, reason);
	}

	protected bool ApplySquad(SCR_PlayerController player, int slotId, AICF_SquadLoadout squad, out string reason)
	{
		reason = "INVALID_RECIPE";
		if (!squad || squad.m_sProfile != AICF_ContentProfile.GetActive().GetProfileKey()) return false;
		AICF_LoadoutBatch batch = new AICF_LoadoutBatch();
		for (int i; i < AICF_Stage1Config.MAX_GROUP_SIZE; i++)
		{
			AICF_GroupSlot slot;
			SCR_CampaignFaction faction;
			ResourceName source;
			string role;
			AICF_LoadoutRecipe recipe = AICF_LoadoutRecipe.Decode(squad.m_aRecipes[i]);
			if (!m_Match.ResolveLoadoutContext(player, slotId, i, slot, faction, source, role, squad.m_sFaction, true) ||
				!recipe || !KnownSource(player, slotId, recipe)) return false;
			AICF_LoadoutBinding binding = Validate(recipe, faction, reason);
			if (!binding) return false;
			if (!AICF_LoadoutTransfer.Fits(binding, faction, source, reason)) return false;
			batch.Add(slot, i, binding);
		}
		reason = "REVISION_CONFLICT";
		return batch.Commit();
	}

	static AICF_LoadoutBinding Validate(AICF_LoadoutRecipe recipe, SCR_CampaignFaction faction, out string reason, bool personal = false)
	{
		AICF_LoadoutCatalog catalog = new AICF_LoadoutCatalog(faction);
		if (personal) catalog.SetPersonalRules();
		AICF_LoadoutDraft draft = new AICF_LoadoutDraft();
		if (!draft.Build(recipe, catalog, reason))
			return null;
		if (personal && !AICF_LoadoutInventory.AllowsPersonalInventory(draft.GetCharacter(), catalog))
		{
			reason = "ITEM_NOT_ALLOWED";
			return null;
		}
		AICF_LoadoutBinding binding = new AICF_LoadoutBinding();
		reason = "INVENTORY_RESTORE_FAILED";
		if (!AICF_LoadoutInventory.PackCargoLocal(draft.GetCharacter())) return null;
		binding.m_Recipe = AICF_LoadoutRecipe.Decode(recipe.Encode());
		int catalogCost;
		binding.m_sSignature = AICF_LoadoutInventory.Signature(draft.GetCharacter(), catalog, catalogCost);
		// Каталожная цена не является доплатой за экипировку управляемого бойца.
		binding.m_iCost = 0;
		reason = "INVENTORY_READBACK_FAILED";
		if (binding.m_sSignature.IsEmpty() ||
			!AICF_LoadoutInventory.Capture(draft.GetCharacter(), binding.m_sInventory))
			return null;
		// Проверка повторной выдачи на независимом экземпляре до записи библиотеки.
		AICF_LoadoutRecipe empty = AICF_LoadoutRecipe.Decode(recipe.Encode());
		while (!empty.m_aPaths.IsEmpty())
			empty.Undo();
		if (!draft.Build(empty, catalog, reason))
			return null;
		reason = "INVENTORY_RESTORE_FAILED";
		if (!AICF_LoadoutInventory.Restore(draft.GetCharacter(), binding.m_sInventory))
			return null;
		int cost;
		reason = "INVENTORY_SIGNATURE_MISMATCH";
		if (AICF_LoadoutInventory.Signature(draft.GetCharacter(), catalog, cost) != binding.m_sSignature)
			return null;
		draft.Clear();
		reason = string.Empty;
		return binding;
	}

	// Долговечный owner snapshot обновляется также при записи другим союзником.
	void Update()
	{
		for (int i = m_aViewers.Count() - 1; i >= 0; i--)
		{
			SCR_PlayerController player = m_aViewers[i];
			if (!player || player.AICF_LoadoutSlot() == AICF_PersonalLoadout.SLOT)
			{
				m_aViewers.Remove(i);
				continue;
			}
			AICF_GroupSlot slot;
			SCR_CampaignFaction faction;
			ResourceName source;
			string role;
			if (!m_Match.ResolveLoadoutContext(player, player.AICF_LoadoutSlot(), player.AICF_LoadoutMember(), slot, faction, source, role, player.AICF_LoadoutFaction()) ||
				AICF_ContentProfile.GetActive().GetStableFactionKey(faction.GetFactionKey()) != player.AICF_LoadoutFaction())
			{
				player.AICF_LoadoutView(-1, -1, -1, string.Empty, string.Empty, 0);
				m_aViewers.Remove(i);
				continue;
			}
			AICF_LoadoutBinding binding = slot.GetLoadout(player.AICF_LoadoutMember());
			AICF_LoadoutRecipe viewRecipe = DefaultRecipe(faction, source, player.AICF_LoadoutMember());
			if (binding) viewRecipe = binding.m_Recipe;
			string library = m_Store.List(viewRecipe, slot);
			if (slot.GetLoadoutRevision() != player.AICF_LoadoutRevision() || library != player.AICF_LoadoutLibrary())
			{
				if (binding)
					player.AICF_LoadoutView(slot.GetSlotId(), player.AICF_LoadoutMember(), slot.GetLoadoutRevision(),
						binding.m_Recipe.Encode(), library, binding.m_iCost);
				else
					player.AICF_LoadoutView(slot.GetSlotId(), player.AICF_LoadoutMember(), slot.GetLoadoutRevision(),
						viewRecipe.Encode(), library, 0);
			}
		}
	}

	void Stop()
	{
		foreach (SCR_PlayerController player : m_aViewers)
		{
			if (player)
				player.AICF_LoadoutView(-1, -1, -1, string.Empty, string.Empty, 0);
		}
		m_aViewers.Clear();
		m_Match = null;
	}
}
