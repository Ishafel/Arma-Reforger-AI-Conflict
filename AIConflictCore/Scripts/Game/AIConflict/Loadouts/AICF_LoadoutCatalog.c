class AICF_LoadoutCatalog
{
	protected ref array<SCR_EntityCatalogEntry> m_aEntries = {};
	protected AICF_ContentProfile m_Profile;
	protected FactionKey m_sStableFaction;

	void AICF_LoadoutCatalog(SCR_CampaignFaction faction)
	{
		m_Profile = AICF_ContentProfile.GetActive();
		if (!faction)
			return;
		m_sStableFaction = m_Profile.GetStableFactionKey(faction.GetFactionKey());
		SCR_EntityCatalog catalog = faction.GetFactionEntityCatalogOfType(EEntityCatalogType.ITEM);
		if (catalog)
			catalog.GetEntityList(m_aEntries);
	}

	SCR_EntityCatalogEntry Find(ResourceName prefab)
	{
		foreach (SCR_EntityCatalogEntry entry : m_aEntries)
		{
			if (entry && entry.IsEnabled() && entry.GetPrefab() == prefab &&
				entry.GetEntityDataOfType(SCR_ArsenalItem))
				return entry;
		}
		return null;
	}

	bool Allows(ResourceName prefab)
	{
		SCR_EntityCatalogEntry entry = Find(prefab);
		return entry && m_Profile.AllowsLoadoutItem(m_sStableFaction, entry);
	}

	int Cost(ResourceName prefab)
	{
		SCR_EntityCatalogEntry entry = Find(prefab);
		if (!entry)
			return 0;
		SCR_ArsenalItem item = SCR_ArsenalItem.Cast(entry.GetEntityDataOfType(SCR_ArsenalItem));
		if (!item)
			return 0;
		// Вложенные предметы считаются отдельно по фактическому inventory.
		return Math.Max(0, item.GetSupplyCost(SCR_EArsenalSupplyCostType.DEFAULT, false));
	}

	string Name(ResourceName prefab)
	{
		SCR_EntityCatalogEntry entry = Find(prefab);
		if (entry && !entry.GetEntityName().IsEmpty())
			return WidgetManager.Translate(entry.GetEntityName());
		return FilePath.StripExtension(FilePath.StripPath(prefab));
	}

	// Подсумки модов могут отсутствовать в faction catalog, но иметь native UIInfo.
	string EntityName(IEntity entity)
	{
		if (!entity)
			return string.Empty;
		InventoryItemComponent item = InventoryItemComponent.Cast(entity.FindComponent(InventoryItemComponent));
		if (item && item.GetUIInfo() && !item.GetUIInfo().GetName().IsEmpty())
		{
			string name = WidgetManager.Translate(item.GetUIInfo().GetName());
			if (name == "Pouch")
				return "Подсумок";
			if (!name.IsEmpty() && !name.StartsWith("#"))
				return name;
		}
		ResourceName prefab = SCR_ResourceNameUtils.GetPrefabName(entity);
		if (Find(prefab))
		{
			string catalogName = Name(prefab);
			if (!catalogName.IsEmpty() && !catalogName.StartsWith("#"))
				return catalogName;
		}
		if (entity.FindComponent(BaseInventoryStorageComponent))
			return "Подсумок";
		return "Предмет";
	}

	int ItemType(ResourceName prefab)
	{
		SCR_EntityCatalogEntry entry = Find(prefab);
		if (!entry)
			return 0;
		SCR_ArsenalItem item = SCR_ArsenalItem.Cast(entry.GetEntityDataOfType(SCR_ArsenalItem));
		if (item)
			return item.GetItemType();
		return 0;
	}

	void List(int category, array<string> prefabs, int mode = 0)
	{
		prefabs.Clear();
		foreach (SCR_EntityCatalogEntry entry : m_aEntries)
		{
			if (!entry || !entry.IsEnabled() || !m_Profile.AllowsLoadoutItem(m_sStableFaction, entry) || prefabs.Contains(entry.GetPrefab()))
				continue;
			SCR_ArsenalItem item = SCR_ArsenalItem.Cast(entry.GetEntityDataOfType(SCR_ArsenalItem));
			if (item && (category == 0 || (item.GetItemType() & category) != 0) &&
				(mode == 0 || (item.GetItemMode() & mode) != 0))
				prefabs.Insert(entry.GetPrefab());
		}
		prefabs.Sort();
	}
}
