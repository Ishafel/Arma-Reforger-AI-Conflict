// Resolves ordered faction-owned ResourceNames from the stock vehicle catalog.
// LIGHT_TRANSPORT lists the light candidate first and faction truck fallbacks
// afterwards; acquisition accepts the first prefab whose conservative metadata
// and live compartments
// fit every surviving member under ALL_OR_FALLBACK.
class AICF_VehicleCatalog
{
	bool GetCandidatePrefabsForAcquisition(
		SCR_CampaignFaction faction,
		AICF_EVehicleKind kind,
		AICF_StrategicAssignmentSnapshot assignment,
		string identityContext,
		out array<ResourceName> candidates)
	{
		candidates.Clear();
		if (!assignment || !assignment.IsValid() || !faction ||
			assignment.GetFactionKey() != faction.GetFactionKey())
		{
			return false;
		}

		return GetCandidatePrefabsInternal(
			faction,
			kind,
			identityContext,
			candidates);
	}

	protected bool GetCandidatePrefabsInternal(
		SCR_CampaignFaction faction,
		AICF_EVehicleKind kind,
		string identityContext,
		out array<ResourceName> candidates)
	{
		candidates.Clear();
		if (!faction)
			return false;

		SCR_EntityCatalog catalog = faction.GetFactionEntityCatalogOfType(EEntityCatalogType.VEHICLE);
		if (!catalog)
		{
			AICF_Stage3Diagnostics.Error(
				"VEHICLE_CATALOG_MISSING",
				FormatCatalogContext(faction, kind, identityContext));
			return false;
		}

		array<SCR_EntityCatalogEntry> entries = {};
		catalog.GetEntityList(entries);
		array<string> suffixes = {};
		BuildSuffixPreference(faction.GetFactionKey(), kind, suffixes);
		foreach (string suffix : suffixes)
			AppendCatalogMatch(entries, suffix, candidates);

		for (int candidateIndex = candidates.Count() - 1; candidateIndex >= 0; candidateIndex--)
		{
			ResourceName candidate = candidates[candidateIndex];
			Resource resource = Resource.Load(candidate);
			if (resource && resource.IsValid())
				continue;

			AICF_Stage3Diagnostics.Warning(
				"VEHICLE_PREFAB_CANDIDATE_REJECTED",
				FormatCatalogCandidateRejection(
					faction,
					kind,
					candidate,
					identityContext));
			candidates.Remove(candidateIndex);
		}

		if (!candidates.IsEmpty())
			return true;

		AICF_Stage3Diagnostics.Error(
			"VEHICLE_PREFAB_NOT_FOUND",
			FormatCatalogNotFound(faction, kind, entries.Count(), identityContext));
		return false;
	}

	protected string FormatCatalogContext(
		SCR_CampaignFaction faction,
		AICF_EVehicleKind kind,
		string identityContext)
	{
		string details = identityContext;
		if (!details.IsEmpty())
			details += " ";
		details += string.Format(
			"faction=%1 kind=%2",
			faction.GetFactionKey(),
			AICF_Stage3Diagnostics.KindToString(kind));
		return details;
	}

	protected string FormatCatalogCandidateRejection(
		SCR_CampaignFaction faction,
		AICF_EVehicleKind kind,
		ResourceName candidate,
		string identityContext)
	{
		string details = FormatCatalogContext(faction, kind, identityContext);
		details += string.Format(
			" prefab=%1 reason=RESOURCE_INVALID",
			candidate);
		return details;
	}

	protected string FormatCatalogNotFound(
		SCR_CampaignFaction faction,
		AICF_EVehicleKind kind,
		int catalogEntries,
		string identityContext)
	{
		string details = FormatCatalogContext(faction, kind, identityContext);
		details += string.Format(" catalog_entries=%1", catalogEntries);
		return details;
	}

	// Conservative metadata for the explicitly supported faction candidates.
	// Acquisition uses this before entity creation, then repeats the check on
	// live accessible compartments as a defensive engine/runtime validation.
	bool TryGetConservativeCapacity(
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
			// Four is intentionally conservative. Live validation may observe more,
			// but acquisition never promises seats absent from supported metadata.
			accessibleSeats = 4;
			hasPilot = true;
			hasTurret = true;
			return kind == AICF_EVehicleKind.ARMED_LIGHT;
		}

		return false;
	}

	protected void BuildSuffixPreference(
		FactionKey factionKey,
		AICF_EVehicleKind kind,
		array<string> suffixes)
	{
		if (kind == AICF_EVehicleKind.ARMED_LIGHT)
		{
			if (factionKey == "US")
			{
				suffixes.Insert("Prefabs/Vehicles/Wheeled/Conflict_Variants/M1025_armed_M2HB_Conflict.et");
				suffixes.Insert("Prefabs/Vehicles/Wheeled/M998/M1025_armed_M2HB.et");
			}
			else if (factionKey == "USSR")
			{
				suffixes.Insert("Prefabs/Vehicles/Wheeled/UAZ469/UAZ469_PKM.et");
				suffixes.Insert("Prefabs/Vehicles/Wheeled/Conflict_Variants/BRDM2_Conflict.et");
			}
			return;
		}

		if (kind == AICF_EVehicleKind.LIGHT_TRANSPORT)
		{
			if (factionKey == "US")
				suffixes.Insert("Prefabs/Vehicles/Wheeled/M998/M998_covered_long.et");
			else if (factionKey == "USSR")
				suffixes.Insert("Prefabs/Vehicles/Wheeled/UAZ452/UAZ452_transport.et");
		}

		// Trucks are the primary A0/A1 choice and the mandatory capacity fallback
		// for A2/D0 when a light candidate cannot seat the full living roster.
		if (factionKey == "US")
			suffixes.Insert("Prefabs/Vehicles/Wheeled/M923A1/M923A1_transport.et");
		else if (factionKey == "USSR")
			suffixes.Insert("Prefabs/Vehicles/Wheeled/Ural4320/Ural4320_transport.et");
	}

	protected void AppendCatalogMatch(
		array<SCR_EntityCatalogEntry> entries,
		string suffix,
		array<ResourceName> candidates)
	{
		foreach (SCR_EntityCatalogEntry entry : entries)
		{
			if (!entry)
				continue;

			ResourceName prefab = entry.GetPrefab();
			if (!prefab.IsEmpty() && prefab.Contains(suffix) && !candidates.Contains(prefab))
			{
				candidates.Insert(prefab);
				return;
			}
		}
	}
}
