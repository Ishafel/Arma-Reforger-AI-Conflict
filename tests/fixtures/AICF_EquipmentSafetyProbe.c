modded class SCR_InventoryStorageManagerComponent
{
	static IEntity s_AICFEquipmentRefusedItem;
	override protected bool ShouldForbidRemoveByInstigator(InventoryStorageManagerComponent instigatorManager, BaseInventoryStorageComponent fromStorage, IEntity item)
	{
		if (item && item == s_AICFEquipmentRefusedItem) return true;
		return super.ShouldForbidRemoveByInstigator(instigatorManager, fromStorage, item);
	}
}

// Общий stage с AICF_EquipmentProbe.c. Один native transfer на существующей entity,
// два получателя и реальные launcher prefabs из текущих catalog/profile.
modded class AICF_MatchController
{
	protected ref AICF_FactionState m_EQSafetyState;
	protected SCR_CampaignFaction m_EQSafetyFaction;
	protected ref AICF_EquipmentService m_EQSafetyService;
	protected ref array<SCR_AIGroup> m_aEQSafetyGroups = {};
	protected ref array<ref AICF_EquipmentMember> m_aEQSafetyMembers = {};
	protected ref array<ref AICF_EquipmentOrder> m_aEQSafetyOrders = {};
	protected ref array<string> m_aEQSafetyLaunchers = {};
	protected ref set<string> m_aEQSafetyFamilies = new set<string>();
	protected IEntity m_EQSafetyCorpse;
	protected int m_iEQSafetyPhase;
	protected int m_iEQSafetyStarted;
	protected int m_iEQSafetyChecks;
	protected int m_iEQSafetyFailures;
	protected int m_iEQSafetyLauncher;
	protected int m_iEQSafetyAt;
	protected IEntity m_EQSafetyPendingItem;
	protected EntityID m_EQSafetyPendingId;
	protected ref AICF_EquipmentSlot m_EQSafetyRecord;
	protected EntityID m_EQSafetyPrimary;
	protected bool m_bEQSafetyNegativeCases;
	protected bool m_bEQSafetyCapacityChecked;
	protected ref array<ref AICF_EquipmentPlan> m_aEQSafetyPreviewPlans = {};

	protected void AICF_EQSCheck(string name, bool passed, string detail = "")
	{
		m_iEQSafetyChecks++;
		if (!passed) m_iEQSafetyFailures++;
		Print(string.Format("[AICF][EQUIPMENT_SAFETY] case=%1 passed=%2 phase=%3 %4", name, passed, m_iEQSafetyPhase, detail));
	}

	protected void AICF_EQSFinish()
	{
		foreach (AICF_EquipmentOrder order : m_aEQSafetyOrders) m_OrderPlanner.EndEquipmentOrder(order, m_ObjectiveGraph, m_TargetSelector, false);
		if (m_EQSafetyService) m_EQSafetyService.Stop();
		foreach (SCR_AIGroup group : m_aEQSafetyGroups)
		{
			if (!group) continue;
			group.DespawnMembers();
			RplComponent.DeleteRplEntity(group, false);
		}
		Print(string.Format("[AICF][EQUIPMENT_SAFETY_FINISHED] checks=%1 failures=%2 families=%3 profile=%4", m_iEQSafetyChecks, m_iEQSafetyFailures, m_aEQSafetyFamilies.Count(), m_ContentProfile.GetProfileKey()));
		m_iEQSafetyPhase = 99;
		GetGame().RequestClose();
	}

	protected BaseWeaponComponent AICF_EQSLauncher(IEntity character)
	{
		array<IEntity> items = {};
		AICF_EquipmentInventory.Manager(character).GetItems(items);
		foreach (IEntity item : items)
		{
			BaseWeaponComponent weapon = BaseWeaponComponent.Cast(item.FindComponent(BaseWeaponComponent));
			if (weapon && weapon.GetWeaponType() == EWeaponType.WT_ROCKETLAUNCHER) return weapon;
		}
		return null;
	}

	protected void AICF_EQSClearLauncher(IEntity character)
	{
		BaseWeaponComponent weapon = AICF_EQSLauncher(character);
		if (weapon) AICF_EquipmentInventory.Manager(character).TryDeleteItem(weapon.GetOwner());
	}

	protected void AICF_EQSClearSideAmmo(IEntity character)
	{
		InventoryStorageManagerComponent manager = AICF_EquipmentInventory.Manager(character);
		BaseWeaponComponent primary = AICF_EquipmentInventory.Weapon(character, false);
		array<IEntity> items = {};
		manager.GetItems(items);
		foreach (IEntity item : items)
		{
			BaseMagazineComponent magazine = BaseMagazineComponent.Cast(item.FindComponent(BaseMagazineComponent));
			if (!magazine || !magazine.GetMagazineWell()) continue;
			if (primary && AICF_EquipmentInventory.Compatible(primary.GetCurrentMuzzle(), magazine)) continue;
			manager.TryDeleteItem(item);
		}
	}

	protected AICF_EquipmentOrder AICF_EQSOrder(AICF_EquipmentMember member)
	{
		AICF_EquipmentOrder order = new AICF_EquipmentOrder();
		order.m_Slot = member.m_Slot;
		order.m_Group = member.m_Group;
		order.m_GroupId = member.m_GroupId;
		order.m_Faction = m_EQSafetyFaction;
		order.m_iGeneration = member.m_iGeneration;
		order.m_iIntent = order.m_Slot.GetStrategicIntentRevision();
		order.m_iAssignment = order.m_Slot.GetStrategicAssignmentRevision();
		order.m_iGraphRevision = m_ObjectiveGraph.GetRevision();
		order.m_iToken = 100 + member.m_Slot.GetSlotId();
		order.m_Waypoint = order.m_Slot.GetWaypoint();
		order.m_Corpse = m_EQSafetyCorpse;
		order.m_CorpseId = m_EQSafetyCorpse.GetID();
		order.m_vPosition = m_EQSafetyCorpse.GetOrigin();
		order.m_iDeadline = System.GetTickCount() + AICF_EquipmentConfig.CORPSE_TIMEOUT_MS;
		order.m_aMembers.Insert(member);
		return order;
	}

	protected SCR_AIUtilityComponent AICF_EQSUtility(AICF_EquipmentMember member)
	{
		AIControlComponent control = AIControlComponent.Cast(member.m_Character.FindComponent(AIControlComponent));
		return SCR_AIUtilityComponent.Cast(control.GetControlAIAgent().FindComponent(SCR_AIUtilityComponent));
	}

	protected void AICF_EQSBackpack(IEntity character)
	{
		AICF_LoadoutCatalog catalog = new AICF_LoadoutCatalog(m_EQSafetyFaction);
		InventoryStorageManagerComponent manager = AICF_EquipmentInventory.Manager(character);
		array<string> backpacks = {};
		catalog.List(SCR_EArsenalItemType.BACKPACK, backpacks);
		foreach (string prefab : backpacks)
		{
			if (!manager.CanInsertResource(prefab, EStoragePurpose.PURPOSE_LOADOUT_PROXY)) continue;
			if (manager.TrySpawnPrefabToStorage(prefab, purpose: EStoragePurpose.PURPOSE_LOADOUT_PROXY)) return;
		}
	}

	protected void AICF_EQSAmmoPolicy(AICF_EquipmentMember member)
	{
		BaseWeaponComponent weapon = AICF_EquipmentInventory.Weapon(member.m_Character, false);
		BaseMuzzleComponent muzzle = AICF_EquipmentInventory.Muzzle(weapon);
		BaseMagazineComponent loaded = muzzle.GetMagazine();
		if (!loaded) { AICF_EQSCheck("PRIMARY_MAGAZINE_PRESENT", false); return; }
		array<BaseMagazineComponent> magazines = {};
		array<int> counts = {};
		array<IEntity> items = {};
		AICF_EquipmentInventory.Manager(member.m_Character).GetItems(items);
		magazines.Insert(loaded);
		counts.Insert(loaded.GetAmmoCount());
		foreach (IEntity item : items)
		{
			BaseMagazineComponent magazine = BaseMagazineComponent.Cast(item.FindComponent(BaseMagazineComponent));
			if (!magazine || magazines.Contains(magazine) || !AICF_EquipmentInventory.Compatible(muzzle, magazine)) continue;
			magazines.Insert(magazine);
			counts.Insert(magazine.GetAmmoCount());
		}
		int original = AICF_EquipmentInventory.Rounds(member.m_Character, weapon);
		loaded.SetAmmoCount(loaded.GetAmmoCount() - 1);
		AICF_EquipmentConfig config = new AICF_EquipmentConfig();
		AICF_EQSCheck("ONE_SHOT_REAL_INVENTORY", AICF_EquipmentInventory.Rounds(member.m_Character, weapon) == original - 1 &&
			!config.Low(original - 1, member.m_Plan.m_iPrimaryRounds, false, false));
		foreach (BaseMagazineComponent empty : magazines) empty.SetAmmoCount(0);
		loaded.SetAmmoCount(3);
		if (magazines.Count() > 1) magazines[1].SetAmmoCount(1);
		int chamber;
		for (int b; b < muzzle.GetBarrelsCount(); b++) if (muzzle.IsBarrelChambered(b)) chamber++;
		AICF_EQSCheck("PARTIAL_MAGAZINES_ROUNDS", magazines.Count() > 1 && AICF_EquipmentInventory.Rounds(member.m_Character, weapon) == 4 + chamber,
			string.Format("actual=%1 expected=%2 native=%3 magazine=%4 muzzle_type=%5", AICF_EquipmentInventory.Rounds(member.m_Character, weapon), 4 + chamber, muzzle.GetAmmoCount(), loaded.GetAmmoCount(), muzzle.GetMuzzleType()));
		AICF_EQSCheck("PARTIAL_MAGAZINES_THRESHOLD", config.Low(AICF_EquipmentInventory.Rounds(member.m_Character, weapon), member.m_Plan.m_iPrimaryRounds, false, false));
		if (magazines.Count() > 1)
		{
			array<IEntity> repeated = {loaded.GetOwner(), magazines[1].GetOwner(), loaded.GetOwner(), magazines[1].GetOwner(), null};
			int expected = 4 + chamber;
			AICF_EQSCheck("DUPLICATE_MAGAZINES_ONCE", AICF_EquipmentInventory.RoundsIn(repeated, weapon) == expected);
			AICF_EQSCheck("DUPLICATE_MAGAZINES_LOW", config.Low(AICF_EquipmentInventory.RoundsIn(repeated, weapon), expected * 4, false, false));
			AICF_EQSCheck("COUNT_INPUT_UNCHANGED", repeated.Count() == 5 && loaded.GetAmmoCount() == 3 && magazines[1].GetAmmoCount() == 1);
		}
		IEntity consumable, other;
		foreach (IEntity candidate : items)
			if (candidate && SCR_ConsumableItemComponent.Cast(candidate.FindComponent(SCR_ConsumableItemComponent))) { consumable = candidate; break; }
		if (consumable)
		{
			ResourceName prefab = SCR_ResourceNameUtils.GetPrefabName(consumable);
			array<IEntity> modelItems = {};
			AICF_EquipmentInventory.Manager(member.m_Plan.m_Draft.GetCharacter()).GetItems(modelItems);
			foreach (IEntity modelItem : modelItems)
				if (modelItem && modelItem != consumable && SCR_ResourceNameUtils.GetPrefabName(modelItem) == prefab) { other = modelItem; break; }
			array<IEntity> repeatedConsumables = {consumable, consumable, null};
			AICF_EQSCheck("DUPLICATE_CONSUMABLES_ONCE", member.ConsumablesIn(repeatedConsumables, prefab) == 1);
			AICF_EQSCheck("DUPLICATE_CONSUMABLES_LOW", config.Low(member.ConsumablesIn(repeatedConsumables, prefab), 4, false, false));
			repeatedConsumables.Insert(other);
			AICF_EQSCheck("DISTINCT_SAME_PREFAB_COUNTED", other && member.ConsumablesIn(repeatedConsumables, prefab) == 2);
		}
		else AICF_EQSCheck("DUPLICATE_CONSUMABLES_ONCE", false, "reason=NO_NATIVE_CONSUMABLE");
		foreach (int index, BaseMagazineComponent restore : magazines) restore.SetAmmoCount(counts[index]);
	}

	protected void AICF_EQSFamily(AICF_EquipmentMember member, AICF_EquipmentOrder order)
	{
		if (m_iEQSafetyLauncher >= m_aEQSafetyLaunchers.Count())
		{
			AICF_EQSCheck("FAMILIES_EXECUTED", m_aEQSafetyFamilies.Count() > 0);
			m_iEQSafetyPhase = 5;
			return;
		}
		ResourceName prefab = m_aEQSafetyLaunchers[m_iEQSafetyLauncher++];
		IEntity preview = GetGame().SpawnEntityPrefabLocal(Resource.Load(prefab), member.m_Plan.m_Draft.GetCharacter().GetWorld());
		BaseWeaponComponent previewWeapon;
		if (preview) previewWeapon = BaseWeaponComponent.Cast(preview.FindComponent(BaseWeaponComponent));
		bool isLauncher = previewWeapon && previewWeapon.GetWeaponType() == EWeaponType.WT_ROCKETLAUNCHER;
		if (preview) SCR_EntityHelper.DeleteEntityAndChildren(preview);
		if (!isLauncher)
		{
			Print("[AICF][EQUIPMENT_PREFAB_SKIPPED] reason=NATIVE_NOT_LAUNCHER prefab=" + prefab);
			return;
		}
		AICF_EQSClearSideAmmo(member.m_Character);
		AICF_EQSClearSideAmmo(m_EQSafetyCorpse);
		AICF_EQSClearLauncher(member.m_Character);
		AICF_EQSClearLauncher(m_EQSafetyCorpse);
		InventoryStorageManagerComponent source = AICF_EquipmentInventory.Manager(m_EQSafetyCorpse);
		if (!source.TrySpawnPrefabToStorage(prefab, purpose: EStoragePurpose.PURPOSE_WEAPON_PROXY))
		{
			AICF_EQSCheck("CATALOG_PREFAB_SPAWN", false, "prefab=" + prefab);
			return;
		}
		BaseWeaponComponent launcher = AICF_EQSLauncher(m_EQSafetyCorpse);
		if (!launcher)
		{
			AICF_EQSCheck("CATALOG_LAUNCHER_NATIVE", false, "prefab=" + prefab);
			return;
		}
		BaseMuzzleComponent muzzle = launcher.GetCurrentMuzzle();
		if (!muzzle) { AICF_EQSCheck("MUZZLE_PRESENT", false, "prefab=" + prefab); return; }
		string family = muzzle.GetDefaultMagazineOrProjectileName() + muzzle.IsDisposable().ToString();
		if (muzzle.GetMagazineWell()) family += muzzle.GetMagazineWell().Type().ToString();
		if (m_aEQSafetyFamilies.Contains(family)) return;
		m_aEQSafetyFamilies.Insert(family);
		if (!m_bEQSafetyNegativeCases)
		{
			m_bEQSafetyNegativeCases = true;
			string refused;
			AICF_EquipmentPlan allowed = member.m_Plan;
			AICF_EquipmentPlan rifle = new AICF_EquipmentPlan();
			m_aEQSafetyPreviewPlans.Insert(rifle);
			string role;
			rifle.Build(m_GroupSpawner.ResolveRecruitPrefab(m_EQSafetyFaction, 0, role), m_EQSafetyFaction, null);
			member.m_Plan = rifle;
			AICF_EQSCheck("NON_AT_ROLE_REFUSED", !AICF_EquipmentInventory.Transfer(order, member, m_EQSafetyCorpse, m_EQSafetyCorpse.GetID(), launcher.GetOwner(), launcher.GetOwner().GetID(), refused) && source.Contains(launcher.GetOwner()));
			member.m_Plan = allowed;
			InventoryStorageManagerComponent slotTarget = AICF_EquipmentInventory.Manager(member.m_Character);
			array<IEntity> beforeSlots = {};
			slotTarget.GetItems(beforeSlots);
			ResourceName riflePrefab = SCR_ResourceNameUtils.GetPrefabName(AICF_EquipmentInventory.Weapon(member.m_Character, false).GetOwner());
			for (int occupied; occupied < 4 && slotTarget.TrySpawnPrefabToStorage(riflePrefab, purpose: EStoragePurpose.PURPOSE_WEAPON_PROXY); occupied++) {}
			BaseInventoryStorageComponent weaponRoom = slotTarget.FindStorageForItem(launcher.GetOwner(), EStoragePurpose.PURPOSE_WEAPON_PROXY);
			AICF_EQSCheck("OCCUPIED_WEAPON_SLOT_REFUSED", (!weaponRoom || !slotTarget.CanMoveItemToStorage(launcher.GetOwner(), weaponRoom)) &&
				!AICF_EquipmentInventory.Transfer(order, member, m_EQSafetyCorpse, m_EQSafetyCorpse.GetID(), launcher.GetOwner(), launcher.GetOwner().GetID(), refused) && source.Contains(launcher.GetOwner()) &&
				AICF_EquipmentInventory.Weapon(member.m_Character, false).GetOwner().GetID() == m_EQSafetyPrimary);
			array<IEntity> afterSlots = {};
			slotTarget.GetItems(afterSlots);
			foreach (IEntity filledSlot : afterSlots) if (!beforeSlots.Contains(filledSlot)) slotTarget.TryDeleteItem(filledSlot);
			SCR_InventoryStorageManagerComponent.s_AICFEquipmentRefusedItem = launcher.GetOwner();
			AICF_EQSCheck("NATIVE_TRANSFER_REFUSED", !AICF_EquipmentInventory.Transfer(order, member, m_EQSafetyCorpse, m_EQSafetyCorpse.GetID(), launcher.GetOwner(), launcher.GetOwner().GetID(), refused) && source.Contains(launcher.GetOwner()));
			SCR_InventoryStorageManagerComponent.s_AICFEquipmentRefusedItem = null;
		}
		int loaded = AICF_EquipmentInventory.Loaded(muzzle);
		string reason;
		bool moved = AICF_EquipmentInventory.Transfer(order, member, m_EQSafetyCorpse, m_EQSafetyCorpse.GetID(), launcher.GetOwner(), launcher.GetOwner().GetID(), reason);
		AICF_EQSCheck("LAUNCHER_TRANSFER", moved && AICF_EQSLauncher(member.m_Character) == launcher && member.m_iRevision == 0,
			"prefab=" + prefab + " family=" + family + " reason=" + reason);
		AICF_EQSCheck("PRIMARY_RETAINED", AICF_EquipmentInventory.Weapon(member.m_Character, false).GetOwner().GetID() == m_EQSafetyPrimary);
		if (!moved) return;
		AICF_EQSCheck("LOADED_READBACK", AICF_EquipmentInventory.Loaded(muzzle) == loaded);
		if (muzzle.IsDisposable())
		{
			IEntity spare = GetGame().SpawnEntityPrefabLocal(Resource.Load(prefab), member.m_Plan.m_Draft.GetCharacter().GetWorld());
			BaseWeaponComponent spareWeapon;
			if (spare) spareWeapon = BaseWeaponComponent.Cast(spare.FindComponent(BaseWeaponComponent));
			array<IEntity> repeatedLaunchers = {launcher.GetOwner(), spare, spare, launcher.GetOwner(), null};
			AICF_EQSCheck("DUPLICATE_DISPOSABLES_ONCE", spareWeapon &&
				AICF_EquipmentInventory.RoundsIn(repeatedLaunchers, launcher) == loaded + AICF_EquipmentInventory.Loaded(spareWeapon.GetCurrentMuzzle()), "prefab=" + prefab);
			if (spare) SCR_EntityHelper.DeleteEntityAndChildren(spare);
			BaseMagazineComponent magazine = muzzle.GetMagazine();
			if (magazine) magazine.SetAmmoCount(0);
			for (int barrel; barrel < muzzle.GetBarrelsCount(); barrel++) muzzle.ClearChamber(barrel);
			AICF_EQSCheck("DISPOSABLE_USED_EMPTY", AICF_EquipmentInventory.Loaded(muzzle) == 0 &&
				AICF_EquipmentInventory.Rounds(member.m_Character, launcher) == 0, "prefab=" + prefab);
			AICF_EquipmentConfig emptyPolicy = new AICF_EquipmentConfig();
			AICF_EQSCheck("DISPOSABLE_EMPTY_DEFICIT", emptyPolicy.Low(AICF_EquipmentInventory.Rounds(member.m_Character, launcher), member.m_Plan.m_iLauncherRounds, true, false));
			AICF_EQSClearLauncher(member.m_Character);
			AICF_EQSCheck("DISPOSABLE_SPENT_SOURCE_PREPARED", source.TrySpawnPrefabToStorage(prefab, purpose: EStoragePurpose.PURPOSE_WEAPON_PROXY));
			BaseWeaponComponent spent = AICF_EQSLauncher(m_EQSafetyCorpse);
			if (!spent) { AICF_EQSCheck("DISPOSABLE_SPENT_SOURCE_REFUSED", false); return; }
			BaseMuzzleComponent spentMuzzle = spent.GetCurrentMuzzle();
			if (spentMuzzle.GetMagazine()) spentMuzzle.GetMagazine().SetAmmoCount(0);
			for (int spentBarrel; spentBarrel < spentMuzzle.GetBarrelsCount(); spentBarrel++) spentMuzzle.ClearChamber(spentBarrel);
			AICF_EQSCheck("DISPOSABLE_SPENT_SOURCE_REFUSED", !member.Wants(spent.GetOwner()) &&
				!AICF_EquipmentInventory.Transfer(order, member, m_EQSafetyCorpse, m_EQSafetyCorpse.GetID(), spent.GetOwner(), spent.GetOwner().GetID(), reason) && source.Contains(spent.GetOwner()));
			return;
		}
		// Совместимый выстрел создаётся fixture у источника ДО переноса; production его не создаёт.
		ResourceName roundPrefab = muzzle.GetDefaultMagazineOrProjectileName();
		if (muzzle.GetMagazine()) roundPrefab = SCR_ResourceNameUtils.GetPrefabName(muzzle.GetMagazine().GetOwner());
		array<IEntity> items = {};
		source.GetItems(items);
		IEntity round;
		foreach (IEntity existingRound : items)
		{
			BaseMagazineComponent existingMagazine = BaseMagazineComponent.Cast(existingRound.FindComponent(BaseMagazineComponent));
			if (existingMagazine && existingMagazine.GetAmmoCount() > 0 && AICF_EquipmentInventory.Compatible(muzzle, existingMagazine)) round = existingRound;
		}
		if (!round && (roundPrefab.IsEmpty() || !source.TrySpawnPrefabToStorage(roundPrefab, purpose: EStoragePurpose.PURPOSE_DEPOSIT)))
		{
			AICF_EQSCheck("ROUND_SOURCE_SPAWN", false, "prefab=" + roundPrefab);
			return;
		}
		if (!round)
		{
			items.Clear();
			source.GetItems(items);
			foreach (IEntity item : items) if (SCR_ResourceNameUtils.GetPrefabName(item) == roundPrefab) round = item;
		}
		if (!round) { AICF_EQSCheck("ROUND_PRESENT", false, "prefab=" + roundPrefab); return; }
		BaseMagazineComponent actual = BaseMagazineComponent.Cast(round.FindComponent(BaseMagazineComponent));
		BaseMagazineComponent current = muzzle.GetMagazine();
		if (current) current.SetAmmoCount(0);
		for (int b; b < muzzle.GetBarrelsCount(); b++) muzzle.ClearChamber(b);
		AICF_EQSCheck("ROUND_NATIVE_COMPATIBLE", AICF_EquipmentInventory.Compatible(muzzle, actual));
		if (!actual) return;
		array<IEntity> wrongItems = {};
		source.GetItems(wrongItems);
		foreach (IEntity wrong : wrongItems)
		{
			BaseMagazineComponent wrongMagazine = BaseMagazineComponent.Cast(wrong.FindComponent(BaseMagazineComponent));
			if (!wrongMagazine || wrongMagazine.GetAmmoCount() < 1 || AICF_EquipmentInventory.Compatible(muzzle, wrongMagazine)) continue;
			AICF_EQSCheck("INCOMPATIBLE_ROUND_REFUSED", !member.Wants(wrong) &&
				!AICF_EquipmentInventory.Transfer(order, member, m_EQSafetyCorpse, m_EQSafetyCorpse.GetID(), wrong, wrong.GetID(), reason) && source.Contains(wrong));
			break;
		}
		int beforeRounds = actual.GetAmmoCount();
		EntityID roundId = round.GetID();
		if (!m_bEQSafetyCapacityChecked)
		{
			m_bEQSafetyCapacityChecked = true;
			InventoryStorageManagerComponent target = AICF_EquipmentInventory.Manager(member.m_Character);
			array<IEntity> beforeFill = {};
			target.GetItems(beforeFill);
			ResourceName filler = SCR_ResourceNameUtils.GetPrefabName(AICF_EquipmentInventory.Muzzle(AICF_EquipmentInventory.Weapon(member.m_Character, false)).GetMagazine().GetOwner());
			for (int fill; fill < 512 && target.TrySpawnPrefabToStorage(filler, purpose: EStoragePurpose.PURPOSE_DEPOSIT); fill++) {}
			BaseInventoryStorageComponent room = target.FindStorageForItem(round, EStoragePurpose.PURPOSE_DEPOSIT);
			bool full = !room || !target.CanMoveItemToStorage(round, room);
			AICF_EQSCheck("FULL_STORAGE_REFUSED", full && !AICF_EquipmentInventory.Transfer(order, member, m_EQSafetyCorpse, m_EQSafetyCorpse.GetID(), round, roundId, reason) &&
				source.Contains(round) && actual.GetAmmoCount() == beforeRounds && !target.Contains(round));
			array<IEntity> afterFill = {};
			target.GetItems(afterFill);
			foreach (IEntity filled : afterFill) if (!beforeFill.Contains(filled)) target.TryDeleteItem(filled);
		}
		bool transferred = AICF_EquipmentInventory.Transfer(order, member, m_EQSafetyCorpse, m_EQSafetyCorpse.GetID(), round, roundId, reason);
		AICF_EQSCheck("ROUND_TRANSFER", transferred && actual.GetAmmoCount() == beforeRounds && round.GetID() == roundId &&
			!source.Contains(round), "prefab=" + roundPrefab + " reason=" + reason);
	}

	override protected void Update()
	{
		super.Update();
		string enabled;
		if (!System.GetCLIParam("aicfEquipmentProbe", enabled) || (enabled != "safety" && enabled != "counts") || !m_bRosterReady || m_iEQSafetyPhase == 99) return;
		int now = System.GetTickCount();
		if (!m_iEQSafetyStarted)
		{
			m_iEQSafetyStarted = now;
			m_EquipmentService.Stop();
			m_EQSafetyFaction = m_USSRFaction;
			m_EQSafetyState = new AICF_FactionState(m_EQSafetyFaction.GetFactionKey(), m_Config);
			m_EQSafetyService = new AICF_EquipmentService(m_Campaign, m_OrderPlanner, m_ObjectiveGraph, m_TargetSelector, m_GroupSpawner);
			string role;
			ResourceName source = m_GroupSpawner.ResolveRecruitPrefab(m_EQSafetyFaction, 3, role);
			for (int index; index < 3; index++)
			{
				SCR_AIGroup group = m_GroupSpawner.SpawnGroup(m_EQSafetyFaction, m_EQSafetyFaction.GetMainBase(), index, 1, false);
				m_aEQSafetyGroups.Insert(group);
				group.SetLifecyclePolicy(SCR_EAIGroupLifecyclePolicy.Manual);
				group.SetDeleteWhenEmpty(false);
				vector position = m_EQSafetyFaction.GetMainBase().GetSpawnPoint().GetOrigin();
				if (index == 2) position += "20 0 0";
				group.SetOrigin(position);
				group.m_aUnitPrefabSlots.Clear();
				group.m_aUnitPrefabSlots.Insert(source);
				AICF_GroupSlot slot = m_EQSafetyState.GetSlot(index);
				slot.SetDesiredSize(1);
				slot.BeginInitialSpawn();
				slot.BindSpawnedGroup(group);
				group.AICF_TrackMemberPositions();
				m_GroupSpawner.BeginRosterSpawn(group, 1);
			}
			array<SCR_CampaignFaction> factions = {m_USFaction, m_USSRFaction};
			SCR_CampaignFaction fia = SCR_CampaignFaction.Cast(GetGame().GetFactionManager().GetFactionByKey("FIA"));
			if (fia) factions.Insert(fia);
			foreach (SCR_CampaignFaction faction : factions)
			{
				AICF_LoadoutCatalog catalog = new AICF_LoadoutCatalog(faction);
				array<string> launchers = {};
				catalog.List(SCR_EArsenalItemType.ROCKET_LAUNCHER, launchers);
				foreach (string prefab : launchers) if (!m_aEQSafetyLaunchers.Contains(prefab)) m_aEQSafetyLaunchers.Insert(prefab);
			}
			m_aEQSafetyLaunchers.Sort();
			return;
		}
		if (now - m_iEQSafetyStarted > 360000)
		{
			AICF_EQSCheck("TIMEOUT", false);
			AICF_EQSFinish();
			return;
		}
		if (m_iEQSafetyPhase == 0)
		{
			foreach (SCR_AIGroup pending : m_aEQSafetyGroups) if (pending.GetAgentsCount() != 1) return;
			string role;
			ResourceName source = m_GroupSpawner.ResolveRecruitPrefab(m_EQSafetyFaction, 3, role);
			for (int i; i < 3; i++)
			{
				AICF_GroupSlot ready = m_EQSafetyState.GetSlot(i);
				ready.MarkReady();
				ready.RecordDeploymentMembers();
				int agents, recovered;
				m_ManagedAILODPolicy.KeepCaptureEligible(ready.GetGroup(), agents, recovered);
				m_OrderPlanner.AssignPlayerPointOrder(ready, m_EQSafetyFaction, ready.GetRosterMember(0).GetOrigin());
				if (i == 2) { m_EQSafetyCorpse = ready.GetRosterMember(0); continue; }
				AICF_EquipmentMember member = new AICF_EquipmentMember();
				member.m_Slot = ready;
				member.m_Group = ready.GetGroup();
				member.m_GroupId = ready.GetGroup().GetID();
				member.m_Character = ready.GetRosterMember(0);
				member.m_CharacterId = member.m_Character.GetID();
				member.m_Faction = m_EQSafetyFaction;
				member.m_iIndex = 0;
				member.m_iGeneration = ready.GetSpawnGeneration();
				AICF_EquipmentPlan plan = new AICF_EquipmentPlan();
				AICF_EQSCheck("AT_PLAN", plan.Build(source, m_EQSafetyFaction, null) && plan.m_iLauncherRounds > 0);
				member.m_Plan = plan;
				m_aEQSafetyMembers.Insert(member);
				if (i == 0) AICF_EQSAmmoPolicy(member);
			}
			if (enabled == "counts") { AICF_EQSFinish(); return; }
			AICF_EQSCheck("LIVING_SOURCE", !AICF_EquipmentInventory.Corpse(m_EQSafetyCorpse));
			SCR_CharacterDamageManagerComponent damage = SCR_CharacterDamageManagerComponent.Cast(m_EQSafetyCorpse.FindComponent(SCR_CharacterDamageManagerComponent));
			damage.GetDefaultHitZone().SetHealthScaled(0.5);
			AICF_EQSCheck("WOUNDED_SOURCE", !AICF_EquipmentInventory.Corpse(m_EQSafetyCorpse));
			AICF_EQSBackpack(m_EQSafetyCorpse);
			foreach (AICF_EquipmentMember backpackMember : m_aEQSafetyMembers) AICF_EQSBackpack(backpackMember.m_Character);
			damage.Kill(Instigator.CreateInstigatorGM());
			m_EQSafetyPrimary = AICF_EquipmentInventory.Weapon(m_aEQSafetyMembers[0].m_Character, false).GetOwner().GetID();
			for (int memberIndex; memberIndex < 2; memberIndex++)
			{
				AICF_EquipmentMember actor = m_aEQSafetyMembers[memberIndex];
				AICF_EQSClearLauncher(actor.m_Character);
				AICF_EquipmentOrder order = AICF_EQSOrder(actor);
				m_aEQSafetyOrders.Insert(order);
				AICF_EQSCheck("PHYSICAL_ROUTE_REQUEST", m_OrderPlanner.BeginEquipmentOrder(order));
			}
			m_iEQSafetyPhase = 1;
			return;
		}
		AICF_EquipmentMember receiver = m_aEQSafetyMembers[0];
		AICF_EquipmentOrder active = m_aEQSafetyOrders[0];
		if (m_iEQSafetyPhase == 1)
		{
			foreach (AICF_EquipmentMember approaching : m_aEQSafetyMembers)
				if (!AICF_EquipmentConfig.InRange(approaching.m_Character.GetOrigin(), m_EQSafetyCorpse.GetOrigin(), 3)) return;
			AICF_EQSCheck("TWO_PHYSICAL_RECIPIENTS", true);
			BaseWeaponComponent item = AICF_EQSLauncher(m_EQSafetyCorpse);
			if (!item) { AICF_EQSCheck("SOURCE_LAUNCHER_PRESENT", false); AICF_EQSFinish(); return; }
			m_EQSafetyPendingItem = item.GetOwner();
			m_EQSafetyPendingId = m_EQSafetyPendingItem.GetID();
			string reason;
			bool first = AICF_EquipmentInventory.Transfer(active, receiver, m_EQSafetyCorpse, m_EQSafetyCorpse.GetID(), m_EQSafetyPendingItem, m_EQSafetyPendingId, reason);
			bool second = AICF_EquipmentInventory.Transfer(m_aEQSafetyOrders[1], m_aEQSafetyMembers[1], m_EQSafetyCorpse, m_EQSafetyCorpse.GetID(), m_EQSafetyPendingItem, m_EQSafetyPendingId, reason);
			AICF_EQSCheck("ONE_ITEM_ONE_OWNER", first && !second && AICF_EquipmentInventory.Manager(receiver.m_Character).Contains(m_EQSafetyPendingItem) &&
				!AICF_EquipmentInventory.Manager(m_aEQSafetyMembers[1].m_Character).Contains(m_EQSafetyPendingItem));
			AICF_EQSCheck("WITHDRAWN_ITEM_REJECTED", !AICF_EquipmentInventory.Transfer(active, receiver, m_EQSafetyCorpse, m_EQSafetyCorpse.GetID(), m_EQSafetyPendingItem, m_EQSafetyPendingId, reason));
			AICF_EQSUtility(receiver).m_ThreatSystem.SetThreatValues(1, 1, 0, 0);
			m_iEQSafetyPhase = 2;
			return;
		}
		if (m_iEQSafetyPhase == 2)
		{
			AICF_EQSCheck("COMBAT_BEFORE_START", !m_OrderPlanner.CanEquipmentService(receiver.m_Slot, m_EQSafetyFaction) && !AICF_EquipmentInventory.Quiet(receiver.m_Group));
			m_EQSafetyRecord = new AICF_EquipmentSlot();
			m_EQSafetyRecord.m_Slot = receiver.m_Slot;
			m_EQSafetyRecord.m_Group = receiver.m_Group;
			m_EQSafetyRecord.m_GroupId = receiver.m_GroupId;
			m_EQSafetyRecord.m_iGeneration = receiver.m_iGeneration;
			m_EQSafetyRecord.m_Order = active;
			m_EQSafetyRecord.m_aMembers.Insert(receiver);
			receiver.m_bPending = true;
			m_EQSafetyService.AICF_ProbeTick(m_EQSafetyRecord, now);
			AICF_EQSCheck("COMBAT_CANCELS_SERVICE", !m_EQSafetyRecord.m_Order && !receiver.m_Slot.IsResupplyingEquipment() && receiver.m_bPending && now < m_EQSafetyRecord.m_iQuietUntil);
			AICF_EQSCheck("COMBAT_BEFORE_NEW_START", !m_OrderPlanner.BeginEquipmentOrder(AICF_EQSOrder(receiver)));
			AICF_EQSUtility(receiver).m_ThreatSystem.SetThreatValues(0, 0, 0, 0);
			m_iEQSafetyPhase = 3;
			return;
		}
		if (m_iEQSafetyPhase == 3)
		{
			if (!AICF_EquipmentInventory.Quiet(receiver.m_Group)) return;
			AICF_EQSClearLauncher(receiver.m_Character);
			AICF_EquipmentOrder fresh = AICF_EQSOrder(receiver);
			if (!m_OrderPlanner.BeginEquipmentOrder(fresh)) return;
			m_aEQSafetyOrders[0] = fresh;
			m_iEQSafetyPhase = 4;
			return;
		}
		if (m_iEQSafetyPhase == 4)
		{
			if (!AICF_EquipmentConfig.InRange(receiver.m_Character.GetOrigin(), m_EQSafetyCorpse.GetOrigin(), 3))
			{
				if (now >= m_iEQSafetyAt)
				{
					m_iEQSafetyAt = now + 10000;
					active.m_vPosition = m_EQSafetyCorpse.GetOrigin();
					m_OrderPlanner.BeginEquipmentOrder(active);
				}
				return;
			}
			AICF_EQSFamily(receiver, active);
			return;
		}
		if (m_iEQSafetyPhase == 5)
		{
			string reason;
			EntityID corpseId = m_EQSafetyCorpse.GetID();
			AICF_EQDeplete(receiver.m_Character);
			array<IEntity> sourceItems = {};
			InventoryStorageManagerComponent donor = AICF_EquipmentInventory.Manager(m_EQSafetyCorpse);
			ResourceName primaryAmmo = SCR_ResourceNameUtils.GetPrefabName(AICF_EquipmentInventory.Muzzle(AICF_EquipmentInventory.Weapon(receiver.m_Character, false)).GetMagazine().GetOwner());
			AICF_EQSCheck("ALLEGIANCE_AMMO_PREPARED", donor.TrySpawnPrefabToStorage(primaryAmmo, purpose: EStoragePurpose.PURPOSE_DEPOSIT) && donor.TrySpawnPrefabToStorage(primaryAmmo, purpose: EStoragePurpose.PURPOSE_DEPOSIT));
			donor.GetItems(sourceItems);
			int allegianceCase;
			foreach (IEntity supplied : sourceItems)
			{
				BaseMagazineComponent ammo = BaseMagazineComponent.Cast(supplied.FindComponent(BaseMagazineComponent));
				if (!ammo || ammo.GetAmmoCount() < 1 || !AICF_EquipmentInventory.Compatible(AICF_EquipmentInventory.Muzzle(AICF_EquipmentInventory.Weapon(receiver.m_Character, false)), ammo)) continue;
				Faction faction = m_USFaction;
				string name = "ENEMY_CORPSE_AMMO";
				if (allegianceCase == 1) { faction = GetGame().GetFactionManager().GetFactionByKey("FIA"); name = "FIA_CORPSE_AMMO"; }
				FactionAffiliationComponent.Cast(m_EQSafetyCorpse.FindComponent(FactionAffiliationComponent)).SetAffiliatedFaction(faction);
				int rounds = ammo.GetAmmoCount();
				EntityID sameId = supplied.GetID();
				AICF_EQSCheck(name, faction && AICF_EquipmentInventory.Transfer(active, receiver, m_EQSafetyCorpse, corpseId, supplied, sameId, reason) &&
					!donor.Contains(supplied) && AICF_EquipmentInventory.Manager(receiver.m_Character).Contains(supplied) && supplied.GetID() == sameId && ammo.GetAmmoCount() == rounds);
				allegianceCase++;
				if (allegianceCase == 2) break;
			}
			AICF_EQSCheck("SOURCE_ALLEGIANCES_EXECUTED", allegianceCase == 2);
			AICF_EQSCheck("CORPSE_DELETE_ORDER_CURRENT", active.Current() && receiver.Current());
			RplComponent.DeleteRplEntity(m_EQSafetyCorpse, false);
			AICF_EQSCheck("REMOVED_CORPSE_REFUSED", !AICF_EquipmentInventory.Transfer(active, receiver, m_EQSafetyCorpse, corpseId, m_EQSafetyPendingItem, m_EQSafetyPendingId, reason));
			m_OrderPlanner.AssignPlayerPointOrder(receiver.m_Slot, m_EQSafetyFaction, receiver.m_Character.GetOrigin() + "10 0 0");
			AIWaypoint manual = receiver.m_Slot.GetWaypoint();
			m_OrderPlanner.EndEquipmentOrder(active, m_ObjectiveGraph, m_TargetSelector, true);
			AICF_EQSCheck("NEW_MANUAL_ORDER_PRESERVED", receiver.m_Slot.GetWaypoint() == manual && !active.Current());
			AICF_EQSCheck("STALE_TRANSFER_REFUSED", !AICF_EquipmentInventory.Transfer(active, receiver, m_EQSafetyCorpse, corpseId, m_EQSafetyPendingItem, m_EQSafetyPendingId, reason));
			AICF_EQSFinish();
		}
	}
}
