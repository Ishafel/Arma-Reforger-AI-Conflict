// Stage 2 reliability events share the Stage 1 run id and clock so a single
// server log can be correlated without timestamp heuristics.
class AICF_Stage2Diagnostics
{
	static const string PREFIX = "[AICF][STAGE2]";
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
		AICF_Stage1Diagnostics.RecordExternalError("STAGE2", eventName, message);
	}
}
