// Локальный PRNG не меняет глобальную последовательность игрового AI.
class AICF_FIAGarrisonRandom
{
	protected int m_iState;
	void AICF_FIAGarrisonRandom(int seed) { m_iState = Math.Clamp(seed, 1, 2147483646); }
	int Next(int count)
	{
		// Park-Miller / Schrage: промежуточные значения помещаются в signed int.
		int value = 16807 * (m_iState % 127773) - 2836 * (m_iState / 127773);
		if (value <= 0) value += 2147483647;
		m_iState = value;
		if (count <= 1) return 0;
		return value % count;
	}
}

class AICF_FIAGarrisonPlanner
{
	int m_iSeed;
	int m_iStrongpoint = -1;
	string m_sError;
	ref array<ref AICF_FIAGarrison> m_aPlan = {};
	protected ref AICF_FIAGarrisonRandom m_Random;

	static void Composition(AICF_EDifficulty difficulty, bool strongpoint, bool tanksAvailable, int roll, out int apcs, out int tanks, out int passengerLimit)
	{
		apcs = 0;
		tanks = 0;
		passengerLimit = 0;
		if (difficulty == AICF_EDifficulty.EASY) return;
		if (difficulty == AICF_EDifficulty.MEDIUM)
		{
			apcs = 1;
			passengerLimit = 2 + 2 * (roll % 2);
			return;
		}
		passengerLimit = 16;
		if (strongpoint) { apcs = 2; tanks = 1 + roll % 2; }
		else { tanks = roll % 3; apcs = 2 - tanks; }
		if (!tanksAvailable) { apcs += tanks; tanks = 0; }
	}

	bool Build(SCR_CampaignFaction faction, AICF_ObjectiveGraph graph, AICF_EDifficulty difficulty)
	{
		if (!Replication.IsServer() || !faction || !graph || m_Random) return false;
		// В runtime native sample встречается отрицательным даже при positive bounds.
		// Нормализуем после modulo (без AbsInt(INT_MIN)), добавляя entropy запуска.
		int sample = Math.RandomInt(0, 1000000) ^ System.GetUnixTime() ^ System.GetTickCount();
		m_iSeed = 1 + Math.AbsInt(sample % 999999);
		m_Random = new AICF_FIAGarrisonRandom(m_iSeed);
		if (difficulty == AICF_EDifficulty.EASY) return true;
		AICF_FIAGarrisonCatalog catalog = new AICF_FIAGarrisonCatalog();
		if (!catalog.Build(faction, AICF_ContentProfile.GetActive())) { m_sError = "NO_FIA_APC_OR_PASSENGER_TEMPLATE"; return false; }
		if (difficulty == AICF_EDifficulty.HARD)
		{
			array<int> headquarters = {};
			array<int> candidates = {};
			AICF_FIAStrongpointPolicy.Collect(graph, headquarters, candidates);
			if (candidates.IsEmpty())
			{
				m_sError = "NO_ELIGIBLE_STRONGPOINT";
				AICF_Stage3Diagnostics.Warning("FIA_STRONGPOINT_UNAVAILABLE", string.Format("nodes=%1 hqs=%2 candidates=0 action=RECONFIGURE_HQ_OR_BASE_WHITELIST", graph.GetNodeCount(), headquarters.Count()));
				return false;
			}
			m_iStrongpoint = candidates[m_Random.Next(candidates.Count())];
			AICF_Stage3Diagnostics.Info("FIA_STRONGPOINT_SELECTED", string.Format("node=%1 candidates=%2 graph_revision=%3 seed=%4", m_iStrongpoint, candidates.Count(), graph.GetRevision(), m_iSeed));
			if (catalog.m_aTanks.IsEmpty()) AICF_Stage3Diagnostics.Warning("FIA_GARRISON_TANK_FALLBACK", "reason=PROFILE_HAS_NO_FIA_TANK replacement=APC");
		}
		for (int order; order < graph.GetNodeCount(); order++)
		{
			// Усиленная точка получает первые spawn slots, до обычных гарнизонов.
			int i = order;
			if (m_iStrongpoint >= 0) i = (m_iStrongpoint + order) % graph.GetNodeCount();
			AICF_ObjectiveNode node = graph.GetNode(i);
			if (!node || !node.IsObjective()) continue;
			SCR_CampaignMilitaryBaseComponent base = node.GetBase();
			if (!base || !base.GetOwner() || base.IsHQ() || base.GetFaction() != faction) continue;
			int apcs, tanks, passengerLimit;
			int choices = 3;
			if (difficulty == AICF_EDifficulty.MEDIUM || i == m_iStrongpoint) choices = 2;
			Composition(difficulty, i == m_iStrongpoint, !catalog.m_aTanks.IsEmpty(), m_Random.Next(choices), apcs, tanks, passengerLimit);
			AICF_Stage3Diagnostics.Info("FIA_GARRISON_COMPOSITION", string.Format("node=%1 base=%2 strongpoint=%3 apcs=%4 tanks=%5 passenger_limit=%6 seed=%7", i, base.GetOwner().GetID(), i == m_iStrongpoint, apcs, tanks, passengerLimit, m_iSeed));
			for (int asset; asset < apcs + tanks; asset++)
			{
				AICF_FIAGarrison g = new AICF_FIAGarrison();
				g.m_iSlot = 10000 + m_aPlan.Count();
				g.m_iNode = i;
				g.m_bStrongpoint = i == m_iStrongpoint;
				g.m_Base = base;
				g.m_BaseId = base.GetOwner().GetID();
				g.m_vHome = base.GetOwner().GetOrigin();
				g.m_Faction = faction;
				g.m_bTank = asset >= apcs;
				g.m_bPassengers = !g.m_bTank;
				if (g.m_bTank) g.m_sPrefab = catalog.m_aTanks[m_Random.Next(catalog.m_aTanks.Count())];
				else
				{
					g.m_sPrefab = catalog.m_aAPCs[m_Random.Next(catalog.m_aAPCs.Count())];
					AICF_FIAGarrisonTemplate template = catalog.m_aPassengers[m_Random.Next(catalog.m_aPassengers.Count())];
					g.m_sPassengerTemplate = template.m_sPrefab;
					for (int member; member < Math.Min(passengerLimit, template.m_aMembers.Count()); member++) g.m_aPassengerPrefabs.Insert(template.m_aMembers[member]);
				}
				m_aPlan.Insert(g);
				g.Log("FIA_GARRISON_ASSET_PLAN", string.Format("node=%1 strongpoint=%2 prefab=%3 passenger_template=%4 passengers_max=%5", i, g.m_bStrongpoint, g.m_sPrefab, g.m_sPassengerTemplate, g.m_aPassengerPrefabs.Count()));
			}
		}
		return true;
	}
}
