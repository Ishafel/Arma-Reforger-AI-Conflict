// Те же нашивки входят в baseline куклы, server validation и реальную выдачу.
class AICF_WCSPatches
{
	static bool ApplyLocal(IEntity model, FactionKey faction, ResourceName source)
	{
		if (!model || model.GetWorld() == GetGame().GetWorld()) return false;
		InventoryStorageManagerComponent manager = InventoryStorageManagerComponent.Cast(model.FindComponent(InventoryStorageManagerComponent));
		if (!manager) return false;
		array<IEntity> items = {};
		AICF_RHSPMCArmament.Items(model, items);
		foreach (IEntity item : items)
		{
			set<BaseInventoryStorageComponent> storages = new set<BaseInventoryStorageComponent>();
			SCR_PlayerArsenalLoadout.FindStorageComponents(item, storages);
			foreach (BaseInventoryStorageComponent storage : storages)
			{
				for (int index; index < storage.GetSlotsCount(); index++)
				{
					InventoryStorageSlot slot = storage.GetSlot(index);
					if (!AICF_RHSDefaultPatches.IsPatchSlot(slot) || slot.IsLocked() || slot.GetAttachedEntity()) continue;
					ResourceName patch = AICF_RHSDefaultPatches.Preferred(faction, source, slot.GetSourceName(), AICF_RHSPMCEquipment.Variant(source));
					if (!manager.CanInsertResourceInStorage(patch, storage, index)) patch = AICF_RHSDefaultPatches.Flag(faction);
					if (!manager.CanInsertResourceInStorage(patch, storage, index)) continue;
					if (!AICF_LoadoutInventory.InsertLocal(patch, storage, index, manager)) return false;
				}
			}
		}
		return true;
	}
}
