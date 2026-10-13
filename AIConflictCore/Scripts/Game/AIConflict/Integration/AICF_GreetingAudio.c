class AICF_GreetingSound
{
	int m_iToken;
	int m_iExpires;
	IEntity m_Entity;
	AudioHandle m_Handle = AudioHandle.Invalid;
}

// Существующий replicated game mode доставляет только новые события: JIP snapshot не содержит реплик.
modded class SCR_GameModeCampaign
{
	protected int m_iAICFLastGreetingToken;
	protected ref array<ref AICF_GreetingSound> m_aAICFGreetingSounds = {};

	void AICF_PlayGreeting(ChimeraCharacter member, int token)
	{
		if (!Replication.IsServer() || !IsMaster() || !member || token <= 0) return;
		RplComponent replication = member.GetRplComponent();
		if (!replication) return;
		Rpc(AICF_Greeting_Broadcast, replication.Id(), token);
		if (!System.IsConsoleApp()) AICF_Greeting_Broadcast(replication.Id(), token);
	}

	[RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
	protected void AICF_Greeting_Broadcast(RplId id, int token)
	{
		if (token <= m_iAICFLastGreetingToken || !IsRunning()) return;
		m_iAICFLastGreetingToken = token;
		if (System.IsConsoleApp()) return;
		RplComponent replication = RplComponent.Cast(Replication.FindItem(id));
		if (!replication) return; // Не задерживаем звук до появления далёкой streamed entity.
		IEntity member = replication.GetEntity();
		if (!AICF_GroupRuntime.IsAliveCharacter(member)) return;
		vector transform[4];
		member.GetWorldTransform(transform);
		transform[3] = transform[3] + "0 1.6 0";
		AudioSystem.PlayEventInitialize(AICF_SocialConfig.GREETING_PROJECT);
		AudioHandle handle = AudioSystem.PlayEvent(AICF_SocialConfig.GREETING_PROJECT, AICF_SocialConfig.GREETING_EVENT, transform);
		Print(string.Format("[AICF][GREETING_AUDIO] token=%1 member=%2 handle=%3 started=%4", token, id, handle, handle != AudioHandle.Invalid));
		if (handle == AudioHandle.Invalid) return;
		AICF_GreetingSound sound = new AICF_GreetingSound();
		sound.m_iToken = token;
		sound.m_Entity = member;
		sound.m_Handle = handle;
		sound.m_iExpires = System.GetTickCount() + AICF_SocialConfig.SALUTE_MS;
		m_aAICFGreetingSounds.Insert(sound);
		GetGame().GetCallqueue().Remove(AICF_UpdateGreetingSounds);
		GetGame().GetCallqueue().CallLater(AICF_UpdateGreetingSounds, AICF_SocialConfig.TICK_MS, true);
	}

	void AICF_StopGreeting(int token)
	{
		if (!Replication.IsServer() || !IsMaster()) return;
		Rpc(AICF_StopGreeting_Broadcast, token);
		AICF_StopGreeting_Broadcast(token);
	}

	protected void AICF_ReleaseGreetingSound(AICF_GreetingSound sound)
	{
		if (sound.m_Handle != AudioHandle.Invalid) AudioSystem.TerminateSound(sound.m_Handle);
	}

	[RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
	protected void AICF_StopGreeting_Broadcast(int token)
	{
		for (int i = m_aAICFGreetingSounds.Count() - 1; i >= 0; i--)
		{
			AICF_GreetingSound sound = m_aAICFGreetingSounds[i];
			if (token > 0 && sound.m_iToken != token) continue;
			AICF_ReleaseGreetingSound(sound);
			m_aAICFGreetingSounds.Remove(i);
		}
		if (m_aAICFGreetingSounds.IsEmpty()) GetGame().GetCallqueue().Remove(AICF_UpdateGreetingSounds);
	}

	protected void AICF_UpdateGreetingSounds()
	{
		for (int i = m_aAICFGreetingSounds.Count() - 1; i >= 0; i--)
		{
			AICF_GreetingSound sound = m_aAICFGreetingSounds[i];
			if (!IsRunning() || !AICF_GroupRuntime.IsAliveCharacter(sound.m_Entity) ||
				System.GetTickCount() >= sound.m_iExpires)
			{
				AICF_ReleaseGreetingSound(sound);
				m_aAICFGreetingSounds.Remove(i);
				continue;
			}
			vector transform[4];
			sound.m_Entity.GetWorldTransform(transform);
			transform[3] = transform[3] + "0 1.6 0";
			AudioSystem.SetSoundTransformation(sound.m_Handle, transform);
		}
		if (m_aAICFGreetingSounds.IsEmpty()) GetGame().GetCallqueue().Remove(AICF_UpdateGreetingSounds);
	}

	override void OnGameEnd()
	{
		AICF_StopGreeting_Broadcast(0);
		super.OnGameEnd();
	}

	void ~SCR_GameModeCampaign()
	{
		if (GetGame()) AICF_StopGreeting_Broadcast(0);
	}
}
