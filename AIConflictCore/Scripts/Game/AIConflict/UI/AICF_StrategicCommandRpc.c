// The RPC lives on the player-owned controller, so authority derives the
// requesting player identity from the replicated owner instead of trusting an
// arbitrary player id supplied by UI code.
modded class SCR_PlayerController
{
	void AICF_RequestStrategicOrder(int slotId, int targetCallsign)
	{
		Rpc(RpcAsk_AICFStrategicOrder, slotId, targetCallsign);
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
