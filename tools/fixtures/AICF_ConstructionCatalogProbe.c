// Только terminal catalog probe: registry, labels и stock budgets.
// Полная геометрия проверяется production metadata в отдельном runtime probe.
modded class AICF_BaseBuilderService
{
	protected ref array<ResourceName> m_aAICFCatalog;
	protected int m_iAICFCatalogCursor;

	override void Update()
	{
		string mode;
		if (!System.GetCLIParam("aicfConstructionCatalog", mode) || mode != "1")
		{
			super.Update();
			return;
		}
		if (!m_aAICFCatalog)
		{
			m_aAICFCatalog = m_BuildingManager.GetPlaceablePrefabs();
			array<SCR_CampaignMilitaryBaseComponent> bases = {};
			m_Campaign.GetBaseManager().GetBases(bases);
			foreach (SCR_CampaignMilitaryBaseComponent base : bases)
			{
				SCR_CampaignBuildingProviderComponent provider = base.GetMasterProvider();
				if (!provider)
					continue;
				Print(string.Format("[AICF][CONSTRUCTION_CATALOG_BASE] base=%1 provider=%2 faction=%3 radius=%4 supplies=%5 traits=%6",
					AICF_Stage1Diagnostics.BaseKey(base), provider.GetOwner().GetID(), base.GetFaction(), provider.GetBuildingRadius(), base.GetSupplies(), provider.GetAvailableTraits()));
			}
		}
		int processed;
		while (m_iAICFCatalogCursor < m_aAICFCatalog.Count() && processed < 12)
		{
			ResourceName prefab = m_aAICFCatalog[m_iAICFCatalogCursor++];
			processed++;
			SCR_EditableEntityUIInfo info = SCR_EditableEntityUIInfo.ExtractEditableUIInfoFromPrefab(prefab);
			if (!info || (!info.HasEntityLabel(EEditableEntityLabel.SERVICE_LIVING_AREA) &&
				!info.HasEntityLabel(EEditableEntityLabel.SERVICE_ARMORY) &&
				!info.HasEntityLabel(EEditableEntityLabel.SERVICE_VEHICLE_DEPOT_LIGHT) &&
				!info.HasEntityLabel(EEditableEntityLabel.SERVICE_VEHICLE_DEPOT_HEAVY)))
				continue;
			array<EEditableEntityLabel> labels = {};
			info.GetEntityLabels(labels);
			string labelText;
			foreach (EEditableEntityLabel label : labels)
				labelText += typename.EnumToString(EEditableEntityLabel, label) + ",";
			array<ref SCR_EntityBudgetValue> budgets = {};
			info.GetEntityAndChildrenBudgetCost(budgets);
			string budgetText;
			foreach (SCR_EntityBudgetValue budget : budgets)
				budgetText += typename.EnumToString(EEditableEntityBudget, budget.GetBudgetType()) + "=" + budget.GetBudgetValue() + ",";
			Print(string.Format("[AICF][CONSTRUCTION_CATALOG] id=%1 prefab=%2 faction=%3 name=%4 labels=%5 budgets=%6",
				m_iAICFCatalogCursor - 1, prefab, info.GetFactionKey(), info.GetName(), labelText, budgetText));
		}
		if (m_iAICFCatalogCursor >= m_aAICFCatalog.Count())
		{
			Print("[AICF][CONSTRUCTION_CATALOG_DONE] complete=1");
			GetGame().RequestClose();
		}
	}
}
