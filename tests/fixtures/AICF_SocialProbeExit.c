// Isolated stage: наблюдаем настоящий client graph после отмены server-owned idle.
class AICF_SocialExitObservation
{
	ChimeraCharacter m_Member;
	int m_iVariant;
	int m_iAt;
	bool m_bSeen;
}

modded class AICF_MatchController
{
	protected bool m_bSocialEarlyExit;
	protected int m_iSocialEarlyAt;

	override protected void AICF_SocialFinalTick(int now)
	{
		if (!m_bSocialEarlyExit && m_iSocialLiveVariant == 1 && m_iSocialLivePhase == 2)
		{
			if (m_iSocialEarlyAt == 0) m_iSocialEarlyAt = now;
			if (now - m_iSocialEarlyAt < 500) return;
			AICF_SocialActor actor = m_SocialProbeActor;
			m_Campaign.AICF_ObserveEarlySocialExit(actor.m_Member);
			actor.m_Slot.RecordStrategicAssignment(actor.m_Slot.GetTargetBase(), "ATTACK");
			AICF_SocialCheck("EARLY_PUSHUPS_ORDER_CANCEL", actor.m_iPhase == 0 && !actor.m_bOwnsLoiter);
			m_bSocialEarlyExit = true;
			m_iSocialLivePhase = 0;
			// Даём клиенту проверить выход до следующего запуска.
			m_iSocialLiveAt = now - 28000;
			return;
		}
		super.AICF_SocialFinalTick(now);
	}
}

modded class AICF_SocialActor
{
	override void StopAnimation()
	{
		if (m_bOwnsLoiter && Controller() && Controller().IsLoitering())
		{
			SCR_GameModeCampaign campaign = SCR_GameModeCampaign.Cast(GetGame().GetGameMode());
			if (campaign) campaign.AICF_ObserveSocialExit(m_Member, m_iVariant);
		}
		super.StopAnimation();
	}
}

modded class SCR_GameModeCampaign
{
	protected ref array<ref AICF_SocialExitObservation> m_aSocialExits = {};
	protected ref array<IEntity> m_aSocialExitTargets = {};

	override void AICF_PlayGreeting(ChimeraCharacter member, int token)
	{
		// Live matrix держит каждый emote после реплики, затем отменяет приказом.
		// Предварительная server matrix сразу запускает следующий emote: её tag
		// нельзя выдавать за продолжение предыдущей анимации.
		if (!m_aSocialExitTargets.Contains(member)) m_aSocialExitTargets.Insert(member);
		super.AICF_PlayGreeting(member, token);
	}

	void AICF_ObserveSocialExit(ChimeraCharacter member, int variant)
	{
		if (!m_aSocialExitTargets.Contains(member)) return;
		m_aSocialExitTargets.RemoveItem(member);
		Rpc(AICF_ObserveSocialExit_Broadcast, member.GetRplComponent().Id(), variant);
	}

	void AICF_ObserveEarlySocialExit(ChimeraCharacter member)
	{
		Rpc(AICF_ObserveSocialExit_Broadcast, member.GetRplComponent().Id(), 4);
	}

	[RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
	protected void AICF_ObserveSocialExit_Broadcast(RplId id, int variant)
	{
		if (Replication.IsServer()) return;
		RplComponent replication = RplComponent.Cast(Replication.FindItem(id));
		if (!replication) return;
		ChimeraCharacter member = ChimeraCharacter.Cast(replication.GetEntity());
		if (!member) return;
		CharacterAnimationComponent animation = member.GetAnimationComponent();
		TAnimGraphTag tag = GameAnimationUtils.RegisterAnimationTag("IsLoitering");
		AICF_SocialExitObservation observation = new AICF_SocialExitObservation();
		observation.m_Member = member;
		observation.m_iVariant = variant;
		observation.m_iAt = System.GetTickCount();
		observation.m_bSeen = animation.IsPrimaryTag(tag) || animation.IsSecondaryTag(tag);
		m_aSocialExits.Insert(observation);
		GetGame().GetCallqueue().Remove(AICF_CheckSocialExits);
		GetGame().GetCallqueue().CallLater(AICF_CheckSocialExits, 100, true);
	}

	protected void AICF_CheckSocialExits()
	{
		for (int i = m_aSocialExits.Count() - 1; i >= 0; i--)
		{
			AICF_SocialExitObservation observation = m_aSocialExits[i];
			if (System.GetTickCount() - observation.m_iAt < 1500) continue;
			bool tagActive;
			bool commandActive;
			if (observation.m_Member)
			{
				CharacterAnimationComponent animation = observation.m_Member.GetAnimationComponent();
				TAnimGraphTag tag = GameAnimationUtils.RegisterAnimationTag("IsLoitering");
				tagActive = animation.IsPrimaryTag(tag) || animation.IsSecondaryTag(tag);
				commandActive = SCR_CharacterCommandLoiter.Cast(animation.GetCommandScripted()) != null;
			}
			Print(string.Format("[AICF][SOCIAL_EXIT] variant=%1 seen=%2 tag=%3 command=%4 pass=%5", observation.m_iVariant,
				observation.m_bSeen, tagActive, commandActive, observation.m_Member && !tagActive && !commandActive));
			m_aSocialExits.Remove(i);
		}
		if (m_aSocialExits.IsEmpty()) GetGame().GetCallqueue().Remove(AICF_CheckSocialExits);
	}

	void ~SCR_GameModeCampaign()
	{
		if (GetGame()) GetGame().GetCallqueue().Remove(AICF_CheckSocialExits);
	}
}
