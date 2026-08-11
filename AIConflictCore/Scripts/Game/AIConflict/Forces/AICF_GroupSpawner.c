// Creates one managed infantry group at a validated friendly Conflict base.
class AICF_GroupSpawner
{
	SCR_AIGroup SpawnGroup(
		SCR_CampaignFaction faction,
		SCR_CampaignMilitaryBaseComponent spawnBase,
		int slotId)
	{
		if (!Replication.IsServer() || !faction || !spawnBase || !spawnBase.GetOwner())
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
		if (!ConfigureManagedRoster(group, AICF_Stage1Config.MANAGED_GROUP_SIZE, sourceRosterSize))
		{
			AICF_Stage35Diagnostics.Error(
				"GROUP_ROSTER_CONFIG_INVALID",
				string.Format(
					"faction=%1 slot=%2 prefab=%3 source_roster=%4 expected=%5",
					faction.GetFactionKey(),
					slotId,
					groupPrefab,
					sourceRosterSize,
					AICF_Stage1Config.MANAGED_GROUP_SIZE));
			RplComponent.DeleteRplEntity(group, false);
			return null;
		}

		group.SetSpawnImmediately(false);
		group.SpawnUnits();

		AICF_Stage35Diagnostics.Info(
			"GROUP_ENTITY_SPAWNED",
			string.Format(
				"faction=%1 slot=%2 base={%3} prefab=%4 source_roster=%5 expected_agents=%6 spawning_pending=1",
				faction.GetFactionKey(),
				slotId,
				AICF_Stage1Diagnostics.DescribeBase(spawnBase),
				groupPrefab,
				sourceRosterSize,
				AICF_Stage1Config.MANAGED_GROUP_SIZE));
		return group;
	}

	protected bool ConfigureManagedRoster(
		SCR_AIGroup group,
		int expectedSize,
		out int sourceRosterSize)
	{
		sourceRosterSize = 0;
		if (!group || expectedSize <= 0 || !group.m_aUnitPrefabSlots)
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

		group.m_aUnitPrefabSlots.Clear();
		int repeatStart;
		if (sourceRosterSize > 1)
			repeatStart = 1;
		int repeatCount = sourceRosterSize - repeatStart;
		for (int memberIndex = 0; memberIndex < expectedSize; memberIndex++)
		{
			int sourceIndex = memberIndex;
			if (sourceIndex >= sourceRosterSize)
				sourceIndex = repeatStart + ((memberIndex - sourceRosterSize) % repeatCount);

			group.m_aUnitPrefabSlots.Insert(sourceRoster[sourceIndex]);
		}

		return group.m_aUnitPrefabSlots.Count() == expectedSize;
	}

	protected vector GetSlotOffset(int slotId)
	{
		switch (slotId % AICF_Stage1Config.GROUP_SLOTS_PER_FACTION)
		{
			case 0:
				return "6 0 6";
			case 1:
				return "-6 0 6";
			case 2:
				return "6 0 -6";
			case 3:
				return "-6 0 -6";
		}

		return vector.Zero;
	}
}
