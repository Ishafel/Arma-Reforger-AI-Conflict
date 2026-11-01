enum AICF_ECoopVehicleRole
{
	FOOT,
	TRANSPORT,
	ARMED_LIGHT,
	APC,
	TANK
}

// Initial stable-slot intent only: no live roster transfer, spawn or transaction.
// Execution still uses the ordinary preset admission/acquisition/boarding flows.
class AICF_CoopFleetDefaults
{
	static AICF_ECoopVehicleRole Role(int slotId, bool tanks)
	{
		if (slotId < 0 || slotId >= AICF_Stage1Config.GROUP_SLOTS_PER_FACTION) return AICF_ECoopVehicleRole.FOOT;
		if (slotId < 4) return AICF_ECoopVehicleRole.TRANSPORT;
		if (slotId == 4) return AICF_ECoopVehicleRole.ARMED_LIGHT;
		if (slotId < 7) return AICF_ECoopVehicleRole.APC;
		if (tanks && slotId < 9) return AICF_ECoopVehicleRole.TANK;
		return AICF_ECoopVehicleRole.FOOT;
	}

	static void Apply(AICF_FactionState state, AICF_MatchRules rules)
	{
		if (!Replication.IsServer() || !state || !rules || !rules.IsValid() || rules.GetMode() == AICF_EMatchMode.STANDARD) return;
		if (rules.GetManagedSlotCount(state.GetFactionKey()) <= 0 || state.GetSlotCount() == 0) return;
		AICF_ContentProfile profile = AICF_ContentProfile.GetActive();
		FactionKey stableKey = profile.GetStableFactionKey(state.GetFactionKey());
		if (stableKey != "US" && stableKey != "USSR") return;
		SCR_CampaignFaction faction = SCR_CampaignFaction.Cast(GetGame().GetFactionManager().GetFactionByKey(state.GetFactionKey()));
		if (!faction) return;
		AICF_VehiclePresetCatalog catalog = new AICF_VehiclePresetCatalog(faction);
		for (int role = AICF_ECoopVehicleRole.TRANSPORT; role <= AICF_ECoopVehicleRole.TANK; role++)
		{
			if (role == AICF_ECoopVehicleRole.TANK && !profile.HasCoopTankSlots()) continue;
			array<string> suffixes = {};
			profile.BuildCoopVehicleSuffixPreference(stableKey, role, suffixes);
			array<ResourceName> candidates = {};
			catalog.OwnCandidates(suffixes, candidates);
			AICF_VehiclePreset recipe;
			int size;
			string reason = "OWN_FACTION_MODEL_UNAVAILABLE";
			foreach (ResourceName prefab : candidates)
			{
				if (!catalog.Inspect(prefab, reason)) continue;
				recipe = catalog.Draft(prefab);
				array<BaseCompartmentSlot> seats = catalog.Seats();
				size = Math.Min(AICF_Stage1Config.MAX_GROUP_SIZE, seats.Count());
				array<int> members = {};
				array<int> mandatory = {};
				for (int member; member < size; member++) members.Insert(member);
				foreach (int index, BaseCompartmentSlot seat : seats)
					if (profile.IsVehiclePresetCrewSeat(prefab, stableKey, seat)) mandatory.Insert(index);
				if (role == AICF_ECoopVehicleRole.TRANSPORT && size < AICF_Stage1Config.MAX_GROUP_SIZE) { recipe = null; reason = "TRANSPORT_TOO_SMALL"; continue; }
				if (!AICF_VehicleSeatAssignment.AutoAssign(recipe, members, mandatory)) { recipe = null; reason = "MANDATORY_CREW_UNAVAILABLE"; continue; }
				if (role != AICF_ECoopVehicleRole.TRANSPORT) recipe.m_iBehavior = AICF_EVehicleBehavior.COMBAT;
				Print(string.Format("[AICF][COOP_FLEET_MODEL_INSPECTED] faction=%1 role=%2 prefab=%3 seats=%4 mandatory=%5 assigned=%6 tracked_simulation=%7 wheeled_simulation=%8", stableKey, role, prefab, seats.Count(), mandatory.Count(), size, catalog.HasInspectedTrackedSimulation(), catalog.HasInspectedWheeledSimulation()));
				break;
			}
			catalog.Clear();
			for (int slotId; slotId < state.GetSlotCount(); slotId++)
			{
				if (Role(slotId, profile.HasCoopTankSlots()) != role) continue;
				AICF_GroupSlot slot = state.GetSlot(slotId);
				// Never reapply defaults over a player binding or a materialized slot.
				if (!slot || slot.GetState() != AICF_EGroupSlotState.EMPTY || slot.HasVehiclePreset() || slot.GetVehiclePresetRevision() != 0) continue;
				if (!recipe)
				{
					Print(string.Format("[AICF][COOP_FLEET_DEFAULT_UNAVAILABLE] faction=%1 slot=%2 role=%3 reason=%4", stableKey, slotId, role, reason), LogLevel.WARNING);
					continue;
				}
				if (!slot.SetVehiclePreset(recipe, 0)) continue;
				slot.SetDesiredSize(size);
				Print(string.Format("[AICF][COOP_FLEET_DEFAULT] faction=%1 slot=%2 role=%3 prefab=%4 size=%5 behavior=%6", stableKey, slotId, role, recipe.m_sPrefab, size, recipe.m_iBehavior));
			}
		}
	}
}
