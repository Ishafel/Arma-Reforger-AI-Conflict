// Только изолированный RHS/WCS stage. Не устанавливать в production.
class AICF_MenuPriceProbe
{
	static int Cost(array<ref SCR_EntityBudgetValue> budgets)
	{
		int result;
		foreach (SCR_EntityBudgetValue budget : budgets)
			if (budget.GetBudgetType() == EEditableEntityBudget.CAMPAIGN)
				result += budget.GetBudgetValue();
		return result;
	}

	static string Budgets(array<ref SCR_EntityBudgetValue> budgets)
	{
		string result;
		foreach (SCR_EntityBudgetValue budget : budgets)
			if (budget.GetBudgetType() != EEditableEntityBudget.CAMPAIGN)
				result += string.Format("%1:%2,", budget.GetBudgetType(), budget.GetBudgetValue());
		return result;
	}

	static bool Check(ResourceName prefab, SCR_EntityCatalogSpawnerData spawner, int index, FactionKey faction)
	{
		SCR_EditableEntityUIInfo info = SCR_EditableEntityUIInfo.ExtractEditableUIInfoFromPrefab(prefab);
		if (!info)
		{
			Print("[AICF][MENU_PRICE_CASE] pass=0 reason=NO_INFO prefab=" + prefab);
			return false;
		}
		array<ref SCR_EntityBudgetValue> total = {};
		array<ref SCR_EntityBudgetValue> own = {};
		array<ref SCR_EntityBudgetValue> children = {};
		array<ref SCR_EntityBudgetValue> repeated = {};
		array<ref SCR_EntityBudgetValue> accumulated = {new SCR_EntityBudgetValue(EEditableEntityBudget.CAMPAIGN, 7)};
		info.GetEntityAndChildrenBudgetCost(total);
		info.GetEntityBudgetCost(own);
		info.GetEntityChildrenBudgetCost(children);
		info.GetEntityAndChildrenBudgetCost(repeated);
		info.GetEntityAndChildrenBudgetCost(accumulated);
		info.GetEntityAndChildrenBudgetCost(accumulated);
		int quote = AICF_RHSMenuPricePolicy.GetInstance().Quote(prefab);
		int price = Cost(total);
		bool pass = quote > 0 && price == quote && Cost(repeated) == price && Cost(accumulated) == 7 + price * 2;
		pass = pass && Cost(own) + Cost(children) == price && Budgets(total) == Budgets(repeated);
		int debit = Cost(own);
		SCR_EditableGroupUIInfo group = SCR_EditableGroupUIInfo.Cast(info);
		int count;
		string roles;
		if (group)
		{
			array<ref SCR_EntityBudgetValue> groupDebit = {};
			group.GetGroupBudgetCost(groupDebit);
			debit = Cost(groupDebit);
			Resource resource = Resource.Load(prefab);
			IEntitySource source = SCR_BaseContainerTools.FindEntitySource(resource);
			array<ResourceName> members = {};
			source.Get("m_aUnitPrefabSlots", members);
			count = members.Count();
			foreach (ResourceName member : members)
			{
				roles += FilePath.StripPath(member) + ":" + AICF_RHSMenuPricePolicy.CharacterCost(member) + ",";
				Print(string.Format("[AICF][MENU_PRICE_MEMBER] group=%1 prefab=%2 cost=%3", prefab, member, AICF_RHSMenuPricePolicy.CharacterCost(member)));
			}
			pass = pass && price >= 50 * count && price <= 100 * count;
			SCR_EditableGroupUIInfo copy = new SCR_EditableGroupUIInfo();
			copy.CopyFrom(group);
			array<ref SCR_EntityBudgetValue> copied = {};
			copy.GetEntityAndChildrenBudgetCost(copied);
			pass = pass && Cost(copied) == price;
		}
		pass = pass && debit == price;
		int spawnerPrice = -1;
		if (spawner)
		{
			spawnerPrice = spawner.GetSupplyCost();
			pass = pass && spawnerPrice == price;
			array<ref SCR_EntityBudgetValue> copiedVehicle = {};
			SCR_EditableVehicleUIInfo vehicle = SCR_EditableVehicleUIInfo.Cast(info);
			if (vehicle)
			{
				SCR_EditableVehicleUIInfo vehicleCopy = new SCR_EditableVehicleUIInfo();
				vehicleCopy.CopyFrom(vehicle);
				vehicleCopy.GetEntityAndChildrenBudgetCost(copiedVehicle);
			}
			else
			{
				AICF_MenuEntityUIInfo plainCopy = new AICF_MenuEntityUIInfo();
				plainCopy.CopyFrom(info);
				plainCopy.GetEntityAndChildrenBudgetCost(copiedVehicle);
			}
			pass = pass && Cost(copiedVehicle) == price;
		}
		// Active profile gate, затем повтор на том же UIInfo: нет stale early cache.
		AICF_ContentProfile active = AICF_ContentProfile.GetActive();
		AICF_ContentProfile.SetActive(new AICF_ContentProfile());
		array<ref SCR_EntityBudgetValue> originalBudgets = {};
		info.GetEntityAndChildrenBudgetCost(originalBudgets);
		AICF_ContentProfile.SetActive(active);
		array<ref SCR_EntityBudgetValue> restored = {};
		info.GetEntityAndChildrenBudgetCost(restored);
		pass = pass && Cost(restored) == price && Budgets(originalBudgets) == Budgets(total);
		string row = string.Format("[AICF][MENU_PRICE_CASE] pass=%1 faction=%2 index=%3 old=%4 new=%5 debit=%6 spawner=%7 members=%8 prefab=%9", pass, faction, index, Cost(originalBudgets), price, debit, spawnerPrice, count, prefab);
		Print(row + " roles=" + roles);
		return pass;
	}

	static void Features(BaseContainer source, array<string> resources, set<BaseContainer> visited, int depth = 0)
	{
		if (!source || depth > 18 || visited.Contains(source))
			return;
		visited.Insert(source);
		for (int i; i < source.GetNumVars(); i++)
		{
			string name = source.GetVarName(i);
			DataVarType variable = source.GetDataVarType(i);
			if (variable == DataVarType.OBJECT)
				Features(source.GetObject(name), resources, visited, depth + 1);
			else if (variable == DataVarType.OBJECT_ARRAY)
			{
				BaseContainerList list = source.GetObjectArray(name);
				if (list)
					for (int n; n < list.Count(); n++)
						Features(list.Get(n), resources, visited, depth + 1);
			}
			else if (variable == DataVarType.RESOURCE_NAME)
			{
				ResourceName value;
				source.Get(name, value);
				if ((value.Contains("Prefabs/Weapons/Magazines/") || value.Contains("Prefabs/Weapons/Ammo/") || value.Contains("ERA") || value.Contains("Optic")) && !resources.Contains(value))
				resources.Insert(value);
			}
		}
		IEntitySource entity = IEntitySource.Cast(source);
		if (entity)
			for (int c; c < entity.GetComponentCount(); c++)
				Features(entity.GetComponent(c), resources, visited, depth + 1);
		for (int child; child < source.GetNumChildren(); child++)
			Features(source.GetChild(child), resources, visited, depth + 1);
	}

	static void MeasureEntity(IEntity entity, array<BaseCompartmentSlot> slots, array<string> weapons)
	{
		BaseCompartmentManagerComponent manager = BaseCompartmentManagerComponent.Cast(entity.FindComponent(BaseCompartmentManagerComponent));
		if (manager)
		{
			array<BaseCompartmentSlot> found = {};
			manager.GetCompartments(found);
			foreach (BaseCompartmentSlot slot : found)
				if (!slots.Contains(slot)) slots.Insert(slot);
		}
		if (entity.FindComponent(BaseWeaponComponent))
		{
			string weapon = SCR_ResourceNameUtils.GetPrefabName(entity);
			if (!weapons.Contains(weapon)) weapons.Insert(weapon);
		}
		IEntity child = entity.GetChildren();
		while (child)
		{
			MeasureEntity(child, slots, weapons);
			child = child.GetSibling();
		}
	}

	static void Measure(ResourceName prefab)
	{
		string enabled;
		if (!Replication.IsServer() || !System.GetCLIParam("aicfMenuVehicleMeasure", enabled)) return;
		EntitySpawnParams params = new EntitySpawnParams();
		params.TransformMode = ETransformMode.WORLD;
		params.Transform[3] = "0 1000 0";
		IEntity vehicle = GetGame().SpawnEntityPrefabLocal(Resource.Load(prefab), null, params);
		if (!vehicle)
		{
			Print("[AICF][MENU_PRICE_MEASURE] pass=0 prefab=" + prefab);
			return;
		}
		array<BaseCompartmentSlot> slots = {};
		array<string> weapons = {};
		MeasureEntity(vehicle, slots, weapons);
		string counts, weaponText;
		foreach (BaseCompartmentSlot slot : slots) counts += typename.EnumToString(ECompartmentType, slot.GetType()) + ",";
		foreach (string weapon : weapons) weaponText += weapon + ";";
		Print(string.Format("[AICF][MENU_PRICE_MEASURE] pass=1 prefab=%1 seats=%2 types=%3 weapons=%4", prefab, slots.Count(), counts, weaponText));
		SCR_EntityHelper.DeleteEntityAndChildren(vehicle);
	}

	static void Run()
	{
		int total, passed;
		SCR_EntityCatalogManagerComponent manager = SCR_EntityCatalogManagerComponent.GetInstance();
		foreach (FactionKey key : {"RHS_USAF", "RHS_AFRF"})
		{
			SCR_Faction faction = SCR_Faction.Cast(GetGame().GetFactionManager().GetFactionByKey(key));
			array<SCR_EntityCatalogEntry> entries = {};
			faction.GetFactionEntityCatalogOfType(EEntityCatalogType.VEHICLE).GetEntityList(entries);
			foreach (SCR_EntityCatalogEntry entry : entries)
			{
				SCR_EntityCatalogSpawnerData spawner = SCR_EntityCatalogSpawnerData.Cast(entry.GetEntityDataOfType(SCR_EntityCatalogSpawnerData));
				if (!spawner) continue;
				total++;
				if (Check(entry.GetPrefab(), spawner, entry.GetCatalogIndex(), key)) passed++;
				Resource resource = Resource.Load(entry.GetPrefab());
				array<string> resources = {};
				set<BaseContainer> visited = new set<BaseContainer>();
				Features(SCR_BaseContainerTools.FindEntitySource(resource), resources, visited);
				string resourceText;
				foreach (string item : resources) resourceText += item + ";";
				Print(string.Format("[AICF][MENU_PRICE_FEATURES] prefab=%1 resources=%2", entry.GetPrefab(), resourceText));
				Measure(entry.GetPrefab());
			}
			array<ResourceName> groups = {};
			manager.GetFilteredEditorPrefabs(EEntityCatalogType.GROUP, EEditorMode.BUILDING, faction, groups);
			foreach (int index, ResourceName group : groups)
			{
				total++;
				if (Check(group, null, index, key)) passed++;
			}
		}
		Print(string.Format("[AICF][MENU_PRICE_RESULT] server=%1 passed=%2 total=%3", Replication.IsServer(), passed, total));
	}
}

modded class AICF_MatchController
{
	protected bool m_bAICFMenuPriceChecked;
	protected int m_iAICFMenuPriceDoneAt;
	protected ref AICF_MenuTransactionProbe m_AICFMenuTransactions;

	override protected void Update()
	{
		super.Update();
		string enabled;
		if (!System.GetCLIParam("aicfMenuPriceProbe", enabled) || !m_bRosterReady)
			return;
		if (!m_bAICFMenuPriceChecked)
		{
			m_bAICFMenuPriceChecked = true;
			AICF_MenuPriceProbe.Run();
			m_iAICFMenuPriceDoneAt = System.GetTickCount();
			string transactions;
			if (System.GetCLIParam("aicfMenuTransactions", transactions))
			{
				m_AICFMenuTransactions = new AICF_MenuTransactionProbe();
				m_AICFMenuTransactions.Start();
			}
		}
		if (m_AICFMenuTransactions)
		{
			if (!m_AICFMenuTransactions.Tick()) return;
			m_AICFMenuTransactions = null;
		}
		int wait;
		if (enabled == "client") wait = 60000;
		if (System.GetTickCount() - m_iAICFMenuPriceDoneAt >= wait)
			GetGame().RequestClose();
	}

	override protected void Stop(bool cleanupEntities)
	{
		if (m_AICFMenuTransactions) m_AICFMenuTransactions.Cleanup();
		AICF_MenuTransactionProbe.s_Active = null;
		super.Stop(cleanupEntities);
	}
}

modded class SCR_GameModeCampaign
{
	protected bool m_bAICFMenuPriceClientDone;

	override void OnGameStart()
	{
		super.OnGameStart();
		string enabled;
		if (!Replication.IsServer() && System.GetCLIParam("aicfMenuPriceProbe", enabled))
			GetGame().GetCallqueue().CallLater(AICF_CheckMenuPriceClient, 1000, true);
	}

	protected void AICF_CheckMenuPriceClient()
	{
		if (m_bAICFMenuPriceClientDone || !AICF_RHSMenuPricePolicy.IsActive()) return;
		m_bAICFMenuPriceClientDone = true;
		AICF_MenuPriceProbe.Run();
		GetGame().RequestClose();
	}

	override void OnGameEnd()
	{
		GetGame().GetCallqueue().Remove(AICF_CheckMenuPriceClient);
		super.OnGameEnd();
	}
}
