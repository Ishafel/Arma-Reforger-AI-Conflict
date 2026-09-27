enum AICF_ESquadCommand
{
	RECRUIT,
	RETURN_TO_AI
}

// Только intent на player-owned controller; ответ адресован его владельцу.
modded class SCR_PlayerController
{
	protected int m_iAICFSquadCommandSequence;
	protected int m_iAICFSquadCommandSlot;
	protected string m_sAICFSquadCommandReason;

	void AICF_RequestSquadCommand(int slotId, AICF_ESquadCommand command)
	{
		Rpc(RpcAsk_AICFSquadCommand, slotId, command);
	}

	int AICF_GetSquadCommandSequence()
	{
		return m_iAICFSquadCommandSequence;
	}

	string AICF_GetSquadCommandMessage()
	{
		return AICF_Localization.Format("{AICF:AICF_UI_SquadCommandResult}",
			string.Format("%1", m_iAICFSquadCommandSlot + 1),
			"{AICF:AICF_UI_SquadCommand_" + m_sAICFSquadCommandReason + "}");
	}

	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_AICFSquadCommand(int slotId, AICF_ESquadCommand command)
	{
		AICF_MatchController controller = AICF_MatchController.GetActiveController();
		string reason = "MATCH_UNAVAILABLE";
		bool accepted;
		if (controller)
			accepted = controller.RequestPlayerSquadCommand(this, slotId, command, reason);
		AICF_Stage4Diagnostics.Info("PLAYER_SQUAD_COMMAND_RESULT", string.Format(
			"player=%1 slot=%2 command=%3 accepted=%4 reason=%5 authority=SERVER",
			GetPlayerId(), slotId, command, accepted, reason));
		Rpc(RpcDo_AICFSquadCommandResult, slotId, reason);
	}

	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	protected void RpcDo_AICFSquadCommandResult(int slotId, string reason)
	{
		m_iAICFSquadCommandSlot = slotId;
		m_sAICFSquadCommandReason = reason;
		m_iAICFSquadCommandSequence++;
	}
}
