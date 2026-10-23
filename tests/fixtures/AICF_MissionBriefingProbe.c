// Isolated terminal server/direct-client stage only. Native journal data and
// layout readbacks; no synthetic input, world edits or player interactions.
modded class SCR_GameModeCampaign
{
	protected int m_iAICFBriefingChecks;
	protected int m_iAICFBriefingFailures;

	override void OnGameStart()
	{
		super.OnGameStart();
		if (!System.IsCLIParam("aicfBriefingProbe")) return;
		GetGame().GetCallqueue().CallLater(AICF_BriefingProbeRun, 15000, false);
	}

	protected void AICF_BriefingCheck(string name, bool passed)
	{
		m_iAICFBriefingChecks++;
		if (!passed) m_iAICFBriefingFailures++;
		Print(string.Format("[AICF][BRIEFING_CASE] server=%1 case=%2 pass=%3", Replication.IsServer(), name, passed));
	}

	protected void AICF_BriefingProbeRun()
	{
		SCR_RespawnBriefingComponent component = SCR_RespawnBriefingComponent.GetInstance();
		AICF_BriefingCheck("NATIVE_COMPONENT", component != null);
		if (component)
		{
			SCR_JournalSetupConfig config = component.GetJournalSetup();
			AICF_BriefingCheck("ACTIVE_JOURNAL", AICF_MissionJournal.Cast(config) != null);
			AICF_BriefingCheck("REPEATED_LOAD", component.LoadJournalConfig() && component.GetJournalSetup() == config);
			AICF_BriefingPages(config, "ACTIVE");
			component.ResetConfig();
			AICF_BriefingCheck("RESET_REBUILDS", component.LoadJournalConfig() && AICF_MissionJournal.Cast(component.GetJournalSetup()) != null);
			AICF_BriefingPages(component.GetJournalSetup(), "RESET");
		}
		AICF_BriefingCheck("NULL_HEADER", !AICF_MissionBriefing.SupportsHeader(null) && !AICF_MissionBriefing.Create(null));
		SCR_MissionHeaderCampaign stock = SCR_MissionHeaderCampaign.Cast(MissionHeader.ReadMissionHeader("{C41618FD18E9D714}Missions/23_Campaign_Arland.conf"));
		AICF_BriefingCheck("STOCK_UNCHANGED", stock && !AICF_MissionBriefing.Create(stock));
		AICF_BriefingHeaders();
		if (!Replication.IsServer()) AICF_BriefingWidgets(component);
		Print(string.Format("[AICF][BRIEFING_DONE] server=%1 checks=%2 failed=%3", Replication.IsServer(), m_iAICFBriefingChecks, m_iAICFBriefingFailures));
		int delay = 1000;
		if (Replication.IsServer()) delay = 150000;
		GetGame().GetCallqueue().CallLater(AICF_BriefingProbeClose, delay, false);
	}

	protected void AICF_BriefingPages(SCR_JournalSetupConfig config, string prefix)
	{
		if (!config) return;
		SCR_JournalConfig journal = config.GetJournalConfig();
		AICF_BriefingCheck(prefix + "_ALL_FACTIONS", journal && config.GetJournalConfig("US") == journal && config.GetJournalConfig("USSR") == journal && config.GetJournalConfig("content_faction") == journal);
		if (!journal) return;
		array<ref SCR_JournalEntry> entries = journal.GetEntries();
		AICF_BriefingCheck(prefix + "_PAGES", entries && entries.Count() == 10 && config.GetJournalEntryToBeShown() == 0);
		if (!entries || entries.Count() != 10) return;
		bool standard = AICF_MatchRules.FromMissionHeader().GetMode() == AICF_EMatchMode.STANDARD;
		string expected = "#AICF_Briefing_Text_RulesCoop";
		if (standard) expected = "#AICF_Briefing_Text_RulesStandard";
		AICF_BriefingCheck(prefix + "_MODE", entries[1].GetEntryText() == expected);
		array<int> ids = {};
		foreach (SCR_JournalEntry entry : entries)
		{
			AICF_BriefingCheck(prefix + "_UNIQUE_" + entry.GetEntryID(), entry.GetEntryID() >= 10000 && ids.Find(entry.GetEntryID()) < 0);
			ids.Insert(entry.GetEntryID());
		}
	}

	protected void AICF_BriefingWidgets(SCR_RespawnBriefingComponent component)
	{
		if (!component) return;
		SCR_JournalSetupConfig config = component.GetJournalSetup();
		if (!config || !config.GetJournalConfig()) return;
		string original;
		WidgetManager.GetLanguage(original);
		array<string> languages = {"en_us", "ru_ru", "de_de"};
		foreach (string language : languages)
		{
			WidgetManager.SetLanguage(language);
			SCR_JournalWidgets widgets = new SCR_JournalWidgets();
			Widget root = GetGame().GetWorkspace().CreateWidgets(widgets.GetLayout());
			AICF_BriefingCheck("NATIVE_LAYOUT_" + language, root && widgets.Init(root));
			if (!root) continue;
			foreach (int index, SCR_JournalEntry entry : config.GetJournalConfig().GetEntries())
			{
				Widget button = GetGame().GetWorkspace().CreateWidgets(entry.GetEntryButtonLayout(), widgets.m_wEntriesWrapper);
				SCR_MapJournalUIButton handler = SCR_MapJournalUIButton.Cast(button.FindHandler(SCR_MapJournalUIButton));
				AICF_BriefingCheck("BUTTON_" + language + index, handler != null);
				if (!handler) continue;
				handler.SetContent(entry.GetEntryName());
				handler.SetEntry(entry, index);
				handler.ShowEntry(widgets.m_wEntryLayout);
				Widget page = entry.GetWidget();
				TextWidget text;
				TextWidget title;
				if (page)
				{
					text = TextWidget.Cast(page.FindAnyWidget("Text"));
					title = TextWidget.Cast(page.FindAnyWidget("Title"));
				}
				string expected = WidgetManager.Translate(entry.GetEntryText(), "H4CK_STR");
				expected.Replace("H4CK_STR", entry.GetEntryTextParam1());
				AICF_BriefingCheck("TEXT_" + language + index, text && title && text.GetText() == expected && title.GetText() == WidgetManager.Translate(entry.GetEntryName()) && expected.Length() > 600 && !expected.Contains("#AICF_") && !expected.Contains("%1") && !expected.Contains("H4CK_STR"));
				if (page) delete page;
			}
			delete root;
		}
		WidgetManager.SetLanguage(original);
	}

	protected void AICF_BriefingHeaders()
	{
		array<string> loaded = {};
		GameProject.GetLoadedAddons(loaded);
		AICF_ScenarioRegistry registry = new AICF_ScenarioRegistry();
		int count;
		foreach (AICF_ScenarioVariant variant : registry.GetVariants())
		{
			if (loaded.Find(variant.m_sOwner) < 0) continue;
			for (int mode; mode < 3; mode++)
			{
				for (int difficulty; difficulty < 3; difficulty++)
				{
					count++;
					SCR_MissionHeaderCampaign header = SCR_MissionHeaderCampaign.Cast(MissionHeader.ReadMissionHeader(variant.Resolve(mode, difficulty)));
					SCR_JournalSetupConfig config = AICF_MissionBriefing.Create(header);
					bool passed = config && config.GetJournalConfig() && config.GetJournalConfig().GetEntries().Count() == 10;
					string expected = "#AICF_Briefing_Text_RulesCoop";
					if (mode == 0) expected = "#AICF_Briefing_Text_RulesStandard";
					if (passed) passed = config.GetJournalConfig().GetEntries()[1].GetEntryText() == expected;
					AICF_BriefingCheck(string.Format("HEADER_%1_%2_%3", variant.m_sKey, mode, difficulty), passed);
				}
			}
		}
		AICF_BriefingCheck("HEADER_COVERAGE", count >= 9);
		Print(string.Format("[AICF][BRIEFING_HEADERS] server=%1 count=%2", Replication.IsServer(), count));
	}

	protected void AICF_BriefingProbeClose() { GetGame().RequestClose(); }

	override void OnGameEnd()
	{
		GetGame().GetCallqueue().Remove(AICF_BriefingProbeRun);
		GetGame().GetCallqueue().Remove(AICF_BriefingProbeClose);
		super.OnGameEnd();
	}
}
