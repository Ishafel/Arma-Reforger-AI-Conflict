// Immutable server-side identity captured while a player point order waits for
// its streamed navmesh tile. Every delayed retry is fail-closed against this
// snapshot before it may change the durable strategic assignment.
class AICF_PlayerPointOrderRequest
{
	protected SCR_PlayerController m_Requester;
	protected SCR_AIGroup m_Group;
	protected int m_iPlayerId;
	protected int m_iSlotId;
	protected int m_iGroupGeneration;
	protected int m_iAssignmentRevision;
	protected int m_iIntentRevision;
	protected int m_iRequestToken;
	protected int m_iStartedAtMs;
	protected FactionKey m_sStableFactionKey;
	protected vector m_vClientPosition;

	void AICF_PlayerPointOrderRequest(
		SCR_PlayerController requester,
		SCR_AIGroup group,
		int playerId,
		int slotId,
		int groupGeneration,
		int assignmentRevision,
		int intentRevision,
		int requestToken,
		FactionKey stableFactionKey,
		vector clientPosition)
	{
		m_Requester = requester;
		m_Group = group;
		m_iPlayerId = playerId;
		m_iSlotId = slotId;
		m_iGroupGeneration = groupGeneration;
		m_iAssignmentRevision = assignmentRevision;
		m_iIntentRevision = intentRevision;
		m_iRequestToken = requestToken;
		m_iStartedAtMs = System.GetTickCount();
		m_sStableFactionKey = stableFactionKey;
		m_vClientPosition = clientPosition;
	}

	SCR_PlayerController GetRequester()
	{
		return m_Requester;
	}

	SCR_AIGroup GetGroup()
	{
		return m_Group;
	}

	int GetPlayerId()
	{
		return m_iPlayerId;
	}

	int GetSlotId()
	{
		return m_iSlotId;
	}

	int GetGroupGeneration()
	{
		return m_iGroupGeneration;
	}

	int GetAssignmentRevision()
	{
		return m_iAssignmentRevision;
	}

	int GetIntentRevision()
	{
		return m_iIntentRevision;
	}

	int GetRequestToken()
	{
		return m_iRequestToken;
	}

	int GetStartedAtMs()
	{
		return m_iStartedAtMs;
	}

	FactionKey GetStableFactionKey()
	{
		return m_sStableFactionKey;
	}

	vector GetClientPosition()
	{
		return m_vClientPosition;
	}
}
