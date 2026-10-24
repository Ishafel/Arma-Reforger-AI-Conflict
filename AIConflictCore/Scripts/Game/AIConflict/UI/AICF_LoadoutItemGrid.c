// Виртуальная прокрутка: живут только видимые карточки, независимо от размера
// faction catalog. Миниатюры prefab рисует штатный UI manager; изменяемый
// персонаж/recipe никогда не передаются в его общий cache.
class AICF_LoadoutItemCard
{
	Widget m_Root;
	Widget m_Input;
	Widget m_Accent;
	ItemPreviewWidget m_Preview;
	TextWidget m_Name;
	TextWidget m_Detail;
	ResourceName m_sBoundPrefab;
}

class AICF_LoadoutItemGrid : ScriptedWidgetEventHandler
{
	protected AICF_StrategicUIController m_Controller;
	protected Widget m_Root;
	protected TextWidget m_Range;
	protected TextWidget m_Empty;
	protected ref array<ref AICF_LoadoutItemCard> m_aCards = {};
	protected ref array<string> m_aPrefabs = {};
	protected ref array<string> m_aNames = {};
	protected ref array<string> m_aDetails = {};
	protected ItemPreviewManagerEntity m_Manager;
	protected SliderWidget m_Scrollbar;
	protected Widget m_ScrollTrack;
	protected Widget m_ScrollThumb;
	protected bool m_bScrollUpdating;
	protected int m_iColumns;
	protected int m_iFirst;
	protected int m_iSelected = -1;
	protected bool m_bEnabled = true;
	ref ScriptInvoker m_OnSelected = new ScriptInvoker();
	ref ScriptInvoker m_OnActivated = new ScriptInvoker();
	ref ScriptInvoker m_OnRemoved = new ScriptInvoker();

	bool Create(AICF_StrategicUIController controller, Widget parent, float l, float t, float r, float b, int columns, int rows)
	{
		m_Controller = controller;
		m_iColumns = columns;
		m_Root = controller.CreateRect(parent, l, t, r, b, Color.FromSRGBA(12, 25, 35, 255), true);
		if (!m_Root)
			return false;
		m_Root.SetZOrder(10);
		m_Root.AddHandler(this);
		for (int i; i < columns * rows; i++)
		{
			AICF_LoadoutItemCard card = new AICF_LoadoutItemCard();
			m_aCards.Insert(card);
			float x = Math.Mod(i, columns) / (float)columns * 0.91;
			float y = Math.Floor(i / columns) / rows * 0.91;
			card.m_Root = controller.CreateRect(m_Root, x + 0.008, y + 0.006, x + 0.91 / columns - 0.008, y + 0.91 / rows - 0.006, Color.FromSRGBA(27, 43, 54, 255), true);
			if (!card.m_Root)
				return false;
			card.m_Root.SetZOrder(5);
			card.m_Accent = controller.CreateRect(card.m_Root, 0, 0, 0.015, 1, Color.FromSRGBA(226, 167, 79, 255), false);
			card.m_Accent.SetZOrder(3);
			card.m_Input = card.m_Root.FindAnyWidget("AICF_RectInput");
			if (!card.m_Input)
				return false;
			card.m_Input.SetName(i.ToString());
			card.m_Input.AddHandler(this);
			card.m_Preview = ItemPreviewWidget.Cast(GetGame().GetWorkspace().CreateWidget(GameWidgetType.ItemPreviewWidgetTypeID,
				WidgetFlags.BLEND | WidgetFlags.IGNORE_CURSOR, Color.White, 0, card.m_Root));
			if (!card.m_Preview)
				return false;
			FrameSlot.SetAnchorMin(card.m_Preview, 0.06, 0.02);
			FrameSlot.SetAnchorMax(card.m_Preview, 0.94, 0.58);
			FrameSlot.SetOffsets(card.m_Preview, 0, 0, 0, 0);
			card.m_Preview.SetFlags(WidgetFlags.BLEND | WidgetFlags.IGNORE_CURSOR);
			card.m_Preview.SetZOrder(2);
			card.m_Preview.SetMaxFPS(5);
			card.m_Preview.SetResolutionScale(0.5, 0.5);
			card.m_Preview.SetClearColor(true, 0);
			card.m_Preview.SetBlendMode(RenderTargetWidgetBlendMode.ALPHA_BLEND);
			card.m_Preview.SetFormat(RenderTargetWidgetFormat.LDR_SRGB);
			card.m_Name = controller.CreateText(card.m_Root, 0.035, 0.59, 0.965, 0.85, "", 16, Color.FromSRGBA(230, 237, 242, 255));
			card.m_Name.SetTextWrapping(true);
			card.m_Detail = controller.CreateText(card.m_Root, 0.035, 0.85, 0.965, 0.99, "", 14, Color.FromSRGBA(170, 194, 206, 255));
			if (columns == 1)
			{
				FrameSlot.SetAnchorMin(card.m_Preview, 0.025, 0.05);
				FrameSlot.SetAnchorMax(card.m_Preview, 0.20, 0.95);
				FrameSlot.SetAnchorMin(card.m_Name, 0.23, 0.04);
				FrameSlot.SetAnchorMax(card.m_Name, 0.97, 0.51);
				FrameSlot.SetAnchorMin(card.m_Detail, 0.23, 0.51);
				FrameSlot.SetAnchorMax(card.m_Detail, 0.97, 0.96);
			}
		}
		m_ScrollTrack = controller.CreateRect(m_Root, 0.963, 0.01, 0.98, 0.91, Color.FromSRGBA(45, 61, 72, 255), false);
		m_ScrollThumb = controller.CreateRect(m_Root, 0.948, 0.01, 0.996, 0.15, Color.FromSRGBA(226, 167, 79, 255), false);
		m_ScrollTrack.SetZOrder(6);
		m_ScrollThumb.SetZOrder(7);
		// Native input slider, как в setup time slider; видимый thumb показывает
		// долю viewport, а карточки остаются виртуализованными.
		m_Scrollbar = SliderWidget.Cast(GetGame().GetWorkspace().CreateWidget(WidgetType.SliderWidgetTypeID,
			WidgetFlags.VISIBLE | WidgetFlags.BLEND, Color.White, 20, m_Root));
		if (!m_Scrollbar) return false;
		m_Scrollbar.SetName("AICF_LoadoutScroll");
		m_Scrollbar.SetOpacity(0);
		m_Scrollbar.SetVertical(true);
		m_Scrollbar.SetFlipped(true);
		m_Scrollbar.SetRange(0, 1);
		m_Scrollbar.SetStep(columns);
		FrameSlot.SetAnchorMin(m_Scrollbar, 0.925, 0.01);
		FrameSlot.SetAnchorMax(m_Scrollbar, 1, 0.91);
		FrameSlot.SetOffsets(m_Scrollbar, 0, 0, 0, 0);
		m_Scrollbar.AddHandler(this);
		m_Range = controller.CreateText(m_Root, 0.01, 0.92, 0.91, 1, "", 14, Color.FromSRGBA(170, 194, 206, 255), true);
		m_Empty = controller.CreateText(m_Root, 0.06, 0.25, 0.94, 0.65, "{AICF:AICF_UI_No_items_03a89de6}", 17, Color.FromSRGBA(170, 194, 206, 255), true);
		m_Empty.SetTextWrapping(true);
		m_Empty.SetVisible(false);
		return true;
	}

	protected void UpdateScrollbar()
	{
		int last = LastPageOffset(m_aPrefabs.Count(), m_iColumns, m_aCards.Count());
		bool overflow = last > 0;
		m_Scrollbar.SetVisible(overflow);
		m_ScrollTrack.SetVisible(overflow);
		m_ScrollThumb.SetVisible(overflow);
		m_Scrollbar.SetEnabled(m_bEnabled && overflow);
		m_bScrollUpdating = true;
		m_Scrollbar.SetRange(0, Math.Max(1, last));
		m_Scrollbar.SetCurrent(m_iFirst);
		m_bScrollUpdating = false;
		if (!overflow) return;
		float fill = Math.Clamp(m_aCards.Count() / (float)m_aPrefabs.Count(), 0.08, 1);
		float top = 0.01 + (0.90 - 0.90 * fill) * m_iFirst / last;
		FrameSlot.SetAnchorMin(m_ScrollThumb, 0.948, top);
		FrameSlot.SetAnchorMax(m_ScrollThumb, 0.996, top + 0.90 * fill);
	}

	void SetEmptyText(string text) { m_Empty.SetText(AICF_Localization.Resolve(text)); }

	bool SetItems(array<string> prefabs, array<string> names, array<string> details, bool preserveScroll = false)
	{
		bool unchanged = preserveScroll && prefabs.Count() == m_aPrefabs.Count();
		if (unchanged)
		{
			for (int i; i < prefabs.Count(); i++)
			{
				if (prefabs[i] != m_aPrefabs[i])
				{
					unchanged = false;
					break;
				}
			}
		}
		GetGame().GetCallqueue().Remove(Render);
		if (!m_Manager)
			m_Manager = ResolveThumbnailManager();
		m_aPrefabs.Copy(prefabs);
		m_aNames.Copy(names);
		m_aDetails.Copy(details);
		if (!unchanged)
			m_iFirst = 0;
		m_iSelected = -1;
		// Bind после native widget initialization; до него RT скрыт.
		GetGame().GetCallqueue().CallLater(Render, 1, false);
		return unchanged;
	}

	// Штатный inventory UI создаёт такой же client-local manager по требованию.
	// Здесь он получает только prefab names, никогда изменяемый draft character.
	static ItemPreviewManagerEntity ResolveThumbnailManager()
	{
		if (RplSession.Mode() == RplMode.Dedicated)
			return null;
		ChimeraWorld world = ChimeraWorld.CastFrom(GetGame().GetWorld());
		if (!world)
			return null;
		if (!world.GetItemPreviewManager())
		{
			Resource resource = Resource.Load("{9F18C476AB860F3B}Prefabs/World/Game/ItemPreviewManager.et");
			if (resource && resource.IsValid())
				GetGame().SpawnEntityPrefabLocal(resource, world);
		}
		return world.GetItemPreviewManager();
	}

	int GetSelected() { return m_iSelected; }
	int GetVisibleCapacity() { return m_aCards.Count(); }

	void Select(int index, bool reveal = true)
	{
		m_iSelected = index;
		if (reveal && index >= 0 && (index < m_iFirst || index >= m_iFirst + m_aCards.Count()))
		{
			m_iFirst = Math.Floor(index / m_iColumns) * m_iColumns;
			Render();
		}
		RefreshColors();
	}

	void SetEnabled(bool enabled)
	{
		m_bEnabled = enabled;
		foreach (AICF_LoadoutItemCard card : m_aCards)
			card.m_Input.SetEnabled(enabled);
		UpdateScrollbar();
	}

	void Scroll(int rows)
	{
		if (!m_bEnabled)
			return;
		int maxFirst = LastPageOffset(m_aPrefabs.Count(), m_iColumns, m_aCards.Count());
		int first = Math.ClampInt(m_iFirst + rows * m_iColumns, 0, maxFirst);
		if (first == m_iFirst)
			return;
		m_iFirst = first;
		Render();
	}

	static int LastPageOffset(int itemCount, int columns, int visibleCount)
	{
		if (columns <= 0 || visibleCount < columns)
			return 0;
		return Math.Max(0, (Math.Ceil(itemCount / (float)columns) - visibleCount / columns) * columns);
	}

	protected void Render()
	{
		GetGame().GetCallqueue().Remove(Render);
		for (int i; i < m_aCards.Count(); i++)
		{
			AICF_LoadoutItemCard card = m_aCards[i];
			int index = m_iFirst + i;
			bool visible = index < m_aPrefabs.Count();
			ResourceName prefab;
			if (visible)
				prefab = m_aPrefabs[index];
			// Один и тот же prefab уже привязан к этому widget. Обновление
			// текста/selection не требует сброса native thumbnail cache.
			if (card.m_sBoundPrefab != prefab)
			{
				card.m_Preview.SetVisible(false);
				if (m_Manager)
				{
					m_Manager.SetPreviewItem(card.m_Preview, null);
					if (!prefab.IsEmpty())
						m_Manager.SetPreviewItemFromPrefab(card.m_Preview, prefab, null, false, 5000);
					card.m_sBoundPrefab = prefab;
				}
			}
			card.m_Preview.SetVisible(m_Manager && !prefab.IsEmpty());
			card.m_Root.SetVisible(visible);
			if (!visible)
				continue;
			card.m_Name.SetText(AICF_Localization.Resolve(m_aNames[index]));
			card.m_Detail.SetText(AICF_Localization.Resolve(m_aDetails[index]));
		}
		string range = "{AICF:AICF_UI_No_items_03a89de6}";
		if (!m_aPrefabs.IsEmpty())
			range = AICF_Localization.Format("{AICF:AICF_UI_of_10c4fe74}", string.Format("%1", m_iFirst + 1), string.Format("%1", Math.Min(m_iFirst + m_aCards.Count(), m_aPrefabs.Count())), string.Format("%1", m_aPrefabs.Count()));
		m_Range.SetText(AICF_Localization.Resolve(range));
		m_Empty.SetVisible(m_aPrefabs.IsEmpty());
		SetEnabled(m_bEnabled);
		RefreshColors();
	}

	void RefreshColors()
	{
		m_Controller.SetRectColor(m_Root, Color.FromSRGBA(12, 25, 35, 255));
		m_Controller.SetRectColor(m_ScrollTrack, Color.FromSRGBA(45, 61, 72, 255));
		m_Controller.SetRectColor(m_ScrollThumb, Color.FromSRGBA(226, 167, 79, 255));
		foreach (int i, AICF_LoadoutItemCard card : m_aCards)
		{
			bool selected = m_iFirst + i == m_iSelected;
			Color color = Color.FromSRGBA(23, 36, 46, 255);
			if (selected)
				color = Color.FromSRGBA(39, 57, 68, 255);
			m_Controller.SetRectColor(card.m_Root, color);
			m_Controller.SetRectColor(card.m_Accent, Color.FromSRGBA(226, 167, 79, 255));
			card.m_Accent.SetVisible(selected);
		}
	}

	// Отсоединяем widgets и отменяем отложенный bind. Общий UI manager не удаляем.
	void ReleasePreviews()
	{
		GetGame().GetCallqueue().Remove(Render);
		foreach (AICF_LoadoutItemCard card : m_aCards)
		{
			if (!card.m_Preview)
				continue;
			card.m_Preview.SetVisible(false);
			if (m_Manager)
				m_Manager.SetPreviewItem(card.m_Preview, null);
			card.m_sBoundPrefab = string.Empty;
		}
		m_Manager = null;
	}

	override bool OnClick(Widget w, int x, int y, int button)
	{
		if (!m_bEnabled || button != 0)
			return true;
		foreach (int i, AICF_LoadoutItemCard card : m_aCards)
		{
			if (w != card.m_Input)
				continue;
			int index = m_iFirst + i;
			if (index >= m_aPrefabs.Count())
				return true;
			Select(index);
			m_OnSelected.Invoke(this, index);
			return true;
		}
		return false;
	}

	override bool OnDoubleClick(Widget w, int x, int y, int button)
	{
		if (!m_bEnabled || button != SCR_EMouseButtons.LEFT)
			return true;
		foreach (int i, AICF_LoadoutItemCard card : m_aCards)
		{
			if (w != card.m_Input)
				continue;
			int index = m_iFirst + i;
			if (index >= m_aPrefabs.Count())
				return true;
			Select(index);
			m_OnSelected.Invoke(this, index);
			m_OnActivated.Invoke(this, index);
			return true;
		}
		return false;
	}

	// ButtonWidget OnClick предназначен для ЛКМ; ПКМ принимаем явно на отпускании.
	override bool OnMouseButtonUp(Widget w, int x, int y, int button)
	{
		if (button != SCR_EMouseButtons.RIGHT)
			return false;
		if (!m_bEnabled)
			return true;
		foreach (int i, AICF_LoadoutItemCard card : m_aCards)
		{
			if (w != card.m_Input)
				continue;
			int index = m_iFirst + i;
			if (index >= m_aPrefabs.Count())
				return true;
			Select(index);
			m_OnSelected.Invoke(this, index);
			m_OnRemoved.Invoke(this, index);
			return true;
		}
		return false;
	}

	override bool OnMouseWheel(Widget w, int x, int y, int wheel)
	{
		Scroll(-wheel);
		return true;
	}

	override bool OnChange(Widget w, bool finished)
	{
		if (w != m_Scrollbar) return false;
		if (m_bScrollUpdating || !m_bEnabled) return true;
		int last = LastPageOffset(m_aPrefabs.Count(), m_iColumns, m_aCards.Count());
		m_iFirst = Math.ClampInt(Math.Round(m_Scrollbar.GetCurrent() / m_iColumns) * m_iColumns, 0, last);
		Render();
		return true;
	}

	void Close()
	{
		ReleasePreviews();
		foreach (AICF_LoadoutItemCard card : m_aCards)
		{
			if (card.m_Input)
				card.m_Input.RemoveHandler(this);
		}
		m_aCards.Clear();
		if (m_Scrollbar) m_Scrollbar.RemoveHandler(this);
		if (m_Root)
		{
			m_Root.RemoveHandler(this);
			m_Root.RemoveFromHierarchy();
		}
		m_Root = null;
		m_Scrollbar = null;
		m_ScrollTrack = null;
		m_ScrollThumb = null;
	}
}
