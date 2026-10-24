// Isolated stage only. Reads the real frontend catalog without opening menus.
class AICF_ScenarioCatalogProbe
{
	protected int m_iAICFCatalogChecks;
	protected int m_iAICFCatalogFailures;

	void Start()
	{
		WorkshopApi workshop = GetGame().GetBackendApi().GetWorkshop();
		if (workshop && workshop.NeedAddonsScan()) workshop.ScanOfflineItems();
		GetGame().GetCallqueue().CallLater(AICF_CatalogReadback, 5000, false);
		GetGame().GetCallqueue().CallLater(AICF_CatalogTimeout, 30000, false);
	}

	protected void AICF_CatalogCheck(string name, bool passed)
	{
		m_iAICFCatalogChecks++;
		if (!passed) m_iAICFCatalogFailures++;
		Print(string.Format("[AICF][CATALOG_CASE] case=%1 pass=%2", name, passed));
	}

	protected int AICF_CatalogCount(array<MissionWorkshopItem> items, ResourceName resource)
	{
		int count;
		foreach (MissionWorkshopItem item : items)
		{
			if (item.Id() == resource) count++;
		}
		return count;
	}

	protected void AICF_CatalogFilter(string name, ResourceName resource)
	{
		SCR_WorkshopUiCommon.AICF_ProbeTagMetadata(resource);
		Resource config = BaseContainerTools.LoadContainer(resource);
		BaseContainer category = config.GetResource().ToBaseContainer();
		BaseContainerList filters = category.GetObjectArray("m_aFilters");
		bool valid = filters && filters.Count() > 0;
		for (int i; filters && i < filters.Count(); i++)
		{
			BaseContainer filter = filters.Get(i);
			string key, image;
			ResourceName thumbnail;
			filter.Get("m_sInternalName", key);
			filter.Get("m_sImageName", image);
			key.ToLower();
			if (SCR_WorkshopUiCommon.GetTagImage(key) != image) valid = false;
			if (filter.Get("m_sDefaultThumbnail", thumbnail) && !thumbnail.IsEmpty() && SCR_WorkshopUiCommon.s_sAddonTagDefaultThumbnailMap.Get(key) != thumbnail) valid = false;
		}
		AICF_CatalogCheck(name, valid);
	}

	protected void AICF_CatalogReadback()
	{
		ResourceName setup = "{A1CF261019500001}Missions/AICF_Conflict_Arland_Setup.conf";
		SCR_MissionHeaderCampaign header = SCR_MissionHeaderCampaign.Cast(MissionHeader.ReadMissionHeader(setup));
		WorkshopApi workshop = GetGame().GetBackendApi().GetWorkshop();
		array<MissionWorkshopItem> items = {};
		workshop.GetPageScenarios(items, 0, 1000);
		array<ResourceName> first = GetGame().GetDefaultGameConfigs();
		array<ResourceName> second = GetGame().GetDefaultGameConfigs();
		MissionWorkshopItem example = workshop.GetInGameScenario(setup);
		if (example) Print(string.Format("[AICF][CATALOG_ITEM] default=%1 page=%2 native_header=%3", first.Find(setup), AICF_CatalogCount(items, setup), example.GetHeader() != null));
		AICF_CatalogCheck("EXAMPLE", header && first.Find(setup) >= 0 && example && AICF_CatalogCount(items, setup) == 1 && SCR_ScenarioUICommon.AICF_GetSetupHeader(example));
		AICF_CatalogCheck("VISIBLE_NAME", example && header.m_bShowInScenarioMenu && WidgetManager.Translate(example.Name()) == "AI Conflict");
		AICF_CatalogCheck("REPEAT", first.Count() == second.Count() && first.Find(setup) == second.Find(setup));
		ResourceName stock = "{C41618FD18E9D714}Missions/23_Campaign_Arland.conf";
		AICF_CatalogCheck("STOCK_PRESERVED", first.Find(stock) >= 0 && workshop.GetInGameScenario(stock));
		AICF_ScenarioRegistry registry = new AICF_ScenarioRegistry();
		for (int mode; mode < 3; mode++)
		{
			for (int difficulty; difficulty < 3; difficulty++)
			{
				ResourceName preset;
				if (header) preset = header.AICF_ResolveSetupPreset(mode, difficulty);
				SCR_MissionHeaderCampaign targetHeader = SCR_MissionHeaderCampaign.Cast(MissionHeader.ReadMissionHeader(preset));
				AICF_CatalogCheck(string.Format("TARGET_%1_%2", mode, difficulty), !preset.IsEmpty() && registry.IsLeaf(preset) && first.Find(preset) < 0 && targetHeader && targetHeader.m_eAICFMatchMode == mode && targetHeader.m_eAICFDifficulty == difficulty && AICF_CatalogCount(items, preset) == 0);
			}
		}
		AICF_CatalogFilter("FILTER_TYPE", SCR_WorkshopUiCommon.ADDON_TYPE_FILTER_CATEGORY_CONFIG);
		AICF_CatalogFilter("FILTER_MANW", SCR_WorkshopUiCommon.ADDON_MANW_FILTER_CATEGORY_CONFIG);
		Print(string.Format("[AICF][CATALOG_DONE] checks=%1 failed=%2", m_iAICFCatalogChecks, m_iAICFCatalogFailures));
		GetGame().GetCallqueue().CallLater(AICF_CatalogClose, 1000, false);
	}

	protected void AICF_CatalogClose()
	{
		GetGame().RequestClose();
	}

	protected void AICF_CatalogTimeout()
	{
		Print("[AICF][CATALOG_TIMEOUT]");
		GetGame().RequestClose();
	}

	void Stop()
	{
		GetGame().GetCallqueue().Remove(AICF_CatalogReadback);
		GetGame().GetCallqueue().Remove(AICF_CatalogClose);
		GetGame().GetCallqueue().Remove(AICF_CatalogTimeout);
	}
}

modded class SCR_WorkshopUiCommon
{
	static void AICF_ProbeTagMetadata(ResourceName resource)
	{
		InitAddonTagMaps(resource);
	}
}

modded class ArmaReforgerScripted
{
	protected ref AICF_ScenarioCatalogProbe m_AICFCatalogProbe;

	override bool OnGameStart()
	{
		bool started = super.OnGameStart();
		if (System.IsCLIParam("aicfCatalogProbe"))
		{
			m_AICFCatalogProbe = new AICF_ScenarioCatalogProbe();
			m_AICFCatalogProbe.Start();
		}
		return started;
	}

	override void OnGameEnd()
	{
		if (m_AICFCatalogProbe) m_AICFCatalogProbe.Stop();
		m_AICFCatalogProbe = null;
		super.OnGameEnd();
	}
}
