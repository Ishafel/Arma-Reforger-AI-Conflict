// Test-only source stage: bypass the warm UI hook through native MissionWorkshopItem.Play.
modded class SCR_ScenarioUICommon
{
	static AICF_ScenarioSetupScreen AICF_ColdProbeScreen() { return s_AICFSetupScreen; }
}

modded class AICF_ScenarioSetupScreen
{
	SCR_DSConfig AICF_ColdProbeConfig() { return m_Config; }
	bool AICF_ColdProbeIsHost() { return m_bHost; }
}

class AICF_WorkshopBootstrapProbe
{
	static const string PHASE_KEY = "AICF_WorkshopBootstrapProbe";
	protected int m_iFailed;
	protected int m_iChecks;
	protected int m_iWaits;

	void Start()
	{
		GetGame().GetCallqueue().CallLater(Readback, 3000, false);
		GetGame().GetCallqueue().CallLater(Timeout, 90000, false);
	}

	protected void Check(string name, bool passed)
	{
		m_iChecks++;
		if (!passed) m_iFailed++;
		Print(string.Format("[AICF][COLD_CASE] case=%1 pass=%2", name, passed));
	}

	protected void Readback()
	{
		string phase;
		GameSessionStorage.s_Data.Find(PHASE_KEY, phase);
		bool menu = GameStateTransitions.IsInsideMainMenu();
		ResourceName setup = "{A1CF261019500001}Missions/AICF_Conflict_Arland_Setup.conf";
		MissionWorkshopItem entry = GetGame().GetBackendApi().GetWorkshop().GetInGameScenario(setup);
		MissionWorkshopItem current = WorkshopApi.GetCurrentMission();
		if (!entry && current && current.Id() == setup) entry = current;
		Print(string.Format("[AICF][COLD_PHASE] phase=%1 menu=%2", phase, menu));
		if (phase.IsEmpty() && menu)
		{
			SCR_MissionHeaderCampaign header = SCR_MissionHeaderCampaign.Cast(MissionHeader.ReadMissionHeader(setup));
			Check("LITERAL_NAME", header && header.m_sName == "AI Conflict");
			AICF_ScenarioRegistry baseRegistry = new AICF_ScenarioRegistry();
			AICF_ScenarioVariant baseVariant = baseRegistry.Find(0, 0);
			Check("FRONTEND_GRAPH", header && baseVariant && header.AICF_ResolveSetupPreset(0, 0) == baseVariant.Resolve(0, 0) && baseVariant.m_aAddons.Count() == 2);
			Check("NATIVE_ENTRY", entry != null);
			if (m_iFailed) { Close(); return; }
			GameSessionStorage.s_Data[PHASE_KEY] = "BOOT";
			if (System.IsCLIParam("aicfWarmBootstrapControl"))
			{
				SCR_ScenarioUICommon.AICF_OpenSetup(entry);
				GetGame().GetCallqueue().CallLater(Readback, 3000, false);
				return;
			}
			// Direct native launch reproduces the boundary used before addon scripts exist.
			if (System.IsCLIParam("aicfColdHostProbe"))
			{
				SCR_DSConfig config = new SCR_DSConfig();
				GetGame().GetBackendApi().SetDefaultIpPort(config);
				config.bindAddress = "127.0.0.1";
				config.publicAddress = "127.0.0.1";
				config.bindPort = 24073;
				config.publicPort = 24073;
				config.game.name = "AICF cold bootstrap probe";
				config.game.password = "fixture-only";
				config.game.maxPlayers = 4;
				config.game.visible = false;
				config.game.gameProperties.battlEye = false;
				config.game.scenarioId = setup;
				AICF_ScenarioRegistry registry = new AICF_ScenarioRegistry();
				AICF_ScenarioVariant variant = registry.Find(0, 0);
				config.game.hostedScenarioModId = variant.m_sOwner;
				config.game.mods = variant.BuildHostMods();
				config.Pack();
				// Source MissionWorkshopItem.Host has no packaged metadata/header.
				// Exercise the real preparation Listen context through native transition.
				Check("HOST_REQUEST", GameStateTransitions.RequestPublicServerTransition(config));
				GetGame().RequestReload(variant.m_aAddons);
			}
			else entry.Play();
			return;
		}
		if (phase == "BOOT" && (menu || SCR_ScenarioUICommon.AICF_IsSetupOpen()))
		{
			AICF_ScenarioSetupScreen screen = SCR_ScenarioUICommon.AICF_ColdProbeScreen();
			Check("SETUP_OPEN", screen && screen.GetRoot());
			Check("BOOTSTRAP_CONTEXT", AICF_ScenarioBootstrap.ShouldIntercept() == !System.IsCLIParam("aicfWarmBootstrapControl"));
			if (!screen || !entry) { Close(); return; }
			if (System.IsCLIParam("aicfColdCancelProbe"))
			{
				GameSessionStorage.s_Data[PHASE_KEY] = "CANCEL";
				screen.Cancel();
				return;
			}
			AICF_ScenarioSetupSelection selection = screen.GetSelection();
			Check("DEFAULTS", selection.GetMode() == 2 && selection.GetDifficulty() == 1);
			if (System.IsCLIParam("aicfColdHostProbe"))
			{
				Check("HOST_CONTEXT", screen.AICF_ColdProbeIsHost() && RplSession.Mode() == RplMode.Listen);
			}
			Check("SELECT", selection.SelectVariant(0, 0) && selection.Select(0, 0));
			Check("SELECT_ENVIRONMENT", selection.SelectEnvironment(577, 4));
			GameSessionStorage.s_Data[PHASE_KEY] = "LEAF";
			Check("CAN_START", AICF_ScenarioLaunch.CanStart(selection, entry, screen.AICF_ColdProbeConfig()));
			Check("LAUNCH", AICF_ScenarioLaunch.Start(selection, entry, screen.AICF_ColdProbeConfig()));
			if (m_iFailed) Close();
			return;
		}
		if (phase == "BOOT" && !menu)
		{
			if (m_iWaits++ < 35)
			{
				GetGame().GetCallqueue().CallLater(Readback, 1000, false);
				return;
			}
			Check("BOOT_RETURN", false);
			Close();
			return;
		}
		if (phase == "LEAF" && !menu)
		{
			MissionHeader header = GetGame().GetMissionHeader();
			Check("SELECTED_HEADER", header && header.GetHeaderResourceName() == "{BC2437E4861B4FD2}Missions/AICF_Conflict_Arland.conf");
			Check("NO_BOOTSTRAP", !AICF_ScenarioBootstrap.ShouldIntercept());
			SCR_MissionHeader selected = SCR_MissionHeader.Cast(header);
			Check("APPLIED_ENVIRONMENT", selected && selected.m_AICFEnvironment && selected.m_AICFEnvironment.m_iStartMinutes == 577 && selected.m_AICFEnvironment.m_iWeather == 4);
			if (System.IsCLIParam("aicfColdHostProbe")) Check("HOST_LISTEN", RplSession.Mode() == RplMode.Listen);
			Print(string.Format("[AICF][COLD_DONE] checks=%1 failed=%2", m_iChecks, m_iFailed));
			GetGame().GetCallqueue().CallLater(Close, 2000, false);
		}
		if (phase == "CANCEL" && menu)
		{
			Check("CANCEL_RETURN", !AICF_ScenarioBootstrap.ShouldIntercept());
			Check("CANCEL_CLOSED", !SCR_ScenarioUICommon.AICF_IsSetupOpen());
			Print(string.Format("[AICF][COLD_DONE] checks=%1 failed=%2", m_iChecks, m_iFailed));
			GetGame().GetCallqueue().CallLater(Close, 1000, false);
		}
	}

	protected void Timeout()
	{
		Print("[AICF][COLD_TIMEOUT]", LogLevel.ERROR);
		Close();
	}
	protected void Close()
	{
		GameSessionStorage.s_Data.Remove(PHASE_KEY);
		GetGame().RequestClose();
	}
	void Stop()
	{
		GetGame().GetCallqueue().Remove(Readback);
		GetGame().GetCallqueue().Remove(Timeout);
		GetGame().GetCallqueue().Remove(Close);
	}
}

modded class ArmaReforgerScripted
{
	protected ref AICF_WorkshopBootstrapProbe m_AICFColdProbe;
	override bool OnGameStart()
	{
		bool started = super.OnGameStart();
		if (System.IsCLIParam("aicfColdBootstrapProbe"))
		{
			m_AICFColdProbe = new AICF_WorkshopBootstrapProbe();
			m_AICFColdProbe.Start();
		}
		return started;
	}
	override void OnGameEnd()
	{
		if (m_AICFColdProbe) m_AICFColdProbe.Stop();
		m_AICFColdProbe = null;
		super.OnGameEnd();
	}
}
