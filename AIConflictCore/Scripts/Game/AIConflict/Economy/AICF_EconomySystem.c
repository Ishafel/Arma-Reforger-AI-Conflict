// Authoritative Stage 4 composition root. It owns request pacing, combined
// deployment reservations, and abstract supply deliveries.
class AICF_EconomySystem
{
	protected AICF_Stage4Config m_Config;
	protected SCR_GameModeCampaign m_Campaign;
	protected AICF_ConflictAdapter m_ConflictAdapter;
	protected AICF_ObjectiveGraph m_Graph;
	protected int m_iReinforcementDelayMs;
	protected ref AICF_SupplyNetwork m_SupplyNetwork;
	protected ref AICF_ReinforcementBaseSelector m_BaseSelector;
	protected ref AICF_SupplyDeliverySystem m_DeliverySystem;
	protected ref array<ref AICF_ReinforcementRequest> m_aRequests = {};
	protected ref array<ref AICF_DeploymentReservation> m_aReservations = {};
	protected int m_iNextRequestId = 1;
	protected int m_iLastHeartbeatAtMs;
	protected bool m_bGraphContextReady = true;
	protected bool m_bInitialSupplyProbePending;

	void AICF_EconomySystem(
		AICF_Stage4Config config,
		SCR_GameModeCampaign campaign,
		AICF_ConflictAdapter conflictAdapter,
		AICF_ObjectiveGraph graph,
		int reinforcementDelayMs)
	{
		m_Config = config;
		m_Campaign = campaign;
		m_ConflictAdapter = conflictAdapter;
		m_Graph = graph;
		m_iReinforcementDelayMs = Math.Max(0, reinforcementDelayMs);
		m_SupplyNetwork = new AICF_SupplyNetwork(graph, conflictAdapter, config);
		m_BaseSelector = new AICF_ReinforcementBaseSelector(graph, m_SupplyNetwork, conflictAdapter);
		m_DeliverySystem = new AICF_SupplyDeliverySystem(config, m_SupplyNetwork, graph);
	}

	bool IsEnabled()
	{
		return m_Config && m_Config.GetEconomyEnabled();
	}

	int GetRetryIntervalMs()
	{
		return m_Config.GetRetryIntervalMs();
	}

	bool HasRejectedUnsafeSite()
	{
		return m_BaseSelector && m_BaseSelector.HasRejectedUnsafeSite();
	}

	void SetGraphContextReady(bool ready)
	{
		m_bGraphContextReady = ready;
	}

	void ProbeInitialSupplies()
	{
		if (!IsEnabled() || !m_SupplyNetwork)
			return;
		// Conflict fills the stock pools after the AICF bootstrap. Defer the
		// snapshot instead of publishing startup-zero values as calibration data.
		m_bInitialSupplyProbePending = true;
	}

	void UpdateLogistics(SCR_CampaignFaction usFaction, SCR_CampaignFaction ussrFaction)
	{
		if (!IsEnabled())
			return;
		TryCompleteInitialSupplyProbe(usFaction, ussrFaction);
		// A capture invalidates the stock radio snapshot. Keep all abstract cargo
		// accounted but frozen until the authoritative graph rebuild completes.
		if (m_bGraphContextReady)
		{
			m_DeliverySystem.Update(usFaction);
			m_DeliverySystem.Update(ussrFaction);
		}

		int nowMs = System.GetTickCount();
		if (m_iLastHeartbeatAtMs <= 0 ||
			System.GetTickCount(m_iLastHeartbeatAtMs) >= m_Config.GetHeartbeatIntervalMs())
		{
			m_iLastHeartbeatAtMs = nowMs;
			LogHeartbeat(usFaction);
			LogHeartbeat(ussrFaction);
		}
	}

	void BeginRequest(
		SCR_CampaignFaction faction,
		AICF_GroupSlot slot,
		SCR_CampaignMilitaryBaseComponent savedTargetBase)
	{
		if (!IsEnabled() || !faction || !slot)
			return;
		AICF_ReinforcementRequest existing = FindRequest(faction.GetFactionKey(), slot.GetSlotId());
		if (existing)
			return;

		ref AICF_ReinforcementRequest request = new AICF_ReinforcementRequest(
			faction.GetFactionKey(),
			slot.GetSlotId(),
			m_iNextRequestId++,
			savedTargetBase);
		m_aRequests.Insert(request);
		AICF_Stage4Diagnostics.Info(
			"REINFORCEMENT_REQUESTED",
			string.Format(
				"request=%1 faction=%2 slot=%3 saved_target=%4 required_progress_ms=%5 supply_cost=%6",
				request.GetRequestId(),
				faction.GetFactionKey(),
				slot.GetSlotId(),
				AICF_Stage1Diagnostics.BaseKey(savedTargetBase),
				GetRequiredProgressMs(),
				m_Config.GetReplacementSupplyCost()));
	}

	void AdvanceRequest(SCR_CampaignFaction faction, AICF_GroupSlot slot)
	{
		if (!IsEnabled() || !faction || !slot)
			return;
		AICF_ReinforcementRequest request = FindRequest(faction.GetFactionKey(), slot.GetSlotId());
		if (!request)
		{
			BeginRequest(faction, slot, null);
			request = FindRequest(faction.GetFactionKey(), slot.GetSlotId());
		}
		if (!request)
			return;

		bool wasReady = request.IsReady(GetRequiredProgressMs());
		AICF_ESupplyNetworkTier tier = AICF_ESupplyNetworkTier.BLOCKED;
		if (m_bGraphContextReady)
		{
			tier = m_SupplyNetwork.EvaluateReinforcementTier(
				faction,
				m_Config.GetReplacementSupplyCost());
		}
		int pacePercent = m_Config.GetPacePercent(tier);
		bool tierChanged = request.Advance(tier, pacePercent, GetRequiredProgressMs());
		bool becameReady = !wasReady && request.IsReady(GetRequiredProgressMs());
		if (tierChanged || becameReady)
		{
			AICF_Stage4Diagnostics.Info(
				"REINFORCEMENT_PACING",
				string.Format(
					"request=%1 faction=%2 slot=%3 tier=%4 pace_percent=%5 progress_ms=%6 required_ms=%7 ready=%8",
					request.GetRequestId(),
					faction.GetFactionKey(),
					slot.GetSlotId(),
					AICF_Stage4Diagnostics.TierToString(tier),
					pacePercent,
					request.GetProgressMs(),
					GetRequiredProgressMs(),
					request.IsReady(GetRequiredProgressMs())));
		}
	}

	bool IsRequestAttemptDue(SCR_CampaignFaction faction, AICF_GroupSlot slot, int nowMs)
	{
		if (!IsEnabled() || !faction || !slot)
			return false;
		AICF_ReinforcementRequest request = FindRequest(faction.GetFactionKey(), slot.GetSlotId());
		return request && request.CanAttempt(GetRequiredProgressMs(), nowMs);
	}

	bool TryBeginDeployment(
		AICF_FactionState factionState,
		SCR_CampaignFaction faction,
		AICF_GroupSlot slot,
		out SCR_CampaignMilitaryBaseComponent spawnBase)
	{
		spawnBase = null;
		if (!IsEnabled() || !m_bGraphContextReady || !factionState || !faction || !slot ||
			!slot.IsReplacementDeployment())
			return false;

		AICF_ReinforcementRequest request = FindRequest(faction.GetFactionKey(), slot.GetSlotId());
		if (!request || !request.CanAttempt(GetRequiredProgressMs(), System.GetTickCount()))
			return false;
		if (FindReservation(faction.GetFactionKey(), slot.GetSlotId()))
		{
			AICF_Stage4Diagnostics.Error(
				"RESERVATION_DUPLICATE",
				string.Format("faction=%1 slot=%2", faction.GetFactionKey(), slot.GetSlotId()));
			return false;
		}

		int attemptToken = request.BeginAttempt(slot.GetSpawnGeneration());
		if (!m_BaseSelector.Select(
			faction,
			slot,
			request.GetSavedTargetBase(),
			m_Config.GetReplacementSupplyCost(),
			spawnBase))
		{
			request.ScheduleRetry(m_Config.GetRetryIntervalMs());
			AICF_Stage4Diagnostics.Warning(
				"REINFORCEMENT_BLOCKED",
				string.Format(
					"request=%1 token=%2 faction=%3 slot=%4 reason=NO_SAFE_AFFORDABLE_BASE",
					request.GetRequestId(),
					attemptToken,
					faction.GetFactionKey(),
					slot.GetSlotId()));
			return false;
		}

		if (!factionState.TryReserveDeployment(AICF_EDeploymentKind.REPLACEMENT))
		{
			request.ScheduleRetry(m_Config.GetRetryIntervalMs());
			AICF_Stage4Diagnostics.Warning(
				"REINFORCEMENT_BLOCKED",
				string.Format(
					"request=%1 token=%2 faction=%3 slot=%4 reason=INSUFFICIENT_TICKETS tickets=%5 reserved=%6",
					request.GetRequestId(),
					attemptToken,
					faction.GetFactionKey(),
					slot.GetSlotId(),
					factionState.GetTickets(),
					factionState.GetReservedTickets()));
			return false;
		}

		int supplyCost = m_Config.GetReplacementSupplyCost();
		float suppliesBefore = spawnBase.GetSupplies();
		if (suppliesBefore < supplyCost)
		{
			factionState.ReleaseDeploymentReservation(AICF_EDeploymentKind.REPLACEMENT);
			request.ScheduleRetry(m_Config.GetRetryIntervalMs());
			spawnBase = null;
			return false;
		}

		spawnBase.AddSupplies(-supplyCost);
		ref AICF_DeploymentReservation reservation = new AICF_DeploymentReservation(
			faction.GetFactionKey(),
			slot.GetSlotId(),
			request.GetRequestId(),
			attemptToken,
			slot.GetSpawnGeneration(),
			m_Graph.GetRevision(),
			spawnBase,
			supplyCost,
			suppliesBefore);
		m_aReservations.Insert(reservation);
		AICF_Stage4Diagnostics.Info(
			"DEPLOYMENT_RESERVED",
			string.Format(
				"request=%1 token=%2 faction=%3 slot=%4 generation=%5 base=%6 graph_revision=%7 ticket_cost=%8 supply_cost=%9",
				request.GetRequestId(),
				attemptToken,
				faction.GetFactionKey(),
				slot.GetSlotId(),
				slot.GetSpawnGeneration(),
				AICF_Stage1Diagnostics.BaseKey(spawnBase),
				m_Graph.GetRevision(),
				factionState.GetReplacementTicketCost(),
				supplyCost) + string.Format(
				" supplies_before=%1 supplies_after_expected=%2",
				suppliesBefore,
				suppliesBefore - supplyCost));
		return true;
	}

	bool ValidateReservationForCommit(
		AICF_FactionState factionState,
		SCR_CampaignFaction faction,
		AICF_GroupSlot slot,
		out string failureReason)
	{
		return ValidateReservation(
			factionState,
			faction,
			slot,
			"COMMIT",
			failureReason);
	}

	bool ValidateReservationForSpawn(
		AICF_FactionState factionState,
		SCR_CampaignFaction faction,
		AICF_GroupSlot slot,
		out string failureReason)
	{
		return ValidateReservation(
			factionState,
			faction,
			slot,
			"SPAWN",
			failureReason);
	}

	protected bool ValidateReservation(
		AICF_FactionState factionState,
		SCR_CampaignFaction faction,
		AICF_GroupSlot slot,
		string phase,
		out string failureReason)
	{
		failureReason = string.Empty;
		AICF_DeploymentReservation reservation = FindReservationForSlot(faction, slot);
		AICF_ReinforcementRequest request = FindRequestForSlot(faction, slot);
		if (!m_bGraphContextReady)
			failureReason = "GRAPH_REBUILD_PENDING";
		else if (!reservation || !request)
			failureReason = "RESERVATION_MISSING";
		else if (reservation.GetRequestId() != request.GetRequestId() ||
			reservation.GetAttemptToken() != request.GetAttemptToken())
			failureReason = "REQUEST_TOKEN_STALE";
		else if (reservation.GetSlotGeneration() != slot.GetSpawnGeneration() ||
			request.GetAttemptSlotGeneration() != slot.GetSpawnGeneration())
			failureReason = "GROUP_GENERATION_STALE";
		else if (reservation.GetGraphRevision() != m_Graph.GetRevision())
			failureReason = "GRAPH_REVISION_STALE";
		else if (!reservation.HasTicketReservation() || factionState.GetReservedTickets() <= 0)
			failureReason = "TICKET_RESERVATION_MISSING";
		else if (!reservation.HasSupplyReservation())
			failureReason = "SUPPLY_RESERVATION_MISSING";
		else if (!reservation.GetBase() || reservation.GetBase().GetSuppliesMax() <= 0)
			failureReason = "SUPPLY_POOL_MISSING";
		else
			failureReason = m_ConflictAdapter.GetSpawnRejectionReason(reservation.GetBase(), faction);

		if (!failureReason.IsEmpty())
		{
			AICF_Stage4Diagnostics.Warning(
				"RESERVATION_REVALIDATION_FAILED",
				string.Format(
					"phase=%1 faction=%2 slot=%3 generation=%4 graph_revision=%5 reason=%6",
					phase,
					faction.GetFactionKey(),
					slot.GetSlotId(),
					slot.GetSpawnGeneration(),
					m_Graph.GetRevision(),
					failureReason));
			return false;
		}

		AICF_Stage4Diagnostics.Info(
			"RESERVATION_REVALIDATED",
			string.Format(
				"phase=%1 request=%2 token=%3 faction=%4 slot=%5 generation=%6 base=%7 graph_revision=%8 reserved_supplies=%9",
				phase,
				reservation.GetRequestId(),
				reservation.GetAttemptToken(),
				faction.GetFactionKey(),
				slot.GetSlotId(),
				slot.GetSpawnGeneration(),
				AICF_Stage1Diagnostics.BaseKey(reservation.GetBase()),
				m_Graph.GetRevision(),
				reservation.GetSupplyCost()) + string.Format(
				" current_stock=%1",
				reservation.GetBase().GetSupplies()));
		return true;
	}

	bool TryCommitDeployment(
		AICF_FactionState factionState,
		SCR_CampaignFaction faction,
		AICF_GroupSlot slot)
	{
		string failureReason;
		if (!ValidateReservationForCommit(factionState, faction, slot, failureReason))
			return false;
		AICF_DeploymentReservation reservation = FindReservationForSlot(faction, slot);
		if (!factionState.TryCommitDeployment(AICF_EDeploymentKind.REPLACEMENT))
			return false;
		reservation.MarkCommitted();
		return true;
	}

	void FinalizeDeployment(SCR_CampaignFaction faction, AICF_GroupSlot slot)
	{
		AICF_DeploymentReservation reservation = FindReservationForSlot(faction, slot);
		if (!reservation || !reservation.IsCommitted())
			return;
		reservation.ClearSupplyReservation();
		AICF_Stage4Diagnostics.Info(
			"DEPLOYMENT_COMMITTED",
			string.Format(
				"request=%1 token=%2 faction=%3 slot=%4 generation=%5 base=%6 ticket_debit=1 supply_debit=%7 roster=5/5",
				reservation.GetRequestId(),
				reservation.GetAttemptToken(),
				faction.GetFactionKey(),
				slot.GetSlotId(),
				slot.GetSpawnGeneration(),
				AICF_Stage1Diagnostics.BaseKey(reservation.GetBase()),
				reservation.GetSupplyCost()));
		RemoveRequest(faction.GetFactionKey(), slot.GetSlotId());
		RemoveReservation(faction.GetFactionKey(), slot.GetSlotId());
	}

	void AbortDeployment(
		AICF_FactionState factionState,
		SCR_CampaignFaction faction,
		AICF_GroupSlot slot,
		string reason)
	{
		if (!IsEnabled() || !factionState || !faction || !slot)
			return;
		AICF_DeploymentReservation reservation = FindReservationForSlot(faction, slot);
		if (reservation)
		{
			if (reservation.IsCommitted())
				factionState.RollbackCommittedDeployment(AICF_EDeploymentKind.REPLACEMENT);
			else if (reservation.HasTicketReservation())
				factionState.ReleaseDeploymentReservation(AICF_EDeploymentKind.REPLACEMENT);
			if (reservation.HasSupplyReservation() && reservation.GetBase())
			{
				reservation.GetBase().AddSupplies(reservation.GetSupplyCost());
				reservation.ClearSupplyReservation();
			}

			AICF_Stage4Diagnostics.Info(
				"DEPLOYMENT_ABORTED",
				string.Format(
					"request=%1 token=%2 faction=%3 slot=%4 generation=%5 base=%6 ticket_rollback=1 supply_rollback=%7 reason=%8",
					reservation.GetRequestId(),
					reservation.GetAttemptToken(),
					faction.GetFactionKey(),
					slot.GetSlotId(),
					reservation.GetSlotGeneration(),
					AICF_Stage1Diagnostics.BaseKey(reservation.GetBase()),
					reservation.GetSupplyCost(),
					reason));
			RemoveReservation(faction.GetFactionKey(), slot.GetSlotId());
		}

		AICF_ReinforcementRequest request = FindRequest(faction.GetFactionKey(), slot.GetSlotId());
		if (request)
			request.ScheduleRetry(m_Config.GetRetryIntervalMs());
	}

	void RollbackCommittedDeployment(
		AICF_FactionState factionState,
		SCR_CampaignFaction faction,
		AICF_GroupSlot slot,
		string reason)
	{
		AbortDeployment(factionState, faction, slot, reason);
	}

	SCR_CampaignMilitaryBaseComponent GetReservedBase(
		SCR_CampaignFaction faction,
		AICF_GroupSlot slot)
	{
		AICF_DeploymentReservation reservation = FindReservationForSlot(faction, slot);
		if (!reservation)
			return null;
		return reservation.GetBase();
	}

	int GetPendingRequestCount(FactionKey factionKey)
	{
		int count;
		foreach (AICF_ReinforcementRequest request : m_aRequests)
		{
			if (request && request.GetFactionKey() == factionKey)
				count++;
		}
		return count;
	}

	int GetShipmentCount(FactionKey factionKey)
	{
		return m_DeliverySystem.GetInTransitCount(factionKey);
	}

	int GetTotalSupplies(SCR_CampaignFaction faction)
	{
		return m_SupplyNetwork.GetFactionTotalSupplies(faction, false);
	}

	int GetConnectedSupplies(SCR_CampaignFaction faction)
	{
		return m_SupplyNetwork.GetFactionTotalSupplies(faction, true);
	}

	AICF_ESupplyNetworkTier GetFactionTier(SCR_CampaignFaction faction)
	{
		if (!m_bGraphContextReady)
			return AICF_ESupplyNetworkTier.BLOCKED;
		return m_SupplyNetwork.EvaluateReinforcementTier(
			faction,
			m_Config.GetReplacementSupplyCost());
	}

	void Stop(
		AICF_FactionState usState,
		SCR_CampaignFaction usFaction,
		AICF_FactionState ussrState,
		SCR_CampaignFaction ussrFaction)
	{
		if (!IsEnabled())
			return;
		for (int index = m_aReservations.Count() - 1; index >= 0; index--)
		{
			AICF_DeploymentReservation reservation = m_aReservations[index];
			AICF_FactionState state = usState;
			SCR_CampaignFaction faction = usFaction;
			if (reservation.GetFactionKey() == "USSR")
			{
				state = ussrState;
				faction = ussrFaction;
			}
			AICF_GroupSlot slot = state.GetSlot(reservation.GetSlotId());
			AbortDeployment(state, faction, slot, "SYSTEM_STOP");
		}
		m_DeliverySystem.Stop(usFaction, ussrFaction);
		m_aRequests.Clear();
		m_aReservations.Clear();
	}

	protected int GetRequiredProgressMs()
	{
		return m_iReinforcementDelayMs;
	}

	protected void TryCompleteInitialSupplyProbe(
		SCR_CampaignFaction usFaction,
		SCR_CampaignFaction ussrFaction)
	{
		if (!m_bInitialSupplyProbePending ||
			!m_SupplyNetwork.HasInitializedFactionSupplyPool(usFaction) ||
			!m_SupplyNetwork.HasInitializedFactionSupplyPool(ussrFaction))
			return;
		m_SupplyNetwork.ProbeInitialSupplies();
		m_bInitialSupplyProbePending = false;
	}

	protected AICF_ReinforcementRequest FindRequest(FactionKey factionKey, int slotId)
	{
		foreach (AICF_ReinforcementRequest request : m_aRequests)
		{
			if (request && request.GetFactionKey() == factionKey && request.GetSlotId() == slotId)
				return request;
		}
		return null;
	}

	protected AICF_ReinforcementRequest FindRequestForSlot(
		SCR_CampaignFaction faction,
		AICF_GroupSlot slot)
	{
		if (!faction || !slot)
			return null;
		return FindRequest(faction.GetFactionKey(), slot.GetSlotId());
	}

	protected AICF_DeploymentReservation FindReservation(FactionKey factionKey, int slotId)
	{
		foreach (AICF_DeploymentReservation reservation : m_aReservations)
		{
			if (reservation && reservation.GetFactionKey() == factionKey && reservation.GetSlotId() == slotId)
				return reservation;
		}
		return null;
	}

	protected AICF_DeploymentReservation FindReservationForSlot(
		SCR_CampaignFaction faction,
		AICF_GroupSlot slot)
	{
		if (!faction || !slot)
			return null;
		return FindReservation(faction.GetFactionKey(), slot.GetSlotId());
	}

	protected void RemoveRequest(FactionKey factionKey, int slotId)
	{
		for (int index = m_aRequests.Count() - 1; index >= 0; index--)
		{
			AICF_ReinforcementRequest request = m_aRequests[index];
			if (request && request.GetFactionKey() == factionKey && request.GetSlotId() == slotId)
				m_aRequests.Remove(index);
		}
	}

	protected void RemoveReservation(FactionKey factionKey, int slotId)
	{
		for (int index = m_aReservations.Count() - 1; index >= 0; index--)
		{
			AICF_DeploymentReservation reservation = m_aReservations[index];
			if (reservation && reservation.GetFactionKey() == factionKey && reservation.GetSlotId() == slotId)
				m_aReservations.Remove(index);
		}
	}

	protected void LogHeartbeat(SCR_CampaignFaction faction)
	{
		if (!faction)
			return;
		FactionKey factionKey = faction.GetFactionKey();
		int dispatched = m_DeliverySystem.GetDispatchedSupplies(factionKey);
		int delivered = m_DeliverySystem.GetDeliveredSupplies(factionKey);
		int returned = m_DeliverySystem.GetReturnedSupplies(factionKey);
		int inTransit = m_DeliverySystem.GetInTransitSupplies(factionKey);
		int delta = dispatched - delivered - returned - inTransit;
		if (delta != 0)
		{
			AICF_Stage4Diagnostics.Error(
				"SHIPMENT_BALANCE_FAILED",
				string.Format(
					"faction=%1 dispatched=%2 delivered=%3 returned=%4 in_transit=%5 delta=%6",
					factionKey,
					dispatched,
					delivered,
					returned,
					inTransit,
					delta));
		}
		AICF_Stage4Diagnostics.Info(
			"HEARTBEAT",
			string.Format(
				"faction=%1 tier=%2 total_supplies=%3 connected_supplies=%4 pending_requests=%5 reservations=%6 shipments=%7",
				factionKey,
				AICF_Stage4Diagnostics.TierToString(GetFactionTier(faction)),
				GetTotalSupplies(faction),
				GetConnectedSupplies(faction),
				GetPendingRequestCount(factionKey),
				CountReservations(factionKey),
				GetShipmentCount(factionKey)) + string.Format(
				" dispatched=%1 delivered=%2 returned=%3 in_transit=%4 balance_delta=%5",
				dispatched,
				delivered,
				returned,
				inTransit,
				delta));
	}

	protected int CountReservations(FactionKey factionKey)
	{
		int count;
		foreach (AICF_DeploymentReservation reservation : m_aReservations)
		{
			if (reservation && reservation.GetFactionKey() == factionKey)
				count++;
		}
		return count;
	}
}
