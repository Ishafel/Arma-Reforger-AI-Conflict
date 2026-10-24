// Порядок presets: STANDARD, COOP_US, COOP_USSR; внутри каждого Easy/Medium/Hard.
// Карта и concrete ресурсы принадлежат integration header, а не Core.
modded class SCR_MissionHeaderCampaign
{
	[Attribute("", UIWidgets.ResourcePickerThumbnail, "Launch presets ordered by match mode and difficulty", "conf")]
	ref array<ResourceName> m_aAICFSetupPresets;

	[Attribute("0", desc: "Universal AI Conflict setup entry")]
	bool m_bAICFSetupEntry;

	ResourceName AICF_ResolveSetupPreset(int mode, int difficulty)
	{
		if (!m_bAICFSetupEntry) return "";
		if (mode < 0 || mode > 2 || difficulty < 0 || difficulty > 2) return "";
		if (!m_aAICFSetupPresets || m_aAICFSetupPresets.Count() != 9) return "";
		ResourceName resource = m_aAICFSetupPresets[mode * 3 + difficulty];
		SCR_MissionHeaderCampaign target = SCR_MissionHeaderCampaign.Cast(MissionHeader.ReadMissionHeader(resource));
		if (!target || target.m_bAICFSetupEntry) return "";
		if (target.m_eAICFMatchMode != mode || target.m_eAICFDifficulty != difficulty) return "";
		if (target.GetWorldResourceName() != GetWorldResourceName()) return "";
		return resource;
	}
}

class AICF_ScenarioSetupSelection
{
	protected ref SCR_MissionHeaderCampaign m_Header;
	protected int m_iMode = 2;
	protected int m_iDifficulty = 1;
	protected ref AICF_ScenarioRegistry m_Registry = new AICF_ScenarioRegistry();
	protected int m_iMap;
	protected int m_iContent;
	protected ref AICF_ScenarioEnvironment m_Environment = new AICF_ScenarioEnvironment();

	void AICF_ScenarioSetupSelection(SCR_MissionHeaderCampaign header) { m_Header = header; }
	int GetMode() { return m_iMode; }
	int GetDifficulty() { return m_iDifficulty; }
	int GetMap() { return m_iMap; }
	int GetContent() { return m_iContent; }
	AICF_ScenarioEnvironment GetEnvironment() { return m_Environment; }
	bool SelectEnvironment(int minutes, int weather)
	{
		if (minutes < 0 || minutes > AICF_ScenarioEnvironment.RANDOM_TIME || weather < 0 || weather > 4) return false;
		m_Environment.m_iStartMinutes = minutes;
		m_Environment.m_iWeather = weather;
		return true;
	}
	AICF_ScenarioRegistry GetRegistry() { return m_Registry; }
	AICF_ScenarioVariant GetVariant() { return m_Registry.Find(m_iMap, m_iContent); }
	ResourceName GetResource()
	{
		if (!m_Header || !m_Header.m_bAICFSetupEntry) return "";
		AICF_ScenarioVariant variant = GetVariant();
		if (!variant) return "";
		return variant.Resolve(m_iMode, m_iDifficulty);
	}
	bool Select(int mode, int difficulty)
	{
		if (!m_Header || !m_Header.m_bAICFSetupEntry || !GetVariant() || GetVariant().Resolve(mode, difficulty).IsEmpty()) return false;
		m_iMode = mode;
		m_iDifficulty = difficulty;
		return true;
	}
	bool SelectVariant(int mapIndex, int content)
	{
		AICF_ScenarioVariant variant = m_Registry.Find(mapIndex, content);
		string missing;
		if (!m_Header || !m_Header.m_bAICFSetupEntry || !variant || !variant.IsContentEnabled() || variant.Resolve(m_iMode, m_iDifficulty).IsEmpty() || !variant.IsAvailable(missing)) return false;
		m_iMap = mapIndex;
		m_iContent = content;
		return true;
	}
}
