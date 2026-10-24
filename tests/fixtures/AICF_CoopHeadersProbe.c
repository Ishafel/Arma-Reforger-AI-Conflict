// Isolated full-content graph: native targets plus all logical co-op plans.
modded class SCR_GameModeCampaign
{
 override void OnGameStart()
 {
	super.OnGameStart();
	if (!Replication.IsServer() || !System.IsCLIParam("aicfCoopHeaderProbe")) return;
	AICF_ScenarioRegistry registry = new AICF_ScenarioRegistry();
	int count;
	bool failed;
	foreach (AICF_ScenarioVariant variant : registry.GetVariants())
	{
	 for (int mode = 1; mode < 3; mode++)
	 {
		for (int difficulty; difficulty < 3; difficulty++)
		{
		 ResourceName target = variant.Resolve(mode, difficulty);
		 SCR_MissionHeaderCampaign header = SCR_MissionHeaderCampaign.Cast(MissionHeader.ReadMissionHeader(target));
		 bool passed = header && !header.m_bAICFSetupEntry && !header.GetWorldResourceName().IsEmpty() && target == variant.m_sMission;
		 string side = "US";
		 if (mode == 2) side = "USSR";
		 string level = "Easy";
		 if (difficulty == 1) level = "Medium";
		 if (difficulty == 2) level = "Hard";
		 count++;
		 if (!passed) failed = true;
		 Print(string.Format("[AICF][COOP_HEADER_CASE] variant=%1 side=%2 difficulty=%3 pass=%4", variant.m_sKey, side, level, passed));
		}
	 }
	}
	Print(string.Format("[AICF][COOP_HEADERS_DONE] count=%1 failed=%2", count, failed));
 }
}
