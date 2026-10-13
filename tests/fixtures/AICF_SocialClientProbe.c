// Дополнение к SocialProbe: terminal direct-connect, штатный spawn, production greeting/audio RPC.
modded class SCR_GameModeCampaign
{
	protected int m_iSocialClientStarted;
	protected int m_iSocialClientAudio;
	protected int m_iSocialClientLastAudio;
	protected int m_iSocialClientDuplicate;
	protected ref array<int> m_aSocialPlayedTokens = {};
	protected ref array<int> m_aSocialGestureTokens = {};
	protected bool m_bSocialResourcesChecked;

	override void EOnInit(IEntity owner)
	{
		super.EOnInit(owner);
		string flag;
		if (Replication.IsServer() || !System.GetCLIParam("aicfSocialClientProbe", flag) || flag != "1") return;
		m_iSocialClientStarted = System.GetTickCount();
		Print("[AICF][SOCIAL_CLIENT] initialized=1");
		GetGame().GetCallqueue().CallLater(AICF_SocialClientTick, 500, true);
	}

	override protected void AICF_Greeting_Broadcast(RplId id, int token)
	{
		bool duplicate = token <= m_iAICFLastGreetingToken;
		int count = m_aAICFGreetingSounds.Count();
		super.AICF_Greeting_Broadcast(id, token);
		if (System.IsConsoleApp()) return;
		if (duplicate)
		{
			if (count == m_aAICFGreetingSounds.Count()) m_iSocialClientDuplicate++;
			Print(string.Format("[AICF][SOCIAL_CLIENT] token=%1 duplicate_rejected=%2", token, count == m_aAICFGreetingSounds.Count()));
			return;
		}
		if (m_aAICFGreetingSounds.Count() > count && m_aAICFGreetingSounds[m_aAICFGreetingSounds.Count() - 1].m_Handle != AudioHandle.Invalid)
		{
			m_iSocialClientAudio++;
			m_iSocialClientLastAudio = System.GetTickCount();
			AICF_GreetingSound sound = m_aAICFGreetingSounds[m_aAICFGreetingSounds.Count() - 1];
			Print(string.Format("[AICF][SOCIAL_CLIENT] token=%1 audio_queued=1 faction=%2", token,
				SCR_Faction.GetEntityFaction(sound.m_Entity).GetFactionKey()));
		}
	}

	override protected void AICF_UpdateGreetingSounds()
	{
		foreach (AICF_GreetingSound sound : m_aAICFGreetingSounds)
		{
			ChimeraCharacter character = ChimeraCharacter.Cast(sound.m_Entity);
			CharacterAnimationComponent animation = character.GetCharacterController().GetAnimationComponent();
			TAnimGraphTag tag = GameAnimationUtils.RegisterAnimationTag("TagCharacterGesture");
			if (!m_aSocialGestureTokens.Contains(sound.m_iToken) && (animation.IsPrimaryTag(tag) || animation.IsSecondaryTag(tag)))
			{
				m_aSocialGestureTokens.Insert(sound.m_iToken);
				Print(string.Format("[AICF][SOCIAL_CLIENT] token=%1 salute_tag=1", sound.m_iToken));
			}
			if (!m_aSocialPlayedTokens.Contains(sound.m_iToken) && AudioSystem.IsSoundPlayed(sound.m_Handle))
			{
				m_aSocialPlayedTokens.Insert(sound.m_iToken);
				Print(string.Format("[AICF][SOCIAL_CLIENT] token=%1 audio_played=1", sound.m_iToken));
			}
		}
		super.AICF_UpdateGreetingSounds();
	}

	void AICF_SocialDuplicate(ChimeraCharacter member, int token)
	{
		Rpc(AICF_Greeting_Broadcast, member.GetRplComponent().Id(), token);
	}

	void AICF_SocialPlace(IEntity entity, vector position)
	{
		SCR_CharacterDamageManagerComponent damage = SCR_CharacterDamageManagerComponent.Cast(entity.FindComponent(SCR_CharacterDamageManagerComponent));
		if (damage) damage.EnableDamageHandling(false);
		position[1] = GetGame().GetWorld().GetSurfaceY(position[0], position[2]) + 0.1;
		entity.SetOrigin(position);
		RplComponent replication = RplComponent.Cast(entity.FindComponent(RplComponent));
		Rpc(AICF_SocialPlace_Broadcast, replication.Id(), position);
	}

	[RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
	protected void AICF_SocialPlace_Broadcast(RplId id, vector position)
	{
		RplComponent replication = RplComponent.Cast(Replication.FindItem(id));
		if (replication && replication.IsOwner())
		{
			replication.GetEntity().SetOrigin(position);
			ChimeraCharacter character = ChimeraCharacter.Cast(replication.GetEntity());
			if (character) character.GetCharacterController().SetHeadingAngle(-90 * Math.DEG2RAD, true);
		}
	}

	protected void AICF_SocialClientTick()
	{
		string jip;
		int now = System.GetTickCount();
		if (System.GetCLIParam("aicfSocialJIP", jip) && jip == "1")
		{
			if (now - m_iSocialClientStarted < 15000) return;
			Print(string.Format("[AICF][SOCIAL_CLIENT] finished=1 jip=1 audio=%1 last_token=%2", m_iSocialClientAudio, m_iAICFLastGreetingToken));
			GetGame().GetCallqueue().Remove(AICF_SocialClientTick);
			GetGame().RequestClose();
			return;
		}
		if ((m_iSocialClientAudio >= 5 && now - m_iSocialClientLastAudio > 5000) || now - m_iSocialClientStarted > 300000)
		{
			Print(string.Format("[AICF][SOCIAL_CLIENT] finished=1 jip=0 audio=%1 duplicates=%2", m_iSocialClientAudio, m_iSocialClientDuplicate));
			GetGame().GetCallqueue().Remove(AICF_SocialClientTick);
			GetGame().RequestClose();
			return;
		}
		PlayerController player = GetGame().GetPlayerController();
		if (!m_bSocialResourcesChecked && player)
		{
			AICF_ContentProfile profile = AICF_ContentProfile.GetActive();
			FactionKey us = profile.GetRuntimeFactionKey("US");
			FactionKey ussr = profile.GetRuntimeFactionKey("USSR");
			bool mapped = GetGame().GetFactionManager().GetFactionByKey(us) && GetGame().GetFactionManager().GetFactionByKey(ussr) && profile.GetStableFactionKey(us) == "US" && profile.GetStableFactionKey(ussr) == "USSR";
			Print(string.Format("[AICF][SOCIAL_RESOURCE] project=%1 us=%2 ussr=%3 mapped=%4", AudioSystem.PlayEventInitialize(AICF_SocialConfig.GREETING_PROJECT), us, ussr, mapped));
			m_bSocialResourcesChecked = true;
		}
		if (!player || AICF_GroupRuntime.IsAliveCharacter(player.GetControlledEntity())) return;
		string side = "US";
		if (!System.GetCLIParam("aicfSocialFaction", side)) side = "US";
		FactionKey key = AICF_ContentProfile.GetActive().GetRuntimeFactionKey(side);
		SCR_CampaignFaction faction = SCR_CampaignFaction.Cast(GetGame().GetFactionManager().GetFactionByKey(key));
		if (!faction || !faction.GetMainBase()) { Print("[AICF][SOCIAL_CLIENT_WAIT] reason=FACTION_HQ"); return; }
		SCR_PlayerFactionAffiliationComponent affiliation = SCR_PlayerFactionAffiliationComponent.Cast(player.FindComponent(SCR_PlayerFactionAffiliationComponent));
		if (affiliation.GetAffiliatedFaction() != faction) { Print("[AICF][SOCIAL_CLIENT_WAIT] reason=FACTION"); affiliation.RequestFaction(faction); return; }
		if (!SCR_GroupsManagerComponent.GetInstance().GetPlayerGroup(player.GetPlayerId()))
		{
			SCR_PlayerControllerGroupComponent.Cast(player.FindComponent(SCR_PlayerControllerGroupComponent)).RequestCreateGroup();
			Print("[AICF][SOCIAL_CLIENT_WAIT] reason=GROUP");
			return;
		}
		SCR_EntityCatalog catalog = faction.GetFactionEntityCatalogOfType(EEntityCatalogType.CHARACTER);
		array<SCR_EntityCatalogEntry> entries = {};
		catalog.GetEntityList(entries);
		ResourceName prefab;
		foreach (SCR_EntityCatalogEntry entry : entries)
		{
			if (entry && entry.IsEnabled() && !entry.GetPrefab().IsEmpty()) { prefab = entry.GetPrefab(); break; }
		}
		SCR_SpawnPoint chosen;
		float nearest = float.MAX;
		foreach (SCR_SpawnPoint point : SCR_SpawnPoint.GetSpawnPointsForFaction(key))
		{
			if (!point || !point.IsSpawnPointActive()) continue;
			float distance = vector.DistanceXZ(point.GetOrigin(), faction.GetMainBase().GetOwner().GetOrigin());
			if (distance >= nearest) continue;
			nearest = distance;
			chosen = point;
		}
		if (chosen && !prefab.IsEmpty()) SCR_RespawnComponent.Cast(player.GetRespawnComponent()).RequestSpawn(new SCR_SpawnPointSpawnData(prefab, chosen.GetRplId()));
		Print(string.Format("[AICF][SOCIAL_CLIENT_WAIT] reason=SPAWN chosen=%1 prefab=%2", chosen != null, prefab));
	}

	void ~SCR_GameModeCampaign()
	{
		if (GetGame()) GetGame().GetCallqueue().Remove(AICF_SocialClientTick);
	}
}
