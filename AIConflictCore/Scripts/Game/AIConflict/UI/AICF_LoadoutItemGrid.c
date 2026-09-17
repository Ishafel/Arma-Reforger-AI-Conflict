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
	protected Widget m_Previous;
	protected Widget m_Next;
	protected int m_iColumns;
	protected int m_iFirst;
	protected int m_iSelected = -1;
	protected bool m_bEnabled = true;
	ref ScriptInvoker m_OnSelected = new ScriptInvoker();
	ref ScriptInvoker m_OnActivated = new ScriptInvoker();

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
			float x = Math.Mod(i, columns) / (float)columns;
			float y = Math.Floor(i / columns) / rows * 0.91;
			card.m_Root = controller.CreateRect(m_Root, x + 0.008, y + 0.006, x + 1.0 / columns - 0.008, y + 0.91 / rows - 0.006, Color.FromSRGBA(27, 43, 54, 255), true);
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
		m_Previous = PageButton(0.01, 0.23, "Выше");
		m_Next = PageButton(0.77, 0.99, "Ниже");
		m_Range = controller.CreateText(m_Root, 0.24, 0.92, 0.76, 1, "", 14, Color.FromSRGBA(170, 194, 206, 255), true);
		m_Empty = controller.CreateText(m_Root, 0.06, 0.25, 0.94, 0.65, "Нет предметов", 17, Color.FromSRGBA(170, 194, 206, 255), true);
		m_Empty.SetTextWrapping(true);
		m_Empty.SetVisible(false);
		return true;
	}

	protected Widget PageButton(float l, float r, string caption)
	{
		Widget frame = m_Controller.CreateRect(m_Root, l, 0.925, r, 0.995, Color.FromSRGBA(34, 49, 59, 255), true);
		frame.SetZOrder(5);
		m_Controller.CreateText(frame, 0, 0, 1, 1, caption, 15, Color.FromSRGBA(230, 237, 242, 255), true);
		Widget input = frame.FindAnyWidget("AICF_RectInput");
		input.AddHandler(this);
		return input;
	}

	void SetEmptyText(string text) { m_Empty.SetText(text); }

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
		ReleasePreviews();
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
		bool overflow = m_aPrefabs.Count() > m_aCards.Count();
		m_Previous.GetParent().SetVisible(overflow);
		m_Next.GetParent().SetVisible(overflow);
		m_Previous.SetEnabled(enabled && m_iFirst > 0);
		m_Next.SetEnabled(enabled && m_iFirst + m_aCards.Count() < m_aPrefabs.Count());
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
			card.m_Preview.SetVisible(false);
			if (m_Manager)
				m_Manager.SetPreviewItem(card.m_Preview, null);
			int index = m_iFirst + i;
			bool visible = index < m_aPrefabs.Count();
			card.m_Root.SetVisible(visible);
			if (!visible)
				continue;
			card.m_Name.SetText(m_aNames[index]);
			card.m_Detail.SetText(m_aDetails[index]);
			if (m_Manager && !m_aPrefabs[index].IsEmpty())
			{
				card.m_Preview.SetVisible(true);
				m_Manager.SetPreviewItemFromPrefab(card.m_Preview, m_aPrefabs[index], null, false, 5000);
			}
		}
		string range = "Нет предметов";
		if (!m_aPrefabs.IsEmpty())
			range = string.Format("%1–%2 из %3", m_iFirst + 1, Math.Min(m_iFirst + m_aCards.Count(), m_aPrefabs.Count()), m_aPrefabs.Count());
		m_Range.SetText(range);
		m_Empty.SetVisible(m_aPrefabs.IsEmpty());
		SetEnabled(m_bEnabled);
		RefreshColors();
	}

	void RefreshColors()
	{
		m_Controller.SetRectColor(m_Root, Color.FromSRGBA(12, 25, 35, 255));
		if (m_Previous)
			m_Controller.SetRectColor(m_Previous.GetParent(), Color.FromSRGBA(34, 49, 59, 255));
		if (m_Next)
			m_Controller.SetRectColor(m_Next.GetParent(), Color.FromSRGBA(34, 49, 59, 255));
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
		}
		m_Manager = null;
	}

	override bool OnClick(Widget w, int x, int y, int button)
	{
		if (!m_bEnabled || button != 0)
			return true;
		if (w == m_Previous || w == m_Next)
		{
			int rows = m_aCards.Count() / m_iColumns;
			if (w == m_Previous)
				rows = -rows;
			Scroll(rows);
			return true;
		}
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

	override bool OnMouseWheel(Widget w, int x, int y, int wheel)
	{
		Scroll(-wheel);
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
		if (m_Previous)
			m_Previous.RemoveHandler(this);
		if (m_Next)
			m_Next.RemoveHandler(this);
		if (m_Root)
		{
			m_Root.RemoveHandler(this);
			m_Root.RemoveFromHierarchy();
		}
		m_Root = null;
	}
}
