// Isolated UI fixture, после PersonalLibraryProbe. Только owner RPC и hidden
// metadata; не открывает меню и не вызывает mouse/focus callbacks.
// Вся цепочка проверяет pre-spawn editor: native deployment не должен
// переводить игрока в другой context посреди personal owner RPC.
modded class SCR_RespawnComponent
{
	override bool RequestSpawn(SCR_SpawnData data)
	{
		string flag;
		if (System.GetCLIParam("aicfLibraryPreviewProbe", flag) && flag == "1") return false;
		return super.RequestSpawn(data);
	}
}

class AICF_LibraryPreviewChecks
{
	static bool Empty(AICF_LoadoutRecipe recipe, SCR_CampaignFaction faction, bool weapons)
	{
		AICF_LoadoutDraft draft = new AICF_LoadoutDraft();
		AICF_LoadoutCatalog catalog = new AICF_LoadoutCatalog(faction);
		string reason;
		if (!recipe || recipe.m_aPaths.IsEmpty() || !draft.Build(recipe, catalog, reason)) return false;
		AICF_LoadoutNavigation tree = new AICF_LoadoutNavigation();
		tree.Build(draft.GetCharacter(), catalog);
		InventoryStorageManagerComponent manager = AICF_EquipmentInventory.Manager(draft.GetCharacter());
		foreach (AICF_LoadoutLocation location : tree.m_Locations)
		{
			for (int slot; slot < location.m_Storage.GetSlotsCount(); slot++)
			{
				if (!AICF_LoadoutSlotView.VisibleSlot(location.m_Storage, slot)) continue;
				IEntity item = location.m_Storage.Get(slot);
				if (!item) continue;
				if (!weapons && !manager.CanRemoveItemFromStorage(item, location.m_Storage)) continue;
				if (weapons && item.FindComponent(BaseWeaponComponent)) return false;
				if (!weapons && item.FindComponent(BaseMagazineComponent)) return false;
			}
		}
		return true;
	}

	static AICF_LoadoutRecipe Strip(AICF_LoadoutRecipe original, bool weapons)
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
		InventoryStorageManagerComponent manager = AICF_EquipmentInventory.Manager(draft.GetCharacter());
		// Вложенные адреса снимаются раньше родительских, slots — с конца.
		for (int i = tree.m_Locations.Count() - 1; i >= 0; i--)
		{
			AICF_LoadoutLocation location = tree.m_Locations[i];
			for (int slot = location.m_Storage.GetSlotsCount() - 1; slot >= 0; slot--)
			{
				if (!AICF_LoadoutSlotView.VisibleSlot(location.m_Storage, slot)) continue;
				IEntity item = location.m_Storage.Get(slot);
				if (!item) continue;
				bool remove = BaseMagazineComponent.Cast(item.FindComponent(BaseMagazineComponent)) != null;
				if (remove && !weapons && !manager.CanRemoveItemFromStorage(item, location.m_Storage)) continue;
				if (weapons) remove = BaseWeaponComponent.Cast(item.FindComponent(BaseWeaponComponent)) != null;
				if (remove) recipe.Add(location.m_sPath, location.m_sStorage, slot, "", 1);
			}
		}
		return recipe;
	}
}

modded class AICF_MatchController
{
	override protected void AICF_QolStorage(SCR_CampaignFaction faction)
	{
		super.AICF_QolStorage(faction);
		string flag;
		if (!System.GetCLIParam("aicfLibraryPreviewProbe", flag) || flag != "1") return;
		string role;
		ResourceName source = m_GroupSpawner.ResolveRecruitPrefab(faction, 0, role);
		AICF_LoadoutRecipe base = AICF_LoadoutService.DefaultRecipe(faction, source, 0);
		array<bool> variants = {true, false};
		foreach (bool weapons : variants)
		{
			AICF_LoadoutRecipe recipe = AICF_LibraryPreviewChecks.Strip(base, weapons);
			string reason;
			string label = "NO_MAGAZINES";
			if (weapons) label = "UNARMED";
			string prefix = "LIBRARY_" + base.m_sFaction + "_" + label;
			AICF_QolChecks.Check(prefix + "_EMPTY", AICF_LibraryPreviewChecks.Empty(recipe, faction, weapons));
			AICF_QolChecks.Check(prefix + "_AI_VALIDATE", recipe && AICF_LoadoutService.Validate(recipe, faction, reason));
			AICF_QolChecks.Check(prefix + "_PERSONAL_VALIDATE", recipe && AICF_LoadoutService.Validate(recipe, faction, reason, true));
		}
	}
}

modded class AICF_LoadoutLibraryView
{
	void AICF_MetadataProbe(string prefix, AICF_LoadoutRecipe recipe, AICF_LoadoutCatalog catalog)
	{
		AICF_QolChecks.Check(prefix + "_WORLD", Show(recipe, catalog) && m_wPreview.IsVisible() && m_Preview.IsReady());
		AICF_QolChecks.Check(prefix + "_SUMMARY", m_aLines.Count() >= 5 && !m_aRows[0].GetText().IsEmpty());
		m_iFirst = 9999;
		Render();
		AICF_QolChecks.Check(prefix + "_SCROLL", m_iFirst == Math.Max(0, m_aLines.Count() - ROWS) && m_wScroll.GetCurrent() == m_iFirst);
		Clear();
		AICF_QolChecks.Check(prefix + "_CLEAR", !m_wPreview.IsVisible() && !m_Preview.GetWorld() && !m_Draft.GetCharacter());
	}
}

class AICF_LibraryProbeEditor : AICF_LoadoutEditor
{
	void AICF_LibraryProbeEditor(AICF_StrategicUIController controller) { m_Controller = controller; }

	void Run(Widget hidden, SCR_PlayerController player, AICF_LoadoutRecipe recipe, bool personal)
	{
		m_Player = player;
		m_bPersonal = personal;
		m_iSlotId = 0;
		if (personal) m_iSlotId = AICF_PersonalLoadout.SLOT;
		m_sTargetFaction = recipe.m_sFaction;
		m_Recipe = recipe;
		m_sDraftData = "draft sentinel";
		m_sAppliedData = "active sentinel";
		m_Catalog = new AICF_LoadoutCatalog(SCR_CampaignFaction.Cast(GetGame().GetFactionManager().GetFactionByKey(
			AICF_ContentProfile.GetActive().GetRuntimeFactionKey(recipe.m_sFaction))));
		string prefix = "LIBRARY_WIDGET_AI";
		if (personal) prefix = "LIBRARY_WIDGET_PERSONAL";
		bool built = CreateInspector(hidden);
		AICF_QolChecks.Check(prefix + "_BUILD", built && !m_wRoot.IsVisibleInHierarchy());
		if (!built) { Close(); return; }
		m_LibraryView.AICF_MetadataProbe(prefix, recipe, m_Catalog);
		AICF_QolChecks.Check(prefix + "_DRAFT_UNCHANGED", m_Recipe == recipe && m_sDraftData == "draft sentinel" && m_sAppliedData == "active sentinel");
		Widget input = m_aButtons[0];
		Widget frame = m_aButtonFrames[0];
		ImageWidget background = ImageWidget.Cast(frame.FindAnyWidget("AICF_RectBackground"));
		frame.SetEnabled(true);
		m_wHoveredButton = null;
		RefreshColors();
		int normal = background.GetColor().PackToInt();
		m_wHoveredButton = input;
		RefreshColors();
		int hover = background.GetColor().PackToInt();
		m_wPressedButton = input;
		RefreshColors();
		AICF_QolChecks.Check(prefix + "_FEEDBACK_COLORS", hover != normal && background.GetColor().PackToInt() != hover);
		frame.SetEnabled(false);
		RefreshColors();
		int disabled = background.GetColor().PackToInt();
		m_wHoveredButton = null;
		m_wPressedButton = null;
		RefreshColors();
		AICF_QolChecks.Check(prefix + "_DISABLED", background.GetColor().PackToInt() == disabled);
		Close();
	}
}

modded class SCR_GameModeCampaign
{
	protected bool m_bLibraryRpcStarted;
	protected int m_iLibraryStep;
	protected int m_iLibraryToken;
	protected int m_iLibraryStartedAt;
	protected int m_iLibraryId;
	protected int m_iLibrarySquadId;
	protected int m_iLibraryBeforeRevision;
	protected ref AICF_LoadoutRecipe m_LibraryBase;
	protected ref AICF_LoadoutRecipe m_LibraryCandidate;
	protected ref array<int> m_aLibraryOps = {0,3,8,2,1,3,8,1,6,8,8,8,8,8,8,8,8,8,8,8,0,5,8,6,1,5,8,1,8,8,0};

	override protected void AICF_PersonalRpcEnd()
	{
		string flag;
		if (!System.GetCLIParam("aicfLibraryPreviewProbe", flag) || flag != "1") { super.AICF_PersonalRpcEnd(); return; }
		if (m_bLibraryRpcStarted) return;
		m_bLibraryRpcStarted = true;
		GetGame().GetCallqueue().Remove(AICF_PersonalRpcTick);
		m_iLibraryStartedAt = System.GetTickCount();
		GetGame().GetCallqueue().CallLater(AICF_LibraryRpcTick, 1500, true);
	}

	protected void AICF_LibraryRpcEnd()
	{
		GetGame().GetCallqueue().Remove(AICF_LibraryRpcTick);
		Print(string.Format("[AICF][LIBRARY_RPC_DONE] failures=%1 steps=%2", AICF_QolChecks.s_iFailures, m_iLibraryStep));
		super.AICF_PersonalRpcEnd();
	}

	protected void AICF_LibraryRpcTick()
	{
		if (System.GetTickCount(m_iLibraryStartedAt) > 120000) { AICF_QolChecks.Check("LIBRARY_TIMEOUT", false); AICF_LibraryRpcEnd(); return; }
		SCR_PlayerController player = SCR_PlayerController.Cast(GetGame().GetPlayerController());
		if (!player) return;
		if (m_iLibraryToken)
		{
			if (player.AICF_LoadoutToken() != m_iLibraryToken) return;
			int step = m_iLibraryStep;
			bool expected = step != 19 && step != 28 && step != 29;
			AICF_QolChecks.Check("LIBRARY_RPC_" + step.ToString(), player.AICF_LoadoutAccepted() == expected);
			Print(string.Format("[AICF][LIBRARY_RPC] step=%1 accepted=%2 reason=%3", step, player.AICF_LoadoutAccepted(), player.AICF_LoadoutStatus()));
			if (step == 0 || step == 20) m_LibraryBase = AICF_LoadoutRecipe.Decode(player.AICF_LoadoutData());
			if (step == 1 || step == 5 || step == 21 || step == 25) m_iLibraryId = AICF_QolFind(player, m_LibraryCandidate.m_sName);
			if (step == 8) m_iLibrarySquadId = AICF_QolFind(player, "Library preview squad");
			if (m_aLibraryOps[step] == 8)
			{
				AICF_QolChecks.Check("LIBRARY_READ_ONLY_" + step.ToString(), player.AICF_LoadoutRevision() == m_iLibraryBeforeRevision);
				if (expected) AICF_QolChecks.Check("LIBRARY_STATUS_" + step.ToString(), player.AICF_LoadoutStatus() == "TEMPLATE_INSPECTED");
			}
			if (step == 2 || step == 22)
			{
				Widget hidden = GetGame().GetWorkspace().CreateWidget(WidgetType.FrameWidgetTypeID, WidgetFlags.IGNORE_CURSOR, Color.White, 0);
				hidden.SetVisible(false);
				AICF_InspectorGroupModel model = new AICF_InspectorGroupModel();
				AICF_LibraryProbeEditor editor = new AICF_LibraryProbeEditor(model);
				editor.Run(hidden, player, m_LibraryCandidate, step == 22);
				hidden.RemoveFromHierarchy();
			}
			m_iLibraryToken = 0;
			m_iLibraryStep++;
		}
		if (m_iLibraryStep >= m_aLibraryOps.Count()) { AICF_LibraryRpcEnd(); return; }
		int operation = m_aLibraryOps[m_iLibraryStep];
		int revision = player.AICF_LoadoutRevision();
		int libraryRevision = AICF_QolRevision(player);
		int slot;
		string faction = "US";
		if (m_iLibraryStep >= 20) { slot = AICF_PersonalLoadout.SLOT; faction = ""; }
		string payload;
		if (m_iLibraryStep == 1 || m_iLibraryStep == 5 || m_iLibraryStep == 21 || m_iLibraryStep == 25)
		{
			bool unarmed = m_iLibraryStep == 1 || m_iLibraryStep == 21;
			m_LibraryCandidate = AICF_LibraryPreviewChecks.Strip(m_LibraryBase, unarmed);
			if (!m_LibraryCandidate) { AICF_QolChecks.Check("LIBRARY_STRIP_BUILD", false); AICF_LibraryRpcEnd(); return; }
			m_LibraryCandidate.m_sName = "Library no magazines";
			if (unarmed) m_LibraryCandidate.m_sName = "Library unarmed";
			payload = m_LibraryCandidate.Encode();
		}
		if (operation == 8 || operation == 2 || operation == 6) payload = m_iLibraryId.ToString();
		if (operation == 1) payload = m_LibraryCandidate.Encode();
		if (operation == 8 && slot == 0)
		{
			int position;
			if (m_iLibraryStep >= 9 && m_iLibraryStep <= 18) { position = m_iLibraryStep - 9; payload = m_iLibrarySquadId.ToString(); }
			payload += ":" + position.ToString();
		}
		if (m_iLibraryStep == 8) payload = "Library preview squad";
		if (m_iLibraryStep == 19) libraryRevision--;
		if (m_iLibraryStep == 28) revision--;
		if (m_iLibraryStep == 29) payload = "999999";
		m_iLibraryBeforeRevision = player.AICF_LoadoutRevision();
		m_iLibraryToken = player.AICF_RequestLoadout(slot, 0, revision, operation, payload, faction, libraryRevision);
	}

	override void OnGameEnd()
	{
		GetGame().GetCallqueue().Remove(AICF_LibraryRpcTick);
		super.OnGameEnd();
	}
}
