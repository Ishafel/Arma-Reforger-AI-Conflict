// Только новый персонаж на spawn point, до AssignEntity_S. Possess existing AI
// и сохранённый arsenal loadout сюда не входят; действующие игроки не меняются.
class AICF_WCSPlayerEquipment
{
	static bool IsDefault(SCR_BasePlayerLoadout loadout)
	{
		SCR_FactionPlayerLoadout factionLoadout = SCR_FactionPlayerLoadout.Cast(loadout);
		if (!AICF_WCSRHSContentProfile.Cast(AICF_ContentProfile.GetActive()) ||
			!factionLoadout || SCR_PlayerArsenalLoadout.Cast(loadout)) return false;
		FactionKey faction = factionLoadout.GetFactionKey();
		return faction == "RHS_USAF" || faction == "RHS_AFRF";
	}

	static bool Eligible(SCR_ChimeraCharacter character, SCR_FactionPlayerLoadout loadout)
	{
		if (!Replication.IsServer() || !character || !IsDefault(loadout) ||
			character.GetWorld() != GetGame().GetWorld() || character.GetFactionKey() != loadout.GetFactionKey()) return false;
		RplComponent rpl = RplComponent.Cast(character.FindComponent(RplComponent));
		CharacterControllerComponent controller = character.GetCharacterController();
		return rpl && rpl.IsMaster() && controller && !controller.IsDead() && !controller.IsPlayerControlled();
	}

	static bool Apply(SCR_ChimeraCharacter character, SCR_FactionPlayerLoadout loadout)
	{
		if (!Eligible(character, loadout)) return false;
		SCR_CampaignFaction faction = SCR_CampaignFaction.Cast(SCR_Faction.GetEntityFaction(character));
		if (!faction) return false;
		AICF_GroupSpawner roster = new AICF_GroupSpawner();
		string role;
		ResourceName source = roster.ResolveRecruitPrefab(faction, 0, role);
		if (source.IsEmpty() || role != "SQUAD_LEADER") return false;
		EntityID identity = character.GetID();
		string before;
		if (!AICF_LoadoutInventory.Capture(character, before)) return false;
		AICF_LoadoutCatalog catalog = new AICF_LoadoutCatalog(faction);
		AICF_LoadoutRecipe recipe = new AICF_LoadoutRecipe();
		recipe.m_sCharacter = source;
		recipe.m_sFaction = AICF_ContentProfile.GetActive().GetStableFactionKey(faction.GetFactionKey());
		AICF_LoadoutDraft draft = new AICF_LoadoutDraft();
		string reason;
		string snapshot;
		int ignored;
		string expected;
		bool built = draft.Build(recipe, catalog, reason) &&
			AICF_WCSInfantryEquipment.ValidateWCS(draft.GetCharacter(), faction.GetFactionKey(), source) &&
			AICF_LoadoutInventory.Capture(draft.GetCharacter(), snapshot);
		if (built) expected = AICF_LoadoutInventory.Signature(draft.GetCharacter(), catalog, ignored);
		draft.Clear();
		string current;
		if (!built || expected.IsEmpty() || !Eligible(character, loadout) || character.GetID() != identity ||
			!AICF_LoadoutInventory.Capture(character, current) || current != before) return false;
		bool applied = AICF_LoadoutInventory.Restore(character, snapshot) &&
			AICF_LoadoutInventory.Signature(character, catalog, ignored) == expected &&
			AICF_WCSInfantryEquipment.ValidateWCS(character, faction.GetFactionKey(), source);
		if (!applied)
		{
			bool rollback = AICF_LoadoutInventory.Restore(character, before);
			Print(string.Format("[AICF][WCS][PLAYER_KIT_FAILED] entity=%1 rollback=%2", identity, rollback), LogLevel.ERROR);
			return false;
		}
		Print(string.Format("[AICF][WCS][PLAYER_KIT_APPLIED] entity=%1 faction=%2 source=%3 inventory_verified=1", identity, faction.GetFactionKey(), source));
		return true;
	}
}

modded class SCR_SpawnPointSpawnHandlerComponent
{
	override protected bool PrepareEntity_S(SCR_SpawnRequestComponent requestComponent, IEntity entity, SCR_SpawnData data)
	{
		if (!super.PrepareEntity_S(requestComponent, entity, data)) return false;
		if (!Replication.IsServer() || !requestComponent || !requestComponent.GetPlayerController()) return true;
		SCR_PlayerLoadoutComponent component = SCR_PlayerLoadoutComponent.Cast(requestComponent.GetPlayerController().FindComponent(SCR_PlayerLoadoutComponent));
		if (!component || !AICF_WCSPlayerEquipment.IsDefault(component.GetLoadout())) return true;
		return AICF_WCSPlayerEquipment.Apply(SCR_ChimeraCharacter.Cast(entity), SCR_FactionPlayerLoadout.Cast(component.GetLoadout()));
	}
}
