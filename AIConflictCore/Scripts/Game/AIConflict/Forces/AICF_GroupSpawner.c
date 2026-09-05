// Creates one managed infantry group at a validated friendly Conflict base.
class AICF_GroupSpawner
{
	protected ref AICF_ContentProfile m_ContentProfile;
	protected ref array<FactionKey> m_aSourceFactions = {};
	protected ref array<ref array<ResourceName>> m_aSourceRosters = {};

	void AICF_GroupSpawner(AICF_ContentProfile contentProfile = null)
	{
		m_ContentProfile = contentProfile;
		if (!m_ContentProfile)
			m_ContentProfile = AICF_ContentProfile.GetActive();
	}

	SCR_AIGroup SpawnGroup(
		SCR_CampaignFaction faction,
		SCR_CampaignMilitaryBaseComponent spawnBase,
		int slotId,
		int desiredSize,
		bool managedDeployment = true)
	{
		if (!Replication.IsServer() || !faction || !spawnBase || !spawnBase.GetOwner() ||
			desiredSize < AICF_Stage1Config.MIN_GROUP_SIZE ||
			desiredSize > AICF_Stage1Config.MAX_GROUP_SIZE)
		{
			AICF_Stage1Diagnostics.Error("GROUP_SPAWN_INPUT_INVALID", "Spawn requires server authority, faction, and base");
			return null;
		}

		if (spawnBase.GetFaction() != faction)
		{
			AICF_Stage1Diagnostics.Error(
				"GROUP_SPAWN_BASE_HOSTILE",
				string.Format("faction=%1 slot=%2 base={%3}", faction.GetFactionKey(), slotId, AICF_Stage1Diagnostics.DescribeBase(spawnBase)));
			return null;
		}

		SCR_SpawnPoint spawnPoint = spawnBase.GetSpawnPoint();
		if (!spawnPoint)
		{
			AICF_Stage1Diagnostics.Error(
				"GROUP_SPAWN_POINT_MISSING",
				string.Format("faction=%1 slot=%2 base={%3}", faction.GetFactionKey(), slotId, AICF_Stage1Diagnostics.DescribeBase(spawnBase)));
			return null;
		}

		ResourceName groupPrefab = faction.GetDefendersGroupPrefab();
		Resource groupResource = Resource.Load(groupPrefab);
		if (groupPrefab == ResourceName.Empty || !groupResource || !groupResource.IsValid())
		{
			AICF_Stage1Diagnostics.Error(
				"GROUP_PREFAB_INVALID",
				string.Format("faction=%1 slot=%2 prefab=%3", faction.GetFactionKey(), slotId, groupPrefab));
			return null;
		}

		vector position;
		vector rotation;
		spawnPoint.GetPositionAndRotation(position, rotation);
		position = position + GetSlotOffset(slotId);

		EntitySpawnParams spawnParams = new EntitySpawnParams();
		spawnParams.TransformMode = ETransformMode.WORLD;
		Math3D.AnglesToMatrix(rotation, spawnParams.Transform);
		spawnParams.Transform[3] = position;

		// Stock Conflict defender groups contain three faction-correct unit slots.
		// Suppress their automatic member spawn so the authoritative group can be
		// resized before any character entity exists. The flag is process-global and
		// one-shot in stock code, so clear it explicitly after the spawn boundary as
		// well as on failure.
		SCR_AIGroup.IgnoreSpawning(true);
		IEntity spawnedEntity = GetGame().SpawnEntityPrefabEx(groupPrefab, false, params: spawnParams);
		SCR_AIGroup.IgnoreSpawning(false);
		SCR_AIGroup group = SCR_AIGroup.Cast(spawnedEntity);
		if (!group)
		{
			if (spawnedEntity)
				RplComponent.DeleteRplEntity(spawnedEntity, false);

			AICF_Stage1Diagnostics.Error(
				"GROUP_SPAWN_FAILED",
				string.Format("faction=%1 slot=%2 prefab=%3", faction.GetFactionKey(), slotId, groupPrefab));
			return null;
		}

		// Validate the faction before scheduling any delayed member creation. If a
		// faction prefab is ever misconfigured, no wrong-faction characters enter
		// SCR_AIGroup's asynchronous spawn queue.
		Faction groupFaction = group.GetFaction();
		if ((!groupFaction || groupFaction.GetFactionKey() != faction.GetFactionKey()) &&
			m_ContentProfile.AllowsGroupFactionRebinding())
		{
			group.SetFaction(faction);
			groupFaction = group.GetFaction();
			if (groupFaction && groupFaction.GetFactionKey() == faction.GetFactionKey())
			{
				AICF_Stage35Diagnostics.Info(
					"GROUP_FACTION_REBOUND",
					string.Format(
						"faction=%1 slot=%2 profile=%3 timing=PRE_ROSTER_REQUEST",
						faction.GetFactionKey(),
						slotId,
						m_ContentProfile.GetProfileKey()));
			}
		}
		if (!groupFaction || groupFaction.GetFactionKey() != faction.GetFactionKey())
		{
			string actualFactionKey = "NONE";
			if (groupFaction)
				actualFactionKey = groupFaction.GetFactionKey();

			AICF_Stage1Diagnostics.Error(
				"GROUP_FACTION_MISMATCH",
				string.Format(
					"requested=%1 actual=%2 slot=%3 profile=%4",
					faction.GetFactionKey(),
					actualFactionKey,
					slotId,
					m_ContentProfile.GetProfileKey()));
			RplComponent.DeleteRplEntity(group, false);
			return null;
		}

		int sourceRosterSize;
		int fallbackSlots;
		string configuredRoles;
		string configuredPrefabs;
		if (!ConfigureManagedRoster(
			group,
			faction,
			desiredSize,
			sourceRosterSize,
			fallbackSlots,
			configuredRoles,
			configuredPrefabs))
		{
			AICF_Stage35Diagnostics.Error(
				"GROUP_ROSTER_CONFIG_INVALID",
				string.Format(
					"faction=%1 slot=%2 prefab=%3 source_roster=%4 expected=%5",
					faction.GetFactionKey(),
					slotId,
					groupPrefab,
					sourceRosterSize,
					desiredSize));
			RplComponent.DeleteRplEntity(group, false);
			return null;
		}

		// Keep member materialisation behind a second phase. MatchController first
		// binds the group and subscribes its generation-fenced observers, then calls
		// BeginRosterSpawn(). This prevents a completion callback from racing the
		// authoritative managed-slot ownership boundary.
		group.SetSpawnImmediately(false);
		if (!managedDeployment)
			return group;

		AICF_Stage35Diagnostics.Info(
			"GROUP_ROSTER_CONFIGURED",
			string.Format(
				"faction=%1 slot=%2 size=%3 profile=%4 roles=%5 fallback_slots=%6",
				faction.GetFactionKey(),
				slotId,
				desiredSize,
				m_ContentProfile.GetProfileKey(),
				configuredRoles,
				fallbackSlots) + string.Format(" prefabs=%1", configuredPrefabs));

		AICF_Stage35Diagnostics.Info(
			"GROUP_ENTITY_SPAWNED",
			string.Format(
				"faction=%1 slot=%2 group=%3 base={%4} prefab=%5 source_roster=%6 expected_agents=%7 spawn_request_issued=0 pending_owner=AWAITING_MANAGED_BIND",
				faction.GetFactionKey(),
				slotId,
				AICF_GroupRuntime.FormatEntityId(group),
				AICF_Stage1Diagnostics.DescribeBase(spawnBase),
				groupPrefab,
				sourceRosterSize,
				desiredSize));
		return group;
	}

	bool BeginRosterSpawn(SCR_AIGroup group, int expectedSize)
	{
		if (!Replication.IsServer() || !group || expectedSize <= 0 ||
			!group.m_aUnitPrefabSlots || group.m_aUnitPrefabSlots.Count() != expectedSize ||
			group.GetAgentsCount() != 0)
		{
			return false;
		}

		// Arma Reforger 1.8 moved runtime member materialisation to SCR_AIWorld's
		// global queue. Direct SpawnUnits() is now a synchronous fallback: a member
		// blocked by an unloaded navmesh tile is not re-enqueued. RequestSpawn keeps
		// the stock retry/budget/importance ownership and is the production API used
		// by the 1.8 Scenario Framework.
		group.SetNumberOfMembersToSpawn(expectedSize);
		group.RequestSpawn(expectedSize);
		return true;
	}

	// Сохраняем исходный roster до resize, чтобы пополнение использовало тот же
	// stock fallback. RHS profile по-прежнему запрещает fallback fail-closed.
	ResourceName ResolveRecruitPrefab(SCR_CampaignFaction faction, int memberIndex, out string role)
	{
		if (!faction)
			return ResourceName.Empty;
		array<string> suffixes = {};
		if (!m_ContentProfile.BuildCharacterRoleCandidates(
			m_ContentProfile.GetStableFactionKey(faction.GetFactionKey()), memberIndex, role, suffixes))
			return ResourceName.Empty;
		array<SCR_EntityCatalogEntry> entries = {};
		SCR_EntityCatalog catalog = faction.GetFactionEntityCatalogOfType(EEntityCatalogType.CHARACTER);
		if (catalog)
			catalog.GetEntityList(entries);
		ResourceName prefab = FindCharacterPrefab(entries, suffixes);
		if (!prefab.IsEmpty() || !m_ContentProfile.AllowsSourceRosterFallback())
			return prefab;
		int sourceIndex = m_aSourceFactions.Find(faction.GetFactionKey());
		if (sourceIndex < 0)
			return ResourceName.Empty;
		prefab = GetSourceRosterFallback(m_aSourceRosters[sourceIndex], memberIndex);
		// Цена соответствует фактическому бойцу из stock defender roster.
		role = "RIFLEMAN";
		for (int index = 0; index < AICF_Stage1Config.MAX_GROUP_SIZE; index++)
		{
			string candidateRole;
			if (m_ContentProfile.BuildCharacterRoleCandidates(
				m_ContentProfile.GetStableFactionKey(faction.GetFactionKey()), index, candidateRole, suffixes) &&
				FindCharacterPrefab(entries, suffixes) == prefab)
			{
				role = candidateRole;
				break;
			}
		}
		return prefab;
	}

	protected bool ConfigureManagedRoster(
		SCR_AIGroup group,
		SCR_CampaignFaction faction,
		int expectedSize,
		out int sourceRosterSize,
		out int fallbackSlots,
		out string configuredRoles,
		out string configuredPrefabs)
	{
		sourceRosterSize = 0;
		fallbackSlots = 0;
		configuredRoles = string.Empty;
		configuredPrefabs = string.Empty;
		if (!group || !faction || !m_ContentProfile || expectedSize <= 0 ||
			!group.m_aUnitPrefabSlots)
			return false;
		FactionKey stableKey = m_ContentProfile.GetStableFactionKey(faction.GetFactionKey());
		if (stableKey.IsEmpty())
			return false;

		array<ResourceName> sourceRoster = {};
		sourceRoster.Copy(group.m_aUnitPrefabSlots);
		sourceRosterSize = sourceRoster.Count();
		if (sourceRoster.IsEmpty())
			return false;

		foreach (ResourceName unitPrefab : sourceRoster)
		{
			Resource unitResource = Resource.Load(unitPrefab);
			if (unitPrefab.IsEmpty() || !unitResource || !unitResource.IsValid())
				return false;
		}

		array<SCR_EntityCatalogEntry> characterEntries = {};
		int sourceIndex = m_aSourceFactions.Find(faction.GetFactionKey());
		if (sourceIndex < 0)
		{
			m_aSourceFactions.Insert(faction.GetFactionKey());
			m_aSourceRosters.Insert(sourceRoster);
		}
		SCR_EntityCatalog characterCatalog = faction.GetFactionEntityCatalogOfType(EEntityCatalogType.CHARACTER);
		if (characterCatalog)
			characterCatalog.GetEntityList(characterEntries);

		group.m_aUnitPrefabSlots.Clear();
		for (int memberIndex = 0; memberIndex < expectedSize; memberIndex++)
		{
			string role;
			array<string> prefabSuffixes = {};
			if (!m_ContentProfile.BuildCharacterRoleCandidates(
				stableKey,
				memberIndex,
				role,
				prefabSuffixes))
			{
				return false;
			}
			ResourceName selectedPrefab = FindCharacterPrefab(characterEntries, prefabSuffixes);
			if (selectedPrefab.IsEmpty())
			{
				if (!m_ContentProfile.AllowsSourceRosterFallback())
				{
					AICF_Stage35Diagnostics.Error(
						"CONTENT_ROLE_PREFAB_MISSING",
						string.Format(
							"faction=%1 stable_side=%2 profile=%3 member=%4 role=%5 candidates=%6 fallback=DENIED",
							faction.GetFactionKey(),
							stableKey,
							m_ContentProfile.GetProfileKey(),
							memberIndex + 1,
							role,
							JoinSuffixes(prefabSuffixes)));
					return false;
				}
				selectedPrefab = GetSourceRosterFallback(sourceRoster, memberIndex);
				fallbackSlots++;
			}

			if (selectedPrefab.IsEmpty())
				return false;

			if (!configuredRoles.IsEmpty())
			{
				configuredRoles += ",";
				configuredPrefabs += ",";
			}
			configuredRoles += string.Format("%1:%2", memberIndex + 1, role);
			configuredPrefabs += string.Format("%1:%2", memberIndex + 1, selectedPrefab);
			group.m_aUnitPrefabSlots.Insert(selectedPrefab);
		}

		return group.m_aUnitPrefabSlots.Count() == expectedSize;
	}

	protected ResourceName FindCharacterPrefab(
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

				Resource resource = Resource.Load(prefab);
				if (resource && resource.IsValid())
					return prefab;
			}
		}

		return ResourceName.Empty;
	}

	protected string JoinSuffixes(array<string> suffixes)
	{
		string result;
		foreach (string suffix : suffixes)
		{
			if (!result.IsEmpty())
				result += ",";
			result += suffix;
		}
		return result;
	}

	protected ResourceName GetSourceRosterFallback(
		array<ResourceName> sourceRoster,
		int memberIndex)
	{
		int sourceRosterSize = sourceRoster.Count();
		if (sourceRosterSize <= 0)
			return ResourceName.Empty;
		if (memberIndex < sourceRosterSize)
			return sourceRoster[memberIndex];

		int repeatStart;
		if (sourceRosterSize > 1)
			repeatStart = 1;
		int repeatCount = sourceRosterSize - repeatStart;
		return sourceRoster[repeatStart + ((memberIndex - sourceRosterSize) % repeatCount)];
	}

	protected vector GetSlotOffset(int slotId)
	{
		switch (slotId % AICF_Stage1Config.GROUP_SLOTS_PER_FACTION)
		{
			case 0:
				return "12 0 6";
			case 1:
				return "6 0 6";
			case 2:
				return "0 0 6";
			case 3:
				return "-6 0 6";
			case 4:
				return "-12 0 6";
			case 5:
				return "12 0 -6";
			case 6:
				return "6 0 -6";
			case 7:
				return "0 0 -6";
			case 8:
				return "-6 0 -6";
			case 9:
				return "-12 0 -6";
		}

		return vector.Zero;
	}
}
