// Native base selection data remains owned by the Everon integration.
class AICF_EveronRuntimeBase : SCR_CampaignCustomBase
{
	void AICF_EveronRuntimeBase(string name, bool hq)
	{
		m_sBaseName = name;
		m_bCanBeHQ = hq;
		m_bIsControlPoint = !hq;
		m_bDisableWhenUnusedAsHQ = true;
		m_fRadioRange = 2000;
	}
}
modded class AICF_ScenarioRegistry
{
	override bool ConfigureRuntimeMap(SCR_MissionHeaderCampaign header, AICF_ScenarioVariant variant)
	{
		if (!header || !variant) return false;
		if (variant.m_iMap == 1) return true;
		if (variant.m_iMap != 2) return super.ConfigureRuntimeMap(header, variant);
		header.m_bCustomBaseWhitelist = true;
		header.m_bEstablishingBasesEnabled = false;
		header.m_aCampaignCustomBaseList = {};
		header.m_aCampaignCustomBaseList.Insert(new AICF_EveronRuntimeBase("MilitaryBaseAirfield", true));
		header.m_aCampaignCustomBaseList.Insert(new AICF_EveronRuntimeBase("MilitaryHospital", true));
		header.m_aCampaignCustomBaseList.Insert(new AICF_EveronRuntimeBase("MainBaseNorth", false));
		header.m_aCampaignCustomBaseList.Insert(new AICF_EveronRuntimeBase("SmallBaseMaidensBay", false));
		header.m_aCampaignCustomBaseList.Insert(new AICF_EveronRuntimeBase("TownBaseMeaux", false));
		header.m_aCampaignCustomBaseList.Insert(new AICF_EveronRuntimeBase("TownBaseTyrone", false));
		header.m_aCampaignCustomBaseList.Insert(new AICF_EveronRuntimeBase("SmallBaseHornbeamValley", false));
		return true;
	}
}
