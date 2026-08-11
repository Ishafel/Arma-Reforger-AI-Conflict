// Stage 3.5 shares the authoritative Stage 1 clock while keeping roster,
// operational-role and scale evidence separate from the Stage 3 trip contract.
class AICF_Stage35Diagnostics
{
	static const string PREFIX = "[AICF][STAGE3.5]";
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
		Print(string.Format(
			"%1[ERROR][%2] run=%3 t_ms=%4 %5",
			PREFIX,
			eventName,
			AICF_Stage1Diagnostics.GetRunId(),
			AICF_Stage1Diagnostics.GetElapsedMs(),
			message), LogLevel.ERROR);
		AICF_Stage1Diagnostics.RecordExternalError("STAGE3.5", eventName, message);
	}
}
