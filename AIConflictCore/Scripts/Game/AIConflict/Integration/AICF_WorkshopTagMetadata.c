// 1.8.0.13 cannot instantiate SCR_FilterCategory as a standalone config root:
// its constructor is protected. Tag metadata does not need a category instance.
modded class SCR_WorkshopUiCommon
{
	override static void OnGameStart()
	{
		SCR_AddonManager manager = SCR_AddonManager.GetInstance();
		manager.m_OnAddonsChecked.Remove(SCR_WorkshopUiCommon.Callback_OnAddonsChecked);
		manager.m_OnAddonsChecked.Insert(SCR_WorkshopUiCommon.Callback_OnAddonsChecked);
		// The native static caller binds its own implementation; route both reads here.
		InitAddonTagMaps(ADDON_TYPE_FILTER_CATEGORY_CONFIG);
		InitAddonTagMaps(ADDON_MANW_FILTER_CATEGORY_CONFIG);
	}

	override protected static void InitAddonTagMaps(ResourceName configPath)
	{
		Resource resource = BaseContainerTools.LoadContainer(configPath);
		if (!resource || !resource.IsValid()) return;
		BaseContainer category = resource.GetResource().ToBaseContainer();
		if (!category) return;
		BaseContainerList filters = category.GetObjectArray("m_aFilters");
		if (!filters) return;
		for (int i; i < filters.Count(); i++)
		{
			BaseContainer filter = filters.Get(i);
			if (!filter) continue;
			string name, image;
			if (!filter.Get("m_sInternalName", name)) continue;
			filter.Get("m_sImageName", image);
			name.ToLower();
			s_sAddonTagImageMap.Insert(name, image);
			typename filterType = filter.GetClassName().ToType();
			if (!filterType || !filterType.IsInherited(SCR_ContentBrowserFilterTag)) continue;
			ResourceName thumbnail;
			if (filter.Get("m_sDefaultThumbnail", thumbnail) && !thumbnail.IsEmpty())
				s_sAddonTagDefaultThumbnailMap.Insert(name, thumbnail);
		}
	}
}
