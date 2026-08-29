// Extends the existing Arland bootstrap at its content-composition boundary.
// It does not add OnGameStart, a controller, subscriptions or repeating loops.
modded class SCR_GameModeCampaign
{
	override protected AICF_ContentProfile AICF_CreateContentProfile()
	{
		return new AICF_RHSContentProfile();
	}
}
