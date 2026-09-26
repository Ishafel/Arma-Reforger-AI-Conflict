// Умеренная реакция на фоновый бой только во время текущего пехотного перехода.
// Ничего не записывает в threat memory, waypoint, combat mode или скорость.
class AICF_InfantryAdvancePolicy
{
	static const float OBJECTIVE_RADIUS_METERS = 30.0;
	static const float CLOSE_THREAT_RADIUS_METERS = 75.0;
	static const float DISTANT_THREAT_RADIUS_METERS = 150.0;
	static const float MAX_BACKGROUND_DANGER = 2.0;
	static const float MAX_SUPPRESSION = 0.01;
	static const float OBSERVE_DURATION_FACTOR = 0.5;
	static const float MAX_OBSERVE_SECONDS = 3.0;
	static const float DISTANT_OBSERVE_SECONDS = 1.5;

	static bool IsAdvancing(SCR_AIUtilityComponent utility)
	{
		if (!Replication.IsServer() || !utility || !GetGame())
			return false;
		IEntity character = utility.m_OwnerEntity;
		if (!AICF_GroupRuntime.IsAliveCharacter(character) || character.GetWorld() != GetGame().GetWorld())
			return false;
		CharacterControllerComponent control = CharacterControllerComponent.Cast(character.FindComponent(CharacterControllerComponent));
		RplComponent rpl = RplComponent.Cast(character.FindComponent(RplComponent));
		CompartmentAccessComponent access = CompartmentAccessComponent.Cast(character.FindComponent(CompartmentAccessComponent));
		if (!control || control.IsPlayerControlled() || !rpl || !rpl.IsMaster() || !access ||
			access.IsInCompartment() || access.IsGettingIn() || access.IsGettingOut())
			return false;
		AIAgent agent = utility.GetAIAgent();
		if (!agent || agent.GetControlledEntity() != character)
			return false;
		SCR_AIGroup group = SCR_AIGroup.Cast(agent.GetParentGroup());
		AICF_MatchController controller = AICF_MatchController.GetActiveController();
		if (!group || !controller)
			return false;
		AICF_GroupSlot slot = controller.FindManagedInfantrySlot(group);
		if (!slot || slot.GetGroup() != group || !slot.IsCombatReady() ||
			slot.GetUnitType() != AICF_EGroupUnitType.INFANTRY || !slot.HasStrategicDestination() ||
			slot.IsSystemHoldOrder() || slot.IsTemporaryRouteReplanHold() || slot.IsPersistentStuckFieldHold() ||
			slot.IsLoneSurvivorRetreat() || slot.IsRecruitingInfantry())
			return false;
		AIWaypoint waypoint = slot.GetWaypoint();
		if (!waypoint || group.GetCurrentWaypoint() != waypoint ||
			SCR_SearchAndDestroyWaypoint.Cast(waypoint) || SCR_SmartActionWaypoint.Cast(waypoint))
			return false;
		// Уже прибывший боец сохраняет штатное удержание позиции, даже если ALL ждёт остальных.
		float radius = Math.Max(OBJECTIVE_RADIUS_METERS, waypoint.GetCompletionRadius());
		return vector.DistanceSqXZ(character.GetOrigin(), waypoint.GetOrigin()) > radius * radius;
	}

	static bool CanShortenObservation(SCR_AIUtilityComponent utility, int sectorId)
	{
		if (!utility || !utility.m_CombatComponent || !utility.m_ThreatSystem || !utility.m_SectorThreatFilter ||
			sectorId < 0 || sectorId >= SCR_AISectorThreatFilter.SECTOR_COUNT)
			return false;
		if (utility.m_CombatComponent.GetCurrentTarget() || utility.m_ThreatSystem.GetSuppressionMeasure() > MAX_SUPPRESSION)
			return false;
		float suppression, shots, injury, endangered;
		utility.m_ThreatSystem.GetThreatValues(suppression, shots, injury, endangered);
		if (injury > 0 || !IsAdvancing(utility))
			return false;
		SCR_AISectorThreatFilter threats = utility.m_SectorThreatFilter;
		if (!threats.IsSectorActive(sectorId))
			return false;
		// Проверяем оба сектора: слабый основной не должен скрыть прямой обстрел сбоку.
		for (int index; index < SCR_AISectorThreatFilter.SECTOR_COUNT; index++)
		{
			if (!threats.IsSectorActive(index))
				continue;
			SCR_EAIThreatSectorFlags flags = threats.GetSectorFlags(index);
			if ((flags & (SCR_EAIThreatSectorFlags.DIRECTED_AT_ME | SCR_EAIThreatSectorFlags.CAUSED_DAMAGE)) ||
				threats.GetSectorDanger(index) >= MAX_BACKGROUND_DANGER ||
				vector.DistanceSqXZ(utility.m_OwnerEntity.GetOrigin(), threats.GetSectorPos(index)) <=
					CLOSE_THREAT_RADIUS_METERS * CLOSE_THREAT_RADIUS_METERS)
				return false;
		}
		return true;
	}

	static float GetObserveWindow(float nativeDuration, float threatDistance)
	{
		float limit = MAX_OBSERVE_SECONDS;
		if (threatDistance >= DISTANT_THREAT_RADIUS_METERS)
			limit = DISTANT_OBSERVE_SECONDS;
		return Math.Min(nativeDuration * OBSERVE_DURATION_FACTOR, limit);
	}
}

modded class SCR_AIObserveThreatSystemBehavior
{
	override float CustomEvaluate()
	{
		// Сначала штатное обновление таймеров. При любой неопределённости оставляем его результат.
		float nativePriority = super.CustomEvaluate();
		if (nativePriority <= PRIORITY_BEHAVIOR_OBSERVE_THREATS_LOW_PRIORITY ||
			!AICF_InfantryAdvancePolicy.CanShortenObservation(m_Utility, m_iCurrentSector))
			return nativePriority;
		float distance = vector.DistanceXZ(m_Utility.m_OwnerEntity.GetOrigin(),
			m_Utility.m_SectorThreatFilter.GetSectorPos(m_iCurrentSector));
		float window = AICF_InfantryAdvancePolicy.GetObserveWindow(m_fHighPriorityDuration_s, distance);
		float elapsed = GetGame().GetWorld().GetTimestamp().DiffSeconds(m_TimestampStartHighPriorityState);
		if (elapsed < window)
			return nativePriority;
		// Движение получает преимущество над осматриванием. Стрельба, лечение и опасность не меняются.
		return PRIORITY_BEHAVIOR_OBSERVE_THREATS_LOW_PRIORITY;
	}
}
