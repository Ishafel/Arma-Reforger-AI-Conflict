// Состояние тишины принадлежит конкретной service entity, а не центру базы.
class AICF_BarracksCombatZone
{
	SCR_ServicePointComponent m_Service;
	EntityID m_Id;
	vector m_vPosition;
	int m_iQuietUntilMs;
	int m_iSeen;
	bool m_bBlocked = true;

	bool Matches(SCR_ServicePointComponent service)
	{
		return service && service == m_Service && service.GetOwner() &&
			service.GetOwner().GetID() == m_Id && service.GetOwner().GetOrigin() == m_vPosition;
	}

	void Log(string eventName, string reason)
	{
		AICF_Stage4Diagnostics.Info(eventName, string.Format(
			"service=%1 position=%2 radius_m=%3 quiet_ms=%4 reason=%5",
			m_Id, m_vPosition, AICF_InfantryRecruitmentConfig.COMBAT_RADIUS_METERS,
			AICF_InfantryRecruitmentConfig.COMBAT_QUIET_MS, reason));
	}
}

// Штатные события фактического выстрела/броска работают для AI и игроков.
class AICF_BarracksShotObserver
{
	IEntity m_Character;
	EntityID m_Id;
	protected EventHandlerManagerComponent m_Events;

	void Start(IEntity character)
	{
		m_Character = character;
		m_Id = character.GetID();
		m_Events = EventHandlerManagerComponent.Cast(character.FindComponent(EventHandlerManagerComponent));
		if (!m_Events)
			return;
		m_Events.RegisterScriptHandler("OnProjectileShot", this, OnWeaponFired, true);
		m_Events.RegisterScriptHandler("OnGrenadeThrown", this, OnGrenadeThrown, true);
	}

	protected void OnWeaponFired(int playerID, BaseWeaponComponent weapon, IEntity entity)
	{
		if (!m_Character || m_Character.GetID() != m_Id)
			return;
		vector position = m_Character.GetOrigin();
		if (weapon && weapon.GetOwner())
			position = weapon.GetOwner().GetOrigin();
		AICF_BarracksCombatSafety.RecordCombat(position, "SHOT");
	}

	protected void OnGrenadeThrown(int playerID, BaseWeaponComponent weapon, IEntity entity)
	{
		if (!weapon || weapon.GetWeaponType() == EWeaponType.WT_SMOKEGRENADE)
			return;
		OnWeaponFired(playerID, weapon, entity);
	}

	void Stop()
	{
		if (m_Events)
		{
			m_Events.RemoveScriptHandler("OnProjectileShot", this, OnWeaponFired, true);
			m_Events.RemoveScriptHandler("OnGrenadeThrown", this, OnGrenadeThrown, true);
		}
		m_Events = null;
		m_Character = null;
	}
}

// Только наблюдение; waypoint, donor и деньги остаются у прежних владельцев.
class AICF_BarracksCombatSafety
{
	protected static AICF_BarracksCombatSafety s_Active;
	protected SCR_GameModeCampaign m_Campaign;
	protected ref array<ref AICF_BarracksCombatZone> m_aZones = {};
	protected ref array<ref AICF_BarracksShotObserver> m_aObservers = {};
	protected int m_iRevision;

	protected bool IsAuthority()
	{
		return Replication.IsServer() && m_Campaign && m_Campaign.IsMaster() && m_Campaign.IsRunning();
	}

	static bool IsSafe(SCR_ServicePointComponent service)
	{
		if (!s_Active || !s_Active.IsAuthority())
			return false;
		AICF_BarracksCombatZone zone = s_Active.FindZone(service);
		return zone && System.GetTickCount() >= zone.m_iQuietUntilMs;
	}

	static void RecordCombat(vector position, string reason)
	{
		if (!s_Active || !s_Active.IsAuthority())
			return;
		int now = System.GetTickCount();
		float radius = AICF_InfantryRecruitmentConfig.COMBAT_RADIUS_METERS;
		foreach (AICF_BarracksCombatZone zone : s_Active.m_aZones)
		{
			if (!zone.Matches(zone.m_Service) || vector.DistanceSqXZ(position, zone.m_vPosition) > radius * radius)
				continue;
			if (!zone.m_bBlocked)
				zone.Log("BARRACKS_COMBAT_BLOCKED", reason);
			zone.m_bBlocked = true;
			zone.m_iQuietUntilMs = now + AICF_InfantryRecruitmentConfig.COMBAT_QUIET_MS;
		}
	}

	static void TrackCharacter(IEntity character)
	{
		if (s_Active && s_Active.IsAuthority() && character && character.GetWorld() == GetGame().GetWorld())
			s_Active.AddCharacter(character);
	}

	protected bool AddCharacter(IEntity entity)
	{
		if (!SCR_ChimeraCharacter.Cast(entity))
			return true;
		foreach (AICF_BarracksShotObserver existing : m_aObservers)
		{
			if (existing.m_Character == entity && existing.m_Id == entity.GetID())
				return true;
		}
		AICF_BarracksShotObserver observer = new AICF_BarracksShotObserver();
		observer.Start(entity);
		m_aObservers.Insert(observer);
		return true;
	}

	protected AICF_BarracksCombatZone FindZone(SCR_ServicePointComponent service)
	{
		foreach (AICF_BarracksCombatZone zone : m_aZones)
		{
			if (zone.Matches(service))
				return zone;
		}
		return null;
	}

	void Update(SCR_GameModeCampaign campaign, AICF_ObjectiveGraph graph)
	{
		m_Campaign = campaign;
		if (!IsAuthority() || !graph)
			return;
		if (s_Active != this)
		{
			if (s_Active)
				return;
			s_Active = this;
			// Единственный полный обход: персонажи, появившиеся до старта сервиса.
			vector mins, maxs;
			GetGame().GetWorld().GetBoundBox(mins, maxs);
			GetGame().GetWorld().QueryEntitiesByAABB(mins, maxs, AddCharacter, null, EQueryEntitiesFlags.DYNAMIC);
		}
		m_iRevision++;
		int now = System.GetTickCount();
		for (int index = 0; index < graph.GetNodeCount(); index++)
		{
			AICF_ObjectiveNode node = graph.GetNode(index);
			if (!node || !node.GetBase())
				continue;
			array<SCR_ServicePointComponent> services = {};
			node.GetBase().GetServices(services);
			foreach (SCR_ServicePointComponent service : services)
			{
				if (!service || !service.GetOwner() || service.GetType() != SCR_EServicePointType.BARRACKS)
					continue;
				AICF_BarracksCombatZone zone = FindZone(service);
				if (!zone)
				{
					zone = new AICF_BarracksCombatZone();
					zone.m_Service = service;
					zone.m_Id = service.GetOwner().GetID();
					zone.m_vPosition = service.GetOwner().GetOrigin();
					// История новой службы неизвестна: сначала наблюдаем полные 30 секунд.
					zone.m_iQuietUntilMs = now + AICF_InfantryRecruitmentConfig.COMBAT_QUIET_MS;
					m_aZones.Insert(zone);
					zone.Log("BARRACKS_COMBAT_BLOCKED", "INITIAL_OBSERVATION");
				}
				zone.m_iSeen = m_iRevision;
				if (zone.m_bBlocked && now >= zone.m_iQuietUntilMs)
				{
					zone.m_bBlocked = false;
					zone.Log("BARRACKS_COMBAT_RESUMED", "QUIET");
				}
			}
		}
		for (int zoneIndex = m_aZones.Count() - 1; zoneIndex >= 0; zoneIndex--)
		{
			if (m_aZones[zoneIndex].m_iSeen != m_iRevision)
				m_aZones.Remove(zoneIndex);
		}
		for (int observerIndex = m_aObservers.Count() - 1; observerIndex >= 0; observerIndex--)
		{
			AICF_BarracksShotObserver observer = m_aObservers[observerIndex];
			if (observer.m_Character && observer.m_Character.GetID() == observer.m_Id)
				continue;
			observer.Stop();
			m_aObservers.Remove(observerIndex);
		}
	}

	void Stop()
	{
		if (s_Active == this)
			s_Active = null;
		foreach (AICF_BarracksShotObserver observer : m_aObservers)
			observer.Stop();
		m_aObservers.Clear();
		m_aZones.Clear();
		m_Campaign = null;
	}

	void ~AICF_BarracksCombatSafety()
	{
		Stop();
	}
}
