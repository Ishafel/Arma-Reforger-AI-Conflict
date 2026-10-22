// Только isolated WCS + RHS stage. Не оборачивает Plan.Build: дополнительный
// stack frame может маскировать lifetime temporary native references.
class AICF_EquipmentCleanupProbe
{
	static ref array<IEntity> s_aItems = {};
	static int s_iBuilt;
}

modded class AICF_EquipmentService
{
	void AICF_ProbeEviction(AICF_GroupSlot slot, SCR_CampaignFaction faction)
	{
		for (int index; index < 10; index++)
		{
			AICF_EquipmentPlan plan = Plan(slot, faction, index);
			if (!plan) continue;
			AICF_EquipmentCleanupProbe.s_iBuilt++;
			if (index == 0) continue;
			array<IEntity> items = {};
			AICF_EquipmentInventory.Manager(plan.m_Draft.GetCharacter()).GetItems(items);
			AICF_EquipmentInventory.UniqueItems(items, AICF_EquipmentCleanupProbe.s_aItems);
		}
	}
}

modded class AICF_MatchController
{
	protected int m_iAICFCleanupReady;
	override protected void Update()
	{
		super.Update();
		if (!m_bRosterReady || !System.IsCLIParam("aicfEquipmentFailureProbe")) return;
		if (!m_iAICFCleanupReady)
		{
			m_iAICFCleanupReady = System.GetTickCount();
			m_EquipmentService.AICF_ProbeEviction(m_USState.GetSlot(0), m_USFaction);
			m_EquipmentService.AICF_ProbeEviction(m_USSRState.GetSlot(0), m_USSRFaction);
		}
		if (System.GetTickCount(m_iAICFCleanupReady) < 20000) return;
		int alive;
		foreach (IEntity item : AICF_EquipmentCleanupProbe.s_aItems) if (item) alive++;
		Print(string.Format("[AICF][EQUIPMENT_CLEANUP][DONE] built=%1 sampled=%2 alive=%3", AICF_EquipmentCleanupProbe.s_iBuilt, AICF_EquipmentCleanupProbe.s_aItems.Count(), alive));
		GetGame().RequestClose();
	}
}
