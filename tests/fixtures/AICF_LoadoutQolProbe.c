// Terminal-only fixture. Copy to isolated Core/UI after production sources.
// Native respawn UI использует тот же player-owned result token. Во время
// AI protocol probe её автоматический read не должен подменять ожидаемый ответ.
modded class AICF_PersonalLoadoutUI
{
	override void Update(Widget parent, bool characterSelected, ItemPreviewWidget loadoutImage = null)
	{
		string enabled;
		if (System.GetCLIParam("aicfLoadoutQolProbe", enabled) && enabled == "1") return;
		super.Update(parent, characterSelected, loadoutImage);
	}
}

class AICF_QolChecks
{
	static int s_iFailures;
	static void Check(string rule, bool passed)
	{
		if (!passed) s_iFailures++;
		Print(string.Format("[AICF][QOL_CHECK] rule=%1 passed=%2", rule, passed));
	}

	static AICF_LoadoutRecipe WithoutHelmet(AICF_LoadoutRecipe original)
	{
		AICF_LoadoutRecipe recipe = AICF_LoadoutRecipe.Decode(original.Encode());
		SCR_CampaignFaction faction = SCR_CampaignFaction.Cast(GetGame().GetFactionManager().GetFactionByKey(
			AICF_ContentProfile.GetActive().GetRuntimeFactionKey(recipe.m_sFaction)));
		AICF_LoadoutCatalog catalog = new AICF_LoadoutCatalog(faction);
		AICF_LoadoutDraft draft = new AICF_LoadoutDraft();
		string reason;
		if (!draft.Build(recipe, catalog, reason)) return null;
		AICF_LoadoutNavigation tree = new AICF_LoadoutNavigation();
		tree.Build(draft.GetCharacter(), catalog);
		foreach (AICF_LoadoutLocation location : tree.m_Locations)
		{
			if (!location.m_sPath.IsEmpty()) continue;
			for (int i; i < location.m_Storage.GetSlotsCount(); i++)
			{
				typename area = AICF_LoadoutSlotView.Area(location.m_Storage, i);
				if (area != typename.Empty && area.IsInherited(LoadoutHeadCoverArea) && location.m_Storage.Get(i))
				{
					recipe.Add(location.m_sPath, location.m_sStorage, i, "", 1);
					draft.Clear();
					return recipe;
				}
			}
		}
		draft.Clear();
		return null;
	}
}

modded class AICF_LoadoutService
{
	AICF_LoadoutStore AICF_QolStore() { return m_Store; }
	bool AICF_QolApplySquad(SCR_PlayerController player, int id, AICF_SquadLoadout squad, out string reason)
	{
		return ApplySquad(player, id, squad, reason);
	}
}

modded class AICF_MatchController
{
	protected bool m_bQolStarted;
	protected bool m_bQolClientSeen;
	protected int m_iQolAt;
	protected int m_iQolEnemyReads;

	override protected void Update()
	{
		super.Update();
		string enabled;
		if (!System.GetCLIParam("aicfLoadoutQolProbe", enabled) || enabled != "1") return;
		if (!m_bQolStarted && m_bRosterReady)
		{
			m_bQolStarted = true;
			m_iQolAt = System.GetTickCount();
			AICF_QolStorage(m_USFaction);
			AICF_QolStorage(m_USSRFaction);
			Print(string.Format("[AICF][QOL_STORAGE_DONE] failures=%1", AICF_QolChecks.s_iFailures));
		}
		if (!m_bQolStarted) return;
		int count = GetGame().GetPlayerManager().GetPlayerCount();
		if (count > 0) m_bQolClientSeen = true;
		if ((m_bQolClientSeen && count == 0) || System.GetTickCount(m_iQolAt) > 300000)
		{
			AICF_QolChecks.Check("CLIENT_CONNECTED", m_bQolClientSeen);
			Print(string.Format("[AICF][QOL_SERVER_DONE] failures=%1", AICF_QolChecks.s_iFailures));
			GetGame().RequestClose();
		}
	}

	protected void AICF_QolStorage(SCR_CampaignFaction faction)
	{
		string role, reason;
		ResourceName source = m_GroupSpawner.ResolveRecruitPrefab(faction, 0, role);
		AICF_LoadoutRecipe recipe = AICF_LoadoutService.DefaultRecipe(faction, source, 0);
		recipe.m_sName = "QoL storage " + recipe.m_sFaction;
		AICF_LoadoutRecipe changed = AICF_QolChecks.WithoutHelmet(recipe);
		AICF_QolChecks.Check("STORAGE_CHANGED_" + recipe.m_sFaction, changed != null);
		if (!changed) return;
		AICF_LoadoutBinding binding = AICF_LoadoutService.Validate(changed, faction, reason);
		AICF_QolChecks.Check("STORAGE_VALIDATED_" + recipe.m_sFaction, binding != null);
		if (!binding) return;
		FileIO.MakeDirectory(AICF_LoadoutStore.DIRECTORY);
		JsonSaveContext legacy = new JsonSaveContext();
		recipe.Write(legacy);
		AICF_QolChecks.Check("LEGACY_WRITE_" + recipe.m_sFaction, legacy.SaveToFile(AICF_LoadoutStore.DIRECTORY + "/template_255.json"));
		AICF_LoadoutStore store = new AICF_LoadoutStore();
		AICF_QolChecks.Check("LEGACY_READ_" + recipe.m_sFaction, store.Get(255) && store.Get(255).Encode() == recipe.Encode());
		AICF_QolChecks.Check("UPSERT_" + recipe.m_sFaction, store.Save(changed, reason));
		int revision = store.GetRevision();
		AICF_QolChecks.Check("DEDUPE_" + recipe.m_sFaction, store.Save(changed, reason) && store.GetRevision() == revision);
		AICF_LoadoutStore reopened = new AICF_LoadoutStore();
		AICF_QolChecks.Check("RELOAD_" + recipe.m_sFaction, reopened.Get(255) && reopened.Get(255).Encode() == changed.Encode());
		JsonSaveContext partial = new JsonSaveContext();
		partial.WriteValue("schemaVersion", 2);
		partial.SaveToFile(AICF_LoadoutStore.DIRECTORY + "/template_255_0.json");
		reopened = new AICF_LoadoutStore();
		AICF_QolChecks.Check("PARTIAL_WRITE_PRESERVES_" + recipe.m_sFaction, reopened.Get(255) && reopened.Get(255).Encode() == changed.Encode());
		AICF_QolChecks.Check("DELETE_" + recipe.m_sFaction, store.Delete(255, reason));
		reopened = new AICF_LoadoutStore();
		AICF_QolChecks.Check("NO_LEGACY_REVIVAL_" + recipe.m_sFaction, !reopened.Get(255) && FileIO.FileExists(AICF_LoadoutStore.DIRECTORY + "/template_255.json"));
		AICF_GroupSlot first = new AICF_GroupSlot(100, AICF_EGroupRole.ATTACK);
		AICF_GroupSlot second = new AICF_GroupSlot(101, AICF_EGroupRole.ATTACK);
		AICF_LoadoutBatch batch = new AICF_LoadoutBatch();
		batch.Add(first, 0, binding);
		batch.Add(second, 0, binding);
		second.SetLoadout(1, binding, 0);
		AICF_QolChecks.Check("BATCH_STALE_ATOMIC_" + recipe.m_sFaction, !batch.Commit() && first.GetLoadoutRevision() == 0 && !first.GetLoadout(0));
		AICF_LoadoutBinding invalid = new AICF_LoadoutBinding();
		invalid.m_Recipe = new AICF_LoadoutRecipe();
		batch = new AICF_LoadoutBatch();
		batch.Add(first, 0, binding);
		batch.Add(second, 0, invalid);
		AICF_QolChecks.Check("BATCH_INVALID_ATOMIC_" + recipe.m_sFaction, !batch.Commit() && first.GetLoadoutRevision() == 0 && !first.GetLoadout(0));
		batch = new AICF_LoadoutBatch();
		batch.Add(first, 0, binding);
		batch.Add(second, 0, binding);
		AICF_QolChecks.Check("BATCH_COMMIT_" + recipe.m_sFaction, batch.Commit());
		AICF_QolChecks.Check("BINDING_INDEPENDENCE_" + recipe.m_sFaction, first.GetLoadout(0) != second.GetLoadout(0) &&
			first.GetLoadout(0).m_iRevision == 1 && second.GetLoadout(0).m_iRevision == 2);
		FileIO.DeleteFile(AICF_LoadoutStore.DIRECTORY + "/template_255.json");
		FileIO.DeleteFile(AICF_LoadoutStore.DIRECTORY + "/template_255_0.json");
		FileIO.DeleteFile(AICF_LoadoutStore.DIRECTORY + "/template_255_1.json");
	}

	override void RequestLoadout(SCR_PlayerController player, int token, int slot, int member, int revision, int operation, string payload,
		string targetFaction = "", int libraryRevision = -1)
	{
		string enabled;
		if (!System.GetCLIParam("aicfLoadoutQolProbe", enabled) || enabled != "1" || slot == AICF_PersonalLoadout.SLOT)
		{
			super.RequestLoadout(player, token, slot, member, revision, operation, payload, targetFaction, libraryRevision);
			return;
		}
		int beforeLibrary;
		if (m_Loadouts) beforeLibrary = m_Loadouts.AICF_QolStore().GetRevision();
		super.RequestLoadout(player, token, slot, member, revision, operation, payload, targetFaction, libraryRevision);
		bool accepted = player.AICF_LoadoutAccepted();
		if (accepted)
		{
			AICF_FactionState state = m_USState;
			if (player.AICF_LoadoutFaction() == "USSR") state = m_USSRState;
			JsonLoadContext snapshot = new JsonLoadContext();
			array<string> assignedNames = {};
			bool exactNames = snapshot.LoadFromString(player.AICF_LoadoutLibrary()) && snapshot.ReadValue("assignedNames", assignedNames) &&
				assignedNames.Count() == AICF_Stage1Config.MAX_GROUP_SIZE;
			if (exactNames)
			{
				for (int position; position < assignedNames.Count(); position++)
				{
					AICF_LoadoutBinding assigned = state.GetSlot(slot).GetLoadout(position);
					string expectedName = string.Empty;
					if (assigned) expectedName = assigned.m_Recipe.m_sName;
					if (assignedNames[position] != expectedName) exactNames = false;
				}
			}
			AICF_QolChecks.Check("ASSIGNED_NAMES_SERVER_" + token.ToString(), exactNames);
			if (!exactNames)
			{
				Print("[AICF][ASSIGNED_NAMES_DEBUG] " + player.AICF_LoadoutLibrary());
				Print(string.Format("[AICF][ASSIGNED_NAMES_CONTEXT] slot=%1 side=%2 count=%3", slot, player.AICF_LoadoutFaction(), assignedNames.Count()));
			}
		}
		if (operation == 1 || operation == 5 || operation == 7)
			AICF_QolChecks.Check("APPLY_NO_LIBRARY_WRITE_" + token.ToString(), beforeLibrary == m_Loadouts.AICF_QolStore().GetRevision());
		if (operation == 5 && accepted)
		{
			Print("[AICF][QOL_ROLE_EXPECTED] " + payload);
			Print("[AICF][QOL_ROLE_ACTUAL] " + m_USState.GetSlot(0).GetLoadout(member).m_Recipe.Encode());
			// ResourceName нормализуется движком различно на server/client:
			// GUID и {GUID}path — одна identity. Сравниваем server round-trip.
			AICF_LoadoutRecipe expected = AICF_LoadoutRecipe.Decode(payload);
			for (int i; i < m_USState.GetSlotCount(); i++)
				AICF_QolChecks.Check("ROLE_SLOT_" + i.ToString(), m_USState.GetSlot(i).GetLoadout(member) &&
					m_USState.GetSlot(i).GetLoadout(member).m_Recipe.Encode() == expected.Encode());
		}
		if (operation == 7 && accepted)
		{
			AICF_SquadLoadout squad = AICF_SquadLoadout.Decode(m_Loadouts.AICF_QolStore().GetPreset(payload.ToInt()).m_sData);
			for (int j; j < AICF_Stage1Config.MAX_GROUP_SIZE; j++)
				AICF_QolChecks.Check("SQUAD_POSITION_" + j.ToString(), m_USState.GetSlot(slot).GetLoadout(j) &&
					m_USState.GetSlot(slot).GetLoadout(j).m_Recipe.Encode() == squad.m_aRecipes[j]);
			int before = m_USState.GetSlot(slot).GetLoadoutRevision();
			AICF_LoadoutRecipe bad = AICF_LoadoutRecipe.Decode(squad.m_aRecipes[9]);
			bad.m_sCharacter = AICF_LoadoutRecipe.Decode(squad.m_aRecipes[0]).m_sCharacter;
			squad.m_aRecipes[9] = bad.Encode();
			string reason;
			AICF_QolChecks.Check("SQUAD_BAD_LAST_ATOMIC", !m_Loadouts.AICF_QolApplySquad(player, slot, squad, reason) &&
				m_USState.GetSlot(slot).GetLoadoutRevision() == before);
		}
		if (operation == 0 && targetFaction == "USSR")
		{
			m_iQolEnemyReads++;
			if (m_iQolEnemyReads == 1)
			{
				AICF_QolChecks.Check("ENEMY_DENIED", !accepted);
				AICF_QolChecks.Check("ADMIN_GRANT", GetGame().GetPlayerManager().GivePlayerRole(player.GetPlayerId(), EPlayerRole.SESSION_ADMINISTRATOR | EPlayerRole.GAME_MASTER));
			}
			else if (m_iQolEnemyReads < 4) AICF_QolChecks.Check("ENEMY_ADMIN_READ", accepted);
			else AICF_QolChecks.Check("ENEMY_REVOKED", !accepted);
		}
		if (operation == 4 && targetFaction == "USSR" && accepted)
			AICF_QolChecks.Check("ADMIN_REVOKE", GetGame().GetPlayerManager().ClearPlayerRole(player.GetPlayerId(), EPlayerRole.SESSION_ADMINISTRATOR | EPlayerRole.GAME_MASTER));
	}
}

modded class SCR_GameModeCampaign
{
	protected int m_iQolStep;
	protected int m_iQolToken;
	protected int m_iQolStartedAt;
	protected int m_iQolMemberRevision;
	protected int m_iQolLibraryRevision;
	protected int m_iQolSavedId = -1;
	protected int m_iQolSquadId = -1;
	protected int m_iQolEnemyId = -1;
	protected ref AICF_LoadoutRecipe m_QolBase;
	protected ref AICF_LoadoutRecipe m_QolChanged;

	override void OnGameStart()
	{
		super.OnGameStart();
		string enabled;
		if (Replication.IsServer() || !System.GetCLIParam("aicfLoadoutQolProbe", enabled) || enabled != "1") return;
		m_iQolStartedAt = System.GetTickCount();
		GetGame().GetCallqueue().CallLater(AICF_QolTick, 1500, true);
	}

	protected int AICF_QolFind(SCR_PlayerController player, string name)
	{
		JsonLoadContext json = new JsonLoadContext();
		array<string> names = {};
		array<int> ids = {};
		if (!json.LoadFromString(player.AICF_LoadoutLibrary()) || !json.ReadValue("names", names) || !json.ReadValue("ids", ids)) return -1;
		int index = names.Find(name);
		if (index < 0) return -1;
		return ids[index];
	}

	protected int AICF_QolRevision(SCR_PlayerController player)
	{
		JsonLoadContext json = new JsonLoadContext();
		int revision = -1;
		if (json.LoadFromString(player.AICF_LoadoutLibrary())) json.ReadValue("revision", revision);
		return revision;
	}

	protected void AICF_QolTick()
	{
		if (System.GetTickCount(m_iQolStartedAt) > 240000) { AICF_QolChecks.Check("CLIENT_TIMEOUT", false); AICF_QolClose(); return; }
		SCR_PlayerController player = SCR_PlayerController.Cast(GetGame().GetPlayerController());
		if (!player) return;
		Faction faction = GetGame().GetFactionManager().GetFactionByKey(AICF_ContentProfile.GetActive().GetRuntimeFactionKey("US"));
		SCR_PlayerFactionAffiliationComponent affiliation = SCR_PlayerFactionAffiliationComponent.Cast(player.FindComponent(SCR_PlayerFactionAffiliationComponent));
		if (!affiliation || !faction) return;
		if (affiliation.GetAffiliatedFaction() != faction) { affiliation.RequestFaction(faction); return; }
		if (m_iQolToken)
		{
			if (player.AICF_LoadoutToken() != m_iQolToken) return;
			// После удаления в enemy library fixture отзывает admin. Owner view
			// должен исчезнуть до оценки клиентского результата этой операции.
			if (m_iQolStep == 27 && player.AICF_LoadoutSlot() != -1) return;
			bool expected = m_iQolStep != 12 && m_iQolStep != 13 && m_iQolStep != 20 && m_iQolStep != 25 && m_iQolStep != 28;
			AICF_QolChecks.Check("RPC_STEP_" + m_iQolStep.ToString(), player.AICF_LoadoutAccepted() == expected);
			if (player.AICF_LoadoutAccepted())
			{
				JsonLoadContext snapshot = new JsonLoadContext();
				array<string> assignedNames = {};
				bool correctSnapshot;
				if (m_iQolStep == 27) correctSnapshot = player.AICF_LoadoutSlot() == -1 && player.AICF_LoadoutLibrary().IsEmpty();
				else correctSnapshot = snapshot.LoadFromString(player.AICF_LoadoutLibrary()) &&
					snapshot.ReadValue("assignedNames", assignedNames) && assignedNames.Count() == AICF_Stage1Config.MAX_GROUP_SIZE;
				AICF_QolChecks.Check("ASSIGNED_NAMES_CLIENT_" + m_iQolStep.ToString(), correctSnapshot);
			}
			Print(string.Format("[AICF][QOL_RPC] step=%1 accepted=%2 reason=%3 revision=%4 library=%5", m_iQolStep,
				player.AICF_LoadoutAccepted(), player.AICF_LoadoutStatus(), player.AICF_LoadoutRevision(), AICF_QolRevision(player)));
			if (m_iQolStep == 1)
			{
				m_QolBase = AICF_LoadoutRecipe.Decode(player.AICF_LoadoutData());
				if (!m_QolBase) { AICF_QolClose(); return; }
				m_QolBase.m_sName = "QoL Leader";
				m_QolChanged = AICF_QolChecks.WithoutHelmet(m_QolBase);
				AICF_QolChecks.Check("CLIENT_CHANGED_DRAFT", m_QolChanged != null);
				if (!m_QolChanged) { AICF_QolClose(); return; }
			}
			if (m_iQolStep == 3) AICF_QolChecks.Check("REPEATED_APPLY_NO_REVISION", player.AICF_LoadoutRevision() == m_iQolMemberRevision);
			if (m_iQolStep == 4) m_iQolSavedId = AICF_QolFind(player, "QoL Leader");
			if (m_iQolStep == 5) AICF_QolChecks.Check("REPEATED_SAVE_NO_REVISION", AICF_QolRevision(player) == m_iQolLibraryRevision);
			if (m_iQolStep == 8) m_iQolSquadId = AICF_QolFind(player, "QoL Squad");
			if (m_iQolStep == 11) AICF_QolChecks.Check("UPDATE_SAME_ID", AICF_QolFind(player, "QoL Leader") == m_iQolSavedId);
			if (m_iQolStep == 14) AICF_QolChecks.Check("DELETED_FROM_LIST", AICF_QolFind(player, "QoL Leader") == -1);
			if (m_iQolStep == 19) AICF_QolChecks.Check("SQUAD_DELETED_FROM_LIST", AICF_QolFind(player, "QoL Squad") == -1);
			if (m_iQolStep == 23) m_iQolEnemyId = AICF_QolFind(player, "QoL Enemy");
			m_iQolMemberRevision = player.AICF_LoadoutRevision();
			m_iQolLibraryRevision = AICF_QolRevision(player);
			m_iQolToken = 0;
		}
		m_iQolStep++;
		int operation;
		string payload;
		string side = "US";
		int libraryRevision = m_iQolLibraryRevision;
		switch (m_iQolStep)
		{
			case 1: operation = 0; break;
			case 2: operation = 1; payload = m_QolBase.Encode(); break;
			case 3: operation = 1; payload = m_QolBase.Encode(); break;
			case 4: operation = 3; payload = m_QolBase.Encode(); break;
			case 5: operation = 3; payload = m_QolBase.Encode(); break;
			case 6: operation = 2; payload = m_iQolSavedId.ToString(); break;
			case 7: operation = 5; payload = m_QolBase.Encode(); break;
			case 8: operation = 6; payload = "QoL Squad"; break;
			case 9: operation = 1; payload = m_QolChanged.Encode(); break;
			case 10: operation = 7; payload = m_iQolSquadId.ToString(); break;
			case 11: operation = 3; payload = m_QolChanged.Encode(); break;
			case 12: operation = 4; payload = m_iQolSavedId.ToString(); libraryRevision--; break;
			case 13: operation = 1; payload = "{}"; break;
			case 14: operation = 4; payload = m_iQolSavedId.ToString(); break;
			case 15: operation = 1; payload = m_QolChanged.Encode(); break;
			case 16: operation = 6; payload = "QoL Squad"; break;
			case 17: operation = 1; payload = m_QolBase.Encode(); break;
			case 18: operation = 7; payload = m_iQolSquadId.ToString(); break;
			case 19: operation = 4; payload = m_iQolSquadId.ToString(); break;
			case 20: operation = 0; side = "USSR"; break;
			case 21: operation = 0; side = "USSR"; break;
			case 22: operation = 1; side = "USSR"; payload = player.AICF_LoadoutData(); break;
			case 23:
				operation = 3; side = "USSR";
				AICF_LoadoutRecipe enemy = AICF_LoadoutRecipe.Decode(player.AICF_LoadoutData());
				enemy.m_sName = "QoL Enemy"; payload = enemy.Encode(); break;
			case 24: operation = 0; break;
			case 25: operation = 4; payload = m_iQolEnemyId.ToString(); break;
			case 26: operation = 0; side = "USSR"; break;
			case 27: operation = 4; side = "USSR"; payload = m_iQolEnemyId.ToString(); break;
			case 28: operation = 0; side = "USSR"; break;
			default: AICF_QolClose(); return;
		}
		m_iQolToken = player.AICF_RequestLoadout(0, 0, m_iQolMemberRevision, operation, payload, side, libraryRevision);
	}

	protected void AICF_QolClose()
	{
		GetGame().GetCallqueue().Remove(AICF_QolTick);
		Print(string.Format("[AICF][QOL_CLIENT_DONE] failures=%1 step=%2", AICF_QolChecks.s_iFailures, m_iQolStep));
		GetGame().RequestClose();
	}

	override void OnGameEnd()
	{
		GetGame().GetCallqueue().Remove(AICF_QolTick);
		super.OnGameEnd();
	}
}
