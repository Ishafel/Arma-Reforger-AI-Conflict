// Centralized Stage 0 logging. Keep the prefix stable: the testing guide treats it as an interface.
class AICF_Diagnostics
{
	static const string PREFIX = "[AICF][STAGE0]";

	static void Info(string eventName, string message)
	{
		Print(string.Format("%1[INFO][%2] %3", PREFIX, eventName, message), LogLevel.NORMAL);
	}

	static void Warning(string eventName, string message)
	{
		Print(string.Format("%1[WARNING][%2] %3", PREFIX, eventName, message), LogLevel.WARNING);
	}

	static void Error(string eventName, string message)
	{
		Print(string.Format("%1[ERROR][%2] %3", PREFIX, eventName, message), LogLevel.ERROR);
	}

	static void Result(bool success, string message)
	{
		if (success)
			Print(string.Format("%1[RESULT][PASS] %2", PREFIX, message), LogLevel.NORMAL);
		else
			Print(string.Format("%1[RESULT][FAIL] %2", PREFIX, message), LogLevel.ERROR);
	}

	static string DescribeBase(SCR_CampaignMilitaryBaseComponent base)
	{
		if (!base)
			return "<null-base>";

		string factionKey = "NONE";
		Faction faction = base.GetFaction();
		if (faction)
			factionKey = faction.GetFactionKey();

		return string.Format(
			"name=\"%1\" callsign=%2 type=%3 faction=%4",
			base.GetBaseName(),
			base.GetCallsign(),
			BaseTypeToString(base.GetType()),
			factionKey);
	}

	static string BaseTypeToString(SCR_ECampaignBaseType baseType)
	{
		switch (baseType)
		{
			case SCR_ECampaignBaseType.BASE:
				return "BASE";
			case SCR_ECampaignBaseType.RELAY:
				return "RELAY";
			case SCR_ECampaignBaseType.SOURCE_BASE:
				return "SOURCE_BASE";
		}

		return "UNKNOWN";
	}
}
