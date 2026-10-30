// FIA сохраняет собственный stock roster и RHS equipment hook.
class AICF_FIAPatrolCrew : AICF_GroupSpawner
{
	SCR_AIGroup Create(SCR_CampaignFaction faction, vector position, int crewSize = 2)
	{
		if (!Replication.IsServer() || !faction || faction.GetFactionKey() != "FIA" || crewSize < 1 || crewSize > 16) return null;
		SCR_EntityCatalog catalog = faction.GetFactionEntityCatalogOfType(EEntityCatalogType.CHARACTER);
		if (!catalog)
		{
			AICF_Stage3Diagnostics.Warning("FIA_PATROL_CREW_REJECTED", "reason=NO_CHARACTER_CATALOG");
			return null;
		}
		array<SCR_EntityCatalogEntry> entries = {};
		catalog.GetEntityList(entries);
		array<string> suffixes = {"Prefabs/Characters/Factions/INDFOR/FIA/Character_FIA_Rifleman.et"};
		ResourceName character = FindCharacterPrefab(entries, suffixes);
		ResourceName prefab = "{000CD338713F2B5A}Prefabs/AI/Groups/Group_Base.et";
		Resource resource = Resource.Load(prefab);
		if (character.IsEmpty() || !resource || !resource.IsValid())
		{
			AICF_Stage3Diagnostics.Warning("FIA_PATROL_CREW_REJECTED", string.Format("reason=PREFAB_MISSING character=%1 group=%2", character, prefab));
			return null;
		}
		EntitySpawnParams params = new EntitySpawnParams();
		params.TransformMode = ETransformMode.WORLD;
		params.Transform[3] = position;
		SCR_AIGroup.IgnoreSpawning(true);
		SCR_AIGroup group = SCR_AIGroup.Cast(GetGame().SpawnEntityPrefabEx(prefab, false, params: params));
		SCR_AIGroup.IgnoreSpawning(false);
		if (!group) return null;
		group.SetFaction(faction);
		if (group.GetFaction() != faction || !group.m_aUnitPrefabSlots || group.GetAgentsCount() != 0)
		{
			RplComponent.DeleteRplEntity(group, false);
			return null;
		}
		group.SetLifecyclePolicy(SCR_EAIGroupLifecyclePolicy.Manual);
		group.SetSpawnImmediately(false);
		group.m_aUnitPrefabSlots.Clear();
		for (int i; i < crewSize; i++) group.m_aUnitPrefabSlots.Insert(character);
		SCR_AIGroupUtilityComponent utility = group.GetGroupUtilityComponent();
		if (utility)
		{
			utility.SetCombatMode(EAIGroupCombatMode.FIRE_AT_WILL);
			utility.EvaluateCombatMode();
		}
		return group;
	}

	// В том числе вложенный пулемётный turret штатного автомобиля.
	static void Seats(IEntity entity, array<BaseCompartmentSlot> seats)
	{
		if (!entity) return;
		BaseCompartmentManagerComponent manager = BaseCompartmentManagerComponent.Cast(entity.FindComponent(BaseCompartmentManagerComponent));
		array<BaseCompartmentSlot> local = {};
		if (manager) manager.GetCompartments(local);
		foreach (BaseCompartmentSlot seat : local)
		{
			if (seat && !seats.Contains(seat)) seats.Insert(seat);
		}
		SlotManagerComponent slots = SlotManagerComponent.Cast(entity.FindComponent(SlotManagerComponent));
		if (!slots) return;
		array<EntitySlotInfo> infos = {};
		slots.GetSlotInfos(infos);
		foreach (EntitySlotInfo info : infos)
		{
			if (info) Seats(info.GetAttachedEntity(), seats);
		}
	}

	bool Board(AICF_FIAPatrol p)
	{
		if (!p.VehicleIdentity() || !p.GroupIdentity()) return false;
		if (p.m_Lifecycle && !p.m_Lifecycle.Ready()) return false;
		if (p.m_Lifecycle && p.m_Lifecycle.LivingCapacity() < 2)
		{
			Seat(p, p.m_Driver, p.m_PilotSeat);
			Seat(p, p.m_Gunner, p.m_TurretSeat);
			bool seated = true;
			if (AICF_GroupRuntime.IsAliveCharacter(p.m_Driver)) seated = p.m_PilotSeat && p.m_PilotSeat.GetOccupant() == p.m_Driver;
			if (AICF_GroupRuntime.IsAliveCharacter(p.m_Gunner)) seated = seated && p.m_TurretSeat && p.m_TurretSeat.GetOccupant() == p.m_Gunner;
			return seated;
		}
		int actual, wrong, dead;
		if (!AICF_GroupRuntime.HasExactFactionRoster(p.m_Group, "FIA", 2, actual, wrong, dead)) return false;
		if (!p.m_Driver && !p.m_Gunner)
		{
			array<AIAgent> agents = {};
			p.m_Group.GetAgents(agents);
			if (agents.Count() != 2) return false;
			p.m_Driver = ChimeraCharacter.Cast(agents[0].GetControlledEntity());
			p.m_Gunner = ChimeraCharacter.Cast(agents[1].GetControlledEntity());
			if (!p.m_Driver || !p.m_Gunner) return false;
			p.m_DriverId = p.m_Driver.GetID();
			p.m_GunnerId = p.m_Gunner.GetID();
		}
		if (!p.CrewIdentity()) return false;
		array<BaseCompartmentSlot> seats = {};
		Seats(p.m_Vehicle, seats);
		foreach (BaseCompartmentSlot seat : seats)
		{
			if (!seat.IsCompartmentAccessible()) continue;
			if (!p.m_PilotSeat && seat.GetType() == ECompartmentType.PILOT) p.m_PilotSeat = seat;
			if (!p.m_TurretSeat && seat.GetType() == ECompartmentType.TURRET) p.m_TurretSeat = seat;
		}
		Seat(p, p.m_Driver, p.m_PilotSeat);
		Seat(p, p.m_Gunner, p.m_TurretSeat);
		return p.Seated();
	}

	protected void Seat(AICF_FIAPatrol p, ChimeraCharacter character, BaseCompartmentSlot seat)
	{
		if (!p.GroupIdentity() || !p.VehicleIdentity() || !AICF_GroupRuntime.IsAliveCharacter(character) ||
			!AICF_VehicleBoardingMutationFence.IsAuthoritativeAIEntity(character) ||
			!seat || seat.GetOccupant() || seat.IsReserved() || !seat.IsCompartmentAccessible()) return;
		AIControlComponent control = AIControlComponent.Cast(character.FindComponent(AIControlComponent));
		if (!control || !control.GetAIAgent() || control.GetAIAgent().GetParentGroup() != p.m_Group) return;
		CompartmentAccessComponent access = character.GetCompartmentAccessComponent();
		if (!access || access.IsInCompartment() || access.IsGettingIn() || access.IsGettingOut()) return;
		AICF_AIQuotaVehicleDispatch quota = AICF_AIQuotaVehicleDispatch.Begin(character, seat.GetOwner(), seat);
		if (!quota) return;
		if (!quota.IsCurrent() || !p.GroupIdentity() || !p.VehicleIdentity() || !p.CrewIdentity() ||
			seat.GetVehicle() != p.m_Vehicle || seat.GetOccupant() || seat.IsReserved() || character.GetCompartmentAccessComponent() != access)
		{
			quota.Cancel();
			return;
		}
		bool accepted = access.GetInVehicle(seat.GetOwner(), seat, true, -1, ECloseDoorAfterActions.CLOSE_DOOR, true);
		quota.ObserveNativeResult(accepted);
		quota.Cancel();
	}
}
