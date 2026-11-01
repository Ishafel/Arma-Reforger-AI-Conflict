// Play/StopEmission меняют отдельные particle entities. Fixed callback может
// выполняться при заблокированной иерархии; обновляем косметику в обычном frame.
modded class SCR_CigaretAnimationComponent
{
	protected static const float AICF_PARTICLE_POSE_JUMP_METERS = 5.0;
	protected static const float AICF_PARTICLE_SMOOTH_SPEED_MPS = 100.0;
	protected bool m_bAICFParticlePose;
	protected vector m_vAICFParticlePose;
	protected int m_iAICFParticlePoseResyncs;

	override protected void OnAddedToParent(IEntity child, IEntity parent)
	{
		super.OnAddedToParent(child, parent);
		ClearEventMask(child, EntityEvent.POSTFIXEDFRAME);
		SetEventMask(child, EntityEvent.FRAME);
		m_vAICFParticlePose = child.GetOrigin();
		m_bAICFParticlePose = true;
	}

	override void EOnPostFixedFrame(IEntity owner, float timeSlice)
	{
		// Не допускаем старый dispatch, уже поставленный в fixed-step очередь.
	}

	override void EOnFrame(IEntity owner, float timeSlice)
	{
		if (!owner || !ChimeraCharacter.Cast(owner.GetParent()) || !m_CigarSmokeEmitter || !m_MouthSmokeEmitter) return;
		vector position = owner.GetOrigin();
		float distance = vector.Distance(position, m_vAICFParticlePose);
		if (m_bAICFParticlePose && AICF_IsParticlePoseDiscontinuous(distance, timeSlice))
			AICF_ResyncParticlePose(owner, timeSlice, distance);
		m_vAICFParticlePose = position;
		m_bAICFParticlePose = true;
		if (!m_CigarSmokeEmitter || !m_MouthSmokeEmitter) return;
		super.EOnPostFixedFrame(owner, timeSlice);
	}

	protected bool AICF_IsParticlePoseDiscontinuous(float distanceMeters, float timeSlice)
	{
		return distanceMeters > Math.Max(AICF_PARTICLE_POSE_JUMP_METERS, Math.Max(0.0, timeSlice) * AICF_PARTICLE_SMOOTH_SPEED_MPS);
	}

	// После streaming/attachment скачка старое native motion history соединяет
	// несовместимые позы. Пересоздаём только собственные косметические entities,
	// оставляя нагрев/охлаждение и обычную native видимость mouth без изменений.
	protected void AICF_ResyncParticlePose(IEntity owner, float timeSlice, float distance)
	{
		bool cigarPlaying = m_CigarSmokeEmitter && m_CigarSmokeEmitter.GetState() == EParticleEffectState.PLAYING;
		bool mouthPlaying = m_MouthSmokeEmitter && m_MouthSmokeEmitter.GetState() == EParticleEffectState.PLAYING;
		delete m_CigarSmokeEmitter;
		delete m_MouthSmokeEmitter;
		super.OnAddedToParent(owner, owner.GetParent());
		ClearEventMask(owner, EntityEvent.POSTFIXEDFRAME);
		SetEventMask(owner, EntityEvent.FRAME);
		if (cigarPlaying && m_CigarSmokeEmitter) m_CigarSmokeEmitter.Play();
		if (mouthPlaying && m_MouthSmokeEmitter) m_MouthSmokeEmitter.Play();
		m_iAICFParticlePoseResyncs++;
		Print(string.Format("[AICF][CIGARETTE_POSE_RESYNC] owner=%1 parent=%2 distance_m=%3 time_slice=%4 cigar_playing=%5 mouth_playing=%6 resyncs=%7 actor_moved=0 heat_unchanged=1",
			owner.GetID(), owner.GetParent().GetID(), distance, timeSlice, cigarPlaying, mouthPlaying, m_iAICFParticlePoseResyncs));
	}

	override protected void OnRemovedFromParent(IEntity child, IEntity parent)
	{
		m_bAICFParticlePose = false;
		ClearEventMask(child, EntityEvent.FRAME | EntityEvent.POSTFIXEDFRAME);
		super.OnRemovedFromParent(child, parent);
	}

	override void OnDelete(IEntity owner)
	{
		m_bAICFParticlePose = false;
		if (owner) ClearEventMask(owner, EntityEvent.FRAME | EntityEvent.POSTFIXEDFRAME);
		super.OnDelete(owner);
	}
}
