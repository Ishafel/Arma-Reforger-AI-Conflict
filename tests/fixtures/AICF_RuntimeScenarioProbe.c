// Test-only probe for native targets, runtime rules and retired resource rejection.
class AICF_RuntimeScenarioProbe
{
	protected int m_iChecks;
	protected int m_iFailed;
	protected int m_iAttempts;

	void Check(string name, bool passed)
	{
		m_iChecks++;
		if (!passed) m_iFailed++;
		Print(string.Format("[AICF][RUNTIME_CASE] case=%1 pass=%2", name, passed));
	}
	void Start()
	{
		GetGame().GetCallqueue().CallLater(Read, 15000, false);
	}
	protected void Read()
	{
		SCR_GameModeCampaign campaign = SCR_GameModeCampaign.Cast(GetGame().GetGameMode());
		if ((!campaign || !campaign.AICF_HasRuntimeScenario() || !AICF_RuntimeScenario.IsActive()) && m_iAttempts++ < 120)
		{
			GetGame().GetCallqueue().CallLater(Read, 250, false);
			return;
		}
		AICF_RuntimeScenarioSelection active = AICF_RuntimeScenario.GetSelection();
		SCR_MissionHeaderCampaign header = SCR_MissionHeaderCampaign.Cast(GetGame().GetMissionHeader());
		Check("ACTIVE", active && campaign && campaign.AICF_HasRuntimeScenario() && header);
		if (!active || !campaign || !header) { Finish(); return; }
		Check("TARGET", active.GetTarget() == header.GetHeaderResourceName() && !header.m_bAICFSetupEntry);
		Check("MODE", header.m_eAICFMatchMode == active.m_iMode && campaign.AICF_GetRuntimeMode() == active.m_iMode);
		Check("DIFFICULTY", header.m_eAICFDifficulty == active.m_iDifficulty && AICF_Difficulty.Get() == active.m_iDifficulty);
		Check("RULES", AICF_MatchRules.FromMissionHeader().GetMode() == active.m_iMode);
		Check("NO_SAVE", header.m_eSaveTypes == 0);
		Check("RANK", header.m_eStartingRank == SCR_ECharacterRank.GENERAL && campaign.GetStartingRank() == SCR_ECharacterRank.GENERAL);
		string expected;
		if (System.GetCLIParam("aicfProbeExpectedVariant", expected)) Check("EXPECTED_VARIANT", active.GetVariant().m_sKey == expected);
		if (System.GetCLIParam("aicfProbeExpectedMode", expected)) Check("EXPECTED_MODE", active.m_iMode == expected.ToInt());
		if (System.GetCLIParam("aicfProbeExpectedDifficulty", expected)) Check("EXPECTED_DIFFICULTY", active.m_iDifficulty == expected.ToInt());
		if (Replication.IsServer() && active.m_iMap == 2)
		{
			array<SCR_MilitaryBaseComponent> bases = {};
			SCR_MilitaryBaseSystem.GetInstance().GetBases(bases);
			int initialized, hqs, points;
			foreach (SCR_MilitaryBaseComponent base : bases)
			{
				SCR_CampaignMilitaryBaseComponent capture = SCR_CampaignMilitaryBaseComponent.Cast(base);
				if (!capture || !capture.IsInitialized()) continue;
				initialized++;
				if (capture.IsHQ()) hqs++;
				if (capture.IsControlPoint()) points++;
				Print(string.Format("[AICF][RUNTIME_BASE] name=%1 hq=%2 point=%3", capture.GetOwner().GetName(), capture.IsHQ(), capture.IsControlPoint()));
			}
			Check("NORTH_HEADER", header.m_bCustomBaseWhitelist && header.m_aCampaignCustomBaseList.Count() == 7);
			Check("NORTH_BASES", initialized == 7 && hqs == 2 && points == 5);
			Print(string.Format("[AICF][RUNTIME_NORTH] initialized=%1 hqs=%2 points=%3", initialized, hqs, points));
		}
		AICF_ScenarioRegistry registry = new AICF_ScenarioRegistry();
		foreach (AICF_ScenarioVariant variant : registry.GetVariants())
		{
			for (int mode; mode < 3; mode++)
			{
				for (int difficulty; difficulty < 3; difficulty++)
				{
					AICF_RuntimeScenarioSelection value = new AICF_RuntimeScenarioSelection();
					value.m_iMap = variant.m_iMap;
					value.m_iContent = variant.m_iContent;
					value.m_iMode = mode;
					value.m_iDifficulty = difficulty;
					value.m_iIssued = System.GetTickCount();
					AICF_RuntimeScenarioSelection decoded = AICF_RuntimeScenarioSelection.Decode(value.Encode());
					Check(string.Format("PLAN_%1_%2_%3", variant.m_sKey, mode, difficulty), decoded && decoded.GetTarget() == variant.m_sMission && decoded.m_iMode == mode && decoded.m_iDifficulty == difficulty);
				}
			}
		}
		Check("INVALID_JSON", !AICF_RuntimeScenarioSelection.Decode("{broken"));
		AICF_RuntimeScenarioSelection invalid = new AICF_RuntimeScenarioSelection();
		invalid.m_iIssued = System.GetTickCount();
		invalid.m_iMode = 3;
		Check("INVALID_MODE", !invalid.IsValid());
		invalid.m_iMode = 0;
		invalid.m_iDifficulty = -1;
		Check("INVALID_DIFFICULTY", !invalid.IsValid());
		invalid.m_iDifficulty = 0;
		invalid.m_iMap = 4;
		Check("INVALID_MAP", !invalid.IsValid());
		if (Replication.IsServer() && System.IsCLIParam("aicfProbeRetiredResources")) CheckRetired();
		Print(string.Format("[AICF][RUNTIME_SELECTION] variant=%1 mode=%2 difficulty=%3 target=%4 authority=%5", active.GetVariant().m_sKey, active.m_iMode, active.m_iDifficulty, active.GetTarget(), Replication.IsServer()));
		Finish();
	}
	protected void CheckRetired()
	{
		array<string> retired = {};
		// RETIRED RESOURCES BEGIN
		retired.Insert("{BC2437E4861B4FD2}Missions/AICF_Conflict_Arland.conf");
		retired.Insert("{A1CF261008300001}Missions/AICF_Conflict_Arland_Medium.conf");
		retired.Insert("{A1CF261008300002}Missions/AICF_Conflict_Arland_Hard.conf");
		retired.Insert("{A1CF261018460111}Missions/AICF_Conflict_Arland_Coop_US.conf");
		retired.Insert("{A1CF261018460112}Missions/AICF_Conflict_Arland_Coop_US_Medium.conf");
		retired.Insert("{A1CF261018460113}Missions/AICF_Conflict_Arland_Coop_US_Hard.conf");
		retired.Insert("{A1CF261018460001}Missions/AICF_Conflict_Arland_Coop_USSR.conf");
		retired.Insert("{A1CF261018460002}Missions/AICF_Conflict_Arland_Coop_USSR_Medium.conf");
		retired.Insert("{A1CF261018460003}Missions/AICF_Conflict_Arland_Coop_USSR_Hard.conf");
		retired.Insert("{4C5D73A5614F41D9}Missions/AICF_Conflict_Everon.conf");
		retired.Insert("{A1CF261008300003}Missions/AICF_Conflict_Everon_Medium.conf");
		retired.Insert("{A1CF261008300004}Missions/AICF_Conflict_Everon_Hard.conf");
		retired.Insert("{A1CF261018460211}Missions/AICF_Conflict_Everon_Coop_US.conf");
		retired.Insert("{A1CF261018460212}Missions/AICF_Conflict_Everon_Coop_US_Medium.conf");
		retired.Insert("{A1CF261018460213}Missions/AICF_Conflict_Everon_Coop_US_Hard.conf");
		retired.Insert("{A1CF261018460221}Missions/AICF_Conflict_Everon_Coop_USSR.conf");
		retired.Insert("{A1CF261018460222}Missions/AICF_Conflict_Everon_Coop_USSR_Medium.conf");
		retired.Insert("{A1CF261018460223}Missions/AICF_Conflict_Everon_Coop_USSR_Hard.conf");
		retired.Insert("{A1CF190919300000}Missions/AICF_Conflict_Everon_North.conf");
		retired.Insert("{A1CF261008300005}Missions/AICF_Conflict_Everon_North_Medium.conf");
		retired.Insert("{A1CF261008300006}Missions/AICF_Conflict_Everon_North_Hard.conf");
		retired.Insert("{A1CF261018460311}Missions/AICF_Conflict_Everon_North_Coop_US.conf");
		retired.Insert("{A1CF261018460312}Missions/AICF_Conflict_Everon_North_Coop_US_Medium.conf");
		retired.Insert("{A1CF261018460313}Missions/AICF_Conflict_Everon_North_Coop_US_Hard.conf");
		retired.Insert("{A1CF261018460321}Missions/AICF_Conflict_Everon_North_Coop_USSR.conf");
		retired.Insert("{A1CF261018460322}Missions/AICF_Conflict_Everon_North_Coop_USSR_Medium.conf");
		retired.Insert("{A1CF261018460323}Missions/AICF_Conflict_Everon_North_Coop_USSR_Hard.conf");
		retired.Insert("{97E4BCB73F044C66}Missions/AICF_RHS_Conflict_Arland.conf");
		retired.Insert("{A1CF261008300007}Missions/AICF_RHS_Conflict_Arland_Medium.conf");
		retired.Insert("{A1CF261008300008}Missions/AICF_RHS_Conflict_Arland_Hard.conf");
		retired.Insert("{A1CF261018460411}Missions/AICF_RHS_Conflict_Arland_Coop_US.conf");
		retired.Insert("{A1CF261018460412}Missions/AICF_RHS_Conflict_Arland_Coop_US_Medium.conf");
		retired.Insert("{A1CF261018460413}Missions/AICF_RHS_Conflict_Arland_Coop_US_Hard.conf");
		retired.Insert("{A1CF261018460421}Missions/AICF_RHS_Conflict_Arland_Coop_USSR.conf");
		retired.Insert("{A1CF261018460422}Missions/AICF_RHS_Conflict_Arland_Coop_USSR_Medium.conf");
		retired.Insert("{A1CF261018460423}Missions/AICF_RHS_Conflict_Arland_Coop_USSR_Hard.conf");
		retired.Insert("{57FA3D0337BE47E5}Missions/AICF_RHS_Conflict_Everon.conf");
		retired.Insert("{A1CF261008300009}Missions/AICF_RHS_Conflict_Everon_Medium.conf");
		retired.Insert("{A1CF261008300010}Missions/AICF_RHS_Conflict_Everon_Hard.conf");
		retired.Insert("{A1CF261018460511}Missions/AICF_RHS_Conflict_Everon_Coop_US.conf");
		retired.Insert("{A1CF261018460512}Missions/AICF_RHS_Conflict_Everon_Coop_US_Medium.conf");
		retired.Insert("{A1CF261018460513}Missions/AICF_RHS_Conflict_Everon_Coop_US_Hard.conf");
		retired.Insert("{A1CF261018460521}Missions/AICF_RHS_Conflict_Everon_Coop_USSR.conf");
		retired.Insert("{A1CF261018460522}Missions/AICF_RHS_Conflict_Everon_Coop_USSR_Medium.conf");
		retired.Insert("{A1CF261018460523}Missions/AICF_RHS_Conflict_Everon_Coop_USSR_Hard.conf");
		retired.Insert("{A1CF190919100000}Missions/AICF_RHS_Conflict_Everon_North.conf");
		retired.Insert("{A1CF261008300011}Missions/AICF_RHS_Conflict_Everon_North_Medium.conf");
		retired.Insert("{A1CF261008300012}Missions/AICF_RHS_Conflict_Everon_North_Hard.conf");
		retired.Insert("{A1CF261018460611}Missions/AICF_RHS_Conflict_Everon_North_Coop_US.conf");
		retired.Insert("{A1CF261018460612}Missions/AICF_RHS_Conflict_Everon_North_Coop_US_Medium.conf");
		retired.Insert("{A1CF261018460613}Missions/AICF_RHS_Conflict_Everon_North_Coop_US_Hard.conf");
		retired.Insert("{A1CF261018460621}Missions/AICF_RHS_Conflict_Everon_North_Coop_USSR.conf");
		retired.Insert("{A1CF261018460622}Missions/AICF_RHS_Conflict_Everon_North_Coop_USSR_Medium.conf");
		retired.Insert("{A1CF261018460623}Missions/AICF_RHS_Conflict_Everon_North_Coop_USSR_Hard.conf");
		retired.Insert("{A1CF260928100002}Missions/AICF_WCS_RHS_Conflict_Arland.conf");
		retired.Insert("{A1CF261008300013}Missions/AICF_WCS_RHS_Conflict_Arland_Medium.conf");
		retired.Insert("{A1CF261008300014}Missions/AICF_WCS_RHS_Conflict_Arland_Hard.conf");
		retired.Insert("{A1CF261018460711}Missions/AICF_WCS_RHS_Conflict_Arland_Coop_US.conf");
		retired.Insert("{A1CF261018460712}Missions/AICF_WCS_RHS_Conflict_Arland_Coop_US_Medium.conf");
		retired.Insert("{A1CF261018460713}Missions/AICF_WCS_RHS_Conflict_Arland_Coop_US_Hard.conf");
		retired.Insert("{A1CF261018460721}Missions/AICF_WCS_RHS_Conflict_Arland_Coop_USSR.conf");
		retired.Insert("{A1CF261018460722}Missions/AICF_WCS_RHS_Conflict_Arland_Coop_USSR_Medium.conf");
		retired.Insert("{A1CF261018460723}Missions/AICF_WCS_RHS_Conflict_Arland_Coop_USSR_Hard.conf");
		retired.Insert("{A1CF261006100002}Missions/AICF_WCS_RHS_Conflict_Everon.conf");
		retired.Insert("{A1CF261008300015}Missions/AICF_WCS_RHS_Conflict_Everon_Medium.conf");
		retired.Insert("{A1CF261008300016}Missions/AICF_WCS_RHS_Conflict_Everon_Hard.conf");
		retired.Insert("{A1CF261018460811}Missions/AICF_WCS_RHS_Conflict_Everon_Coop_US.conf");
		retired.Insert("{A1CF261018460812}Missions/AICF_WCS_RHS_Conflict_Everon_Coop_US_Medium.conf");
		retired.Insert("{A1CF261018460813}Missions/AICF_WCS_RHS_Conflict_Everon_Coop_US_Hard.conf");
		retired.Insert("{A1CF261018460821}Missions/AICF_WCS_RHS_Conflict_Everon_Coop_USSR.conf");
		retired.Insert("{A1CF261018460822}Missions/AICF_WCS_RHS_Conflict_Everon_Coop_USSR_Medium.conf");
		retired.Insert("{A1CF261018460823}Missions/AICF_WCS_RHS_Conflict_Everon_Coop_USSR_Hard.conf");
		retired.Insert("{A1CF261006100003}Missions/AICF_WCS_RHS_Conflict_Everon_North.conf");
		retired.Insert("{A1CF261008100001}Missions/AICF_WCS_RHS_Conflict_Everon_North_Medium.conf");
		retired.Insert("{A1CF261008100002}Missions/AICF_WCS_RHS_Conflict_Everon_North_Hard.conf");
		retired.Insert("{A1CF261018460911}Missions/AICF_WCS_RHS_Conflict_Everon_North_Coop_US.conf");
		retired.Insert("{A1CF261018460912}Missions/AICF_WCS_RHS_Conflict_Everon_North_Coop_US_Medium.conf");
		retired.Insert("{A1CF261018460913}Missions/AICF_WCS_RHS_Conflict_Everon_North_Coop_US_Hard.conf");
		retired.Insert("{A1CF261018460921}Missions/AICF_WCS_RHS_Conflict_Everon_North_Coop_USSR.conf");
		retired.Insert("{A1CF261018460922}Missions/AICF_WCS_RHS_Conflict_Everon_North_Coop_USSR_Medium.conf");
		retired.Insert("{A1CF261018460923}Missions/AICF_WCS_RHS_Conflict_Everon_North_Coop_USSR_Hard.conf");
		// RETIRED RESOURCES END
		foreach (int index, string resource : retired)
			Check(string.Format("RETIRED_%1", index), !MissionHeader.ReadMissionHeader(resource));
	}
	protected void Finish()
	{
		Print(string.Format("[AICF][RUNTIME_DONE] checks=%1 failed=%2", m_iChecks, m_iFailed));
		string hold;
		int delay = 2000;
		if (Replication.IsServer() && System.GetCLIParam("aicfRuntimeHold", hold)) delay = hold.ToInt() * 1000;
		GetGame().GetCallqueue().CallLater(Close, delay, false);
	}
	protected void Close() { GetGame().RequestClose(); }
	void Stop()
	{
		GetGame().GetCallqueue().Remove(Read);
		GetGame().GetCallqueue().Remove(Close);
	}
}
modded class ArmaReforgerScripted
{
	protected ref AICF_RuntimeScenarioProbe m_AICFRuntimeProbe;
	override protected void OnWorldPostProcess(World world)
	{
		if (System.IsCLIParam("aicfRuntimeScenarioProbe") && System.IsCLIParam("server") && System.IsCLIParam("aicfProbeEnvironment"))
		{
			string key, mode, difficulty;
			System.GetCLIParam("aicfScenarioVariant", key);
			System.GetCLIParam("aicfMatchMode", mode);
			System.GetCLIParam("aicfDifficulty", difficulty);
			AICF_ScenarioRegistry registry = new AICF_ScenarioRegistry();
			AICF_ScenarioVariant variant = registry.FindKey(key);
			AICF_ScenarioEnvironment environment = new AICF_ScenarioEnvironment();
			environment.m_iStartMinutes = 577;
			environment.m_iWeather = 4;
			if (variant) AICF_RuntimeScenario.QueueValues(variant.m_iMap, variant.m_iContent, mode.ToInt(), difficulty.ToInt(), environment);
		}
		super.OnWorldPostProcess(world);
	}
	override bool OnGameStart()
	{
		bool result = super.OnGameStart();
		if (result && System.IsCLIParam("aicfRuntimeScenarioProbe"))
		{
			m_AICFRuntimeProbe = new AICF_RuntimeScenarioProbe();
			m_AICFRuntimeProbe.Start();
		}
		return result;
	}
	override void OnGameEnd()
	{
		if (m_AICFRuntimeProbe) m_AICFRuntimeProbe.Stop();
		m_AICFRuntimeProbe = null;
		super.OnGameEnd();
	}
}
