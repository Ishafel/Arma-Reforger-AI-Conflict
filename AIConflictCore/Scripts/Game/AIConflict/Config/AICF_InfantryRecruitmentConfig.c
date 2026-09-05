// Supplies за одного бойца; типы и prefab остаются у content profile.
class AICF_InfantryRecruitmentConfig
{
	static const float MAX_DISTANCE_METERS = 500;
	static const float ARRIVAL_METERS = 35;
	static const int APPROACH_TIMEOUT_MS = 180000;
	static const int SPAWN_TIMEOUT_MS = 30000;
	static const int VISIT_TIMEOUT_MS = 300000;
	static const int RETRY_MS = 60000;
	static const int PURCHASE_INTERVAL_MS = 3000;
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
