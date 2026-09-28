// Только данные проверенных native prefab. Роль определяется по concrete RHS
// персонажу после штатного randomizer; faction и source roster не заменяются.
class AICF_WCSInfantryKit
{
	static bool SupportsRole(ResourceName source)
	{
		array<string> roles = {"SL", "Medic", "MG", "LAT", "AT", "GL", "AR", "TL", "SR", "AMG", "AAR", "AAT", "Rifleman"};
		foreach (string role : roles)
		{
			if (source.EndsWith("_" + role + ".et") || source.EndsWith("_" + role + "_2.et") || source.EndsWith("_" + role + "_3.et")) return true;
		}
		return false;
	}

	static ResourceName Weapon(FactionKey faction, ResourceName source)
	{
		if (faction == "RHS_USAF")
		{
			if (source.Contains("_MG."))
				return "{C2A090B4AF474D27}Prefabs/Weapons/MachineGuns/M240/WCS_Variants/MG_M240_MDO_SQUAD.et";
			if (source.Contains("_AR."))
				return "{37C1DF2751538357}Prefabs/Weapons/MachineGuns/M249/Variants/MG_M249_HAMR_SQUAD.et";
			if (source.Contains("_GL."))
				return "{0F730BC32F4F3CD1}Prefabs/Weapons/Rifles/M4A1/WCS_Variants/Rifle_M4A1_BLOCK_II_M203_SQUAD.et";
			return "{4E0FB248B9FB1F5B}Prefabs/Weapons/Rifles/M4A1/Variants/Rifle_M4A1_WCS_URGI_Suppressed_SQUAD.et";
		}
		if (faction == "RHS_AFRF")
		{
			if (source.Contains("_MG.") || source.Contains("_MG_"))
				return "{337537C0F36E664D}Prefabs/Weapons/MachineGuns/PKP/Variants/MG_PKP_NPZ_SQUAD.et";
			if (source.Contains("_AR.") || source.Contains("_AR_"))
				return "{5AED86092AC8BE1C}Prefabs/Weapons/MachineGuns/RPK74M/WCS_Variants/MG_RPK74M_SPECTER_SQUAD.et";
			if (source.Contains("_GL.") || source.Contains("_GL_"))
				return "{2F9BF587FDB19E32}Prefabs/Weapons/Rifles/AK12/Variants/Rifle_AK12_UBGL_SQUAD.et";
			if (source.Contains("_Medic"))
				return "{0A83316977E4E4B7}Prefabs/Weapons/Rifles/AK74M/WCS_Variants/Rifle_AK105_SQUAD.et";
			return "{397B6CBE56D21341}Prefabs/Weapons/Rifles/AK12/Variants/Rifle_AK12_EOTECH_SQUAD.et";
		}
		return ResourceName.Empty;
	}

	static int MinimumMagazines(ResourceName source)
	{
		if (source.Contains("_MG.") || source.Contains("_MG_")) return 3;
		if (source.Contains("_AR.") || source.Contains("_AR_")) return 4;
		return 6;
	}

	static bool HasRole(ResourceName source, string role)
	{
		return source.EndsWith("_" + role + ".et") || source.EndsWith("_" + role + "_2.et") || source.EndsWith("_" + role + "_3.et");
	}

	static bool NeedsBackpack(FactionKey faction, ResourceName source)
	{
		// Медицина, коробки лент, ПТ и запас ВОГ. У остальных ролей
		// достаточно штатных карманов и подсумков, без отдельного рюкзака.
		return HasRole(source, "Medic") || HasRole(source, "MG") || HasRole(source, "AMG") ||
			HasRole(source, "AAR") || HasRole(source, "AT") || HasRole(source, "AAT") ||
			(faction == "RHS_USAF" && HasRole(source, "AR")) || (faction == "RHS_AFRF" && HasRole(source, "GL"));
	}

	static ResourceName Eyewear(FactionKey faction, ResourceName source)
	{
		if (faction == "RHS_AFRF")
		{
			if (EyewearOnHelmet(faction, source))
				return "{8F3364A5F7906886}Prefabs/Characters/Eyewear/6b50/Eyewear_6b50_wear_on_6b47.et";
			return "{0F7546A421CCA5B6}Prefabs/Characters/Eyewear/6b50/Eyewear_6b50_strap_on_6b47.et";
		}
		if (HasRole(source, "SL") || HasRole(source, "TL"))
			return "{79A869C91A7A1368}Prefabs/Characters/Eyewear/gascan/Eyewear_gascan_Blk.et";
		if (HasRole(source, "Medic"))
			return "{F3672F802360D2E4}Prefabs/Characters/Eyewear/crossbow/Eyewear_ess_crossbow_blk_clr.et";
		return "{608F66AB8B36B85D}Prefabs/Characters/Eyewear/crossbow/Eyewear_ess_crossbow_blk.et";
	}

	static bool EyewearOnHelmet(FactionKey faction, ResourceName source)
	{
		return faction == "RHS_AFRF" && (HasRole(source, "SL") || HasRole(source, "SR") || HasRole(source, "Medic"));
	}

	static ResourceName Vest(FactionKey faction, ResourceName source)
	{
		if (faction == "RHS_AFRF") return "{B89BB29B04D26E70}Prefabs/Characters/Vests/Vest_TV102/Variants/Vest_TV102_rifleman_EMR_7.et";
		if (HasRole(source, "GL")) return "{8EFA9C3C56A20555}Prefabs/Characters/Vests/Vest_JPC/variants/Vest_JPC_AOR2_UBGL.et";
		if (HasRole(source, "MG") || HasRole(source, "AR")) return "{C8C57BE65A14F1F9}Prefabs/Characters/Vests/Vest_JPC/variants/Vest_JPC_okinawa_AOR2_MG.et";
		return "{A950AD564E4C0941}Prefabs/Characters/Vests/Vest_JPC/variants/Vest_JPC_ronin_4_AOR2.et";
	}

	static bool IsNightVision(ResourceName prefab)
	{
		string path = prefab;
		path.ToLower();
		return path.Contains("/nightvision/") || path.Contains("/nvg_") || path.Contains("/nvgs/");
	}

	static void Clothes(FactionKey faction, ResourceName source, array<ResourceName> clothes)
	{
		// Native объём карманов не меняется. Рюкзак только для груза роли.
		if (faction == "RHS_USAF")
		{
			clothes.Insert("{14FBE23B34423018}Prefabs/Characters/Uniforms/Shirt_CryeG3_RHS/MC/Shirt_CryeG3_RHS_Tucked_SleevesDown_MC.et");
			clothes.Insert("{0C0D206E5BC2C74B}Prefabs/Characters/Uniforms/Pants_CryeG3_RHS/MC/Pants_CryeG3_RHS_MC.et");
			clothes.Insert(Vest(faction, source));
			clothes.Insert("{B76EAFDBFBA131D9}Prefabs/Characters/HeadGear/Helmets/Helmet_OPSCORE/WCS_Variants/Helmet_OPSCORE_XP_Standard_Issue.et");
			clothes.Insert("{514585D29848282C}Prefabs/Characters/Handwear/Gloves_MechanixMpact/Gloves_MechanixMpact_Coyote.et");
			clothes.Insert("{68029AF966E46794}Prefabs/Characters/Footwear/Boots_DEFCON/Boots_DEFCON.et");
			if (NeedsBackpack(faction, source)) clothes.Insert("{9B3A55E8FCBE4CF1}Prefabs/Items/Equipment/Backpacks/Backpack_Rush12/Backpack_Rush12_MC.et");
		}
		else if (faction == "RHS_AFRF")
		{
			clothes.Insert("{02C0CCAC06216B40}Prefabs/Characters/Uniforms/Shirt_VKPO3/Shirt_VKPO3_EMR.et");
			clothes.Insert("{2B3629EF5CC75ECD}Prefabs/Characters/Uniforms/Pants_VKPO3/Pants_VKPO3_EMR.et");
			clothes.Insert(Vest(faction, source));
			clothes.Insert("{F54C67F5761694B5}Prefabs/Characters/HeadGear/Helmets/Helmet_6B47/WCS_Variants/Helmet_6B47_6M2-1_Standard_Issue.et");
			clothes.Insert("{67D4FA63CF57EB29}Prefabs/Characters/Handwear/Gloves_MechanixMpact/Gloves_MechanixMpact_Covert.et");
			clothes.Insert("{26E618F9E0E12F18}Prefabs/Characters/Footwear/Boots_Luma/Boots_Luma_Black.et");
			if (NeedsBackpack(faction, source)) clothes.Insert("{4B880ADA3DF7DD76}Prefabs/Items/Equipment/Backpacks/Backpack_Rush12/Backpack_Rush12_Olive.et");
		}
		clothes.Insert(Eyewear(faction, source));
	}
}
