// Отдельный экран перед native Play/Host. Текущий матч не перенастраивает.
class AICF_ScenarioSetupScreen : ScriptedWidgetEventHandler
{
	protected ref AICF_StrategicUIController m_Style = new AICF_StrategicUIController();
	protected ref AICF_ScenarioSetupSelection m_Selection;
	protected ref SCR_DSConfig m_Config;
	protected Widget m_wRoot;
	protected Widget m_wPreviousFocus;
	protected TextWidget m_wSummary;
	protected TextWidget m_wStatus;
	protected ref array<Widget> m_aFrames = {};
	protected ref array<Widget> m_aButtons = {};
	protected bool m_bClosing;
	protected bool m_bHost;

	bool Open(AICF_ScenarioSetupHeader header, SCR_DSConfig config = null)
	{
		if (!header || m_wRoot || !GetGame().GetWorkspace()) return false;
		m_bClosing = false;
		m_Selection = new AICF_ScenarioSetupSelection(header);
		m_Config = config;
		m_bHost = config != null;
		m_wPreviousFocus = GetGame().GetWorkspace().GetFocusedWidget();
		m_wRoot = m_Style.CreateRect(GetGame().GetWorkspace(), 0, 0, 1, 1, Color.FromSRGBA(23, 34, 37, 255), true);
		if (!m_wRoot) return false;
		m_wRoot.SetName("AICF_ScenarioSetupScreen");
		m_wRoot.SetZOrder(10000);
		m_wRoot.AddHandler(this);
		m_Style.CreateText(m_wRoot, 0.08, 0.07, 0.92, 0.14, "{AICF:AICF_Setup_Title}", 36, Color.White);
		m_Style.CreateText(m_wRoot, 0.08, 0.15, 0.92, 0.20, AICF_Localization.Key(header.m_sName), 22, Color.White);
		m_Style.CreateText(m_wRoot, 0.08, 0.26, 0.25, 0.32, "{AICF:AICF_Setup_Mode}", 21, Color.White);
		AddButton("{AICF:AICF_Setup_Standard}", 0.28, 0.26, 0.57, 0.33);
		AddButton("Co-op", 0.59, 0.26, 0.88, 0.33);
		m_Style.CreateText(m_wRoot, 0.08, 0.38, 0.25, 0.44, "{AICF:AICF_Setup_Side}", 21, Color.White);
		AddButton("{AICF:AICF_Setup_US}", 0.28, 0.38, 0.57, 0.45);
		AddButton("{AICF:AICF_Setup_USSR}", 0.59, 0.38, 0.88, 0.45);
		m_Style.CreateText(m_wRoot, 0.08, 0.50, 0.27, 0.56, "{AICF:AICF_Setup_Difficulty}", 21, Color.White);
		AddButton("{AICF:AICF_Setup_Easy}", 0.28, 0.50, 0.47, 0.57);
		AddButton("{AICF:AICF_Setup_Medium}", 0.49, 0.50, 0.68, 0.57);
		AddButton("{AICF:AICF_Setup_Hard}", 0.70, 0.50, 0.88, 0.57);
		m_wSummary = m_Style.CreateText(m_wRoot, 0.08, 0.63, 0.92, 0.75, "", 21, Color.White);
		m_wSummary.SetTextWrapping(true);
		m_wStatus = m_Style.CreateText(m_wRoot, 0.08, 0.77, 0.92, 0.82, "", 18, Color.White);
		string start = "{AICF:AICF_Setup_Play}";
		if (m_bHost) start = "{AICF:AICF_Setup_Host}";
		AddButton(start, 0.64, 0.85, 0.88, 0.93);
		AddButton("{AICF:AICF_Setup_Cancel}", 0.08, 0.85, 0.30, 0.93);
		if (!m_wSummary || !m_wStatus || m_aButtons.Count() != 9) { Close(); return false; }
		Refresh();
		GetGame().GetWorkspace().AddModal(m_wRoot, m_aButtons[0]);
		return true;
	}

	protected void AddButton(string text, float left, float top, float right, float bottom)
	{
		Widget frame = m_Style.CreateRect(m_wRoot, left, top, right, bottom, Color.FromSRGBA(42, 60, 62, 255), true);
		if (!frame) return;
		frame.SetZOrder(10);
		m_Style.CreateText(frame, 0, 0, 1, 1, text, 22, Color.White, true);
		Widget input = frame.FindAnyWidget("AICF_RectInput");
		if (!input) return;
		m_aFrames.Insert(frame);
		m_aButtons.Insert(input);
	}

	AICF_ScenarioSetupSelection GetSelection() { return m_Selection; }
	Widget GetRoot() { return m_wRoot; }
	bool Select(int mode, int difficulty)
	{
		if (!m_Selection || !m_Selection.Select(mode, difficulty)) return false;
		Refresh();
		return true;
	}

	protected void Refresh()
	{
		int mode = m_Selection.GetMode();
		int difficulty = m_Selection.GetDifficulty();
		foreach (int index, Widget frame : m_aFrames)
		{
			bool selected = (index == 0 && mode == 0) || (index == 1 && mode > 0) ||
				(index == 2 && mode == 1) || (index == 3 && mode == 2) || (index == difficulty + 4);
			Color color = Color.FromSRGBA(42, 60, 62, 255);
			if (selected) color = Color.FromSRGBA(111, 85, 36, 255);
			m_Style.SetRectColor(frame, color);
		}
		m_aButtons[2].SetEnabled(mode != 0);
		m_aButtons[3].SetEnabled(mode != 0);
		string summary = "{AICF:AICF_Setup_StandardSummary}";
		if (mode == 1) summary = "{AICF:AICF_Setup_USSummary}";
		if (mode == 2) summary = "{AICF:AICF_Setup_USSRSummary}";
		m_wSummary.SetText(AICF_Localization.Resolve(summary));
		m_aButtons[7].SetEnabled(!m_Selection.GetResource().IsEmpty());
	}

	// Resolve concrete native mission before leaving the setup screen.
	bool PrepareLaunch(out MissionWorkshopItem mission)
	{
		mission = null;
		if (!m_Selection) return false;
		ResourceName resource = m_Selection.GetResource();
		if (resource.IsEmpty()) return false;
		mission = SCR_ScenarioUICommon.GetInGameScenario(resource);
		if (!mission) return false;
		if (m_bHost) return m_Config && m_Config.game && SCR_ScenarioUICommon.CanHost(mission);
		return SCR_ScenarioUICommon.CanPlay(mission);
	}

	protected void Launch()
	{
		MissionWorkshopItem mission;
		if (!PrepareLaunch(mission))
		{
			m_wStatus.SetText(AICF_Localization.Resolve("{AICF:AICF_Setup_Unavailable}"));
			return;
		}
		SCR_DSConfig config = m_Config;
		bool host = m_bHost;
		if (host) config.game.scenarioId = mission.Id();
		SCR_ScenarioUICommon.AICF_CloseSetup();
		if (host) SCR_ScenarioUICommon.TryHostScenario(mission, config);
		else SCR_ScenarioUICommon.TryPlayScenario(mission);
	}

	override bool OnClick(Widget w, int x, int y, int button)
	{
		if (button != 0 || m_bClosing) return false;
		int index = m_aButtons.Find(w);
		if (index < 0) return false;
		int mode = m_Selection.GetMode();
		int difficulty = m_Selection.GetDifficulty();
		if (index == 0) Select(0, difficulty);
		else if (index == 1 && mode == 0) Select(2, difficulty);
		else if ((index == 2 || index == 3) && mode > 0) Select(index - 1, difficulty);
		else if (index >= 4 && index <= 6) Select(mode, index - 4);
		else if (index == 7) Launch();
		else if (index == 8) SCR_ScenarioUICommon.AICF_CloseSetup();
		return true;
	}

	override bool OnModalClosed(Widget modalRoot)
	{
		if (!m_bClosing && modalRoot == m_wRoot) SCR_ScenarioUICommon.AICF_CloseSetup();
		return true;
	}

	void Close()
	{
		if (m_bClosing) return;
		m_bClosing = true;
		if (m_wRoot)
		{
			m_wRoot.RemoveHandler(this);
			GetGame().GetWorkspace().RemoveModal(m_wRoot);
			m_wRoot.RemoveFromHierarchy();
		}
		m_wRoot = null;
		m_wSummary = null;
		m_wStatus = null;
		m_aButtons.Clear();
		m_aFrames.Clear();
		if (m_wPreviousFocus) GetGame().GetWorkspace().SetFocusedWidget(m_wPreviousFocus);
		m_wPreviousFocus = null;
		m_Selection = null;
		m_Config = null;
	}
}
