class AICF_EquipmentParkedItem
{
	IEntity m_Item;
	EntityID m_Id;
	BaseInventoryStorageComponent m_Storage;
	int m_iSlot;
	bool Detached()
	{
		if (!m_Item || m_Item.GetID() != m_Id) return false;
		InventoryItemComponent component = InventoryItemComponent.Cast(m_Item.FindComponent(InventoryItemComponent));
		return component && !component.GetParentSlot();
	}
}

// Callback только фиксирует receipt; inventory, slot и приказ он не меняет.
class AICF_EquipmentReceipt : ScriptedInventoryOperationCallback
{
	bool m_bDone;
	bool m_bFailed;
	override protected void OnComplete() { m_bDone = true; }
	override protected void OnFailed() { m_bDone = true; m_bFailed = true; }
}

class AICF_EquipmentTransaction
{
	int m_iToken;
	int m_iRevision;
	int m_iPhase;
	int m_iIndex;
	int m_iDeadline;
	int m_iRollbackPass;
	int m_iRecoveryDeadline;
	bool m_bCancelled;
	bool m_bFinished;
	bool m_bRestored;
	bool m_bOwnsMovementLock;
	IEntity m_HandItem;
	EntityID m_HandId;
	vector m_vParkPosition;
	ref AICF_EquipmentReceipt m_Receipt;
	ref array<ref AICF_EquipmentParkedItem> m_aOriginal = {};
	ref array<ref AICF_EquipmentParkedItem> m_aExtra = {};
}

// Native tasks завершаются после возврата Try*. Ожидаем receipt и physical
// readback; исходные entities с фактическими патронами сохраняются до commit.
class AICF_EquipmentApplicator
{
	protected void Roots(IEntity character, array<ref AICF_EquipmentParkedItem> result)
	{
		set<BaseInventoryStorageComponent> storages = new set<BaseInventoryStorageComponent>();
		SCR_PlayerArsenalLoadout.FindStorageComponents(character, storages);
		foreach (BaseInventoryStorageComponent storage : storages)
		{
			if (!AICF_LoadoutInventory.Editable(storage)) continue;
			array<InventoryItemComponent> items = {};
			storage.GetOwnedItems(items, false);
			foreach (InventoryItemComponent item : items)
			{
				if (!item || !item.GetParentSlot()) continue;
				// Compartment storages тоже могут попасть в GetOwnedItems(false).
				// Их содержимое сохраняется внутри корневой одежды, отдельно не снимается.
				if (item.GetParentSlot().GetStorage().GetOwner() != character) continue;
				bool duplicate;
				foreach (AICF_EquipmentParkedItem existing : result) if (existing.m_Item == item.GetOwner()) duplicate = true;
				if (duplicate) continue;
				AICF_EquipmentParkedItem saved = new AICF_EquipmentParkedItem();
				saved.m_Item = item.GetOwner();
				saved.m_Id = saved.m_Item.GetID();
				saved.m_Storage = item.GetParentSlot().GetStorage();
				saved.m_iSlot = item.GetParentSlot().GetID();
				result.Insert(saved);
			}
		}
	}

	protected bool Actor(AICF_EquipmentMember member)
	{
		return member && member.m_Character && member.m_Character.GetID() == member.m_CharacterId &&
			member.m_Slot && member.m_Slot.GetGroup() == member.m_Group && member.m_Slot.GetSpawnGeneration() == member.m_iGeneration &&
			member.m_Group && member.m_Group.GetID() == member.m_GroupId && SCR_Faction.GetEntityFaction(member.m_Character) == member.m_Faction &&
			AICF_EquipmentInventory.SafeMember(member.m_Character, member.m_Group);
	}

	protected bool RestoreNew(AICF_EquipmentMember member)
	{
		return AICF_LoadoutInventory.Restore(member.m_Character, member.m_Plan.m_sInventory);
	}

	void Cancel(AICF_EquipmentMember member)
	{
		if (member && member.m_Transaction) member.m_Transaction.m_bCancelled = true;
	}

	void Release(AICF_EquipmentMember member)
	{
		if (!member || !member.m_Transaction || !member.m_Transaction.m_bOwnsMovementLock) return;
		AICF_EquipmentTransaction tx = member.m_Transaction;
		tx.m_bOwnsMovementLock = false;
		if (!member.m_Character || member.m_Character.GetID() != member.m_CharacterId) return;
		CharacterControllerComponent controller = CharacterControllerComponent.Cast(member.m_Character.FindComponent(CharacterControllerComponent));
		// GiveExt при player takeover передаёт Rpl ownership. Cleanup собственного
		// movement flag всё равно выполняет server для той же immutable entity.
		if (controller && Replication.IsServer()) controller.SetDisableMovementControls(false);
	}

	// Продолжается после освобождения visit; прежний приказ не возвращается.
	bool Recover(AICF_EquipmentMember member)
	{
		AICF_EquipmentTransaction tx = member.m_Transaction;
		if (!tx || tx.m_bFinished) return true;
		if (!Actor(member)) { Release(member); tx.m_bFinished = true; return true; }
		if (!tx.m_iRecoveryDeadline) tx.m_iRecoveryDeadline = System.GetTickCount() + 60000;
		if (System.GetTickCount() >= tx.m_iRecoveryDeadline)
		{
			Print(string.Format("[AICF][EQUIPMENT_RECOVERY_TIMEOUT] member=%1 generation=%2 revision=%3 reason=ORIGINAL_ENTITIES_RETAINED", member.m_CharacterId, member.m_iGeneration, tx.m_iRevision), LogLevel.WARNING);
			Release(member);
			tx.m_bFinished = true;
			return true;
		}
		if (tx.m_Receipt && !tx.m_Receipt.m_bDone) return false;
		tx.m_Receipt = null;
		InventoryStorageManagerComponent manager = AICF_EquipmentInventory.Manager(member.m_Character);
		if (tx.m_iPhase < 4)
		{
			tx.m_iPhase = 4;
			tx.m_iIndex = 0;
			array<ref AICF_EquipmentParkedItem> current = {};
			Roots(member.m_Character, current);
			foreach (AICF_EquipmentParkedItem item : current)
			{
				bool original;
				foreach (AICF_EquipmentParkedItem saved : tx.m_aOriginal) if (saved.m_Item == item.m_Item && saved.m_Id == item.m_Id) original = true;
				if (!original) tx.m_aExtra.Insert(item);
			}
		}
		if (tx.m_iPhase == 4)
		{
			if (tx.m_iIndex < tx.m_aExtra.Count())
			{
				AICF_EquipmentParkedItem extra = tx.m_aExtra[tx.m_iIndex++];
				if (extra.m_Item && extra.m_Item.GetID() == extra.m_Id && manager.Contains(extra.m_Item))
				{
					tx.m_Receipt = new AICF_EquipmentReceipt();
					if (!manager.TryDeleteItem(extra.m_Item, tx.m_Receipt)) tx.m_Receipt = null;
				}
				return false;
			}
			tx.m_iPhase = 5;
			tx.m_iIndex = 0;
		}
		if (tx.m_iIndex < tx.m_aOriginal.Count())
		{
			AICF_EquipmentParkedItem originalItem = tx.m_aOriginal[tx.m_iIndex++];
			if (originalItem.Detached() && originalItem.m_Storage)
			{
				int targetSlot = originalItem.m_iSlot;
				if (!manager.CanInsertItemInStorage(originalItem.m_Item, originalItem.m_Storage, targetSlot) &&
					originalItem.m_Storage.GetPurpose() == EStoragePurpose.PURPOSE_DEPOSIT) targetSlot = -1;
				tx.m_Receipt = new AICF_EquipmentReceipt();
				if (!manager.CanInsertItemInStorage(originalItem.m_Item, originalItem.m_Storage, targetSlot) ||
					!manager.TryInsertItemInStorage(originalItem.m_Item, originalItem.m_Storage, targetSlot, tx.m_Receipt))
				{
					tx.m_Receipt = null;
					Print(string.Format("[AICF][EQUIPMENT_ROLLBACK_ITEM] member=%1 item=%2 prefab=%3 slot=%4 reason=NATIVE_INSERT_REFUSED",
						member.m_CharacterId, originalItem.m_Id, SCR_ResourceNameUtils.GetPrefabName(originalItem.m_Item), targetSlot), LogLevel.WARNING);
				}
			}
			return false;
		}
		tx.m_bRestored = true;
		foreach (AICF_EquipmentParkedItem verify : tx.m_aOriginal)
		{
			if (verify.m_Item && verify.m_Item.GetID() == verify.m_Id && manager.Contains(verify.m_Item)) continue;
			tx.m_bRestored = false;
			IEntity parentOwner;
			InventoryItemComponent actualItem;
			if (verify.m_Item) actualItem = InventoryItemComponent.Cast(verify.m_Item.FindComponent(InventoryItemComponent));
			if (actualItem && actualItem.GetParentSlot()) parentOwner = actualItem.GetParentSlot().GetStorage().GetOwner();
			Print(string.Format("[AICF][EQUIPMENT_ROLLBACK_ITEM] member=%1 item=%2 slot=%3 exists=%4 parent=%5 prefab=%6 reason=READBACK_NOT_OWNED",
				member.m_CharacterId, verify.m_Id, verify.m_iSlot, verify.m_Item != null, parentOwner, SCR_ResourceNameUtils.GetPrefabName(verify.m_Item)) +
				string.Format(" member_position=%1 item_position=%2", member.m_Character.GetOrigin(), tx.m_vParkPosition), LogLevel.WARNING);
		}
		// Несколько clothing areas зависят от уже возвращённых соседей. Повтор
		// после полного прохода ограничен; предметы не создаются заново.
		if (!tx.m_bRestored && tx.m_iRollbackPass < 2)
		{
			tx.m_iRollbackPass++;
			tx.m_iIndex = 0;
			return false;
		}
		tx.m_bFinished = true;
		Release(member);
		CharacterControllerComponent controller = CharacterControllerComponent.Cast(member.m_Character.FindComponent(CharacterControllerComponent));
		if (tx.m_bRestored && tx.m_HandItem && tx.m_HandItem.GetID() == tx.m_HandId && manager.Contains(tx.m_HandItem))
			controller.TryEquipRightHandItem(tx.m_HandItem, EEquipItemType.EEquipTypeWeapon, true);
		return true;
	}

	bool Apply(AICF_EquipmentOrder order, AICF_EquipmentMember member, out string reason)
	{
		reason = "ISSUE_FENCE";
		if (!order || !order.Current() || !member || !member.Current() || !order.CanIssue(member) || !AICF_EquipmentInventory.Quiet(order.m_Group) ||
			!AICF_EquipmentConfig.InRange(member.m_Character.GetOrigin(), order.m_vPosition, AICF_EquipmentConfig.SERVICE_METERS)) return false;
		CharacterControllerComponent controller = CharacterControllerComponent.Cast(member.m_Character.FindComponent(CharacterControllerComponent));
		if (!controller || controller.IsUsingItem() || controller.IsWeaponADS()) return false;
		AICF_EquipmentTransaction tx = member.m_Transaction;
		if (!tx)
		{
			if (controller.IsChangingItem() || controller.GetDisableMovementControls()) return false;
			tx = new AICF_EquipmentTransaction();
			tx.m_iToken = order.m_iToken;
			tx.m_iRevision = member.m_iRevision;
			tx.m_iDeadline = System.GetTickCount() + 30000;
			tx.m_vParkPosition = member.m_Character.GetOrigin();
			tx.m_HandItem = controller.GetRightHandItem();
			if (tx.m_HandItem) tx.m_HandId = tx.m_HandItem.GetID();
			Roots(member.m_Character, tx.m_aOriginal);
			// Сначала снимаем предмет из рук, пока AI ещё не успел выбрать его снова.
			for (int rootIndex; rootIndex < tx.m_aOriginal.Count(); rootIndex++)
			{
				if (tx.m_aOriginal[rootIndex].m_Item != tx.m_HandItem) continue;
				AICF_EquipmentParkedItem hand = tx.m_aOriginal[rootIndex];
				tx.m_aOriginal.Remove(rootIndex);
				tx.m_aOriginal.InsertAt(hand, 0);
				break;
			}
			member.m_Transaction = tx;
			member.m_Slot.SetEquipmentInventoryTransaction(tx);
			controller.SetDisableMovementControls(true);
			tx.m_bOwnsMovementLock = true;
		}
		if (tx.m_iToken != order.m_iToken || tx.m_iRevision != member.m_iRevision || System.GetTickCount() >= tx.m_iDeadline) tx.m_bCancelled = true;
		if (tx.m_bCancelled)
		{
			if (!Recover(member)) { reason = "ISSUE_RECOVERING"; return false; }
			reason = "ISSUE_FAILED_ORIGINAL_RETAINED_AT_FEET";
			if (tx.m_bRestored) reason = "ISSUE_FAILED_ORIGINAL_RESTORED";
			order.Log("EQUIPMENT_ISSUE_REFUSED", reason, member);
			member.m_Slot.ClearEquipmentInventoryTransaction(tx);
			member.m_Transaction = null;
			return false;
		}
		reason = "ISSUE_PENDING";
		if (tx.m_Receipt)
		{
			if (!tx.m_Receipt.m_bDone) return false;
			if (tx.m_Receipt.m_bFailed) tx.m_bCancelled = true;
			tx.m_Receipt = null;
			if (tx.m_bCancelled) return false;
		}
		InventoryStorageManagerComponent manager = AICF_EquipmentInventory.Manager(member.m_Character);
		if (tx.m_iPhase == 0)
		{
			if (controller.GetRightHandItem())
			{
				controller.TryEquipRightHandItem(null, EEquipItemType.EEquipTypeUnarmedDeliberate, true);
				if (controller.GetRightHandItem()) return false;
			}
			if (controller.IsChangingItem()) return false;
			tx.m_iPhase = 1;
		}
		if (tx.m_iPhase == 1)
		{
			if (tx.m_iIndex > 0 && !tx.m_aOriginal[tx.m_iIndex - 1].Detached()) { tx.m_bCancelled = true; return false; }
			if (tx.m_iIndex > 0)
			{
				// У wearable entity origin может остаться от места рождения персонажа.
				// После подтверждённого detach кладём его у фактического carrier.
				AICF_EquipmentParkedItem parked = tx.m_aOriginal[tx.m_iIndex - 1];
				if (parked.m_Item && parked.m_Item.GetID() == parked.m_Id && parked.Detached()) parked.m_Item.SetOrigin(tx.m_vParkPosition);
			}
			if (tx.m_iIndex < tx.m_aOriginal.Count())
			{
				AICF_EquipmentParkedItem item = tx.m_aOriginal[tx.m_iIndex++];
				tx.m_Receipt = new AICF_EquipmentReceipt();
				if (!item.m_Item || item.m_Item.GetID() != item.m_Id || item.m_Storage.Get(item.m_iSlot) != item.m_Item ||
					!manager.CanRemoveItemFromStorage(item.m_Item, item.m_Storage) || !manager.TryRemoveItemFromStorage(item.m_Item, item.m_Storage, tx.m_Receipt))
				{
					tx.m_Receipt = null;
					tx.m_bCancelled = true;
				}
				return false;
			}
			tx.m_iPhase = 2;
			if (!RestoreNew(member)) tx.m_bCancelled = true;
			return false;
		}
		int ignored;
		AICF_LoadoutCatalog catalog = new AICF_LoadoutCatalog(member.m_Faction);
		if (AICF_LoadoutInventory.Signature(member.m_Character, catalog, ignored) != member.m_Plan.m_sSignature) return false;
		if (!member.m_Slot.MarkMemberLoadoutApplied(member.m_iIndex, member.m_Character, member.m_iRevision)) { tx.m_bCancelled = true; return false; }
		foreach (AICF_EquipmentParkedItem discarded : tx.m_aOriginal)
			if (discarded.Detached()) RplComponent.DeleteRplEntity(discarded.m_Item, false);
		BaseWeaponComponent primary = AICF_EquipmentInventory.Weapon(member.m_Character, false);
		if (primary) controller.TryEquipRightHandItem(primary.GetOwner(), EEquipItemType.EEquipTypeWeapon, true);
		tx.m_bFinished = true;
		Release(member);
		member.m_Transaction = null;
		reason = "ISSUED";
		member.m_Slot.ClearEquipmentInventoryTransaction(tx);
		order.Log("EQUIPMENT_ISSUED", reason, member);
		return true;
	}
}
