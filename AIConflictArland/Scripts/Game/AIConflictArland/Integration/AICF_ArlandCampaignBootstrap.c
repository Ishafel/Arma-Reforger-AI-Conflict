// Thin, scripts-only integration with the stock Conflict game mode. No Arland world is copied.
modded class SCR_GameModeCampaign
{
	protected bool m_bAICFStage0Scheduled;
	protected bool m_bAICFBootstrapLogged;
	protected bool m_bAICFWaitingForConflict;
	protected bool m_bAICFWaitingForBases;
	protected ref AICF_Stage0Controller m_AICFStage0Controller;

	override void OnGameStart()
	{
		// Stock Conflict initializes its manager, bases, and radio coverage inside this call.
		super.OnGameStart();

		if (!GetGame().InPlayMode() || !Replication.IsServer() || !IsMaster())
			return;

		if (!m_bAICFBootstrapLogged)
		{
			m_bAICFBootstrapLogged = true;
			AICF_Diagnostics.Info("BOOTSTRAP_SERVER", "Authoritative Conflict game mode detected");
		}
		AICF_TryStartStage0();
	}

	protected void AICF_TryStartStage0()
	{
		if (m_AICFStage0Controller || m_bAICFStage0Scheduled)
		{
			AICF_Diagnostics.Warning("BOOTSTRAP_DUPLICATE_SKIPPED", "Stage 0 initialization is already scheduled or running");
			return;
		}

		if (!HasStarted())
		{
			if (!m_bAICFWaitingForConflict)
			{
				m_bAICFWaitingForConflict = true;
				AICF_Diagnostics.Info("BOOTSTRAP_WAIT_CONFLICT", "Waiting for SCR_GameModeCampaign.GetOnStarted()");
				GetOnStarted().Insert(AICF_OnConflictStarted);
			}
			else
			{
				AICF_Diagnostics.Warning("BOOTSTRAP_WAIT_ALREADY_REGISTERED", "Conflict readiness callback is already registered");
			}
			return;
		}

		SCR_CampaignMilitaryBaseManager baseManager = GetBaseManager();
		if (!baseManager)
		{
			AICF_Diagnostics.Error("BOOTSTRAP_BASE_MANAGER_MISSING", "Conflict started without a base manager");
			AICF_Diagnostics.Result(false, "Conflict base manager is missing");
			return;
		}

		if (!baseManager.IsBasesInitDone())
		{
			if (!m_bAICFWaitingForBases)
			{
				m_bAICFWaitingForBases = true;
				AICF_Diagnostics.Info("BOOTSTRAP_WAIT_BASES", "Waiting for all Conflict bases to initialize");
				baseManager.GetOnAllBasesInitialized().Insert(AICF_OnAllBasesInitialized);
			}
			else
			{
				AICF_Diagnostics.Warning("BOOTSTRAP_WAIT_ALREADY_REGISTERED", "Base readiness callback is already registered");
			}
			return;
		}

		AICF_ScheduleStage0();
	}

	protected void AICF_OnConflictStarted()
	{
		GetOnStarted().Remove(AICF_OnConflictStarted);
		m_bAICFWaitingForConflict = false;
		AICF_TryStartStage0();
	}

	protected void AICF_OnAllBasesInitialized()
	{
		SCR_CampaignMilitaryBaseManager baseManager = GetBaseManager();
		if (baseManager)
			baseManager.GetOnAllBasesInitialized().Remove(AICF_OnAllBasesInitialized);
		m_bAICFWaitingForBases = false;

		// The stock invoker fires before its final faction coverage recalculation. Defer one frame.
		AICF_ScheduleStage0();
	}

	protected void AICF_ScheduleStage0()
	{
		if (m_AICFStage0Controller || m_bAICFStage0Scheduled)
		{
			AICF_Diagnostics.Warning("BOOTSTRAP_DUPLICATE_SKIPPED", "Duplicate Stage 0 schedule request ignored");
			return;
		}

		m_bAICFStage0Scheduled = true;
		if (m_bAICFWaitingForConflict)
		{
			GetOnStarted().Remove(AICF_OnConflictStarted);
			m_bAICFWaitingForConflict = false;
		}

		SCR_CampaignMilitaryBaseManager baseManager = GetBaseManager();
		if (m_bAICFWaitingForBases && baseManager)
		{
			baseManager.GetOnAllBasesInitialized().Remove(AICF_OnAllBasesInitialized);
			m_bAICFWaitingForBases = false;
		}

		AICF_Diagnostics.Info("CONFLICT_READY", "Conflict and its base manager are ready; scheduling Stage 0 next frame");
		GetGame().GetCallqueue().CallLater(AICF_RunStage0, 0, false);
	}

	protected void AICF_RunStage0()
	{
		if (m_AICFStage0Controller)
		{
			AICF_Diagnostics.Warning("BOOTSTRAP_DUPLICATE_SKIPPED", "Stage 0 controller already exists");
			return;
		}

		m_AICFStage0Controller = new AICF_Stage0Controller();
		m_AICFStage0Controller.Start(this);
	}
}
