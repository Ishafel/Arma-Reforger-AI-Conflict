// Только isolated stage. Native анимации на реальных initial roster; управляемые часы gate.
modded class AICF_SocialService
{
	protected ref array<ref AICF_SocialActor> m_aSocialProbePool = {};

	AICF_SocialActor AICF_ProbeActor(AICF_GroupSlot slot)
	{
		Faction faction = SCR_Faction.GetEntityFaction(AICF_GroupRuntime.ResolveAliveLeader(slot.GetGroup()));
		foreach (AICF_SocialActor pooled : m_aSocialProbePool)
			if (SCR_Faction.GetEntityFaction(pooled.m_Member) == faction && pooled.Available()) return pooled;
		foreach (AICF_SocialActor available : m_aActors)
			if (SCR_Faction.GetEntityFaction(available.m_Member) == faction && available.Available()) return available;
		foreach (AICF_SocialActor actor : m_aActors)
			if (actor.m_Slot == slot && actor.HasIdentity()) return actor;
		return null;
	}

	void AICF_ProbePause()
	{
		GetGame().GetCallqueue().Remove(Tick);
		foreach (AICF_SocialActor actor : m_aActors) actor.Cancel(m_Campaign, System.GetTickCount());
		foreach (AICF_SocialActor pooled : m_aActors) m_aSocialProbePool.Insert(pooled);
	}

	void AICF_ProbeAdvance(AICF_SocialActor actor, int now)
	{
		Advance(actor, now);
	}

	void AICF_ProbeObserve(int now)
	{
		ObservePlayers(now);
	}

	AICF_GreetingGate AICF_ProbeGate()
	{
		return m_Gate;
	}

	void AICF_ProbeOnly(AICF_SocialActor selected)
	{
		foreach (AICF_SocialActor actor : m_aActors)
			if (actor != selected) actor.Cancel(m_Campaign, System.GetTickCount());
		m_aActors.Clear();
		m_aActors.Insert(selected);
	}

	array<ref AICF_SocialActor> AICF_ProbePool() { return m_aSocialProbePool; }
	void AICF_ProbePair(AICF_SocialActor first, AICF_SocialActor second)
	{
		m_aActors.Clear();
		m_aActors.Insert(first);
		m_aActors.Insert(second);
	}
	bool AICF_ProbeStopped() { return m_bStopped && m_aActors.IsEmpty() && !m_Gate; }
}

modded class AICF_MatchController
{
	protected int m_iSocialProbeStarted;
	protected int m_iSocialProbePassed;
	protected int m_iSocialProbeFailed;
	protected int m_iSocialProbeVariant;
	protected int m_iSocialProbeAt;
	protected ref AICF_SocialActor m_SocialProbeActor;
	protected bool m_bSocialProbeRequested;
	protected bool m_bSocialProbeSeen;
	protected bool m_bSocialProbeLoop;
	protected AICF_GreetingPlayer m_SocialRespawnLedger;
	protected IEntity m_SocialPreviousPlayer;

	protected AICF_GroupSlot AICF_SocialTargetSlot()
	{
		string side;
		if (System.GetCLIParam("aicfSocialFaction", side) && side == "USSR") return m_USSRState.GetSlot(0);
		return m_USState.GetSlot(0);
	}

	override protected void Update()
	{
		string flag;
		if (!System.GetCLIParam("aicfSocialProbe", flag) || flag != "1" || !m_bRosterReady)
		{
			super.Update();
			return;
		}
		if (!m_bStopped && !m_bSocialProbeLoop)
		{
			m_bSocialProbeLoop = true;
			GetGame().GetCallqueue().CallLater(AICF_SocialProbeTick, AICF_SocialConfig.TICK_MS, true);
		}
	}

	protected void AICF_SocialCheck(string name, bool pass)
	{
		if (pass) m_iSocialProbePassed++;
		else m_iSocialProbeFailed++;
		Print(string.Format("[AICF][SOCIAL_PROBE] case=%1 pass=%2", name, pass));
	}

	protected void AICF_SocialGateChecks()
	{
		AICF_GreetingGate gate = new AICF_GreetingGate();
		AICF_GreetingPlayer player = gate.Player("human-a");
		AICF_SocialCheck("FIRST_APPROACH", player.Observe(5) && player.Reserve(1000, 1));
		AICF_SocialCheck("CONCURRENT_RESERVATION", !player.Reserve(1000, 2));
		AICF_SocialCheck("COMMIT", player.Commit(1, 1000));
		player.Observe(8);
		AICF_SocialCheck("COOLDOWN_29999", player.Observe(5) && !player.Reserve(30999, 2));
		AICF_SocialCheck("STAY_NO_REPEAT", !player.Observe(5) && !player.Observe(6) && !player.Observe(5));
		player.Observe(8);
		AICF_SocialCheck("COOLDOWN_30000", player.Observe(5) && player.Reserve(31000, 3));
		AICF_SocialCheck("INDEPENDENT_PLAYERS", gate.Player("human-b").Reserve(1000, 4));
		player.Release(2);
		AICF_SocialCheck("STALE_RELEASE", !player.Reserve(31000, 5));
		player.Release(3);
		AICF_SocialCheck("FAILED_BEFORE_START", player.Reserve(31000, 5) && player.m_iNextAt == 31000);
		player.Commit(5, 31000);
		AICF_SocialCheck("RESPAWN_SAME_LEDGER", gate.Player("human-a") == player && !gate.Player("human-a").Reserve(32000, 6));
		AICF_SocialCheck("TOKEN_MONOTONIC", gate.NextToken() == 1 && gate.NextToken() == 2);
		AICF_GreetingPlayer edge = gate.Player("edge");
		AICF_SocialCheck("OUTSIDE_RADIUS", !edge.Observe(5.01));
		AICF_SocialCheck("INNER_BOUNDARY", edge.Observe(5));
		AICF_SocialCheck("OUTER_BOUNDARY_NO_REARM", !edge.Observe(7) && !edge.Observe(5));
		AICF_SocialCheck("OUTER_CROSS_REARMS", !edge.Observe(7.01) && edge.Observe(5));
		AICF_SocialCheck("FAILED_COMMIT", !edge.Commit(88, 12345) && edge.m_iNextAt == 0);
		array<int> variants = {};
		array<int> delays = {};
		for (int i; i < 4; i++)
		{
			variants.Insert(AICF_SocialConfig.Variant(42, i));
			delays.Insert(AICF_SocialConfig.Delay(42, i));
		}
		AICF_SocialCheck("SPREAD", variants[0] != variants[1] && variants[1] != variants[2] && variants[2] != variants[3] && delays[0] != delays[1] && delays[1] != delays[2]);
		AICF_GroupSlot unready = new AICF_GroupSlot(0, AICF_EGroupRole.ATTACK, 0);
		AICF_SocialCheck("READINESS", !m_Social.InitialWaiting(unready));
	}

	protected void AICF_SocialProbeTick()
	{
		if (m_bStopped) { GetGame().GetCallqueue().Remove(AICF_SocialProbeTick); return; }
		int now = System.GetTickCount();
		if (!m_iSocialProbeStarted)
		{
			m_iSocialProbeStarted = now;
			AICF_SocialGateChecks();
			m_Construction.Stop();
			AICF_SocialCheck("US_INITIAL_WAIT", m_Social.InitialWaiting(m_USState.GetSlot(0)));
			AICF_SocialCheck("USSR_INITIAL_WAIT", m_Social.InitialWaiting(m_USSRState.GetSlot(0)));
		}
		if (m_iSocialProbeVariant >= 4)
		{
			AICF_SocialFinalTick(now);
			return;
		}
		if (now - m_iSocialProbeStarted > 150000)
		{
			AICF_SocialCheck("TIMEOUT", false);
			AICF_SocialClose();
			return;
		}
		if (!m_SocialProbeActor)
		{
			m_SocialProbeActor = m_Social.AICF_ProbeActor(AICF_SocialTargetSlot());
			if (!m_SocialProbeActor) return;
			m_Social.AICF_ProbePause();
			m_SocialProbeActor.Cancel(m_Campaign, now);
			m_iSocialProbeAt = now;
		}
		AICF_SocialActor actor = m_SocialProbeActor;
		if (!m_bSocialProbeRequested && !actor.Available())
		{
			AICF_SocialActor other = m_Social.AICF_ProbeActor(AICF_SocialTargetSlot());
			if (other && other.Available())
			{
				m_SocialProbeActor = other;
				actor = other;
			}
		}
		SCR_CharacterControllerComponent controller = actor.Controller();
		if (!m_bSocialProbeRequested)
		{
			if (!actor.Available())
			{
				SCR_AIUtilityComponent utility = SCR_AIUtilityComponent.Cast(actor.m_Member.GetAIControlComponent().GetControlAIAgent().FindComponent(SCR_AIUtilityComponent));
				string behavior;
				if (utility && utility.GetCurrentBehavior()) behavior = utility.GetCurrentBehavior().Type().ToString();
				SCR_AIDefendBehavior defend = SCR_AIDefendBehavior.Cast(utility.GetCurrentBehavior());
				float distance = -1;
				bool related;
				if (defend)
				{
					distance = vector.DistanceSqXZ(actor.m_Member.GetOrigin(), defend.m_fDefendLocation.m_Value);
					related = defend.m_RelatedWaypoint.m_Value == actor.m_Slot.GetWaypoint();
				}
				Print(string.Format("[AICF][SOCIAL_DIAG] velocity=%1 threat=%2 measure=%3 distanceSq=%4 related=%5 wp=%6 stance=%7", controller.GetVelocity(), utility.m_ThreatSystem.GetState(), utility.m_ThreatSystem.GetThreatMeasure(), distance, related, actor.m_Group.GetCurrentWaypoint() == actor.m_Slot.GetWaypoint(), controller.GetStance()));
				Print(string.Format("[AICF][SOCIAL_DIAG2] identity=%1 ready=%2 type=%3 player=%4 using=%5 ads=%6 item=%7 stance_change=%8 falling=%9", actor.HasIdentity(), actor.m_Slot.IsCombatReady(), actor.m_Slot.GetUnitType(), GetGame().GetPlayerManager().GetPlayerIdFromControlledEntity(actor.m_Member), controller.IsUsingItem(), controller.IsWeaponADS(), controller.IsChangingItem(), controller.IsChangingStance(), controller.IsFalling()));
				Print(string.Format("[AICF][SOCIAL_WAIT] posture=%1 behavior=%2 speed=%3 loiter=%4 gesture=%5 phase=%6", actor.m_Slot.GetOperationalPosture(), behavior, controller.GetMovementSpeed(), controller.IsLoitering(), controller.IsPlayingGesture(), actor.m_iPhase));
				return;
			}
			actor.m_iVariant = m_iSocialProbeVariant;
			m_bSocialProbeRequested = actor.BeginIdle(now);
			m_iSocialProbeAt = now;
			return;
		}
		m_Social.AICF_ProbeAdvance(actor, now);
		if (actor.m_iPhase == 2 && !m_bSocialProbeSeen)
		{
			AICF_SocialCheck("NO_RESTART_" + m_iSocialProbeVariant, !actor.BeginIdle(now));
			m_bSocialProbeSeen = true;
		}
		if (!m_bSocialProbeSeen)
		{
			if (actor.m_iPhase == 0) m_bSocialProbeRequested = false;
			return;
		}
		if (now - m_iSocialProbeAt < 7000) return;
		AICF_SocialCheck("NATIVE_VARIANT_" + m_iSocialProbeVariant, m_bSocialProbeSeen);
		actor.Cancel(m_Campaign, now);
		AICF_SocialCheck("OUT_TRANSITION_" + m_iSocialProbeVariant, !actor.BeginSalute());
		m_bSocialProbeRequested = false;
		m_bSocialProbeSeen = false;
		m_iSocialProbeVariant++;
	}


	protected int m_iSocialLivePhase;
	protected int m_iSocialLiveVariant;
	protected int m_iSocialLiveAt;
	protected int m_iSocialLiveToken;
	protected bool m_bSocialLiveDisconnected;
	protected bool m_bSocialLiveJIP;
	protected bool m_bSocialSalutePriority;
	protected IEntity m_SocialLivePlayer;
	protected int m_iSocialLivePlayerId;

	protected void AICF_SocialFinalTick(int now)
	{
		string flag;
		if (!System.GetCLIParam("aicfSocialClientProbe", flag) || flag != "1")
		{
			AICF_SocialPolicyTick(now);
			return;
		}
		if (now - m_iSocialProbeStarted > 480000)
		{
			AICF_SocialCheck("CLIENT_TIMEOUT", false);
			AICF_SocialClose();
			return;
		}
		PlayerManager manager = GetGame().GetPlayerManager();
		array<int> ids = {};
		manager.GetPlayers(ids);
		if (m_iSocialLiveVariant >= 5)
		{
			if (ids.IsEmpty()) m_bSocialLiveDisconnected = true;
			if (m_bSocialLiveDisconnected && !ids.IsEmpty()) m_bSocialLiveJIP = true;
			if (m_bSocialLiveJIP && ids.IsEmpty()) AICF_SocialClose();
			return;
		}
		if (!m_SocialLivePlayer)
		{
			foreach (int id : ids)
			{
				IEntity entity = manager.GetPlayerControlledEntity(id);
				if (!AICF_GroupRuntime.IsAliveCharacter(entity)) continue;
				m_SocialLivePlayer = entity;
				m_iSocialLivePlayerId = id;
				if (m_SocialRespawnLedger)
				{
					AICF_GreetingPlayer after = m_Social.AICF_ProbeGate().Resolve(id, manager.GetPlayerController(id), GetGame().GetBackendApi().GetPlayerIdentityId(id));
					AICF_SocialCheck("REAL_RESPAWN_COOLDOWN", entity != m_SocialPreviousPlayer && after == m_SocialRespawnLedger && !after.Reserve(now, 9999));
					m_SocialRespawnLedger = null;
					return;
				}
				m_iSocialLiveAt = now;
				AICF_SocialCheck("LIVE_PLAYER", true);
				AICF_GreetingGate gate = new AICF_GreetingGate();
				PlayerController pc = manager.GetPlayerController(id);
				AICF_GreetingPlayer a = gate.Resolve(101, pc, "stable-a");
				a.Reserve(1000, 1);
				a.Commit(1, 1000);
				gate.Disconnect(101);
				AICF_SocialCheck("RECONNECT_STABLE", gate.Resolve(102, pc, "stable-a") == a && !a.Reserve(2000, 2));
				AICF_SocialCheck("REUSED_NUMERIC_ID", gate.Resolve(101, pc, "stable-b") != a && gate.Resolve(101, pc, "stable-b").Reserve(2000, 3));
				AICF_GreetingPlayer fallback = gate.Resolve(103, pc, "");
				fallback.Reserve(1000, 4);
				fallback.Commit(4, 1000);
				AICF_SocialCheck("LATE_BACKEND_IDENTITY", !gate.Resolve(103, pc, "stable-c").Reserve(2000, 5));
				break;
			}
			return;
		}
		AICF_SocialActor actor = m_SocialProbeActor;
		if (m_iSocialLivePhase == 0)
		{
			if (!actor.Available())
			{
				AICF_SocialActor available = m_Social.AICF_ProbeActor(AICF_SocialTargetSlot());
				if (available && available.Available()) { actor = available; m_SocialProbeActor = available; }
			}
			m_Social.AICF_ProbeOnly(actor);
			m_Campaign.AICF_SocialPlace(m_SocialLivePlayer, actor.m_Member.GetOrigin() + "20 0 0");
			m_Social.AICF_ProbeObserve(now);
			if (m_iSocialLiveVariant > 0 && now - m_iSocialLiveAt < 31000) return;
			if (!actor.Available()) return;
			if (m_iSocialLiveVariant == 4)
			{
				actor.m_iDueAt = now + 60000;
				m_Social.AICF_ProbeOnly(actor);
				m_Campaign.AICF_SocialPlace(m_SocialLivePlayer, actor.m_Member.GetOrigin() + "2 0 0");
				m_iSocialLivePhase = 2;
				return;
			}
			actor.m_iVariant = m_iSocialLiveVariant;
			if (!actor.BeginIdle(now)) return;
			m_Social.AICF_ProbeOnly(actor);
			m_iSocialLivePhase = 1;
			return;
		}
		m_Social.AICF_ProbeAdvance(actor, now);
		if (m_iSocialLivePhase == 1 && actor.m_iPhase == 2)
		{
			m_Campaign.AICF_SocialPlace(m_SocialLivePlayer, actor.m_Member.GetOrigin() + "2 0 0");
			m_iSocialLivePhase = 2;
			return;
		}
		if (m_iSocialLivePhase == 2)
		{
			// После hard exit native AI может уйти к своему waypoint между ticks.
			// Свободный SALUTE проверяем на новом допустимом бойце, не ждём ушедшего.
			if (m_iSocialLiveVariant == 4 && (!actor.Available() || vector.Distance(actor.m_Member.GetOrigin(), m_SocialLivePlayer.GetOrigin()) > 5))
			{
				m_iSocialLivePhase = 0;
				return;
			}
			if (actor.m_iPhase == 0 && m_iSocialLiveVariant < 4) { m_iSocialLivePhase = 0; return; }
			m_Social.AICF_ProbeObserve(now);
			Print(string.Format("[AICF][SOCIAL_LIVE_WAIT] phase=%1 actor_phase=%2 token=%3 available=%4 stance=%5 loiter=%6 item=%7 distance=%8", m_iSocialLivePhase, actor.m_iPhase, actor.m_iToken, actor.Available(), actor.Controller().GetStance(), actor.Controller().IsLoitering(), actor.Controller().IsChangingItem(), vector.Distance(actor.m_Member.GetOrigin(), m_SocialLivePlayer.GetOrigin())));
			if (actor.m_iToken == 0) return;
			m_iSocialLiveToken = actor.m_iToken;
			if (m_iSocialLiveVariant < 4)
				AICF_SocialCheck("VOICE_ONLY_" + m_iSocialLiveVariant, !actor.m_bOwnsGesture && actor.m_iPhase == 2 && actor.Controller().IsLoitering());
			else
				AICF_SocialCheck("LIVE_SALUTE_FREE", actor.m_bOwnsGesture && !actor.Controller().IsLoitering());
			m_Campaign.AICF_SocialDuplicate(actor.m_Member, actor.m_iToken);
			m_iSocialLiveAt = now;
			m_iSocialLivePhase = 3;
			return;
		}
		if (m_iSocialLivePhase == 3 && m_iSocialLiveVariant == 4 && !m_bSocialSalutePriority && now - m_iSocialLiveAt > 2500)
		{
			m_bSocialSalutePriority = true;
			AIWaypoint waypoint = actor.m_Slot.GetWaypoint();
			actor.m_Slot.RecordStrategicAssignment(actor.m_Slot.GetTargetBase(), "ATTACK");
			AICF_SocialCheck("SALUTE_ORDER_CANCEL", !actor.m_bOwnsGesture && actor.m_iToken == 0 && actor.m_Slot.GetWaypoint() == waypoint);
		}
		if (m_iSocialLivePhase == 3 && now - m_iSocialLiveAt > 5000)
		{
			AICF_SocialCheck("GREETING_BOUNDED_" + m_iSocialLiveVariant, actor.m_iToken == 0 && !actor.m_bOwnsGesture);
			if (m_iSocialLiveVariant < 4)
			{
				AICF_SocialCheck("IDLE_CONTINUES_" + m_iSocialLiveVariant, actor.m_iPhase == 2 && actor.Controller().IsLoitering());
				AIWaypoint unchanged = actor.m_Slot.GetWaypoint();
				actor.m_Slot.RecordStrategicAssignment(actor.m_Slot.GetTargetBase(), "ATTACK");
				AICF_SocialCheck("IDLE_ORDER_CANCEL_" + m_iSocialLiveVariant, actor.m_iPhase == 0 && !actor.m_bOwnsLoiter && actor.m_Slot.GetWaypoint() == unchanged);
			}
			actor.Cancel(m_Campaign, now);
			if (m_iSocialLiveVariant == 0)
			{
				m_SocialRespawnLedger = m_Social.AICF_ProbeGate().Resolve(m_iSocialLivePlayerId, manager.GetPlayerController(m_iSocialLivePlayerId), GetGame().GetBackendApi().GetPlayerIdentityId(m_iSocialLivePlayerId));
				m_SocialPreviousPlayer = m_SocialLivePlayer;
				SCR_CharacterDamageManagerComponent damage = SCR_CharacterDamageManagerComponent.Cast(m_SocialLivePlayer.FindComponent(SCR_CharacterDamageManagerComponent));
				damage.EnableDamageHandling(true);
				damage.Kill(Instigator.CreateInstigatorGM());
				m_SocialLivePlayer = null;
			}
			m_iSocialLiveVariant++;
			m_iSocialLivePhase = 0;
			Print(string.Format("[AICF][SOCIAL_LIVE] completed=%1 token=%2 awaiting_jip=%3", m_iSocialLiveVariant, m_iSocialLiveToken, m_iSocialLiveVariant == 5));
		}
	}


	protected int m_iSocialPolicyPhase;
	protected ref AICF_SocialActor m_SocialPolicyActor;
	protected int m_iSocialPolicyGeneration;
	protected SCR_CampaignBuildingCompositionComponent m_SocialPolicyBuilding;

	protected void AICF_SocialPrepareReplacement()
	{
		SCR_CampaignMilitaryBaseComponent base = m_USFaction.GetMainBase();
		vector position = base.GetSpawnPoint().GetOrigin() + "35 0 25";
		position[1] = GetGame().GetWorld().GetSurfaceY(position[0], position[2]);
		EntitySpawnParams params = new EntitySpawnParams();
		params.TransformMode = ETransformMode.WORLD;
		params.Transform[3] = position;
		SCR_EditorLinkComponent.IgnoreSpawning(true);
		IEntity entity = GetGame().SpawnEntityPrefabEx(AICF_ContentProfile.GetActive().GetConstructionPrefab("US", AICF_EConstructionType.SMALL_BARRACKS), false, params: params);
		SCR_EditorLinkComponent.IgnoreSpawning(false);
		if (!entity) return;
		FactionAffiliationComponent.Cast(entity.FindComponent(FactionAffiliationComponent)).SetAffiliatedFaction(m_USFaction);
		m_SocialPolicyBuilding = SCR_CampaignBuildingCompositionComponent.Cast(entity.FindComponent(SCR_CampaignBuildingCompositionComponent));
		m_SocialPolicyBuilding.SetProviderEntity(base.GetMasterProvider().GetOwner());
	}

	protected void AICF_SocialPolicyTick(int now)
	{
		if (now - m_iSocialProbeStarted > 300000)
		{
			AICF_SocialCheck("POLICY_TIMEOUT", false);
			AICF_SocialClose();
			return;
		}
		AICF_SocialActor actor = m_SocialPolicyActor;
		if (m_iSocialPolicyPhase == 0)
		{
			array<ref AICF_SocialActor> eligible = {};
			foreach (AICF_SocialActor item : m_Social.AICF_ProbePool())
				if (item.Available() && SCR_Faction.GetEntityFaction(item.m_Member) == m_USFaction) eligible.Insert(item);
			if (eligible.Count() < 3) return;
			actor = eligible[0];
			AICF_SocialActor other;
			foreach (AICF_SocialActor candidate : eligible)
				if (candidate != actor && vector.Distance(actor.m_Member.GetOrigin(), candidate.m_Member.GetOrigin()) < 8) { other = candidate; break; }
			if (!other) return;
			IEntity player;
			foreach (AICF_SocialActor body : eligible)
				if (body != actor && body != other) { player = body.m_Member; break; }
			vector firstOrigin = actor.m_Member.GetOrigin();
			vector secondOrigin = other.m_Member.GetOrigin();
			vector playerOrigin = player.GetOrigin();
			m_Social.AICF_ProbePair(actor, other);
			player.SetOrigin(firstOrigin * 0.75 + secondOrigin * 0.25);
			float nearest;
			AICF_SocialCheck("POLICY_NEAREST_ALLY", m_Social.SelectCandidate(player, nearest) == actor);
			AICF_SocialCheck("POLICY_ENEMY", !m_Social.SelectCandidate(AICF_GroupRuntime.ResolveAliveLeader(m_USSRState.GetSlot(0).GetGroup()), nearest));
			player.SetOrigin((firstOrigin + secondOrigin) * 0.5);
			AICF_SocialActor tied = m_Social.SelectCandidate(player, nearest);
			m_Social.AICF_ProbePair(other, actor);
			AICF_SocialCheck("POLICY_STABLE_TIE", tied && tied == m_Social.SelectCandidate(player, nearest));
			AICF_GreetingPlayer shared = new AICF_GreetingPlayer();
			AICF_SocialCheck("TWO_BOT_RESERVATION", shared.Reserve(now, 10));
			actor.m_iToken = 10;
			AICF_SocialCheck("TWO_BOT_SHARED_COOLDOWN", m_Social.SelectCandidate(player, nearest) == other && !shared.Reserve(now, 11) && shared.Commit(10, now) && !shared.Reserve(now + 29999, 11));
			other.m_iToken = 11;
			AICF_SocialCheck("POLICY_BUSY", !m_Social.SelectCandidate(player, nearest));
			actor.m_iToken = 0;
			actor.m_iGeneration++;
			AICF_SocialCheck("POLICY_STALE_GENERATION", !m_Social.SelectCandidate(player, nearest));
			actor.m_iGeneration--;
			other.m_iToken = 0;
			player.SetOrigin(firstOrigin + "20 0 0");
			AICF_SocialCheck("POLICY_OUTSIDE", !m_Social.SelectCandidate(player, nearest));
			player.SetOrigin(playerOrigin);
			m_SocialPolicyActor = actor;
			m_iSocialPolicyPhase = 1;
			return;
		}
		if (m_iSocialPolicyPhase == 1 || m_iSocialPolicyPhase == 3 || m_iSocialPolicyPhase == 5)
		{
			actor = null;
			foreach (AICF_SocialActor initial : m_Social.AICF_ProbePool())
				if (initial.Available() && m_Social.InitialWaiting(initial.m_Slot) && SCR_Faction.GetEntityFaction(initial.m_Member) == m_USFaction) { actor = initial; break; }
			if (!actor || !actor.Available() || !m_Social.InitialWaiting(actor.m_Slot)) return;
			actor.m_iVariant = 0;
			if (!actor.BeginIdle(now)) return;
			m_SocialPolicyActor = actor;
			m_Social.AICF_ProbeOnly(actor);
			m_iSocialPolicyPhase++;
			return;
		}
		if (m_iSocialPolicyPhase == 2 || m_iSocialPolicyPhase == 4 || m_iSocialPolicyPhase == 6)
		{
			m_Social.AICF_ProbeAdvance(actor, now);
			if (actor.m_iPhase != 2) return;
			if (m_iSocialPolicyPhase == 2)
			{
				AIWaypoint waypoint = actor.m_Slot.GetWaypoint();
				actor.m_Slot.RecordStrategicAssignment(actor.m_Slot.GetTargetBase(), "ATTACK");
				AICF_SocialCheck("MUSTER_END_CANCEL", actor.m_iPhase == 0 && !m_Social.InitialWaiting(actor.m_Slot) && actor.m_Slot.GetWaypoint() == waypoint);
				m_iSocialPolicyPhase = 3;
			}
			else if (m_iSocialPolicyPhase == 4)
			{
				bool assigned = m_OrderPlanner.AssignPlayerPointOrder(actor.m_Slot, m_USFaction, actor.m_Member.GetOrigin() + "25 0 0");
				AICF_SocialCheck("MOVEMENT_CANCEL", assigned && actor.m_iPhase == 0 && actor.m_Slot.GetWaypoint() && actor.m_Group.GetCurrentWaypoint() == actor.m_Slot.GetWaypoint());
				m_iSocialPolicyPhase = 5;
			}
			else
			{
				SCR_AIUtilityComponent utility = SCR_AIUtilityComponent.Cast(actor.m_Member.GetAIControlComponent().GetControlAIAgent().FindComponent(SCR_AIUtilityComponent));
				utility.m_ThreatSystem.SetThreatValues(1, 1, 0, 1);
				utility.m_ThreatSystem.Update(utility, 0);
				m_Social.AICF_ProbeAdvance(actor, now);
				AICF_SocialCheck("THREAT_CANCEL", actor.m_iPhase == 0 && !actor.Available());
				m_iSocialPolicyGeneration = actor.m_Slot.GetSpawnGeneration();
				AICF_SocialPrepareReplacement();
				SCR_CharacterDamageManagerComponent.Cast(actor.m_Member.FindComponent(SCR_CharacterDamageManagerComponent)).Kill(Instigator.CreateInstigatorGM());
				AICF_SocialCheck("DEATH_REJECTED", !actor.Available());
				m_iSocialPolicyPhase = 7;
			}
			return;
		}
		if (m_iSocialPolicyPhase == 7)
		{
			if (m_SocialPolicyBuilding && !m_SocialPolicyBuilding.IsCompositionSpawned() && m_SocialPolicyBuilding.GetCompositionLayout())
			{
				SCR_CampaignBuildingLayoutComponent layout = m_SocialPolicyBuilding.GetCompositionLayout();
				layout.AddBuildingValue(layout.GetToBuildValue());
			}
			SCR_CampaignMilitaryBaseComponent base = m_USFaction.GetMainBase();
			base.AddSupplies(base.GetSuppliesMax() - base.GetSupplies());
			ProcessFaction(m_USState, m_USFaction);
			if (!actor.m_Slot.IsCombatReady() || actor.m_Slot.GetSpawnGeneration() <= m_iSocialPolicyGeneration) return;
			AICF_SocialCheck("REPLACEMENT_STALE", !actor.HasIdentity() && !actor.BeginIdle(now) && !m_Social.InitialWaiting(actor.m_Slot));
			m_Social.Stop();
			AICF_SocialCheck("STOP_CLEANUP", m_Social.AICF_ProbeStopped() && actor.m_iPhase == 0);
			AICF_SocialClose();
		}
	}

	protected void AICF_SocialClose()
	{
		GetGame().GetCallqueue().Remove(AICF_SocialProbeTick);
		Print(string.Format("[AICF][SOCIAL_PROBE] finished=1 passed=%1 failed=%2", m_iSocialProbePassed, m_iSocialProbeFailed));
		Stop(true);
		GetGame().RequestClose();
	}
}
