// Только isolated stage. Реальный stock prop и native particle playback.
modded class SCR_CigaretAnimationComponent
{
	protected int m_iAICFCigaretteFrames;
	protected int m_iAICFCigaretteFixed;

	override void EOnFrame(IEntity owner, float timeSlice)
	{
		m_iAICFCigaretteFrames++;
		super.EOnFrame(owner, timeSlice);
	}
	override void EOnPostFixedFrame(IEntity owner, float timeSlice)
	{
		m_iAICFCigaretteFixed++;
		super.EOnPostFixedFrame(owner, timeSlice);
	}
	int AICF_ProbeFrames() { return m_iAICFCigaretteFrames; }
	int AICF_ProbeFixed() { return m_iAICFCigaretteFixed; }
	bool AICF_ProbeEmitters() { return m_CigarSmokeEmitter && m_MouthSmokeEmitter; }
	ParticleEffectEntity AICF_ProbeCigar() { return m_CigarSmokeEmitter; }
	ParticleEffectEntity AICF_ProbeMouth() { return m_MouthSmokeEmitter; }
	void AICF_ProbeCool()
	{
		m_fMinDistanceSqFromCharacterFace = 0;
		m_fEmissiveMultiplier = 1;
	}
	bool AICF_ProbePlaying()
	{
		return AICF_ProbeEmitters() && m_CigarSmokeEmitter.GetState() == EParticleEffectState.PLAYING &&
			m_MouthSmokeEmitter.GetState() == EParticleEffectState.PLAYING;
	}
}

modded class AICF_MatchController
{
	protected int m_iAICFCigarettePhase;
	protected int m_iAICFCigaretteCycle;
	protected int m_iAICFCigaretteDeadline;
	protected int m_iAICFCigaretteStarted;
	protected int m_iAICFCigaretteChecks;
	protected int m_iAICFCigaretteFailures;
	protected int m_iAICFCigaretteSamples;
	protected int m_iAICFCigaretteFrameSample;
	protected bool m_bAICFCigarettePlaying;
	protected IEntity m_AICFCigaretteProp;
	protected ChimeraCharacter m_AICFCigaretteParent;
	protected SCR_CigaretAnimationComponent m_AICFCigarette;
	protected ParticleEffectEntity m_AICFCigaretteCigar;
	protected ParticleEffectEntity m_AICFCigaretteMouth;

	protected void AICF_CigaretteCheck(string name, bool passed)
	{
		m_iAICFCigaretteChecks++;
		if (!passed) m_iAICFCigaretteFailures++;
		Print(string.Format("[AICF][CIGARETTE_CASE] case=%1 cycle=%2 pass=%3", name, m_iAICFCigaretteCycle, passed));
	}
	override protected void Update()
	{
		super.Update();
		if (!System.IsCLIParam("aicfCigaretteProbe") || !m_bRosterReady || m_bStopped) return;
		int now = System.GetTickCount();
		if (!m_iAICFCigaretteStarted) m_iAICFCigaretteStarted = now;
		if (System.GetTickCount(m_iAICFCigaretteStarted) > 240000)
		{
			AICF_CigaretteCheck("TIMEOUT", false);
			GetGame().RequestClose();
			return;
		}
		if (now < m_iAICFCigaretteDeadline) return;
		if (m_iAICFCigarettePhase == 0)
		{
			AICF_GroupSlot slot = m_USState.GetSlot(0);
			if (m_iAICFCigaretteCycle % 2 == 1) slot = m_USSRState.GetSlot(0);
			m_AICFCigaretteParent = null;
			if (slot) m_AICFCigaretteParent = ChimeraCharacter.Cast(AICF_GroupRuntime.ResolveAliveLeader(slot.GetGroup()));
			AICF_CigaretteCheck("PARENT", m_AICFCigaretteParent != null);
			if (!m_AICFCigaretteParent) { GetGame().RequestClose(); return; }
			EntitySpawnParams params = new EntitySpawnParams();
			params.Transform[3] = m_AICFCigaretteParent.GetOrigin();
			m_AICFCigaretteProp = GetGame().SpawnEntityPrefab(Resource.Load("{6636333D6A19AF17}Prefabs/Props/Civilian/Cigar/AnimationProp_Cigar.et"), GetGame().GetWorld(), params);
			AICF_CigaretteCheck("PROP", m_AICFCigaretteProp != null);
			if (!m_AICFCigaretteProp) { GetGame().RequestClose(); return; }
			m_AICFCigaretteParent.AddChild(m_AICFCigaretteProp, -1);
			m_AICFCigaretteProp.SetOrigin(m_AICFCigaretteParent.CoordToParent("0 1.2 0.2"));
			m_AICFCigarette = SCR_CigaretAnimationComponent.Cast(m_AICFCigaretteProp.FindComponent(SCR_CigaretAnimationComponent));
			AICF_CigaretteCheck("COMPONENT", m_AICFCigarette && m_AICFCigarette.AICF_ProbeEmitters());
			if (!m_AICFCigarette) { GetGame().RequestClose(); return; }
			int mask = m_AICFCigarette.GetEventMask();
			AICF_CigaretteCheck("FRAME_MASK", (mask & EntityEvent.FRAME) != 0 && (mask & EntityEvent.POSTFIXEDFRAME) == 0);
			m_AICFCigaretteCigar = m_AICFCigarette.AICF_ProbeCigar();
			m_AICFCigaretteMouth = m_AICFCigarette.AICF_ProbeMouth();
			m_iAICFCigaretteSamples = 0;
			m_bAICFCigarettePlaying = false;
			m_iAICFCigarettePhase = 1;
		}
		else if (m_iAICFCigarettePhase == 1)
		{
			if (m_AICFCigarette.AICF_ProbePlaying()) m_bAICFCigarettePlaying = true;
			m_AICFCigarette.AICF_ProbeCool();
			m_iAICFCigaretteSamples++;
			if (m_iAICFCigaretteSamples >= 8)
			{
				AICF_CigaretteCheck("PLAYBACK", m_bAICFCigarettePlaying && m_AICFCigarette.AICF_ProbeFrames() > 10 && m_AICFCigarette.AICF_ProbeFixed() == 0);
				m_AICFCigaretteParent.RemoveChild(m_AICFCigaretteProp, true);
				m_iAICFCigaretteFrameSample = m_AICFCigarette.AICF_ProbeFrames();
				m_iAICFCigarettePhase = 2;
			}
		}
		else if (m_iAICFCigarettePhase == 2)
		{
			AICF_CigaretteCheck("DETACH", m_AICFCigarette.AICF_ProbeFrames() == m_iAICFCigaretteFrameSample &&
				(m_AICFCigarette.GetEventMask() & (EntityEvent.FRAME | EntityEvent.POSTFIXEDFRAME)) == 0);
			delete m_AICFCigaretteProp;
			m_iAICFCigarettePhase = 3;
		}
		else
		{
			AICF_CigaretteCheck("DELETE", !m_AICFCigaretteProp && !m_AICFCigaretteCigar && !m_AICFCigaretteMouth);
			m_iAICFCigaretteCycle++;
			m_iAICFCigarettePhase = 0;
			if (m_iAICFCigaretteCycle == 6)
			{
				Print(string.Format("[AICF][CIGARETTE_DONE] checks=%1 failed=%2 cycles=%3", m_iAICFCigaretteChecks, m_iAICFCigaretteFailures, m_iAICFCigaretteCycle));
				GetGame().RequestClose();
			}
		}
		m_iAICFCigaretteDeadline = now + 250;
	}

	override protected void Stop(bool cleanupEntities)
	{
		if (m_AICFCigaretteProp) delete m_AICFCigaretteProp;
		super.Stop(cleanupEntities);
	}
}
