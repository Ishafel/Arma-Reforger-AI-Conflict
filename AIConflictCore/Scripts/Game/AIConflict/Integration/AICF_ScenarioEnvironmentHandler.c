// Native manager остаётся владельцем часов, ускорения и сетевой синхронизации.
modded class SCR_TimeAndWeatherHandlerComponent
{
	protected bool m_bAICFEnvironmentApplied;

	void AICF_ApplyRuntimeOptions(SCR_MissionHeaderCampaign header)
	{
		if (!Replication.IsServer() || !header || !header.m_AICFEnvironment) return;
		m_iStartingHours = header.m_iStartingHours;
		m_iStartingMinutes = header.m_iStartingMinutes;
		m_bRandomStartingDaytime = header.m_bRandomStartingDaytime;
		m_fDayTimeAcceleration = header.m_fDayTimeAcceleration;
		m_fNightTimeAcceleration = header.m_fNightTimeAcceleration;
		m_bRandomStartingWeather = header.m_bRandomStartingWeather;
		m_bRandomWeatherChanges = header.m_bRandomWeatherChanges;
	}

	override void SetupDaytimeAndWeather(int hours, int minutes, int seconds = 0, string loadedWeatherState = "", bool loadDone = false)
	{
		SCR_MissionHeader header = SCR_MissionHeader.Cast(GetGame().GetMissionHeader());
		if (!Replication.IsServer() || !header || !header.m_AICFEnvironment || loadDone || m_bSavedSettingApplied || !loadedWeatherState.IsEmpty())
		{
			super.SetupDaytimeAndWeather(hours, minutes, seconds, loadedWeatherState, loadDone);
			return;
		}
		if (m_bAICFEnvironmentApplied) return;
		ChimeraWorld world = ChimeraWorld.CastFrom(GetOwner().GetWorld());
		if (!world || !world.GetTimeAndWeatherManager()) return;
		TimeAndWeatherManagerEntity manager = world.GetTimeAndWeatherManager();
		AICF_ScenarioEnvironment settings = header.m_AICFEnvironment;
		if (!settings.IsValid())
		{
			super.SetupDaytimeAndWeather(hours, minutes, seconds, loadedWeatherState, loadDone);
			return;
		}
		m_bAICFEnvironmentApplied = true;
		m_bRandomStartingDaytime = settings.m_iStartMinutes == AICF_ScenarioEnvironment.RANDOM_TIME;
		m_bRandomStartingDaytimeRange = false;
		m_bUsePredefineStartingTimeAndWeather = false;
		if (!m_bRandomStartingDaytime)
		{
			hours = settings.GetHour();
			minutes = settings.GetMinute();
			seconds = 0;
		}
		string weather = settings.GetWeatherName();
		bool found = weather.IsEmpty();
		array<ref WeatherState> states = {};
		manager.GetWeatherStatesList(states);
		foreach (WeatherState state : states)
			if (state && state.GetStateName() == weather) found = true;
		if (!found)
		{
			Print(string.Format("[AICF][ENVIRONMENT_FALLBACK] requested=%1 reason=WEATHER_UNAVAILABLE", weather), LogLevel.WARNING);
			weather = "";
		}
		m_bRandomStartingWeather = weather.IsEmpty();
		super.SetupDaytimeAndWeather(hours, minutes, seconds, weather, false);
		string actual;
		WeatherState current = manager.GetCurrentWeatherState();
		if (current) actual = current.GetStateName();
		Print(string.Format("[AICF][ENVIRONMENT_APPLIED] time_minutes=%1 weather_mode=%2 hour=%3 weather=%4 authority=server", settings.m_iStartMinutes, settings.m_iWeather, manager.GetTimeOfTheDay(), actual));
	}
}
