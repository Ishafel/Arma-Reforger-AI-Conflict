// Thin stock Conflict integration: wait for supported-map readiness, then start one Stage 1 match loop.
modded class SCR_GameModeCampaign
{
	protected bool m_bAICFStage1Scheduled;
	protected bool m_bAICFBootstrapLogged;
	protected bool m_bAICFWaitingForConflict;
	protected bool m_bAICFWaitingForBases;
	protected ref AICF_Stage1Config m_AICFStage1Config;
	protected ref AICF_MatchController m_AICFMatchController;
	protected ref AICF_StockRadioBridgeNormalizer m_AICFRadioBridgeNormalizer;
	protected ref AICF_StrategicUIController m_AICFStrategicUIController;
	protected ref AICF_ContentProfile m_AICFContentProfile;
	protected string m_sAICFMapKey;

	override void OnGameStart()
	{
		// Stock Conflict initializes its manager, bases, and radio coverage inside this call.
		super.OnGameStart();

		m_sAICFMapKey = AICF_ResolveSupportedMapKey();
		if (m_sAICFMapKey.IsEmpty())
			return;

		string peerRole = "client";
		if (Replication.IsServer())
			peerRole = "server";
		AICF_Stage1Diagnostics.Configure(string.Format("stage1-%1-%2", peerRole, System.GetTickCount()));
		m_AICFContentProfile = AICF_CreateContentProfile();
		AICF_ContentProfile.SetActive(m_AICFContentProfile);
		// Reject an invalid immutable process mode before any AICF subscription or
		// repeating callqueue is installed on the authoritative peer.
		if (GetGame().InPlayMode() && Replication.IsServer() && IsMaster() &&
			!AICF_ValidateStage1Config())
		{
			return;
		}
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
			AICF_Stage1Diagnostics.Info(
				"BOOTSTRAP_SERVER",
				string.Format("authority=server_master map=%1", m_sAICFMapKey));
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

		// Keep faction-key normalization active while the vanilla lifecycle tears
		// down groups and emits their final callbacks.
		AICF_ContentProfile.ClearActive(m_AICFContentProfile);
		m_AICFContentProfile = null;
	}

	protected AICF_ContentProfile AICF_CreateContentProfile()
	{
		return new AICF_ContentProfile();
	}

	protected AICF_StockRadioBridgeNormalizer AICF_CreateRadioBridgeNormalizer()
	{
		return new AICF_StockRadioBridgeNormalizer();
	}

	protected string AICF_ResolveSupportedMapKey()
	{
		string worldFile = GetGame().GetWorldFile();
		if (worldFile.Contains("CTI_Campaign_Arland"))
			return "Arland";
		if (worldFile.Contains("CTI_Campaign_Eden"))
			return "Everon";

		return string.Empty;
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

		// Retain the prevalidated instance handed to MatchController. The fallback
		// protects direct/integration calls without re-reading a valid config.
		if (!AICF_ValidateStage1Config())
			return;

		// The shared lifecycle owns exactly one normalizer. A supported map may
		// specialize its bounded data-driven policy through the factory boundary.
		if (!m_AICFRadioBridgeNormalizer)
		{
			m_AICFRadioBridgeNormalizer = AICF_CreateRadioBridgeNormalizer();
			if (!m_AICFRadioBridgeNormalizer.Start(this))
			{
				m_AICFRadioBridgeNormalizer = null;
				AICF_Stage1Diagnostics.Warning(
					"RADIO_BRIDGE_UNAVAILABLE",
					"Stock radio bridge normalization could not be started");
			}
		}

		AICF_Stage1Diagnostics.Info("CONFLICT_READY", "state=READY scheduling=NEXT_FRAME");
		GetGame().GetCallqueue().CallLater(AICF_RunStage1, 0, false);
	}

	protected bool AICF_ValidateStage1Config()
	{
		if (!m_AICFStage1Config)
			m_AICFStage1Config = new AICF_Stage1Config();
		if (m_AICFStage1Config.IsAICommanderModeValid())
			return true;

		string invalidMode = m_AICFStage1Config.GetInvalidAICommanderMode();
		string detail = string.Format(
			"parameter=aicfAICommanderMode value=\"%1\" allowed=BOTH,US,USSR",
			invalidMode);
		AICF_Stage1Diagnostics.Error("CONFIG_INVALID", detail);
		AICF_Stage1Diagnostics.Result(
			false,
			string.Format("reason=CONFIG_INVALID detail=%1", detail));
		return false;
	}

	protected void AICF_RunStage1()
	{
		if (m_AICFMatchController)
			return;

		m_AICFMatchController = new AICF_MatchController();
		m_AICFMatchController.Start(
			this,
			m_AICFStage1Config,
			m_AICFContentProfile,
			m_sAICFMapKey);
	}
}
