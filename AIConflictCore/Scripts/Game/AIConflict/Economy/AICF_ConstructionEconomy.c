// Supply admission и rollback принадлежат EconomySystem. Поиск не резервирует
// деньги между ticks; единственная reservation живёт внутри синхронного commit.
class AICF_ConstructionResourceSnapshot
{
	SCR_ResourceContainer m_Container;
	EntityID m_OwnerId;
	float m_fValue;
}

modded class AICF_EconomySystem
{
	protected AICF_ConstructionOrder m_ConstructionReservation;
	protected ref array<ref AICF_ConstructionResourceSnapshot> m_aConstructionSnapshot = {};
	protected int m_iConstructionPropsBefore;

	bool QuoteConstruction(AICF_ConstructionOrder order, AICF_ConstructionConfig config)
	{
		if (!Replication.IsServer() || !m_Campaign || !m_Campaign.IsMaster() || !m_Campaign.IsRunning() ||
			!order || !order.IdentityValid() || !order.m_Metadata || !order.m_Metadata.AllowsProvider(order.m_Provider) ||
			!m_SupplyNetwork.IsOperationalOwnedBase(order.m_Base, order.m_Faction))
		{
			if (order)
				order.m_sReason = "BASE_OR_PROVIDER_UNSAFE";
			return false;
		}
		SCR_ResourceComponent resource = order.m_Provider.GetResourceComponent();
		if (!resource || !resource.IsResourceTypeEnabled())
		{
			order.m_sReason = "SUPPLY_POOL_UNAVAILABLE";
			return false;
		}
		order.m_Consumer = resource.GetConsumer(EResourceGeneratorID.DEFAULT, EResourceType.SUPPLIES);
		if (!order.m_Consumer || !order.m_Consumer.IsConsuming())
		{
			order.m_sReason = "SUPPLY_CONSUMER_UNAVAILABLE";
			return false;
		}
		// Stock building manager передаёт CAMPAIGN budget без buy multiplier;
		// RequestConsumtion также не умножает переданную стоимость. Множитель
		// consumer относится к другим покупкам и не применяется второй раз.
		SCR_EditableEntityUIInfo info = SCR_EditableEntityUIInfo.ExtractEditableUIInfoFromPrefab(order.m_Metadata.m_sPrefab);
		if (!info)
			return false;
		array<ref SCR_EntityBudgetValue> costs = {};
		info.GetEntityAndChildrenBudgetCost(costs);
		array<ref SCR_EntityBudgetValue> children = {};
		info.GetEntityChildrenBudgetCost(children);
		foreach (SCR_EntityBudgetValue child : children)
		{
			if (child.GetBudgetType() == EEditableEntityBudget.CAMPAIGN && child.GetBudgetValue() > 0)
			{
				order.m_sReason = "UNSUPPORTED_CHILD_SUPPLY_BUDGET";
				return false;
			}
		}
		order.m_iCost = 0;
		order.m_iPropsCost = 0;
		order.m_iReserve = m_Config.GetReplacementSupplyCost() * config.m_iReserveGroups;
		if (m_SupplyNetwork.IsSupplySource(order.m_Base, order.m_Faction))
			order.m_iReserve = Math.Max(order.m_iReserve, m_Config.GetSourceReserveSupplies());
		SCR_ResourceConsumtionResponse availability = order.m_Consumer.RequestAvailability(0);
		order.m_fBefore = availability.GetAvailableSupply();
		order.m_fAfter = order.m_fBefore;
		foreach (SCR_EntityBudgetValue cost : costs)
		{
			EEditableEntityBudget type = cost.GetBudgetType();
			if (type == EEditableEntityBudget.CAMPAIGN)
				order.m_iCost = cost.GetBudgetValue();
			if (type == EEditableEntityBudget.PROPS)
				order.m_iPropsCost = cost.GetBudgetValue();
			if (!order.m_Provider.GetBudgetData(type))
				continue;
			SCR_CampaignBuildingProviderComponent realProvider = order.m_Provider;
			int current = order.m_Provider.GetBudgetValue(type, realProvider);
			int pending = Math.Max(0, realProvider.GetAccumulatedBudgetChanges(type));
			if (type == EEditableEntityBudget.CAMPAIGN)
			{
				if (order.m_fBefore < cost.GetBudgetValue() + order.m_iReserve + pending)
				{
					order.m_sReason = "INSUFFICIENT_SUPPLIES";
					return false;
				}
			}
			else
			{
				int maximum = order.m_Provider.GetMaxBudgetValueFromMasterIfNeeded(type);
				if (maximum >= 0 && current + pending + cost.GetBudgetValue() > maximum)
				{
					order.m_sReason = "PROVIDER_BUDGET_LIMIT";
					return false;
				}
			}
		}
		if (order.m_iCost <= 0 || order.m_fBefore < order.m_iCost + order.m_iReserve)
		{
			order.m_sReason = "INVALID_COST_OR_RESERVE";
			return false;
		}
		order.m_sReason = "ELIGIBLE";
		return true;
	}

	bool ReserveConstruction(AICF_ConstructionOrder order, AICF_ConstructionConfig config)
	{
		if (m_ConstructionReservation || !order || order.m_bPaid || !QuoteConstruction(order, config))
			return false;
		m_aConstructionSnapshot.Clear();
		SCR_ResourceContainerQueueBase queue = order.m_Consumer.GetContainerQueue();
		if (!queue || order.m_Consumer.GetContainerCount() > 128)
		{
			order.m_sReason = "RESOURCE_QUEUE_UNAVAILABLE_OR_LIMIT";
			return false;
		}
		for (int i; i < order.m_Consumer.GetContainerCount(); i++)
		{
			SCR_ResourceContainer container = queue.GetContainerAt(i);
			if (!SnapshotConstructionContainer(container, 0))
			{
				m_aConstructionSnapshot.Clear();
				order.m_sReason = "UNSUPPORTED_RESOURCE_CONTAINER";
				return false;
			}
		}
		m_ConstructionReservation = order;
		m_iConstructionPropsBefore = order.m_Provider.GetCurrentPropValue();
		order.m_iPropsBefore = m_iConstructionPropsBefore;
		order.m_iPropsAfter = m_iConstructionPropsBefore;
		order.Log("CONSTRUCTION_RESERVED", "supply_debited=0 scope=SYNCHRONOUS_COMMIT");
		return true;
	}

	protected bool SnapshotConstructionContainer(SCR_ResourceContainer container, int depth)
	{
		if (!container || !container.GetOwner() || depth > 4 || m_aConstructionSnapshot.Count() >= 128)
			return false;
		SCR_ResourceEncapsulator encapsulator = container.GetResourceEncapsulator();
		if (encapsulator)
		{
			SCR_ResourceContainerQueueBase queue = encapsulator.GetContainerQueue();
			if (!queue || encapsulator.GetContainerCount() > 128)
				return false;
			for (int i; i < encapsulator.GetContainerCount(); i++)
			{
				if (!SnapshotConstructionContainer(queue.GetContainerAt(i), depth + 1))
					return false;
			}
			return true;
		}
		// Не допускаем debit, способный удалить entity до возможного rollback.
		if (container.GetOnEmptyBehavior() == EResourceContainerOnEmptyBehavior.DELETE)
			return false;
		foreach (AICF_ConstructionResourceSnapshot existing : m_aConstructionSnapshot)
		{
			if (existing.m_Container == container)
				return true;
		}
		AICF_ConstructionResourceSnapshot snapshot = new AICF_ConstructionResourceSnapshot();
		snapshot.m_Container = container;
		snapshot.m_OwnerId = container.GetOwner().GetID();
		snapshot.m_fValue = container.GetResourceValue();
		m_aConstructionSnapshot.Insert(snapshot);
		return true;
	}

	bool CommitConstruction(AICF_ConstructionOrder order, SCR_CampaignBuildingManagerComponent manager)
	{
		if (!order || m_ConstructionReservation != order || order.m_bPaid || !order.IdentityValid() || !order.m_Composition)
			return false;
		// Stock manager выполняет единственное списание и prop accounting. Поздний
		// per-entity callback подавляется только для exact AI receipt на этом root.
		manager.AICF_ApplyConstructionBudget(order);
		order.m_fAfter = order.m_Consumer.GetAggregatedResourceValue();
		order.m_iPropsAfter = order.m_Provider.GetCurrentPropValue();
		if (!order.IdentityValid() || Math.AbsFloat(order.m_fBefore - order.m_fAfter - order.m_iCost) > 0.01 ||
			order.m_iPropsAfter - order.m_iPropsBefore != order.m_iPropsCost)
		{
			order.m_sReason = "STOCK_DEBIT_POSTCONDITION";
			RollbackConstruction(order);
			return false;
		}
		order.m_bPaid = true;
		m_ConstructionReservation = null;
		m_aConstructionSnapshot.Clear();
		order.Log("CONSTRUCTION_PAYMENT", "debit_count=1 tickets=0 reservation_released=1");
		return true;
	}

	void RollbackConstruction(AICF_ConstructionOrder order)
	{
		if (!order || m_ConstructionReservation != order)
			return;
		// До возврата проверяются все identities; rollback только синхронный,
		// не переносится новому владельцу и не совмещается со stock refund.
		bool valid = order.IdentityValid();
		foreach (AICF_ConstructionResourceSnapshot snapshot : m_aConstructionSnapshot)
		{
			if (!snapshot.m_Container || !snapshot.m_Container.GetOwner() || snapshot.m_Container.GetOwner().GetID() != snapshot.m_OwnerId)
				valid = false;
		}
		if (valid)
		{
			foreach (AICF_ConstructionResourceSnapshot snapshot : m_aConstructionSnapshot)
			{
				if (snapshot.m_Container.GetResourceValue() < snapshot.m_fValue)
				{
					snapshot.m_Container.SetResourceValue(snapshot.m_fValue);
					snapshot.m_Container.GetComponent().Replicate();
				}
			}
			int propDelta = order.m_Provider.GetCurrentPropValue() - m_iConstructionPropsBefore;
			if (propDelta > 0)
				order.m_Provider.AddPropValue(-propDelta);
			order.m_fAfter = order.m_Consumer.GetAggregatedResourceValue();
			order.m_iPropsAfter = order.m_Provider.GetCurrentPropValue();
		}
		m_ConstructionReservation = null;
		m_aConstructionSnapshot.Clear();
		order.Log("CONSTRUCTION_ROLLBACK", string.Format("restored=%1 stock_refund=0 reservation_released=1", valid));
	}
}
