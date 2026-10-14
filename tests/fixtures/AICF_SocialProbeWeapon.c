// Только isolated stage, вместе с AICF_SocialProbe.c. Проверяет фактический предмет в руках.
modded class AICF_MatchController
{
	protected int m_iWeaponCase;
	protected int m_iWeaponPhase;
	protected int m_iWeaponAt;
	protected IEntity m_WeaponExpected;
	protected ref AICF_SocialActor m_WeaponActor;

	override protected void AICF_SocialProbeTick()
	{
		string flag;
		if (!System.GetCLIParam("aicfSocialWeaponProbe", flag)) { super.AICF_SocialProbeTick(); return; }
		int now = System.GetTickCount();
		if (!m_iSocialProbeStarted)
		{
			m_iSocialProbeStarted = now;
			if (System.GetCLIParam("aicfSocialWeaponEarly", flag)) m_iWeaponCase = 6;
			m_Construction.Stop();
		}
		if (now - m_iSocialProbeStarted > 240000)
		{
			AICF_SocialCheck("WEAPON_TIMEOUT", false);
			AICF_SocialClose();
			return;
		}
		if (m_iWeaponCase >= 9)
		{
			if (m_iWeaponPhase != 90)
			{
				m_iWeaponPhase = 90;
				m_iWeaponAt = now;
				m_Campaign.AICF_WeaponClientClose();
			}
			if (now - m_iWeaponAt > 2500) AICF_SocialClose();
			return;
		}
		if (System.GetCLIParam("aicfSocialClientProbe", flag) && m_iWeaponCase == 0 && m_iWeaponPhase == 0)
		{
			array<int> players = {};
			GetGame().GetPlayerManager().GetPlayers(players);
			if (players.IsEmpty() || !AICF_GroupRuntime.IsAliveCharacter(GetGame().GetPlayerManager().GetPlayerControlledEntity(players[0]))) return;
		}
		if (m_iWeaponPhase == 0)
		{
			AICF_SocialActor candidate = m_Social.AICF_ProbeActor(m_USState.GetSlot(0));
			foreach (AICF_SocialActor initial : m_Social.AICF_ProbePool())
				if (initial.Available() && m_Social.InitialWaiting(initial.m_Slot) && SCR_Faction.GetEntityFaction(initial.m_Member) == m_USFaction) { candidate = initial; break; }
			if (!candidate || !candidate.Available() || !m_Social.InitialWaiting(candidate.m_Slot)) return;
			m_Social.AICF_ProbePause();
			m_WeaponActor = candidate;
			// AI не должен сам выбрать оружие и скрыть ошибку cleanup.
			candidate.m_Member.GetAIControlComponent().GetControlAIAgent().DeactivateAI();
			candidate.Controller().SetMovement(0, vector.Zero);
			array<IEntity> weapons = {};
			candidate.Controller().GetWeaponManagerComponent().GetWeaponsList(weapons);
			if (weapons.IsEmpty()) return;
			m_WeaponExpected = null;
			foreach (IEntity weapon : weapons)
			{
				BaseWeaponComponent component = BaseWeaponComponent.Cast(weapon.FindComponent(BaseWeaponComponent));
				if (component && (component.GetWeaponType() == EWeaponType.WT_RIFLE || component.GetWeaponType() == EWeaponType.WT_MACHINEGUN || component.GetWeaponType() == EWeaponType.WT_SNIPERRIFLE)) { m_WeaponExpected = weapon; break; }
			}
			if (!m_WeaponExpected)
			{
				AICF_SocialCheck("WEAPON_NO_FIREARM", false);
				foreach (IEntity missing : weapons) Print("[AICF][WEAPON_LIST] " + missing);
				AICF_SocialClose();
				return;
			}
			if (m_iWeaponCase == 7)
			{
				candidate.Controller().SelectWeapon(null);
				m_WeaponExpected = null;
			}
			else candidate.Controller().SelectWeapon(BaseWeaponComponent.Cast(m_WeaponExpected.FindComponent(BaseWeaponComponent)));
			m_iWeaponPhase = 1;
			m_iWeaponAt = now;
			return;
		}
		AICF_SocialActor actor = m_WeaponActor;
		SCR_CharacterControllerComponent controller = actor.Controller();
		if (m_iWeaponPhase == 1)
		{
			if (!controller.IsChangingItem() && controller.GetCurrentItemInHands() != m_WeaponExpected)
			{
				if (m_WeaponExpected) controller.TryEquipRightHandItem(m_WeaponExpected, EEquipItemType.EEquipTypeWeapon);
				else controller.TryEquipRightHandItem(null, EEquipItemType.EEquipTypeUnarmedDeliberate);
			}
			if (!controller.IsChangingItem() && controller.GetCurrentItemInHands() == m_WeaponExpected && !actor.Available())
			{
				Print(string.Format("[AICF][WEAPON_RESELECT] speed=%1 velocity=%2 gesture=%3 tag=%4 loiter=%5 stance=%6 ads=%7 using=%8", controller.GetMovementSpeed(), controller.GetVelocity(), controller.IsPlayingGesture(), actor.HasGestureAnimation(), controller.IsLoitering(), controller.IsChangingStance(), controller.IsWeaponADS(), controller.IsUsingItem()));
				m_iWeaponPhase = 0;
				return;
			}
			if (now - m_iWeaponAt > 15000)
			{
				Print(string.Format("[AICF][WEAPON_SETUP] expected=%1 actual=%2 changing=%3 available=%4 phase=%5", m_WeaponExpected, controller.GetCurrentItemInHands(), controller.IsChangingItem(), actor.Available(), actor.m_iPhase));
				AICF_SocialCheck("WEAPON_SETUP_TIMEOUT", false);
				AICF_SocialClose();
				return;
			}
			if (controller.IsChangingItem() || controller.GetCurrentItemInHands() != m_WeaponExpected || !actor.Available()) return;
			actor.m_iVariant = m_iWeaponCase % 4;
			if (!actor.BeginIdle(now)) return;
			AICF_SocialCheck("WEAPON_INITIAL_" + m_iWeaponCase, controller.GetCurrentItemInHands() == m_WeaponExpected);
			m_iWeaponPhase = 2;
			m_iWeaponAt = now;
			if (m_iWeaponCase == 6)
			{
				Print(string.Format("[AICF][WEAPON_EARLY] changing=%1 held=%2 loiter=%3", controller.IsChangingItem(), controller.GetCurrentItemInHands(), controller.IsLoitering()));
				actor.Cancel(m_Campaign, now);
				m_iWeaponPhase = 3;
			}
			return;
		}
		if (m_iWeaponPhase == 2)
		{
			m_Social.AICF_ProbeAdvance(actor, now);
			if (actor.m_iPhase != 2) return;
			if (now - m_iWeaponAt < 6000) return;
			AICF_SocialCheck("WEAPON_HOLSTERED_" + m_iWeaponCase, !controller.GetCurrentItemInHands() && controller.IsLoitering());
			if (m_iWeaponCase < 4 || m_iWeaponCase == 7)
			{
				// Истечение штатного idle deadline; не вызываем StopAnimation напрямую.
				actor.m_iDeadline = now;
				m_Social.AICF_ProbeAdvance(actor, now);
			}
			else if (m_iWeaponCase == 4)
				actor.m_Slot.RecordStrategicAssignment(actor.m_Slot.GetTargetBase(), "ATTACK");
			else if (m_iWeaponCase == 5)
			{
				SCR_AIUtilityComponent utility = SCR_AIUtilityComponent.Cast(actor.m_Member.GetAIControlComponent().GetControlAIAgent().FindComponent(SCR_AIUtilityComponent));
				utility.m_ThreatSystem.SetThreatValues(1, 1, 0, 1);
				utility.m_ThreatSystem.Update(utility, 0);
				m_Social.AICF_ProbeAdvance(actor, now);
			}
			else m_Social.Stop();
			AICF_SocialCheck("WEAPON_CANCEL_" + m_iWeaponCase, actor.m_iPhase == 0);
			m_iWeaponPhase = 3;
			m_iWeaponAt = now;
			return;
		}
		if (m_iWeaponPhase == 3 && now - m_iWeaponAt >= 4000)
		{
			bool restored = controller.GetCurrentItemInHands() == m_WeaponExpected && !controller.IsChangingItem();
			AICF_SocialCheck("WEAPON_RESTORED_" + m_iWeaponCase, restored);
			m_Campaign.AICF_WeaponObserve(actor.m_Member, m_WeaponExpected, m_iWeaponCase);
			Print(string.Format("[AICF][WEAPON_STATE] case=%1 expected=%2 actual=%3 changing=%4 loiter=%5", m_iWeaponCase, m_WeaponExpected, controller.GetCurrentItemInHands(), controller.IsChangingItem(), controller.IsLoitering()));
			actor.m_Member.GetAIControlComponent().GetControlAIAgent().ActivateAI();
			if (System.GetCLIParam("aicfSocialWeaponEarly", flag)) { AICF_SocialClose(); return; }
			m_iWeaponCase++;
			m_iWeaponPhase = 0;
		}
	}
}

modded class SCR_GameModeCampaign
{
	void AICF_WeaponObserve(ChimeraCharacter member, IEntity weapon, int index)
	{
		RplId weaponId = RplId.Invalid();
		if (weapon) weaponId = RplComponent.Cast(weapon.FindComponent(RplComponent)).Id();
		Rpc(AICF_WeaponObserve_Broadcast, member.GetRplComponent().Id(), weaponId, index);
	}

	[RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
	protected void AICF_WeaponObserve_Broadcast(RplId memberId, RplId weaponId, int index)
	{
		if (Replication.IsServer()) return;
		RplComponent replication = RplComponent.Cast(Replication.FindItem(memberId));
		ChimeraCharacter member;
		if (replication) member = ChimeraCharacter.Cast(replication.GetEntity());
		IEntity held;
		if (member) held = member.GetCharacterController().GetCurrentItemInHands();
		RplId heldId = RplId.Invalid();
		if (held) heldId = RplComponent.Cast(held.FindComponent(RplComponent)).Id();
		Print(string.Format("[AICF][WEAPON_CLIENT] case=%1 pass=%2", index, member && heldId == weaponId && !member.GetCharacterController().IsChangingItem()));
	}

	void AICF_WeaponClientClose() { Rpc(AICF_WeaponClientClose_Broadcast); }

	[RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
	protected void AICF_WeaponClientClose_Broadcast()
	{
		if (!Replication.IsServer()) GetGame().RequestClose();
	}
}
