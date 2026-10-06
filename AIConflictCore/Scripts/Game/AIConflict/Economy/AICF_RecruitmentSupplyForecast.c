// Read-only адаптер закреплённого stock SupplyIncomeTimer (1.8.0.13).
// Не вызывает CalculateSupplyRegenerationAmount: тот меняет replicated state.
modded class SCR_CampaignMilitaryBaseComponent
{
	float AICF_GetRecruitmentCeiling()
	{
		float ceiling = GetSuppliesMax();
		SCR_GameModeCampaign campaign = SCR_GameModeCampaign.GetInstance();
		if (!IsHQ() && campaign)
			ceiling = Math.Min(ceiling, campaign.GetSuppliesReplenishThreshold());
		return Math.Max(0, ceiling);
	}

	void AICF_GetRecruitmentIncome(out float amount, out float interval, out float nextArrival)
	{
		amount = 0;
		interval = 0;
		nextArrival = 0;
		SCR_GameModeCampaign campaign = SCR_GameModeCampaign.GetInstance();
		SCR_CampaignFaction faction = GetCampaignFaction();
		if (!Replication.IsServer() || !campaign || !campaign.IsMaster() || !campaign.IsRunning() ||
			!GetOwner() || !faction || !m_SpawnPoint)
			return;
		ChimeraWorld world = GetOwner().GetWorld();
		if (!world || (m_eType != SCR_ECampaignBaseType.SOURCE_BASE &&
			!IsHQRadioTrafficPossible(faction, SCR_ERadioCoverageStatus.BOTH_WAYS)) ||
			(m_CapturingFaction && m_CapturingFaction != faction) ||
			world.GetServerTimestamp().Less(m_fRespawnAvailableSince))
			return;
		// Последняя рассчитанная stock сумма уже включает quick/neighbor bonus.
		// Не обещаем доставку логистики или доход, ещё не наблюдавшийся у stock.
		float ceiling = AICF_GetRecruitmentCeiling();
		amount = Math.Max(0, Math.Min(GetSuppliesIncome(), ceiling));
		interval = GetSuppliesArrivalTimer();
		nextArrival = Math.Max(0, GetSuppliesArrivalTime().DiffSeconds(world.GetServerTimestamp()));
	}
}

// Чистая оценка; intentions не являются денежной reservation.
class AICF_RecruitmentSupplyForecast
{
	float m_fStock;
	float m_fCeiling;
	float m_fOtherArrival;
	ref array<int> m_aCosts = {};
	float m_fIncome;
	float m_fInterval;
	float m_fNextArrival;
	int m_iOwnDemand;
	int m_iOtherDemand;
	int m_iMembers;
	float m_fTravelSeconds;
	float m_fSupplySeconds;
	float m_fWaitSeconds;
	float m_fCompletionSeconds;

	bool Evaluate(float distance, float remainingSeconds)
	{
		m_fTravelSeconds = Math.Max(0, distance - AICF_InfantryRecruitmentConfig.ARRIVAL_METERS) /
			AICF_InfantryRecruitmentConfig.PLANNING_SPEED_MPS;
		m_fSupplySeconds = 0;
		m_fCompletionSeconds = float.MAX;
		m_fWaitSeconds = float.MAX;
		if (m_iOwnDemand + m_iOtherDemand > Math.Max(m_fStock, m_fCeiling))
			return EvaluateOverflow(remainingSeconds);
		float deficit = Math.Max(0, m_iOwnDemand + m_iOtherDemand - m_fStock);
		if (deficit > 0)
		{
			if (m_fIncome <= 0 || m_fInterval <= 0)
				return false;
			float packages = Math.Ceil(deficit / m_fIncome);
			m_fSupplySeconds = m_fNextArrival + (packages - 1) * m_fInterval;
		}
		m_fWaitSeconds = Math.Max(0, m_fSupplySeconds - m_fTravelSeconds);
		m_fCompletionSeconds = Math.Max(m_fTravelSeconds, m_fSupplySeconds) +
			m_iMembers * AICF_InfantryRecruitmentConfig.PURCHASE_INTERVAL_MS / 1000.0;
		return m_iOwnDemand > 0 && m_fWaitSeconds <= AICF_InfantryRecruitmentConfig.SUPPLY_HORIZON_SECONDS &&
			m_fCompletionSeconds <= remainingSeconds;
	}

	bool CanApproach(bool playerRequested)
	{
		float deadline = AICF_InfantryRecruitmentConfig.APPROACH_TIMEOUT_MS / 1000.0;
		if (playerRequested)
			deadline = AICF_InfantryRecruitmentConfig.PLAYER_APPROACH_TIMEOUT_MS / 1000.0;
		// Tick прекращает подход при >= deadline, поэтому равенство не допускается.
		return m_fTravelSeconds < deadline;
	}

	// Консервативная очередь: остальные визиты покупают первыми, расход не
	// начинается раньше прибытия последнего покупателя. Не обещаем освобождение
	// склада ещё идущими группами. Пакет на границе времени приходит до покупки.
	protected bool EvaluateOverflow(float remainingSeconds)
	{
		int quoted;
		foreach (int price : m_aCosts) quoted += price;
		if (quoted != m_iOwnDemand + m_iOtherDemand || m_iOwnDemand <= 0)
			return false;
		float now = Math.Max(m_fTravelSeconds, m_fOtherArrival);
		float stock = m_fStock;
		float next = Math.Max(0, m_fNextArrival);
		bool income = m_fIncome > 0 && m_fInterval > 0 && m_fCeiling > 0;
		foreach (int cost : m_aCosts)
		{
			if (cost <= 0 || now > remainingSeconds) return false;
			if (income && next <= now)
			{
				float arrived = Math.Floor((now - next) / m_fInterval) + 1;
				// Доставленный извне запас выше replenish threshold не исчезает.
				stock = Math.Max(stock, Math.Min(m_fCeiling, stock + arrived * m_fIncome));
				next += arrived * m_fInterval;
			}
			if (stock < cost)
			{
				if (!income || cost > m_fCeiling) return false;
				float packages = Math.Ceil((cost - stock) / m_fIncome);
				now = next + (packages - 1) * m_fInterval;
				stock = Math.Min(m_fCeiling, stock + packages * m_fIncome);
				next = now + m_fInterval;
				m_fSupplySeconds = now;
			}
			stock -= cost;
			now += AICF_InfantryRecruitmentConfig.PURCHASE_INTERVAL_MS / 1000.0;
		}
		m_fCompletionSeconds = now;
		m_fWaitSeconds = Math.Max(0, now - m_fTravelSeconds -
			m_iMembers * AICF_InfantryRecruitmentConfig.PURCHASE_INTERVAL_MS / 1000.0);
		return m_fWaitSeconds <= AICF_InfantryRecruitmentConfig.SUPPLY_HORIZON_SECONDS && now <= remainingSeconds;
	}

	string Describe()
	{
		return string.Format("stock=%1 income=%2 income_interval_s=%3 next_income_s=%4 own_demand=%5 other_demand=%6",
			m_fStock, m_fIncome, m_fInterval, m_fNextArrival, m_iOwnDemand, m_iOtherDemand) +
			string.Format(" travel_s=%1 supply_s=%2 wait_s=%3 completion_s=%4", m_fTravelSeconds,
				m_fSupplySeconds, m_fWaitSeconds, m_fCompletionSeconds) +
			string.Format(" income_ceiling=%1 other_arrival_s=%2", m_fCeiling, m_fOtherArrival);
	}
}
