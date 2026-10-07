// Только staging addon и canonical terminal launcher. Player/UI/JIP не имитирует.
class AICF_PersonalDeniedProbeCatalog : AICF_LoadoutCatalog
{
	override bool IsDenied(ResourceName prefab) { return true; }
}

modded class AICF_MatchController
{
	protected bool m_bAICFPersonalProbeDone;
	protected int m_iAICFPersonalProbeFailures;

	protected void AICF_PersonalCheck(string rule, bool passed)
	{
		if (!passed) m_iAICFPersonalProbeFailures++;
		Print(string.Format("[AICF][PERSONAL_LOADOUT_PROBE] rule=%1 passed=%2", rule, passed));
	}

	override protected void Update()
	{
		super.Update();
		string enabled;
		if (!System.GetCLIParam("aicfPersonalLoadoutProbe", enabled) || enabled != "1" || !m_bRosterReady || m_bAICFPersonalProbeDone) return;
		m_bAICFPersonalProbeDone = true;
		AICF_PersonalCheck("PATH_TRAVERSAL_REJECTED", !AICF_PersonalLoadoutStore.SafeKey("../other") && !AICF_PersonalLoadoutStore.SafeKey("a/b") && !AICF_PersonalLoadoutStore.SafeKey(""));
		AICF_PersonalProbeFaction(m_USFaction);
		AICF_PersonalProbeFaction(m_USSRFaction);
		Print(string.Format("[AICF][PERSONAL_LOADOUT_PROBE_DONE] failures=%1", m_iAICFPersonalProbeFailures));
		GetGame().RequestClose();
	}

	protected void AICF_PersonalProbeFaction(SCR_CampaignFaction faction)
	{
		AICF_LoadoutRecipe recipe = new AICF_LoadoutRecipe();
		string role, reason;
		recipe.m_sCharacter = m_GroupSpawner.ResolveRecruitPrefab(faction, 0, role);
		recipe.m_sProfile = AICF_ContentProfile.GetActive().GetProfileKey();
		recipe.m_sFaction = AICF_ContentProfile.GetActive().GetStableFactionKey(faction.GetFactionKey());
		recipe.m_sName = "Personal probe";
		string before = recipe.Encode();
		AICF_PersonalSessionStore session = new AICF_PersonalSessionStore();
		AICF_PersonalSessionStore otherPlayer = new AICF_PersonalSessionStore();
		int sessionRevision;
		AICF_PersonalCheck("SESSION_SAVE", session.Save(recipe, recipe, 1));
		AICF_LoadoutRecipe sessionCopy = session.Load(recipe, sessionRevision);
		AICF_PersonalCheck("SESSION_READBACK", sessionCopy && sessionRevision == 1 && sessionCopy.Encode() == before);
		AICF_PersonalCheck("SESSION_OWNER_ISOLATION", !otherPlayer.Load(recipe, sessionRevision) && sessionRevision == 0);
		AICF_PersonalCheck("SESSION_STALE_REVISION", !session.Save(recipe, recipe, 1));
		AICF_LoadoutRecipe foreign = AICF_LoadoutRecipe.Decode(before);
		foreign.m_sProfile = "OTHER";
		AICF_PersonalCheck("SESSION_CONTEXT_ISOLATION", !session.Load(foreign, sessionRevision) && !session.Save(recipe, foreign, 2));
		sessionCopy.m_sName = "mutated copy";
		AICF_PersonalCheck("SESSION_IMMUTABLE_COPY", session.Load(recipe, sessionRevision).Encode() == before);
		AICF_LoadoutBinding binding = AICF_LoadoutService.Validate(recipe, faction, reason, true);
		AICF_PersonalCheck("VALID_RECIPE_" + recipe.m_sFaction, binding != null);
		Print(string.Format("[AICF][PERSONAL_LOADOUT_PROBE_DETAIL] side=%1 reason=%2", recipe.m_sFaction, reason));
		AICF_PersonalCheck("SOURCE_UNCHANGED", recipe.Encode() == before);
		AICF_LoadoutRecipe wrong = AICF_LoadoutRecipe.Decode(before);
		wrong.m_sFaction = "OTHER";
		AICF_PersonalCheck("WRONG_SIDE_REJECTED", !AICF_PersonalLoadout.SameContext(wrong, recipe));
		wrong = AICF_LoadoutRecipe.Decode(before);
		wrong.m_sProfile = "OTHER";
		AICF_PersonalCheck("WRONG_PROFILE_REJECTED", !AICF_PersonalLoadout.SameContext(wrong, recipe));
		wrong = AICF_LoadoutRecipe.Decode(before);
		wrong.m_sCharacter = "OTHER";
		AICF_PersonalCheck("WRONG_ROLE_REJECTED", !AICF_PersonalLoadout.SameContext(wrong, recipe));
		wrong = AICF_LoadoutRecipe.Decode(before);
		wrong.Add("", "EquipedWeaponStorageComponent", 0, "{0000000000000000}forbidden.et", 1);
		AICF_PersonalCheck("FOREIGN_ITEM_REJECTED", !AICF_LoadoutService.Validate(wrong, faction, reason, true) && reason == "ITEM_NOT_ALLOWED");
		AICF_LoadoutCatalog catalog = new AICF_LoadoutCatalog(faction);
		AICF_LoadoutDraft draft = new AICF_LoadoutDraft();
		bool built = draft.Build(recipe, catalog, reason);
		AICF_PersonalDeniedProbeCatalog denied = new AICF_PersonalDeniedProbeCatalog(faction);
		AICF_PersonalCheck("NESTED_BLACKLIST_REJECTED", built && !AICF_LoadoutInventory.AllowsPersonalInventory(draft.GetCharacter(), denied));
		draft.Clear();
		string path = AICF_PersonalLoadoutStore.DIRECTORY + "/probe_" + recipe.m_sFaction;
		AICF_PersonalCheck("SAVE_FIRST", AICF_PersonalLoadoutStore.Save(path, recipe, 1));
		int revision;
		AICF_LoadoutRecipe restored = AICF_PersonalLoadoutStore.Load(path, revision);
		AICF_PersonalCheck("RELOAD_FIRST", restored && revision == 1 && restored.Encode() == before);
		recipe.m_sName = "Edited personal probe";
		AICF_PersonalCheck("SAVE_SECOND", AICF_PersonalLoadoutStore.Save(path, recipe, 2));
		restored = AICF_PersonalLoadoutStore.Load(path, revision);
		AICF_PersonalCheck("RELOAD_LATEST", restored && revision == 2 && restored.Encode() == recipe.Encode());
		JsonSaveContext corrupt = new JsonSaveContext();
		corrupt.WriteValue("revision", 3);
		corrupt.WriteValue("recipe", "invalid");
		corrupt.SaveToFile(path + "_1.json");
		restored = AICF_PersonalLoadoutStore.Load(path, revision);
		AICF_PersonalCheck("INTERRUPTED_WRITE_RECOVERY", restored && revision == 2 && restored.Encode() == recipe.Encode());
	}
}
