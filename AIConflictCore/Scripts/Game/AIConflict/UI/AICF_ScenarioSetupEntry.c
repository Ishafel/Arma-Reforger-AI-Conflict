modded class SCR_ScenarioUICommon
{
	protected static ref AICF_ScenarioSetupScreen s_AICFSetupScreen;

	static bool AICF_IsSetupOpen() { return s_AICFSetupScreen != null; }

	static SCR_MissionHeaderCampaign AICF_GetSetupHeader(MissionWorkshopItem scenario)
	{
		if (!scenario) return null;
		// Catalog entries can expose metadata while GetHeader() is still null.
		SCR_MissionHeaderCampaign header = SCR_MissionHeaderCampaign.Cast(MissionHeader.ReadMissionHeader(scenario.Id()));
		if (!header || !header.m_bAICFSetupEntry) return null;
		return header;
	}

	static bool AICF_OpenSetup(MissionWorkshopItem scenario, SCR_DSConfig config = null)
	{
		if (!scenario) return false;
		SCR_MissionHeaderCampaign header = AICF_GetSetupHeader(scenario);
		if (!header) return false;
		AICF_CloseSetup();
		s_AICFSetupScreen = new AICF_ScenarioSetupScreen();
		if (!s_AICFSetupScreen.Open(header, config, scenario)) AICF_CloseSetup();
		// Identified setup headers never fall through to an unintended direct launch.
		return true;
	}

	static void AICF_CloseSetup()
	{
		AICF_ScenarioSetupScreen screen = s_AICFSetupScreen;
		s_AICFSetupScreen = null;
		if (screen) screen.Close();
	}

	override static void TryPlayScenario(MissionWorkshopItem scenario)
	{
		if (AICF_OpenSetup(scenario)) return;
		super.TryPlayScenario(scenario);
	}

	override static void TryHostScenario(notnull MissionWorkshopItem scenario, notnull SCR_DSConfig config)
	{
		if (AICF_OpenSetup(scenario, config)) return;
		super.TryHostScenario(scenario, config);
	}
}

modded class SCR_ScenarioMenu
{
	override void OnMenuClose()
	{
		SCR_ScenarioUICommon.AICF_CloseSetup();
		super.OnMenuClose();
	}
}

modded class ServerHostingUI
{
	override void OnMenuClose()
	{
		SCR_ScenarioUICommon.AICF_CloseSetup();
		super.OnMenuClose();
	}
}
