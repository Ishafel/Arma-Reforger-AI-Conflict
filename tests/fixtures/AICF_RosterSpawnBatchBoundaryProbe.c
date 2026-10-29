// Isolated read-only default startup, or one explicitly selected MAX10 slot.
modded class AICF_MatchController
{
	protected int m_iAICFBatchBoundaryStarted;
	protected int m_iAICFBatchBoundaryReady;
	protected bool m_bAICFBatchBoundaryDone;
	protected ref array<EntityID> m_aAICFBatchBoundaryActors = {};
	protected ref array<string> m_aAICFBatchBoundaryRpl = {};

	protected bool AICF_BatchBoundaryEnabled()
	{
		string flag;
		return Replication.IsServer() && System.GetCLIParam("aicfRosterBatchBoundary", flag) && flag == "1";
	}

	protected bool AICF_BatchBoundaryMax()
	{
		string flag;
		return System.GetCLIParam("aicfRosterBatchMaxTen", flag) && flag == "1";
	}

	override protected bool SpawnInitialRoster(AICF_FactionState factionState, SCR_CampaignFaction faction)
	{
		if (AICF_BatchBoundaryEnabled())
		{
			if (m_iAICFBatchBoundaryStarted == 0)
			{
				m_iAICFBatchBoundaryStarted = System.GetTickCount();
				GetGame().GetCallqueue().CallLater(AICF_BatchBoundaryTick, 250, true);
			}
			if (AICF_BatchBoundaryMax() && factionState == m_USSRState)
			{
				// Explicit controlled configuration before ordinary initial bind.
				factionState.GetSlot(0).SetUnitType(AICF_EGroupUnitType.MOTORIZED_LIGHT);
				factionState.GetSlot(0).SetDesiredSize(10);
			}
		}
		return super.SpawnInitialRoster(factionState, faction);
	}

	protected void AICF_BatchBoundaryFinish(bool passed, string reason)
	{
		if (m_bAICFBatchBoundaryDone) return;
		m_bAICFBatchBoundaryDone = true;
		GetGame().GetCallqueue().Remove(AICF_BatchBoundaryTick);
		Print(string.Format("[AICF][ROSTER_BATCH_BOUNDARY_DONE] passed=%1 max_ten=%2 reason=%3 actual=%4", passed,
			AICF_BatchBoundaryMax(), reason, m_aAICFBatchBoundaryActors.Count()));
		GetGame().RequestClose();
	}

	protected void AICF_BatchBoundaryTick()
	{
		if (m_bAICFBatchBoundaryDone || !AICF_BatchBoundaryEnabled()) return;
		if (System.GetTickCount(m_iAICFBatchBoundaryStarted) > 60000)
		{
			AICF_BatchBoundaryFinish(false, "ORIGINAL_STARTUP_DEADLINE"); return;
		}
		if (!m_bRosterReady) return;
		array<AICF_FactionState> states = {m_USState, m_USSRState};
		int groups, total;
		foreach (AICF_FactionState state : states)
		{
			for (int index; index < state.GetSlotCount(); index++)
			{
				AICF_GroupSlot slot = state.GetSlot(index);
				int expected = 1;
				if (AICF_BatchBoundaryMax() && state == m_USSRState && index == 0) expected = 10;
				int actual, wrong, dead;
				if (!slot || slot.GetSpawnGeneration() != 1 || slot.GetState() != AICF_EGroupSlotState.READY ||
					!AICF_GroupRuntime.HasExactFactionRoster(slot.GetGroup(), state.GetFactionKey(), expected, actual, wrong, dead))
				{
					AICF_BatchBoundaryFinish(false, "EXACT_ORIGINAL_READY_ROSTER_CHANGED"); return;
				}
				array<AIAgent> agents = {};
				slot.GetGroup().GetAgents(agents);
				foreach (AIAgent agent : agents)
				{
					IEntity member = agent.GetControlledEntity();
					string rpl = AICF_VehicleBehaviorBinding.ComponentRpl(member);
					if (m_iAICFBatchBoundaryReady == 0)
					{
						m_aAICFBatchBoundaryActors.Insert(member.GetID());
						m_aAICFBatchBoundaryRpl.Insert(rpl);
					}
					else if (m_aAICFBatchBoundaryActors[total] != member.GetID() || m_aAICFBatchBoundaryRpl[total] != rpl)
					{
						AICF_BatchBoundaryFinish(false, "ORIGINAL_ACTOR_OR_COMPONENT_RPL_CHANGED"); return;
					}
					total++;
				}
				groups++;
			}
		}
		if (m_iAICFBatchBoundaryReady == 0)
		{
			m_iAICFBatchBoundaryReady = System.GetTickCount();
			Print(string.Format("[AICF][ROSTER_BATCH_BOUNDARY_READY] groups=%1 actual=%2 max_ten=%3 originals=1 component_rpl=1 next_poll_ready=1",
				groups, total, AICF_BatchBoundaryMax()));
		}
		if (System.GetTickCount(m_iAICFBatchBoundaryReady) < 10000) return;
		int wanted = 20;
		if (AICF_BatchBoundaryMax()) wanted = 29;
		AICF_BatchBoundaryFinish(groups == 20 && total == wanted, "EXACT_DEFAULT_OR_CONTROLLED_MAX_TEN_10S_TAIL");
	}

	override protected void Stop(bool cleanupEntities)
	{
		if (GetGame()) GetGame().GetCallqueue().Remove(AICF_BatchBoundaryTick);
		super.Stop(cleanupEntities);
	}
}
