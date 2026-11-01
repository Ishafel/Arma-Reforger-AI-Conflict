// Frontend-only, offscreen native form geometry; no input, focus or campaign.
class AICF_OpeningProbeModel : AICF_StrategicUIController
{
	override bool GetLoadoutGroup(int slotId, out string name, out int size, string targetFaction = "")
	{
		name = "Probe group";
		size = 10;
		return slotId >= 0 && slotId < 10;
	}
}

class AICF_OpeningProbeInspector : AICF_LoadoutEditor
{
	void AICF_OpeningProbeInspector(AICF_StrategicUIController controller) { m_Controller = controller; }
	bool Build(Widget parent)
	{
		m_sTargetFaction = "USSR";
		return CreateInspector(parent);
	}
	void Measure()
	{
		m_wRoot.Update();
		m_wPanel.Update();
		float x, y, w, h;
		m_wPanel.GetScreenPos(x, y);
		m_wPanel.GetScreenSize(w, h);
		Print(string.Format("[AICF][OPENING_INSPECTOR] root_visible=%1 root_opacity=%2 panel_visible=%3 panel_opacity=%4 x=%5 y=%6 w=%7 h=%8 root_color=%9", m_wRoot.IsVisibleInHierarchy(), m_wRoot.GetOpacity(), m_wPanel.IsVisibleInHierarchy(), m_wPanel.GetOpacity(), x, y, w, h, m_wRoot.GetColorInt()));
	}
}

class AICF_OpeningProbeVehicle : AICF_VehiclePresetEditor
{
	void AICF_OpeningProbeVehicle(AICF_StrategicUIController controller) { m_Controller = controller; }
	bool Build(Widget parent) { return Create(parent, 0); }
	void Measure()
	{
		m_wRoot.Update();
		m_wPanel.Update();
		float x, y, w, h;
		m_wPanel.GetScreenPos(x, y);
		m_wPanel.GetScreenSize(w, h);
		Print(string.Format("[AICF][OPENING_VEHICLE] root_visible=%1 root_opacity=%2 panel_visible=%3 panel_opacity=%4 x=%5 y=%6 w=%7 h=%8 root_color=%9", m_wRoot.IsVisibleInHierarchy(), m_wRoot.GetOpacity(), m_wPanel.IsVisibleInHierarchy(), m_wPanel.GetOpacity(), x, y, w, h, m_wRoot.GetColorInt()));
	}
}

class AICF_PresetEditorOpeningProbe
{
	protected Widget m_Host;
	protected ref AICF_OpeningProbeModel m_Model;
	protected ref AICF_OpeningProbeInspector m_Inspector;
	protected ref AICF_OpeningProbeVehicle m_Vehicle;
	protected ScriptCallQueue m_Queue;
	protected int m_iStep;
	protected bool m_bStopped;
	void Start()
	{
		m_Queue = GetGame().GetCallqueue();
		m_Queue.CallLaterByName(this, "Tick", 500, true);
	}
	void Tick()
	{
		if (m_bStopped) return;
		if (!m_Host)
		{
			m_Host = GetGame().GetWorkspace().CreateWidget(WidgetType.FrameWidgetTypeID, WidgetFlags.VISIBLE | WidgetFlags.IGNORE_CURSOR, Color.White, 0);
			FrameSlot.SetAnchor(m_Host, 0, 0);
			FrameSlot.SetAlignment(m_Host, 0, 0);
			FrameSlot.SetSizeToContent(m_Host, false);
			FrameSlot.SetPos(m_Host, -30000, -30000);
			FrameSlot.SetSize(m_Host, 1920, 1080);
			m_Host.SetOpacity(0);
			m_Model = new AICF_OpeningProbeModel();
			m_Inspector = new AICF_OpeningProbeInspector(m_Model);
			m_Vehicle = new AICF_OpeningProbeVehicle(m_Model);
			Print(string.Format("[AICF][OPENING_BUILD] inspector=%1 vehicle=%2", m_Inspector.Build(m_Host), m_Vehicle.Build(m_Host)));
		}
		m_Host.Update();
		m_iStep++;
		if (m_iStep < 3) return;
		m_Inspector.Measure();
		m_Vehicle.Measure();
		bool infantryOpened = m_Inspector.Open(null, 0);
		bool vehicleOpened = m_Vehicle.Open(null, 0);
		Print(string.Format("[AICF][OPENING_REJECTION] passed=%1 infantry=%2 vehicle=%3", !infantryOpened && !vehicleOpened && !m_Inspector.IsInputCaptured() && !m_Vehicle.IsInputCaptured(), infantryOpened, vehicleOpened));
		Print("[AICF][OPENING_DONE] scope=GEOMETRY_ONLY");
		Stop();
		GetGame().RequestClose();
	}
	void Stop()
	{
		if (m_bStopped) return;
		m_bStopped = true;
		if (m_Queue) m_Queue.RemoveByName(this, "Tick");
		if (m_Inspector) m_Inspector.Close();
		if (m_Vehicle) m_Vehicle.Close();
		m_Inspector = null;
		m_Vehicle = null;
		m_Model = null;
		if (m_Host) m_Host.RemoveFromHierarchy();
		m_Host = null;
	}
}

modded class ArmaReforgerScripted
{
	protected ref AICF_PresetEditorOpeningProbe m_AICFOpeningProbe;
	override void OnAfterInit(BaseWorld world)
	{
		super.OnAfterInit(world);
		string flag;
		if (m_AICFOpeningProbe || !System.GetCLIParam("aicfPresetEditorOpeningProbe", flag) || flag != "1") return;
		m_AICFOpeningProbe = new AICF_PresetEditorOpeningProbe();
		m_AICFOpeningProbe.Start();
	}
	override void OnGameEnd()
	{
		if (m_AICFOpeningProbe) m_AICFOpeningProbe.Stop();
		m_AICFOpeningProbe = null;
		super.OnGameEnd();
	}
}
