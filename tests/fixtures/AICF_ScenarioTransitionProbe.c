// Isolated frontend only. Invokes production launch without GUI input.
class AICF_ScenarioTransitionProbe
{
	static SCR_DSConfig HostConfig()
	{
		if (!System.IsCLIParam("aicfProbeHost")) return null;
		SCR_DSConfig config = new SCR_DSConfig();
		GetGame().GetBackendApi().SetDefaultIpPort(config);
		config.bindPort = 24021;
		config.publicPort = 24021;
		config.game.name = "AICF Host terminal probe";
		config.game.password = "fixture-only";
		config.game.passwordAdmin = "fixture-admin";
		config.game.maxPlayers = 4;
		config.game.visible = false;
		config.game.gameProperties.battlEye = false;
		SCR_DSGameProperties.Cast(config.game.gameProperties).disableThirdPerson = true;
		return config;
	}

	static void ReadHostConfig(AICF_ScenarioSetupSelection selection)
	{
		if (!System.IsCLIParam("aicfProbeHost")) return;
		bool listen = RplSession.Mode() == RplMode.Listen;
		Print(string.Format("[AICF][HOST_MODE] mode=%1 listen=%2", RplSession.Mode(), listen));
		// GetRunningDSConfig, ServerLobbyApi и GetServerInfo здесь не дают config.
		// Не выдаём request readback за подтверждение engine password/player limit.
	}

	static AICF_ScenarioSetupSelection Selection()
	{
		AICF_ScenarioSetupHeader header = AICF_ScenarioSetupHeader.Cast(MissionHeader.ReadMissionHeader("{A1CF261019500001}Missions/AICF_Conflict_Arland_Setup.conf"));
		AICF_ScenarioSetupSelection selection = new AICF_ScenarioSetupSelection(header);
		string mapIndex, content, mode, difficulty;
		if (!System.GetCLIParam("aicfProbeMap", mapIndex) || !System.GetCLIParam("aicfProbeContent", content) || !System.GetCLIParam("aicfProbeMode", mode) || !System.GetCLIParam("aicfProbeDifficulty", difficulty)) return null;
		if (!selection.SelectVariant(mapIndex.ToInt(), content.ToInt()))
		{
			AICF_ScenarioVariant unavailable = selection.GetRegistry().Find(mapIndex.ToInt(), content.ToInt());
			string missing;
			if (unavailable)
			{
				unavailable.IsAvailable(missing);
				Print(string.Format("[AICF][TRANSITION_UNAVAILABLE] variant=%1 missing=%2", unavailable.m_sKey, missing));
				WorkshopItem item = GetGame().GetBackendApi().GetWorkshop().FindItem(missing);
				if (item)
				{
					Revision revision = item.GetActiveRevision();
					Print(string.Format("[AICF][TRANSITION_REVISION] active=%1 local=%2 ready=%3", revision != null, item.GetLocalRevision() != null, item.IsReadyToRun()));
					if (revision) Print(string.Format("[AICF][TRANSITION_REVISION_STATE] version=%1 downloaded=%2 corrupted=%3", revision.GetVersion(), revision.IsDownloaded(), revision.IsCorrupted()));
				}
			}
			return null;
		}
		if (!selection.Select(mode.ToInt(), difficulty.ToInt())) return null;
		string time, weather;
		if (System.GetCLIParam("aicfProbeTime", time) && System.GetCLIParam("aicfProbeWeather", weather))
			if (!selection.SelectEnvironment(time.ToInt(), weather.ToInt())) return null;
		return selection;
	}
	static void Launch()
	{
		if (!GameStateTransitions.IsInsideMainMenu()) return;
		AICF_ScenarioSetupSelection selection = Selection();
		MissionWorkshopItem entry = SCR_ScenarioUICommon.GetInGameScenario("{A1CF261019500001}Missions/AICF_Conflict_Arland_Setup.conf");
		int complete, total;
		SCR_DownloadManager.GetInstance().GetDownloadQueueState(complete, total);
		string missing;
		if (selection) Print(string.Format("[AICF][TRANSITION_PREFLIGHT] entry=%1 native=%2 dependencies=%3 forced=%4 completed=%5 total=%6", entry != null, SCR_ScenarioUICommon.CanPlay(entry), selection.GetVariant().IsAvailable(missing), AICF_ScenarioLaunch.IsForcedGraphCompatible(selection.GetVariant()), complete, total));
		string project;
		if (System.GetCLIParam("gproj", project))
		{
			BaseContainer settings = GameProject.GetGameProjectSettings();
			if (settings)
			{
				string guid;
				if (settings.GetVarIndex("GUID") >= 0) settings.Get("GUID", guid);
				Print(string.Format("[AICF][TRANSITION_PROJECT] class=%1 guid=%2 count=%3", settings.GetClassName(), guid, settings.GetNumVars()));
			}
		}
		SCR_DSConfig config = HostConfig();
		bool ready = AICF_ScenarioLaunch.CanStart(selection, entry, config);
		Print(string.Format("[AICF][TRANSITION_CASE] case=PREFLIGHT pass=%1", ready));
		bool accepted = AICF_ScenarioLaunch.Start(selection, entry, config);
		Print(string.Format("[AICF][TRANSITION_CASE] case=ACCEPTED pass=%1", accepted));
		if (config)
		{
			SCR_DSConfig expectedConfig = HostConfig();
			bool retained = config.bindPort == expectedConfig.bindPort && config.publicPort == expectedConfig.publicPort;
			retained = retained && config.game.name == expectedConfig.game.name && config.game.maxPlayers == expectedConfig.game.maxPlayers;
			retained = retained && config.game.password == expectedConfig.game.password && config.game.passwordAdmin == expectedConfig.game.passwordAdmin;
			retained = retained && !config.game.visible && !config.game.gameProperties.battlEye && SCR_DSGameProperties.Cast(config.game.gameProperties).disableThirdPerson;
			Print(string.Format("[AICF][HOST_REQUEST] pass=%1", retained));
		}
		if (!accepted) GetGame().RequestClose();
	}
	static void Close() { GetGame().RequestClose(); }
	static void Timeout() { Print("[AICF][TRANSITION_TIMEOUT]"); Close(); }
}

modded class ArmaReforgerScripted
{
	override bool OnGameStart()
	{
		bool started = super.OnGameStart();
		if (System.IsCLIParam("aicfTransitionProbe"))
		{
			WorkshopApi workshop = GetGame().GetBackendApi().GetWorkshop();
			if (workshop.NeedAddonsScan()) workshop.ScanOfflineItems();
			GetGame().GetCallqueue().CallLater(AICF_ScenarioTransitionProbe.Launch, 12000, false);
			GetGame().GetCallqueue().CallLater(AICF_ScenarioTransitionProbe.Timeout, 180000, false);
		}
		return started;
	}
	override void OnGameEnd()
	{
		GetGame().GetCallqueue().Remove(AICF_ScenarioTransitionProbe.Launch);
		GetGame().GetCallqueue().Remove(AICF_ScenarioTransitionProbe.Timeout);
		GetGame().GetCallqueue().Remove(AICF_ScenarioTransitionProbe.Close);
		super.OnGameEnd();
	}
}

modded class AICF_MatchController
{
	protected bool m_bAICFTransitionRead;
	protected int m_iAICFTransitionFrameSample;
	protected int m_iAICFTransitionObservedAt;
	protected ref AICF_GroupSlot m_AICFTransitionClock;
	protected bool m_bAICFTransitionPauseChecked;
	override protected void Update()
	{
		ChimeraWorld world = ChimeraWorld.CastFrom(GetGame().GetWorld());
		// Только fixture: Welcome pause не должна блокировать автоматический readback.
		if (System.IsCLIParam("aicfProbeTime") && world) world.PauseGameTime(false);
		if (System.IsCLIParam("aicfTransitionProbe"))
		{
			if (!m_iAICFTransitionObservedAt && m_USState && m_USSRState && m_USState.GetSlotCount() + m_USSRState.GetSlotCount() > 0)
			{
				m_iAICFTransitionObservedAt = System.GetTickCount();
				m_AICFTransitionClock = new AICF_GroupSlot(99, AICF_EGroupRole.ATTACK);
				m_AICFTransitionClock.BeginInitialSpawn();
			}
			string resume;
			if (world && m_iAICFTransitionObservedAt && System.GetCLIParam("aicfProbeResumeAfter", resume))
			{
				if (System.GetTickCount(m_iAICFTransitionObservedAt) < resume.ToInt() * 1000)
					world.PauseGameTime(true);
				else
				{
					if (!m_bAICFTransitionPauseChecked)
					{
						bool pausePassed = world.IsGameTimePaused() && resume.ToInt() >= 35 && m_AICFTransitionClock.GetSpawnSimulationAgeMs() < 2000;
						Print(string.Format("[AICF][TRANSITION_PAUSE] pass=%1 real_ms=%2 simulation_ms=%3", pausePassed, System.GetTickCount(m_iAICFTransitionObservedAt), m_AICFTransitionClock.GetSpawnSimulationAgeMs()));
						m_bAICFTransitionPauseChecked = true;
					}
					world.PauseGameTime(false);
				}
			}
		}
		super.Update();
		if (m_bAICFTransitionRead || !System.IsCLIParam("aicfTransitionProbe")) return;
		if (System.GetTickCount(m_iAICFTransitionFrameSample) >= 5000)
		{
			m_iAICFTransitionFrameSample = System.GetTickCount();
			Print(string.Format("[AICF][TRANSITION_FRAME] fps=%1 frame_ms=%2 roster=%3 paused=%4", System.GetFPS(), System.GetFrameTimeS() * 1000, m_bRosterReady, world && world.IsGameTimePaused()));
		}
		if (m_bStopped)
		{
			m_bAICFTransitionRead = true;
			Print("[AICF][TRANSITION_ABORT] controller_stopped=1");
			GetGame().RequestClose();
			return;
		}
		if (!m_bRosterReady) return;
		if (System.IsCLIParam("aicfProbeResumeAfter") && !m_bAICFTransitionPauseChecked) return;
		m_bAICFTransitionRead = true;
		AICF_ScenarioSetupSelection selection = AICF_ScenarioTransitionProbe.Selection();
		if (System.IsCLIParam("aicfProbeTime"))
		{
			TimeAndWeatherManagerEntity manager = world.GetTimeAndWeatherManager();
			AICF_ScenarioEnvironment expected = selection.GetEnvironment();
			string weather = manager.GetCurrentWeatherState().GetStateName();
			float hour = manager.GetTimeOfTheDay();
			float wanted = expected.GetHour() + expected.GetMinute() / 60.0;
			float difference = Math.AbsFloat(hour - wanted);
			if (difference > 12) difference = 24 - difference;
			bool matches = expected.m_iStartMinutes == AICF_ScenarioEnvironment.RANDOM_TIME || difference < 0.25;
			matches = matches && (expected.m_iWeather == 0 || weather == expected.GetWeatherName());
			Print(string.Format("[AICF][TRANSITION_ENVIRONMENT] pass=%1 time_minutes=%2 weather_mode=%3 hour=%4 weather=%5", matches, expected.m_iStartMinutes, expected.m_iWeather, hour, weather));
			SCR_MissionHeader environmentHeader = SCR_MissionHeader.Cast(GetGame().GetMissionHeader());
			float acceleration = environmentHeader.m_fDayTimeAcceleration;
			if (manager.IsNightHour(hour)) acceleration = environmentHeader.m_fNightTimeAcceleration;
			bool clock = manager.GetIsDayAutoAdvanced() && Math.AbsFloat(manager.GetDayDuration() - 86400.0 / acceleration) < 0.001 && !manager.IsWeatherLooping();
			Print(string.Format("[AICF][ENVIRONMENT_CLOCK] pass=%1", clock));
			// Повторная инициализация не должна сбрасывать часы и погоду.
			SCR_TimeAndWeatherHandlerComponent.GetInstance().SetupDaytimeAndWeather(7, 15);
			bool unchanged = manager.GetTimeOfTheDay() == hour && manager.GetCurrentWeatherState().GetStateName() == weather;
			Print(string.Format("[AICF][ENVIRONMENT_REPEAT] pass=%1", unchanged));
			SCR_TimeAndWeatherHandlerComponent.GetInstance().SetupDaytimeAndWeather(6, 30, 0, weather, true);
			bool restored = Math.AbsFloat(manager.GetTimeOfTheDay() - 6.5) < 0.001 && manager.GetCurrentWeatherState().GetStateName() == weather;
			SCR_TimeAndWeatherHandlerComponent.GetInstance().SetupDaytimeAndWeather(15, 0);
			Print(string.Format("[AICF][ENVIRONMENT_SAVE_RESTORE] pass=%1", restored && Math.AbsFloat(manager.GetTimeOfTheDay() - 6.5) < 0.001));
		}
		SCR_MissionHeaderCampaign header = SCR_MissionHeaderCampaign.Cast(GetGame().GetMissionHeader());
		bool target = selection && header && header.GetHeaderResourceName() == selection.GetResource() && header.m_eAICFMatchMode == selection.GetMode() && header.m_eAICFDifficulty == selection.GetDifficulty();
		Print(string.Format("[AICF][TRANSITION_CASE] case=ACTIVE_HEADER pass=%1", target));
		array<string> loaded = {};
		GameProject.GetLoadedAddons(loaded);
		bool graph = selection != null;
		if (selection)
		{
			array<string> expected = selection.GetVariant().m_aAddons;
			foreach (string id : expected) if (loaded.Find(id) < 0) graph = false;
			foreach (string id : loaded)
			{
				if (!GameProject.IsVanillaAddon(id) && expected.Find(id) < 0) graph = false;
				Print(string.Format("[AICF][TRANSITION_LOADED] id=%1 vanilla=%2", id, GameProject.IsVanillaAddon(id)));
			}
		}
		Print(string.Format("[AICF][TRANSITION_CASE] case=EXACT_GRAPH pass=%1", graph));
		AICF_ScenarioTransitionProbe.ReadHostConfig(selection);
		Print(string.Format("[AICF][TRANSITION_DONE] header=%1 graph=%2 roster=1", target, graph));
		GetGame().GetCallqueue().Remove(AICF_ScenarioTransitionProbe.Launch);
		GetGame().GetCallqueue().Remove(AICF_ScenarioTransitionProbe.Timeout);
		GetGame().GetCallqueue().CallLater(AICF_ScenarioTransitionProbe.Close, 3000, false);
	}
}
