// Frontend-only lifecycle stress. Требует AICF_PresetEditorOpeningProbe.c.
// Native widgets находятся вне экрана; input/focus/campaign не используются.
class AICF_PreviewLifetimeVehicle : AICF_OpeningProbeVehicle
{
	void AICF_PreviewLifetimeVehicle(AICF_StrategicUIController controller) { m_Controller = controller; }

	bool Cycle(int index)
	{
		m_aPrefabs.Clear();
		m_aCatalogNames.Clear();
		m_aCatalogCosts.Clear();
		m_aCatalogSources.Clear();
		m_aCatalogCategories.Clear();
		ResourceName prefab = "{259EE7B78C51B624}Prefabs/Vehicles/Wheeled/UAZ469/UAZ469.et";
		if (Math.Mod(index, 2) == 1) prefab = "{9B1BF9644E0378D6}Prefabs/Vehicles/Wheeled/M998/M998_covered_long.et";
		m_aPrefabs.Insert(prefab);
		m_aCatalogNames.Insert("Lifecycle vehicle");
		m_aCatalogCosts.Insert(1);
		m_aCatalogSources.Insert(AICF_EVehicleCatalogSource.FACTION);
		m_aCatalogCategories.Insert(0);
		RenderCatalog(prefab);
		return m_Preview && m_Preview.IsReady() && m_Preview.GetWorld() != GetGame().GetWorld();
	}

	bool ResetPreview()
	{
		RenderCatalog();
		return m_Preview && !m_Preview.GetWorld() && !m_Preview.IsReady();
	}
}

class AICF_PreviewLifetimeProbe
{
	protected Widget m_Host;
	protected ref AICF_OpeningProbeModel m_Model;
	protected ref AICF_PreviewLifetimeVehicle m_Editor;
	protected ScriptCallQueue m_Queue;
	protected int m_iCycles;
	protected int m_iFailures;
	protected bool m_bStopped;

	void Start()
	{
		m_Queue = GetGame().GetCallqueue();
		m_Queue.CallLaterByName(this, "Tick", 1000, true);
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
		}
		if (!m_Editor)
		{
			m_Editor = new AICF_PreviewLifetimeVehicle(m_Model);
			if (!m_Editor.Build(m_Host)) { Finish(false); return; }
		}
		bool passed = m_Editor.Cycle(m_iCycles);
		if (!passed) m_iFailures++;
		m_iCycles++;
		Print(string.Format("[AICF][PREVIEW_LIFETIME_CYCLE] index=%1 passed=%2 scope=VEHICLE_WORLD physical_input=0", m_iCycles, passed));
		// Смена модели между frames; каждый пятый цикл — настоящее удаление
		// формы и создание заново, остальные — RefreshPreview той же формы.
		if (Math.Mod(m_iCycles, 5) == 0)
		{
			if (!m_Editor.ResetPreview()) m_iFailures++;
			m_Editor.Close();
			m_Editor = null;
		}
		if (m_iCycles >= 60 || !passed) Finish(passed && m_iFailures == 0);
	}

	protected void Finish(bool passed)
	{
		Print(string.Format("[AICF][PREVIEW_LIFETIME_FINISHED] passed=%1 cycles=%2 failures=%3 scope=VEHICLE_WORLD original_oom_reproduced=UNKNOWN", passed, m_iCycles, m_iFailures));
		Stop();
		GetGame().RequestClose();
	}

	void Stop()
	{
		if (m_bStopped) return;
		m_bStopped = true;
		if (m_Queue) m_Queue.RemoveByName(this, "Tick");
		if (m_Editor) m_Editor.Close();
		m_Editor = null;
		m_Model = null;
		if (m_Host) m_Host.RemoveFromHierarchy();
		m_Host = null;
	}
}

modded class ArmaReforgerScripted
{
	protected ref AICF_PreviewLifetimeProbe m_AICFPreviewLifetime;
	override void OnAfterInit(BaseWorld world)
	{
		super.OnAfterInit(world);
		string flag;
		if (m_AICFPreviewLifetime || !System.GetCLIParam("aicfPreviewLifetimeProbe", flag) || flag != "1") return;
		m_AICFPreviewLifetime = new AICF_PreviewLifetimeProbe();
		m_AICFPreviewLifetime.Start();
	}
	override void OnGameEnd()
	{
		if (m_AICFPreviewLifetime) m_AICFPreviewLifetime.Stop();
		m_AICFPreviewLifetime = null;
		super.OnGameEnd();
	}
}
