// Stock seizing ignores AI agents at max LOD. Managed combat groups must remain
// simulated so they can move, fight, and contribute to Conflict capture queries.
class AICF_ManagedAILODPolicy
{
	bool KeepCaptureEligible(SCR_AIGroup group, out int agentCount, out int recoveredFromMaxLOD)
	{
		agentCount = 0;
		recoveredFromMaxLOD = 0;
		if (!Replication.IsServer() || !group)
			return false;

		array<AIAgent> agents = {};
		group.GetAgents(agents);
		int maxLOD = AIAgent.GetMaxLOD();
		int captureEligibleLOD = maxLOD - 1;

		foreach (AIAgent agent : agents)
		{
			if (!agent)
				continue;

			agentCount++;
			if (agent.GetLOD() == maxLOD)
			{
				agent.SetLOD(captureEligibleLOD);
				recoveredFromMaxLOD++;
			}

			agent.PreventMaxLOD();
		}

		return agentCount > 0;
	}

	void Release(SCR_AIGroup group)
	{
		if (!Replication.IsServer() || !group)
			return;

		array<AIAgent> agents = {};
		group.GetAgents(agents);
		foreach (AIAgent agent : agents)
		{
			if (agent)
				agent.AllowMaxLOD();
		}
	}
}
