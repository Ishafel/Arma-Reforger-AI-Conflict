// Stage-only priority control. All production metadata/site/payment/worker gates run.
modded class AICF_AICommander
{
	override int SelectConstructionType(array<bool> covered, int startType)
	{
		if (!System.IsCLIParam("aicfSupplyDepotConstructionProbe")) return super.SelectConstructionType(covered, startType);
		if (!Replication.IsServer() || !IsEnabled() || !covered || covered.Count() != AICF_EConstructionType.COUNT) return -1;
		if (!covered[AICF_EConstructionType.SUPPLY_DEPOT]) return AICF_EConstructionType.SUPPLY_DEPOT;
		return -1;
	}
}

modded class AICF_ConstructionPlanner
{
	bool AICF_SupplyHumanVeto(SCR_CampaignFaction faction)
	{
		return !Commander(faction);
	}

	override protected AICF_ConstructionMetadata Metadata(AICF_ConstructionOrder order)
	{
		AICF_ConstructionMetadata result = super.Metadata(order);
		if (System.IsCLIParam("aicfSupplyDepotConstructionProbe") && result && order.m_eType == AICF_EConstructionType.SUPPLY_DEPOT)
			Print(string.Format("[AICF][SUPPLY_CONSTRUCTION_PROBE] event=METADATA faction=%1 valid=%2 prefab=%3 reason=%4", order.m_sFaction, result && result.m_bValid, AICF_ContentProfile.GetActive().GetConstructionPrefab(order.m_sFaction, order.m_eType), result.m_sInvalidReason));
		return result;
	}
}

modded class AICF_StockConstructionAdapter
{
	override bool Place(AICF_ConstructionOrder order, AICF_ConstructionConfig config, AICF_EconomySystem economy, SCR_CampaignBuildingManagerComponent manager, AICF_BaseBuilderService builders)
	{
		bool result = super.Place(order, config, economy, manager, builders);
		if (System.IsCLIParam("aicfSupplyDepotConstructionProbe") && result && order.m_eType == AICF_EConstructionType.SUPPLY_DEPOT)
			Print(string.Format("[AICF][SUPPLY_CONSTRUCTION_PROBE] event=PLACED faction=%1 accepted=%2 paid=%3 cost=%4 before=%5 after=%6 token=%7", order.m_sFaction, result, order.m_bPaid, order.m_iCost, order.m_fBefore, order.m_fAfter, order.m_sToken));
		return result;
	}
}

modded class AICF_MatchController
{
	protected int m_iSupplyProbeStarted;
	protected int m_iSupplyInitialCapacity;
	protected int m_iSupplyQuote;
	protected bool m_bSupplyVeto;
	protected bool m_bSupplyDone;

	override protected void Update()
	{
		super.Update();
		if (!System.IsCLIParam("aicfSupplyDepotConstructionProbe") || m_bStopped || m_bSupplyDone || !m_bRosterReady) return;
		if (System.IsCLIParam("aicfSupplyDepotRegistryOnly"))
		{
			m_bSupplyDone = true;
			bool us = AICF_SupplyRegistry(m_USFaction);
			bool ussr = AICF_SupplyRegistry(m_USSRFaction);
			Print(string.Format("[AICF][SUPPLY_CONSTRUCTION_PROBE] event=REGISTRY_DONE passed=%1 profile=%2 construction_required=0", us && ussr, m_ContentProfile.GetProfileKey()));
			GetGame().GetCallqueue().CallLater(AICF_SupplyProbeClose, 1000, false);
			return;
		}
		if (FileIO.FileExists("$profile:AICF_SupplyProbe.stop"))
		{
			m_bSupplyDone = true;
			Print("[AICF][SUPPLY_CONSTRUCTION_PROBE] event=EXTERNAL_STOP incomplete=1");
			GetGame().GetCallqueue().CallLater(AICF_SupplyProbeClose, 1000, false);
			return;
		}
		SCR_CampaignMilitaryBaseComponent base = m_USFaction.GetMainBase();
		if (!base || !base.IsInitialized() || !base.GetMasterProvider() || !m_Construction) return;
		int now = System.GetTickCount();
		if (!m_iSupplyProbeStarted)
		{
			m_iSupplyProbeStarted = now;
			m_iSupplyInitialCapacity = base.GetSuppliesMax();
			m_iSupplyQuote = m_USAICommander.GetMaximumVehiclePresetCost();
			m_bSupplyVeto = m_Construction.AICF_SupplyHumanVeto(m_USSRFaction);
			Print(string.Format("[AICF][SUPPLY_CONSTRUCTION_PROBE] event=START faction=US capacity=%1 quote=%2 human_veto=%3 priority_controlled=1 economy_seed=0 capacity_write=0", m_iSupplyInitialCapacity, m_iSupplyQuote, m_bSupplyVeto));
		}
		bool online;
		array<SCR_ServicePointComponent> services = {};
		base.GetServices(services);
		foreach (SCR_ServicePointComponent service : services)
			if (service && service.GetType() == SCR_EServicePointType.SUPPLY_DEPOT && service.GetServiceState() == SCR_EServicePointStatus.ONLINE) online = true;
		bool passed = m_bSupplyVeto && m_iSupplyQuote > m_iSupplyInitialCapacity && online && base.GetSuppliesMax() > m_iSupplyInitialCapacity && base.GetSuppliesMax() >= m_iSupplyQuote;
		if (!passed && now - m_iSupplyProbeStarted < 900000) return;
		m_bSupplyDone = true;
		Print(string.Format("[AICF][SUPPLY_CONSTRUCTION_PROBE] event=DONE passed=%1 initial_capacity=%2 capacity=%3 quote=%4 online=%5 human_veto=%6", passed, m_iSupplyInitialCapacity, base.GetSuppliesMax(), m_iSupplyQuote, online, m_bSupplyVeto));
		GetGame().GetCallqueue().CallLater(AICF_SupplyProbeClose, 5000, false);
	}

	void AICF_SupplyProbeClose()
	{
		GetGame().RequestClose();
	}

	protected bool AICF_SupplyRegistry(SCR_CampaignFaction faction)
	{
		FactionKey stable = m_ContentProfile.GetStableFactionKey(faction.GetFactionKey());
		ResourceName prefab = m_ContentProfile.GetConstructionPrefab(stable, AICF_EConstructionType.SUPPLY_DEPOT);
		SCR_CampaignBuildingManagerComponent manager = SCR_CampaignBuildingManagerComponent.Cast(m_Campaign.FindComponent(SCR_CampaignBuildingManagerComponent));
		AICF_ConstructionMetadata metadata = new AICF_ConstructionMetadata();
		metadata.Load(prefab, AICF_EConstructionType.SUPPLY_DEPOT, manager, faction);
		bool passed = !prefab.IsEmpty() && manager.GetCompositionId(prefab) >= 0 && metadata.m_bValid;
		Print(string.Format("[AICF][SUPPLY_CONSTRUCTION_PROBE] event=REGISTRY faction=%1 runtime=%2 passed=%3 prefab=%4 valid=%5 composition_id=%6 reason=%7", stable, faction.GetFactionKey(), passed, prefab, metadata.m_bValid, manager.GetCompositionId(prefab), metadata.m_sInvalidReason));
		metadata.ReleasePreview();
		return passed;
	}

	override protected void Stop(bool cleanupEntities)
	{
		GetGame().GetCallqueue().Remove(AICF_SupplyProbeClose);
		super.Stop(cleanupEntities);
	}
}
