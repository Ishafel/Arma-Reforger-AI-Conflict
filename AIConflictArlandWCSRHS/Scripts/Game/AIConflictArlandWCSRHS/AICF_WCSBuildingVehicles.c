// Завод использует building registry, а не только faction VEHICLE catalog.
// Дополнение одного и того же registry сохраняет native numeric IDs на peers.
class AICF_WCSBuildingVehicles
{
	static FactionKey FactionFor(ResourceName prefab)
	{
		if (!prefab.Contains("/Tracked/")) return FactionKey.Empty;
		if (AICF_WCSVehicleCatalog.IsSupported(prefab, "RHS_USAF")) return "RHS_USAF";
		if (AICF_WCSVehicleCatalog.IsSupported(prefab, "RHS_AFRF")) return "RHS_AFRF";
		return FactionKey.Empty;
	}

	static void Append(array<ResourceName> prefabs)
	{
		if (!prefabs || !prefabs.Contains("{4CBE94C72D8A4C6B}PrefabsEditable/Auto/ConflictRHS/E_VehicleMaintenance_M_AFRF_01.et")) return;
		array<ResourceName> catalogs = {
			"{D83A5EFEEF6CBF61}Configs/EntityCatalog/US/Vehicles_EntityCatalog_US.conf",
			"{09C2ED5F617A0390}Configs/EntityCatalog/USSR/Vehicles_EntityCatalog_USSR.conf"
		};
		array<ResourceName> additions = {};
		foreach (ResourceName catalogPath : catalogs)
		{
			Resource resource = Resource.Load(catalogPath);
			if (!resource || !resource.IsValid()) continue;
			SCR_EntityCatalog catalog = SCR_EntityCatalog.Cast(BaseContainerTools.CreateInstanceFromContainer(resource.GetResource().ToBaseContainer()));
			array<SCR_EntityCatalogEntry> entries = {};
			AICF_WCSVehicleCatalog.Entries(catalog, entries);
			foreach (SCR_EntityCatalogEntry entry : entries)
			{
				ResourceName prefab = entry.GetPrefab();
				if (!entry.IsEnabled() || FactionFor(prefab).IsEmpty() || prefabs.Contains(prefab) || additions.Contains(prefab) ||
					!entry.GetEntityDataOfType(SCR_EntityCatalogSpawnerData)) continue;
				additions.Insert(prefab);
			}
		}
		additions.Sort();
		prefabs.InsertAll(additions);
	}
}

[BaseContainerProps(configRoot: true), SCR_PlaceableEntitiesRegistryTitleField()]
modded class SCR_PlaceableEntitiesRegistry
{
	protected bool m_bAICFWCSRegistryPrepared;
	override array<ResourceName> GetPrefabs(bool onlyExposed = false)
	{
		array<ResourceName> prefabs = super.GetPrefabs(onlyExposed);
		if (!m_bAICFWCSRegistryPrepared && (!onlyExposed || IsExposed()))
		{
			AICF_WCSBuildingVehicles.Append(prefabs);
			m_bAICFWCSRegistryPrepared = true;
		}
		return prefabs;
	}
}

// В этом сценарии vanilla US/USSR представлены фракциями RHS. Переводим
// только vehicle labels; цены, traits и ограничения provider остаются native.
[BaseContainerProps(), SCR_BaseContainerLocalizedTitleField("Name")]
modded class SCR_EditableVehicleUIInfo
{
	override int GetEntityLabels(out notnull array<EEditableEntityLabel> entityLabels)
	{
		super.GetEntityLabels(entityLabels);
		AICF_WCSRHSContentProfile profile = AICF_WCSRHSContentProfile.Cast(AICF_ContentProfile.GetActive());
		if (!profile || !GetGame().GetFactionManager()) return entityLabels.Count();
		array<FactionKey> keys = {"US", "USSR"};
		array<EEditableEntityLabel> labels = {EEditableEntityLabel.FACTION_US, EEditableEntityLabel.FACTION_USSR};
		foreach (int index, FactionKey key : keys)
		{
			if (!entityLabels.Contains(labels[index])) continue;
			SCR_Faction faction = SCR_Faction.Cast(GetGame().GetFactionManager().GetFactionByKey(profile.GetRuntimeFactionKey(key)));
			if (!faction) continue;
			entityLabels.RemoveItem(labels[index]);
			if (!entityLabels.Contains(faction.GetFactionLabel())) entityLabels.Insert(faction.GetFactionLabel());
		}
		return entityLabels.Count();
	}
	override bool HasEntityLabel(EEditableEntityLabel label)
	{
		array<EEditableEntityLabel> labels = {};
		GetEntityLabels(labels);
		return labels.Contains(label);
	}
}
modded class SCR_CampaignBuildingPlacingEditorComponent
{
	override protected void OnEntityCreatedServer(array<SCR_EditableEntityComponent> entities)
	{
		super.OnEntityCreatedServer(entities);
		if (!Replication.IsServer() || !AICF_WCSRHSContentProfile.Cast(AICF_ContentProfile.GetActive())) return;
		foreach (SCR_EditableEntityComponent editable : entities)
		{
			if (!editable || !editable.GetOwner()) continue;
			IEntity entity = editable.GetOwner();
			FactionKey key = AICF_WCSBuildingVehicles.FactionFor(SCR_ResourceNameUtils.GetPrefabName(entity));
			if (key.IsEmpty()) continue;
			RplComponent rpl = RplComponent.Cast(entity.FindComponent(RplComponent));
			FactionAffiliationComponent affiliation = FactionAffiliationComponent.Cast(entity.FindComponent(FactionAffiliationComponent));
			Faction faction = GetGame().GetFactionManager().GetFactionByKey(key);
			if (!rpl || !rpl.IsMaster() || !affiliation || !faction) continue;
			Faction current = affiliation.GetAffiliatedFaction();
			if (current == faction) continue;
			if (current && current.GetFactionKey() != AICF_ContentProfile.GetActive().GetStableFactionKey(key)) continue;
			affiliation.SetAffiliatedFaction(faction);
		}
	}
}
