modded class AICF_EconomySystem
{
	bool QuoteInfantryRecruit(AICF_InfantryRecruitmentOrder order)
	{
		return Replication.IsServer() && m_Campaign && m_Campaign.IsMaster() && m_Campaign.IsRunning() &&
			m_bGraphContextReady && order && order.IsCurrent(order.m_Slot) && order.HasSafeBarracks() &&
			order.m_iCost > 0 && order.m_fPaid == 0 && order.m_Base.GetSupplies() >= order.m_iCost;
	}

	// Цена снимается только после materialisation, непосредственно перед передачей бойца.
	bool DebitInfantryRecruit(AICF_InfantryRecruitmentOrder order)
	{
		if (!QuoteInfantryRecruit(order) || !order.IsPhysicallyPresent())
			return false;
		float before = order.m_Base.GetSupplies();
		order.m_Base.AddSupplies(-order.m_iCost);
		float after = order.m_Base.GetSupplies();
		order.m_fPaid = Math.Max(0, before - after);
		if (Math.AbsFloat(order.m_fPaid - order.m_iCost) > 0.01)
		{
			RefundInfantryRecruit(order);
			return false;
		}
		order.Log("INFANTRY_RECRUIT_DEBITED", string.Format(
			"role=%1 cost=%2 supplies_before=%3 supplies_after=%4 ticket_cost=0",
			order.m_sRole, order.m_iCost, before, after));
		return true;
	}

	void RefundInfantryRecruit(AICF_InfantryRecruitmentOrder order)
	{
		if (!Replication.IsServer() || !order || order.m_fPaid <= 0)
			return;
		if (!order.m_Base || !order.m_Base.GetOwner() || order.m_Base.GetOwner().GetID() != order.m_BaseId)
			return;
		order.m_Base.AddSupplies(order.m_fPaid);
		order.Log("INFANTRY_RECRUIT_REFUNDED", string.Format("amount=%1", order.m_fPaid));
		order.m_fPaid = 0;
	}

	void CommitInfantryRecruit(AICF_InfantryRecruitmentOrder order)
	{
		if (Replication.IsServer() && order)
			order.m_fPaid = 0;
	}
}
