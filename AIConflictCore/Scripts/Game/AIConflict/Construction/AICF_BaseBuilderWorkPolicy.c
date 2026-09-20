// Приоритет действует только на живого authoritative работника службы.
// Stock Evaluate складывает CustomEvaluate и EvaluatePriorityLevel: отрицательный
// итог не позволяет бою, укрытию, бегству или самолечению вытеснить работу.
// Не меняем очереди/состояние stock actions и не отключаем движение по navmesh.
modded class SCR_AIActionBase
{
	override float EvaluatePriorityLevel()
	{
		SCR_AIBehaviorBase behavior = SCR_AIBehaviorBase.Cast(this);
		if (behavior && behavior.m_Utility &&
			GetCause() >= SCR_EAIBehaviorCause.DANGER_LOW &&
			AICF_BaseBuilderService.IsDedicatedWorker(behavior.m_Utility.m_OwnerEntity))
			return -1000000;
		return super.EvaluatePriorityLevel();
	}
}
