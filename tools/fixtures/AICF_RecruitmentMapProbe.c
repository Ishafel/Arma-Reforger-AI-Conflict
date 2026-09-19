// Только отдельный stage: synthetic slot и реальные faction/base identities.
// Не меняет приказы, ownership или состав игрового roster.
class AICF_RecruitmentMapProbeSlot : AICF_GroupSlot
{
	bool m_bProbeRecruiting;
	SCR_CampaignMilitaryBaseComponent m_ProbeTarget;
	override bool IsCombatReady() { return true; }
	override bool IsRecruitingInfantry() { return m_bProbeRecruiting; }
	override SCR_CampaignMilitaryBaseComponent GetTargetBase() { return m_ProbeTarget; }
}

class AICF_RecruitmentMapProbeSystem : AICF_GroupMapMarkerSystem
{
	int m_iChecks;
	int m_iFailures;
	void Check(string name, bool passed)
	{
		m_iChecks++;
		if (!passed) m_iFailures++;
		Print(string.Format("[AICF][RECRUITMENT_MAP_CASE] name=%1 passed=%2", name, passed));
	}
	void Run(SCR_GameModeCampaign campaign)
	{
		SCR_CampaignFaction west = campaign.GetFactionByEnum(SCR_ECampaignFaction.BLUFOR);
		SCR_CampaignFaction east = campaign.GetFactionByEnum(SCR_ECampaignFaction.OPFOR);
		Check("FACTIONS_READY", west && east && west.GetMainBase() && east.GetMainBase());
		if (!west || !east || !west.GetMainBase() || !east.GetMainBase()) return;
		AICF_RecruitmentMapProbeSlot slot = new AICF_RecruitmentMapProbeSlot(2, AICF_EGroupRole.ATTACK);
		slot.m_ProbeTarget = east.GetMainBase();
		Check("ENEMY_ATTACK_VISIBLE", IsAttackObjective(slot, west));
		slot.m_bProbeRecruiting = true;
		Check("RECRUITMENT_ATTACK_HIDDEN", !IsAttackObjective(slot, west));
		string language;
		WidgetManager.GetLanguage(language);
		string task = DescribeTask(null, slot, null);
		WidgetManager.SetLanguage("ru_ru");
		Check("RECRUITMENT_RU", AICF_Localization.Resolve(task) == "Пополнение состава в казарме");
		WidgetManager.SetLanguage("en_us");
		Check("RECRUITMENT_EN", AICF_Localization.Resolve(task) == "Replenishing squad at barracks");
		WidgetManager.SetLanguage(language);
		slot.m_bProbeRecruiting = false;
		Check("ATTACK_RESTORED", IsAttackObjective(slot, west));
		slot.m_ProbeTarget = west.GetMainBase();
		Check("FRIENDLY_ATTACK_HIDDEN", !IsAttackObjective(slot, west));
		slot.m_ProbeTarget = east.GetMainBase();
		slot.SetRoleAndIndex(AICF_EGroupRole.DEFEND, 0);
		Check("DEFENDER_ATTACK_HIDDEN", !IsAttackObjective(slot, west));
		Check("MISSING_SLOT_HIDDEN", !IsAttackObjective(null, west));
	}
}

modded class SCR_GameModeCampaign
{
	override void OnGameStart()
	{
		super.OnGameStart();
		string enabled;
		if (System.GetCLIParam("aicfRecruitmentMapProbe", enabled) && enabled == "1")
			GetGame().GetCallqueue().CallLater(AICF_RunRecruitmentMapProbe, 20000, false);
	}
	protected void AICF_RunRecruitmentMapProbe()
	{
		AICF_RecruitmentMapProbeSystem probe = new AICF_RecruitmentMapProbeSystem();
		probe.Run(this);
		Print(string.Format("[AICF][RECRUITMENT_MAP_FINISHED] checks=%1 failures=%2", probe.m_iChecks, probe.m_iFailures));
		GetGame().RequestClose();
	}
	void ~SCR_GameModeCampaign()
	{
		if (GetGame()) GetGame().GetCallqueue().Remove(AICF_RunRecruitmentMapProbe);
	}
}
