// Listen не меняет набор scripts при ScenarioChange. Загружаем граф через menu,
// затем используем сохранённую движком Host config без копирования паролей.
class AICF_ScenarioHostIntent
{
	int m_iPhase;
	int m_iIssued;
	ResourceName m_sEntry;
	int m_iMap;
	int m_iContent;
	int m_iMode;
	int m_iDifficulty;
	ref AICF_ScenarioEnvironment m_Environment = new AICF_ScenarioEnvironment();

	AICF_ScenarioVariant GetVariant()
	{
		AICF_ScenarioRegistry registry = new AICF_ScenarioRegistry();
		return registry.Find(m_iMap, m_iContent);
	}
	ResourceName GetTarget()
	{
		AICF_ScenarioVariant variant = GetVariant();
		if (!variant) return "";
		return variant.Resolve(m_iMode, m_iDifficulty);
	}
	string Encode()
	{
		JsonSaveContext json = new JsonSaveContext();
		json.WriteValue("schema", 1);
		json.WriteValue("phase", m_iPhase);
		json.WriteValue("issued", m_iIssued);
		json.WriteValue("entry", m_sEntry);
		json.WriteValue("map", m_iMap);
		json.WriteValue("content", m_iContent);
		json.WriteValue("mode", m_iMode);
		json.WriteValue("difficulty", m_iDifficulty);
		json.WriteValue("minutes", m_Environment.m_iStartMinutes);
		json.WriteValue("weather", m_Environment.m_iWeather);
		return json.SaveToString();
	}
	static AICF_ScenarioHostIntent Decode(string data)
	{
		JsonLoadContext json = new JsonLoadContext();
		AICF_ScenarioHostIntent intent = new AICF_ScenarioHostIntent();
		int schema;
		if (!json.LoadFromString(data) || !json.ReadValue("schema", schema) || schema != 1 ||
			!json.ReadValue("phase", intent.m_iPhase) || intent.m_iPhase < 0 || intent.m_iPhase > 2 ||
			!json.ReadValue("issued", intent.m_iIssued) || !json.ReadValue("entry", intent.m_sEntry) ||
			!json.ReadValue("map", intent.m_iMap) || !json.ReadValue("content", intent.m_iContent) ||
			!json.ReadValue("mode", intent.m_iMode) || !json.ReadValue("difficulty", intent.m_iDifficulty) ||
			!json.ReadValue("minutes", intent.m_Environment.m_iStartMinutes) ||
			!json.ReadValue("weather", intent.m_Environment.m_iWeather) || !intent.m_Environment.IsValid()) return null;
		int age = System.GetTickCount(intent.m_iIssued);
		if (age < 0 || age > 600000 || intent.GetTarget().IsEmpty()) return null;
		SCR_MissionHeaderCampaign entry = SCR_MissionHeaderCampaign.Cast(MissionHeader.ReadMissionHeader(intent.m_sEntry));
		if (!entry || !entry.m_bAICFSetupEntry) return null;
		return intent;
	}
}

class AICF_ScenarioHostTransition
{
	static const string SESSION_KEY = "AICF_ScenarioHostTransition";
	protected string m_sIntent;
	protected int m_iAttempts;

	static bool HasPending() { return GameSessionStorage.s_Data.Contains(SESSION_KEY); }
	static bool HasExactGraph(AICF_ScenarioVariant variant)
	{
		if (!variant) return false;
		array<string> loaded = {};
		GameProject.GetLoadedAddons(loaded);
		foreach (string id : variant.m_aAddons)
			if (loaded.Find(id) < 0) return false;
		foreach (string id : loaded)
			if (!GameProject.IsVanillaAddon(id) && variant.m_aAddons.Find(id) < 0) return false;
		return true;
	}
	static bool Queue(AICF_ScenarioSetupSelection selection, MissionWorkshopItem entry)
	{
		if (!selection || !AICF_ScenarioBootstrap.IsHost(entry) || HasPending() || GameStateTransitions.IsTransitionRequestedOrInProgress()) return false;
		AICF_ScenarioHostIntent intent = new AICF_ScenarioHostIntent();
		intent.m_iIssued = System.GetTickCount();
		intent.m_sEntry = entry.Id();
		intent.m_iMap = selection.GetMap();
		intent.m_iContent = selection.GetContent();
		intent.m_iMode = selection.GetMode();
		intent.m_iDifficulty = selection.GetDifficulty();
		intent.m_Environment.m_iStartMinutes = selection.GetEnvironment().m_iStartMinutes;
		intent.m_Environment.m_iWeather = selection.GetEnvironment().m_iWeather;
		string data = intent.Encode();
		if (!AICF_ScenarioHostIntent.Decode(data)) return false;
		GameSessionStorage.s_Data[SESSION_KEY] = data;
		GameStateTransitions.RequestGameplayEndTransition();
		// GameplayEnd сам выбирает vanilla. Явный reload сохраняет обработчик intent.
		GetGame().RequestReload(selection.GetVariant().m_aAddons);
		Print("[AICF][HOST_HANDOFF] phase=RETURN");
		return true;
	}
	void Start()
	{
		if (!GameSessionStorage.s_Data.Find(SESSION_KEY, m_sIntent)) return;
		GetGame().GetCallqueue().CallLater(Resume, 500, false);
	}
	protected void Fail(string reason)
	{
		GameSessionStorage.s_Data.Remove(SESSION_KEY);
		AICF_ScenarioEnvironment.Clear();
		Print(string.Format("[AICF][HOST_HANDOFF][FAIL] reason=%1", reason), LogLevel.ERROR);
		// Возвращаем picker только в той же локальной preparation session.
		if (!AICF_ScenarioBootstrap.ShouldIntercept()) return;
		MissionWorkshopItem entry = WorkshopApi.GetCurrentMission();
		if (AICF_ScenarioBootstrap.IsHost(entry)) SCR_ScenarioUICommon.AICF_OpenSetup(entry);
	}
	protected void Resume()
	{
		string current;
		if (!GameSessionStorage.s_Data.Find(SESSION_KEY, current) || current != m_sIntent) return;
		AICF_ScenarioHostIntent intent = AICF_ScenarioHostIntent.Decode(current);
		if (!intent) { Fail("INVALID_INTENT"); return; }
		if (GameStateTransitions.IsTransitionRequestedOrInProgress())
		{
			if (m_iAttempts++ < 120)
			{
				GetGame().GetCallqueue().CallLater(Resume, 250, false);
				return;
			}
			Fail("TRANSITION_TIMEOUT");
			return;
		}
		AICF_ScenarioVariant variant = intent.GetVariant();
		if (!HasExactGraph(variant)) { Fail("GRAPH"); return; }
		if (intent.m_iPhase == 0)
		{
			if (!GameStateTransitions.IsInsideMainMenu()) { Fail("MENU_CONTEXT"); return; }
			intent.m_iPhase = 1;
			GameSessionStorage.s_Data[SESSION_KEY] = intent.Encode();
			// null использует исходную native config, как штатный HostGameConfig.
			bool accepted = GameStateTransitions.RequestPublicServerTransition(null);
			if (!accepted) { Fail("HOST_REJECTED"); return; }
			GetGame().RequestReload(variant.m_aAddons);
			GetGame().GetMenuManager().CloseAllMenus();
			Print("[AICF][HOST_HANDOFF] phase=HOST");
			return;
		}
		MissionHeader active = GetGame().GetMissionHeader();
		if (GameStateTransitions.IsInsideMainMenu() || RplSession.Mode() != RplMode.Listen || !active) { Fail("HOST_CONTEXT"); return; }
		if (intent.m_iPhase == 1)
		{
			SCR_MissionHeaderCampaign setup = SCR_MissionHeaderCampaign.Cast(active);
			if (active.GetHeaderResourceName() != intent.m_sEntry || !setup || !setup.m_bAICFSetupEntry) { Fail("ENTRY_IDENTITY"); return; }
			if (!AICF_ScenarioEnvironment.Queue(intent.GetTarget(), intent.m_Environment)) { Fail("ENVIRONMENT"); return; }
			intent.m_iPhase = 2;
			GameSessionStorage.s_Data[SESSION_KEY] = intent.Encode();
			if (!GameStateTransitions.RequestScenarioChangeTransition(intent.GetTarget(), "", variant.GetAddonList())) { Fail("LEAF_REJECTED"); return; }
			GetGame().GetMenuManager().CloseAllMenus();
			Print("[AICF][HOST_HANDOFF] phase=LEAF");
			return;
		}
		if (active.GetHeaderResourceName() != intent.GetTarget()) { Fail("TARGET_IDENTITY"); return; }
		GameSessionStorage.s_Data.Remove(SESSION_KEY);
		Print("[AICF][HOST_HANDOFF] phase=READY");
	}
	void Stop() { GetGame().GetCallqueue().Remove(Resume); }
}

modded class ArmaReforgerScripted
{
	protected ref AICF_ScenarioHostTransition m_AICFHostTransition;
	override bool OnGameStart()
	{
		bool started = super.OnGameStart();
		if (started)
		{
			m_AICFHostTransition = new AICF_ScenarioHostTransition();
			m_AICFHostTransition.Start();
		}
		return started;
	}
	override void OnGameEnd()
	{
		if (m_AICFHostTransition) m_AICFHostTransition.Stop();
		m_AICFHostTransition = null;
		super.OnGameEnd();
	}
}
