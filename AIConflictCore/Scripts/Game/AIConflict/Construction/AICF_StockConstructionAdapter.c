// Receipt принадлежит принятой stock composition и живёт вместе с ней, в том
// числе после capture/Stop planner. Stock RplSave сохраняет provider для JIP.
modded class SCR_CampaignBuildingCompositionComponent
{
	ref AICF_ConstructionOrder m_AICFConstructionReceipt;
}

modded class SCR_ServicePointComponent
{
	static bool AICF_IsSpawnAsOffline()
	{
		return s_bSpawnAsOffline;
	}
}

// Чистый эквивалент registry lookup без фиктивной editable/gameplay entity.
[BaseContainerProps(configRoot: true), SCR_BaseContainerCustomTitleResourceName("", true)]
modded class SCR_CampaignBuildingCompositionOutlineManager
{
	ResourceName AICF_ResolveOutline(ResourceName prefab, SCR_EditableEntityUIInfo info)
	{
		foreach (SCR_CampaignBuildingCompositionOutline entry : m_aCompositionLayouts)
		{
			if (entry.GetEditableEntity() == prefab)
				return entry.GetCompositionLayout();
		}
		if (info.HasEntityLabel(EEditableEntityLabel.SLOT_FLAT_SMALL))
			return m_sSlotFlatSmallLayout;
		if (info.HasEntityLabel(EEditableEntityLabel.SLOT_FLAT_MEDIUM))
			return m_sSlotFlatMediumLayout;
		if (info.HasEntityLabel(EEditableEntityLabel.SLOT_FLAT_LARGE))
			return m_sSlotFlatLargeLayout;
		return ResourceName.Empty;
	}
}

modded class SCR_CampaignBuildingManagerComponent
{
	void AICF_ApplyConstructionBudget(AICF_ConstructionOrder order)
	{
		if (!Replication.IsServer() || !order || order.m_bPaid || !order.m_Composition ||
			order.m_Composition.m_AICFConstructionReceipt != order)
			return;
		SCR_EditableEntityComponent editable = SCR_EditableEntityComponent.Cast(order.m_Composition.GetOwner().FindComponent(SCR_EditableEntityComponent));
		if (editable)
			super.OnEntityCoreBudgetUpdated(EEditableEntityBudget.CAMPAIGN, 0, order.m_iCost, order.m_iCost, editable);
	}

	override protected void OnEntityCoreBudgetUpdated(EEditableEntityBudget entityBudget, int originalBudgetValue, int budgetChange, int updatedBudgetValue, SCR_EditableEntityComponent entity)
	{
		if (entity && entity.GetOwner())
		{
			SCR_CampaignBuildingCompositionComponent composition = SCR_CampaignBuildingCompositionComponent.Cast(entity.GetOwner().FindComponent(SCR_CampaignBuildingCompositionComponent));
			if (composition && composition.m_AICFConstructionReceipt)
			{
				// Core учитывает свои глобальные budgets штатно. Только supply/prop
				// side effect manager уже применён единожды в синхронном commit.
				if (budgetChange >= 0 || !composition.m_AICFConstructionReceipt.m_bPaid)
					return;
			}
		}
		// Принятый объект при обычном демонтаже получает штатный fractional refund.
		super.OnEntityCoreBudgetUpdated(entityBudget, originalBudgetValue, budgetChange, updatedBudgetValue, entity);
	}
}

class AICF_StockConstructionAdapter
{
	bool Place(AICF_ConstructionOrder order, AICF_ConstructionConfig config, AICF_EconomySystem economy,
		SCR_CampaignBuildingManagerComponent manager, AICF_BaseBuilderService builders)
	{
		if (!order || order.m_bCommitStarted || order.m_bAccepted || !order.IdentityValid() ||
			!order.m_bSiteReserved || !economy.ReserveConstruction(order, config))
		{
			if (order)
				order.Log("CONSTRUCTION_ADMISSION_FAILED");
			return false;
		}
		order.m_bCommitStarted = true;
		bool ignored = SCR_EditorLinkComponent.IsSpawningIgnored();
		bool offline = SCR_ServicePointComponent.AICF_IsSpawnAsOffline();
		IEntity temporary = manager.GetTemporaryProvider();
		EntitySpawnParams params = new EntitySpawnParams();
		params.TransformMode = ETransformMode.WORLD;
		Math3D.MatrixCopy(order.m_aTransform, params.Transform);
		SCR_EditorLinkComponent.IgnoreSpawning(true);
		SCR_ServicePointComponent.SpawnAsOffline(true);
		IEntity entity = GetGame().SpawnEntityPrefabEx(order.m_Metadata.m_sPrefab, false, params: params);
		SCR_EditorLinkComponent.IgnoreSpawning(ignored);
		SCR_ServicePointComponent.SpawnAsOffline(offline);
		manager.SetTemporaryProvider(temporary);
		if (entity)
		{
			order.m_Composition = SCR_CampaignBuildingCompositionComponent.Cast(entity.FindComponent(SCR_CampaignBuildingCompositionComponent));
			order.m_LayoutId = entity.GetID();
		}
		if (order.m_Composition)
		{
			order.m_Composition.m_AICFConstructionReceipt = order;
			order.m_Composition.SetProviderEntityServer(order.m_Provider.GetOwner());
		}
		SCR_CampaignBuildingLayoutComponent layout;
		SCR_ServicePointComponent service;
		if (order.m_Composition)
			layout = order.m_Composition.GetCompositionLayout();
		if (entity)
			service = SCR_ServicePointComponent.Cast(entity.FindComponent(SCR_ServicePointComponent));
		bool factionReady = AICF_ContentProfile.GetActive().PrepareConstructionFaction(entity, order.m_sFaction, order.m_eType, order.m_Faction);
		bool valid = order.IdentityValid() && factionReady && entity && order.m_Composition && !order.m_Composition.IsCompositionSpawned() &&
			layout && layout.GetPrefabId() == order.m_Metadata.m_iPrefabId && layout.GetToBuildValue() > 0 &&
			layout.GetCurrentBuildValue() == 0 && SCR_Faction.GetEntityFaction(entity) == order.m_Faction &&
			!AICF_ConstructionMetadata.HasOnlineService(entity, order.m_eType);
		if (!valid)
		{
			string structural = string.Format("identity_valid=%1 composition_present=%2 layout_present=%3 service_present=%4",
				order.IdentityValid(), order.m_Composition != null, layout != null, service != null);
			if (order.m_Composition)
				structural += " spawned=" + order.m_Composition.IsCompositionSpawned();
			if (layout)
				structural += string.Format(" layout_prefab=%1 expected_prefab=%2 progress=%3 total=%4", layout.GetPrefabId(), order.m_Metadata.m_iPrefabId, layout.GetCurrentBuildValue(), layout.GetToBuildValue());
			if (service)
				structural += " service_state=" + typename.EnumToString(SCR_EServicePointStatus, service.GetServiceState());
			if (entity)
				structural += " faction_matches=" + (SCR_Faction.GetEntityFaction(entity) == order.m_Faction);
			order.Log("CONSTRUCTION_STRUCTURE_REJECTED", structural);
		}
		if (valid)
			valid = builders.RegisterConstruction(order.m_Composition, order.m_Base, order.m_Provider, order.m_Faction);
		if (valid)
			valid = PayConstruction(order, economy, manager);
		if (!valid)
		{
			// Нет stock debit до всех structural postconditions. Если debit primitive
			// отказал частично, Economy уже восстановила exact pool до удаления.
			economy.RollbackConstruction(order);
			builders.UnregisterFailedConstruction(order.m_Composition);
			order.m_sReason = "PLACEMENT_POSTCONDITION";
			order.Log("CONSTRUCTION_PLACEMENT_FAILED");
			if (entity && entity.GetID() == order.m_LayoutId)
				RplComponent.DeleteRplEntity(entity, false);
			return false;
		}
		order.m_bAccepted = true;
		order.m_sReason = "UNFINISHED_LAYOUT_ACCEPTED";
		order.Log("CONSTRUCTION_PLACED", "service_online=0");
		return true;
	}

	protected bool PayConstruction(AICF_ConstructionOrder order, AICF_EconomySystem economy, SCR_CampaignBuildingManagerComponent manager)
	{
		return economy.CommitConstruction(order, manager);
	}
}

modded class SCR_CampaignBuildingLayoutComponent
{
	bool AICF_CompletionClear()
	{
		if (!Replication.IsServer() || !GetOwner())
			return false;
		IEntity root = GetOwner().GetRootParent();
		SCR_CampaignBuildingCompositionComponent composition = SCR_CampaignBuildingCompositionComponent.Cast(root.FindComponent(SCR_CampaignBuildingCompositionComponent));
		if (!composition || !composition.m_AICFConstructionReceipt)
			return true;
		AICF_ConstructionOrder receipt = composition.m_AICFConstructionReceipt;
		bool clear = AICF_ConstructionSiteSearch.CompletionClear(receipt);
		if (!clear && System.GetTickCount() - receipt.m_iLastBlockedAt >= 10000)
		{
			receipt.m_iLastBlockedAt = System.GetTickCount();
			receipt.Log("CONSTRUCTION_COMPLETION_WAIT", "progress_added=0");
		}
		return clear;
	}

	override void AddBuildingValue(int value)
	{
		if (GetCurrentBuildValue() + value >= GetToBuildValue() && !AICF_CompletionClear())
			return;
		super.AddBuildingValue(value);
	}

	override void SetBuildingValue(float newValue)
	{
		if (newValue >= GetToBuildValue() && !AICF_CompletionClear())
			return;
		super.SetBuildingValue(newValue);
	}
}
