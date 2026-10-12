// Только отдельный stage. Сравнение с Find/List из fb6f3e1 в том же процессе.
// Это время каталога без UI/render, не замер плавности игрового интерфейса.
class AICF_LoadoutLegacyCatalog : AICF_LoadoutCatalog
{
	override SCR_EntityCatalogEntry Find(ResourceName prefab)
	{
		foreach (SCR_EntityCatalogEntry entry : m_aEntries)
		{
			if (entry && entry.IsEnabled() && entry.GetPrefab() == prefab &&
				entry.GetEntityDataOfType(SCR_ArsenalItem))
				return entry;
		}
		return null;
	}

	override void List(int category, array<string> prefabs, int mode = 0, string source = "")
	{
		prefabs.Clear();
		foreach (SCR_EntityCatalogEntry entry : m_aEntries)
		{
			if (!entry || !entry.IsEnabled() || !m_Profile.AllowsLoadoutItem(m_sStableFaction, entry) || IsDenied(entry.GetPrefab()) || prefabs.Contains(entry.GetPrefab()))
				continue;
			SCR_ArsenalItem item = SCR_ArsenalItem.Cast(entry.GetEntityDataOfType(SCR_ArsenalItem));
			if (item && (category == 0 || (item.GetItemType() & category) != 0) &&
				(mode == 0 || (item.GetItemMode() & mode) != 0) && (source.IsEmpty() || Source(entry.GetPrefab()) == source))
				prefabs.Insert(entry.GetPrefab());
		}
		prefabs.Sort();
	}
}

modded class AICF_MatchController
{
	protected bool m_bAICFPerformanceDone;
	protected int m_iAICFPerformanceFailures;
	protected int m_iAICFPerformanceChecks;

	protected void AICF_PerformanceCheck(string rule, bool passed)
	{
		m_iAICFPerformanceChecks++;
		if (!passed) m_iAICFPerformanceFailures++;
		Print(string.Format("[AICF][LOADOUT_PERF_CHECK] rule=%1 passed=%2", rule, passed));
	}

	override protected void Update()
	{
		super.Update();
		string enabled;
		if (!System.GetCLIParam("aicfLoadoutPerformanceProbe", enabled) || enabled != "1" || !m_bRosterReady || m_bAICFPerformanceDone)
			return;
		m_bAICFPerformanceDone = true;
		AICF_PerformanceFaction(m_USFaction);
		AICF_PerformanceFaction(m_USSRFaction);
		Print(string.Format("[AICF][LOADOUT_PERF_DONE] checks=%1 failures=%2", m_iAICFPerformanceChecks, m_iAICFPerformanceFailures));
		GetGame().RequestClose();
	}

	protected void AICF_PerformanceFaction(SCR_CampaignFaction faction)
	{
		AICF_LoadoutCatalog current = new AICF_LoadoutCatalog(faction);
		AICF_LoadoutLegacyCatalog legacy = new AICF_LoadoutLegacyCatalog(faction);
		array<SCR_EntityCatalogEntry> entries = {};
		faction.GetFactionEntityCatalogOfType(EEntityCatalogType.ITEM).GetEntityList(entries);
		int mismatches;
		foreach (SCR_EntityCatalogEntry entry : entries)
		{
			if (!entry) continue;
			ResourceName prefab = entry.GetPrefab();
			if (current.Find(prefab) != legacy.Find(prefab) || current.Name(prefab) != legacy.Name(prefab) ||
				current.Cost(prefab) != legacy.Cost(prefab) || current.ItemType(prefab) != legacy.ItemType(prefab) ||
				current.Allows(prefab) != legacy.Allows(prefab)) mismatches++;
		}
		AICF_PerformanceCheck("ENTRY_EQUIVALENCE_" + faction.GetFactionKey(), !entries.IsEmpty() && mismatches == 0);
		AICF_PerformanceCheck("UNKNOWN_REJECTED", !current.Find("{0000000000000000}missing.et") && !current.Allows("{0000000000000000}missing.et"));
		array<string> sources = {};
		AICF_ContentProfile.GetActive().GetLoadoutSources(sources);
		sources.Insert(string.Empty);
		array<int> categories = {0, SCR_EArsenalItemType.RIFLE, SCR_EArsenalItemType.TORSO, SCR_EArsenalItemType.WEAPON_ATTACHMENT};
		array<int> modes = {0, SCR_EArsenalItemMode.AMMUNITION};
		foreach (string source : sources)
		{
			foreach (int category : categories)
			{
				foreach (int mode : modes)
				{
					array<string> before = {}, after = {};
					legacy.List(category, before, mode, source);
					current.List(category, after, mode, source);
					bool same = before.Count() == after.Count();
					foreach (int index, string expected : before)
						if (!after.IsIndexValid(index) || after[index] != expected) same = false;
					AICF_PerformanceCheck(string.Format("LIST_%1_%2_%3_%4", faction.GetFactionKey(), source, category, mode), same);
				}
			}
		}
		// Обе реализации прогреты проверками; порядок замеров чередуется.
		for (int sample; sample < 6; sample++)
		{
			int oldMs, newMs, oldSum, newSum;
			if (Math.Mod(sample, 2) == 0)
			{
				oldMs = AICF_PerformanceMeasure(legacy, oldSum);
				newMs = AICF_PerformanceMeasure(current, newSum);
			}
			else
			{
				newMs = AICF_PerformanceMeasure(current, newSum);
				oldMs = AICF_PerformanceMeasure(legacy, oldSum);
			}
			AICF_PerformanceCheck("BENCHMARK_CHECKSUM", oldSum > 0 && oldSum == newSum);
			Print(string.Format("[AICF][LOADOUT_PERF_SAMPLE] faction=%1 sample=%2 rounds=5 entries=%3 before_ms=%4 after_ms=%5 checksum=%6",
				faction.GetFactionKey(), sample, entries.Count(), oldMs, newMs, newSum));
		}
	}

	protected int AICF_PerformanceMeasure(AICF_LoadoutCatalog catalog, out int checksum)
	{
		checksum = 0;
		int start = System.GetTickCount();
		for (int round; round < 5; round++)
		{
			array<string> items = {};
			catalog.List(0, items);
			foreach (string prefab : items)
				checksum += catalog.Name(prefab).Length();
		}
		return System.GetTickCount(start);
	}
}
