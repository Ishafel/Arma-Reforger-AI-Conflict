// Dedicated direct-connect без backend identity не должен назначать NULL_UUID
// уже отслеживаемому controller. StopTracking(false) сохраняет native anonymous
// nonpersistent поведение, освобождая config штатным публичным boundary.
[BaseContainerProps(category: "Respawn")]
modded class SCR_SpawnLogic
{
	override void OnPlayerAuditSuccess_S(int playerId)
	{
		if (Replication.IsServer() && m_Persistence && playerId > 0 &&
			GetGame().GetBackendApi().GetPlayerIdentityId(playerId).IsEmpty())
		{
			SCR_PlayerController controller = SCR_PlayerController.Cast(GetGame().GetPlayerManager().GetPlayerController(playerId));
			bool stopped = false;
			if (controller) stopped = m_Persistence.StopTracking(controller, false);
			Print(string.Format("[AICF][PLAYER_PERSISTENCE_SESSION_IDENTITY] player=%1 backend_identity_empty=1 tracking_released=%2 persistent_data_removed=0", playerId, stopped));
			return;
		}
		super.OnPlayerAuditSuccess_S(playerId);
	}
}
