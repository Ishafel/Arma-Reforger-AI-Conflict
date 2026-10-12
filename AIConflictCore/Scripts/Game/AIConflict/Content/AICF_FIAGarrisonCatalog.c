// Только ресурсы текущего FIA catalog и явно разрешённые profile assets.
class AICF_FIAGarrisonTemplate
{
	ResourceName m_sPrefab;
	ref array<ResourceName> m_aMembers = {};
}

class AICF_FIAGarrisonCatalog
{
	ref array<ResourceName> m_aAPCs = {};
	ref array<ResourceName> m_aTanks = {};
	ref array<ref AICF_FIAGarrisonTemplate> m_aPassengers = {};

	static Resource LoadExact(ResourceName prefab)
	{
		if (prefab.IsEmpty()) return null;
		Resource resource = Resource.Load(prefab);
		if (!resource || !resource.IsValid() || !resource.GetResource() || resource.GetResource().GetResourceName() != prefab) return null;
		return resource;
	}

	static bool HasFIAIdentity(Resource resource)
	{
		IEntitySource entitySource = resource.GetResource().ToEntitySource();
		if (!entitySource) return false;
		IEntityComponentSource source = SCR_BaseContainerTools.FindComponentSource(entitySource, FactionAffiliationComponent);
		string faction;
		if (source) return source.Get("faction affiliation", faction) && faction == "FIA";
		source = SCR_BaseContainerTools.FindComponentSource(entitySource, SCR_FactionAffiliationComponent);
		return source && source.Get("m_DefaultFaction", faction) && faction == "FIA";
	}

	bool Build(SCR_CampaignFaction faction, AICF_ContentProfile profile)
	{
		if (!Replication.IsServer() || !faction || faction.GetFactionKey() != "FIA" || !profile) return false;
		array<ResourceName> apcs = {};
		array<ResourceName> tanks = {};
		profile.GetFIAGarrisonCandidates(faction, apcs, tanks);
		ValidateVehicles(apcs, m_aAPCs);
		ValidateVehicles(tanks, m_aTanks);
		SCR_EntityCatalog catalog = faction.GetFactionEntityCatalogOfType(EEntityCatalogType.GROUP);
		array<SCR_EntityCatalogEntry> entries = {};
		if (catalog) catalog.GetEntityList(entries);
		array<ResourceName> prefabs = {};
		foreach (SCR_EntityCatalogEntry entry : entries)
		{
			if (entry && entry.IsEnabled() && !prefabs.Contains(entry.GetPrefab())) prefabs.Insert(entry.GetPrefab());
		}
		prefabs.Sort();
		foreach (ResourceName prefab : prefabs)
		{
			Resource resource = LoadExact(prefab);
			if (!resource) continue;
			IEntitySource source = resource.GetResource().ToEntitySource();
			if (!source || !source.GetClassName().ToType().IsInherited(SCR_AIGroup)) continue;
			AICF_FIAGarrisonTemplate template = new AICF_FIAGarrisonTemplate();
			template.m_sPrefab = prefab;
			if (!source.Get("m_aUnitPrefabSlots", template.m_aMembers) || template.m_aMembers.IsEmpty()) continue;
			bool valid = true;
			foreach (ResourceName member : template.m_aMembers)
			{
				Resource character = LoadExact(member);
				if (!character || !HasFIAIdentity(character)) { valid = false; break; }
				IEntitySource memberSource = character.GetResource().ToEntitySource();
				if (!memberSource || !memberSource.GetClassName().ToType().IsInherited(ChimeraCharacter)) { valid = false; break; }
			}
			if (valid) m_aPassengers.Insert(template);
		}
		AICF_Stage3Diagnostics.Info("FIA_GARRISON_CATALOG", string.Format("profile=%1 apcs=%2 tanks=%3 passenger_templates=%4", profile.GetProfileKey(), m_aAPCs.Count(), m_aTanks.Count(), m_aPassengers.Count()));
		return !m_aAPCs.IsEmpty() && !m_aPassengers.IsEmpty();
	}

	protected void ValidateVehicles(array<ResourceName> candidates, array<ResourceName> result)
	{
		foreach (ResourceName prefab : candidates)
		{
			Resource resource = LoadExact(prefab);
			if (!resource || !HasFIAIdentity(resource) || result.Contains(prefab)) continue;
			IEntitySource source = resource.GetResource().ToEntitySource();
			if (source && source.GetClassName().ToType().IsInherited(Vehicle)) result.Insert(prefab);
		}
		result.Sort();
	}
}
