modded class SCR_PlayerController
{
	[RplProp(condition: RplCondition.OwnerOnly)]
	protected string m_sAICFVehiclePresetView;
	[RplProp(condition: RplCondition.OwnerOnly)]
	protected int m_iAICFVehiclePresetToken;
	[RplProp(condition: RplCondition.OwnerOnly)]
	protected bool m_bAICFVehiclePresetAccepted;
	[RplProp(condition: RplCondition.OwnerOnly)]
	protected string m_sAICFVehiclePresetReason;
	protected int m_iAICFVehiclePresetSent;
	protected int m_iAICFVehiclePresetRateAt;

	string AICF_VehiclePresetView() { return m_sAICFVehiclePresetView; }
	int AICF_VehiclePresetToken() { return m_iAICFVehiclePresetToken; }
	bool AICF_VehiclePresetAccepted() { return m_bAICFVehiclePresetAccepted; }
	string AICF_VehiclePresetReason() { return m_sAICFVehiclePresetReason; }

	int AICF_RequestVehiclePreset(int slot, int revision, int generation, int libraryRevision, int operation, string payload = "")
	{
		if (this != GetGame().GetPlayerController() || payload.Length() > AICF_VehiclePreset.MAX_BYTES) return 0;
		m_iAICFVehiclePresetSent = Math.Max(m_iAICFVehiclePresetSent, m_iAICFVehiclePresetToken) + 1;
		Rpc(RpcAsk_AICFVehiclePreset, m_iAICFVehiclePresetSent, slot, revision, generation, libraryRevision, operation, payload);
		return m_iAICFVehiclePresetSent;
	}

	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_AICFVehiclePreset(int token, int slot, int revision, int generation, int libraryRevision, int operation, string payload)
	{
		if (!Replication.IsServer() || token <= m_iAICFVehiclePresetToken) return;
		if (payload.Length() > AICF_VehiclePreset.MAX_BYTES || slot < 0 || slot >= AICF_Stage1Config.GROUP_SLOTS_PER_FACTION ||
			operation < 0 || operation > AICF_EVehiclePresetOperation.RESET)
			{ AICF_VehiclePresetResult(token, false, "INVALID_REQUEST", ""); return; }
		int now = System.GetTickCount();
		if (now < m_iAICFVehiclePresetRateAt) { AICF_VehiclePresetResult(token, false, "RATE_LIMITED", m_sAICFVehiclePresetView); return; }
		m_iAICFVehiclePresetRateAt = now + 750;
		AICF_MatchController match = AICF_MatchController.GetActiveController();
		if (!match) { AICF_VehiclePresetResult(token, false, "MATCH_UNAVAILABLE", ""); return; }
		match.RequestVehiclePreset(this, token, slot, revision, generation, libraryRevision, operation, payload);
	}

	void AICF_PublishVehiclePresetView(string view)
	{
		if (!Replication.IsServer() || view == m_sAICFVehiclePresetView) return;
		m_sAICFVehiclePresetView = view;
		Replication.BumpMe();
	}

	void AICF_VehiclePresetResult(int token, bool accepted, string reason, string view)
	{
		if (!Replication.IsServer() || token < m_iAICFVehiclePresetToken ||
			(token == m_iAICFVehiclePresetToken && accepted == m_bAICFVehiclePresetAccepted && reason == m_sAICFVehiclePresetReason && view == m_sAICFVehiclePresetView)) return;
		m_iAICFVehiclePresetToken = token;
		m_bAICFVehiclePresetAccepted = accepted;
		m_sAICFVehiclePresetReason = reason;
		m_sAICFVehiclePresetView = view;
		Replication.BumpMe();
	}
}
