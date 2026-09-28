// Карта, readiness, roster и транспорт сохраняют владельцев из RHS integration.
class AICF_WCSRHSContentProfile : AICF_RHSContentProfile
{
	override string GetProfileKey()
	{
		return "WCS_RHS_ARLAND";
	}

	override void GetLoadoutSources(array<string> sources)
	{
		super.GetLoadoutSources(sources);
		sources.Insert("WCS");
	}

	override bool PrepareDefaultLoadout(IEntity model, ResourceName source, FactionKey stableKey)
	{
		if (!super.PrepareDefaultLoadout(model, source, stableKey)) return false;
		if (!AICF_WCSInfantryKit.SupportsRole(source)) return true;
		return AICF_WCSInfantryEquipment.BuildDefault(model, GetRuntimeFactionKey(stableKey), source);
	}

	override string GetLoadoutItemSource(ResourceName prefab)
	{
		array<string> addons = SCR_AddonTool.GetResourceAddons(prefab);
		foreach (string addon : addons)
		{
			if (addon.StartsWith("WCS")) return "WCS";
		}
		return super.GetLoadoutItemSource(prefab);
	}

	override ResourceName GetPMCPrimaryOverride(ResourceName source, int variant)
	{
		// WCS меняет старые ION variants и canonical spelling PKM.
		// Сохраняем loaded/spare/suppressed проверки прежнего PMC adapter.
		if (source.EndsWith("_MG.et"))
			return "{4BE4931DF7B1ABCD}Prefabs/Weapons/MachineGuns/PKM/MG_PKM_B51_EOT.et";
		if (source.EndsWith("_Sharpshooter.et")) return ResourceName.Empty;
		return "{4E0FB248B9FB1F5B}Prefabs/Weapons/Rifles/M4A1/Variants/Rifle_M4A1_WCS_URGI_Suppressed_SQUAD.et";
	}
}

modded class SCR_GameModeCampaign
{
	override protected AICF_ContentProfile AICF_CreateContentProfile()
	{
		return new AICF_WCSRHSContentProfile();
	}
}
