// Mount BT передаёт controller agent, а sound handler ждёт callsign персонажа.
// Сохраняем адресата и саму реплику: меняется только представление той же цели.
modded class SCR_AICommsHandler
{
	override void AddRequest(SCR_AITalkRequest request)
	{
		if (Replication.IsServer() && request && request.m_eCommType == ECommunicationType.REPORT_MOUNT_AS)
		{
			AIAgent agent = AIAgent.Cast(request.m_Entity);
			if (agent)
			{
				IEntity character = agent.GetControlledEntity();
				if (character && character.FindComponent(SCR_CallsignCharacterComponent))
					request.m_Entity = character;
			}
		}
		super.AddRequest(request);
	}
}
