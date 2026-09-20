// Вооружение ЧВК собирается только в локальном equipment draft.
// Комплектные RHS prefab владеют совместимостью обвеса и заряженным магазином.
class AICF_RHSPMCArmament
{
	static ResourceName PrimaryPrefab(ResourceName source, int variant)
	{
		if (source.EndsWith("_MG.et"))
			return "{4BE4931DF7B1ABCD}Prefabs/Weapons/MachineGuns/PKM/MG_PKM_B51_Eot.et";
		if (source.EndsWith("_Sharpshooter.et"))
			return "{896DDA7B352187EF}Prefabs/Weapons/Rifles/SVD/Rifle_SVD_1P21_TGPV.et";
		if (source.EndsWith("_Scout.et"))
			return "{951E3D5E86826A47}Prefabs/Weapons/Rifles/M4A1/Variants/ION/Variants/Rifle_DOM4A1_ION_ACOG_ReapIR_SD.et";
		if (variant == 1)
			return "{40DC4A6EE53B969E}Prefabs/Weapons/Rifles/M4A1/Variants/ION/Variants/Rifle_DOMk18_ION_RMR_SD.et";
		if (variant == 2)
			return "{3E54573B11FB0578}Prefabs/Weapons/Rifles/M4A1/Variants/ION/Variants/Rifle_AR15_GA_UD145_BLK_ION1.et";
		return "{7EC8EC2BE897F1B0}Prefabs/Weapons/Rifles/M4A1/Variants/ION/Variants/Rifle_DOM4A1_FSP_ION_VCOGRMR_SD.et";
	}

	static void Items(IEntity entity, array<IEntity> result, int depth = 0)
	{
		if (!entity || depth > 6) return;
		set<BaseInventoryStorageComponent> storages = new set<BaseInventoryStorageComponent>();
		SCR_PlayerArsenalLoadout.FindStorageComponents(entity, storages);
		foreach (BaseInventoryStorageComponent storage : storages)
		{
			array<InventoryItemComponent> items = {};
			storage.GetOwnedItems(items, false);
			foreach (InventoryItemComponent item : items)
			{
				IEntity child = item.GetOwner();
				if (result.Contains(child)) continue;
				result.Insert(child);
				Items(child, result, depth + 1);
			}
		}
	}

	static BaseWeaponComponent Primary(IEntity model)
	{
		array<IEntity> items = {};
		Items(model, items);
		foreach (IEntity item : items)
		{
			BaseWeaponComponent weapon = BaseWeaponComponent.Cast(item.FindComponent(BaseWeaponComponent));
			if (!weapon) continue;
			EWeaponType type = weapon.GetWeaponType();
			if (type == EWeaponType.WT_RIFLE || type == EWeaponType.WT_MACHINEGUN || type == EWeaponType.WT_SNIPERRIFLE)
				return weapon;
		}
		return null;
	}

	static IEntity ReplaceLocal(IEntity old, ResourceName prefab, InventoryStorageManagerComponent manager)
	{
		if (!old || old.GetWorld() == GetGame().GetWorld()) return null;
		InventoryItemComponent item = InventoryItemComponent.Cast(old.FindComponent(InventoryItemComponent));
		if (!item || !item.GetParentSlot()) return null;
		InventoryStorageSlot slot = item.GetParentSlot();
		BaseInventoryStorageComponent storage = slot.GetStorage();
		int index = slot.GetID();
		if (!AICF_LoadoutInventory.DeleteLocal(old)) return null;
		return AICF_LoadoutInventory.InsertLocal(prefab, storage, index, manager);
	}

	static int MinimumSpareMagazines(ResourceName source)
	{
		if (source.EndsWith("_MG.et")) return 3;
		if (source.EndsWith("_Sharpshooter.et")) return 5;
		if (source.EndsWith("_Ammo.et")) return 26;
		return 6;
	}

	static ResourceName SupportMagazinePrefab()
	{
		return "{0110F6D92703114A}Prefabs/Weapons/Magazines/Box_762x54_PK_100rnd_7N13_4Ball_1Tracer.et";
	}

	static bool Compatible(BaseMagazineComponent magazine, BaseMuzzleComponent muzzle)
	{
		return magazine && muzzle && magazine.GetMagazineWell() && muzzle.GetMagazineWell() &&
			magazine.GetMagazineWell().Type() == muzzle.GetMagazineWell().Type();
	}

	static int SpareMagazines(IEntity model, BaseMuzzleComponent muzzle)
	{
		array<IEntity> items = {};
		Items(model, items);
		int count;
		foreach (IEntity item : items)
		{
			BaseMagazineComponent magazine = BaseMagazineComponent.Cast(item.FindComponent(BaseMagazineComponent));
			if (Compatible(magazine, muzzle) && magazine != muzzle.GetMagazine() && magazine.GetAmmoCount() > 0)
				count++;
		}
		return count;
	}

	static bool PrepareLocal(IEntity model, ResourceName source, int variant, out ResourceName magazine, out int addMagazines)
	{
		if (!model || model.GetWorld() == GetGame().GetWorld()) return false;
		BaseWeaponComponent old = Primary(model);
		if (!old || !old.GetCurrentMuzzle() || !old.GetCurrentMuzzle().GetMagazineWell()) return false;
		typename oldWell = old.GetCurrentMuzzle().GetMagazineWell().Type();
		InventoryStorageManagerComponent manager = InventoryStorageManagerComponent.Cast(model.FindComponent(InventoryStorageManagerComponent));
		IEntity replacement = ReplaceLocal(old.GetOwner(), PrimaryPrefab(source, variant), manager);
		if (!replacement) return false;
		BaseWeaponComponent weapon = BaseWeaponComponent.Cast(replacement.FindComponent(BaseWeaponComponent));
		if (!weapon) return false;
		BaseMuzzleComponent muzzle = weapon.GetCurrentMuzzle();
		if (!muzzle || !muzzle.GetMagazine() || !muzzle.GetMagazineWell()) return false;
		// Удаляем только запасные магазины прежнего основного калибра.
		// ПТ-боезапас и пулемётные коробки помощника остаются при бойце.
		if (oldWell != muzzle.GetMagazineWell().Type())
		{
			array<IEntity> items = {};
			Items(model, items);
			foreach (IEntity item : items)
			{
				BaseMagazineComponent prior = BaseMagazineComponent.Cast(item.FindComponent(BaseMagazineComponent));
				if (prior && prior.GetMagazineWell() && prior.GetMagazineWell().Type() == oldWell &&
					!AICF_LoadoutInventory.DeleteLocal(item)) return false;
			}
		}
		if (source.EndsWith("_AMG.et"))
		{
			array<IEntity> supportItems = {};
			Items(model, supportItems);
			foreach (IEntity supportItem : supportItems)
			{
				if (SCR_ResourceNameUtils.GetPrefabName(supportItem).Contains("/UK59/Box_") &&
					!AICF_LoadoutInventory.DeleteLocal(supportItem)) return false;
			}
		}
		magazine = SCR_ResourceNameUtils.GetPrefabName(muzzle.GetMagazine().GetOwner());
		addMagazines = Math.Max(0, MinimumSpareMagazines(source) - SpareMagazines(model, muzzle));
		return muzzle.GetAmmoCount() > 0 && !magazine.IsEmpty();
	}

	static bool Validate(IEntity model, ResourceName source, int variant)
	{
		BaseWeaponComponent weapon = Primary(model);
		if (!weapon || SCR_ResourceNameUtils.GetPrefabName(weapon.GetOwner()) != PrimaryPrefab(source, variant)) return false;
		BaseMuzzleComponent muzzle = weapon.GetCurrentMuzzle();
		if (!muzzle || muzzle.GetAmmoCount() <= 0 || SpareMagazines(model, muzzle) < MinimumSpareMagazines(source)) return false;
		if (source.EndsWith("_AMG.et"))
		{
			array<IEntity> supportItems = {};
			Items(model, supportItems);
			int boxes;
			foreach (IEntity supportItem : supportItems)
			{
				if (SCR_ResourceNameUtils.GetPrefabName(supportItem) != SupportMagazinePrefab()) continue;
				BaseMagazineComponent supportMagazine = BaseMagazineComponent.Cast(supportItem.FindComponent(BaseMagazineComponent));
				if (supportMagazine && supportMagazine.GetAmmoCount() == 100) boxes++;
			}
			if (boxes < 3) return false;
		}
		// ПКМ с EOTech остаётся ленточным пулемётом; совместимого штатного
		// suppressed варианта в проверенном каталоге нет.
		return source.EndsWith("_MG.et") || muzzle.IsMuzzleSuppressed();
	}
}
