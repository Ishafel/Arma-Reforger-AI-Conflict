// RHS stage; требует Core AICF_FIARandomGarrisonProbe.
class AICF_PMCGarrisonProbe
{
	static int s_iPendingChecks;
	static int s_iFailures;
}

modded class SCR_ChimeraCharacter
{
	void AICF_ProbeEquipmentPending(bool pending) { m_bAICFPMCEquipmentPending = pending; }
}

modded class AICF_FIAGarrisonCrew
{
	override bool BoardGarrison(AICF_FIAGarrison g)
	{
		bool pending;
		array<EntityID> occupiedBefore = {};
		array<AIAgent> agents = {};
		if (g && g.GroupIdentity()) g.m_Group.GetAgents(agents);
		foreach (AIAgent agent : agents)
		{
			SCR_ChimeraCharacter member = SCR_ChimeraCharacter.Cast(agent.GetControlledEntity());
			if (member && member.AICF_IsPMCEquipmentPending()) pending = true;
			if (member && member.GetCompartmentAccessComponent().IsInCompartment()) occupiedBefore.Insert(member.GetID());
		}
		bool result = super.BoardGarrison(g);
		if (pending)
		{
			AICF_PMCGarrisonProbe.s_iPendingChecks++;
			if (result) AICF_PMCGarrisonProbe.s_iFailures++;
			foreach (AIAgent agent : agents)
			{
				ChimeraCharacter member = ChimeraCharacter.Cast(agent.GetControlledEntity());
				if (member && member.GetCompartmentAccessComponent().IsInCompartment() && !occupiedBefore.Contains(member.GetID())) AICF_PMCGarrisonProbe.s_iFailures++;
			}
		}
		return result;
	}
}

modded class AICF_FIAGarrisonService
{
	override protected void RandomFinish()
	{
		int members;
		int invalid;
		bool injected;
		foreach (AICF_FIAGarrison g : s_aDefenders)
		{
			if (!g.m_bReady) continue;
			if (!injected && !g.m_aCrew.IsEmpty())
			{
				SCR_ChimeraCharacter subject = SCR_ChimeraCharacter.Cast(g.m_aCrew[0]);
				bool wasPending = subject.AICF_IsPMCEquipmentPending();
				subject.AICF_ProbeEquipmentPending(true);
				AICF_FIAGarrisonCrew crew = new AICF_FIAGarrisonCrew();
				bool admitted = crew.BoardGarrison(g);
				subject.AICF_ProbeEquipmentPending(wasPending);
				RandomCheck("PMC_INJECTED_PENDING_REJECTED", !admitted && g.OwnsMember(subject));
				injected = true;
			}
			foreach (ChimeraCharacter member : g.m_aCrew)
			{
				members++;
				ResourceName source = SCR_ResourceNameUtils.GetPrefabName(member);
				SCR_ChimeraCharacter character = SCR_ChimeraCharacter.Cast(member);
				if (!character || !AICF_RHSPMCArmament.Validate(member, source, character.AICF_PMCWeaponVariant())) invalid++;
			}
		}
		RandomCheck("PMC_PENDING_BOARDING_BLOCKED", AICF_PMCGarrisonProbe.s_iFailures == 0);
		RandomCheck("PMC_GARRISON_ARMAMENT", invalid == 0);
		Print(string.Format("[AICF][PMC_GARRISON_PROBE] members=%1 invalid=%2 pending_checks=%3 failures=%4 injected=%5", members, invalid, AICF_PMCGarrisonProbe.s_iPendingChecks, AICF_PMCGarrisonProbe.s_iFailures, injected));
		super.RandomFinish();
	}
}
