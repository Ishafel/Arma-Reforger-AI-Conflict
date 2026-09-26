// Cleanup не требует каталогов game mode: они могут быть удалены раньше служб.
modded class SCR_BaseItemSupportStationComponent
{
	protected bool m_bAICFDeleting;

	override void OnDelete(IEntity owner)
	{
		m_bAICFDeleting = true;
		if (GetGame())
			GetGame().GetCallqueue().Remove(DelayedInit);
		super.OnDelete(owner);
	}

	protected override bool InitValidSetup()
	{
		if (m_bAICFDeleting)
			return GetSupportStationType() != ESupportStationType.NONE;
		return super.InitValidSetup();
	}
}
