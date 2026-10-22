// RHS ставит повторный visual refresh в global game queue. Private inventory
// собирается и удаляется синхронно; такой callback переживает его native slots.
// Сохраняем немедленный refresh и штатное поведение в мире кампании.
modded class RHS_EquipmentStorageComponent
{
	override protected void RequestAttachedItemVisualsRefresh(int slotID = -1)
	{
		IEntity owner = GetOwner();
		if (owner && owner.GetWorld() != GetGame().GetWorld())
		{
			GetGame().GetCallqueue().Remove(RefreshAttachedItemVisuals);
			RefreshAttachedItemVisuals(slotID);
			return;
		}
		super.RequestAttachedItemVisualsRefresh(slotID);
	}
}
