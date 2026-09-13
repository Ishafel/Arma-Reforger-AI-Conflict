// Виджет создаётся внутри stock layout и уничтожается вместе с ним. Подписки
// и CallLater не нужны; hover не поглощает клики/перетаскивание карты.
class AICF_MapMarkerCardWidget
{
	protected static const float CARD_X = 16;
	protected static const float LABEL_PADDING_X = 8;
	protected static const float LABEL_PADDING_Y = 4;
	protected static const float DETAILS_PADDING = 8;
	protected static const float LABEL_MAX_TEXT_WIDTH = 240;
	protected static const float DETAILS_MAX_TEXT_WIDTH = 360;
	protected Widget m_Root;
	protected Widget m_LabelBackground;
	protected Widget m_LabelAccent;
	protected Widget m_DetailsBackground;
	protected TextWidget m_Label;
	protected TextWidget m_Details;
	protected float m_fLabelHeight = 32;
	protected int m_iOriginalZOrder;

	void Init(Widget root, Color color, string badge)
	{
		if (!root) return;
		m_Root = root;
		m_iOriginalZOrder = root.GetZOrder();
		root.ClearFlags(WidgetFlags.CLIPCHILDREN | WidgetFlags.IGNORE_CURSOR);
		Widget stockIcon = root.FindAnyWidget("MarkerIcon");
		Widget stockText = root.FindAnyWidget("MarkerText");
		if (stockIcon) stockIcon.SetVisible(false);
		if (stockText) stockText.SetVisible(false);
		// «О» — отряд, «Л» — логистика. Центр значка — живая позиция цели.
		Widget icon = Rectangle(root, color, 24, 24, -12, -12);
		if (icon) icon.ClearFlags(WidgetFlags.IGNORE_CURSOR);
		TextWidget letter = Text(root, 24, 24, -12, -12, 16);
		if (letter)
		{
			letter.SetFlags(WidgetFlags.CENTER | WidgetFlags.VCENTER);
			letter.SetText(badge);
		}
		m_LabelBackground = Rectangle(root, Color.FromSRGBA(5, 10, 14, 235), 1, 1, CARD_X, 0);
		if (m_LabelBackground) m_LabelBackground.ClearFlags(WidgetFlags.IGNORE_CURSOR);
		m_LabelAccent = Rectangle(root, color, 2, 1, CARD_X, 0);
		m_Label = Text(root, LABEL_MAX_TEXT_WIDTH, 1, CARD_X + LABEL_PADDING_X, 0, 13);
		// Панели соприкасаются: перевод курсора в подробности не закрывает hover.
		m_DetailsBackground = Rectangle(root, Color.FromSRGBA(5, 10, 14, 250), 1, 1, CARD_X, 0);
		if (m_DetailsBackground) m_DetailsBackground.ClearFlags(WidgetFlags.IGNORE_CURSOR);
		m_Details = Text(root, DETAILS_MAX_TEXT_WIDTH, 1, CARD_X + DETAILS_PADDING, 0, 15);
		ShowDetails(false);
	}

	protected Widget Rectangle(Widget parent, Color color, float width, float height, float x, float y)
	{
		Widget widget = GetGame().GetWorkspace().CreateWidget(WidgetType.ImageWidgetTypeID,
			WidgetFlags.VISIBLE | WidgetFlags.IGNORE_CURSOR | WidgetFlags.BLEND | WidgetFlags.STRETCH, color, 1, parent);
		Place(widget, width, height, x, y);
		return widget;
	}

	protected TextWidget Text(Widget parent, float width, float height, float x, float y, int size)
	{
		TextWidget text = TextWidget.Cast(GetGame().GetWorkspace().CreateWidget(WidgetType.TextWidgetTypeID,
			WidgetFlags.VISIBLE | WidgetFlags.IGNORE_CURSOR | WidgetFlags.BLEND | WidgetFlags.NO_LOCALIZATION,
			Color.White, 2, parent));
		if (!text) return null;
		text.SetFont(AICF_GroupMapMarkerEntry.ATTACK_BADGE_FONT);
		text.SetExactFontSize(size);
		text.ClearFlags(WidgetFlags.CENTER | WidgetFlags.VCENTER | WidgetFlags.RALIGN);
		text.SetTextOffset(0, 0);
		Place(text, width, height, x, y);
		return text;
	}

	protected void Place(Widget widget, float width, float height, float x, float y)
	{
		if (!widget) return;
		FrameSlot.SetAnchor(widget, 0.5, 0.5);
		FrameSlot.SetAlignment(widget, 0, 0);
		FrameSlot.SetSize(widget, width, height);
		FrameSlot.SetPos(widget, x, y);
	}

	void SetLabel(string label)
	{
		if (!m_Label) return;
		m_Label.SetText(label);
		LayoutLabel();
	}

	void SetDetails(string details)
	{
		if (!m_Details) return;
		m_Details.SetText(details);
		LayoutDetails();
	}

	// GetTextSize возвращает размеры в reference resolution, как FrameSlot.
	// Сначала измеряем строки без переноса, затем высоту при ограниченной ширине.
	protected void MeasureText(TextWidget text, float maxWidth, out float width, out float height)
	{
		text.SetTextWrapping(false);
		text.GetTextSize(width, height);
		width = Math.Clamp(Math.Ceil(width), 1, maxWidth);
		FrameSlot.SetSize(text, width, Math.Max(1, Math.Ceil(height)));
		text.SetTextWrapping(true);
		float wrappedWidth;
		text.GetTextSize(wrappedWidth, height);
		height = Math.Max(1, Math.Ceil(height));
	}

	protected void LayoutLabel()
	{
		if (!m_Label) return;
		float width, height;
		MeasureText(m_Label, LABEL_MAX_TEXT_WIDTH, width, height);
		m_fLabelHeight = height + 2 * LABEL_PADDING_Y;
		float top = -m_fLabelHeight * 0.5;
		Place(m_LabelBackground, width + 2 * LABEL_PADDING_X, m_fLabelHeight, CARD_X, top);
		Place(m_LabelAccent, 2, m_fLabelHeight, CARD_X, top);
		Place(m_Label, width, height, CARD_X + LABEL_PADDING_X, top + LABEL_PADDING_Y);
		LayoutDetails();
	}

	protected void LayoutDetails()
	{
		if (!m_Details) return;
		float width, height;
		MeasureText(m_Details, DETAILS_MAX_TEXT_WIDTH, width, height);
		float top = m_fLabelHeight * 0.5;
		Place(m_DetailsBackground, width + 2 * DETAILS_PADDING, height + 2 * DETAILS_PADDING, CARD_X, top);
		Place(m_Details, width, height, CARD_X + DETAILS_PADDING, top + DETAILS_PADDING);
	}

	void ShowDetails(bool show)
	{
		if (m_DetailsBackground) m_DetailsBackground.SetVisible(show);
		if (m_Details) m_Details.SetVisible(show);
		if (show) LayoutLabel();
		if (!m_Root) return;
		if (show) m_Root.SetZOrder(100);
		else m_Root.SetZOrder(m_iOriginalZOrder);
	}

	bool Contains(Widget widget)
	{
		while (widget)
		{
			if (widget == m_Root) return true;
			widget = widget.GetParent();
		}
		return false;
	}
}
