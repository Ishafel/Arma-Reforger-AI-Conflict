// Только отдельный stage: проверка реальных merged catalogs без клиента.
modded class AICF_MatchController
{
	protected int m_iAICFWCSProbeTicks;
	protected int m_iAICFWCSProbeFailures;

	override protected void Update()
	{
		super.Update();
		string enabled;
		if (!System.GetCLIParam("aicfWCSProbe", enabled) || enabled != "1" || !m_bRosterReady) return;
		m_iAICFWCSProbeTicks++;
		if (m_iAICFWCSProbeTicks == 2)
		{
			array<SCR_CampaignFaction> factions = {m_USFaction, m_USSRFaction};
			foreach (SCR_CampaignFaction faction : factions)
			{
				array<ResourceName> original = AICF_WCSProbeBaseline.s_US;
				if (faction.GetFactionKey() == "RHS_AFRF") original = AICF_WCSProbeBaseline.s_USSR;
				int preservedErrors;
				int added;
				int spawnable;
				int ifv;
				array<SCR_EntityCatalogEntry> entries = {};
				faction.GetFactionEntityCatalogOfType(EEntityCatalogType.VEHICLE).GetEntityList(entries);
				if (original.IsEmpty() || entries.Count() < original.Count()) preservedErrors++;
				foreach (int index, ResourceName expected : original)
				{
					if (!entries.IsIndexValid(index) || entries[index].GetPrefab() != expected || entries[index].GetCatalogIndex() != index) preservedErrors++;
				}
				m_iAICFWCSProbeFailures += preservedErrors;
				Print(string.Format("[AICF][WCS_PROBE_PRESERVED] faction=%1 before=%2 errors=%3", faction.GetFactionKey(), original.Count(), preservedErrors));
				array<ResourceName> unique = {};
				foreach (SCR_EntityCatalogEntry entry : entries)
				{
					if (unique.Contains(entry.GetPrefab())) m_iAICFWCSProbeFailures++;
					unique.Insert(entry.GetPrefab());
					if (!AICF_WCSVehicleCatalog.IsSupported(entry.GetPrefab(), faction.GetFactionKey())) continue;
					added++;
					Resource prefab = Resource.Load(entry.GetPrefab());
					if (!prefab || !prefab.IsValid()) m_iAICFWCSProbeFailures++;
					SCR_EntityCatalogSpawnerData data = SCR_EntityCatalogSpawnerData.Cast(entry.GetEntityDataOfType(SCR_EntityCatalogSpawnerData));
					int cost;
					int slots;
					if (data)
					{
						cost = data.GetSupplyCost();
						slots = data.GetValidSlotSizes();
						if (cost <= 0 || slots == 0) m_iAICFWCSProbeFailures++;
						spawnable++;
						if (entry.GetPrefab().Contains("/Tracked/BMP3/") || entry.GetPrefab().Contains("/Tracked/M2A2/")) ifv++;
					}
					Print(string.Format("[AICF][WCS_PROBE_VEHICLE] faction=%1 index=%2 cost=%3 slots=%4 prefab=%5", faction.GetFactionKey(), entry.GetCatalogIndex(), cost, slots, entry.GetPrefab()));
				}
				if (added == 0 || spawnable == 0 || ifv == 0) m_iAICFWCSProbeFailures++;
				Print(string.Format("[AICF][WCS_PROBE_IFV] faction=%1 spawnable=%2", faction.GetFactionKey(), ifv));
				Print(string.Format("[AICF][WCS_PROBE_FACTION] faction=%1 total=%2 added=%3 spawnable=%4", faction.GetFactionKey(), entries.Count(), added, spawnable));
			}
			Print(string.Format("[AICF][WCS_PROBE_RESULT] failures=%1", m_iAICFWCSProbeFailures));
		}
		string infantry;
		if (m_iAICFWCSProbeTicks >= 4 && !System.GetCLIParam("aicfWCSInfantryProbe", infantry)) GetGame().RequestClose();
	}
}

class AICF_WCSProbeBaseline
{
	static ref array<ResourceName> s_US = {};
	static ref array<ResourceName> s_USSR = {};
}

modded class AICF_WCSVehicleCatalog
{
	override int Build(FactionKey faction, array<ref SCR_EntityCatalog> originals)
	{
		array<SCR_EntityCatalogEntry> before = {};
		foreach (SCR_EntityCatalog original : originals)
		{
			if (original.GetCatalogType() == EEntityCatalogType.VEHICLE)
				Entries(original, before);
		}
		array<ResourceName> names = AICF_WCSProbeBaseline.s_US;
		if (faction == "RHS_AFRF") names = AICF_WCSProbeBaseline.s_USSR;
		names.Clear();
		foreach (SCR_EntityCatalogEntry entry : before)
		{
			if (entry.IsEnabled()) names.Insert(entry.GetPrefab());
		}
		return super.Build(faction, originals);
	}
}
