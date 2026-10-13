// Общие цены штатных меню. Автоматическое пополнение AICF не использует этот API.
class AICF_RHSMenuPricePolicy
{
	protected static ref AICF_RHSMenuPricePolicy s_Policy;

	static AICF_RHSMenuPricePolicy GetInstance()
	{
		if (!s_Policy)
			s_Policy = new AICF_RHSMenuPricePolicy();
		return s_Policy;
	}

	static bool IsActive()
	{
		return AICF_RHSContentProfile.Cast(AICF_ContentProfile.GetActive()) != null;
	}

	static string Path(ResourceName prefab)
	{
		int end = prefab.IndexOf("}");
		if (end >= 0)
			return prefab.Substring(end + 1, prefab.Length() - end - 1);
		return prefab;
	}

	static bool IsInfantry(ResourceName prefab)
	{
		return prefab.Contains("/RHS_USAF/RHS_USAF_USMC_MEF/") ||
			prefab.Contains("/RHS_USAF/RHS_USAF_USMC_MEF_D/") ||
			prefab.Contains("/RHS_AFRF/MSV/VKPO_Demiseason/") ||
			prefab.Contains("/RHS_AFRF/MSV/VKPO_Summer/") ||
			prefab.Contains("/RHS_AFRF/MSV/Flora/") ||
			prefab.Contains("/RHS_AFRF/MSV/VSR/");
	}

	// RandomSoldiers меняет вариант внешности/оружия внутри той же роли.
	static int CharacterCost(ResourceName prefab)
	{
		if (!prefab.Contains("Prefabs/Characters/") || !IsInfantry(prefab))
			return -1;
		string name = FilePath.StripPath(prefab);
		name.Replace("_Random", "");
		if (name.EndsWith("_AT.et") || name.EndsWith("_LAT.et") || name.EndsWith("_Sniper.et"))
			return 100;
		if (name.EndsWith("_MG.et") || name.EndsWith("_AR.et") || name.EndsWith("_GL.et") || name.EndsWith("_Marksman.et"))
			return 75;
		if (name.EndsWith("_Medic.et"))
			return 75;
		if (name.EndsWith("_Rifleman.et") || name.EndsWith("_SL.et") || name.EndsWith("_TL.et") ||
			name.EndsWith("_SR.et") || name.EndsWith("_AMG.et") || name.EndsWith("_AAT.et") || name.EndsWith("_Spotter.et"))
			return 50;
		return -1;
	}

	int Quote(ResourceName prefab)
	{
		if (prefab.Contains("Prefabs/Groups/") && IsInfantry(prefab))
		{
			Resource resource = Resource.Load(prefab);
			if (!resource || !resource.IsValid())
				return -1;
			IEntitySource source = SCR_BaseContainerTools.FindEntitySource(resource);
			array<ResourceName> members = {};
			if (!source || !source.Get("m_aUnitPrefabSlots", members) || members.IsEmpty())
				return -1;
			int total;
			foreach (ResourceName member : members)
			{
				int cost = CharacterCost(member);
				if (cost < 0)
					return -1;
				total += cost;
			}
			return total;
		}
		return VehicleCost(Path(prefab));
	}

	// Только известные семейства каталога; чужие prefab сохраняют native budget.
	int VehicleCost(string path)
	{
		if (path.StartsWith("Prefabs/Vehicles/Wheeled/M151A2/"))
		{
			if (path.EndsWith("M151A2_M2HB.et")) return 280;
			if (path.EndsWith("M151A2_transport.et")) return 120;
			if (path.EndsWith("M151A2.et")) return 110;
		}
		if (path.StartsWith("Prefabs/Vehicles/Wheeled/M998/"))
		{
			if (path.Contains("/M1025_armed_M2HB_USAF")) return 400;
			if (path.EndsWith("M1025_USAF.et")) return 240;
			if (path.EndsWith("M997_maxi_ambulance_USAF.et")) return 225;
			if (path.EndsWith("M998_covered_long_USAF.et")) return 200;
			if (path.EndsWith("M998_covered_USAF.et")) return 150;
		}
		if (path.StartsWith("Prefabs/Vehicles/Wheeled/UAZ469/"))
		{
			if (path.EndsWith("UAZ469_PKM.et")) return 280;
			if (path.EndsWith("UAZ469_Camo_uncovered.et")) return 125;
			if (path.EndsWith("UAZ469_Camo.et")) return 150;
		}
		if (path == "Prefabs/Vehicles/Wheeled/UAZ452/UAZ452_ambulance.et") return 225;
		if (path.StartsWith("Prefabs/Vehicles/Wheeled/M923A1/") || path.StartsWith("Prefabs/Vehicles/Wheeled/Ural4320/"))
		{
			if (path.EndsWith("_transport.et") || path.EndsWith("_transport_covered.et")) return 200;
			if (path.EndsWith("_engineer.et")) return 260;
			if (path.EndsWith("_tanker.et")) return 160;
			if (path.EndsWith("_command.et")) return 420;
			if (path.EndsWith("_arsenal.et") || path.EndsWith("_Arsenal.et")) return 380;
			if (path.EndsWith("_repair.et")) return 175;
		}
		if (path == "Prefabs/MP/Campaign/Assets/CampaignMobileAssemblyEast.et") return 420;
		if (path == "Prefabs/Vehicles/Wheeled/BTR70/BTR70_AFRF.et") return 1000;
		if (path == "Prefabs/Vehicles/Wheeled/LAV25/LAV25_RHS.et" || path == "Prefabs/Vehicles/Wheeled/LAV25/LAV25_RHS_D.et") return 1500;
		if (path.StartsWith("Prefabs/Vehicles/Wheeled/K17/APC_K17_"))
		{
			if (path.Contains("_unarmed")) return 800;
			if (path.Contains("_Epoch")) return 2000;
			if (path.Contains("_Berezok")) return 2200;
		}
		if (path.StartsWith("Prefabs/Vehicles/Wheeled/K4386/K4386"))
		{
			if (path.Contains("_Armed")) return 1700;
			return 700;
		}
		if (path == "Prefabs/Vehicles/Helicopters/UH1H/UH1H.et") return 1250;
		if (path == "Prefabs/Vehicles/Helicopters/UH1H/UH1H_armed.et") return 1500;
		if (path == "Prefabs/Vehicles/Helicopters/Mi8MT/Mi8MT_unarmed_transport.et") return 1375;
		if (path == "Prefabs/Vehicles/Helicopters/Mi8MT/Mi8MT_armed_black.et") return 2125;
		if (path == "Prefabs/Vehicles/Helicopters/Mi8MT/Mi8MTT-A_Black.et") return 1625;
		return -1;
	}
}
