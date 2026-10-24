// Isolated stage only. Native catalog/widgets/plans; no mouse/keyboard automation.
modded class AICF_ScenarioSetupScreen
{
	bool AICF_UniversalWidgets()
	{
		return m_aButtons.Count() == 20 && m_aSurfaces.Count() == 20 && m_aAccents.Count() == 20 && m_aFlags.Count() == 2 && m_iFlagFailures == 0 && m_wHelp && !m_wHelp.GetText().IsEmpty() && !m_wHelp.GetText().Contains("{AICF:");
	}
	bool AICF_MapPreviews()
	{
		Print(string.Format("[AICF][UNIVERSAL_PREVIEWS] images=%1 failures=%2", m_aMapImages.Count(), m_iPreviewFailures));
		for (int mapIndex; mapIndex < 3; mapIndex++)
		{
			SCR_MissionHeader source = SCR_MissionHeader.Cast(MissionHeader.ReadMissionHeader(m_Selection.GetRegistry().Find(mapIndex, 0).m_sPreviewHeader));
			if (source) Print(string.Format("[AICF][UNIVERSAL_PREVIEW_SOURCE] map=%1 preview=%2 icon=%3 loading=%4", mapIndex, source.m_sPreviewImage, source.m_sIcon, source.m_sLoadingScreen));
		}
		if (m_aMapImages.Count() != 3 || m_iPreviewFailures != 0) return false;
		foreach (ImageWidget image : m_aMapImages)
		{
			int textureWidth, textureHeight;
			float width, height;
			image.GetImageSize(0, textureWidth, textureHeight);
			image.GetScreenSize(width, height);
			if (textureWidth <= 0 || textureHeight <= 0 || width <= 0 || height <= 0) return false;
			if (Math.AbsFloat(width / height - textureWidth / (float)textureHeight) > 0.02) return false;
		}
		return true;
	}
	bool AICF_TextPalette()
	{
		return m_wTimeValue.GetColorInt() == Gold().PackToInt() && m_wSummary.GetColorInt() == Color.FromSRGBA(237, 240, 242, 255).PackToInt();
	}
	bool AICF_MapCardIdentity()
	{
		if (m_aMapTags.Count() != 3 || m_aMapDescriptions.Count() != 3) return false;
		array<int> colors = {};
		array<string> descriptions = {};
		for (int mapIndex; mapIndex < 3; mapIndex++)
		{
			TextWidget tag = m_aMapTags[mapIndex];
			TextWidget description = m_aMapDescriptions[mapIndex];
			if (!tag || !description || tag.GetText().IsEmpty() || description.GetText().IsEmpty()) return false;
			if (tag.GetText().Contains("{AICF:") || description.GetText().Contains("{AICF:")) return false;
			if (colors.Find(tag.GetColorInt()) >= 0 || descriptions.Find(description.GetText()) >= 0) return false;
			colors.Insert(tag.GetColorInt());
			descriptions.Insert(description.GetText());
			bool grid = m_aFrames[mapIndex].FindAnyWidget("AICF_CardSectorGrid") != null;
			if (grid) return false;
			array<TextWidget> labels = { m_aLabels[mapIndex], tag, description };
			float cardX, cardY, cardWidth, cardHeight;
			m_aFrames[mapIndex].GetScreenPos(cardX, cardY);
			m_aFrames[mapIndex].GetScreenSize(cardWidth, cardHeight);
			foreach (TextWidget label : labels)
			{
				float x, y, width, height;
				label.GetScreenPos(x, y);
				label.GetScreenSize(width, height);
				if (width <= 0 || height <= 0 || x < cardX - 1 || y < cardY - 1 || x + width > cardX + cardWidth + 1 || y + height > cardY + cardHeight + 1) return false;
				float textWidth, textHeight;
				label.GetTextSize(textWidth, textHeight);
				bool fits = textWidth <= GetGame().GetWorkspace().DPIUnscale(width) + 2 && textHeight <= GetGame().GetWorkspace().DPIUnscale(height) + 2;
				Print(string.Format("[AICF][UNIVERSAL_CARD_TEXT] map=%1 text=%2 width=%3 height=%4 glyph_width=%5 glyph_height=%6 fits=%7", mapIndex, label.GetText(), width, height, textWidth, textHeight, fits));
				if (!fits) return false;
			}
			Print(string.Format("[AICF][UNIVERSAL_CARD] map=%1 tag=%2 description=%3 color=%4 grid=%5", mapIndex, tag.GetText(), description.GetText(), tag.GetColorInt(), grid));
		}
		SCR_MissionHeader source = SCR_MissionHeader.Cast(MissionHeader.ReadMissionHeader(m_Selection.GetRegistry().Find(1, 0).m_sPreviewHeader));
		return source && !source.m_sLoadingScreen.IsEmpty() && source.m_sLoadingScreen != source.m_sPreviewImage;
	}
	bool AICF_BriefingLayout()
	{
		float rootX, rootY, rootWidth, rootHeight;
		m_wRoot.GetScreenPos(rootX, rootY);
		m_wRoot.GetScreenSize(rootWidth, rootHeight);
		foreach (int index, Widget frame : m_aFrames)
		{
			if (!frame.IsVisible()) continue;
			float x, y, width, height;
			frame.GetScreenPos(x, y);
			frame.GetScreenSize(width, height);
			if (width <= 0 || height <= 0 || x < rootX - 1 || y < rootY - 1 || x + width > rootX + rootWidth + 1 || y + height > rootY + rootHeight + 1) return false;
			foreach (int otherIndex, Widget other : m_aFrames)
			{
				if (otherIndex <= index || !other.IsVisible()) continue;
				float otherX, otherY, otherWidth, otherHeight;
				other.GetScreenPos(otherX, otherY);
				other.GetScreenSize(otherWidth, otherHeight);
				if (x < otherX + otherWidth - 1 && x + width > otherX + 1 && y < otherY + otherHeight - 1 && y + height > otherY + 1) return false;
			}
		}
		return m_wBriefingTitle && !m_wBriefingTitle.GetText().IsEmpty() && !m_wBriefingTitle.GetText().Contains("{AICF:") && m_wSummary.GetNumLines() >= 4;
	}
	bool AICF_TimeSliderReadback(int minutes, string label)
	{
		return m_wTimeSlider && m_wTimeSlider.GetMin() == 0 && m_wTimeSlider.GetMax() == 1439 && m_wTimeSlider.GetStep() == 1 && !m_wTimeSlider.IsVertical() && m_wTimeSlider.GetCurrent() == minutes && m_wTimeValue.GetText() == label;
	}
	bool AICF_UniversalSides() { return m_aButtons[8].IsEnabled() && m_aButtons[9].IsEnabled(); }
	bool AICF_GuideButton()
	{
		return m_wGuideButton && m_wGuideButton.IsVisible() && m_wGuide && !m_wGuide.IsVisible() && !m_bGuideOpen;
	}
	bool AICF_GuideModal()
	{
		return m_wGuide && m_wGuide.IsVisible() && m_bGuideOpen && GetGame().GetWorkspace().GetModal() == m_wGuide;
	}
	protected bool AICF_TraceButton(Widget button)
	{
		Widget modal = GetGame().GetWorkspace().GetModal();
		if (!modal || !button || !button.IsEnabledInHierarchy()) return false;
		float x, y, width, height;
		button.GetScreenPos(x, y);
		button.GetScreenSize(width, height);
		array<float> fractions = {0.5, 0.2, 0.8};
		foreach (float horizontal : fractions)
			foreach (float vertical : fractions)
			{
				array<Widget> hits = {};
				WidgetManager.TraceWidgets(Math.Round(x + width * horizontal), Math.Round(y + height * vertical), modal, hits);
				Widget first;
				foreach (Widget candidate : hits)
				{
					if (!ButtonWidget.Cast(candidate) || (candidate.GetFlags() & WidgetFlags.IGNORE_CURSOR) != 0) continue;
					first = candidate;
					break;
				}
				string parent = "NULL";
				if (first && first.GetParent()) parent = first.GetParent().GetName();
				Print(string.Format("[AICF][UNIVERSAL_GUIDE_HIT] expected_parent=%1 actual_parent=%2 horizontal=%3 vertical=%4 pass=%5", button.GetParent().GetName(), parent, horizontal, vertical, first == button));
				if (first != button) return false;
			}
		return true;
	}
	bool AICF_GuideHitTarget() { return AICF_TraceButton(m_wGuideButton); }
	bool AICF_GuideCloseHitTarget() { return AICF_TraceButton(m_wGuideClose); }
	bool AICF_GuideLayout()
	{
		if (!AICF_GuideModal() || m_aGuideTexts.Count() != 3) return false;
		foreach (TextWidget text : m_aGuideTexts)
		{
			float width, height, textWidth, textHeight;
			text.GetScreenSize(width, height);
			text.GetTextSize(textWidth, textHeight);
			bool fits = width > 0 && height > 0 && textWidth <= GetGame().GetWorkspace().DPIUnscale(width) + 2 && textHeight <= GetGame().GetWorkspace().DPIUnscale(height) + 2;
			Print(string.Format("[AICF][UNIVERSAL_GUIDE_TEXT] width=%1 height=%2 glyph_width=%3 glyph_height=%4 fits=%5", width, height, textWidth, textHeight, fits));
			if (!fits || text.GetText().IsEmpty() || text.GetText().Contains("{AICF:")) return false;
		}
		return true;
	}
	bool AICF_GuideRestored()
	{
		return AICF_GuideButton() && GetGame().GetWorkspace().GetModal() == m_wRoot && GetGame().GetWorkspace().GetFocusedWidget() == m_wGuideButton;
	}
	bool AICF_UniversalStart() { return m_aButtons[13].IsEnabled(); }
	bool AICF_ContentVisible(int content) { return m_aFrames[content + 3].IsVisible(); }
	string AICF_UniversalSummary() { return m_wSummary.GetText(); }
	void AICF_UniversalHelp(int index) { Help(index); }
}

class AICF_UniversalScenarioProbe
{
	protected int m_iChecks;
	protected int m_iFailures;
	protected ref AICF_ScenarioSetupScreen m_Screen;
	protected Widget m_PreviousModal;
	protected ref AICF_ScenarioSetupSelection m_Selection;
	protected ref array<WorkshopItem> m_aContentItems = {};
	protected ref array<bool> m_aContentEnabled = {};
	protected ResourceName m_sGuideTarget;
	protected string m_sPreviousLanguage;

	void Check(string name, bool passed)
	{
		m_iChecks++;
		if (!passed) m_iFailures++;
		Print(string.Format("[AICF][UNIVERSAL_CASE] case=%1 pass=%2", name, passed));
	}
	void Start()
	{
		WorkshopApi workshop = GetGame().GetBackendApi().GetWorkshop();
		if (workshop.NeedAddonsScan()) workshop.ScanOfflineItems();
		GetGame().GetCallqueue().CallLater(ConfigureContent, 4000, false);
		GetGame().GetCallqueue().CallLater(Readback, 8000, false);
		GetGame().GetCallqueue().CallLater(Timeout, 90000, false);
	}
	// Только isolated fixture и свежий profile. После проверки возвращает enable state.
	protected void ConfigureContent()
	{
		string requested;
		if (!System.GetCLIParam("aicfProbeEnabledContent", requested)) return;
		AICF_ScenarioRegistry registry = new AICF_ScenarioRegistry();
		array<string> enabled = {};
		if (requested == "RHS") enabled.Copy(registry.Find(0, 1).m_aContentAddons);
		if (requested == "WCSRHS" || requested == "WCSOnly" || requested == "PartialWCS") enabled.Copy(registry.Find(0, 2).m_aContentAddons);
		if (requested == "WCSOnly")
			foreach (string rhs : registry.Find(0, 1).m_aContentAddons) enabled.RemoveItem(rhs);
		if (requested == "PartialRHS")
		{
			enabled.Copy(registry.Find(0, 1).m_aContentAddons);
			enabled.Remove(0);
		}
		if (requested == "PartialWCS") enabled.Remove(enabled.Count() - 1);
		bool configured = requested == "Vanilla" || requested == "RHS" || requested == "WCSRHS" || requested == "WCSOnly" || requested == "PartialRHS" || requested == "PartialWCS";
		WorkshopApi workshop = GetGame().GetBackendApi().GetWorkshop();
		foreach (string id : registry.Find(0, 2).m_aContentAddons)
		{
			WorkshopItem item = workshop.FindItem(id);
			if (!item || item.IsLoaded()) { configured = false; continue; }
			m_aContentItems.Insert(item);
			m_aContentEnabled.Insert(item.IsEnabled());
			item.Enable(enabled.Find(id) >= 0);
			if (item.IsEnabled() != (enabled.Find(id) >= 0)) configured = false;
		}
		Check("ENABLED_CONFIGURATION", configured);
	}
	protected void Readback()
	{
		string language;
		if (System.GetCLIParam("aicfProbeLanguage", language))
		{
			WidgetManager.GetLanguage(m_sPreviousLanguage);
			WidgetManager.SetLanguage(language);
		}
		SCR_MissionHeaderCampaign header = SCR_MissionHeaderCampaign.Cast(MissionHeader.ReadMissionHeader("{A1CF261019500001}Missions/AICF_Conflict_Arland_Setup.conf"));
		m_Selection = new AICF_ScenarioSetupSelection(header);
		AICF_ScenarioRegistry registry = m_Selection.GetRegistry();
		Check("REGISTRY", registry.GetVariants().Count() == 9);
		array<string> none = {};
		array<string> rhsEnabled = registry.Find(0, 1).m_aContentAddons;
		array<string> allEnabled = registry.Find(0, 2).m_aContentAddons;
		bool enabledMatrix = true;
		foreach (AICF_ScenarioVariant contentVariant : registry.GetVariants())
		{
			if (contentVariant.HasEnabledContent(none) != (contentVariant.m_iContent == 0)) enabledMatrix = false;
			if (contentVariant.HasEnabledContent(rhsEnabled) != (contentVariant.m_iContent < 2)) enabledMatrix = false;
			if (!contentVariant.HasEnabledContent(allEnabled)) enabledMatrix = false;
			foreach (string omitted : contentVariant.m_aContentAddons)
			{
				array<string> partial = {};
				partial.Copy(contentVariant.m_aContentAddons);
				partial.RemoveItem(omitted);
				if (contentVariant.HasEnabledContent(partial)) enabledMatrix = false;
			}
		}
		Check("ENABLED_MATRIX", enabledMatrix);
		array<string> available = {};
		GameProject.GetAvailableAddons(available);
		Print(string.Format("[AICF][UNIVERSAL_SCAN] available_count=%1 rhs_in_scan=%2", available.Count(), available.Find("1337C0DE5DABBEEF")));
		WorkshopItem rhs = GetGame().GetBackendApi().GetWorkshop().FindItem("1337C0DE5DABBEEF");
		if (rhs)
		{
			Revision revision = rhs.GetActiveRevision();
			if (!revision) revision = rhs.GetLocalRevision();
			Print(string.Format("[AICF][UNIVERSAL_RHS] local_revision=%1 ready=%2 enabled=%3 loaded=%4", revision != null, rhs.IsReadyToRun(), rhs.IsEnabled(), rhs.IsLoaded()));
			if (revision) Print(string.Format("[AICF][UNIVERSAL_RHS_REVISION] downloaded=%1 corrupted=%2 version=%3", revision.IsDownloaded(), revision.IsCorrupted(), revision.GetVersion()));
		}
		int readyCount;
		foreach (AICF_ScenarioVariant variant : registry.GetVariants())
		{
			string missing;
			bool valid = variant.HasDependencies(variant.m_aAddons, missing);
			// Every individual dependency is required, including integration addons.
			foreach (string removed : variant.m_aAddons)
			{
				array<string> incomplete = {};
				foreach (string id : variant.m_aAddons) if (id != removed) incomplete.Insert(id);
				if (variant.HasDependencies(incomplete, missing) || missing != removed) valid = false;
			}
			for (int mode; mode < 3; mode++)
				for (int difficulty; difficulty < 3; difficulty++)
					if (variant.Resolve(mode, difficulty).IsEmpty() || !registry.IsLeaf(variant.Resolve(mode, difficulty))) valid = false;
			if (!variant.Resolve(3, 0).IsEmpty() || !variant.Resolve(0, -1).IsEmpty()) valid = false;
			Check("PLAN_" + variant.m_sKey, valid);
			bool ready = variant.IsAvailable(missing);
			if (ready) readyCount++;
			Print(string.Format("[AICF][UNIVERSAL_AVAILABLE] variant=%1 available=%2 missing=%3 graph=%4", variant.m_sKey, ready, missing, variant.GetAddonList()));
		}
		string expectedReady;
		bool hasExpected = System.GetCLIParam("aicfExpectedReadyVariants", expectedReady);
		Check("EXPECTED_AVAILABLE", hasExpected && readyCount == expectedReady.ToInt());
		string absent;
		Check("VANILLA_READY", registry.Find(0, 0).IsAvailable(absent));
		Check("BASE_SOURCE_COMPATIBLE", AICF_ScenarioLaunch.IsForcedGraphCompatible(registry.Find(0, 0)));
		ResourceName before = m_Selection.GetResource();
		Check("ATOMIC_INVALID", !m_Selection.SelectVariant(99, 0) && !m_Selection.SelectVariant(0, 99) && m_Selection.GetResource() == before);
		array<ref DSMod> mods = registry.Find(0, 0).BuildHostMods();
		Check("HOST_GRAPH", mods.Count() == 2 && mods[0].modId == "9178E5822AFE48EA" && mods[1].modId == "B52C5F6AEDBF423E");
		Check("NULL_ENTRY_BLOCKED", !AICF_ScenarioLaunch.CanStart(m_Selection, null));
		array<string> forced = {"9178E5822AFE48EA", "B52C5F6AEDBF423E"};
		bool acceptsBase = AICF_ScenarioLaunch.SupportsForcedAddons(registry.Find(0, 0), forced);
		forced.Insert("A1CF260928100001");
		Check("FORCED_GRAPH", acceptsBase && !AICF_ScenarioLaunch.SupportsForcedAddons(registry.Find(0, 0), forced) && AICF_ScenarioLaunch.SupportsForcedAddons(registry.Find(0, 2), forced));
		m_PreviousModal = GetGame().GetWorkspace().GetModal();
		m_Screen = new AICF_ScenarioSetupScreen();
		Check("UI_OPEN", m_Screen.Open(header));
		GetGame().GetCallqueue().CallLater(Guide, 1000, false);
	}
	protected void Guide()
	{
		m_sGuideTarget = m_Selection.GetResource();
		Check("UI_GUIDE_BUTTON", m_Screen.AICF_GuideButton());
		Check("UI_GUIDE_HIT_TARGET", m_Screen.AICF_GuideHitTarget());
		Check("UI_GUIDE_OPEN", m_Screen.OpenGuide() && m_Screen.OpenGuide() && m_Screen.AICF_GuideModal());
		GetGame().GetCallqueue().CallLater(GuideReadback, 500, false);
	}
	protected void GuideReadback()
	{
		string requested, actual;
		System.GetCLIParam("aicfProbeLanguage", requested);
		WidgetManager.GetLanguage(actual);
		Print(string.Format("[AICF][UNIVERSAL_GUIDE_LANGUAGE] requested=%1 actual=%2 label=%3", requested, actual, AICF_Localization.Resolve("{AICF:AICF_Setup_GuideButton}")));
		Check("UI_GUIDE_LAYOUT", m_Screen.AICF_GuideLayout() && (requested.IsEmpty() || requested == actual));
		Check("UI_GUIDE_HIT_CLOSE", m_Screen.AICF_GuideCloseHitTarget());
		m_Screen.CloseGuide();
		m_Screen.CloseGuide();
		Check("UI_GUIDE_RESTORE", m_Screen.AICF_GuideRestored() && m_Selection.GetResource() == m_sGuideTarget && m_Selection.GetEnvironment().m_iStartMinutes == 480);
		Check("UI_GUIDE_REOPEN", m_Screen.OpenGuide() && m_Screen.AICF_GuideModal());
		m_Screen.CloseGuide();
		GetGame().GetCallqueue().CallLater(Widgets, 100, false);
	}
	protected void Widgets()
	{
		float width, height;
		Widget root = m_Screen.GetRoot();
		if (root) root.GetScreenSize(width, height);
		Check("UI_BOUNDS", root && width > 600 && height > 400);
		Check("UI_MODAL", root && GetGame().GetWorkspace().GetModal() == root);
		Check("UI_FLAGS_HELP", m_Screen.AICF_UniversalWidgets());
		Check("UI_MAP_PREVIEWS", m_Screen.AICF_MapPreviews());
		Check("UI_CARD_IDENTITY", m_Screen.AICF_MapCardIdentity());
		Check("UI_BRIEFING_LAYOUT", m_Screen.AICF_BriefingLayout());
		Check("UI_TEXT_PALETTE", m_Screen.AICF_TextPalette());
		Check("UI_START_ENABLED", m_Screen.AICF_UniversalStart());
		string expectedMask;
		bool hasMask = System.GetCLIParam("aicfExpectedContentMask", expectedMask);
		int actualMask;
		for (int visibleContent; visibleContent < 3; visibleContent++)
			if (m_Screen.AICF_ContentVisible(visibleContent)) actualMask = actualMask | (1 << visibleContent);
		Check("UI_CONTENT_MASK", hasMask && actualMask == expectedMask.ToInt());
		Print(string.Format("[AICF][UNIVERSAL_CONTENT] expected=%1 visible=%2", expectedMask, actualMask));
		bool environmentValid = m_Screen.GetSelection().GetEnvironment().m_iStartMinutes == 480 && m_Screen.GetSelection().GetEnvironment().m_iWeather == 0;
		for (int time; time <= 1440; time++)
			for (int weather; weather < 5; weather++)
			{
				AICF_ScenarioSetupSelection source = m_Screen.GetSelection();
				if (!source.SelectEnvironment(time, weather) || source.GetEnvironment().m_iStartMinutes != time || source.GetEnvironment().m_iWeather != weather) environmentValid = false;
				if (time < 1440 && source.GetEnvironment().GetHour() * 60 + source.GetEnvironment().GetMinute() != time) environmentValid = false;
			}
		m_Screen.SelectEnvironment(817, 4);
		if (m_Screen.SelectEnvironment(-1, 0) || m_Screen.SelectEnvironment(1441, 0) || m_Screen.SelectEnvironment(0, 5) || m_Screen.GetSelection().GetEnvironment().m_iStartMinutes != 817 || m_Screen.GetSelection().GetEnvironment().m_iWeather != 4) environmentValid = false;
		Check("UI_ENVIRONMENT", environmentValid && !m_Screen.AICF_UniversalSummary().Contains("{AICF:"));
		bool sliderValid = m_Screen.AICF_TimeSliderReadback(817, "13:37") && m_Screen.AICF_UniversalSummary().Contains("13:37");
		m_Screen.SelectEnvironment(0, 4);
		sliderValid = sliderValid && m_Screen.AICF_TimeSliderReadback(0, "00:00");
		m_Screen.SelectEnvironment(1439, 4);
		sliderValid = sliderValid && m_Screen.AICF_TimeSliderReadback(1439, "23:59");
		m_Screen.SelectEnvironment(817, 4);
		Check("UI_NO_RANDOM_TIME", !m_Screen.SelectEnvironment(AICF_ScenarioEnvironment.RANDOM_TIME, 4) && m_Screen.AICF_TimeSliderReadback(817, "13:37"));
		Check("UI_TIME_SLIDER", sliderValid && m_Screen.AICF_TimeSliderReadback(817, "13:37"));
		ResourceName target = m_Selection.GetResource();
		AICF_ScenarioEnvironment settings = m_Screen.GetSelection().GetEnvironment();
		bool queued = AICF_ScenarioEnvironment.Queue(target, settings);
		AICF_ScenarioEnvironment consumed = AICF_ScenarioEnvironment.Consume(target);
		bool roundTrip = queued && consumed && consumed.m_iStartMinutes == 817 && consumed.m_iWeather == 4 && !AICF_ScenarioEnvironment.Consume(target);
		AICF_ScenarioEnvironment.Queue(target, settings);
		bool wrongTarget = !AICF_ScenarioEnvironment.Consume("unknown") && !AICF_ScenarioEnvironment.Consume(target);
		GameSessionStorage.s_Data[AICF_ScenarioEnvironment.SESSION_KEY] = "{}";
		bool malformed = !AICF_ScenarioEnvironment.Consume(target);
		JsonSaveContext legacy = new JsonSaveContext();
		legacy.WriteValue("schema", 1);
		legacy.WriteValue("target", target);
		legacy.WriteValue("issued", System.GetTickCount());
		legacy.WriteValue("minutes", 817);
		legacy.WriteValue("weather", 4);
		GameSessionStorage.s_Data[AICF_ScenarioEnvironment.SESSION_KEY] = legacy.SaveToString();
		Check("ENVIRONMENT_STORAGE", roundTrip && wrongTarget && malformed && !AICF_ScenarioEnvironment.Consume(target));
		for (int mode; mode < 3; mode++)
		{
			for (int difficulty; difficulty < 3; difficulty++)
			{
				bool selected = m_Screen.Select(mode, difficulty);
				Check(string.Format("UI_%1_%2", mode, difficulty), selected && m_Screen.AICF_UniversalSides() == (mode > 0) && !m_Screen.AICF_UniversalSummary().IsEmpty() && !m_Screen.AICF_UniversalSummary().Contains("{AICF:"));
			}
		}
		bool allHelp = true;
		for (int index = -1; index < 20; index++)
		{
			m_Screen.AICF_UniversalHelp(index);
			if (!m_Screen.AICF_UniversalWidgets()) allHelp = false;
		}
		Check("UI_ALL_HELP", allHelp);
		foreach (AICF_ScenarioVariant variant : m_Selection.GetRegistry().GetVariants())
		{
			string missing;
			bool available = variant.IsContentEnabled() && variant.IsAvailable(missing) && AICF_ScenarioLaunch.IsForcedGraphCompatible(variant);
			bool selected = m_Screen.SelectVariant(variant.m_iMap, variant.m_iContent);
			Check("UI_VARIANT_" + variant.m_sKey, selected == available && m_Screen.AICF_UniversalWidgets() && m_Screen.AICF_ContentVisible(variant.m_iContent) == variant.IsContentEnabled());
		}
		m_Screen.OpenGuide();
		m_Screen.Close();
		Check("UI_CLOSED", !m_Screen.GetRoot() && GetGame().GetWorkspace().GetModal() == m_PreviousModal);
		m_Screen.Close();
		Check("UI_IDEMPOTENT", !m_Screen.GetRoot());
		Print(string.Format("[AICF][UNIVERSAL_DONE] checks=%1 failed=%2", m_iChecks, m_iFailures));
		GetGame().GetCallqueue().CallLater(Close, 1000, false);
	}
	protected void Timeout() { Print("[AICF][UNIVERSAL_TIMEOUT]"); Close(); }
	protected void Close() { GetGame().RequestClose(); }
	void Stop()
	{
		foreach (int index, WorkshopItem item : m_aContentItems) item.Enable(m_aContentEnabled[index]);
		m_aContentItems.Clear();
		m_aContentEnabled.Clear();
		GetGame().GetCallqueue().Remove(ConfigureContent);
		if (m_Screen) m_Screen.Close();
		m_Screen = null;
		if (!m_sPreviousLanguage.IsEmpty()) WidgetManager.SetLanguage(m_sPreviousLanguage);
		GetGame().GetCallqueue().Remove(Readback);
		GetGame().GetCallqueue().Remove(Guide);
		GetGame().GetCallqueue().Remove(GuideReadback);
		GetGame().GetCallqueue().Remove(Widgets);
		GetGame().GetCallqueue().Remove(Close);
		GetGame().GetCallqueue().Remove(Timeout);
	}
}

modded class ArmaReforgerScripted
{
	protected ref AICF_UniversalScenarioProbe m_AICFUniversalProbe;
	override bool OnGameStart()
	{
		bool started = super.OnGameStart();
		if (System.IsCLIParam("aicfUniversalProbe"))
		{
			m_AICFUniversalProbe = new AICF_UniversalScenarioProbe();
			m_AICFUniversalProbe.Start();
		}
		return started;
	}
	override void OnGameEnd()
	{
		if (m_AICFUniversalProbe) m_AICFUniversalProbe.Stop();
		m_AICFUniversalProbe = null;
		super.OnGameEnd();
	}
}
