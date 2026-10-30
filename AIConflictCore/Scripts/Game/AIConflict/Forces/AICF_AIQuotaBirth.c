enum AICF_EAIQuotaBirthStatus
{
	VETO,
	IN_NATIVE_FACTORY,
	NATIVE_NULL,
	BOUND,
	ROLLED_BACK,
	QUARANTINED
}

// Только exact local фабрики. Не ambient permission для любого OnAgentAdded.
class AICF_AIQuotaBirth
{
	AICF_AIWorldQuotaService m_Service;
	ref AICF_AIQuotaGroupQueue m_Queue;
	ref AICF_AIQuotaDispatch m_Ticket;
	int m_iSlotIndex;
	ResourceName m_Prefab;
	AICF_EAIQuotaBirthStatus m_Status;
	IEntity m_Entity;
	ref AICF_AIQuotaBody m_Body;
	bool m_bFactoryObserved;
	bool m_bFinished;
	string m_sRejectReason;

	bool IsRequestCurrent()
	{
		if (!m_Service || !m_Queue || !m_Queue.IsCurrent() || !m_Ticket || m_iSlotIndex < 0 || m_Prefab.IsEmpty()) return false;
		SCR_AIGroup group = m_Queue.m_Group;
		return group.m_aUnitPrefabSlots && group.m_aUnitPrefabSlots.IsIndexValid(m_iSlotIndex) &&
			group.m_aUnitPrefabSlots[m_iSlotIndex] == m_Prefab;
	}

	static bool IsNonPlayerUnassigned(IEntity entity)
	{
		if (!Replication.IsServer() || !GetGame() || !entity) return false;
		ChimeraCharacter character = ChimeraCharacter.Cast(entity);
		if (!character || !character.GetAIControlComponent() || !character.GetCharacterController() ||
			character.GetCharacterController().IsPlayerControlled()) return false;
		PlayerManager players = GetGame().GetPlayerManager();
		if (!players || players.GetPlayerIdFromControlledEntity(entity) > 0 ||
			SCR_PossessingManagerComponent.GetPlayerIdFromMainEntity(entity) > 0) return false;
		CompartmentAccessComponent access = character.GetCompartmentAccessComponent();
		if (character.IsInVehicle() || CompartmentAccessComponent.GetVehicleIn(character) ||
			(access && (access.IsInCompartment() || access.GetCompartment() || access.IsGettingIn() || access.IsGettingOut() || access.IsSwitchingSeatsAnim())))
			return false;
		AIAgent agent = character.GetAIControlComponent().GetControlAIAgent();
		return !agent || (agent.GetControlledEntity() == character && !agent.GetParentGroup());
	}
}
