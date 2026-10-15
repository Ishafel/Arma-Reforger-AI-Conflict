// Только WCS stage, после AICF_WCSRHSContentProfile.c.
modded class AICF_WCSRHSContentProfile
{
	override ResourceName GetPMCPrimaryOverride(ResourceName source, int variant)
	{
		if (AICF_ARMNegative.s_bUnavailable) return "{0000000000000052}Prefabs/AICF_MissingWeapon.et";
		return super.GetPMCPrimaryOverride(source, variant);
	}
}
