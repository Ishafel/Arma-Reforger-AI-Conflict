class AICF_LoadoutService
{
	protected ref AICF_LoadoutStore m_Store = new AICF_LoadoutStore();
	protected ref array<SCR_PlayerController> m_aViewers = {};
	protected AICF_MatchController m_Match;

	void AICF_LoadoutService(AICF_MatchController match) { m_Match = match; }

	void Request(SCR_PlayerController player, int token, int slotId, int member, int revision, int operation, string payload)
	{
		AICF_GroupSlot slot;
		SCR_CampaignFaction faction;
		ResourceName source;
		string role;
		string reason = "MATCH_OR_FACTION_UNAVAILABLE";
		if (!m_Match.ResolveLoadoutContext(player, slotId, member, slot, faction, source, role))
		{
			player.AICF_LoadoutResult(token, false, reason);
			return;
		}
		AICF_LoadoutRecipe recipe = new AICF_LoadoutRecipe();
		recipe.m_sProfile = AICF_ContentProfile.GetActive().GetProfileKey();
		recipe.m_sFaction = AICF_ContentProfile.GetActive().GetStableFactionKey(faction.GetFactionKey());
		recipe.m_sCharacter = source;
		recipe.m_sName = "{AICF:AICF_UI_Loadout_6c772ac0}" + (member + 1).ToString();
		AICF_LoadoutBinding current = slot.GetLoadout(member);
		bool accepted = operation == 0;
		if (operation == 1 || operation == 2)
		{
			AICF_LoadoutRecipe candidate;
			if (operation == 1)
				candidate = AICF_LoadoutRecipe.Decode(payload);
			else
			{
				int id = payload.ToInt(-1);
				if (id >= 0 && id.ToString() == payload)
					candidate = m_Store.Get(id);
			}
			reason = "INVALID_RECIPE";
			if (candidate && candidate.m_sProfile == recipe.m_sProfile && candidate.m_sFaction == recipe.m_sFaction &&
				candidate.m_sCharacter == source)
			{
				if (revision != slot.GetLoadoutRevision())
					reason = "REVISION_CONFLICT";
				else if (operation == 1 && current && current.m_Recipe.Encode() == candidate.Encode())
					accepted = true;
				else
				{
					AICF_LoadoutBinding binding = Validate(candidate, faction, reason);
					if (binding && operation == 2)
					{
						recipe = candidate;
						accepted = true;
					}
					else if (binding && m_Store.Save(candidate, reason))
						accepted = slot.SetLoadout(member, binding, revision);
				}
			}
		}
		current = slot.GetLoadout(member);
		if (current && !(operation == 2 && accepted))
			recipe = current.m_Recipe;
		int cost;
		if (current)
			cost = current.m_iCost;
		player.AICF_LoadoutView(slotId, member, slot.GetLoadoutRevision(), recipe.Encode(), m_Store.List(recipe), cost);
		if (!m_aViewers.Contains(player))
			m_aViewers.Insert(player);
		if (accepted)
		{
			reason = "READY";
			if (operation == 1)
				reason = "SAVED";
			if (operation == 2)
				reason = "TEMPLATE_LOADED";
		}
		player.AICF_LoadoutResult(token, accepted, reason);
		AICF_Stage4Diagnostics.Info("LOADOUT_REQUEST_RESULT", string.Format(
			"player=%1 faction=%2 slot=%3 member=%4 token=%5 revision=%6 accepted=%7 reason=%8",
			player.GetPlayerId(), faction.GetFactionKey(), slotId, member, token, slot.GetLoadoutRevision(), accepted, reason));
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
		if (!AICF_LoadoutInventory.HasUsableWeapons(draft.GetCharacter(), reason))
			return null;
		AICF_LoadoutBinding binding = new AICF_LoadoutBinding();
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
			if (!m_Match.ResolveLoadoutContext(player, player.AICF_LoadoutSlot(), player.AICF_LoadoutMember(), slot, faction, source, role) ||
				AICF_ContentProfile.GetActive().GetStableFactionKey(faction.GetFactionKey()) != player.AICF_LoadoutFaction())
			{
				player.AICF_LoadoutView(-1, -1, -1, string.Empty, string.Empty, 0);
				m_aViewers.Remove(i);
				continue;
			}
			AICF_LoadoutBinding binding = slot.GetLoadout(player.AICF_LoadoutMember());
			if (slot.GetLoadoutRevision() != player.AICF_LoadoutRevision())
			{
				if (binding)
					player.AICF_LoadoutView(slot.GetSlotId(), player.AICF_LoadoutMember(), slot.GetLoadoutRevision(),
						binding.m_Recipe.Encode(), m_Store.List(binding.m_Recipe), binding.m_iCost);
				else
					player.AICF_LoadoutView(slot.GetSlotId(), player.AICF_LoadoutMember(), slot.GetLoadoutRevision(),
						player.AICF_LoadoutData(), player.AICF_LoadoutLibrary(), 0);
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
