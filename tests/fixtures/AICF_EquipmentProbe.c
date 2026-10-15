// Только isolated stage. Production policy, реальная навигация и native inventory readback.
modded class AICF_EquipmentService
{
	AICF_EquipmentSlot AICF_ProbeRecord(AICF_GroupSlot slot)
	{
		foreach (AICF_EquipmentSlot record : m_aSlots) if (record.m_Slot == slot) return record;
		return null;
	}
	void AICF_ProbeTick(AICF_EquipmentSlot record, int now) { Tick(record, now, true); }
	bool AICF_ProbeEvaluate(AICF_EquipmentSlot record) { return Evaluate(record); }
	bool AICF_ProbeSelect(AICF_EquipmentOrder order) { return SelectService(order); }
	bool AICF_ProbeCorpse(AICF_EquipmentOrder order) { return SelectCorpse(order); }
	void AICF_ProbeAdopt(AICF_EquipmentSlot record) { m_aSlots.Insert(record); }
	bool AICF_ProbeStopped() { return m_bStopped && m_aSlots.IsEmpty() && m_aPlans.IsEmpty() && m_aRecoveries.IsEmpty(); }
}

modded class AICF_VehicleCoordinator
{
	string AICF_ProbeEquipmentAdmission(AICF_GroupSlot slot) { return GetInfantryOrderAdmissionFenceReason(slot); }
	bool AICF_ProbeActiveTripFence(AICF_EquipmentService service, AICF_FactionState state, SCR_CampaignFaction faction, AICF_GroupSlot slot)
	{
		AICF_StrategicAssignmentSnapshot assignment;
		if (!m_OrderPlanner.TryCreateAssignmentSnapshot(slot, faction, 0, assignment)) return false;
		AICF_TransportTripRegistry existing = m_Trips;
		m_Trips = new AICF_TransportTripRegistry();
		int now = System.GetTickCount();
		AICF_TransportTrip trip = m_Trips.Create(assignment, now, now + 60000, "equipment-probe");
		AIWaypoint before = slot.GetWaypoint();
		service.Update(state, faction, null, null, this, true);
		AICF_EquipmentSlot record = service.AICF_ProbeRecord(slot);
		bool passed;
		if (record && trip && GetSlotView(slot))
		{
			service.AICF_ProbeTick(record, now + AICF_EquipmentConfig.QUIET_MS);
			passed = !record.m_Order && before == slot.GetWaypoint() && !trip.IsTerminal() && service.AICF_ProbeEvaluate(record);
		}
		m_Trips = existing;
		return passed;
	}
}

modded class AICF_EquipmentApplicator
{
	static bool s_bAICFRefuseRestore;
	static int s_iAICFRefused;
	static int s_iAICFRestoreCalls;
	override protected bool RestoreNew(AICF_EquipmentMember member)
	{
		s_iAICFRestoreCalls++;
		if (s_bAICFRefuseRestore)
		{
			s_bAICFRefuseRestore = false;
			s_iAICFRefused++;
			return false;
		}
		return super.RestoreNew(member);
	}
}

modded class AICF_EquipmentOrder
{
	protected static ref set<string> s_aAICFMissing = new set<string>();
	override bool CanIssue(AICF_EquipmentMember member)
	{
		bool result = super.CanIssue(member);
		if (!result && member && member.m_Plan)
		{
			string key = m_ArsenalId.ToString() + member.m_iRevision.ToString();
			if (!s_aAICFMissing.Contains(key))
			{
				s_aAICFMissing.Insert(key);
				array<ResourceName> available = {};
				if (m_Arsenal) m_Arsenal.GetAvailablePrefabs(available);
				Print(string.Format("[AICF][EQUIPMENT_CAPABILITY] has_service=%1 arsenal=%2 available=%3 required=%4", HasService(), m_ArsenalId, available.Count(), member.m_Plan.m_aRequired.Count()));
				foreach (ResourceName prefab : member.m_Plan.m_aRequired)
					if (!available.Contains(prefab)) Print("[AICF][EQUIPMENT_CAPABILITY_MISSING] prefab=" + prefab);
			}
		}
		return result;
	}
}

modded class AICF_MatchController
{
	protected ref AICF_FactionState m_EquipmentProbeState;
	protected ref AICF_EquipmentService m_EquipmentProbeService;
	protected ref array<SCR_AIGroup> m_aEquipmentProbeGroups = {};
	protected IEntity m_EquipmentProbeCorpse;
	protected IEntity m_EquipmentProbeItem;
	protected EntityID m_EquipmentProbeItemId;
	protected int m_iEquipmentProbePhase;
	protected int m_iEquipmentProbeStarted;
	protected int m_iEquipmentProbeChecks;
	protected int m_iEquipmentProbeFailures;
	protected int m_iEquipmentProbePhaseAt;
	protected vector m_vEquipmentProbeStart;
	protected float m_fEquipmentProbeSupplies;
	protected int m_iEquipmentProbeTickets;
	protected ref AICF_EquipmentOrder m_EquipmentProbeOldOrder;
	protected ref array<SCR_CampaignBuildingCompositionComponent> m_aEquipmentProbeBuildings = {};
	protected ref AICF_EquipmentMember m_EquipmentProbeOldMember;
	protected bool m_bEquipmentProbeFailureChecked;
	protected bool m_bEquipmentProbeFailureArmed;
	protected string m_sEquipmentProbeBeforeFailure;
	protected bool m_bEquipmentProbeRevisionChecked;
	protected int m_iEquipmentProbeApplied;
	protected int m_iEquipmentProbeDebugAt;
	protected bool m_bEquipmentProbeDefaultChecked;
	protected bool m_bEquipmentProbePartialChecked;
	protected bool m_bEquipmentProbeLedgerChanged;

	protected void AICF_EQCheck(string name, bool pass, string detail = "")
	{
		m_iEquipmentProbeChecks++;
		if (!pass) m_iEquipmentProbeFailures++;
		Print(string.Format("[AICF][EQUIPMENT_PROBE] case=%1 passed=%2 phase=%3 %4", name, pass, m_iEquipmentProbePhase, detail));
	}

	protected void AICF_EQSpawn(SCR_CampaignFaction faction, int slotId, vector position)
	{
		SCR_AIGroup group = m_GroupSpawner.SpawnGroup(faction, faction.GetMainBase(), slotId, 1, false);
		m_aEquipmentProbeGroups.Insert(group);
		AICF_EQCheck("SPAWN_REQUEST_" + slotId, group != null);
		if (!group) return;
		group.SetOrigin(position);
		group.SetLifecyclePolicy(SCR_EAIGroupLifecyclePolicy.Manual);
		group.SetDeleteWhenEmpty(false);
		AICF_GroupSlot slot = m_EquipmentProbeState.GetSlot(slotId);
		slot.SetDesiredSize(1);
		if (slotId == 1) AICF_EQBinding(slot, "spawn-assigned");
		slot.BeginInitialSpawn();
		slot.BindSpawnedGroup(group);
		group.AICF_TrackMemberPositions();
		m_GroupSpawner.BeginRosterSpawn(group, 1);
	}

	protected void AICF_EQDeplete(IEntity character)
	{
		BaseWeaponComponent weapon = AICF_EquipmentInventory.Weapon(character, false);
		array<IEntity> items = {};
		AICF_EquipmentInventory.Manager(character).GetItems(items);
		foreach (IEntity item : items)
		{
			BaseMagazineComponent magazine = BaseMagazineComponent.Cast(item.FindComponent(BaseMagazineComponent));
			if (magazine && weapon && AICF_EquipmentInventory.Compatible(AICF_EquipmentInventory.Muzzle(weapon), magazine)) magazine.SetAmmoCount(0);
		}
		if (weapon)
			for (int i; i < AICF_EquipmentInventory.Muzzle(weapon).GetBarrelsCount(); i++) AICF_EquipmentInventory.Muzzle(weapon).ClearChamber(i);
	}

	protected string AICF_EQInventoryIdentity(IEntity character)
	{
		array<IEntity> items = {};
		array<string> identity = {};
		AICF_EquipmentInventory.Manager(character).GetItems(items);
		foreach (IEntity item : items)
		{
			string id = item.GetID().ToString();
			if (!identity.Contains(id)) identity.Insert(id);
		}
		identity.Sort();
		return SCR_StringHelper.Join(";", identity) + string.Format(" rounds=%1 at=%2",
			AICF_EquipmentInventory.Rounds(character, AICF_EquipmentInventory.Weapon(character, false)),
			AICF_EquipmentInventory.Rounds(character, AICF_EquipmentInventory.Weapon(character, true)));
	}

	protected AICF_LoadoutBinding AICF_EQBinding(AICF_GroupSlot slot, string name)
	{
		string role;
		AICF_LoadoutRecipe recipe = new AICF_LoadoutRecipe();
		recipe.m_sCharacter = m_GroupSpawner.ResolveRecruitPrefab(m_USFaction, 0, role);
		recipe.m_sFaction = m_ContentProfile.GetStableFactionKey(m_USFaction.GetFactionKey());
		recipe.m_sProfile = m_ContentProfile.GetProfileKey();
		recipe.m_sName = name;
		string reason;
		AICF_LoadoutBinding binding = AICF_LoadoutService.Validate(recipe, m_USFaction, reason);
		AICF_EQCheck("BINDING_VALID_" + name, binding != null, "reason=" + reason);
		if (binding) slot.SetLoadout(0, binding, slot.GetLoadoutRevision());
		return binding;
	}

	protected void AICF_EQPlaceArmory(SCR_CampaignMilitaryBaseComponent base, vector position)
	{
		position[1] = GetGame().GetWorld().GetSurfaceY(position[0], position[2]);
		EntitySpawnParams params = new EntitySpawnParams();
		params.TransformMode = ETransformMode.WORLD;
		params.Transform[3] = position;
		SCR_EditorLinkComponent.IgnoreSpawning(true);
		IEntity entity = GetGame().SpawnEntityPrefabEx(m_ContentProfile.GetConstructionPrefab("US", AICF_EConstructionType.ARMORY), false, params: params);
		SCR_EditorLinkComponent.IgnoreSpawning(false);
		if (!entity) return;
		FactionAffiliationComponent affiliation = FactionAffiliationComponent.Cast(entity.FindComponent(FactionAffiliationComponent));
		if (affiliation) affiliation.SetAffiliatedFaction(m_USFaction);
		SCR_CampaignBuildingCompositionComponent building = SCR_CampaignBuildingCompositionComponent.Cast(entity.FindComponent(SCR_CampaignBuildingCompositionComponent));
		if (building)
		{
			building.SetProviderEntity(base.GetMasterProvider().GetOwner());
			m_aEquipmentProbeBuildings.Insert(building);
		}
	}

	protected void AICF_EQClose()
	{
		m_EquipmentProbeService.Stop();
		foreach (SCR_AIGroup group : m_aEquipmentProbeGroups)
		{
			if (!group) continue;
			group.DespawnMembers();
			RplComponent.DeleteRplEntity(group, false);
		}
		Print(string.Format("[AICF][EQUIPMENT_PROBE_FINISHED] checks=%1 failures=%2 phase=%3 profile=%4", m_iEquipmentProbeChecks, m_iEquipmentProbeFailures, m_iEquipmentProbePhase, m_ContentProfile.GetProfileKey()));
		m_iEquipmentProbePhase = 99;
		GetGame().RequestClose();
	}

	override protected void Update()
	{
		super.Update();
		string enabled;
		if (!System.GetCLIParam("aicfEquipmentProbe", enabled) || (enabled != "1" && enabled != "inventory") || !m_bRosterReady || m_iEquipmentProbePhase == 99) return;
		int now = System.GetTickCount();
		if (!m_iEquipmentProbeStarted)
		{
			m_iEquipmentProbeStarted = now;
			m_EquipmentService.Stop();
			m_Construction.Stop();
			m_EquipmentProbeState = new AICF_FactionState(m_USFaction.GetFactionKey(), m_Config);
			m_EquipmentProbeService = new AICF_EquipmentService(m_Campaign, m_OrderPlanner, m_ObjectiveGraph, m_TargetSelector, m_GroupSpawner);
			vector position = m_USFaction.GetMainBase().GetSpawnPoint().GetOrigin();
			if (enabled == "inventory")
			{
				position += "65 0 20";
				AICF_EQPlaceArmory(m_USFaction.GetMainBase(), position);
			}
			AICF_EQSpawn(m_USFaction, 0, position);
			AICF_EQSpawn(m_USFaction, 1, position + "5 0 5");
			AICF_EQSpawn(m_USFaction, 2, position + "25 0 0");
			AICF_EquipmentConfig config = new AICF_EquipmentConfig();
			AICF_EQCheck("THRESHOLD_ABOVE", !config.Low(26, 100, false, false));
			AICF_EQCheck("THRESHOLD_BELOW", config.Low(25, 100, false, false));
			AICF_EQCheck("ONE_SHOT", !config.Low(99, 100, false, false));
			AICF_EQCheck("HYSTERESIS", config.Low(74, 100, false, true) && !config.Low(75, 100, false, true));
			AICF_EQCheck("AT_EMPTY", config.Low(0, 1, true, false) && !config.Low(1, 1, true, false));
			AICF_EQCheck("RADIUS", AICF_EquipmentConfig.InRange("0 0 0", "3 0 0", 3) && !AICF_EquipmentConfig.InRange("0 0 0", "3.1 0 0", 3));
			return;
		}
		if (now - m_iEquipmentProbeStarted > 420000)
		{
			AICF_EQCheck("TIMEOUT", false);
			AICF_EQClose();
			return;
		}
		foreach (SCR_CampaignBuildingCompositionComponent building : m_aEquipmentProbeBuildings)
			if (building && !building.IsCompositionSpawned() && building.GetCompositionLayout())
				building.GetCompositionLayout().AddBuildingValue(building.GetCompositionLayout().GetToBuildValue());
		AICF_GroupSlot slot = m_EquipmentProbeState.GetSlot(0);
		if (m_iEquipmentProbePhase == 0)
		{
			foreach (SCR_AIGroup group : m_aEquipmentProbeGroups) if (!group || group.GetAgentsCount() != 1) return;
			for (int i; i < 3; i++)
			{
				AICF_GroupSlot ready = m_EquipmentProbeState.GetSlot(i);
				if (i == 1) AICF_EQCheck("SPAWN_ASSIGNED_APPLY", ready.ApplyDeploymentLoadouts(m_USFaction));
				ready.MarkReady();
				ready.RecordDeploymentMembers();
				if (i == 1)
				{
					AICF_LoadoutCatalog assignedCatalog = new AICF_LoadoutCatalog(m_USFaction);
					int assignedCost;
					AICF_EQCheck("SPAWN_ASSIGNED_INVENTORY", AICF_LoadoutInventory.Signature(ready.GetRosterMember(0), assignedCatalog, assignedCost) == ready.GetLoadout(0).m_sSignature &&
						ready.GetMemberLoadoutRevision(0) == ready.GetLoadout(0).m_iRevision);
				}
				int managed, recovered;
				m_ManagedAILODPolicy.KeepCaptureEligible(ready.GetGroup(), managed, recovered);
				m_OrderPlanner.AssignPlayerPointOrder(ready, m_USFaction, ready.GetRosterMember(0).GetOrigin(), false);
			}
			m_EquipmentProbeCorpse = m_EquipmentProbeState.GetSlot(2).GetRosterMember(0);
			AICF_EQCheck("LIVE_SOURCE_REFUSED", !AICF_EquipmentInventory.Corpse(m_EquipmentProbeCorpse));
			array<IEntity> items = {};
			AICF_EquipmentInventory.Manager(m_EquipmentProbeCorpse).GetItems(items);
			foreach (IEntity item : items)
			{
				BaseMagazineComponent magazine = BaseMagazineComponent.Cast(item.FindComponent(BaseMagazineComponent));
				if (magazine && magazine.GetAmmoCount() > 0 && magazine != AICF_EquipmentInventory.Weapon(m_EquipmentProbeCorpse, false).GetCurrentMagazine())
				{
					m_EquipmentProbeItem = item;
					m_EquipmentProbeItemId = item.GetID();
					break;
				}
			}
			SCR_CharacterDamageManagerComponent.Cast(m_EquipmentProbeCorpse.FindComponent(SCR_CharacterDamageManagerComponent)).Kill(Instigator.CreateInstigatorGM());
			if (enabled != "inventory") AICF_EQDeplete(slot.GetRosterMember(0));
			m_vEquipmentProbeStart = slot.GetRosterMember(0).GetOrigin();
			m_fEquipmentProbeSupplies = m_USFaction.GetMainBase().GetSupplies();
			m_iEquipmentProbeTickets = m_USState.GetTickets();
			m_iEquipmentProbePhase = 1;
			if (enabled == "inventory")
			{
				AICF_EQBinding(slot, "first");
				AICF_EQBinding(slot, "latest");
				m_iEquipmentProbePhase = 3;
			}
		}
		// Чужие штатные shipments/reinforcements продолжаются в мире. Сравниваем
		// ledger непосредственно вокруг каждой production service mutation.
		float supplyBeforeTick = m_USFaction.GetMainBase().GetSupplies();
		int ticketsBeforeTick = m_USState.GetTickets();
		m_EquipmentProbeService.Update(m_EquipmentProbeState, m_USFaction, null, null, null, true);
		if (supplyBeforeTick != m_USFaction.GetMainBase().GetSupplies() || ticketsBeforeTick != m_USState.GetTickets()) m_bEquipmentProbeLedgerChanged = true;
		AICF_EquipmentSlot record = m_EquipmentProbeService.AICF_ProbeRecord(slot);
		if (!record || record.m_aMembers.IsEmpty()) return;
		AICF_EquipmentMember member = record.m_aMembers[0];
		if (member.m_Plan && !m_bEquipmentProbeDefaultChecked)
		{
			m_bEquipmentProbeDefaultChecked = true;
			AICF_EQCheck("SPAWN_DEFAULT_WEAPON", AICF_EquipmentInventory.Weapon(member.m_Character, false) &&
				SCR_ResourceNameUtils.GetPrefabName(AICF_EquipmentInventory.Weapon(member.m_Character, false).GetOwner()) ==
				SCR_ResourceNameUtils.GetPrefabName(member.m_Plan.ModelWeapon(false).GetOwner()));
		}
		if (m_iEquipmentProbePhase == 3 && now >= m_iEquipmentProbeDebugAt)
		{
			m_iEquipmentProbeDebugAt = now + 10000;
			array<SCR_ServicePointComponent> services = {};
			m_USFaction.GetMainBase().GetServices(services);
			Print(string.Format("[AICF][EQUIPMENT_PROBE_WAIT] services=%1 buildings=%2 can_start=%3 quiet=%4 pending=%5 target_rounds=%6",
				services.Count(), m_aEquipmentProbeBuildings.Count(), m_OrderPlanner.CanEquipmentService(slot, m_USFaction), AICF_EquipmentInventory.Quiet(slot.GetGroup()), member.m_bPending, member.m_Plan.m_iPrimaryRounds));
			foreach (SCR_ServicePointComponent service : services)
				Print(string.Format("[AICF][EQUIPMENT_PROBE_SERVICE] id=%1 type=%2 state=%3 position=%4", service.GetOwner().GetID(), service.GetType(), service.GetServiceState(), service.GetOwner().GetOrigin()));
		}
		if (m_bEquipmentProbeFailureArmed && !m_bEquipmentProbeFailureChecked && AICF_EquipmentApplicator.s_iAICFRefused > 0 && !member.m_Transaction)
		{
			AICF_EQCheck("LIVE_ISSUE_ROLLBACK", AICF_EquipmentInventory.SafeMember(member.m_Character, member.m_Group) &&
				AICF_EQInventoryIdentity(member.m_Character) == m_sEquipmentProbeBeforeFailure && slot.GetMemberLoadoutRevision(0) != member.m_iRevision &&
				!CharacterControllerComponent.Cast(member.m_Character.FindComponent(CharacterControllerComponent)).GetDisableMovementControls());
			Print("[AICF][EQUIPMENT_PROBE_ROLLBACK] phase=BEFORE " + m_sEquipmentProbeBeforeFailure);
			Print("[AICF][EQUIPMENT_PROBE_ROLLBACK] phase=AFTER " + AICF_EQInventoryIdentity(member.m_Character));
			m_bEquipmentProbeFailureChecked = true;
		}
		if (m_iEquipmentProbePhase == 1 && record.m_Order)
		{
			AICF_EQCheck("PLAN_DEFAULT", member.m_Plan && member.m_Plan.m_iPrimaryRounds > 0);
			AICF_EQCheck("CORPSE_SELECTED", record.m_Order.m_Corpse == m_EquipmentProbeCorpse);
			string refusal;
			AICF_EQCheck("NO_DISTANT_TRANSFER", !AICF_EquipmentInventory.Transfer(record.m_Order, member, m_EquipmentProbeCorpse,
				m_EquipmentProbeCorpse.GetID(), m_EquipmentProbeItem, m_EquipmentProbeItemId, refusal));
			m_iEquipmentProbePhase = 2;
		}
		if (m_iEquipmentProbePhase == 2 && AICF_EquipmentInventory.Manager(member.m_Character).Contains(m_EquipmentProbeItem))
		{
			if (!m_bEquipmentProbePartialChecked)
			{
			AICF_EQCheck("PHYSICAL_APPROACH", vector.Distance(member.m_Character.GetOrigin(), m_vEquipmentProbeStart) > 10 &&
				AICF_EquipmentConfig.InRange(member.m_Character.GetOrigin(), m_EquipmentProbeCorpse.GetOrigin(), 3));
			AICF_EQCheck("SAME_ITEM_IDENTITY", m_EquipmentProbeItem.GetID() == m_EquipmentProbeItemId &&
				!AICF_EquipmentInventory.Manager(m_EquipmentProbeCorpse).Contains(m_EquipmentProbeItem));
			AICF_EQCheck("ECONOMY_UNCHANGED", !m_bEquipmentProbeLedgerChanged);
			AICF_EQCheck("PARTIAL_LOOT_NEED_RETAINED", member.m_bPending && record.m_Order && !record.m_Order.m_Service);
			m_bEquipmentProbePartialChecked = true;
			}
			if (member.m_bPending || record.m_Order) return;
			AICF_EQCheck("SUFFICIENT_LOOT_CANCELS_VISIT", !slot.IsResupplyingEquipment() && AICF_EquipmentInventory.Rounds(member.m_Character,
				AICF_EquipmentInventory.Weapon(member.m_Character, false)) >= member.m_Plan.m_iPrimaryRounds * 0.75);
			m_iEquipmentProbePhase = 3;
			m_iEquipmentProbePhaseAt = now;
			m_EquipmentProbeOldOrder = record.m_Order;
			AICF_EQBinding(slot, "first");
			m_EquipmentProbeService.Update(m_EquipmentProbeState, m_USFaction, null, null, null, true);
			AICF_EquipmentMember current = record.m_aMembers[0];
			m_EquipmentProbeOldMember = new AICF_EquipmentMember();
			m_EquipmentProbeOldMember.m_Slot = slot;
			m_EquipmentProbeOldMember.m_Group = slot.GetGroup();
			m_EquipmentProbeOldMember.m_GroupId = slot.GetGroup().GetID();
			m_EquipmentProbeOldMember.m_Character = current.m_Character;
			m_EquipmentProbeOldMember.m_CharacterId = current.m_CharacterId;
			m_EquipmentProbeOldMember.m_iGeneration = current.m_iGeneration;
			m_EquipmentProbeOldMember.m_iIndex = 0;
			m_EquipmentProbeOldMember.m_Faction = m_USFaction;
			m_EquipmentProbeOldMember.m_iRevision = current.m_iRevision;
			m_EquipmentProbeOldMember.m_Plan = current.m_Plan;
			AICF_EQBinding(slot, "latest");
			AICF_EQCheck("STALE_BINDING_REJECTED", !m_EquipmentProbeOldMember.Current() && slot.GetMemberLoadoutRevision(0) == 0);
			vector basePosition = m_USFaction.GetMainBase().GetSpawnPoint().GetOrigin();
			AICF_EQPlaceArmory(m_USFaction.GetMainBase(), basePosition + "65 0 20");
			AICF_EQPlaceArmory(m_USFaction.GetMainBase(), basePosition + "100 0 40");
		}
		if (m_iEquipmentProbePhase == 3 && record.m_Order && record.m_Order.m_Service)
		{
			AICF_EquipmentOrder visit = record.m_Order;
			if (!m_bEquipmentProbeRevisionChecked)
			{
				AICF_EQCheck("RECRUITMENT_FENCED", !m_OrderPlanner.CanRecruitInfantry(slot, m_USFaction) && !m_OrderPlanner.CanPlayerRecruitInfantry(slot, m_USFaction));
				AICF_EQCheck("TRANSPORT_FENCED", m_VehicleCoordinator.AICF_ProbeEquipmentAdmission(slot) == "EQUIPMENT_SERVICE");
				AICF_EQCheck("SECOND_SERVICE_FENCED", !m_OrderPlanner.CanEquipmentService(slot, m_USFaction));
				AICF_EQCheck("LATEST_REVISION_TARGET", member.m_iRevision == slot.GetLoadout(0).m_iRevision &&
					slot.GetMemberLoadoutRevision(0) < member.m_iRevision);
				string distant;
				AICF_EquipmentApplicator applicator = new AICF_EquipmentApplicator();
				if (enabled != "inventory") AICF_EQCheck("NO_DISTANT_ISSUE", !applicator.Apply(visit, member, distant));
				m_bEquipmentProbeRevisionChecked = true;
			}
			if (!m_bEquipmentProbeFailureArmed && AICF_EquipmentConfig.InRange(member.m_Character.GetOrigin(), visit.m_vPosition, 17))
			{
				m_sEquipmentProbeBeforeFailure = AICF_EQInventoryIdentity(member.m_Character);
				AICF_EquipmentApplicator.s_bAICFRefuseRestore = true;
				m_bEquipmentProbeFailureArmed = true;
			}
		}
		if (m_iEquipmentProbePhase == 3 && slot.GetMemberLoadoutRevision(0) == slot.GetLoadout(0).m_iRevision)
		{
			AICF_EQCheck("LATEST_REVISION_APPLIED", m_bEquipmentProbeRevisionChecked && member.m_iRevision == slot.GetMemberLoadoutRevision(0));
			AICF_EQCheck("TARGET_ROUNDS", AICF_EquipmentInventory.Rounds(member.m_Character, AICF_EquipmentInventory.Weapon(member.m_Character, false)) >= member.m_Plan.m_iPrimaryRounds);
			AICF_EQCheck("ROLLBACK_CASE_EXECUTED", m_bEquipmentProbeFailureChecked);
			AICF_EQCheck("ISSUE_UNLOCKED_AND_LEDGER_UNCHANGED", !CharacterControllerComponent.Cast(member.m_Character.FindComponent(CharacterControllerComponent)).GetDisableMovementControls() &&
				!m_bEquipmentProbeLedgerChanged);
			m_iEquipmentProbeApplied = now;
			m_iEquipmentProbePhase = 4;
		}
		if (m_iEquipmentProbePhase == 4 && now - m_iEquipmentProbeApplied >= 30000)
		{
			AICF_EQCheck("NO_RESUPPLY_LOOP", !record.m_Order && !member.m_bPending && slot.GetMemberLoadoutRevision(0) == member.m_iRevision);
			AICF_EQCheck("ORIGINAL_INTENT_RESTORED", slot.HasPlayerStrategicIntent() && !slot.IsResupplyingEquipment());
			m_iEquipmentProbePhase = 5;
			AICF_EQClose();
		}
	}
}
