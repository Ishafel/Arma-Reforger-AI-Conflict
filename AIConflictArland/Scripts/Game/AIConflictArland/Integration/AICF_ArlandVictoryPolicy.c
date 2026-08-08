// Stage 1 uses ticket exhaustion; suppress stock Conflict's territorial countdown while this integration is loaded.
modded class SCR_GameModeCampaign
{
	override protected void CheckForWinner()
	{
		// Intentionally empty. AICF_VictorySystem calls EndGameMode exactly once.
	}
}
