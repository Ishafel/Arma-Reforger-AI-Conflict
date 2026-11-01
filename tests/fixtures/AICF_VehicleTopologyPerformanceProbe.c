// Только изолированный stage с явным CLI. Ничего не создаёт/не сажает/не лечит.
// Старые дополнительные обходы намеренно дороги: это benchmark, не ordinary FPS.
class AICF_VehicleTopologyLegacyProbe : AICF_VehiclePreset
{
static void OldCollect(IEntity vehicle, array<BaseCompartmentSlot> seats){
		seats.Clear();
		if (!vehicle) return;
		BaseCompartmentManagerComponent manager = BaseCompartmentManagerComponent.Cast(vehicle.FindComponent(BaseCompartmentManagerComponent));
		if (!manager) return;
		array<BaseCompartmentSlot> found = {};
		manager.GetCompartments(found);
		foreach (BaseCompartmentSlot seat : found)
		{
			if (!seat || !seat.IsCompartmentAccessible() || seat.GetVehicle() != vehicle) continue;
			int index = seats.Count();
			for (int i; i < seats.Count(); i++)
				if (RelativeOwnerIndex(seat) < RelativeOwnerIndex(seats[i]) ||
					(RelativeOwnerIndex(seat) == RelativeOwnerIndex(seats[i]) && seat.GetCompartmentSlotID() < seats[i].GetCompartmentSlotID()))
				{ index = i; break; }
			seats.InsertAt(seat, index);
		}
	} static bool OldCollectCaptured(IEntity vehicle, AICF_VehiclePreset preset, array<BaseCompartmentSlot> seats){
		seats.Clear();
		if (!vehicle || !preset || preset.m_aSeats.IsEmpty() || preset.m_aSeats.Count() > MAX_SEATS ||
			preset.m_aSeats.Count() != preset.m_aMembers.Count()) return false;
		BaseCompartmentManagerComponent manager = BaseCompartmentManagerComponent.Cast(vehicle.FindComponent(BaseCompartmentManagerComponent));
		if (!manager || manager.GetOwner() != vehicle) return false;
		array<BaseCompartmentSlot> raw = {};
		manager.GetCompartments(raw);
		if (raw.IsEmpty() || raw.Count() > MAX_SEATS) return false;
		array<string> keys = {};
		foreach (BaseCompartmentSlot seat : raw)
		{
			if (!seat || seat.GetVehicle() != vehicle || !seat.GetOwner() || !seat.GetManager() ||
				seat.GetManager().GetOwner() != seat.GetOwner() || RelativeOwnerIndex(seat) < 0) return false;
			string key = SeatKey(seat);
			if (keys.Contains(key)) return false;
			keys.Insert(key);
			// Catalog мог изначально исключить недоступные места. Новое доступное
			// место вне captured recipe означает настоящую несовместимость.
			if (!preset.m_aSeats.Contains(key) && seat.IsCompartmentAccessible()) return false;
		}
		array<BaseCompartmentSlot> resolved = {};
		foreach (string captured : preset.m_aSeats)
		{
			int index = keys.Find(captured);
			if (index < 0 || resolved.Contains(raw[index])) return false;
			resolved.Insert(raw[index]);
		}
		seats.Copy(resolved);
		return true;
	} bool OldMatchesSeats(array<BaseCompartmentSlot> seats){
		if (seats.Count() != m_aSeats.Count()) return false;
		for (int i; i < seats.Count(); i++)
			if (SeatKey(seats[i]) != m_aSeats[i]) return false;
		return true;
	}
}

class AICF_VehicleTopologyPerformanceContext
{
	AICF_TransportTrip m_Trip;
	AICF_GroupSlot m_Slot;
	SCR_AIGroup m_Group;
	Vehicle m_Vehicle;
	EntityID m_GroupId;
	EntityID m_VehicleId;
	string m_sVehicleRpl;
	string m_sGroupRpl;
	int m_iGroupGeneration;
	int m_iTripGeneration;
	int m_iLeaseGeneration;
	int m_iVehicleGeneration;
	int m_iAssignmentRevision;
	int m_iPresetRevision;
	int m_iInventoryRawItems;
	ref array<IEntity> m_aMembers = {};
	ref array<EntityID> m_aMemberIds = {};

	static AICF_VehicleTopologyPerformanceContext Capture(AICF_GroupSlot slot, AICF_TransportTrip trip)
	{
		if (!Replication.IsServer() || !slot || !slot.IsCombatReady() || !trip || !trip.IsValid() || trip.IsTerminal() ||
			!trip.GetVehiclePreset() || !trip.IsVehiclePresetCurrent() || !trip.GetLease() || !trip.GetLease().HasPhysicalAsset()) return null;
		SCR_AIGroup group = slot.GetGroup();
		Vehicle vehicle = trip.GetLease().GetVehicle();
		if (!group || trip.GetAssignment().GetGroup() != group ||
			!AICF_VehicleBoardingMutationFence.IsAuthoritativeReplicatedEntity(group) ||
			!AICF_VehicleBoardingMutationFence.IsAuthoritativeReplicatedEntity(vehicle)) return null;
		array<AIAgent> agents = {};
		group.GetAgents(agents);
		if (agents.Count() != 10 || AICF_GroupRuntime.CountAliveAgents(group) != 10) return null;
		AICF_VehicleTopologyPerformanceContext scope = new AICF_VehicleTopologyPerformanceContext();
		scope.m_Trip = trip;
		scope.m_Slot = slot;
		scope.m_Group = group;
		scope.m_Vehicle = vehicle;
		scope.m_GroupId = group.GetID();
		scope.m_VehicleId = vehicle.GetID();
		scope.m_sVehicleRpl = Replication.FindItemId(vehicle).ToString();
		scope.m_sGroupRpl = Replication.FindItemId(group).ToString();
		scope.m_iGroupGeneration = trip.GetGroupGeneration();
		scope.m_iTripGeneration = trip.GetTripGeneration();
		scope.m_iLeaseGeneration = trip.GetLease().GetLeaseGeneration();
		scope.m_iVehicleGeneration = trip.GetLease().GetVehicleGeneration();
		scope.m_iAssignmentRevision = trip.GetAssignment().GetAssignmentRevision();
		scope.m_iPresetRevision = trip.GetVehiclePresetRevision();
		foreach (AIAgent agent : agents)
		{
			if (!agent || agent.GetParentGroup() != group) return null;
			IEntity member = agent.GetControlledEntity();
			if (!AICF_VehicleShortRecovery.IsCurrentMember(trip, member) || !SCR_AIDamageHandling.IsConscious(member) ||
				!AICF_VehicleBoardingMutationFence.IsAuthoritativeAIEntity(member) ||
				!AICF_VehicleNativeSubgroup.Settled(member, vehicle) || scope.m_aMembers.Contains(member)) return null;
			scope.m_aMembers.Insert(member);
			scope.m_aMemberIds.Insert(member.GetID());
		}
		BaseCompartmentManagerComponent manager = BaseCompartmentManagerComponent.Cast(vehicle.FindComponent(BaseCompartmentManagerComponent));
		if (!manager || manager.GetOwner() != vehicle) return null;
		array<BaseCompartmentSlot> seats = {};
		manager.GetCompartments(seats);
		array<IEntity> occupants = {};
		foreach (BaseCompartmentSlot seat : seats)
		{
			if (!seat || seat.GetVehicle() != vehicle) return null;
			IEntity occupant = seat.GetOccupant();
			if (!occupant) continue;
			if (!scope.m_aMembers.Contains(occupant) || occupants.Contains(occupant)) return null;
			occupants.Insert(occupant);
		}
		if (occupants.Count() != 10) return null;
		foreach (IEntity equipped : scope.m_aMembers)
		{
			InventoryStorageManagerComponent inventory = AICF_EquipmentInventory.Manager(equipped);
			if (!inventory) return null;
			array<IEntity> items = {};
			inventory.GetItems(items);
			if (items.IsEmpty()) return null;
			scope.m_iInventoryRawItems += items.Count();
		}
		return scope;
	}

	bool Current()
	{
		AICF_VehicleTopologyPerformanceContext repeated = Capture(m_Slot, m_Trip);
		if (!repeated || repeated.m_Group != m_Group || repeated.m_Vehicle != m_Vehicle ||
			repeated.m_GroupId != m_GroupId || repeated.m_sGroupRpl != m_sGroupRpl || repeated.m_VehicleId != m_VehicleId || repeated.m_sVehicleRpl != m_sVehicleRpl ||
			repeated.m_iGroupGeneration != m_iGroupGeneration || repeated.m_iTripGeneration != m_iTripGeneration ||
			repeated.m_iLeaseGeneration != m_iLeaseGeneration || repeated.m_iVehicleGeneration != m_iVehicleGeneration ||
			repeated.m_iAssignmentRevision != m_iAssignmentRevision || repeated.m_iPresetRevision != m_iPresetRevision) return false;
		for (int index; index < m_aMembers.Count(); index++)
			if (!repeated.m_aMembers.Contains(m_aMembers[index]) || !m_aMembers[index] || m_aMembers[index].GetID() != m_aMemberIds[index]) return false;
		return true;
	}
}

modded class AICF_GroupTrace
{
	protected bool m_bTopologyPerformanceDone;

	override void Update(AICF_FactionState state, AICF_VehicleCoordinator vehicles)
	{
		super.Update(state, vehicles);
		if (m_bTopologyPerformanceDone || !System.IsCLIParam("aicfTopologyPerformanceProbe") ||
			!Replication.IsServer() || !state || !vehicles) return;
		for (int index; index < state.GetSlotCount(); index++)
		{
			AICF_GroupSlot slot = state.GetSlot(index);
			if (!slot) continue;
			AICF_VehicleTopologyPerformanceContext scope = AICF_VehicleTopologyPerformanceContext.Capture(slot, vehicles.GetTrip(slot));
			if (!scope) continue;
			m_bTopologyPerformanceDone = true;
			AICF_BenchmarkTopology(scope);
			return;
		}
	}

	protected static bool AICF_SameTopology(array<BaseCompartmentSlot> first, array<BaseCompartmentSlot> second)
	{
		if (first.Count() != second.Count()) return false;
		for (int index; index < first.Count(); index++)
			if (first[index] != second[index] || AICF_VehiclePreset.SeatKey(first[index]) != AICF_VehiclePreset.SeatKey(second[index])) return false;
		return true;
	}

	protected void AICF_BenchmarkTopology(AICF_VehicleTopologyPerformanceContext scope)
	{
		int iterations = 1;
		string value;
		if (System.GetCLIParam("aicfTopologyPerformanceIterations", value)) iterations = Math.Clamp(value.ToInt(), 1, 3);
		AICF_VehiclePreset preset = scope.m_Trip.GetVehiclePreset();
		AICF_VehicleTopologyLegacyProbe legacy = new AICF_VehicleTopologyLegacyProbe();
		legacy.m_aSeats.Copy(preset.m_aSeats);
		int failures;
		for (int iteration; iteration < iterations; iteration++)
		{
			bool before = scope.Current();
			array<BaseCompartmentSlot> previous = {};
			array<BaseCompartmentSlot> current = {};
			int start = System.GetTickCount();
			AICF_VehicleTopologyLegacyProbe.OldCollect(scope.m_Vehicle, previous);
			int oldCollectMs = System.GetTickCount() - start;
			start = System.GetTickCount();
			AICF_VehiclePreset.Collect(scope.m_Vehicle, current);
			int collectMs = System.GetTickCount() - start;
			bool sameCollect = AICF_SameTopology(previous, current);
			start = System.GetTickCount();
			bool oldMatches = legacy.OldMatchesSeats(previous);
			int oldMatchesMs = System.GetTickCount() - start;
			start = System.GetTickCount();
			bool matches = preset.MatchesSeats(current);
			int matchesMs = System.GetTickCount() - start;
			array<BaseCompartmentSlot> previousCaptured = {};
			array<BaseCompartmentSlot> captured = {};
			start = System.GetTickCount();
			bool oldResolved = AICF_VehicleTopologyLegacyProbe.OldCollectCaptured(scope.m_Vehicle, preset, previousCaptured);
			int oldCapturedMs = System.GetTickCount() - start;
			start = System.GetTickCount();
			bool resolved = AICF_VehiclePreset.CollectCaptured(scope.m_Vehicle, preset, captured);
			int capturedMs = System.GetTickCount() - start;
			bool sameCaptured = oldResolved == resolved && AICF_SameTopology(previousCaptured, captured);
			bool after = scope.Current();
			bool passed = before && after && sameCollect && sameCaptured && oldMatches == matches;
			if (!passed) failures++;
			int insideBefore;
			int insideAfter;
			if (before) insideBefore = scope.m_aMembers.Count();
			if (after) insideAfter = scope.m_aMembers.Count();
			string details = string.Format("operation_id=%1 numeric_slot=%2 vehicle=%3 iteration=%4 passed=%5 physical_inside_before=%6 physical_inside_after=%7 exact_identity=%8 diagnostic_only=1",
				scope.m_Trip.GetOperationId(), scope.m_Slot.GetSlotId(), scope.m_VehicleId, iteration, passed, insideBefore, insideAfter, before && after);
			details += string.Format(" old_collect_ms=%1 collect_ms=%2 old_matches_ms=%3 matches_ms=%4 old_captured_ms=%5 captured_ms=%6 collect_count=%7 captured_count=%8",
				oldCollectMs, collectMs, oldMatchesMs, matchesMs, oldCapturedMs, capturedMs, current.Count(), captured.Count());
			details += string.Format(" same_collect_keys=%1 same_captured_keys=%2 old_matches=%3 matches=%4 old_resolved=%5 resolved=%6 physical_mutation=0 gameplay_writes=0",
				sameCollect, sameCaptured, oldMatches, matches, oldResolved, resolved);
			details += string.Format(" prefab=%1 equipped_members=10 inventory_raw_items=%2", preset.m_sPrefab, scope.m_iInventoryRawItems);
			Print("[AICF][TOPOLOGY_PERFORMANCE_CASE] " + details);
		}
		Print(string.Format("[AICF][TOPOLOGY_PERFORMANCE_DONE] cases=%1 failures=%2 native_objects=1 same_exact_vehicle=1 required_live_occupants=10 ordinary_fps_measurement=0", iterations, failures));
	}
}
