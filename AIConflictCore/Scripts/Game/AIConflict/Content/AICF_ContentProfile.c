// Narrow content boundary shared by stock and optional content addons. Runtime
// faction keys remain authoritative entity identity; stable keys preserve AICF
// policy, CLI, UI and diagnostic contracts.
class AICF_ContentProfile
{
	protected static ref AICF_ContentProfile s_ActiveProfile;
	protected static ref AICF_ContentProfile s_LastDiagnosticProfile;

	static void SetActive(AICF_ContentProfile profile)
	{
		s_ActiveProfile = profile;
		s_LastDiagnosticProfile = profile;
	}

	static AICF_ContentProfile GetActive()
	{
		if (!s_ActiveProfile && s_LastDiagnosticProfile)
			return s_LastDiagnosticProfile;
		if (!s_ActiveProfile)
			s_ActiveProfile = new AICF_ContentProfile();
		return s_ActiveProfile;
	}

	static void ClearActive(AICF_ContentProfile profile)
	{
		if (s_ActiveProfile == profile)
			s_ActiveProfile = null;
		// Late vanilla teardown callbacks can still emit AICF diagnostics after
		// OnGameEnd. Retain only their normalization context; the next game start
		// replaces it through SetActive before installing any AICF lifecycle.
	}

	string GetProfileKey()
	{
		return "STOCK";
	}

	FactionKey GetRuntimeFactionKey(FactionKey stableKey)
	{
		if (stableKey == "US" || stableKey == "USSR")
			return stableKey;
		return FactionKey.Empty;
	}

	FactionKey GetStableFactionKey(FactionKey runtimeKey)
	{
		if (runtimeKey == "US" || runtimeKey == "USSR")
			return runtimeKey;
		return FactionKey.Empty;
	}

	bool IsExpectedRuntimeFaction(FactionKey stableKey, FactionKey runtimeKey)
	{
		FactionKey expectedRuntimeKey = GetRuntimeFactionKey(stableKey);
		return !expectedRuntimeKey.IsEmpty() && expectedRuntimeKey == runtimeKey;
	}

	bool AllowsSourceRosterFallback()
	{
		return true;
	}

	bool AllowsGroupFactionRebinding()
	{
		return false;
	}

	bool BuildCharacterRoleCandidates(
		FactionKey stableKey,
		int memberIndex,
		out string role,
		out array<string> suffixes)
	{
		suffixes.Clear();
		string prefix = "Character_US_";
		if (stableKey == "USSR")
			prefix = "Character_USSR_";
		else if (stableKey != "US")
			return false;

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
				if (stableKey == "USSR")
				{
					role = "SENIOR_RIFLEMAN";
					suffixes.Insert(prefix + "SR.et");
				}
				else
				{
					role = "TEAM_LEADER";
					suffixes.Insert(prefix + "TL.et");
				}
				break;
			case 7:
				role = "MACHINE_GUNNER_ASSISTANT";
				suffixes.Insert(prefix + "AMG.et");
				break;
			case 8:
				role = "ANTI_TANK_ASSISTANT";
				suffixes.Insert(prefix + "AAT.et");
				break;
			default:
				role = "RIFLEMAN";
				suffixes.Insert(prefix + "Rifleman.et");
				break;
		}

		return !suffixes.IsEmpty();
	}

	void BuildVehicleSuffixPreference(
		FactionKey stableKey,
		AICF_EVehicleKind kind,
		array<string> suffixes)
	{
		if (kind == AICF_EVehicleKind.ARMED_LIGHT)
		{
			if (stableKey == "US")
			{
				suffixes.Insert("Prefabs/Vehicles/Wheeled/Conflict_Variants/M1025_armed_M2HB_Conflict.et");
				suffixes.Insert("Prefabs/Vehicles/Wheeled/M998/M1025_armed_M2HB.et");
			}
			else if (stableKey == "USSR")
			{
				suffixes.Insert("Prefabs/Vehicles/Wheeled/UAZ469/UAZ469_PKM.et");
				suffixes.Insert("Prefabs/Vehicles/Wheeled/Conflict_Variants/BRDM2_Conflict.et");
			}
			return;
		}

		if (kind == AICF_EVehicleKind.LIGHT_TRANSPORT)
		{
			if (stableKey == "US")
				suffixes.Insert("Prefabs/Vehicles/Wheeled/M998/M998_covered_long.et");
			else if (stableKey == "USSR")
				suffixes.Insert("Prefabs/Vehicles/Wheeled/UAZ452/UAZ452_transport.et");
		}

		if (stableKey == "US")
			suffixes.Insert("Prefabs/Vehicles/Wheeled/M923A1/M923A1_transport.et");
		else if (stableKey == "USSR")
			suffixes.Insert("Prefabs/Vehicles/Wheeled/Ural4320/Ural4320_transport.et");
	}

	bool TryGetConservativeVehicleCapacity(
		ResourceName prefab,
		AICF_EVehicleKind kind,
		out int accessibleSeats,
		out bool hasPilot,
		out bool hasTurret)
	{
		accessibleSeats = 0;
		hasPilot = false;
		hasTurret = false;
		if (prefab.IsEmpty())
			return false;

		if (prefab.Contains("M923A1_transport.et") ||
			prefab.Contains("Ural4320_transport.et"))
		{
			accessibleSeats = 15;
			hasPilot = true;
			return kind != AICF_EVehicleKind.ARMED_LIGHT;
		}
		if (prefab.Contains("UAZ452_transport.et"))
		{
			accessibleSeats = 8;
			hasPilot = true;
			return kind != AICF_EVehicleKind.ARMED_LIGHT;
		}
		if (prefab.Contains("M998_covered_long.et"))
		{
			accessibleSeats = 4;
			hasPilot = true;
			return kind != AICF_EVehicleKind.ARMED_LIGHT;
		}
		if (prefab.Contains("M1025_armed_M2HB") ||
			prefab.Contains("UAZ469_PKM.et") ||
			prefab.Contains("BRDM2_Conflict.et"))
		{
			accessibleSeats = 4;
			hasPilot = true;
			hasTurret = true;
			return kind == AICF_EVehicleKind.ARMED_LIGHT;
		}

		return false;
	}

	string NormalizeDiagnosticMessage(string message)
	{
		FactionKey runtimeUS = GetRuntimeFactionKey("US");
		FactionKey runtimeUSSR = GetRuntimeFactionKey("USSR");
		if (!runtimeUS.IsEmpty() && runtimeUS != "US")
			message.Replace("=" + runtimeUS, "=US");
		if (!runtimeUSSR.IsEmpty() && runtimeUSSR != "USSR")
			message.Replace("=" + runtimeUSSR, "=USSR");
		return message;
	}
}

// Unlike STAGE diagnostics, this one deliberately exposes both runtime and
// stable identities once at composition time as mapping evidence.
class AICF_ContentDiagnostics
{
	static void ProfileSelected(AICF_ContentProfile profile)
	{
		if (!profile)
			return;
		Print(
			string.Format(
				"[AICF][CONTENT][INFO][PROFILE_SELECTED] profile=%1 runtime_blufor=%2 stable_blufor=US runtime_opfor=%3 stable_opfor=USSR",
				profile.GetProfileKey(),
				profile.GetRuntimeFactionKey("US"),
				profile.GetRuntimeFactionKey("USSR")),
			LogLevel.NORMAL);
	}
}
