// Только isolated RHS North stage. Воспроизводит исходную позицию из полного
// server log; перемещение персонажей — подготовка теста, не recovery.
modded class AICF_MatchController
{
	protected bool m_bAICFNativeProbe;
	protected SCR_AIGroup m_AICFNativeGroup;
	protected vector m_vAICFNativeStart;

	override protected void TryLogRosterReady()
	{
		super.TryLogRosterReady();
		string enabled;
		if (!m_bRosterReady || m_bAICFNativeProbe || !System.GetCLIParam("aicfNativeMoveProbe", enabled) || enabled != "1")
			return;
		m_bAICFNativeProbe = true;
		GetGame().GetCallqueue().CallLater(AICF_PrepareNativeProbe, 10000, false);
		GetGame().GetCallqueue().CallLater(AICF_CloseNativeProbe, 190000, false);
	}

	protected void AICF_PrepareNativeProbe()
	{
		AICF_GroupSlot slot = m_USState.GetSlot(1);
		m_AICFNativeGroup = slot.GetGroup();
		m_vAICFNativeStart = "5975.74 0.0859407 9670.9";
		array<AIAgent> agents = {};
		m_AICFNativeGroup.GetAgents(agents);
		int count;
		foreach (AIAgent agent : agents)
		{
			ChimeraCharacter character = ChimeraCharacter.Cast(agent.GetControlledEntity());
			if (!AICF_GroupRuntime.IsAliveCharacter(character))
				continue;
			vector transform[4];
			character.GetWorldTransform(transform);
			transform[3] = m_vAICFNativeStart + Vector(count * 2, 0, 0);
			character.Teleport(transform);
			count++;
		}
		Print(string.Format("[AICF][NATIVE_MOVE_PROBE_SETUP] group=%1 members=%2 position=%3 teleport=TEST_INITIAL_CONDITION result_injection=NONE", m_AICFNativeGroup.GetID(), count, m_vAICFNativeStart));
		GetGame().GetCallqueue().CallLater(AICF_AssignNativeProbe, 1000, false);
	}

	protected void AICF_AssignNativeProbe()
	{
		// В начале кампании эта база ещё не достижима по radio graph. Законный
		// point order сохраняет endpoint без обхода TARGET_INVALID/replan gates.
		bool assigned = m_OrderPlanner.AssignPlayerPointOrder(m_USState.GetSlot(1), m_USFaction, "6436.89 10.086 9667.35");
		Print(string.Format("[AICF][NATIVE_MOVE_PROBE_ASSIGNED] target_kind=POSITION assigned=%1", assigned));
	}

	protected void AICF_CloseNativeProbe()
	{
		IEntity leader = AICF_GroupRuntime.ResolveAliveLeader(m_AICFNativeGroup);
		if (leader)
			Print(string.Format("[AICF][NATIVE_MOVE_PROBE_FINISHED] leader=%1 position=%2 displacement_m=%3", leader.GetID(), leader.GetOrigin(), vector.DistanceXZ(leader.GetOrigin(), m_vAICFNativeStart)));
		else
			Print("[AICF][NATIVE_MOVE_PROBE_FINISHED] leader=NONE");
		GetGame().RequestClose();
	}

	override protected void Stop(bool cleanupEntities)
	{
		if (GetGame())
		{
			GetGame().GetCallqueue().Remove(AICF_PrepareNativeProbe);
			GetGame().GetCallqueue().Remove(AICF_AssignNativeProbe);
			GetGame().GetCallqueue().Remove(AICF_CloseNativeProbe);
		}
		super.Stop(cleanupEntities);
	}
}
