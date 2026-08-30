// AICF exposes the highest stock rank to both character- and XP-based callers.
// This removes rank admission gates without modifying serialized vanilla/RHS
// catalog data. Supply, faction, capacity, and authority checks remain owned by
// their stock systems.
modded class SCR_CharacterRankComponent
{
	override protected SCR_ECharacterRank GetCharacterRank()
	{
		return SCR_ECharacterRank.GENERAL;
	}
}

modded class SCR_PlayerXPHandlerComponent
{
	override SCR_ECharacterRank GetPlayerRankByXP()
	{
		return SCR_ECharacterRank.GENERAL;
	}
}
