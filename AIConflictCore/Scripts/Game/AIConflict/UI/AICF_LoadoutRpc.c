modded class SCR_PlayerController
{
	[RplProp(condition: RplCondition.OwnerOnly)]
	protected string m_sAICFLoadoutView;
	[RplProp(condition: RplCondition.OwnerOnly)]
	protected string m_sAICFLoadoutLibrary;
	[RplProp(condition: RplCondition.OwnerOnly)]
	protected int m_iAICFLoadoutSlot = -1;
	[RplProp(condition: RplCondition.OwnerOnly)]
	protected int m_iAICFLoadoutMember = -1;
	[RplProp(condition: RplCondition.OwnerOnly)]
	protected int m_iAICFLoadoutRevision = -1;
	[RplProp(condition: RplCondition.OwnerOnly)]
	protected int m_iAICFLoadoutCost;
	[RplProp(condition: RplCondition.OwnerOnly)]
	protected int m_iAICFLoadoutToken;
	[RplProp(condition: RplCondition.OwnerOnly)]
	protected string m_sAICFLoadoutResult;
	[RplProp(condition: RplCondition.OwnerOnly)]
	protected bool m_bAICFLoadoutAccepted;
	protected int m_iAICFLoadoutSent;
	protected int m_iAICFLoadoutRateAt;

	int AICF_LoadoutSlot() { return m_iAICFLoadoutSlot; }
	int AICF_LoadoutMember() { return m_iAICFLoadoutMember; }
	int AICF_LoadoutRevision() { return m_iAICFLoadoutRevision; }
	int AICF_LoadoutToken() { return m_iAICFLoadoutToken; }
	int AICF_LoadoutCost() { return m_iAICFLoadoutCost; }
	string AICF_LoadoutData() { return m_sAICFLoadoutView; }
	string AICF_LoadoutLibrary() { return m_sAICFLoadoutLibrary; }
	string AICF_LoadoutStatus() { return m_sAICFLoadoutResult; }
	bool AICF_LoadoutAccepted() { return m_bAICFLoadoutAccepted; }
	string AICF_LoadoutFaction()
	{
		AICF_LoadoutRecipe recipe = AICF_LoadoutRecipe.Decode(m_sAICFLoadoutView);
		if (recipe)
			return recipe.m_sFaction;
		return string.Empty;
	}

	int AICF_RequestLoadout(int slot, int member, int revision, int operation, string payload = "")
	{
		if (this != GetGame().GetPlayerController() || payload.Length() > AICF_LoadoutRecipe.MAX_BYTES)
			return 0;
		m_iAICFLoadoutSent = Math.Max(m_iAICFLoadoutSent, m_iAICFLoadoutToken) + 1;
		Rpc(RpcAsk_AICFLoadout, m_iAICFLoadoutSent, slot, member, revision, operation, payload);
		return m_iAICFLoadoutSent;
	}

	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_AICFLoadout(int token, int slot, int member, int revision, int operation, string payload)
	{
		if (!Replication.IsServer() || token <= m_iAICFLoadoutToken)
			return;
		if (payload.Length() > AICF_LoadoutRecipe.MAX_BYTES || operation < 0 || operation > 2)
		{
			AICF_LoadoutResult(token, false, "INVALID_REQUEST");
			return;
		}
		int now = System.GetTickCount();
		if (now < m_iAICFLoadoutRateAt)
		{
			AICF_LoadoutResult(token, false, "RATE_LIMITED");
			return;
		}
		m_iAICFLoadoutRateAt = now + 750;
		AICF_MatchController match = AICF_MatchController.GetActiveController();
		if (!match)
		{
			AICF_LoadoutResult(token, false, "MATCH_OR_FACTION_UNAVAILABLE");
			return;
		}
		match.RequestLoadout(this, token, slot, member, revision, operation, payload);
	}

	void AICF_LoadoutView(int slot, int member, int revision, string data, string library, int cost)
	{
		if (!Replication.IsServer() || (slot == m_iAICFLoadoutSlot && member == m_iAICFLoadoutMember &&
			revision == m_iAICFLoadoutRevision && data == m_sAICFLoadoutView && library == m_sAICFLoadoutLibrary && cost == m_iAICFLoadoutCost))
			return;
		m_iAICFLoadoutSlot = slot;
		m_iAICFLoadoutMember = member;
		m_iAICFLoadoutRevision = revision;
		m_iAICFLoadoutCost = cost;
		m_sAICFLoadoutView = data;
		m_sAICFLoadoutLibrary = library;
		Replication.BumpMe();
	}

	void AICF_LoadoutResult(int token, bool accepted, string reason)
	{
		if (!Replication.IsServer() || token < m_iAICFLoadoutToken ||
			(token == m_iAICFLoadoutToken && accepted == m_bAICFLoadoutAccepted && reason == m_sAICFLoadoutResult))
			return;
		m_iAICFLoadoutToken = token;
		m_bAICFLoadoutAccepted = accepted;
		m_sAICFLoadoutResult = reason;
		Replication.BumpMe();
	}
}
