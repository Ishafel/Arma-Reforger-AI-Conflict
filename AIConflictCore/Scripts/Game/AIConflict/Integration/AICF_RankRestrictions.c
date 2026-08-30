// AICF keeps authoritative player XP at or above the faction-specific GENERAL
// threshold and exposes GENERAL to both character- and XP-based callers. This
// removes rank admission gates without modifying serialized vanilla/RHS catalog
// data. Supply, faction, capacity, and authority checks remain owned by their
// stock systems.
modded class SCR_CharacterRankComponent
{
	override protected SCR_ECharacterRank GetCharacterRank()
	{
		return SCR_ECharacterRank.GENERAL;
	}
}

modded class SCR_PlayerXPHandlerComponent
{
	// All stock XP rewards, penalties, reconnect restoration, and persistence
	// restoration converge on AddPlayerXP(). UpdatePlayerRank() is also guarded
	// so the stock spawn lifecycle rechecks the floor after faction assignment
	// and the character never observes a rank below GENERAL.
	override void AddPlayerXP(
		SCR_EXPRewards rewardID,
		float multiplier = 1.0,
		bool volunteer = false,
		int addDirectly = 0)
	{
		super.AddPlayerXP(rewardID, multiplier, volunteer, addDirectly);
		AICF_ApplyGeneralXPFloor();
	}

	override void UpdatePlayerRank(bool notify = true)
	{
		if (!AICF_IsGeneralXPFloorAuthority())
			return;

		AICF_ApplyGeneralXPFloor();

		SCR_PlayerController playerController = SCR_PlayerController.Cast(GetOwner());
		if (!playerController)
			return;

		IEntity player = playerController.GetMainEntity();
		if (!player)
			return;

		SCR_CharacterRankComponent rankComponent = SCR_CharacterRankComponent.Cast(
			player.FindComponent(SCR_CharacterRankComponent));
		if (!rankComponent)
			return;

		// The Reforger 1.8 stock container ends at MAJOR. Persist GENERAL in the
		// replicated character state instead of asking that incomplete catalog to
		// translate the effective maximum-rank XP floor back to an enum.
		rankComponent.SetCharacterRank(SCR_ECharacterRank.GENERAL, !notify);
		AICF_LogGeneralXPFloorVerified();
	}

	override SCR_ECharacterRank GetPlayerRankByXP()
	{
		return SCR_ECharacterRank.GENERAL;
	}

	protected bool AICF_ApplyGeneralXPFloor()
	{
		if (!AICF_IsGeneralXPFloorAuthority())
			return false;

		int generalXPFloor;
		SCR_ECharacterRank catalogFloorRank;
		if (!AICF_TryGetGeneralXPFloor(generalXPFloor, catalogFloorRank))
			return false;
		if (m_iPlayerXP >= generalXPFloor)
			return false;

		SCR_PlayerController playerController = SCR_PlayerController.Cast(GetOwner());
		int previousXP = m_iPlayerXP;
		int correction = generalXPFloor - previousXP;

		// Route the correction through stock mutation, listener, replication, and
		// owner-RPC handling. The nested UpdatePlayerRank() sees XP at the floor,
		// so this recursion terminates immediately.
		AddPlayerXP(SCR_EXPRewards.STARTING_RANK, 1.0, false, correction);

		Print(string.Format(
			"[AICF][RANK][INFO][XP_FLOOR_APPLIED] player_id=%1 previous_xp=%2 floor_xp=%3 correction=%4 catalog_floor_rank=%5 rank=GENERAL",
			playerController.GetPlayerId(),
			previousXP,
			generalXPFloor,
			correction,
			typename.EnumToString(SCR_ECharacterRank, catalogFloorRank)));
		return true;
	}

	protected void AICF_LogGeneralXPFloorVerified()
	{
		if (!AICF_IsGeneralXPFloorAuthority())
			return;

		int generalXPFloor;
		SCR_ECharacterRank catalogFloorRank;
		if (!AICF_TryGetGeneralXPFloor(generalXPFloor, catalogFloorRank) || m_iPlayerXP < generalXPFloor)
			return;

		SCR_PlayerController playerController = SCR_PlayerController.Cast(GetOwner());
		int characterReady;
		if (playerController.GetMainEntity())
			characterReady = 1;

		Print(string.Format(
			"[AICF][RANK][INFO][XP_FLOOR_VERIFIED] player_id=%1 current_xp=%2 floor_xp=%3 character_ready=%4 catalog_floor_rank=%5 rank=GENERAL",
			playerController.GetPlayerId(),
			m_iPlayerXP,
			generalXPFloor,
			characterReady,
			typename.EnumToString(SCR_ECharacterRank, catalogFloorRank)));
	}

	bool AICF_GetGeneralXPFloor(out int generalXPFloor, out SCR_ECharacterRank catalogFloorRank)
	{
		return AICF_TryGetGeneralXPFloor(generalXPFloor, catalogFloorRank);
	}

	protected bool AICF_TryGetGeneralXPFloor(
		out int generalXPFloor,
		out SCR_ECharacterRank catalogFloorRank)
	{
		generalXPFloor = 0;
		catalogFloorRank = SCR_ECharacterRank.INVALID;

		SCR_PlayerController playerController = SCR_PlayerController.Cast(GetOwner());
		if (!playerController || playerController.GetPlayerId() <= 0)
			return false;

		SCR_FactionManager factionManager = SCR_FactionManager.Cast(GetGame().GetFactionManager());
		if (!factionManager)
			return false;

		// GetFactionRanks() falls back to the default container before faction
		// assignment. Gate on the manager's authoritative player map: it is
		// updated before OnPlayerFactionSet_S(), while the affiliation component
		// can still expose its previous value during that callback.
		if (!factionManager.GetPlayerFaction(playerController.GetPlayerId()))
			return false;

		SCR_RankContainer ranks = factionManager.GetFactionRanks(playerController.GetPlayerId());
		if (!ranks)
			return false;

		SCR_RankInfo generalRank = ranks.GetRankByID(SCR_ECharacterRank.GENERAL);
		if (generalRank)
		{
			generalXPFloor = ranks.GetRequiredRankXP(SCR_ECharacterRank.GENERAL);
			catalogFloorRank = SCR_ECharacterRank.GENERAL;
			return generalXPFloor != int.MAX;
		}

		// Reforger 1.8's stock Conflict RankContainer ends at MAJOR even though
		// SCR_ECharacterRank and MissionHeader expose GENERAL. Use the highest
		// configured non-renegade threshold as the effective GENERAL floor.
		int highestConfiguredXP = int.MIN;
		foreach (SCR_RankInfo rankInfo : ranks.GetAllRanks())
		{
			if (!rankInfo || rankInfo.IsRankRenegade())
				continue;

			int requiredXP = rankInfo.GetRequiredRankXP();
			if (requiredXP == int.MAX || requiredXP <= highestConfiguredXP)
				continue;

			highestConfiguredXP = requiredXP;
			catalogFloorRank = rankInfo.GetRankID();
		}

		if (catalogFloorRank == SCR_ECharacterRank.INVALID)
			return false;

		generalXPFloor = highestConfiguredXP;
		return true;
	}

	protected bool AICF_IsGeneralXPFloorAuthority()
	{
		if (!Replication.IsServer())
			return false;

		SCR_BaseGameMode gameMode = SCR_BaseGameMode.Cast(GetGame().GetGameMode());
		return gameMode && gameMode.IsMaster();
	}
}

modded class SCR_FactionManager
{
	override protected void OnPlayerFactionSet_S(
		SCR_PlayerFactionAffiliationComponent playerComponent,
		Faction faction)
	{
		super.OnPlayerFactionSet_S(playerComponent, faction);

		SCR_PlayerController playerController = SCR_PlayerController.Cast(
			playerComponent.GetPlayerController());
		if (!playerController)
			return;

		SCR_PlayerXPHandlerComponent xpHandler = SCR_PlayerXPHandlerComponent.Cast(
			playerController.FindComponent(SCR_PlayerXPHandlerComponent));
		int handlerReady;
		int factionMapped;
		int floorReady;
		int generalXPFloor;
		int currentXP;
		SCR_ECharacterRank catalogFloorRank = SCR_ECharacterRank.INVALID;
		if (GetPlayerFaction(playerController.GetPlayerId()))
			factionMapped = 1;

		if (xpHandler)
		{
			handlerReady = 1;
			if (xpHandler.AICF_GetGeneralXPFloor(generalXPFloor, catalogFloorRank))
				floorReady = 1;

			xpHandler.UpdatePlayerRank(false);
			currentXP = xpHandler.GetPlayerXP();
		}

		Print(string.Format(
			"[AICF][RANK][INFO][XP_FLOOR_FACTION_RECHECK] player_id=%1 handler_ready=%2 faction_mapped=%3 floor_ready=%4 floor_xp=%5 current_xp=%6 catalog_floor_rank=%7 rank=GENERAL",
			playerController.GetPlayerId(),
			handlerReady,
			factionMapped,
			floorReady,
			generalXPFloor,
			currentXP,
			typename.EnumToString(SCR_ECharacterRank, catalogFloorRank)));
	}
}
