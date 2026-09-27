// Только isolated stage. Повторяем пустые входы native BT-узлов без изменения
// оружия/инвентаря; временно снимаем лишь selected-weapon reference и сразу
// восстанавливаем его в том же синхронном вызове.
modded class SCR_AICombatComponent
{
	void AICF_ProbeSetSelectedWeapon(BaseWeaponComponent weapon, int muzzle)
	{
		m_SelectedWeaponComp = weapon;
		m_iSelectedMuzzle = muzzle;
	}
}

class AICF_AttackInputProbe : SCR_AIUpdateTargetAttackData
{
	void AICF_BindCombat(SCR_AICombatComponent combat)
	{
		m_CombatComponent = combat;
	}

	bool AICF_FirstUpdatePending()
	{
		return m_bFirstSimulate;
	}
}

modded class AICF_MatchController
{
	protected bool m_bAICFCombatInputProbeStarted;
	protected int m_iAICFCombatInputChecks;
	protected int m_iAICFCombatInputFailures;

	protected void AICF_InputCheck(string name, bool passed)
	{
		m_iAICFCombatInputChecks++;
		if (!passed)
			m_iAICFCombatInputFailures++;
		Print(string.Format("[AICF][COMBAT_INPUT_PROBE_CHECK] name=%1 passed=%2", name, passed));
	}

	protected void AICF_TestCombatInputs(AICF_GroupSlot slot, string side)
	{
		IEntity character = AICF_GroupRuntime.ResolveAliveLeader(slot.GetGroup());
		AIAgent agent = slot.GetGroup().GetLeaderAgent();
		SCR_AICombatComponent combat = SCR_AICombatComponent.Cast(character.FindComponent(SCR_AICombatComponent));
		AICF_InputCheck(side + "_SETUP", agent && combat);
		if (!agent || !combat)
			return;
		BaseWeaponComponent selected;
		int muzzle;
		combat.GetSelectedWeapon(selected, muzzle);
		AICF_AttackInputProbe attack = new AICF_AttackInputProbe();
		attack.AICF_BindCombat(combat);
		combat.AICF_ProbeSetSelectedWeapon(null, -1);
		bool attackFailed = true;
		for (int i; i < 64; i++)
			attackFailed = (attack.EOnTaskSimulate(agent, 0.016) == ENodeResult.FAIL) && attackFailed;
		combat.AICF_ProbeSetSelectedWeapon(selected, muzzle);
		AICF_InputCheck(side + "_NO_WEAPON_64_FAILS", attackFailed);
		AICF_InputCheck(side + "_FIRST_UPDATE_RETRYABLE", attack.AICF_FirstUpdatePending());
		BaseWeaponComponent restored;
		int restoredMuzzle;
		combat.GetSelectedWeapon(restored, restoredMuzzle);
		AICF_InputCheck(side + "_SELECTION_RESTORED", restored == selected && restoredMuzzle == muzzle);
		SCR_AIGetSuppressionVolumeCenterPosition center = new SCR_AIGetSuppressionVolumeCenterPosition();
		SCR_AIGetSuppressionVolumeLine line = new SCR_AIGetSuppressionVolumeLine();
		bool centerFailed = true;
		bool lineFailed = true;
		for (int i; i < 64; i++)
		{
			centerFailed = (center.EOnTaskSimulate(agent, 0.016) == ENodeResult.FAIL) && centerFailed;
			lineFailed = (line.EOnTaskSimulate(agent, 0.016) == ENodeResult.FAIL) && lineFailed;
		}
		AICF_InputCheck(side + "_NO_VOLUME_CENTER_64_FAILS", centerFailed);
		AICF_InputCheck(side + "_NO_VOLUME_LINE_64_FAILS", lineFailed);
	}

	override protected void TryLogRosterReady()
	{
		super.TryLogRosterReady();
		string enabled;
		if (!m_bRosterReady || m_bAICFCombatInputProbeStarted || !System.GetCLIParam("aicfCombatInputProbe", enabled) || enabled != "1")
			return;
		m_bAICFCombatInputProbeStarted = true;
		AICF_TestCombatInputs(m_USState.GetSlot(0), "US");
		AICF_TestCombatInputs(m_USSRState.GetSlot(0), "USSR");
		GetGame().GetCallqueue().CallLater(AICF_FinishCombatInputProbe, 30000, false);
	}

	protected void AICF_FinishCombatInputProbe()
	{
		Print(string.Format("[AICF][COMBAT_INPUT_PROBE_FINISHED] checks=%1 failures=%2", m_iAICFCombatInputChecks, m_iAICFCombatInputFailures));
		GetGame().RequestClose();
	}

	override protected void Stop(bool cleanupEntities)
	{
		if (GetGame())
			GetGame().GetCallqueue().Remove(AICF_FinishCombatInputProbe);
		super.Stop(cleanupEntities);
	}
}
