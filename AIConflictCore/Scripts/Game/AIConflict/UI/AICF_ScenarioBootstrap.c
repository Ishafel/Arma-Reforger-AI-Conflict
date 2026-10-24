// Native Workshop Play loads Core only after entering the setup header.
// This header prepares local UI; only the selected leaf starts an AICF match.
class AICF_ScenarioBootstrap
{
	protected ResourceName m_sEntry;
	protected int m_iAttempts;
	protected bool m_bScanRequested;
	static bool ShouldIntercept()
	{
		if (!GetGame().InPlayMode()) return false;
		if (GameStateTransitions.IsInsideMainMenu()) return false;
		if (System.IsCLIParam("server") || System.IsCLIParam("client") || System.IsCLIParam("world")) return false;
		if (RplSession.Mode() == RplMode.Client || RplSession.Mode() == RplMode.Dedicated) return false;
		SCR_MissionHeaderCampaign header = SCR_MissionHeaderCampaign.Cast(GetGame().GetMissionHeader());
		return header && header.m_bAICFSetupEntry;
	}
	static bool CanConfigure(MissionWorkshopItem entry)
	{
		if (!entry || !ShouldIntercept()) return false;
		return entry.Id() == GetGame().GetMissionHeader().GetHeaderResourceName();
	}

	static bool IsHost(MissionWorkshopItem entry)
	{
		return CanConfigure(entry) && RplSession.Mode() == RplMode.Listen;
	}

	static void Cancel()
	{
		if (!ShouldIntercept() || GameStateTransitions.IsTransitionRequestedOrInProgress()) return;
		// GameplayEnd otherwise selects vanilla and unloads the local menu adapters.
		array<string> loaded = {};
		GameProject.GetLoadedAddons(loaded);
		array<string> addons = {};
		foreach (string id : loaded)
			if (!GameProject.IsVanillaAddon(id)) addons.Insert(id);
		if (addons.IsEmpty()) return;
		Print("[AICF][SCENARIO_BOOTSTRAP] phase=CANCEL");
		GameStateTransitions.RequestGameplayEndTransition();
		GetGame().RequestReload(addons);
	}

	void Start()
	{
		if (!ShouldIntercept() || AICF_ScenarioHostTransition.HasPending()) return;
		m_sEntry = GetGame().GetMissionHeader().GetHeaderResourceName();
		GetGame().GetCallqueue().CallLater(Open, 1000, false);
	}
	protected void Open()
	{
		MissionHeader active = GetGame().GetMissionHeader();
		if (!ShouldIntercept() || !active || active.GetHeaderResourceName() != m_sEntry) return;
		WorkshopApi workshop = GetGame().GetBackendApi().GetWorkshop();
		if (workshop.NeedAddonsScan())
		{
			if (!m_bScanRequested)
			{
				m_bScanRequested = true;
				workshop.ScanOfflineItems();
			}
			if (m_iAttempts++ < 120)
			{
				GetGame().GetCallqueue().CallLater(Open, 250, false);
				return;
			}
			Print("[AICF][SCENARIO_BOOTSTRAP][FAIL] scan_timeout", LogLevel.ERROR);
			return;
		}
		MissionWorkshopItem entry = WorkshopApi.GetCurrentMission();
		if (!entry || entry.Id() != m_sEntry) entry = GetGame().GetBackendApi().GetWorkshop().GetInGameScenario(m_sEntry);
		if (!entry && m_iAttempts++ < 120)
		{
			GetGame().GetCallqueue().CallLater(Open, 250, false);
			return;
		}
		GetGame().GetMenuManager().CloseAllMenus();
		if (!entry || !SCR_ScenarioUICommon.AICF_OpenSetup(entry) || !SCR_ScenarioUICommon.AICF_IsSetupOpen())
		{
			Print("[AICF][SCENARIO_BOOTSTRAP][FAIL] setup_unavailable", LogLevel.ERROR);
			return;
		}
		Print(string.Format("[AICF][SCENARIO_BOOTSTRAP] phase=OPEN entry=%1", m_sEntry));
	}
	void Stop() { GetGame().GetCallqueue().Remove(Open); }
}
modded class ArmaReforgerScripted
{
	protected ref AICF_ScenarioBootstrap m_AICFScenarioBootstrap;
	override bool OnGameStart()
	{
		bool started = super.OnGameStart();
		if (!started) return false;
		m_AICFScenarioBootstrap = new AICF_ScenarioBootstrap();
		m_AICFScenarioBootstrap.Start();
		return started;
	}
	override void OnGameEnd()
	{
		if (m_AICFScenarioBootstrap) m_AICFScenarioBootstrap.Stop();
		m_AICFScenarioBootstrap = null;
		super.OnGameEnd();
	}
	override protected void OnBeforeWorldCleanup()
	{
		SCR_ScenarioUICommon.AICF_CloseSetup();
		super.OnBeforeWorldCleanup();
	}
}
