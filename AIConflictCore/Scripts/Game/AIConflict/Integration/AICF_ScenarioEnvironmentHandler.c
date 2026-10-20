// Native manager остаётся владельцем часов, ускорения и сетевой синхронизации.
modded class SCR_TimeAndWeatherHandlerComponent
{
	protected bool m_bAICFEnvironmentApplied;

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
		m_bRandomStartingDaytime = settings.m_iTime == 4;
		m_bRandomStartingDaytimeRange = false;
		m_bUsePredefineStartingTimeAndWeather = false;
		if (!m_bRandomStartingDaytime)
		{
			hours = settings.GetHour();
			minutes = 0;
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
		Print(string.Format("[AICF][ENVIRONMENT_APPLIED] time_mode=%1 weather_mode=%2 hour=%3 weather=%4 authority=server", settings.m_iTime, settings.m_iWeather, manager.GetTimeOfTheDay(), actual));
	}
}
