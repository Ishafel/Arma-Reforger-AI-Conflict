class AICF_LoadoutLocation
{
	string m_sPath;
	string m_sStorage;
	string m_sName;
	BaseInventoryStorageComponent m_Storage;
	IEntity m_Owner;
}

// Одна страница на предмет, даже если у него несколько native storage.
// Адрес операции всегда принадлежит конкретной storage; UI их не смешивает.
class AICF_LoadoutNavigation
{
	ref array<ref AICF_LoadoutLocation> m_Locations = {};
	protected ref set<IEntity> m_Visited = new set<IEntity>();
	protected AICF_LoadoutCatalog m_Catalog;

	void Build(IEntity character, AICF_LoadoutCatalog catalog)
	{
		Clear();
		m_Catalog = catalog;
		Collect(character, string.Empty, 0);
		m_Visited.Clear();
		m_Catalog = null;
	}

	void Clear()
	{
		m_Locations.Clear();
		m_Visited.Clear();
	}

	protected void Collect(IEntity entity, string path, int depth)
	{
		if (!entity || depth > 6 || m_Visited.Contains(entity))
			return;
		m_Visited.Insert(entity);
		set<BaseInventoryStorageComponent> storages = new set<BaseInventoryStorageComponent>();
		SCR_PlayerArsenalLoadout.FindStorageComponents(entity, storages);
		foreach (BaseInventoryStorageComponent storage : storages)
		{
			if (!AICF_LoadoutInventory.Editable(storage))
				continue;
			string storageId = AICF_LoadoutInventory.StorageId(entity, storage);
			if (storageId.IsEmpty())
				continue;
			AICF_LoadoutLocation location = new AICF_LoadoutLocation();
			location.m_sPath = path;
			location.m_sStorage = storageId;
			location.m_sName = m_Catalog.EntityName(entity);
			location.m_Storage = storage;
			location.m_Owner = entity;
			m_Locations.Insert(location);
			// GetItem даёт native компонент именно этого места, включая RHS
			// подсумки. FindComponent на владельце может вернуть другой компонент.
			for (int slot; slot < storage.GetSlotsCount(); slot++)
			{
				InventoryItemComponent child = storage.GetItem(slot);
				if (!child || !child.GetParentSlot() || child.GetParentSlot().GetStorage() != storage ||
					child.GetParentSlot().GetID() != slot)
					continue;
				Collect(child.GetOwner(), path + storageId + "#" + slot.ToString() + "/", depth + 1);
			}
		}
	}

	int FindPath(string path)
	{
		foreach (int i, AICF_LoadoutLocation location : m_Locations)
		{
			if (location.m_sPath == path)
				return i;
		}
		return -1;
	}

	int Child(AICF_LoadoutLocation parent, int slot)
	{
		if (!parent || slot < 0 || slot >= parent.m_Storage.GetSlotsCount())
			return -1;
		IEntity item = AICF_LoadoutClothing.CoveringItem(parent.m_Storage, slot);
		if (!item)
			return -1;
		foreach (int i, AICF_LoadoutLocation child : m_Locations)
		{
			if (child.m_Owner == item)
			{
				array<int> locations = {}, slots = {};
				Rows(child.m_sPath, locations, slots);
				if (!slots.IsEmpty())
					return i;
			}
		}
		return -1;
	}

	static string ParentPath(string path)
	{
		array<string> parts = {};
		path.Split("/", parts, true);
		string parent;
		for (int i; i < parts.Count() - 1; i++)
			parent += parts[i] + "/";
		return parent;
	}

	void Rows(string path, array<int> locations, array<int> slots)
	{
		locations.Clear();
		slots.Clear();
		// Корень: одежда, затем оружие. Внутри: крепления, затем карманы.
		for (int pass; pass < 2; pass++)
		{
			foreach (int index, AICF_LoadoutLocation location : m_Locations)
			{
				if (location.m_sPath != path)
					continue;
				BaseInventoryStorageComponent storage = location.m_Storage;
				bool fixedSlots = AICF_LoadoutSlotView.HasFixedSlots(storage);
				bool second = !fixedSlots;
				if (path.IsEmpty())
				{
					if (!EquipedLoadoutStorageComponent.Cast(storage) && !EquipedWeaponStorageComponent.Cast(storage))
						continue;
					second = EquipedWeaponStorageComponent.Cast(storage) != null;
				}
				if ((pass == 1) != second)
					continue;
				if (!path.IsEmpty() && !fixedSlots && SCR_UniversalInventoryStorageComponent.Cast(storage))
				{
					locations.Insert(index);
					slots.Insert(-1);
				}
				for (int slot; slot < storage.GetSlotsCount(); slot++)
				{
					if (!AICF_LoadoutSlotView.VisibleSlot(storage, slot))
						continue;
					if (!path.IsEmpty() && !fixedSlots && !storage.Get(slot))
						continue;
					locations.Insert(index);
					slots.Insert(slot);
				}
			}
		}
	}

	string CapacityText(string path)
	{
		float used, capacity;
		foreach (AICF_LoadoutLocation location : m_Locations)
		{
			// Считаем только конечные карманы: ClothNode уже включает их объём.
			if (!location.m_sPath.StartsWith(path) || !SCR_UniversalInventoryStorageComponent.Cast(location.m_Storage))
				continue;
			used += location.m_Storage.GetOccupiedSpace();
			capacity += location.m_Storage.GetMaxVolumeCapacity();
		}
		return AICF_LoadoutSlotView.FormatCapacity(used, capacity);
	}
}
