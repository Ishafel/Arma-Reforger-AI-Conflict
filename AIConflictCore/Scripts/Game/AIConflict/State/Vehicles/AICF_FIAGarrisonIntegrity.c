// Наблюдение начинается при появлении identity, до READY/первого recovery.
// Health invokers ловят урон и последующий ремонт между scheduler ticks.
class AICF_FIAGarrisonIntegrity
{
	protected AICF_FIAGarrison m_Garrison;
	protected IEntity m_Entity;
	protected EntityID m_EntityId;
	protected int m_iGeneration;
	protected bool m_bVehicle;
	protected bool m_bEssential;
	protected SCR_DamageManagerComponent m_Damage;
	protected ref array<SCR_HitZone> m_aZones = {};
	protected bool m_bStopped;

	void Start(AICF_FIAGarrison g, IEntity entity, bool vehicle, bool essential)
	{
		m_Garrison = g;
		m_Entity = entity;
		m_iGeneration = g.m_iGeneration;
		m_bVehicle = vehicle;
		m_bEssential = essential;
		if (!entity) { g.BlockRecovery("UNKNOWN_DAMAGE_ENTITY"); return; }
		m_EntityId = entity.GetID();
		m_Damage = SCR_DamageManagerComponent.Cast(entity.FindComponent(SCR_DamageManagerComponent));
		if (!m_Damage) { g.BlockRecovery("UNKNOWN_DAMAGE_MANAGER"); return; }
		m_Damage.GetOnDamage().Insert(OnDamage);
		array<HitZone> zones = {};
		m_Damage.GetAllHitZonesInHierarchy(zones);
		if (zones.IsEmpty()) { g.BlockRecovery("UNKNOWN_HITZONES"); return; }
		foreach (HitZone zone : zones)
		{
			SCR_HitZone scripted = SCR_HitZone.Cast(zone);
			if (!scripted) { g.BlockRecovery("UNKNOWN_HITZONE_OBSERVER"); continue; }
			m_aZones.Insert(scripted);
			scripted.GetOnHealthChanged().Insert(OnHealthChanged);
		}
		Check();
	}

	bool Check()
	{
		if (!Replication.IsServer() || m_bStopped || !m_Garrison) return false;
		if (!m_Entity || m_Entity.GetID() != m_EntityId || m_Garrison.m_iGeneration != m_iGeneration ||
			(m_bVehicle && (m_Garrison.m_Vehicle != m_Entity || m_Garrison.m_VehicleId != m_EntityId)))
		{
			m_Garrison.BlockRecovery("STALE_DAMAGE_IDENTITY");
			return false;
		}
		if (!m_Damage || m_Entity.FindComponent(SCR_DamageManagerComponent) != m_Damage)
		{
			m_Garrison.BlockRecovery("UNKNOWN_DAMAGE_MANAGER");
			return false;
		}
		if (m_Damage.IsDestroyed())
		{
			string loss = "CREW_DEAD";
			if (m_bVehicle) loss = "VEHICLE_DESTROYED";
			else if (!m_bEssential) loss = "PASSENGER_DEAD";
			m_Garrison.BlockRecovery(loss, m_EntityId);
			return false;
		}
		array<HitZone> current = {};
		m_Damage.GetAllHitZonesInHierarchy(current);
		if (current.Count() != m_aZones.Count()) { m_Garrison.BlockRecovery("UNKNOWN_HITZONES"); return false; }
		foreach (SCR_HitZone zone : m_aZones)
		{
			if (!zone || !current.Contains(zone) || zone.GetMaxHealth() <= 0)
			{
				m_Garrison.BlockRecovery("UNKNOWN_HITZONE_STATE");
				return false;
			}
			// Пассажир остаётся исходным member после плановой высадки.
			if ((m_bVehicle || m_bEssential) && !(zone.GetHealth() >= zone.GetMaxHealth()))
			{
				string reason = "CREW_WOUNDED";
				if (m_bVehicle) reason = "VEHICLE_DAMAGED";
				m_Garrison.BlockRecovery(reason, m_EntityId);
				return false;
			}
		}
		return true;
	}

	bool Matches(IEntity entity, EntityID entityId)
	{
		return m_Entity == entity && m_EntityId == entityId;
	}

	protected void OnHealthChanged()
	{
		Check();
	}

	protected void OnDamage(BaseDamageContext context)
	{
		// Повторное чтение фактически изменённого здоровья, без veto для поглощённого попадания.
		Check();
	}

	void Stop()
	{
		if (m_bStopped) return;
		m_bStopped = true;
		if (m_Damage) m_Damage.GetOnDamage().Remove(OnDamage);
		foreach (SCR_HitZone zone : m_aZones)
		{
			if (zone && zone.GetOnHealthChanged(false)) zone.GetOnHealthChanged(false).Remove(OnHealthChanged);
		}
		m_aZones.Clear();
		m_Garrison = null;
	}
}
