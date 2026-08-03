// Creates the single Stage 0 group for a faction. It intentionally has no reinforcement logic.
class AICF_TestGroupSpawner
{
	SCR_AIGroup SpawnTestGroup(SCR_CampaignFaction faction)
	{
		if (!Replication.IsServer())
		{
			AICF_Diagnostics.Error("GROUP_CLIENT_SPAWN_BLOCKED", "Test groups may only be created by the server");
			return null;
		}

		if (!faction)
		{
			AICF_Diagnostics.Error("GROUP_FACTION_MISSING", "Cannot create a group for a null faction");
			return null;
		}

		SCR_CampaignMilitaryBaseComponent hq = faction.GetMainBase();
		if (!hq || !hq.GetOwner())
		{
			AICF_Diagnostics.Error("GROUP_HQ_MISSING", string.Format("Faction %1 has no valid HQ", faction.GetFactionKey()));
			return null;
		}

		SCR_SpawnPoint spawnPoint = hq.GetSpawnPoint();
		if (!spawnPoint)
		{
			AICF_Diagnostics.Error(
				"GROUP_SPAWN_POINT_MISSING",
				string.Format("Faction %1 HQ has no SCR_SpawnPoint", faction.GetFactionKey()));
			return null;
		}

		ResourceName groupPrefab = faction.GetDefendersGroupPrefab();
		if (groupPrefab == ResourceName.Empty)
		{
			AICF_Diagnostics.Error(
				"GROUP_PREFAB_MISSING",
				string.Format("Faction %1 has no defenders group prefab", faction.GetFactionKey()));
			return null;
		}

		Resource groupResource = Resource.Load(groupPrefab);
		if (!groupResource || !groupResource.IsValid())
		{
			AICF_Diagnostics.Error(
				"GROUP_PREFAB_INVALID",
				string.Format("Faction %1 defenders group resource is invalid: %2", faction.GetFactionKey(), groupPrefab));
			return null;
		}

		vector position;
		vector rotation;
		spawnPoint.GetPositionAndRotation(position, rotation);

		EntitySpawnParams spawnParams = new EntitySpawnParams();
		spawnParams.TransformMode = ETransformMode.WORLD;
		Math3D.AnglesToMatrix(rotation, spawnParams.Transform);
		spawnParams.Transform[3] = position;

		IEntity spawnedEntity = GetGame().SpawnEntityPrefabEx(groupPrefab, false, params: spawnParams);
		SCR_AIGroup group = SCR_AIGroup.Cast(spawnedEntity);
		if (!group)
		{
			if (spawnedEntity)
				RplComponent.DeleteRplEntity(spawnedEntity, false);

			AICF_Diagnostics.Error(
				"GROUP_SPAWN_FAILED",
				string.Format("SpawnEntityPrefabEx did not return an SCR_AIGroup for faction %1 prefab %2", faction.GetFactionKey(), groupPrefab));
			return null;
		}

		if (!group.GetSpawnImmediately())
			group.SpawnUnits();

		Faction groupFaction = group.GetFaction();
		if (!groupFaction || groupFaction.GetFactionKey() != faction.GetFactionKey())
		{
			string actualFaction = "NONE";
			if (groupFaction)
				actualFaction = groupFaction.GetFactionKey();

			AICF_Diagnostics.Error(
				"GROUP_FACTION_MISMATCH",
				string.Format("Requested faction %1, spawned group faction %2", faction.GetFactionKey(), actualFaction));
			RplComponent.DeleteRplEntity(group, false);
			return null;
		}

		AICF_Diagnostics.Info(
			"GROUP_CREATED",
			string.Format("faction=%1 prefab=%2 immediate=%3 initial_agents=%4",
				faction.GetFactionKey(),
				groupPrefab,
				group.GetSpawnImmediately(),
				group.GetAgentsCount()));
		return group;
	}
}
