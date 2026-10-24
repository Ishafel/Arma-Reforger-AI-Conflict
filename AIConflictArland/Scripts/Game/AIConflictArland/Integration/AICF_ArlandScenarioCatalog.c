// Source addons loaded by -gproj/-addons have no installed WorkshopItem.
// Register the sole owned mission. Runtime choices use native mission targets.
modded class ArmaReforgerScripted
{
	override array<ResourceName> GetDefaultGameConfigs()
	{
		array<ResourceName> resources = super.GetDefaultGameConfigs();
		ResourceName setup = "{A1CF261019500001}Missions/AICF_Conflict_Arland_Setup.conf";
		SCR_MissionHeaderCampaign header = SCR_MissionHeaderCampaign.Cast(MissionHeader.ReadMissionHeader(setup));
		if (!header || !header.m_bAICFSetupEntry) return resources;

		if (!resources) resources = {};
		InsertNewScenario(setup, resources);
		return resources;
	}
}
