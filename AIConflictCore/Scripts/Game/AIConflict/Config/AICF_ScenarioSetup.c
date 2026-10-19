// Порядок presets: STANDARD, COOP_US, COOP_USSR; внутри каждого Easy/Medium/Hard.
// Карта и concrete ресурсы принадлежат integration header, а не Core.
class AICF_ScenarioSetupHeader : SCR_MissionHeaderCampaign
{
	[Attribute("", UIWidgets.ResourcePickerThumbnail, "Launch presets ordered by match mode and difficulty", "conf")]
	ref array<ResourceName> m_aAICFSetupPresets;

	ResourceName ResolvePreset(int mode, int difficulty)
	{
		if (mode < 0 || mode > 2 || difficulty < 0 || difficulty > 2) return "";
		if (!m_aAICFSetupPresets || m_aAICFSetupPresets.Count() != 9) return "";
		ResourceName resource = m_aAICFSetupPresets[mode * 3 + difficulty];
		SCR_MissionHeaderCampaign target = SCR_MissionHeaderCampaign.Cast(MissionHeader.ReadMissionHeader(resource));
		if (!target || AICF_ScenarioSetupHeader.Cast(target)) return "";
		if (target.m_eAICFMatchMode != mode || target.m_eAICFDifficulty != difficulty) return "";
		if (target.GetWorldResourceName() != GetWorldResourceName()) return "";
		return resource;
	}
}

class AICF_ScenarioSetupSelection
{
	protected ref AICF_ScenarioSetupHeader m_Header;
	protected int m_iMode = 2;
	protected int m_iDifficulty = 1;

	void AICF_ScenarioSetupSelection(AICF_ScenarioSetupHeader header) { m_Header = header; }
	int GetMode() { return m_iMode; }
	int GetDifficulty() { return m_iDifficulty; }
	ResourceName GetResource()
	{
		if (!m_Header) return "";
		return m_Header.ResolvePreset(m_iMode, m_iDifficulty);
	}
	bool Select(int mode, int difficulty)
	{
		if (!m_Header || m_Header.ResolvePreset(mode, difficulty).IsEmpty()) return false;
		m_iMode = mode;
		m_iDifficulty = difficulty;
		return true;
	}
}
