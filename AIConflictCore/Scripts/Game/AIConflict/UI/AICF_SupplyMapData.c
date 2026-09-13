// Read-only view of stock bases. No dependency on the server-only objective graph.
class AICF_SupplyMapBase
{
	SCR_CampaignMilitaryBaseComponent m_Base;
	EntityID m_EntityId;
	string m_sName;
	string m_sSortKey;

	void AICF_SupplyMapBase(SCR_CampaignMilitaryBaseComponent base)
	{
		m_Base = base;
		m_EntityId = base.GetOwner().GetID();
		m_sName = WidgetManager.Translate(base.GetBaseName());
		if (m_sName.IsEmpty())
			m_sName = "База";
		m_sSortKey = string.Format("%1|%2", m_sName, m_EntityId);
	}

	bool IsOwnedBy(Faction faction)
	{
		return faction && m_Base && m_Base.GetOwner() &&
			m_Base.GetOwner().GetID() == m_EntityId && m_Base.IsInitialized() &&
			m_Base.GetFaction() == faction;
	}

	RplId NetworkId()
	{
		if (!m_Base || !m_Base.GetOwner() || m_Base.GetOwner().GetID() != m_EntityId) return RplId.Invalid();
		RplComponent rpl = RplComponent.Cast(m_Base.GetOwner().FindComponent(RplComponent));
		if (!rpl) return RplId.Invalid();
		return rpl.Id();
	}
}

class AICF_SupplyMapData
{
	static void Collect(Faction faction, out array<ref AICF_SupplyMapBase> result)
	{
		result.Clear();
		SCR_MilitaryBaseSystem system = SCR_MilitaryBaseSystem.GetInstance();
		if (!faction || !system)
			return;

		array<SCR_MilitaryBaseComponent> bases = {};
		system.GetBases(bases);
		foreach (SCR_MilitaryBaseComponent militaryBase : bases)
		{
			SCR_CampaignMilitaryBaseComponent base = SCR_CampaignMilitaryBaseComponent.Cast(militaryBase);
			if (!base || !base.GetOwner() || !base.IsInitialized() || base.GetFaction() != faction)
				continue;

			AICF_SupplyMapBase entry = new AICF_SupplyMapBase(base);
			result.Insert(entry);
			// Name first, immutable entity identity as tie-break. Never store selection by label.
			int index = result.Count() - 1;
			while (index > 0 && result[index].m_sSortKey.Compare(result[index - 1].m_sSortKey) < 0)
			{
				result.SwapItems(index, index - 1);
				index--;
			}
		}
	}

	// A negative aggregate is the stock consumer's not-yet-replicated sentinel.
	// Missing storage is different from an existing consumer awaiting its snapshot.
	static bool ReadSupplies(AICF_SupplyMapBase entry, Faction faction, out int available)
	{
		available = 0;
		if (!entry || !entry.IsOwnedBy(faction))
			return false;
		if (!entry.m_Base.GetResourceConsumer())
			return false;

		float supplies = entry.m_Base.GetSupplies();
		if (supplies < 0 || supplies != supplies)
			return false;
		available = Math.Max(0, Math.Floor(supplies));
		return true;
	}

	static bool ReadFreeSpace(AICF_SupplyMapBase entry, Faction faction, out int free)
	{
		free = 0;
		int supplies;
		if (!ReadSupplies(entry, faction, supplies)) return false;
		float capacity = entry.m_Base.GetSuppliesMax();
		if (capacity < 0 || capacity != capacity) return false;
		free = Math.Max(0, Math.Floor(capacity - entry.m_Base.GetSupplies()));
		return true;
	}
}
