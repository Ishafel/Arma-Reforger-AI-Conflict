// The RPC lives on the player-owned controller, so authority derives the
// requesting player identity from the replicated owner instead of trusting an
// arbitrary player id supplied by UI code.
modded class SCR_PlayerController
{
	protected int m_iAICFStrategicPointOrderResultSequence;
	protected int m_iAICFStrategicPointOrderResultSlot = -1;
	protected bool m_bAICFStrategicPointOrderResultAccepted;
	protected string m_sAICFStrategicPointOrderResultReason;
	protected vector m_vAICFStrategicPointOrderResultRequest;
	protected vector m_vAICFStrategicPointOrderResultPosition;

	void AICF_RequestStrategicOrder(int slotId, int targetCallsign)
	{
		Rpc(RpcAsk_AICFStrategicOrder, slotId, targetCallsign);
	}

	void AICF_RequestStrategicPointOrder(int slotId, vector clientPosition)
	{
		Rpc(RpcAsk_AICFStrategicPointOrder, slotId, clientPosition);
	}

	int AICF_GetStrategicPointOrderResultSequence()
	{
		return m_iAICFStrategicPointOrderResultSequence;
	}

	int AICF_GetStrategicPointOrderResultSlot()
	{
		return m_iAICFStrategicPointOrderResultSlot;
	}

	bool AICF_WasStrategicPointOrderAccepted()
	{
		return m_bAICFStrategicPointOrderResultAccepted;
	}

	string AICF_GetStrategicPointOrderResultReason()
	{
		return m_sAICFStrategicPointOrderResultReason;
	}

	vector AICF_GetStrategicPointOrderResultRequest()
	{
		return m_vAICFStrategicPointOrderResultRequest;
	}

	vector AICF_GetStrategicPointOrderResultPosition()
	{
		return m_vAICFStrategicPointOrderResultPosition;
	}

	void AICF_RequestGroupConfiguration(
		int slotId,
		int roleCode,
		int unitTypeCode,
		int desiredSize)
	{
		Rpc(
			RpcAsk_AICFGroupConfiguration,
			slotId,
			roleCode,
			unitTypeCode,
			desiredSize);
	}

	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_AICFStrategicOrder(int slotId, int targetCallsign)
	{
		AICF_MatchController controller = AICF_MatchController.GetActiveController();
		if (!controller)
			return;
		controller.RequestPlayerOrder(GetPlayerId(), slotId, targetCallsign);
	}

	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_AICFStrategicPointOrder(int slotId, vector clientPosition)
	{
		AICF_MatchController controller = AICF_MatchController.GetActiveController();
		string rejectionReason;
		vector resolvedPosition;
		bool accepted = false;
		bool responseDeferred = false;
		if (!controller)
			rejectionReason = "MATCH_UNAVAILABLE";
		else
			accepted = controller.RequestPlayerPointOrder(
				this,
				GetPlayerId(),
				slotId,
				clientPosition,
				rejectionReason,
				resolvedPosition,
				responseDeferred);

		if (!responseDeferred)
		{
			AICF_SendStrategicPointOrderResult(
				slotId,
				clientPosition,
				accepted,
				rejectionReason,
				resolvedPosition);
		}
	}

	void AICF_SendStrategicPointOrderResult(
		int slotId,
		vector clientPosition,
		bool accepted,
		string rejectionReason,
		vector resolvedPosition)
	{
		if (!Replication.IsServer())
			return;
		Rpc(
			RpcDo_AICFStrategicPointOrderResult,
			slotId,
			clientPosition,
			accepted,
			rejectionReason,
			resolvedPosition);
	}

	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	protected void RpcDo_AICFStrategicPointOrderResult(
		int slotId,
		vector clientPosition,
		bool accepted,
		string rejectionReason,
		vector resolvedPosition)
	{
		m_iAICFStrategicPointOrderResultSlot = slotId;
		m_vAICFStrategicPointOrderResultRequest = clientPosition;
		m_bAICFStrategicPointOrderResultAccepted = accepted;
		m_sAICFStrategicPointOrderResultReason = rejectionReason;
		m_vAICFStrategicPointOrderResultPosition = resolvedPosition;
		m_iAICFStrategicPointOrderResultSequence++;
	}

	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_AICFGroupConfiguration(
		int slotId,
		int roleCode,
		int unitTypeCode,
		int desiredSize)
	{
		AICF_MatchController controller = AICF_MatchController.GetActiveController();
		if (!controller)
			return;
		controller.RequestPlayerGroupConfiguration(
			GetPlayerId(),
			slotId,
			roleCode,
			unitTypeCode,
			desiredSize);
	}
}
