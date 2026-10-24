// Source addons loaded by -gproj/-addons have no installed WorkshopItem.
// Register one universal entry. Leaf resources remain available by GUID for CLI.
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
