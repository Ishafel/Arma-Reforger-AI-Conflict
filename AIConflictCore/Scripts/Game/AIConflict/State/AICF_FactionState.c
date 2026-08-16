// Server-owned model for one faction. Slot order is stable for the lifetime of a match.
class AICF_FactionState
{
	protected FactionKey m_sFactionKey;
	protected ref array<ref AICF_GroupSlot> m_aGroupSlots = {};
	protected ref AICF_TicketLedger m_TicketLedger;

	void AICF_FactionState(FactionKey factionKey, AICF_Stage1Config config)
	{
		m_sFactionKey = factionKey;

		AICF_Stage1Config resolvedConfig = config;
		if (!resolvedConfig)
			resolvedConfig = new AICF_Stage1Config();

		m_TicketLedger = new AICF_TicketLedger(
			resolvedConfig.GetInitialTickets(),
			resolvedConfig.GetReplacementTicketCost());

		BuildDefaultSlots(resolvedConfig.GetActiveForcesRolesEnabled());
	}

	FactionKey GetFactionKey()
	{
		return m_sFactionKey;
	}

	int GetSlotCount()
	{
		return m_aGroupSlots.Count();
	}

	AICF_GroupSlot GetSlot(int slotId)
	{
		if (slotId < 0 || slotId >= m_aGroupSlots.Count())
			return null;

		return m_aGroupSlots[slotId];
	}

	AICF_GroupSlot FindSlotByGroup(SCR_AIGroup group)
	{
		if (!group)
			return null;

		foreach (AICF_GroupSlot slot : m_aGroupSlots)
		{
			if (slot.GetGroup() == group)
				return slot;
		}

		return null;
	}

	int CountSlotsByRole(AICF_EGroupRole role)
	{
		int count;
		foreach (AICF_GroupSlot slot : m_aGroupSlots)
		{
			if (slot.GetRole() == role)
				count++;
		}

		return count;
	}

	int CountSlotsByState(AICF_EGroupSlotState state)
	{
		int count;
		foreach (AICF_GroupSlot slot : m_aGroupSlots)
		{
			if (slot.GetState() == state)
				count++;
		}

		return count;
	}

	bool ApplyCommanderConfiguration(
		int slotId,
		AICF_EGroupRole role,
		AICF_EGroupUnitType unitType,
		int desiredSize)
	{
		AICF_GroupSlot slot = GetSlot(slotId);
		if (!slot || role < AICF_EGroupRole.ATTACK || role > AICF_EGroupRole.RESERVE ||
			unitType < AICF_EGroupUnitType.INFANTRY ||
			unitType > AICF_EGroupUnitType.MOTORIZED_ARMED_LIGHT ||
			desiredSize < AICF_Stage1Config.MIN_GROUP_SIZE ||
			desiredSize > AICF_Stage1Config.MAX_GROUP_SIZE ||
			(unitType == AICF_EGroupUnitType.MOTORIZED_ARMED_LIGHT &&
				(desiredSize < 2 || desiredSize > 4)))
		{
			return false;
		}

		slot.SetRoleAndIndex(role, 0);
		slot.SetUnitType(unitType);
		slot.SetDesiredSize(desiredSize);
		ReindexRoles();
		return true;
	}

	bool HasCombatReadyGroups()
	{
		foreach (AICF_GroupSlot slot : m_aGroupSlots)
		{
			if (slot.IsCombatReady())
				return true;
		}

		return false;
	}

	int GetTickets()
	{
		return m_TicketLedger.GetTickets();
	}

	int GetSpentTickets()
	{
		return m_TicketLedger.GetSpentTickets();
	}

	int GetReservedTickets()
	{
		return m_TicketLedger.GetReservedTickets();
	}

	bool CanAffordDeployment(AICF_EDeploymentKind deploymentKind)
	{
		return m_TicketLedger.CanAffordDeployment(deploymentKind);
	}

	bool TryReserveDeployment(AICF_EDeploymentKind deploymentKind)
	{
		return m_TicketLedger.TryReserveDeployment(deploymentKind);
	}

	void ReleaseDeploymentReservation(AICF_EDeploymentKind deploymentKind)
	{
		m_TicketLedger.ReleaseDeploymentReservation(deploymentKind);
	}

	bool TryCommitDeployment(AICF_EDeploymentKind deploymentKind)
	{
		return m_TicketLedger.TryCommitDeployment(deploymentKind);
	}

	int GetReplacementTicketCost()
	{
		return m_TicketLedger.GetReplacementTicketCost();
	}

	bool RollbackCommittedDeployment(AICF_EDeploymentKind deploymentKind)
	{
		return m_TicketLedger.RollbackCommittedDeployment(deploymentKind);
	}

	protected void BuildDefaultSlots(bool activeForcesRolesEnabled)
	{
		m_aGroupSlots.Clear();
		int attackSlots = AICF_Stage1Config.ATTACK_SLOTS_PER_FACTION;
		int defendSlots = AICF_Stage1Config.DEFEND_SLOTS_PER_FACTION;
		if (!activeForcesRolesEnabled)
		{
			attackSlots = AICF_Stage1Config.LEGACY_ATTACK_SLOTS_PER_FACTION;
			defendSlots = AICF_Stage1Config.LEGACY_DEFEND_SLOTS_PER_FACTION;
		}

		int attackIndex;
		int defendIndex;
		int reserveIndex;
		for (int slotId = 0; slotId < AICF_Stage1Config.GROUP_SLOTS_PER_FACTION; slotId++)
		{
			AICF_EGroupRole role = AICF_EGroupRole.RESERVE;
			int roleIndex;
			if (slotId < attackSlots)
			{
				role = AICF_EGroupRole.ATTACK;
				roleIndex = attackIndex++;
			}
			else if (slotId < attackSlots + defendSlots)
			{
				role = AICF_EGroupRole.DEFEND;
				roleIndex = defendIndex++;
			}
			else
			{
				roleIndex = reserveIndex++;
			}

			m_aGroupSlots.Insert(new AICF_GroupSlot(slotId, role, roleIndex));
		}
	}

	protected void ReindexRoles()
	{
		int attackIndex;
		int defendIndex;
		int reserveIndex;
		foreach (AICF_GroupSlot slot : m_aGroupSlots)
		{
			if (!slot)
				continue;
			switch (slot.GetRole())
			{
				case AICF_EGroupRole.ATTACK:
					slot.SetRoleAndIndex(AICF_EGroupRole.ATTACK, attackIndex++);
					break;
				case AICF_EGroupRole.DEFEND:
					slot.SetRoleAndIndex(AICF_EGroupRole.DEFEND, defendIndex++);
					break;
				case AICF_EGroupRole.RESERVE:
					slot.SetRoleAndIndex(AICF_EGroupRole.RESERVE, reserveIndex++);
					break;
			}
		}
	}
}
