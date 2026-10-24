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
	protected ref array<string> m_aAICFEnabledForProbe = {};
	protected void IntentCheck(string name, bool passed)
	{
		if (!passed) m_iFailed++;
		Print(string.Format("[AICF][HOST_INTENT_CASE] case=%1 pass=%2", name, passed));
	}
	protected void IntentContracts(ResourceName setup, MissionWorkshopItem entry)
	{
		AICF_ScenarioHostIntent intent = new AICF_ScenarioHostIntent();
		intent.m_iIssued = System.GetTickCount();
		intent.m_sEntry = setup;
		intent.m_iMap = 1;
		intent.m_iContent = 2;
		intent.m_iMode = 2;
		intent.m_iDifficulty = 2;
		intent.m_Environment.m_iStartMinutes = 577;
		intent.m_Environment.m_iWeather = 4;
		string data = intent.Encode();
		AICF_ScenarioHostIntent decoded = AICF_ScenarioHostIntent.Decode(data);
		IntentCheck("ROUNDTRIP", decoded && decoded.GetTarget() == intent.GetTarget() && decoded.m_sEntry == setup && decoded.m_Environment.m_iStartMinutes == 577 && decoded.m_Environment.m_iWeather == 4);
		IntentCheck("INVALID_JSON", !AICF_ScenarioHostIntent.Decode("invalid"));
		string invalid = data;
		invalid.Replace("\"schema\": 1", "\"schema\": 2");
		invalid.Replace("\"schema\":1", "\"schema\":2");
		IntentCheck("SCHEMA", invalid != data && !AICF_ScenarioHostIntent.Decode(invalid));
		intent.m_iPhase = 3;
		IntentCheck("PHASE", !AICF_ScenarioHostIntent.Decode(intent.Encode()));
		intent.m_iPhase = 0;
		intent.m_iIssued = System.GetTickCount() - 600001;
		IntentCheck("TTL", !AICF_ScenarioHostIntent.Decode(intent.Encode()));
		intent.m_iIssued = System.GetTickCount();
		intent.m_iMap = -1;
		IntentCheck("MAP", !AICF_ScenarioHostIntent.Decode(intent.Encode()));
		intent.m_iMap = 1;
		intent.m_iContent = 3;
		IntentCheck("CONTENT", !AICF_ScenarioHostIntent.Decode(intent.Encode()));
		intent.m_iContent = 2;
		intent.m_iMode = 3;
		IntentCheck("MODE", !AICF_ScenarioHostIntent.Decode(intent.Encode()));
		intent.m_iMode = 2;
		intent.m_iDifficulty = -1;
		IntentCheck("DIFFICULTY", !AICF_ScenarioHostIntent.Decode(intent.Encode()));
		intent.m_iDifficulty = 2;
		intent.m_Environment.m_iStartMinutes = 1441;
		IntentCheck("MINUTES", !AICF_ScenarioHostIntent.Decode(intent.Encode()));
		intent.m_Environment.m_iStartMinutes = 577;
		intent.m_Environment.m_iWeather = 5;
		IntentCheck("WEATHER", !AICF_ScenarioHostIntent.Decode(intent.Encode()));
		intent.m_Environment.m_iWeather = 4;
		AICF_ScenarioRegistry registry = new AICF_ScenarioRegistry();
		intent.m_sEntry = registry.Find(0, 0).Resolve(0, 0);
		IntentCheck("ENTRY", !AICF_ScenarioHostIntent.Decode(intent.Encode()));
		intent.m_sEntry = setup;
		IntentCheck("GRAPH", AICF_ScenarioHostTransition.HasExactGraph(registry.Find(0, 0)) && !AICF_ScenarioHostTransition.HasExactGraph(intent.GetVariant()));
		SCR_MissionHeaderCampaign header = SCR_MissionHeaderCampaign.Cast(MissionHeader.ReadMissionHeader(setup));
		AICF_ScenarioSetupSelection selection = new AICF_ScenarioSetupSelection(header);
		IntentCheck("HOST_ONLY", !AICF_ScenarioHostTransition.Queue(selection, entry) && !AICF_ScenarioHostTransition.HasPending());
	}
	protected SCR_DSConfig HostConfig()
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
		return config;
	}

	protected AICF_ScenarioVariant Target()
	{
		string key;
		if (!System.GetCLIParam("aicfColdVariant", key)) key = "Stock";
		AICF_ScenarioRegistry registry = new AICF_ScenarioRegistry();
		foreach (AICF_ScenarioVariant variant : registry.GetVariants())
			if (variant.m_sKey == key) return variant;
		return null;
	}
	protected int TargetMode()
	{
		string value;
		if (System.GetCLIParam("aicfColdMode", value)) return value.ToInt();
		return 0;
	}
	protected int TargetDifficulty()
	{
		string value;
		if (System.GetCLIParam("aicfColdDifficulty", value)) return value.ToInt();
		return 0;
	}
	protected bool EnableTargetContent(AICF_ScenarioVariant target)
	{
		if (!target) return false;
		WorkshopApi workshop = GetGame().GetBackendApi().GetWorkshop();
		foreach (string id : target.m_aContentAddons)
		{
			WorkshopItem item = workshop.FindItem(id);
			if (!item)
			{
				Print(string.Format("[AICF][COLD_CONTENT] missing=%1", id));
				return false;
			}
			if (item.IsEnabled()) continue;
			if (item.IsLoaded()) return false;
			item.Enable(true);
			m_aAICFEnabledForProbe.Insert(id);
			if (!item.IsEnabled()) return false;
		}
		return target.IsContentEnabled();
	}

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
		if (phase == "RETRY" && menu)
		{
			Check("RETRY_RETURN", !AICF_ScenarioBootstrap.ShouldIntercept() && entry != null);
			if (m_iFailed) { Close(); return; }
			GameSessionStorage.s_Data[PHASE_KEY] = "BOOT";
			SCR_ScenarioUICommon.AICF_OpenSetup(entry, HostConfig());
			GetGame().GetCallqueue().CallLater(Readback, 3000, false);
			return;
		}
		if (phase.IsEmpty() && menu)
		{
			if (System.IsCLIParam("aicfHostIntentContracts")) IntentContracts(setup, entry);
			SCR_MissionHeaderCampaign header = SCR_MissionHeaderCampaign.Cast(MissionHeader.ReadMissionHeader(setup));
			Check("LITERAL_NAME", header && header.m_sName == "AI Conflict");
			AICF_ScenarioRegistry baseRegistry = new AICF_ScenarioRegistry();
			AICF_ScenarioVariant baseVariant = baseRegistry.Find(0, 0);
			Check("FRONTEND_GRAPH", header && header.m_bAICFSetupEntry && baseVariant && !baseVariant.Resolve(0, 0).IsEmpty() && baseVariant.m_aAddons.Count() == 2);
			Check("NATIVE_ENTRY", entry != null);
			if (m_iFailed) { Close(); return; }
			GameSessionStorage.s_Data[PHASE_KEY] = "BOOT";
			if (System.IsCLIParam("aicfWarmBootstrapControl"))
			{
				SCR_DSConfig warmConfig;
				if (System.IsCLIParam("aicfColdHostProbe")) warmConfig = HostConfig();
				SCR_ScenarioUICommon.AICF_OpenSetup(entry, warmConfig);
				GetGame().GetCallqueue().CallLater(Readback, 3000, false);
				return;
			}
			// Direct native launch reproduces the boundary used before addon scripts exist.
			if (System.IsCLIParam("aicfColdHostProbe"))
			{
				SCR_DSConfig config = HostConfig();
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
			if (System.IsCLIParam("aicfColdRetryHost") && !menu)
			{
				Check("RETRY_CANCEL_CONTEXT", screen && screen.GetRoot() && screen.AICF_ColdProbeIsHost() && AICF_ScenarioBootstrap.IsHost(entry));
				if (m_iFailed) { Close(); return; }
				GameSessionStorage.s_Data[PHASE_KEY] = "RETRY";
				screen.Cancel();
				return;
			}
			Check("SETUP_OPEN", screen && screen.GetRoot());
			bool warmContext = System.IsCLIParam("aicfWarmBootstrapControl") || (System.IsCLIParam("aicfColdRetryHost") && menu);
			Check("BOOTSTRAP_CONTEXT", AICF_ScenarioBootstrap.ShouldIntercept() == !warmContext);
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
				bool hostContext = RplSession.Mode() == RplMode.Listen;
				if (warmContext) hostContext = menu && screen.AICF_ColdProbeConfig() != null;
				Check("HOST_CONTEXT", screen.AICF_ColdProbeIsHost() && hostContext);
			}
			AICF_ScenarioVariant target = Target();
			if (System.IsCLIParam("aicfColdVariant")) Check("CONTENT_READY", EnableTargetContent(target));
			Check("SELECT", target && selection.SelectVariant(target.m_iMap, target.m_iContent) && selection.Select(TargetMode(), TargetDifficulty()));
			if (m_iFailed) { Close(); return; }
			Check("SELECT_ENVIRONMENT", selection.SelectEnvironment(577, 4));
			GameSessionStorage.s_Data[PHASE_KEY] = "LEAF";
			Check("CAN_START", AICF_ScenarioLaunch.CanStart(selection, entry, screen.AICF_ColdProbeConfig()));
			bool launched = AICF_ScenarioLaunch.Start(selection, entry, screen.AICF_ColdProbeConfig());
			Check("LAUNCH", launched);
			if (System.IsCLIParam("aicfWarmBootstrapControl") && System.IsCLIParam("aicfColdHostProbe")) Check("HOST_REQUEST", launched);
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
			AICF_ScenarioVariant target = Target();
			Check("SELECTED_HEADER", header && target && header.GetHeaderResourceName() == target.Resolve(TargetMode(), TargetDifficulty()));
			Check("NO_BOOTSTRAP", !AICF_ScenarioBootstrap.ShouldIntercept());
			AICF_RuntimeScenarioSelection runtime = AICF_RuntimeScenario.GetSelection();
			Check("APPLIED_RULES", runtime && target && runtime.GetVariant().m_sKey == target.m_sKey &&
				runtime.m_iMode == TargetMode() && runtime.m_iDifficulty == TargetDifficulty());
			SCR_MissionHeader selected = SCR_MissionHeader.Cast(header);
			Check("APPLIED_ENVIRONMENT", selected && selected.m_AICFEnvironment && selected.m_AICFEnvironment.m_iStartMinutes == 577 && selected.m_AICFEnvironment.m_iWeather == 4);
			if (System.IsCLIParam("aicfColdHostProbe")) Check("HOST_LISTEN", RplSession.Mode() == RplMode.Listen);
			if (System.IsCLIParam("aicfColdVariant"))
			{
				string actualWorld = GetGame().GetWorldFile();
				string expectedWorld;
				if (header) expectedWorld = header.GetWorldPath();
				Print(string.Format("[AICF][COLD_WORLD] actual=%1 expected=%2", actualWorld, expectedWorld));
				actualWorld.Replace("\\", "/");
				expectedWorld.Replace("\\", "/");
				actualWorld.ToLower();
				expectedWorld.ToLower();
				Check("TARGET_WORLD", !expectedWorld.IsEmpty() && (actualWorld == expectedWorld || actualWorld.EndsWith("/" + expectedWorld) || actualWorld.EndsWith(":" + expectedWorld)));
				array<string> loaded = {};
				GameProject.GetLoadedAddons(loaded);
				foreach (string loadedId : loaded)
					Print(string.Format("[AICF][COLD_ADDON] id=%1 vanilla=%2", loadedId, GameProject.IsVanillaAddon(loadedId)));
				bool exact = target != null;
				if (target)
				{
					foreach (string id : target.m_aAddons)
						if (loaded.Find(id) < 0) exact = false;
					foreach (string id : loaded)
						if (!GameProject.IsVanillaAddon(id) && target.m_aAddons.Find(id) < 0) exact = false;
				}
				Check("TARGET_GRAPH", exact);
			}
			Print(string.Format("[AICF][COLD_DONE] checks=%1 failed=%2", m_iChecks, m_iFailed));
			int closeDelay = 2000;
			if (System.IsCLIParam("aicfColdReadyProbe")) closeDelay = 45000;
			GetGame().GetCallqueue().CallLater(Close, closeDelay, false);
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
		WorkshopApi workshop = GetGame().GetBackendApi().GetWorkshop();
		foreach (string id : m_aAICFEnabledForProbe)
		{
			WorkshopItem item = workshop.FindItem(id);
			if (item && !item.IsLoaded()) item.Enable(false);
		}
		m_aAICFEnabledForProbe.Clear();
		GetGame().GetCallqueue().Remove(Readback);
		GetGame().GetCallqueue().Remove(Timeout);
		GetGame().GetCallqueue().Remove(Close);
	}
}

modded class AICF_MatchController
{
	protected bool m_bAICFColdReady;
	override protected void Update()
	{
		super.Update();
		if (m_bAICFColdReady || !System.IsCLIParam("aicfColdReadyProbe") || !m_bRosterReady) return;
		m_bAICFColdReady = true;
		Print(string.Format("[AICF][COLD_READY] listen=%1 address=%2 pending=%3", RplSession.Mode() == RplMode.Listen, RplSession.TryGetListenAddress(), AICF_ScenarioHostTransition.HasPending()));
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
