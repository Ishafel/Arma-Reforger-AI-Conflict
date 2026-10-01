// Индекс конкретных ресурсов, а не модификаций их цепочки наследования.
// Существующий GUID сохраняет исходный источник после override другим addon.
class AICF_WCSItemOrigins
{
	protected ref map<ResourceName, string> m_mSources = new map<ResourceName, string>();
	protected string m_sScanningSource;

	void AICF_WCSItemOrigins()
	{
		AddAddon("ArmaReforger", "Vanilla");
		array<string> guids = {};
		GameProject.GetLoadedAddons(guids);
		// RHS обрабатывается до WCS независимо от порядка загруженных addons.
		foreach (string guid : guids)
		{
			string addon = GameProject.GetAddonID(guid);
			if (addon.StartsWith("RHS")) AddAddon(addon, "RHS");
		}
		foreach (string guid : guids)
		{
			string addon = GameProject.GetAddonID(guid);
			if (addon.StartsWith("WCS")) AddAddon(addon, "WCS");
		}
	}

	protected void AddAddon(string addon, string source)
	{
		m_sScanningSource = source;
		SearchResourcesFilter filter = new SearchResourcesFilter();
		filter.rootPath = "$" + addon + ":Prefabs";
		filter.fileExtensions = {"et"};
		ResourceDatabase.SearchResources(filter, RecordResource);
	}

	protected void RecordResource(ResourceName prefab, string exactPath = "")
	{
		if (!prefab.IsEmpty() && !m_mSources.Contains(prefab))
			m_mSources.Insert(prefab, m_sScanningSource);
	}

	bool Find(ResourceName prefab, out string source)
	{
		return m_mSources.Find(prefab, source);
	}
}
