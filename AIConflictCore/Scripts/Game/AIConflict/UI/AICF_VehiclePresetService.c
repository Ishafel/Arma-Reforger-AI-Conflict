enum AICF_EVehiclePresetOperation
{
	READ,
	SELECT,
	APPLY,
	SAVE,
	LOAD,
	DELETE,
	RESET,
	CONFIRM_CREW,
	CANCEL_CREW
}

class AICF_VehiclePresetViewer
{
	SCR_PlayerController m_Player;
	int m_iSlot;
	ref AICF_VehiclePreset m_Draft;
	string m_sSeatNames;
	ref AICF_VehicleCrewPlan m_Plan;
	int m_iConfirmation;
}

class AICF_VehiclePresetService
{
	protected AICF_MatchController m_Match;
	protected int m_iNextConfirmation;
	protected ref AICF_VehiclePresetStore m_Store = new AICF_VehiclePresetStore();
	protected ref array<ref AICF_VehiclePresetViewer> m_aViewers = {};

	void AICF_VehiclePresetService(AICF_MatchController match) { m_Match = match; }

	void Request(SCR_PlayerController player, int token, int slotId, int revision, int generation, int libraryRevision, int operation, string payload)
	{
		if (!Replication.IsServer() || !player) return;
		AICF_GroupSlot slot;
		SCR_CampaignFaction faction;
		string reason = "COMMAND_NOT_ALLOWED";
		if (!m_Match.ResolveVehiclePresetContext(player, slotId, slot, faction))
		{
			player.AICF_VehiclePresetResult(token, false, reason, "");
			return;
		}
		AICF_VehiclePresetViewer viewer = Viewer(player, slotId);
		AICF_VehiclePresetCatalog catalog = new AICF_VehiclePresetCatalog(faction);
		AICF_VehicleCrewPlan confirmation = viewer.m_Plan;
		int confirmationId = viewer.m_iConfirmation;
		viewer.m_Plan = null;
		viewer.m_iConfirmation = 0;
		bool accepted;
		bool libraryOperation = operation == AICF_EVehiclePresetOperation.SAVE || operation == AICF_EVehiclePresetOperation.LOAD || operation == AICF_EVehiclePresetOperation.DELETE;
		if (operation < 0 || operation > AICF_EVehiclePresetOperation.CANCEL_CREW) reason = "INVALID_REQUEST";
		else if (operation != AICF_EVehiclePresetOperation.READ && (revision != slot.GetVehiclePresetRevision() || generation != slot.GetSpawnGeneration())) reason = "REVISION_CONFLICT";
		else if (libraryOperation && libraryRevision != m_Store.Revision()) reason = "LIBRARY_CONFLICT";
		else if (operation == AICF_EVehiclePresetOperation.READ)
		{
			viewer.m_Draft = AICF_VehiclePreset.Decode(slot.GetVehiclePresetData());
			accepted = true;
			reason = "READY";
		}
		else if (operation == AICF_EVehiclePresetOperation.SELECT)
		{
			if (catalog.Inspect(payload, reason))
			{
				viewer.m_Draft = catalog.Draft(payload);
				accepted = true;
			}
		}
		else if (operation == AICF_EVehiclePresetOperation.CANCEL_CREW)
		{
			accepted = true;
			reason = "READY";
		}
		else if (operation == AICF_EVehiclePresetOperation.RESET)
		{
			accepted = slot.SetVehiclePreset(null, revision);
			if (accepted) reason = "AUTOMATIC_POLICY_RESTORED";
		}
		else if (operation == AICF_EVehiclePresetOperation.LOAD || operation == AICF_EVehiclePresetOperation.DELETE)
		{
			int id = payload.ToInt(-1);
			AICF_VehiclePreset loaded;
			if (id >= 0 && id.ToString() == payload) loaded = m_Store.Get(id);
			reason = "PRESET_UNAVAILABLE";
			if (InContext(loaded, faction))
			{
				if (operation == AICF_EVehiclePresetOperation.DELETE) accepted = m_Store.Delete(id, reason);
				else if (catalog.Inspect(loaded.m_sPrefab, reason) && loaded.MatchesSeats(catalog.Seats()))
					{ viewer.m_Draft = loaded; accepted = true; reason = "LOADED"; }
			}
		}
		else
		{
			string candidateData = payload;
			if (operation == AICF_EVehiclePresetOperation.CONFIRM_CREW)
			{
				candidateData = "";
				if (confirmation && confirmationId > 0 && payload == confirmationId.ToString() && confirmation.Current(slot, confirmation.m_sPreset))
					candidateData = confirmation.m_sPreset;
			}
			AICF_VehiclePreset candidate = AICF_VehiclePreset.Decode(candidateData);
			reason = "INVALID_PRESET";
			if (InContext(candidate, faction) && catalog.Inspect(candidate.m_sPrefab, reason))
			{
				if (!candidate.MatchesSeats(catalog.Seats())) reason = "SEAT_TOPOLOGY_CHANGED";
				else if (operation == AICF_EVehiclePresetOperation.SAVE) accepted = m_Store.Save(candidate, reason);
				else if ((operation == AICF_EVehiclePresetOperation.APPLY || operation == AICF_EVehiclePresetOperation.CONFIRM_CREW) &&
					candidate.ValidateRoster(slot, catalog.Seats(), reason, true) && m_Match.PreflightVehiclePreset(slot, faction, candidate, reason))
				{
					AICF_VehicleCrewPlan plan = new AICF_VehicleCrewPlan(slot, candidate);
					if (!plan.m_aReturning.IsEmpty() || slot.GetDesiredSize() != plan.m_aKept.Count())
					{
						reason = "CREW_RETURN_UNAVAILABLE";
						if (m_Match.CanResizeVehicleCrew(slot) && m_Match.VehicleCrewReturn().CanReturn(plan, faction, reason))
						{
							if (operation == AICF_EVehiclePresetOperation.CONFIRM_CREW && confirmation)
								accepted = m_Match.VehicleCrewReturn().Commit(confirmation, faction, candidate, reason);
							else
							{
								viewer.m_Plan = plan;
								viewer.m_iConfirmation = ++m_iNextConfirmation;
								viewer.m_Draft = candidate;
								reason = "CONFIRM_CREW_REDUCTION";
							}
						}
					}
					else if (candidate.ValidateRoster(slot, catalog.Seats(), reason))
					{
						accepted = slot.SetVehiclePreset(candidate, revision);
						if (accepted) reason = "WAITING_FOR_VEHICLE_LIFECYCLE";
					}
				}

				if (accepted) viewer.m_Draft = candidate;
			}
		}
		string presentationReason;
		if (viewer.m_Draft && catalog.Inspect(viewer.m_Draft.m_sPrefab, presentationReason)) viewer.m_sSeatNames = catalog.SeatNames();
		// Presentation не подменяет результат последней операции.
		catalog.Clear();
		Publish(viewer, slot, faction, catalog);
		player.AICF_VehiclePresetResult(token, accepted, reason, player.AICF_VehiclePresetView());
		AICF_Stage4Diagnostics.Info("VEHICLE_PRESET_RESULT", string.Format(
			"player=%1 faction=%2 slot=%3 token=%4 operation=%5 accepted=%6 reason=%7 revision=%8 generation=%9",
			player.GetPlayerId(), faction.GetFactionKey(), slotId, token, operation, accepted, reason, slot.GetVehiclePresetRevision(), slot.GetSpawnGeneration()));
	}

	protected bool InContext(AICF_VehiclePreset preset, SCR_CampaignFaction faction)
	{
		return preset && preset.m_sProfile == AICF_ContentProfile.GetActive().GetProfileKey() &&
			preset.m_sFaction == AICF_ContentProfile.GetActive().GetStableFactionKey(faction.GetFactionKey());
	}

	protected AICF_VehiclePresetViewer Viewer(SCR_PlayerController player, int slotId)
	{
		foreach (AICF_VehiclePresetViewer viewer : m_aViewers)
			if (viewer.m_Player == player)
			{
				if (viewer.m_iSlot != slotId) { viewer.m_Draft = null; viewer.m_sSeatNames = ""; viewer.m_Plan = null; viewer.m_iConfirmation = 0; viewer.m_iSlot = slotId; }
				return viewer;
			}
		AICF_VehiclePresetViewer created = new AICF_VehiclePresetViewer();
		created.m_Player = player;
		created.m_iSlot = slotId;
		m_aViewers.Insert(created);
		return created;
	}

	protected void Publish(AICF_VehiclePresetViewer viewer, AICF_GroupSlot slot, SCR_CampaignFaction faction, AICF_VehiclePresetCatalog catalog)
	{
		JsonSaveContext json = new JsonSaveContext();
		json.WriteValue("slot", slot.GetSlotId());
		json.WriteValue("faction", AICF_ContentProfile.GetActive().GetStableFactionKey(faction.GetFactionKey()));
		json.WriteValue("revision", slot.GetVehiclePresetRevision());
		json.WriteValue("generation", slot.GetSpawnGeneration());
		json.WriteValue("size", slot.GetDesiredSize());
		array<int> activeMembers = {};
		for (int member; member < AICF_Stage1Config.MAX_GROUP_SIZE; member++)
			if (AICF_GroupRuntime.IsAliveCharacter(slot.GetRosterMember(member))) activeMembers.Insert(member);
		json.WriteValue("activeMembers", activeMembers);
		json.WriteValue("status", m_Match.VehiclePresetStatus(slot));
		if (viewer.m_Plan && !viewer.m_Plan.Current(slot, viewer.m_Plan.m_sPreset)) { viewer.m_Plan = null; viewer.m_iConfirmation = 0; }
		json.WriteValue("confirmation", viewer.m_iConfirmation);
		int kept, returning;
		if (viewer.m_Plan) { kept = viewer.m_Plan.m_aKept.Count(); returning = viewer.m_Plan.m_aReturning.Count(); }
		json.WriteValue("kept", kept);
		json.WriteValue("returning", returning);
		string draft;
		if (viewer.m_Draft) draft = viewer.m_Draft.Encode();
		json.WriteValue("draft", draft);
		json.WriteValue("assigned", slot.GetVehiclePresetData());
		json.WriteValue("seatNames", viewer.m_sSeatNames);
		json.WriteValue("catalog", catalog.List());
		json.WriteValue("library", m_Store.List(AICF_ContentProfile.GetActive().GetProfileKey(), AICF_ContentProfile.GetActive().GetStableFactionKey(faction.GetFactionKey())));
		viewer.m_Player.AICF_PublishVehiclePresetView(json.SaveToString());
	}

	void Update()
	{
		for (int i = m_aViewers.Count() - 1; i >= 0; i--)
		{
			AICF_VehiclePresetViewer viewer = m_aViewers[i];
			AICF_GroupSlot slot;
			SCR_CampaignFaction faction;
			if (!m_Match.ResolveVehiclePresetContext(viewer.m_Player, viewer.m_iSlot, slot, faction) ||
				(viewer.m_Draft && !InContext(viewer.m_Draft, faction)))
			{
				if (viewer.m_Player) viewer.m_Player.AICF_PublishVehiclePresetView("");
				m_aViewers.Remove(i);
				continue;
			}
			AICF_VehiclePresetCatalog catalog = new AICF_VehiclePresetCatalog(faction);
			Publish(viewer, slot, faction, catalog);
		}
	}

	void Stop()
	{
		foreach (AICF_VehiclePresetViewer viewer : m_aViewers)
			if (viewer.m_Player) viewer.m_Player.AICF_PublishVehiclePresetView("");
		m_aViewers.Clear();
		m_Match = null;
	}
}
