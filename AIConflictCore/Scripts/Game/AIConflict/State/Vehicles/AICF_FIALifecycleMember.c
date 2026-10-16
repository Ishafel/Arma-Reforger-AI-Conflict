// Логическое место переживает native dematerialization; entity identity — нет.
class AICF_FIALifecycleMember
{
	int m_iSeat;
	ResourceName m_sPrefab;
	ChimeraCharacter m_Entity;
	EntityID m_Id;
	AIAgent m_Agent;
	bool m_bLost;
	bool m_bRestoring;
	bool m_bRestoreIssued;
	int m_iRestoreAtMs;
	string m_sInventory;
	ref array<string> m_aZoneNames = {};
	ref array<float> m_aHealth = {};

	bool Capture()
	{
		if (!AICF_GroupRuntime.IsAliveCharacter(m_Entity) || m_Entity.GetID() != m_Id ||
			!AICF_VehicleBoardingMutationFence.IsAuthoritativeAIEntity(m_Entity)) return false;
		if (!AICF_ContentProfile.GetActive().IsFIAEquipmentReady(m_Entity) ||
			!AICF_LoadoutInventory.Capture(m_Entity, m_sInventory)) return false;
		DamageManagerComponent damage = m_Entity.GetDamageManager();
		array<HitZone> zones = {};
		if (!damage) return false;
		damage.GetAllHitZonesInHierarchy(zones);
		if (zones.IsEmpty()) return false;
		m_aZoneNames.Clear();
		m_aHealth.Clear();
		foreach (HitZone zone : zones)
		{
			if (!zone || zone.GetMaxHealth() <= 0) return false;
			m_aZoneNames.Insert(zone.GetName());
			m_aHealth.Insert(zone.GetHealth());
		}
		return true;
	}

	bool Restore()
	{
		if (!m_bRestoring) return true;
		if (!m_Entity || m_Entity.GetID() != m_Id ||
			!AICF_VehicleBoardingMutationFence.IsAuthoritativeAIEntity(m_Entity)) return false;
		if (!AICF_ContentProfile.GetActive().IsFIAEquipmentReady(m_Entity)) return false;
		if (!m_bRestoreIssued)
		{
			if (!AICF_LoadoutInventory.Restore(m_Entity, m_sInventory)) return false;
			m_bRestoreIssued = true;
			m_iRestoreAtMs = System.GetTickCount() + 2000;
			return false;
		}
		if (System.GetTickCount() < m_iRestoreAtMs) return false;
		DamageManagerComponent damage = m_Entity.GetDamageManager();
		array<HitZone> zones = {};
		if (!damage) return false;
		damage.GetAllHitZonesInHierarchy(zones);
		if (zones.Count() != m_aHealth.Count()) return false;
		foreach (int index, HitZone zone : zones)
		{
			if (!zone || zone.GetName() != m_aZoneNames[index]) return false;
		}
		foreach (int index, HitZone zone : zones)
		{
			if (!m_Entity || m_Entity.GetID() != m_Id ||
				!AICF_VehicleBoardingMutationFence.IsAuthoritativeAIEntity(m_Entity)) return false;
			zone.SetHealth(Math.Min(zone.GetHealth(), m_aHealth[index]));
		}
		m_bRestoring = false;
		return true;
	}
}
