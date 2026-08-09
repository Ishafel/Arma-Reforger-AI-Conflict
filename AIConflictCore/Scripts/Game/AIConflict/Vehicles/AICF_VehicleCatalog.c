// Resolves exact faction-owned ResourceNames from the stock vehicle catalog.
// Matching a verified path suffix preserves the GUID provided by the loaded
// 1.7 catalog and avoids embedding guessed resource identifiers in the addon.
class AICF_VehicleCatalog
{
	ResourceName SelectPrefab(SCR_CampaignFaction faction, AICF_EVehicleKind kind)
	{
		if (!faction)
			return ResourceName.Empty;

		SCR_EntityCatalog catalog = faction.GetFactionEntityCatalogOfType(EEntityCatalogType.VEHICLE);
		if (!catalog)
		{
			AICF_Stage3Diagnostics.Error(
				"VEHICLE_CATALOG_MISSING",
				string.Format("faction=%1 kind=%2", faction.GetFactionKey(), AICF_Stage3Diagnostics.KindToString(kind)));
			return ResourceName.Empty;
		}

		array<SCR_EntityCatalogEntry> entries = {};
		catalog.GetEntityList(entries);
		ResourceName result;
		if (kind == AICF_EVehicleKind.TRANSPORT)
			result = FindTransport(entries, faction.GetFactionKey());
		else
			result = FindArmedLight(entries, faction.GetFactionKey());

		if (result.IsEmpty())
		{
			AICF_Stage3Diagnostics.Error(
				"VEHICLE_PREFAB_NOT_FOUND",
				string.Format("faction=%1 kind=%2 catalog_entries=%3", faction.GetFactionKey(), AICF_Stage3Diagnostics.KindToString(kind), entries.Count()));
			return ResourceName.Empty;
		}

		Resource resource = Resource.Load(result);
		if (!resource || !resource.IsValid())
		{
			AICF_Stage3Diagnostics.Error(
				"VEHICLE_PREFAB_INVALID",
				string.Format("faction=%1 kind=%2 prefab=%3", faction.GetFactionKey(), AICF_Stage3Diagnostics.KindToString(kind), result));
			return ResourceName.Empty;
		}

		return result;
	}

	protected ResourceName FindTransport(array<SCR_EntityCatalogEntry> entries, FactionKey factionKey)
	{
		array<string> suffixes = {};
		if (factionKey == "US")
		{
			suffixes.Insert("Prefabs/Vehicles/Wheeled/M923A1/M923A1_transport.et");
			suffixes.Insert("Prefabs/Vehicles/Wheeled/M998/M998_covered_long.et");
		}
		else if (factionKey == "USSR")
		{
			suffixes.Insert("Prefabs/Vehicles/Wheeled/Ural4320/Ural4320_transport.et");
			suffixes.Insert("Prefabs/Vehicles/Wheeled/UAZ452/UAZ452_transport.et");
		}

		return FindBySuffix(entries, suffixes);
	}

	protected ResourceName FindArmedLight(array<SCR_EntityCatalogEntry> entries, FactionKey factionKey)
	{
		array<string> suffixes = {};
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

		return FindBySuffix(entries, suffixes);
	}

	protected ResourceName FindBySuffix(
		array<SCR_EntityCatalogEntry> entries,
		array<string> suffixes)
	{
		foreach (string suffix : suffixes)
		{
			foreach (SCR_EntityCatalogEntry entry : entries)
			{
				if (!entry)
					continue;

				ResourceName prefab = entry.GetPrefab();
				if (!prefab.IsEmpty() && prefab.Contains(suffix))
					return prefab;
			}
		}

		return ResourceName.Empty;
	}
}
