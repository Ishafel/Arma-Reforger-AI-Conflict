// Вместе с AICF_FIARandomGarrisonProbe, только в изолированном Core stage.
class AICF_MagazineGuardProbe : SCR_AISwitchMagazine
{
	bool Bind(ChimeraCharacter member)
	{
		m_CompartmentAccessComp = member.GetCompartmentAccessComponent();
		m_WeaponMgrComp = BaseWeaponManagerComponent.Cast(member.FindComponent(BaseWeaponManagerComponent));
		m_ControlComp = CharacterControllerComponent.Cast(member.FindComponent(CharacterControllerComponent));
		m_InventoryMgr = SCR_InventoryStorageManagerComponent.Cast(member.FindComponent(SCR_InventoryStorageManagerComponent));
		return m_CompartmentAccessComp && m_WeaponMgrComp && m_ControlComp && m_InventoryMgr;
	}
}

modded class AICF_FIAGarrisonService
{
	override protected void RandomFinish()
	{
		AICF_MagazineGuardProbe task = new AICF_MagazineGuardProbe();
		RandomCheck("MAGAZINE_NO_ACCESS", task.EOnTaskSimulate(null, 0) == ENodeResult.FAIL);
		int checked;
		bool valid = true;
		foreach (AICF_FIAGarrison g : s_aDefenders)
		{
			if (!g.m_bReady) continue;
			foreach (ChimeraCharacter member : g.m_aCrew)
			{
				BaseCompartmentSlot seat = member.GetCompartmentAccessComponent().GetCompartment();
				if (!seat || !seat.GetOwner() || seat.GetOwner().FindComponent(BaseWeaponManagerComponent)) continue;
				checked++;
				if (!task.Bind(member) || task.EOnTaskSimulate(null, 0) != ENodeResult.FAIL) valid = false;
			}
		}
		RandomCheck("MAGAZINE_INVALID_SEATS", valid);
		Print(string.Format("[AICF][MAGAZINE_GUARD_PROBE] invalid_seats=%1 passed=%2", checked, valid));
		super.RandomFinish();
	}
}
