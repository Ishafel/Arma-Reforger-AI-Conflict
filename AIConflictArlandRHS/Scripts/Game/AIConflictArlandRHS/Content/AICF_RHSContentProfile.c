// RHS Status Quo 0.16.5150 content contract for the stock RHS Arland Conflict
// mission. Every path is resolved from the owning faction catalog; no asset is
// copied and no uncatalogued ResourceName is admitted.
class AICF_RHSContentProfile : AICF_ContentProfile
{
	override string GetProfileKey()
	{
		return "RHS_USMC_MSV_0_16_5150";
	}

	override FactionKey GetRuntimeFactionKey(FactionKey stableKey)
	{
		if (stableKey == "US")
			return "RHS_USAF";
		if (stableKey == "USSR")
			return "RHS_AFRF";
		return FactionKey.Empty;
	}

	override FactionKey GetStableFactionKey(FactionKey runtimeKey)
	{
		if (runtimeKey == "RHS_USAF" || runtimeKey == "US")
			return "US";
		if (runtimeKey == "RHS_AFRF" || runtimeKey == "USSR")
			return "USSR";
		return FactionKey.Empty;
	}

	override bool AllowsSourceRosterFallback()
	{
		return false;
	}

	override bool AllowsGroupFactionRebinding()
	{
		// RHS campaign factions currently point GetDefendersGroupPrefab() at a
		// stock-affiliated empty controller. Rebind before any roster request;
		// member prefabs remain strict faction-catalog RHS selections.
		return true;
	}

	override bool BuildCharacterRoleCandidates(
		FactionKey stableKey,
		int memberIndex,
		out string role,
		out array<string> suffixes)
	{
		suffixes.Clear();
		string prefix;
		if (stableKey == "US")
		{
			prefix = "Prefabs/Characters/Factions/BLUFOR/RHS_USAF/RHS_USAF_USMC_MEF/Character_RHS_USAF_USMC_";
		}
		else if (stableKey == "USSR")
		{
			prefix = "Prefabs/Characters/Factions/OPFOR/RHS_AFRF/MSV/VKPO_Demiseason/Character_RHS_RF_MSV_VKPO_DS_";
		}
		else
		{
			return false;
		}

		switch (memberIndex)
		{
			case 0:
				role = "SQUAD_LEADER";
				suffixes.Insert(prefix + "SL.et");
				break;
			case 1:
				role = "MEDIC";
				suffixes.Insert(prefix + "Medic.et");
				break;
			case 2:
				role = "MACHINE_GUNNER";
				suffixes.Insert(prefix + "MG.et");
				break;
			case 3:
				role = "ANTI_TANK";
				if (stableKey == "US")
					suffixes.Insert(prefix + "LAT.et");
				else
					suffixes.Insert(prefix + "AT.et");
				break;
			case 4:
				role = "GRENADIER";
				suffixes.Insert(prefix + "GL.et");
				break;
			case 5:
				role = "AUTOMATIC_RIFLEMAN";
				suffixes.Insert(prefix + "AR.et");
				break;
			case 6:
				if (stableKey == "US")
				{
					role = "TEAM_LEADER";
					suffixes.Insert(prefix + "TL.et");
				}
				else
				{
					role = "SENIOR_RIFLEMAN";
					suffixes.Insert(prefix + "SR.et");
				}
				break;
			case 7:
				role = "MACHINE_GUNNER_ASSISTANT";
				suffixes.Insert(prefix + "AMG.et");
				break;
			case 8:
				role = "ANTI_TANK_ASSISTANT";
				if (stableKey == "US")
				{
					// RHS USMC MEF exposes no AAT prefab. Its deterministic AAR
					// assistant loadout is the supported ammunition-bearer mapping.
					suffixes.Insert(prefix + "AAR.et");
				}
				else
				{
					suffixes.Insert(prefix + "AAT.et");
				}
				break;
			default:
				role = "RIFLEMAN";
				suffixes.Insert(prefix + "Rifleman.et");
				break;
		}

		return !suffixes.IsEmpty();
	}

	override void BuildVehicleSuffixPreference(
		FactionKey stableKey,
		AICF_EVehicleKind kind,
		array<string> suffixes)
	{
		if (kind == AICF_EVehicleKind.ARMED_LIGHT)
		{
			if (stableKey == "US")
				suffixes.Insert("Prefabs/Vehicles/Wheeled/M998/M1025_armed_M2HB_USAF.et");
			else if (stableKey == "USSR")
				suffixes.Insert("Prefabs/Vehicles/Wheeled/K4386/K4386_Armed.et");
			return;
		}

		if (kind == AICF_EVehicleKind.LIGHT_TRANSPORT)
		{
			if (stableKey == "US")
				suffixes.Insert("Prefabs/Vehicles/Wheeled/M998/M998_covered_long_USAF.et");
			else if (stableKey == "USSR")
				suffixes.Insert("Prefabs/Vehicles/Wheeled/K4386/K4386.et");
		}

		// Explicit faction-catalog fallbacks seat a complete ten-member roster.
		if (stableKey == "US")
			suffixes.Insert("Prefabs/Vehicles/Wheeled/M923A1/M923A1_transport.et");
		else if (stableKey == "USSR")
			suffixes.Insert("Prefabs/Vehicles/Wheeled/Ural4320/Ural4320_transport.et");
	}

	override bool TryGetConservativeVehicleCapacity(
		ResourceName prefab,
		AICF_EVehicleKind kind,
		out int accessibleSeats,
		out bool hasPilot,
		out bool hasTurret)
	{
		accessibleSeats = 0;
		hasPilot = false;
		hasTurret = false;
		if (prefab.Contains("M998_covered_long_USAF.et"))
		{
			accessibleSeats = 4;
			hasPilot = true;
			return kind == AICF_EVehicleKind.LIGHT_TRANSPORT;
		}
		if (prefab.Contains("M923A1_transport.et"))
		{
			accessibleSeats = 15;
			hasPilot = true;
			return kind != AICF_EVehicleKind.ARMED_LIGHT;
		}
		if (prefab.Contains("M1025_armed_M2HB_USAF.et"))
		{
			accessibleSeats = 5;
			hasPilot = true;
			hasTurret = true;
			return kind == AICF_EVehicleKind.ARMED_LIGHT;
		}
		if (prefab.Contains("K4386_Armed.et"))
		{
			accessibleSeats = 7;
			hasPilot = true;
			hasTurret = true;
			return kind == AICF_EVehicleKind.ARMED_LIGHT;
		}
		if (prefab.Contains("K4386.et"))
		{
			accessibleSeats = 8;
			hasPilot = true;
			return kind == AICF_EVehicleKind.LIGHT_TRANSPORT;
		}
		if (prefab.Contains("Ural4320_transport.et"))
		{
			accessibleSeats = 15;
			hasPilot = true;
			return kind != AICF_EVehicleKind.ARMED_LIGHT;
		}

		return false;
	}
}
