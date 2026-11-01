// Isolated read-only timings. Each wrapper calls super exactly once and
// returns its actual result. Nested inclusive durations must not be summed.
class AICF_ServerPerformanceSample
{
	string m_sName;
	int m_iCalls;
	int m_iMs;
	int m_iMaxMs;
}

class AICF_ServerPerformanceProbe
{
	static bool s_bLegacyBodyLookup;
	protected static ref array<ref AICF_ServerPerformanceSample> s_aSamples = {};
	protected static int s_iReportAt;

	static void Observe(string name, int started)
	{
		int elapsed = System.GetTickCount(started);
		AICF_ServerPerformanceSample sample;
		foreach (AICF_ServerPerformanceSample candidate : s_aSamples)
			if (candidate.m_sName == name) { sample = candidate; break; }
		if (!sample)
		{
			sample = new AICF_ServerPerformanceSample();
			sample.m_sName = name;
			s_aSamples.Insert(sample);
		}
		sample.m_iCalls++;
		sample.m_iMs += elapsed;
		if (elapsed > sample.m_iMaxMs) sample.m_iMaxMs = elapsed;
	}

	static void Report()
	{
		int now = System.GetTickCount();
		if (now - s_iReportAt < 15000) return;
		int window = now - s_iReportAt;
		s_iReportAt = now;
		foreach (AICF_ServerPerformanceSample sample : s_aSamples)
		{
			Print(string.Format("[AICF][SERVER_PERFORMANCE] name=%1 calls=%2 inclusive_ms=%3 max_ms=%4 window_ms=%5 added_gameplay_calls=0", sample.m_sName, sample.m_iCalls, sample.m_iMs, sample.m_iMaxMs, window));
			sample.m_iCalls = 0;
			sample.m_iMs = 0;
			sample.m_iMaxMs = 0;
		}
	}
}

modded class AICF_AIWorldQuotaService
{
	protected bool m_bBodyLookupBenchDone;
	protected bool m_bBodyLookupBenchBusy;

	override protected AICF_AIQuotaBody FindBody(IEntity entity)
	{
		if (!AICF_ServerPerformanceProbe.s_bLegacyBodyLookup) return super.FindBody(entity);
		if (!entity) return null;
		foreach (AICF_AIQuotaBody body : m_aBodies)
			if (body && !body.m_bReleased && body.m_Entity == entity) return body;
		return null;
	}

	protected void AICF_BenchmarkBodyLookup()
	{
		if (m_bBodyLookupBenchDone || m_bBodyLookupBenchBusy || !System.IsCLIParam("aicfBodyLookupBenchmark")) return;
		if (!m_Ledger || m_Ledger.GetPhysical(AICF_EAIQuotaBucket.STOCK) != 128 || m_aBodies.Count() < 140) return;
		m_bBodyLookupBenchDone = true;
		m_bBodyLookupBenchBusy = true;
		int active = m_World.GetCurrentNumOfActiveAIs();
		int intended = m_World.GetActiveGroupIntendedCount();
		int bodies = m_aBodies.Count();
		bool stable = true;
		for (int round; round < 4; round++)
		{
			for (int implementation; implementation < 2; implementation++)
			{
				AICF_ServerPerformanceProbe.s_bLegacyBodyLookup = implementation == 0;
				int started = System.GetTickCount();
				for (int repeat; repeat < 8; repeat++)
					if (!super.RefreshCensus()) stable = false;
				int elapsed = System.GetTickCount(started);
				if (active != m_World.GetCurrentNumOfActiveAIs() || intended != m_World.GetActiveGroupIntendedCount() || bodies != m_aBodies.Count()) stable = false;
				Print(string.Format("[AICF][BODY_LOOKUP_PERFORMANCE] round=%1 legacy=%2 calls=8 elapsed_ms=%3 active=%4 intended=%5 bodies=%6 stable=%7 controlled_extra_refreshes=1", round, AICF_ServerPerformanceProbe.s_bLegacyBodyLookup, elapsed, active, intended, bodies, stable));
			}
		}
		AICF_ServerPerformanceProbe.s_bLegacyBodyLookup = false;
		m_bBodyLookupBenchBusy = false;
		Print(string.Format("[AICF][BODY_LOOKUP_PERFORMANCE_DONE] stable=%1 added_refresh_calls=64 ordinary_fps_proof=0", stable));
	}

	override bool RefreshCensus()
	{
		int started = System.GetTickCount();
		bool actual = super.RefreshCensus();
		AICF_ServerPerformanceProbe.Observe("RefreshCensus", started);
		AICF_ServerPerformanceProbe.Report();
		if (actual) AICF_BenchmarkBodyLookup();
		return actual;
	}

	override protected bool BuildCounts(AICF_AIQuotaCensus snapshot, array<int> counts)
	{
		int started = System.GetTickCount();
		bool actual = super.BuildCounts(snapshot, counts);
		AICF_ServerPerformanceProbe.Observe("BuildCounts", started);
		return actual;
	}

	override protected bool InitializeQuotaCharacterBindings(AICF_AIQuotaCensus snapshot)
	{
		int started = System.GetTickCount();
		bool actual = super.InitializeQuotaCharacterBindings(snapshot);
		AICF_ServerPerformanceProbe.Observe("CharacterBindings", started);
		return actual;
	}
}

modded class AICF_AIQuotaCensus
{
	override bool Capture(SCR_AIWorld world)
	{
		int started = System.GetTickCount();
		bool actual = super.Capture(world);
		AICF_ServerPerformanceProbe.Observe("CensusCapture", started);
		return actual;
	}

	override bool IsCurrent()
	{
		int started = System.GetTickCount();
		bool actual = super.IsCurrent();
		AICF_ServerPerformanceProbe.Observe("CensusCurrent", started);
		return actual;
	}
}

modded class AICF_AIQuotaVehicleUse
{
	override bool IsCurrent()
	{
		int started = System.GetTickCount();
		bool actual = super.IsCurrent();
		AICF_ServerPerformanceProbe.Observe("VehicleUseCurrent", started);
		return actual;
	}
}

modded class AICF_GroupTrace
{
	override void Update(AICF_FactionState state, AICF_VehicleCoordinator vehicles)
	{
		int started = System.GetTickCount();
		super.Update(state, vehicles);
		AICF_ServerPerformanceProbe.Observe("GroupTrace", started);
	}
}

modded class AICF_MatchController
{
	protected int m_iServerPerformanceStarted;
	protected bool m_bServerPerformanceClosed;

	override protected void Update()
	{
		if (!m_iServerPerformanceStarted) m_iServerPerformanceStarted = System.GetTickCount();
		super.Update();
		string value;
		if (m_bServerPerformanceClosed || !System.GetCLIParam("aicfServerPerformanceDurationMs", value)) return;
		int duration = value.ToInt();
		if (duration < 30000 || duration > 1200000 || System.GetTickCount(m_iServerPerformanceStarted) < duration) return;
		m_bServerPerformanceClosed = true;
		Print("[AICF][SERVER_PERFORMANCE_DONE] scope=ISOLATED_TIMING_ONLY normal_shutdown_requested=1");
		GetGame().RequestClose();
	}
}
