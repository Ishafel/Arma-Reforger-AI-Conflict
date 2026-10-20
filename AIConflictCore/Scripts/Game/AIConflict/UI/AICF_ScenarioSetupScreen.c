// Локальная настройка перед загрузкой операции. Текущий матч не изменяет.
class AICF_ScenarioSetupScreen : ScriptedWidgetEventHandler
{
	protected ref AICF_StrategicUIController m_Style = new AICF_StrategicUIController();
	protected ref AICF_ScenarioSetupSelection m_Selection;
	protected ref SCR_DSConfig m_Config;
	protected MissionWorkshopItem m_Entry;
	protected Widget m_wRoot;
	protected Widget m_wPreviousFocus;
	protected TextWidget m_wSummary;
	protected TextWidget m_wStatus;
	protected TextWidget m_wHelp;
	protected ref array<Widget> m_aFrames = {};
	protected ref array<Widget> m_aButtons = {};
	protected ref array<TextWidget> m_aLabels = {};
	protected ref array<Widget> m_aFlags = {};
	protected bool m_bClosing;
	protected bool m_bHost;
	protected int m_iFlagFailures;

	bool Open(AICF_ScenarioSetupHeader header, SCR_DSConfig config = null, MissionWorkshopItem entry = null)
	{
		if (!header || m_wRoot || !GetGame().GetWorkspace()) return false;
		m_bClosing = false;
		m_Selection = new AICF_ScenarioSetupSelection(header);
		// A source client may have a content graph pinned by its startup arguments.
		if (!AICF_ScenarioLaunch.IsForcedGraphCompatible(m_Selection.GetVariant()))
		{
			foreach (AICF_ScenarioVariant initial : m_Selection.GetRegistry().GetVariants())
				if (AICF_ScenarioLaunch.IsForcedGraphCompatible(initial) && m_Selection.SelectVariant(initial.m_iMap, initial.m_iContent)) break;
		}
		m_Config = config;
		m_Entry = entry;
		m_bHost = config != null;
		m_wPreviousFocus = GetGame().GetWorkspace().GetFocusedWidget();
		m_wRoot = m_Style.CreateRect(GetGame().GetWorkspace(), 0, 0, 1, 1, Color.FromSRGBA(23, 34, 37, 255), true);
		if (!m_wRoot) return false;
		m_wRoot.SetName("AICF_ScenarioSetupScreen");
		m_wRoot.SetZOrder(10000);
		m_wRoot.AddHandler(this);
		m_Style.CreateText(m_wRoot, 0.05, 0.04, 0.95, 0.10, "AI Conflict", 36, Color.White);
		m_Style.CreateText(m_wRoot, 0.05, 0.11, 0.95, 0.16, "{AICF:AICF_Setup_Title}", 23, Color.White);
		string headings[5] = {"Map", "Content", "Mode", "Side", "Difficulty"};
		for (int row; row < 5; row++)
			m_Style.CreateText(m_wRoot, 0.05, 0.21 + row * 0.115, 0.22, 0.26 + row * 0.115, "{AICF:AICF_Setup_" + headings[row] + "}", 18, Color.White);
		for (int mapIndex; mapIndex < 3; mapIndex++)
		{
			AICF_ScenarioVariant variant = m_Selection.GetRegistry().Find(mapIndex, 0);
			if (!variant) { Close(); return false; }
			AddButton(variant.m_sMapLabel, 0.23 + mapIndex * 0.145, 0.20, 0.365 + mapIndex * 0.145, 0.275);
		}
		AddButton("Vanilla", 0.23, 0.315, 0.365, 0.39);
		AddButton("RHS", 0.375, 0.315, 0.51, 0.39);
		AddButton("WCS + RHS", 0.52, 0.315, 0.655, 0.39);
		AddButton("Conflict", 0.23, 0.43, 0.435, 0.505);
		AddButton("Co-op", 0.45, 0.43, 0.655, 0.505);
		AddButton("", 0.23, 0.545, 0.435, 0.62);
		AddButton("", 0.45, 0.545, 0.655, 0.62);
		AddButton("{AICF:AICF_Setup_Easy}", 0.23, 0.66, 0.365, 0.735);
		AddButton("{AICF:AICF_Setup_Medium}", 0.375, 0.66, 0.51, 0.735);
		AddButton("{AICF:AICF_Setup_Hard}", 0.52, 0.66, 0.655, 0.735);
		m_Style.CreateRect(m_wRoot, 0.69, 0.20, 0.95, 0.735, Color.FromSRGBA(30, 46, 49, 255), false);
		m_wHelp = m_Style.CreateText(m_wRoot, 0.71, 0.22, 0.93, 0.71, "", 19, Color.White);
		m_wSummary = m_Style.CreateText(m_wRoot, 0.05, 0.76, 0.95, 0.815, "", 20, Color.White);
		m_wStatus = m_Style.CreateText(m_wRoot, 0.05, 0.825, 0.95, 0.88, "", 17, Color.White);
		string start = "{AICF:AICF_Setup_Play}";
		if (m_bHost) start = "{AICF:AICF_Setup_Host}";
		AddButton(start, 0.70, 0.90, 0.95, 0.97);
		AddButton("{AICF:AICF_Setup_Cancel}", 0.05, 0.90, 0.25, 0.97);
		if (!m_wSummary || !m_wStatus || !m_wHelp || m_aButtons.Count() != 15) { Close(); return false; }
		m_wSummary.SetTextWrapping(true);
		m_wStatus.SetTextWrapping(true);
		m_wHelp.SetTextWrapping(true);
		Refresh();
		Help(13);
		GetGame().GetWorkspace().AddModal(m_wRoot, m_aButtons[0]);
		return true;
	}

	protected void AddButton(string text, float left, float top, float right, float bottom)
	{
		Widget frame = m_Style.CreateRect(m_wRoot, left, top, right, bottom, Color.FromSRGBA(42, 60, 62, 255), true);
		if (!frame) return;
		frame.SetZOrder(10);
		TextWidget label = m_Style.CreateText(frame, 0, 0, 1, 1, text, 20, Color.White, true);
		Widget input = frame.FindAnyWidget("AICF_RectInput");
		if (!input || !label) return;
		m_aFrames.Insert(frame);
		m_aButtons.Insert(input);
		m_aLabels.Insert(label);
	}

	protected void Flag(int button, int kind)
	{
		Widget flag = m_Style.CreateRect(m_aFrames[button], 0.04, 0.23, 0.24, 0.77, Color.FromSRGBA(10, 40, 90, 255), false);
		if (!flag) { m_iFlagFailures++; return; }
		flag.SetZOrder(3);
		m_aFlags.Insert(flag);
		FrameSlot.SetAnchorMin(m_aLabels[button], 0.26, 0);
		if (kind == 2)
		{
			m_Style.CreateRect(flag, 0, 0, 1, 0.333, Color.White, false);
			m_Style.CreateRect(flag, 0, 0.333, 1, 0.667, Color.FromSRGBA(0, 57, 166, 255), false);
			m_Style.CreateRect(flag, 0, 0.667, 1, 1, Color.FromSRGBA(213, 43, 30, 255), false);
			return;
		}
		if (kind == 3)
		{
			// Стилизованный компас НАТО без внешней texture dependency.
			m_Style.CreateRect(flag, 0.48, 0.10, 0.52, 0.90, Color.White, false);
			m_Style.CreateRect(flag, 0.12, 0.48, 0.88, 0.52, Color.White, false);
			m_Style.CreateText(flag, 0.15, 0, 0.85, 1, "+", 28, Color.White, true);
			return;
		}
		ImageWidget image = ImageWidget.Cast(flag.FindAnyWidget("AICF_RectBackground"));
		ResourceName texture = "{A73947B37A287314}UI/Textures/Flags/flag_us_ca.edds";
		if (kind == 1) texture = "{53395E04A4D442C0}UI/Textures/Flags/flag_cccp_ca.edds";
		if (image && image.LoadImageTexture(0, texture)) image.SetColor(Color.White);
		else m_iFlagFailures++;
	}

	AICF_ScenarioSetupSelection GetSelection() { return m_Selection; }
	Widget GetRoot() { return m_wRoot; }
	bool Select(int mode, int difficulty)
	{
		if (!m_Selection || !m_Selection.Select(mode, difficulty)) return false;
		Refresh();
		return true;
	}
	bool SelectVariant(int mapIndex, int content)
	{
		if (!m_Selection || !AICF_ScenarioLaunch.IsForcedGraphCompatible(m_Selection.GetRegistry().Find(mapIndex, content))) return false;
		if (!m_Selection || !m_Selection.SelectVariant(mapIndex, content)) return false;
		Refresh();
		return true;
	}

	protected void Refresh()
	{
		int mode = m_Selection.GetMode();
		int difficulty = m_Selection.GetDifficulty();
		int mapIndex = m_Selection.GetMap();
		int content = m_Selection.GetContent();
		AICF_ScenarioVariant variant = m_Selection.GetVariant();
		foreach (int index, Widget frame : m_aFrames)
		{
			bool selected = index == mapIndex || index == content + 3 || (index == 6 && mode == 0) || (index == 7 && mode > 0) || (index == mode + 7 && mode > 0) || index == difficulty + 10;
			Color color = Color.FromSRGBA(42, 60, 62, 255);
			if (selected) color = Color.FromSRGBA(111, 85, 36, 255);
			m_Style.SetRectColor(frame, color);
		}
		string missing;
		for (int mapButton; mapButton < 3; mapButton++)
		{
			AICF_ScenarioVariant mapVariant = m_Selection.GetRegistry().Find(mapButton, 0);
			bool mapAvailable;
			for (int candidate; candidate < 3; candidate++)
			{
				mapVariant = m_Selection.GetRegistry().Find(mapButton, candidate);
				if (mapVariant && mapVariant.IsAvailable(missing) && AICF_ScenarioLaunch.IsForcedGraphCompatible(mapVariant)) mapAvailable = true;
			}
			m_aButtons[mapButton].SetEnabled(mapAvailable);
		}
		string unavailable;
		for (int contentButton; contentButton < 3; contentButton++)
		{
			AICF_ScenarioVariant contentVariant = m_Selection.GetRegistry().Find(mapIndex, contentButton);
			bool available = contentVariant && contentVariant.IsAvailable(missing) && AICF_ScenarioLaunch.IsForcedGraphCompatible(contentVariant);
			m_aButtons[contentButton + 3].SetEnabled(available);
			m_aFrames[contentButton + 3].SetOpacity(1);
			if (!available)
			{
				m_aFrames[contentButton + 3].SetOpacity(0.45);
				if (!unavailable.IsEmpty()) unavailable += " · ";
				unavailable += m_aLabels[contentButton + 3].GetText();
			}
		}
		m_aButtons[8].SetEnabled(mode != 0);
		m_aButtons[9].SetEnabled(mode != 0);
		m_aLabels[8].SetText(AICF_Localization.Resolve(variant.m_sWestLabel));
		m_aLabels[9].SetText(AICF_Localization.Resolve(variant.m_sEastLabel));
		foreach (Widget oldFlag : m_aFlags) oldFlag.RemoveFromHierarchy();
		m_aFlags.Clear();
		m_iFlagFailures = 0;
		int westFlag;
		int eastFlag = 1;
		if (content > 0) eastFlag = 2;
		if (content == 2) westFlag = 3;
		Flag(8, westFlag);
		Flag(9, eastFlag);
		string side = AICF_Localization.Resolve("{AICF:AICF_Setup_BothSides}");
		if (mode == 1) side = AICF_Localization.Resolve(variant.m_sWestLabel);
		if (mode == 2) side = AICF_Localization.Resolve(variant.m_sEastLabel);
		string modeLabel = "Conflict";
		if (mode > 0) modeLabel = "Co-op";
		m_wSummary.SetText(string.Format("%1 · %2 · %3 · %4 · %5", AICF_Localization.Resolve(variant.m_sMapLabel), m_aLabels[content + 3].GetText(), modeLabel, side, m_aLabels[difficulty + 10].GetText()));
		string status = AICF_Localization.Resolve("{AICF:AICF_Setup_Reload}");
		if (!unavailable.IsEmpty()) status = AICF_Localization.Resolve("{AICF:AICF_Setup_Missing}") + " " + unavailable;
		if (!AICF_ScenarioLaunch.IsForcedGraphCompatible(m_Selection.GetRegistry().Find(mapIndex, 0))) status = AICF_Localization.Resolve("{AICF:AICF_Setup_Pinned}");
		m_wStatus.SetText(status);
		m_aButtons[13].SetEnabled(variant.IsAvailable(missing) && AICF_ScenarioLaunch.IsForcedGraphCompatible(variant) && !m_Selection.GetResource().IsEmpty());
	}

	protected void Help(int index)
	{
		string key;
		if (index < 3) key = "MapHelp";
		else if (index < 6) key = "ContentHelp";
		else if (index == 6) key = "StandardSummary";
		else if (index < 10) key = "CoopHelp";
		else if (index < 13) key = "DifficultyHelp" + (index - 10).ToString();
		else if (index == 14) key = "CancelHelp";
		else
		{
			key = "PlayHelp";
			if (m_bHost) key = "HostHelp";
		}
		m_wHelp.SetText(AICF_Localization.Resolve("{AICF:AICF_Setup_" + key + "}"));
	}

	protected void Launch()
	{
		if (!AICF_ScenarioLaunch.Start(m_Selection, m_Entry, m_Config))
			m_wStatus.SetText(AICF_Localization.Resolve("{AICF:AICF_Setup_Unavailable}"));
	}
	override bool OnClick(Widget w, int x, int y, int button)
	{
		if (button != 0 || m_bClosing) return false;
		int index = m_aButtons.Find(w);
		if (index < 0 || !w.IsEnabled()) return false;
		int mode = m_Selection.GetMode();
		int difficulty = m_Selection.GetDifficulty();
		if (index < 3)
		{
			if (!SelectVariant(index, m_Selection.GetContent()))
				for (int candidate; candidate < 3; candidate++)
					if (SelectVariant(index, candidate)) break;
		}
		else if (index < 6) SelectVariant(m_Selection.GetMap(), index - 3);
		else if (index == 6) Select(0, difficulty);
		else if (index == 7 && mode == 0) Select(2, difficulty);
		else if ((index == 8 || index == 9) && mode > 0) Select(index - 7, difficulty);
		else if (index >= 10 && index <= 12) Select(mode, index - 10);
		else if (index == 13) Launch();
		else if (index == 14) SCR_ScenarioUICommon.AICF_CloseSetup();
		if (m_wRoot) Help(index);
		return true;
	}
	override bool OnFocus(Widget w, int x, int y)
	{
		int index = m_aButtons.Find(w);
		if (index >= 0) Help(index);
		return false;
	}
	override bool OnMouseEnter(Widget w, int x, int y)
	{
		int index = m_aButtons.Find(w);
		if (index >= 0) Help(index);
		return false;
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
		m_wHelp = null;
		m_aButtons.Clear();
		m_aFrames.Clear();
		m_aLabels.Clear();
		m_aFlags.Clear();
		if (m_wPreviousFocus) GetGame().GetWorkspace().SetFocusedWidget(m_wPreviousFocus);
		m_wPreviousFocus = null;
		m_Selection = null;
		m_Config = null;
		m_Entry = null;
	}
}
