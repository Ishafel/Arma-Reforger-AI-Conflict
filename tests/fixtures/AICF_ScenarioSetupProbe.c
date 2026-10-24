// Только isolated stage. Native headers и UI readbacks, без GUI input/Play/Host.
class AICF_ScenarioSetupProbe
{
	static const ResourceName HEADER = "{A1CF261019500001}Missions/AICF_Conflict_Arland_Setup.conf";
	int m_iChecks;
	int m_iFailed;
	void Check(string name, bool passed)
	{
		m_iChecks++;
		if (!passed) m_iFailed++;
		Print(string.Format("[AICF][SETUP_CASE] case=%1 pass=%2", name, passed));
	}
	void Headers()
	{
		SCR_MissionHeaderCampaign header = SCR_MissionHeaderCampaign.Cast(MissionHeader.ReadMissionHeader(HEADER));
		Check("HEADER", header && header.m_bAICFSetupEntry && header.m_bShowInScenarioMenu);
		if (!header) return;
		AICF_ScenarioSetupSelection selection = new AICF_ScenarioSetupSelection(header);
		Check("DEFAULT", selection.GetMode() == 2 && selection.GetDifficulty() == 1 && !selection.GetResource().IsEmpty());
		for (int mode; mode < 3; mode++)
		{
			for (int difficulty; difficulty < 3; difficulty++)
			{
				bool selected = selection.Select(mode, difficulty);
				SCR_MissionHeaderCampaign preset = SCR_MissionHeaderCampaign.Cast(MissionHeader.ReadMissionHeader(selection.GetResource()));
				Check(string.Format("PRESET_%1_%2", mode, difficulty), selected && preset && !preset.m_bAICFSetupEntry && selection.GetMode() == mode && selection.GetDifficulty() == difficulty && preset.GetWorldResourceName() == header.GetWorldResourceName());
			}
		}
		SCR_MissionHeaderCampaign active = SCR_MissionHeaderCampaign.Cast(GetGame().GetMissionHeader());
		Check("ACTIVE_PRESET", active && AICF_RuntimeScenario.IsActive() && AICF_RuntimeScenario.GetSelection().GetTarget() == active.GetHeaderResourceName());
		Check("INVALID_BOUNDS", !selection.Select(-1, 0) && !selection.Select(3, 0) && !selection.Select(0, -1) && !selection.Select(0, 3));
		ResourceName before = selection.GetResource();
		Check("FAILURE_PRESERVES_SELECTION", !selection.Select(3, 2) && selection.GetResource() == before);
		AICF_ScenarioSetupSelection empty = new AICF_ScenarioSetupSelection(null);
		Check("NULL_HEADER", empty.GetResource().IsEmpty() && !empty.Select(0, 0));
		AICF_ScenarioRegistry registry = new AICF_ScenarioRegistry();
		Check("INVALID_MAP", !registry.Find(-1, 0));
		Check("INVALID_CONTENT", !registry.Find(0, 3));
		Check("NO_RECURSION", selection.GetResource() != HEADER);

	}
	void Done() { Print(string.Format("[AICF][SETUP_DONE] checks=%1 failed=%2", m_iChecks, m_iFailed)); }
}

modded class AICF_MatchController
{
	protected bool m_bAICFSetupHeadersDone;
	override protected void Update()
	{
		super.Update();
		string enabled;
		if (m_bAICFSetupHeadersDone || !m_bRosterReady || m_bStopped || !System.GetCLIParam("aicfSetupProbe", enabled)) return;
		m_bAICFSetupHeadersDone = true;
		AICF_ScenarioSetupProbe probe = new AICF_ScenarioSetupProbe();
		probe.Headers();
		probe.Done();
	}
}

modded class AICF_ScenarioSetupScreen
{
	bool AICF_ProbeSideEnabled() { return m_aButtons[8].IsEnabled() && m_aButtons[9].IsEnabled(); }
	string AICF_ProbeSummary() { return m_wSummary.GetText(); }
}

modded class SCR_GameModeCampaign
{
	protected ref AICF_ScenarioSetupScreen m_AICFSetupProbeScreen;
	protected ref AICF_ScenarioSetupProbe m_AICFSetupUIProbe;
	protected Widget m_wAICFSetupPreviousModal;
	protected bool m_bAICFSetupUIDone;

	override protected void AICF_CoopClientReadback()
	{
		string enabled;
		if (!m_bAICFSetupUIDone && System.GetCLIParam("aicfSetupProbe", enabled) && m_iCoopClientSamples >= 2)
		{
			if (!m_AICFSetupUIProbe)
			{
				m_AICFSetupUIProbe = new AICF_ScenarioSetupProbe();
				m_wAICFSetupPreviousModal = GetGame().GetWorkspace().GetModal();
				SCR_MissionHeaderCampaign header = SCR_MissionHeaderCampaign.Cast(MissionHeader.ReadMissionHeader(AICF_ScenarioSetupProbe.HEADER));
				m_AICFSetupProbeScreen = new AICF_ScenarioSetupScreen();
				m_AICFSetupUIProbe.Check("UI_OPEN", m_AICFSetupProbeScreen.Open(header));
			}
			else
			{
				Widget root = m_AICFSetupProbeScreen.GetRoot();
				float width, height;
				if (root) root.GetScreenSize(width, height);
				m_AICFSetupUIProbe.Check("UI_BOUNDS", root && width > 600 && height > 400);
				m_AICFSetupUIProbe.Check("UI_MODAL", root && GetGame().GetWorkspace().GetModal() == root);
				for (int mode; mode < 3; mode++)
				{
					for (int difficulty; difficulty < 3; difficulty++)
					{
						bool selected = m_AICFSetupProbeScreen.Select(mode, difficulty);
						m_AICFSetupUIProbe.Check(string.Format("UI_%1_%2", mode, difficulty), selected && m_AICFSetupProbeScreen.GetSelection().GetMode() == mode && m_AICFSetupProbeScreen.GetSelection().GetDifficulty() == difficulty && m_AICFSetupProbeScreen.AICF_ProbeSideEnabled() == (mode > 0) && !m_AICFSetupProbeScreen.AICF_ProbeSummary().Contains("{AICF:") && !m_AICFSetupProbeScreen.AICF_ProbeSummary().IsEmpty());
					}
				}
				ResourceName selectedResource = m_AICFSetupProbeScreen.GetSelection().GetResource();
				m_AICFSetupUIProbe.Check("UI_INVALID", !m_AICFSetupProbeScreen.Select(3, 0) && selectedResource == m_AICFSetupProbeScreen.GetSelection().GetResource());
				m_AICFSetupProbeScreen.Close();
				m_AICFSetupUIProbe.Check("UI_CLOSED", !m_AICFSetupProbeScreen.GetRoot() && GetGame().GetWorkspace().GetModal() == m_wAICFSetupPreviousModal);
				m_AICFSetupProbeScreen.Close();
				m_AICFSetupUIProbe.Check("UI_CLOSE_IDEMPOTENT", !m_AICFSetupProbeScreen.GetRoot());
				m_AICFSetupUIProbe.Done();
				m_AICFSetupProbeScreen = null;
				m_bAICFSetupUIDone = true;
			}
		}
		super.AICF_CoopClientReadback();
	}

	override void OnGameEnd()
	{
		if (m_AICFSetupProbeScreen) m_AICFSetupProbeScreen.Close();
		m_AICFSetupProbeScreen = null;
		m_AICFSetupUIProbe = null;
		super.OnGameEnd();
	}
}
