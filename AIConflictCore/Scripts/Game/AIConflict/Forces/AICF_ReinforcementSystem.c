// Selects a safe stock Conflict spawn site immediately before creating a replacement.
class AICF_ReinforcementSystem
{
	protected bool m_bRejectedUnsafeSite;

	bool HasRejectedUnsafeSite()
	{
		return m_bRejectedUnsafeSite;
	}

	bool TrySpawn(
		SCR_GameModeCampaign campaign,
		SCR_CampaignFaction faction,
		AICF_GroupSlot slot,
		AICF_ConflictAdapter conflictAdapter,
		AICF_GroupSpawner groupSpawner,
		out SCR_AIGroup group,
		out SCR_CampaignMilitaryBaseComponent spawnBase)
	{
		group = null;
		spawnBase = null;
		if (!Replication.IsServer() || !campaign || !faction || !slot || !conflictAdapter || !groupSpawner)
			return false;

		array<SCR_CampaignMilitaryBaseComponent> candidates = {};
		SCR_CampaignMilitaryBaseComponent mainBase = faction.GetMainBase();
		if (mainBase)
			candidates.Insert(mainBase);

		SCR_CampaignMilitaryBaseManager baseManager = campaign.GetBaseManager();
		if (baseManager)
		{
			array<SCR_CampaignMilitaryBaseComponent> bases = {};
			baseManager.GetBases(bases);
			foreach (SCR_CampaignMilitaryBaseComponent base : bases)
			{
				if (base && !candidates.Contains(base))
					candidates.Insert(base);
			}
		}

		foreach (SCR_CampaignMilitaryBaseComponent candidate : candidates)
		{
			string rejectionReason = conflictAdapter.GetSpawnRejectionReason(candidate, faction);
			if (!rejectionReason.IsEmpty())
			{
				if (rejectionReason == "ENEMY_OWNED" || rejectionReason == "CONTESTED")
					m_bRejectedUnsafeSite = true;

				AICF_Stage1Diagnostics.Info(
					"SPAWN_SITE_REJECTED",
					string.Format(
						"faction=%1 slot=%2 base=%3 reason=%4",
						faction.GetFactionKey(),
						slot.GetSlotId(),
						AICF_Stage1Diagnostics.BaseKey(candidate),
						rejectionReason));
				continue;
			}

			// Preserve first-safe selection, but inspect every candidate so rejected hostile or
			// contested sites always produce deterministic safety evidence for acceptance.
			if (!spawnBase)
				spawnBase = candidate;
		}

		if (!spawnBase)
		{
			AICF_Stage1Diagnostics.Warning(
				"REINFORCEMENT_BLOCKED",
				string.Format("faction=%1 slot=%2 reason=NO_SAFE_FRIENDLY_BASE", faction.GetFactionKey(), slot.GetSlotId()));
			return false;
		}

		AICF_Stage1Diagnostics.Info(
			"SPAWN_SITE_SELECTED",
			string.Format(
				"faction=%1 slot=%2 base=%3 owner=%4 contested=0",
				faction.GetFactionKey(),
				slot.GetSlotId(),
				AICF_Stage1Diagnostics.BaseKey(spawnBase),
				spawnBase.GetFaction().GetFactionKey()));

		group = groupSpawner.SpawnGroup(faction, spawnBase, slot.GetSlotId());
		return group != null;
	}
}
