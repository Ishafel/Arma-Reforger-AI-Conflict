// Только stage: native filters построенного арсенала, heavy provider и вкладки.
modded class AICF_MatchController
{
	protected bool m_bAICFWCSAccessChecked;
	protected int AICF_WCSReadAccess(BaseContainer source, SCR_Faction faction, array<ResourceName> items, array<EEditableEntityLabel> traits, int depth = 0)
	{
		if (!source || depth > 12) return 0;
		int arsenals;
		IEntitySource entity = IEntitySource.Cast(source);
		if (entity)
		{
			for (int i; i < entity.GetComponentCount(); i++)
			{
				IEntityComponentSource component = entity.GetComponent(i);
				typename type = component.GetClassName().ToType();
				if (type.IsInherited(SCR_CampaignBuildingProviderComponent))
				{
					array<EEditableEntityLabel> providerTraits = {};
					component.Get("m_aAvailableTraits", providerTraits);
					foreach (EEditableEntityLabel trait : providerTraits)
						if (!traits.Contains(trait)) traits.Insert(trait);
				}
				if (!type.IsInherited(SCR_ArsenalComponent)) continue;
				SCR_EArsenalItemType itemTypes;
				SCR_EArsenalItemMode itemModes;
				component.Get("m_eSupportedArsenalItemTypes", itemTypes);
				component.Get("m_eSupportedArsenalItemModes", itemModes);
				BaseContainer overwrite = component.GetObject("m_OverwriteArsenalConfig");
				array<SCR_ArsenalItem> filtered = {};
				SCR_ArsenalItemListConfig config;
				if (overwrite)
				{
					config = SCR_ArsenalItemListConfig.Cast(BaseContainerTools.CreateInstanceFromContainer(overwrite));
					bool checkFaction;
					component.Get("m_bCheckFactionForOverwriteArsenalConfig", checkFaction);
					SCR_Faction filterFaction;
					if (checkFaction) filterFaction = faction;
					if (config) filtered = config.GetFilteredArsenalItems(itemTypes, itemModes, -1, filterFaction);
				}
				else filtered = SCR_EntityCatalogManagerComponent.GetInstance().GetFilteredArsenalItems(itemTypes, itemModes, SCR_ArsenalManagerComponent.GetArsenalGameModeType_Static(), faction);
				foreach (SCR_ArsenalItem item : filtered)
					if (!items.Contains(item.GetItemResourceName())) items.Insert(item.GetItemResourceName());
				arsenals++;
			}
		}
		for (int child; child < source.GetNumChildren(); child++)
			arsenals += AICF_WCSReadAccess(source.GetChild(child), faction, items, traits, depth + 1);
		return arsenals;
	}
	override protected void Update()
	{
		super.Update();
		string enabled;
		if (!System.GetCLIParam("aicfWCSInfantryProbe", enabled) || enabled != "1" || !m_bRosterReady || m_bAICFWCSAccessChecked) return;
		m_bAICFWCSAccessChecked = true;
		array<SCR_CampaignFaction> factions = {m_USFaction, m_USSRFaction};
		SCR_CampaignBuildingManagerComponent building = SCR_CampaignBuildingManagerComponent.Cast(GetGame().GetGameMode().FindComponent(SCR_CampaignBuildingManagerComponent));
		foreach (SCR_CampaignFaction faction : factions)
		{
			FactionKey stable = AICF_ContentProfile.GetActive().GetStableFactionKey(faction.GetFactionKey());
			array<ResourceName> arsenalItems = {};
			array<EEditableEntityLabel> traits = {};
			Resource armory = Resource.Load(AICF_ContentProfile.GetActive().GetConstructionPrefab(stable, AICF_EConstructionType.ARMORY));
			int arsenals = AICF_WCSReadAccess(SCR_BaseContainerTools.FindEntitySource(armory), faction, arsenalItems, traits);
			array<ResourceName> required = {};
			AICF_WCSInfantryKit.Clothes(faction.GetFactionKey(), "_SL.et", required);
			foreach (string role : {"_SL.et", "_MG.et", "_AR.et", "_GL.et", "_Medic.et"}) required.Insert(AICF_WCSInfantryKit.Weapon(faction.GetFactionKey(), role));
			int found;
			foreach (ResourceName prefab : required)
			{
				if (arsenalItems.Contains(prefab)) found++;
				else Print("[AICF][WCS_ARSENAL_MISSING] faction=" + stable + " prefab=" + prefab);
			}
			Print(string.Format("[AICF][WCS_ARSENAL_RESULT] faction=%1 arsenals=%2 filtered=%3 required=%4 found=%5", stable, arsenals, arsenalItems.Count(), required.Count(), found));
			Resource heavy = Resource.Load(AICF_ContentProfile.GetActive().GetConstructionPrefab(stable, AICF_EConstructionType.HEAVY_DEPOT));
			AICF_WCSReadAccess(SCR_BaseContainerTools.FindEntitySource(heavy), faction, arsenalItems, traits);
			array<SCR_EntityCatalogEntry> vehicles = {};
			faction.GetFactionEntityCatalogOfType(EEntityCatalogType.VEHICLE).GetEntityList(vehicles);
			int tracked;
			int available;
			foreach (SCR_EntityCatalogEntry vehicle : vehicles)
			{
				ResourceName prefab = vehicle.GetPrefab();
				if (AICF_WCSBuildingVehicles.FactionFor(prefab) != faction.GetFactionKey()) continue;
				tracked++;
				SCR_EditableEntityUIInfo info = SCR_EditableEntityUIInfo.ExtractEditableUIInfoFromPrefab(prefab);
				array<EEditableEntityLabel> labels = {};
				if (info) info.GetEntityLabels(labels);
				bool traitMatch;
				foreach (EEditableEntityLabel trait : traits) if (labels.Contains(trait)) traitMatch = true;
				bool valid = building && building.GetCompositionId(prefab) >= 0 && labels.Contains(faction.GetFactionLabel()) && traitMatch;
				if (valid) available++;
				else Print(string.Format("[AICF][WCS_FACTORY_MISSING] faction=%1 prefab=%2 labels=%3 traits=%4", stable, prefab, labels, traits));
			}
			Print(string.Format("[AICF][WCS_FACTORY_RESULT] faction=%1 tracked=%2 available=%3 traits=%4", stable, tracked, available, traits));
			AICF_LoadoutCatalog catalog = new AICF_LoadoutCatalog(faction);
			foreach (string sourceName : {"Vanilla", "RHS", "WCS"})
			{
				array<string> tabItems = {};
				catalog.List(0, tabItems, 0, sourceName);
				bool valid = !tabItems.IsEmpty();
				foreach (string tabItem : tabItems) if (!catalog.Allows(tabItem) || catalog.Source(tabItem) != sourceName) valid = false;
				Print(string.Format("[AICF][WCS_TAB_RESULT] faction=%1 tab=%2 items=%3 valid=%4", stable, sourceName, tabItems.Count(), valid));
			}
		}
	}
}
