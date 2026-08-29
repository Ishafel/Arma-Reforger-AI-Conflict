// Stage 4 shares the authoritative Stage 1 run id and elapsed clock.
class AICF_Stage4Diagnostics
{
	static const string PREFIX = "[AICF][STAGE4]";
	protected static bool s_bHadError;

	static void Configure()
	{
		s_bHadError = false;
	}

	static bool HasErrors()
	{
		return s_bHadError;
	}

	static void Info(string eventName, string message)
	{
		message = AICF_ContentProfile.GetActive().NormalizeDiagnosticMessage(message);
		Print(string.Format(
			"%1[INFO][%2] run=%3 t_ms=%4 %5",
			PREFIX,
			eventName,
			AICF_Stage1Diagnostics.GetRunId(),
			AICF_Stage1Diagnostics.GetElapsedMs(),
			message), LogLevel.NORMAL);
	}

	static void Warning(string eventName, string message)
	{
		message = AICF_ContentProfile.GetActive().NormalizeDiagnosticMessage(message);
		Print(string.Format(
			"%1[WARNING][%2] run=%3 t_ms=%4 %5",
			PREFIX,
			eventName,
			AICF_Stage1Diagnostics.GetRunId(),
			AICF_Stage1Diagnostics.GetElapsedMs(),
			message), LogLevel.WARNING);
	}

	static void Error(string eventName, string message)
	{
		s_bHadError = true;
		message = AICF_ContentProfile.GetActive().NormalizeDiagnosticMessage(message);
		Print(string.Format(
			"%1[ERROR][%2] run=%3 t_ms=%4 %5",
			PREFIX,
			eventName,
			AICF_Stage1Diagnostics.GetRunId(),
			AICF_Stage1Diagnostics.GetElapsedMs(),
			message), LogLevel.ERROR);
		AICF_Stage1Diagnostics.RecordExternalError("STAGE4", eventName, message);
	}

	static string TierToString(AICF_ESupplyNetworkTier tier)
	{
		return typename.EnumToString(AICF_ESupplyNetworkTier, tier);
	}
}
