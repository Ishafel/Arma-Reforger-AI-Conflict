// Только terminal fixture: временно копировать в Core, включать aicfCallsignProbe.
// Не меняет production roster; проверка replacement использует отдельный slot.
modded class AICF_MatchController
{
	protected bool m_bAICFCallsignProbeDone;
	protected int m_iAICFCallsignProbeCases;
	protected int m_iAICFCallsignProbeFailures;

	override protected void Update()
	{
		super.Update();
		string flag;
		if (!System.GetCLIParam("aicfCallsignProbe", flag) || flag != "1" ||
			!m_bRosterReady || m_bStopped || m_bAICFCallsignProbeDone)
			return;
		m_bAICFCallsignProbeDone = true;
		AICF_CheckCallsignFaction(m_USState, "US");
		AICF_CheckCallsignFaction(m_USSRState, "USSR");
		AICF_CheckCallsignModels();
		Print(string.Format("[AICF][CALLSIGN_PROBE_FINISHED] cases=%1 failures=%2",
			m_iAICFCallsignProbeCases, m_iAICFCallsignProbeFailures));
		GetGame().RequestClose();
	}

	protected void AICF_CallSignCheck(bool passed, string name)
	{
		m_iAICFCallsignProbeCases++;
		if (!passed) m_iAICFCallsignProbeFailures++;
		Print(string.Format("[AICF][CALLSIGN_PROBE] case=%1 passed=%2", name, passed));
	}

	protected void AICF_CheckCallsignFaction(AICF_FactionState state, string side)
	{
		array<string> seen = {};
		AICF_CallSignCheck(state.GetSlotCount() == 10, side + "_ten_slots");
		for (int i; i < state.GetSlotCount(); i++)
		{
			AICF_GroupSlot slot = state.GetSlot(i);
			string id = slot.GetCallsignId();
			AICF_CallSignCheck(id.StartsWith("AICF_Callsign_" + side + "_") && !seen.Contains(id), side + "_unique_pool");
			seen.Insert(id);
			string name = slot.GetDisplayName();
			AICF_CallSignCheck(name == "{AICF:" + id + "}-" + (i + 1).ToString(), side + "_number");
			SCR_AIGroup group = slot.GetGroup();
			AICF_CallSignCheck(group && group.AICF_GetCallsign() == name, side + "_entity");
			array<string> fields = {};
			BuildGroupSummary(state, i).Split("|", fields, false);
			AICF_CallSignCheck(fields.Count() == 14 && fields[0] == name, side + "_summary");
			if (!group) continue;
			string resolved = AICF_Localization.Resolve(name);
			AICF_CallSignCheck(group.GetCustomName() == resolved && group.GetCustomNameWithOriginal() == resolved, side + "_native_name");
			string company, platoon, squad, character, format;
			group.GetCallsigns(company, platoon, squad, character, format);
			AICF_CallSignCheck(string.Format(format, company, platoon, squad, character) == resolved, side + "_native_callsign");
			Print(string.Format("[AICF][CALLSIGN_PROBE_ASSIGNMENT] side=%1 slot=%2 id=%3 name=%4", side, i, id, resolved));
		}
	}

	protected void AICF_CheckCallsignModels()
	{
		array<string> sides = {"US", "USSR"};
		foreach (string side : sides)
		{
			array<string> pool = {};
			AICF_GroupCallsigns.BuildShuffledPool(side, pool);
			array<string> seen = {};
			foreach (string id : pool)
			{
				AICF_CallSignCheck(!seen.Contains(id) && id.StartsWith("AICF_Callsign_" + side + "_"), side + "_full_pool_unique");
				seen.Insert(id);
				string translated = AICF_Localization.Resolve("{AICF:" + id + "}");
				AICF_CallSignCheck(!translated.IsEmpty() && !translated.Contains("AICF"), side + "_translation_exists");
			}
			AICF_CallSignCheck(seen.Count() == 50, side + "_pool_size");
			bool different;
			for (int attempt; attempt < 5; attempt++)
			{
				array<string> next = {};
				AICF_GroupCallsigns.BuildShuffledPool(side, next);
				for (int index; index < 10; index++)
					if (next[index] != pool[index]) different = true;
			}
			AICF_CallSignCheck(different, side + "_fresh_shuffle");
		}
		AICF_GroupSlot testSlot = new AICF_GroupSlot(3, AICF_EGroupRole.ATTACK, 0, "AICF_Callsign_USSR_Dozor");
		string saved = testSlot.GetDisplayName();
		testSlot.SetRoleAndIndex(AICF_EGroupRole.DEFEND, 2);
		AICF_CallSignCheck(testSlot.GetDisplayName() == saved, "role_stable");
		AICF_CallSignCheck(testSlot.BeginInitialSpawn() && testSlot.MarkDestroyed() &&
			testSlot.BeginReinforcementWait(0) && testSlot.BeginReplacementSpawn(System.GetTickCount()), "replacement_lifecycle");
		AICF_CallSignCheck(testSlot.GetDisplayName() == saved && testSlot.GetCallsignId() == "AICF_Callsign_USSR_Dozor", "replacement_stable");
		string language;
		WidgetManager.GetLanguage(language);
		WidgetManager.SetLanguage("ru_ru");
		AICF_CallSignCheck(AICF_Localization.Resolve(saved) == "Дозор-4", "ru_name");
		AICF_CallSignCheck(AICF_Localization.Resolve("{AICF:AICF_Callsign_US_Tombstone}-7") == "Tombstone-7", "us_ru");
		WidgetManager.SetLanguage("en_us");
		AICF_CallSignCheck(AICF_Localization.Resolve(saved) == "Dozor-4", "en_name");
		AICF_CallSignCheck(AICF_Localization.Resolve("{AICF:AICF_Callsign_US_Tombstone}-7") == "Tombstone-7", "us_en");
		AICF_CallSignCheck(testSlot.GetDisplayName() == saved, "language_stable_id");
		WidgetManager.SetLanguage(language);
	}
}
