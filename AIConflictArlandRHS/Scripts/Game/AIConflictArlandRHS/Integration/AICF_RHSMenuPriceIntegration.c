// Один quote для preview, server budget, spawner и native refund.
// Меняются только возвращаемые CAMPAIGN costs, не prefab/resources/registry IDs.
class AICF_MenuPriceBinding
{
	ResourceName m_sPrefab;
	int m_iPrice = -1;
	ref array<ref SCR_EntityBudgetValue> m_aOwn = {};
	ref array<ref SCR_EntityBudgetValue> m_aChildren = {};

	void Bind(ResourceName prefab)
	{
		if (prefab == m_sPrefab)
			return;
		m_sPrefab = prefab;
		m_iPrice = AICF_RHSMenuPricePolicy.GetInstance().Quote(prefab);
		m_aOwn.Clear();
		m_aChildren.Clear();
		if (m_iPrice < 0)
			return;
		Resource resource = Resource.Load(prefab);
		IEntityComponentSource component = SCR_EditableEntityComponentClass.GetEditableEntitySource(resource);
		BaseContainer info;
		if (component)
			info = component.GetObject("m_UIInfo");
		if (!info)
		{
			m_iPrice = -1;
			return;
		}
		// Не вызываем native MergeBudgetCosts даже во временный массив: при
		// повторяющихся типах он меняет первый объект исходного budget массива.
		info.Get("m_EntityBudgetCost", m_aOwn);
		info.Get("m_EntityChildrenBudgetCost", m_aChildren);
	}

	bool IsActive()
	{
		return m_iPrice >= 0 && AICF_RHSMenuPricePolicy.IsActive();
	}

	// Native MergeBudgetCosts удерживает/меняет исходные budget objects.
	// Копируем остальные бюджеты; price добавляем к уже накопленной сумме.
	static void Append(array<ref SCR_EntityBudgetValue> source, array<ref SCR_EntityBudgetValue> destination, int campaignCost)
	{
		foreach (SCR_EntityBudgetValue budget : source)
		{
			if (budget.GetBudgetType() != EEditableEntityBudget.CAMPAIGN)
				SCR_EntityBudgetValue.AddBudgetValueToBudgetArray(destination, budget.GetBudgetType(), budget.GetBudgetValue());
		}
		if (campaignCost > 0)
			SCR_EntityBudgetValue.AddBudgetValueToBudgetArray(destination, EEditableEntityBudget.CAMPAIGN, campaignCost);
	}

	static SCR_UIInfo BindInfo(SCR_UIInfo info, ResourceName prefab)
	{
		SCR_EditableGroupUIInfo group = SCR_EditableGroupUIInfo.Cast(info);
		if (group)
			group.m_AICFMenuPrice.Bind(prefab);
		SCR_EditableVehicleUIInfo vehicle = SCR_EditableVehicleUIInfo.Cast(info);
		if (vehicle)
			vehicle.m_AICFMenuPrice.Bind(prefab);
		// Mi8MTT-A использует базовый UIInfo, а не vehicle UIInfo.
		// Не modded base: native CopyFrom обращается к private полям своего типа.
		if (info && info.Type() == SCR_EditableEntityUIInfo && AICF_RHSMenuPricePolicy.GetInstance().Quote(prefab) >= 0)
		{
			AICF_MenuEntityUIInfo wrapped = new AICF_MenuEntityUIInfo();
			wrapped.Initialize(info, prefab);
			return wrapped;
		}
		return info;
	}
}

[BaseContainerProps()]
class AICF_MenuEntityUIInfo : SCR_EditableEntityUIInfo
{
	protected ref AICF_MenuPriceBinding m_Price = new AICF_MenuPriceBinding();

	override void CopyFrom(SCR_UIName source)
	{
		super.CopyFrom(source);
		AICF_MenuEntityUIInfo info = AICF_MenuEntityUIInfo.Cast(source);
		if (info) m_Price.Bind(info.m_Price.m_sPrefab);
	}

	void Initialize(SCR_UIInfo source, ResourceName prefab)
	{
		CopyFrom(source);
		m_Price.Bind(prefab);
	}

	override bool GetEntityBudgetCost(out notnull array<ref SCR_EntityBudgetValue> outBudgets)
	{
		if (!m_Price.IsActive()) return super.GetEntityBudgetCost(outBudgets);
		AICF_MenuPriceBinding.Append(m_Price.m_aOwn, outBudgets, m_Price.m_iPrice);
		return true;
	}

	override void GetEntityChildrenBudgetCost(out notnull array<ref SCR_EntityBudgetValue> outBudgets)
	{
		if (!m_Price.IsActive())
		{
			super.GetEntityChildrenBudgetCost(outBudgets);
			return;
		}
		AICF_MenuPriceBinding.Append(m_Price.m_aChildren, outBudgets, 0);
	}

	override void GetEntityAndChildrenBudgetCost(out notnull array<ref SCR_EntityBudgetValue> outBudgets)
	{
		if (!m_Price.IsActive())
		{
			super.GetEntityAndChildrenBudgetCost(outBudgets);
			return;
		}
		AICF_MenuPriceBinding.Append(m_Price.m_aOwn, outBudgets, m_Price.m_iPrice);
		AICF_MenuPriceBinding.Append(m_Price.m_aChildren, outBudgets, 0);
	}
}

// Component sources не содержат путь владельца. Запоминаем связь в точке,
// где native извлекает компонент из конкретного prefab, без изменения source.
class AICF_MenuPriceSources
{
	protected static ref map<IEntityComponentSource, ResourceName> s_Prefabs = new map<IEntityComponentSource, ResourceName>();
	protected static ref map<ResourceName, ref Resource> s_Resources = new map<ResourceName, ref Resource>();

	static void Register(IEntitySource owner, IEntityComponentSource component)
	{
		if (!owner || !component)
			return;
		ResourceName prefab = SCR_BaseContainerTools.GetPrefabResourceName(owner);
		if (prefab.IsEmpty())
			return;
		bool group = prefab.Contains("Prefabs/Groups/") && AICF_RHSMenuPricePolicy.IsInfantry(prefab);
		if (!group && AICF_RHSMenuPricePolicy.GetInstance().VehicleCost(AICF_RHSMenuPricePolicy.Path(prefab)) < 0)
			return;
		s_Prefabs.Set(component, prefab);
		if (!s_Resources.Contains(prefab))
			s_Resources.Set(prefab, Resource.Load(prefab));
	}

	static ResourceName Find(IEntityComponentSource component)
	{
		ResourceName prefab;
		s_Prefabs.Find(component, prefab);
		return prefab;
	}

	static void Clear()
	{
		s_Prefabs.Clear();
		s_Resources.Clear();
	}
}

modded class SCR_EditableEntityComponentClass
{
	static override IEntityComponentSource GetEditableEntitySource(IEntitySource entitySource)
	{
		IEntityComponentSource component = super.GetEditableEntitySource(entitySource);
		AICF_MenuPriceSources.Register(entitySource, component);
		return component;
	}

	static override SCR_EditableEntityUIInfo GetInfo(IEntityComponentSource componentSource)
	{
		SCR_EditableEntityUIInfo info = super.GetInfo(componentSource);
		return SCR_EditableEntityUIInfo.Cast(AICF_MenuPriceBinding.BindInfo(info, AICF_MenuPriceSources.Find(componentSource)));
	}
}

modded class SCR_GameModeCampaign
{
	override void OnGameEnd()
	{
		super.OnGameEnd();
		AICF_MenuPriceSources.Clear();
	}
}

[BaseContainerProps()]
modded class SCR_EditableGroupUIInfo
{
	ref AICF_MenuPriceBinding m_AICFMenuPrice = new AICF_MenuPriceBinding();

	override void CopyFrom(SCR_UIName source)
	{
		super.CopyFrom(source);
		SCR_EditableGroupUIInfo info = SCR_EditableGroupUIInfo.Cast(source);
		if (info)
			m_AICFMenuPrice.Bind(info.m_AICFMenuPrice.m_sPrefab);
	}

	// Native entity core списывает own budget группы один раз.
	override bool GetGroupBudgetCost(out notnull array<ref SCR_EntityBudgetValue> outBudgets)
	{
		if (!m_AICFMenuPrice.IsActive())
			return super.GetGroupBudgetCost(outBudgets);
		AICF_MenuPriceBinding.Append(m_AICFMenuPrice.m_aOwn, outBudgets, m_AICFMenuPrice.m_iPrice);
		return true;
	}

	// Server preflight читает children: group.GetEntityBudgetCost пустой.
	override void GetEntityChildrenBudgetCost(out notnull array<ref SCR_EntityBudgetValue> outBudgets)
	{
		if (!m_AICFMenuPrice.IsActive())
		{
			super.GetEntityChildrenBudgetCost(outBudgets);
			return;
		}
		AICF_MenuPriceBinding.Append(m_AICFMenuPrice.m_aChildren, outBudgets, m_AICFMenuPrice.m_iPrice);
	}

	override void GetEntityAndChildrenBudgetCost(out notnull array<ref SCR_EntityBudgetValue> outBudgets)
	{
		if (!m_AICFMenuPrice.IsActive())
		{
			super.GetEntityAndChildrenBudgetCost(outBudgets);
			return;
		}
		AICF_MenuPriceBinding.Append(m_AICFMenuPrice.m_aOwn, outBudgets, m_AICFMenuPrice.m_iPrice);
		AICF_MenuPriceBinding.Append(m_AICFMenuPrice.m_aChildren, outBudgets, 0);
	}
}

[BaseContainerProps(), SCR_BaseContainerLocalizedTitleField("Name")]
modded class SCR_EditableVehicleUIInfo
{
	ref AICF_MenuPriceBinding m_AICFMenuPrice = new AICF_MenuPriceBinding();

	override void CopyFrom(SCR_UIName source)
	{
		super.CopyFrom(source);
		SCR_EditableVehicleUIInfo info = SCR_EditableVehicleUIInfo.Cast(source);
		if (info) m_AICFMenuPrice.Bind(info.m_AICFMenuPrice.m_sPrefab);
	}

	override bool GetEntityBudgetCost(out notnull array<ref SCR_EntityBudgetValue> outBudgets)
	{
		if (!m_AICFMenuPrice.IsActive())
			return super.GetEntityBudgetCost(outBudgets);
		AICF_MenuPriceBinding.Append(m_AICFMenuPrice.m_aOwn, outBudgets, m_AICFMenuPrice.m_iPrice);
		return true;
	}

	override void GetEntityChildrenBudgetCost(out notnull array<ref SCR_EntityBudgetValue> outBudgets)
	{
		if (!m_AICFMenuPrice.IsActive())
		{
			super.GetEntityChildrenBudgetCost(outBudgets);
			return;
		}
		AICF_MenuPriceBinding.Append(m_AICFMenuPrice.m_aChildren, outBudgets, 0);
	}

	override void GetEntityAndChildrenBudgetCost(out notnull array<ref SCR_EntityBudgetValue> outBudgets)
	{
		if (!m_AICFMenuPrice.IsActive())
		{
			super.GetEntityAndChildrenBudgetCost(outBudgets);
			return;
		}
		AICF_MenuPriceBinding.Append(m_AICFMenuPrice.m_aOwn, outBudgets, m_AICFMenuPrice.m_iPrice);
		AICF_MenuPriceBinding.Append(m_AICFMenuPrice.m_aChildren, outBudgets, 0);
	}
}

// Instantiated UIInfo приходит из component class без InitFromSource.
modded class SCR_EditableEntityComponent
{
	protected ref SCR_UIInfo m_AICFMenuWrappedInfo;
	protected ResourceName m_sAICFMenuWrappedPrefab;

	override SCR_UIInfo GetInfo(IEntity owner = null)
	{
		SCR_UIInfo result = super.GetInfo(owner);
		if (!owner)
			owner = GetOwner();
		if (!owner || !owner.GetPrefabData())
			return result;
		ResourceName prefab = owner.GetPrefabData().GetPrefabName();
		if (m_AICFMenuWrappedInfo && prefab == m_sAICFMenuWrappedPrefab)
			return m_AICFMenuWrappedInfo;
		SCR_UIInfo bound = AICF_MenuPriceBinding.BindInfo(result, prefab);
		if (bound != result)
		{
			m_AICFMenuWrappedInfo = bound;
			m_sAICFMenuWrappedPrefab = prefab;
		}
		return bound;
	}
}

[BaseContainerProps(configRoot: true), BaseContainerCustomCheckIntTitleField("m_bEnabled", "SpawnerData", "DISABLED - SpawnerData", 1)]
modded class SCR_EntityCatalogSpawnerData
{
	protected int m_iAICFMenuPrice = -2;

	override int GetSupplyCost()
	{
		// InitData может исполниться до OnGameStart и выбора profile.
		if (AICF_RHSMenuPricePolicy.IsActive())
		{
			if (m_iAICFMenuPrice == -2)
				m_iAICFMenuPrice = AICF_RHSMenuPricePolicy.GetInstance().Quote(m_sDefaultPrefab);
			if (m_iAICFMenuPrice >= 0)
				return m_iAICFMenuPrice;
		}
		return super.GetSupplyCost();
	}
}
