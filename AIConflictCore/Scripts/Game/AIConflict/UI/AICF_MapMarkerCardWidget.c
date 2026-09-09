// Виджет создаётся внутри stock layout и уничтожается вместе с ним. Подписки
// и CallLater не нужны; hover не поглощает клики/перетаскивание карты.
class AICF_MapMarkerCardWidget
{
	protected Widget m_Root;
	protected Widget m_DetailsBackground;
	protected TextWidget m_Label;
	protected TextWidget m_Details;
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
		Widget labelBackground = Rectangle(root, Color.FromSRGBA(5, 10, 14, 235), 250, 44, 16, -22);
		if (labelBackground) labelBackground.ClearFlags(WidgetFlags.IGNORE_CURSOR);
		Rectangle(root, color, 2, 44, 16, -22);
		m_Label = Text(root, 232, 36, 26, -18, 13);
		// Панели соприкасаются: перевод курсора в подробности не закрывает hover.
		m_DetailsBackground = Rectangle(root, Color.FromSRGBA(5, 10, 14, 250), 500, 184, 16, 22);
		if (m_DetailsBackground) m_DetailsBackground.ClearFlags(WidgetFlags.IGNORE_CURSOR);
		m_Details = Text(root, 476, 164, 28, 32, 15);
		ShowDetails(false);
	}

	protected Widget Rectangle(Widget parent, Color color, int width, int height, int x, int y)
	{
		Widget widget = GetGame().GetWorkspace().CreateWidget(WidgetType.ImageWidgetTypeID,
			WidgetFlags.VISIBLE | WidgetFlags.IGNORE_CURSOR | WidgetFlags.BLEND | WidgetFlags.STRETCH, color, 1, parent);
		Place(widget, width, height, x, y);
		return widget;
	}

	protected TextWidget Text(Widget parent, int width, int height, int x, int y, int size)
	{
		TextWidget text = TextWidget.Cast(GetGame().GetWorkspace().CreateWidget(WidgetType.TextWidgetTypeID,
			WidgetFlags.VISIBLE | WidgetFlags.IGNORE_CURSOR | WidgetFlags.BLEND | WidgetFlags.NO_LOCALIZATION,
			Color.White, 2, parent));
		if (!text) return null;
		text.SetFont(AICF_GroupMapMarkerEntry.ATTACK_BADGE_FONT);
		text.SetExactFontSize(size);
		Place(text, width, height, x, y);
		return text;
	}

	protected void Place(Widget widget, int width, int height, int x, int y)
	{
		if (!widget) return;
		FrameSlot.SetAnchor(widget, 0.5, 0.5);
		FrameSlot.SetAlignment(widget, 0, 0);
		FrameSlot.SetSize(widget, width, height);
		FrameSlot.SetPos(widget, x, y);
	}

	void SetLabel(string label)
	{
		if (m_Label) m_Label.SetText(label);
	}

	void SetDetails(string details)
	{
		if (m_Details) m_Details.SetText(details);
	}

	void ShowDetails(bool show)
	{
		if (m_DetailsBackground) m_DetailsBackground.SetVisible(show);
		if (m_Details) m_Details.SetVisible(show);
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
