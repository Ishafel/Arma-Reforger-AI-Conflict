// Один faction ITEM catalog для штатного арсенала и редактора экипировки.
class AICF_WCSItemCatalog : SCR_EntityCatalog
{
	protected string Family(ResourceName prefab)
	{
		string name = FilePath.StripPath(prefab);
		if (prefab.Contains("/Uniforms/"))
		{
			if (name.Contains("Pants") || name.Contains("Trousers")) return "Pants";
			return "Shirt";
		}
		array<string> families = {"Rifles", "MachineGuns", "Handguns", "Launchers", "Magazines", "Attachments", "Vests", "HeadGear", "Handwear", "Footwear", "Eyewear", "Facewear", "Belts", "Backpacks"};
		foreach (string family : families)
			if (prefab.Contains("/" + family + "/")) return family;
		return string.Empty;
	}

	protected SCR_EntityCatalogEntry CopyEntry(ResourceName prefab, SCR_EntityCatalogEntry template)
	{
		Resource copy = BaseContainerTools.CreateContainerFromInstance(template);
		if (!copy || !copy.IsValid()) return null;
		BaseContainer container = copy.GetResource().ToBaseContainer();
		if (!container.Set("m_sEntityPrefab", prefab)) return null;
		return SCR_EntityCatalogEntry.Cast(BaseContainerTools.CreateInstanceFromContainer(container));
	}

	protected SCR_EntityCatalogEntry InheritedEntry(ResourceName prefab, map<ResourceName, SCR_EntityCatalogEntry> nativeEntries)
	{
		Resource resource = Resource.Load(prefab);
		if (!resource || !resource.IsValid()) return null;
		BaseContainer ancestor = resource.GetResource().ToBaseContainer();
		for (int depth; ancestor && depth < 32; depth++)
		{
			SCR_EntityCatalogEntry template;
			ResourceName ancestorName = SCR_BaseContainerTools.GetPrefabResourceName(ancestor);
			if (nativeEntries.Find(ancestorName, template) && template.GetEntityDataOfType(SCR_ArsenalItem))
			{
				return CopyEntry(prefab, template);
			}
			ancestor = ancestor.GetAncestor();
		}
		return null;
	}

	int Build(FactionKey faction, array<ref SCR_EntityCatalog> originals)
	{
		m_eEntityCatalogType = EEntityCatalogType.ITEM;
		m_aEntityEntryList = {};
		array<ResourceName> known = {};
		array<SCR_EntityCatalogEntry> entries = {};
		foreach (SCR_EntityCatalog original : originals)
		{
			if (original.GetCatalogType() == EEntityCatalogType.ITEM)
				AICF_WCSVehicleCatalog.Entries(original, entries);
		}
		foreach (SCR_EntityCatalogEntry entry : entries)
		{
			if (!entry.IsEnabled()) continue;
			m_aEntityEntryList.Insert(entry);
			known.Insert(entry.GetPrefab());
		}
		int preserved = m_aEntityEntryList.Count();
		ResourceName path;
		if (faction == "RHS_USAF") path = "{5F7EC52FC40A03E2}Configs/EntityCatalog/US/InventoryItems_EntityCatalog_US.conf";
		else if (faction == "RHS_AFRF") path = "{C53421647C3D0D2E}Configs/EntityCatalog/USSR/InventoryItems_EntityCatalog_USSR.conf";
		else return 0;
		Resource resource = Resource.Load(path);
		if (!resource || !resource.IsValid()) return 0;
		SCR_EntityCatalog source = SCR_EntityCatalog.Cast(BaseContainerTools.CreateInstanceFromContainer(resource.GetResource().ToBaseContainer()));
		entries.Clear();
		AICF_WCSVehicleCatalog.Entries(source, entries);
		foreach (SCR_EntityCatalogEntry candidate : entries)
		{
			if (!candidate.IsEnabled() || known.Contains(candidate.GetPrefab())) continue;
			m_aEntityEntryList.Insert(candidate);
			known.Insert(candidate.GetPrefab());
		}
		// У WCS одежда и оружие часто поставлены без собственного arsenal entry.
		// Клонируем native metadata ближайшего разрешённого предка: цена, rank,
		// game modes и тип остаются его контрактом, новый prefab получает свой UI.
		map<ResourceName, SCR_EntityCatalogEntry> nativeEntries = new map<ResourceName, SCR_EntityCatalogEntry>();
		map<string, SCR_EntityCatalogEntry> families = new map<string, SCR_EntityCatalogEntry>();
		foreach (SCR_EntityCatalogEntry nativeEntry : m_aEntityEntryList)
		{
			nativeEntries.Insert(nativeEntry.GetPrefab(), nativeEntry);
			string family = Family(nativeEntry.GetPrefab());
			if (!family.IsEmpty() && !families.Contains(family) && SCR_EntityCatalogInventoryItem.Cast(nativeEntry) && nativeEntry.GetEntityDataOfType(SCR_ArsenalItem))
				families.Insert(family, nativeEntry);
		}
		array<ResourceName> resources = {};
		AICF_WCSItemResources.Fill(resources);
		foreach (ResourceName prefab : resources)
		{
			if (known.Contains(prefab)) continue;
			SCR_EntityCatalogEntry inherited = InheritedEntry(prefab, nativeEntries);
			if (!inherited)
			{
				SCR_EntityCatalogEntry template;
				if (families.Find(Family(prefab), template))
				{
					Resource itemResource = Resource.Load(prefab);
					IEntitySource itemSource = SCR_BaseContainerTools.FindEntitySource(itemResource);
					if (itemSource && SCR_ComponentHelper.GetInventoryItemComponentSource(itemSource)) inherited = CopyEntry(prefab, template);
				}
			}
			if (!inherited) continue;
			m_aEntityEntryList.Insert(inherited);
			known.Insert(prefab);
		}
		int added = m_aEntityEntryList.Count() - preserved;
		Print(string.Format("[AICF][WCS][ITEM_CATALOG] faction=%1 preserved=%2 added=%3", faction, preserved, added));
		return added;
	}
}
