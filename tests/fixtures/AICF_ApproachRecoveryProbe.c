// Только isolated stage: штатные полные отряды, сбор evidence и bounded shutdown.
modded class AICF_OrderPlanner
{
	override protected bool TryResolveFalseCompletionEndpoint(SCR_AIGroup group, vector objectivePosition,
		int endpointRevision, out vector endpoint, out string failureReason, vector rejectedEndpoint = vector.Zero)
	{
		bool result = super.TryResolveFalseCompletionEndpoint(group, objectivePosition, endpointRevision, endpoint, failureReason, rejectedEndpoint);
		bool water = result && ChimeraWorldUtils.TryGetWaterSurfaceSimple(GetGame().GetWorld(), endpoint - "0 0.5 0");
		Print(string.Format("[AICF][RECOVERY_PROBE_ENDPOINT] resolved=%1 water=%2 endpoint=%3 rejected=%4 reason=%5", result, water, endpoint, rejectedEndpoint, failureReason));
		return result;
	}
}

modded class AICF_MatchController
{
	protected bool m_bAICFRecoveryProbeStarted;
	protected int m_iAICFRecoveryProbePoll;

	override protected void TryLogRosterReady()
	{
		super.TryLogRosterReady();
		string enabled;
		if (!m_bRosterReady || m_bAICFRecoveryProbeStarted ||
			!System.GetCLIParam("aicfRecoveryProbe", enabled) || enabled != "1")
			return;
		m_bAICFRecoveryProbeStarted = true;
		array<vector> oldEndpoints = {"4553.78 0.0917189 11530.6", "4512.64 0.0917189 11448", "4495.06 0.0917189 11442.4"};
		foreach (vector point : oldEndpoints)
			Print(string.Format("[AICF][RECOVERY_PROBE_OLD_ENDPOINT] endpoint=%1 water=%2 foot_water=%3 terrain_y=%4", point, ChimeraWorldUtils.TryGetWaterSurfaceSimple(GetGame().GetWorld(), point), ChimeraWorldUtils.TryGetWaterSurfaceSimple(GetGame().GetWorld(), point - "0 0.5 0"), GetGame().GetWorld().GetSurfaceY(point[0], point[2])));
		GetGame().GetCallqueue().CallLater(AICF_PollRecoveryProbe, 10000, true);
	}

	protected void AICF_PollRecoveryProbe()
	{
		m_iAICFRecoveryProbePoll++;
		AICF_GroupSlot slot = m_USState.GetSlot(4);
		IEntity leader = AICF_GroupRuntime.ResolveAliveLeader(slot.GetGroup());
		if (leader)
			Print(string.Format("[AICF][RECOVERY_PROBE_A4] poll=%1 position=%2 target=%3 revision=%4 no_progress=%5 pending=%6", m_iAICFRecoveryProbePoll, leader.GetOrigin(), AICF_Stage1Diagnostics.BaseKey(slot.GetTargetBase()), slot.GetFalseCompletionEndpointRevision(), slot.GetFalseCompletionNoProgressCount(), slot.HasPendingOrderRecovery()));
		if (m_iAICFRecoveryProbePoll < 42)
			return;
		Print("[AICF][RECOVERY_PROBE_FINISHED] seconds=420");
		GetGame().GetCallqueue().Remove(AICF_PollRecoveryProbe);
		GetGame().RequestClose();
	}

	override protected void Stop(bool cleanupEntities)
	{
		if (GetGame())
			GetGame().GetCallqueue().Remove(AICF_PollRecoveryProbe);
		super.Stop(cleanupEntities);
	}
}
