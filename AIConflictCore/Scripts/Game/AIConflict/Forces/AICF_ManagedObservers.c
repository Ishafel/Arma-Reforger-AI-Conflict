// Наблюдатели живых управляемых сил. Spawn, budget и lifecycle остаются native.
class AICF_ManagedObservers
{
	// Отдельный диапазон локальных ключей AICF; чётный US, нечётный USSR.
	protected static const int OBSERVER_KEY_BASE = -1513160000;
	protected SCR_GameModeCampaign m_Campaign;
	protected ObserversSystem m_Observers;
	protected ref array<int> m_aKeys = {};
	protected bool m_bStopped;

	protected IEntity ResolveMember(SCR_AIGroup group, FactionKey expected)
	{
		if (!group || (expected != "US" && expected != "USSR")) return null;
		RplComponent rpl = RplComponent.Cast(group.FindComponent(RplComponent));
		if (rpl && !rpl.IsMaster()) return null;
		IEntity member = AICF_GroupRuntime.ResolveAliveLeader(group);
		if (!member) return null;
		Faction faction = SCR_Faction.GetEntityFaction(member);
		if (!faction || AICF_ContentProfile.GetActive().GetStableFactionKey(faction.GetFactionKey()) != expected) return null;
		return member;
	}

	void Start(SCR_GameModeCampaign campaign)
	{
		if (!Replication.IsServer() || !campaign || !campaign.IsMaster()) return;
		m_Campaign = campaign;
		m_Observers = ObserversSystem.Cast(GetGame().GetWorld().FindSystem(ObserversSystem));
	}

	// Читается текущая incarnation slot, без callback со старой group identity.
	protected void Observe(AICF_FactionState state, int side, array<int> current)
	{
		if (!state) return;
		for (int index; index < state.GetSlotCount(); index++)
		{
			AICF_GroupSlot slot = state.GetSlot(index);
			if (!slot || !slot.IsCombatReady()) continue;
			FactionKey expected = AICF_ContentProfile.GetActive().GetStableFactionKey(state.GetFactionKey());
			IEntity member = ResolveMember(slot.GetGroup(), expected);
			if (!member) continue;
			int key = OBSERVER_KEY_BASE + 2 * slot.GetSlotId() + side;
			// Entity origin следует за живым бойцом, а не controller группы.
			m_Observers.InsertObserverSP(key, 0, 0, member);
			current.Insert(key);
		}
	}

	void Update(AICF_FactionState us, AICF_FactionState ussr)
	{
		if (m_bStopped || !m_Observers) return;
		array<int> current = {};
		if (Replication.IsServer() && m_Campaign && m_Campaign.IsMaster() && m_Campaign.IsRunning())
		{
			if (us && AICF_ContentProfile.GetActive().GetStableFactionKey(us.GetFactionKey()) == "US") Observe(us, 0, current);
			if (ussr && AICF_ContentProfile.GetActive().GetStableFactionKey(ussr.GetFactionKey()) == "USSR") Observe(ussr, 1, current);
		}
		foreach (int key : m_aKeys)
		{
			if (!current.Contains(key)) m_Observers.RemoveObserverSP(key);
		}
		m_aKeys.Copy(current);
	}

	void Stop()
	{
		if (m_bStopped) return;
		m_bStopped = true;
		if (m_Observers)
		{
			foreach (int key : m_aKeys) m_Observers.RemoveObserverSP(key);
		}
		m_aKeys.Clear();
		m_Observers = null;
		m_Campaign = null;
	}
}
