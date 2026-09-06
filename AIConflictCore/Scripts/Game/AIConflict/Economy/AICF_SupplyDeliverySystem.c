// Совместимая aggregate boundary Stage4/UI: только physical custody/receipts.
class AICF_SupplyDeliverySystem
{
	protected ref array<ref AICF_LogisticsWorker> m_aWorkers;
	void AICF_SupplyDeliverySystem(AICF_Stage4Config config, AICF_SupplyNetwork network, AICF_ObjectiveGraph graph) {}
	void SetWorkers(array<ref AICF_LogisticsWorker> workers) { m_aWorkers = workers; }
	int GetInTransitCount(FactionKey faction)
	{
		int count;
		if (!m_aWorkers) return 0;
		foreach (AICF_LogisticsWorker w : m_aWorkers)
		{
			if (w.m_Faction.GetFactionKey() == faction && ((w.m_Job && !w.m_Job.m_bCancelled) || AICF_LogisticsLedger.CustodyCargo(w) > 0)) count++;
		}
		return count;
	}
	float Total(FactionKey faction, int metric)
	{
		float total;
		if (!m_aWorkers) return 0;
		foreach (AICF_LogisticsWorker w : m_aWorkers)
		{
			if (w.m_Faction.GetFactionKey() != faction) continue;
			switch (metric)
			{
				case 0: total += w.m_fLoaded; break;
				case 1: total += w.m_fDelivered; break;
				case 2: total += w.m_fReturned; break;
				case 3: total += AICF_LogisticsLedger.CustodyCargo(w); break;
				case 4: total += w.m_fLost; break;
				case 5: total += w.m_fReleased; break;
				case 6: total += w.m_fExternalIn; break;
				case 7: total += w.m_fExternalOut; break;
				case 8: total += w.m_fDiscrepancy; break;
			}
		}
		return total;
	}
	float GetDispatchedSupplies(FactionKey faction) { return Total(faction, 0); }
	float GetDeliveredSupplies(FactionKey faction) { return Total(faction, 1); }
	float GetReturnedSupplies(FactionKey faction) { return Total(faction, 2); }
	float GetInTransitSupplies(FactionKey faction) { return Total(faction, 3); }
	float BalanceDelta(FactionKey faction)
	{
		return Total(faction, 0) + Total(faction, 6) - Total(faction, 1) - Total(faction, 2) - Total(faction, 3) - Total(faction, 4) - Total(faction, 5) - Total(faction, 7);
	}
	string ExtendedFields(FactionKey faction)
	{
		return string.Format(" schema_version=2 lost=%1 released=%2 external_in=%3 external_out=%4 discrepancy=%5", Total(faction, 4), Total(faction, 5), Total(faction, 6), Total(faction, 7), Total(faction, 8));
	}
}
