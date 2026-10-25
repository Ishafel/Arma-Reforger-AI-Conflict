// Сводка готового inventory: включает defaults, карманы, магазины в оружии
// и вложенные attachments. Один entity учитывается ровно один раз.
class AICF_LoadoutSummary
{
	static void Collect(IEntity entity, AICF_LoadoutCatalog catalog, array<string> lines,
		set<IEntity> seen, map<string, int> counts, map<string, string> names, int depth = 0)
	{
		if (!entity || depth > 6) return;
		set<BaseInventoryStorageComponent> storages = new set<BaseInventoryStorageComponent>();
		AICF_LoadoutInventory.FindStorageComponents(entity, storages);
		foreach (BaseInventoryStorageComponent storage : storages)
		{
			if (!AICF_LoadoutInventory.Editable(storage)) continue;
			for (int slot; slot < storage.GetSlotsCount(); slot++)
			{
				IEntity child = AICF_LoadoutClothing.CoveringItem(storage, slot);
				bool worn = depth == 0 && AICF_LoadoutSlotView.HasFixedSlots(storage);
				if (worn && !AICF_LoadoutSlotView.VisibleSlot(storage, slot)) continue;
				if (child && seen.Contains(child)) continue;
				if (worn)
				{
					string label;
					AICF_LoadoutSlotView.Describe(storage, slot, label);
					string itemName = "{AICF:AICF_UI_LibraryEmptySlot}";
					if (child) itemName = catalog.EntityName(child);
					lines.Insert(AICF_Localization.Resolve(label) + " — " + AICF_Localization.Resolve(itemName));
				}
				if (!child) continue;
				seen.Insert(child);
				if (!worn)
				{
					string key = SCR_ResourceNameUtils.GetPrefabName(child);
					int count;
					counts.Find(key, count);
					counts.Set(key, count + 1);
					names.Set(key, catalog.EntityName(child));
				}
				Collect(child, catalog, lines, seen, counts, names, depth + 1);
			}
		}
	}

	static void Build(IEntity entity, AICF_LoadoutCatalog catalog, array<string> lines)
	{
		lines.Clear();
		set<IEntity> seen = new set<IEntity>();
		map<string, int> counts = new map<string, int>();
		map<string, string> names = new map<string, string>();
		Collect(entity, catalog, lines, seen, counts, names);
		array<string> carried = {};
		foreach (string key, int count : counts)
			carried.Insert(names.Get(key) + " ×" + count.ToString());
		carried.Sort();
		lines.InsertAll(carried);
	}
}
