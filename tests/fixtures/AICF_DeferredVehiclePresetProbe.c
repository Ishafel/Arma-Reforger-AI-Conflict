// Isolated server/client actual RPC → ordinary paid recruitment → deferred crew.
// No player input, GUI automation, synthetic roster, health or economy changes.
class AICF_DeferredReconnectReceipt
{
	static SCR_PlayerController s_First;
	static int s_iPlayer;
	static string s_sAssigned;
	static bool s_bSecondRead;
	static void Clear() { s_First = null; s_iPlayer = 0; s_sAssigned = ""; s_bSecondRead = false; }
}

modded class AICF_VehiclePresetService
{
	override void Request(SCR_PlayerController player, int token, int slotId, int revision, int generation, int libraryRevision, int operation, string payload)
	{
		super.Request(player, token, slotId, revision, generation, libraryRevision, operation, payload);
		string flag;
		if (!System.GetCLIParam("aicfDeferredVehiclePresetSequential", flag) || !Replication.IsServer()) return;
		AICF_GroupSlot slot;
		SCR_CampaignFaction faction;
		if (!m_Match.ResolveVehiclePresetContext(player, slotId, slot, faction) || slotId != 0 || !slot.HasVehiclePreset()) return;
		array<int> connected = {};
		GetGame().GetPlayerManager().GetPlayers(connected);
		if (operation == AICF_EVehiclePresetOperation.CONFIRM_CREW && AICF_DeferredReconnectReceipt.s_sAssigned.IsEmpty())
		{
			AICF_DeferredReconnectReceipt.s_First = player;
			AICF_DeferredReconnectReceipt.s_iPlayer = player.GetPlayerId();
			AICF_DeferredReconnectReceipt.s_sAssigned = slot.GetVehiclePresetData();
		}
		else if (operation == AICF_EVehiclePresetOperation.READ && !AICF_DeferredReconnectReceipt.s_sAssigned.IsEmpty() &&
			player != AICF_DeferredReconnectReceipt.s_First && slot.GetVehiclePresetData() == AICF_DeferredReconnectReceipt.s_sAssigned &&
			!connected.Contains(AICF_DeferredReconnectReceipt.s_iPlayer))
		{
			AICF_DeferredReconnectReceipt.s_bSecondRead = true;
			Print("[AICF][DEFERRED_JIP_SERVER_READ] owner_context=1 same_binding=1 first_disconnected=1");
		}
	}
}

modded class AICF_OrderPlanner
{
	override bool CanRecruitInfantry(AICF_GroupSlot slot, SCR_CampaignFaction faction)
	{
		string flag;
		if (System.GetCLIParam("aicfDeferredVehiclePresetProbe", flag) && slot && faction &&
			slot.GetSlotId() == 0 && AICF_ContentProfile.GetActive().GetStableFactionKey(faction.GetFactionKey()) == "USSR" && !slot.HasVehiclePreset()) return false;
		return super.CanRecruitInfantry(slot, faction);
	}
}

modded class AICF_VehicleCrewReturn
{
	override bool Commit(AICF_VehicleCrewPlan plan, SCR_CampaignFaction faction, AICF_VehiclePreset preset, out string reason)
	{
		string flag;
		if (!System.GetCLIParam("aicfDeferredVehiclePresetProbe", flag)) return super.Commit(plan, faction, preset, reason);
		IEntity kept = plan.m_Slot.GetRosterMember(plan.m_aKept[0]);
		string before, after;
		bool captured = AICF_LoadoutInventory.Capture(kept, before);
		bool result = super.Commit(plan, faction, preset, reason);
		if (result) Print(string.Format("[AICF][DEFERRED_CREW_COMMIT] original_kept=%1 retained=%2 kit_unchanged=%3 generation=%4",
			kept.GetID(), plan.m_Slot.GetRosterMember(0) == kept, captured && AICF_LoadoutInventory.Capture(kept, after) && before == after, plan.m_Slot.GetSpawnGeneration()));
		return result;
	}
}

modded class AICF_MatchController
{
	protected int m_iDeferredProbeStarted;
	protected bool m_bDeferredProbeBound;
	protected bool m_bDeferredProbeReduced;
	protected bool m_bDeferredProbeDone;
	protected IEntity m_DeferredKept;
	protected int m_iDeferredProbeChecks;
	protected int m_iDeferredProbeFailures;

	protected void AICF_DeferredCheck(string rule, bool passed)
	{
		m_iDeferredProbeChecks++;
		if (!passed) m_iDeferredProbeFailures++;
		Print(string.Format("[AICF][DEFERRED_CHECK] rule=%1 passed=%2", rule, passed));
	}

	override protected void Update()
	{
		super.Update();
		string flag;
		if (!Replication.IsServer() || !System.GetCLIParam("aicfDeferredVehiclePresetProbe", flag) || !m_bRosterReady || m_bDeferredProbeDone) return;
		if (!m_iDeferredProbeStarted)
		{
			m_iDeferredProbeStarted = System.GetTickCount();
			// Isolate the one paid recruitment operation; original target slot is untouched.
			for (int other = 1; other < m_USSRState.GetSlotCount(); other++) m_USSRState.GetSlot(other).SetDesiredSize(1);
			for (int west = 0; west < m_USState.GetSlotCount(); west++) m_USState.GetSlot(west).SetDesiredSize(1);
		}
		if (System.GetTickCount() - m_iDeferredProbeStarted > 660000) { AICF_DeferredCheck("TIMEOUT", false); AICF_DeferredClose(); return; }
		AICF_GroupSlot slot = m_USSRState.GetSlot(0);
		if (!slot || !slot.IsCombatReady()) return;
		if (!slot.HasVehiclePreset()) return;
		if (!m_bDeferredProbeBound)
		{
			m_bDeferredProbeBound = true;
			AICF_DeferredCheck("BOUND_BEFORE_MUSTER_COMPLETE", slot.GetDesiredSize() == 10 && slot.GetGroup().GetAgentsCount() < 10 && slot.IsVehicleCrewResizePending());
			AICF_DeferredCheck("NO_VEHICLE_BEFORE_MUSTER", !m_VehicleCoordinator.GetTrip(slot) || !m_VehicleCoordinator.GetTrip(slot).GetLease());
		}
		string sequential;
		if (System.GetCLIParam("aicfDeferredVehiclePresetSequential", sequential))
		{
			if (AICF_DeferredReconnectReceipt.s_bSecondRead)
			{
				AICF_DeferredCheck("BINDING_SURVIVED_RECONNECT", slot.GetVehiclePresetData() == AICF_DeferredReconnectReceipt.s_sAssigned);
				m_bDeferredProbeDone = true;
				Print(string.Format("[AICF][DEFERRED_SEQUENTIAL_DONE] checks=%1 failures=%2", m_iDeferredProbeChecks, m_iDeferredProbeFailures));
				SCR_GameModeCampaign.Cast(GetGame().GetGameMode()).AICF_DeferredScheduleClose(10000);
			}
			return;
		}
		if (!m_bDeferredProbeReduced && slot.GetDesiredSize() == 1)
		{
			m_bDeferredProbeReduced = true;
			m_DeferredKept = slot.GetRosterMember(0);
			AICF_DeferredCheck("DEFERRED_CREW_REDUCED", m_DeferredKept && !slot.IsVehicleCrewResizePending() && slot.GetGroup().GetAgentsCount() == 1 && slot.GetEquipmentSourceIndex(0) == 9);
			vector target = m_DeferredKept.GetOrigin() + "400 0 0";
			target[1] = GetGame().GetWorld().GetSurfaceY(target[0], target[2]);
			AICF_DeferredCheck("POST_MUSTER_POINT_ORDER", m_OrderPlanner.AssignPlayerPointOrder(slot, m_USSRFaction, target, false));
		}
		if (!m_bDeferredProbeReduced) return;
		AICF_TransportTrip trip = m_VehicleCoordinator.GetTrip(slot);
		if (!trip || !trip.GetLease() || !trip.GetLease().GetVehicle()) return;
		Vehicle vehicle = trip.GetLease().GetVehicle();
		array<BaseCompartmentSlot> seats = {};
		AICF_VehiclePreset.Collect(vehicle, seats);
		foreach (BaseCompartmentSlot seat : seats)
			if (seat.GetType() == ECompartmentType.PILOT && seat.GetOccupant() == m_DeferredKept)
			{
				CompartmentAccessComponent access = CompartmentAccessComponent.Cast(m_DeferredKept.FindComponent(CompartmentAccessComponent));
				if (!access || access.GetCompartment() != seat || access.IsGettingIn() || access.IsGettingOut()) continue;
				AICF_DeferredCheck("PURCHASED_EXACT_DRIVER", vehicle.GetPrefabData().GetPrefabName().Contains("UAZ469") && slot.GetRosterMember(0) == m_DeferredKept);
				AICF_DeferredClose();
				return;
			}
	}

	protected void AICF_DeferredClose()
	{
		m_bDeferredProbeDone = true;
		Print(string.Format("[AICF][DEFERRED_DONE] checks=%1 failures=%2", m_iDeferredProbeChecks, m_iDeferredProbeFailures));
		SCR_GameModeCampaign.Cast(GetGame().GetGameMode()).AICF_DeferredScheduleClose(10000);
	}
}

modded class SCR_GameModeCampaign
{
	void AICF_DeferredScheduleClose(int delay)
	{
		GetGame().GetCallqueue().CallLater(AICF_DeferredRequestClose, delay, false);
	}
	protected void AICF_DeferredRequestClose() { GetGame().RequestClose(); }
	protected int m_iDeferredClientStarted;
	protected int m_iDeferredClientStep;
	protected int m_iDeferredClientToken;
	protected ref AICF_VehiclePreset m_DeferredClientPreset;
	protected int m_iDeferredClientChecks;
	protected int m_iDeferredClientFailures;
	protected bool m_bDeferredClientSpawnRequested;
	protected bool m_bDeferredClientLoadoutRequested;
	protected bool m_bDeferredClientAvatarSeen;

	protected bool AICF_DeferredNativeAvatar(SCR_PlayerController player, Faction faction)
	{
		if (player.GetControlledEntity())
		{
			if (!m_bDeferredClientAvatarSeen)
			{
				m_bDeferredClientAvatarSeen = true;
				Print("[AICF][DEFERRED_NATIVE_PLAYER] stock_respawn=1 managed_possession=0");
			}
			return true;
		}
		array<ref SCR_BasePlayerLoadout> loadouts = {};
		GetGame().GetLoadoutManager().GetPlayerLoadoutsByFaction(faction, loadouts);
		SCR_BasePlayerLoadout loadout;
		foreach (SCR_BasePlayerLoadout candidate : loadouts)
			if (candidate && !SCR_PlayerArsenalLoadout.Cast(candidate)) { loadout = candidate; break; }
		SCR_PlayerLoadoutComponent component = SCR_PlayerLoadoutComponent.Cast(player.FindComponent(SCR_PlayerLoadoutComponent));
		if (!component || !loadout) return false;
		if (component.GetLoadout() != loadout)
		{
			if (!m_bDeferredClientLoadoutRequested) { m_bDeferredClientLoadoutRequested = true; component.RequestLoadout(loadout); }
			return false;
		}
		if (m_bDeferredClientSpawnRequested) return false;
		array<SCR_SpawnPoint> points = SCR_SpawnPoint.GetSpawnPointsForFaction(faction.GetFactionKey());
		if (!points || points.IsEmpty() || loadout.GetLoadoutResource().IsEmpty()) return false;
		SCR_SpawnPoint chosen;
		foreach (SCR_SpawnPoint point : points)
			if (point && point.IsSpawnPointActive() && point.IsSpawnPointEnabled() && point.GetRplId().IsValid()) { chosen = point; break; }
		SCR_RespawnComponent respawn = SCR_RespawnComponent.Cast(player.GetRespawnComponent());
		if (chosen && respawn) m_bDeferredClientSpawnRequested = respawn.RequestSpawn(new SCR_SpawnPointSpawnData(loadout.GetLoadoutResource(), chosen.GetRplId()));
		return false;
	}

	override void OnGameStart()
	{
		super.OnGameStart();
		string flag;
		if (Replication.IsServer() || !System.GetCLIParam("aicfDeferredVehiclePresetProbe", flag)) return;
		m_iDeferredClientStarted = System.GetTickCount();
		GetGame().GetCallqueue().CallLater(AICF_DeferredClientTick, 1200, true);
	}

	protected void AICF_DeferredClientCheck(string rule, bool passed)
	{
		m_iDeferredClientChecks++;
		if (!passed) m_iDeferredClientFailures++;
		Print(string.Format("[AICF][DEFERRED_CLIENT_CHECK] rule=%1 passed=%2", rule, passed));
	}

	protected void AICF_DeferredClientTick()
	{
		if (System.GetTickCount() - m_iDeferredClientStarted > 690000) { AICF_DeferredClientCheck("TIMEOUT", false); AICF_DeferredClientClose(); return; }
		SCR_PlayerController player = SCR_PlayerController.Cast(GetGame().GetPlayerController());
		if (!player) return;
		Faction faction = GetGame().GetFactionManager().GetFactionByKey(AICF_ContentProfile.GetActive().GetRuntimeFactionKey("USSR"));
		SCR_PlayerFactionAffiliationComponent affiliation = SCR_PlayerFactionAffiliationComponent.Cast(player.FindComponent(SCR_PlayerFactionAffiliationComponent));
		if (!affiliation || !faction) return;
		if (affiliation.GetAffiliatedFaction() != faction) { affiliation.RequestFaction(faction); return; }
		if (!AICF_DeferredNativeAvatar(player, faction)) return;
		if (m_iDeferredClientToken > 0 && player.AICF_VehiclePresetToken() != m_iDeferredClientToken) return;
		JsonLoadContext json = new JsonLoadContext();
		int revision = -1, generation = -1, library = -1, size, confirmation;
		string assigned, draft, catalog;
		array<int> active = {}, selectable = {};
		json.LoadFromString(player.AICF_VehiclePresetView());
		json.ReadValue("revision", revision);
		json.ReadValue("generation", generation);
		json.ReadValue("size", size);
		json.ReadValue("confirmation", confirmation);
		json.ReadValue("activeMembers", active);
		json.ReadValue("selectableMembers", selectable);
		json.ReadValue("assigned", assigned);
		json.ReadValue("draft", draft);
		json.ReadValue("catalog", catalog);
		string jip;
		if (m_iDeferredClientStep == 1 && System.GetCLIParam("aicfDeferredVehiclePresetJIP", jip))
		{
			AICF_VehiclePreset restored = AICF_VehiclePreset.Decode(assigned);
			bool valid = player.AICF_VehiclePresetAccepted() && restored && restored.m_sFaction == "USSR" && restored.m_sPrefab.Contains("UAZ469_uncovered_CIV_blue.et") && restored.m_aMembers.Contains(9);
			Print(string.Format("[AICF][DEFERRED_JIP_CHECK] assigned_readback=1 passed=%1", valid));
			if (!valid) m_iDeferredClientFailures++;
			GetGame().GetCallqueue().Remove(AICF_DeferredClientTick);
			Print(string.Format("[AICF][DEFERRED_JIP_DONE] checks=1 failures=%1", m_iDeferredClientFailures));
			AICF_DeferredScheduleClose(2000);
			return;
		}
		switch (m_iDeferredClientStep)
		{
			case 0:
				m_iDeferredClientToken = player.AICF_RequestVehiclePreset(0, -1, -1, -1, AICF_EVehiclePresetOperation.READ, "");
				break;
			case 1:
				if (revision < 0) return;
				AICF_DeferredClientCheck("PARTIAL_ROSTER_FUTURE_POSITIONS", size == 10 && active.Count() < 10 && selectable.Count() == 10);
				JsonLoadContext list = new JsonLoadContext();
				array<ResourceName> prefabs = {};
				list.LoadFromString(catalog);
				list.ReadValue("prefabs", prefabs);
				ResourceName chosen;
				foreach (ResourceName prefab : prefabs)
					if (prefab.Contains("UAZ469_uncovered_CIV_blue.et")) { chosen = prefab; break; }
				AICF_DeferredClientCheck("CATALOG_NATIVE_UAZ", !chosen.IsEmpty());
				if (chosen.IsEmpty()) { AICF_DeferredClientClose(); return; }
				m_iDeferredClientToken = player.AICF_RequestVehiclePreset(0, revision, generation, -1, AICF_EVehiclePresetOperation.SELECT, chosen);
				break;
			case 2:
				m_DeferredClientPreset = AICF_VehiclePreset.Decode(draft);
				AICF_DeferredClientCheck("SELECT_NATIVE_TOPOLOGY", player.AICF_VehiclePresetAccepted() && m_DeferredClientPreset);
				if (!m_DeferredClientPreset) { AICF_DeferredClientClose(); return; }
				for (int seat; seat < m_DeferredClientPreset.m_aSeats.Count(); seat++)
				{
					array<string> parts = {};
					m_DeferredClientPreset.m_aSeats[seat].Split(":", parts, false);
					if (parts[2].ToInt() == ECompartmentType.PILOT) m_DeferredClientPreset.Assign(seat, 9);
				}
				m_iDeferredClientToken = player.AICF_RequestVehiclePreset(0, revision, generation, -1, AICF_EVehiclePresetOperation.APPLY, m_DeferredClientPreset.Encode());
				break;
			case 3:
				AICF_DeferredClientCheck("CONFIRM_BEFORE_RECRUITMENT", confirmation > 0 && assigned.IsEmpty() && size == 10 && active.Count() < 10);
				if (confirmation <= 0) { AICF_DeferredClientClose(); return; }
				m_iDeferredClientToken = player.AICF_RequestVehiclePreset(0, revision, generation, -1, AICF_EVehiclePresetOperation.CONFIRM_CREW, confirmation.ToString());
				break;
			case 4:
				AICF_DeferredClientCheck("BOUND_WHILE_RECRUITING", player.AICF_VehiclePresetAccepted() && !assigned.IsEmpty() && size == 10 && active.Count() < 10);
				string sequentialClient;
				if (System.GetCLIParam("aicfDeferredVehiclePresetSequential", sequentialClient)) { AICF_DeferredClientClose(); return; }
				break;
			case 5:
				if (size != 1) return;
				AICF_VehiclePreset applied = AICF_VehiclePreset.Decode(assigned);
				AICF_DeferredClientCheck("DEFERRED_CREW_OWNER_READBACK", applied && applied.m_aMembers.Contains(0) && !applied.m_aMembers.Contains(9) && active.Count() == 1);
				AICF_DeferredClientClose();
				return;
		}
		m_iDeferredClientStep++;
	}

	protected void AICF_DeferredClientClose()
	{
		GetGame().GetCallqueue().Remove(AICF_DeferredClientTick);
		Print(string.Format("[AICF][DEFERRED_CLIENT_DONE] checks=%1 failures=%2", m_iDeferredClientChecks, m_iDeferredClientFailures));
		AICF_DeferredScheduleClose(2000);
	}

	override void OnGameEnd()
	{
		GetGame().GetCallqueue().Remove(AICF_DeferredClientTick);
		GetGame().GetCallqueue().Remove(AICF_DeferredRequestClose);
		AICF_DeferredReconnectReceipt.Clear();
		super.OnGameEnd();
	}
}
