// Дополнительные guards применяются только к нашему запросу. Обычный GM и
// штатное восстановление персонажа продолжают использовать native поведение.
modded class SCR_PossessSpawnHandlerComponent
{
	protected AICF_SquadRespawnAttempt AICF_Attempt(SCR_SpawnRequestComponent request, SCR_SpawnData data)
	{
		SCR_PlayerController player = SCR_PlayerController.Cast(request.GetPlayerController());
		SCR_PossessSpawnData possess = SCR_PossessSpawnData.Cast(data);
		if (!player || !possess) return null;
		AICF_SquadRespawnAttempt attempt = player.AICF_GetSquadRespawnAttempt();
		if (!attempt || attempt.m_RplId != possess.GetRplId()) return null;
		return attempt;
	}

	override SCR_ESpawnResult CanHandleRequest_S(SCR_SpawnRequestComponent requestComponent, SCR_SpawnData data)
	{
		AICF_SquadRespawnAttempt attempt = AICF_Attempt(requestComponent, data);
		if (attempt && !AICF_SquadRespawnPolicy.IsCurrent(SCR_PlayerController.Cast(requestComponent.GetPlayerController()), attempt, GetEntity(data)))
			return SCR_ESpawnResult.CANNOT_POSSES;
		return super.CanHandleRequest_S(requestComponent, data);
	}

	override bool CanRequestSpawn_S(SCR_SpawnRequestComponent requestComponent, SCR_SpawnData data, out SCR_ESpawnResult result)
	{
		if (AICF_Attempt(requestComponent, data))
			return GetRespawnSystemComponent().CanRequestSpawn_S(requestComponent, this, data, result);
		return super.CanRequestSpawn_S(requestComponent, data, result);
	}

	override protected bool AssignEntity_S(SCR_SpawnRequestComponent requestComponent, IEntity entity, SCR_SpawnData data)
	{
		AICF_SquadRespawnAttempt attempt = AICF_Attempt(requestComponent, data);
		// После асинхронного preload повторяем identity, membership и occupancy.
		if (attempt && !AICF_SquadRespawnPolicy.IsCurrent(SCR_PlayerController.Cast(requestComponent.GetPlayerController()), attempt, entity))
			return false;
		return super.AssignEntity_S(requestComponent, entity, data);
	}
}

modded class SCR_PossessSpawnRequestComponent
{
	override protected void SendResponse_S(SCR_ESpawnResult response, SCR_SpawnData data)
	{
		SCR_PlayerController player = SCR_PlayerController.Cast(GetPlayerController());
		SCR_PossessSpawnData possess = SCR_PossessSpawnData.Cast(data);
		if (player && possess && player.AICF_GetSquadRespawnAttempt() && player.AICF_GetSquadRespawnAttempt().m_RplId == possess.GetRplId())
			player.AICF_FinishSquadRespawn(response);
		super.SendResponse_S(response, data);
	}
}
