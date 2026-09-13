// RHS has fewer authored callsigns than initialized bases on full Everon.
// Stock 1.8 removes one pool entry per base without checking for exhaustion.
// Keep unique base indexes; SCR_Faction.GetBaseCallsignByIndex already wraps
// indexes into each faction's authored names and radio signals on every peer.
modded class SCR_CampaignMilitaryBaseManager
{
	override protected void GetSharedCallsignPool(notnull array<int> outIndexes)
	{
		super.GetSharedCallsignPool(outIndexes);
		if (!GetGame().InPlayMode() || !Replication.IsServer() || !m_Campaign || !m_Campaign.IsMaster())
			return;
		if (!GetGame().GetWorldFile().Contains("CTI_Campaign_Eden_RHS"))
			return;

		int originalCount = outIndexes.Count();
		if (originalCount == 0)
			return;

		int initializedBases;
		foreach (SCR_CampaignMilitaryBaseComponent base : m_aBases)
		{
			if (base && base.IsInitialized())
				initializedBases++;
		}
		if (initializedBases <= originalCount)
			return;

		for (int index = originalCount; index < initializedBases; index++)
			outIndexes.Insert(index);

		Print(string.Format(
			"[AICF][CONTENT][INFO][BASE_CALLSIGN_POOL_EXTENDED] map=Everon original=%1 required=%2 policy=STOCK_WRAPPED_NAMES",
			originalCount,
			initializedBases));
	}
}
