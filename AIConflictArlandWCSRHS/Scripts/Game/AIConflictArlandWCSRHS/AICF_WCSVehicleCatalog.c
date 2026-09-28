// Дополняет native faction catalog до его Init; существующие entries и IDs
// остаются первыми. Одинаковая конфигурация создаётся на server и client/JIP.
class AICF_WCSVehicleCatalog : SCR_EntityCatalog
{
	static bool IsSupported(ResourceName prefab, FactionKey faction)
	{
		if (faction == "RHS_USAF")
			return prefab.Contains("Prefabs/Vehicles/Tracked/M1A1/") ||
				prefab.Contains("Prefabs/Vehicles/Tracked/M2A2/") ||
				prefab.Contains("/M1025_armed_BGM-71") || prefab.Contains("/M1025_armed_M134");
		if (faction == "RHS_AFRF")
			return prefab.Contains("Prefabs/Vehicles/Tracked/T72A/") ||
				prefab.Contains("Prefabs/Vehicles/Tracked/BMP3/") ||
				prefab.Contains("Prefabs/Vehicles/Tracked/T72B/") || prefab.Contains("/UAZ469_Kornet.et");
		return false;
	}

	static void Entries(SCR_EntityCatalog catalog, array<SCR_EntityCatalogEntry> entries)
	{
		if (!catalog) return;
		array<SCR_EntityCatalogEntry> direct = {};
		catalog.GetEntityList(direct);
		entries.InsertAll(direct);
		SCR_EntityCatalogMultiList multi = SCR_EntityCatalogMultiList.Cast(catalog);
		if (!multi) return;
		array<SCR_EntityCatalogMultiListEntry> lists = {};
		multi.GetMultiList(lists);
		foreach (SCR_EntityCatalogMultiListEntry list : lists)
		{
			foreach (SCR_EntityCatalogEntry entry : list.m_aEntities)
				entries.Insert(entry);
		}
	}

	int Build(FactionKey faction, array<ref SCR_EntityCatalog> originals)
	{
		m_eEntityCatalogType = EEntityCatalogType.VEHICLE;
		m_aEntityEntryList = {};
		array<ResourceName> known = {};
		array<SCR_EntityCatalogEntry> existing = {};
		foreach (SCR_EntityCatalog original : originals)
		{
			if (original.GetCatalogType() == EEntityCatalogType.VEHICLE)
				Entries(original, existing);
		}
		foreach (SCR_EntityCatalogEntry entry : existing)
		{
			if (!entry.IsEnabled()) continue;
			m_aEntityEntryList.Insert(entry);
			known.Insert(entry.GetPrefab());
		}
		int preserved = m_aEntityEntryList.Count();

		ResourceName path;
		if (faction == "RHS_USAF")
			path = "{D83A5EFEEF6CBF61}Configs/EntityCatalog/US/Vehicles_EntityCatalog_US.conf";
		else if (faction == "RHS_AFRF")
			path = "{09C2ED5F617A0390}Configs/EntityCatalog/USSR/Vehicles_EntityCatalog_USSR.conf";
		else
			return 0;
		Resource resource = Resource.Load(path);
		if (!resource || !resource.IsValid())
		{
			Print("[AICF][WCS][CATALOG_FAILED] source=" + path, LogLevel.ERROR);
			return 0;
		}
		SCR_EntityCatalog source = SCR_EntityCatalog.Cast(BaseContainerTools.CreateInstanceFromContainer(resource.GetResource().ToBaseContainer()));
		array<SCR_EntityCatalogEntry> candidates = {};
		Entries(source, candidates);
		foreach (SCR_EntityCatalogEntry candidate : candidates)
		{
			ResourceName prefab = candidate.GetPrefab();
			if (!candidate.IsEnabled() || !IsSupported(prefab, faction) || known.Contains(prefab)) continue;
			m_aEntityEntryList.Insert(candidate);
			known.Insert(prefab);
		}
		int added = m_aEntityEntryList.Count() - preserved;
		Print(string.Format("[AICF][WCS][VEHICLE_CATALOG] faction=%1 preserved=%2 added=%3", faction, preserved, added));
		return added;
	}
}

modded class SCR_Faction
{
	override void Init(IEntity owner)
	{
		if (!SCR_Global.IsEditMode() && m_aEntityCatalogs &&
			(GetFactionKey() == "RHS_USAF" || GetFactionKey() == "RHS_AFRF"))
		{
			AICF_WCSItemCatalog items = new AICF_WCSItemCatalog();
			if (items.Build(GetFactionKey(), m_aEntityCatalogs) > 0)
			{
				for (int itemIndex = m_aEntityCatalogs.Count() - 1; itemIndex >= 0; itemIndex--)
				{
					if (m_aEntityCatalogs[itemIndex].GetCatalogType() == EEntityCatalogType.ITEM)
						m_aEntityCatalogs.RemoveOrdered(itemIndex);
				}
				m_aEntityCatalogs.Insert(items);
			}
			AICF_WCSVehicleCatalog additions = new AICF_WCSVehicleCatalog();
			if (additions.Build(GetFactionKey(), m_aEntityCatalogs) > 0)
			{
				// Все исходные entry objects уже удерживаются в прежнем порядке.
				// Одна плоская list не меняет indices при native merge multilists.
				for (int index = m_aEntityCatalogs.Count() - 1; index >= 0; index--)
				{
					if (m_aEntityCatalogs[index].GetCatalogType() == EEntityCatalogType.VEHICLE)
						m_aEntityCatalogs.RemoveOrdered(index);
				}
				m_aEntityCatalogs.Insert(additions);
			}
		}
		super.Init(owner);
	}
}
