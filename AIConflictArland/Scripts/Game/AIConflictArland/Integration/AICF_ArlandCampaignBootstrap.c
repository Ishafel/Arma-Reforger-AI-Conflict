// Thin Arland integration: wait for stock Conflict readiness, then start one Stage 1 match loop.
modded class SCR_GameModeCampaign
{
	protected bool m_bAICFStage1Scheduled;
	protected bool m_bAICFBootstrapLogged;
	protected bool m_bAICFWaitingForConflict;
	protected bool m_bAICFWaitingForBases;
	protected ref AICF_MatchController m_AICFMatchController;
	protected ref AICF_ArlandRadioBridgeNormalizer m_AICFRadioBridgeNormalizer;
	protected ref AICF_StrategicUIController m_AICFStrategicUIController;

	override void OnGameStart()
	{
		// Stock Conflict initializes its manager, bases, and radio coverage inside this call.
		super.OnGameStart();

		string peerRole = "client";
		if (Replication.IsServer())
			peerRole = "server";
		AICF_Stage1Diagnostics.Configure(string.Format("stage1-%1-%2", peerRole, System.GetTickCount()));
		if (!m_AICFStrategicUIController)
		{
			m_AICFStrategicUIController = new AICF_StrategicUIController();
			m_AICFStrategicUIController.Start(this);
		}

		if (!GetGame().InPlayMode() || !Replication.IsServer() || !IsMaster())
			return;

		if (!m_bAICFBootstrapLogged)
		{
			m_bAICFBootstrapLogged = true;
			AICF_Stage1Diagnostics.Info("BOOTSTRAP_SERVER", "authority=server_master map=Arland");
		}

		AICF_TryStartStage1();
	}

	override void OnGameEnd()
	{
		if (m_AICFStrategicUIController)
		{
			m_AICFStrategicUIController.Stop();
			m_AICFStrategicUIController = null;
		}
		if (m_AICFRadioBridgeNormalizer)
		{
			m_AICFRadioBridgeNormalizer.Stop();
			m_AICFRadioBridgeNormalizer = null;
		}

		super.OnGameEnd();
	}

	protected void AICF_TryStartStage1()
	{
		if (m_AICFMatchController || m_bAICFStage1Scheduled)
		{
			AICF_Stage1Diagnostics.Warning("BOOTSTRAP_DUPLICATE_SKIPPED", "Stage 1 is already scheduled or running");
			return;
		}

		if (!HasStarted())
		{
			if (!m_bAICFWaitingForConflict)
			{
				m_bAICFWaitingForConflict = true;
				AICF_Stage1Diagnostics.Info("BOOTSTRAP_WAIT_CONFLICT", "state=WAITING_FOR_CONFLICT");
				GetOnStarted().Insert(AICF_OnConflictStarted);
			}
			return;
		}

		SCR_CampaignMilitaryBaseManager baseManager = GetBaseManager();
		if (!baseManager)
		{
			AICF_Stage1Diagnostics.Error("BOOTSTRAP_BASE_MANAGER_MISSING", "Conflict started without a base manager");
			AICF_Stage1Diagnostics.Result(false, "reason=BOOTSTRAP_BASE_MANAGER_MISSING");
			return;
		}

		if (!baseManager.IsBasesInitDone())
		{
			if (!m_bAICFWaitingForBases)
			{
				m_bAICFWaitingForBases = true;
				AICF_Stage1Diagnostics.Info("BOOTSTRAP_WAIT_BASES", "state=WAITING_FOR_BASES");
				baseManager.GetOnAllBasesInitialized().Insert(AICF_OnAllBasesInitialized);
			}
			return;
		}

		AICF_ScheduleStage1();
	}

	protected void AICF_OnConflictStarted()
	{
		GetOnStarted().Remove(AICF_OnConflictStarted);
		m_bAICFWaitingForConflict = false;
		AICF_TryStartStage1();
	}

	protected void AICF_OnAllBasesInitialized()
	{
		SCR_CampaignMilitaryBaseManager baseManager = GetBaseManager();
		if (baseManager)
			baseManager.GetOnAllBasesInitialized().Remove(AICF_OnAllBasesInitialized);
		m_bAICFWaitingForBases = false;

		// Stock invokes this before its final radio/faction coverage recalculation.
		AICF_ScheduleStage1();
	}

	protected void AICF_ScheduleStage1()
	{
		if (m_AICFMatchController || m_bAICFStage1Scheduled)
			return;

		m_bAICFStage1Scheduled = true;
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

		if (!m_AICFRadioBridgeNormalizer)
		{
			m_AICFRadioBridgeNormalizer = new AICF_ArlandRadioBridgeNormalizer();
			if (!m_AICFRadioBridgeNormalizer.Start(this))
			{
				m_AICFRadioBridgeNormalizer = null;
				AICF_Stage1Diagnostics.Warning(
					"RADIO_BRIDGE_UNAVAILABLE",
					"Arland radio bridge normalization could not be started");
			}
		}

		AICF_Stage1Diagnostics.Info("CONFLICT_READY", "state=READY scheduling=NEXT_FRAME");
		GetGame().GetCallqueue().CallLater(AICF_RunStage1, 0, false);
	}

	protected void AICF_RunStage1()
	{
		if (m_AICFMatchController)
			return;

		m_AICFMatchController = new AICF_MatchController();
		m_AICFMatchController.Start(this);
	}
}
