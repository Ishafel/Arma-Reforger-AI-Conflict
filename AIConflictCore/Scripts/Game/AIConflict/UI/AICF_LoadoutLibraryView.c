// Предпросмотр библиотеки владеет отдельными draft/world и не меняет editor.
class AICF_LoadoutLibraryView : ScriptedWidgetEventHandler
{
	protected const int ROWS = 9;
	protected AICF_StrategicUIController m_Controller;
	protected RenderTargetWidget m_wPreview;
	protected Widget m_wSummary;
	protected SliderWidget m_wScroll;
	protected Widget m_wThumb;
	protected ref array<TextWidget> m_aRows = {};
	protected ref array<string> m_aLines = {};
	protected ref AICF_LoadoutDraft m_Draft = new AICF_LoadoutDraft();
	protected ref AICF_LoadoutPreview m_Preview = new AICF_LoadoutPreview();
	protected int m_iFirst;
	protected bool m_bUpdating;

	bool Create(AICF_StrategicUIController controller, Widget parent)
	{
		m_Controller = controller;
		m_wPreview = RenderTargetWidget.Cast(GetGame().GetWorkspace().CreateWidget(WidgetType.RenderTargetWidgetTypeID,
			WidgetFlags.BLEND, Color.White, 10, parent));
		if (!m_wPreview) return false;
		FrameSlot.SetAnchorMin(m_wPreview, 0, 0.15);
		FrameSlot.SetAnchorMax(m_wPreview, 0.40, 0.84);
		FrameSlot.SetOffsets(m_wPreview, 0, 0, 0, 0);
		m_wPreview.SetMaxFPS(30);
		m_wPreview.SetResolutionScale(1, 1);
		m_wPreview.SetClearColor(true, Color.BLACK);
		m_wPreview.AddHandler(this);
		m_wSummary = controller.CreateRect(parent, 0.43, 0.15, 1, 0.84, Color.FromSRGBA(12, 25, 35, 255), true);
		if (!m_wSummary) return false;
		m_wSummary.SetZOrder(10);
		Widget input = m_wSummary.FindAnyWidget("AICF_RectInput");
		if (input) input.AddHandler(this);
		for (int i; i < ROWS; i++)
		{
			TextWidget row = controller.CreateText(m_wSummary, 0.025, i / (float)ROWS, 0.90, (i + 1) / (float)ROWS,
				"", 17, Color.FromSRGBA(230, 237, 242, 255));
			if (!row) return false;
			row.SetTextWrapping(true);
			m_aRows.Insert(row);
		}
		controller.CreateRect(m_wSummary, 0.95, 0.01, 0.97, 0.99, Color.FromSRGBA(45, 61, 72, 255), false);
		m_wThumb = controller.CreateRect(m_wSummary, 0.93, 0.01, 0.99, 0.99, Color.FromSRGBA(226, 167, 79, 255), false);
		m_wThumb.SetZOrder(6);
		m_wScroll = SliderWidget.Cast(GetGame().GetWorkspace().CreateWidget(WidgetType.SliderWidgetTypeID,
			WidgetFlags.VISIBLE | WidgetFlags.BLEND, Color.White, 20, m_wSummary));
		if (!m_wScroll) return false;
		FrameSlot.SetAnchorMin(m_wScroll, 0.90, 0);
		FrameSlot.SetAnchorMax(m_wScroll, 1, 1);
		FrameSlot.SetOffsets(m_wScroll, 0, 0, 0, 0);
		m_wScroll.SetOpacity(0);
		m_wScroll.SetVertical(true);
		m_wScroll.SetFlipped(true);
		m_wScroll.SetStep(1);
		m_wScroll.AddHandler(this);
		Message("{AICF:AICF_UI_LibrarySelectPreview}");
		return true;
	}

	void Clear()
	{
		if (m_wPreview)
		{
			m_wPreview.SetVisible(false);
			m_wPreview.SetWorld(null, 0);
		}
		m_Preview.Clear();
		m_Draft.Clear();
	}

	void Message(string message)
	{
		Clear();
		m_aLines.Clear();
		m_aLines.Insert(AICF_Localization.Resolve(message));
		m_iFirst = 0;
		Render();
	}

	void Close()
	{
		Clear();
		if (m_wPreview) m_wPreview.RemoveHandler(this);
		if (m_wScroll) m_wScroll.RemoveHandler(this);
		if (m_wSummary)
		{
			Widget input = m_wSummary.FindAnyWidget("AICF_RectInput");
			if (input) input.RemoveHandler(this);
		}
	}

	bool Show(AICF_LoadoutRecipe recipe, AICF_LoadoutCatalog catalog)
	{
		Clear();
		string reason;
		if (!recipe || !m_Draft.Build(recipe, catalog, reason) || !m_Preview.Build(m_Draft.GetCharacter()))
		{
			Message("{AICF:AICF_UI_Unable_to_build_the_preview_Reopen_the_edi_37bfc2dd}");
			return false;
		}
		AICF_LoadoutSummary.Build(m_Draft.GetCharacter(), catalog, m_aLines);
		m_iFirst = 0;
		m_wPreview.SetWorld(m_Preview.GetWorld(), 0);
		m_wPreview.SetVisible(true);
		Render();
		return true;
	}

	protected void Render()
	{
		int last = Math.Max(0, m_aLines.Count() - ROWS);
		m_iFirst = Math.ClampInt(m_iFirst, 0, last);
		foreach (int i, TextWidget row : m_aRows)
		{
			string text;
			if (m_iFirst + i < m_aLines.Count()) text = m_aLines[m_iFirst + i];
			row.SetText(text);
		}
		m_bUpdating = true;
		m_wScroll.SetRange(0, Math.Max(1, last));
		m_wScroll.SetCurrent(m_iFirst);
		m_bUpdating = false;
		m_wScroll.SetEnabled(last > 0);
		m_wThumb.SetVisible(last > 0);
		float fill = Math.Min(1, ROWS / (float)Math.Max(1, m_aLines.Count()));
		float top = 0.01;
		if (last > 0) top += 0.98 * (1 - fill) * m_iFirst / last;
		FrameSlot.SetAnchorMin(m_wThumb, 0.93, top);
		FrameSlot.SetAnchorMax(m_wThumb, 0.99, top + 0.98 * fill);
	}

	override bool OnChange(Widget w, bool finished)
	{
		if (w != m_wScroll || m_bUpdating) return false;
		m_iFirst = Math.Round(m_wScroll.GetCurrent());
		Render();
		return true;
	}

	override bool OnMouseWheel(Widget w, int x, int y, int wheel)
	{
		if (w == m_wPreview) m_Preview.Zoom(-wheel * 3);
		else { m_iFirst -= wheel; Render(); }
		return true;
	}
}
