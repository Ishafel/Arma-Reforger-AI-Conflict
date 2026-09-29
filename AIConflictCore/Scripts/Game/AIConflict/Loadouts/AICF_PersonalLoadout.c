// Личный рецепт не является binding отряда и не меняет native loadout роли.
class AICF_PersonalLoadout
{
	static const int SLOT = -2;

	static AICF_LoadoutRecipe Context(SCR_PlayerController player, out SCR_CampaignFaction faction)
	{
		if (!player) return null;
		faction = SCR_CampaignFaction.Cast(SCR_FactionManager.SGetPlayerFaction(player.GetPlayerId()));
		SCR_PlayerLoadoutComponent component = SCR_PlayerLoadoutComponent.Cast(player.FindComponent(SCR_PlayerLoadoutComponent));
		if (!faction || !component) return null;
		SCR_FactionPlayerLoadout loadout = SCR_FactionPlayerLoadout.Cast(component.GetLoadout());
		if (!loadout || SCR_PlayerArsenalLoadout.Cast(loadout) || loadout.GetFactionKey() != faction.GetFactionKey()) return null;
		AICF_ContentProfile profile = AICF_ContentProfile.GetActive();
		string side = profile.GetStableFactionKey(faction.GetFactionKey());
		if (side != "US" && side != "USSR") return null;
		AICF_LoadoutRecipe context = new AICF_LoadoutRecipe();
		context.m_sProfile = profile.GetProfileKey();
		context.m_sFaction = side;
		context.m_sCharacter = profile.GetPersonalLoadoutSource(faction, loadout.GetDefaultLoadoutResource());
		context.m_sName = "{AICF:AICF_UI_PersonalName}";
		if (context.m_sCharacter.IsEmpty()) return null;
		return context;
	}

	static bool SameContext(AICF_LoadoutRecipe recipe, AICF_LoadoutRecipe context)
	{
		return recipe && context && recipe.m_sProfile == context.m_sProfile &&
			recipe.m_sFaction == context.m_sFaction && recipe.m_sCharacter == context.m_sCharacter;
	}

	static void Request(SCR_PlayerController player, int token, int revision, int operation, string payload)
	{
		if (!Replication.IsServer() || !player) return;
		SCR_SpawnLockComponent lock = SCR_SpawnLockComponent.Cast(player.FindComponent(SCR_SpawnLockComponent));
		if (AICF_GroupRuntime.IsAliveCharacter(player.GetControlledEntity()) || (lock && lock.IsLocked(true)))
		{
			player.AICF_LoadoutResult(token, false, "SPAWN_IN_PROGRESS");
			return;
		}
		SCR_CampaignFaction faction;
		AICF_LoadoutRecipe context = Context(player, faction);
		if (!context)
		{
			player.AICF_SetPersonalState(string.Empty, false, false);
			player.AICF_LoadoutResult(token, false, "MATCH_OR_FACTION_UNAVAILABLE");
			return;
		}
		string key = context.Encode();
		if (player.AICF_PersonalContext() != key) player.AICF_SetPersonalState(key, false, false);
		string path = AICF_PersonalLoadoutStore.Path(player, context);
		int storedRevision;
		AICF_LoadoutRecipe stored = AICF_PersonalLoadoutStore.Load(path, storedRevision);
		bool available = SameContext(stored, context);
		if (available && operation == 0)
		{
			string invalidReason;
			available = AICF_LoadoutService.Validate(stored, faction, invalidReason, true) != null;
		}
		bool selected = player.AICF_PersonalSelected();
		bool accepted = operation == 0;
		string reason = "READY";
		AICF_LoadoutRecipe candidate = AICF_LoadoutRecipe.Decode(payload);
		if (operation == 4)
		{
			selected = false;
			accepted = true;
		}
		else if (operation == 1 || operation == 3)
		{
			reason = "REVISION_CONFLICT";
			if (revision == storedRevision && SameContext(candidate, context))
			{
				if (operation == 3) candidate = stored;
				reason = "INVALID_RECIPE";
				if (SameContext(candidate, context))
				{
					AICF_LoadoutBinding binding = AICF_LoadoutService.Validate(candidate, faction, reason, true);
					if (binding && operation == 3)
					{
						selected = true;
						accepted = true;
					}
					else if (binding)
					{
						reason = "LIBRARY_WRITE_FAILED";
						if (storedRevision < 2147483646 && AICF_PersonalLoadoutStore.Save(path, candidate, storedRevision + 1))
						{
							storedRevision++;
							stored = candidate;
							available = true;
							accepted = true;
						}
					}
				}
			}
		}
		else if (operation != 0) reason = "INVALID_REQUEST";
		if (accepted)
		{
			reason = "READY";
			if (operation == 1) reason = "SAVED";
		}
		if (!available) selected = false;
		player.AICF_SetPersonalState(key, available, selected);
		AICF_LoadoutRecipe view = context;
		if (available) view = stored;
		AICF_LoadoutCatalog catalog = new AICF_LoadoutCatalog(faction);
		catalog.SetPersonalRules();
		player.AICF_LoadoutView(SLOT, 0, storedRevision, view.Encode(), catalog.PersonalRulesSnapshot(), 0);
		player.AICF_LoadoutResult(token, accepted, reason);
	}

	// Перед AssignEntity_S повторно строим рецепт по текущему faction catalog.
	// Невалидный рецепт оставляет подготовленное штатное снаряжение нетронутым.
	static bool Apply(SCR_PlayerController player, IEntity entity)
	{
		if (!Replication.IsServer() || !player || !player.AICF_PersonalSelected()) return true;
		SCR_CampaignFaction faction;
		AICF_LoadoutRecipe context = Context(player, faction);
		if (!context || context.Encode() != player.AICF_PersonalContext()) return true;
		int revision;
		AICF_LoadoutRecipe recipe = AICF_PersonalLoadoutStore.Load(AICF_PersonalLoadoutStore.Path(player, context), revision);
		string reason = "INVALID_RECIPE";
		AICF_LoadoutBinding binding;
		if (SameContext(recipe, context)) binding = AICF_LoadoutService.Validate(recipe, faction, reason, true);
		if (!binding)
		{
			player.AICF_SetPersonalState(context.Encode(), false, false);
			Print(string.Format("[AICF][PERSONAL_LOADOUT_FALLBACK] player=%1 reason=%2", player.GetPlayerId(), reason));
			return true;
		}
		if (!entity || entity.GetWorld() != GetGame().GetWorld() || SCR_Faction.GetEntityFaction(entity) != faction) return false;
		RplComponent rpl = RplComponent.Cast(entity.FindComponent(RplComponent));
		CharacterControllerComponent controller = CharacterControllerComponent.Cast(entity.FindComponent(CharacterControllerComponent));
		if (!rpl || !rpl.IsMaster() || !controller || controller.IsPlayerControlled() || controller.IsDead()) return false;
		EntityID identity = entity.GetID();
		string before;
		if (!AICF_LoadoutInventory.Capture(entity, before)) return false;
		AICF_LoadoutCatalog catalog = new AICF_LoadoutCatalog(faction);
		int ignored;
		string signature = AICF_LoadoutInventory.Signature(entity, catalog, ignored);
		if (entity.GetID() != identity || controller.IsPlayerControlled() || !rpl.IsMaster()) return false;
		bool applied = AICF_LoadoutInventory.Restore(entity, binding.m_sInventory) &&
			AICF_LoadoutInventory.Signature(entity, catalog, ignored) == binding.m_sSignature;
		if (applied)
		{
			Print(string.Format("[AICF][PERSONAL_LOADOUT_APPLIED] player=%1 entity=%2 revision=%3", player.GetPlayerId(), identity, revision));
			return true;
		}
		player.AICF_SetPersonalState(context.Encode(), false, false);
		bool rollback = AICF_LoadoutInventory.Restore(entity, before) &&
			AICF_LoadoutInventory.Signature(entity, catalog, ignored) == signature;
		Print(string.Format("[AICF][PERSONAL_LOADOUT_FALLBACK] player=%1 reason=RESTORE_FAILED rollback=%2", player.GetPlayerId(), rollback));
		// При неудачном rollback native handler уничтожит невыданный персонаж.
		return rollback;
	}
}

modded class SCR_PlayerController
{
	[RplProp(condition: RplCondition.OwnerOnly)]
	protected string m_sAICFPersonalContext;
	[RplProp(condition: RplCondition.OwnerOnly)]
	protected bool m_bAICFPersonalAvailable;
	[RplProp(condition: RplCondition.OwnerOnly)]
	protected bool m_bAICFPersonalSelected;

	string AICF_PersonalContext() { return m_sAICFPersonalContext; }
	bool AICF_PersonalAvailable() { return m_bAICFPersonalAvailable; }
	bool AICF_PersonalSelected() { return m_bAICFPersonalSelected; }

	void AICF_SetPersonalState(string context, bool available, bool selected)
	{
		if (!Replication.IsServer() || (context == m_sAICFPersonalContext && available == m_bAICFPersonalAvailable && selected == m_bAICFPersonalSelected)) return;
		m_sAICFPersonalContext = context;
		m_bAICFPersonalAvailable = available;
		m_bAICFPersonalSelected = selected;
		Replication.BumpMe();
	}
}

modded class SCR_SpawnPointSpawnHandlerComponent
{
	override protected bool AssignEntity_S(SCR_SpawnRequestComponent requestComponent, IEntity entity, SCR_SpawnData data)
	{
		if (AICF_MatchController.GetActiveController() && !AICF_PersonalLoadout.Apply(SCR_PlayerController.Cast(requestComponent.GetPlayerController()), entity)) return false;
		return super.AssignEntity_S(requestComponent, entity, data);
	}
}
