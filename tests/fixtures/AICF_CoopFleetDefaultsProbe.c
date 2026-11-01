// Stage-only readback of initial production bindings. No fixture vehicle/crew spawn.
modded class AICF_MatchController
{
	protected bool m_bCoopFleetProbeDone;
	protected ref array<ResourceName> m_aCoopCrewInspected = {};
	protected ref array<bool> m_aCoopCrewPassed = {};

	override protected void Update()
	{
		super.Update();
		if (!System.IsCLIParam("aicfCoopFleetProbe") || m_bCoopFleetProbeDone || !m_bRosterReady || m_bStopped) return;
		m_bCoopFleetProbeDone = true;
		int checks;
		int failures;
		AICF_CoopFleetCheck(m_USState, m_USFaction, checks, failures);
		AICF_CoopFleetCheck(m_USSRState, m_USSRFaction, checks, failures);
		Print(string.Format("[AICF][COOP_FLEET_DONE] checks=%1 failures=%2 target_slots_per_faction=10 profile=%3", checks, failures, m_ContentProfile.GetProfileKey()));
		GetGame().GetCallqueue().CallLater(AICF_CoopFleetProbeClose, 5000, false);
	}

	protected void AICF_CoopFleetCheck(AICF_FactionState state, SCR_CampaignFaction faction, inout int checks, inout int failures)
	{
		FactionKey stable = m_ContentProfile.GetStableFactionKey(faction.GetFactionKey());
		bool shape = state && state.GetSlotCount() == 10 && m_Config.GetAICommanderMode() == "BOTH" && m_Config.GetMinimumManagedAgents() == 200;
		checks++;
		if (!shape) failures++;
		Print(string.Format("[AICF][COOP_FLEET_CASE] faction=%1 case=SHAPE pass=%2", stable, shape));
		if (!state) return;
		AICF_VehiclePresetCatalog catalog = new AICF_VehiclePresetCatalog(faction);
		Print(string.Format("[AICF][COOP_FLEET_CATALOG] faction=%1 catalog=%2", stable, catalog.List()));
		catalog.Clear();
		for (int slotId; slotId < state.GetSlotCount(); slotId++)
		{
			AICF_GroupSlot slot = state.GetSlot(slotId);
			AICF_ECoopVehicleRole role = AICF_CoopFleetDefaults.Role(slotId, m_ContentProfile.HasCoopTankSlots());
			AICF_VehiclePreset preset = AICF_VehiclePreset.Decode(slot.GetVehiclePresetData());
			bool passed = role == AICF_ECoopVehicleRole.FOOT && !preset;
			string reason = "FOOT";
			if (role != AICF_ECoopVehicleRole.FOOT)
			{
				reason = "DEFAULT_UNAVAILABLE";
				if (preset)
				{
					array<string> suffixes = {};
					m_ContentProfile.BuildCoopVehicleSuffixPreference(stable, role, suffixes);
					bool candidate;
					foreach (string suffix : suffixes)
						if (preset.m_sPrefab.Contains(suffix)) candidate = true;
					int assigned;
					foreach (int member : preset.m_aMembers)
						if (member >= 0) assigned++;
					int behavior = AICF_EVehicleBehavior.COMBAT;
					if (role == AICF_ECoopVehicleRole.TRANSPORT) behavior = AICF_EVehicleBehavior.TRANSPORT;
					passed = candidate && preset.m_sFaction == stable && preset.m_sProfile == m_ContentProfile.GetProfileKey() &&
						preset.m_iBehavior == behavior && preset.m_iReturnDistance == 300 &&
						assigned == slot.GetDesiredSize() && preset.ValidateShape(reason) && AICF_CoopFleetInspectCrew(catalog, slot, preset, role);
				}
			}
			checks++;
			if (!passed) failures++;
			Print(string.Format("[AICF][COOP_FLEET_CASE] faction=%1 slot=%2 role=%3 pass=%4 desired_size=%5 reason=%6", stable, slotId, role, passed, slot.GetDesiredSize(), reason));
		}
	}

	protected bool AICF_CoopFleetInspectCrew(AICF_VehiclePresetCatalog catalog, AICF_GroupSlot slot, AICF_VehiclePreset preset, AICF_ECoopVehicleRole role)
	{
		int previous = m_aCoopCrewInspected.Find(preset.m_sPrefab);
		if (previous >= 0) return m_aCoopCrewPassed[previous];
		string reason;
		bool passed = catalog.Inspect(preset.m_sPrefab, reason) && preset.MatchesSeats(catalog.Seats());
		int crew, cargoCrew;
		bool missingLoaderRejected = true;
		if (passed)
		{
			array<BaseCompartmentSlot> seats = catalog.Seats();
			foreach (int index, BaseCompartmentSlot seat : seats)
			{
				bool policyCrew = AICF_VehicleBehaviorFlow.IsCrewSeat(seat, preset);
				bool mandatory = m_ContentProfile.IsVehiclePresetCrewSeat(preset.m_sPrefab, preset.m_sFaction, seat);
				if (policyCrew != mandatory || (mandatory && preset.m_aMembers[index] < 0)) passed = false;
				if (policyCrew) crew++;
				if (role == AICF_ECoopVehicleRole.TANK && !policyCrew) passed = false;
				if (policyCrew && seat.GetType() == ECompartmentType.CARGO)
				{
					cargoCrew++;
					AICF_VehiclePreset incomplete = AICF_VehiclePreset.Decode(preset.Encode());
					incomplete.m_aMembers[index] = -1;
					if (incomplete.ValidatePlannedRoster(slot, seats, reason) || reason != "MANDATORY_CREW_MISSING") missingLoaderRejected = false;
				}
			}
		}
		passed = passed && missingLoaderRejected;
		Print(string.Format("[AICF][COOP_FLEET_CREW_INSPECTED] faction=%1 role=%2 seats=%3 crew=%4 cargo_crew=%5 missing_loader_rejected=%6 passed=%7 prefab=%8", preset.m_sFaction, role, preset.m_aSeats.Count(), crew, cargoCrew, missingLoaderRejected, passed, preset.m_sPrefab));
		catalog.Clear();
		m_aCoopCrewInspected.Insert(preset.m_sPrefab);
		m_aCoopCrewPassed.Insert(passed);
		return passed;
	}

	void AICF_CoopFleetProbeClose()
	{
		GetGame().RequestClose();
	}

	override protected void Stop(bool cleanupEntities)
	{
		GetGame().GetCallqueue().Remove(AICF_CoopFleetProbeClose);
		super.Stop(cleanupEntities);
	}
}

modded class SCR_GameModeCampaign
{
	override void OnGameStart()
	{
		super.OnGameStart();
		if (Replication.IsServer() && System.IsCLIParam("aicfCoopFleetProbe"))
			GetGame().GetCallqueue().CallLater(AICF_CoopFleetProbeDeadline, 180000, false);
	}

	void AICF_CoopFleetProbeDeadline()
	{
		Print("[AICF][COOP_FLEET_TIMEOUT] readiness did not complete", LogLevel.ERROR);
		GetGame().RequestClose();
	}

	override void OnGameEnd()
	{
		GetGame().GetCallqueue().Remove(AICF_CoopFleetProbeDeadline);
		super.OnGameEnd();
	}
}
