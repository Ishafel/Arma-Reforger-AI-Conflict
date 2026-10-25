class AICF_VehiclePresetEditor : ScriptedWidgetEventHandler
{
	protected static const ResourceName COMBO = "{4B5AE6E64037FFB4}UI/layouts/WidgetLibrary/ComboBox/WLib_ComboBox.layout";
	protected static const ResourceName EDIT = "{0022F0B45ADBC5AC}UI/layouts/WidgetLibrary/EditBox/WLib_EditBox.layout";
	protected AICF_StrategicUIController m_Controller;
	protected SCR_PlayerController m_Player;
	protected SCR_MapCursorModule m_Cursor;
	protected Widget m_wRoot;
	protected Widget m_wPanel;
	protected TextWidget m_wStatus;
	protected TextWidget m_wUnseated;
	protected EditBoxWidget m_wName;
	protected SCR_ComboBoxComponent m_Catalog;
	protected SCR_ComboBoxComponent m_SourceFilter;
	protected SCR_ComboBoxComponent m_CategoryFilter;
	protected SCR_ComboBoxComponent m_Library;
	protected ref array<SCR_ComboBoxComponent> m_aSeats = {};
	protected ref array<TextWidget> m_aSeatLabels = {};
	protected ref array<Widget> m_aButtons = {};
	protected ref array<ResourceName> m_aPrefabs = {};
	protected ref array<string> m_aCatalogNames = {};
	protected ref array<int> m_aCatalogCosts = {};
	protected ref array<int> m_aCatalogSources = {};
	protected ref array<int> m_aCatalogCategories = {};
	protected ref array<int> m_aVisibleVehicles = {};
	protected ref map<string, int> m_mCatalogNameCounts = new map<string, int>();
	protected ref array<int> m_aLibraryIds = {};
	protected ref array<int> m_aActiveMembers = {};
	protected ref array<string> m_aSeatNames = {};
	protected ref AICF_VehiclePreset m_Draft;
	protected int m_iSlot;
	protected int m_iRevision = -1;
	protected int m_iGeneration = -1;
	protected int m_iLibraryRevision = -1;
	protected int m_iSize;
	protected int m_iPage;
	protected int m_iToken;
	protected int m_iOperation;
	protected int m_iNextRequestAt;
	protected bool m_bPending;
	protected bool m_bRendering;
	protected bool m_bCaptured;
	protected string m_sView;
	protected string m_sCatalogData;
	protected string m_sLibraryData;
	protected bool m_bQueued;
	protected int m_iQueuedOperation;
	protected int m_iQueuedRevision;
	protected int m_iQueuedGeneration;
	protected int m_iQueuedLibraryRevision;
	protected string m_sQueuedPayload;

	void AICF_VehiclePresetEditor(AICF_StrategicUIController controller) { m_Controller = controller; }
	bool IsInputCaptured() { return m_wRoot != null; }

	protected void Place(Widget widget, float l, float t, float r, float b)
	{
		FrameSlot.SetAnchorMin(widget, l, t);
		FrameSlot.SetAnchorMax(widget, r, b);
		FrameSlot.SetOffsets(widget, 0, 0, 0, 0);
		widget.SetZOrder(12);
	}

	protected TextWidget Label(float l, float t, float r, float b, string text)
	{
		return m_Controller.CreateText(m_wPanel, l, t, r, b, text, 18, Color.White);
	}

	protected void Button(string name, string text, float l, float t, float r, float b)
	{
		Widget frame = m_Controller.CreateRect(m_wPanel, l, t, r, b, Color.FromSRGBA(46, 74, 91, 255), true);
		frame.SetZOrder(12);
		m_Controller.CreateText(frame, 0, 0, 1, 1, text, 18, Color.White, true);
		Widget input = frame.FindAnyWidget("AICF_RectInput");
		input.SetName(name);
		input.AddHandler(this);
		m_aButtons.Insert(input);
	}

	protected SCR_ComboBoxComponent Combo(float l, float t, float r, float b)
	{
		Widget widget = GetGame().GetWorkspace().CreateWidgets(COMBO, m_wPanel);
		Place(widget, l, t, r, b);
		SCR_ComboBoxComponent component = SCR_ComboBoxComponent.Cast(widget.FindHandler(SCR_ComboBoxComponent));
		component.SetLabel("");
		if (component.GetLabelWidget()) component.GetLabelWidget().SetVisible(false);
		component.m_fMaxListHeight = 300;
		component.m_bCreateListBelow = true;
		component.m_OnChanged.Insert(OnChanged);
		return component;
	}

	void Open(Widget parent, int slot)
	{
		Close();
		m_Player = SCR_PlayerController.Cast(GetGame().GetPlayerController());
		if (!m_Player || !parent) return;
		SCR_MapEntity mapEntity = SCR_MapEntity.GetMapInstance();
		if (!mapEntity || !mapEntity.IsOpen()) return;
		m_Cursor = SCR_MapCursorModule.Cast(mapEntity.GetMapModule(SCR_MapCursorModule));
		if (!m_Cursor || (m_Cursor.GetCursorState() & EMapCursorState.CS_DIALOG)) return;
		if (!Create(parent, slot)) { Close(); return; }
		m_bCaptured = true;
		m_Cursor.AICF_SetVehiclePresetDialog(this);
		GetGame().GetWorkspace().AddModal(m_wRoot, m_wPanel);
		GetGame().GetInputManager().AddActionListener(UIConstants.MENU_ACTION_BACK, EActionTrigger.DOWN, OnBack);
		Request(AICF_EVehiclePresetOperation.READ, "");
	}

	protected bool Create(Widget parent, int slot)
	{
		m_iSlot = slot;
		m_wRoot = m_Controller.CreateRect(parent, 0, 0, 1, 1, Color.FromSRGBA(0, 0, 0, 185), true);
		m_wRoot.SetZOrder(410);
		m_wPanel = m_Controller.CreateRect(m_wRoot, 0.08, 0.05, 0.92, 0.95, Color.FromSRGBA(9, 19, 28, 255), true);
		m_wPanel.SetZOrder(5);
		Label(0.03, 0.02, 0.75, 0.08, "{AICF:AICF_UI_VehicleTitle}");
		Button("close", "{AICF:AICF_UI_Close_23b64977}", 0.85, 0.02, 0.97, 0.08);
		string name;
		int size;
		m_Controller.GetLoadoutGroup(slot, name, size);
		Label(0.03, 0.08, 0.97, 0.13, name);
		m_SourceFilter = Combo(0.03, 0.14, 0.49, 0.19);
		m_CategoryFilter = Combo(0.51, 0.14, 0.97, 0.19);
		array<string> sourceKeys = {"{AICF:AICF_UI_VehicleAllSources}", "{AICF:AICF_UI_VehicleFactionSource}", "{AICF:AICF_UI_VehicleCivilianSource}"};
		foreach (string sourceKey : sourceKeys)
			m_SourceFilter.AddItem(AICF_Localization.Resolve(sourceKey));
		array<string> categoryKeys = {"{AICF:AICF_UI_VehicleAllCategories}", "{AICF:AICF_UI_VehicleCars}", "{AICF:AICF_UI_VehicleTrucks}", "{AICF:AICF_UI_VehicleArmor}", "{AICF:AICF_UI_VehicleOther}"};
		foreach (string categoryKey : categoryKeys)
			m_CategoryFilter.AddItem(AICF_Localization.Resolve(categoryKey));
		m_SourceFilter.SetCurrentItem(0, false, false, false);
		m_CategoryFilter.SetCurrentItem(0, false, false, false);
		m_Catalog = Combo(0.03, 0.205, 0.61, 0.255);
		Button("select", "{AICF:AICF_UI_VehicleSelect}", 0.64, 0.205, 0.97, 0.255);
		for (int row; row < 8; row++)
		{
			float y = 0.28 + row * 0.047;
			m_aSeatLabels.Insert(Label(0.03, y, 0.59, y + 0.043, ""));
			m_aSeats.Insert(Combo(0.61, y, 0.97, y + 0.043));
		}
		Button("previous", "<", 0.03, 0.67, 0.09, 0.72);
		Button("next", ">", 0.10, 0.67, 0.16, 0.72);
		m_wUnseated = Label(0.19, 0.67, 0.97, 0.73, "");
		Widget edit = GetGame().GetWorkspace().CreateWidgets(EDIT, m_wPanel);
		Place(edit, 0.03, 0.75, 0.42, 0.81);
		m_wName = EditBoxWidget.Cast(edit.FindAnyWidget("EditBox"));
		Button("save", "{AICF:AICF_UI_VehicleSave}", 0.44, 0.75, 0.60, 0.81);
		Button("apply", "{AICF:AICF_UI_VehicleApply}", 0.62, 0.75, 0.79, 0.81);
		Button("reset", "{AICF:AICF_UI_VehicleReset}", 0.81, 0.75, 0.97, 0.81);
		m_Library = Combo(0.03, 0.83, 0.60, 0.89);
		Button("load", "{AICF:AICF_UI_VehicleLoad}", 0.62, 0.83, 0.79, 0.89);
		Button("delete", "{AICF:AICF_UI_VehicleDelete}", 0.81, 0.83, 0.97, 0.89);
		m_wStatus = Label(0.03, 0.91, 0.97, 0.99, "{AICF:AICF_UI_VehicleWaiting}");
		m_wStatus.SetTextWrapping(true);
		return m_wName != null;
	}

	protected void Request(int operation, string payload)
	{
		if (m_bPending || !m_Player) return;
		m_bQueued = true;
		m_iQueuedOperation = operation;
		m_sQueuedPayload = payload;
		m_iQueuedRevision = m_iRevision;
		m_iQueuedGeneration = m_iGeneration;
		m_iQueuedLibraryRevision = m_iLibraryRevision;
		SendQueued();
	}

	protected void SendQueued()
	{
		if (!m_bQueued || !m_Player || m_bPending || System.GetTickCount() < m_iNextRequestAt) return;
		m_bQueued = false;
		m_iToken = m_Player.AICF_RequestVehiclePreset(m_iSlot, m_iQueuedRevision, m_iQueuedGeneration, m_iQueuedLibraryRevision, m_iQueuedOperation, m_sQueuedPayload);
		m_bPending = m_iToken > 0;
		m_iOperation = m_iQueuedOperation;
		m_iNextRequestAt = System.GetTickCount() + 1000;
		if (m_wStatus) m_wStatus.SetText(AICF_Localization.Resolve("{AICF:AICF_UI_VehicleWaiting}"));
	}

	void Refresh()
	{
		if (!m_wRoot || !m_Player) return;
		SendQueued();
		bool completed = m_bPending && m_Player.AICF_VehiclePresetToken() == m_iToken;
		string view = m_Player.AICF_VehiclePresetView();
		if (completed) m_bPending = false;
		if (view.IsEmpty()) { if (completed || !m_sView.IsEmpty()) Close(); return; }
		if (view == m_sView && !completed) return;
		JsonLoadContext json = new JsonLoadContext();
		int slot, revision, generation;
		string draft, catalog, library, names, status;
		if (!json.LoadFromString(view) || !json.ReadValue("slot", slot) || slot != m_iSlot || !json.ReadValue("revision", revision) ||
			!json.ReadValue("generation", generation)) return;
		bool changed = revision != m_iRevision || generation != m_iGeneration;
		m_iRevision = revision;
		m_iGeneration = generation;
		json.ReadValue("size", m_iSize);
		json.ReadValue("activeMembers", m_aActiveMembers);
		json.ReadValue("status", status);
		json.ReadValue("draft", draft);
		json.ReadValue("catalog", catalog);
		json.ReadValue("library", library);
		json.ReadValue("seatNames", names);
		m_sView = view;
		m_bRendering = true;
		JsonLoadContext list = new JsonLoadContext();
		array<string> captions = {};
		if (catalog != m_sCatalogData && list.LoadFromString(catalog))
		{
			ResourceName selectedPrefab = SelectedPrefab();
			m_sCatalogData = catalog;
			list.ReadValue("prefabs", m_aPrefabs);
			list.ReadValue("names", m_aCatalogNames);
			list.ReadValue("costs", m_aCatalogCosts);
			list.ReadValue("sources", m_aCatalogSources);
			list.ReadValue("categories", m_aCatalogCategories);
			m_mCatalogNameCounts.Clear();
			foreach (string catalogName : m_aCatalogNames)
			{
				int count = 0;
				m_mCatalogNameCounts.Find(catalogName, count);
				m_mCatalogNameCounts.Set(catalogName, count + 1);
			}
			RenderCatalog(selectedPrefab);
		}
		list = new JsonLoadContext();
		if (library != m_sLibraryData && list.LoadFromString(library))
		{
			m_sLibraryData = library;
			list.ReadValue("ids", m_aLibraryIds);
			list.ReadValue("names", captions);
			list.ReadValue("revision", m_iLibraryRevision);
			m_Library.ClearAll();
			foreach (string caption : captions) m_Library.AddItem(caption);
		}
		if (changed || (completed && m_Player.AICF_VehiclePresetAccepted()))
		{
			m_Draft = AICF_VehiclePreset.Decode(draft);
			m_aSeatNames.Clear();
			list = new JsonLoadContext();
			if (list.LoadFromString(names)) list.ReadValue("names", m_aSeatNames);
			m_iPage = 0;
			if (m_Draft) m_wName.SetText(m_Draft.m_sName);
			else m_wName.SetText("");
			if (m_Draft) RevealDraft();
			RenderSeats();
		}
		else RenderSeats();
		m_bRendering = false;
		if (completed && !m_Player.AICF_VehiclePresetAccepted()) status = m_Player.AICF_VehiclePresetReason();
		m_wStatus.SetText(Status(status));
	}

	protected ResourceName SelectedPrefab()
	{
		if (!m_Catalog) return ResourceName.Empty;
		int visible = m_Catalog.GetCurrentIndex();
		if (!m_aVisibleVehicles.IsIndexValid(visible)) return ResourceName.Empty;
		int index = m_aVisibleVehicles[visible];
		if (!m_aPrefabs.IsIndexValid(index)) return ResourceName.Empty;
		return m_aPrefabs[index];
	}

	protected void RenderCatalog(ResourceName selectedPrefab = ResourceName.Empty)
	{
		bool previous = m_bRendering;
		m_bRendering = true;
		m_Catalog.ClearAll();
		m_aVisibleVehicles.Clear();
		int source = m_SourceFilter.GetCurrentIndex();
		int category = m_CategoryFilter.GetCurrentIndex();
		int selected = -1;
		for (int index; index < m_aPrefabs.Count(); index++)
		{
			if (!m_aCatalogNames.IsIndexValid(index) || !m_aCatalogCosts.IsIndexValid(index) || !m_aCatalogSources.IsIndexValid(index) || !m_aCatalogCategories.IsIndexValid(index)) continue;
			if (source > 0 && m_aCatalogSources[index] != source) continue;
			if (category > 0 && m_aCatalogCategories[index] != category) continue;
			if (m_aPrefabs[index] == selectedPrefab) selected = m_aVisibleVehicles.Count();
			m_aVisibleVehicles.Insert(index);
			string caption = AICF_Localization.Resolve(m_aCatalogNames[index]);
			int sameName;
			m_mCatalogNameCounts.Find(m_aCatalogNames[index], sameName);
			if (sameName > 1)
			{
				string variant = FilePath.StripExtension(FilePath.StripPath(m_aPrefabs[index]));
				variant.Replace("_", " ");
				caption += " — " + variant;
			}
			if (source == AICF_EVehicleCatalogSource.ALL)
			{
				string origin = "{AICF:AICF_UI_VehicleFactionSource}";
				if (m_aCatalogSources[index] == AICF_EVehicleCatalogSource.CIVILIAN) origin = "{AICF:AICF_UI_VehicleCivilianSource}";
				caption = AICF_Localization.Resolve(origin) + ": " + caption;
			}
			string price = m_aCatalogCosts[index].ToString();
			if (m_aCatalogCosts[index] <= 0) price = AICF_Localization.Resolve("{AICF:AICF_UI_VehicleNoPrice}");
			m_Catalog.AddItem(caption + " (" + price + ")");
		}
		if (selected < 0 && !m_aVisibleVehicles.IsEmpty()) selected = 0;
		m_Catalog.SetCurrentItem(selected, false, false, false);
		m_bRendering = previous;
	}

	protected void RevealDraft()
	{
		int index = m_aPrefabs.Find(m_Draft.m_sPrefab);
		if (index < 0 || !m_aCatalogSources.IsIndexValid(index) || !m_aCatalogCategories.IsIndexValid(index)) return;
		if (!m_aVisibleVehicles.Contains(index))
		{
			m_SourceFilter.SetCurrentItem(m_aCatalogSources[index], false, false, false);
			m_CategoryFilter.SetCurrentItem(m_aCatalogCategories[index], false, false, false);
		}
		RenderCatalog(m_Draft.m_sPrefab);
	}

	protected string Status(string reason)
	{
		string message = "{AICF:AICF_UI_VehicleUnavailable}";
		switch (reason)
		{
			case "READY":
			case "LOADED": message = "{AICF:AICF_UI_VehicleDraft}"; break;
			case "SAVED": message = "{AICF:AICF_UI_VehicleSaved}"; break;
			case "AUTOMATIC_POLICY_RESTORED":
			case "AUTOMATIC_POLICY": message = "{AICF:AICF_UI_VehicleAutomatic}"; break;
			case "INSUFFICIENT_SEATS": message = "{AICF:AICF_UI_VehicleTooSmall}"; break;
			case "MANDATORY_CREW_MISSING":
			case "DRIVER_SEAT_MISSING": message = "{AICF:AICF_UI_VehicleCrewMissing}"; break;
			case "MEMBER_WITHOUT_SEAT": message = "{AICF:AICF_UI_VehicleMemberMissing}"; break;
			case "MEMBER_INACTIVE": message = "{AICF:AICF_UI_VehicleInactive}"; break;
			case "INSUFFICIENT_SUPPLIES": message = "{AICF:AICF_UI_VehicleNoSupplies}"; break;
			case "FLEET_CAP": message = "{AICF:AICF_UI_VehicleFleetCap}"; break;
			case "REVISION_CONFLICT":
			case "LIBRARY_CONFLICT": message = "{AICF:AICF_UI_VehicleStale}"; break;
			case "DUPLICATE_MEMBER":
			case "DUPLICATE_SEAT": message = "{AICF:AICF_UI_VehicleDuplicate}"; break;
			case "SEAT_TOPOLOGY_CHANGED":
			case "PREFAB_UNAVAILABLE":
				message = "{AICF:AICF_UI_VehicleUnsupported}"; break;
			case "UNSUPPORTED_VEHICLE": message = "{AICF:AICF_UI_VehicleUnsupportedType}"; break;
			case "VEHICLE_COST_UNAVAILABLE": message = "{AICF:AICF_UI_VehicleCostUnavailable}"; break;
			case "WAITING_FOR_ORDER_OR_ADMISSION":
			case "WAITING_FOR_VEHICLE_LIFECYCLE":
			case "WAITING_FOR_SITE":
			case "SITE_PLANNED":
			case "STAGING_CONFIRMED":
			case "SPAWN_COMMIT": message = "{AICF:AICF_UI_VehiclePending}"; break;
			case "APPROACHING_SITE": message = "{AICF:AICF_UI_VehicleApproaching}"; break;
			case "BOARDING": message = "{AICF:AICF_UI_VehicleBoarding}"; break;
			case "TRANSIT": message = "{AICF:AICF_UI_VehicleTransit}"; break;
			case "CLEANUP_PENDING":
			case "DISMOUNT":
			case "HANDOFF": message = "{AICF:AICF_UI_VehicleCleanup}"; break;
		}
		return AICF_Localization.Resolve(message);
	}

	protected void RenderSeats()
	{
		bool previousRendering = m_bRendering;
		m_bRendering = true;
		for (int row; row < m_aSeats.Count(); row++)
		{
			int index = m_iPage * 8 + row;
			bool visible = m_Draft && index < m_Draft.m_aSeats.Count();
			m_aSeats[row].GetRootWidget().SetVisible(visible);
			m_aSeatLabels[row].SetVisible(visible);
			if (!visible) continue;
			string name = m_Draft.m_aSeats[index];
			if (index < m_aSeatNames.Count()) name = AICF_Localization.Resolve(m_aSeatNames[index]);
			m_aSeatLabels[row].SetText((index + 1).ToString() + ". " + name);
			m_aSeats[row].ClearAll();
			m_aSeats[row].AddItem(AICF_Localization.Resolve("{AICF:AICF_UI_VehicleSeatFree}"));
			for (int member; member < AICF_Stage1Config.MAX_GROUP_SIZE; member++)
			{
				string caption = AICF_Localization.Format("{AICF:AICF_UI_VehicleMember}", (member + 1).ToString());
				if (!m_aActiveMembers.Contains(member)) caption = AICF_Localization.Format("{AICF:AICF_UI_VehicleInactiveMember}", (member + 1).ToString());
				m_aSeats[row].AddItem(AICF_Localization.Resolve(caption));
			}
			m_aSeats[row].SetCurrentItem(m_Draft.m_aMembers[index] + 1, false, false, false);
		}
		string unseated;
		foreach (int position : m_aActiveMembers)
			if (!m_Draft || !m_Draft.m_aMembers.Contains(position)) unseated += (position + 1).ToString() + " ";
		m_wUnseated.SetText(AICF_Localization.Format("{AICF:AICF_UI_VehicleUnseated}", unseated));
		m_bRendering = previousRendering;
	}

	protected void OnChanged(SCR_ComboBoxComponent combo, int item)
	{
		if (m_bRendering) return;
		if (combo == m_SourceFilter || combo == m_CategoryFilter)
		{
			RenderCatalog(SelectedPrefab());
			return;
		}
		if (m_bPending || m_bQueued || !m_Draft) return;
		int row = m_aSeats.Find(combo);
		if (row < 0) return;
		m_Draft.Assign(m_iPage * 8 + row, item - 1);
		RenderSeats();
	}

	override bool OnClick(Widget w, int x, int y, int button)
	{
		if (!m_aButtons.Contains(w)) return false;
		string action = w.GetName();
		if (action == "close") { Close(); return true; }
		if (m_bPending || m_bQueued) return true;
		if (action == "select")
		{
			ResourceName selected = SelectedPrefab();
			if (!selected.IsEmpty()) Request(AICF_EVehiclePresetOperation.SELECT, selected);
		}
		else if (action == "load" || action == "delete")
		{
			int libraryIndex = m_Library.GetCurrentIndex();
			int operation = AICF_EVehiclePresetOperation.LOAD;
			if (action == "delete") operation = AICF_EVehiclePresetOperation.DELETE;
			if (m_aLibraryIds.IsIndexValid(libraryIndex)) Request(operation, m_aLibraryIds[libraryIndex].ToString());
		}
		else if (action == "reset") Request(AICF_EVehiclePresetOperation.RESET, "");
		else if (m_Draft && (action == "save" || action == "apply"))
		{
			m_Draft.m_sName = m_wName.GetText();
			int operation = AICF_EVehiclePresetOperation.APPLY;
			if (action == "save") operation = AICF_EVehiclePresetOperation.SAVE;
			Request(operation, m_Draft.Encode());
		}
		else if (action == "previous" && m_iPage > 0) { m_iPage--; RenderSeats(); }
		else if (action == "next" && m_Draft && (m_iPage + 1) * 8 < m_Draft.m_aSeats.Count()) { m_iPage++; RenderSeats(); }
		return true;
	}

	protected void OnBack(float value = 0, EActionTrigger reason = EActionTrigger.DOWN) { Close(); }

	protected void ClearCombo(SCR_ComboBoxComponent combo)
	{
		if (!combo) return;
		combo.CloseList();
		combo.m_OnChanged.Remove(OnChanged);
	}

	void Close()
	{
		if (m_Cursor) m_Cursor.AICF_ClearVehiclePresetDialog(this);
		m_Cursor = null;
		if (m_bCaptured && m_wRoot)
		{
			GetGame().GetInputManager().RemoveActionListener(UIConstants.MENU_ACTION_BACK, EActionTrigger.DOWN, OnBack);
			GetGame().GetWorkspace().RemoveModal(m_wRoot);
		}
		ClearCombo(m_Catalog);
		ClearCombo(m_SourceFilter);
		ClearCombo(m_CategoryFilter);
		ClearCombo(m_Library);
		foreach (SCR_ComboBoxComponent combo : m_aSeats) ClearCombo(combo);
		foreach (Widget input : m_aButtons) input.RemoveHandler(this);
		m_aSeats.Clear();
		m_aSeatLabels.Clear();
		m_aButtons.Clear();
		if (m_wRoot) m_wRoot.RemoveFromHierarchy();
		m_wRoot = null;
		m_Catalog = null;
		m_SourceFilter = null;
		m_CategoryFilter = null;
		m_Library = null;
		m_Draft = null;
		m_Player = null;
		m_bPending = false;
		m_bCaptured = false;
		m_bQueued = false;
		m_iNextRequestAt = 0;
		m_iRevision = -1;
		m_iGeneration = -1;
		m_sView = "";
		m_sCatalogData = "";
		m_sLibraryData = "";
		m_aActiveMembers.Clear();
		m_aPrefabs.Clear();
		m_aCatalogNames.Clear();
		m_aCatalogCosts.Clear();
		m_aCatalogSources.Clear();
		m_aCatalogCategories.Clear();
		m_aVisibleVehicles.Clear();
		m_mCatalogNameCounts.Clear();
	}
}
