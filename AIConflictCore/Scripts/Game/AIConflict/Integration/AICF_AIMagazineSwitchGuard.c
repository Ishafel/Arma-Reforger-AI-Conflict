// Stock 1.8 выбирает weapon manager владельца compartment без null guard.
// У пассажирских мест manager отсутствует: смена магазина здесь недопустима.
modded class SCR_AISwitchMagazine
{
	override ENodeResult EOnTaskSimulate(AIAgent owner, float dt)
	{
		if (!m_CompartmentAccessComp) return ENodeResult.FAIL;
		BaseCompartmentSlot seat = m_CompartmentAccessComp.GetCompartment();
		if (seat)
		{
			IEntity seatOwner = seat.GetOwner();
			if (!seatOwner || !seatOwner.FindComponent(BaseWeaponManagerComponent)) return ENodeResult.FAIL;
		}
		return super.EOnTaskSimulate(owner, dt);
	}
}
