// Stage-only. No widgets/input are created. An isolated slot has no coordinator,
// spawn request or economy transaction; native Decode/summary/formatter are real.
class AICF_VehiclePresetSummaryUIProbe : AICF_StrategicUIController
{
	string Read(string summary) { return FormatGroupSummary(summary); }
}

modded class AICF_MatchController
{
	protected bool m_bPresetSummaryProbeDone;
	protected int m_iPresetSummaryChecks;
	protected int m_iPresetSummaryFailures;

	void AICF_PresetSummaryCheck(string name, bool passed)
	{
		m_iPresetSummaryChecks++;
		if (!passed) m_iPresetSummaryFailures++;
		Print(string.Format("[AICF][PRESET_SUMMARY_CASE] case=%1 passed=%2", name, passed));
	}

	override protected void Update()
	{
		super.Update();
		if (!System.IsCLIParam("aicfVehiclePresetSummaryProbe") || m_bPresetSummaryProbeDone || !m_bRosterReady || m_bStopped) return;
		m_bPresetSummaryProbeDone = true;
		AICF_RunPresetSummaryProbe();
		GetGame().GetCallqueue().CallLater(AICF_ClosePresetSummaryProbe, 5000, false);
	}

	protected void AICF_RunPresetSummaryProbe()
	{
		SCR_CampaignFaction faction = m_USFaction;
		if (m_Config.GetMatchRules().GetManagedSlotCount(faction.GetFactionKey()) == 0) faction = m_USSRFaction;
		AICF_FactionState isolated = new AICF_FactionState(faction.GetFactionKey(), m_Config);
		AICF_GroupSlot slot = isolated.GetSlot(0);
		AICF_PresetSummaryCheck("ISOLATED_SLOT", slot && !slot.GetGroup() && !slot.HasVehiclePreset());
		if (!slot) return;
		AICF_VehiclePresetCatalog catalog = new AICF_VehiclePresetCatalog(faction);
		array<string> suffixes = {""};
		array<ResourceName> candidates = {};
		catalog.OwnCandidates(suffixes, candidates);
		catalog.Clear();
		AICF_PresetSummaryCheck("OWN_CATALOG", !candidates.IsEmpty());
		if (candidates.IsEmpty()) return;
		AICF_VehiclePreset preset = new AICF_VehiclePreset();
		preset.m_sProfile = m_ContentProfile.GetProfileKey();
		preset.m_sFaction = m_ContentProfile.GetStableFactionKey(faction.GetFactionKey());
		preset.m_sPrefab = candidates[0];
		preset.m_sName = "Броня %7C|~;{AICF:literal} #AICF_UI_CommandVehiclePreset";
		preset.m_aSeats.Insert("0:0:0");
		preset.m_aMembers.Insert(0);
		AICF_PresetSummaryCheck("ASSIGN_BEFORE_PURCHASE", slot.SetVehiclePreset(preset, 0) && !slot.GetGroup() && slot.GetVehiclePresetRevision() == 1);
		string wire = BuildGroupSummary(isolated, 0);
		array<string> fields = {};
		wire.Split("|", fields, false);
		AICF_PresetSummaryCheck("WIRE15", fields.Count() == 15);
		if (fields.Count() != 15) return;
		AICF_PresetSummaryCheck("ASSIGNED_LITERAL", AICF_VehiclePresetSummary.DecodeName(fields[14]) == preset.m_sName);
		AICF_VehiclePresetSummaryUIProbe ui = new AICF_VehiclePresetSummaryUIProbe();
		string originalLanguage;
		WidgetManager.GetLanguage(originalLanguage);
		array<string> languages = {"en_us", "ru_ru"};
		foreach (string language : languages)
		{
			WidgetManager.SetLanguage(language);
			string text = ui.Read(wire);
			string label = AICF_Localization.Resolve("{AICF:AICF_UI_CommandVehiclePreset}");
			AICF_PresetSummaryCheck(language + "_LITERAL", text.Contains("\n" + label + ": " + preset.m_sName));
			AICF_PresetSummaryCheck(language + "_NO_DOUBLE_DECODE", text.Contains("%7C|~;{AICF:literal}"));
			string legacy;
			for (int index; index < 14; index++)
			{
				if (index > 0) legacy += "|";
				legacy += fields[index];
			}
			AICF_PresetSummaryCheck(language + "_LEGACY14", !ui.Read(legacy).Contains("\n" + label + ": "));
		}
		WidgetManager.SetLanguage(originalLanguage);
		AICF_PresetSummaryCheck("RESET", slot.SetVehiclePreset(null, 1) && AICF_VehiclePresetSummary.Field(slot).IsEmpty());
		string resetWire = BuildGroupSummary(isolated, 0);
		array<string> resetFields = {};
		resetWire.Split("|", resetFields, false);
		AICF_PresetSummaryCheck("RESET_WIRE", resetFields.Count() == 15 && resetFields[14].IsEmpty());
		AICF_PresetSummaryCheck("RESET_RENDER", !ui.Read(resetWire).Contains(preset.m_sName));
		Print(string.Format("[AICF][PRESET_SUMMARY_DONE] checks=%1 failures=%2 native_json=1 native_localization=1 actual_formatter=1 isolated_slot=1 no_purchase=1", m_iPresetSummaryChecks, m_iPresetSummaryFailures));
	}

	void AICF_ClosePresetSummaryProbe() { GetGame().RequestClose(); }

	override protected void Stop(bool cleanupEntities)
	{
		GetGame().GetCallqueue().Remove(AICF_ClosePresetSummaryProbe);
		super.Stop(cleanupEntities);
	}
}
