// Диагностический native control: весь production Scripts graph AICF отсутствует.
// ROSTER_READY обозначает нулевой managed roster этого control, а не production gate.
modded class ArmaReforgerScripted
{
	protected void AICF_NativeControlHeader()
	{
		string flag;
		if (!System.GetCLIParam("aicfControlNoSave", flag)) return;
		SCR_MissionHeaderCampaign header = SCR_MissionHeaderCampaign.Cast(GetMissionHeader());
		if (header) header.m_eSaveTypes = 0;
	}
	override protected void OnWorldPostProcess(World world)
	{
		AICF_NativeControlHeader();
		super.OnWorldPostProcess(world);
	}
	override bool OnGameStart()
	{
		AICF_NativeControlHeader();
		return super.OnGameStart();
	}
}

modded class SCR_PlayerController
{
	void AICF_NativeVehicleControlDone() { Rpc(RpcAsk_AICFNativeVehicleControlDone); }
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_AICFNativeVehicleControlDone()
	{
		SCR_GameModeCampaign campaign = SCR_GameModeCampaign.Cast(GetGame().GetGameMode());
		string previewType;
		if (System.GetCLIParam("aicfControlPreview", previewType))
		{
			SharedItemRef previewWorld = BaseWorld.CreateWorld(previewType, "AICF_ControlVehicleWorld");
			ChimeraWorld world = previewWorld.GetRef();
			if (world && world != GetGame().GetWorld())
			{
				world.LoadSystems("{3A1DEDB2A5818B53}Configs/Systems/MapPreviewSystemsConfig.conf");
				SCR_CampaignFaction faction = SCR_CampaignFaction.Cast(GetGame().GetFactionManager().GetFactionByKey("USSR"));
				array<SCR_EntityCatalogEntry> entries = {};
				faction.GetFactionEntityCatalogOfType(EEntityCatalogType.VEHICLE).GetEntityList(entries);
				foreach (SCR_EntityCatalogEntry entry : entries)
				{
					if (!entry.GetPrefab().Contains("UAZ469_uncovered.et")) continue;
					IEntity model = GetGame().SpawnEntityPrefabLocal(Resource.Load(entry.GetPrefab()), world);
					Print("[AICF][NATIVE_VEHICLE_CONTROL_MODEL] valid=" + (model != null).ToString());
					if (model) SCR_EntityHelper.DeleteEntityAndChildren(model);
					break;
				}
			}
		}
		Print("[AICF][NATIVE_VEHICLE_CONTROL_SERVER] player=" + GetPlayerId().ToString());
		if (campaign) campaign.AICF_NativeVehicleControlScheduleClose();
	}
}

modded class SCR_GameModeCampaign
{
	protected int m_iAICFNativeVehicleControlAt;
	protected bool m_bAICFNativeVehicleControlReady;
	protected int m_iAICFNativeVehicleControlConnections;
	void AICF_NativeVehicleControlScheduleClose()
	{
		m_iAICFNativeVehicleControlConnections++;
		string reconnect;
		if (System.GetCLIParam("aicfControlJIP", reconnect) && m_iAICFNativeVehicleControlConnections < 2) return;
		GetGame().GetCallqueue().CallLater(AICF_NativeVehicleControlClose, 10000, false);
	}
	override void OnGameStart()
	{
		super.OnGameStart();
		m_iAICFNativeVehicleControlAt = System.GetTickCount();
		GetGame().GetCallqueue().CallLater(AICF_NativeVehicleControlTick, 1500, true);
	}
	protected void AICF_NativeVehicleControlTick()
	{
		if (Replication.IsServer())
		{
			if (m_bAICFNativeVehicleControlReady || !IsRunning()) return;
			m_bAICFNativeVehicleControlReady = true;
			string managed;
			if (!System.GetCLIParam("aicfControlManaged", managed)) Print("[AICF][STAGE1][INFO][ROSTER_READY] control=NATIVE_ONLY managed_groups=0");
			return;
		}
		SCR_PlayerController player = SCR_PlayerController.Cast(GetGame().GetPlayerController());
		string factionFlag;
		if (player && System.GetCLIParam("aicfControlFaction", factionFlag))
		{
			Faction faction = GetGame().GetFactionManager().GetFactionByKey("USSR");
			SCR_PlayerFactionAffiliationComponent affiliation = SCR_PlayerFactionAffiliationComponent.Cast(player.FindComponent(SCR_PlayerFactionAffiliationComponent));
			if (!affiliation || !faction) return;
			if (affiliation.GetAffiliatedFaction() != faction) { affiliation.RequestFaction(faction); return; }
		}
		if (!player || System.GetTickCount(m_iAICFNativeVehicleControlAt) < 10000) return;
		Print("[AICF][NATIVE_VEHICLE_CONTROL_CLIENT] connected=1");
		player.AICF_NativeVehicleControlDone();
		GetGame().GetCallqueue().Remove(AICF_NativeVehicleControlTick);
		GetGame().GetCallqueue().CallLater(AICF_NativeVehicleControlClose, 2000, false);
	}
	protected void AICF_NativeVehicleControlClose() { GetGame().RequestClose(); }
	override void OnGameEnd()
	{
		GetGame().GetCallqueue().Remove(AICF_NativeVehicleControlTick);
		GetGame().GetCallqueue().Remove(AICF_NativeVehicleControlClose);
		super.OnGameEnd();
	}
}
