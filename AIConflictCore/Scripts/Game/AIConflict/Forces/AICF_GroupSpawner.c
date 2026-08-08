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

		IEntity spawnedEntity = GetGame().SpawnEntityPrefabEx(groupPrefab, false, params: spawnParams);
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

		if (!group.GetSpawnImmediately())
			group.SpawnUnits();

		Faction groupFaction = group.GetFaction();
		if (!groupFaction || groupFaction.GetFactionKey() != faction.GetFactionKey())
		{
			AICF_Stage1Diagnostics.Error(
				"GROUP_FACTION_MISMATCH",
				string.Format("requested=%1 slot=%2", faction.GetFactionKey(), slotId));
			RplComponent.DeleteRplEntity(group, false);
			return null;
		}

		AICF_Stage1Diagnostics.Info(
			"GROUP_SPAWNED",
			string.Format(
				"faction=%1 slot=%2 base={%3} prefab=%4 initial_agents=%5",
				faction.GetFactionKey(),
				slotId,
				AICF_Stage1Diagnostics.DescribeBase(spawnBase),
				groupPrefab,
				group.GetAgentsCount()));
		return group;
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
