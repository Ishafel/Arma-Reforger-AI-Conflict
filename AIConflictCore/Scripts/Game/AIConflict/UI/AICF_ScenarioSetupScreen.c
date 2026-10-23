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
	protected TextWidget m_wBriefingTitle;
	protected TextWidget m_wStatus;
	protected TextWidget m_wHelp;
	protected SliderWidget m_wTimeSlider;
	protected TextWidget m_wTimeValue;
	protected Widget m_wTimeTrack;
	protected Widget m_wTimeFill;
	protected Widget m_wTimeThumb;
	protected float m_fFontScale = 1;
	protected int m_iHotButton = -1;
	protected int m_iPreviewFailures;
	protected bool m_bTimeRefreshing;
	protected ref array<Widget> m_aFrames = {};
	protected ref array<Widget> m_aSurfaces = {};
	protected ref array<Widget> m_aAccents = {};
	protected ref array<ImageWidget> m_aMapImages = {};
	protected ref array<TextWidget> m_aMapTags = {};
	protected ref array<TextWidget> m_aMapDescriptions = {};
	protected ref array<TextWidget> m_aTextWidgets = {};
	protected ref array<int> m_aTextSizes = {};
	protected float m_fLayoutWidth;
	protected float m_fLayoutHeight;
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
		m_iHotButton = -1;
		m_iPreviewFailures = 0;
		m_Selection = new AICF_ScenarioSetupSelection(header);
		if (!AICF_ScenarioLaunch.IsForcedGraphCompatible(m_Selection.GetVariant()))
		{
			foreach (AICF_ScenarioVariant initial : m_Selection.GetRegistry().GetVariants())
				if (AICF_ScenarioLaunch.IsForcedGraphCompatible(initial) && m_Selection.SelectVariant(initial.m_iMap, initial.m_iContent)) break;
		}
		m_Config = config;
		m_Entry = entry;
		m_bHost = config != null;
		m_wPreviousFocus = GetGame().GetWorkspace().GetFocusedWidget();
		m_wRoot = m_Style.CreateRect(GetGame().GetWorkspace(), 0, 0, 1, 1, Color.FromSRGBA(12, 18, 22, 255), true);
		if (!m_wRoot) return false;
		m_wRoot.SetName("AICF_ScenarioSetupScreen");
		m_wRoot.SetZOrder(10000);
		m_wRoot.AddHandler(this);
		float width, height;
		m_wRoot.GetScreenSize(width, height);
		m_fFontScale = Math.Clamp(height / 1080.0, 0.78, 1.35);
		m_Style.CreateRect(m_wRoot, 0.04, 0.048, 0.044, 0.106, Gold(), false);
		Text(m_wRoot, 0.055, 0.035, 0.62, 0.108, "AI CONFLICT", 44);
		Text(m_wRoot, 0.055, 0.111, 0.65, 0.146, "{AICF:AICF_Setup_Title}", 19, Muted());
		string session = "{AICF:AICF_Setup_LocalSession}";
		if (m_bHost) session = "{AICF:AICF_Setup_NetworkSession}";
		Widget sessionBadge = m_Style.CreateRect(m_wRoot, 0.76, 0.062, 0.945, 0.112, Color.FromSRGBA(28, 38, 43, 255), false);
		Text(sessionBadge, 0.03, 0, 0.97, 1, session, 16, Muted(), true);
		m_Style.CreateRect(m_wRoot, 0.055, 0.16, 0.945, 0.162, Border(), false);

		Heading("01", "Map", 0.055, 0.17, 0.65);
		for (int mapIndex; mapIndex < 3; mapIndex++)
		{
			AICF_ScenarioVariant variant = m_Selection.GetRegistry().Find(mapIndex, 0);
			if (!variant) { Close(); return false; }
			AddButton(variant.m_sMapLabel, 0.055 + mapIndex * 0.203, 0.212, 0.244 + mapIndex * 0.203, 0.373, 21);
			AddMapPreview(mapIndex, variant);
		}
		Text(m_wRoot, 0.055, 0.404, 0.20, 0.458, "{AICF:AICF_Setup_Content}", 17, Muted());
		AddButton("Vanilla", 0.21, 0.404, 0.347, 0.458);
		AddButton("RHS", 0.36, 0.404, 0.497, 0.458);
		AddButton("WCS + RHS", 0.51, 0.404, 0.647, 0.458);

		Heading("02", "Mode", 0.055, 0.495, 0.345);
		Text(m_wRoot, 0.38, 0.495, 0.65, 0.526, "{AICF:AICF_Setup_Side}", 17, Muted());
		AddButton("Conflict", 0.055, 0.539, 0.194, 0.596);
		AddButton("Co-op", 0.206, 0.539, 0.345, 0.596);
		AddButton("", 0.38, 0.539, 0.508, 0.596, 18);
		AddButton("", 0.52, 0.539, 0.648, 0.596, 18);
		Text(m_wRoot, 0.055, 0.62, 0.65, 0.649, "{AICF:AICF_Setup_Difficulty}", 17, Muted());
		AddButton("{AICF:AICF_Setup_Easy}", 0.055, 0.661, 0.244, 0.714);
		AddButton("{AICF:AICF_Setup_Medium}", 0.258, 0.661, 0.447, 0.714);
		AddButton("{AICF:AICF_Setup_Hard}", 0.461, 0.661, 0.65, 0.714);

		Widget briefing = m_Style.CreateRect(m_wRoot, 0.69, 0.18, 0.945, 0.865, Color.FromSRGBA(21, 29, 34, 255), false);
		m_Style.CreateRect(briefing, 0, 0, 1, 0.004, Gold(), false);
		Text(m_wRoot, 0.71, 0.196, 0.926, 0.23, "{AICF:AICF_Setup_Briefing}", 15, Muted());
		m_wBriefingTitle = Text(m_wRoot, 0.71, 0.24, 0.926, 0.3, "", 29);
		m_wSummary = Text(m_wRoot, 0.71, 0.312, 0.926, 0.492, "", 20);
		m_Style.CreateRect(m_wRoot, 0.71, 0.517, 0.926, 0.519, Border(), false);
		Text(m_wRoot, 0.71, 0.536, 0.926, 0.57, "{AICF:AICF_Setup_DetailsTitle}", 15, Muted());
		m_wHelp = Text(m_wRoot, 0.71, 0.587, 0.926, 0.838, "", 18, Muted());
		m_Style.CreateRect(m_wRoot, 0, 0.894, 1, 1, Color.FromSRGBA(18, 26, 31, 255), false);
		m_wStatus = Text(m_wRoot, 0.245, 0.91, 0.69, 0.977, "", 16, Muted());
		string start = "{AICF:AICF_Setup_Play}";
		if (m_bHost) start = "{AICF:AICF_Setup_Host}";
		AddButton(start, 0.72, 0.917, 0.945, 0.974, 21);
		AddButton("{AICF:AICF_Setup_Cancel}", 0.055, 0.917, 0.212, 0.974, 19);

		Text(m_wRoot, 0.055, 0.742, 0.20, 0.78, "{AICF:AICF_Setup_Time}", 17, Muted());
		m_wTimeTrack = m_Style.CreateRect(m_wRoot, 0.21, 0.754, 0.55, 0.76, Border(), false);
		m_wTimeFill = m_Style.CreateRect(m_wTimeTrack, 0, 0, 0.333, 1, Gold(), false);
		m_wTimeThumb = m_Style.CreateRect(m_wRoot, 0.32, 0.741, 0.325, 0.774, Gold(), false);
		m_wTimeValue = Text(m_wRoot, 0.563, 0.73, 0.65, 0.788, "08:00", 30, Gold(), true);
		Text(m_wRoot, 0.21, 0.772, 0.268, 0.796, "00:00", 14, Muted());
		Text(m_wRoot, 0.49, 0.772, 0.55, 0.796, "23:59", 14, Muted());
		m_wTimeSlider = SliderWidget.Cast(GetGame().GetWorkspace().CreateWidget(WidgetType.SliderWidgetTypeID, WidgetFlags.VISIBLE | WidgetFlags.BLEND, Color.White, 20, m_wRoot));
		if (!m_wTimeSlider) { Close(); return false; }
		m_wTimeSlider.SetName("AICF_TimeSlider");
		m_wTimeSlider.SetOpacity(0);
		m_bTimeRefreshing = true;
		m_wTimeSlider.SetVertical(false);
		m_wTimeSlider.SetRange(0, 1439);
		m_wTimeSlider.SetStep(1);
		m_bTimeRefreshing = false;
		FrameSlot.SetAnchorMin(m_wTimeSlider, 0.21, 0.736);
		FrameSlot.SetAnchorMax(m_wTimeSlider, 0.55, 0.78);
		FrameSlot.SetOffsets(m_wTimeSlider, 0, 0, 0, 0);
		Text(m_wRoot, 0.055, 0.813, 0.20, 0.864, "{AICF:AICF_Setup_Weather}", 17, Muted());
		for (int weather; weather < 5; weather++)
			AddButton("{AICF:AICF_Setup_Weather" + weather.ToString() + "}", 0.21 + weather * 0.09, 0.813, 0.29 + weather * 0.09, 0.864, 16);
		if (!m_wBriefingTitle || !m_wSummary || !m_wStatus || !m_wHelp || !m_wTimeValue || !m_wTimeTrack || !m_wTimeFill || !m_wTimeThumb || m_aButtons.Count() != 20) { Close(); return false; }
		m_wBriefingTitle.SetTextWrapping(true);
		m_wSummary.SetTextWrapping(true);
		m_wStatus.SetTextWrapping(true);
		m_wHelp.SetTextWrapping(true);
		m_wSummary.ClearFlags(WidgetFlags.VCENTER);
		m_wHelp.ClearFlags(WidgetFlags.VCENTER);
		Refresh();
		Help(13);
		GetGame().GetWorkspace().AddModal(m_wRoot, m_aButtons[0]);
		return true;
	}

	protected ref Color Gold() { return Color.FromSRGBA(224, 177, 87, 255); }
	protected ref Color Muted() { return Color.FromSRGBA(167, 181, 189, 255); }
	protected ref Color Border() { return Color.FromSRGBA(53, 67, 75, 255); }
	protected TextWidget Text(Widget parent, float left, float top, float right, float bottom, string label, int size, Color color = null, bool centered = false)
	{
		ref Color retainedColor = color;
		if (!retainedColor) retainedColor = Color.FromSRGBA(237, 240, 242, 255);
		TextWidget text = m_Style.CreateText(parent, left, top, right, bottom, label, Math.Max(13, Math.Round(size * m_fFontScale)), retainedColor, centered);
		if (text) { m_aTextWidgets.Insert(text); m_aTextSizes.Insert(size); }
		return text;
	}
	protected void Heading(string number, string key, float left, float top, float right)
	{
		Text(m_wRoot, left, top, left + 0.026, top + 0.03, number, 15, Gold());
		Text(m_wRoot, left + 0.035, top, right, top + 0.03, "{AICF:AICF_Setup_" + key + "}", 18);
	}
	protected void AddButton(string label, float left, float top, float right, float bottom, int fontSize = 20)
	{
		Widget frame = m_Style.CreateRect(m_wRoot, left, top, right, bottom, Border(), true);
		if (!frame) return;
		frame.SetZOrder(10);
		Widget surface = m_Style.CreateRect(frame, 0.004, 0.025, 0.996, 0.975, Color.FromSRGBA(26, 36, 43, 255), false);
		Widget accent = m_Style.CreateRect(frame, 0, 0.965, 1, 1, Gold(), false);
		TextWidget caption = Text(frame, 0.025, 0, 0.975, 1, label, fontSize, null, true);
		Widget input = frame.FindAnyWidget("AICF_RectInput");
		if (!input || !caption || !surface || !accent) return;
		accent.SetZOrder(3);
		m_aFrames.Insert(frame);
		m_aSurfaces.Insert(surface);
		m_aAccents.Insert(accent);
		m_aButtons.Insert(input);
		m_aLabels.Insert(caption);
	}
	protected void AddMapPreview(int button, AICF_ScenarioVariant variant)
	{
		FrameSlot.SetAnchorMin(m_aLabels[button], 0.055, 0.65);
		FrameSlot.SetAnchorMax(m_aLabels[button], 0.945, 0.82);
		m_aLabels[button].ClearFlags(WidgetFlags.CENTER);
		ref Color accent = Color.FromSRGBA(variant.m_vMapAccent[0], variant.m_vMapAccent[1], variant.m_vMapAccent[2], 255);
		m_aMapDescriptions.Insert(Text(m_aFrames[button], 0.055, 0.82, 0.945, 0.96, variant.m_sMapDescription, 14, Muted()));
		string missing;
		if (!variant.IsAvailable(missing)) return;
		SCR_MissionHeader source = SCR_MissionHeader.Cast(MissionHeader.ReadMissionHeader(variant.m_sPreviewHeader));
		if (!source) { m_iPreviewFailures++; return; }
		Widget area = m_Style.CreateRect(m_aFrames[button], 0.008, 0.012, 0.992, 0.63, Color.White, false);
		ImageWidget image;
		if (area) image = ImageWidget.Cast(area.FindAnyWidget("AICF_RectBackground"));
		ResourceName texture = source.m_sPreviewImage;
		if (variant.m_bPreviewArtwork) texture = source.m_sLoadingScreen;
		if (!image || !image.LoadImageTexture(0, texture)) { m_iPreviewFailures++; return; }
		area.SetFlags(WidgetFlags.CLIPCHILDREN);
		image.SetFlags(WidgetFlags.INHERIT_CLIPPING);
		image.SetSaturation(0.65);
		image.SetColor(Color.White);
		m_aMapImages.Insert(image);
		Widget tint = m_Style.CreateRect(area, 0, 0, 1, 1, Color.FromSRGBA(variant.m_vMapAccent[0], variant.m_vMapAccent[1], variant.m_vMapAccent[2], 45), false);
		if (tint) tint.SetZOrder(1);
		if (variant.m_bPreviewSectorGrid) AddSectorGrid(area, accent);
		Widget badge = m_Style.CreateRect(area, 0.04, 0.08, 0.94, 0.36, Color.FromSRGBA(12, 21, 27, 230), false);
		if (!badge) { m_iPreviewFailures++; return; }
		badge.SetZOrder(3);
		m_Style.CreateRect(badge, 0, 0, 0.015, 1, accent, false);
		m_aMapTags.Insert(Text(badge, 0.045, 0, 0.965, 1, variant.m_sMapTag, 13, accent));
		m_Style.CreateRect(m_aFrames[button], 0.055, 0.615, 0.23, 0.635, accent, false);
	}
	// Декоративная сетка обозначает ограниченный сектор, без world-координат.
	protected void AddSectorGrid(Widget area, Color accent)
	{
		Widget grid = m_Style.CreateRect(area, 0, 0, 1, 1, Color.FromSRGBA(11, 27, 42, 110), false);
		if (!grid) return;
		grid.SetName("AICF_CardSectorGrid");
		grid.SetZOrder(2);
		for (int column = 1; column < 6; column++)
			m_Style.CreateRect(grid, column / 6.0, 0, column / 6.0 + 0.002, 1, Color.FromSRGBA(174, 208, 231, 75), false);
		for (int row = 1; row < 4; row++)
			m_Style.CreateRect(grid, 0, row / 4.0, 1, row / 4.0 + 0.004, Color.FromSRGBA(174, 208, 231, 75), false);
		Text(grid, 0.8, 0.56, 0.94, 0.93, "N", 24, accent, true);
	}
	// Размеры новых widgets становятся окончательными после native layout pass.
	// При неизменном размере root работа выполняется только один раз.
	override bool OnUpdate(Widget w)
	{
		if (w != m_wRoot || m_bClosing) return false;
		float width, height;
		m_wRoot.GetScreenSize(width, height);
		if (width <= 0 || height <= 0 || (width == m_fLayoutWidth && height == m_fLayoutHeight)) return false;
		m_fFontScale = Math.Clamp(height / 1080.0, 0.78, 1.35);
		foreach (int index, TextWidget text : m_aTextWidgets)
			text.SetExactFontSize(Math.Max(13, Math.Round(m_aTextSizes[index] * m_fFontScale)));
		bool fitted = true;
		foreach (ImageWidget image : m_aMapImages)
		{
			int textureWidth, textureHeight;
			float frameWidth, frameHeight;
			image.GetImageSize(0, textureWidth, textureHeight);
			image.GetParent().GetScreenSize(frameWidth, frameHeight);
			if (textureWidth <= 0 || textureHeight <= 0 || frameWidth <= 0 || frameHeight <= 0) { fitted = false; continue; }
			float imageRatio = textureWidth / (float)textureHeight;
			float frameRatio = frameWidth / frameHeight;
			float scaleX = 1;
			float scaleY = 1;
			if (imageRatio > frameRatio) scaleX = imageRatio / frameRatio;
			else scaleY = frameRatio / imageRatio;
			FrameSlot.SetAnchorMin(image, (1 - scaleX) * 0.5, (1 - scaleY) * 0.5);
			FrameSlot.SetAnchorMax(image, (1 + scaleX) * 0.5, (1 + scaleY) * 0.5);
		}
		if (fitted) { m_fLayoutWidth = width; m_fLayoutHeight = height; }
		return false;
	}
	protected void StyleButtons()
	{
		int mode = m_Selection.GetMode();
		int weather = m_Selection.GetEnvironment().m_iWeather;
		foreach (int index, Widget frame : m_aFrames)
		{
			bool selected = index == m_Selection.GetMap() || index == m_Selection.GetContent() + 3 || (index == 6 && mode == 0) || (index == 7 && mode > 0) || (mode > 0 && index == mode + 7) || index == m_Selection.GetDifficulty() + 10 || index == weather + 15;
			Color border = Border();
			Color surface = Color.FromSRGBA(26, 36, 43, 255);
			Color label = Color.FromSRGBA(237, 240, 242, 255);
			if (selected)
			{
				border = Gold();
				surface = Color.FromSRGBA(58, 52, 35, 255);
			}
			else if ((index == m_iHotButton || m_aButtons[index] == GetGame().GetWorkspace().GetFocusedWidget()) && m_aButtons[index].IsEnabled())
			{
				border = Color.FromSRGBA(175, 193, 202, 255);
				surface = Color.FromSRGBA(42, 56, 64, 255);
			}
			if (index == 13)
			{
				border = Gold();
				surface = Gold();
				label = Color.FromSRGBA(18, 24, 28, 255);
				if (m_iHotButton == index || m_aButtons[index] == GetGame().GetWorkspace().GetFocusedWidget()) surface = Color.FromSRGBA(245, 201, 125, 255);
			}
			m_Style.SetRectColor(frame, border);
			m_Style.SetRectColor(m_aSurfaces[index], surface);
			m_aLabels[index].SetColor(label);
			m_aAccents[index].SetVisible(selected);
			float opacity = 1;
			if (!m_aButtons[index].IsEnabled()) opacity = 0.4;
			frame.SetOpacity(opacity);
		}
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
		if (time < 0 || time > 1439) return false;
		if (!m_Selection || !m_Selection.SelectEnvironment(time, weather)) return false;
		// Перетаскивание не пересоздаёт флаги и не сканирует установленные addons.
		RefreshEnvironment();
		return true;
	}
	protected string TimeLabel()
	{
		AICF_ScenarioEnvironment settings = m_Selection.GetEnvironment();
		return settings.GetHour().ToString(2) + ":" + settings.GetMinute().ToString(2);
	}
	protected void RefreshEnvironment()
	{
		AICF_ScenarioEnvironment settings = m_Selection.GetEnvironment();
		m_bTimeRefreshing = true;
		m_wTimeSlider.SetCurrent(settings.m_iStartMinutes);
		m_bTimeRefreshing = false;
		m_wTimeValue.SetText(TimeLabel());
		float ratio = settings.m_iStartMinutes / 1439.0;
		FrameSlot.SetAnchorMax(m_wTimeFill, ratio, 1);
		float thumbLeft = 0.21 + 0.335 * ratio;
		FrameSlot.SetAnchorMin(m_wTimeThumb, thumbLeft, 0.741);
		FrameSlot.SetAnchorMax(m_wTimeThumb, thumbLeft + 0.005, 0.774);
		StyleButtons();
		RefreshSummary();
	}

	protected void Refresh()
	{
		int mode = m_Selection.GetMode();
		int mapIndex = m_Selection.GetMap();
		int content = m_Selection.GetContent();
		AICF_ScenarioVariant variant = m_Selection.GetVariant();
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
			FrameSlot.SetAnchorMin(m_aFrames[contentButton + 3], 0.21 + visibleContent * 0.15, 0.404);
			FrameSlot.SetAnchorMax(m_aFrames[contentButton + 3], 0.347 + visibleContent * 0.15, 0.458);
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
		string status = AICF_Localization.Resolve("{AICF:AICF_Setup_Reload}");
		if (!unavailable.IsEmpty()) status = AICF_Localization.Resolve("{AICF:AICF_Setup_Missing}") + " " + unavailable;
		if (!AICF_ScenarioLaunch.IsForcedGraphCompatible(m_Selection.GetRegistry().Find(mapIndex, 0))) status = AICF_Localization.Resolve("{AICF:AICF_Setup_Pinned}");
		m_wStatus.SetText(status);
		m_aButtons[13].SetEnabled(variant.IsContentEnabled() && variant.IsAvailable(missing) && AICF_ScenarioLaunch.IsForcedGraphCompatible(variant) && !m_Selection.GetResource().IsEmpty());
		RefreshEnvironment();
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
		m_wBriefingTitle.SetText(AICF_Localization.Resolve(variant.m_sMapLabel));
		m_wSummary.SetText(string.Format("%1 · %2\n%3\n%4\n%5 · %6", m_aLabels[content + 3].GetText(), modeLabel, side, m_aLabels[m_Selection.GetDifficulty() + 10].GetText(), TimeLabel(), m_aLabels[m_Selection.GetEnvironment().m_iWeather + 15].GetText()));
	}

	protected void Help(int index)
	{
		string key;
		if (index == -1) key = "TimeHelp";
		else if (index < 3) key = "MapHelp";
		else if (index < 6) key = "ContentHelp";
		else if (index == 6) key = "StandardSummary";
		else if (index < 10) key = "CoopHelp";
		else if (index < 13) key = "DifficultyHelp" + (index - 10).ToString();
		else if (index == 14) key = "CancelHelp";
		else if (index >= 15) key = "WeatherHelp";
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
		else if (index >= 15) SelectEnvironment(m_Selection.GetEnvironment().m_iStartMinutes, index - 15);
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
		if (m_bClosing || !m_Selection) return false;
		if (w == m_wTimeSlider) Help(-1);
		int index = m_aButtons.Find(w);
		if (index >= 0) { m_iHotButton = index; StyleButtons(); Help(index); }
		return false;
	}
	override bool OnMouseEnter(Widget w, int x, int y)
	{
		if (m_bClosing || !m_Selection) return false;
		if (w == m_wTimeSlider) Help(-1);
		int index = m_aButtons.Find(w);
		if (index >= 0) { m_iHotButton = index; StyleButtons(); Help(index); }
		return false;
	}
	override bool OnMouseLeave(Widget w, Widget enterW, int x, int y)
	{
		if (m_bClosing || !m_Selection) return false;
		if (m_iHotButton == m_aButtons.Find(w)) m_iHotButton = -1;
		StyleButtons();
		return false;
	}
	override bool OnFocusLost(Widget w, int x, int y)
	{
		if (m_bClosing || !m_Selection) return false;
		if (m_iHotButton == m_aButtons.Find(w)) m_iHotButton = -1;
		StyleButtons();
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
		m_wBriefingTitle = null;
		m_wStatus = null;
		m_wHelp = null;
		m_wTimeSlider = null;
		m_wTimeValue = null;
		m_wTimeTrack = null;
		m_wTimeFill = null;
		m_wTimeThumb = null;
		m_aButtons.Clear();
		m_aFrames.Clear();
		m_aSurfaces.Clear();
		m_aAccents.Clear();
		m_aMapImages.Clear();
		m_aMapTags.Clear();
		m_aMapDescriptions.Clear();
		m_aTextWidgets.Clear();
		m_aTextSizes.Clear();
		m_fLayoutWidth = 0;
		m_fLayoutHeight = 0;
		m_aLabels.Clear();
		m_aFlags.Clear();
		if (m_wPreviousFocus) GetGame().GetWorkspace().SetFocusedWidget(m_wPreviousFocus);
		m_wPreviousFocus = null;
		m_Selection = null;
		m_Config = null;
		m_Entry = null;
	}
}
