// Только Arland integration stage. Изолирует stock teardown от AICF bootstrap.
// -aicfBridgeProbe native|native-settled|native-ended
modded class SCR_GameModeCampaign
{
	override protected string AICF_ResolveSupportedMapKey()
	{
		string mode;
		if (System.GetCLIParam("aicfBridgeProbe", mode)) return string.Empty;
		return super.AICF_ResolveSupportedMapKey();
	}

	override void OnGameStart()
	{
		super.OnGameStart();
		string mode;
		if (!Replication.IsServer() || !System.GetCLIParam("aicfBridgeProbe", mode)) return;
		int delay;
		if (mode != "native") delay = 10000;
		Print(string.Format("[AICF][BRIDGE_LIFECYCLE_PROBE] mode=%1 bootstrap=DISABLED delay=%2", mode, delay));
		GetGame().GetCallqueue().CallLater(AICF_BridgeProbeFinish, delay, false);
	}

	protected void AICF_BridgeProbeFinish()
	{
		string mode;
		System.GetCLIParam("aicfBridgeProbe", mode);
		if (mode == "native-ended" && IsRunning())
			EndGameMode(SCR_GameModeEndData.CreateSimple(EGameOverTypes.FACTION_DRAW));
		GetGame().GetCallqueue().CallLater(AICF_BridgeProbeClose, 1000, false);
	}

	protected void AICF_BridgeProbeClose()
	{
		Print("[AICF][BRIDGE_LIFECYCLE_PROBE] finished=1");
		GetGame().RequestClose();
	}

	override void OnGameEnd()
	{
		GetGame().GetCallqueue().Remove(AICF_BridgeProbeFinish);
		GetGame().GetCallqueue().Remove(AICF_BridgeProbeClose);
		super.OnGameEnd();
	}
}
