// Только isolated runtime-source вместе с AICF_LogisticsRuntimeProbe.c.
// GUI не открывается. Используется тот же client intent/RPC, что кнопкой формы.
modded class SCR_GameModeCampaign
{
	protected int m_iAICFManualProbeStep;
	protected int m_iAICFManualProbeAttempts;
	protected bool m_bAICFManualProbeAccepted;
	protected string m_sAICFManualProbeLastStatus;

	override void OnGameStart()
	{
		super.OnGameStart();
		string enabled;
		if (Replication.IsServer() || !System.GetCLIParam("aicfManualSupplyProbe", enabled) || enabled != "1") return;
		string startStep;
		if (System.GetCLIParam("aicfManualSupplyProbeStartStep", startStep)) m_iAICFManualProbeStep = startStep.ToInt();
		GetGame().GetCallqueue().CallLater(AICF_ManualProbeTick, 20000, true);
		GetGame().GetCallqueue().CallLater(AICF_ManualProbeClose, 330000, false);
	}

	protected void AICF_ManualProbeTick()
	{
		SCR_PlayerController player = SCR_PlayerController.Cast(GetGame().GetPlayerController());
		SCR_CampaignFaction faction = SCR_CampaignFaction.Cast(SCR_FactionManager.SGetLocalPlayerFaction());
		if (!player) return;
		string stable;
		if (System.GetCLIParam("aicfLogisticsClientSpawnFaction", stable))
		{
			Faction desired = GetGame().GetFactionManager().GetFactionByKey(AICF_ContentProfile.GetActive().GetRuntimeFactionKey(stable));
			SCR_PlayerFactionAffiliationComponent affiliation = SCR_PlayerFactionAffiliationComponent.Cast(player.FindComponent(SCR_PlayerFactionAffiliationComponent));
			if (desired && affiliation && affiliation.GetAffiliatedFaction() != desired)
			{
				affiliation.RequestFaction(desired);
				Print("[AICF][MANUAL_PROBE_FACTION] test_only=1 requested=" + stable);
				return;
			}
		}
		if (!faction || !faction.GetMainBase()) return;
		string status = player.AICF_GetSupplyStatus();
		if (status != m_sAICFManualProbeLastStatus)
		{
			m_sAICFManualProbeLastStatus = status;
			Print(string.Format("[AICF][MANUAL_PROBE_RESPONSE] test_only=1 step=%1 busy=%2 status=%3", m_iAICFManualProbeStep, player.AICF_IsSupplyBusy(), status));
		}
		if (m_iAICFManualProbeStep >= 4 && player.AICF_IsSupplyBusy() && !status.Contains("Ожидается ответ")) m_bAICFManualProbeAccepted = true;
		if (m_bAICFManualProbeAccepted && !player.AICF_IsSupplyBusy())
		{
			AICF_ManualProbeClose();
			return;
		}
		if (player.AICF_IsSupplyBusy() || m_bAICFManualProbeAccepted) return;
		array<ref AICF_SupplyMapBase> bases = {};
		AICF_SupplyMapData.Collect(faction, bases);
		AICF_SupplyMapBase source;
		AICF_SupplyMapBase destination;
		float nearest = float.MAX;
		foreach (AICF_SupplyMapBase entry : bases)
		{
			if (entry.m_Base == faction.GetMainBase()) { destination = entry; continue; }
			int supplies;
			if (!AICF_SupplyMapData.ReadSupplies(entry, faction, supplies) || supplies < 100) continue;
			float distance = vector.DistanceXZ(entry.m_Base.GetOwner().GetOrigin(), faction.GetMainBase().GetOwner().GetOrigin());
			if (distance >= nearest) continue;
			nearest = distance;
			source = entry;
		}
		if (!source || !destination || !source.NetworkId().IsValid() || !destination.NetworkId().IsValid()) return;
		if (m_iAICFManualProbeStep == 0)
			player.AICF_RequestSupplyTransport(source.NetworkId(), source.NetworkId(), 100);
		else if (m_iAICFManualProbeStep == 1)
			player.AICF_RequestSupplyTransport(source.NetworkId(), destination.NetworkId(), -1);
		else if (m_iAICFManualProbeStep == 2)
		{
			array<SCR_MilitaryBaseComponent> all = {};
			SCR_MilitaryBaseSystem.GetInstance().GetBases(all);
			foreach (SCR_MilitaryBaseComponent candidate : all)
			{
				SCR_CampaignMilitaryBaseComponent enemy = SCR_CampaignMilitaryBaseComponent.Cast(candidate);
				if (!enemy || !enemy.GetFaction() || enemy.GetFaction() == faction || !enemy.GetOwner()) continue;
				AICF_SupplyMapBase target = new AICF_SupplyMapBase(enemy);
				if (!target.NetworkId().IsValid()) continue;
				player.AICF_RequestSupplyTransport(source.NetworkId(), target.NetworkId(), 100);
				break;
			}
		}
		else
		{
			if (++m_iAICFManualProbeAttempts > 8) return;
			player.AICF_RequestSupplyTransport(source.NetworkId(), destination.NetworkId(), 100);
			player.AICF_RequestSupplyTransport(source.NetworkId(), destination.NetworkId(), 100);
		}
		Print(string.Format("[AICF][MANUAL_PROBE_SENT] test_only=1 step=%1 source=%2 destination=%3 duplicate=%4", m_iAICFManualProbeStep, source.NetworkId(), destination.NetworkId(), m_iAICFManualProbeStep >= 3));
		m_iAICFManualProbeStep++;
	}

	protected void AICF_ManualProbeClose()
	{
		GetGame().GetCallqueue().Remove(AICF_ManualProbeTick);
		Print(string.Format("[AICF][MANUAL_PROBE_CLIENT_FINISHED] test_only=1 accepted=%1", m_bAICFManualProbeAccepted));
		GetGame().RequestClose();
	}

	void ~SCR_GameModeCampaign()
	{
		if (!GetGame()) return;
		GetGame().GetCallqueue().Remove(AICF_ManualProbeTick);
		GetGame().GetCallqueue().Remove(AICF_ManualProbeClose);
	}
}
