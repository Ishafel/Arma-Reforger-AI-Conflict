// Только runtime fixture: скопировать временно в Core/Construction, запустить
// canonical launcher с -aicfBuilderProbe 1, после остановки удалить копию.
// Создание layouts здесь намеренно обходит player placement/supply transaction.
// Fixture проверяет службу строителей, но не player UI и не оплату placement.
modded class AICF_BaseBuilderService
{
	protected bool m_bAICFProbePlaced;
	protected int m_iAICFProbeLastLog;
	protected int m_iAICFProbeStartedAt;
	protected int m_iAICFProbePhase;

	override void Update()
	{
		super.Update();
		string enabled;
		if (!System.GetCLIParam("aicfBuilderProbe", enabled) || (enabled != "1" && enabled != "2" && enabled != "3") || !m_Campaign)
			return;
		if (m_iAICFProbeStartedAt == 0)
			m_iAICFProbeStartedAt = System.GetTickCount();
		// В client probe проекты появляются после загрузки клиента/JIP.
		if (enabled == "3" && System.GetTickCount() - m_iAICFProbeStartedAt < 45000)
			return;
		if (!m_bAICFProbePlaced)
		{
			m_bAICFProbePlaced = true;
			AICF_ProbePlace(m_Campaign.GetFactionByEnum(SCR_ECampaignFaction.BLUFOR), 2);
			AICF_ProbePlace(m_Campaign.GetFactionByEnum(SCR_ECampaignFaction.OPFOR), 1);
		}
		int now = System.GetTickCount();
		if (now - m_iAICFProbeStartedAt >= 300000 || m_iAICFProbePhase == 3)
		{
			Print("[AICF][BUILDER_PROBE] finished=1");
			GetGame().RequestClose();
			return;
		}
		if (enabled == "2")
			AICF_ProbeLifecycle();
		if (enabled == "3")
		{
			foreach (AICF_BaseBuilder marked : m_aBuilders)
			{
				if (!marked.m_Character)
					continue;
				SCR_CharacterControllerComponent controller = SCR_CharacterControllerComponent.Cast(marked.m_Character.FindComponent(SCR_CharacterControllerComponent));
				if (controller)
					controller.AICF_ProbeMarkBuilder(marked.m_iSlotId + 1);
			}
		}
		if (now - m_iAICFProbeLastLog < 10000)
			return;
		m_iAICFProbeLastLog = now;
		foreach (AICF_BaseBuilder builder : m_aBuilders)
		{
			if (!builder.m_Group)
				continue;
			vector position;
			if (builder.m_Character)
				position = builder.m_Character.GetOrigin();
			Log(builder, "BUILDER_PROBE", string.Format("agents=%1 position=%2 work=%3 destination=%4 returning=%5",
				builder.m_Group.GetAgentsCount(), position, builder.m_vWorkPosition, builder.m_vDestination, builder.m_bReturning));
		}
	}

	protected void AICF_ProbeLifecycle()
	{
		SCR_CampaignFaction faction = m_Campaign.GetFactionByEnum(SCR_ECampaignFaction.BLUFOR);
		if (!faction || !faction.GetMainBase())
			return;
		AICF_BaseBuilder builder = FindBuilder(faction.GetMainBase());
		if (!builder || !builder.m_Character)
			return;
		if (m_iAICFProbePhase == 0 && builder.m_iIdleAtMs > 0)
		{
			Log(builder, "BUILDER_PROBE_REUSE");
			AICF_ProbePlace(faction, 1, 30);
			m_iAICFProbePhase = 1;
		}
		else if (m_iAICFProbePhase == 1 && builder.m_Target && builder.m_iWorkAtMs > 0)
		{
			Log(builder, "BUILDER_PROBE_KILL");
			SCR_CharacterDamageManagerComponent damage = SCR_CharacterDamageManagerComponent.Cast(builder.m_Character.FindComponent(SCR_CharacterDamageManagerComponent));
			if (damage)
				damage.Kill(Instigator.CreateInstigatorGM());
			m_iAICFProbePhase = 2;
		}
		else if (m_iAICFProbePhase == 2 && builder.m_iGeneration >= 2 && builder.m_iIdleAtMs > 0)
		{
			Log(builder, "BUILDER_PROBE_OWNER_CHANGE");
			builder.m_Base.SetFaction(m_Campaign.GetFactionByEnum(SCR_ECampaignFaction.OPFOR));
			m_iAICFProbePhase = 3;
		}
	}

	protected void AICF_ProbePlace(SCR_CampaignFaction faction, int count, float offset = 0)
	{
		if (!faction || !faction.GetMainBase())
			return;
		SCR_CampaignMilitaryBaseComponent base = faction.GetMainBase();
		SCR_CampaignBuildingProviderComponent provider = base.GetMasterProvider();
		if (!provider || !base.GetSpawnPoint())
		{
			Print("[AICF][BUILDER_PROBE] missing provider/catalog/spawn", LogLevel.ERROR);
			return;
		}
		array<ResourceName> entries = m_BuildingManager.GetPlaceablePrefabs();
		ResourceName selected;
		foreach (ResourceName prefab : entries)
		{
			if (prefab.Contains("Sandbag"))
			{
				selected = prefab;
				break;
			}
		}
		if (selected.IsEmpty())
		{
			foreach (ResourceName entry : entries)
			{
				Print("[AICF][BUILDER_PROBE] candidate=" + entry);
			}
			return;
		}
		vector spawn, rotation;
		base.GetSpawnPoint().GetPositionAndRotation(spawn, rotation);
		for (int i = 0; i < count; i++)
		{
			EntitySpawnParams params = new EntitySpawnParams();
			params.TransformMode = ETransformMode.WORLD;
			params.Transform[3] = spawn + Vector(15 + offset + i * 15, 0, 15);
			params.Transform[3][1] = GetGame().GetWorld().GetSurfaceY(params.Transform[3][0], params.Transform[3][2]);
			SCR_EditorLinkComponent.IgnoreSpawning(true);
			IEntity entity = GetGame().SpawnEntityPrefabEx(selected, false, params: params);
			SCR_EditorLinkComponent.IgnoreSpawning(false);
			if (!entity)
				continue;
			FactionAffiliationComponent affiliation = FactionAffiliationComponent.Cast(entity.FindComponent(FactionAffiliationComponent));
			if (affiliation)
				affiliation.SetAffiliatedFaction(faction);
			SCR_CampaignBuildingCompositionComponent composition = SCR_CampaignBuildingCompositionComponent.Cast(entity.FindComponent(SCR_CampaignBuildingCompositionComponent));
			if (!composition)
				continue;
			composition.SetProviderEntity(provider.GetOwner());
			m_aPlaced.Insert(composition);
			Print(string.Format("[AICF][BUILDER_PROBE] placed=%1 layout=%2 faction=%3 base=%4 valid=%5 position=%6 provider_radius=%7",
				selected, composition.GetCompositionLayout(), SCR_Faction.GetEntityFaction(entity),
				AICF_Stage1Diagnostics.BaseKey(base), IsTargetValid(FindBuilder(base), composition), entity.GetOrigin(), provider.GetBuildingRadius()));
		}
	}
}

// Только fixture: JIP-свойство отмечает конкретный replicated character.
// Клиент читает собственные transform, hand attachment и native item-use state.
modded class SCR_CharacterControllerComponent
{
	[RplProp(onRplName: "AICF_ProbeBuilderReplicated")]
	protected int m_iAICFProbeBuilderSlot;
	protected int m_iAICFProbeSamples;

	void AICF_ProbeMarkBuilder(int slot)
	{
		if (!Replication.IsServer() || m_iAICFProbeBuilderSlot == slot)
			return;
		m_iAICFProbeBuilderSlot = slot;
		Replication.BumpMe();
	}

	protected void AICF_ProbeBuilderReplicated()
	{
		if (Replication.IsServer() || !m_iAICFProbeBuilderSlot)
			return;
		GetGame().GetCallqueue().Remove(AICF_ProbeClientSample);
		GetGame().GetCallqueue().CallLater(AICF_ProbeClientSample, 1000, true);
	}

	protected void AICF_ProbeClientSample()
	{
		IEntity entity = GetOwner();
		if (!entity || m_iAICFProbeSamples++ >= 300)
		{
			GetGame().GetCallqueue().Remove(AICF_ProbeClientSample);
			return;
		}
		RplComponent rpl = RplComponent.Cast(entity.FindComponent(RplComponent));
		SCR_GadgetManagerComponent gadgets = SCR_GadgetManagerComponent.GetGadgetManager(entity);
		IEntity tool;
		if (gadgets)
			tool = gadgets.GetGadgetByType(EGadgetType.BUILDING_TOOL);
		Print(string.Format("[AICF][BUILDER_CLIENT_PROBE] slot=%1 rpl=%2 position=%3 using=%4 tool_attached=%5 proxy=%6",
			m_iAICFProbeBuilderSlot - 1, rpl.Id(), entity.GetOrigin(), IsUsingItem(),
			tool && GetAttachedGadgetAtLeftHandSlot() == tool, rpl.IsProxy()));
	}

	void ~SCR_CharacterControllerComponent()
	{
		if (GetGame())
			GetGame().GetCallqueue().Remove(AICF_ProbeClientSample);
	}
}
