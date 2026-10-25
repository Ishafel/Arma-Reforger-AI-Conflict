// После EquipmentProbe, только isolated stage. Production service, живой AI,
// штатный armory и native receipts; GUI/input не используются.
class AICF_IncompleteRecipe
{
	static AICF_LoadoutRecipe Strip(AICF_LoadoutRecipe original, SCR_CampaignFaction faction, bool weapons)
	{
		AICF_LoadoutRecipe recipe = AICF_LoadoutRecipe.Decode(original.Encode());
		AICF_LoadoutDraft draft = new AICF_LoadoutDraft();
		AICF_LoadoutCatalog catalog = new AICF_LoadoutCatalog(faction);
		string reason;
		if (!draft.Build(recipe, catalog, reason)) return null;
		AICF_LoadoutNavigation tree = new AICF_LoadoutNavigation();
		tree.Build(draft.GetCharacter(), catalog);
		InventoryStorageManagerComponent manager = AICF_EquipmentInventory.Manager(draft.GetCharacter());
		for (int i = tree.m_Locations.Count() - 1; i >= 0; i--)
		{
			AICF_LoadoutLocation location = tree.m_Locations[i];
			for (int slot = location.m_Storage.GetSlotsCount() - 1; slot >= 0; slot--)
			{
				if (!AICF_LoadoutSlotView.VisibleSlot(location.m_Storage, slot)) continue;
				IEntity item = location.m_Storage.Get(slot);
				if (!item) continue;
				bool remove = item.FindComponent(BaseMagazineComponent) != null;
				if (remove && !weapons)
				{
					bool removable = manager.CanRemoveItemFromStorage(item, location.m_Storage);
					Print(string.Format("[AICF][INCOMPLETE_REMOVE_QUERY] prefab=%1 removable=%2", SCR_ResourceNameUtils.GetPrefabName(item), removable));
					if (!removable) continue;
				}
				if (weapons) remove = item.FindComponent(BaseWeaponComponent) != null;
				if (remove) recipe.Add(location.m_sPath, location.m_sStorage, slot, "", 1);
			}
		}
		return recipe;
	}
}

modded class AICF_EquipmentApplicator
{
	override protected bool RestoreNew(AICF_EquipmentMember member)
	{
		bool restored = super.RestoreNew(member);
		if (!restored)
		{
			AICF_LoadoutCatalog catalog = new AICF_LoadoutCatalog(member.m_Faction);
			int cost;
			Print("[AICF][INCOMPLETE_EXPECTED] " + member.m_Plan.m_sSignature);
			Print("[AICF][INCOMPLETE_ACTUAL] " + AICF_LoadoutInventory.Signature(member.m_Character, catalog, cost));
		}
		return restored;
	}
}

modded class AICF_MatchController
{
	protected int m_iIncompleteStarted;
	protected int m_iIncompleteCase;
	protected int m_iIncompleteAppliedAt;
	protected int m_iIncompleteChecks;
	protected int m_iIncompleteFailures;
	protected ref AICF_LoadoutBinding m_IncompleteBinding;
	protected bool m_bIncompleteLedgerChanged;
	protected bool m_bIncompleteFinished;

	protected void AICF_ICheck(string name, bool passed)
	{
		m_iIncompleteChecks++;
		if (!passed) m_iIncompleteFailures++;
		Print(string.Format("[AICF][INCOMPLETE_CHECK] rule=%1 passed=%2", name, passed));
	}

	protected void AICF_IMatrix(SCR_CampaignFaction faction)
	{
		string role, reason;
		ResourceName source = m_GroupSpawner.ResolveRecruitPrefab(faction, 0, role);
		AICF_LoadoutRecipe original = AICF_LoadoutService.DefaultRecipe(faction, source, 0);
		for (int kind; kind < 2; kind++)
		{
			string prefix = original.m_sFaction + "_" + kind.ToString();
			AICF_LoadoutRecipe recipe = AICF_IncompleteRecipe.Strip(original, faction, kind == 0);
			AICF_LoadoutBinding binding = AICF_LoadoutService.Validate(recipe, faction, reason);
			AICF_ICheck(prefix + "_VALIDATE", binding != null);
			AICF_EquipmentPlan plan = new AICF_EquipmentPlan();
			bool built = binding && plan.Build(source, faction, binding);
			if (built && kind == 1)
			{
				BaseMuzzleComponent muzzle = AICF_EquipmentInventory.Muzzle(plan.ModelWeapon(false));
				int magazineRounds = -1;
				if (muzzle && muzzle.GetMagazine()) magazineRounds = muzzle.GetMagazine().GetAmmoCount();
				Print(string.Format("[AICF][INCOMPLETE_AMMO] side=%1 target=%2 native=%3 magazine=%4 type=%5", original.m_sFaction, plan.m_iPrimaryRounds, muzzle.GetAmmoCount(), magazineRounds, muzzle.GetMuzzleType()));
			}
			AICF_ICheck(prefix + "_PLAN", built && plan.m_sFailureReason.IsEmpty() && plan.m_iPrimaryRounds == 0);
			AICF_ICheck(prefix + "_READBACK", built && plan.m_sSignature == binding.m_sSignature);
			AICF_EquipmentConfig config = new AICF_EquipmentConfig();
			AICF_ICheck(prefix + "_NO_AMMO_NEED", built && !config.Low(0, plan.m_iPrimaryRounds, false, false) && !config.Low(0, plan.m_iPrimaryRounds, false, true));
			plan = null;
			if (binding)
			{
				binding.m_sSignature = "INVALID";
				AICF_EquipmentPlan bad = new AICF_EquipmentPlan();
				AICF_ICheck(prefix + "_BAD_READBACK", !bad.Build(source, faction, binding) && bad.m_sFailureReason == "BINDING_SIGNATURE_MISMATCH");
			}
		}
	}

	protected bool AICF_ISet(AICF_GroupSlot slot)
	{
		string role, reason;
		ResourceName source = m_GroupSpawner.ResolveRecruitPrefab(m_USFaction, 0, role);
		AICF_LoadoutRecipe recipe = AICF_LoadoutService.DefaultRecipe(m_USFaction, source, 0);
		if (m_iIncompleteCase < 2) recipe = AICF_IncompleteRecipe.Strip(recipe, m_USFaction, m_iIncompleteCase == 0);
		m_IncompleteBinding = AICF_LoadoutService.Validate(recipe, m_USFaction, reason);
		bool assigned = m_IncompleteBinding && slot.SetLoadout(0, m_IncompleteBinding, slot.GetLoadoutRevision());
		if (assigned) m_IncompleteBinding = slot.GetLoadout(0);
		AICF_ICheck("LIVE_" + m_iIncompleteCase.ToString() + "_ASSIGN", assigned);
		return assigned;
	}

	protected void AICF_IClose()
	{
		m_bIncompleteFinished = true;
		if (m_EquipmentProbeService) m_EquipmentProbeService.Stop();
		AICF_ICheck("STOP", m_EquipmentProbeService && m_EquipmentProbeService.AICF_ProbeStopped());
		Print(string.Format("[AICF][INCOMPLETE_DONE] checks=%1 failures=%2 cases=%3", m_iIncompleteChecks, m_iIncompleteFailures, m_iIncompleteCase));
		AICF_EQClose();
	}

	override protected void Update()
	{
		super.Update();
		string flag;
		if (!System.GetCLIParam("aicfEquipmentProbe", flag) || flag != "incomplete" || !m_bRosterReady || m_bIncompleteFinished) return;
		int now = System.GetTickCount();
		if (!m_iIncompleteStarted)
		{
			m_iIncompleteStarted = now;
			string startCase;
			if (System.GetCLIParam("aicfIncompleteStartCase", startCase)) m_iIncompleteCase = startCase.ToInt();
			m_EquipmentService.Stop();
			m_Construction.Stop();
			AICF_IMatrix(m_USFaction);
			AICF_IMatrix(m_USSRFaction);
			m_EquipmentProbeState = new AICF_FactionState(m_USFaction.GetFactionKey(), m_Config);
			m_EquipmentProbeService = new AICF_EquipmentService(m_Campaign, m_OrderPlanner, m_ObjectiveGraph, m_TargetSelector, m_GroupSpawner);
			string diagnostic;
			if (System.GetCLIParam("aicfIncompletePlanOnly", diagnostic) && diagnostic == "1")
			{
				m_EquipmentProbeService.Stop();
				m_bIncompleteFinished = true;
				Print(string.Format("[AICF][INCOMPLETE_MATRIX_DONE] checks=%1 failures=%2", m_iIncompleteChecks, m_iIncompleteFailures));
				GetGame().RequestClose();
				return;
			}
			vector position = m_USFaction.GetMainBase().GetSpawnPoint().GetOrigin() + "65 0 20";
			AICF_EQPlaceArmory(m_USFaction.GetMainBase(), position);
			AICF_EQSpawn(m_USFaction, 0, position);
			return;
		}
		if (now - m_iIncompleteStarted > 300000) { AICF_ICheck("TIMEOUT", false); AICF_IClose(); return; }
		foreach (SCR_CampaignBuildingCompositionComponent building : m_aEquipmentProbeBuildings)
			if (building && !building.IsCompositionSpawned() && building.GetCompositionLayout())
				building.GetCompositionLayout().AddBuildingValue(building.GetCompositionLayout().GetToBuildValue());
		AICF_GroupSlot slot = m_EquipmentProbeState.GetSlot(0);
		if (!slot.IsCombatReady())
		{
			if (!slot.GetGroup() || slot.GetGroup().GetAgentsCount() != 1) return;
			slot.MarkReady();
			slot.RecordDeploymentMembers();
			int managed, recovered;
			m_ManagedAILODPolicy.KeepCaptureEligible(slot.GetGroup(), managed, recovered);
			m_OrderPlanner.AssignPlayerPointOrder(slot, m_USFaction, slot.GetRosterMember(0).GetOrigin(), false);
		}
		if (!m_IncompleteBinding && !AICF_ISet(slot)) { AICF_IClose(); return; }
		float supplies = m_USFaction.GetMainBase().GetSupplies();
		int tickets = m_USState.GetTickets();
		m_EquipmentProbeService.Update(m_EquipmentProbeState, m_USFaction, null, null, null, true);
		if (supplies != m_USFaction.GetMainBase().GetSupplies() || tickets != m_USState.GetTickets()) m_bIncompleteLedgerChanged = true;
		AICF_EquipmentSlot record = m_EquipmentProbeService.AICF_ProbeRecord(slot);
		if (!record || record.m_aMembers.IsEmpty()) return;
		AICF_EquipmentMember member = record.m_aMembers[0];
		if (slot.GetMemberLoadoutRevision(0) != m_IncompleteBinding.m_iRevision) return;
		string prefix = "LIVE_" + m_iIncompleteCase.ToString();
		if (!m_iIncompleteAppliedAt)
		{
			m_iIncompleteAppliedAt = now;
			AICF_LoadoutCatalog catalog = new AICF_LoadoutCatalog(m_USFaction);
			int cost;
			AICF_ICheck(prefix + "_SERVICE_PLAN", member.m_Plan && member.Current());
			AICF_ICheck(prefix + "_APPLIED", member.m_iRevision == m_IncompleteBinding.m_iRevision);
			AICF_ICheck(prefix + "_INVENTORY", AICF_LoadoutInventory.Signature(member.m_Character, catalog, cost) == m_IncompleteBinding.m_sSignature);
			AICF_ICheck(prefix + "_WEAPON", (AICF_EquipmentInventory.Weapon(member.m_Character, false) != null) == (m_iIncompleteCase != 0));
			AICF_ICheck(prefix + "_UNLOCKED", !CharacterControllerComponent.Cast(member.m_Character.FindComponent(CharacterControllerComponent)).GetDisableMovementControls());
		}
		if (now - m_iIncompleteAppliedAt < 15000) return;
		AICF_ICheck(prefix + "_NO_LOOP", !member.m_bPending && !record.m_Order && !slot.IsResupplyingEquipment());
		AICF_ICheck(prefix + "_LEDGER", !m_bIncompleteLedgerChanged);
		m_iIncompleteCase++;
		m_iIncompleteAppliedAt = 0;
		m_IncompleteBinding = null;
		if (m_iIncompleteCase >= 3) AICF_IClose();
	}
}
