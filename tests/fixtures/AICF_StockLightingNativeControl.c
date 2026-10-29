// Native-only stock inspection scene. No AICF gameplay/editor/preview classes.
modded class SCR_GameModeCampaign
{
	protected ref SharedItemRef m_AICFStockLightingWorldRef;
	protected BaseWorld m_AICFStockLightingWorld;
	protected IEntity m_AICFStockLightingEntity;
	override void OnGameStart()
	{
		super.OnGameStart();
		string enabled;
		if (!Replication.IsServer() && System.GetCLIParam("aicfStockLightingControl", enabled))
			GetGame().GetCallqueue().CallLater(AICF_StockLightingNativeBegin, 3000, false);
	}
	protected void AICF_StockLightingNativeBegin()
	{
		Print("[AICF][STOCK_LIGHTING_NATIVE_CONTROL] phase=BEGIN production_scripts=0 editor_widgets=0 vehicle_preview_model=0");
		m_AICFStockLightingWorldRef = BaseWorld.CreateWorld("InspectionPreview", "AICF_StockLightingNativeControl");
		m_AICFStockLightingWorld = m_AICFStockLightingWorldRef.GetRef();
		m_AICFStockLightingWorld.SetCameraType(0, CameraType.PERSPECTIVE);
		m_AICFStockLightingWorld.SetCameraFarPlane(0, 50);
		m_AICFStockLightingWorld.SetCameraNearPlane(0, 0.001);
		Resource resource = Resource.Load("{4391FE7994EE6FE2}Prefabs/World/Game/InventoryPreviewWorld.et");
		m_AICFStockLightingEntity = GetGame().SpawnEntityPrefab(resource, m_AICFStockLightingWorld);
		Print(string.Format("[AICF][STOCK_LIGHTING_NATIVE_CONTROL] phase=BUILT scene_present=%1 world_private=%2 scene_same_world=%3 native_spawn=SpawnEntityPrefab", m_AICFStockLightingEntity != null, m_AICFStockLightingWorld != GetGame().GetWorld(), m_AICFStockLightingEntity && m_AICFStockLightingEntity.GetWorld() == m_AICFStockLightingWorld));
	}
	override void OnGameEnd()
	{
		GetGame().GetCallqueue().Remove(AICF_StockLightingNativeBegin);
		if (m_AICFStockLightingEntity) SCR_EntityHelper.DeleteEntityAndChildren(m_AICFStockLightingEntity);
		m_AICFStockLightingEntity = null;
		m_AICFStockLightingWorld = null;
		m_AICFStockLightingWorldRef = null;
		super.OnGameEnd();
	}
}
