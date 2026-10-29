// Только isolated stage. По умолчанию read-only trace; отдельный fixture flag
// включает A/B guard пустой identity перед штатным SetId. Production CLI его не читает.
[BaseContainerProps(category: "Respawn")]
modded class SCR_SpawnLogic
{
	protected void AICF_TracePersistencePlayer(string boundary, int playerId)
	{
		string flag;
		if (!System.GetCLIParam("aicfPersistenceReconnectTrace", flag) || !Replication.IsServer())
			return;

		PlayerManager manager = GetGame().GetPlayerManager();
		SCR_PlayerController controller;
		if (manager)
			controller = SCR_PlayerController.Cast(manager.GetPlayerController(playerId));
		bool tracked;
		if (m_Persistence && controller)
			tracked = m_Persistence.IsTracked(controller);
		Print(string.Format("[AICF][PERSISTENCE_PLAYER_BOUNDARY] boundary=%1 player=%2 controller=%3 persistence=%4 tracked=%5 disconnect_policy=%6 collection=%7", boundary, playerId, controller, m_Persistence != null, tracked, m_eDisconnectPlayerControllerBehaviour, m_PlayerCollection != null));
		Print(string.Format("[AICF][PERSISTENCE_PLAYER_IDENTITY] boundary=%1 player=%2 backend_identity_empty=%3", boundary, playerId, GetGame().GetBackendApi().GetPlayerIdentityId(playerId).IsEmpty()));
	}

	override void OnPlayerAuditSuccess_S(int playerId)
	{
		AICF_TracePersistencePlayer("AUDIT_BEFORE", playerId);
		string guard;
		if (System.GetCLIParam("aicfPersistenceGuardNullIdentity", guard) && Replication.IsServer() &&
			GetGame().GetBackendApi().GetPlayerIdentityId(playerId).IsEmpty())
		{
			string stopTracking;
			if (System.GetCLIParam("aicfPersistenceStopNullTracking", stopTracking) && m_Persistence)
			{
				SCR_PlayerController controller = SCR_PlayerController.Cast(GetGame().GetPlayerManager().GetPlayerController(playerId));
				if (controller) m_Persistence.StopTracking(controller, false);
				AICF_TracePersistencePlayer("AUDIT_STOP_NULL_TRACKING", playerId);
				return;
			}
			AICF_TracePersistencePlayer("AUDIT_SKIP_NULL_IDENTITY", playerId);
			return;
		}
		super.OnPlayerAuditSuccess_S(playerId);
		AICF_TracePersistencePlayer("AUDIT_AFTER", playerId);
	}

	override void OnPlayerDisconnected_S(int playerId, KickCauseCode cause, int timeout)
	{
		AICF_TracePersistencePlayer("DISCONNECT_BEFORE", playerId);
		super.OnPlayerDisconnected_S(playerId, cause, timeout);
		AICF_TracePersistencePlayer("DISCONNECT_AFTER", playerId);
	}
}

modded class SCR_PlayerControllerSerializer
{
	override protected ESerializeResult Serialize(notnull IEntity entity, notnull SaveContext context)
	{
		ESerializeResult result = super.Serialize(entity, context);
		string flag;
		if (System.GetCLIParam("aicfPersistenceReconnectTrace", flag))
			Print(string.Format("[AICF][PERSISTENCE_CONTROLLER_SERIALIZE] serializer=%1 entity=%2 result=%3", this, entity, result));
		return result;
	}
}
