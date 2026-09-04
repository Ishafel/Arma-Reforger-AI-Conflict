// AICF uses ticket exhaustion on supported stock maps; suppress stock Conflict's territorial countdown.
modded class SCR_GameModeCampaign
{
	override protected void CheckForWinner()
	{
		// Intentionally empty. AICF_VictorySystem calls EndGameMode exactly once.
	}
}
