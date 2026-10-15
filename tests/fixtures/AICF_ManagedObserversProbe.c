// Только post-fix stage, вместе с AICF_AmbientPatrolProbe.c.
class AICF_ObserverPolicyProbe : AICF_ManagedObservers
{
	IEntity Resolve(SCR_AIGroup group, FactionKey expected) { return ResolveMember(group, expected); }
}

class AICF_ObserverProbeState
{
	static int s_iUnowned;
}

modded class AICF_ManagedObservers
{
	override void Start(SCR_GameModeCampaign campaign)
	{
		array<vector> observers = {};
		ObserversSystem system = ObserversSystem.Cast(GetGame().GetWorld().FindSystem(ObserversSystem));
		system.GetObserversSP(observers);
		AICF_ObserverProbeState.s_iUnowned = observers.Count();
		Print(string.Format("[AICF][AMBIENT_OBSERVERS] phase=BEFORE_SERVICE count=%1 positions=%2", observers.Count(), observers));
		super.Start(campaign);
	}
}

modded class AICF_MatchController
{
	protected SCR_AIGroup m_AICFObserverNegative;
	protected ref AICF_ObserverPolicyProbe m_AICFObserverPolicy = new AICF_ObserverPolicyProbe();
	protected IEntity m_AICFObserverVictim;

	override protected void AmbientPolicyNegatives()
	{
		AICF_AmbientProbe.Check("POLICY_NULL", !m_AICFObserverPolicy.Resolve(null, "US"));
		FactionKey expected = "US";
		FactionKey wrong = "USSR";
		if (m_bAICFAmbientUSSR) { expected = "USSR"; wrong = "US"; }
		AICF_AmbientProbe.Check("POLICY_LIVE", m_AICFObserverPolicy.Resolve(m_AICFAmbientAttack, expected) != null);
		AICF_AmbientProbe.Check("POLICY_WRONG_FACTION", !m_AICFObserverPolicy.Resolve(m_AICFAmbientAttack, wrong));
		AICF_AmbientProbe.Check("POLICY_FIA_REJECTED", !m_AICFObserverPolicy.Resolve(m_AICFAmbientAttack, "FIA"));
		m_AICFObserverNegative = m_GroupSpawner.SpawnGroup(m_USFaction, m_USFaction.GetMainBase(), 98, 1, false);
		vector spawnPosition;
		if (m_AICFObserverNegative)
		{
			spawnPosition = m_AICFObserverNegative.GetOrigin();
			m_AICFObserverNegative.SetDeleteWhenEmpty(false);
			m_AICFObserverNegative.SetOrigin(m_AICFAmbientPoint.GetOwner().GetOrigin());
		}
		AICF_AmbientProbe.Check("POLICY_EMPTY_CONTROLLER", m_AICFObserverNegative && !m_AICFObserverPolicy.Resolve(m_AICFObserverNegative, "US"));
		if (m_AICFObserverNegative)
		{
			m_AICFObserverNegative.SetOrigin(spawnPosition);
			m_GroupSpawner.BeginRosterSpawn(m_AICFObserverNegative, 1);
		}
	}

	override protected void AmbientSnapshot(string phase)
	{
		super.AmbientSnapshot(phase);
		if (phase == "HELD")
		{
			Print(string.Format("[AICF][AMBIENT_DEAD_POLICY] group=%1 member_alive=%2 resolved=%3", AICF_GroupRuntime.FormatEntityId(m_AICFObserverNegative), AICF_GroupRuntime.IsAliveCharacter(m_AICFObserverVictim), m_AICFObserverPolicy.Resolve(m_AICFObserverNegative, "US") != null));
			AICF_AmbientProbe.Check("POLICY_KILLED_ROSTER", m_AICFObserverNegative && !AICF_GroupRuntime.IsAliveCharacter(m_AICFObserverVictim) && !m_AICFObserverPolicy.Resolve(m_AICFObserverNegative, "US"));
			if (m_AICFObserverNegative) SCR_EntityHelper.DeleteEntityAndChildren(m_AICFObserverNegative);
			m_AICFObserverNegative = null;
			m_AICFObserverVictim = null;
			return;
		}
		if (phase != "APPROACH") return;
		IEntity member = AICF_GroupRuntime.ResolveAliveLeader(m_AICFObserverNegative);
		AICF_AmbientProbe.Check("NEGATIVE_ROSTER_MATERIALIZED", member != null);
		if (member)
		{
			SCR_CharacterDamageManagerComponent damage = SCR_CharacterDamageManagerComponent.Cast(ChimeraCharacter.Cast(member).GetDamageManager());
			damage.EnableDamageHandling(true);
			damage.Kill(Instigator.CreateInstigatorGM());
			m_AICFObserverVictim = member;
			m_AICFObserverNegative.SetOrigin(m_AICFAmbientPoint.GetOwner().GetOrigin());
		}
	}

	override protected void AmbientFinish()
	{
		m_iAICFAmbientPhase = 99;
		if (m_ManagedObservers)
		{
			m_ManagedObservers.Stop();
			m_ManagedObservers.Stop();
			m_ManagedObservers.Update(m_USState, m_USSRState);
		}
		GetGame().GetCallqueue().CallLater(AmbientStoppedReadback, 1000, false);
	}

	protected void AmbientStoppedReadback()
	{
		GetGame().GetCallqueue().Remove(AmbientStoppedReadback);
		array<vector> observers = {};
		ObserversSystem system = ObserversSystem.Cast(GetGame().GetWorld().FindSystem(ObserversSystem));
		system.GetObserversSP(observers);
		Print(string.Format("[AICF][AMBIENT_OBSERVERS] phase=AFTER_STOP count=%1 unowned=%2 positions=%3", observers.Count(), AICF_ObserverProbeState.s_iUnowned, observers));
		AICF_AmbientProbe.Check("STOP_REMOVES_ALL_OWNED_OBSERVERS", observers.Count() == AICF_ObserverProbeState.s_iUnowned);
		m_AICFObserverPolicy = null;
		AICF_AmbientProbe.s_mCalls.Clear();
		super.AmbientFinish();
	}
}
