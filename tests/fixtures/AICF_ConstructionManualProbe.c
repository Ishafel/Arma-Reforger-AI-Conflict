// Read-only диагностика ручного проекта. Копируется только в isolated addon.
modded class AICF_BaseBuilderService
{
	override void Start(SCR_GameModeCampaign campaign, AICF_OrderPlanner planner, int combatBudget)
	{
		super.Start(campaign, planner, combatBudget);
		if (!Replication.IsServer() || !campaign)
			return;
		array<SCR_CampaignMilitaryBaseComponent> bases = {};
		campaign.GetBaseManager().GetBases(bases);
		foreach (SCR_CampaignMilitaryBaseComponent base : bases)
		{
			SCR_CampaignFaction faction = SCR_CampaignFaction.Cast(base.GetFaction());
			if (!faction || faction.GetMainBase() != base || !base.GetOwner())
				continue;
			Print(string.Format("[AICF][CONSTRUCTION_MANUAL_HQ] test_only=1 faction=%1 base=%2 name=%3 position=%4",
				faction.GetFactionKey(), base.GetOwner().GetID(), base.GetBaseName(), base.GetOwner().GetOrigin()));
		}
	}

	override protected void OnPlaced(int prefabID, SCR_EditableEntityComponent editableEntity, int playerId, SCR_CampaignBuildingProviderComponent provider)
	{
		super.OnPlaced(prefabID, editableEntity, playerId, provider);
		if (m_bStopped || !Replication.IsServer() || !editableEntity || !editableEntity.GetOwner() || !provider || !provider.GetOwner())
			return;
		IEntity entity = editableEntity.GetOwner();
		Print(string.Format("[AICF][CONSTRUCTION_MANUAL_PLACED] test_only=1 player=%1 entity=%2 prefab=%3 position=%4 angles=%5 provider=%6 provider_position=%7",
			playerId, entity.GetID(), editableEntity.GetPrefab(), entity.GetOrigin(), entity.GetYawPitchRoll(),
			provider.GetOwner().GetID(), provider.GetOwner().GetOrigin()));
	}
}
