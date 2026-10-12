// Только отдельный Core stage: -aicfWeatherProbe baseline|random.
// Наблюдение штатной инициализации погоды без подмены PRNG, часов или переходов.
class AICF_WeatherProbeState
{
	static bool s_bCapture;
	static int s_iRequests;
	static string s_sSelected;
	static int s_iChecks;
	static int s_iFailures;

	static void Check(string rule, bool passed)
	{
		s_iChecks++;
		if (!passed) s_iFailures++;
		Print(string.Format("[AICF][WEATHER_CHECK] rule=%1 passed=%2", rule, passed));
	}
}

modded class TimeAndWeatherManagerEntity
{
	override void ForceWeatherTo(bool setLooping, string weatherID = string.Empty, float transitionDuration = 0, float stateDuration = 0.001, int playerThatChangedWeather = 0)
	{
		if (AICF_WeatherProbeState.s_bCapture)
		{
			AICF_WeatherProbeState.s_iRequests++;
			AICF_WeatherProbeState.s_sSelected = weatherID;
			array<ref WeatherState> states = {};
			GetWeatherStatesList(states);
			Print(string.Format("[AICF][WEATHER_SELECTED] weather=%1 candidates=%2 looping=%3 state_hours=%4", weatherID, states.Count(), setLooping, stateDuration));
		}
		super.ForceWeatherTo(setLooping, weatherID, transitionDuration, stateDuration, playerThatChangedWeather);
	}
}

modded class SCR_TimeAndWeatherHandlerComponent
{
	override void SetupDaytimeAndWeather(int hours, int minutes, int seconds = 0, string loadedWeatherState = "", bool loadDone = false)
	{
		string mode;
		if (!Replication.IsServer() || !System.GetCLIParam("aicfWeatherProbe", mode))
		{
			super.SetupDaytimeAndWeather(hours, minutes, seconds, loadedWeatherState, loadDone);
			return;
		}
		ChimeraWorld world = ChimeraWorld.CastFrom(GetOwner().GetWorld());
		TimeAndWeatherManagerEntity manager = world.GetTimeAndWeatherManager();
		int yearBefore, monthBefore, dayBefore;
		manager.GetDate(yearBefore, monthBefore, dayBefore);
		AICF_WeatherProbeState.s_bCapture = true;
		super.SetupDaytimeAndWeather(hours, minutes, seconds, loadedWeatherState, loadDone);
		AICF_WeatherProbeState.s_bCapture = false;
		int yearAfter, monthAfter, dayAfter;
		manager.GetDate(yearAfter, monthAfter, dayAfter);
		SCR_MissionHeader header = SCR_MissionHeader.Cast(GetGame().GetMissionHeader());
		bool random = mode == "random";
		int requests;
		if (random) requests = 1;
		float expectedTime = header.m_iStartingHours + header.m_iStartingMinutes / 60.0;
		AICF_WeatherProbeState.Check("CONFIGURED_TIME", manager.GetTimeOfTheDay() == expectedTime && !m_bRandomStartingDaytime && !m_bRandomStartingDaytimeRange && !m_bUsePredefineStartingTimeAndWeather);
		AICF_WeatherProbeState.Check("DAY_DURATION_CONFIG", manager.GetDayDuration() == 86400.0 / header.m_fDayTimeAcceleration && header.m_fDayTimeAcceleration == 6 && header.m_fNightTimeAcceleration == 24);
		AICF_WeatherProbeState.Check("AUTO_ADVANCE", manager.GetIsDayAutoAdvanced());
		AICF_WeatherProbeState.Check("DATE_UNCHANGED", yearBefore == yearAfter && monthBefore == monthAfter && dayBefore == dayAfter);
		AICF_WeatherProbeState.Check("HEADER_WEATHER", header.m_bRandomStartingWeather == random);
		AICF_WeatherProbeState.Check("START_REQUESTS", AICF_WeatherProbeState.s_iRequests == requests);
		AICF_WeatherProbeState.Check("WEATHER_PROGRESS_ENABLED", !manager.IsWeatherLooping());
		if (random)
			AICF_WeatherProbeState.Check("APPLIED_STATE", manager.GetCurrentWeatherState().GetStateName() == AICF_WeatherProbeState.s_sSelected);
		Print(string.Format("[AICF][WEATHER_CLOCK] configured=%1 actual=%2 day_seconds=%3 auto_advance=%4", expectedTime, manager.GetTimeOfTheDay(), manager.GetDayDuration(), manager.GetIsDayAutoAdvanced()));
	}
}

modded class AICF_MatchController
{
	protected bool m_bAICFWeatherObserved;
	protected bool m_bAICFWeatherDone;
	protected float m_fAICFWeatherTime;
	protected float m_fAICFWeatherEngineTime;
	protected float m_fAICFWeatherDuration;

	override protected void Update()
	{
		super.Update();
		string mode;
		if (m_bAICFWeatherDone || !m_bRosterReady || !System.GetCLIParam("aicfWeatherProbe", mode)) return;
		ChimeraWorld world = ChimeraWorld.CastFrom(GetGame().GetWorld());
		TimeAndWeatherManagerEntity manager = world.GetTimeAndWeatherManager();
		if (!m_bAICFWeatherObserved)
		{
			m_bAICFWeatherObserved = true;
			m_fAICFWeatherTime = manager.GetTimeOfTheDay();
			m_fAICFWeatherEngineTime = manager.GetEngineTime();
			m_fAICFWeatherDuration = manager.GetDayDuration();
			AICF_WeatherProbeState.Check("CLOCK_RUNNING", manager.GetIsDayAutoAdvanced() && m_fAICFWeatherDuration > 0);
			string headers;
			System.GetCLIParam("aicfWeatherHeaders", headers);
			array<string> paths = {};
			headers.Split(";", paths, true);
			bool headersValid = !paths.IsEmpty();
			foreach (string path : paths)
			{
				SCR_MissionHeader header = SCR_MissionHeader.Cast(MissionHeader.ReadMissionHeader(path));
				bool valid = header && header.m_bOverrideScenarioTimeAndWeather && header.m_bRandomStartingWeather == (mode == "random") && !header.m_bRandomStartingDaytime && header.m_iStartingHours == 8 && header.m_iStartingMinutes == 0 && header.m_fDayTimeAcceleration == 6 && header.m_fNightTimeAcceleration == 24;
				if (mode == "random" && header) valid = valid && header.m_bRandomWeatherChanges;
				if (!valid) headersValid = false;
				Print(string.Format("[AICF][WEATHER_HEADER] path=%1 passed=%2", path, valid));
			}
			AICF_WeatherProbeState.Check("HEADER_INHERITANCE", headersValid);
			int requestsBefore = AICF_WeatherProbeState.s_iRequests;
			Start(m_Campaign, m_Config, m_ContentProfile);
			AICF_WeatherProbeState.Check("DUPLICATE_START", requestsBefore == AICF_WeatherProbeState.s_iRequests && m_fAICFWeatherTime == manager.GetTimeOfTheDay());
			return;
		}
		// Синхронный clock менеджера исключает рассогласование фаз world/weather update.
		float elapsedSeconds = manager.GetEngineTime() - m_fAICFWeatherEngineTime;
		if (elapsedSeconds < 10) return;
		float advancedHours = manager.GetTimeOfTheDay() - m_fAICFWeatherTime;
		if (advancedHours < 0) advancedHours += 24;
		float expectedHours = elapsedSeconds * 24.0 / m_fAICFWeatherDuration;
		AICF_WeatherProbeState.Check("DAY_DURATION_STABLE", manager.GetDayDuration() == m_fAICFWeatherDuration);
		AICF_WeatherProbeState.Check("CLOCK_PROGRESSION", advancedHours > 0 && Math.AbsFloat(advancedHours - expectedHours) < 0.0001);
		Print(string.Format("[AICF][WEATHER_PROGRESS] engine_seconds=%1 advanced_hours=%2 expected_hours=%3", elapsedSeconds, advancedHours, expectedHours));
		Print(string.Format("[AICF][WEATHER_DONE] mode=%1 checks=%2 failures=%3 requests=%4 weather=%5", mode, AICF_WeatherProbeState.s_iChecks, AICF_WeatherProbeState.s_iFailures, AICF_WeatherProbeState.s_iRequests, AICF_WeatherProbeState.s_sSelected));
		m_bAICFWeatherDone = true;
		GetGame().RequestClose();
	}
}
