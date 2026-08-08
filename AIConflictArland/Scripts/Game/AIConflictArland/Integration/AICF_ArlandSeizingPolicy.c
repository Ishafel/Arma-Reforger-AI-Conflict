// Arland's AI-only match loop must be able to progress stock Conflict capture timers
// without requiring a player character inside each capture area.
modded class SCR_CampaignSeizingComponent
{
	override void OnPostInit(IEntity owner)
	{
		m_bCapturingRequiresPlayer = false;
		super.OnPostInit(owner);
	}
}
