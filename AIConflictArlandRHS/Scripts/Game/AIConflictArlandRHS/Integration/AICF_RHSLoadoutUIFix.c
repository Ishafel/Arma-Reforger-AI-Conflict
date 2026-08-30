// The RHS Campaign_* player loadouts have editable UI info without a faction
// reference. Vanilla SCR_LoadoutButton dereferences that optional faction when
// painting the badge, which aborts deployment-menu population with a VM error.
// Preserve the icon and leave only the unavailable badge hidden.
modded class SCR_LoadoutButton
{
	override void SetLoadout(SCR_BasePlayerLoadout loadout)
	{
		m_Loadout = loadout;
		if (!loadout)
			return;

		SCR_EditableEntityUIInfo entityUIInfo = GetUIInfo();
		if (!entityUIInfo)
			return;

		string iconPath = entityUIInfo.GetIconPath();
		if (iconPath.IsEmpty())
			SetImage(entityUIInfo.GetImageSetPath(), entityUIInfo.GetIconSetName());
		else
			SetImage(iconPath);

		Faction faction = entityUIInfo.GetFaction();
		if (!m_wBadge || !faction)
			return;

		m_wBadge.SetOpacity(1.00);
		m_wBadge.SetColor(faction.GetFactionColor());
	}
}
