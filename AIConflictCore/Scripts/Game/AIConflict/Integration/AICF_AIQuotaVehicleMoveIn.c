// Script convenience/RPC seam. Human/private-world сохраняют native путь.
// Допуск AI связан с exact actor/slot/Rpl: старый RPC с иной identity
// не может использовать разрешение нового запроса.
modded class SCR_CompartmentAccessComponent
{
	protected ref AICF_AIQuotaVehicleDispatch m_AICFMoveInQuota;
	protected string m_sAICFMoveInOwnerRpl;
	protected int m_iAICFMoveInSlot;

	override bool MoveInVehicle(IEntity vehicle, ECompartmentType compartmentType, bool performWhenPaused = false, BaseCompartmentSlot customSlot = null)
	{
		IEntity actor = GetOwner();
		if (!AICF_AIQuotaVehicleDispatch.FindPolicy(actor)) return super.MoveInVehicle(vehicle, compartmentType, performWhenPaused, customSlot);
		if (!vehicle) return false;
		BaseCompartmentSlot seat = FindFreeAndAccessibleCompartment(vehicle, compartmentType, customSlot);
		if (!seat) return false;
		IEntity seatOwner = seat.GetOwner();
		RplComponent rpl;
		if (seatOwner) rpl = RplComponent.Cast(seatOwner.FindComponent(RplComponent));
		if (!rpl || !rpl.Id().IsValid()) return false;
		AICF_AIQuotaVehicleDispatch quota = AICF_AIQuotaVehicleDispatch.Begin(actor, seatOwner, seat);
		if (!quota) return false;
		if (!quota.Matches(actor, seatOwner, seat) || GetOwner() != actor || seat.GetOwner() != seatOwner)
		{
			quota.Cancel();
			return false;
		}
		if (m_AICFMoveInQuota) m_AICFMoveInQuota.Cancel();
		GetGame().GetCallqueue().Remove(AICF_RetryQuotaMoveInOwner);
		m_AICFMoveInQuota = quota;
		m_sAICFMoveInOwnerRpl = rpl.Id().ToString();
		m_iAICFMoveInSlot = seat.GetCompartmentSlotID();
		// Тот же выбранный slot сохраняет native validation/locking без второго
		// неконтролируемого выбора места после admission.
		bool issued = super.MoveInVehicle(vehicle, compartmentType, performWhenPaused, seat);
		if (issued) quota.MarkDispatched();
		else
		{
			quota.Cancel();
			if (m_AICFMoveInQuota == quota) m_AICFMoveInQuota = null;
		}
		return issued;
	}

	override protected void MoveInVehicleOwner(RplId slotEntityID, int slotID, bool performWhenPaused)
	{
		IEntity actor = GetOwner();
		AICF_AIQuotaVehicleDispatch quota = m_AICFMoveInQuota;
		if (!quota && !AICF_AIQuotaVehicleDispatch.FindPolicy(actor))
		{
			super.MoveInVehicleOwner(slotEntityID, slotID, performWhenPaused);
			return;
		}
		if (quota && (slotEntityID.ToString() != m_sAICFMoveInOwnerRpl || slotID != m_iAICFMoveInSlot)) return;
		if (quota && !quota.IsCurrent())
		{
			quota.Cancel();
			if (m_AICFMoveInQuota == quota) m_AICFMoveInQuota = null;
			return;
		}
		RplComponent rpl = RplComponent.Cast(Replication.FindItem(slotEntityID));
		if (!rpl)
		{
			// Тот же pinned global limit10 и cadence1000; повтор всегда проходит
			// наш exact request guard, без обхода через base function pointer.
			if (!quota) return;
			if (s_iWaitForVehicleTries >= WAIT_FOR_VEHICLE_MAX_TRIES)
			{
				s_iWaitForVehicleTries = 0;
				quota.Cancel();
				if (m_AICFMoveInQuota == quota) m_AICFMoveInQuota = null;
				return;
			}
			GetGame().GetCallqueue().CallLater(AICF_RetryQuotaMoveInOwner, 1000, false, slotEntityID, slotID, performWhenPaused);
			s_iWaitForVehicleTries++;
			return;
		}
		IEntity seatOwner = rpl.GetEntity();
		BaseCompartmentManagerComponent manager;
		if (seatOwner) manager = BaseCompartmentManagerComponent.Cast(seatOwner.FindComponent(BaseCompartmentManagerComponent));
		BaseCompartmentSlot seat;
		if (manager) seat = manager.FindCompartment(slotID);
		if (!quota) quota = AICF_AIQuotaVehicleDispatch.Begin(actor, seatOwner, seat);
		if (!quota) return;
		if (!quota.Matches(actor, seatOwner, seat) || GetOwner() != actor || !seat || seat.GetOwner() != seatOwner ||
			seat.GetCompartmentSlotID() != slotID || !rpl.Id().IsValid() || rpl.Id() != slotEntityID)
		{
			quota.Cancel();
			if (m_AICFMoveInQuota == quota) m_AICFMoveInQuota = null;
			return;
		}
		// Manager/Rpl getters выше могут вызвать cleanup. Повторяем admission
		// после последнего такого чтения, непосредственно перед native GetIn.
		if (!quota.IsCurrent() || GetOwner() != actor || !quota.IsCurrent())
		{
			quota.Cancel();
			if (m_AICFMoveInQuota == quota) m_AICFMoveInQuota = null;
			return;
		}
		s_iWaitForVehicleTries = 0;
		bool accepted = GetInVehicle(seatOwner, seat, true, -1, ECloseDoorAfterActions.INVALID, performWhenPaused);
		quota.ObserveNativeResult(accepted);
		quota.Cancel();
		if (m_AICFMoveInQuota == quota) m_AICFMoveInQuota = null;
	}

	protected void AICF_RetryQuotaMoveInOwner(RplId slotEntityID, int slotID, bool performWhenPaused)
	{
		MoveInVehicleOwner(slotEntityID, slotID, performWhenPaused);
	}

	override protected void OnDelete(IEntity owner)
	{
		if (GetGame())
		{
			GetGame().GetCallqueue().Remove(AICF_RetryQuotaMoveInOwner);
			GetGame().GetCallqueue().Remove(MoveInVehicleOwner);
		}
		if (m_AICFMoveInQuota) m_AICFMoveInQuota.Cancel();
		m_AICFMoveInQuota = null;
		super.OnDelete(owner);
	}
}
