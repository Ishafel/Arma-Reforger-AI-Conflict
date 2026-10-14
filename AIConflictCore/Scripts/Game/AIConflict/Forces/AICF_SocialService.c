// Только presentation; сервис не назначает waypoint, не меняет roster или economy.
class AICF_SocialService
{
	protected SCR_GameModeCampaign m_Campaign;
	protected AICF_FactionState m_US;
	protected AICF_FactionState m_USSR;
	protected ref array<ref AICF_SocialActor> m_aActors = {};
	protected ref array<AICF_GroupSlot> m_aDeparted = {};
	protected ref AICF_GreetingGate m_Gate = new AICF_GreetingGate();
	protected bool m_bStopped;
	protected int m_iSeed;

	void Start(SCR_GameModeCampaign campaign, AICF_FactionState us, AICF_FactionState ussr)
	{
		if (!Replication.IsServer() || !campaign || !campaign.IsMaster()) return;
		m_Campaign = campaign;
		m_US = us;
		m_USSR = ussr;
		m_iSeed = Math.RandomInt(0, 997);
		SubscribeSlots(us, true);
		SubscribeSlots(ussr, true);
		campaign.GetOnPlayerDisconnected().Insert(OnDisconnected);
		GetGame().GetCallqueue().CallLater(Tick, AICF_SocialConfig.TICK_MS, true);
	}

	void Stop()
	{
		if (m_bStopped) return;
		m_bStopped = true;
		GetGame().GetCallqueue().Remove(Tick);
		if (m_Campaign) m_Campaign.GetOnPlayerDisconnected().Remove(OnDisconnected);
		SubscribeSlots(m_US, false);
		SubscribeSlots(m_USSR, false);
		foreach (AICF_SocialActor actor : m_aActors) actor.Cancel(m_Campaign, System.GetTickCount());
		m_aActors.Clear();
		m_aDeparted.Clear();
		m_Gate = null;
	}

	protected void SubscribeSlots(AICF_FactionState faction, bool subscribe)
	{
		if (!faction) return;
		for (int i; i < faction.GetSlotCount(); i++)
		{
			ScriptInvoker invoker = faction.GetSlot(i).GetOnPresentationInvalidated();
			if (subscribe) invoker.Insert(OnOrderChanged);
			else invoker.Remove(OnOrderChanged);
		}
	}

	protected void OnOrderChanged(AICF_GroupSlot slot)
	{
		if (m_bStopped) return;
		foreach (AICF_SocialActor actor : m_aActors)
			if (actor.m_Slot == slot && (actor.m_iPhase > 0 || actor.m_iToken > 0)) actor.Cancel(m_Campaign, System.GetTickCount());
	}

	protected void OnDisconnected(int id, KickCauseCode cause, int timeout)
	{
		foreach (AICF_SocialActor actor : m_aActors)
			if (actor.m_iPlayerId == id && actor.m_iToken > 0) actor.FinishGreeting(m_Campaign);
		m_Gate.Disconnect(id);
	}

	bool InitialWaiting(AICF_GroupSlot slot)
	{
		if (!slot || !slot.IsCombatReady() || slot.GetSpawnGeneration() != 1 ||
			slot.GetUnitType() != AICF_EGroupUnitType.INFANTRY || m_aDeparted.Contains(slot)) return false;
		if (slot.HasPlayerStrategicIntent())
		{
			m_aDeparted.Insert(slot);
			return false;
		}
		if (slot.IsWaitingForInfantryMuster() || slot.IsAwaitingPlayerCommand()) return true;
		// Начальный подход к казарме не закрывает muster, но во время движения emote запрещён.
		if (slot.NeedsInfantryMuster() && slot.HasActiveRecruitmentOrder()) return true;
		if (!slot.GetOperationalPosture().IsEmpty()) m_aDeparted.Insert(slot);
		return false;
	}

	protected void Discover(AICF_FactionState faction, int now)
	{
		if (!faction) return;
		for (int index; index < faction.GetSlotCount(); index++)
		{
			AICF_GroupSlot slot = faction.GetSlot(index);
			if (!slot || !slot.IsCombatReady() || slot.GetUnitType() != AICF_EGroupUnitType.INFANTRY) continue;
			InitialWaiting(slot);
			array<AIAgent> agents = {};
			slot.GetGroup().GetAgents(agents);
			foreach (int memberIndex, AIAgent agent : agents)
			{
				ChimeraCharacter member = ChimeraCharacter.Cast(agent.GetControlledEntity());
				if (!AICF_GroupRuntime.IsAliveCharacter(member)) continue;
				AICF_SocialActor found;
				foreach (AICF_SocialActor actor : m_aActors)
				{
					if (actor.m_Member == member && actor.m_Slot == slot && actor.HasIdentity())
					{
						found = actor;
						break;
					}
				}
				if (!found)
				{
					found = new AICF_SocialActor(slot, member, m_iSeed + index * 7, memberIndex, now);
					m_aActors.Insert(found);
				}
				found.m_bSeen = true;
			}
		}
	}

	protected void Tick()
	{
		if (m_bStopped || !Replication.IsServer() || !m_Campaign || !m_Campaign.IsMaster()) return;
		if (!m_Campaign.IsRunning()) return;
		int now = System.GetTickCount();
		foreach (AICF_SocialActor unseen : m_aActors) unseen.m_bSeen = false;
		Discover(m_US, now);
		Discover(m_USSR, now);
		for (int i = m_aActors.Count() - 1; i >= 0; i--)
		{
			AICF_SocialActor actor = m_aActors[i];
			if (!actor.m_bSeen || !actor.HasIdentity())
			{
				actor.Cancel(m_Campaign, now);
				m_aActors.Remove(i);
				continue;
			}
			Advance(actor, now);
		}
		ObservePlayers(now);
	}

	protected void Advance(AICF_SocialActor actor, int now)
	{
		bool initial = InitialWaiting(actor.m_Slot);
		if (!actor.Available() || ((actor.m_iPhase > 0 || actor.m_iToken > 0) && !actor.SameOrder()) ||
			(actor.m_iPhase > 0 && !initial))
		{
			if (actor.m_iPhase > 0 || actor.m_iToken > 0) actor.Cancel(m_Campaign, now);
			return;
		}
		SCR_CharacterControllerComponent controller = actor.Controller();
		if (actor.m_iToken > 0 && (now >= actor.m_iGreetingDeadline ||
			GetGame().GetPlayerManager().GetPlayerControlledEntity(actor.m_iPlayerId) != actor.m_PlayerEntity ||
			!Allied(actor, actor.m_PlayerEntity) || vector.Distance(actor.m_Member.GetOrigin(), actor.m_PlayerEntity.GetOrigin()) > AICF_SocialConfig.EXIT_METERS))
			actor.FinishGreeting(m_Campaign);
		if (actor.m_iPhase == 1 && now >= actor.m_iDeadline)
		{
			actor.Log("SOCIAL_START_FAILED", now);
			actor.Cancel(m_Campaign, now);
			return;
		}
		if (actor.m_bPreparingStance && actor.m_iPhase == 1 &&
			controller.GetStance() == ECharacterStance.STAND && !controller.IsChangingStance())
		{
			actor.m_bPreparingStance = false;
			actor.m_iPhase = 0;
			actor.BeginIdle(now);
		}
		if (actor.m_iPhase == 0)
		{
			if (initial && actor.m_iToken == 0 && now >= actor.m_iDueAt)
			{
				actor.m_iDueAt = now + AICF_SocialConfig.PREPARE_MS;
				actor.BeginIdle(now);
			}
			return;
		}
		if (actor.m_iPhase == 1 && controller.IsLoitering())
		{
			actor.m_iPhase = 2;
			actor.m_iDeadline = now + AICF_SocialConfig.IDLE_MS;
			actor.Log("IDLE_STARTED", now);
		}
		if (actor.m_iPhase == 2 && (!controller.IsLoitering() || now >= actor.m_iDeadline))
		{
			// Естественное завершение idle независимо от текущего звука.
			actor.StopAnimation();
			actor.m_iPhase = 0;
			actor.m_iDueAt = now + AICF_SocialConfig.Delay(actor.m_iGeneration, actor.m_Slot.GetSlotId());
		}
	}

	bool Allied(AICF_SocialActor actor, IEntity player)
	{
		if (!AICF_GroupRuntime.IsAliveCharacter(player)) return false;
		Faction playerFaction = SCR_Faction.GetEntityFaction(player);
		Faction botFaction = SCR_Faction.GetEntityFaction(actor.m_Member);
		if (!playerFaction || !botFaction || playerFaction != botFaction) return false;
		FactionKey key = AICF_ContentProfile.GetActive().GetStableFactionKey(playerFaction.GetFactionKey());
		return key == "US" || key == "USSR";
	}

	protected void ObservePlayers(int now)
	{
		PlayerManager manager = GetGame().GetPlayerManager();
		array<int> ids = {};
		manager.GetPlayers(ids);
		ids.Sort();
		foreach (int id : ids)
		{
			IEntity entity = manager.GetPlayerControlledEntity(id);
			AICF_GreetingPlayer player = m_Gate.Resolve(id, manager.GetPlayerController(id), GetGame().GetBackendApi().GetPlayerIdentityId(id));
			if (!player || !AICF_GroupRuntime.IsAliveCharacter(entity)) continue;
			float nearest;
			AICF_SocialActor candidate = SelectCandidate(entity, nearest);
			if (!player.Observe(nearest) || !candidate) continue;
			int token = m_Gate.NextToken();
			if (!player.Reserve(now, token)) continue;
			candidate.m_Player = player;
			candidate.m_PlayerEntity = entity;
			candidate.m_iPlayerId = id;
			candidate.m_iToken = token;
			candidate.m_iGreetingDeadline = now + AICF_SocialConfig.SALUTE_MS;
			candidate.SnapshotOrder();
			// SALUTE опционален: занятый собственным emote боец продолжает его и говорит.
			bool salute = candidate.BeginSalute();
			player.Commit(token, now);
			candidate.Log("GREETING_STARTED", now);
			AICF_Stage1Diagnostics.Info("GREETING_MODE", string.Format("token=%1 salute=%2 idle_phase=%3", token, salute, candidate.m_iPhase));
			m_Campaign.AICF_PlayGreeting(candidate.m_Member, token);
		}
	}

	AICF_SocialActor SelectCandidate(IEntity entity, out float nearest)
	{
		nearest = 1000000;
		float eligibleDistance = 1000000;
		AICF_SocialActor candidate;
		foreach (AICF_SocialActor actor : m_aActors)
		{
			if (!Allied(actor, entity) || actor.m_Member == entity) continue;
			float distance = vector.Distance(entity.GetOrigin(), actor.m_Member.GetOrigin());
			nearest = Math.Min(nearest, distance);
			if (distance > AICF_SocialConfig.ENTER_METERS || actor.m_iToken > 0 || !actor.Available()) continue;
			if (distance < eligibleDistance || (distance == eligibleDistance && candidate && actor.m_MemberId.ToString().Compare(candidate.m_MemberId.ToString()) < 0))
			{
				candidate = actor;
				eligibleDistance = distance;
			}
		}
		return candidate;
	}
}
