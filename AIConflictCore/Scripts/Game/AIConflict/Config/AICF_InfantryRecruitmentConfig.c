// Supplies за одного бойца; типы и prefab остаются у content profile.
class AICF_InfantryRecruitmentConfig
{
	static const float MAX_DISTANCE_METERS = 500;
	static const float ARRIVAL_METERS = 35;
	static const float COMBAT_RADIUS_METERS = 100;
	static const int COMBAT_QUIET_MS = 30000;
	static const int APPROACH_TIMEOUT_MS = 180000;
	static const int SPAWN_TIMEOUT_MS = 30000;
	static const int VISIT_TIMEOUT_MS = 300000;
	static const int PLAYER_APPROACH_TIMEOUT_MS = 900000;
	static const int PLAYER_VISIT_TIMEOUT_MS = 1200000;
	static const int RETRY_MS = 60000;
	static const int PURCHASE_INTERVAL_MS = 3000;
	static const float PLANNING_SPEED_MPS = 3;
	static const float SUPPLY_HORIZON_SECONDS = 120;
	static const int SUPPLY_WAIT_TIMEOUT_MS = 120000;
	static const int REPLAN_INTERVAL_MS = 30000;
	static const int REPLAN_COOLDOWN_MS = 60000;
	static const float REPLAN_GAIN_SECONDS = 30;
	static const float REPLAN_GAIN_FRACTION = 0.25;
	int m_iRiflemanCost = 10;
	int m_iMedicCost = 15;
	int m_iGrenadierCost = 20;
	int m_iSpecialistCost = 20;

	void AICF_InfantryRecruitmentConfig()
	{
		string value;
		if (System.GetCLIParam("aicfRecruitRiflemanCost", value))
			m_iRiflemanCost = Math.ClampInt(value.ToInt(), 1, 1000);
		if (System.GetCLIParam("aicfRecruitMedicCost", value))
			m_iMedicCost = Math.ClampInt(value.ToInt(), 1, 1000);
		if (System.GetCLIParam("aicfRecruitGrenadierCost", value))
			m_iGrenadierCost = Math.ClampInt(value.ToInt(), 1, 1000);
		if (System.GetCLIParam("aicfRecruitSpecialistCost", value))
			m_iSpecialistCost = Math.ClampInt(value.ToInt(), 1, 1000);
	}

	int Cost(string role)
	{
		if (role == "MEDIC")
			return m_iMedicCost;
		if (role == "GRENADIER" || role == "ANTI_TANK")
			return m_iGrenadierCost;
		if (role == "MACHINE_GUNNER" || role == "AUTOMATIC_RIFLEMAN")
			return m_iSpecialistCost;
		return m_iRiflemanCost;
	}
}
