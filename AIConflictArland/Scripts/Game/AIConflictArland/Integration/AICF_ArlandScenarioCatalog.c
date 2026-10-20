// Source addons loaded by -gproj/-addons have no installed WorkshopItem.
// Register the example and its launch targets in the native frontend catalog.
modded class ArmaReforgerScripted
{
	override array<ResourceName> GetDefaultGameConfigs()
	{
		array<ResourceName> resources = super.GetDefaultGameConfigs();
		ResourceName setup = "{A1CF261019500001}Missions/AICF_Conflict_Arland_Setup.conf";
		AICF_ScenarioSetupHeader header = AICF_ScenarioSetupHeader.Cast(MissionHeader.ReadMissionHeader(setup));
		if (!header) return resources;

		array<ResourceName> presets = {setup};
		for (int mode; mode < 3; mode++)
		{
			for (int difficulty; difficulty < 3; difficulty++)
			{
				ResourceName preset = header.ResolvePreset(mode, difficulty);
				if (preset.IsEmpty()) return resources;
				presets.Insert(preset);
			}
		}

		if (!resources) resources = {};
		foreach (ResourceName resource : presets)
			InsertNewScenario(resource, resources);
		return resources;
	}
}
