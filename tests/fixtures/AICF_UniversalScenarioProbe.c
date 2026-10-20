// Isolated stage only. Native catalog/widgets/plans; no mouse/keyboard automation.
modded class AICF_ScenarioSetupScreen
{
	bool AICF_UniversalWidgets()
	{
		return m_aButtons.Count() == 21 && m_aFlags.Count() == 2 && m_iFlagFailures == 0 && m_wHelp && !m_wHelp.GetText().IsEmpty() && !m_wHelp.GetText().Contains("{AICF:");
	}
	bool AICF_TimeSliderReadback(int minutes, string label)
	{
		return m_wTimeSlider && m_wTimeSlider.GetMin() == 0 && m_wTimeSlider.GetMax() == 1439 && m_wTimeSlider.GetStep() == 1 && !m_wTimeSlider.IsVertical() && m_wTimeSlider.GetCurrent() == minutes && m_wTimeValue.GetText() == label;
	}
	bool AICF_UniversalSides() { return m_aButtons[8].IsEnabled() && m_aButtons[9].IsEnabled(); }
	bool AICF_UniversalStart() { return m_aButtons[13].IsEnabled(); }
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
		GetGame().GetCallqueue().CallLater(Readback, 8000, false);
		GetGame().GetCallqueue().CallLater(Timeout, 90000, false);
	}
	protected void Readback()
	{
		AICF_ScenarioSetupHeader header = AICF_ScenarioSetupHeader.Cast(MissionHeader.ReadMissionHeader("{A1CF261019500001}Missions/AICF_Conflict_Arland_Setup.conf"));
		m_Selection = new AICF_ScenarioSetupSelection(header);
		AICF_ScenarioRegistry registry = m_Selection.GetRegistry();
		Check("REGISTRY", registry.GetVariants().Count() == 9);
		array<string> available = {};
		GameProject.GetAvailableAddons(available);
		Print(string.Format("[AICF][UNIVERSAL_SCAN] available_count=%1 rhs_in_scan=%2", available.Count(), available.Find("1337C0DE5DABBEEF")));
		WorkshopItem rhs = GetGame().GetBackendApi().GetWorkshop().FindItem("1337C0DE5DABBEEF");
		if (rhs)
		{
			Revision revision = rhs.GetLocalRevision();
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
		GetGame().GetCallqueue().CallLater(Widgets, 1000, false);
	}
	protected void Widgets()
	{
		float width, height;
		Widget root = m_Screen.GetRoot();
		if (root) root.GetScreenSize(width, height);
		Check("UI_BOUNDS", root && width > 600 && height > 400);
		Check("UI_MODAL", root && GetGame().GetWorkspace().GetModal() == root);
		Check("UI_FLAGS_HELP", m_Screen.AICF_UniversalWidgets());
		Check("UI_START_ENABLED", m_Screen.AICF_UniversalStart());
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
		m_Screen.SelectEnvironment(AICF_ScenarioEnvironment.RANDOM_TIME, 4);
		sliderValid = sliderValid && m_Screen.AICF_TimeSliderReadback(817, AICF_Localization.Resolve("{AICF:AICF_Setup_Time4}"));
		m_Screen.SelectEnvironment(817, 4);
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
		for (int index; index < 21; index++)
		{
			m_Screen.AICF_UniversalHelp(index);
			if (!m_Screen.AICF_UniversalWidgets()) allHelp = false;
		}
		Check("UI_ALL_HELP", allHelp);
		foreach (AICF_ScenarioVariant variant : m_Selection.GetRegistry().GetVariants())
		{
			string missing;
			bool available = variant.IsAvailable(missing) && AICF_ScenarioLaunch.IsForcedGraphCompatible(variant);
			bool selected = m_Screen.SelectVariant(variant.m_iMap, variant.m_iContent);
			Check("UI_VARIANT_" + variant.m_sKey, selected == available && m_Screen.AICF_UniversalWidgets());
		}
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
		if (m_Screen) m_Screen.Close();
		m_Screen = null;
		GetGame().GetCallqueue().Remove(Readback);
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
