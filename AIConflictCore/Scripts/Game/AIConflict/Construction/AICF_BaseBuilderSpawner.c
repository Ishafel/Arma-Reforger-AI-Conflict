// Вспомогательный одиночный roster; армейские slots и reinforcement не меняются.
class AICF_BaseBuilderSpawner : AICF_GroupSpawner
{
	SCR_AIGroup CreateBuilder(SCR_CampaignFaction faction, vector position)
	{
		if (!Replication.IsServer() || !faction)
			return null;

		AICF_ContentProfile profile = AICF_ContentProfile.GetActive();
		array<string> suffixes = {};
		string role;
		if (!profile.BuildCharacterRoleCandidates(profile.GetStableFactionKey(faction.GetFactionKey()), 9, role, suffixes))
			return null;
		SCR_EntityCatalog catalog = faction.GetFactionEntityCatalogOfType(EEntityCatalogType.CHARACTER);
		if (!catalog)
			return null;
		array<SCR_EntityCatalogEntry> entries = {};
		catalog.GetEntityList(entries);
		ResourceName characterPrefab = FindCharacterPrefab(entries, suffixes);
		ResourceName groupPrefab = faction.GetDefendersGroupPrefab();
		Resource resource = Resource.Load(groupPrefab);
		if (characterPrefab.IsEmpty() || !resource || !resource.IsValid())
			return null;

		EntitySpawnParams params = new EntitySpawnParams();
		params.TransformMode = ETransformMode.WORLD;
		params.Transform[3] = position;
		SCR_AIGroup.IgnoreSpawning(true);
		IEntity entity = GetGame().SpawnEntityPrefabEx(groupPrefab, false, params: params);
		SCR_AIGroup.IgnoreSpawning(false);
		SCR_AIGroup group = SCR_AIGroup.Cast(entity);
		if (!group)
		{
			if (entity)
				RplComponent.DeleteRplEntity(entity, false);
			return null;
		}
		if (group.GetFaction() != faction && profile.AllowsGroupFactionRebinding())
			group.SetFaction(faction);
		if (group.GetFaction() != faction || !group.m_aUnitPrefabSlots || group.GetAgentsCount() != 0)
		{
			RplComponent.DeleteRplEntity(group, false);
			return null;
		}
		group.SetLifecyclePolicy(SCR_EAIGroupLifecyclePolicy.Manual);
		group.SetSpawnImmediately(false);
		group.m_aUnitPrefabSlots.Clear();
		group.m_aUnitPrefabSlots.Insert(characterPrefab);
		return group;
	}
}
