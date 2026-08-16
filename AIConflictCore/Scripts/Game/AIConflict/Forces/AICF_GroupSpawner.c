// Creates one managed infantry group at a validated friendly Conflict base.
class AICF_GroupSpawner
{
	SCR_AIGroup SpawnGroup(
		SCR_CampaignFaction faction,
		SCR_CampaignMilitaryBaseComponent spawnBase,
		int slotId,
		int desiredSize)
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
		if (!groupFaction || groupFaction.GetFactionKey() != faction.GetFactionKey())
		{
			AICF_Stage1Diagnostics.Error(
				"GROUP_FACTION_MISMATCH",
				string.Format("requested=%1 slot=%2", faction.GetFactionKey(), slotId));
			RplComponent.DeleteRplEntity(group, false);
			return null;
		}

		int sourceRosterSize;
		int fallbackSlots;
		string configuredRoles;
		if (!ConfigureManagedRoster(group, faction, desiredSize, sourceRosterSize, fallbackSlots, configuredRoles))
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

		AICF_Stage35Diagnostics.Info(
			"GROUP_ROSTER_CONFIGURED",
			string.Format(
				"faction=%1 slot=%2 size=%3 roles=%4 fallback_slots=%5",
				faction.GetFactionKey(),
				slotId,
				desiredSize,
				configuredRoles,
				fallbackSlots));

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

	protected bool ConfigureManagedRoster(
		SCR_AIGroup group,
		SCR_CampaignFaction faction,
		int expectedSize,
		out int sourceRosterSize,
		out int fallbackSlots,
		out string configuredRoles)
	{
		sourceRosterSize = 0;
		fallbackSlots = 0;
		configuredRoles = string.Empty;
		if (!group || !faction || expectedSize <= 0 || !group.m_aUnitPrefabSlots)
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
		SCR_EntityCatalog characterCatalog = faction.GetFactionEntityCatalogOfType(EEntityCatalogType.CHARACTER);
		if (characterCatalog)
			characterCatalog.GetEntityList(characterEntries);

		group.m_aUnitPrefabSlots.Clear();
		for (int memberIndex = 0; memberIndex < expectedSize; memberIndex++)
		{
			string role;
			string prefabSuffix;
			BuildRoleSlot(faction.GetFactionKey(), memberIndex, role, prefabSuffix);
			ResourceName selectedPrefab = FindCharacterPrefab(characterEntries, prefabSuffix);
			if (selectedPrefab.IsEmpty())
			{
				selectedPrefab = GetSourceRosterFallback(sourceRoster, memberIndex);
				fallbackSlots++;
			}

			if (selectedPrefab.IsEmpty())
				return false;

			if (!configuredRoles.IsEmpty())
				configuredRoles += ",";
			configuredRoles += string.Format("%1:%2", memberIndex + 1, role);
			group.m_aUnitPrefabSlots.Insert(selectedPrefab);
		}

		return group.m_aUnitPrefabSlots.Count() == expectedSize;
	}

	// A managed squad has a stable ten-position table. Smaller squads take the
	// prefix, so slot 1 is always command and the first four always contain the
	// commander, medic, machine-gunner and anti-tank specialist requested by the
	// match configuration.
	protected void BuildRoleSlot(
		FactionKey factionKey,
		int memberIndex,
		out string role,
		out string prefabSuffix)
	{
		string prefix = "Character_US_";
		if (factionKey == "USSR")
			prefix = "Character_USSR_";

		switch (memberIndex)
		{
			case 0:
				role = "SQUAD_LEADER";
				prefabSuffix = prefix + "SL.et";
				return;
			case 1:
				role = "MEDIC";
				prefabSuffix = prefix + "Medic.et";
				return;
			case 2:
				role = "MACHINE_GUNNER";
				prefabSuffix = prefix + "MG.et";
				return;
			case 3:
				role = "ANTI_TANK";
				prefabSuffix = prefix + "AT.et";
				return;
			case 4:
				role = "GRENADIER";
				prefabSuffix = prefix + "GL.et";
				return;
			case 5:
				role = "AUTOMATIC_RIFLEMAN";
				prefabSuffix = prefix + "AR.et";
				return;
			case 6:
				if (factionKey == "USSR")
				{
					role = "SENIOR_RIFLEMAN";
					prefabSuffix = prefix + "SR.et";
				}
				else
				{
					role = "TEAM_LEADER";
					prefabSuffix = prefix + "TL.et";
				}
				return;
			case 7:
				role = "MACHINE_GUNNER_ASSISTANT";
				prefabSuffix = prefix + "AMG.et";
				return;
			case 8:
				role = "ANTI_TANK_ASSISTANT";
				prefabSuffix = prefix + "AAT.et";
				return;
		}

		role = "RIFLEMAN";
		prefabSuffix = prefix + "Rifleman.et";
	}

	protected ResourceName FindCharacterPrefab(
		array<SCR_EntityCatalogEntry> entries,
		string prefabSuffix)
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

		return ResourceName.Empty;
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
