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

modded class SCR_ArsenalComponent
{
	override void RefreshArsenal(bool init = false, SCR_Faction faction = null)
	{
		// До регистрации нет получателей RPC. Начальное состояние передаёт
		// штатный RplSave/RplLoad, включая актуальные types и modes.
		bool initializing = init || !Replication.FindItemId(this).IsValid();
		super.RefreshArsenal(initializing, faction);
	}
}
