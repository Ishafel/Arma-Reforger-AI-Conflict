// Реальное состояние оружия; shared magazine не считается дважды с muzzle.
class AICF_EquipmentInventory
{
	static InventoryStorageManagerComponent Manager(IEntity entity)
	{
		if (!entity) return null;
		return InventoryStorageManagerComponent.Cast(entity.FindComponent(InventoryStorageManagerComponent));
	}

	static bool Compatible(BaseMuzzleComponent muzzle, BaseMagazineComponent magazine)
	{
		return muzzle && magazine && muzzle.GetMagazineWell() && magazine.GetMagazineWell() &&
			muzzle.GetMagazineWell().Type() == magazine.GetMagazineWell().Type();
	}

	static int Loaded(BaseMuzzleComponent muzzle)
	{
		if (!muzzle) return 0;
		int rounds = muzzle.GetAmmoCount();
		// Rifle muzzle implementations могут включать патронник в GetAmmoCount.
		// Native magazine + отдельные chambers дают одинаковый счёт для stock/RHS.
		if (muzzle.GetMuzzleType() == EMuzzleType.MT_BaseMuzzle && muzzle.GetMagazine()) rounds = muzzle.GetMagazine().GetAmmoCount();
		for (int i; i < muzzle.GetBarrelsCount(); i++)
			if (muzzle.IsBarrelChambered(i)) rounds++;
		return rounds;
	}

	static BaseMuzzleComponent Muzzle(BaseWeaponComponent weapon)
	{
		if (!weapon) return null;
		if (Primary(weapon))
		{
			// Основной muzzle принадлежит самой rifle entity; выбранный UGL
			// может принадлежать attachment. Не удерживаем native список muzzles.
			BaseMuzzleComponent primaryMuzzle = BaseMuzzleComponent.Cast(weapon.GetOwner().FindComponent(BaseMuzzleComponent));
			if (primaryMuzzle && primaryMuzzle.GetMuzzleType() == EMuzzleType.MT_BaseMuzzle) return primaryMuzzle;
			BaseMuzzleComponent current = weapon.GetCurrentMuzzle();
			if (current && current.GetMuzzleType() == EMuzzleType.MT_BaseMuzzle) return current;
			return null;
		}
		return weapon.GetCurrentMuzzle();
	}

	static int Rounds(IEntity character, BaseWeaponComponent weapon)
	{
		InventoryStorageManagerComponent manager = Manager(character);
		if (!manager || !weapon) return 0;
		BaseMuzzleComponent muzzle = Muzzle(weapon);
		if (!muzzle) return 0;
		int rounds = Loaded(muzzle);
		array<IEntity> items = {};
		manager.GetItems(items);
		foreach (IEntity item : items)
		{
			if (!item) continue;
			if (muzzle.IsDisposable())
			{
				BaseWeaponComponent spare = BaseWeaponComponent.Cast(item.FindComponent(BaseWeaponComponent));
				if (spare && spare != weapon && SCR_ResourceNameUtils.GetPrefabName(item) == SCR_ResourceNameUtils.GetPrefabName(weapon.GetOwner()))
					rounds += Loaded(spare.GetCurrentMuzzle());
				continue;
			}
			BaseMagazineComponent magazine = BaseMagazineComponent.Cast(item.FindComponent(BaseMagazineComponent));
			if (magazine && magazine != muzzle.GetMagazine() && Compatible(muzzle, magazine))
				rounds += magazine.GetAmmoCount();
		}
		return rounds;
	}

	static bool Primary(BaseWeaponComponent weapon)
	{
		if (!weapon) return false;
		EWeaponType type = weapon.GetWeaponType();
		return type == EWeaponType.WT_RIFLE || type == EWeaponType.WT_MACHINEGUN || type == EWeaponType.WT_SNIPERRIFLE;
	}

	static BaseWeaponComponent Weapon(IEntity character, bool launcher)
	{
		BaseWeaponManagerComponent manager = BaseWeaponManagerComponent.Cast(character.FindComponent(BaseWeaponManagerComponent));
		if (!manager) return null;
		array<WeaponSlotComponent> slots = {};
		manager.GetWeaponsSlots(slots);
		foreach (WeaponSlotComponent slot : slots)
		{
			if (!slot.GetWeaponEntity() || slot.GetWeaponSlotIndex() >= 3) continue;
			BaseWeaponComponent weapon = BaseWeaponComponent.Cast(slot.GetWeaponEntity().FindComponent(BaseWeaponComponent));
			if (!weapon) continue;
			if (launcher && weapon.GetWeaponType() == EWeaponType.WT_ROCKETLAUNCHER) return weapon;
			if (!launcher && Primary(weapon)) return weapon;
		}
		return null;
	}

	static bool Corpse(IEntity entity)
	{
		ChimeraCharacter character = ChimeraCharacter.Cast(entity);
		if (!character || !character.GetCharacterController() ||
			character.GetCharacterController().GetLifeState() != ECharacterLifeState.DEAD ||
			character.GetCharacterController().IsPlayerControlled() ||
			GetGame().GetPlayerManager().GetPlayerIdFromControlledEntity(character) > 0 ||
			SCR_PossessingManagerComponent.GetPlayerIdFromMainEntity(character) > 0) return false;
		RplComponent rpl = RplComponent.Cast(character.FindComponent(RplComponent));
		return rpl && rpl.IsMaster() && Manager(character);
	}

	static bool SafeMember(IEntity entity, SCR_AIGroup group)
	{
		if (!Replication.IsServer() || !group || !AICF_GroupRuntime.IsAliveCharacter(entity)) return false;
		ChimeraCharacter character = ChimeraCharacter.Cast(entity);
		CharacterControllerComponent controller = character.GetCharacterController();
		AIControlComponent control = character.GetAIControlComponent();
		RplComponent rpl = RplComponent.Cast(character.FindComponent(RplComponent));
		CompartmentAccessComponent access = character.GetCompartmentAccessComponent();
		return controller && !controller.IsPlayerControlled() && rpl && rpl.IsMaster() &&
			GetGame().GetPlayerManager().GetPlayerIdFromControlledEntity(entity) <= 0 &&
			SCR_PossessingManagerComponent.GetPlayerIdFromMainEntity(entity) <= 0 &&
			control && control.GetControlAIAgent() && control.GetControlAIAgent().GetParentGroup() == group &&
			access && !access.IsInCompartment() && !access.IsGettingIn() && !access.IsGettingOut();
	}

	static bool Quiet(SCR_AIGroup group)
	{
		if (!group) return false;
		array<AIAgent> agents = {};
		group.GetAgents(agents);
		int alive;
		foreach (AIAgent agent : agents)
		{
			if (!agent || !AICF_GroupRuntime.IsAliveCharacter(agent.GetControlledEntity())) continue;
			alive++;
			if (!SafeMember(agent.GetControlledEntity(), group)) return false;
			SCR_AIUtilityComponent utility = SCR_AIUtilityComponent.Cast(agent.FindComponent(SCR_AIUtilityComponent));
			if (!utility || !utility.m_ThreatSystem || utility.m_ThreatSystem.GetState() != EAIThreatState.SAFE ||
				utility.m_ThreatSystem.GetThreatMeasure() > 0.01) return false;
		}
		return alive > 0;
	}

	// Одна native операция переносит ту же entity. Нет remove+spawn или RPC-копии.
	static bool Transfer(AICF_EquipmentOrder order, AICF_EquipmentMember member, IEntity corpse, EntityID corpseId,
		IEntity item, EntityID itemId, out string reason)
	{
		reason = "TRANSFER_FENCE";
		if (!order || !order.Current() || !member || !member.Current() || !Corpse(corpse) || corpse.GetID() != corpseId ||
			!item || item.GetID() != itemId || !Quiet(order.m_Group) ||
			!AICF_EquipmentConfig.InRange(member.m_Character.GetOrigin(), corpse.GetOrigin(), AICF_EquipmentConfig.TRANSFER_METERS)) return false;
		InventoryStorageManagerComponent source = Manager(corpse);
		InventoryStorageManagerComponent target = Manager(member.m_Character);
		if (!source || !target || !source.Contains(item) || !member.Wants(item)) return false;
		InventoryItemComponent component = InventoryItemComponent.Cast(item.FindComponent(InventoryItemComponent));
		if (!component) return false;
		InventoryStorageSlot parent = component.GetParentSlot();
		if (!parent || parent.GetAttachedEntity() != item) return false;
		EStoragePurpose purpose = EStoragePurpose.PURPOSE_DEPOSIT;
		BaseWeaponComponent carriedWeapon = BaseWeaponComponent.Cast(item.FindComponent(BaseWeaponComponent));
		if (carriedWeapon && carriedWeapon.GetWeaponType() == EWeaponType.WT_ROCKETLAUNCHER) purpose = EStoragePurpose.PURPOSE_WEAPON_PROXY;
		BaseInventoryStorageComponent storage = target.FindStorageForItem(item, purpose);
		reason = "CAPACITY_OR_SLOT";
		if (!storage || !target.CanMoveItemToStorage(item, storage) || !source.CanRemoveItemFromStorage(item, parent.GetStorage())) return false;
		// Повтор после поиска storage: инвентарные hooks могут вызвать синхронные callbacks.
		if (!order.Current() || !member.Current() || !Corpse(corpse) || !source.Contains(item) ||
			component.GetParentSlot() != parent || !member.Wants(item)) return false;
		reason = "NATIVE_TRANSFER_FAILED";
		if (!target.TryMoveItemToStorage(item, storage)) return false;
		reason = "TRANSFER_READBACK";
		return item && item.GetID() == itemId && target.Contains(item) && !source.Contains(item);
	}
}
