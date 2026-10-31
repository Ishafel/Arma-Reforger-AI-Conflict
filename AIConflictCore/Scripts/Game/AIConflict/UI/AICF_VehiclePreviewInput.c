// То же управление, что у character inspector: ЛКМ — поворот,
// СКМ — сдвиг по высоте, колесо — приближение. Только private preview camera.
class AICF_VehiclePreviewInput : ScriptedWidgetEventHandler
{
	protected RenderTargetWidget m_wTarget;
	protected AICF_VehiclePreview m_Preview;
	protected bool m_bRotating;
	protected bool m_bPanning;
	protected int m_iMouseX;
	protected int m_iMouseY;
	protected int m_iGeneration;
	protected ScriptCallQueue m_DragQueue;
	protected bool m_bDragScheduled;

	void Attach(RenderTargetWidget widget, AICF_VehiclePreview preview)
	{
		if (m_wTarget == widget && m_Preview == preview) return;
		Detach();
		if (!widget || !preview) return;
		m_wTarget = widget;
		m_Preview = preview;
		widget.AddHandler(this);
	}

	protected bool Ready()
	{
		return GetGame() && m_wTarget && m_Preview &&
			m_wTarget.IsVisibleInHierarchy() && m_wTarget.IsEnabledInHierarchy() && m_Preview.IsReady();
	}

	protected void StopDrag()
	{
		m_bRotating = false;
		m_bPanning = false;
		m_iGeneration++;
		if (!m_bDragScheduled) { m_DragQueue = null; return; }
		// Сначала отзываем собственный timer; повторный Detach/dtor не строит
		// bound reference на уже уничтожаемый handler и не трогает чужую queue.
		m_bDragScheduled = false;
		ScriptCallQueue queue = m_DragQueue;
		m_DragQueue = null;
		if (queue) queue.RemoveByName(this, "UpdateDrag");
	}

	void Detach()
	{
		if (!m_wTarget && !m_Preview && !m_bDragScheduled) return;
		RenderTargetWidget widget = m_wTarget;
		m_wTarget = null;
		m_Preview = null;
		StopDrag();
		if (widget) widget.RemoveHandler(this);
	}

	override void HandlerDeattached(Widget w)
	{
		if (w != m_wTarget) return;
		m_wTarget = null;
		m_Preview = null;
		StopDrag();
	}

	override bool OnMouseButtonDown(Widget w, int x, int y, int button)
	{
		if (w != m_wTarget || !Ready()) return false;
		if (button != SCR_EMouseButtons.LEFT && button != SCR_EMouseButtons.MIDDLE) return false;
		StopDrag();
		int generation = m_iGeneration;
		if (!Ready()) return false;
		ScriptCallQueue queue = GetGame().GetCallqueue();
		if (!queue || generation != m_iGeneration || !m_wTarget || !m_Preview) return false;
		m_bRotating = button == SCR_EMouseButtons.LEFT;
		m_bPanning = button == SCR_EMouseButtons.MIDDLE;
		m_iMouseX = x;
		m_iMouseY = y;
		m_DragQueue = queue;
		m_bDragScheduled = true;
		queue.CallLaterByName(this, "UpdateDrag", 33, true, m_iGeneration);
		return true;
	}

	override bool OnMouseButtonUp(Widget w, int x, int y, int button)
	{
		if (w != m_wTarget) return false;
		bool handled = (button == SCR_EMouseButtons.LEFT && m_bRotating) ||
			(button == SCR_EMouseButtons.MIDDLE && m_bPanning);
		if (handled) StopDrag();
		return handled;
	}

	override bool OnMouseLeave(Widget w, Widget enterW, int x, int y)
	{
		if (w != m_wTarget) return false;
		StopDrag();
		return false;
	}

	override bool OnHide(Widget w)
	{
		if (w == m_wTarget) StopDrag();
		return false;
	}

	override bool OnDisable(Widget w)
	{
		if (w == m_wTarget) StopDrag();
		return false;
	}

	// ByName callbacks требуют public метод; identity остаётся закрыта generation gate.
	void UpdateDrag(int generation)
	{
		// Устаревший callback не снимает timer нового drag/переподключения.
		if (generation != m_iGeneration) return;
		if ((!m_bRotating && !m_bPanning) || !Ready()) { StopDrag(); return; }
		int x, y;
		WidgetManager.GetMousePos(x, y);
		if (generation != m_iGeneration || !m_Preview) return;
		if (m_bRotating && x != m_iMouseX) m_Preview.Rotate((x - m_iMouseX) * 0.5);
		if (m_bPanning && y != m_iMouseY)
		{
			float width, height;
			m_wTarget.GetScreenSize(width, height);
			if (generation != m_iGeneration || !m_Preview) return;
			if (height > 0) m_Preview.Pan((y - m_iMouseY) / height);
		}
		if (generation != m_iGeneration) return;
		m_iMouseX = x;
		m_iMouseY = y;
	}

	override bool OnMouseWheel(Widget w, int x, int y, int wheel)
	{
		if (w != m_wTarget || !Ready()) return false;
		m_Preview.Zoom(-wheel * 3);
		return true;
	}

	void ~AICF_VehiclePreviewInput()
	{
		if (m_wTarget || m_Preview || m_bDragScheduled) Detach();
	}
}
