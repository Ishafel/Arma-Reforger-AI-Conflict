// Isolated stage. Run only with the full Everon WCS graph, which mounts all parents.
modded class SCR_GameModeCampaign
{
	protected int m_iAICFHeaderCount;
	protected bool m_bAICFHeaderFailed;

	protected bool AICF_HeaderSameBases(SCR_MissionHeaderCampaign header, SCR_MissionHeaderCampaign parent)
	{
		if (header.m_bCustomBaseWhitelist != parent.m_bCustomBaseWhitelist) return false;
		int count, parentCount;
		if (header.m_aCampaignCustomBaseList) count = header.m_aCampaignCustomBaseList.Count();
		if (parent.m_aCampaignCustomBaseList) parentCount = parent.m_aCampaignCustomBaseList.Count();
		if (count != parentCount) return false;
		for (int i; i < count; i++)
		{
			SCR_CampaignCustomBase first = header.m_aCampaignCustomBaseList[i];
			SCR_CampaignCustomBase second = parent.m_aCampaignCustomBaseList[i];
			if (!first || !second || first.GetBaseName() != second.GetBaseName() ||
				first.IsControlPoint() != second.IsControlPoint() || first.GetCanBeHQ() != second.GetCanBeHQ() ||
				first.GetDisableWhenUnusedAsHQ() != second.GetDisableWhenUnusedAsHQ() || first.GetRadioRange() != second.GetRadioRange()) return false;
		}
		return true;
	}

	protected void AICF_HeaderCase(ResourceName resource, ResourceName parentResource, string variant, string side, int difficulty)
	{
		m_iAICFHeaderCount++;
		SCR_MissionHeaderCampaign header = SCR_MissionHeaderCampaign.Cast(MissionHeader.ReadMissionHeader(resource));
		SCR_MissionHeaderCampaign parent = SCR_MissionHeaderCampaign.Cast(MissionHeader.ReadMissionHeader(parentResource));
		AICF_EMatchMode expected = AICF_EMatchMode.COOP_US;
		if (side == "USSR") expected = AICF_EMatchMode.COOP_USSR;
		bool passed = header && parent && header.m_eAICFMatchMode == expected && header.m_eAICFDifficulty == difficulty &&
			header.GetWorldResourceName() == parent.GetWorldResourceName() && !header.GetWorldResourceName().IsEmpty() &&
			header.m_bAICFFIATank == parent.m_bAICFFIATank && header.m_bAICFFIAPassengers == parent.m_bAICFFIAPassengers && AICF_HeaderSameBases(header, parent);
		if (!passed) m_bAICFHeaderFailed = true;
		string level = "Easy";
		if (difficulty == 1) level = "Medium";
		if (difficulty == 2) level = "Hard";
		Print(string.Format("[AICF][COOP_HEADER_CASE] variant=%1 side=%2 difficulty=%3 pass=%4", variant, side, level, passed));
	}

	override void OnGameStart()
	{
		super.OnGameStart();
		string flag;
		if (!Replication.IsServer() || !System.GetCLIParam("aicfCoopHeaderProbe", flag) || flag != "1") return;
		// AICF HEADER MATRIX BEGIN
		AICF_HeaderCase("{A1CF261018460111}Missions/AICF_Conflict_Arland_Coop_US.conf", "{BC2437E4861B4FD2}Missions/AICF_Conflict_Arland.conf", "Stock", "US", 0);
		AICF_HeaderCase("{A1CF261018460112}Missions/AICF_Conflict_Arland_Coop_US_Medium.conf", "{A1CF261008300001}Missions/AICF_Conflict_Arland_Medium.conf", "Stock", "US", 1);
		AICF_HeaderCase("{A1CF261018460113}Missions/AICF_Conflict_Arland_Coop_US_Hard.conf", "{A1CF261008300002}Missions/AICF_Conflict_Arland_Hard.conf", "Stock", "US", 2);
		AICF_HeaderCase("{A1CF261018460001}Missions/AICF_Conflict_Arland_Coop_USSR.conf", "{BC2437E4861B4FD2}Missions/AICF_Conflict_Arland.conf", "Stock", "USSR", 0);
		AICF_HeaderCase("{A1CF261018460002}Missions/AICF_Conflict_Arland_Coop_USSR_Medium.conf", "{A1CF261008300001}Missions/AICF_Conflict_Arland_Medium.conf", "Stock", "USSR", 1);
		AICF_HeaderCase("{A1CF261018460003}Missions/AICF_Conflict_Arland_Coop_USSR_Hard.conf", "{A1CF261008300002}Missions/AICF_Conflict_Arland_Hard.conf", "Stock", "USSR", 2);
		AICF_HeaderCase("{A1CF261018460211}Missions/AICF_Conflict_Everon_Coop_US.conf", "{4C5D73A5614F41D9}Missions/AICF_Conflict_Everon.conf", "Everon", "US", 0);
		AICF_HeaderCase("{A1CF261018460212}Missions/AICF_Conflict_Everon_Coop_US_Medium.conf", "{A1CF261008300003}Missions/AICF_Conflict_Everon_Medium.conf", "Everon", "US", 1);
		AICF_HeaderCase("{A1CF261018460213}Missions/AICF_Conflict_Everon_Coop_US_Hard.conf", "{A1CF261008300004}Missions/AICF_Conflict_Everon_Hard.conf", "Everon", "US", 2);
		AICF_HeaderCase("{A1CF261018460221}Missions/AICF_Conflict_Everon_Coop_USSR.conf", "{4C5D73A5614F41D9}Missions/AICF_Conflict_Everon.conf", "Everon", "USSR", 0);
		AICF_HeaderCase("{A1CF261018460222}Missions/AICF_Conflict_Everon_Coop_USSR_Medium.conf", "{A1CF261008300003}Missions/AICF_Conflict_Everon_Medium.conf", "Everon", "USSR", 1);
		AICF_HeaderCase("{A1CF261018460223}Missions/AICF_Conflict_Everon_Coop_USSR_Hard.conf", "{A1CF261008300004}Missions/AICF_Conflict_Everon_Hard.conf", "Everon", "USSR", 2);
		AICF_HeaderCase("{A1CF261018460311}Missions/AICF_Conflict_Everon_North_Coop_US.conf", "{A1CF190919300000}Missions/AICF_Conflict_Everon_North.conf", "EveronNorth", "US", 0);
		AICF_HeaderCase("{A1CF261018460312}Missions/AICF_Conflict_Everon_North_Coop_US_Medium.conf", "{A1CF261008300005}Missions/AICF_Conflict_Everon_North_Medium.conf", "EveronNorth", "US", 1);
		AICF_HeaderCase("{A1CF261018460313}Missions/AICF_Conflict_Everon_North_Coop_US_Hard.conf", "{A1CF261008300006}Missions/AICF_Conflict_Everon_North_Hard.conf", "EveronNorth", "US", 2);
		AICF_HeaderCase("{A1CF261018460321}Missions/AICF_Conflict_Everon_North_Coop_USSR.conf", "{A1CF190919300000}Missions/AICF_Conflict_Everon_North.conf", "EveronNorth", "USSR", 0);
		AICF_HeaderCase("{A1CF261018460322}Missions/AICF_Conflict_Everon_North_Coop_USSR_Medium.conf", "{A1CF261008300005}Missions/AICF_Conflict_Everon_North_Medium.conf", "EveronNorth", "USSR", 1);
		AICF_HeaderCase("{A1CF261018460323}Missions/AICF_Conflict_Everon_North_Coop_USSR_Hard.conf", "{A1CF261008300006}Missions/AICF_Conflict_Everon_North_Hard.conf", "EveronNorth", "USSR", 2);
		AICF_HeaderCase("{A1CF261018460411}Missions/AICF_RHS_Conflict_Arland_Coop_US.conf", "{97E4BCB73F044C66}Missions/AICF_RHS_Conflict_Arland.conf", "RHS", "US", 0);
		AICF_HeaderCase("{A1CF261018460412}Missions/AICF_RHS_Conflict_Arland_Coop_US_Medium.conf", "{A1CF261008300007}Missions/AICF_RHS_Conflict_Arland_Medium.conf", "RHS", "US", 1);
		AICF_HeaderCase("{A1CF261018460413}Missions/AICF_RHS_Conflict_Arland_Coop_US_Hard.conf", "{A1CF261008300008}Missions/AICF_RHS_Conflict_Arland_Hard.conf", "RHS", "US", 2);
		AICF_HeaderCase("{A1CF261018460421}Missions/AICF_RHS_Conflict_Arland_Coop_USSR.conf", "{97E4BCB73F044C66}Missions/AICF_RHS_Conflict_Arland.conf", "RHS", "USSR", 0);
		AICF_HeaderCase("{A1CF261018460422}Missions/AICF_RHS_Conflict_Arland_Coop_USSR_Medium.conf", "{A1CF261008300007}Missions/AICF_RHS_Conflict_Arland_Medium.conf", "RHS", "USSR", 1);
		AICF_HeaderCase("{A1CF261018460423}Missions/AICF_RHS_Conflict_Arland_Coop_USSR_Hard.conf", "{A1CF261008300008}Missions/AICF_RHS_Conflict_Arland_Hard.conf", "RHS", "USSR", 2);
		AICF_HeaderCase("{A1CF261018460511}Missions/AICF_RHS_Conflict_Everon_Coop_US.conf", "{57FA3D0337BE47E5}Missions/AICF_RHS_Conflict_Everon.conf", "EveronRHS", "US", 0);
		AICF_HeaderCase("{A1CF261018460512}Missions/AICF_RHS_Conflict_Everon_Coop_US_Medium.conf", "{A1CF261008300009}Missions/AICF_RHS_Conflict_Everon_Medium.conf", "EveronRHS", "US", 1);
		AICF_HeaderCase("{A1CF261018460513}Missions/AICF_RHS_Conflict_Everon_Coop_US_Hard.conf", "{A1CF261008300010}Missions/AICF_RHS_Conflict_Everon_Hard.conf", "EveronRHS", "US", 2);
		AICF_HeaderCase("{A1CF261018460521}Missions/AICF_RHS_Conflict_Everon_Coop_USSR.conf", "{57FA3D0337BE47E5}Missions/AICF_RHS_Conflict_Everon.conf", "EveronRHS", "USSR", 0);
		AICF_HeaderCase("{A1CF261018460522}Missions/AICF_RHS_Conflict_Everon_Coop_USSR_Medium.conf", "{A1CF261008300009}Missions/AICF_RHS_Conflict_Everon_Medium.conf", "EveronRHS", "USSR", 1);
		AICF_HeaderCase("{A1CF261018460523}Missions/AICF_RHS_Conflict_Everon_Coop_USSR_Hard.conf", "{A1CF261008300010}Missions/AICF_RHS_Conflict_Everon_Hard.conf", "EveronRHS", "USSR", 2);
		AICF_HeaderCase("{A1CF261018460611}Missions/AICF_RHS_Conflict_Everon_North_Coop_US.conf", "{A1CF190919100000}Missions/AICF_RHS_Conflict_Everon_North.conf", "EveronNorthRHS", "US", 0);
		AICF_HeaderCase("{A1CF261018460612}Missions/AICF_RHS_Conflict_Everon_North_Coop_US_Medium.conf", "{A1CF261008300011}Missions/AICF_RHS_Conflict_Everon_North_Medium.conf", "EveronNorthRHS", "US", 1);
		AICF_HeaderCase("{A1CF261018460613}Missions/AICF_RHS_Conflict_Everon_North_Coop_US_Hard.conf", "{A1CF261008300012}Missions/AICF_RHS_Conflict_Everon_North_Hard.conf", "EveronNorthRHS", "US", 2);
		AICF_HeaderCase("{A1CF261018460621}Missions/AICF_RHS_Conflict_Everon_North_Coop_USSR.conf", "{A1CF190919100000}Missions/AICF_RHS_Conflict_Everon_North.conf", "EveronNorthRHS", "USSR", 0);
		AICF_HeaderCase("{A1CF261018460622}Missions/AICF_RHS_Conflict_Everon_North_Coop_USSR_Medium.conf", "{A1CF261008300011}Missions/AICF_RHS_Conflict_Everon_North_Medium.conf", "EveronNorthRHS", "USSR", 1);
		AICF_HeaderCase("{A1CF261018460623}Missions/AICF_RHS_Conflict_Everon_North_Coop_USSR_Hard.conf", "{A1CF261008300012}Missions/AICF_RHS_Conflict_Everon_North_Hard.conf", "EveronNorthRHS", "USSR", 2);
		AICF_HeaderCase("{A1CF261018460711}Missions/AICF_WCS_RHS_Conflict_Arland_Coop_US.conf", "{A1CF260928100002}Missions/AICF_WCS_RHS_Conflict_Arland.conf", "ArlandWCSRHS", "US", 0);
		AICF_HeaderCase("{A1CF261018460712}Missions/AICF_WCS_RHS_Conflict_Arland_Coop_US_Medium.conf", "{A1CF261008300013}Missions/AICF_WCS_RHS_Conflict_Arland_Medium.conf", "ArlandWCSRHS", "US", 1);
		AICF_HeaderCase("{A1CF261018460713}Missions/AICF_WCS_RHS_Conflict_Arland_Coop_US_Hard.conf", "{A1CF261008300014}Missions/AICF_WCS_RHS_Conflict_Arland_Hard.conf", "ArlandWCSRHS", "US", 2);
		AICF_HeaderCase("{A1CF261018460721}Missions/AICF_WCS_RHS_Conflict_Arland_Coop_USSR.conf", "{A1CF260928100002}Missions/AICF_WCS_RHS_Conflict_Arland.conf", "ArlandWCSRHS", "USSR", 0);
		AICF_HeaderCase("{A1CF261018460722}Missions/AICF_WCS_RHS_Conflict_Arland_Coop_USSR_Medium.conf", "{A1CF261008300013}Missions/AICF_WCS_RHS_Conflict_Arland_Medium.conf", "ArlandWCSRHS", "USSR", 1);
		AICF_HeaderCase("{A1CF261018460723}Missions/AICF_WCS_RHS_Conflict_Arland_Coop_USSR_Hard.conf", "{A1CF261008300014}Missions/AICF_WCS_RHS_Conflict_Arland_Hard.conf", "ArlandWCSRHS", "USSR", 2);
		AICF_HeaderCase("{A1CF261018460811}Missions/AICF_WCS_RHS_Conflict_Everon_Coop_US.conf", "{A1CF261006100002}Missions/AICF_WCS_RHS_Conflict_Everon.conf", "EveronWCSRHS", "US", 0);
		AICF_HeaderCase("{A1CF261018460812}Missions/AICF_WCS_RHS_Conflict_Everon_Coop_US_Medium.conf", "{A1CF261008300015}Missions/AICF_WCS_RHS_Conflict_Everon_Medium.conf", "EveronWCSRHS", "US", 1);
		AICF_HeaderCase("{A1CF261018460813}Missions/AICF_WCS_RHS_Conflict_Everon_Coop_US_Hard.conf", "{A1CF261008300016}Missions/AICF_WCS_RHS_Conflict_Everon_Hard.conf", "EveronWCSRHS", "US", 2);
		AICF_HeaderCase("{A1CF261018460821}Missions/AICF_WCS_RHS_Conflict_Everon_Coop_USSR.conf", "{A1CF261006100002}Missions/AICF_WCS_RHS_Conflict_Everon.conf", "EveronWCSRHS", "USSR", 0);
		AICF_HeaderCase("{A1CF261018460822}Missions/AICF_WCS_RHS_Conflict_Everon_Coop_USSR_Medium.conf", "{A1CF261008300015}Missions/AICF_WCS_RHS_Conflict_Everon_Medium.conf", "EveronWCSRHS", "USSR", 1);
		AICF_HeaderCase("{A1CF261018460823}Missions/AICF_WCS_RHS_Conflict_Everon_Coop_USSR_Hard.conf", "{A1CF261008300016}Missions/AICF_WCS_RHS_Conflict_Everon_Hard.conf", "EveronWCSRHS", "USSR", 2);
		AICF_HeaderCase("{A1CF261018460911}Missions/AICF_WCS_RHS_Conflict_Everon_North_Coop_US.conf", "{A1CF261006100003}Missions/AICF_WCS_RHS_Conflict_Everon_North.conf", "EveronNorthWCSRHS", "US", 0);
		AICF_HeaderCase("{A1CF261018460912}Missions/AICF_WCS_RHS_Conflict_Everon_North_Coop_US_Medium.conf", "{A1CF261008100001}Missions/AICF_WCS_RHS_Conflict_Everon_North_Medium.conf", "EveronNorthWCSRHS", "US", 1);
		AICF_HeaderCase("{A1CF261018460913}Missions/AICF_WCS_RHS_Conflict_Everon_North_Coop_US_Hard.conf", "{A1CF261008100002}Missions/AICF_WCS_RHS_Conflict_Everon_North_Hard.conf", "EveronNorthWCSRHS", "US", 2);
		AICF_HeaderCase("{A1CF261018460921}Missions/AICF_WCS_RHS_Conflict_Everon_North_Coop_USSR.conf", "{A1CF261006100003}Missions/AICF_WCS_RHS_Conflict_Everon_North.conf", "EveronNorthWCSRHS", "USSR", 0);
		AICF_HeaderCase("{A1CF261018460922}Missions/AICF_WCS_RHS_Conflict_Everon_North_Coop_USSR_Medium.conf", "{A1CF261008100001}Missions/AICF_WCS_RHS_Conflict_Everon_North_Medium.conf", "EveronNorthWCSRHS", "USSR", 1);
		AICF_HeaderCase("{A1CF261018460923}Missions/AICF_WCS_RHS_Conflict_Everon_North_Coop_USSR_Hard.conf", "{A1CF261008100002}Missions/AICF_WCS_RHS_Conflict_Everon_North_Hard.conf", "EveronNorthWCSRHS", "USSR", 2);
		// AICF HEADER MATRIX END
		Print(string.Format("[AICF][COOP_HEADERS_DONE] count=%1 failed=%2", m_iAICFHeaderCount, m_bAICFHeaderFailed));
	}
}
