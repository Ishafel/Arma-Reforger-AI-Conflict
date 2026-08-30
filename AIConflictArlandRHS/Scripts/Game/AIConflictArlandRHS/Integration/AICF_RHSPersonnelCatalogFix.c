// RHS character catalogs use their own role labels, while the stock Conflict
// personnel service filters entries by vanilla labels. Reuse the deterministic
// RHS content-profile table to bind only catalog-backed, spawnable characters
// to character spawners. Vehicle and other catalog spawners retain vanilla
// behavior through super.SetCurrentFactionCatalog().
modded class SCR_CatalogEntitySpawnerComponent
{
	override protected void SetCurrentFactionCatalog()
	{
		super.SetCurrentFactionCatalog();

		AICF_ContentProfile contentProfile = AICF_ContentProfile.GetActive();
		if (!contentProfile ||
			contentProfile.GetProfileKey() != "RHS_USMC_MSV_0_16_5150" ||
			!m_aCatalogTypes ||
			m_aCatalogTypes.Count() != 1 ||
			!m_aCatalogTypes.Contains(EEntityCatalogType.CHARACTER))
		{
			return;
		}

		SCR_Faction faction = SCR_Faction.Cast(GetFaction());
		if (!faction)
			return;

		FactionKey stableFactionKey = contentProfile.GetStableFactionKey(
			faction.GetFactionKey());
		if (stableFactionKey.IsEmpty())
			return;

		SCR_EntityCatalog characterCatalog = faction.GetFactionEntityCatalogOfType(
			EEntityCatalogType.CHARACTER);
		if (!characterCatalog)
			return;

		array<SCR_EntityCatalogEntry> characterEntries = {};
		characterCatalog.GetEntityList(characterEntries);
		array<SCR_EntityCatalogEntry> personnelEntries = {};
		string roles;
		int missingRoles;
		for (int memberIndex = 0; memberIndex < AICF_Stage1Config.DEFAULT_GROUP_SIZE; memberIndex++)
		{
			string role;
			array<string> prefabSuffixes = {};
			if (!contentProfile.BuildCharacterRoleCandidates(
				stableFactionKey,
				memberIndex,
				role,
				prefabSuffixes))
			{
				missingRoles++;
				continue;
			}

			SCR_EntityCatalogEntry personnelEntry = AICF_FindPersonnelEntry(
				characterEntries,
				prefabSuffixes);
			if (!personnelEntry || personnelEntries.Contains(personnelEntry))
			{
				missingRoles++;
				continue;
			}

			personnelEntries.Insert(personnelEntry);
			if (!roles.IsEmpty())
				roles += ",";
			roles += role;
		}

		if (personnelEntries.IsEmpty())
		{
			Print(
				string.Format(
					"[AICF][CONTENT][ERROR][PERSONNEL_CATALOG_BIND_FAILED] profile=%1 faction=%2 stable_side=%3 catalog_entries=%4 missing_roles=%5",
					contentProfile.GetProfileKey(),
					faction.GetFactionKey(),
					stableFactionKey,
					characterEntries.Count(),
					missingRoles),
				LogLevel.ERROR);
			return;
		}

		int originalCount = m_aAssetList.Count();
		m_aAssetList.Clear();
		m_aAssetList.InsertAll(personnelEntries);
		AssignUserActions();
		Print(
			string.Format(
				"[AICF][CONTENT][INFO][PERSONNEL_CATALOG_BOUND] profile=%1 faction=%2 stable_side=%3 original_count=%4 bound_count=%5 missing_roles=%6 roles=%7",
				contentProfile.GetProfileKey(),
				faction.GetFactionKey(),
				stableFactionKey,
				originalCount,
				personnelEntries.Count(),
				missingRoles,
				roles),
			LogLevel.NORMAL);
	}

	protected SCR_EntityCatalogEntry AICF_FindPersonnelEntry(
		array<SCR_EntityCatalogEntry> entries,
		array<string> prefabSuffixes)
	{
		foreach (string prefabSuffix : prefabSuffixes)
		{
			foreach (SCR_EntityCatalogEntry entry : entries)
			{
				if (!entry)
					continue;

				ResourceName prefab = entry.GetPrefab();
				if (prefab.IsEmpty() || !prefab.Contains(prefabSuffix))
					continue;
				if (!entry.GetEntityUiInfo())
					continue;
				if (!SCR_EntityCatalogSpawnerData.Cast(
					entry.GetEntityDataOfType(SCR_EntityCatalogSpawnerData)))
				{
					continue;
				}

				return entry;
			}
		}

		return null;
	}
}
