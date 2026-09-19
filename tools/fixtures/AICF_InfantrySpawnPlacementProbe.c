// Только отдельный stage. Координаты — воспроизведение пользовательского отчёта.
modded class AICF_MatchController
{
	protected bool m_bAICFPlacementProbeStarted;
	protected bool m_bAICFPlacementProbePassed;
	protected ref array<vector> m_aAICFPlacementProbeOrigins = {};

	override protected void TryLogRosterReady()
	{
		super.TryLogRosterReady();
		string enabled;
		if (!m_bRosterReady || m_bAICFPlacementProbeStarted ||
			!System.GetCLIParam("aicfSpawnPlacementProbe", enabled) || enabled != "1")
			return;
		m_bAICFPlacementProbeStarted = true;
		SCR_AIGroup group = m_USSRState.GetSlot(1).GetGroup();
		vector bad = "4986.53 28.5163 11953.8";
		vector position;
		bool rejected = !AICF_InfantrySpawnPlacement.IsUsable(group, bad, position);
		bool found = AICF_InfantrySpawnPlacement.FindPosition(group, bad, position);
		float shift = vector.DistanceXZ(bad, position);
		m_bAICFPlacementProbePassed = rejected && found && shift > 2 && shift <= 34;
		Print(string.Format("[AICF][SPAWN_PLACEMENT_PROBE] rejected_container=%1 found_exit=%2 shift_m=%3 selected=%4 passed=%5", rejected, found, shift, position, m_bAICFPlacementProbePassed));
		for (int side; side < 2; side++)
		{
			AICF_FactionState state = m_USState;
			if (side == 1) state = m_USSRState;
			for (int slotId; slotId < state.GetSlotCount(); slotId++)
			{
				IEntity member = AICF_GroupRuntime.ResolveAliveLeader(state.GetSlot(slotId).GetGroup());
				if (!member)
				{
					m_bAICFPlacementProbePassed = false;
					m_aAICFPlacementProbeOrigins.Insert(vector.Zero);
				}
				else m_aAICFPlacementProbeOrigins.Insert(member.GetOrigin());
			}
		}
		GetGame().GetCallqueue().CallLater(AICF_FinishPlacementProbe, 90000, false);
	}

	protected void AICF_FinishPlacementProbe()
	{
		int moved;
		int alive;
		for (int side; side < 2; side++)
		{
			AICF_FactionState state = m_USState;
			if (side == 1) state = m_USSRState;
			for (int slotId; slotId < state.GetSlotCount(); slotId++)
			{
				IEntity member = AICF_GroupRuntime.ResolveAliveLeader(state.GetSlot(slotId).GetGroup());
				if (!member) continue;
				alive++;
				float distance = vector.DistanceXZ(member.GetOrigin(), m_aAICFPlacementProbeOrigins[side * 10 + slotId]);
				if (slotId < 6 && distance > 25) moved++;
				Print(string.Format("[AICF][SPAWN_PLACEMENT_MOTION] side=%1 slot=%2 distance_m=%3", side, slotId, distance));
			}
		}
		Print(string.Format("[AICF][SPAWN_PLACEMENT_PROBE_FINISHED] passed=%1 alive=%2 moving_attackers=%3", m_bAICFPlacementProbePassed && alive == 20 && moved == 12, alive, moved));
		GetGame().RequestClose();
	}

	void ~AICF_MatchController()
	{
		if (GetGame()) GetGame().GetCallqueue().Remove(AICF_FinishPlacementProbe);
	}
}
