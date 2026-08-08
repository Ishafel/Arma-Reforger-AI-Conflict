// Isolates stock Conflict API access from the map-independent Stage 0 model.
class AICF_ConflictAdapter
{
	bool CollectBases(
		SCR_GameModeCampaign campaign,
		out array<SCR_CampaignMilitaryBaseComponent> objectiveBases,
		out array<SCR_CampaignMilitaryBaseComponent> graphBases)
	{
		objectiveBases.Clear();
		graphBases.Clear();

		if (!campaign)
		{
			AICF_Diagnostics.Error("CONFLICT_API_MISSING", "SCR_GameModeCampaign is null");
			return false;
		}

		SCR_CampaignMilitaryBaseManager baseManager = campaign.GetBaseManager();
		if (!baseManager)
		{
			AICF_Diagnostics.Error("CONFLICT_API_MISSING", "SCR_CampaignMilitaryBaseManager is null");
			return false;
		}

		if (!baseManager.IsBasesInitDone())
		{
			AICF_Diagnostics.Error("BASE_DISCOVERY_NOT_READY", "Conflict reports that base initialization is not complete");
			return false;
		}

		baseManager.GetBases(objectiveBases);
		if (objectiveBases.IsEmpty())
		{
			AICF_Diagnostics.Error("BASE_DISCOVERY_EMPTY", "Conflict returned no BASE or SOURCE_BASE objectives");
			return false;
		}

		SCR_MilitaryBaseSystem militaryBaseSystem = SCR_MilitaryBaseSystem.GetInstance();
		if (!militaryBaseSystem)
		{
			AICF_Diagnostics.Error("CONFLICT_API_MISSING", "SCR_MilitaryBaseSystem is null");
			return false;
		}

		array<SCR_MilitaryBaseComponent> allMilitaryBases = {};
		militaryBaseSystem.GetBases(allMilitaryBases);
		foreach (SCR_MilitaryBaseComponent militaryBase : allMilitaryBases)
		{
			SCR_CampaignMilitaryBaseComponent campaignBase = SCR_CampaignMilitaryBaseComponent.Cast(militaryBase);
			if (!campaignBase)
				continue;

			if (!campaignBase.IsInitialized())
			{
				AICF_Diagnostics.Warning(
					"BASE_DISCOVERY_SKIPPED",
					string.Format("Uninitialized campaign base skipped: %1", AICF_Diagnostics.DescribeBase(campaignBase)));
				continue;
			}

			if (!graphBases.Contains(campaignBase))
				graphBases.Insert(campaignBase);
		}

		// The manager intentionally excludes RELAY nodes. Preserve every valid objective even if
		// the global system changes ordering or momentarily omits it.
		foreach (SCR_CampaignMilitaryBaseComponent objectiveBase : objectiveBases)
		{
			if (objectiveBase && !graphBases.Contains(objectiveBase))
				graphBases.Insert(objectiveBase);
		}

		if (graphBases.IsEmpty())
		{
			AICF_Diagnostics.Error("BASE_DISCOVERY_EMPTY", "The military base system returned no initialized Conflict bases");
			return false;
		}

		AICF_Diagnostics.Info(
			"BASE_DISCOVERY",
			string.Format("objectives=%1 graph_nodes=%2 active=%3 expected=%4",
				objectiveBases.Count(),
				graphBases.Count(),
				baseManager.GetActiveBasesCount(),
				baseManager.GetTargetActiveBasesCount()));
		return true;
	}

	bool ResolveFaction(
		SCR_GameModeCampaign campaign,
		SCR_ECampaignFaction side,
		FactionKey expectedKey,
		out SCR_CampaignFaction faction)
	{
		faction = null;
		if (!campaign)
		{
			AICF_Diagnostics.Error("FACTION_API_MISSING", "SCR_GameModeCampaign is null");
			return false;
		}

		faction = campaign.GetFactionByEnum(side);
		if (!faction)
		{
			AICF_Diagnostics.Error("FACTION_NOT_FOUND", string.Format("Conflict side %1 has no SCR_CampaignFaction", side));
			return false;
		}

		FactionKey actualKey = faction.GetFactionKey();
		if (actualKey != expectedKey)
		{
			AICF_Diagnostics.Error(
				"FACTION_MISMATCH",
				string.Format("Expected faction key %1 for side %2, got %3", expectedKey, side, actualKey));
			faction = null;
			return false;
		}

		if (!faction.GetMainBase())
		{
			AICF_Diagnostics.Error("FACTION_HQ_MISSING", string.Format("Faction %1 has no main base", actualKey));
			faction = null;
			return false;
		}

		AICF_Diagnostics.Info(
			"FACTION_READY",
			string.Format("faction=%1 hq={%2}", actualKey, AICF_Diagnostics.DescribeBase(faction.GetMainBase())));
		return true;
	}

	bool SelectSafeSpawnBase(
		SCR_GameModeCampaign campaign,
		SCR_CampaignFaction faction,
		out SCR_CampaignMilitaryBaseComponent spawnBase)
	{
		spawnBase = null;
		if (!campaign || !faction)
			return false;

		SCR_CampaignMilitaryBaseComponent mainBase = faction.GetMainBase();
		if (IsSafeSpawnBase(mainBase, faction))
		{
			spawnBase = mainBase;
			return true;
		}

		SCR_CampaignMilitaryBaseManager baseManager = campaign.GetBaseManager();
		if (!baseManager)
			return false;

		array<SCR_CampaignMilitaryBaseComponent> bases = {};
		baseManager.GetBases(bases);
		foreach (SCR_CampaignMilitaryBaseComponent base : bases)
		{
			if (!IsSafeSpawnBase(base, faction))
				continue;

			spawnBase = base;
			return true;
		}

		return false;
	}

	bool IsSafeSpawnBase(
		SCR_CampaignMilitaryBaseComponent base,
		SCR_CampaignFaction faction)
	{
		return GetSpawnRejectionReason(base, faction).IsEmpty();
	}

	string GetSpawnRejectionReason(
		SCR_CampaignMilitaryBaseComponent base,
		SCR_CampaignFaction faction)
	{
		if (!base || !base.GetOwner())
			return "BASE_MISSING";
		if (!faction)
			return "FACTION_MISSING";
		if (!base.IsInitialized())
			return "BASE_NOT_INITIALIZED";
		if (base.GetFaction() != faction)
			return "ENEMY_OWNED";
		if (base.GetCaptureState() != SCR_EBaseCaptureState.NONE || base.IsBeingCaptured() || base.AreEnemiesPresent())
			return "CONTESTED";

		SCR_SpawnPoint spawnPoint = base.GetSpawnPoint();
		if (!spawnPoint)
			return "SPAWN_POINT_MISSING";
		if (!spawnPoint.IsSpawnPointEnabled())
			return "SPAWN_POINT_DISABLED";
		if (!spawnPoint.IsSpawnPointActive())
			return "SPAWN_POINT_INACTIVE";
		if (spawnPoint.GetFactionKey() != faction.GetFactionKey())
			return "SPAWN_FACTION_MISMATCH";

		return string.Empty;
	}
}
