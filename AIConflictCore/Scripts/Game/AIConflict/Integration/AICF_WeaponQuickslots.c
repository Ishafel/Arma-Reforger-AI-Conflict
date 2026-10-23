// Native InitAsPlayer дедуплицирует одинаковый default quickslot двух винтовок.
// Дополняем только отсутствующие привязки по настоящему weapon slot index.
modded class SCR_CharacterInventoryStorageComponent
{
	override void InitAsPlayer(IEntity pOwner, bool pControlled)
	{
		super.InitAsPlayer(pOwner, pControlled);
		if (!pControlled || !pOwner || TurretCompartmentSlot.Cast(GetCurrentCompartment())) return;
		BaseWeaponManagerComponent manager = BaseWeaponManagerComponent.Cast(pOwner.FindComponent(BaseWeaponManagerComponent));
		if (!manager) return;
		array<WeaponSlotComponent> slots = {};
		manager.GetWeaponsSlots(slots);
		foreach (WeaponSlotComponent slot : slots)
		{
			int index = slot.GetWeaponSlotIndex();
			if (index < 0 || index >= SCR_InventoryMenuUI.WEAPON_SLOTS_COUNT) continue;
			IEntity weapon = slot.GetWeaponEntity();
			InventoryStorageSlot info = slot.GetSlotInfo();
			if (!weapon || !info || info.GetAttachedEntity() != weapon || GetEntityIndexInQuickslots(weapon) >= 0 || GetItemFromQuickSlot(index)) continue;
			StoreItemToQuickSlot(weapon, index);
		}
	}
}
