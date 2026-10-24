// Только isolated stage после Qol/Files/Inspector. Native files, owner RPC и
// invisible widget metadata; без открытия меню или имитации ввода.
class AICF_PersonalLibraryImage
{
	static string Active(string path)
	{
		string image;
		for (int generation; generation < 2; generation++)
		{
			string filePath = path + "_" + generation.ToString() + ".json";
			if (!FileIO.FileExists(filePath)) { image += "MISSING\n"; continue; }
			FileHandle file = FileIO.OpenFile(filePath, FileMode.READ);
			if (!file) return "UNREADABLE";
			string data;
			int length = file.GetLength();
			if (length < 0 || length > 600000) { file.Close(); return "UNREADABLE"; }
			int count = file.Read(data, length);
			file.Close();
			if (count != length) return "UNREADABLE";
			image += data + "\n";
		}
		return image;
	}
}

modded class AICF_MatchController
{
	protected bool m_bPersonalReloadDone;

	override protected void AICF_QolStorage(SCR_CampaignFaction faction)
	{
		string enabled;
		if (!System.GetCLIParam("aicfPersonalLibraryProbe", enabled) || enabled != "1") { super.AICF_QolStorage(faction); return; }
		string role;
		ResourceName source = m_GroupSpawner.ResolveRecruitPrefab(faction, 0, role);
		AICF_LoadoutRecipe recipe = AICF_LoadoutService.DefaultRecipe(faction, source, 0);
		recipe = AICF_LoadoutRecipe.Decode(recipe.Encode());
		recipe.m_sName = "Legacy personal";
		string label = "PERSONAL_STORE_" + recipe.m_sFaction + "_";
		string activePath = AICF_PersonalLoadoutStore.DIRECTORY + "/fixture_" + recipe.m_sProfile + "_" + recipe.m_sFaction;
		string path = activePath + "_library";
		AICF_PersonalPresetLibrary library = new AICF_PersonalPresetLibrary(path, true);
		string reload;
		int revision;
		if (System.GetCLIParam("aicfPersonalLibraryReload", reload) && reload == "1")
		{
			AICF_QolChecks.Check(label + "RESTART_MEMBER", library.Get(1) && library.Get(1).m_sName == "Personal B");
			AICF_QolChecks.Check(label + "RESTART_TOMBSTONE", !library.Get(0));
			AICF_LoadoutRecipe old = AICF_PersonalLoadoutStore.Load(activePath, revision);
			AICF_QolChecks.Check(label + "RESTART_ACTIVE", old && old.m_sName == "Legacy personal" && revision == 1);
			string snapshot = library.Snapshot(recipe, "{}");
			JsonLoadContext list = new JsonLoadContext();
			array<int> ids = {};
			AICF_QolChecks.Check(label + "RESTART_LIST", list.LoadFromString(snapshot) && list.ReadValue("ids", ids) && ids.Count() == 1 && ids[0] == 1);
			return;
		}
		super.AICF_QolStorage(faction);
		AICF_QolChecks.Check(label + "LEGACY_ACTIVE", AICF_PersonalLoadoutStore.Save(activePath, recipe, 1));
		AICF_QolChecks.Check(label + "SCALAR_RECIPE_REJECTED", !AICF_LoadoutRecipe.Decode("1") && !AICF_LoadoutRecipe.Decode("[]") && !AICF_LoadoutRecipe.Decode("null"));
		string before = AICF_PersonalLibraryImage.Active(activePath);
		string aiBefore, aiAfter;
		AICF_LoadoutFileImage.Read(aiBefore);
		recipe.m_sName = "Personal A";
		string reason;
		bool savedA = library.Save(recipe, reason);
		Print(string.Format("[AICF][PERSONAL_STORE_DIAG] side=%1 saved=%2 reason=%3 namespace=%4 first_generation=%5 envelope=%6", recipe.m_sFaction,
			savedA, reason, path, FileIO.FileExists(path + "_preset_0_1.json"), AICF_LoadoutStore.MemberPreset(recipe).IsValid()));
		AICF_QolChecks.Check(label + "SAVE_A", savedA && library.Get(0) && library.Get(0).m_sName == "Personal A");
		int firstRevision = library.GetRevision();
		AICF_QolChecks.Check(label + "DEDUPE", library.Save(recipe, reason) && library.GetRevision() == firstRevision);
		AICF_LoadoutRecipe changed = AICF_QolChecks.WithoutHelmet(recipe);
		AICF_QolChecks.Check(label + "UPDATE_A", changed && library.Save(changed, reason) && library.Get(0).Encode() == changed.Encode());
		recipe.m_sName = "Personal B";
		AICF_QolChecks.Check(label + "SAVE_B", library.Save(recipe, reason) && library.Get(1));
		JsonSaveContext invalid = new JsonSaveContext();
		invalid.WriteValue("schemaVersion", 2);
		invalid.WriteValue("version", 3);
		invalid.SaveToFile(path + "_preset_0_1.json");
		AICF_PersonalPresetLibrary recovered = new AICF_PersonalPresetLibrary(path, true);
		AICF_QolChecks.Check(label + "PARTIAL_RECOVERY", recovered.Get(0) && recovered.Get(0).Encode() == changed.Encode());
		AICF_QolChecks.Check(label + "DELETE_A", recovered.Delete(0, reason) && !recovered.Get(0));
		AICF_PersonalPresetLibrary loaded = new AICF_PersonalPresetLibrary(path, true);
		AICF_QolChecks.Check(label + "RELOAD_TOMBSTONE", !loaded.Get(0) && loaded.Get(1) && loaded.Get(1).m_sName == "Personal B");
		AICF_PersonalPresetLibrary memory = new AICF_PersonalPresetLibrary(path, false);
		AICF_QolChecks.Check(label + "SESSION_NO_DISK_READ", !memory.Get(1));
		AICF_QolChecks.Check(label + "SESSION_SAVE", memory.Save(recipe, reason) && memory.Get(0));
		AICF_PersonalPresetLibrary other = new AICF_PersonalPresetLibrary(path, false);
		AICF_QolChecks.Check(label + "SESSION_ISOLATION", !other.Get(0));
		AICF_LoadoutRecipe wrong = AICF_LoadoutRecipe.Decode(recipe.Encode());
		wrong.m_sCharacter = "";
		AICF_QolChecks.Check(label + "CONTEXT_SCOPE", !AICF_PersonalLoadout.SameContext(wrong, recipe));
		AICF_QolChecks.Check(label + "OLD_ACTIVE_BYTES", before != "UNREADABLE" && before == AICF_PersonalLibraryImage.Active(activePath));
		AICF_QolChecks.Check(label + "AI_FILES_UNCHANGED", AICF_LoadoutFileImage.Read(aiAfter) && aiBefore == aiAfter);
	}

	override protected void Update()
	{
		super.Update();
		string reload;
		string seedOnly;
		bool finishSeed = System.GetCLIParam("aicfPersonalLibrarySeedOnly", seedOnly) && seedOnly == "1";
		bool finishReload = System.GetCLIParam("aicfPersonalLibraryReload", reload) && reload == "1";
		if (!m_bRosterReady || m_bPersonalReloadDone || (!finishSeed && !finishReload)) return;
		m_bPersonalReloadDone = true;
		if (finishReload) Print(string.Format("[AICF][PERSONAL_RELOAD_DONE] failures=%1", AICF_QolChecks.s_iFailures));
		else Print(string.Format("[AICF][PERSONAL_SEED_DONE] failures=%1", AICF_QolChecks.s_iFailures));
		GetGame().RequestClose();
	}
}

modded class SCR_GameModeCampaign
{
	protected bool m_bPersonalRpcStarted;
	protected int m_iPersonalRpcAt;
	protected int m_iPersonalRpcStep;
	protected int m_iPersonalRpcToken;
	protected int m_iPersonalIdA = -1;
	protected int m_iPersonalIdB = -1;
	protected int m_iPersonalActiveRevision;
	protected int m_iPersonalLibraryRevision;
	protected string m_sPersonalActiveData;
	protected ref AICF_LoadoutRecipe m_PersonalRpcBase;
	protected ref AICF_LoadoutRecipe m_PersonalRpcA;
	protected ref array<int> m_aPersonalOps = {0, 1, 5, 5, 6, 7, 7, 6, 5, 5, 1, 4, 3, 7, 0, 1};

	override protected void AICF_QolClose()
	{
		string enabled;
		if (!System.GetCLIParam("aicfPersonalLibraryProbe", enabled) || enabled != "1") { super.AICF_QolClose(); return; }
		if (m_bPersonalRpcStarted) return;
		m_bPersonalRpcStarted = true;
		GetGame().GetCallqueue().Remove(AICF_QolTick);
		m_iPersonalRpcAt = System.GetTickCount();
		GetGame().GetCallqueue().CallLater(AICF_PersonalRpcTick, 1500, true);
	}

	protected void AICF_PersonalRpcEnd()
	{
		GetGame().GetCallqueue().Remove(AICF_PersonalRpcTick);
		Print(string.Format("[AICF][PERSONAL_RPC_DONE] failures=%1 steps=%2", AICF_QolChecks.s_iFailures, m_iPersonalRpcStep));
		super.AICF_QolClose();
	}

	protected void AICF_PersonalRpcTick()
	{
		if (System.GetTickCount(m_iPersonalRpcAt) > 180000) { AICF_QolChecks.Check("PERSONAL_RPC_TIMEOUT", false); AICF_PersonalRpcEnd(); return; }
		SCR_PlayerController player = SCR_PlayerController.Cast(GetGame().GetPlayerController());
		if (!player) return;
		SCR_CampaignFaction faction = SCR_CampaignFaction.Cast(SCR_FactionManager.SGetLocalPlayerFaction());
		SCR_PlayerLoadoutComponent component = SCR_PlayerLoadoutComponent.Cast(player.FindComponent(SCR_PlayerLoadoutComponent));
		if (!faction || !component) return;
		array<ref SCR_BasePlayerLoadout> roles = {};
		GetGame().GetLoadoutManager().GetPlayerLoadoutsByFaction(faction, roles);
		SCR_BasePlayerLoadout selected;
		foreach (SCR_BasePlayerLoadout role : roles) { if (!SCR_PlayerArsenalLoadout.Cast(role)) { selected = role; break; } }
		if (!selected) return;
		if (component.GetLoadout() != selected) { component.RequestLoadout(selected); return; }
		if (m_iPersonalRpcToken)
		{
			if (player.AICF_LoadoutToken() != m_iPersonalRpcToken) return;
			int step = m_iPersonalRpcStep;
			bool expected = step != 6 && step != 7 && step != 8 && step != 15;
			AICF_QolChecks.Check("PERSONAL_RPC_STEP_" + step.ToString(), player.AICF_LoadoutAccepted() == expected);
			Print(string.Format("[AICF][PERSONAL_RPC] step=%1 accepted=%2 reason=%3", step, player.AICF_LoadoutAccepted(), player.AICF_LoadoutStatus()));
			if (step == 0)
			{
				m_PersonalRpcBase = AICF_LoadoutRecipe.Decode(player.AICF_LoadoutData());
				AICF_QolChecks.Check("PERSONAL_RPC_BASE", m_PersonalRpcBase != null);
				if (!m_PersonalRpcBase) { AICF_PersonalRpcEnd(); return; }
				m_PersonalRpcBase.m_sName = "Personal active";
			}
			if (step == 1 || step == 10)
			{
				m_sPersonalActiveData = AICF_LoadoutRecipe.Decode(player.AICF_LoadoutData()).Encode();
				m_iPersonalActiveRevision = player.AICF_LoadoutRevision();
			}
			if (step == 2) m_iPersonalIdA = AICF_QolFind(player, "Personal RPC A");
			if (step == 3) m_iPersonalIdB = AICF_QolFind(player, "Personal RPC B");
			if ((step >= 2 && step <= 9) || step == 13 || step == 14)
			{
				AICF_QolChecks.Check("PERSONAL_RPC_ACTIVE_REVISION_" + step.ToString(), player.AICF_LoadoutRevision() == m_iPersonalActiveRevision && player.AICF_PersonalSelected());
				if (step != 4) AICF_QolChecks.Check("PERSONAL_RPC_ACTIVE_DATA_" + step.ToString(), AICF_LoadoutRecipe.Decode(player.AICF_LoadoutData()).Encode() == m_sPersonalActiveData);
			}
			if (step == 4) AICF_QolChecks.Check("PERSONAL_RPC_LOAD_DRAFT", AICF_LoadoutRecipe.Decode(player.AICF_LoadoutData()).m_sName == "Personal RPC A");
			if (step == 11) AICF_QolChecks.Check("PERSONAL_RPC_DEFAULT", !player.AICF_PersonalSelected());
			if (step == 12) AICF_QolChecks.Check("PERSONAL_RPC_SELECT", player.AICF_PersonalSelected());
			m_iPersonalLibraryRevision = AICF_QolRevision(player);
			m_iPersonalRpcToken = 0;
			m_iPersonalRpcStep++;
		}
		if (m_iPersonalRpcStep >= m_aPersonalOps.Count()) { AICF_PersonalRpcEnd(); return; }
		int operation = m_aPersonalOps[m_iPersonalRpcStep];
		int revision = player.AICF_LoadoutRevision();
		int libraryRevision = m_iPersonalLibraryRevision;
		string payload;
		if (m_iPersonalRpcStep == 1) payload = m_PersonalRpcBase.Encode();
		if (m_iPersonalRpcStep == 2 || m_iPersonalRpcStep == 3 || m_iPersonalRpcStep == 8 || m_iPersonalRpcStep == 9)
		{
			m_PersonalRpcA = AICF_LoadoutRecipe.Decode(m_PersonalRpcBase.Encode());
			m_PersonalRpcA.m_sName = "Personal RPC A";
			if (m_iPersonalRpcStep == 3) m_PersonalRpcA.m_sName = "Personal RPC B";
			if (m_iPersonalRpcStep == 8) m_PersonalRpcA.m_sFaction = "USSR";
			if (m_iPersonalRpcStep == 9) m_PersonalRpcA = AICF_QolChecks.WithoutHelmet(m_PersonalRpcA);
			payload = m_PersonalRpcA.Encode();
		}
		if (m_iPersonalRpcStep == 4 || m_iPersonalRpcStep == 6 || m_iPersonalRpcStep == 13) payload = m_iPersonalIdA.ToString();
		if (m_iPersonalRpcStep == 5 || m_iPersonalRpcStep == 7) payload = m_iPersonalIdB.ToString();
		if (m_iPersonalRpcStep == 6) libraryRevision--;
		if (m_iPersonalRpcStep == 10) payload = m_PersonalRpcA.Encode();
		if (m_iPersonalRpcStep == 12) payload = player.AICF_LoadoutData();
		if (m_iPersonalRpcStep == 15) { payload = m_PersonalRpcA.Encode(); revision--; }
		m_iPersonalRpcToken = player.AICF_RequestLoadout(AICF_PersonalLoadout.SLOT, 0, revision, operation, payload, "", libraryRevision);
	}

	override void OnGameEnd()
	{
		GetGame().GetCallqueue().Remove(AICF_PersonalRpcTick);
		super.OnGameEnd();
	}
}
