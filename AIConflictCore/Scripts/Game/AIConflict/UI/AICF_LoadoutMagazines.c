// Подбор запасного магазина в изолированном черновике. Ничего не экипирует:
// редактор добавляет результат обычной операцией рецепта, проверяемой сервером.
class AICF_LoadoutMagazines
{
	static bool Compatible(ResourceName prefab, BaseMuzzleComponent muzzle, AICF_LoadoutCatalog catalog, ItemPreviewManagerEntity preview)
	{
		if (prefab.IsEmpty() || !muzzle || muzzle.IsDisposable() || !muzzle.GetMagazineWell() ||
			!catalog || !catalog.Allows(prefab) || !preview || preview.GetWorld() == GetGame().GetWorld() ||
			muzzle.GetOwner().GetWorld() == GetGame().GetWorld())
			return false;
		IEntity candidate = preview.ResolvePreviewEntityForPrefab(prefab);
		// Entity менеджера и его native prefab-preview world различаются.
		if (!candidate || candidate.GetWorld() != muzzle.GetOwner().GetWorld())
			return false;
		BaseMagazineComponent magazine = BaseMagazineComponent.Cast(candidate.FindComponent(BaseMagazineComponent));
		// Тот же контракт magazine well, что в stock CanReloadCurrentWeapon.
		return magazine && magazine.GetMaxAmmoCount() > 0 && magazine.GetMagazineWell() &&
			magazine.GetMagazineWell().Type() == muzzle.GetMagazineWell().Type();
	}

	static AICF_LoadoutLocation Destination(ResourceName prefab, AICF_LoadoutNavigation tree, InventoryStorageManagerComponent manager)
	{
		if (!tree || !manager || manager.GetOwner().GetWorld() == GetGame().GetWorld())
			return null;
		array<string> addresses = {}, sorted = {};
		array<int> indices = {};
		foreach (int i, AICF_LoadoutLocation location : tree.m_Locations)
		{
			// Только конечные карманы, включая подсумки RHS. Не слот оружия,
			// не его магазин и не агрегирующая storage одежды.
			if (location.m_sPath.IsEmpty() || !SCR_UniversalInventoryStorageComponent.Cast(location.m_Storage) ||
				AICF_LoadoutSlotView.HasFixedSlots(location.m_Storage) ||
				AICF_LoadoutInventory.ResolvePath(manager.GetOwner(), location.m_sPath) != location.m_Owner ||
				AICF_LoadoutInventory.FindStorage(location.m_Owner, location.m_sStorage) != location.m_Storage)
				continue;
			addresses.Insert(location.m_sPath + "|" + location.m_sStorage);
			indices.Insert(i);
		}
		sorted.Copy(addresses);
		sorted.Sort();
		foreach (string address : sorted)
		{
			AICF_LoadoutLocation target = tree.m_Locations[indices[addresses.Find(address)]];
			if (manager.CanInsertResourceInStorage(prefab, target.m_Storage, -1))
				return target;
		}
		return null;
	}

	static bool Resolve(AICF_LoadoutDraft draft, AICF_LoadoutCatalog catalog, AICF_LoadoutNavigation tree,
		BaseMuzzleComponent muzzle, out ResourceName prefab, out AICF_LoadoutLocation destination, out string reason)
	{
		prefab = string.Empty;
		destination = null;
		reason = "Выберите оружие с магазином.";
		if (!draft || !draft.GetCharacter() || !draft.GetPreview() || !catalog || !tree || !muzzle ||
			muzzle.IsDisposable() || !muzzle.GetMagazineWell() ||
			draft.GetCharacter().GetWorld() == GetGame().GetWorld() ||
			muzzle.GetOwner().GetWorld() != draft.GetCharacter().GetWorld())
			return false;
		InventoryStorageManagerComponent manager = InventoryStorageManagerComponent.Cast(draft.GetCharacter().FindComponent(InventoryStorageManagerComponent));
		if (!manager)
			return false;
		array<string> candidates = {}, visited = {};
		BaseMagazineComponent loaded = muzzle.GetMagazine();
		if (loaded)
			candidates.Insert(SCR_ResourceNameUtils.GetPrefabName(loaded.GetOwner()));
		candidates.Insert(muzzle.GetDefaultMagazineOrProjectileName());
		bool compatible;
		// Полный каталог нужен только при недоступности текущего/штатного.
		// Метод вызывается по кнопке, никогда из кадрового Refresh.
		for (int pass; pass < 2; pass++)
		{
			if (pass == 1)
				catalog.List(0, candidates, SCR_EArsenalItemMode.AMMUNITION);
			foreach (ResourceName candidate : candidates)
			{
				if (visited.Contains(candidate))
					continue;
				visited.Insert(candidate);
				if (!Compatible(candidate, muzzle, catalog, draft.GetPreview()))
					continue;
				compatible = true;
				destination = Destination(candidate, tree, manager);
				if (!destination)
					continue;
				prefab = candidate;
				reason = string.Empty;
				return true;
			}
		}
		reason = "В каталоге фракции нет совместимого магазина для этого оружия.";
		if (compatible)
			reason = "Для совместимого магазина нет места в карманах. Освободите место или наденьте контейнер.";
		return false;
	}
}
