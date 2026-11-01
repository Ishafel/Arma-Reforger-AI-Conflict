// Native compartment producer создаёт actor ДО появления parent group. Это
// отдельный typed factory context, не послабление обычного vehicle Entry.
class AICF_AIQuotaCompartmentCreation : AICF_AIQuotaCreationGuard
{
	AICF_AIWorldQuotaService m_Service;
	int m_iEpoch;
	int m_iSerial;
	ref AICF_AIQuotaVehicleEntry m_SeatScope;
	ref AICF_AIQuotaVehicleOwner m_CarOwner;
	AICF_AIQuotaBody m_CarBody;
	ref AICF_AIQuotaReservation m_CharacterReservation;
	AICF_AIQuotaCreation m_Creation;
	ref AICF_AIQuotaBody m_CharacterBody;
	ResourceName m_CharacterPrefab;
	ResourceName m_GroupPrefab;
	FactionKey m_sFaction;
	AIGroup m_Group;
	bool m_bHadGroup;
	ref AICF_AIQuotaCatalogIdentity m_GroupIdentity;
	ref AICF_AIQuotaCatalogIdentity m_NewGroupIdentity;
	Faction m_GroupFaction;
	FactionKey m_sGroupFaction;
	ref AICF_AIQuotaVehicleEntity m_Newborn;
	AIControlComponent m_Control;
	bool m_bNativeIssued;
	bool m_bHasNativeResult;
	bool m_bAccepted;
	bool m_bSourceEnded;
	bool m_bClosed;

	static AICF_AIQuotaCompartmentCreation Capture(AICF_AIWorldQuotaService service,
		BaseCompartmentSlot seat, ResourceName characterPrefab, AIGroup group, ResourceName groupPrefab)
	{
		if (!service || !service.IsOwnerCurrent() || !service.IsConfigured() || !seat || !seat.GetOwner() || !seat.GetVehicle()) return null;
		AICF_AIQuotaCompartmentCreation context = new AICF_AIQuotaCompartmentCreation();
		context.m_Service = service;
		context.m_iEpoch = service.GetEpoch();
		context.m_iSerial = service.AllocateContextSerial();
		context.m_CharacterPrefab = characterPrefab;
		context.m_GroupPrefab = groupPrefab;
		context.m_Group = group;
		context.m_bHadGroup = group != null;
		if (!ReadCharacterMetadata(characterPrefab, context.m_sFaction)) return null;
		if (group)
		{
			SCR_AIGroup scripted = SCR_AIGroup.Cast(group);
			if (!scripted || !scripted.GetFaction()) return null;
			context.m_GroupIdentity = AICF_AIQuotaCatalogIdentity.Capture(scripted, true);
			context.m_GroupFaction = scripted.GetFaction();
			context.m_sGroupFaction = context.m_GroupFaction.GetFactionKey();
		}
		else if (!ReadEmptyGroupMetadata(groupPrefab)) return null;
		AICF_AIQuotaVehicleEntry scope = new AICF_AIQuotaVehicleEntry();
		scope.m_Seat = seat;
		scope.m_NativeVehicle = seat.GetOwner();
		scope.m_Vehicle = seat.GetVehicle();
		scope.m_Manager = seat.GetManager();
		scope.m_iManagerId = seat.GetCompartmentMgrID();
		scope.m_iSlotId = seat.GetCompartmentSlotID();
		scope.m_VehicleIdentity = AICF_AIQuotaVehicleEntity.Capture(scope.m_Vehicle);
		scope.m_SeatOwnerIdentity = AICF_AIQuotaVehicleEntity.Capture(seat.GetOwner());
		context.m_SeatScope = scope;
		if (!context.IsCurrent() || !seat.IsCompartmentAccessible() || !context.IsCurrent()) return null;
		return context;
	}

	// Сигнатуры/имена полей закреплены в Script Diff1.8.0.13:
	// PossessSpawnHandler.GetFactionFromPrefab и SCR_AIGroup.GetFormationFromPrefab.
	static bool ReadCharacterMetadata(ResourceName prefab, out FactionKey faction)
	{
		faction = string.Empty;
		if (prefab.IsEmpty() || !GetGame() || !GetGame().GetFactionManager()) return false;
		Resource resource = Resource.Load(prefab);
		if (!resource || !resource.IsValid() || !resource.GetResource()) return false;
		IEntitySource source = resource.GetResource().ToEntitySource();
		if (!source || !source.GetClassName().ToType().IsInherited(ChimeraCharacter)) return false;
		IEntityComponentSource affiliation = SCR_BaseContainerTools.FindComponentSource(resource, FactionAffiliationComponent);
		if (!affiliation || !affiliation.Get("faction affiliation", faction) || faction.IsEmpty()) return false;
		return GetGame().GetFactionManager().GetFactionByKey(faction) != null;
	}

	static bool ReadEmptyGroupMetadata(ResourceName prefab)
	{
		if (prefab.IsEmpty()) return false;
		Resource resource = Resource.Load(prefab);
		if (!resource || !resource.IsValid() || !resource.GetResource()) return false;
		IEntitySource source = resource.GetResource().ToEntitySource();
		if (!source || !source.GetClassName().ToType().IsInherited(SCR_AIGroup)) return false;
		array<ResourceName> units = {};
		return source.Get("m_aUnitPrefabSlots", units) && units.IsEmpty();
	}

	bool IsGroupCurrent()
	{
		if (!m_bHadGroup) return !m_Group;
		SCR_AIGroup group = SCR_AIGroup.Cast(m_Group);
		return group && m_GroupIdentity && m_GroupIdentity.IsCurrent() && m_GroupIdentity.m_Entity == group &&
			m_GroupFaction && group.GetFaction() == m_GroupFaction && !m_sGroupFaction.IsEmpty() &&
			m_GroupFaction.GetFactionKey() == m_sGroupFaction;
	}

	bool IsSeatScopeCurrent()
	{
		AICF_AIQuotaVehicleEntry scope = m_SeatScope;
		if (!scope || !scope.m_VehicleIdentity || !scope.m_SeatOwnerIdentity || !scope.IsSeatCurrent()) return false;
		if (!scope.m_VehicleIdentity.IsCurrent() || !scope.m_SeatOwnerIdentity.IsCurrent()) return false;
		array<BaseCompartmentSlot> seats = {};
		scope.m_Manager.GetCompartments(seats);
		if (seats.Count() > AICF_AIQuotaVehicleEntry.MAX_COMPARTMENTS) return false;
		int matches;
		foreach (BaseCompartmentSlot seat : seats)
		{
			if (!seat) return false;
			if (seat == scope.m_Seat) matches++;
		}
		return matches == 1 && scope.m_VehicleIdentity.IsCurrent() && scope.m_SeatOwnerIdentity.IsCurrent() && scope.IsSeatCurrent();
	}

	override bool IsCurrent()
	{
		if (m_bSourceEnded || m_bClosed || !m_Service || !m_Service.IsOwnerCurrent() || m_Service.GetEpoch() != m_iEpoch || m_iSerial <= 0) return false;
		if (!IsGroupCurrent() || !IsSeatScopeCurrent()) return false;
		IEntity occupant = m_SeatScope.m_Seat.GetOccupant();
		if (occupant && (!m_Newborn || occupant != m_Newborn.m_Entity)) return false;
		if (m_CarOwner && !m_Service.IsCompartmentCarHoldCurrent(this)) return false;
		return IsGroupCurrent() && IsSeatScopeCurrent();
	}

	override FactionKey GetFactionKey() { return m_sFaction; }
	override AICF_EAIQuotaBucket GetBucket() { return AICF_EAIQuotaBucket.STOCK; }

	bool IsNewbornCurrent()
	{
		if (!IsCurrent() || !m_Newborn || !m_Newborn.IsCurrent() || !m_CharacterBody ||
			m_CharacterBody.m_Entity != m_Newborn.m_Entity || !m_CharacterBody.MatchesIdentity()) return false;
		ChimeraCharacter actor = ChimeraCharacter.Cast(m_Newborn.m_Entity);
		if (!actor || actor.GetAIControlComponent() != m_Control || !m_Control) return false;
		CharacterControllerComponent controller = actor.GetCharacterController();
		PlayerManager players = GetGame().GetPlayerManager();
		if (!controller || controller.IsPlayerControlled() || !players || players.GetPlayerIdFromControlledEntity(actor) > 0 ||
			SCR_PossessingManagerComponent.GetPlayerIdFromMainEntity(actor) > 0) return false;
		AIAgent agent = m_Control.GetControlAIAgent();
		if (agent && (agent.GetControlledEntity() != actor || agent.GetParentGroup())) return false;
		IEntity linked = CompartmentAccessComponent.GetVehicleIn(actor);
		if (linked && linked != m_SeatScope.m_Vehicle) return false;
		CompartmentAccessComponent access = actor.GetCompartmentAccessComponent();
		if (access && access.GetCompartment() && access.GetCompartment() != m_SeatScope.m_Seat) return false;
		FactionAffiliationComponent affiliation = FactionAffiliationComponent.Cast(actor.FindComponent(FactionAffiliationComponent));
		if (!controller || controller.GetLifeState() != ECharacterLifeState.ALIVE || !affiliation || !affiliation.GetAffiliatedFaction()) return false;
		return affiliation.GetAffiliatedFaction().GetFactionKey() == m_sFaction && m_Newborn.IsCurrent() && IsCurrent();
	}

	bool IsDestinationCurrent(AIGroup group)
	{
		if (m_bHadGroup) return group == m_Group && IsGroupCurrent();
		SCR_AIGroup created = SCR_AIGroup.Cast(group);
		return created && m_NewGroupIdentity && m_NewGroupIdentity.m_Entity == created && m_NewGroupIdentity.IsCurrent() &&
			created.GetAgentsCount() == 0 && created.GetPlayerCount() == 0 && created.GetFaction() &&
			created.GetFaction().GetFactionKey() == m_sFaction && !created.CountTowardsAIWorldLimit() &&
			created.m_aUnitPrefabSlots && created.m_aUnitPrefabSlots.IsEmpty() && m_NewGroupIdentity.IsCurrent();
	}
}
