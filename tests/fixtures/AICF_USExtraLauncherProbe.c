// Только изолированный WCS stage. Инвентарь всех ролей до/после изменения.
modded class AICF_MatchController
{
	protected bool m_bAICFExtraLATDone;
	protected int m_iAICFExtraLATFailures;
	protected int m_iAICFExtraLATChecks;

	protected void AICF_ExtraLATCheck(string rule, bool passed)
	{
		m_iAICFExtraLATChecks++;
		if (!passed) m_iAICFExtraLATFailures++;
		Print(string.Format("[AICF][EXTRA_LAT_CHECK] rule=%1 passed=%2", rule, passed));
	}

	override protected void Update()
	{
		super.Update();
		string enabled;
		if (m_bAICFExtraLATDone || !m_bRosterReady || !System.GetCLIParam("aicfExtraLATProbe", enabled) || enabled != "1") return;
		m_bAICFExtraLATDone = true;
		array<SCR_CampaignFaction> factions = {m_USFaction, m_USSRFaction};
		foreach (SCR_CampaignFaction faction : factions)
		{
			AICF_LoadoutCatalog catalog = new AICF_LoadoutCatalog(faction);
			for (int index; index < 10; index++)
			{
				AICF_LoadoutRecipe recipe = new AICF_LoadoutRecipe();
				string role, reason;
				recipe.m_sCharacter = m_GroupSpawner.ResolveRecruitPrefab(faction, index, role);
				recipe.m_sFaction = AICF_ContentProfile.GetActive().GetStableFactionKey(faction.GetFactionKey());
				AICF_LoadoutDraft draft = new AICF_LoadoutDraft();
				bool built = draft.Build(recipe, catalog, reason);
				if (!built) m_iAICFExtraLATFailures++;
				Print(string.Format("[AICF][EXTRA_LAT_BUILD] faction=%1 index=%2 role=%3 built=%4 reason=%5", recipe.m_sFaction, index, role, built, reason));
				if (built)
				{
					AICF_ExtraLATCheck("VALID_" + recipe.m_sFaction + index, AICF_WCSInfantryEquipment.ValidateWCS(draft.GetCharacter(), faction.GetFactionKey(), recipe.m_sCharacter));
					AICF_LoadoutNavigation tree = new AICF_LoadoutNavigation();
					tree.Build(draft.GetCharacter(), catalog);
					array<int> locations = {}, slots = {};
					tree.Rows("", locations, slots);
					foreach (int row, int location : locations)
					{
						BaseInventoryStorageComponent storage = tree.m_Locations[location].m_Storage;
						WeaponSlotComponent slot = AICF_LoadoutSlotView.WeaponSlot(storage, slots[row]);
						if (!slot || slot.GetWeaponSlotType() != "primary") continue;
						IEntity item = storage.Get(slots[row]);
						ResourceName prefab;
						int ammo;
						if (item)
						{
							prefab = SCR_ResourceNameUtils.GetPrefabName(item);
							BaseWeaponComponent weapon = BaseWeaponComponent.Cast(item.FindComponent(BaseWeaponComponent));
							if (weapon && weapon.GetCurrentMuzzle()) ammo = weapon.GetCurrentMuzzle().GetAmmoCount();
						}
						Print(string.Format("[AICF][EXTRA_LAT_SLOT] faction=%1 index=%2 weapon_slot=%3 prefab=%4 ammo=%5", recipe.m_sFaction, index, slot.GetWeaponSlotIndex(), prefab, ammo));
						if (recipe.m_sFaction == "US" && (index == 6 || index == 7) && slot.GetWeaponSlotIndex() == 1)
							AICF_ExtraLATCheck("ADDED_" + index, prefab == "{9C5C20FB0E01E64F}Prefabs/Weapons/Launchers/M72/Launcher_M72A3.et" && ammo == 1);
						if (recipe.m_sFaction == "USSR" && (index == 6 || index == 7) && slot.GetWeaponSlotIndex() == 1)
							AICF_ExtraLATCheck("USSR_UNCHANGED_" + index, prefab.IsEmpty());
					}
					tree.Clear();
					if (recipe.m_sFaction == "US" && (index == 6 || index == 7))
					{
						int ignored;
						string signature = AICF_LoadoutInventory.Signature(draft.GetCharacter(), catalog, ignored);
						string snapshot;
						AICF_LoadoutDraft restored = new AICF_LoadoutDraft();
						bool same = AICF_LoadoutInventory.Capture(draft.GetCharacter(), snapshot) && restored.Build(recipe, catalog, reason) &&
							AICF_LoadoutInventory.Restore(restored.GetCharacter(), snapshot) && signature == AICF_LoadoutInventory.Signature(restored.GetCharacter(), catalog, ignored);
						AICF_ExtraLATCheck("RESTORE_" + index, same);
						restored.Clear();
					}
				}
				draft.Clear();
			}
		}
		Print(string.Format("[AICF][EXTRA_LAT_DONE] checks=%1 failures=%2", m_iAICFExtraLATChecks, m_iAICFExtraLATFailures));
		GetGame().RequestClose();
	}
}
