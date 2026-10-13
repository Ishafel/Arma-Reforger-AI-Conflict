// Один ledger на матч; identity не зависит от controlled entity или numeric player ID.
class AICF_GreetingPlayer
{
	int m_iNextAt;
	int m_iPendingToken;
	bool m_bInside;

	bool Observe(float distance)
	{
		if (distance > AICF_SocialConfig.EXIT_METERS)
			m_bInside = false;
		if (m_bInside || distance > AICF_SocialConfig.ENTER_METERS)
			return false;
		m_bInside = true;
		return true;
	}

	bool Reserve(int now, int token)
	{
		if (token <= 0 || m_iPendingToken != 0 || now < m_iNextAt)
			return false;
		m_iPendingToken = token;
		return true;
	}

	bool Commit(int token, int now)
	{
		if (token <= 0 || m_iPendingToken != token)
			return false;
		m_iNextAt = now + AICF_SocialConfig.COOLDOWN_MS;
		m_iPendingToken = 0;
		return true;
	}

	void Release(int token)
	{
		if (token == m_iPendingToken)
			m_iPendingToken = 0;
	}
}

class AICF_GreetingConnection
{
	PlayerController m_Controller;
	string m_sIdentity;
}

class AICF_GreetingGate
{
	protected ref map<string, ref AICF_GreetingPlayer> m_mPlayers = new map<string, ref AICF_GreetingPlayer>();
	protected ref map<int, ref AICF_GreetingConnection> m_mConnections = new map<int, ref AICF_GreetingConnection>();
	protected int m_iConnectionSequence;
	protected int m_iTokenSequence;

	AICF_GreetingPlayer Player(string identity)
	{
		if (identity.IsEmpty()) return null;
		AICF_GreetingPlayer player = m_mPlayers.Get(identity);
		if (!player)
		{
			player = new AICF_GreetingPlayer();
			m_mPlayers.Set(identity, player);
		}
		return player;
	}

	// Stable identity появится после backend audit. Переносим уже начатый cooldown.
	AICF_GreetingPlayer Resolve(int id, PlayerController controller, string stableIdentity)
	{
		if (id <= 0 || !controller) return null;
		AICF_GreetingConnection connection = m_mConnections.Get(id);
		if (!connection || connection.m_Controller != controller)
		{
			connection = new AICF_GreetingConnection();
			connection.m_Controller = controller;
			m_iConnectionSequence++;
			connection.m_sIdentity = "connection:" + m_iConnectionSequence;
			m_mConnections.Set(id, connection);
		}
		if (!stableIdentity.IsEmpty() && connection.m_sIdentity != "backend:" + stableIdentity)
		{
			// Другая подтверждённая identity не наследует ledger прежнего человека.
			if (connection.m_sIdentity.IndexOf("backend:") == 0)
			{
				connection.m_sIdentity = "backend:" + stableIdentity;
				return Player(connection.m_sIdentity);
			}
			AICF_GreetingPlayer previous = Player(connection.m_sIdentity);
			AICF_GreetingPlayer stable = Player("backend:" + stableIdentity);
			stable.m_iNextAt = Math.Max(stable.m_iNextAt, previous.m_iNextAt);
			stable.m_bInside = stable.m_bInside || previous.m_bInside;
			// Pending actor хранит предыдущий ledger: не меняем его до завершения.
			if (previous.m_iPendingToken != 0) return previous;
			connection.m_sIdentity = "backend:" + stableIdentity;
		}
		return Player(connection.m_sIdentity);
	}

	void Disconnect(int id)
	{
		m_mConnections.Remove(id);
	}

	int NextToken()
	{
		return ++m_iTokenSequence;
	}
}
