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
				InventoryItemComponent component = null;
				if (child) component = InventoryItemComponent.Cast(child.FindComponent(InventoryItemComponent));
				SCR_ItemAttributeCollection attributes = null;
				if (component) attributes = SCR_ItemAttributeCollection.Cast(component.GetAttributes());
				if (attributes && !attributes.IsVisible(component))
				{
					if (child && !seen.Contains(child))
					{
						seen.Insert(child);
						Collect(child, catalog, lines, seen, counts, names, depth + 1);
					}
					continue;
				}
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
					string category = "{AICF:AICF_UI_Equipment_286b1543}";
					int type = catalog.ItemType(key);
					if (child.FindComponent(BaseMagazineComponent)) category = "{AICF:AICF_UI_Magazine_b81150f8}";
					else if (type & SCR_EArsenalItemType.HEAL) category = "{AICF:AICF_UI_Medical_a7375e8d}";
					else if (type & (SCR_EArsenalItemType.LETHAL_THROWABLE | SCR_EArsenalItemType.NON_LETHAL_THROWABLE)) category = "{AICF:AICF_UI_Grenades_and_smoke_7db7b67f}";
					else if (child.FindComponent(BaseWeaponComponent)) category = "{AICF:AICF_UI_Weapons_b16fba51}";
					else if (child.FindComponent(BaseInventoryStorageComponent)) category = "{AICF:AICF_UI_Pouch_e1c97d80}";
					string name = AICF_Localization.Resolve(catalog.EntityName(child));
					string label = AICF_Localization.Resolve(category);
					if (name != label) name = label + " — " + name;
					names.Set(key, name);
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
