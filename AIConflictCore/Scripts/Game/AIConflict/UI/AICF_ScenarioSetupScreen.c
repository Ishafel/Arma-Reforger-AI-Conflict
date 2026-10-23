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
	protected SliderWidget m_wTimeSlider;
	protected TextWidget m_wTimeValue;
	protected Widget m_wTimeTrack;
	protected Widget m_wTimeFill;
	protected Widget m_wTimeThumb;
	protected int m_iLastFixedMinutes = 480;
	protected bool m_bTimeRefreshing;
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
		string headings[7] = {"Map", "Content", "Mode", "Side", "Difficulty", "Time", "Weather"};
		for (int row; row < 7; row++)
			m_Style.CreateText(m_wRoot, 0.05, 0.19 + row * 0.08, 0.22, 0.235 + row * 0.08, "{AICF:AICF_Setup_" + headings[row] + "}", 18, Color.White);
		for (int mapIndex; mapIndex < 3; mapIndex++)
		{
			AICF_ScenarioVariant variant = m_Selection.GetRegistry().Find(mapIndex, 0);
			if (!variant) { Close(); return false; }
			AddButton(variant.m_sMapLabel, 0.23 + mapIndex * 0.145, 0.18, 0.365 + mapIndex * 0.145, 0.245);
		}
		AddButton("Vanilla", 0.23, 0.26, 0.365, 0.325);
		AddButton("RHS", 0.375, 0.26, 0.51, 0.325);
		AddButton("WCS + RHS", 0.52, 0.26, 0.655, 0.325);
		AddButton("Conflict", 0.23, 0.34, 0.435, 0.405);
		AddButton("Co-op", 0.45, 0.34, 0.655, 0.405);
		AddButton("", 0.23, 0.42, 0.435, 0.485);
		AddButton("", 0.45, 0.42, 0.655, 0.485);
		AddButton("{AICF:AICF_Setup_Easy}", 0.23, 0.50, 0.365, 0.565);
		AddButton("{AICF:AICF_Setup_Medium}", 0.375, 0.50, 0.51, 0.565);
		AddButton("{AICF:AICF_Setup_Hard}", 0.52, 0.50, 0.655, 0.565);
		m_Style.CreateRect(m_wRoot, 0.69, 0.18, 0.95, 0.725, Color.FromSRGBA(30, 46, 49, 255), false);
		m_wHelp = m_Style.CreateText(m_wRoot, 0.71, 0.22, 0.93, 0.71, "", 19, Color.White);
		m_wSummary = m_Style.CreateText(m_wRoot, 0.05, 0.75, 0.95, 0.82, "", 19, Color.White);
		m_wStatus = m_Style.CreateText(m_wRoot, 0.05, 0.825, 0.95, 0.88, "", 17, Color.White);
		string start = "{AICF:AICF_Setup_Play}";
		if (m_bHost) start = "{AICF:AICF_Setup_Host}";
		AddButton(start, 0.70, 0.90, 0.95, 0.97);
		AddButton("{AICF:AICF_Setup_Cancel}", 0.05, 0.90, 0.25, 0.97);
		AddButton("{AICF:AICF_Setup_Time4}", 0.58, 0.58, 0.655, 0.645, 15);
		m_wTimeTrack = m_Style.CreateRect(m_wRoot, 0.23, 0.604, 0.50, 0.613, Color.FromSRGBA(60, 79, 81, 255), false);
		m_wTimeFill = m_Style.CreateRect(m_wTimeTrack, 0, 0, 0.333, 1, Color.FromSRGBA(231, 175, 74, 255), false);
		m_wTimeThumb = m_Style.CreateRect(m_wRoot, 0.31, 0.59, 0.32, 0.627, Color.FromSRGBA(231, 175, 74, 255), false);
		m_wTimeValue = m_Style.CreateText(m_wRoot, 0.505, 0.585, 0.577, 0.635, "08:00", 20, Color.White, true);
		m_Style.CreateText(m_wRoot, 0.23, 0.625, 0.29, 0.648, "00:00", 12, Color.White);
		m_Style.CreateText(m_wRoot, 0.455, 0.625, 0.51, 0.648, "23:59", 12, Color.White);
		m_wTimeSlider = SliderWidget.Cast(GetGame().GetWorkspace().CreateWidget(WidgetType.SliderWidgetTypeID, WidgetFlags.VISIBLE | WidgetFlags.BLEND, Color.White, 20, m_wRoot));
		if (!m_wTimeSlider) { Close(); return false; }
		m_wTimeSlider.SetName("AICF_TimeSlider");
		m_wTimeSlider.SetOpacity(0);
		m_bTimeRefreshing = true;
		m_wTimeSlider.SetVertical(false);
		m_wTimeSlider.SetRange(0, 1439);
		m_wTimeSlider.SetStep(1);
		m_bTimeRefreshing = false;
		FrameSlot.SetAnchorMin(m_wTimeSlider, 0.23, 0.58);
		FrameSlot.SetAnchorMax(m_wTimeSlider, 0.50, 0.645);
		FrameSlot.SetOffsets(m_wTimeSlider, 0, 0, 0, 0);
		for (int weather; weather < 5; weather++)
			AddButton("{AICF:AICF_Setup_Weather" + weather.ToString() + "}", 0.23 + weather * 0.087, 0.66, 0.307 + weather * 0.087, 0.725, 15);
		if (!m_wSummary || !m_wStatus || !m_wHelp || !m_wTimeValue || !m_wTimeTrack || !m_wTimeFill || !m_wTimeThumb || m_aButtons.Count() != 21) { Close(); return false; }
		m_wSummary.SetTextWrapping(true);
		m_wStatus.SetTextWrapping(true);
		m_wHelp.SetTextWrapping(true);
		Refresh();
		Help(13);
		GetGame().GetWorkspace().AddModal(m_wRoot, m_aButtons[0]);
		return true;
	}

	protected void AddButton(string text, float left, float top, float right, float bottom, int fontSize = 20)
	{
		Widget frame = m_Style.CreateRect(m_wRoot, left, top, right, bottom, Color.FromSRGBA(42, 60, 62, 255), true);
		if (!frame) return;
		frame.SetZOrder(10);
		TextWidget label = m_Style.CreateText(frame, 0, 0, 1, 1, text, fontSize, Color.White, true);
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
	bool SelectEnvironment(int time, int weather)
	{
		if (!m_Selection || !m_Selection.SelectEnvironment(time, weather)) return false;
		// Перетаскивание не пересоздаёт флаги и не сканирует установленные addons.
		RefreshEnvironment();
		return true;
	}
	protected string TimeLabel()
	{
		AICF_ScenarioEnvironment settings = m_Selection.GetEnvironment();
		if (settings.m_iStartMinutes == AICF_ScenarioEnvironment.RANDOM_TIME)
			return AICF_Localization.Resolve("{AICF:AICF_Setup_Time4}");
		return settings.GetHour().ToString(2) + ":" + settings.GetMinute().ToString(2);
	}
	protected void RefreshEnvironment()
	{
		AICF_ScenarioEnvironment settings = m_Selection.GetEnvironment();
		bool random = settings.m_iStartMinutes == AICF_ScenarioEnvironment.RANDOM_TIME;
		if (!random) m_iLastFixedMinutes = settings.m_iStartMinutes;
		m_bTimeRefreshing = true;
		m_wTimeSlider.SetCurrent(m_iLastFixedMinutes);
		m_bTimeRefreshing = false;
		m_wTimeValue.SetText(TimeLabel());
		float ratio = m_iLastFixedMinutes / 1439.0;
		FrameSlot.SetAnchorMax(m_wTimeFill, ratio, 1);
		float thumbLeft = 0.23 + 0.26 * ratio;
		FrameSlot.SetAnchorMin(m_wTimeThumb, thumbLeft, 0.59);
		FrameSlot.SetAnchorMax(m_wTimeThumb, thumbLeft + 0.01, 0.627);
		float opacity = 1;
		if (random) opacity = 0.45;
		m_wTimeTrack.SetOpacity(opacity);
		m_wTimeThumb.SetOpacity(opacity);
		for (int index = 15; index < 21; index++)
		{
			Color color = Color.FromSRGBA(42, 60, 62, 255);
			if ((index == 15 && random) || index == settings.m_iWeather + 16) color = Color.FromSRGBA(111, 85, 36, 255);
			m_Style.SetRectColor(m_aFrames[index], color);
		}
		RefreshSummary();
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
				if (mapVariant && mapVariant.IsContentEnabled() && mapVariant.IsAvailable(missing) && AICF_ScenarioLaunch.IsForcedGraphCompatible(mapVariant)) mapAvailable = true;
			}
			m_aButtons[mapButton].SetEnabled(mapAvailable);
		}
		string unavailable;
		int visibleContent;
		for (int contentButton; contentButton < 3; contentButton++)
		{
			AICF_ScenarioVariant contentVariant = m_Selection.GetRegistry().Find(mapIndex, contentButton);
			bool visible = contentVariant && contentVariant.IsContentEnabled();
			m_aFrames[contentButton + 3].SetVisible(visible);
			bool available = visible && contentVariant.IsAvailable(missing) && AICF_ScenarioLaunch.IsForcedGraphCompatible(contentVariant);
			m_aButtons[contentButton + 3].SetEnabled(available);
			if (!visible) continue;
			FrameSlot.SetAnchorMin(m_aFrames[contentButton + 3], 0.23 + visibleContent * 0.145, 0.26);
			FrameSlot.SetAnchorMax(m_aFrames[contentButton + 3], 0.365 + visibleContent * 0.145, 0.325);
			visibleContent++;
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
		RefreshEnvironment();
		string status = AICF_Localization.Resolve("{AICF:AICF_Setup_Reload}");
		if (!unavailable.IsEmpty()) status = AICF_Localization.Resolve("{AICF:AICF_Setup_Missing}") + " " + unavailable;
		if (!AICF_ScenarioLaunch.IsForcedGraphCompatible(m_Selection.GetRegistry().Find(mapIndex, 0))) status = AICF_Localization.Resolve("{AICF:AICF_Setup_Pinned}");
		m_wStatus.SetText(status);
		m_aButtons[13].SetEnabled(variant.IsContentEnabled() && variant.IsAvailable(missing) && AICF_ScenarioLaunch.IsForcedGraphCompatible(variant) && !m_Selection.GetResource().IsEmpty());
	}

	protected void RefreshSummary()
	{
		int mode = m_Selection.GetMode();
		int content = m_Selection.GetContent();
		AICF_ScenarioVariant variant = m_Selection.GetVariant();
		string side = AICF_Localization.Resolve("{AICF:AICF_Setup_BothSides}");
		if (mode == 1) side = AICF_Localization.Resolve(variant.m_sWestLabel);
		if (mode == 2) side = AICF_Localization.Resolve(variant.m_sEastLabel);
		string modeLabel = "Conflict";
		if (mode > 0) modeLabel = "Co-op";
		m_wSummary.SetText(string.Format("%1 · %2 · %3 · %4 · %5 · %6 · %7", AICF_Localization.Resolve(variant.m_sMapLabel), m_aLabels[content + 3].GetText(), modeLabel, side, m_aLabels[m_Selection.GetDifficulty() + 10].GetText(), TimeLabel(), m_aLabels[m_Selection.GetEnvironment().m_iWeather + 16].GetText()));
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
		else if (index == 15) key = "TimeHelp";
		else if (index >= 16) key = "WeatherHelp";
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
		else if (index == 15)
		{
			int time = AICF_ScenarioEnvironment.RANDOM_TIME;
			if (m_Selection.GetEnvironment().m_iStartMinutes == time) time = m_iLastFixedMinutes;
			SelectEnvironment(time, m_Selection.GetEnvironment().m_iWeather);
		}
		else if (index >= 16) SelectEnvironment(m_Selection.GetEnvironment().m_iStartMinutes, index - 16);
		if (m_wRoot) Help(index);
		return true;
	}
	override bool OnChange(Widget w, bool finished)
	{
		if (w != m_wTimeSlider || m_bClosing || m_bTimeRefreshing) return false;
		SelectEnvironment(Math.Round(m_wTimeSlider.GetCurrent()), m_Selection.GetEnvironment().m_iWeather);
		return true;
	}
	override bool OnFocus(Widget w, int x, int y)
	{
		if (w == m_wTimeSlider) Help(15);
		int index = m_aButtons.Find(w);
		if (index >= 0) Help(index);
		return false;
	}
	override bool OnMouseEnter(Widget w, int x, int y)
	{
		if (w == m_wTimeSlider) Help(15);
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
		m_wTimeSlider = null;
		m_wTimeValue = null;
		m_wTimeTrack = null;
		m_wTimeFill = null;
		m_wTimeThumb = null;
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
