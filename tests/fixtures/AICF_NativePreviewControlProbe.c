// Изолированный диагностический control: штатный ItemPreviewManager без
// Inspector/personal widgets. Сравнивает private и campaign world, не меняет PP.
modded class AICF_PersonalLoadoutUI
{
	override void Update(Widget parent, bool characterSelected, ItemPreviewWidget loadoutImage = null)
	{
		string flag;
		if (System.GetCLIParam("aicfNativePreviewControl", flag) && flag == "1") return;
		super.Update(parent, characterSelected, loadoutImage);
	}
}

modded class SCR_BaseGameMode
{
	protected ref SharedItemRef m_NativeControlWorld;
	protected ItemPreviewManagerEntity m_NativeControlManager;

	override void OnGameStart()
	{
		super.OnGameStart();
		string flag;
		if (!System.GetCLIParam("aicfNativePreviewControl", flag) || flag != "1") return;
		if (Replication.IsServer()) GetGame().GetCallqueue().CallLater(AICF_NativeControlClose, 120000, false);
		else
		{
			GetGame().GetCallqueue().CallLater(AICF_NativeControlPrivate, 3000, false);
			GetGame().GetCallqueue().CallLater(AICF_NativeControlGlobal, 20000, false);
			GetGame().GetCallqueue().CallLater(AICF_NativeControlClose, 40000, false);
		}
	}

	protected void AICF_NativeControlPrivate()
	{
		Print("[AICF][PREVIEW_CONTROL] phase=private_begin");
		m_NativeControlWorld = BaseWorld.CreateWorld("ChimeraWorld", "AICF_NativePreviewControl");
		ChimeraWorld world = m_NativeControlWorld.GetRef();
		world.LoadSystems("{3A1DEDB2A5818B53}Configs/Systems/MapPreviewSystemsConfig.conf");
		Resource resource = Resource.Load("{9F18C476AB860F3B}Prefabs/World/Game/ItemPreviewManager.et");
		m_NativeControlManager = ItemPreviewManagerEntity.Cast(GetGame().SpawnEntityPrefabLocal(resource, world));
		IEntity item = m_NativeControlManager.ResolvePreviewEntityForPrefab("{3E413771E1834D2F}Prefabs/Weapons/Rifles/M16/Rifle_M16A2.et");
		Print(string.Format("[AICF][PREVIEW_CONTROL] phase=private_end manager=%1 own_world=%2", m_NativeControlManager != null, item && item.GetWorld() == world));
	}

	protected void AICF_NativeControlGlobal()
	{
		Print("[AICF][PREVIEW_CONTROL] phase=global_begin");
		ChimeraWorld world = GetGame().GetWorld();
		bool existed = world.GetItemPreviewManager() != null;
		if (!existed)
		{
			Resource resource = Resource.Load("{9F18C476AB860F3B}Prefabs/World/Game/ItemPreviewManager.et");
			GetGame().SpawnEntityPrefabLocal(resource, world);
		}
		ItemPreviewManagerEntity manager = world.GetItemPreviewManager();
		IEntity item = manager.ResolvePreviewEntityForPrefab("{3E413771E1834D2F}Prefabs/Weapons/Rifles/M16/Rifle_M16A2.et");
		Print(string.Format("[AICF][PREVIEW_CONTROL] phase=global_end existed=%1 manager=%2 model=%3", existed, manager != null, item != null));
	}

	protected void AICF_NativeControlClear()
	{
		GetGame().GetCallqueue().Remove(AICF_NativeControlPrivate);
		GetGame().GetCallqueue().Remove(AICF_NativeControlGlobal);
		GetGame().GetCallqueue().Remove(AICF_NativeControlClose);
		if (m_NativeControlManager) delete m_NativeControlManager;
		m_NativeControlManager = null;
		m_NativeControlWorld = null;
	}

	protected void AICF_NativeControlClose()
	{
		AICF_NativeControlClear();
		Print("[AICF][PREVIEW_CONTROL] phase=close");
		GetGame().RequestClose();
	}

	override void OnGameEnd()
	{
		AICF_NativeControlClear();
		super.OnGameEnd();
	}
}
