// AICF завершает матч по тикетам ИЛИ всем неоспариваемым точкам без stock countdown.
modded class SCR_GameModeCampaign
{
	override protected void CheckForWinner()
	{
		// Intentionally empty. AICF_VictorySystem calls EndGameMode exactly once.
	}
}
