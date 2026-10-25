// После LibraryPreviewProbe, только isolated stage. Не открывает видимые окна,
// не вызывает input/focus callbacks. Все серверные проверки — через production.
class AICF_TransferProbeRecipe
{
	static AICF_LoadoutRecipe Sparse(AICF_LoadoutRecipe original)
	{
		AICF_LoadoutRecipe recipe = AICF_LoadoutRecipe.Decode(original.Encode());
		SCR_CampaignFaction faction = SCR_CampaignFaction.Cast(GetGame().GetFactionManager().GetFactionByKey(
			AICF_ContentProfile.GetActive().GetRuntimeFactionKey(recipe.m_sFaction)));
		AICF_LoadoutDraft draft = new AICF_LoadoutDraft();
		AICF_LoadoutCatalog catalog = new AICF_LoadoutCatalog(faction);
		string reason;
		if (!draft.Build(recipe, catalog, reason)) return null;
		AICF_LoadoutNavigation tree = new AICF_LoadoutNavigation();
		tree.Build(draft.GetCharacter(), catalog);
		foreach (AICF_LoadoutLocation location : tree.m_Locations)
		{
			if (!location.m_sPath.IsEmpty() && SCR_UniversalInventoryStorageComponent.Cast(location.m_Storage) && location.m_Storage.Get(0) && location.m_Storage.Get(1))
			{
				recipe.Add(location.m_sPath, location.m_sStorage, 0, "", 1);
				return recipe;
			}
		}
		return null;
	}
}

modded class AICF_MatchController
{
	override protected void AICF_QolStorage(SCR_CampaignFaction faction)
	{
		super.AICF_QolStorage(faction);
		string flag;
		if (!System.GetCLIParam("aicfRoleTransferProbe", flag) || flag != "1") return;
		string role;
		ResourceName medic = m_GroupSpawner.ResolveRecruitPrefab(faction, 1, role);
		AICF_LoadoutRecipe recipe = AICF_LoadoutService.DefaultRecipe(faction, medic, 1);
		recipe = AICF_TransferProbeRecipe.Sparse(recipe);
		AICF_QolChecks.Check("ROLETRANSFER_" + faction.GetFactionKey() + "_SPARSE", recipe != null);
		if (!recipe) return;
		recipe.m_sName = "Transfer medic";
		string reason;
		AICF_LoadoutBinding binding = AICF_LoadoutService.Validate(recipe, faction, reason);
		string prefix = "ROLETRANSFER_" + recipe.m_sFaction;
		AICF_QolChecks.Check(prefix + "_SOURCE", binding != null);
		for (int i; i < AICF_Stage1Config.MAX_GROUP_SIZE; i++)
		{
			ResourceName source = m_GroupSpawner.ResolveRecruitPrefab(faction, i, role);
			bool fits = AICF_LoadoutTransfer.Fits(binding, faction, source, reason);
			AICF_QolChecks.Check(prefix + "_FITS_" + i.ToString(), fits);
			if (!fits && binding)
			{
				AICF_LoadoutRecipe target = AICF_LoadoutService.DefaultRecipe(faction, source, i);
				AICF_LoadoutDraft diagnostic = new AICF_LoadoutDraft();
				AICF_LoadoutCatalog diagnosticCatalog = new AICF_LoadoutCatalog(faction);
				string baseline;
				bool built = diagnostic.Build(target, diagnosticCatalog, reason);
				if (built) AICF_LoadoutInventory.Capture(diagnostic.GetCharacter(), baseline);
				bool restored = built && AICF_LoadoutInventory.Restore(diagnostic.GetCharacter(), binding.m_sInventory);
				int ignored;
				Print(string.Format("[AICF][ROLE_COMPAT_DEBUG] side=%1 member=%2 built=%3 restored=%4 reason=%5", recipe.m_sFaction, i, built, restored, reason));
				Print("[AICF][ROLE_SOURCE_INVENTORY] " + binding.m_sInventory);
				Print("[AICF][ROLE_TARGET_INVENTORY] " + baseline);
				Print("[AICF][ROLE_SOURCE_SIGNATURE] " + binding.m_sSignature);
				if (built) Print("[AICF][ROLE_TARGET_SIGNATURE] " + AICF_LoadoutInventory.Signature(diagnostic.GetCharacter(), diagnosticCatalog, ignored));
			}
		}
		AICF_LoadoutStore store = new AICF_LoadoutStore("$profile:RoleTransfer", false);
		store.Save(recipe, reason);
		ResourceName rifleman = m_GroupSpawner.ResolveRecruitPrefab(faction, 9, role);
		AICF_GroupSlot slot = m_USState.GetSlot(0);
		if (recipe.m_sFaction == "USSR") slot = m_USSRState.GetSlot(0);
		AICF_LoadoutRecipe other = AICF_LoadoutService.DefaultRecipe(faction, rifleman, 9);
		array<int> ids = {};
		JsonLoadContext list = new JsonLoadContext();
		AICF_QolChecks.Check(prefix + "_LIBRARY", list.LoadFromString(store.List(other, slot)) && list.ReadValue("ids", ids) && ids.Count() == 1);
		AICF_LoadoutCatalog catalog = new AICF_LoadoutCatalog(faction);
		AICF_LoadoutDraft draft = new AICF_LoadoutDraft();
		array<string> lines = {};
		bool resolved = draft.Build(recipe, catalog, reason);
		if (resolved) AICF_LoadoutSummary.Build(draft.GetCharacter(), catalog, lines);
		foreach (string line : lines)
		{
			if (line.Contains("{AICF:") || line.Contains("#AICF_")) resolved = false;
			Print("[AICF][ROLE_SUMMARY] " + recipe.m_sFaction + " " + line);
		}
		AICF_QolChecks.Check(prefix + "_LABELS", resolved && lines.Count() > 0);
	}

	override void RequestLoadout(SCR_PlayerController player, int token, int slot, int member, int revision, int operation, string payload,
		string targetFaction = "", int libraryRevision = -1)
	{
		string flag;
		if (payload == "ROLE_TRANSFER_BEGIN" && operation == 0 && System.GetCLIParam("aicfRoleTransferProbe", flag) && flag == "1")
			AICF_QolChecks.s_bExtraPhase = true;
		bool phase = AICF_QolChecks.s_bExtraPhase;
		string before, after;
		bool readable;
		if (phase && (operation == 1 || operation == 7 || operation == 9)) readable = AICF_LoadoutFileImage.Read(before);
		super.RequestLoadout(player, token, slot, member, revision, operation, payload, targetFaction, libraryRevision);
		if (!phase) return;
		if (operation == 1 || operation == 7 || operation == 9)
			AICF_QolChecks.Check("ROLETRANSFER_FILES_" + token.ToString(), readable && AICF_LoadoutFileImage.Read(after) && before == after);
		if (operation == 9 && player.AICF_LoadoutAccepted())
		{
			for (int i; i < AICF_Stage1Config.MAX_GROUP_SIZE; i++)
			{
				AICF_LoadoutBinding binding = m_USState.GetSlot(slot).GetLoadout(i);
				AICF_QolChecks.Check("ROLETRANSFER_MEMBER_" + i.ToString(), binding && binding.m_Recipe.m_sName == "Transfer medic");
			}
		}
		if (operation == 7 && player.AICF_LoadoutAccepted())
		{
			AICF_SquadLoadout squad = AICF_SquadLoadout.Decode(m_Loadouts.AICF_QolStore().GetPreset(payload.ToInt()).m_sData);
			AICF_LoadoutRecipe bad = AICF_LoadoutRecipe.Decode(squad.m_aRecipes[9]);
			bad.m_sProfile = "INVALID";
			squad.m_aRecipes[9] = bad.Encode();
			int previous = m_USState.GetSlot(slot).GetLoadoutRevision();
			string reason;
			AICF_QolChecks.Check("ROLETRANSFER_BAD_LAST_ATOMIC", !m_Loadouts.AICF_QolApplySquad(player, slot, squad, reason) && previous == m_USState.GetSlot(slot).GetLoadoutRevision());
		}
	}
}

modded class AICF_LoadoutPreview
{
	float AICF_ProbeTargetHeight() { return m_vTarget[1]; }
}

modded class AICF_LoadoutItemGrid
{
	bool AICF_ProbeThin()
	{
		vector left = FrameSlot.GetAnchorMin(m_ScrollThumb);
		vector right = FrameSlot.GetAnchorMax(m_ScrollThumb);
		vector hitLeft = FrameSlot.GetAnchorMin(m_Scrollbar);
		vector hitRight = FrameSlot.GetAnchorMax(m_Scrollbar);
		return Math.AbsFloat(right[0] - left[0] - 0.017) < 0.001 && hitRight[0] - hitLeft[0] > 0.07;
	}
}

class AICF_TransferEditorProbe : AICF_LoadoutEditor
{
	void AICF_TransferEditorProbe(AICF_StrategicUIController controller) { m_Controller = controller; }
	override protected void FocusSlotCatalog() {}
	bool Begin(Widget hidden, SCR_PlayerController player)
	{
		m_Player = player;
		m_Faction = SCR_FactionManager.SGetLocalPlayerFaction();
		m_PlayerCharacter = player.GetControlledEntity();
		m_sTargetFaction = "US";
		m_iSlotId = 0;
		m_iMember = 0;
		if (!CreateInspector(hidden)) return false;
		m_Catalog = new AICF_LoadoutCatalog(SCR_CampaignFaction.Cast(m_Faction));
		m_ItemAreas = new AICF_LoadoutItemAreas();
		m_Draft = new AICF_LoadoutDraft();
		m_Preview = new AICF_LoadoutPreview();
		Request(0);
		return true;
	}
	bool Ready() { return !m_bPending && m_bDraftReady; }
	void Next(int member) { SelectTarget(0, member); }
	void Check(int phase)
	{
		AICF_QolChecks.Check("ROLETRANSFER_EDITOR_" + phase.ToString(), Ready() && m_Recipe && m_Preview && m_Preview.IsReady());
		if (phase == 0)
		{
			AICF_QolChecks.Check("ROLETRANSFER_RETRY", m_iRequestRetries > 0);
			AICF_QolChecks.Check("ROLETRANSFER_CAMERA", m_Preview.AICF_ProbeTargetHeight() > 0);
			AICF_QolChecks.Check("ROLETRANSFER_THIN", m_Slots.AICF_ProbeThin() && m_Items.AICF_ProbeThin());
		}
	}
}

modded class SCR_GameModeCampaign
{
	protected bool m_bTransferStarted;
	protected int m_iTransferStep;
	protected int m_iTransferToken;
	protected int m_iTransferAt;
	protected int m_iTransferId;
	protected int m_iTransferSquad;
	protected int m_iTransferRevision;
	protected ref AICF_LoadoutRecipe m_TransferRecipe;
	protected ref array<int> m_aTransferOps = {0,3,0,2,1,0,8,9,6,7,9,9,0,9,0};
	protected ref array<int> m_aTransferMembers = {1,1,2,2,2,9,9,9,9,9,9,9,1,0,0};
	protected Widget m_wTransferHidden;
	protected ref AICF_InspectorGroupModel m_TransferModel;
	protected ref AICF_TransferEditorProbe m_TransferEditor;
	protected int m_iTransferPhase;

	override protected void AICF_LibraryRpcEnd()
	{
		string flag;
		if (!System.GetCLIParam("aicfRoleTransferProbe", flag) || flag != "1") { super.AICF_LibraryRpcEnd(); return; }
		if (m_bTransferStarted) return;
		m_bTransferStarted = true;
		AICF_QolChecks.s_bExtraPhase = true;
		GetGame().GetCallqueue().Remove(AICF_LibraryRpcTick);
		m_iTransferAt = System.GetTickCount();
		GetGame().GetCallqueue().CallLater(AICF_TransferTick, 1500, true);
	}

	protected void AICF_TransferEnd()
	{
		GetGame().GetCallqueue().Remove(AICF_TransferTick);
		GetGame().GetCallqueue().Remove(AICF_TransferEditorTick);
		if (m_TransferEditor) m_TransferEditor.Close();
		m_TransferEditor = null;
		m_TransferModel = null;
		if (m_wTransferHidden) m_wTransferHidden.RemoveFromHierarchy();
		m_wTransferHidden = null;
		Print(string.Format("[AICF][ROLETRANSFER_DONE] failures=%1 steps=%2 phases=%3", AICF_QolChecks.s_iFailures, m_iTransferStep, m_iTransferPhase));
		super.AICF_LibraryRpcEnd();
	}

	protected void AICF_TransferTick()
	{
		if (System.GetTickCount(m_iTransferAt) > 120000) { AICF_QolChecks.Check("ROLETRANSFER_TIMEOUT", false); AICF_TransferEnd(); return; }
		SCR_PlayerController player = SCR_PlayerController.Cast(GetGame().GetPlayerController());
		if (!player) return;
		if (m_iTransferToken)
		{
			if (player.AICF_LoadoutToken() != m_iTransferToken) return;
			int step = m_iTransferStep;
			bool accepted = step != 10 && step != 11 && step != 13;
			AICF_QolChecks.Check("ROLETRANSFER_RPC_" + step.ToString(), player.AICF_LoadoutAccepted() == accepted);
			Print(string.Format("[AICF][ROLETRANSFER_RPC] step=%1 accepted=%2 reason=%3", step, player.AICF_LoadoutAccepted(), player.AICF_LoadoutStatus()));
			if (step == 0)
			{
				m_TransferRecipe = AICF_LoadoutRecipe.Decode(player.AICF_LoadoutData());
				m_TransferRecipe = AICF_TransferProbeRecipe.Sparse(m_TransferRecipe);
				if (!m_TransferRecipe) { AICF_QolChecks.Check("ROLETRANSFER_CLIENT_SPARSE", false); AICF_TransferEnd(); return; }
				m_TransferRecipe.m_sName = "Transfer medic";
			}
			if (step == 1) m_iTransferId = AICF_QolFind(player, "Transfer medic");
			if (step == 2) AICF_QolChecks.Check("ROLETRANSFER_VISIBLE", AICF_QolFind(player, "Transfer medic") == m_iTransferId);
			if (step == 3) AICF_QolChecks.Check("ROLETRANSFER_LOAD", AICF_LoadoutRecipe.Decode(player.AICF_LoadoutData()).m_sName == "Transfer medic");
			if (step == 8) m_iTransferSquad = AICF_QolFind(player, "Transfer full squad");
			if (step == 10 || step == 11) AICF_QolChecks.Check("ROLETRANSFER_UNCHANGED_" + step.ToString(), player.AICF_LoadoutRevision() == m_iTransferRevision);
			m_iTransferStep++;
			m_iTransferToken = 0;
		}
		if (m_iTransferStep >= m_aTransferOps.Count())
		{
			GetGame().GetCallqueue().Remove(AICF_TransferTick);
			m_wTransferHidden = GetGame().GetWorkspace().CreateWidget(WidgetType.FrameWidgetTypeID, WidgetFlags.IGNORE_CURSOR, Color.White, 0);
			m_wTransferHidden.SetVisible(false);
			m_TransferModel = new AICF_InspectorGroupModel();
			m_TransferEditor = new AICF_TransferEditorProbe(m_TransferModel);
			// Предыдущий ответ обрабатывается раз в1500ms, уже после cooldown.
			// Свежий read перед editor даёт настоящий owner RATE_LIMITED,
			// а не предполагает, что задержанный ответ ещё занимает cooldown.
			player.AICF_RequestLoadout(0, 0, player.AICF_LoadoutRevision(), 0, "", "US", AICF_QolRevision(player));
			bool ready = m_TransferEditor.Begin(m_wTransferHidden, player);
			AICF_QolChecks.Check("ROLETRANSFER_EDITOR_BUILD", ready);
			if (!ready) { AICF_TransferEnd(); return; }
			GetGame().GetCallqueue().CallLater(AICF_TransferEditorTick, 33, true);
			return;
		}
		int operation = m_aTransferOps[m_iTransferStep];
		int revision = player.AICF_LoadoutRevision();
		int slot;
		string faction = "US";
		string payload;
		if (m_iTransferStep == 0) payload = "ROLE_TRANSFER_BEGIN";
		if (operation == 1 || operation == 3 || operation == 9) payload = m_TransferRecipe.Encode();
		if (operation == 2) payload = m_iTransferId.ToString();
		if (operation == 8) payload = m_iTransferId.ToString() + ":9";
		if (operation == 6) payload = "Transfer full squad";
		if (operation == 7) payload = m_iTransferSquad.ToString();
		m_iTransferRevision = revision;
		if (m_iTransferStep == 10) revision--;
		if (m_iTransferStep == 11)
		{
			AICF_LoadoutRecipe wrong = AICF_LoadoutRecipe.Decode(payload);
			wrong.m_sProfile = "INVALID";
			payload = wrong.Encode();
		}
		if (m_iTransferStep == 13) { slot = AICF_PersonalLoadout.SLOT; faction = ""; }
		m_iTransferToken = player.AICF_RequestLoadout(slot, m_aTransferMembers[m_iTransferStep], revision, operation, payload, faction, AICF_QolRevision(player));
	}

	protected void AICF_TransferEditorTick()
	{
		if (System.GetTickCount(m_iTransferAt) > 120000) { AICF_QolChecks.Check("ROLETRANSFER_EDITOR_TIMEOUT", false); AICF_TransferEnd(); return; }
		m_TransferEditor.Refresh();
		if (!m_TransferEditor.Ready()) return;
		m_TransferEditor.Check(m_iTransferPhase);
		m_iTransferPhase++;
		if (m_iTransferPhase >= 6) { AICF_TransferEnd(); return; }
		m_TransferEditor.Next(m_iTransferPhase % 3);
	}

	override void OnGameEnd()
	{
		GetGame().GetCallqueue().Remove(AICF_TransferTick);
		GetGame().GetCallqueue().Remove(AICF_TransferEditorTick);
		if (m_TransferEditor) m_TransferEditor.Close();
		if (m_wTransferHidden) m_wTransferHidden.RemoveFromHierarchy();
		m_wTransferHidden = null;
		super.OnGameEnd();
	}
}
