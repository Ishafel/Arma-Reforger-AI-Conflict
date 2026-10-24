// One launch selection replaces resource-per-mode/difficulty missions.
class AICF_RuntimeScenarioSelection
{
	int m_iMap;
	int m_iContent;
	int m_iMode;
	int m_iDifficulty;
	int m_iIssued;
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
	bool IsValid()
	{
		return m_Environment && m_Environment.IsValid() && !GetTarget().IsEmpty();
	}
	string Encode()
	{
		JsonSaveContext json = new JsonSaveContext();
		// Preserve the complete identity even before the target addon is loaded.
		string target = GetTarget();
		json.WriteValue("schema", 1);
		json.WriteValue("issued", m_iIssued);
		json.WriteValue("target", target);
		json.WriteValue("map", m_iMap);
		json.WriteValue("content", m_iContent);
		json.WriteValue("mode", m_iMode);
		json.WriteValue("difficulty", m_iDifficulty);
		json.WriteValue("minutes", m_Environment.m_iStartMinutes);
		json.WriteValue("weather", m_Environment.m_iWeather);
		return json.SaveToString();
	}
	static AICF_RuntimeScenarioSelection Decode(string data)
	{
		JsonLoadContext json = new JsonLoadContext();
		AICF_RuntimeScenarioSelection selection = new AICF_RuntimeScenarioSelection();
		int schema;
		string target;
		if (!json.LoadFromString(data) || !json.ReadValue("schema", schema) || schema != 1 ||
			!json.ReadValue("issued", selection.m_iIssued) || !json.ReadValue("target", target) ||
			!json.ReadValue("map", selection.m_iMap) || !json.ReadValue("content", selection.m_iContent) ||
			!json.ReadValue("mode", selection.m_iMode) || !json.ReadValue("difficulty", selection.m_iDifficulty) ||
			!json.ReadValue("minutes", selection.m_Environment.m_iStartMinutes) ||
			!json.ReadValue("weather", selection.m_Environment.m_iWeather) ||
			!selection.IsValid() || selection.GetTarget() != target) return null;
		int age = System.GetTickCount(selection.m_iIssued);
		if (age < 0 || age > 600000) return null;
		return selection;
	}
}

// Native mission objects can be cached. Restore every field changed for this match.
class AICF_RuntimeHeaderSnapshot
{
	ref SCR_MissionHeaderCampaign m_Header;
	string m_sName, m_sGameMode;
	ESaveGameType m_eSaveTypes;
	SCR_ECharacterRank m_eRank;
	bool m_bIgnoreRank, m_bOverride, m_bRandomDay, m_bRandomWeather, m_bWeatherChanges;
	bool m_bWhitelist, m_bEstablish;
	int m_iHours, m_iMinutes;
	float m_fDay, m_fNight;
	AICF_EMatchMode m_eMode;
	AICF_EDifficulty m_eDifficulty;
	ref array<ref SCR_CampaignCustomBase> m_Bases;

	void AICF_RuntimeHeaderSnapshot(SCR_MissionHeaderCampaign header)
	{
		m_Header = header;
		m_sName = header.m_sName;
		m_sGameMode = header.m_sGameMode;
		m_eSaveTypes = header.m_eSaveTypes;
		m_eRank = header.m_eStartingRank;
		m_bIgnoreRank = header.m_bIgnoreMinimumVehicleRank;
		m_bOverride = header.m_bOverrideScenarioTimeAndWeather;
		m_bRandomDay = header.m_bRandomStartingDaytime;
		m_bRandomWeather = header.m_bRandomStartingWeather;
		m_bWeatherChanges = header.m_bRandomWeatherChanges;
		m_iHours = header.m_iStartingHours;
		m_iMinutes = header.m_iStartingMinutes;
		m_fDay = header.m_fDayTimeAcceleration;
		m_fNight = header.m_fNightTimeAcceleration;
		m_bWhitelist = header.m_bCustomBaseWhitelist;
		m_bEstablish = header.m_bEstablishingBasesEnabled;
		m_Bases = header.m_aCampaignCustomBaseList;
		m_eMode = header.m_eAICFMatchMode;
		m_eDifficulty = header.m_eAICFDifficulty;
	}
	void Restore()
	{
		if (!m_Header) return;
		m_Header.m_sName = m_sName;
		m_Header.m_sGameMode = m_sGameMode;
		m_Header.m_eSaveTypes = m_eSaveTypes;
		m_Header.m_eStartingRank = m_eRank;
		m_Header.m_bIgnoreMinimumVehicleRank = m_bIgnoreRank;
		m_Header.m_bOverrideScenarioTimeAndWeather = m_bOverride;
		m_Header.m_bRandomStartingDaytime = m_bRandomDay;
		m_Header.m_bRandomStartingWeather = m_bRandomWeather;
		m_Header.m_bRandomWeatherChanges = m_bWeatherChanges;
		m_Header.m_iStartingHours = m_iHours;
		m_Header.m_iStartingMinutes = m_iMinutes;
		m_Header.m_fDayTimeAcceleration = m_fDay;
		m_Header.m_fNightTimeAcceleration = m_fNight;
		m_Header.m_bCustomBaseWhitelist = m_bWhitelist;
		m_Header.m_bEstablishingBasesEnabled = m_bEstablish;
		m_Header.m_aCampaignCustomBaseList = m_Bases;
		m_Header.m_eAICFMatchMode = m_eMode;
		m_Header.m_eAICFDifficulty = m_eDifficulty;
		m_Header.m_AICFEnvironment = null;
		m_Header = null;
	}
}

class AICF_RuntimeScenario
{
	static const string SESSION_KEY = "AICF_RuntimeScenario";
	protected static ref AICF_RuntimeScenarioSelection s_Selection;
	protected static ref AICF_RuntimeHeaderSnapshot s_Snapshot;

	static AICF_RuntimeScenarioSelection GetSelection() { return s_Selection; }
	static bool IsActive() { return s_Selection != null; }
	static void ClearPending() { GameSessionStorage.s_Data.Remove(SESSION_KEY); }
	static bool Queue(AICF_ScenarioSetupSelection selection)
	{
		ClearPending();
		if (!selection) return false;
		return QueueValues(selection.GetMap(), selection.GetContent(), selection.GetMode(), selection.GetDifficulty(), selection.GetEnvironment());
	}
	static bool QueueValues(int mapIndex, int content, int mode, int difficulty, AICF_ScenarioEnvironment environment)
	{
		ClearPending();
		if (!environment) return false;
		AICF_RuntimeScenarioSelection value = new AICF_RuntimeScenarioSelection();
		value.m_iMap = mapIndex;
		value.m_iContent = content;
		value.m_iMode = mode;
		value.m_iDifficulty = difficulty;
		value.m_iIssued = System.GetTickCount();
		value.m_Environment.m_iStartMinutes = environment.m_iStartMinutes;
		value.m_Environment.m_iWeather = environment.m_iWeather;
		if (!value.IsValid()) return false;
		string data = value.Encode();
		if (!AICF_RuntimeScenarioSelection.Decode(data)) return false;
		GameSessionStorage.s_Data[SESSION_KEY] = data;
		return true;
	}
	protected static AICF_RuntimeScenarioSelection ReadCLI()
	{
		if (!System.IsCLIParam("server")) return null;
		string key, mode, difficulty;
		if (!System.GetCLIParam("aicfScenarioVariant", key)) return null;
		if (!System.GetCLIParam("aicfMatchMode", mode) || (mode != "0" && mode != "1" && mode != "2") ||
			!System.GetCLIParam("aicfDifficulty", difficulty) || (difficulty != "0" && difficulty != "1" && difficulty != "2")) return null;
		AICF_ScenarioRegistry registry = new AICF_ScenarioRegistry();
		AICF_ScenarioVariant variant = registry.FindKey(key);
		if (!variant) return null;
		AICF_RuntimeScenarioSelection value = new AICF_RuntimeScenarioSelection();
		value.m_iMap = variant.m_iMap;
		value.m_iContent = variant.m_iContent;
		value.m_iMode = mode.ToInt();
		value.m_iDifficulty = difficulty.ToInt();
		value.m_iIssued = System.GetTickCount();
		if (!value.IsValid()) return null;
		return value;
	}
	protected static void ApplyPresentation(SCR_MissionHeaderCampaign header, AICF_RuntimeScenarioSelection value)
	{
		if (!s_Snapshot) s_Snapshot = new AICF_RuntimeHeaderSnapshot(header);
		header.m_sName = value.GetVariant().m_sMapLabel;
		header.m_sGameMode = "AI Conflict";
		header.m_eAICFMatchMode = value.m_iMode;
		header.m_eAICFDifficulty = value.m_iDifficulty;
		header.m_eSaveTypes = 0;
		header.m_eStartingRank = SCR_ECharacterRank.GENERAL;
		header.m_bIgnoreMinimumVehicleRank = true;
	}
	static void BeforeStart()
	{
		if (!GetGame().InPlayMode()) return;
		if (s_Selection) return;
		if (System.IsCLIParam("client") || RplSession.Mode() == RplMode.Client)
		{
			ApplyReplica();
			return;
		}
		SCR_MissionHeaderCampaign header = SCR_MissionHeaderCampaign.Cast(GetGame().GetMissionHeader());
		if (!header || header.m_bAICFSetupEntry) return;
		string data;
		AICF_RuntimeScenarioSelection value;
		if (GameSessionStorage.s_Data.Find(SESSION_KEY, data))
		{
			ClearPending();
			value = AICF_RuntimeScenarioSelection.Decode(data);
		}
		else value = ReadCLI();
		if (!value) return;
		if (value.GetTarget() != header.GetHeaderResourceName() || !AICF_ScenarioHostTransition.HasExactGraph(value.GetVariant()))
		{
			Print("[AICF][SCENARIO_RUNTIME][FAIL] reason=TARGET_OR_GRAPH", LogLevel.ERROR);
			return;
		}
		ApplyPresentation(header, value);
		AICF_ScenarioRegistry registry = new AICF_ScenarioRegistry();
		if (!registry.ConfigureRuntimeMap(header, value.GetVariant()))
		{
			End();
			Print("[AICF][SCENARIO_RUNTIME][FAIL] reason=MAP_PROFILE", LogLevel.ERROR);
			return;
		}
		header.m_bOverrideScenarioTimeAndWeather = true;
		header.m_bRandomStartingWeather = true;
		header.m_bRandomWeatherChanges = true;
		header.m_bRandomStartingDaytime = false;
		header.m_iStartingHours = value.m_Environment.GetHour();
		header.m_iStartingMinutes = value.m_Environment.GetMinute();
		header.m_fDayTimeAcceleration = 6;
		header.m_fNightTimeAcceleration = 24;
		header.m_AICFEnvironment = value.m_Environment;
		s_Selection = value;
		SCR_GameModeCampaign campaign = SCR_GameModeCampaign.Cast(GetGame().GetGameMode());
		if (campaign) campaign.AICF_ApplyRuntimeNativeOptions(header);
		SCR_TimeAndWeatherHandlerComponent timeHandler = SCR_TimeAndWeatherHandlerComponent.GetInstance();
		if (timeHandler) timeHandler.AICF_ApplyRuntimeOptions(header);
		Print(string.Format("[AICF][SCENARIO_RUNTIME] variant=%1 mode=%2 difficulty=%3 target=%4", value.GetVariant().m_sKey, value.m_iMode, value.m_iDifficulty, value.GetTarget()));
	}
	static void ApplyReplica()
	{
		SCR_GameModeCampaign campaign = SCR_GameModeCampaign.Cast(GetGame().GetGameMode());
		if (!campaign || !campaign.AICF_HasRuntimeScenario()) return;
		AICF_ScenarioRegistry registry = new AICF_ScenarioRegistry();
		AICF_ScenarioVariant variant = registry.FindKey(campaign.AICF_GetRuntimeVariant());
		SCR_MissionHeaderCampaign header = SCR_MissionHeaderCampaign.Cast(GetGame().GetMissionHeader());
		if (!variant || !header) return;
		AICF_RuntimeScenarioSelection value = new AICF_RuntimeScenarioSelection();
		value.m_iMap = variant.m_iMap;
		value.m_iContent = variant.m_iContent;
		value.m_iMode = campaign.AICF_GetRuntimeMode();
		value.m_iDifficulty = campaign.AICF_GetRuntimeDifficulty();
		if (!value.IsValid() || value.GetTarget() != header.GetHeaderResourceName()) return;
		ApplyPresentation(header, value);
		campaign.AICF_ApplyRuntimeReplicaOptions();
		s_Selection = value;
	}
	static void End()
	{
		if (s_Snapshot) s_Snapshot.Restore();
		s_Snapshot = null;
		s_Selection = null;
	}
}

modded class ArmaReforgerScripted
{
	override protected void OnWorldPostProcess(World world)
	{
		AICF_RuntimeScenario.BeforeStart();
		super.OnWorldPostProcess(world);
	}
	override bool OnGameStart()
	{
		AICF_RuntimeScenario.BeforeStart();
		return super.OnGameStart();
	}
	override void OnGameEnd()
	{
		super.OnGameEnd();
		AICF_RuntimeScenario.End();
	}
}
