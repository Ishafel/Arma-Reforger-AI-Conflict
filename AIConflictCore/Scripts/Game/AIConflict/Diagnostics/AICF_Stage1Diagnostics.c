// Stable server log contract for the infantry vertical slice.
class AICF_Stage1Diagnostics
{
	static const string PREFIX = "[AICF][STAGE1]";
	protected static string s_sRunId = "unconfigured";
	protected static int s_iStartTickMs;
	protected static bool s_bHadError;

	static void Configure(string runId)
	{
		s_sRunId = runId;
		if (s_sRunId.IsEmpty())
			s_sRunId = "unnamed";

		s_iStartTickMs = System.GetTickCount();
		s_bHadError = false;
	}

	static string GetRunId()
	{
		return s_sRunId;
	}

	static bool IsConfigured()
	{
		return s_sRunId != "unconfigured";
	}

	static int GetElapsedMs()
	{
		if (s_iStartTickMs <= 0)
			return 0;

		return System.GetTickCount(s_iStartTickMs);
	}

	static bool HasErrors()
	{
		return s_bHadError;
	}

	static void RecordExternalError(string source, string eventName, string message)
	{
		if (!IsConfigured())
			return;

		Error(
			"CORE_ERROR_BRIDGE",
			string.Format("source=%1 event=%2 detail=%3", source, eventName, message));
	}

	static void Info(string eventName, string message)
	{
		InfoAt(eventName, message, GetElapsedMs());
	}

	static void InfoAt(string eventName, string message, int elapsedMs)
	{
		message = AICF_ContentProfile.GetActive().NormalizeDiagnosticMessage(message);
		Print(string.Format("%1[INFO][%2] run=%3 t_ms=%4 %5", PREFIX, eventName, s_sRunId, elapsedMs, message), LogLevel.NORMAL);
	}

	static void Warning(string eventName, string message)
	{
		message = AICF_ContentProfile.GetActive().NormalizeDiagnosticMessage(message);
		Print(string.Format("%1[WARNING][%2] run=%3 t_ms=%4 %5", PREFIX, eventName, s_sRunId, GetElapsedMs(), message), LogLevel.WARNING);
	}

	static void Error(string eventName, string message)
	{
		s_bHadError = true;
		message = AICF_ContentProfile.GetActive().NormalizeDiagnosticMessage(message);
		Print(string.Format("%1[ERROR][%2] run=%3 t_ms=%4 %5", PREFIX, eventName, s_sRunId, GetElapsedMs(), message), LogLevel.ERROR);
	}

	static void Result(bool success, string message)
	{
		message = AICF_ContentProfile.GetActive().NormalizeDiagnosticMessage(message);
		if (success && s_bHadError)
		{
			success = false;
			message = string.Format("reason=PRIOR_STAGE1_ERROR %1", message);
		}

		if (success)
			Print(string.Format("%1[RESULT][PASS] run=%2 t_ms=%3 %4", PREFIX, s_sRunId, GetElapsedMs(), message), LogLevel.NORMAL);
		else
			Print(string.Format("%1[RESULT][FAIL] run=%2 t_ms=%3 %4", PREFIX, s_sRunId, GetElapsedMs(), message), LogLevel.ERROR);
	}

	static string RoleToString(AICF_EGroupRole role)
	{
		switch (role)
		{
			case AICF_EGroupRole.ATTACK:
				return "ATTACK";
			case AICF_EGroupRole.DEFEND:
				return "DEFEND";
			case AICF_EGroupRole.RESERVE:
				return "RESERVE";
		}

		return "UNKNOWN";
	}

	static string StateToString(AICF_EGroupSlotState state)
	{
		switch (state)
		{
			case AICF_EGroupSlotState.EMPTY:
				return "EMPTY";
			case AICF_EGroupSlotState.SPAWNING:
				return "SPAWNING";
			case AICF_EGroupSlotState.READY:
				return "READY";
			case AICF_EGroupSlotState.DESTROYED:
				return "DESTROYED";
			case AICF_EGroupSlotState.WAITING:
				return "WAITING";
		}

		return "UNKNOWN";
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
			AICF_Diagnostics.BaseTypeToString(base.GetType()),
			factionKey);
	}

	static string BaseKey(SCR_CampaignMilitaryBaseComponent base)
	{
		if (!base)
			return "NONE";

		return base.GetCallsign().ToString();
	}
}
