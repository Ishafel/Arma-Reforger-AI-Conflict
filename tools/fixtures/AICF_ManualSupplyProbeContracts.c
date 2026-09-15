// Только isolated runtime-source с AICF_ManualSupplyClientProbe.c.
// Проверяет учёт результатов probe; не выдаёт синтетические статусы за рейсы.
class AICF_ManualSupplyProbeContracts
{
	protected int m_iCases;
	protected int m_iFailures;

	protected void Check(string scenario, bool passed)
	{
		m_iCases++;
		if (!passed) m_iFailures++;
		Print(string.Format("[AICF][MANUAL_PROBE_CONTRACT] test_only=1 case=%1 passed=%2", scenario, passed));
	}

	protected bool CanClose(AICF_ManualSupplyProbeResults results, bool submissionsDone = true, bool pending = false)
	{
		return AICF_ManualSupplyProbeResults.CanClose(submissionsDone, pending, results.m_aAccepted.Count(), results.m_aFinished.Count());
	}

	void Run()
	{
		AICF_ManualSupplyProbeResults results = new AICF_ManualSupplyProbeResults();
		Check("NO_SUBMISSIONS", !CanClose(results, false));
		Check("PENDING_REPLY", !CanClose(results, true, true));
		Check("INVALID_TOKEN", !results.Observe(0, true) && !results.Observe(-1, false));
		results.Observe(1, true);
		Check("FIRST_ACTIVE", !CanClose(results));
		Check("ACCEPTANCE_DEDUP", !results.Observe(1, true) && results.m_aAccepted.Count() == 1);
		results.Observe(2, false);
		Check("SECOND_REJECTED_FIRST_ACTIVE", !CanClose(results) && results.m_aFinished.IsEmpty());
		results.Observe(1, false);
		Check("REJECTED_SECOND_FIRST_FINISHED", CanClose(results));
		Check("RESULT_DEDUP", !results.Observe(1, false) && results.m_aFinished.Count() == 1);
		Check("FINISHED_BUT_SUBMISSIONS_OPEN", !CanClose(results, false));
		Check("FINISHED_BUT_REPLY_PENDING", !CanClose(results, true, true));

		results = new AICF_ManualSupplyProbeResults();
		results.Observe(3, true);
		results.Observe(4, true);
		results.Observe(4, false);
		Check("SECOND_FINISHED_FIRST_ACTIVE", !CanClose(results) && results.m_aAccepted.Count() == 2 && results.m_aFinished.Count() == 1);
		results.Observe(3, false);
		Check("OLDER_TOKEN_FINISHES_LAST", CanClose(results));

		results = new AICF_ManualSupplyProbeResults();
		results.Observe(5, true);
		results.Observe(6, true);
		results.Observe(5, false);
		Check("FIRST_FINISHED_SECOND_ACTIVE", !CanClose(results));
		results.Observe(6, false);
		Check("SECOND_TOKEN_FINISHES_LAST", CanClose(results));
		Print(string.Format("[AICF][MANUAL_PROBE_CONTRACTS_FINISHED] test_only=1 cases=%1 failures=%2", m_iCases, m_iFailures));
	}
}

modded class SCR_GameModeCampaign
{
	override void OnGameStart()
	{
		super.OnGameStart();
		string enabled;
		if (!Replication.IsServer() || !System.GetCLIParam("aicfManualSupplyProbeContracts", enabled) || enabled != "1") return;
		GetGame().GetCallqueue().CallLater(AICF_ManualProbeRunContracts, 5000, false);
	}

	protected void AICF_ManualProbeRunContracts()
	{
		AICF_ManualSupplyProbeContracts contracts = new AICF_ManualSupplyProbeContracts();
		contracts.Run();
		GetGame().RequestClose();
	}

	void ~SCR_GameModeCampaign()
	{
		if (GetGame()) GetGame().GetCallqueue().Remove(AICF_ManualProbeRunContracts);
	}
}
