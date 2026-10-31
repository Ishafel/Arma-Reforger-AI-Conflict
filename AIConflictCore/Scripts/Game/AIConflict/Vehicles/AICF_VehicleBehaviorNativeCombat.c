// Сохраняем native fireteam/dismount путь десанта, но решение о замене crew
// в настроенной боевой машине принимает flow только после локального боя.
modded class SCR_AIVehicleCombatActivity
{
	override float CustomEvaluate()
	{
		float nativePriority = super.CustomEvaluate();
		if (AICF_VehicleBehaviorPolicy.ShouldYieldNativeCombat(this)) return 0;
		return nativePriority;
	}

	override void VehicleUsageLogic(SCR_AIGroupVehicle vehicle)
	{
		if (!AICF_VehicleBehaviorPolicy.OwnsCombatActivity(this, vehicle))
		{
			super.VehicleUsageLogic(vehicle);
			return;
		}
		AICommunicationComponent comms = m_Utility.m_Owner.GetCommunicationComponent();
		SCR_AIVehicleUsageComponent usage = vehicle.GetVehicleUsageComponent();
		IEntity entity = vehicle.GetEntity();
		if (!comms || !usage || !entity) return;
		array<ref SCR_AIGroupFireteam> cargo = {};
		m_Utility.m_FireteamMgr.FindFireteamsOfVehicle(cargo, usage, SCR_AIGroupFireteamVehicleCargo);
		array<AIAgent> agents = {};
		int ordinal;
		foreach (SCR_AIGroupFireteam fireteam : cargo)
		{
			fireteam.GetMembers(agents);
			foreach (AIAgent agent : agents)
			{
				if (!agent) continue;
				SCR_AIMessageHandling.SendDismountMessage(agent, entity, ordinal, this, comms, ACTIVITY_NAME);
				fireteam.RemoveMember(agent);
				ordinal++;
			}
		}
	}

	override void OnAgentLifeStateChanged(AIAgent incapacitatedAgent, SCR_AIInfoComponent infoIncap, IEntity vehicle, ECharacterLifeState lifeState)
	{
		if (AICF_VehicleBehaviorPolicy.ShouldDeferCrewReplacement(Vehicle.Cast(vehicle))) return;
		super.OnAgentLifeStateChanged(incapacitatedAgent, infoIncap, vehicle, lifeState);
	}
}

// Native turret investigation сам создаёт GetOut, минуя goal-message reaction.
// Оставляем боевой экипаж на оружии; аварийные damage/fire выходы не меняются.
modded class SCR_AICombatComponent
{
	override bool DismountTurretCondition(inout vector targetPos, bool targetPosProvided, out float threatPriority)
	{
		if (AICF_VehicleBehaviorPolicy.KeepCombatTurret(m_Utility, m_CurrentVehicle))
		{
			threatPriority = 0;
			return false;
		}
		return super.DismountTurretCondition(targetPos, targetPosProvided, threatPriority);
	}
}

[BaseContainerProps()]
modded class SCR_AIGoalReaction_GetOutVehicle
{
	override void PerformReaction(notnull SCR_AIUtilityComponent utility, SCR_AIMessageBase message)
	{
		if (AICF_VehicleBehaviorPolicy.SuppressGetOut(utility, SCR_AIMessage_GetOut.Cast(message))) return;
		super.PerformReaction(utility, message);
	}
}
