// Черновики живут только до закрытия формы и принадлежат точной позиции.
// Новая server revision запрещает перенос устаревшего черновика на её данные.
class AICF_LoadoutDraftMemory
{
	protected ref map<int, string> m_Data = new map<int, string>();
	protected ref map<int, int> m_Revisions = new map<int, int>();

	protected int Key(int slot, int member)
	{
		if (slot < 0 || slot >= AICF_Stage1Config.GROUP_SLOTS_PER_FACTION || member < 0 || member >= AICF_Stage1Config.MAX_GROUP_SIZE)
			return -1;
		return slot * AICF_Stage1Config.MAX_GROUP_SIZE + member;
	}

	void Remember(int slot, int member, int revision, string data)
	{
		int key = Key(slot, member);
		if (key < 0 || revision < 0 || data.IsEmpty() || data.Length() > AICF_LoadoutRecipe.MAX_BYTES)
			return;
		Forget(slot, member);
		m_Data.Insert(key, data);
		m_Revisions.Insert(key, revision);
	}

	string Recover(int slot, int member, int revision, out bool stale)
	{
		stale = false;
		int savedRevision;
		int key = Key(slot, member);
		if (key < 0 || !m_Revisions.Find(key, savedRevision))
			return string.Empty;
		if (savedRevision != revision)
		{
			stale = true;
			Forget(slot, member);
			return string.Empty;
		}
		string data;
		m_Data.Find(key, data);
		return data;
	}

	void Forget(int slot, int member)
	{
		int key = Key(slot, member);
		m_Data.Remove(key);
		m_Revisions.Remove(key);
	}

	void Clear()
	{
		m_Data.Clear();
		m_Revisions.Clear();
	}

	bool HasChanges() { return !m_Data.IsEmpty(); }
}

// Локальный черновик. Единственный внешний side effect формы — player-owned RPC.
class AICF_LoadoutEditor : ScriptedWidgetEventHandler
{
	protected static AICF_LoadoutEditor s_CapturedEditor;
	static bool IsAIEditorOpen() { return s_CapturedEditor && s_CapturedEditor.m_bCaptured && !s_CapturedEditor.m_bPersonal; }
	protected static const ResourceName COMBO = "{4B5AE6E64037FFB4}UI/layouts/WidgetLibrary/ComboBox/WLib_ComboBox.layout";
	protected static const ResourceName EDIT = "{0022F0B45ADBC5AC}UI/layouts/WidgetLibrary/EditBox/WLib_EditBox.layout";
	protected AICF_StrategicUIController m_Controller;
	protected SCR_PlayerController m_Player;
	protected Faction m_Faction;
	protected IEntity m_PlayerCharacter;
	protected SCR_MapCursorModule m_Cursor;
	protected Widget m_wRoot;
	protected Widget m_wPanel;
	protected Widget m_wSave;
	protected TextWidget m_wStatus;
	protected TextWidget m_wCost;
	protected TextWidget m_wQuantity;
	protected TextWidget m_wSelection;
	protected TextWidget m_wTarget;
	protected TextWidget m_wCapacity;
	protected TextWidget m_wCatalogContext;
	protected ref array<TextWidget> m_aRoleCaptions = {};
	protected ref array<TextWidget> m_aRoleKits = {};
	protected ref array<string> m_aAssignedNames = {};
	protected TextWidget m_wRecipient;
	protected Widget m_wModal;
	protected Widget m_wLibrarySection;
	protected Widget m_wMemberSaveSection;
	protected Widget m_wSquadSaveSection;
	protected Widget m_wConfirmSection;
	protected TextWidget m_wModalTitle;
	protected TextWidget m_wModalHelp;
	protected int m_iModal;
	protected int m_iConfirmOperation;
	protected int m_iConfirmLibraryRevision;
	protected int m_iConfirmSlotRevision;
	protected string m_sConfirmPayload;
	protected EditBoxWidget m_wName;
	protected EditBoxWidget m_wSquadName;
	protected EditBoxWidget m_wSearch;
	protected RenderTargetWidget m_wPreview;
	protected ref AICF_LoadoutPreview m_Preview;
	protected SCR_ComboBoxComponent m_Group;
	protected SCR_ComboBoxComponent m_Side;
	protected string m_sTargetFaction;
	protected string m_sLibraryData;
	protected int m_iLibraryRevision;
	protected int m_iOperation;
	protected ref array<string> m_aTemplateKinds = {};
	protected ref array<string> m_aTemplateNames = {};
	protected SCR_ComboBoxComponent m_Member;
	protected SCR_ComboBoxComponent m_Library;
	protected TextWidget m_wLocation;
	protected string m_sViewPath;
	protected bool m_bShowAttachments;
	protected bool m_bContentsView;
	protected bool m_bCanShowContents;
	protected ref array<string> m_aViewPaths = {};
	protected ref array<bool> m_aViewModes = {};
	protected ref AICF_LoadoutNavigation m_Navigation = new AICF_LoadoutNavigation();
	protected ref array<int> m_aSlotLocations = {};
	protected SCR_ComboBoxComponent m_ContentCategory;
	protected bool m_bWeaponFilter;
	protected bool m_bWeaponCategoryOptions;
	protected ref AICF_LoadoutWeaponCategories m_WeaponCategories = new AICF_LoadoutWeaponCategories();
	protected ref array<int> m_aContentTypes = {};
	protected ref array<int> m_aContentModes = {};
	protected ref AICF_LoadoutItemGrid m_Slots;
	protected ref AICF_LoadoutItemGrid m_Items;
	protected ref array<Widget> m_aButtons = {};
	protected ref array<Widget> m_aButtonFrames = {};
	protected ref array<SCR_ComboBoxComponent> m_aCombos = {};
	protected ref array<int> m_aGroupIds = {};
	protected ref array<string> m_aGroupNames = {};
	protected ref AICF_LoadoutDraftMemory m_DraftMemory = new AICF_LoadoutDraftMemory();
	protected ref array<int> m_aSlots = {};
	protected ref array<string> m_aItems = {};
	protected ref array<int> m_aTemplateIds = {};
	protected ref AICF_LoadoutItemAreas m_ItemAreas;
	protected typename m_TargetArea;
	protected string m_sTargetWeaponType;
	protected bool m_bWeaponTarget;
	protected bool m_bAttachmentTarget;
	protected bool m_bContentFilter;
	protected string m_sCatalogFocus;
	protected int m_iTargetMode;
	protected ref AICF_LoadoutDraft m_Draft;
	protected ref AICF_LoadoutRecipe m_Recipe;
	protected string m_sAppliedData;
	protected string m_sDraftData;
	protected ref AICF_LoadoutCatalog m_Catalog;
	protected int m_iSlotId;
	protected int m_iMember;
	protected int m_iRevision;
	protected int m_iToken;
	protected int m_iRequestAt;
	protected int m_iQuantity = 1;
	protected int m_iSourceIndex;
	protected ref array<string> m_aSources = {};
	protected int m_iTargetType;
	protected bool m_bPending;
	protected bool m_bSearchPending;
	protected int m_iSearchRevision;
	protected bool m_bRendering;
	protected bool m_bCaptured;
	protected bool m_bRotating;
	protected bool m_bPanning;
	protected bool m_bDraftReady;
	protected bool m_bPersonal;
	protected ResourceName m_sPersonalSource;
	protected int m_iMouseX;
	protected int m_iMouseY;

	void AICF_LoadoutEditor(AICF_StrategicUIController controller) { m_Controller = controller; }
	bool IsInputCaptured() { return m_bCaptured; }

	void Open(Widget mapRoot, int slotId, bool personal = false, bool opponent = false)
	{
		Close();
		m_bPersonal = personal;
		m_Player = SCR_PlayerController.Cast(GetGame().GetPlayerController());
		m_Faction = SCR_FactionManager.SGetLocalPlayerFaction();
		SCR_MapEntity mapEntity = SCR_MapEntity.GetMapInstance();
		if (!mapRoot || !m_Player || !m_Faction || RplSession.Mode() == RplMode.Dedicated)
			return;
		// Оба редактора захватывают ввод карты, включая карту экрана появления.
		if (!mapEntity || !mapEntity.IsOpen()) return;
		m_Cursor = SCR_MapCursorModule.Cast(mapEntity.GetMapModule(SCR_MapCursorModule));
		if (!m_Cursor || (m_Cursor.GetCursorState() & EMapCursorState.CS_DIALOG)) return;
		if (m_bPersonal)
		{
			SCR_CampaignFaction faction;
			AICF_LoadoutRecipe context = AICF_PersonalLoadout.Context(m_Player, faction);
			if (!context) return;
			m_sPersonalSource = context.m_sCharacter;
		}
		m_PlayerCharacter = m_Player.GetControlledEntity();
		m_iSlotId = slotId;
		m_sTargetFaction = AICF_ContentProfile.GetActive().GetStableFactionKey(m_Faction.GetFactionKey());
		if (opponent && !personal && SCR_Global.IsAdmin(m_Player.GetPlayerId()))
		{
			if (m_sTargetFaction == "US") m_sTargetFaction = "USSR";
			else m_sTargetFaction = "US";
		}
		m_iMember = 0;
		m_Catalog = new AICF_LoadoutCatalog(SCR_CampaignFaction.Cast(GetGame().GetFactionManager().GetFactionByKey(
			AICF_ContentProfile.GetActive().GetRuntimeFactionKey(m_sTargetFaction))));
		m_ItemAreas = new AICF_LoadoutItemAreas();
		m_Draft = new AICF_LoadoutDraft();
		m_Preview = new AICF_LoadoutPreview();
		if (!Create(mapRoot))
		{
			Close();
			return;
		}
		m_bCaptured = true;
		if (m_Cursor) m_Cursor.AICF_SetLoadoutDialog(this);
		s_CapturedEditor = this;
		GetGame().GetInputManager().AddActionListener(UIConstants.MENU_ACTION_BACK, EActionTrigger.DOWN, OnBack);
		Request(0);
	}

	protected void Place(Widget widget, float l, float t, float r, float b)
	{
		FrameSlot.SetAnchorMin(widget, l, t);
		FrameSlot.SetAnchorMax(widget, r, b);
		FrameSlot.SetOffsets(widget, 0, 0, 0, 0);
		widget.SetZOrder(10);
	}

	protected TextWidget Label(float l, float t, float r, float b, string text, int size = 18, Widget parent = null)
	{
		if (!parent) parent = m_wPanel;
		return m_Controller.CreateText(parent, l, t, r, b, text, size, Color.FromSRGBA(230, 237, 242, 255));
	}

	protected Widget Button(string name, string text, float l, float t, float r, float b, Widget parent = null)
	{
		if (!parent) parent = m_wPanel;
		Widget root = m_Controller.CreateRect(parent, l, t, r, b, Color.FromSRGBA(46, 74, 91, 255), true);
		if (!root)
			return null;
		root.SetName(name);
		root.SetZOrder(12);
		m_aButtonFrames.Insert(root);
		TextWidget caption = m_Controller.CreateText(root, 0, 0, 1, 1, text, 18, Color.FromSRGBA(255, 255, 255, 255), true);
		caption.SetName("Caption");
		Widget input = root.FindAnyWidget("AICF_RectInput");
		if (input)
		{
			input.SetName(name);
			input.AddHandler(this);
			m_aButtons.Insert(input);
		}
		return root;
	}

	protected SCR_ComboBoxComponent Combo(float l, float t, float r, float b, Widget parent = null)
	{
		if (!parent) parent = m_wPanel;
		Widget widget = GetGame().GetWorkspace().CreateWidgets(COMBO, parent);
		if (!widget)
			return null;
		Place(widget, l, t, r, b);
		SCR_ComboBoxComponent component = SCR_ComboBoxComponent.Cast(widget.FindHandler(SCR_ComboBoxComponent));
		if (component)
		{
			component.SetLabel(string.Empty);
			if (component.GetLabelWidget())
				component.GetLabelWidget().SetVisible(false);
			component.m_fMaxListHeight = 300;
			component.m_bCreateListBelow = true;
			component.m_OnChanged.Insert(OnChanged);
			m_aCombos.Insert(component);
		}
		return component;
	}

	protected bool Create(Widget parent)
	{
		if (!m_bPersonal) return CreateInspector(parent);
		m_wRoot = m_Controller.CreateRect(parent, 0, 0, 1, 1, Color.FromSRGBA(0, 0, 0, 185), true);
		if (!m_wRoot)
			return false;
		m_wRoot.SetZOrder(400);
		m_wPanel = m_Controller.CreateRect(m_wRoot, 0.045, 0.055, 0.955, 0.945, Color.FromSRGBA(9, 19, 28, 255), true);
		if (!m_wPanel)
			return false;
		// Выше полноэкранной input-поверхности scrim (z=1).
		m_wPanel.SetZOrder(5);
		string title = "{AICF:AICF_UI_SQUAD_LOADOUT_3cb76f02}";
		if (m_bPersonal) title = "{AICF:AICF_UI_PersonalName}";
		Label(0.025, 0.02, 0.30, 0.08, title, 24);
		m_Side = Combo(0.31, 0.025, 0.45, 0.08);
		m_Group = Combo(0.47, 0.025, 0.82, 0.08);
		Button("close", "{AICF:AICF_UI_Close_23b64977}", 0.84, 0.025, 0.975, 0.08);
		m_Member = Combo(0.025, 0.09, 0.29, 0.15);
		m_Library = Combo(0.31, 0.09, 0.56, 0.15);
		Button("load", "{AICF:AICF_UI_Load_8e2c5587}", 0.57, 0.09, 0.68, 0.15);
		Button("deletePreset", "{AICF:AICF_UI_PresetDelete}", 0.69, 0.09, 0.78, 0.15);
		Button("undo", "{AICF:AICF_UI_Undo_950921cc}", 0.79, 0.09, 0.88, 0.15);
		Button("reset", "{AICF:AICF_UI_Default_c3e32002}", 0.89, 0.09, 0.975, 0.15);
		Label(0.025, 0.165, 0.29, 0.20, "{AICF:AICF_UI_PREVIEW_7dc4cefd}", 17);
		Label(0.31, 0.165, 0.465, 0.20, "{AICF:AICF_UI_LOADOUT_6d15a8e1}", 17);
		Button("contents", "{AICF:AICF_UI_LoadoutMounts}", 0.475, 0.165, 0.63, 0.20);
		ShowButton("contents", false);
		Button("back", "{AICF:AICF_UI_Back_b1e3ce09}", 0.31, 0.205, 0.385, 0.255);
		m_wLocation = Label(0.395, 0.205, 0.63, 0.265, "{AICF:AICF_UI_On_soldier_193813db}", 17);
		m_wLocation.SetTextWrapping(true);
		m_aSources.Clear();
		AICF_ContentProfile.GetActive().GetLoadoutSources(m_aSources);
		m_iSourceIndex = m_aSources.Count() - 1;
		float sourceWidth = 0.325 / Math.Max(1, m_aSources.Count());
		foreach (int sourceIndex, string sourceName : m_aSources)
			Button("source_" + sourceIndex.ToString(), sourceName, 0.65 + sourceIndex * sourceWidth, 0.165,
				0.65 + (sourceIndex + 1) * sourceWidth - 0.005, 0.20);
		m_wCatalogContext = Label(0.65, 0.205, 0.975, 0.265, "{AICF:AICF_UI_Select_a_slot_in_the_center_702cc83a}", 17);
		m_wCatalogContext.SetTextWrapping(true);
		m_ContentCategory = Combo(0.65, 0.205, 0.975, 0.265);
		if (!m_ContentCategory)
			return false;
		m_ContentCategory.GetRootWidget().SetVisible(false);
		m_wSearch = Edit(0.65, 0.275, 0.865, 0.33, "{AICF:AICF_UI_Search_by_name_ce5c4685}");
		Button("clearFilters", "{AICF:AICF_UI_Clear_52ecee71}", 0.875, 0.275, 0.975, 0.33);
		if (!m_wSearch)
			return false;
		m_wSearch.AddHandler(this);
		m_Slots = new AICF_LoadoutItemGrid();
		m_Items = new AICF_LoadoutItemGrid();
		m_wCapacity = Label(0.31, 0.268, 0.63, 0.313, "{AICF:AICF_UI_Capacity_879d4588}", 14);
		m_wCapacity.SetTextWrapping(true);
		Button("addMagazine", "{AICF:AICF_UI_Magazine_1efa5341}", 0.475, 0.268, 0.63, 0.313);
		ShowButton("addMagazine", false);
		if (!m_Slots.Create(m_Controller, m_wPanel, 0.31, 0.32, 0.63, 0.745, 1, 8) ||
			!m_Items.Create(m_Controller, m_wPanel, 0.65, 0.34, 0.975, 0.735, 2, 3))
			return false;
		m_Slots.m_OnSelected.Insert(OnCardSelected);
		m_Slots.m_OnActivated.Insert(OnSlotActivated);
		m_Slots.m_OnRemoved.Insert(OnSlotRemoved);
		m_Items.m_OnSelected.Insert(OnCardSelected);
		m_Items.m_OnActivated.Insert(OnItemActivated);
		m_wTarget = Label(0.31, 0.75, 0.63, 0.835, "{AICF:AICF_UI_Select_a_slot_bf11638b}", 16);
		m_wTarget.SetTextWrapping(true);
		m_wSelection = Label(0.65, 0.735, 0.975, 0.79, "{AICF:AICF_UI_Select_an_item_a9a7e1ac}", 16);
		m_wSelection.SetTextWrapping(true);
		Button("less", "-", 0.65, 0.79, 0.68, 0.835);
		m_wQuantity = Label(0.68, 0.79, 0.73, 0.835, "×1", 17);
		Button("more", "+", 0.73, 0.79, 0.76, 0.835);
		Button("apply", "{AICF:AICF_UI_Select_an_item_a9a7e1ac}", 0.77, 0.79, 0.975, 0.835);
		if (m_bPersonal)
			Label(0.025, 0.745, 0.29, 0.84, "{AICF:AICF_UI_Rotate_hold_left_mouse_button_Zoom_scroll__b1449a1e}", 16);
		else
			m_wSquadName = Edit(0.025, 0.79, 0.29, 0.835, "{AICF:AICF_UI_PresetSquadName}");
		m_wPreview = RenderTargetWidget.Cast(GetGame().GetWorkspace().CreateWidget(WidgetType.RenderTargetWidgetTypeID,
			WidgetFlags.VISIBLE | WidgetFlags.BLEND, Color.White, 0, m_wPanel));
		if (!m_wPreview || !m_Group || !m_Member || !m_Library || !m_wLocation || !m_wCatalogContext)
			return false;
		Place(m_wPreview, 0.025, 0.205, 0.29, 0.735);
		m_wPreview.SetFlags(WidgetFlags.VISIBLE | WidgetFlags.BLEND);
		m_wPreview.SetMaxFPS(30);
		m_wPreview.SetResolutionScale(1, 1);
		m_wPreview.SetClearColor(true, Color.BLACK);
		m_wPreview.SetVisible(false);
		m_wPreview.AddHandler(this);
		m_wName = Edit(0.025, 0.855, 0.29, 0.91, "{AICF:AICF_UI_Loadout_name_up_to_80_characters_45993fd3}");
		if (!m_wName)
			return false;
		m_wName.AddHandler(this);
		m_wCost = Label(0.41, 0.855, 0.79, 0.91, "{AICF:AICF_UI_Loadout_free_9cbed783}", 17);
		m_wSave = Button("save", "{AICF:AICF_UI_Save_10c5c91a}", 0.31, 0.855, 0.44, 0.91);
		Button("assign", "{AICF:AICF_UI_PresetApply}", 0.45, 0.855, 0.58, 0.91);
		Button("assignAll", "{AICF:AICF_UI_PresetRoleAll}", 0.59, 0.855, 0.77, 0.91);
		Button("saveSquad", "{AICF:AICF_UI_PresetSaveSquad}", 0.78, 0.855, 0.975, 0.91);
		m_wCost.SetVisible(m_bPersonal);
		m_wStatus = Label(0.025, 0.925, 0.975, 0.985, "", 17);
		m_wStatus.SetTextWrapping(true);
		return InitializeForm();
	}

	protected bool CreateInspector(Widget parent)
	{
		m_wRoot = m_Controller.CreateRect(parent, 0, 0, 1, 1, Color.FromSRGBA(0, 0, 0, 185), true);
		if (!m_wRoot) return false;
		m_wRoot.SetZOrder(400);
		m_wPanel = m_Controller.CreateRect(m_wRoot, 0.025, 0.035, 0.975, 0.965, Color.FromSRGBA(9, 19, 28, 255), true);
		if (!m_wPanel) return false;
		m_wPanel.SetZOrder(5);
		m_Controller.CreateRect(m_wPanel, 0.012, 0.17, 0.233, 0.84, Color.FromSRGBA(16, 27, 34, 255), false);
		Label(0.025, 0.018, 0.60, 0.072, "{AICF:AICF_UI_SQUAD_LOADOUT_3cb76f02}", 26);
		Button("library", "{AICF:AICF_UI_PresetLibrary}", 0.70, 0.022, 0.865, 0.077);
		Button("close", "{AICF:AICF_UI_Close_23b64977}", 0.875, 0.022, 0.975, 0.077);
		Label(0.025, 0.083, 0.16, 0.108, "{AICF:AICF_UI_InspectorSide}", 15);
		Label(0.20, 0.083, 0.39, 0.108, "{AICF:AICF_UI_Squad_daa7fec8}", 15);
		m_Side = Combo(0.025, 0.108, 0.185, 0.16);
		m_Group = Combo(0.20, 0.108, 0.46, 0.16);
		m_wRecipient = Label(0.48, 0.09, 0.975, 0.16, "", 18);
		m_wRecipient.SetTextWrapping(true);
		Label(0.025, 0.177, 0.225, 0.215, "{AICF:AICF_UI_InspectorRecipient}", 19);
		for (int i; i < AICF_Stage1Config.MAX_GROUP_SIZE; i++)
		{
			float y = 0.225 + i * 0.059;
			Widget row = Button("member_" + i.ToString(), "", 0.022, y, 0.225, y + 0.055);
			if (!row) return false;
			TextWidget caption = TextWidget.Cast(row.FindAnyWidget("Caption"));
			Place(caption, 0.035, 0, 0.97, 0.49);
			caption.SetExactFontSize(17);
			m_aRoleCaptions.Insert(caption);
			TextWidget kit = Label(0.035, 0.49, 0.97, 1, "", 14, row);
			kit.SetTextWrapping(true);
			m_aRoleKits.Insert(kit);
		}
		// Скрытый combo сохраняет общий код выбора цели личного и AI редактора.
		m_Member = Combo(0, 0, 0.1, 0.05);
		if (!m_Member) return false;
		m_Member.GetRootWidget().SetVisible(false);
		Label(0.25, 0.177, 0.60, 0.215, "{AICF:AICF_UI_PREVIEW_7dc4cefd}", 18);
		m_wPreview = RenderTargetWidget.Cast(GetGame().GetWorkspace().CreateWidget(WidgetType.RenderTargetWidgetTypeID,
			WidgetFlags.VISIBLE | WidgetFlags.BLEND, Color.White, 0, m_wPanel));
		if (!m_wPreview) return false;
		Place(m_wPreview, 0.25, 0.22, 0.61, 0.455);
		m_wPreview.SetFlags(WidgetFlags.VISIBLE | WidgetFlags.BLEND);
		m_wPreview.SetMaxFPS(30);
		m_wPreview.SetResolutionScale(1, 1);
		m_wPreview.SetClearColor(true, Color.BLACK);
		m_wPreview.SetVisible(false);
		m_wPreview.AddHandler(this);
		Label(0.25, 0.458, 0.61, 0.49, "{AICF:AICF_UI_InspectorPreviewHint}", 14);
		Button("back", "{AICF:AICF_UI_Back_b1e3ce09}", 0.25, 0.50, 0.315, 0.548);
		m_wLocation = Label(0.325, 0.50, 0.46, 0.548, "{AICF:AICF_UI_On_soldier_193813db}", 16);
		m_wLocation.SetTextWrapping(true);
		Button("contents", "{AICF:AICF_UI_LoadoutContents}", 0.47, 0.50, 0.61, 0.548);
		m_wCapacity = Label(0.25, 0.55, 0.46, 0.58, "", 14);
		Button("addMagazine", "{AICF:AICF_UI_Magazine_1efa5341}", 0.47, 0.55, 0.61, 0.59);
		m_Slots = new AICF_LoadoutItemGrid();
		m_Items = new AICF_LoadoutItemGrid();
		if (!m_Slots.Create(m_Controller, m_wPanel, 0.25, 0.595, 0.61, 0.79, 1, 4) ||
			!m_Items.Create(m_Controller, m_wPanel, 0.64, 0.35, 0.975, 0.735, 2, 3)) return false;
		m_Slots.m_OnSelected.Insert(OnCardSelected);
		m_Slots.m_OnActivated.Insert(OnSlotActivated);
		m_Slots.m_OnRemoved.Insert(OnSlotRemoved);
		m_Items.m_OnSelected.Insert(OnCardSelected);
		m_Items.m_OnActivated.Insert(OnItemActivated);
		m_wTarget = Label(0.25, 0.792, 0.61, 0.84, "{AICF:AICF_UI_Select_a_slot_bf11638b}", 15);
		m_wTarget.SetTextWrapping(true);
		m_aSources.Clear();
		AICF_ContentProfile.GetActive().GetLoadoutSources(m_aSources);
		m_iSourceIndex = m_aSources.Count() - 1;
		float sourceWidth = 0.335 / Math.Max(1, m_aSources.Count());
		foreach (int sourceIndex, string sourceName : m_aSources)
			Button("source_" + sourceIndex.ToString(), sourceName, 0.64 + sourceIndex * sourceWidth, 0.177,
				0.64 + (sourceIndex + 1) * sourceWidth - 0.005, 0.225);
		m_wCatalogContext = Label(0.64, 0.23, 0.975, 0.28, "{AICF:AICF_UI_Select_a_slot_in_the_center_702cc83a}", 16);
		m_wCatalogContext.SetTextWrapping(true);
		m_ContentCategory = Combo(0.64, 0.23, 0.975, 0.28);
		if (!m_ContentCategory) return false;
		m_ContentCategory.GetRootWidget().SetVisible(false);
		m_wSearch = Edit(0.64, 0.29, 0.865, 0.34, "{AICF:AICF_UI_Search_by_name_ce5c4685}");
		if (!m_wSearch) return false;
		m_wSearch.AddHandler(this);
		Button("clearFilters", "{AICF:AICF_UI_Clear_52ecee71}", 0.875, 0.29, 0.975, 0.34);
		m_wSelection = Label(0.64, 0.738, 0.975, 0.785, "", 16);
		m_wSelection.SetTextWrapping(true);
		Button("less", "-", 0.64, 0.79, 0.675, 0.84);
		m_wQuantity = Label(0.675, 0.79, 0.72, 0.84, "×1", 17);
		Button("more", "+", 0.72, 0.79, 0.755, 0.84);
		Button("apply", "{AICF:AICF_UI_Select_an_item_a9a7e1ac}", 0.765, 0.79, 0.975, 0.84);
		Button("undo", "{AICF:AICF_UI_Undo_950921cc}", 0.025, 0.86, 0.115, 0.915);
		Button("reset", "{AICF:AICF_UI_Default_c3e32002}", 0.125, 0.86, 0.225, 0.915);
		Button("saveSquad", "{AICF:AICF_UI_PresetSaveSquad}", 0.25, 0.86, 0.43, 0.915);
		m_wSave = Button("save", "{AICF:AICF_UI_Save_10c5c91a}", 0.45, 0.86, 0.61, 0.915);
		Button("assign", "{AICF:AICF_UI_PresetApply}", 0.64, 0.86, 0.835, 0.915);
		Button("assignAll", "{AICF:AICF_UI_InspectorMore}", 0.845, 0.86, 0.975, 0.915);
		m_wCost = Label(0, 0, 0.1, 0.03, "", 16);
		m_wCost.SetVisible(false);
		m_wStatus = Label(0.025, 0.925, 0.975, 0.99, "", 16);
		m_wStatus.SetTextWrapping(true);
		if (!CreateInspectorDialogs()) return false;
		return InitializeForm();
	}

	protected bool CreateInspectorDialogs()
	{
		m_wModal = m_Controller.CreateRect(m_wPanel, 0, 0, 1, 1, Color.FromSRGBA(0, 0, 0, 210), true);
		if (!m_wModal) return false;
		m_wModal.SetZOrder(100);
		Widget panel = m_Controller.CreateRect(m_wModal, 0.22, 0.23, 0.78, 0.73, Color.FromSRGBA(20, 33, 42, 255), true);
		if (!panel) return false;
		panel.SetZOrder(5);
		m_wModalTitle = Label(0.04, 0.035, 0.96, 0.16, "", 24, panel);
		m_wModalHelp = Label(0.04, 0.18, 0.96, 0.39, "", 18, panel);
		m_wModalHelp.SetTextWrapping(true);
		m_wLibrarySection = m_Controller.CreateRect(panel, 0.04, 0.42, 0.96, 0.81, Color.FromSRGBA(20, 33, 42, 255), false);
		m_wMemberSaveSection = m_Controller.CreateRect(panel, 0.04, 0.42, 0.96, 0.81, Color.FromSRGBA(20, 33, 42, 255), false);
		m_wSquadSaveSection = m_Controller.CreateRect(panel, 0.04, 0.42, 0.96, 0.81, Color.FromSRGBA(20, 33, 42, 255), false);
		m_wConfirmSection = m_Controller.CreateRect(panel, 0.04, 0.42, 0.96, 0.81, Color.FromSRGBA(20, 33, 42, 255), false);
		m_Library = Combo(0, 0, 1, 0.35, m_wLibrarySection);
		Button("load", "{AICF:AICF_UI_Load_8e2c5587}", 0, 0.55, 0.52, 0.94, m_wLibrarySection);
		Button("deletePreset", "{AICF:AICF_UI_PresetDelete}", 0.55, 0.55, 1, 0.94, m_wLibrarySection);
		m_wName = Edit(0, 0, 1, 0.35, "{AICF:AICF_UI_Loadout_name_up_to_80_characters_45993fd3}", m_wMemberSaveSection);
		m_wSquadName = Edit(0, 0, 1, 0.35, "{AICF:AICF_UI_PresetSquadName}", m_wSquadSaveSection);
		if (!m_Library || !m_wName || !m_wSquadName) return false;
		m_wName.AddHandler(this);
		m_wSquadName.AddHandler(this);
		Button("confirmSave", "{AICF:AICF_UI_Save_10c5c91a}", 0.50, 0.55, 1, 0.94, m_wMemberSaveSection);
		Button("confirmSquadSave", "{AICF:AICF_UI_PresetSaveSquad}", 0.50, 0.55, 1, 0.94, m_wSquadSaveSection);
		Button("confirmOperation", "{AICF:AICF_UI_InspectorConfirm}", 0.50, 0.55, 1, 0.94, m_wConfirmSection);
		Button("cancelModal", "{AICF:AICF_UI_Close_23b64977}", 0.68, 0.85, 0.96, 0.97, panel);
		m_wModal.SetVisible(false);
		return true;
	}

	protected bool InitializeForm()
	{
		m_bRendering = true;
		if (!m_Side) return false;
		m_Side.AddItem("US");
		m_Side.AddItem("USSR");
		int sideIndex;
		if (m_sTargetFaction == "USSR") sideIndex = 1;
		m_Side.SetCurrentItem(sideIndex, false, false, false);
		m_Side.GetRootWidget().SetVisible(!m_bPersonal);
		if (m_bPersonal)
		{
			m_aGroupIds.Insert(AICF_PersonalLoadout.SLOT);
			m_aGroupNames.Insert("{AICF:AICF_UI_PersonalName}");
			m_Group.GetRootWidget().SetVisible(false);
			m_Member.GetRootWidget().SetVisible(false);
			m_Library.GetRootWidget().SetVisible(false);
			ShowButton("load", false);
			ShowButton("deletePreset", false);
			ShowButton("assign", false);
			ShowButton("assignAll", false);
			ShowButton("saveSquad", false);
			Place(m_wSave, 0.81, 0.855, 0.975, 0.91);
		}
		if (!m_bPersonal) FillGroups();
		int groupIndex = m_aGroupIds.Find(m_iSlotId);
		if (groupIndex < 0)
		{
			m_bRendering = false;
			return false;
		}
		if (!m_bPersonal) m_Group.SetCurrentItem(groupIndex, false, false, false);
		if (!m_bPersonal) PopulateMembers();
		array<string> categories = {};
		AICF_LoadoutContentCategories.Fill(categories, m_aContentTypes, m_aContentModes);
		foreach (string category : categories)
			m_ContentCategory.AddItem(AICF_Localization.Resolve(category));
		m_ContentCategory.SetCurrentItem(0, false, false, false);
		m_bRendering = false;
		RefreshColors();
		return m_wSave && m_wStatus && m_wCost;
	}

	protected void FillGroups()
	{
		m_Group.ClearAll();
		m_aGroupIds.Clear();
		m_aGroupNames.Clear();
		for (int slotId; slotId < AICF_Stage1Config.GROUP_SLOTS_PER_FACTION; slotId++)
		{
			string name;
			int size;
			if (!m_Controller.GetLoadoutGroup(slotId, name, size, m_sTargetFaction)) continue;
			m_aGroupIds.Insert(slotId);
			m_aGroupNames.Insert(name);
			m_Group.AddItem(AICF_Localization.Resolve(name));
		}
	}

	protected void SelectSide(int index)
	{
		if (m_bPersonal || m_bPending || !SCR_Global.IsAdmin(m_Player.GetPlayerId()) || index < 0 || index > 1) return;
		string key = "US";
		if (index == 1) key = "USSR";
		if (key == m_sTargetFaction) return;
		CancelSearch();
		m_sTargetFaction = key;
		m_DraftMemory.Clear();
		m_Recipe = null;
		m_Catalog = new AICF_LoadoutCatalog(SCR_CampaignFaction.Cast(GetGame().GetFactionManager().GetFactionByKey(
			AICF_ContentProfile.GetActive().GetRuntimeFactionKey(key))));
		m_bRendering = true;
		FillGroups();
		m_Group.SetCurrentItem(0, false, false, false);
		m_bRendering = false;
		if (!m_aGroupIds.IsEmpty()) SelectTarget(m_aGroupIds[0], 0);
		else Close();
	}

	protected void PopulateMembers()
	{
		string name;
		int size;
		m_Controller.GetLoadoutGroup(m_iSlotId, name, size, m_sTargetFaction);
		array<string> roles = {"{AICF:AICF_UI_Squad_leader_7b4a4eb4}", "{AICF:AICF_UI_Medic_c00fd754}", "{AICF:AICF_UI_Machine_gunner_108ebf6d}", "{AICF:AICF_UI_AT_specialist_59307692}", "{AICF:AICF_UI_Grenadier_faf2382d}", "{AICF:AICF_UI_Automatic_rifleman_c608ac6e}", "{AICF:AICF_UI_Senior_rifleman_c345ab80}", "{AICF:AICF_UI_Assistant_machine_gunner_3d8f4d8a}", "{AICF:AICF_UI_Assistant_AT_specialist_9a98954f}", "{AICF:AICF_UI_Rifleman_a22efcce}"};
		m_Member.ClearAll();
		for (int i; i < Math.Min(size, roles.Count()); i++)
			m_Member.AddItem(AICF_Localization.Resolve((i + 1).ToString() + ". " + roles[i]));
		for (int row; row < m_aRoleCaptions.Count(); row++)
		{
			m_aRoleCaptions[row].SetText(AICF_Localization.Resolve((row + 1).ToString() + ". " + roles[row]));
			SetButtonState("member_" + row.ToString(), !m_bPending && row < size);
		}
		m_iMember = Math.ClampInt(m_iMember, 0, Math.Max(0, size - 1));
		m_Member.SetCurrentItem(m_iMember, false, false, false);
	}

	protected string TargetName()
	{
		if (m_bPersonal) return "{AICF:AICF_UI_PersonalName}";
		int index = m_aGroupIds.Find(m_iSlotId);
		string group = "{AICF:AICF_UI_Squad_daa7fec8}" + (m_iSlotId + 1).ToString();
		if (index >= 0)
			group = m_aGroupNames[index];
		return group + "{AICF:AICF_UI_position_0f329d22}" + (m_iMember + 1).ToString();
	}

	protected void InspectorRows()
	{
		if (m_bPersonal || !m_wRecipient) return;
		string recipient = m_sTargetFaction + " · " + TargetName();
		if (m_iMember < m_aAssignedNames.Count() && !m_aAssignedNames[m_iMember].IsEmpty()) recipient += "\n" + m_aAssignedNames[m_iMember];
		if (HasDraftChanges()) recipient += " · {AICF:AICF_UI_InspectorDraft}";
		m_wRecipient.SetText(AICF_Localization.Resolve(recipient));
		string groupName;
		int size;
		m_Controller.GetLoadoutGroup(m_iSlotId, groupName, size, m_sTargetFaction);
		for (int i; i < m_aRoleKits.Count(); i++)
		{
			string name = "{AICF:AICF_UI_InspectorDefaultKit}";
			if (i < m_aAssignedNames.Count() && !m_aAssignedNames[i].IsEmpty()) name = m_aAssignedNames[i];
			if (i == m_iMember && HasDraftChanges()) name += " · {AICF:AICF_UI_InspectorDraft}";
			m_aRoleKits[i].SetText(AICF_Localization.Resolve(name));
			SetButtonState("member_" + i.ToString(), !m_bPending && i < size);
		}
	}

	protected void HideModal()
	{
		if (m_iModal != 0)
		{
			string focusAction = "library";
			if (m_iModal == 2) focusAction = "save";
			else if (m_iModal == 3) focusAction = "saveSquad";
			foreach (Widget input : m_aButtons)
			{
				if (input.GetName() == focusAction) GetGame().GetWorkspace().SetFocusedWidget(input);
			}
		}
		if (m_wModal) m_wModal.SetVisible(false);
		m_iModal = 0;
		m_sConfirmPayload = string.Empty;
	}

	protected void ShowModal(int kind, string title, string help)
	{
		if (!m_wModal) return;
		foreach (SCR_ComboBoxComponent combo : m_aCombos) combo.CloseList();
		m_iModal = kind;
		m_wModalTitle.SetText(AICF_Localization.Resolve(title));
		m_wModalHelp.SetText(AICF_Localization.Resolve(help));
		m_wLibrarySection.SetVisible(kind == 1);
		m_wMemberSaveSection.SetVisible(kind == 2);
		m_wSquadSaveSection.SetVisible(kind == 3);
		m_wConfirmSection.SetVisible(kind == 4);
		m_wModal.SetVisible(true);
		if (kind == 2) GetGame().GetWorkspace().SetFocusedWidget(m_wName);
		else if (kind == 3) GetGame().GetWorkspace().SetFocusedWidget(m_wSquadName);
	}

	protected void ConfirmOperation(int operation, string payload, string help)
	{
		m_iConfirmOperation = operation;
		m_iConfirmLibraryRevision = m_iLibraryRevision;
		m_iConfirmSlotRevision = m_iRevision;
		m_sConfirmPayload = payload;
		ShowModal(4, "{AICF:AICF_UI_InspectorConfirm}", help);
		string caption = "{AICF:AICF_UI_Load_8e2c5587}";
		if (operation == 4) caption = "{AICF:AICF_UI_PresetDelete}";
		else if (operation == 5) caption = "{AICF:AICF_UI_PresetRoleAll}";
		else if (operation == 7) caption = "{AICF:AICF_UI_PresetApplySquad}";
		else if (operation == -1) caption = "{AICF:AICF_UI_Close_23b64977}";
		SetButtonState("confirmOperation", true, caption);
	}

	protected void OnBack()
	{
		if (m_iModal != 0) { HideModal(); return; }
		if (!m_bPersonal && (HasDraftChanges() || m_DraftMemory.HasChanges()))
		{
			ConfirmOperation(-1, string.Empty, "{AICF:AICF_UI_InspectorDiscard}");
			return;
		}
		Close();
	}

	protected void RememberDraft()
	{
		if (!m_Recipe || !m_bDraftReady)
			return;
		m_Recipe.m_sName = m_wName.GetText();
		string data = m_Recipe.Encode();
		if (data != m_sAppliedData)
			m_DraftMemory.Remember(m_iSlotId, m_iMember, m_iRevision, data);
		else
			m_DraftMemory.Forget(m_iSlotId, m_iMember);
	}

	protected void SelectTarget(int slotId, int member)
	{
		if (m_bPending || slotId < 0 || !m_aGroupIds.Contains(slotId) || member < 0 || member >= AICF_Stage1Config.MAX_GROUP_SIZE)
			return;
		RememberDraft();
		CancelSearch();
		m_iSlotId = slotId;
		m_iMember = member;
		m_iRevision = -1;
		m_Recipe = null;
		m_sAppliedData = string.Empty;
		m_sDraftData = string.Empty;
		m_bDraftReady = false;
		m_aAssignedNames.Clear();
		m_bRotating = false;
		m_bPanning = false;
		GetGame().GetCallqueue().Remove(RotatePreview);
		GetGame().GetCallqueue().Remove(FocusSlotCatalog);
		m_bRendering = true;
		PopulateMembers();
		m_sViewPath = string.Empty;
		m_aViewPaths.Clear();
		m_aViewModes.Clear();
		m_bShowAttachments = false;
		m_bContentsView = false;
		m_bCanShowContents = false;
		m_Library.ClearAll();
		m_bRendering = false;
		m_aTemplateIds.Clear();
		m_Navigation.Clear();
		m_aSlotLocations.Clear();
		m_aSlots.Clear();
		m_aItems.Clear();
		array<string> empty = {};
		m_Slots.SetItems(empty, empty, empty);
		m_Items.SetItems(empty, empty, empty);
		m_wPreview.SetVisible(false);
		m_wName.SetText("");
		m_wCapacity.SetText(AICF_Localization.Resolve("{AICF:AICF_UI_Capacity_879d4588}"));
		m_wTarget.SetText(AICF_Localization.Resolve("{AICF:AICF_UI_Waiting_for_the_selected_squad_loadout_7e943dc9}"));
		m_wSelection.SetText("");
		m_wSearch.SetText("");
		m_ContentCategory.SetCurrentItem(0, false, false, false);
		m_iQuantity = 1;
		m_wQuantity.SetText(AICF_Localization.Resolve("×1"));
		m_sCatalogFocus = string.Empty;
		Request(0);
	}

	protected EditBoxWidget Edit(float l, float t, float r, float b, string placeholder, Widget parent = null)
	{
		if (!parent) parent = m_wPanel;
		Widget root = GetGame().GetWorkspace().CreateWidgets(EDIT, parent);
		if (!root)
			return null;
		Place(root, l, t, r, b);
		SCR_EditBoxComponent component = SCR_EditBoxComponent.Cast(root.FindHandler(SCR_EditBoxComponent));
		if (component)
			component.UseLabel(false);
		EditBoxWidget input = EditBoxWidget.Cast(root.FindAnyWidget("EditBox"));
		if (input)
			input.SetPlaceholderText(AICF_Localization.Resolve(placeholder));
		return input;
	}

	protected void RefreshColors()
	{
		m_Controller.SetRectColor(m_wRoot, Color.FromSRGBA(0, 0, 0, 185));
		m_Controller.SetRectColor(m_wPanel, Color.FromSRGBA(9, 19, 28, 255));
		foreach (Widget frame : m_aButtonFrames)
		{
			Color color = Color.FromSRGBA(34, 49, 59, 255);
			Color textColor = Color.FromSRGBA(230, 237, 242, 255);
			if (!frame.IsEnabled())
			{
				color = Color.FromSRGBA(23, 32, 39, 255);
				textColor = Color.FromSRGBA(142, 158, 170, 255);
			}
			else if ((m_bPersonal && frame == m_wSave) || (!m_bPersonal && frame.GetName() == "assign"))
			{
				color = Color.FromSRGBA(226, 167, 79, 255);
				textColor = Color.FromSRGBA(9, 19, 28, 255);
			}
			else if (frame.GetName() == "source_" + m_iSourceIndex.ToString())
				color = Color.FromSRGBA(50, 104, 132, 255);
			else if (frame.GetName() == "member_" + m_iMember.ToString())
				color = Color.FromSRGBA(50, 74, 85, 255);
			m_Controller.SetRectColor(frame, color);
			TextWidget caption = TextWidget.Cast(frame.FindAnyWidget("Caption"));
			if (caption)
				caption.SetColor(textColor);
		}
		if (m_Slots)
			m_Slots.RefreshColors();
		if (m_Items)
			m_Items.RefreshColors();
	}

	protected void Request(int operation, string payload = "")
	{
		if (!m_Player || m_bPending)
			return;
		// Даже при отказе RPC выдача должна соответствовать введённому тексту.
		if (m_bSearchPending && m_bDraftReady)
			Items();
		CancelSearch();
		string faction = m_sTargetFaction;
		if (m_bPersonal) faction = string.Empty;
		m_iOperation = operation;
		m_iToken = m_Player.AICF_RequestLoadout(m_iSlotId, m_iMember, m_iRevision, operation, payload, faction, m_iLibraryRevision);
		m_bPending = m_iToken > 0;
		m_iRequestAt = System.GetTickCount();
		m_wStatus.SetText(AICF_Localization.Resolve("{AICF:AICF_UI_Waiting_for_server_response_ded60968}"));
		m_wSave.SetEnabled(false);
		SetBusy();
	}

	protected void SetBusy()
	{
		foreach (SCR_ComboBoxComponent combo : m_aCombos)
			combo.SetEnabled(!m_bPending);
		if (m_Side && !m_bPersonal) m_Side.SetEnabled(!m_bPending && SCR_Global.IsAdmin(m_Player.GetPlayerId()));
		foreach (Widget button : m_aButtons)
		{
			if (button.GetName() != "close" && button.GetName() != "save")
				button.SetEnabled(!m_bPending);
		}
		if (m_wName)
			m_wName.SetEnabled(!m_bPending);
		if (m_wSquadName) m_wSquadName.SetEnabled(!m_bPending);
		if (m_wSearch)
			m_wSearch.SetEnabled(!m_bPending && m_bDraftReady);
		if (m_Slots)
			m_Slots.SetEnabled(!m_bPending && m_bDraftReady);
		if (m_Items)
			m_Items.SetEnabled(!m_bPending && m_bDraftReady && !m_bSearchPending);
		UpdateActions();
	}

	protected void SetButtonState(string name, bool enabled, string text = "")
	{
		foreach (Widget frame : m_aButtonFrames)
		{
			if (frame.GetName() != name)
				continue;
			frame.SetEnabled(enabled);
			TextWidget caption = TextWidget.Cast(frame.FindAnyWidget("Caption"));
			if (caption && !text.IsEmpty())
				caption.SetText(AICF_Localization.Resolve(text));
		}
	}

	protected void ShowButton(string name, bool visible)
	{
		foreach (Widget frame : m_aButtonFrames)
		{
			if (frame.GetName() == name)
				frame.SetVisible(visible);
		}
	}

	protected bool HasDraftChanges()
	{
		if (!m_Recipe || !m_bDraftReady)
			return false;
		// Rebuild обновляет сериализацию один раз; refresh не кодирует весь рецепт.
		return m_sDraftData != m_sAppliedData || m_wName.GetText() != AICF_Localization.Resolve(m_Recipe.m_sName);
	}

	protected bool HasValidName()
	{
		string name = m_wName.GetText();
		return !name.IsEmpty() && name.Length() <= 80 && !name.Contains("\n") && !name.Contains("\r");
	}

	protected void UpdateActions()
	{
		if (!m_Items || !m_Slots)
			return;
		bool ready = !m_bPending && m_bDraftReady;
		AICF_LoadoutLocation location = SelectedLocation();
		int selected = m_Slots.GetSelected();
		bool hasSlot = location && selected >= 0 && selected < m_aSlots.Count();
		int slot = -1;
		if (hasSlot)
			slot = m_aSlots[selected];
		bool occupied = hasSlot && slot >= 0 && AICF_LoadoutClothing.CoveringItem(location.m_Storage, slot);
		bool adding = hasSlot && slot < 0;
		int item = m_Items.GetSelected();
		bool hasItem = !m_bSearchPending && item >= 0 && item < m_aItems.Count();
		bool typeMatches = hasItem;
		if (hasItem && ready)
			typeMatches = MatchesTarget(m_aItems[item]);
		string action = "{AICF:AICF_UI_Equip_c8f1e23d}";
		if (occupied)
			action = "{AICF:AICF_UI_Replace_05ad7ee6}";
		else if (adding)
			action = "{AICF:AICF_UI_Add_2cbc8982}";
		if (!hasItem)
			action = "{AICF:AICF_UI_Select_an_item_a9a7e1ac}";
		else if (!typeMatches)
			action = "{AICF:AICF_UI_Wrong_item_type_2a9edec4}";
		SetButtonState("apply", ready && hasSlot && hasItem && typeMatches, action);
		bool magazineAction = FocusedMuzzle() != null;
		ShowButton("addMagazine", magazineAction);
		SetButtonState("addMagazine", ready && magazineAction);
		float capacityRight = 0.63;
		if (magazineAction)
			capacityRight = 0.465;
		if (m_bPersonal) Place(m_wCapacity, 0.31, 0.268, capacityRight, 0.313);
		else
		{
			capacityRight = 0.61;
			if (magazineAction) capacityRight = 0.46;
			Place(m_wCapacity, 0.25, 0.55, capacityRight, 0.59);
		}
		bool nested = !m_sViewPath.IsEmpty();
		ShowButton("contents", m_bCanShowContents);
		string viewCaption = "{AICF:AICF_UI_LoadoutContents}";
		if (m_bContentsView)
			viewCaption = "{AICF:AICF_UI_LoadoutMounts}";
		SetButtonState("contents", ready && m_bCanShowContents, viewCaption);
		ShowButton("back", nested);
		SetButtonState("back", ready && nested);
		ShowButton("less", adding);
		ShowButton("more", adding);
		m_wQuantity.SetVisible(adding);
		SetButtonState("less", ready && adding && m_iQuantity > 1);
		SetButtonState("more", ready && adding && m_iQuantity < 16);
		SetButtonState("undo", ready && m_Recipe && !m_Recipe.m_aPaths.IsEmpty());
		SetButtonState("reset", ready && m_Recipe && !m_Recipe.m_aPaths.IsEmpty());
		SetButtonState("load", !m_bPending && !m_aTemplateIds.IsEmpty());
		int libraryIndex = m_Library.GetCurrentIndex();
		bool hasPreset = libraryIndex >= 0 && libraryIndex < m_aTemplateIds.Count();
		string loadCaption = "{AICF:AICF_UI_Load_8e2c5587}";
		if (hasPreset && m_aTemplateKinds[libraryIndex] == "squad") loadCaption = "{AICF:AICF_UI_PresetApplySquad}";
		SetButtonState("load", !m_bPending && hasPreset, loadCaption);
		SetButtonState("deletePreset", !m_bPending && hasPreset);
		m_ContentCategory.SetEnabled(ready && HasCategoryFilter());
		bool filtered = !m_wSearch.GetText().IsEmpty() || (HasCategoryFilter() && m_ContentCategory.GetCurrentIndex() > 0);
		ShowButton("clearFilters", filtered);
		SetButtonState("clearFilters", ready && filtered);
		bool changed = HasDraftChanges();
		string saveCaption = "{AICF:AICF_UI_No_changes_c9c43ec8}";
		if (changed)
			saveCaption = "{AICF:AICF_UI_Save_10c5c91a}";
		if (!HasValidName())
			saveCaption = "{AICF:AICF_UI_Enter_a_name_8e8e1dc4}";
		if (!m_bDraftReady)
			saveCaption = "{AICF:AICF_UI_Unavailable_3e05df47}";
		if (m_bPending)
			saveCaption = "{AICF:AICF_UI_Waiting_1b82ef05}";
		bool currentRevision = m_Player && m_Player.AICF_LoadoutRevision() == m_iRevision;
		if (!m_bPersonal) saveCaption = "{AICF:AICF_UI_Save_10c5c91a}";
		SetButtonState("save", ready && (changed || !m_bPersonal) && (!m_bPersonal || HasValidName()) && currentRevision, saveCaption);
		SetButtonState("assign", ready && changed && HasValidName() && currentRevision);
		SetButtonState("assignAll", ready && HasValidName() && currentRevision);
		SetButtonState("saveSquad", ready && m_sDraftData == m_sAppliedData && currentRevision);
		SetButtonState("confirmSave", ready && HasValidName() && currentRevision);
		SetButtonState("confirmSquadSave", ready && m_sDraftData == m_sAppliedData && m_wSquadName &&
			AICF_LoadoutPreset.ValidName(m_wSquadName.GetText()) && currentRevision);
		InspectorRows();
		if (adding)
			m_wQuantity.SetText(AICF_Localization.Resolve("×" + m_iQuantity.ToString()));
		else
			m_wQuantity.SetText(AICF_Localization.Resolve("×1"));
	}

	void Refresh()
	{
		if (!m_wRoot)
			return;
		RefreshColors();
		if (m_bPersonal)
		{
			SCR_CampaignFaction faction;
			AICF_LoadoutRecipe context = AICF_PersonalLoadout.Context(m_Player, faction);
			if (!context || context.m_sCharacter != m_sPersonalSource) { Close(); return; }
		}
		if (!m_Player || GetGame().GetPlayerController() != m_Player || SCR_FactionManager.SGetLocalPlayerFaction() != m_Faction ||
			m_Player.GetControlledEntity() != m_PlayerCharacter)
		{
			Close();
			return;
		}
		if (!m_bPersonal && m_sTargetFaction != AICF_ContentProfile.GetActive().GetStableFactionKey(m_Faction.GetFactionKey()) &&
			!SCR_Global.IsAdmin(m_Player.GetPlayerId())) { Close(); return; }
		if (m_bPending && m_Player.AICF_LoadoutToken() == m_iToken)
		{
			m_bPending = false;
			string status = m_Player.AICF_LoadoutStatus();
			bool libraryOnly = !m_bPersonal && (m_iOperation == 3 || m_iOperation == 4 || m_iOperation == 6);
			if (!m_bPersonal) Library();
			if (!libraryOnly && m_Player.AICF_LoadoutAccepted() && m_Player.AICF_LoadoutSlot() == m_iSlotId && m_Player.AICF_LoadoutMember() == m_iMember)
			{
				m_Recipe = AICF_LoadoutRecipe.Decode(m_Player.AICF_LoadoutData());
				if (m_bPersonal) m_Catalog.SetPersonalRules(m_Player.AICF_LoadoutLibrary());
				m_iRevision = m_Player.AICF_LoadoutRevision();
				// TEMPLATE_LOADED — только черновик, а не новая привязка позиции.
				if (status == "READY" || status == "SAVED" || status == "SAVED_SESSION" || status == "APPLIED" || status == "ROLE_APPLIED" || status == "SQUAD_APPLIED")
				{
					// Сравниваем local canonical JSON: server ResourceName может
					// сериализоваться как GUID, client — как {GUID}path.
					if (m_Recipe) m_sAppliedData = m_Recipe.Encode();
				}
				if (status == "READY")
				{
					bool stale;
					string remembered = m_DraftMemory.Recover(m_iSlotId, m_iMember, m_iRevision, stale);
					AICF_LoadoutRecipe draft = AICF_LoadoutRecipe.Decode(remembered);
					if (draft && m_Recipe && draft.m_sProfile == m_Recipe.m_sProfile && draft.m_sFaction == m_Recipe.m_sFaction && draft.m_sCharacter == m_Recipe.m_sCharacter)
					{
						m_Recipe = draft;
						status = "DRAFT_RESTORED";
					}
					else if (stale)
						status = "DRAFT_OUTDATED";
				}
				else if (status == "SAVED" || status == "SAVED_SESSION" || status == "TEMPLATE_LOADED" || status == "APPLIED" || status == "ROLE_APPLIED" || status == "SQUAD_APPLIED")
					m_DraftMemory.Forget(m_iSlotId, m_iMember);
				if (m_Recipe)
				{
					m_wName.SetText(AICF_Localization.Resolve(m_Recipe.m_sName));
					if (!Rebuild())
						status = "PREVIEW_BUILD_FAILED";
					Library();
				}
			}
			m_wStatus.SetText(AICF_Localization.Resolve(Status(status)));
		}
		else if (m_bPending && System.GetTickCount(m_iRequestAt) > 15000)
		{
			m_bPending = false;
			m_wStatus.SetText(AICF_Localization.Resolve("{AICF:AICF_UI_No_response_received_Reopen_the_form_to_ch_ddc43d41}"));
		}
		if (!m_bPending && m_Recipe && m_Player.AICF_LoadoutRevision() != m_iRevision)
			m_wStatus.SetText(AICF_Localization.Resolve("{AICF:AICF_UI_Another_player_changed_the_loadout_Reopen__22836f72}"));
		if (!m_bPersonal && !m_bPending && m_sLibraryData != m_Player.AICF_LoadoutLibrary()) Library();
		SetBusy();
	}

	protected string Status(string code)
	{
		if (m_bPersonal && code == "SAVED") return "{AICF:AICF_UI_PersonalSaved}";
		if (m_bPersonal && code == "SAVED_SESSION") return "{AICF:AICF_UI_PersonalSavedSession}";
		if (m_bPersonal && code == "READY") return "{AICF:AICF_UI_PersonalHint}";
		if (!m_bPersonal && code == "READY") return "{AICF:AICF_UI_PresetHint}";
		if (code == "APPLIED" || code == "ROLE_APPLIED" || code == "SQUAD_APPLIED") return "{AICF:AICF_UI_PresetApplied}";
		if (code == "TEMPLATE_SAVED" || code == "SQUAD_SAVED") return "{AICF:AICF_UI_PresetSaved}";
		if (code == "TEMPLATE_DELETED") return "{AICF:AICF_UI_PresetDeleted}";
		if (code == "LIBRARY_CONFLICT") return "{AICF:AICF_UI_PresetConflict}";
		if (!m_bPersonal && code == "TEMPLATE_LOADED") return "{AICF:AICF_UI_PresetLoaded}";
		if (!m_bPersonal && code == "DRAFT_RESTORED") return "{AICF:AICF_UI_PresetDraft}";
		switch (code)
		{
			case "SAVED": return TargetName() + "{AICF:AICF_UI_saved_The_next_soldier_spawned_in_this_squ_b04e31ee}";
			case "READY": return TargetName() + "{AICF:AICF_UI_Choose_items_and_save_the_loadout_Select_a_428e264b}";
			case "DRAFT_RESTORED": return TargetName() + "{AICF:AICF_UI_unsaved_draft_restored_Save_to_assign_the__e38ea7d5}";
			case "DRAFT_OUTDATED": return TargetName() + "{AICF:AICF_UI_new_server_loadout_loaded_the_previous_dra_d1139d57}";
			case "PREVIEW_BUILD_FAILED": return "{AICF:AICF_UI_Unable_to_build_the_preview_Reopen_the_edi_37bfc2dd}";
			case "TEMPLATE_LOADED": return "{AICF:AICF_UI_Template_opened_as_a_draft_Click_Save_to_a_d9bfd474}";
			case "REVISION_CONFLICT": return "{AICF:AICF_UI_Another_player_changed_the_loadout_Reopen__90362632}";
			case "RATE_LIMITED": return "{AICF:AICF_UI_Too_many_requests_Try_again_in_a_second_968cb23c}";
			case "ITEM_NOT_ALLOWED": return "{AICF:AICF_UI_Item_is_not_in_your_faction_s_allowed_cata_85004335}";
			case "INVENTORY_INCOMPATIBLE": return "{AICF:AICF_UI_Item_is_incompatible_with_this_slot_or_the_86ddae2c}";
			case "ITEM_CAPACITY_OR_COMPATIBILITY": return "{AICF:AICF_UI_Item_is_incompatible_or_the_container_has__a42e38e0}";
			case "INVENTORY_RESTORE_FAILED": return "{AICF:AICF_UI_The_full_loadout_cannot_be_restored_Previo_ecb424b6}";
			case "INVENTORY_SIGNATURE_MISMATCH": return "{AICF:AICF_UI_Issued_loadout_differs_from_the_draft_Prev_68d39ed2}";
			case "LIBRARY_FULL": return "{AICF:AICF_UI_Library_is_full_256_versions_An_administra_a077b3f6}";
			case "LIBRARY_WRITE_FAILED": return "{AICF:AICF_UI_Server_could_not_write_the_library_Previou_86cc46a0}";
			case "WEAPON_OR_AMMO_MISSING": return "{AICF:AICF_UI_A_primary_weapon_and_compatible_ammunition_36b95035}";
		}
		return "{AICF:AICF_UI_Loadout_not_saved_e19e3fff}" + code;
	}

	protected void Library()
	{
		m_bRendering = true;
		int selected = -1;
		int oldIndex = m_Library.GetCurrentIndex();
		if (oldIndex >= 0 && oldIndex < m_aTemplateIds.Count()) selected = m_aTemplateIds[oldIndex];
		m_Library.ClearAll();
		m_aTemplateIds.Clear();
		m_aTemplateKinds.Clear();
		m_aTemplateNames.Clear();
		JsonLoadContext context = new JsonLoadContext();
		m_sLibraryData = m_Player.AICF_LoadoutLibrary();
		m_aAssignedNames.Clear();
		if (context.LoadFromString(m_sLibraryData) && context.ReadValue("ids", m_aTemplateIds) && context.ReadValue("names", m_aTemplateNames) &&
			context.ReadValue("kinds", m_aTemplateKinds) && context.ReadValue("revision", m_iLibraryRevision) &&
			m_aTemplateIds.Count() == m_aTemplateNames.Count() && m_aTemplateIds.Count() == m_aTemplateKinds.Count())
		{
			context.ReadValue("assignedNames", m_aAssignedNames);
			if (m_aAssignedNames.Count() != AICF_Stage1Config.MAX_GROUP_SIZE || m_Player.AICF_LoadoutSlot() != m_iSlotId ||
				m_Player.AICF_LoadoutFaction() != m_sTargetFaction) m_aAssignedNames.Clear();
			foreach (int i, string name : m_aTemplateNames)
			{
				string prefix;
				if (m_aTemplateKinds[i] == "squad") prefix = "{AICF:AICF_UI_PresetSquadPrefix}";
				m_Library.AddItem(AICF_Localization.Resolve(prefix + name));
			}
		}
		else { m_aTemplateIds.Clear(); m_aTemplateKinds.Clear(); m_aTemplateNames.Clear(); }
		m_Library.SetCurrentItem(Math.Max(0, m_aTemplateIds.Find(selected)), false, false, false);
		m_bRendering = false;
		InspectorRows();
	}

	protected bool Rebuild()
	{
		CancelSearch();
		GetGame().GetCallqueue().Remove(FocusSlotCatalog);
		string selectedPath, selectedStorage;
		int selectedSlot = -2;
		int previousSelection = m_Slots.GetSelected();
		if (previousSelection >= 0 && previousSelection < m_aSlots.Count())
			selectedSlot = m_aSlots[previousSelection];
		AICF_LoadoutLocation previous = SelectedLocation();
		if (previous)
		{
			selectedPath = previous.m_sPath;
			selectedStorage = previous.m_sStorage;
		}
		m_Navigation.Clear();
		m_aSlotLocations.Clear();
		m_bDraftReady = false;
		m_wPreview.SetVisible(false);
		// Отменяем незавершённые binds старых карточек до смены черновика.
		m_Slots.ReleasePreviews();
		m_Items.ReleasePreviews();
		string reason;
		if (!m_Draft.Build(m_Recipe, m_Catalog, reason))
		{
			m_wStatus.SetText(AICF_Localization.Resolve(Status(reason)));
			return false;
		}
		if (!m_Preview.Build(m_Draft.GetCharacter()))
		{
			m_wStatus.SetText(AICF_Localization.Resolve(Status("PREVIEW_BUILD_FAILED")));
			return false;
		}
		m_wPreview.SetWorld(m_Preview.GetWorld(), 0);
		m_wPreview.SetVisible(true);
		m_bDraftReady = true;
		m_sDraftData = m_Recipe.Encode();
		m_Navigation.Clear();
		m_aSlotLocations.Clear();
		m_Navigation.Build(m_Draft.GetCharacter(), m_Catalog);
		while (!m_sViewPath.IsEmpty() && m_Navigation.FindPath(m_sViewPath) < 0)
		{
			m_sViewPath = AICF_LoadoutNavigation.ParentPath(m_sViewPath);
			m_aViewPaths.Clear();
			m_aViewModes.Clear();
			m_bShowAttachments = false;
		}
		Slots();
		SelectAddress(selectedPath, selectedStorage, selectedSlot);
		Selection();
		Items();
		if (selectedSlot < -1)
			FocusSlotCatalog();
		return true;
	}

	protected AICF_LoadoutLocation SelectedLocation()
	{
		int selected = m_Slots.GetSelected();
		if (selected < 0 || selected >= m_aSlotLocations.Count())
			return null;
		return m_Navigation.m_Locations[m_aSlotLocations[selected]];
	}

	protected void SelectAddress(string path, string storage, int slot)
	{
		foreach (int i, int locationIndex : m_aSlotLocations)
		{
			AICF_LoadoutLocation location = m_Navigation.m_Locations[locationIndex];
			if (location.m_sPath == path && location.m_sStorage == storage && m_aSlots[i] == slot)
			{
				m_Slots.Select(i);
				return;
			}
		}
	}

	protected void Slots()
	{
		m_bCanShowContents = m_Navigation.HasContents(m_sViewPath);
		m_bContentsView = m_bCanShowContents && !m_bShowAttachments;
		if (m_bContentsView)
			m_Navigation.ContentRows(m_sViewPath, m_aSlotLocations, m_aSlots);
		else
			m_Navigation.Rows(m_sViewPath, m_aSlotLocations, m_aSlots);
		array<string> prefabs = {}, names = {}, details = {};
		foreach (int row, int locationIndex : m_aSlotLocations)
		{
			AICF_LoadoutLocation location = m_Navigation.m_Locations[locationIndex];
			int slot = m_aSlots[row];
			string prefab, name, detail;
			if (slot < 0)
			{
				name = "{AICF:AICF_UI_Add_to_pockets_d8ca509d}";
				detail = AICF_LoadoutSlotView.CapacityText(location.m_Storage);
			}
			else
			{
				IEntity entity = AICF_LoadoutClothing.CoveringItem(location.m_Storage, slot);
				AICF_LoadoutSlotView.Describe(location.m_Storage, slot, name);
				detail = "{AICF:AICF_UI_Not_equipped_fd8797a7}";
				if (entity)
				{
					prefab = SCR_ResourceNameUtils.GetPrefabName(entity);
					detail = m_Catalog.EntityName(entity);
					if (!m_sViewPath.IsEmpty())
					{
						name = detail;
						if (name == "{AICF:AICF_UI_Pouch_e1c97d80}")
							name += " " + (slot + 1).ToString();
						detail = "{AICF:AICF_UI_Inside_container_de055fff}";
						if (AICF_LoadoutSlotView.HasFixedSlots(location.m_Storage))
							detail = "{AICF:AICF_UI_Mounted_on_equipment_a4b7c7f7}";
						if (m_Navigation.Child(location, slot) >= 0)
							detail += "{AICF:AICF_UI_double_click_to_open_2a053db9}";
					}
				}
				else if (!m_sViewPath.IsEmpty())
				{
					name = "{AICF:AICF_UI_Free_bd8b5b20}" + name;
					detail = "{AICF:AICF_UI_Select_a_compatible_item_on_the_right_b5b60f6f}";
				}
			}
			if (m_bContentsView)
			{
				string pocket = AICF_Localization.Format("{AICF:AICF_UI_LoadoutPocket}", m_Navigation.PocketNumber(m_sViewPath, locationIndex).ToString());
				if (slot < 0)
					name = AICF_Localization.Format("{AICF:AICF_UI_LoadoutAddPocket}", m_Navigation.PocketNumber(m_sViewPath, locationIndex).ToString());
				else
					detail = pocket + " · " + detail;
			}
			prefabs.Insert(prefab);
			names.Insert(name);
			details.Insert(detail);
		}
		string title = "{AICF:AICF_UI_On_soldier_clothing_and_weapons_67780748}";
		int current = m_Navigation.FindPath(m_sViewPath);
		if (!m_sViewPath.IsEmpty() && current >= 0)
			title = m_Navigation.m_Locations[current].m_sName;
		m_wLocation.SetText(AICF_Localization.Resolve(title));
		m_Slots.SetEmptyText("{AICF:AICF_UI_This_item_has_no_available_slots_13de2bcc}");
		m_Slots.SetItems(prefabs, names, details);
		m_Slots.Select(0);
		Selection();
	}

	protected void Items()
	{
		CancelSearch();
		if (!m_bDraftReady)
			return;
		string previous;
		int selected = m_Items.GetSelected();
		if (selected >= 0 && selected < m_aItems.Count())
			previous = m_aItems[selected];
		// Контекст задаёт выбранное место, а не ручная категория. Заполненность
		// контейнера не скрывает каталог; native capacity проверяется при операции.
		int category = m_iTargetType;
		int mode = m_iTargetMode;
		// Цельный костюм может иметь только TORSO metadata, но занимать и LEGS.
		if (!m_bAttachmentTarget && (m_TargetArea == LoadoutJacketArea || m_TargetArea == LoadoutPantsArea))
			category = SCR_EArsenalItemType.TORSO | SCR_EArsenalItemType.LEGS;
		if (m_bContentFilter)
		{
			int filter = Math.ClampInt(m_ContentCategory.GetCurrentIndex(), 0, m_aContentTypes.Count() - 1);
			category = m_aContentTypes[filter];
			mode = m_aContentModes[filter];
		}
		m_Catalog.List(category, m_aItems, mode, m_aSources[m_iSourceIndex]);
		// Дешёвый поиск по имени предшествует native compatibility, которая
		// может создавать preview entities для каждого кандидата.
		string query = m_wSearch.GetText();
		query.ToLower();
		if (!query.IsEmpty())
		{
			for (int i = m_aItems.Count() - 1; i >= 0; i--)
			{
				string searchable = AICF_Localization.Resolve(m_Catalog.Name(m_aItems[i]));
				searchable.ToLower();
				if (!searchable.Contains(query))
					m_aItems.Remove(i);
			}
		}
		if (!m_bAttachmentTarget)
			m_ItemAreas.Filter(m_aItems, m_TargetArea, m_Draft.GetPreview());
		if (m_bWeaponTarget)
			m_ItemAreas.FilterWeapons(m_aItems, m_sTargetWeaponType, m_Draft.GetPreview());
		if (m_bWeaponFilter)
			m_WeaponCategories.Filter(m_aItems, m_ContentCategory.GetCurrentIndex(), m_Draft.GetPreview());
		if (m_bAttachmentTarget)
		{
			AICF_LoadoutLocation location = SelectedLocation();
			InventoryStorageManagerComponent manager = InventoryStorageManagerComponent.Cast(m_Draft.GetCharacter().FindComponent(InventoryStorageManagerComponent));
			m_ItemAreas.FilterAttachments(m_aItems, location.m_Storage, m_aSlots[m_Slots.GetSelected()], m_Draft.GetPreview(), manager);
		}
		array<string> names = {}, details = {};
		foreach (string prefab : m_aItems)
		{
			names.Insert(m_Catalog.Name(prefab));
			details.Insert(string.Empty);
		}
		string empty = "{AICF:AICF_UI_No_suitable_items_for_this_slot_c8584863}";
		if (!query.IsEmpty() || (HasCategoryFilter() && m_ContentCategory.GetCurrentIndex() > 0))
			empty = "{AICF:AICF_UI_No_items_match_the_selected_filters_Clear__28a8009e}";
		m_Items.SetEmptyText(empty);
		bool sameItems = m_Items.SetItems(m_aItems, names, details, true);
		m_Items.Select(m_aItems.Find(previous), !sameItems);
		m_Items.SetEnabled(!m_bPending && m_bDraftReady);
		Selection();
	}

	protected void CancelSearch()
	{
		GetGame().GetCallqueue().Remove(UpdateSearch);
		m_iSearchRevision++;
		m_bSearchPending = false;
	}

	protected void UpdateSearch(int revision)
	{
		if (revision != m_iSearchRevision || !m_bSearchPending || !m_wRoot || m_bPending || !m_bDraftReady)
			return;
		Items();
	}

	protected void OnCardSelected(AICF_LoadoutItemGrid grid, int index)
	{
		Selection();
		if (grid == m_Slots)
		{
			GetGame().GetCallqueue().Remove(FocusSlotCatalog);
			GetGame().GetCallqueue().CallLater(FocusSlotCatalog, 350, false);
		}
	}

	protected void OnItemActivated(AICF_LoadoutItemGrid grid, int index)
	{
		if (grid != m_Items || index != m_Items.GetSelected())
			return;
		ApplySelection(false);
	}

	protected void OnSlotActivated(AICF_LoadoutItemGrid grid, int index)
	{
		if (m_bPending || !m_bDraftReady || grid != m_Slots || index != m_Slots.GetSelected())
			return;
		OpenContainer();
	}

	protected void OnSlotRemoved(AICF_LoadoutItemGrid grid, int index)
	{
		if (m_bPending || !m_bDraftReady || grid != m_Slots || index != m_Slots.GetSelected())
			return;
		ApplySelection(true);
	}

	protected void FocusSlotCatalog()
	{
		GetGame().GetCallqueue().Remove(FocusSlotCatalog);
		if (!m_wRoot || !m_bDraftReady || m_bPending)
			return;
		AICF_LoadoutLocation location = SelectedLocation();
		string focus;
		if (location)
		{
			focus = m_iMember.ToString() + "|" + location.m_sPath + "|" + location.m_sStorage;
			if (m_bContentsView)
				focus = m_iMember.ToString() + "|contents|" + m_sViewPath;
			if (!m_bContentFilter)
				focus += "|" + m_Slots.GetSelected().ToString();
		}
		// Выбор следующего предмета в том же контейнере не сбрасывает поиск.
		if (focus != m_sCatalogFocus)
			m_wSearch.SetText("");
		m_sCatalogFocus = focus;
		Items();
	}

	protected void Selection()
	{
		int item = m_Items.GetSelected();
		string caption = "{AICF:AICF_UI_Select_an_item_in_the_catalog_71483a5f}";
		if (item >= 0 && item < m_aItems.Count())
			caption = m_Catalog.Name(m_aItems[item]);
		m_wSelection.SetText(AICF_Localization.Resolve(caption));
		AICF_LoadoutLocation location = SelectedLocation();
		int selected = m_Slots.GetSelected();
		m_iTargetType = 0;
		m_TargetArea = typename.Empty;
		m_bWeaponTarget = false;
		m_bAttachmentTarget = false;
		m_bContentFilter = location && !location.m_sPath.IsEmpty() && !AICF_LoadoutSlotView.HasFixedSlots(location.m_Storage);
		m_iTargetMode = 0;
		m_sTargetWeaponType = string.Empty;
		caption = "{AICF:AICF_UI_Select_a_slot_on_the_left_71a62f9e}";
		string context = "{AICF:AICF_UI_Select_a_slot_in_the_center_702cc83a}";
		if (location && selected >= 0 && selected < m_aSlots.Count())
		{
			int slot = m_aSlots[selected];
			caption = "{AICF:AICF_UI_Adding_to_a50f9b24}" + location.m_sName;
			context = "{AICF:AICF_UI_Items_for_e73a1d77}" + location.m_sName;
			if (slot >= 0)
			{
				m_iTargetType = AICF_LoadoutSlotView.Describe(location.m_Storage, slot, caption);
				m_TargetArea = AICF_LoadoutSlotView.Area(location.m_Storage, slot);
				if (location.m_sPath.IsEmpty() || AICF_LoadoutSlotView.HasFixedSlots(location.m_Storage))
					context = "{AICF:AICF_UI_Compatible_81a727a9}" + caption;
				m_bWeaponTarget = EquipedWeaponStorageComponent.Cast(location.m_Storage) != null;
				if (m_bWeaponTarget)
				{
					WeaponSlotComponent weapon = AICF_LoadoutSlotView.WeaponSlot(location.m_Storage, slot);
					if (weapon)
						m_sTargetWeaponType = weapon.GetWeaponSlotType();
					AICF_LoadoutSlotView.WeaponCatalog(m_sTargetWeaponType, m_iTargetType, m_iTargetMode);
				}
				m_bAttachmentTarget = !location.m_sPath.IsEmpty() && AICF_LoadoutSlotView.HasFixedSlots(location.m_Storage);
				if (m_bAttachmentTarget)
					AICF_LoadoutSlotView.AttachmentCatalog(location.m_Storage, slot, m_iTargetType, m_iTargetMode);
				if (!location.m_sPath.IsEmpty() && !m_bAttachmentTarget && location.m_Storage.Get(slot))
					caption = m_Catalog.EntityName(location.m_Storage.Get(slot));
				caption = "{AICF:AICF_UI_Selected_c611549c}" + caption;
				if (ChildLocation() >= 0)
					caption += "{AICF:AICF_UI_Double_click_for_contents_f59c8d4a}";
				if (AICF_LoadoutClothing.CoveringItem(location.m_Storage, slot))
					caption += "{AICF:AICF_UI_Right_click_to_remove_Undo_to_restore_df2ffb9b}";
			}
		}
		m_wTarget.SetText(AICF_Localization.Resolve(caption));
		m_wCatalogContext.SetText(AICF_Localization.Resolve(context));
		m_bWeaponFilter = m_bWeaponTarget && m_sTargetWeaponType == "primary";
		UpdateCategoryOptions();
		m_wCatalogContext.SetVisible(!HasCategoryFilter());
		if (!HasCategoryFilter())
			m_ContentCategory.CloseList();
		m_ContentCategory.GetRootWidget().SetVisible(HasCategoryFilter());
		// Внутри контейнера показываем его объём, даже когда выбран предмет.
		// На бойце — карманы выбранной одежды/рюкзака до входа в содержимое.
		string capacityPath = m_sViewPath;
		if (capacityPath.IsEmpty())
		{
			int child = ChildLocation();
			if (child >= 0)
				capacityPath = m_Navigation.m_Locations[child].m_sPath;
		}
		string capacityText = "{AICF:AICF_UI_Capacity_879d4588}";
		if (!capacityPath.IsEmpty())
			capacityText = m_Navigation.CapacityText(capacityPath);
		if (FocusedMuzzle())
			capacityText = m_Navigation.CapacityText(string.Empty);
		m_wCapacity.SetText(AICF_Localization.Resolve(capacityText));
		UpdateActions();
	}

	protected bool HasCategoryFilter()
	{
		return m_bContentFilter || m_bWeaponFilter;
	}

	protected void UpdateCategoryOptions()
	{
		if (m_bWeaponCategoryOptions == m_bWeaponFilter)
			return;
		// Список для карманов и список для двух primary slots независимы.
		// Между основным и вторым оружием сохраняем выбранную категорию.
		m_bWeaponCategoryOptions = m_bWeaponFilter;
		bool rendering = m_bRendering;
		m_bRendering = true;
		m_ContentCategory.CloseList();
		m_ContentCategory.ClearAll();
		array<string> names = {};
		if (m_bWeaponFilter)
			AICF_LoadoutWeaponCategories.Fill(names);
		else
			AICF_LoadoutContentCategories.Fill(names, m_aContentTypes, m_aContentModes);
		foreach (string name : names)
			m_ContentCategory.AddItem(AICF_Localization.Resolve(name));
		m_ContentCategory.SetCurrentItem(0, false, false, false);
		m_bRendering = rendering;
	}

	protected int ChildLocation()
	{
		int selected = m_Slots.GetSelected();
		if (selected < 0 || selected >= m_aSlots.Count())
			return -1;
		return m_Navigation.Child(SelectedLocation(), m_aSlots[selected]);
	}

	protected void OpenContainer()
	{
		int child = ChildLocation();
		if (child < 0)
			return;
		GetGame().GetCallqueue().Remove(FocusSlotCatalog);
		m_aViewPaths.Insert(m_sViewPath);
		m_aViewModes.Insert(m_bShowAttachments);
		m_sViewPath = m_Navigation.m_Locations[child].m_sPath;
		m_bShowAttachments = false;
		Slots();
		FocusSlotCatalog();
	}

	protected void ParentContainer()
	{
		if (m_sViewPath.IsEmpty())
			return;
		string childPath = m_sViewPath;
		int last = m_aViewPaths.Count() - 1;
		if (last >= 0)
		{
			m_sViewPath = m_aViewPaths[last];
			m_bShowAttachments = m_aViewModes[last];
			m_aViewPaths.Remove(last);
			m_aViewModes.Remove(last);
		}
		else
		{
			m_sViewPath = AICF_LoadoutNavigation.ParentPath(childPath);
			m_bShowAttachments = false;
		}
		Slots();
		foreach (int row, int locationIndex : m_aSlotLocations)
		{
			int child = m_Navigation.Child(m_Navigation.m_Locations[locationIndex], m_aSlots[row]);
			if (child >= 0 && m_Navigation.m_Locations[child].m_sPath == childPath)
			{
				m_Slots.Select(row);
				break;
			}
		}
		Selection();
		FocusSlotCatalog();
	}

	override bool OnChange(Widget w, bool finished)
	{
		if (m_bRendering || m_bPending)
			return false;
		if (w == m_wName || w == m_wSquadName)
		{
			UpdateActions();
			RefreshColors();
			return true;
		}
		if (w != m_wSearch)
			return false;
		CancelSearch();
		m_bSearchPending = true;
		// Текст остаётся в widget; callback читает последнее значение.
		// До обновления запрещён выбор предмета из прежней выдачи.
		m_Items.SetEnabled(false);
		UpdateActions();
		GetGame().GetCallqueue().CallLater(UpdateSearch, 150, false, m_iSearchRevision);
		return true;
	}

	protected void OnChanged(SCR_ComboBoxComponent component, int index)
	{
		if (m_bRendering || !m_wRoot)
			return;
		if (component == m_Group && !m_bPending && index >= 0 && index < m_aGroupIds.Count())
			SelectTarget(m_aGroupIds[index], 0);
		else if (component == m_Side && !m_bPending)
			SelectSide(index);
		else if (component == m_Member && !m_bPending)
			SelectTarget(m_iSlotId, index);
		else if (component == m_ContentCategory && !m_bPending && HasCategoryFilter())
			Items();
	}

	override bool OnClick(Widget w, int x, int y, int button)
	{
		if (button != SCR_EMouseButtons.LEFT)
			return false;
		string action = w.GetName();
		if (action == "close")
		{
			OnBack();
			return true;
		}
		if (action == "cancelModal") { HideModal(); return true; }
		if (m_bPending || !m_Recipe || !m_bDraftReady)
			return true;
		if (!m_bPersonal && action.StartsWith("member_"))
		{
			int member = action.Substring(7, action.Length() - 7).ToInt(-1);
			if (member != m_iMember) SelectTarget(m_iSlotId, member);
		}
		else if (!m_bPersonal && action == "library")
			ShowModal(1, "{AICF:AICF_UI_PresetLibrary}", "{AICF:AICF_UI_InspectorLibraryHelp}");
		else if (!m_bPersonal && action == "save")
			ShowModal(2, "{AICF:AICF_UI_Save_10c5c91a}", "{AICF:AICF_UI_InspectorSaveHelp}");
		else if (!m_bPersonal && action == "saveSquad")
		{
			if (m_wSquadName.GetText().IsEmpty()) m_wSquadName.SetText(AICF_Localization.Resolve("{AICF:AICF_UI_PresetSquadName}"));
			ShowModal(3, "{AICF:AICF_UI_PresetSaveSquad}", "{AICF:AICF_UI_InspectorSquadSaveHelp}");
		}
		else if (!m_bPersonal && action == "assignAll")
		{
			if (!HasValidName() || m_Player.AICF_LoadoutRevision() != m_iRevision) return true;
			m_Recipe.m_sName = m_wName.GetText();
			m_sDraftData = m_Recipe.Encode();
			ConfirmOperation(5, m_sDraftData, m_sTargetFaction + " · " + TargetName() + "\n{AICF:AICF_UI_InspectorRoleAllHelp}");
		}
		else if (!m_bPersonal && action == "confirmOperation")
		{
			int confirmedOperation = m_iConfirmOperation;
			string confirmedPayload = m_sConfirmPayload;
			bool libraryOperation = confirmedOperation == 2 || confirmedOperation == 4 || confirmedOperation == 7;
			HideModal();
			if (confirmedOperation == -1) Close();
			else if (m_iConfirmSlotRevision != m_iRevision || m_Player.AICF_LoadoutRevision() != m_iConfirmSlotRevision ||
				(libraryOperation && m_iConfirmLibraryRevision != m_iLibraryRevision))
				m_wStatus.SetText(AICF_Localization.Resolve("{AICF:AICF_UI_PresetConflict}"));
			else Request(confirmedOperation, confirmedPayload);
			return true;
		}
		else if (action.StartsWith("source_"))
		{
			int sourceIndex = action.Substring(7, action.Length() - 7).ToInt(-1);
			if (m_aSources.IsIndexValid(sourceIndex))
			{
				m_iSourceIndex = sourceIndex;
				Items();
			}
		}
		else if (action == "save" || action == "assign" || action == "assignAll" || action == "confirmSave")
		{
			if ((m_bPersonal && !HasDraftChanges()) || !HasValidName() || m_Player.AICF_LoadoutRevision() != m_iRevision)
				return true;
			m_Recipe.m_sName = m_wName.GetText();
			m_sDraftData = m_Recipe.Encode();
			int operation = 1;
			if (!m_bPersonal && (action == "save" || action == "confirmSave")) operation = 3;
			if (!m_bPersonal && action == "assignAll") operation = 5;
			HideModal();
			Request(operation, m_Recipe.Encode());
		}
		else if (action == "confirmSquadSave" && !m_bPersonal)
		{
			if (m_sDraftData == m_sAppliedData && m_wSquadName && AICF_LoadoutPreset.ValidName(m_wSquadName.GetText()))
			{
				HideModal();
				Request(6, m_wSquadName.GetText());
			}
		}
		else if (action == "load" || action == "deletePreset")
		{
			int index = m_Library.GetCurrentIndex();
			if (index >= 0 && index < m_aTemplateIds.Count())
			{
				int libraryOperation = 2;
				if (m_aTemplateKinds[index] == "squad") libraryOperation = 7;
				if (action == "deletePreset") libraryOperation = 4;
				if (libraryOperation == 7 && m_wSquadName) m_wSquadName.SetText(m_aTemplateNames[index]);
				string presetName = m_aTemplateNames[index];
				if (libraryOperation == 4)
					ConfirmOperation(libraryOperation, m_aTemplateIds[index].ToString(), presetName + "\n{AICF:AICF_UI_InspectorDeleteHelp}");
				else if (libraryOperation == 7)
					ConfirmOperation(libraryOperation, m_aTemplateIds[index].ToString(), m_sTargetFaction + " · " + TargetName() + "\n" + presetName + "\n{AICF:AICF_UI_InspectorSquadApplyHelp}");
				else if (HasDraftChanges())
					ConfirmOperation(libraryOperation, m_aTemplateIds[index].ToString(), presetName + "\n{AICF:AICF_UI_InspectorLoadHelp}");
				else { HideModal(); Request(libraryOperation, m_aTemplateIds[index].ToString()); }
			}
		}
		else if (action == "less" || action == "more")
		{
			int delta = 1;
			if (action == "less")
				delta = -1;
			m_iQuantity = Math.ClampInt(m_iQuantity + delta, 1, 16);
			m_wQuantity.SetText(AICF_Localization.Resolve("×" + m_iQuantity.ToString()));
		}
		else if (action == "back")
			ParentContainer();
		else if (action == "contents" && m_bCanShowContents)
		{
			CancelSearch();
			GetGame().GetCallqueue().Remove(FocusSlotCatalog);
			m_bShowAttachments = !m_bShowAttachments;
			Slots();
			FocusSlotCatalog();
		}
		else if (action == "undo" || action == "reset")
		{
			m_Recipe.Undo();
			if (action == "reset")
			{
				while (!m_Recipe.m_aPaths.IsEmpty())
					m_Recipe.Undo();
			}
			Rebuild();
		}
		else if (action == "apply")
			ApplySelection(false);
		else if (action == "addMagazine")
			AddMagazine();
		else if (action == "clearFilters")
		{
			m_wSearch.SetText("");
			m_ContentCategory.SetCurrentItem(0, false, false, false);
			Items();
		}
		UpdateActions();
		RefreshColors();
		return true;
	}

	protected BaseMuzzleComponent FocusedMuzzle()
	{
		if (!m_bDraftReady || !m_Draft || !m_Draft.GetCharacter())
			return null;
		AICF_LoadoutLocation location = SelectedLocation();
		int selected = m_Slots.GetSelected();
		if (!location || selected < 0 || selected >= m_aSlots.Count())
			return null;
		int slot = m_aSlots[selected];
		IEntity item;
		if (slot >= 0)
			item = AICF_LoadoutClothing.CoveringItem(location.m_Storage, slot);
		BaseWeaponComponent weapon;
		if (item)
			weapon = BaseWeaponComponent.Cast(item.FindComponent(BaseWeaponComponent));
		// Внутри обвесов контекстом остаётся открытое оружие.
		string path = m_sViewPath;
		while (!weapon && !path.IsEmpty())
		{
			IEntity owner = AICF_LoadoutInventory.ResolvePath(m_Draft.GetCharacter(), path);
			if (owner)
				weapon = BaseWeaponComponent.Cast(owner.FindComponent(BaseWeaponComponent));
			path = AICF_LoadoutNavigation.ParentPath(path);
		}
		if (!weapon)
			return null;
		BaseMuzzleComponent muzzle = weapon.GetCurrentMuzzle();
		if (slot >= 0 && location.m_Owner == weapon.GetOwner())
		{
			BaseMuzzleComponent focused = BaseMuzzleComponent.Cast(location.m_Storage.GetSlot(slot).GetParentContainer());
			if (focused)
				muzzle = focused;
		}
		if (!muzzle || muzzle.IsDisposable() || !muzzle.GetMagazineWell())
			return null;
		return muzzle;
	}

	protected void AddMagazine()
	{
		if (m_bPending || !m_Recipe || !m_bDraftReady)
			return;
		if (m_Recipe.m_aPaths.Count() >= AICF_LoadoutRecipe.MAX_OPERATIONS)
		{
			m_wStatus.SetText(AICF_Localization.Resolve("{AICF:AICF_UI_Template_change_limit_reached_Undo_a_step__c54854ad}"));
			return;
		}
		ResourceName prefab;
		AICF_LoadoutLocation destination;
		string reason;
		if (!AICF_LoadoutMagazines.Resolve(m_Draft, m_Catalog, m_Navigation, FocusedMuzzle(), prefab, destination, reason))
		{
			m_wStatus.SetText(AICF_Localization.Resolve(reason));
			return;
		}
		string targetName = destination.m_sName;
		m_Recipe.Add(destination.m_sPath, destination.m_sStorage, -1, prefab, 1);
		if (!Rebuild())
		{
			m_Recipe.Undo();
			Rebuild();
			m_wStatus.SetText(AICF_Localization.Resolve(Status("INVENTORY_INCOMPATIBLE")));
			return;
		}
		if (!m_bPersonal) m_wStatus.SetText(AICF_Localization.Resolve("{AICF:AICF_UI_PresetDraft}"));
		else m_wStatus.SetText(AICF_Localization.Resolve(AICF_Localization.Format("{AICF:AICF_UI_Added_1_Click_Save_to_apply_the_loadout_58267f08}", string.Format("%1", m_Catalog.Name(prefab)), string.Format("%1", targetName))));
	}

	protected bool MatchesTarget(ResourceName prefab)
	{
		if (m_bAttachmentTarget)
		{
			AICF_LoadoutLocation location = SelectedLocation();
			int selected = m_Slots.GetSelected();
			if (!location || selected < 0 || selected >= m_aSlots.Count())
				return false;
			InventoryStorageManagerComponent manager = InventoryStorageManagerComponent.Cast(m_Draft.GetCharacter().FindComponent(InventoryStorageManagerComponent));
			return m_ItemAreas.MatchesAttachment(prefab, location.m_Storage, m_aSlots[selected], m_Draft.GetPreview(), manager);
		}
		if (m_bWeaponTarget)
			return !m_sTargetWeaponType.IsEmpty() && m_ItemAreas.WeaponType(prefab, m_Draft.GetPreview()) == m_sTargetWeaponType;
		if (m_TargetArea != typename.Empty)
			return m_ItemAreas.MatchesArea(prefab, m_TargetArea, m_Draft.GetPreview());
		if (m_iTargetType == 0)
			return true;
		if ((m_Catalog.ItemType(prefab) & m_iTargetType) == 0)
			return false;
		if (m_iTargetType != SCR_EArsenalItemType.VEST_AND_WAIST || m_TargetArea == typename.Empty)
			return true;
		typename actual = m_ItemAreas.Get(prefab, m_Draft.GetPreview());
		return actual != typename.Empty && actual.IsInherited(m_TargetArea);
	}

	// Общий путь кнопки и двойного клика: никакого обхода pending/slot gates.
	protected void ApplySelection(bool removeItem)
	{
		if (m_bPending || !m_Recipe || !m_bDraftReady)
			return;
		if (!removeItem && m_bSearchPending)
			return;
		AICF_LoadoutLocation location = SelectedLocation();
		int selected = m_Slots.GetSelected();
		int item = m_Items.GetSelected();
		if (!location || selected < 0 || selected >= m_aSlots.Count())
			return;
		if (m_Recipe.m_aPaths.Count() >= AICF_LoadoutRecipe.MAX_OPERATIONS)
		{
			m_wStatus.SetText(AICF_Localization.Resolve("{AICF:AICF_UI_Template_change_limit_reached_Undo_a_step__c54854ad}"));
			return;
		}
		ResourceName prefab;
		int count = 1;
		if (!removeItem)
		{
			if (item < 0 || item >= m_aItems.Count())
			{
				m_wStatus.SetText(AICF_Localization.Resolve("{AICF:AICF_UI_Select_an_item_card_in_the_catalog_on_the__8220ecd1}"));
				return;
			}
			prefab = m_aItems[item];
			if (!MatchesTarget(prefab))
			{
				m_wStatus.SetText(AICF_Localization.Resolve(Status("INVENTORY_INCOMPATIBLE")));
				return;
			}
			if (m_aSlots[selected] < 0)
				count = m_iQuantity;
		}
		else if (m_aSlots[selected] < 0 || !AICF_LoadoutClothing.CoveringItem(location.m_Storage, m_aSlots[selected]))
			return;
		m_Recipe.Add(location.m_sPath, location.m_sStorage, m_aSlots[selected], prefab, count);
		if (!Rebuild())
		{
			m_Recipe.Undo();
			Rebuild();
			m_wStatus.SetText(AICF_Localization.Resolve(Status("INVENTORY_INCOMPATIBLE")));
		}
		else
		{
			string changedStatus = "{AICF:AICF_UI_Draft_changed_Click_Save_to_assign_the_loa_31f65d3e}";
			if (!m_bPersonal) changedStatus = "{AICF:AICF_UI_PresetDraft}";
			m_wStatus.SetText(AICF_Localization.Resolve(changedStatus));
		}
		UpdateActions();
		RefreshColors();
	}

	override bool OnMouseButtonDown(Widget w, int x, int y, int button)
	{
		if (w != m_wPreview || (button != SCR_EMouseButtons.LEFT && button != SCR_EMouseButtons.MIDDLE))
			return false;
		m_bRotating = button == SCR_EMouseButtons.LEFT;
		m_bPanning = button == SCR_EMouseButtons.MIDDLE;
		m_iMouseX = x;
		m_iMouseY = y;
		GetGame().GetCallqueue().Remove(RotatePreview);
		GetGame().GetCallqueue().CallLater(RotatePreview, 33, true);
		return true;
	}

	override bool OnMouseButtonUp(Widget w, int x, int y, int button)
	{
		if (button == SCR_EMouseButtons.LEFT)
			m_bRotating = false;
		if (button == SCR_EMouseButtons.MIDDLE)
			m_bPanning = false;
		if (!m_bRotating && !m_bPanning)
			GetGame().GetCallqueue().Remove(RotatePreview);
		return w == m_wPreview;
	}

	override bool OnMouseLeave(Widget w, Widget enterW, int x, int y)
	{
		if (w != m_wPreview)
			return false;
		m_bRotating = false;
		m_bPanning = false;
		GetGame().GetCallqueue().Remove(RotatePreview);
		return w == m_wPreview;
	}

	protected void RotatePreview()
	{
		if (!m_wPreview || (!m_bRotating && !m_bPanning) || !m_Preview || !m_bDraftReady)
			return;
		int x, y;
		WidgetManager.GetMousePos(x, y);
		if (x == m_iMouseX && y == m_iMouseY)
			return;
		if (m_bRotating && x != m_iMouseX)
			m_Preview.Rotate((x - m_iMouseX) * 0.5);
		if (m_bPanning && y != m_iMouseY)
		{
			float width, height;
			m_wPreview.GetScreenSize(width, height);
			if (height > 0)
				m_Preview.Pan((y - m_iMouseY) / height);
		}
		m_iMouseX = x;
		m_iMouseY = y;
	}

	override bool OnMouseWheel(Widget w, int x, int y, int wheel)
	{
		if (w != m_wPreview || !m_Preview || !m_bDraftReady)
			return false;
		m_Preview.Zoom(-wheel * 3);
		return true;
	}

	void Close()
	{
		if (s_CapturedEditor == this) s_CapturedEditor = null;
		CancelSearch();
		m_bRotating = false;
		m_bPanning = false;
		GetGame().GetCallqueue().Remove(RotatePreview);
		GetGame().GetCallqueue().Remove(FocusSlotCatalog);
		if (m_bCaptured)
			GetGame().GetInputManager().RemoveActionListener(UIConstants.MENU_ACTION_BACK, EActionTrigger.DOWN, OnBack);
		foreach (SCR_ComboBoxComponent combo : m_aCombos)
		{
			combo.CloseList();
			combo.m_OnChanged.Remove(OnChanged);
		}
		m_aCombos.Clear();
		foreach (Widget button : m_aButtons)
			button.RemoveHandler(this);
		m_aButtons.Clear();
		m_aButtonFrames.Clear();
		if (m_wPreview)
			m_wPreview.RemoveHandler(this);
		if (m_wSearch)
			m_wSearch.RemoveHandler(this);
		if (m_wName)
			m_wName.RemoveHandler(this);
		if (m_wSquadName) m_wSquadName.RemoveHandler(this);
		if (m_Slots)
		{
			m_Slots.m_OnSelected.Remove(OnCardSelected);
			m_Slots.m_OnActivated.Remove(OnSlotActivated);
			m_Slots.m_OnRemoved.Remove(OnSlotRemoved);
			m_Slots.Close();
		}
		if (m_Items)
		{
			m_Items.m_OnSelected.Remove(OnCardSelected);
			m_Items.m_OnActivated.Remove(OnItemActivated);
			m_Items.Close();
		}
		m_Slots = null;
		m_Items = null;
		if (m_wRoot)
			m_wRoot.RemoveFromHierarchy();
		m_wRoot = null;
		m_wPreview = null;
		if (m_Preview)
			m_Preview.Clear();
		m_Preview = null;
		m_Navigation.Clear();
		m_aSlotLocations.Clear();
		if (m_Draft)
			m_Draft.Clear();
		m_Draft = null;
		m_ItemAreas = null;
		m_Recipe = null;
		m_Catalog = null;
		m_sAppliedData = string.Empty;
		m_sDraftData = string.Empty;
		m_Group = null;
		m_Side = null;
		m_sTargetFaction = string.Empty;
		m_sLibraryData = string.Empty;
		m_aTemplateKinds.Clear();
		m_aTemplateNames.Clear();
		m_aGroupIds.Clear();
		m_aGroupNames.Clear();
		m_DraftMemory.Clear();
		m_Member = null;
		m_Library = null;
		m_sViewPath = string.Empty;
		m_wLocation = null;
		m_aViewPaths.Clear();
		m_aViewModes.Clear();
		m_bShowAttachments = false;
		m_bContentsView = false;
		m_bCanShowContents = false;
		m_ContentCategory = null;
		m_bWeaponFilter = false;
		m_bWeaponCategoryOptions = false;
		m_WeaponCategories.Clear();
		m_aContentTypes.Clear();
		m_aContentModes.Clear();
		m_sCatalogFocus = string.Empty;
		m_bContentFilter = false;
		m_bAttachmentTarget = false;
		m_wCatalogContext = null;
		m_wSearch = null;
		m_wName = null;
		m_wSquadName = null;
		m_aRoleCaptions.Clear();
		m_aRoleKits.Clear();
		m_aAssignedNames.Clear();
		m_wRecipient = null;
		m_wModal = null;
		m_wLibrarySection = null;
		m_wMemberSaveSection = null;
		m_wSquadSaveSection = null;
		m_wConfirmSection = null;
		m_wModalTitle = null;
		m_wModalHelp = null;
		m_iModal = 0;
		m_sConfirmPayload = string.Empty;
		if (m_Cursor && m_bCaptured)
			m_Cursor.AICF_ClearLoadoutDialog(this);
		m_Cursor = null;
		m_bCaptured = false;
		m_bPending = false;
		m_bRotating = false;
		m_bDraftReady = false;
		m_bPersonal = false;
		m_sPersonalSource = string.Empty;
	}
}
