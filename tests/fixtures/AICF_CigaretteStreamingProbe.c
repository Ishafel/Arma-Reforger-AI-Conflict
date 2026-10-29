// Только isolated direct-connect fixture. Большой скачок задаётся локальному
// cigar prop для воспроизведения streaming pose; actor и ТС не перемещаются.
// Нужны VehiclePresetProbe + VehiclePresetClientProbe для штатного avatar.
modded class SCR_CigaretAnimationComponent
{
	protected int m_iAICFStreamingFrames;
	protected int m_iAICFStreamingFixed;
	protected bool m_bAICFStreamingObserved;

	override void EOnFrame(IEntity owner, float timeSlice)
	{
		m_iAICFStreamingFrames++;
		super.EOnFrame(owner, timeSlice);
		// Только измерение fixture: mouth native VISIBLE update не даёт clock
		// на невидимом avatar. Production mouth policy остаётся native.
		if (m_bAICFStreamingObserved && m_MouthSmokeEmitter) m_MouthSmokeEmitter.SetUseFrameEvent(true);
	}

	override void EOnPostFixedFrame(IEntity owner, float timeSlice)
	{
		m_iAICFStreamingFixed++;
		super.EOnPostFixedFrame(owner, timeSlice);
	}
	bool AICF_StreamingFrameOnly() { return (GetEventMask() & EntityEvent.FRAME) != 0 && (GetEventMask() & EntityEvent.POSTFIXEDFRAME) == 0 && m_iAICFStreamingFixed == 0; }
	bool AICF_StreamingEmitters() { return m_CigarSmokeEmitter && m_MouthSmokeEmitter; }
	bool AICF_StreamingPlaying()
	{
		return AICF_StreamingEmitters() && m_CigarSmokeEmitter.GetState() == EParticleEffectState.PLAYING &&
			m_MouthSmokeEmitter.GetState() == EParticleEffectState.PLAYING;
	}
	int AICF_StreamingFrames() { return m_iAICFStreamingFrames; }
	ParticleEffectEntity AICF_StreamingCigar() { return m_CigarSmokeEmitter; }
	ParticleEffectEntity AICF_StreamingMouth() { return m_MouthSmokeEmitter; }
	void AICF_StreamingCool() { m_fMinDistanceSqFromCharacterFace = 0; m_fEmissiveMultiplier = 1; }
	void AICF_StreamingObserve() { m_bAICFStreamingObserved = true; }
	bool AICF_StreamingEmission()
	{
		return AICF_StreamingPlaying() && m_CigarSmokeEmitter.GetTotalSimulationTime() > 0.1 &&
			m_CigarSmokeEmitter.HasActiveParticles() && m_MouthSmokeEmitter.GetTotalSimulationTime() > 0.1 && m_MouthSmokeEmitter.HasActiveParticles();
	}
	bool AICF_StreamingPose(IEntity owner)
	{
		if (!owner || !AICF_StreamingEmitters()) return false;
		SCR_CigaretAnimationComponentClass data = SCR_CigaretAnimationComponentClass.Cast(GetComponentData(owner));
		vector source[4]; vector actual[4];
		owner.GetWorldTransform(source);
		m_CigarSmokeEmitter.GetWorldTransform(actual);
		vector offset = data.GetCigarSmokeEmitterOffsetLS();
		vector expected = source[3] + source[0] * offset[0] + source[1] * offset[1] + source[2] * offset[2];
		float rotationDelta = vector.Distance(source[0], actual[0]) + vector.Distance(source[1], actual[1]) + vector.Distance(source[2], actual[2]);
		Print(string.Format("[AICF][CIGARETTE_STREAM_POSE] prop=%1 actual=%2 expected=%3 distance_m=%4 rotation_delta=%5 direct_parent=%6 leading_parent=%7 cigar_sim_s=%8 cigar_particles=%9",
			owner.GetOrigin(), actual[3], expected, vector.Distance(expected, actual[3]), rotationDelta,
			AICF_GroupTrace.Entity(m_CigarSmokeEmitter.GetParent()), AICF_GroupTrace.Entity(m_CigarSmokeEmitter.GetLeadingParent()),
			m_CigarSmokeEmitter.GetTotalSimulationTime(), m_CigarSmokeEmitter.HasActiveParticles()));
		Print(string.Format("[AICF][CIGARETTE_STREAM_MOUTH] parent=%1 expected_parent=%2 position=%3 actor_distance_m=%4 sim_s=%5 active_particles=%6 resyncs=%7",
			AICF_GroupTrace.Entity(m_MouthSmokeEmitter.GetParent()), AICF_GroupTrace.Entity(owner.GetParent()), m_MouthSmokeEmitter.GetOrigin(),
			vector.Distance(m_MouthSmokeEmitter.GetOrigin(), owner.GetParent().GetOrigin()), m_MouthSmokeEmitter.GetTotalSimulationTime(), m_MouthSmokeEmitter.HasActiveParticles(), m_iAICFParticlePoseResyncs));
		return vector.Distance(expected, actual[3]) <= 0.1 && rotationDelta <= 0.01 && m_CigarSmokeEmitter.GetLeadingParent() == owner && m_MouthSmokeEmitter.GetParent() == owner.GetParent() &&
			vector.Distance(m_MouthSmokeEmitter.GetOrigin(), owner.GetParent().GetOrigin()) <= 3;
	}
}

modded class SCR_GameModeCampaign
{
	protected int m_iAICFSmokeStarted;
	protected int m_iAICFSmokePhase;
	protected int m_iAICFSmokeCycle;
	protected int m_iAICFSmokePhaseAt;
	protected int m_iAICFSmokeChecks;
	protected int m_iAICFSmokeFailures;
	protected bool m_bAICFSmokeDone;
	protected IEntity m_AICFSmokeProp;
	protected ChimeraCharacter m_AICFSmokeActor;
	protected SCR_CigaretAnimationComponent m_AICFSmokeAnimation;
	protected ParticleEffectEntity m_AICFSmokeCigar;
	protected ParticleEffectEntity m_AICFSmokeMouth;
	protected vector m_vAICFSmokeActorStart;

	protected void AICF_SmokeCheck(string name, bool passed, string details = "")
	{
		m_iAICFSmokeChecks++;
		if (!passed) m_iAICFSmokeFailures++;
		Print(string.Format("[AICF][CIGARETTE_STREAM_CHECK] cycle=%1 rule=%2 passed=%3 %4", m_iAICFSmokeCycle, name, passed, details));
	}

	override void OnGameStart()
	{
		super.OnGameStart();
		string flag;
		if (Replication.IsServer() || !System.GetCLIParam("aicfCigaretteStreamingProbe", flag) || flag != "1") return;
		m_iAICFSmokeStarted = System.GetTickCount();
		GetGame().GetCallqueue().CallLater(AICF_SmokeTick, 250, true);
	}

	protected void AICF_SmokeTick()
	{
		int now = System.GetTickCount();
		if (m_bAICFSmokeDone) return;
		if (now - m_iAICFSmokeStarted >= 120000)
		{
			AICF_SmokeCheck("TIMEOUT", false);
			AICF_SmokeFinish();
			return;
		}
		if (m_iAICFSmokePhase == 0)
		{
			PlayerController player = GetGame().GetPlayerController();
			if (!player) return;
			m_AICFSmokeActor = ChimeraCharacter.Cast(player.GetControlledEntity());
			if (!m_AICFSmokeActor || !AICF_GroupRuntime.IsAliveCharacter(m_AICFSmokeActor)) return;
			m_vAICFSmokeActorStart = m_AICFSmokeActor.GetOrigin();
			EntitySpawnParams params = new EntitySpawnParams();
			params.Transform[3] = m_vAICFSmokeActorStart;
			m_AICFSmokeProp = GetGame().SpawnEntityPrefab(Resource.Load("{6636333D6A19AF17}Prefabs/Props/Civilian/Cigar/AnimationProp_Cigar.et"), GetGame().GetWorld(), params);
			AICF_SmokeCheck("PROP", m_AICFSmokeProp != null);
			if (!m_AICFSmokeProp) { AICF_SmokeFinish(); return; }
			m_AICFSmokeActor.AddChild(m_AICFSmokeProp, -1);
			m_AICFSmokeAnimation = SCR_CigaretAnimationComponent.Cast(m_AICFSmokeProp.FindComponent(SCR_CigaretAnimationComponent));
			AICF_SmokeCheck("EMITTERS", m_AICFSmokeAnimation && m_AICFSmokeAnimation.AICF_StreamingEmitters());
			if (!m_AICFSmokeAnimation) { AICF_SmokeFinish(); return; }
			m_AICFSmokeAnimation.AICF_StreamingObserve();
			m_AICFSmokeCigar = m_AICFSmokeAnimation.AICF_StreamingCigar();
			m_AICFSmokeMouth = m_AICFSmokeAnimation.AICF_StreamingMouth();
			m_AICFSmokeProp.SetOrigin(m_vAICFSmokeActorStart + "1000 -800 1000");
			m_AICFSmokeAnimation.AICF_StreamingCool();
			m_iAICFSmokePhaseAt = now;
			m_iAICFSmokePhase = 1;
		}
		else if (m_iAICFSmokePhase == 1 && now - m_iAICFSmokePhaseAt >= 1000)
		{
			AICF_SmokeCheck("FAR_PLAYBACK", m_AICFSmokeAnimation.AICF_StreamingPlaying());
			vector destination = m_AICFSmokeActor.GetOrigin() + "0 1.2 0.2";
			Print(string.Format("[AICF][CIGARETTE_STREAM_JUMP] cycle=%1 prop=%2 from=%3 to=%4 distance_m=%5 actor=%6 actor_displacement_m=%7 cosmetic_only=1",
				m_iAICFSmokeCycle, AICF_GroupTrace.Entity(m_AICFSmokeProp), m_AICFSmokeProp.GetOrigin(), destination,
				vector.Distance(m_AICFSmokeProp.GetOrigin(), destination), AICF_GroupTrace.Entity(m_AICFSmokeActor), vector.Distance(m_AICFSmokeActor.GetOrigin(), m_vAICFSmokeActorStart)));
			m_AICFSmokeProp.SetOrigin(destination);
			m_AICFSmokeAnimation.AICF_StreamingCool();
			m_iAICFSmokePhaseAt = now;
			m_iAICFSmokePhase = 2;
		}
		else if (m_iAICFSmokePhase == 2 && now - m_iAICFSmokePhaseAt >= 1000)
		{
			AICF_SmokeCheck("NEAR_PLAYBACK", m_AICFSmokeAnimation.AICF_StreamingPlaying() && m_AICFSmokeAnimation.AICF_StreamingFrames() > 10 && m_AICFSmokeAnimation.AICF_StreamingFrameOnly());
			AICF_SmokeCheck("NEAR_POSE", m_AICFSmokeAnimation.AICF_StreamingPose(m_AICFSmokeProp));
			AICF_SmokeCheck("ACTUAL_EMISSION", m_AICFSmokeAnimation.AICF_StreamingEmission());
			AICF_SmokeCheck("ACTOR_UNMOVED", vector.Distance(m_AICFSmokeActor.GetOrigin(), m_vAICFSmokeActorStart) < 2);
			// Resync заменяет локальные emitters; delete proof захватывает именно
			// текущие entities, а не уже удалённые ссылки перед скачком.
			m_AICFSmokeCigar = m_AICFSmokeAnimation.AICF_StreamingCigar();
			m_AICFSmokeMouth = m_AICFSmokeAnimation.AICF_StreamingMouth();
			delete m_AICFSmokeProp;
			m_iAICFSmokePhaseAt = now;
			m_iAICFSmokePhase = 3;
		}
		else if (m_iAICFSmokePhase == 3 && now - m_iAICFSmokePhaseAt >= 250)
		{
			AICF_SmokeCheck("DELETE", !m_AICFSmokeProp && !m_AICFSmokeCigar && !m_AICFSmokeMouth);
			m_iAICFSmokeCycle++;
			m_iAICFSmokePhase = 0;
			if (m_iAICFSmokeCycle == 3) AICF_SmokeFinish();
		}
	}

	protected void AICF_SmokeFinish()
	{
		m_bAICFSmokeDone = true;
		if (m_AICFSmokeProp) delete m_AICFSmokeProp;
		GetGame().GetCallqueue().Remove(AICF_SmokeTick);
		Print(string.Format("[AICF][CIGARETTE_STREAM_DONE] cycles=%1 checks=%2 failures=%3 version=2 mouth_offscreen_observer=FRAME", m_iAICFSmokeCycle, m_iAICFSmokeChecks, m_iAICFSmokeFailures));
	}

	override protected void AICF_VehicleUIClose()
	{
		string flag;
		if (!Replication.IsServer() && System.GetCLIParam("aicfCigaretteStreamingProbe", flag) && !m_bAICFSmokeDone)
		{
			GetGame().GetCallqueue().CallLater(AICF_VehicleUIClose, 500, false);
			return;
		}
		super.AICF_VehicleUIClose();
	}

	override void OnGameEnd()
	{
		GetGame().GetCallqueue().Remove(AICF_SmokeTick);
		if (m_AICFSmokeProp) delete m_AICFSmokeProp;
		super.OnGameEnd();
	}
}

modded class SCR_PlayerController
{
	override void AICF_VehicleProbeClientFinished(int failures, bool jip)
	{
		super.AICF_VehicleProbeClientFinished(failures, jip);
		string flag;
		if (!Replication.IsServer() && System.GetCLIParam("aicfCigaretteStreamingProbe", flag)) Rpc(RpcAsk_AICFSmokeClose);
	}

	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_AICFSmokeClose()
	{
		string flag;
		PlayerManager manager = GetGame().GetPlayerManager();
		if (!Replication.IsServer() || !manager || manager.GetPlayerController(GetPlayerId()) != this ||
			!System.GetCLIParam("aicfCigaretteStreamingProbe", flag) || flag != "1") return;
		SCR_GameModeCampaign campaign = SCR_GameModeCampaign.Cast(GetGame().GetGameMode());
		if (campaign) campaign.AICF_VehicleProbeScheduleServerClose();
	}
}
