class AICF_LoadoutCatalog
{
	protected ref array<SCR_EntityCatalogEntry> m_aEntries = {};
	protected AICF_ContentProfile m_Profile;
	protected FactionKey m_sStableFaction;
	protected bool m_bPersonalRules;
	protected ref array<string> m_aDenied = {};

	void SetPersonalRules(string snapshot = "")
	{
		m_bPersonalRules = true;
		m_aDenied.Clear();
		JsonLoadContext json = new JsonLoadContext();
		if (!snapshot.IsEmpty() && json.LoadFromString(snapshot)) json.ReadValue("denied", m_aDenied);
	}

	bool IsDenied(ResourceName prefab)
	{
		if (!m_bPersonalRules) return false;
		if (!Replication.IsServer()) return m_aDenied.Contains(prefab);
		SCR_BaseGameMode mode = SCR_BaseGameMode.Cast(GetGame().GetGameMode());
		if (!mode) return true;
		SCR_ArsenalManagerComponent arsenal = SCR_ArsenalManagerComponent.Cast(mode.FindComponent(SCR_ArsenalManagerComponent));
		if (!arsenal || !arsenal.GetLoadoutSaveBlackListHolder()) return false;
		return arsenal.GetLoadoutSaveBlackListHolder().IsPrefabBlacklisted(prefab);
	}

	string PersonalRulesSnapshot()
	{
		array<string> denied = {};
		foreach (SCR_EntityCatalogEntry entry : m_aEntries)
			if (entry && IsDenied(entry.GetPrefab())) denied.Insert(entry.GetPrefab());
		JsonSaveContext json = new JsonSaveContext();
		json.WriteValue("denied", denied);
		return json.SaveToString();
	}
	protected ref map<ResourceName, string> m_mSources = new map<ResourceName, string>();

	string Source(ResourceName prefab)
	{
		string source;
		if (!m_mSources.Find(prefab, source))
		{
			source = m_Profile.GetLoadoutItemSource(prefab);
			m_mSources.Insert(prefab, source);
		}
		return source;
	}

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
		return entry && m_Profile.AllowsLoadoutItem(m_sStableFaction, entry) && !IsDenied(prefab);
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
				return "{AICF:AICF_UI_Pouch_e1c97d80}";
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
			return "{AICF:AICF_UI_Pouch_e1c97d80}";
		return "{AICF:AICF_UI_Item_343a4b56}";
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

	void List(int category, array<string> prefabs, int mode = 0, string source = "")
	{
		prefabs.Clear();
		foreach (SCR_EntityCatalogEntry entry : m_aEntries)
		{
			if (!entry || !entry.IsEnabled() || !m_Profile.AllowsLoadoutItem(m_sStableFaction, entry) || IsDenied(entry.GetPrefab()) || prefabs.Contains(entry.GetPrefab()))
				continue;
			SCR_ArsenalItem item = SCR_ArsenalItem.Cast(entry.GetEntityDataOfType(SCR_ArsenalItem));
			if (item && (category == 0 || (item.GetItemType() & category) != 0) &&
				(mode == 0 || (item.GetItemMode() & mode) != 0) && (source.IsEmpty() || Source(entry.GetPrefab()) == source))
				prefabs.Insert(entry.GetPrefab());
		}
		prefabs.Sort();
	}
}
