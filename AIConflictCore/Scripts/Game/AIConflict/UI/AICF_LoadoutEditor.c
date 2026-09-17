class AICF_LoadoutLocation
{
	string m_sPath;
	string m_sStorage;
	string m_sName;
	BaseInventoryStorageComponent m_Storage;
}

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
}

// Локальный черновик. Единственный внешний side effect формы — player-owned RPC.
class AICF_LoadoutEditor : ScriptedWidgetEventHandler
{
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
	protected EditBoxWidget m_wName;
	protected EditBoxWidget m_wSearch;
	protected RenderTargetWidget m_wPreview;
	protected ref AICF_LoadoutPreview m_Preview;
	protected SCR_ComboBoxComponent m_Group;
	protected SCR_ComboBoxComponent m_Member;
	protected SCR_ComboBoxComponent m_Library;
	protected SCR_ComboBoxComponent m_Storage;
	protected SCR_ComboBoxComponent m_ContentCategory;
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
	protected ref array<ref AICF_LoadoutLocation> m_aLocations = {};
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
	protected int m_iTargetType;
	protected bool m_bPending;
	protected bool m_bRendering;
	protected bool m_bCaptured;
	protected bool m_bRotating;
	protected bool m_bPanning;
	protected bool m_bDraftReady;
	protected int m_iMouseX;
	protected int m_iMouseY;

	void AICF_LoadoutEditor(AICF_StrategicUIController controller) { m_Controller = controller; }
	bool IsInputCaptured() { return m_bCaptured; }

	void Open(Widget mapRoot, int slotId)
	{
		Close();
		m_Player = SCR_PlayerController.Cast(GetGame().GetPlayerController());
		m_Faction = SCR_FactionManager.SGetLocalPlayerFaction();
		SCR_MapEntity mapEntity = SCR_MapEntity.GetMapInstance();
		if (!mapRoot || !mapEntity || !mapEntity.IsOpen() || !m_Player || !m_Faction || RplSession.Mode() == RplMode.Dedicated)
			return;
		m_Cursor = SCR_MapCursorModule.Cast(mapEntity.GetMapModule(SCR_MapCursorModule));
		if (!m_Cursor || (m_Cursor.GetCursorState() & EMapCursorState.CS_DIALOG))
			return;
		m_PlayerCharacter = m_Player.GetControlledEntity();
		m_iSlotId = slotId;
		m_iMember = 0;
		m_Catalog = new AICF_LoadoutCatalog(SCR_CampaignFaction.Cast(m_Faction));
		m_ItemAreas = new AICF_LoadoutItemAreas();
		m_Draft = new AICF_LoadoutDraft();
		m_Preview = new AICF_LoadoutPreview();
		if (!Create(mapRoot))
		{
			Close();
			return;
		}
		m_bCaptured = true;
		m_Cursor.AICF_SetLoadoutDialog(this);
		GetGame().GetInputManager().AddActionListener(UIConstants.MENU_ACTION_BACK, EActionTrigger.DOWN, Close);
		Request(0);
	}

	protected void Place(Widget widget, float l, float t, float r, float b)
	{
		FrameSlot.SetAnchorMin(widget, l, t);
		FrameSlot.SetAnchorMax(widget, r, b);
		FrameSlot.SetOffsets(widget, 0, 0, 0, 0);
		widget.SetZOrder(10);
	}

	protected TextWidget Label(float l, float t, float r, float b, string text, int size = 18)
	{
		return m_Controller.CreateText(m_wPanel, l, t, r, b, text, size, Color.FromSRGBA(230, 237, 242, 255));
	}

	protected Widget Button(string name, string text, float l, float t, float r, float b)
	{
		Widget root = m_Controller.CreateRect(m_wPanel, l, t, r, b, Color.FromSRGBA(46, 74, 91, 255), true);
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

	protected SCR_ComboBoxComponent Combo(float l, float t, float r, float b)
	{
		Widget widget = GetGame().GetWorkspace().CreateWidgets(COMBO, m_wPanel);
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
		m_wRoot = m_Controller.CreateRect(parent, 0, 0, 1, 1, Color.FromSRGBA(0, 0, 0, 185), true);
		if (!m_wRoot)
			return false;
		m_wRoot.SetZOrder(400);
		m_wPanel = m_Controller.CreateRect(m_wRoot, 0.045, 0.055, 0.955, 0.945, Color.FromSRGBA(9, 19, 28, 255), true);
		if (!m_wPanel)
			return false;
		// Выше полноэкранной input-поверхности scrim (z=1).
		m_wPanel.SetZOrder(5);
		Label(0.025, 0.02, 0.34, 0.08, "ЭКИПИРОВКА ОТРЯДА", 27);
		m_Group = Combo(0.36, 0.025, 0.82, 0.08);
		Button("close", "Закрыть", 0.84, 0.025, 0.975, 0.08);
		m_Member = Combo(0.025, 0.09, 0.29, 0.15);
		m_Library = Combo(0.31, 0.09, 0.56, 0.15);
		Button("load", "Загрузить", 0.57, 0.09, 0.68, 0.15);
		Button("undo", "Отменить шаг", 0.69, 0.09, 0.83, 0.15);
		Button("reset", "Стандартный", 0.84, 0.09, 0.975, 0.15);
		Label(0.025, 0.165, 0.29, 0.20, "ПРЕДПРОСМОТР", 17);
		Label(0.31, 0.165, 0.63, 0.20, "ЭКИПИРОВКА И КОНТЕЙНЕРЫ", 17);
		m_Storage = Combo(0.31, 0.205, 0.63, 0.265);
		Label(0.65, 0.165, 0.975, 0.20, "КАТАЛОГ ПРЕДМЕТОВ", 17);
		m_wCatalogContext = Label(0.65, 0.205, 0.975, 0.265, "Выберите место в центре", 17);
		m_wCatalogContext.SetTextWrapping(true);
		m_ContentCategory = Combo(0.65, 0.205, 0.975, 0.265);
		if (!m_ContentCategory)
			return false;
		m_ContentCategory.GetRootWidget().SetVisible(false);
		m_wSearch = Edit(0.65, 0.275, 0.865, 0.33, "Поиск по названию…");
		Button("clearFilters", "Очистить", 0.875, 0.275, 0.975, 0.33);
		if (!m_wSearch)
			return false;
		m_wSearch.AddHandler(this);
		m_Slots = new AICF_LoadoutItemGrid();
		m_Items = new AICF_LoadoutItemGrid();
		m_wCapacity = Label(0.31, 0.268, 0.63, 0.313, "Вместимость: —", 14);
		m_wCapacity.SetTextWrapping(true);
		if (!m_Slots.Create(m_Controller, m_wPanel, 0.31, 0.32, 0.63, 0.715, 1, 8) ||
			!m_Items.Create(m_Controller, m_wPanel, 0.65, 0.34, 0.975, 0.735, 2, 3))
			return false;
		m_Slots.m_OnSelected.Insert(OnCardSelected);
		m_Slots.m_OnActivated.Insert(OnSlotActivated);
		m_Items.m_OnSelected.Insert(OnCardSelected);
		m_Items.m_OnActivated.Insert(OnItemActivated);
		m_wTarget = Label(0.31, 0.72, 0.63, 0.775, "Выберите место", 16);
		m_wTarget.SetTextWrapping(true);
		Button("back", "Назад", 0.31, 0.79, 0.41, 0.835);
		Button("inside", "Содержимое", 0.42, 0.79, 0.52, 0.835);
		Button("delete", "Снять", 0.53, 0.79, 0.63, 0.835);
		m_wSelection = Label(0.65, 0.735, 0.975, 0.79, "Выберите предмет", 16);
		m_wSelection.SetTextWrapping(true);
		Button("less", "-", 0.65, 0.79, 0.68, 0.835);
		m_wQuantity = Label(0.68, 0.79, 0.73, 0.835, "×1", 17);
		Button("more", "+", 0.73, 0.79, 0.76, 0.835);
		Button("apply", "Выберите предмет", 0.77, 0.79, 0.975, 0.835);
		Label(0.025, 0.745, 0.29, 0.84, "Поворот — зажать ЛКМ\nМасштаб — колесом\nВверх / вниз — зажать колесо", 16);
		m_wPreview = RenderTargetWidget.Cast(GetGame().GetWorkspace().CreateWidget(WidgetType.RenderTargetWidgetTypeID,
			WidgetFlags.VISIBLE | WidgetFlags.BLEND, Color.White, 0, m_wPanel));
		if (!m_wPreview || !m_Group || !m_Member || !m_Library || !m_Storage || !m_wCatalogContext)
			return false;
		Place(m_wPreview, 0.025, 0.205, 0.29, 0.735);
		m_wPreview.SetFlags(WidgetFlags.VISIBLE | WidgetFlags.BLEND);
		m_wPreview.SetMaxFPS(30);
		m_wPreview.SetResolutionScale(1, 1);
		m_wPreview.SetClearColor(true, Color.BLACK);
		m_wPreview.SetVisible(false);
		m_wPreview.AddHandler(this);
		m_wName = Edit(0.025, 0.855, 0.39, 0.91, "Название комплекта (до 80 символов)");
		if (!m_wName)
			return false;
		m_wName.AddHandler(this);
		m_wCost = Label(0.41, 0.855, 0.79, 0.91, "Экипировка — бесплатно", 17);
		m_wSave = Button("save", "Сохранить", 0.81, 0.855, 0.975, 0.91);
		m_wStatus = Label(0.025, 0.925, 0.975, 0.985, "", 17);
		m_wStatus.SetTextWrapping(true);
		m_bRendering = true;
		for (int slotId; slotId < AICF_Stage1Config.GROUP_SLOTS_PER_FACTION; slotId++)
		{
			string name;
			int size;
			if (!m_Controller.GetLoadoutGroup(slotId, name, size))
				continue;
			m_aGroupIds.Insert(slotId);
			m_aGroupNames.Insert(name);
			m_Group.AddItem(name);
		}
		int groupIndex = m_aGroupIds.Find(m_iSlotId);
		if (groupIndex < 0)
		{
			m_bRendering = false;
			return false;
		}
		m_Group.SetCurrentItem(groupIndex, false, false, false);
		PopulateMembers();
		array<string> categories = {};
		AICF_LoadoutContentCategories.Fill(categories, m_aContentTypes, m_aContentModes);
		foreach (string category : categories)
			m_ContentCategory.AddItem(category);
		m_ContentCategory.SetCurrentItem(0, false, false, false);
		m_bRendering = false;
		RefreshColors();
		return m_wSave && m_wStatus && m_wCost;
	}

	protected void PopulateMembers()
	{
		string name;
		int size;
		m_Controller.GetLoadoutGroup(m_iSlotId, name, size);
		array<string> roles = {"Командир", "Медик", "Пулемётчик", "Гранатомётчик AT", "Гренадер", "Автоматчик", "Старший стрелок", "Помощник пулемётчика", "Помощник AT", "Стрелок"};
		m_Member.ClearAll();
		for (int i; i < Math.Min(size, roles.Count()); i++)
			m_Member.AddItem((i + 1).ToString() + ". " + roles[i]);
		m_iMember = Math.ClampInt(m_iMember, 0, Math.Max(0, size - 1));
		m_Member.SetCurrentItem(m_iMember, false, false, false);
	}

	protected string TargetName()
	{
		int index = m_aGroupIds.Find(m_iSlotId);
		string group = "Отряд " + (m_iSlotId + 1).ToString();
		if (index >= 0)
			group = m_aGroupNames[index];
		return group + ", позиция " + (m_iMember + 1).ToString();
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
		m_iSlotId = slotId;
		m_iMember = member;
		m_iRevision = -1;
		m_Recipe = null;
		m_sAppliedData = string.Empty;
		m_sDraftData = string.Empty;
		m_bDraftReady = false;
		m_bRotating = false;
		m_bPanning = false;
		GetGame().GetCallqueue().Remove(RotatePreview);
		m_bRendering = true;
		PopulateMembers();
		m_Storage.ClearAll();
		m_Library.ClearAll();
		m_bRendering = false;
		m_aTemplateIds.Clear();
		m_aLocations.Clear();
		m_aSlots.Clear();
		m_aItems.Clear();
		array<string> empty = {};
		m_Slots.SetItems(empty, empty, empty);
		m_Items.SetItems(empty, empty, empty);
		m_wPreview.SetVisible(false);
		m_wName.SetText("");
		m_wCapacity.SetText("Вместимость: —");
		m_wTarget.SetText("Ожидается комплект выбранного отряда…");
		m_wSelection.SetText("");
		m_wSearch.SetText("");
		m_ContentCategory.SetCurrentItem(0, false, false, false);
		m_iQuantity = 1;
		m_wQuantity.SetText("×1");
		m_sCatalogFocus = string.Empty;
		Request(0);
	}

	protected EditBoxWidget Edit(float l, float t, float r, float b, string placeholder)
	{
		Widget root = GetGame().GetWorkspace().CreateWidgets(EDIT, m_wPanel);
		if (!root)
			return null;
		Place(root, l, t, r, b);
		SCR_EditBoxComponent component = SCR_EditBoxComponent.Cast(root.FindHandler(SCR_EditBoxComponent));
		if (component)
			component.UseLabel(false);
		EditBoxWidget input = EditBoxWidget.Cast(root.FindAnyWidget("EditBox"));
		if (input)
			input.SetPlaceholderText(placeholder);
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
			else if (frame == m_wSave)
			{
				color = Color.FromSRGBA(226, 167, 79, 255);
				textColor = Color.FromSRGBA(9, 19, 28, 255);
			}
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
		m_iToken = m_Player.AICF_RequestLoadout(m_iSlotId, m_iMember, m_iRevision, operation, payload);
		m_bPending = m_iToken > 0;
		m_iRequestAt = System.GetTickCount();
		m_wStatus.SetText("Ожидается ответ сервера…");
		m_wSave.SetEnabled(false);
		SetBusy();
	}

	protected void SetBusy()
	{
		foreach (SCR_ComboBoxComponent combo : m_aCombos)
			combo.SetEnabled(!m_bPending);
		foreach (Widget button : m_aButtons)
		{
			if (button.GetName() != "close" && button.GetName() != "save")
				button.SetEnabled(!m_bPending);
		}
		if (m_wName)
			m_wName.SetEnabled(!m_bPending);
		if (m_wSearch)
			m_wSearch.SetEnabled(!m_bPending && m_bDraftReady);
		if (m_Slots)
			m_Slots.SetEnabled(!m_bPending && m_bDraftReady);
		if (m_Items)
			m_Items.SetEnabled(!m_bPending && m_bDraftReady);
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
				caption.SetText(text);
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
		return m_sDraftData != m_sAppliedData || m_wName.GetText() != m_Recipe.m_sName;
	}

	protected bool HasValidName()
	{
		string name = m_wName.GetText();
		return !name.IsEmpty() && name.Length() <= 80 && !name.Contains("\n") && !name.Contains("\r");
	}

	protected void UpdateActions()
	{
		if (!m_Items || !m_Slots || !m_Storage)
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
		bool hasItem = item >= 0 && item < m_aItems.Count();
		bool typeMatches = hasItem;
		if (hasItem && ready)
			typeMatches = MatchesTarget(m_aItems[item]);
		string action = "Надеть";
		if (occupied)
			action = "Заменить";
		else if (adding)
			action = "Добавить";
		if (!hasItem)
			action = "Выберите предмет";
		else if (!typeMatches)
			action = "Другой тип предмета";
		SetButtonState("apply", ready && hasSlot && hasItem && typeMatches, action);
		string removeCaption = "Снять";
		if (location && !location.m_sPath.IsEmpty())
			removeCaption = "Убрать";
		SetButtonState("delete", ready && occupied, removeCaption);
		bool nested = location && !location.m_sPath.IsEmpty();
		bool hasContent = ChildLocation() >= 0;
		ShowButton("delete", occupied);
		ShowButton("inside", hasContent);
		ShowButton("back", nested);
		SetButtonState("inside", ready && hasContent);
		SetButtonState("back", ready && nested);
		ShowButton("less", adding);
		ShowButton("more", adding);
		m_wQuantity.SetVisible(adding);
		SetButtonState("less", ready && adding && m_iQuantity > 1);
		SetButtonState("more", ready && adding && m_iQuantity < 16);
		SetButtonState("undo", ready && m_Recipe && !m_Recipe.m_aPaths.IsEmpty());
		SetButtonState("reset", ready && m_Recipe && !m_Recipe.m_aPaths.IsEmpty());
		SetButtonState("load", !m_bPending && !m_aTemplateIds.IsEmpty());
		m_ContentCategory.SetEnabled(ready && m_bContentFilter);
		bool filtered = !m_wSearch.GetText().IsEmpty() || (m_bContentFilter && m_ContentCategory.GetCurrentIndex() > 0);
		ShowButton("clearFilters", filtered);
		SetButtonState("clearFilters", ready && filtered);
		bool changed = HasDraftChanges();
		string saveCaption = "Без изменений";
		if (changed)
			saveCaption = "Сохранить";
		if (!HasValidName())
			saveCaption = "Введите имя";
		if (!m_bDraftReady)
			saveCaption = "Недоступно";
		if (m_bPending)
			saveCaption = "Ожидание…";
		SetButtonState("save", ready && changed && HasValidName() && m_Player && m_Player.AICF_LoadoutRevision() == m_iRevision, saveCaption);
		if (adding)
			m_wQuantity.SetText("×" + m_iQuantity.ToString());
		else
			m_wQuantity.SetText("×1");
	}

	void Refresh()
	{
		if (!m_wRoot)
			return;
		RefreshColors();
		if (!m_Player || GetGame().GetPlayerController() != m_Player || SCR_FactionManager.SGetLocalPlayerFaction() != m_Faction ||
			m_Player.GetControlledEntity() != m_PlayerCharacter)
		{
			Close();
			return;
		}
		if (m_bPending && m_Player.AICF_LoadoutToken() == m_iToken)
		{
			m_bPending = false;
			string status = m_Player.AICF_LoadoutStatus();
			if (m_Player.AICF_LoadoutAccepted() && m_Player.AICF_LoadoutSlot() == m_iSlotId && m_Player.AICF_LoadoutMember() == m_iMember)
			{
				m_Recipe = AICF_LoadoutRecipe.Decode(m_Player.AICF_LoadoutData());
				m_iRevision = m_Player.AICF_LoadoutRevision();
				// TEMPLATE_LOADED — только черновик, а не новая привязка позиции.
				if (status == "READY" || status == "SAVED")
					m_sAppliedData = m_Player.AICF_LoadoutData();
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
				else if (status == "SAVED" || status == "TEMPLATE_LOADED")
					m_DraftMemory.Forget(m_iSlotId, m_iMember);
				if (m_Recipe)
				{
					m_wName.SetText(m_Recipe.m_sName);
					if (!Rebuild())
						status = "PREVIEW_BUILD_FAILED";
					Library();
				}
			}
			m_wStatus.SetText(Status(status));
		}
		else if (m_bPending && System.GetTickCount(m_iRequestAt) > 15000)
		{
			m_bPending = false;
			m_wStatus.SetText("Ответ не получен. Закройте и откройте форму, чтобы проверить сохранение.");
		}
		if (!m_bPending && m_Recipe && m_Player.AICF_LoadoutRevision() != m_iRevision)
			m_wStatus.SetText("Комплект изменён другим игроком. Откройте позицию заново; ваш черновик сохранён в форме.");
		SetBusy();
	}

	protected string Status(string code)
	{
		switch (code)
		{
			case "SAVED": return TargetName() + ": сохранено. Комплект получит следующий созданный боец этой позиции в этом отряде.";
			case "READY": return TargetName() + ". Выберите предметы и сохраните комплект. Отряд переключается в списке сверху.";
			case "DRAFT_RESTORED": return TargetName() + ": восстановлен несохранённый черновик. Нажмите «Сохранить комплект» для назначения.";
			case "DRAFT_OUTDATED": return TargetName() + ": загружен новый комплект сервера; прежний черновик устарел после изменения ревизии.";
			case "PREVIEW_BUILD_FAILED": return "Не удалось собрать предпросмотр. Закройте и откройте редактор; сохранение недоступно.";
			case "TEMPLATE_LOADED": return "Шаблон открыт как черновик. Нажмите «Сохранить», чтобы назначить его позиции.";
			case "REVISION_CONFLICT": return "Другой игрок изменил комплект. Закройте и откройте позицию перед повторной записью.";
			case "RATE_LIMITED": return "Слишком частые запросы. Повторите через секунду.";
			case "ITEM_NOT_ALLOWED": return "Предмет отсутствует в разрешённом каталоге вашей фракции.";
			case "INVENTORY_INCOMPATIBLE": return "Предмет несовместим с местом или контейнер заполнен. Выберите другое место или уменьшите количество.";
			case "ITEM_CAPACITY_OR_COMPATIBILITY": return "Предмет не подходит или в контейнере недостаточно места.";
			case "INVENTORY_RESTORE_FAILED": return "Комплект нельзя полностью восстановить. Прежняя настройка сохранена; отмените последнее изменение.";
			case "INVENTORY_SIGNATURE_MISMATCH": return "Результат выдачи отличается от черновика. Прежняя настройка сохранена.";
			case "LIBRARY_FULL": return "Библиотека заполнена (256 версий). Администратор может архивировать старые файлы.";
			case "LIBRARY_WRITE_FAILED": return "Сервер не смог записать библиотеку. Прежний комплект сохранён.";
			case "WEAPON_OR_AMMO_MISSING": return "Нужно основное оружие и подходящие боеприпасы к каждому оружию. Добавьте магазины в карманы.";
		}
		return "Комплект не сохранён: " + code;
	}

	protected void Library()
	{
		m_bRendering = true;
		m_Library.ClearAll();
		m_aTemplateIds.Clear();
		array<string> names = {};
		JsonLoadContext context = new JsonLoadContext();
		if (context.LoadFromString(m_Player.AICF_LoadoutLibrary()) && context.ReadValue("ids", m_aTemplateIds) && context.ReadValue("names", names))
		{
			foreach (string name : names)
				m_Library.AddItem(name);
		}
		m_Library.SetCurrentItem(0, false, false, false);
		m_bRendering = false;
	}

	protected bool Rebuild()
	{
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
		m_aLocations.Clear();
		m_bDraftReady = false;
		m_wPreview.SetVisible(false);
		// Отменяем незавершённые binds старых карточек до смены черновика.
		m_Slots.ReleasePreviews();
		m_Items.ReleasePreviews();
		string reason;
		if (!m_Draft.Build(m_Recipe, m_Catalog, reason))
		{
			m_wStatus.SetText(Status(reason));
			return false;
		}
		if (!m_Preview.Build(m_Draft.GetCharacter()))
		{
			m_wStatus.SetText(Status("PREVIEW_BUILD_FAILED"));
			return false;
		}
		m_wPreview.SetWorld(m_Preview.GetWorld(), 0);
		m_wPreview.SetVisible(true);
		m_bDraftReady = true;
		m_sDraftData = m_Recipe.Encode();
		m_aLocations.Clear();
		Locations(m_Draft.GetCharacter(), "", "На бойце", 0);
		m_bRendering = true;
		m_Storage.ClearAll();
		int selectedIndex;
		foreach (int locationIndex, AICF_LoadoutLocation location : m_aLocations)
		{
			m_Storage.AddItem(location.m_sName);
			if (location.m_sPath == selectedPath && location.m_sStorage == selectedStorage)
				selectedIndex = locationIndex;
		}
		m_Storage.SetCurrentItem(selectedIndex, false, false, false);
		m_bRendering = false;
		Slots();
		if (selectedIndex < m_aLocations.Count() && m_aSlots.Contains(selectedSlot) &&
			m_aLocations[selectedIndex].m_sPath == selectedPath && m_aLocations[selectedIndex].m_sStorage == selectedStorage)
			m_Slots.Select(m_aSlots.Find(selectedSlot));
		Selection();
		Items();
		if (selectedSlot < -1)
			FocusSlotCatalog();
		return true;
	}

	protected void Locations(IEntity entity, string path, string name, int depth)
	{
		if (!entity || depth > 6 || m_aLocations.Count() > 96)
			return;
		set<BaseInventoryStorageComponent> storages = new set<BaseInventoryStorageComponent>();
		SCR_PlayerArsenalLoadout.FindStorageComponents(entity, storages);
		foreach (BaseInventoryStorageComponent storage : storages)
		{
			if (!AICF_LoadoutInventory.Editable(storage))
				continue;
			AICF_LoadoutLocation location = new AICF_LoadoutLocation();
			location.m_Storage = storage;
			location.m_sPath = path;
			location.m_sStorage = AICF_LoadoutInventory.StorageId(entity, storage);
			string kind = "карманы";
			if (EquipedLoadoutStorageComponent.Cast(storage) || SCR_CharacterInventoryStorageComponent.Cast(storage))
				kind = "одежда";
			else if (EquipedWeaponStorageComponent.Cast(storage))
				kind = "оружие";
			else if (AICF_LoadoutSlotView.HasFixedSlots(storage))
				kind = "обвесы";
			location.m_sName = name + " — " + kind;
			m_aLocations.Insert(location);
			array<InventoryItemComponent> items = {};
			storage.GetOwnedItems(items, false);
			foreach (InventoryItemComponent child : items)
			{
				if (child && child.GetParentSlot() && child.GetParentSlot().GetStorage() == storage)
					Locations(child.GetOwner(), path + location.m_sStorage + "#" + child.GetParentSlot().GetID().ToString() + "/",
						m_Catalog.Name(SCR_ResourceNameUtils.GetPrefabName(child.GetOwner())), depth + 1);
			}
		}
	}

	protected AICF_LoadoutLocation SelectedLocation()
	{
		int index = m_Storage.GetCurrentIndex();
		if (index < 0 || index >= m_aLocations.Count())
			return null;
		return m_aLocations[index];
	}

	protected void Slots()
	{
		m_aSlots.Clear();
		array<string> prefabs = {}, names = {}, details = {};
		AICF_LoadoutLocation location = SelectedLocation();
		if (location)
		{
			if (!location.m_sPath.IsEmpty() && !AICF_LoadoutSlotView.HasFixedSlots(location.m_Storage))
			{
				m_aSlots.Insert(-1);
				prefabs.Insert("");
				names.Insert("+ Добавить в контейнер");
				details.Insert("Выберите предмет справа");
			}
			for (int i; i < location.m_Storage.GetSlotsCount(); i++)
			{
				if (!AICF_LoadoutSlotView.VisibleSlot(location.m_Storage, i))
					continue;
				IEntity entity = AICF_LoadoutClothing.CoveringItem(location.m_Storage, i);
				if (!entity && !location.m_sPath.IsEmpty() && !AICF_LoadoutSlotView.HasFixedSlots(location.m_Storage))
					continue;
				string name;
				AICF_LoadoutSlotView.Describe(location.m_Storage, i, name);
				string prefab, detail = "Не надето";
				if (entity)
				{
					prefab = SCR_ResourceNameUtils.GetPrefabName(entity);
					detail = m_Catalog.Name(prefab);
					if (!location.m_sPath.IsEmpty() && !AICF_LoadoutSlotView.HasFixedSlots(location.m_Storage))
					{
						name = detail;
						detail = "В контейнере";
					}
				}
				prefabs.Insert(prefab);
				names.Insert(name);
				details.Insert(detail);
				m_aSlots.Insert(i);
			}
		}
		m_Slots.SetItems(prefabs, names, details);
		m_Slots.Select(0);
		Selection();
	}

	protected void Items()
	{
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
		m_Catalog.List(category, m_aItems, mode);
		if (!m_bAttachmentTarget)
			m_ItemAreas.Filter(m_aItems, m_TargetArea, m_Draft.GetPreview());
		if (m_bWeaponTarget)
			m_ItemAreas.FilterWeapons(m_aItems, m_sTargetWeaponType, m_Draft.GetPreview());
		if (m_bAttachmentTarget)
		{
			AICF_LoadoutLocation location = SelectedLocation();
			InventoryStorageManagerComponent manager = InventoryStorageManagerComponent.Cast(m_Draft.GetCharacter().FindComponent(InventoryStorageManagerComponent));
			m_ItemAreas.FilterAttachments(m_aItems, location.m_Storage, m_aSlots[m_Slots.GetSelected()], m_Draft.GetPreview(), manager);
		}
		string query = m_wSearch.GetText();
		query.ToLower();
		array<string> names = {}, details = {};
		for (int i = m_aItems.Count() - 1; i >= 0; i--)
		{
			string searchable = m_Catalog.Name(m_aItems[i]);
			searchable.ToLower();
			if (!query.IsEmpty() && !searchable.Contains(query))
				m_aItems.Remove(i);
		}
		foreach (string prefab : m_aItems)
		{
			names.Insert(m_Catalog.Name(prefab));
			details.Insert(string.Empty);
		}
		string empty = "Нет подходящих предметов для этого места.";
		if (!query.IsEmpty() || (m_bContentFilter && m_ContentCategory.GetCurrentIndex() > 0))
			empty = "Нет предметов по выбранным фильтрам.\nНажмите «Сброс» или выберите другую категорию.";
		m_Items.SetEmptyText(empty);
		bool sameItems = m_Items.SetItems(m_aItems, names, details, true);
		m_Items.Select(m_aItems.Find(previous), !sameItems);
		Selection();
	}

	protected void OnCardSelected(AICF_LoadoutItemGrid grid, int index)
	{
		Selection();
		if (grid == m_Slots)
			FocusSlotCatalog();
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
		if (ChildLocation() >= 0)
		{
			OpenContainer();
			return;
		}
		AICF_LoadoutLocation location = SelectedLocation();
		if (!location || location.m_sPath.IsEmpty())
			return;
		ApplySelection(true);
	}

	protected void FocusSlotCatalog()
	{
		AICF_LoadoutLocation location = SelectedLocation();
		string focus;
		if (location)
		{
			focus = m_iMember.ToString() + "|" + location.m_sPath + "|" + location.m_sStorage;
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
		string caption = "Выберите предмет в каталоге";
		if (item >= 0 && item < m_aItems.Count())
			caption = m_Catalog.Name(m_aItems[item]);
		m_wSelection.SetText(caption);
		AICF_LoadoutLocation location = SelectedLocation();
		int selected = m_Slots.GetSelected();
		m_iTargetType = 0;
		m_TargetArea = typename.Empty;
		m_bWeaponTarget = false;
		m_bAttachmentTarget = false;
		m_bContentFilter = location && !location.m_sPath.IsEmpty() && !AICF_LoadoutSlotView.HasFixedSlots(location.m_Storage);
		m_iTargetMode = 0;
		m_sTargetWeaponType = string.Empty;
		caption = "Выберите место слева";
		string context = "Выберите место в центре";
		if (location && selected >= 0 && selected < m_aSlots.Count())
		{
			int slot = m_aSlots[selected];
			caption = "Добавление в: " + location.m_sName;
			context = "Предметы для: " + location.m_sName;
			if (slot >= 0)
			{
				m_iTargetType = AICF_LoadoutSlotView.Describe(location.m_Storage, slot, caption);
				m_TargetArea = AICF_LoadoutSlotView.Area(location.m_Storage, slot);
				if (location.m_sPath.IsEmpty() || AICF_LoadoutSlotView.HasFixedSlots(location.m_Storage))
					context = "Подходит: " + caption;
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
					caption = m_Catalog.Name(SCR_ResourceNameUtils.GetPrefabName(location.m_Storage.Get(slot)));
				caption = "Выбрано: " + caption;
				if (ChildLocation() >= 0)
					caption += "\nДвойной клик — содержимое";
				else if (!location.m_sPath.IsEmpty() && location.m_Storage.Get(slot))
					caption += "\nДвойной клик — убрать предмет";
			}
		}
		m_wTarget.SetText(caption);
		m_wCatalogContext.SetText(context);
		m_wCatalogContext.SetVisible(!m_bContentFilter);
		if (!m_bContentFilter)
			m_ContentCategory.CloseList();
		m_ContentCategory.GetRootWidget().SetVisible(m_bContentFilter);
		// Внутри контейнера показываем его объём, даже когда выбран предмет.
		// На бойце — карманы выбранной одежды/рюкзака до входа в содержимое.
		BaseInventoryStorageComponent capacityStorage;
		if (location && !location.m_sPath.IsEmpty())
			capacityStorage = location.m_Storage;
		else
		{
			int child = ChildLocation();
			if (child >= 0)
				capacityStorage = m_aLocations[child].m_Storage;
		}
		m_wCapacity.SetText(AICF_LoadoutSlotView.CapacityText(capacityStorage));
		UpdateActions();
	}

	protected int ChildLocation()
	{
		AICF_LoadoutLocation location = SelectedLocation();
		int selected = m_Slots.GetSelected();
		if (!location || selected < 0 || selected >= m_aSlots.Count() || m_aSlots[selected] < 0)
			return -1;
		IEntity worn = AICF_LoadoutClothing.CoveringItem(location.m_Storage, m_aSlots[selected]);
		if (!worn)
			return -1;
		InventoryItemComponent wornItem = InventoryItemComponent.Cast(worn.FindComponent(InventoryItemComponent));
		if (!wornItem || !wornItem.GetParentSlot())
			return -1;
		string path = location.m_sPath + location.m_sStorage + "#" + wornItem.GetParentSlot().GetID().ToString() + "/";
		int fallback = -1;
		foreach (int i, AICF_LoadoutLocation child : m_aLocations)
		{
			if (child.m_sPath != path)
				continue;
			if (fallback < 0)
				fallback = i;
			float used, capacity;
			if (AICF_LoadoutSlotView.Capacity(child.m_Storage, used, capacity))
				return i;
		}
		return fallback;
	}

	protected void OpenContainer()
	{
		int child = ChildLocation();
		if (child < 0)
			return;
		m_Storage.SetCurrentItem(child, false, false, false);
		Slots();
		FocusSlotCatalog();
	}

	protected void ParentContainer()
	{
		AICF_LoadoutLocation location = SelectedLocation();
		if (!location || location.m_sPath.IsEmpty())
			return;
		array<string> parts = {};
		location.m_sPath.Split("/", parts, true);
		if (parts.IsEmpty())
			return;
		array<string> address = {};
		parts[parts.Count() - 1].Split("#", address, true);
		if (address.Count() != 2)
			return;
		string path;
		for (int p; p < parts.Count() - 1; p++)
			path += parts[p] + "/";
		foreach (int i, AICF_LoadoutLocation parent : m_aLocations)
		{
			if (parent.m_sPath != path || parent.m_sStorage != address[0])
				continue;
			m_Storage.SetCurrentItem(i, false, false, false);
			Slots();
			m_Slots.Select(m_aSlots.Find(address[1].ToInt()));
			Selection();
			FocusSlotCatalog();
			return;
		}
	}

	override bool OnChange(Widget w, bool finished)
	{
		if (m_bRendering || m_bPending)
			return false;
		if (w == m_wName)
		{
			UpdateActions();
			RefreshColors();
			return true;
		}
		if (w != m_wSearch)
			return false;
		Items();
		return true;
	}

	protected void OnChanged(SCR_ComboBoxComponent component, int index)
	{
		if (m_bRendering || !m_wRoot)
			return;
		if (component == m_Group && !m_bPending && index >= 0 && index < m_aGroupIds.Count())
			SelectTarget(m_aGroupIds[index], 0);
		else if (component == m_Member && !m_bPending)
			SelectTarget(m_iSlotId, index);
		else if (component == m_Storage)
		{
			Slots();
			FocusSlotCatalog();
		}
		else if (component == m_ContentCategory && !m_bPending && m_bContentFilter)
			Items();
	}

	override bool OnClick(Widget w, int x, int y, int button)
	{
		string action = w.GetName();
		if (action == "close")
		{
			Close();
			return true;
		}
		if (m_bPending || !m_Recipe || !m_bDraftReady)
			return true;
		if (action == "save")
		{
			if (!HasDraftChanges() || !HasValidName() || m_Player.AICF_LoadoutRevision() != m_iRevision)
				return true;
			m_Recipe.m_sName = m_wName.GetText();
			Request(1, m_Recipe.Encode());
		}
		else if (action == "load")
		{
			int index = m_Library.GetCurrentIndex();
			if (index >= 0 && index < m_aTemplateIds.Count())
				Request(2, m_aTemplateIds[index].ToString());
		}
		else if (action == "less" || action == "more")
		{
			int delta = 1;
			if (action == "less")
				delta = -1;
			m_iQuantity = Math.ClampInt(m_iQuantity + delta, 1, 16);
			m_wQuantity.SetText("×" + m_iQuantity.ToString());
		}
		else if (action == "inside")
			OpenContainer();
		else if (action == "back")
			ParentContainer();
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
		else if (action == "apply" || action == "delete")
			ApplySelection(action == "delete");
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
		AICF_LoadoutLocation location = SelectedLocation();
		int selected = m_Slots.GetSelected();
		int item = m_Items.GetSelected();
		if (!location || selected < 0 || selected >= m_aSlots.Count())
			return;
		if (m_Recipe.m_aPaths.Count() >= AICF_LoadoutRecipe.MAX_OPERATIONS)
		{
			m_wStatus.SetText("Достигнут предел изменений шаблона. Отмените лишний шаг или сбросьте черновик.");
			return;
		}
		ResourceName prefab;
		int count = 1;
		if (!removeItem)
		{
			if (item < 0 || item >= m_aItems.Count())
			{
				m_wStatus.SetText("Выберите карточку предмета в каталоге справа.");
				return;
			}
			prefab = m_aItems[item];
			if (!MatchesTarget(prefab))
			{
				m_wStatus.SetText(Status("INVENTORY_INCOMPATIBLE"));
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
			m_wStatus.SetText(Status("INVENTORY_INCOMPATIBLE"));
		}
		else
			m_wStatus.SetText("Черновик изменён. Нажмите «Сохранить», чтобы назначить комплект позиции.");
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
		m_bRotating = false;
		m_bPanning = false;
		GetGame().GetCallqueue().Remove(RotatePreview);
		if (m_bCaptured)
			GetGame().GetInputManager().RemoveActionListener(UIConstants.MENU_ACTION_BACK, EActionTrigger.DOWN, Close);
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
		if (m_Slots)
		{
			m_Slots.m_OnSelected.Remove(OnCardSelected);
			m_Slots.m_OnActivated.Remove(OnSlotActivated);
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
		m_aLocations.Clear();
		if (m_Draft)
			m_Draft.Clear();
		m_Draft = null;
		m_ItemAreas = null;
		m_Recipe = null;
		m_Catalog = null;
		m_sAppliedData = string.Empty;
		m_sDraftData = string.Empty;
		m_Group = null;
		m_aGroupIds.Clear();
		m_aGroupNames.Clear();
		m_DraftMemory.Clear();
		m_Member = null;
		m_Library = null;
		m_Storage = null;
		m_ContentCategory = null;
		m_aContentTypes.Clear();
		m_aContentModes.Clear();
		m_sCatalogFocus = string.Empty;
		m_bContentFilter = false;
		m_bAttachmentTarget = false;
		m_wCatalogContext = null;
		m_wSearch = null;
		m_wName = null;
		if (m_Cursor && m_bCaptured)
			m_Cursor.AICF_ClearLoadoutDialog(this);
		m_Cursor = null;
		m_bCaptured = false;
		m_bPending = false;
		m_bRotating = false;
		m_bDraftReady = false;
	}
}
