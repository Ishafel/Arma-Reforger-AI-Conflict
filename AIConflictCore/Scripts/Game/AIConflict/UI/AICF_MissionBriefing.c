// Local presentation owned by the native map journal. No world/resource overrides,
// network writes, subscriptions or dependency on roster readiness (including JIP).
class AICF_MissionBriefing
{
	static bool SupportsHeader(SCR_MissionHeaderCampaign header)
	{
		return header && header.m_sGameMode == "AI Conflict";
	}

	static SCR_JournalSetupConfig Create(SCR_MissionHeaderCampaign header)
	{
		if (!SupportsHeader(header))
			return null;
		return new AICF_MissionJournal(header);
	}
}

class AICF_MissionJournal : SCR_JournalSetupConfig
{
	void AICF_MissionJournal(SCR_MissionHeaderCampaign header)
	{
		m_iJournalEntryToShowOnStart = 0;
		m_aJournals = {};
		m_aJournals.Insert(new AICF_MissionJournalPages(header));
	}
}

class AICF_MissionJournalPages : SCR_JournalConfig
{
	void AICF_MissionJournalPages(SCR_MissionHeaderCampaign header)
	{
		// Empty faction is the native fallback: also works before faction selection
		// and for content profiles with different concrete faction keys.
		m_FactionKey = FactionKey.Empty;
		m_aEntries = {};
		Add("Overview", "Overview", header);
		if (header.m_eAICFMatchMode == AICF_EMatchMode.STANDARD)
			Add("Rules", "RulesStandard");
		else
			Add("Rules", "RulesCoop");
		Add("Command", "Command");
		Add("Recruitment", "Recruitment");
		Add("Equipment", "Equipment");
		Add("Logistics", "Logistics");
		Add("Reinforcement", "Reinforcement");
		Add("Construction", "Construction");
		Add("Enemy", "Enemy");
		Add("Settings", "Settings");
	}

	protected void Add(string title, string body, SCR_MissionHeaderCampaign header = null)
	{
		// Keep IDs outside stock briefing updates; display order uses array indices.
		m_aEntries.Insert(new AICF_MissionJournalEntry(10000 + m_aEntries.Count(), title, body, header));
	}
}

class AICF_MissionJournalEntry : SCR_JournalEntry
{
	protected SCR_MissionHeaderCampaign m_AICFHeader;

	void AICF_MissionJournalEntry(int id, string title, string body, SCR_MissionHeaderCampaign header)
	{
		m_iEntryID = id;
		m_eJournalEntryType = SCR_EJournalEntryType.Custom;
		m_sEntryButtonLayout = "{3EAF5389D3D89EE5}UI/layouts/Menus/DeployMenu/JournalButton.layout";
		m_sCustomEntryName = "#AICF_Briefing_Title_" + title;
		m_sEntryText = "#AICF_Briefing_Text_" + body;
		m_AICFHeader = header;
	}

	override Widget SetEntryLayoutTo(out notnull Widget parent)
	{
		// Resolve on display, so changing language does not cache an old translation.
		if (m_AICFHeader)
		{
			string mode = "Conflict";
			if (m_AICFHeader.m_eAICFMatchMode != AICF_EMatchMode.STANDARD) mode = "Co-op";
			string difficulty = "#AICF_Setup_Easy";
			if (m_AICFHeader.m_eAICFDifficulty == AICF_EDifficulty.MEDIUM) difficulty = "#AICF_Setup_Medium";
			else if (m_AICFHeader.m_eAICFDifficulty == AICF_EDifficulty.HARD) difficulty = "#AICF_Setup_Hard";
			m_sEntryTextParam1 = WidgetManager.Translate(m_AICFHeader.m_sName) + "<br/>" + mode + " · " + WidgetManager.Translate(difficulty);
		}
		return super.SetEntryLayoutTo(parent);
	}
}

modded class SCR_RespawnBriefingComponent
{
	override bool LoadJournalConfig()
	{
		SCR_MissionHeaderCampaign header = SCR_MissionHeaderCampaign.Cast(GetGame().GetMissionHeader());
		if (!SCR_GameModeCampaign.Cast(GetGame().GetGameMode()) || !AICF_MissionBriefing.SupportsHeader(header))
			return super.LoadJournalConfig();

		if (!m_JournalConfig)
			m_JournalConfig = AICF_MissionBriefing.Create(header);
		return m_JournalConfig != null;
	}
}
