// Одноразовый intent хозяина переживает native scripts/addons reload в памяти процесса.
class AICF_ScenarioEnvironment
{
	static const string SESSION_KEY = "AICF_ScenarioEnvironment";
	int m_iTime;
	int m_iWeather;

	bool IsValid() { return m_iTime >= 0 && m_iTime <= 4 && m_iWeather >= 0 && m_iWeather <= 4; }

	static bool Queue(ResourceName target, AICF_ScenarioEnvironment settings)
	{
		Clear();
		if (!settings || !settings.IsValid() || target.IsEmpty()) return false;
		JsonSaveContext json = new JsonSaveContext();
		json.WriteValue("schema", 1);
		json.WriteValue("target", target);
		json.WriteValue("issued", System.GetTickCount());
		json.WriteValue("time", settings.m_iTime);
		json.WriteValue("weather", settings.m_iWeather);
		GameSessionStorage.s_Data[SESSION_KEY] = json.SaveToString();
		return !GameSessionStorage.s_Data[SESSION_KEY].IsEmpty();
	}

	static void Clear() { GameSessionStorage.s_Data.Remove(SESSION_KEY); }

	static AICF_ScenarioEnvironment Consume(ResourceName actual)
	{
		string data;
		if (!GameSessionStorage.s_Data.Find(SESSION_KEY, data)) return null;
		// Удаление до проверки: отказ, чужой сценарий и повторный старт не наследуют intent.
		Clear();
		JsonLoadContext json = new JsonLoadContext();
		AICF_ScenarioEnvironment settings = new AICF_ScenarioEnvironment();
		int schema, issued;
		string target;
		if (!json.LoadFromString(data) || !json.ReadValue("schema", schema) || schema != 1 ||
			!json.ReadValue("target", target) || target != actual ||
			!json.ReadValue("issued", issued) || !json.ReadValue("time", settings.m_iTime) ||
			!json.ReadValue("weather", settings.m_iWeather) || !settings.IsValid()) return null;
		int age = System.GetTickCount(issued);
		AICF_ScenarioRegistry registry = new AICF_ScenarioRegistry();
		if (age < 0 || age > 600000 || !registry.IsLeaf(actual)) return null;
		return settings;
	}

	int GetHour()
	{
		int hours[4] = {8, 12, 18, 0};
		if (m_iTime < 0 || m_iTime >= 4) return 8;
		return hours[m_iTime];
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

modded class ArmaReforgerScripted
{
	override protected void OnMissionSet(MissionHeader mission)
	{
		super.OnMissionSet(mission);
		ResourceName actual;
		if (mission) actual = mission.GetHeaderResourceName();
		AICF_ScenarioEnvironment settings = AICF_ScenarioEnvironment.Consume(actual);
		SCR_MissionHeader header = SCR_MissionHeader.Cast(mission);
		if (header) header.m_AICFEnvironment = settings;
	}
}
