// Только isolated stage после общего FIA proximity probe.
// Реальный native AddAgent с синхронным отказом внешнего OnAgentAdded callback.
class AICF_FIADesantTransferProbe
{
	static AICF_FIAGarrison s_Garrison;
	static SCR_AIGroup s_HookGroup;
	static EntityID s_HookId;
	static AIAgent s_Agent;
	static ChimeraCharacter s_Entity;
	static EntityID s_EntityId;
	static bool s_bRejected;
	static bool s_bRollbackChecked;
	static bool s_bRetryChecked;
	static int s_iExpected;

	static void Hook()
	{
		if (!s_Garrison || !s_Garrison.DesantIdentity() || s_HookGroup) return;
		s_HookGroup = s_Garrison.m_DesantGroup;
		s_HookId = s_HookGroup.GetID();
		s_HookGroup.GetOnAgentAdded().Insert(RejectOnce);
	}

	static void RejectOnce(AIAgent agent)
	{
		if (s_bRejected || !agent || !s_Garrison || !s_Garrison.DesantIdentity() ||
			agent.GetParentGroup() != s_Garrison.m_DesantGroup) return;
		s_bRejected = true;
		s_Agent = agent;
		s_Entity = ChimeraCharacter.Cast(agent.GetControlledEntity());
		s_EntityId = s_Entity.GetID();
		s_iExpected = s_Garrison.m_aCrew.Count();
		s_Garrison.m_DesantGroup.RemoveAgent(agent);
		AICF_FIAProximityProbe.Check("TRANSFER_NATIVE_REFUSAL", !agent.GetParentGroup());
	}

	static void Rollback()
	{
		if (!s_bRejected || s_bRollbackChecked) return;
		s_bRollbackChecked = true;
		AICF_FIAProximityProbe.Check("TRANSFER_ROLLBACK_PARENT", s_Agent.GetParentGroup() == s_Garrison.m_Group &&
			s_Agent.GetControlledEntity() == s_Entity && s_Entity.GetID() == s_EntityId);
		AICF_FIAProximityProbe.Check("TRANSFER_ROLLBACK_CAPACITY", s_Garrison.m_Lifecycle.LivingCapacity() == s_Garrison.m_Group.GetAgentsCount());
		AICF_FIAProximityProbe.Check("TRANSFER_ROLLBACK_READY", s_Garrison.m_Lifecycle.Ready());
		Print(string.Format("[AICF][FIA_TRANSFER_ROLLBACK] capacity=%1 agents=%2 ready=%3 entity=%4", s_Garrison.m_Lifecycle.LivingCapacity(),
			s_Garrison.m_Group.GetAgentsCount(), s_Garrison.m_Lifecycle.Ready(), s_EntityId));
	}

	static void Retry()
	{
		if (s_bRetryChecked) return;
		s_bRetryChecked = true;
		if (!s_bRollbackChecked || !s_Agent || !s_Entity)
		{
			AICF_FIAProximityProbe.Check("TRANSFER_RETRY", false);
			return;
		}
		AICF_FIAProximityProbe.Check("TRANSFER_RETRY", s_bRollbackChecked && s_Agent.GetParentGroup() == s_Garrison.m_DesantGroup &&
			s_Garrison.m_Lifecycle.Ready() && s_Garrison.m_DesantLifecycle.Ready());
		AICF_FIAProximityProbe.Check("TRANSFER_SAME_ENTITY", s_Agent.GetControlledEntity() == s_Entity && s_Entity.GetID() == s_EntityId);
		AICF_FIAProximityProbe.Check("TRANSFER_TOTAL", s_Garrison.m_Lifecycle.LivingCapacity() + s_Garrison.m_DesantLifecycle.LivingCapacity() == s_iExpected &&
			s_Garrison.m_Group.GetAgentsCount() + s_Garrison.m_DesantGroup.GetAgentsCount() == s_iExpected);
	}

	static void Stop()
	{
		if (s_HookGroup && s_HookGroup.GetID() == s_HookId) s_HookGroup.GetOnAgentAdded().Remove(RejectOnce);
		s_HookGroup = null;
		s_Garrison = null;
		s_Agent = null;
		s_Entity = null;
	}
}

modded class AICF_FIAGarrisonCrew
{
	override void UpdateGarrison(AICF_FIAGarrison g, int now)
	{
		string mode;
		bool watched = System.GetCLIParam("aicfFIADesantTransferProbe", mode) && g == AICF_FIADesantTransferProbe.s_Garrison;
		if (watched) AICF_FIADesantTransferProbe.Hook();
		super.UpdateGarrison(g, now);
		if (watched) AICF_FIADesantTransferProbe.Rollback();
	}
}

modded class AICF_MatchController
{
	override protected bool AICF_FIAProbePrepareActor()
	{
		bool ready = super.AICF_FIAProbePrepareActor();
		AICF_FIADesantTransferProbe.s_Garrison = m_AICFFIAProbeTarget;
		return ready;
	}

	override protected void Update()
	{
		super.Update();
		string mode;
		if (System.GetCLIParam("aicfFIADesantTransferProbe", mode) && mode == "baseline" &&
			AICF_FIADesantTransferProbe.s_bRollbackChecked && m_iAICFFIAProbePhase != 99) AICF_FIAProbeFinish();
	}

	override protected void AICF_FIAProbeSafetyChecks()
	{
		super.AICF_FIAProbeSafetyChecks();
		AICF_FIADesantTransferProbe.Retry();
	}

	override protected void Stop(bool cleanupEntities)
	{
		AICF_FIADesantTransferProbe.Stop();
		super.Stop(cleanupEntities);
	}
}
