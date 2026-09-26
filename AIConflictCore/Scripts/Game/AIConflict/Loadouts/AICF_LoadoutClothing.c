// Одежда занимает основную область и может освобождать/блокировать другие.
// Проверяем обе стороны конфликта: комбинезон вместо брюк и брюки вместо него.
class AICF_LoadoutClothing
{
	static bool Covers(BaseLoadoutClothComponent cloth, typename area)
	{
		if (!cloth || !cloth.GetAreaType() || area == typename.Empty)
			return false;
		if (cloth.GetAreaType().Type().IsInherited(area))
			return true;
		array<typename> blocked = {};
		cloth.GetBlockedSlots(blocked);
		foreach (typename other : blocked)
		{
			if (other.IsInherited(area))
				return true;
		}
		return false;
	}

	static typename Area(BaseInventoryStorageComponent storage, int slot)
	{
		if (!storage || slot < 0 || slot >= storage.GetSlotsCount())
			return typename.Empty;
		LoadoutSlotInfo info = LoadoutSlotInfo.Cast(storage.GetSlot(slot));
		if (info && info.GetAreaType())
			return info.GetAreaType().Type();
		return typename.Empty;
	}

	static int TargetSlot(BaseInventoryStorageComponent storage, int requested, IEntity candidate)
	{
		EquipedLoadoutStorageComponent clothing = EquipedLoadoutStorageComponent.Cast(storage);
		if (!clothing || requested < 0 || !candidate)
			return requested;
		BaseLoadoutClothComponent cloth = BaseLoadoutClothComponent.Cast(candidate.FindComponent(BaseLoadoutClothComponent));
		if (!Covers(cloth, Area(storage, requested)))
			return -1;
		LoadoutSlotInfo target = clothing.GetSlotFromArea(cloth.GetAreaType().Type());
		if (!target)
			return -1;
		return target.GetID();
	}

	static IEntity CoveringItem(BaseInventoryStorageComponent storage, int slot)
	{
		if (!storage || slot < 0 || slot >= storage.GetSlotsCount())
			return null;
		if (storage.Get(slot))
			return storage.Get(slot);
		if (!EquipedLoadoutStorageComponent.Cast(storage))
			return null;
		typename area = Area(storage, slot);
		for (int i; i < storage.GetSlotsCount(); i++)
		{
			IEntity item = storage.Get(i);
			if (item && Covers(BaseLoadoutClothComponent.Cast(item.FindComponent(BaseLoadoutClothComponent)), area))
				return item;
		}
		return null;
	}

	static bool RemoveConflictsLocal(BaseInventoryStorageComponent storage, IEntity candidate)
	{
		if (!storage || storage.GetOwner().GetWorld() == GetGame().GetWorld() || !candidate ||
			candidate.GetWorld() != storage.GetOwner().GetWorld())
			return false;
		BaseLoadoutClothComponent cloth = BaseLoadoutClothComponent.Cast(candidate.FindComponent(BaseLoadoutClothComponent));
		if (!cloth || !cloth.GetAreaType())
			return false;
		array<IEntity> conflicts = {};
		for (int i; i < storage.GetSlotsCount(); i++)
		{
			IEntity item = storage.Get(i);
			if (!item)
				continue;
			BaseLoadoutClothComponent existing = BaseLoadoutClothComponent.Cast(item.FindComponent(BaseLoadoutClothComponent));
			bool conflict = Covers(cloth, Area(storage, i));
			// Пересечение вторичных областей тоже является конфликтом.
			for (int j; !conflict && j < storage.GetSlotsCount(); j++)
			{
				typename area = Area(storage, j);
				conflict = Covers(cloth, area) && Covers(existing, area);
			}
			if (conflict && !conflicts.Contains(item))
				conflicts.Insert(item);
		}
		foreach (IEntity conflictItem : conflicts)
		{
			if (!AICF_LoadoutInventory.DeleteLocal(conflictItem))
				return false;
		}
		return true;
	}

	// Снимок содержит всю одежду и её содержимое. Сначала снимаем исходную:
	// stock restore удаляет лишние вещи ПОСЛЕ вставки, что мешает комбинезонам.
	// Вызывается только после guard Restore, для server-generated snapshot.
	static bool ClearForRestore(IEntity entity, InventoryStorageManagerComponent manager)
	{
		if (!entity || !manager || manager.GetOwner() != entity)
			return false;
		if (entity.GetWorld() == GetGame().GetWorld())
		{
			CharacterControllerComponent controller = CharacterControllerComponent.Cast(entity.FindComponent(CharacterControllerComponent));
			RplComponent rpl = RplComponent.Cast(entity.FindComponent(RplComponent));
			if (!Replication.IsServer() || !controller || controller.IsPlayerControlled() || !rpl || !rpl.IsMaster())
				return false;
		}
		set<BaseInventoryStorageComponent> storages = new set<BaseInventoryStorageComponent>();
		SCR_PlayerArsenalLoadout.FindStorageComponents(entity, storages);
		foreach (BaseInventoryStorageComponent storage : storages)
		{
			if (!EquipedLoadoutStorageComponent.Cast(storage) || !AICF_LoadoutInventory.Editable(storage))
				continue;
			array<InventoryItemComponent> items = {};
			storage.GetOwnedItems(items, false);
			foreach (InventoryItemComponent item : items)
			{
				if (entity.GetWorld() == GetGame().GetWorld())
				{
					if (!manager.TryDeleteItem(item.GetOwner()))
					{
						Print(string.Format("[AICF][LOADOUT_CLOTHING_REMOVE_FAILED] entity=%1 item=%2", entity.GetID(), SCR_ResourceNameUtils.GetPrefabName(item.GetOwner())), LogLevel.WARNING);
						return false;
					}
				}
				else if (!AICF_LoadoutInventory.DeleteLocal(item.GetOwner()))
					return false;
			}
		}
		return true;
	}
}
