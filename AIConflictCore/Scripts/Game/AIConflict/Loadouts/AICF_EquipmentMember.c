// Backend target не использует UI ItemPreviewManager cache. Создаём собственную
// local entity в собственном мире; её lifetime принадлежит только этому draft.
class AICF_EquipmentDraft : AICF_LoadoutDraft
{
	protected void DetachRoots()
	{
		set<BaseInventoryStorageComponent> storages = new set<BaseInventoryStorageComponent>();
		SCR_PlayerArsenalLoadout.FindStorageComponents(m_Character, storages);
		foreach (BaseInventoryStorageComponent storage : storages)
		{
			array<InventoryItemComponent> items = {};
			storage.GetOwnedItems(items, false);
			foreach (InventoryItemComponent item : items)
			{
				if (!item) continue;
				InventoryStorageSlot parent = item.GetParentSlot();
				if (!parent || parent.GetStorage().GetOwner() != m_Character) continue;
				IEntity entity = item.GetOwner();
				parent.DetachEntity();
				// Detached root больше не является child бойца. Удаляем его
				// вместе с nested inventory до освобождения собственного world.
				if (entity && item && !item.GetParentSlot() && entity.GetWorld() == m_Character.GetWorld())
					SCR_EntityHelper.DeleteEntityAndChildren(entity);
			}
		}
	}

	override void Clear()
	{
		if (m_Character && m_Character.GetWorld() != GetGame().GetWorld())
		{
			DetachRoots();
			SCR_EntityHelper.DeleteEntityAndChildren(m_Character);
		}
		m_Character = null;
		m_Preview = null;
		m_World = null;
	}

	override bool Build(AICF_LoadoutRecipe recipe, AICF_LoadoutCatalog catalog, out string reason)
	{
		Clear();
		reason = "EQUIPMENT_WORLD_UNAVAILABLE";
		m_World = BaseWorld.CreateWorld("ChimeraWorld", "AICF_Equipment_" + (++s_iWorldSequence).ToString());
		if (!m_World || !m_World.IsValid()) return false;
		ChimeraWorld world = m_World.GetRef();
		world.LoadSystems("{3A1DEDB2A5818B53}Configs/Systems/MapPreviewSystemsConfig.conf");
		Resource resource = Resource.Load(recipe.m_sCharacter);
		if (!resource || !resource.IsValid()) { reason = "CHARACTER_RESOURCE_MISSING"; return false; }
		m_Character = GetGame().SpawnEntityPrefabLocal(resource, world);
		if (!m_Character || m_Character.GetWorld() != world || world == GetGame().GetWorld()) return false;
		AIControlComponent control = AIControlComponent.Cast(m_Character.FindComponent(AIControlComponent));
		if (control) control.DeactivateAI();
		KeepAppearance(recipe.m_sCharacter);
		reason = "DEFAULT_LOADOUT_FAILED";
		if (!AICF_ContentProfile.GetActive().PrepareDefaultLoadout(m_Character, recipe.m_sCharacter, recipe.m_sFaction)) return false;
		AICF_LoadoutInventory.Signature(m_Character, catalog, m_iBaselineCost);
		return AICF_LoadoutInventory.Replay(m_Character, recipe, catalog, reason);
	}
}

// Immutable inventory target. Preview entities служат только чтению совместимости.
class AICF_EquipmentPlan
{
	string m_sKey;
	string m_sFailureReason;
	ref AICF_LoadoutDraft m_Draft;
	string m_sInventory;
	string m_sSignature;
	int m_iPrimaryRounds;
	int m_iLauncherRounds;
	ref array<ResourceName> m_aRequired = {};
	ref map<ResourceName, int> m_mConsumables = new map<ResourceName, int>();

	bool Build(ResourceName source, SCR_CampaignFaction faction, AICF_LoadoutBinding binding)
	{
		AICF_LoadoutRecipe recipe = new AICF_LoadoutRecipe();
		recipe.m_sCharacter = source;
		recipe.m_sFaction = AICF_ContentProfile.GetActive().GetStableFactionKey(faction.GetFactionKey());
		AICF_LoadoutCatalog catalog = new AICF_LoadoutCatalog(faction);
		m_Draft = new AICF_EquipmentDraft();
		if (!m_Draft.Build(recipe, catalog, m_sFailureReason)) return false;
		IEntity model = m_Draft.GetCharacter();
		AIControlComponent control = AIControlComponent.Cast(model.FindComponent(AIControlComponent));
		if (control) control.DeactivateAI();
		m_sFailureReason = "BINDING_RESTORE_FAILED";
		if (binding && !AICF_LoadoutInventory.Restore(model, binding.m_sInventory)) return false;
		int ignored;
		m_sSignature = AICF_LoadoutInventory.Signature(model, catalog, ignored);
		m_sFailureReason = "INVENTORY_CAPTURE_FAILED";
		if (m_sSignature.IsEmpty() || !AICF_LoadoutInventory.Capture(model, m_sInventory)) return false;
		BaseWeaponComponent modelPrimary = AICF_EquipmentInventory.Weapon(model, false);
		m_sFailureReason = "PRIMARY_MUZZLE_MISSING";
		BaseMuzzleComponent primaryMuzzle = AICF_EquipmentInventory.Muzzle(modelPrimary);
		if (!modelPrimary || !primaryMuzzle) return false;
		m_iPrimaryRounds = AICF_EquipmentInventory.Rounds(model, modelPrimary);
		// В native probe chained getter в условии цикла удерживал последний
		// MuzzleComponent после Build и ломал delete private model. Ссылка
		// имеет явный scope и освобождается до следующей inventory операции.
		for (int barrel; barrel < primaryMuzzle.GetBarrelsCount(); barrel++)
			if (primaryMuzzle.IsBarrelChambered(barrel)) m_iPrimaryRounds--;
		primaryMuzzle = null;
		m_iLauncherRounds = AICF_EquipmentInventory.Rounds(model, AICF_EquipmentInventory.Weapon(model, true));
		array<IEntity> rawItems = {};
		array<IEntity> items = {};
		AICF_EquipmentInventory.Manager(model).GetItems(rawItems);
		AICF_EquipmentInventory.UniqueItems(rawItems, items);
		set<BaseInventoryStorageComponent> roots = new set<BaseInventoryStorageComponent>();
		SCR_PlayerArsenalLoadout.FindStorageComponents(model, roots);
		foreach (BaseInventoryStorageComponent root : roots)
		{
			if (!AICF_LoadoutInventory.Editable(root)) continue;
			array<InventoryItemComponent> rootItems = {};
			root.GetOwnedItems(rootItems, false);
			foreach (InventoryItemComponent component : rootItems)
			{
				if (!component.GetParentSlot() || component.GetParentSlot().GetStorage().GetOwner() != model) continue;
				ResourceName rootPrefab = SCR_ResourceNameUtils.GetPrefabName(component.GetOwner());
				if (!m_aRequired.Contains(rootPrefab)) m_aRequired.Insert(rootPrefab);
			}
		}
		// Baked содержимое доступного контейнера/оружия приходит вместе с ним.
		// Явно добавленные вложенные предметы всё равно требуют возможности выдачи.
		if (binding)
			foreach (ResourceName added : binding.m_Recipe.m_aPrefabs)
				if (!added.IsEmpty() && !m_aRequired.Contains(added)) m_aRequired.Insert(added);
		foreach (IEntity item : items)
		{
			ResourceName prefab = SCR_ResourceNameUtils.GetPrefabName(item);
			BaseWeaponComponent throwable = BaseWeaponComponent.Cast(item.FindComponent(BaseWeaponComponent));
			bool isThrowable = throwable && (throwable.GetWeaponType() == EWeaponType.WT_FRAGGRENADE || throwable.GetWeaponType() == EWeaponType.WT_SMOKEGRENADE);
			if (!isThrowable && !SCR_ConsumableItemComponent.Cast(item.FindComponent(SCR_ConsumableItemComponent))) continue;
			int count;
			m_mConsumables.Find(prefab, count);
			m_mConsumables.Set(prefab, count + 1);
		}
		m_sFailureReason = "PRIMARY_AMMUNITION_EMPTY";
		if (m_iPrimaryRounds <= 0) return false;
		m_sFailureReason = string.Empty;
		return true;
	}

	BaseWeaponComponent ModelWeapon(bool launcher)
	{
		return AICF_EquipmentInventory.Weapon(m_Draft.GetCharacter(), launcher);
	}

	void ~AICF_EquipmentPlan()
	{
		if (!m_Draft || !m_Draft.GetCharacter() || m_Draft.GetCharacter().GetWorld() == GetGame().GetWorld()) return;
		// Active preview weapon/controller может удерживать muzzle. Сначала
		// разрываем native hand/AI связь, затем Draft освобождает свой мир.
		CharacterControllerComponent.Cast(m_Draft.GetCharacter().FindComponent(CharacterControllerComponent)).TryEquipRightHandItem(null, EEquipItemType.EEquipTypeUnarmedDeliberate, true);
	}
}

class AICF_EquipmentMember
{
	AICF_GroupSlot m_Slot;
	SCR_AIGroup m_Group;
	EntityID m_GroupId;
	IEntity m_Character;
	EntityID m_CharacterId;
	SCR_CampaignFaction m_Faction;
	int m_iGeneration;
	int m_iIndex;
	int m_iRevision;
	ref AICF_EquipmentPlan m_Plan;
	ref AICF_EquipmentTransaction m_Transaction;
	bool m_bPending;
	bool m_bSeen;
	int m_iPrimaryBefore;
	int m_iLauncherBefore;
	string m_sNeedReason;

	static bool UsableConsumable(IEntity item)
	{
		BaseMagazineComponent magazine = BaseMagazineComponent.Cast(item.FindComponent(BaseMagazineComponent));
		return !magazine || (!magazine.IsUsed() && magazine.GetAmmoCount() > 0);
	}

	bool Current()
	{
		if (!m_Slot || !m_Slot.IsCombatReady() || m_Slot.GetGroup() != m_Group || !m_Group ||
			m_Group.GetID() != m_GroupId || m_Group.GetFaction() != m_Faction ||
			m_Slot.GetSpawnGeneration() != m_iGeneration || m_Slot.GetRosterMember(m_iIndex) != m_Character ||
			!m_Character || m_Character.GetID() != m_CharacterId ||
			SCR_Faction.GetEntityFaction(m_Character) != m_Faction || !AICF_EquipmentInventory.SafeMember(m_Character, m_Group)) return false;
		AICF_LoadoutBinding binding = m_Slot.GetLoadout(m_iIndex);
		if (binding) return binding.m_iRevision == m_iRevision;
		return m_iRevision == 0;
	}

	int Consumables(ResourceName prefab)
	{
		array<IEntity> items = {};
		AICF_EquipmentInventory.Manager(m_Character).GetItems(items);
		return ConsumablesIn(items, prefab);
	}

	int ConsumablesIn(array<IEntity> items, ResourceName prefab)
	{
		array<IEntity> unique = {};
		AICF_EquipmentInventory.UniqueItems(items, unique);
		int count;
		foreach (IEntity item : unique)
		{
			if (SCR_ResourceNameUtils.GetPrefabName(item) != prefab) continue;
			BaseWeaponComponent throwable = BaseWeaponComponent.Cast(item.FindComponent(BaseWeaponComponent));
			if (throwable && !UsableConsumable(item)) continue;
			count++;
		}
		return count;
	}

	bool Evaluate(AICF_EquipmentConfig config)
	{
		if (!Current() || !m_Plan) return false;
		if (m_Transaction && !m_Transaction.m_bFinished) { m_bPending = true; return true; }
		BaseWeaponComponent primary = AICF_EquipmentInventory.Weapon(m_Character, false);
		BaseWeaponComponent launcher = AICF_EquipmentInventory.Weapon(m_Character, true);
		m_iPrimaryBefore = AICF_EquipmentInventory.Rounds(m_Character, primary);
		m_iLauncherBefore = AICF_EquipmentInventory.Rounds(m_Character, launcher);
		BaseWeaponComponent requiredPrimary = m_Plan.ModelWeapon(false);
		bool missing = requiredPrimary && (!primary || SCR_ResourceNameUtils.GetPrefabName(primary.GetOwner()) !=
			SCR_ResourceNameUtils.GetPrefabName(requiredPrimary.GetOwner()));
		bool revision = m_Slot.GetMemberLoadoutRevision(m_iIndex) != m_iRevision;
		m_sNeedReason = "AMMUNITION_LOW";
		if (m_Plan.m_iLauncherRounds > 0 && m_iLauncherBefore < 1) m_sNeedReason = "ANTI_TANK_EMPTY";
		if (missing) m_sNeedReason = "WEAPON_MISSING";
		if (revision) m_sNeedReason = "PRESET_REVISION";
		bool need = missing || revision || config.Low(m_iPrimaryBefore, m_Plan.m_iPrimaryRounds, false, m_bPending) ||
			config.Low(m_iLauncherBefore, m_Plan.m_iLauncherRounds, true, m_bPending);
		foreach (ResourceName prefab, int target : m_Plan.m_mConsumables)
			if (config.Low(Consumables(prefab), target, false, m_bPending))
			{
				if (!need) m_sNeedReason = "CONSUMABLE_LOW";
				need = true;
			}
		m_bPending = need;
		return need;
	}

	bool Wants(IEntity item)
	{
		if (!Current() || !m_Plan || !item) return false;
		BaseWeaponComponent candidate = BaseWeaponComponent.Cast(item.FindComponent(BaseWeaponComponent));
		if (candidate)
		{
			if (candidate.GetWeaponType() == EWeaponType.WT_FRAGGRENADE || candidate.GetWeaponType() == EWeaponType.WT_SMOKEGRENADE)
			{
				int wanted;
				ResourceName throwablePrefab = SCR_ResourceNameUtils.GetPrefabName(item);
				return m_Plan.m_mConsumables.Find(throwablePrefab, wanted) && Consumables(throwablePrefab) < wanted &&
					UsableConsumable(item);
			}
			if (candidate.GetWeaponType() != EWeaponType.WT_ROCKETLAUNCHER || m_Plan.m_iLauncherRounds <= 0 ||
				AICF_EquipmentInventory.Weapon(m_Character, true)) return false;
			BaseMuzzleComponent muzzle = candidate.GetCurrentMuzzle();
			return AICF_ContentProfile.GetActive().AllowsScavengedLauncher(candidate) && muzzle &&
				(AICF_EquipmentInventory.Loaded(muzzle) > 0 || (!muzzle.IsDisposable() && muzzle.GetMagazineWell()));
		}
		BaseMagazineComponent magazine = BaseMagazineComponent.Cast(item.FindComponent(BaseMagazineComponent));
		if (magazine && magazine.GetAmmoCount() > 0)
		{
			BaseWeaponComponent primary = AICF_EquipmentInventory.Weapon(m_Character, false);
			if (primary && AICF_EquipmentInventory.Rounds(m_Character, primary) < m_Plan.m_iPrimaryRounds &&
				AICF_EquipmentInventory.Compatible(AICF_EquipmentInventory.Muzzle(primary), magazine)) return true;
			BaseWeaponComponent launcher = AICF_EquipmentInventory.Weapon(m_Character, true);
			if (m_Plan.m_iLauncherRounds > 0 && launcher && !launcher.GetCurrentMuzzle().IsDisposable() &&
				AICF_EquipmentInventory.Rounds(m_Character, launcher) < m_Plan.m_iLauncherRounds &&
				AICF_EquipmentInventory.Compatible(launcher.GetCurrentMuzzle(), magazine)) return true;
			return false;
		}
		int target;
		ResourceName prefab = SCR_ResourceNameUtils.GetPrefabName(item);
		return SCR_ConsumableItemComponent.Cast(item.FindComponent(SCR_ConsumableItemComponent)) &&
			m_Plan.m_mConsumables.Find(prefab, target) && Consumables(prefab) < target;
	}
}
