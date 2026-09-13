// Local presentation only. Stock layouts supply their native mouse/focus handlers.
class AICF_SupplyMapUI : ScriptedWidgetEventHandler
{
	protected static const ResourceName COMBO_LAYOUT = "{4B5AE6E64037FFB4}UI/layouts/WidgetLibrary/ComboBox/WLib_ComboBox.layout";
	protected static const ResourceName SLIDER_LAYOUT = "{4A41296C0E9A889F}UI/layouts/WidgetLibrary/WLib_Slider.layout";
	protected AICF_StrategicUIController m_Controller;
	protected SCR_MapCursorModule m_Cursor;
	protected Widget m_wToggle;
	protected Widget m_wScrim;
	protected Widget m_wPanel;
	protected Widget m_wClose;
	protected Widget m_wCombo;
	protected Widget m_wDestination;
	protected Widget m_wSend;
	protected Widget m_wSlider;
	protected TextWidget m_wAvailable;
	protected TextWidget m_wAmount;
	protected TextWidget m_wStatus;
	protected TextWidget m_wResult;
	protected SCR_ComboBoxComponent m_DestinationCombo;
	protected SCR_ComboBoxComponent m_Combo;
	protected SCR_SliderComponent m_Slider;
	protected Faction m_Faction;
	protected ref array<ref AICF_SupplyMapBase> m_aBases = {};
	protected ref AICF_SupplyMapBase m_Selected;
	protected ref AICF_SupplyMapBase m_Destination;
	protected ref array<ref AICF_SupplyMapBase> m_aDestinations = {};
	protected bool m_bCanSend;
	protected int m_iAmount;
	protected int m_iMaximum;
	protected bool m_bOpen;
	protected bool m_bRendering;
	protected bool m_bInputCaptured;

	void AICF_SupplyMapUI(AICF_StrategicUIController controller)
	{
		m_Controller = controller;
	}

	void Attach(Widget mapRoot)
	{
		Detach();
		if (!mapRoot || RplSession.Mode() == RplMode.Dedicated)
			return;
		m_wToggle = m_Controller.CreateRect(mapRoot, 0.25, 0.025, 0.39, 0.075,
			Color.FromSRGBA(12, 25, 34, 248), true);
		if (!m_wToggle)
			return;
		m_wToggle.SetName("AICF_SupplyToggle");
		m_wToggle.SetZOrder(221);
		m_Controller.CreateText(m_wToggle, 0, 0, 1, 1, "Снабжение", 20,
			Color.FromSRGBA(236, 242, 245, 255), true);
		BindRect(m_wToggle, true);
	}

	bool IsInputCaptured()
	{
		return m_bInputCaptured;
	}

	protected bool CreatePanel()
	{
		if (m_wPanel)
			return true;
		m_wScrim = m_Controller.CreateRect(m_wToggle.GetParent(), 0, 0, 1, 1,
			Color.FromSRGBA(0, 0, 0, 150), true);
		if (!m_wScrim)
			return false;
		m_wScrim.SetZOrder(300);
		BindRect(m_wScrim, true);
		m_wPanel = m_Controller.CreateRect(m_wScrim, 0.29, 0.17, 0.71, 0.81,
			Color.FromSRGBA(8, 18, 26, 250), true);
		if (!m_wPanel)
			return false;
		m_wPanel.SetName("AICF_SupplyPanel");
		m_wPanel.SetZOrder(5);
		BindRect(m_wPanel, true);
		m_Controller.CreateText(m_wPanel, 0.05, 0.03, 0.72, 0.11, "СНАБЖЕНИЕ", 26,
			Color.FromSRGBA(226, 167, 79, 255));
		m_wClose = m_Controller.CreateRect(m_wPanel, 0.77, 0.03, 0.95, 0.11,
			Color.FromSRGBA(91, 34, 34, 250), true);
		if (!m_wClose)
			return false;
		m_wClose.SetZOrder(5);
		m_Controller.CreateText(m_wClose, 0, 0, 1, 1, "Закрыть", 18,
			Color.FromSRGBA(255, 232, 232, 255), true);
		BindRect(m_wClose, true);
		m_Controller.CreateText(m_wPanel, 0.05, 0.14, 0.95, 0.19, "Откуда — база-источник", 19,
			Color.FromSRGBA(236, 242, 245, 255));
		m_Controller.CreateText(m_wPanel, 0.05, 0.30, 0.95, 0.35, "Куда — база назначения", 19,
			Color.FromSRGBA(236, 242, 245, 255));

		m_wCombo = GetGame().GetWorkspace().CreateWidgets(COMBO_LAYOUT, m_wPanel);
		m_wDestination = GetGame().GetWorkspace().CreateWidgets(COMBO_LAYOUT, m_wPanel);
		m_wSlider = GetGame().GetWorkspace().CreateWidgets(SLIDER_LAYOUT, m_wPanel);
		if (!m_wCombo || !m_wDestination || !m_wSlider)
			return false;
		PlaceControl(m_wCombo, 0.05, 0.20, 0.95, 0.28);
		PlaceControl(m_wDestination, 0.05, 0.36, 0.95, 0.44);
		PlaceControl(m_wSlider, 0.05, 0.60, 0.95, 0.69);
		m_Combo = SCR_ComboBoxComponent.Cast(m_wCombo.FindHandler(SCR_ComboBoxComponent));
		m_DestinationCombo = SCR_ComboBoxComponent.Cast(m_wDestination.FindHandler(SCR_ComboBoxComponent));
		m_Slider = SCR_SliderComponent.Cast(m_wSlider.FindHandler(SCR_SliderComponent));
		if (!m_Combo || !m_DestinationCombo || !m_Slider)
			return false;
		HideNativeLabel(m_Combo);
		HideNativeLabel(m_DestinationCombo);
		m_Combo.m_fMaxListHeight = 260;
		m_Combo.m_bCreateListBelow = true;
		m_DestinationCombo.m_fMaxListHeight = 220;
		m_DestinationCombo.m_bCreateListBelow = true;
		HideNativeLabel(m_Slider);
		m_Combo.m_OnChanged.Insert(OnBaseChanged);
		m_DestinationCombo.m_OnChanged.Insert(OnDestinationChanged);
		m_Slider.m_OnChanged.Insert(OnAmountChanged);
		m_wAvailable = m_Controller.CreateText(m_wPanel, 0.05, 0.46, 0.95, 0.52,
			"", 18, Color.FromSRGBA(236, 242, 245, 255));
		m_wAmount = m_Controller.CreateText(m_wPanel, 0.05, 0.53, 0.95, 0.59,
			"", 21, Color.FromSRGBA(226, 167, 79, 255));
		m_wStatus = m_Controller.CreateText(m_wPanel, 0.05, 0.70, 0.95, 0.76,
			"", 16, Color.FromSRGBA(192, 207, 215, 255));
		m_wResult = m_Controller.CreateText(m_wPanel, 0.05, 0.77, 0.95, 0.89,
			"", 16, Color.FromSRGBA(236, 242, 245, 255));
		if (!m_wResult) return false;
		m_wResult.SetTextWrapping(true);
		m_wStatus.SetTextWrapping(true);
		m_wSend = m_Controller.CreateRect(m_wPanel, 0.60, 0.90, 0.95, 0.98,
			Color.FromSRGBA(181, 123, 35, 255), true);
		if (!m_wSend) return false;
		m_wSend.SetName("AICF_SupplySend");
		m_wSend.SetZOrder(5);
		m_Controller.CreateText(m_wSend, 0, 0, 1, 1, "Отправить", 20,
			Color.FromSRGBA(255, 255, 255, 255), true);
		BindRect(m_wSend, true);
		return m_wAvailable && m_wAmount && m_wStatus && m_wResult;
	}

	protected void HideNativeLabel(SCR_ChangeableComponentBase component)
	{
		// UseLabel(false) удаляет label вместе с animation component, на который
		// stock SCR_AutomaticScrollComponent уже ссылается после HandlerAttached.
		// Скрываем существующий label, сохраняя его lifetime до удаления control.
		component.SetLabel(string.Empty);
		Widget label = component.GetLabelWidget();
		if (label)
			label.SetVisible(false);
	}

	protected void PlaceControl(Widget widget, float left, float top, float right, float bottom)
	{
		FrameSlot.SetAnchorMin(widget, left, top);
		FrameSlot.SetAnchorMax(widget, right, bottom);
		FrameSlot.SetOffsets(widget, 0, 0, 0, 0);
		widget.SetZOrder(5);
	}

	protected void BindRect(Widget rect, bool attach)
	{
		if (!rect)
			return;
		Widget input = rect.FindAnyWidget("AICF_RectInput");
		if (!input)
			return;
		if (attach)
			input.AddHandler(this);
		else
			input.RemoveHandler(this);
	}

	protected void Open()
	{
		if (m_bOpen || !m_wToggle)
			return;
		SCR_MapEntity mapEntity = SCR_MapEntity.GetMapInstance();
		if (!mapEntity || !mapEntity.IsOpen())
			return;
		SCR_MapCursorModule cursor = SCR_MapCursorModule.Cast(mapEntity.GetMapModule(SCR_MapCursorModule));
		if (!cursor)
			return;
		// Do not take another stock dialog's cursor state.
		if (!m_bInputCaptured && (cursor.GetCursorState() & EMapCursorState.CS_DIALOG))
			return;
		if (!CreatePanel())
		{
			Print("[AICF][SUPPLY_UI][ERROR] reason=STOCK_WIDGET_UNAVAILABLE", LogLevel.ERROR);
			Detach();
			return;
		}
		m_Controller.CloseCommandForSupplies();
		SCR_MapToolMenuUI tools = SCR_MapToolMenuUI.Cast(mapEntity.GetMapUIComponent(SCR_MapToolMenuUI));
		if (tools)
		{
			foreach (SCR_MapToolEntry tool : tools.GetMenuEntries())
			{
				if (tool && tool.IsExclusiveEntry() && tool.IsEntryActive())
					tool.OnDisableMapUIComponent();
			}
			tools.SetToolMenuFocused(false);
		}
		GetGame().GetCallqueue().Remove(ReleaseInput);
		m_Cursor = cursor;
		m_bInputCaptured = true;
		m_Cursor.AICF_SetSupplyDialog(this);
		m_bOpen = true;
		m_wScrim.SetVisible(true);
		m_wToggle.SetVisible(false);
		GetGame().GetInputManager().AddActionListener(UIConstants.MENU_ACTION_BACK, EActionTrigger.DOWN, OnBack);
		Refresh();
		GetGame().GetWorkspace().SetFocusedWidget(m_wClose);
	}

	void Close()
	{
		if (!m_bOpen)
			return;
		m_bOpen = false;
		GetGame().GetInputManager().RemoveActionListener(UIConstants.MENU_ACTION_BACK, EActionTrigger.DOWN, OnBack);
		if (m_Combo)
			m_Combo.CloseList();
		if (m_DestinationCombo)
			m_DestinationCombo.CloseList();
		if (m_wScrim)
			m_wScrim.SetVisible(false);
		if (m_wToggle)
			m_wToggle.SetVisible(true);
		GetGame().GetWorkspace().SetFocusedWidget(null);
		// Keep the closing mouse-up from also selecting an item underneath on the map.
		GetGame().GetCallqueue().CallLater(ReleaseInput, 0, false);
	}

	protected void OnBack()
	{
		Close();
	}

	protected void ReleaseInput()
	{
		GetGame().GetCallqueue().Remove(ReleaseInput);
		if (m_Cursor)
			m_Cursor.AICF_ClearSupplyDialog(this);
		m_bInputCaptured = false;
		m_Cursor = null;
	}

	void Detach()
	{
		Close();
		ReleaseInput();
		if (m_Combo)
		{
			m_Combo.CloseList();
			m_Combo.m_OnChanged.Remove(OnBaseChanged);
		}
		if (m_Slider)
			m_Slider.m_OnChanged.Remove(OnAmountChanged);
		if (m_DestinationCombo)
		{
			m_DestinationCombo.CloseList();
			m_DestinationCombo.m_OnChanged.Remove(OnDestinationChanged);
		}
		BindRect(m_wSend, false);
		BindRect(m_wClose, false);
		BindRect(m_wPanel, false);
		if (m_wScrim)
		{
			BindRect(m_wScrim, false);
			m_wScrim.RemoveFromHierarchy();
		}
		if (m_wToggle)
		{
			BindRect(m_wToggle, false);
			m_wToggle.RemoveFromHierarchy();
		}
		m_wToggle = null;
		m_wScrim = null;
		m_wPanel = null;
		m_wClose = null;
		m_wCombo = null;
		m_wDestination = null;
		m_wSend = null;
		m_wResult = null;
		m_DestinationCombo = null;
		m_Destination = null;
		m_aDestinations.Clear();
		m_bCanSend = false;
		m_wSlider = null;
		m_wAvailable = null;
		m_wAmount = null;
		m_wStatus = null;
		m_Combo = null;
		m_Slider = null;
		m_Faction = null;
		m_Selected = null;
		m_aBases.Clear();
		m_iAmount = 0;
		m_iMaximum = 0;
	}

	// Called by the existing 500 ms UI tick, including when the local faction is absent.
	void Refresh()
	{
		m_Controller.SetRectColor(m_wToggle, Color.FromSRGBA(12, 25, 34, 248));
		if (!m_bOpen || !m_Combo || !m_Slider)
			return;
		// Pause-menu transitions reset the stock cursor; restore our modal state on return.
		if (m_Cursor)
			m_Cursor.AICF_SetSupplyDialog(this);
		m_Controller.SetRectColor(m_wScrim, Color.FromSRGBA(0, 0, 0, 150));
		m_Controller.SetRectColor(m_wPanel, Color.FromSRGBA(8, 18, 26, 250));
		m_Controller.SetRectColor(m_wClose, Color.FromSRGBA(91, 34, 34, 250));
		Faction faction = SCR_FactionManager.SGetLocalPlayerFaction();
		if (faction != m_Faction)
		{
			m_Faction = faction;
			m_Selected = null;
			m_iAmount = 0;
			m_Destination = null;
		}
		array<ref AICF_SupplyMapBase> current = {};
		AICF_SupplyMapData.Collect(m_Faction, current);
		bool changed = current.Count() != m_aBases.Count();
		if (!changed)
		{
			foreach (int index, AICF_SupplyMapBase entry : current)
			{
				if (entry.m_Base != m_aBases[index].m_Base || entry.m_EntityId != m_aBases[index].m_EntityId ||
					entry.m_sName != m_aBases[index].m_sName)
				{
					changed = true;
					break;
				}
			}
		}
		if (changed || m_Combo.GetNumItems() == 0 || (!m_Selected && !current.IsEmpty()))
			RebuildList(current);
		RefreshAmount();
	}

	protected void RebuildList(array<ref AICF_SupplyMapBase> current)
	{
		m_bRendering = true;
		// Close before replacing indexes: an old popup may not select a different base.
		m_Combo.CloseList();
		m_Combo.ClearAll();
		int selectedIndex = -1;
		m_aBases.Clear();
		foreach (int index, AICF_SupplyMapBase entry : current)
		{
			m_aBases.Insert(entry);
			m_Combo.AddItem(entry.m_sName);
			if (m_Selected && entry.m_Base == m_Selected.m_Base && entry.m_EntityId == m_Selected.m_EntityId)
				selectedIndex = index;
		}
		if (selectedIndex < 0)
		{
			m_Selected = null;
			m_iAmount = 0;
			if (!m_aBases.IsEmpty())
				selectedIndex = 0;
		}
		if (selectedIndex >= 0)
		{
			m_Selected = m_aBases[selectedIndex];
			m_Combo.SetCurrentItem(selectedIndex, false, false, false);
		}
		else
		{
			m_Combo.AddItem("Нет доступных баз");
			m_Combo.SetCurrentItem(0, false, false, false);
		}
		m_Combo.SetEnabled(!m_aBases.IsEmpty());
		m_bRendering = false;
		RebuildDestinations();
	}

	protected void RebuildDestinations()
	{
		m_bRendering = true;
		m_DestinationCombo.CloseList();
		m_DestinationCombo.ClearAll();
		m_aDestinations.Clear();
		int selectedIndex = -1;
		foreach (AICF_SupplyMapBase entry : m_aBases)
		{
			if (m_Selected && entry.m_EntityId == m_Selected.m_EntityId) continue;
			m_aDestinations.Insert(entry);
			m_DestinationCombo.AddItem(entry.m_sName);
			if (m_Destination && entry.m_Base == m_Destination.m_Base && entry.m_EntityId == m_Destination.m_EntityId)
				selectedIndex = m_aDestinations.Count() - 1;
		}
		m_Destination = null;
		// Назначение выбирается явно: потеря базы никогда не подменяет адрес доставки.
		if (selectedIndex < 0)
		{
			m_DestinationCombo.AddItem("Выберите базу назначения");
			selectedIndex = m_aDestinations.Count();
		}
		else m_Destination = m_aDestinations[selectedIndex];
		m_DestinationCombo.SetCurrentItem(selectedIndex, false, false, false);
		m_bRendering = false;
	}

	protected void RefreshAmount()
	{
		bool ready = AICF_SupplyMapData.ReadSupplies(m_Selected, m_Faction, m_iMaximum);
		m_iAmount = Math.ClampInt(m_iAmount, 0, m_iMaximum);
		m_bRendering = true;
		m_Slider.SetEnabled(ready && m_iMaximum > 0);
		m_Slider.SetSliderSettings(0, m_iMaximum, 1, "%1");
		if (m_Slider.GetValue() != m_iAmount)
			m_Slider.SetValue(m_iAmount);
		m_bRendering = false;
		int free;
		bool destinationReady = AICF_SupplyMapData.ReadFreeSpace(m_Destination, m_Faction, free);
		m_wAvailable.SetText(string.Format("У источника: %1  •  Место у назначения: %2", m_iMaximum, free));
		m_wAmount.SetText(string.Format("Выбрано: %1 / %2", m_iAmount, m_iMaximum));
		string status = "Выберите количество и нажмите «Отправить».";
		if (!m_Faction)
			status = "Выберите фракцию, чтобы увидеть её базы.";
		else if (!m_Selected)
			status = "Под контролем вашей фракции нет доступных баз.";
		else if (!ready)
		{
			m_wAvailable.SetText("Припасы базы: ожидаются данные");
			status = "Данные о складе пока недоступны.";
		}
		else if (m_iMaximum == 0)
			status = "На этой базе нет доступных припасов.";
		else if (m_aDestinations.IsEmpty())
			status = "Нужна ещё одна союзная база для доставки.";
		else if (!m_Destination)
			status = "Выберите, куда доставить припасы.";
		else if (!destinationReady)
			status = "Ожидаются данные склада назначения.";
		else if (m_iAmount > free || free == 0)
			status = string.Format("На базе назначения свободно: %1. Уменьшите количество.", free);
		SCR_PlayerController player = SCR_PlayerController.Cast(GetGame().GetPlayerController());
		bool busy = player && player.AICF_IsSupplyBusy();
		m_bCanSend = player && !busy && ready && destinationReady && m_iAmount > 0 && m_iAmount <= free &&
			m_Selected && m_Destination && m_Selected.m_EntityId != m_Destination.m_EntityId &&
			m_Selected.NetworkId().IsValid() && m_Destination.NetworkId().IsValid();
		m_wSend.SetEnabled(m_bCanSend);
		Color sendColor = Color.FromSRGBA(58, 64, 69, 255);
		if (m_bCanSend) sendColor = Color.FromSRGBA(181, 123, 35, 255);
		m_Controller.SetRectColor(m_wSend, sendColor);
		m_Combo.SetEnabled(!busy && !m_aBases.IsEmpty());
		m_DestinationCombo.SetEnabled(!busy && !m_aDestinations.IsEmpty());
		if (busy)
		{
			m_Slider.SetEnabled(false);
		}
		// Во время рейса отдаём место маршруту, состоянию и времени прибытия.
		m_wStatus.SetVisible(!busy);
		if (busy) PlaceControl(m_wResult, 0.05, 0.70, 0.95, 0.89);
		else PlaceControl(m_wResult, 0.05, 0.77, 0.95, 0.89);
		if (player) m_wResult.SetText(player.AICF_GetSupplyStatus());
		else m_wResult.SetText("Ожидается подключение игрока.");
		m_wStatus.SetText(status);
	}

	protected void OnBaseChanged(SCR_ComboBoxComponent component, int index)
	{
		if (m_bRendering || !m_bOpen || !m_aBases.IsIndexValid(index))
			return;
		AICF_SupplyMapBase selected = m_aBases[index];
		if (!selected.IsOwnedBy(SCR_FactionManager.SGetLocalPlayerFaction()))
		{
			Refresh();
			return;
		}
		m_Selected = selected;
		RebuildDestinations();
		Refresh();
	}

	protected void OnDestinationChanged(SCR_ComboBoxComponent component, int index)
	{
		if (m_bRendering || !m_bOpen) return;
		m_Destination = null;
		if (m_aDestinations.IsIndexValid(index) && m_aDestinations[index].IsOwnedBy(SCR_FactionManager.SGetLocalPlayerFaction()))
			m_Destination = m_aDestinations[index];
		Refresh();
	}

	protected void Send()
	{
		Refresh();
		if (!m_bOpen || !m_bCanSend) return;
		SCR_PlayerController player = SCR_PlayerController.Cast(GetGame().GetPlayerController());
		if (!player) return;
		player.AICF_RequestSupplyTransport(m_Selected.NetworkId(), m_Destination.NetworkId(), m_iAmount);
		Refresh();
	}

	protected void OnAmountChanged(SCR_SliderComponent component, float value)
	{
		if (m_bRendering || !m_bOpen)
			return;
		m_iAmount = Math.Max(0, Math.Floor(value));
		// Re-read ownership and supplies during drag, not just at the periodic tick.
		Refresh();
	}

	override bool OnClick(Widget w, int x, int y, int button)
	{
		if (button != 0)
			return m_bOpen;
		Widget parent = w;
		while (parent)
		{
			if (parent == m_wToggle)
			{
				Open();
				return true;
			}
			if (parent == m_wClose)
			{
				Close();
				return true;
			}
			if (parent == m_wSend)
			{
				Send();
				return true;
			}
			if (parent == m_wPanel)
				return true;
			if (parent == m_wScrim)
			{
				Close();
				return true;
			}
			parent = parent.GetParent();
		}
		return false;
	}
}
