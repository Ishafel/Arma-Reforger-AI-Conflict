class AICF_SocialActor
{
	AICF_GroupSlot m_Slot;
	SCR_AIGroup m_Group;
	ChimeraCharacter m_Member;
	EntityID m_MemberId;
	int m_iGeneration;
	int m_iAssignment;
	int m_iIntent;
	int m_iVariant;
	int m_iDueAt;
	int m_iDeadline;
	int m_iPhase; // 0 waiting, 1 idle requested, 2 idle
	int m_iGreetingDeadline;
	int m_iSaluteBlockedUntil;
	int m_iToken;
	int m_iPlayerId;
	IEntity m_PlayerEntity;
	AICF_GreetingPlayer m_Player;
	bool m_bSeen;
	bool m_bOwnsLoiter;
	bool m_bOwnsGesture;
	bool m_bPreparingStance;
	TAnimGraphTag m_GestureTag;
	ref SCR_LoiterCustomAnimData m_LoiterData;
	IEntity m_HolsteredWeapon;
	EntityID m_HolsteredWeaponId;

	void AICF_SocialActor(AICF_GroupSlot slot, ChimeraCharacter member, int seed, int index, int now)
	{
		m_Slot = slot;
		m_Group = slot.GetGroup();
		m_Member = member;
		m_MemberId = member.GetID();
		m_iGeneration = slot.GetSpawnGeneration();
		m_GestureTag = GameAnimationUtils.RegisterAnimationTag("TagCharacterGesture");
		m_iVariant = AICF_SocialConfig.Variant(seed, index);
		m_iDueAt = now + AICF_SocialConfig.Delay(seed, index);
		SnapshotOrder();
	}

	void SnapshotOrder()
	{
		m_iAssignment = m_Slot.GetStrategicAssignmentRevision();
		m_iIntent = m_Slot.GetStrategicIntentRevision();
	}

	bool HasIdentity()
	{
		if (!m_Slot || !m_Group || !m_Member || m_Member.GetID() != m_MemberId ||
			m_Slot.GetGroup() != m_Group || m_Slot.GetSpawnGeneration() != m_iGeneration)
			return false;
		AIControlComponent control = m_Member.GetAIControlComponent();
		return control && control.GetControlAIAgent() && control.GetControlAIAgent().GetParentGroup() == m_Group;
	}

	bool SameOrder()
	{
		return m_Slot && m_iAssignment == m_Slot.GetStrategicAssignmentRevision() &&
			m_iIntent == m_Slot.GetStrategicIntentRevision();
	}

	SCR_CharacterControllerComponent Controller()
	{
		if (!m_Member || m_Member.GetID() != m_MemberId) return null;
		return SCR_CharacterControllerComponent.Cast(m_Member.GetCharacterController());
	}

	bool HasGestureAnimation()
	{
		SCR_CharacterControllerComponent controller = Controller();
		if (!controller) return false;
		CharacterAnimationComponent animation = controller.GetAnimationComponent();
		return animation.IsPrimaryTag(m_GestureTag) || animation.IsSecondaryTag(m_GestureTag);
	}

	bool BeginSalute()
	{
		SCR_CharacterControllerComponent controller = Controller();
		if (System.GetTickCount() < m_iSaluteBlockedUntil) return false;
		if (!controller || m_iPhase != 0 || controller.IsLoitering() || controller.IsPlayingGesture() ||
			HasGestureAnimation() || controller.IsChangingItem() || controller.IsChangingStance() ||
			controller.GetStance() != ECharacterStance.STAND || !controller.CanPlayItemGesture()) return false;
		if (!controller.TryStartCharacterGesture(ECharacterGestures.SALUTE, AICF_SocialConfig.SALUTE_MS)) return false;
		m_bOwnsGesture = true;
		return true;
	}

	bool Available()
	{
		if (!Replication.IsServer() || !HasIdentity() || !m_Slot.IsCombatReady() ||
			m_Slot.GetUnitType() != AICF_EGroupUnitType.INFANTRY || !m_Slot.GetWaypoint() ||
			m_Group.GetCurrentWaypoint() != m_Slot.GetWaypoint())
			return false;
		SCR_CharacterControllerComponent controller = Controller();
		if (!controller || controller.GetLifeState() != ECharacterLifeState.ALIVE ||
			controller.AICF_HasSocialWeaponRecovery() ||
			GetGame().GetPlayerManager().GetPlayerIdFromControlledEntity(m_Member) > 0 ||
			controller.IsFalling() || controller.IsClimbing() || controller.IsSwimming() ||
			controller.IsUsingItem() || controller.IsWeaponADS() || controller.GetVelocity().Length() > 0.1 ||
			controller.GetMovementSpeed() > 0.1)
			return false;
		CompartmentAccessComponent access = m_Member.GetCompartmentAccessComponent();
		if (!access || access.IsInCompartment() || access.IsGettingIn() || access.IsGettingOut()) return false;
		SCR_AIUtilityComponent utility = SCR_AIUtilityComponent.Cast(m_Member.GetAIControlComponent().GetControlAIAgent().FindComponent(SCR_AIUtilityComponent));
		if (!utility || !utility.m_ThreatSystem || utility.m_ThreatSystem.GetState() != EAIThreatState.SAFE ||
			utility.m_ThreatSystem.GetThreatMeasure() > 0.01) return false;
		// Whitelist: лечение, посадка, перезарядка, бой и движение не вытесняются.
		SCR_AIBehaviorBase behavior = utility.GetCurrentBehavior();
		SCR_AIDefendBehavior defend = SCR_AIDefendBehavior.Cast(behavior);
		if (!SCR_AIIdleBehavior.Cast(behavior))
		{
			if (!defend || defend.m_RelatedWaypoint.m_Value != m_Slot.GetWaypoint()) return false;
		}
		if (!m_bOwnsLoiter && controller.IsLoitering()) return false;
		if (!m_bOwnsGesture && (controller.IsPlayingGesture() || HasGestureAnimation())) return false;
		if (m_iPhase == 0 && (controller.IsChangingItem() || controller.IsChangingStance())) return false;
		return true;
	}

	ELoiteringType LoiterType(int variant)
	{
		if (variant == 0) return ELoiteringType.SMOKING;
		if (variant == 1) return ELoiteringType.PUSHUPS;
		if (variant == 2) return ELoiteringType.CUSTOM;
		return ELoiteringType.SIT;
	}

	bool BeginIdle(int now)
	{
		if (!Available() || m_iPhase != 0) return false;
		SCR_CharacterControllerComponent controller = Controller();
		if (controller.GetStance() != ECharacterStance.STAND)
		{
			m_bPreparingStance = true;
			m_iPhase = 1;
			m_iDeadline = now + AICF_SocialConfig.PREPARE_MS;
			SnapshotOrder();
			controller.SetStanceChange(1); // native STANCECHANGE_TOERECTED
			return true;
		}
		ELoiteringType type = LoiterType(m_iVariant);
		if (!controller.CanPlayLoiterAnimation(type))
		{
			m_iVariant = 0;
			type = LoiterType(m_iVariant);
			if (!controller.CanPlayLoiterAnimation(type)) return false;
		}
		SCR_LoiterCustomAnimData data = SCR_LoiterCustomAnimData.CreateInstance();
		if (m_iVariant == 0) data = SCR_LoiterCustomAnimData.CreateInstance(itemPreset: SCR_ELoiterItemID.CIGAR);
		if (m_iVariant == 2) data = SCR_LoiterCustomAnimData.CreateInstance(itemPreset: SCR_ELoiterItemID.CHAIR);
		m_LoiterData = data;
		m_bOwnsLoiter = true;
		m_iPhase = 1;
		m_iDeadline = now + AICF_SocialConfig.PREPARE_MS;
		SnapshotOrder();
		// Native contextual holster сохраняет оружие и возвращает его после loiter.
		m_HolsteredWeapon = controller.GetCurrentItemInHands();
		if (m_HolsteredWeapon) m_HolsteredWeaponId = m_HolsteredWeapon.GetID();
		controller.StartLoitering(null, type, true, true, false, customAnimData: data);
		Log("IDLE_REQUESTED", now);
		return true;
	}

	void StopAnimation()
	{
		// Native loiter command заканчивается раньше визуального out-перехода на proxy.
		if (m_bOwnsLoiter) m_iSaluteBlockedUntil = System.GetTickCount() + AICF_SocialConfig.PREPARE_MS;
		SCR_CharacterControllerComponent controller = Controller();
		if (controller)
		{
			if (m_bOwnsLoiter && controller.GetScrInputContext().m_CustomAnimData == m_LoiterData)
			{
				controller.AICF_StopSocialLoiter(m_LoiterData, this);
			}
			if (m_bOwnsGesture)
			{
				controller.StopCharacterGesture();
			}
		}
		m_bOwnsLoiter = false;
		m_LoiterData = null;
		m_HolsteredWeapon = null;
		m_bOwnsGesture = false;
		m_bPreparingStance = false;
	}

	void Cancel(SCR_GameModeCampaign campaign, int now)
	{
		StopAnimation();
		FinishGreeting(campaign);
		if (m_iPhase != 0) Log("SOCIAL_STOPPED", now);
		m_iPhase = 0;
		m_iDueAt = now + AICF_SocialConfig.Delay(m_iGeneration, m_Slot.GetSlotId());
	}

	void FinishGreeting(SCR_GameModeCampaign campaign)
	{
		// Завершение реплики не трогает текущий idle emote.
		if (m_bOwnsGesture && Controller()) Controller().StopCharacterGesture();
		m_bOwnsGesture = false;
		if (m_Player) m_Player.Release(m_iToken);
		if (m_iToken > 0 && campaign) campaign.AICF_StopGreeting(m_iToken);
		m_iToken = 0;
		m_Player = null;
		m_PlayerEntity = null;
	}

	void Log(string eventName, int now)
	{
		AICF_Stage1Diagnostics.Info(eventName, string.Format(
			"slot=%1 generation=%2 member=%3 variant=%4 token=%5 phase=%6 now_ms=%7",
			m_Slot.GetStableSlotKey(), m_iGeneration, m_MemberId, m_iVariant, m_iToken, m_iPhase, now));
	}
}
