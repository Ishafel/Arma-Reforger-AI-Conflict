// Изолированный stage: native building preflight и entity-core debit.
// Это не пользовательский RPC/menu flow и не проверка refund action.
class AICF_MenuTransactionProbe
{
	static AICF_MenuTransactionProbe s_Active;
	ref array<ResourceName> m_aPrefabs = {};
	ref array<ref SCR_CampaignFaction> m_aFactions = {};
	ref array<float> m_aOriginal = {};
	ref AICF_LogisticsResourceAdapter m_Resources = new AICF_LogisticsResourceAdapter();
	ref AICF_LogisticsResourcePool m_Pool;
	SCR_CampaignBuildingProviderComponent m_Provider;
	SCR_CampaignBuildingManagerComponent m_Manager;
	IEntity m_Entity;
	int m_iCase;
	int m_iPrice;
	int m_iPassed;
	bool m_bPass;
	bool m_bWaiting;
	float m_fBefore;
	float m_fAfter;
	int m_iDebitEvents;

	void Start()
	{
		s_Active = this;
		m_Manager = SCR_CampaignBuildingManagerComponent.Cast(GetGame().GetGameMode().FindComponent(SCR_CampaignBuildingManagerComponent));
		foreach (FactionKey key : {"RHS_USAF", "RHS_AFRF"})
		{
			SCR_CampaignFaction faction = SCR_CampaignFaction.Cast(GetGame().GetFactionManager().GetFactionByKey(key));
			array<ResourceName> groups = {};
			SCR_EntityCatalogManagerComponent.GetInstance().GetFilteredEditorPrefabs(EEntityCatalogType.GROUP, EEditorMode.BUILDING, faction, groups);
			m_aPrefabs.Insert(groups[0]);
			m_aFactions.Insert(faction);
			array<SCR_EntityCatalogEntry> entries = {};
			faction.GetFactionEntityCatalogOfType(EEntityCatalogType.VEHICLE).GetEntityList(entries);
			foreach (SCR_EntityCatalogEntry entry : entries)
			{
				if (!entry.GetPrefab().Contains("M1025_armed_M2HB_USAF.et") && !entry.GetPrefab().Contains("BTR70_AFRF.et")) continue;
				m_aPrefabs.Insert(entry.GetPrefab());
				m_aFactions.Insert(faction);
				break;
			}
		}
	}

	void SetPool(float value)
	{
		foreach (AICF_LogisticsLeaf leaf : m_Pool.m_aLeaves)
		{
			float part = Math.Min(value, leaf.m_Container.GetMaxResourceValue());
			leaf.m_Container.SetResourceValue(part);
			value -= part;
		}
	}

	void Cleanup()
	{
		m_bWaiting = false;
		if (m_Manager) m_Manager.SetTemporaryProvider(null);
		if (m_Provider) m_Provider.ClearAccumulatedBudgetChanges();
		SCR_AIGroup group = SCR_AIGroup.Cast(m_Entity);
		if (group)
		{
			array<AIAgent> agents = {};
			group.GetAgents(agents);
			foreach (AIAgent agent : agents)
				if (agent.GetControlledEntity()) SCR_EntityHelper.DeleteEntityAndChildren(agent.GetControlledEntity());
		}
		if (m_Entity) SCR_EntityHelper.DeleteEntityAndChildren(m_Entity);
		m_Entity = null;
		if (m_Pool)
			foreach (int index, AICF_LogisticsLeaf leaf : m_Pool.m_aLeaves)
				if (leaf.Valid()) leaf.m_Container.SetResourceValue(m_aOriginal[index]);
		m_aOriginal.Clear();
		m_Pool = null;
	}

	bool Tick()
	{
		if (m_bWaiting)
		{
			m_bPass = m_bPass && m_Entity && m_iDebitEvents == 1 && Math.AbsFloat(m_fBefore - m_fAfter - m_iPrice) < 0.01;
			if (m_bPass) m_iPassed++;
			Print(string.Format("[AICF][MENU_TRANSACTION] pass=%1 case=%2 price=%3 before=%4 after=%5 prefab=%6", m_bPass, m_iCase, m_iPrice, m_fBefore, m_fAfter, m_aPrefabs[m_iCase]));
			Cleanup();
			m_iCase++;
			m_bWaiting = false;
		}
		if (m_iCase >= m_aPrefabs.Count())
		{
			Print(string.Format("[AICF][MENU_TRANSACTION_RESULT] passed=%1 total=%2", m_iPassed, m_aPrefabs.Count()));
			s_Active = null;
			return true;
		}
		m_Provider = m_aFactions[m_iCase].GetMainBase().GetMasterProvider();
		m_Pool = m_Resources.Resolve(m_Provider.GetResourceComponent());
		if (!m_Manager || !m_Pool)
		{
			Print("[AICF][MENU_TRANSACTION_RESULT] passed=0 total=4 reason=NO_PROVIDER_POOL");
			s_Active = null;
			return true;
		}
		foreach (AICF_LogisticsLeaf leaf : m_Pool.m_aLeaves) m_aOriginal.Insert(leaf.m_Container.GetResourceValue());
		ResourceName prefab = m_aPrefabs[m_iCase];
		m_iPrice = AICF_RHSMenuPricePolicy.GetInstance().Quote(prefab);
		SCR_EditableEntityUIInfo info = SCR_EditableEntityUIInfo.ExtractEditableUIInfoFromPrefab(prefab);
		array<ref SCR_EntityBudgetValue> budget = {};
		info.GetEntityBudgetCost(budget);
		info.GetEntityChildrenBudgetCost(budget);
		m_Provider.ClearAccumulatedBudgetChanges();
		SetPool(m_iPrice - 1);
		bool denied = !m_Provider.IsThereEnoughBudgetToSpawn(budget);
		m_bPass = denied && m_Pool.Value() == m_iPrice - 1;
		m_Provider.ClearAccumulatedBudgetChanges();
		SetPool(m_iPrice);
		bool exact = m_Provider.IsThereEnoughBudgetToSpawn(budget);
		m_Provider.ClearAccumulatedBudgetChanges(); // Штатная отмена зарезервированного preflight.
		m_bPass = m_bPass && exact && m_Pool.Value() == m_iPrice;
		Print(string.Format("[AICF][MENU_TRANSACTION_PREFLIGHT] case=%1 insufficient=%2 exact=%3 cancel_unchanged=%4", m_iCase, denied, exact, m_Pool.Value() == m_iPrice));
		SetPool(m_iPrice + 100);
		m_iDebitEvents = 0;
		EntitySpawnParams params = new EntitySpawnParams();
		params.TransformMode = ETransformMode.WORLD;
		params.Transform[3] = m_Provider.GetOwner().GetOrigin() + "100 0 100";
		params.Transform[3][1] = GetGame().GetWorld().GetSurfaceY(params.Transform[3][0], params.Transform[3][2]);
		m_Manager.SetTemporaryProvider(m_Provider.GetOwner());
		m_bWaiting = true;
		m_Entity = GetGame().SpawnEntityPrefab(Resource.Load(prefab), null, params);
		return false;
	}
}

// Наблюдаем атомарный native callback: между тиками HQ может добавить income.
modded class SCR_CampaignBuildingManagerComponent
{
	override protected void OnEntityCoreBudgetUpdated(EEditableEntityBudget entityBudget, int originalBudgetValue, int budgetChange, int updatedBudgetValue, SCR_EditableEntityComponent entity)
	{
		AICF_MenuTransactionProbe probe = AICF_MenuTransactionProbe.s_Active;
		bool observe = probe && probe.m_bWaiting && entityBudget == EEditableEntityBudget.CAMPAIGN && entity.GetOwner().GetPrefabData().GetPrefabName() == probe.m_aPrefabs[probe.m_iCase];
		if (observe) probe.m_fBefore = probe.m_Pool.Value();
		super.OnEntityCoreBudgetUpdated(entityBudget, originalBudgetValue, budgetChange, updatedBudgetValue, entity);
		if (observe)
		{
			probe.m_fAfter = probe.m_Pool.Value();
			probe.m_iDebitEvents++;
		}
	}
}
