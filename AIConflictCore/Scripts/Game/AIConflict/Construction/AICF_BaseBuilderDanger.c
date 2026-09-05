// Источник события сохранён явно: исключение действует только для работника
// этого layout, с той же generation и снаружи его footprint. Остальные AI
// продолжают освобождать площадь по штатной реакции, включая чужие события.
class AICF_BaseBuilderDangerEvent : SCR_AIDangerEvent_UnsafeArea
{
	AICF_BaseBuilder m_Builder;
	SCR_CampaignBuildingLayoutComponent m_Layout;
	EntityID m_CharacterId;
	int m_iGeneration;

	bool IsSafeWorker(IEntity character)
	{
		return character && m_Builder && m_Builder.m_Character == character && character.GetID() == m_CharacterId &&
			m_Builder.m_iGeneration == m_iGeneration && m_Layout &&
			AICF_BaseBuilderService.FindWorkingOnLayout(m_Layout) == m_Builder;
	}
}

modded class SCR_CampaignBuildingLayoutComponent
{
	override protected void CreateUnsafeAreaEvent()
	{
		AICF_BaseBuilder builder = AICF_BaseBuilderService.FindWorkingOnLayout(this);
		if (!builder)
		{
			super.CreateUnsafeAreaEvent();
			return;
		}
		vector mins, maxs;
		GetOwner().GetWorldBounds(mins, maxs);
		AICF_BaseBuilderDangerEvent danger = new AICF_BaseBuilderDangerEvent();
		danger.m_Builder = builder;
		danger.m_Layout = this;
		danger.m_CharacterId = builder.m_CharacterId;
		danger.m_iGeneration = builder.m_iGeneration;
		danger.SetDangerType(EAIDangerEventType.Danger_UnsafeArea);
		danger.SetPosition(GetOwner().GetOrigin());
		danger.SetRadius(vector.DistanceXZ(mins, maxs) * 1.25);
		GetGame().GetAIWorld().RequestBroadcastDangerEvent(danger);
	}
}

modded class SCR_AIDangerReaction_UnsafeArea
{
	override bool PerformReaction(notnull SCR_AIUtilityComponent utility, notnull SCR_AIThreatSystem threatSystem, AIDangerEvent dangerEvent, int dangerEventCount)
	{
		AICF_BaseBuilderDangerEvent builderEvent = AICF_BaseBuilderDangerEvent.Cast(dangerEvent);
		if (builderEvent && builderEvent.IsSafeWorker(utility.m_OwnerEntity))
			return false;
		return super.PerformReaction(utility, threatSystem, dangerEvent, dangerEventCount);
	}
}
