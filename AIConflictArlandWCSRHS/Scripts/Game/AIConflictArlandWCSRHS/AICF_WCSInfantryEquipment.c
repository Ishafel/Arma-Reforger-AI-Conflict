// Повторно используем только защищённые local inventory helpers RHS adapter.
// Собственные eligibility, kit и commit не затрагивают FIA или другие profiles.
class AICF_WCSInfantryEquipment : AICF_RHSPMCEquipment
{
	static bool BuildDefault(IEntity model, FactionKey faction, ResourceName source)
	{
		if (!model || model.GetWorld() == GetGame().GetWorld()) return false;
		if (!source.Contains("/RHS_USAF_USMC_MEF/") && !source.Contains("/RHS_AFRF/MSV/")) return true;
		bool dressed = RemoveOldAmmunition(model) && RemoveOptics(model, true) && DressWCS(model, faction, source) && RemoveOptics(model, false);
		bool armed = dressed && ArmWCS(model, faction, source);
		bool patched = armed && AICF_WCSPatches.ApplyLocal(model, faction, source);
		bool valid = patched && ValidateWCS(model, faction, source);
		if (!valid) Print(string.Format("[AICF][WCS][DEFAULT_FAILED] faction=%1 dressed=%2 armed=%3 patched=%4 source=%5", faction, dressed, armed, patched, source), LogLevel.ERROR);
		return valid;
	}

	static bool EligibleWCS(SCR_ChimeraCharacter character, SCR_AIGroup group)
	{
		if (!Replication.IsServer() || !character || !group ||
			character.GetWorld() != GetGame().GetWorld() ||
			!AICF_WCSRHSContentProfile.Cast(AICF_ContentProfile.GetActive())) return false;
		FactionKey faction = character.GetFactionKey();
		ResourceName source = SCR_ResourceNameUtils.GetPrefabName(character);
		if (!AICF_WCSInfantryKit.SupportsRole(source)) return false;
		if ((faction != "RHS_USAF" || !source.Contains("/RHS_USAF_USMC_MEF/")) &&
			(faction != "RHS_AFRF" || !source.Contains("/RHS_AFRF/MSV/"))) return false;
		RplComponent rpl = RplComponent.Cast(character.FindComponent(RplComponent));
		CharacterControllerComponent controller = character.GetCharacterController();
		AIControlComponent control = AIControlComponent.Cast(character.FindComponent(AIControlComponent));
		return rpl && rpl.IsMaster() && controller && !controller.IsPlayerControlled() && !controller.IsDead() &&
			control && control.GetControlAIAgent() && control.GetControlAIAgent().GetParentGroup() == group;
	}

	protected static bool DressWCS(IEntity model, FactionKey faction, ResourceName source)
	{
		if (!model || model.GetWorld() == GetGame().GetWorld()) return false;
		InventoryStorageManagerComponent manager = InventoryStorageManagerComponent.Cast(model.FindComponent(InventoryStorageManagerComponent));
		if (!manager) return false;
		array<IEntity> retained = {};
		CollectCargo(model, retained);
		foreach (IEntity payload : retained)
		{
			InventoryItemComponent component = InventoryItemComponent.Cast(payload.FindComponent(InventoryItemComponent));
			if (!component || !component.GetParentSlot()) return false;
			component.GetParentSlot().DetachEntity();
			if (component.GetParentSlot()) return false;
		}
		set<BaseInventoryStorageComponent> roots = new set<BaseInventoryStorageComponent>();
		SCR_PlayerArsenalLoadout.FindStorageComponents(model, roots);
		foreach (BaseInventoryStorageComponent root : roots)
		{
			array<InventoryItemComponent> worn = {};
			root.GetOwnedItems(worn, false);
			foreach (InventoryItemComponent item : worn)
			{
				IEntity old = item.GetOwner();
				ResourceName prefab = SCR_ResourceNameUtils.GetPrefabName(old);
				if (!old.FindComponent(BaseLoadoutClothComponent)) continue;
				// Функциональные gadgets остаются в своих native slots.
				if (prefab.Contains("/Items/Equipment/") && !prefab.Contains("/Backpacks/")) continue;
				if (old.FindComponent(BaseRadioComponent)) continue;
				if (!AICF_LoadoutInventory.DeleteLocal(old)) return false;
			}
		}
		array<ResourceName> clothes = {};
		AICF_WCSInfantryKit.Clothes(faction, source, clothes);
		foreach (ResourceName clothing : clothes)
		{
			if (clothing.Contains("/Eyewear/") && !RemoveOptics(model, true)) return false;
			IEntity target = model;
			// RHS character не имеет отдельного eye slot: и опущенные очки,
			// и поднятые 6Б50 живут в helmet node. Положение задаёт native prefab.
			if (clothing.Contains("/Eyewear/"))
			{
				target = Helmet(model);
				if (!target) return false;
			}
			if (clothing.Contains("/Eyewear/"))
			{
				if (!EquipEyewear(target, clothing, manager)) return false;
			}
			else if (!EquipLocal(target, clothing, manager)) return false;
			if (clothing == AICF_WCSInfantryKit.Vest(faction, source) && !RemoveBackPanel(model)) return false;
		}
		foreach (IEntity cargo : retained)
		{
			if (!StoreLocal(model, cargo, manager))
			{
				Print("[AICF][WCS][CLOTHING_SPACE] cargo=" + SCR_ResourceNameUtils.GetPrefabName(cargo), LogLevel.WARNING);
				return false;
			}
		}
		return !clothes.IsEmpty();
	}

	protected static bool EquipEyewear(IEntity target, ResourceName prefab, InventoryStorageManagerComponent manager)
	{
		if (!target || target.GetWorld() == GetGame().GetWorld()) return false;
		set<BaseInventoryStorageComponent> storages = new set<BaseInventoryStorageComponent>();
		SCR_PlayerArsenalLoadout.FindStorageComponents(target, storages);
		foreach (BaseInventoryStorageComponent storage : storages)
		{
			for (int slot; slot < storage.GetSlotsCount(); slot++)
			{
				if (AICF_LoadoutClothing.Area(storage, slot) == LoadoutGooglesArea && !storage.Get(slot) &&
					AICF_LoadoutInventory.InsertLocal(prefab, storage, slot, manager)) return true;
			}
		}
		Print(string.Format("[AICF][WCS][EYEWEAR_SLOT_FAILED] target=%1 prefab=%2", SCR_ResourceNameUtils.GetPrefabName(target), prefab), LogLevel.WARNING);
		return false;
	}

	static IEntity Helmet(IEntity model)
	{
		array<IEntity> items = {};
		AICF_RHSPMCArmament.Items(model, items);
		foreach (IEntity item : items)
		{
			if (SCR_ResourceNameUtils.GetPrefabName(item).Contains("/WCS_Variants/Helmet_")) return item;
		}
		return null;
	}

	protected static bool RemoveBackPanel(IEntity model)
	{
		if (!model || model.GetWorld() == GetGame().GetWorld()) return false;
		array<IEntity> items = {};
		AICF_RHSPMCArmament.Items(model, items);
		foreach (IEntity item : items)
		{
			if (item && SCR_ResourceNameUtils.GetPrefabName(item).Contains("ZipOnPanel") && !AICF_LoadoutInventory.DeleteLocal(item)) return false;
		}
		return true;
	}
	protected static bool RemoveOptics(IEntity model, bool eyewear)
	{
		if (!model || model.GetWorld() == GetGame().GetWorld()) return false;
		array<IEntity> items = {};
		AICF_RHSPMCArmament.Items(model, items);
		foreach (IEntity item : items)
		{
			if (!item) continue;
			ResourceName prefab = SCR_ResourceNameUtils.GetPrefabName(item);
			if (AICF_WCSInfantryKit.IsNightVision(prefab) || (eyewear && prefab.Contains("/Eyewear/")))
			{
				if (!AICF_LoadoutInventory.DeleteLocal(item)) return false;
			}
		}
		return true;
	}

	protected static bool RemoveOldAmmunition(IEntity model)
	{
		if (!model || model.GetWorld() == GetGame().GetWorld()) return false;
		BaseWeaponComponent old = AICF_RHSPMCArmament.Primary(model);
		InventoryStorageManagerComponent manager = InventoryStorageManagerComponent.Cast(model.FindComponent(InventoryStorageManagerComponent));
		if (!old || !manager) return false;
		array<BaseMuzzleComponent> oldMuzzles = {};
		old.GetMuzzlesList(oldMuzzles);
		array<IEntity> oldSpare = {};
		array<IEntity> all = {};
		AICF_RHSPMCArmament.Items(model, all);
		foreach (BaseMuzzleComponent oldMuzzle : oldMuzzles)
		{
			foreach (IEntity item : all)
			{
				BaseMagazineComponent mag = BaseMagazineComponent.Cast(item.FindComponent(BaseMagazineComponent));
				if (AICF_RHSPMCArmament.Compatible(mag, oldMuzzle) && mag != oldMuzzle.GetMagazine() && !oldSpare.Contains(item))
					oldSpare.Insert(item);
			}
		}
		foreach (IEntity spare : oldSpare)
		{
			if (!AICF_LoadoutInventory.DeleteLocal(spare)) return false;
		}
		return true;
	}

	protected static bool ArmWCS(IEntity model, FactionKey faction, ResourceName source)
	{
		if (!model || model.GetWorld() == GetGame().GetWorld()) return false;
		BaseWeaponComponent old = AICF_RHSPMCArmament.Primary(model);
		InventoryStorageManagerComponent manager = InventoryStorageManagerComponent.Cast(model.FindComponent(InventoryStorageManagerComponent));
		if (!old || !manager) return false;
		IEntity replacement = AICF_RHSPMCArmament.ReplaceLocal(old.GetOwner(), AICF_WCSInfantryKit.Weapon(faction, source), manager);
		if (!replacement) return false;
		BaseWeaponComponent weapon = BaseWeaponComponent.Cast(replacement.FindComponent(BaseWeaponComponent));
		if (!weapon) return false;
		array<BaseMuzzleComponent> muzzles = {};
		weapon.GetMuzzlesList(muzzles);
		foreach (int index, BaseMuzzleComponent muzzle : muzzles)
		{
			if (!muzzle || !muzzle.GetMagazine() || muzzle.GetAmmoCount() <= 0) return false;
			ResourceName magazine = SCR_ResourceNameUtils.GetPrefabName(muzzle.GetMagazine().GetOwner());
			int count = 6;
			if (index == 0) count = AICF_WCSInfantryKit.MinimumMagazines(source);

			if (!AddMagazinesLocal(model, manager, magazine, count)) return false;
		}
		return !muzzles.IsEmpty();
	}

	static bool ValidateWCS(IEntity model, FactionKey faction, ResourceName source)
	{
		if (!HasEquippedArmor(model, faction)) return false;
		BaseWeaponComponent weapon = AICF_RHSPMCArmament.Primary(model);
		if (!weapon || SCR_ResourceNameUtils.GetPrefabName(weapon.GetOwner()) != AICF_WCSInfantryKit.Weapon(faction, source)) return false;
		array<BaseMuzzleComponent> muzzles = {};
		weapon.GetMuzzlesList(muzzles);
		foreach (int index, BaseMuzzleComponent muzzle : muzzles)
		{
			int count = 6;
			if (index == 0) count = AICF_WCSInfantryKit.MinimumMagazines(source);
			if (!muzzle || muzzle.GetAmmoCount() <= 0 || AICF_RHSPMCArmament.SpareMagazines(model, muzzle) < count) return false;
		}
		array<ResourceName> clothes = {};
		AICF_WCSInfantryKit.Clothes(faction, source, clothes);
		array<IEntity> items = {};
		AICF_RHSPMCArmament.Items(model, items);
		int backpacks;
		foreach (IEntity item : items)
		{
			ResourceName prefab = SCR_ResourceNameUtils.GetPrefabName(item);
			if (AICF_WCSInfantryKit.IsNightVision(prefab)) return false;
			if (prefab.Contains("ZipOnPanel")) return false;
			if (prefab == AICF_WCSInfantryKit.Eyewear(faction, source))
			{
				InventoryItemComponent glasses = InventoryItemComponent.Cast(item.FindComponent(InventoryItemComponent));
				if (!glasses || !glasses.GetParentSlot()) return false;
				IEntity expectedOwner = Helmet(model);
				if (glasses.GetParentSlot().GetStorage().GetOwner() != expectedOwner) return false;
			}
			if (prefab.Contains("/Backpacks/")) backpacks++;
			clothes.RemoveItem(prefab);
		}
		bool backpackValid = backpacks == 0;
		if (AICF_WCSInfantryKit.NeedsBackpack(faction, source)) backpackValid = backpacks == 1;
		return !muzzles.IsEmpty() && clothes.IsEmpty() && backpackValid;
	}

	static bool HasEquippedArmor(IEntity model, FactionKey faction)
	{
		if (!model) return false;
		set<BaseInventoryStorageComponent> roots = new set<BaseInventoryStorageComponent>();
		SCR_PlayerArsenalLoadout.FindStorageComponents(model, roots);
		foreach (BaseInventoryStorageComponent root : roots)
		{
			for (int slot; slot < root.GetSlotsCount(); slot++)
			{
				if (AICF_LoadoutClothing.Area(root, slot) == LoadoutArmoredVestSlotArea &&
					SCR_ResourceNameUtils.GetPrefabName(root.Get(slot)) == AICF_WCSInfantryKit.Armor(faction)) return true;
			}
		}
		return false;
	}

	static bool ApplyWCS(SCR_ChimeraCharacter character, SCR_AIGroup group)
	{
		if (!EligibleWCS(character, group)) return false;
		if (character.m_bAICFWCSKitApplied) return true;
		EntityID identity = character.GetID();
		EntityID groupIdentity = group.GetID();
		ResourceName source = SCR_ResourceNameUtils.GetPrefabName(character);
		FactionKey faction = character.GetFactionKey();
		string before;
		if (!AICF_LoadoutInventory.Capture(character, before))
		{
			Print("[AICF][WCS][CAPTURE_FAILED] source=" + source, LogLevel.ERROR);
			return false;
		}
		AICF_LoadoutCatalog catalog = new AICF_LoadoutCatalog(SCR_CampaignFaction.Cast(SCR_Faction.GetEntityFaction(character)));
		AICF_LoadoutRecipe recipe = new AICF_LoadoutRecipe();
		recipe.m_sCharacter = source;
		recipe.m_sFaction = AICF_ContentProfile.GetActive().GetStableFactionKey(faction);
		AICF_LoadoutDraft draft = new AICF_LoadoutDraft();
		string reason;
		bool built = draft.Build(recipe, catalog, reason);
		bool dressed = built;
		bool armed = built;
		string snapshot;
		bool ready = armed && ValidateWCS(draft.GetCharacter(), faction, source) && AICF_LoadoutInventory.Capture(draft.GetCharacter(), snapshot);
		int ignored;
		string expected;
		if (ready) expected = AICF_LoadoutInventory.Signature(draft.GetCharacter(), catalog, ignored);
		draft.Clear();
		if (!ready || expected.IsEmpty())
		{
			Print(string.Format("[AICF][WCS][KIT_FAILED] entity=%1 built=%2 dressed=%3 armed=%4 source=%5", identity, built, dressed, armed, source), LogLevel.ERROR);
			return false;
		}
		string current;
		if (!EligibleWCS(character, group) || character.GetID() != identity || group.GetID() != groupIdentity ||
			character.GetFactionKey() != faction || SCR_ResourceNameUtils.GetPrefabName(character) != source ||
			!AICF_LoadoutInventory.Capture(character, current) || current != before) return false;
		bool applied = AICF_LoadoutInventory.Restore(character, snapshot) &&
			AICF_LoadoutInventory.Signature(character, catalog, ignored) == expected && ValidateWCS(character, faction, source);
		if (!applied)
		{
			bool rollback = AICF_LoadoutInventory.Restore(character, before);
			Print(string.Format("[AICF][WCS][KIT_COMMIT_FAILED] entity=%1 rollback=%2 source=%3", identity, rollback, source), LogLevel.ERROR);
			return false;
		}
		character.m_bAICFWCSKitApplied = true;
		Print(string.Format("[AICF][WCS][KIT_APPLIED] entity=%1 faction=%2 source=%3 weapon=%4 inventory_verified=1", identity, faction, source, AICF_WCSInfantryKit.Weapon(faction, source)));
		return true;
	}
}

modded class SCR_ChimeraCharacter
{
	// Только authority. Сетевое/JIP состояние — сами native inventory entities.
	bool m_bAICFWCSKitApplied;
}

modded class SCR_AIGroup
{
	override protected bool SpawnGroupMember(bool snapToTerrain, int index, ResourceName res, bool editMode, bool isLast)
	{
		if (!Replication.IsServer() || editMode || !AICF_WCSRHSContentProfile.Cast(AICF_ContentProfile.GetActive()))
			return super.SpawnGroupMember(snapToTerrain, index, res, editMode, isLast);
		array<AIAgent> before = {};
		GetAgents(before);
		bool result = super.SpawnGroupMember(snapToTerrain, index, res, editMode, isLast);
		if (!result) return result;
		array<AIAgent> after = {};
		GetAgents(after);
		foreach (AIAgent agent : after)
		{
			if (!agent || before.Contains(agent) || agent.GetParentGroup() != this) continue;
			SCR_ChimeraCharacter character = SCR_ChimeraCharacter.Cast(agent.GetControlledEntity());
			if (AICF_WCSInfantryEquipment.EligibleWCS(character, this))
				AICF_WCSInfantryEquipment.ApplyWCS(character, this);
		}
		// Native spawn уже завершён: не возвращаем false и не создаём дубль.
		// Сохранённый пользовательский loadout применяется владельцем позднее.
		return result;
	}
}
