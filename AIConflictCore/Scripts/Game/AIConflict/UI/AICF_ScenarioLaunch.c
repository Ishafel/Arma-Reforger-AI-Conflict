// Native transition получает точный граф: Vanilla обязан выгрузить RHS/WCS.
class AICF_ScenarioLaunch
{
	static bool SupportsForcedAddons(AICF_ScenarioVariant variant, array<string> forced)
	{
		if (!variant || !forced) return false;
		foreach (string id : forced)
			if (!GameProject.IsVanillaAddon(id) && variant.m_aAddons.Find(id) < 0) return false;
		return true;
	}

	static bool IsForcedGraphCompatible(AICF_ScenarioVariant variant)
	{
		array<string> forced = {};
		string csv, project;
		if (System.GetCLIParam("addons", csv)) csv.Split(",", forced, true);
		if (System.GetCLIParam("gproj", project))
		{
			project.Replace("\\", "/");
			project.ToLower();
			AICF_ScenarioRegistry registry = new AICF_ScenarioRegistry();
			string guid = registry.ResolveSourceProject(project);
			if (guid.IsEmpty()) return false;
			forced.Insert(guid);
		}
		return SupportsForcedAddons(variant, forced);
	}

	static bool CanStart(AICF_ScenarioSetupSelection selection, MissionWorkshopItem entry, SCR_DSConfig config = null)
	{
		if (!selection || !entry || selection.GetResource().IsEmpty() || !selection.GetEnvironment().IsValid()) return false;
		if ((!GameStateTransitions.IsInsideMainMenu() && !AICF_ScenarioBootstrap.CanConfigure(entry)) || GameStateTransitions.IsTransitionRequestedOrInProgress()) return false;
		if (config)
		{
			if (!config.game || SCR_ScenarioUICommon.IsHostingDisabledOnPlatform() || !SCR_ScenarioUICommon.CanHost(entry)) return false;
		}
		else
		{
			// A preparation Listen session is already hosted; selection changes its scenario.
			if (AICF_ScenarioBootstrap.IsHost(entry) && SCR_ScenarioUICommon.IsHostingDisabledOnPlatform()) return false;
			if (!SCR_ScenarioUICommon.CanPlay(entry)) return false;
		}
		SCR_DownloadManager downloads = SCR_DownloadManager.GetInstance();
		if (downloads)
		{
			int completed, total;
			downloads.GetDownloadQueueState(completed, total);
			if (total > 0) return false;
		}
		else
		{
			if (!AICF_ScenarioBootstrap.CanConfigure(entry)) return false;
			WorkshopApi workshop = GetGame().GetBackendApi().GetWorkshop();
			if (!workshop || workshop.NeedAddonsScan()) return false;
			array<WorkshopItem> nativeDownloads = {};
			int count = workshop.GetDownloads(nativeDownloads);
			if (count < 0 || count != nativeDownloads.Count()) return false;
			foreach (WorkshopItem item : nativeDownloads)
				if (!item || item.IsProcessed() || item.GetPendingDownload()) return false;
		}
		string missing;
		AICF_ScenarioVariant variant = selection.GetVariant();
		return variant && variant.IsContentEnabled() && variant.IsAvailable(missing) && IsForcedGraphCompatible(variant);
	}

	static bool Start(AICF_ScenarioSetupSelection selection, MissionWorkshopItem entry, SCR_DSConfig config = null)
	{
		if (!CanStart(selection, entry, config)) return false;
		AICF_ScenarioVariant variant = selection.GetVariant();
		ResourceName resource = selection.GetResource();
		if (!AICF_ScenarioEnvironment.Queue(resource, selection.GetEnvironment())) return false;
		bool accepted;
		if (!config && AICF_ScenarioBootstrap.IsHost(entry) && !AICF_ScenarioHostTransition.HasExactGraph(variant))
			accepted = AICF_ScenarioHostTransition.Queue(selection, entry);
		else if (!config)
			accepted = GameStateTransitions.RequestScenarioChangeTransition(resource, "", variant.GetAddonList());
		else
		{
			// Сохраняем сеть/пароль/лимиты; при отказе откатываем только launch fields.
			string oldScenario = config.game.scenarioId;
			string oldOwner = config.game.hostedScenarioModId;
			array<ref DSMod> oldMods = config.game.mods;
			config.game.scenarioId = resource;
			config.game.hostedScenarioModId = variant.m_sOwner;
			config.game.mods = variant.BuildHostMods();
			accepted = GameStateTransitions.RequestPublicServerTransition(config);
			// Public transition не принимает addonList. В том же кадре передаём граф
			// pending Host-переходу; иначе engine перезагружает только vanilla.
			if (accepted) GetGame().RequestReload(variant.m_aAddons);
			if (!accepted)
			{
				config.game.scenarioId = oldScenario;
				config.game.hostedScenarioModId = oldOwner;
				config.game.mods = oldMods;
			}
		}
		if (!accepted)
		{
			AICF_ScenarioEnvironment.Clear();
			return false;
		}
		Print(string.Format("[AICF][SCENARIO_LAUNCH] variant=%1 mode=%2 difficulty=%3 resource=%4 addons=%5", variant.m_sKey, selection.GetMode(), selection.GetDifficulty(), resource, variant.GetAddonList()));
		SCR_ScenarioUICommon.AICF_CloseSetup();
		GetGame().GetMenuManager().CloseAllMenus();
		return true;
	}
}
