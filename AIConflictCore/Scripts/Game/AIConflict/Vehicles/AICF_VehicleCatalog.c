// Resolves ordered faction-owned ResourceNames from the stock vehicle catalog.
// LIGHT_TRANSPORT lists the light candidate first and faction truck fallbacks
// afterwards; the coordinator accepts the first prefab whose live compartments
// fit every surviving member under ALL_OR_FALLBACK.
class AICF_VehicleCatalog
{
	ResourceName SelectPrefab(SCR_CampaignFaction faction, AICF_EVehicleKind kind)
	{
		array<ResourceName> candidates = {};
		if (!GetCandidatePrefabs(faction, kind, candidates) || candidates.IsEmpty())
			return ResourceName.Empty;

		return candidates[0];
	}

	bool GetCandidatePrefabs(
		SCR_CampaignFaction faction,
		AICF_EVehicleKind kind,
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
				string.Format("faction=%1 kind=%2", faction.GetFactionKey(), AICF_Stage3Diagnostics.KindToString(kind)));
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
				string.Format(
					"faction=%1 kind=%2 prefab=%3 reason=RESOURCE_INVALID",
					faction.GetFactionKey(),
					AICF_Stage3Diagnostics.KindToString(kind),
					candidate));
			candidates.Remove(candidateIndex);
		}

		if (!candidates.IsEmpty())
			return true;

		AICF_Stage3Diagnostics.Error(
			"VEHICLE_PREFAB_NOT_FOUND",
			string.Format(
				"faction=%1 kind=%2 catalog_entries=%3",
				faction.GetFactionKey(),
				AICF_Stage3Diagnostics.KindToString(kind),
				entries.Count()));
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
