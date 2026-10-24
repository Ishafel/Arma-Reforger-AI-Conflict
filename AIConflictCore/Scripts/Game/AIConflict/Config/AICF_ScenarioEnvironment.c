// Value object; the atomic runtime selection owns serialization and lifecycle.
class AICF_ScenarioEnvironment
{
	static const int RANDOM_TIME = 1440;
	int m_iStartMinutes = 480;
	int m_iWeather;

	bool IsValid() { return m_iStartMinutes >= 0 && m_iStartMinutes <= RANDOM_TIME && m_iWeather >= 0 && m_iWeather <= 4; }

	int GetHour()
	{
		if (m_iStartMinutes < 0 || m_iStartMinutes >= RANDOM_TIME) return 8;
		return m_iStartMinutes / 60;
	}
	int GetMinute()
	{
		if (m_iStartMinutes < 0 || m_iStartMinutes >= RANDOM_TIME) return 0;
		return m_iStartMinutes % 60;
	}

	string GetWeatherName()
	{
		string names[5] = {"", "Clear", "Cloudy", "Overcast", "Rainy"};
		if (m_iWeather < 0 || m_iWeather > 4) return "";
		return names[m_iWeather];
	}
}

modded class SCR_MissionHeader
{
	// Transient, не поле ресурса и не состояние клиентского RPC.
	ref AICF_ScenarioEnvironment m_AICFEnvironment;
}
