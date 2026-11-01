// Opt-in standalone Frontend geometry. Offscreen transparent widgets only;
// no campaign, RPC, actors, focus, simulated physical input or screenshots.
modded class SCR_ComboBoxComponent
{
	TextWidget AICF_SeatLayoutContentText() { return m_wText; }
}

class AICF_VehicleSeatLayoutRect
{
	float m_X;
	float m_Y;
	float m_W;
	float m_H;

	bool Read(Widget widget)
	{
		if (!widget) return false;
		widget.GetScreenPos(m_X, m_Y);
		widget.GetScreenSize(m_W, m_H);
		return Finite(m_X) && Finite(m_Y) && Finite(m_W) && Finite(m_H);
	}
	static bool Finite(float value) { return value == value && Math.AbsFloat(value) < float.MAX; }
	bool Positive() { return m_W > 0 && m_H > 0; }
	string Text() { return string.Format("[%1,%2,%3,%4]", m_X, m_Y, m_W, m_H); }
	bool Inside(AICF_VehicleSeatLayoutRect parent)
	{
		if (!parent || !Positive() || !parent.Positive()) return false;
		return m_X >= parent.m_X - 1.5 && m_Y >= parent.m_Y - 1.5 &&
			m_X + m_W <= parent.m_X + parent.m_W + 1.5 && m_Y + m_H <= parent.m_Y + parent.m_H + 1.5;
	}
}

class AICF_VehicleSeatLayoutUIModel : AICF_StrategicUIController
{
	override bool GetLoadoutGroup(int slotId, out string name, out int size, string targetFaction = "")
	{
		name = "Standalone layout fixture";
		size = 10;
		return slotId == 0;
	}
}

class AICF_VehicleSeatLayoutEditor : AICF_VehiclePresetEditor
{
	void AICF_VehicleSeatLayoutEditor(AICF_StrategicUIController controller) { m_Controller = controller; }
	bool Build(Widget host) { return host && Create(host, 0); }

	void Seed(int count)
	{
		// These are presentation rows, not claimed native compartments.
		m_Draft = new AICF_VehiclePreset();
		m_Draft.m_sProfile = "GEOMETRY_FIXTURE";
		m_Draft.m_sFaction = "USSR";
		m_Draft.m_sName = "Geometry only";
		m_aSeatNames.Clear();
		m_aActiveMembers.Clear();
		for (int member; member < 10; member++) m_aActiveMembers.Insert(member);
		for (int seat; seat < count; seat++)
		{
			m_Draft.m_aSeats.Insert(string.Format("0:%1:%2", seat, ECompartmentType.CARGO));
			m_Draft.m_aMembers.Insert(-1);
			m_aSeatNames.Insert("Место экипажа " + (seat + 1).ToString());
		}
		RenderSeats();
		UpdateLayout();
	}

	void UpdateLayout()
	{
		if (m_wRoot) m_wRoot.Update();
		if (m_wPanel) m_wPanel.Update();
		if (m_wSeatScroll) m_wSeatScroll.Update();
		if (m_wSeatSize) m_wSeatSize.Update();
		if (m_wSeatContent) m_wSeatContent.Update();
	}

	void Measure(AICF_VehicleSeatLayoutProbe probe, int caseIndex, int count)
	{
		UpdateLayout();
		AICF_VehicleSeatLayoutRect viewport = new AICF_VehicleSeatLayoutRect();
		AICF_VehicleSeatLayoutRect content = new AICF_VehicleSeatLayoutRect();
		bool viewportRead = viewport.Read(m_wSeatScroll);
		bool contentRead = content.Read(m_wSeatContent);
		probe.Check(caseIndex, -1, "VIEWPORT_POSITIVE", viewportRead && viewport.Positive(), viewport.Text());
		probe.Check(caseIndex, -1, "CONTENT_POSITIVE", contentRead && content.Positive(), content.Text());
		probe.Check(caseIndex, -1, "ROW_COUNTS", m_aSeats.Count() == count && m_aSeatLabels.Count() == count, string.Format("combos=%1 labels=%2 expected=%3", m_aSeats.Count(), m_aSeatLabels.Count(), count));
		probe.Check(caseIndex, -1, "CONTENT_OVERRIDE", m_wSeatSize && m_wSeatSize.IsHeightOverride() && m_wSeatSize.GetHeightOverride() == count * 58, "workspace_pitch=58");
		AICF_VehicleSeatLayoutRect previous;
		float firstScale;
		for (int index; index < count; index++)
		{
			if (index >= m_aSeats.Count() || index >= m_aSeatLabels.Count())
			{
				probe.Check(caseIndex, index, "ROW_MISSING", false, "bounded_missing_index");
				continue;
			}
			SCR_ComboBoxComponent component = m_aSeats[index];
			Widget combo;
			if (component) combo = component.GetRootWidget();
			Widget row;
			if (combo) row = combo.GetParent();
			TextWidget label = m_aSeatLabels[index];
			bool ownRow = row && row != m_wSeatContent && row.GetParent() == m_wSeatContent;
			bool hierarchy = ownRow && label && label.GetParent() == row;
			probe.Check(caseIndex, index, "ROW_HIERARCHY", hierarchy, "inferred_from_actual_combo_parent=1");
			if (!row || !combo || !label)
			{
				probe.Check(caseIndex, index, "ROW_WIDGETS", false, "native_widget_missing");
				continue;
			}
			AICF_VehicleSeatLayoutRect rowRect = new AICF_VehicleSeatLayoutRect();
			AICF_VehicleSeatLayoutRect labelRect = new AICF_VehicleSeatLayoutRect();
			AICF_VehicleSeatLayoutRect comboRect = new AICF_VehicleSeatLayoutRect();
			bool readRow = rowRect.Read(row);
			bool readLabel = labelRect.Read(label);
			bool readCombo = comboRect.Read(combo);
			probe.Check(caseIndex, index, "ROW_POSITIVE", readRow && readLabel && readCombo && rowRect.Positive() && labelRect.Positive() && comboRect.Positive(), "all_rects_screen_pixels=1");
			float slotY = -1;
			float slotHeight = -1;
			// Old baseline has content itself here, whose parent is SizeLayout.
			// Never call FrameSlot against that wrong native slot type.
			if (ownRow)
			{
				slotY = FrameSlot.GetPosY(row);
				slotHeight = FrameSlot.GetSizeY(row);
			}
			probe.Check(caseIndex, index, "ROW_WORKSPACE_SLOT", ownRow && Math.AbsFloat(slotY - index * 58) <= 0.01 && Math.AbsFloat(slotHeight - 52) <= 0.01, string.Format("slot_y=%1 slot_height=%2", slotY, slotHeight));
			probe.Check(caseIndex, index, "ROW_IN_CONTENT", readRow && contentRead && rowRect.Inside(content), "same_screen_pixel_space=1");
			probe.Check(caseIndex, index, "CHILDREN_IN_ROW", hierarchy && readRow && readLabel && readCombo && labelRect.Inside(rowRect) && comboRect.Inside(rowRect), "positive_size_and_bounds=1");
			TextWidget comboText = component.AICF_SeatLayoutContentText();
			if (comboText) comboText.Update();
			AICF_VehicleSeatLayoutRect textRect = new AICF_VehicleSeatLayoutRect();
			bool textRead = textRect.Read(comboText);
			float glyphWidth, glyphHeight;
			bool textPresent;
			if (comboText)
			{
				comboText.GetTextSize(glyphWidth, glyphHeight);
				textPresent = !comboText.GetText().IsEmpty();
			}
			// GetTextSize returns reference-resolution pixels. Convert via the
			// documented workspace API before comparing against screen rects.
			float glyphPixels = GetGame().GetWorkspace().DPIScale(glyphHeight);
			bool finiteGlyph = AICF_VehicleSeatLayoutRect.Finite(glyphWidth) && AICF_VehicleSeatLayoutRect.Finite(glyphHeight) && AICF_VehicleSeatLayoutRect.Finite(glyphPixels);
			bool glyphAvailable = finiteGlyph && glyphWidth > 0 && glyphHeight > 0;
			bool nativeZero = finiteGlyph && glyphWidth == 0 && glyphHeight == 0;
			bool glyphFits = glyphAvailable && glyphPixels <= rowRect.m_H + 1.5;
			probe.ObserveGlyph(glyphAvailable, nativeZero);
			string glyphStatus = "INVALID_OR_EXCEEDS_ROW";
			if (nativeZero) glyphStatus = "NOT_PROVEN_NATIVE_ZERO";
			else if (glyphFits) glyphStatus = "MEASURED_WITHIN_ROW";
			// Zero offscreen render metrics are unknown glyph coverage, never a
			// glyph/font PASS. Nonempty positive actual text bbox remains strict.
			bool textGeometry = textRead && textPresent && textRect.Inside(rowRect);
			probe.Check(caseIndex, index, "COMBO_TEXT_IN_ROW", textGeometry && (nativeZero || glyphFits), "text_bbox_strict=1 glyph_status=" + glyphStatus);
			bool rootIdentity = combo.FindHandler(SCR_ComboBoxComponent) == component;
			probe.Check(caseIndex, index, "COMBO_ROOT_IDENTITY", rootIdentity && combo.GetParent() == row, "handler_on_measured_root=1");
			bool ordered = rowRect.Positive();
			float scale = rowRect.m_H / 52;
			if (!index)
			{
				firstScale = scale;
				ordered = ordered && Math.AbsFloat(rowRect.m_Y - content.m_Y) <= 1.5;
			}
			else
			{
				ordered = ordered && previous && previous.Positive();
				if (previous)
				{
					ordered = ordered && rowRect.m_Y >= previous.m_Y + previous.m_H - 1.5;
					ordered = ordered && Math.AbsFloat(rowRect.m_Y - previous.m_Y - 58 * firstScale) <= 1.5;
				}
			}
			probe.Check(caseIndex, index, "ROW_ORDER_AND_GAP", ordered && Math.AbsFloat(scale - firstScale) <= 0.02, "pitch58_height52_measured_scale=1");
			string fields = string.Format("[AICF][VEHICLE_SEAT_LAYOUT_ROW] case=%1 row=%2 row_rect=%3 label_rect=%4 combo_rect=%5", caseIndex, index, rowRect.Text(), labelRect.Text(), comboRect.Text());
			fields += string.Format(" slot_y=%1 slot_height=%2 screen_scale=%3 own_row=%4 source=NATIVE_GET_SCREEN geometry_only=1", slotY, slotHeight, scale, ownRow);
			fields += string.Format(" text_rect=%1 glyph_reference_width=%2 glyph_reference_height=%3 glyph_screen_height=%4 text_present=%5", textRect.Text(), glyphWidth, glyphHeight, glyphPixels, textPresent);
			fields += " glyph_status=" + glyphStatus;
			Print(fields);
			previous = rowRect;
		}
		probe.Check(caseIndex, -1, "CONTENT_SCREEN_HEIGHT", firstScale > 0 && content.Positive() && Math.AbsFloat(content.m_H - count * 58 * firstScale) <= 2, "workspace_height_times_measured_row_scale=1");
		// Normalized slider endpoints are the primary API oracle. Pixel max
		// is observed independently; it is not inferred from workspace height.
		m_wSeatScroll.SetSliderPos(0, 1);
		float normX, bottom;
		float pixelX, bottomPixels;
		m_wSeatScroll.GetSliderPos(normX, bottom);
		m_wSeatScroll.GetSliderPosPixels(pixelX, bottomPixels);
		m_wSeatScroll.SetSliderPos(0, 1);
		float repeated, repeatedPixels;
		m_wSeatScroll.GetSliderPos(normX, repeated);
		m_wSeatScroll.GetSliderPosPixels(pixelX, repeatedPixels);
		m_wSeatScroll.SetSliderPos(0, 0);
		float top, topPixels;
		m_wSeatScroll.GetSliderPos(normX, top);
		m_wSeatScroll.GetSliderPosPixels(pixelX, topPixels);
		bool overflow = content.Positive() && viewport.Positive() && content.m_H > viewport.m_H + 1.5;
		bool normalized = bottom >= 0 && bottom <= 1 && AICF_VehicleSeatLayoutRect.Finite(bottomPixels) && bottomPixels >= 0;
		if (overflow) normalized = normalized && bottom == 1 && bottomPixels > 0;
		probe.Check(caseIndex, -1, "SCROLL_NORMALIZED_ENDPOINT", normalized, string.Format("bottom=%1 overflow_required=%2", bottom, overflow));
		probe.Check(caseIndex, -1, "SCROLL_ENDPOINT_REPEAT", repeated == bottom && Math.AbsFloat(bottomPixels - repeatedPixels) <= 0.01, "native_pixel_endpoint_not_workspace_conversion=1");
		probe.Check(caseIndex, -1, "SCROLL_BACK_TO_ZERO", top == 0 && Math.AbsFloat(topPixels) <= 0.01, string.Format("normalized_top=%1 top_pixels=%2", top, topPixels));
		Print(string.Format("[AICF][VEHICLE_SEAT_LAYOUT_SCROLL] case=%1 overflow=%2 normalized_bottom=%3 normalized_repeat=%4 normalized_top=%5 pixel_bottom=%6 pixel_repeat=%7 pixel_top=%8", caseIndex, overflow, bottom, repeated, top, bottomPixels, repeatedPixels, topPixels));
	}
}

class AICF_VehicleSeatLayoutProbe
{
	protected ScriptCallQueue m_Queue;
	protected bool m_bScheduled;
	protected bool m_bStarted;
	protected bool m_bStopped;
	protected int m_iStartedAt;
	protected int m_iCase;
	protected int m_iWaitTicks;
	protected int m_iPhase;
	protected int m_iChecks;
	protected int m_iFailures;
	protected int m_iCasesMeasured;
	protected int m_iGlyphAvailable;
	protected int m_iGlyphUnproven;
	protected Widget m_wHost;
	protected ref AICF_VehicleSeatLayoutUIModel m_Model;
	protected ref AICF_VehicleSeatLayoutEditor m_Editor;

	void Start()
	{
		if (m_bStarted || m_bStopped || !GetGame()) return;
		m_bStarted = true;
		m_iStartedAt = System.GetTickCount();
		m_Queue = GetGame().GetCallqueue();
		if (!m_Queue) { Finish("NO_CALLQUEUE"); return; }
		m_bScheduled = true;
		m_Queue.CallLaterByName(this, "AICF_Tick", 100, true);
		Print("[AICF][VEHICLE_SEAT_LAYOUT_START] cases=9 deadline_ms=30000 interval_ms=100 settle_ticks=2 frontend_only=1 synthetic_rows=1 human_visual_proof=0 actors_created=0 rpc=0");
	}

	void Check(int caseIndex, int row, string rule, bool passed, string fields)
	{
		if (m_bStopped) return;
		m_iChecks++;
		if (!passed) m_iFailures++;
		Print(string.Format("[AICF][VEHICLE_SEAT_LAYOUT_CHECK] case=%1 row=%2 rule=%3 passed=%4 %5", caseIndex, row, rule, passed, fields));
	}

	void ObserveGlyph(bool available, bool nativeZero)
	{
		if (m_bStopped) return;
		if (available) m_iGlyphAvailable++;
		if (nativeZero) m_iGlyphUnproven++;
	}

	// ByName callback must be PUBLIC: native ScriptCallback does not invoke
	// protected methods. Stop removes only this object's named callback.
	void AICF_Tick()
	{
		if (m_bStopped || !GetGame()) return;
		if (System.GetTickCount() - m_iStartedAt >= 30000) { Finish("BOUNDED_TIMEOUT"); return; }
		if (!GetGame().GetWorkspace()) return;
		if (!m_wHost)
		{
			m_wHost = GetGame().GetWorkspace().CreateWidget(WidgetType.FrameWidgetTypeID, WidgetFlags.VISIBLE | WidgetFlags.IGNORE_CURSOR, Color.White, 0);
			if (!m_wHost) { Finish("HOST_CREATE_FAILED"); return; }
			FrameSlot.SetAnchor(m_wHost, 0, 0);
			FrameSlot.SetAlignment(m_wHost, 0, 0);
			FrameSlot.SetSizeToContent(m_wHost, false);
			FrameSlot.SetPos(m_wHost, -30000, -30000);
			FrameSlot.SetSize(m_wHost, 1280, 720);
			m_wHost.SetOpacity(0);
			m_Model = new AICF_VehicleSeatLayoutUIModel();
			m_Editor = new AICF_VehicleSeatLayoutEditor(m_Model);
			if (!m_Editor.Build(m_wHost)) { Finish("EDITOR_CREATE_FAILED"); return; }
		}
		if (m_iWaitTicks > 0)
		{
			m_wHost.Update();
			m_Editor.UpdateLayout();
			m_iWaitTicks--;
			return;
		}
		array<int> counts = {5, 24, 64};
		array<int> widths = {1280, 1920, 2560};
		array<int> heights = {720, 1080, 1440};
		int resolution = m_iCase / 3;
		int count = counts[m_iCase % 3];
		if (m_iPhase == 0)
		{
			FrameSlot.SetSize(m_wHost, widths[resolution], heights[resolution]);
			m_wHost.Update();
			m_Editor.Seed(count);
			m_iWaitTicks = 2;
			m_iPhase = 1;
			Print(string.Format("[AICF][VEHICLE_SEAT_LAYOUT_CASE] case=%1 rows=%2 host_workspace_width=%3 host_workspace_height=%4 offscreen=1 opacity=0 visible_layout_override=1 display_resolution_changed=0", m_iCase, count, widths[resolution], heights[resolution]));
			return;
		}
		AICF_VehicleSeatLayoutRect host = new AICF_VehicleSeatLayoutRect();
		bool hostRead = host.Read(m_wHost);
		Check(m_iCase, -1, "HOST_LAYOUT_VISIBLE_OFFSCREEN", hostRead && host.Positive() && m_wHost.IsVisibleInHierarchy() && m_wHost.GetOpacity() == 0 && host.m_X < -10000 && host.m_Y < -10000, host.Text());
		m_Editor.Measure(this, m_iCase, count);
		m_iCasesMeasured++;
		m_iCase++;
		m_iPhase = 0;
		if (m_iCase >= 9) Finish("GEOMETRY_COMPLETE");
	}

	void Finish(string reason)
	{
		if (m_bStopped) return;
		bool passed = reason == "GEOMETRY_COMPLETE" && m_iCasesMeasured == 9 && m_iChecks > 0 && m_iFailures == 0;
		string glyphStatus = "NOT_PROVEN";
		if (m_iGlyphAvailable == 279 && m_iGlyphUnproven == 0) glyphStatus = "METRICS_AVAILABLE_NONVISUAL";
		string verdict = string.Format("[AICF][VEHICLE_SEAT_LAYOUT_DONE] passed=%1 reason=%2 cases=%3 checks=%4 failures=%5 elapsed_ms=%6 scope=GEOMETRY_ONLY human_visual_proof=0 campaign_required=0 actor_effects=0", passed, reason, m_iCasesMeasured, m_iChecks, m_iFailures, System.GetTickCount() - m_iStartedAt);
		verdict += string.Format(" glyph_status=%1 glyph_available=%2 glyph_unproven_native_zero=%3", glyphStatus, m_iGlyphAvailable, m_iGlyphUnproven);
		Print(verdict);
		Stop();
		if (GetGame()) GetGame().RequestClose();
	}

	void Stop()
	{
		if (m_bStopped) return;
		m_bStopped = true;
		ScriptCallQueue queue = m_Queue;
		m_Queue = null;
		bool scheduled = m_bScheduled;
		m_bScheduled = false;
		if (scheduled && queue) queue.RemoveByName(this, "AICF_Tick");
		if (m_Editor) m_Editor.Close();
		m_Editor = null;
		m_Model = null;
		if (m_wHost) m_wHost.RemoveFromHierarchy();
		m_wHost = null;
	}
}

modded class ArmaReforgerScripted
{
	protected ref AICF_VehicleSeatLayoutProbe m_AICFSeatLayoutProbe;
	override void OnAfterInit(BaseWorld world)
	{
		super.OnAfterInit(world);
		string flag;
		if (m_AICFSeatLayoutProbe || !System.GetCLIParam("aicfVehicleSeatLayoutProbe", flag) || flag != "1") return;
		m_AICFSeatLayoutProbe = new AICF_VehicleSeatLayoutProbe();
		m_AICFSeatLayoutProbe.Start();
	}
	override void OnGameEnd()
	{
		if (m_AICFSeatLayoutProbe) m_AICFSeatLayoutProbe.Stop();
		m_AICFSeatLayoutProbe = null;
		super.OnGameEnd();
	}
}
