// Только временная runtime fixture. Меняет язык в тестовом процессе,
// восстанавливает его после проверок; production язык никогда не меняет.
modded class SCR_GameModeCampaign
{
	protected int m_iAICFLocalizationProbeFailures;
	protected int m_iAICFLocalizationProbeCases;
	protected int m_iAICFLocalizationProbeAttempts;

	override void OnGameStart()
	{
		super.OnGameStart();
		string enabled;
		if (!System.GetCLIParam("aicfLocalizationProbe", enabled) || enabled != "1") return;
		GetGame().GetCallqueue().CallLater(AICF_RunLocalizationProbe, 10000, false);
	}

	protected void AICF_CheckLocalization(string test, string actual, string expected)
	{
		m_iAICFLocalizationProbeCases++;
		bool passed = actual == expected;
		if (!passed) m_iAICFLocalizationProbeFailures++;
		Print(string.Format("[AICF][LOCALIZATION_PROBE] case=%1 passed=%2 actual=%3 expected=%4", test, passed, actual, expected));
	}

	protected void AICF_RunLocalizationProbe()
	{
		if (!AICF_HasAICommanderState() && ++m_iAICFLocalizationProbeAttempts < 30)
		{
			GetGame().GetCallqueue().CallLater(AICF_RunLocalizationProbe, 2000, false);
			return;
		}
		string originalLanguage;
		WidgetManager.GetLanguage(originalLanguage);
		string wire = AICF_Localization.Format("{AICF:AICF_UI_troops_c9179bae}", "A0", "{AICF:AICF_UI_Attack_018b2a67}", "10", "{AICF:AICF_UI_Moving_to_position_08148170}");
		string savedWire = wire;
		WidgetManager.SetLanguage("en_us");
		AICF_CheckLocalization("EN_SCENARIO", WidgetManager.Translate("#AICF_Scenario_Arland_Name"), "AI Conflict - Arland");
		AICF_CheckLocalization("EN_NESTED_MARKER", AICF_Localization.Resolve(wire), "A0 · Attack · 10 troops\nMoving to position");
		AICF_CheckLocalization("EN_BADGE", AICF_Localization.Resolve("{AICF:AICF_UI_L_1e06387a}"), "L");
		AICF_CheckLocalization("EN_STATE", AICF_Localization.Resolve(AICF_Localization.Code("AWAITING_PLAYER_COMMAND")), "Awaiting player orders");
		WidgetManager.SetLanguage("ru_ru");
		AICF_CheckLocalization("RU_SCENARIO", WidgetManager.Translate("#AICF_Scenario_Arland_Name"), "AI Conflict — Арланд");
		AICF_CheckLocalization("RU_NESTED_MARKER", AICF_Localization.Resolve(wire), "A0 · Атака · 10 чел.\nИдёт к точке");
		AICF_CheckLocalization("RU_BADGE", AICF_Localization.Resolve("{AICF:AICF_UI_L_1e06387a}"), "Л");
		AICF_CheckLocalization("RU_STATE", AICF_Localization.Resolve(AICF_Localization.Code("AWAITING_PLAYER_COMMAND")), "Ожидает приказа игрока");
		AICF_CheckLocalization("UNICODE_AND_DELIMITERS", AICF_Localization.Resolve(AICF_Localization.Format("{AICF:AICF_UI_MOVE_3b18d9be}", "Альфа}:17:|~;")), "ДВИЖЕНИЕ Альфа}:17:|~;");
		AICF_CheckLocalization("CONCAT", AICF_Localization.Resolve("{AICF:AICF_UI_Squad_daa7fec8}7"), "Отряд 7");
		AICF_CheckLocalization("STATIC_POINT", AICF_Localization.Resolve(AICF_Localization.Format("{AICF:AICF_UI_MOVE_3b18d9be}", "A0")), "ДВИЖЕНИЕ A0");
		AICF_CheckLocalization("MALFORMED_PRESERVED", AICF_Localization.Resolve("{AICF:broken:9999:x}"), "{AICF:broken:9999:x}");
		AICF_CheckLocalization("WIRE_UNCHANGED", wire, savedWire);
		WidgetManager.SetLanguage("de_de");
		AICF_CheckLocalization("FALLBACK_EN", AICF_Localization.Resolve(wire), "A0 · Attack · 10 troops\nMoving to position");
		WidgetManager.SetLanguage(originalLanguage);
		string summary = AICF_GetStrategicGroupSummary(false, 0);
		array<string> fields = {};
		summary.Split("|", fields, false);
		AICF_CheckLocalization("SUMMARY_FIELDS", fields.Count().ToString(), "14");
		if (fields.Count() == 14)
		{
			WidgetManager.SetLanguage("en_us");
			Print("[AICF][LOCALIZATION_PROBE_SUMMARY] language=en_us vehicle=" + AICF_Localization.Resolve(fields[6]) + " target=" + AICF_Localization.Resolve(fields[4]));
			WidgetManager.SetLanguage("ru_ru");
			Print("[AICF][LOCALIZATION_PROBE_SUMMARY] language=ru_ru vehicle=" + AICF_Localization.Resolve(fields[6]) + " target=" + AICF_Localization.Resolve(fields[4]));
			WidgetManager.SetLanguage(originalLanguage);
		}
		Print(string.Format("[AICF][LOCALIZATION_PROBE_FINISHED] server=%1 cases=%2 failures=%3", Replication.IsServer(), m_iAICFLocalizationProbeCases, m_iAICFLocalizationProbeFailures));
		if (Replication.IsServer()) GetGame().GetCallqueue().CallLater(AICF_CloseLocalizationProbe, 180000, false);
		else AICF_CloseLocalizationProbe();
	}

	protected void AICF_CloseLocalizationProbe()
	{
		GetGame().RequestClose();
	}

	void ~SCR_GameModeCampaign()
	{
		if (!GetGame()) return;
		GetGame().GetCallqueue().Remove(AICF_RunLocalizationProbe);
		GetGame().GetCallqueue().Remove(AICF_CloseLocalizationProbe);
	}
}
