// Только isolated server fixture. Observer не меняет entity/physics/inventory.
// Отдельный baseline flag делает ОДИН настоящий SCR_AIDropWeapon для managed AI;
// ordinary observer этот controlled effect никогда не выполняет.
class AICF_LWObservation
{
	IEntity m_Item;
	EntityID m_Id;
	string m_sRpl;
	string m_sWorld;
	IEntity m_Source;
	EntityID m_SourceId;
	string m_sSourceRpl;
	string m_sContext;
	string m_sInitialContext;
	string m_sScope;
	string m_sLastEvent;
	int m_iEpisode;
	int m_iCreatedAt;
	int m_iNextAt;
	int m_iOwnerRevision;
}

class AICF_LooseWeaponObserver
{
	protected static ref array<ref AICF_LWObservation> s_aRecords = {};
	protected static bool s_bStopped;
	protected static int s_iEpisode;
	protected static int s_iNextTick;
	static int s_iSamples;
	static int s_iMainLoose;
	static int s_iPrivate;
	static int s_iNativeSuccess;
	static int s_iRemoved;
	static const int TTL_MS = 120000;
	static const int MAX_RECORDS = 512;

	static bool Enabled()
	{
		string flag;
		return !s_bStopped && GetGame() && Replication.IsServer() &&
			System.GetCLIParam("aicfLooseWeaponObserver", flag) && flag == "1";
	}

	static string Rpl(IEntity entity)
	{
		if (!entity) return "NONE";
		RplComponent component = RplComponent.Cast(entity.FindComponent(RplComponent));
		if (!component) return "NONE";
		return component.Id().ToString();
	}

	static string World(IEntity entity)
	{
		if (!entity || !entity.GetWorld()) return "NONE";
		return string.Format("%1", entity.GetWorld());
	}

	static void Start()
	{
		s_aRecords.Clear(); s_bStopped = false; s_iEpisode = 0; s_iNextTick = 0;
		s_iSamples = 0; s_iMainLoose = 0; s_iPrivate = 0; s_iNativeSuccess = 0; s_iRemoved = 0;
		if (Enabled()) Print("[AICF][LOOSE_WEAPON_OBSERVER_START] version=1 readonly=1 cleanup_enabled=0 ttl_ms=120000 maximum=512");
	}

	static void Track(IEntity item, IEntity source, string eventName, string scope = "INVENTORY", bool allowModel = false)
	{
		if (!Enabled() || !item || !item.GetWorld()) return;
		BaseWeaponComponent weapon = BaseWeaponComponent.Cast(item.FindComponent(BaseWeaponComponent));
		if (!allowModel && (!weapon || (!AICF_EquipmentInventory.Primary(weapon) && weapon.GetWeaponType() != EWeaponType.WT_ROCKETLAUNCHER))) return;
		AICF_LWObservation record;
		foreach (AICF_LWObservation candidate : s_aRecords)
			if (candidate.m_Item == item && candidate.m_Id == item.GetID() && candidate.m_sRpl == Rpl(item) && candidate.m_sWorld == World(item)) record = candidate;
		if (!record)
		{
			if (s_aRecords.Count() >= MAX_RECORDS)
			{
				Print("[AICF][LOOSE_WEAPON_OBSERVER_CAP] readonly=1 accepted=0");
				return;
			}
			record = new AICF_LWObservation(); record.m_Item = item; record.m_Id = item.GetID();
			record.m_sRpl = Rpl(item); record.m_sWorld = World(item); record.m_sScope = scope;
			record.m_iEpisode = ++s_iEpisode; record.m_iCreatedAt = System.GetTickCount();
			s_aRecords.Insert(record);
		}
		string context = "UNKNOWN_SOURCE";
		AICF_MatchController match = AICF_MatchController.GetActiveController();
		if (source && source.GetWorld() != GetGame().GetWorld()) context = "PRIVATE_SOURCE";
		else if (match) context = match.AICF_LWOwnerContext(source);
		if (record.m_Source != source || record.m_sSourceRpl != Rpl(source) || record.m_sContext != context)
		{
			record.m_iOwnerRevision++;
			record.m_Source = source; if (source) record.m_SourceId = source.GetID();
			record.m_sSourceRpl = Rpl(source); record.m_sContext = context;
			if (record.m_sInitialContext.IsEmpty()) record.m_sInitialContext = context;
		}
		record.m_sLastEvent = eventName;
		if (scope.StartsWith("PRIVATE_")) record.m_sScope = scope;
		if (eventName == "INVENTORY_REMOVED") s_iRemoved++;
		if (eventName == "NATIVE_DROP_SUCCESS") s_iNativeSuccess++;
		if (!eventName.StartsWith("PRIVATE_")) Emit(record, eventName);
	}

	protected static bool Current(AICF_LWObservation record)
	{
		return record && record.m_Item && record.m_Item.GetID() == record.m_Id &&
			Rpl(record.m_Item) == record.m_sRpl && World(record.m_Item) == record.m_sWorld;
	}

	protected static void Emit(AICF_LWObservation record, string trigger)
	{
		if (!Current(record)) return;
		IEntity item = record.m_Item;
		InventoryItemComponent inventory = InventoryItemComponent.Cast(item.FindComponent(InventoryItemComponent));
		InventoryStorageSlot slot;
		if (inventory) slot = inventory.GetParentSlot();
		IEntity storageOwner;
		if (slot && slot.GetStorage()) storageOwner = slot.GetStorage().GetOwner();
		bool sourceCurrent = record.m_Source && record.m_Source.GetID() == record.m_SourceId && Rpl(record.m_Source) == record.m_sSourceRpl;
		int held = -1; int weaponSlot = -1; int contained = -1; int changing = -1;
		if (sourceCurrent)
		{
			CharacterControllerComponent controller = CharacterControllerComponent.Cast(record.m_Source.FindComponent(CharacterControllerComponent));
			InventoryStorageManagerComponent manager = AICF_EquipmentInventory.Manager(record.m_Source);
			if (manager) { contained = 0; if (manager.Contains(item)) contained = 1; }
			if (controller)
			{
				held = 0; changing = 0;
				if (controller.GetCurrentItemInHands() == item || controller.GetRightHandItem() == item ||
					(controller.GetInputContext() && controller.GetInputContext().GetLeftHandGadgetEntity() == item)) held = 1;
				if (controller.IsChangingItem()) changing = 1;
			}
			BaseWeaponManagerComponent weapons = BaseWeaponManagerComponent.Cast(record.m_Source.FindComponent(BaseWeaponManagerComponent));
			if (weapons)
			{
				weaponSlot = 0;
				array<WeaponSlotComponent> slots = {}; weapons.GetWeaponsSlots(slots);
				foreach (WeaponSlotComponent candidate : slots) if (candidate && candidate.GetWeaponEntity() == item) weaponSlot = 1;
			}
		}
		bool mainWorld = item.GetWorld() == GetGame().GetWorld();
		float terrain = 0; float below = 0;
		if (mainWorld) { terrain = item.GetWorld().GetSurfaceY(item.GetOrigin()[0], item.GetOrigin()[2]); below = terrain - item.GetOrigin()[1]; }
		RplComponent replication = RplComponent.Cast(item.FindComponent(RplComponent));
		bool master = replication && replication.IsMaster(); bool proxy = replication && replication.IsProxy();
		string details = string.Format("episode=%1 trigger=%2 scope=%3 entity=%4 rpl=%5 world=%6 main_world=%7 position=%8 prefab=%9",
			record.m_iEpisode, trigger, record.m_sScope, AICF_GroupTrace.Entity(item), record.m_sRpl, record.m_sWorld, mainWorld, item.GetOrigin(), SCR_ResourceNameUtils.GetPrefabName(item));
		details += string.Format(" parent=%1 root_parent=%2 inventory_component=%3 parent_slot=%4 storage_owner=%5 master=%6 proxy=%7 terrain_y=%8 below_terrain_m=%9",
			AICF_GroupTrace.Entity(item.GetParent()), AICF_GroupTrace.Entity(item.GetRootParent()), inventory != null, slot != null, AICF_GroupTrace.Entity(storageOwner), master, proxy, terrain, below);
		details += string.Format(" source=%1 source_rpl=%2 source_current=%3 context=[%4] initial_context=[%5] owner_revision=%6 held=%7 weapon_slot=%8 contained=%9",
			AICF_GroupTrace.Entity(record.m_Source), record.m_sSourceRpl, sourceCurrent, record.m_sContext, record.m_sInitialContext, record.m_iOwnerRevision, held, weaponSlot, contained);
		if (sourceCurrent)
		{
			IEntity source = record.m_Source;
			Faction faction = SCR_Faction.GetEntityFaction(source);
			string factionKey = "NONE";
			if (faction) factionKey = faction.GetFactionKey();
			AIAgent group;
			AIControlComponent control = AIControlComponent.Cast(source.FindComponent(AIControlComponent));
			if (control && control.GetControlAIAgent()) group = control.GetControlAIAgent().GetParentGroup();
			details += string.Format(" source_position=%1 source_group=%2 source_prefab=%3 source_faction=%4 source_main_world=%5",
				source.GetOrigin(), AICF_GroupTrace.Entity(group), SCR_ResourceNameUtils.GetPrefabName(source), factionKey, source.GetWorld() == GetGame().GetWorld());
		}
		details += string.Format(" changing_item=%1 age_ms=%2 ttl_remaining_ms=%3 locked=%4 last_event=%5 readonly=1 cleanup_enabled=0",
			changing, System.GetTickCount() - record.m_iCreatedAt, TTL_MS - (System.GetTickCount() - record.m_iCreatedAt), inventory && inventory.IsLocked(), record.m_sLastEvent);
		Physics physics = item.GetPhysics();
		if (physics)
		{
			bool dynamicBody = physics.IsDynamic();
			if (dynamicBody)
			{
				details += string.Format(" physics=1 simulation=%1 layer=%2 dynamic=1 kinematic=%3 active=%4", physics.GetSimulationState(), physics.GetInteractionLayer(), physics.IsKinematic(), physics.IsActive());
				details += string.Format(" velocity=%1 mass=%2", physics.GetVelocity(), physics.GetMass());
			}
			else details += " physics=1 simulation=-1 layer=-1 dynamic=0 kinematic=-1 active=-1 velocity=UNKNOWN_NON_DYNAMIC mass=UNKNOWN_NON_DYNAMIC";
		}
		else details += " physics=0";
		Print("[AICF][LOOSE_WEAPON_SAMPLE] " + details);
		s_iSamples++;
		if (!mainWorld) s_iPrivate++;
		if (mainWorld && inventory && !slot && held == 0 && weaponSlot == 0 && contained == 0) s_iMainLoose++;
	}

	static void Tick()
	{
		if (!Enabled() || System.GetTickCount() < s_iNextTick) return;
		s_iNextTick = System.GetTickCount() + 1000;
		for (int index = s_aRecords.Count() - 1; index >= 0; index--)
		{
			AICF_LWObservation record = s_aRecords[index];
			bool current = Current(record);
			if (!current || System.GetTickCount() - record.m_iCreatedAt >= TTL_MS)
			{
				Print(string.Format("[AICF][LOOSE_WEAPON_OBSERVATION_END] episode=%1 entity_id=%2 rpl=%3 identity_current=%4 expired=%5 readonly=1 item_deleted_by_observer=0",
					record.m_iEpisode, record.m_Id, record.m_sRpl, current, System.GetTickCount() - record.m_iCreatedAt >= TTL_MS));
				s_aRecords.Remove(index); continue;
			}
			if (System.GetTickCount() < record.m_iNextAt) continue;
			record.m_iNextAt = System.GetTickCount() + 5000;
			Emit(record, "POLL");
		}
	}

	static void Stop()
	{
		if (s_bStopped) return;
		if (Enabled()) Print(string.Format("[AICF][LOOSE_WEAPON_OBSERVER_STOP] samples=%1 main_loose=%2 private_samples=%3 native_drop_success=%4 inventory_removed=%5 records=%6 readonly=1 cleanup_enabled=0",
			s_iSamples, s_iMainLoose, s_iPrivate, s_iNativeSuccess, s_iRemoved, s_aRecords.Count()));
		s_bStopped = true; s_aRecords.Clear();
	}
}

modded class ScriptedInventoryStorageManagerComponent
{
	override protected void OnItemRemoved(BaseInventoryStorageComponent storageOwner, IEntity item)
	{
		super.OnItemRemoved(storageOwner, item);
		AICF_LooseWeaponObserver.Track(item, GetOwner(), "INVENTORY_REMOVED");
	}
	override protected void OnItemAdded(BaseInventoryStorageComponent storageOwner, IEntity item)
	{
		super.OnItemAdded(storageOwner, item);
		AICF_LooseWeaponObserver.Track(item, GetOwner(), "INVENTORY_ADDED");
	}
}

modded class SCR_AIDropWeapon
{
	override ENodeResult EOnTaskSimulate(AIAgent owner, float dt)
	{
		IEntity actor; IEntity item;
		if (owner) actor = owner.GetControlledEntity();
		BaseWeaponManagerComponent weapons;
		if (actor) weapons = BaseWeaponManagerComponent.Cast(actor.FindComponent(BaseWeaponManagerComponent));
		if (weapons && weapons.GetCurrentSlot()) item = weapons.GetCurrentSlot().GetWeaponEntity();
		AICF_LooseWeaponObserver.Track(item, actor, "NATIVE_DROP_BEFORE");
		ENodeResult result = super.EOnTaskSimulate(owner, dt);
		string eventName = "NATIVE_DROP_" + typename.EnumToString(ENodeResult, result);
		AICF_LooseWeaponObserver.Track(item, actor, eventName);
		return result;
	}
}

modded class AICF_EquipmentService
{
	void AICF_LWObservePlans()
	{
		if (!AICF_LooseWeaponObserver.Enabled()) return;
		foreach (int index, AICF_EquipmentPlan plan : m_aPlans)
		{
			if (!plan || !plan.m_Draft) continue;
			IEntity model = plan.m_Draft.GetCharacter();
			if (!model) continue;
			AICF_LooseWeaponObserver.Track(model, model, "PRIVATE_PLAN_POLL", "PRIVATE_MODEL_PLAN_" + index.ToString(), true);
			InventoryStorageManagerComponent manager = AICF_EquipmentInventory.Manager(model);
			if (!manager) continue;
			array<IEntity> raw = {}; array<IEntity> items = {}; manager.GetItems(raw); AICF_EquipmentInventory.UniqueItems(raw, items);
			foreach (IEntity item : items) AICF_LooseWeaponObserver.Track(item, model, "PRIVATE_PLAN_POLL", "PRIVATE_WEAPON_PLAN_" + index.ToString());
		}
	}
}

modded class AICF_MatchController
{
	protected int m_iAICFLWNextPlans;
	protected int m_iAICFLWBaselineStarted;
	protected bool m_bAICFLWDropRequested;
	protected bool m_bAICFLWBaselineDone;

	string AICF_LWOwnerContext(IEntity actor)
	{
		if (!actor || actor.GetWorld() != GetGame().GetWorld()) return "UNKNOWN_SOURCE";
		array<AICF_FactionState> states = {m_USState, m_USSRState};
		foreach (AICF_FactionState state : states)
		{
			if (!state) continue;
			for (int index; index < state.GetSlotCount(); index++)
			{
				AICF_GroupSlot slot = state.GetSlot(index);
				if (!slot || !slot.GetGroup()) continue;
				for (int position; position < AICF_Stage1Config.MAX_GROUP_SIZE; position++)
					if (slot.GetRosterMember(position) == actor)
						return string.Format("MAIN_MANAGED:faction%1:slot%2:generation%3:position%4:group%5", state.GetFactionKey(), index, slot.GetSpawnGeneration(), position, AICF_GroupTrace.Entity(slot.GetGroup()));
			}
		}
		return "MAIN_UNMANAGED";
	}

	protected void AICF_LWRunBaseline()
	{
		string flag;
		if (!System.GetCLIParam("aicfLooseWeaponBaseline", flag) || flag != "1" || m_bAICFLWBaselineDone) return;
		if (!m_iAICFLWBaselineStarted) m_iAICFLWBaselineStarted = System.GetTickCount();
		if (!m_bAICFLWDropRequested)
		{
			for (int index = 1; m_USSRState && index < m_USSRState.GetSlotCount(); index++)
			{
				AICF_GroupSlot slot = m_USSRState.GetSlot(index);
				if (!slot || !slot.IsCombatReady() || !slot.GetGroup()) continue;
				array<AIAgent> agents = {}; slot.GetGroup().GetAgents(agents);
				foreach (AIAgent agent : agents)
				{
					ChimeraCharacter actor = ChimeraCharacter.Cast(agent.GetControlledEntity());
					if (!actor || !AICF_EquipmentInventory.SafeMember(actor, slot.GetGroup()) || actor.GetCharacterController().IsChangingItem()) continue;
					BaseWeaponManagerComponent weapons = BaseWeaponManagerComponent.Cast(actor.FindComponent(BaseWeaponManagerComponent));
					if (!weapons || !weapons.GetCurrentSlot() || !weapons.GetCurrentSlot().GetWeaponEntity()) continue;
					m_bAICFLWDropRequested = true;
					SCR_AIDropWeapon drop = new SCR_AIDropWeapon(); drop.OnInit(agent);
					ENodeResult result = drop.EOnTaskSimulate(agent, 0.016);
					Print(string.Format("[AICF][LOOSE_WEAPON_BASELINE_DROP] source=%1 context=[%2] result=%3 native_task=SCR_AIDropWeapon simulation_changed=0 actor_moved=0",
						AICF_GroupTrace.Entity(actor), AICF_LWOwnerContext(actor), typename.EnumToString(ENodeResult, result)));
					break;
				}
				if (m_bAICFLWDropRequested) break;
			}
		}
		if (System.GetTickCount() - m_iAICFLWBaselineStarted < 60000) return;
		m_bAICFLWBaselineDone = true;
		bool passed = m_bAICFLWDropRequested && AICF_LooseWeaponObserver.s_iNativeSuccess > 0 &&
			AICF_LooseWeaponObserver.s_iRemoved > 0 && AICF_LooseWeaponObserver.s_iMainLoose > 0 && AICF_LooseWeaponObserver.s_iPrivate > 0;
		Print(string.Format("[AICF][LOOSE_WEAPON_BASELINE_DONE] passed=%1 requested=%2 native_success=%3 removed=%4 main_loose=%5 private_samples=%6 readonly_observer=1 simulation_changed=0 cleanup_enabled=0",
			passed, m_bAICFLWDropRequested, AICF_LooseWeaponObserver.s_iNativeSuccess, AICF_LooseWeaponObserver.s_iRemoved, AICF_LooseWeaponObserver.s_iMainLoose, AICF_LooseWeaponObserver.s_iPrivate));
		GetGame().RequestClose();
	}

	override protected void Update()
	{
		super.Update();
		if (!m_bRosterReady || m_bStopped || !AICF_LooseWeaponObserver.Enabled()) return;
		if (System.GetTickCount() >= m_iAICFLWNextPlans && m_EquipmentService)
		{
			m_iAICFLWNextPlans = System.GetTickCount() + 10000;
			m_EquipmentService.AICF_LWObservePlans();
		}
		AICF_LooseWeaponObserver.Tick();
		AICF_LWRunBaseline();
	}

	override protected void Stop(bool cleanupEntities)
	{
		AICF_LooseWeaponObserver.Stop();
		super.Stop(cleanupEntities);
	}
}

modded class SCR_GameModeCampaign
{
	override void OnGameStart()
	{
		super.OnGameStart();
		AICF_LooseWeaponObserver.Start();
	}
	override void OnGameEnd()
	{
		AICF_LooseWeaponObserver.Stop();
		super.OnGameEnd();
	}
}
