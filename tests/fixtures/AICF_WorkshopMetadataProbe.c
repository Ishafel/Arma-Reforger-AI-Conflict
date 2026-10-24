// Test-only vanilla-dependent addon. Core and its scenario filters must stay unloaded.
class AICF_WorkshopMetadataProbe
{
	protected ref array<string> m_aOwners = {"9178E5822AFE48EA", "B52C5F6AEDBF423E", "A4B2E62595F645A4", "9F88011DA22B471C", "FA9FDCCA428A43BA", "A1CF260928100001", "A1CF261006100001"};
	protected int m_iChecks;
	protected int m_iFailed;
	protected int m_iAttempts;
	protected bool m_bEnabledItems;
	void Start()
	{
		WorkshopApi workshop = GetGame().GetBackendApi().GetWorkshop();
		if (workshop && workshop.NeedAddonsScan()) workshop.ScanOfflineItems();
		GetGame().GetCallqueue().CallLater(Read, 3000, false);
		GetGame().GetCallqueue().CallLater(Timeout, 90000, false);
	}
	protected void Check(string name, bool passed)
	{
		m_iChecks++;
		if (!passed) m_iFailed++;
		Print(string.Format("[AICF][METADATA_CASE] case=%1 pass=%2", name, passed));
	}
	protected void Read()
	{
		WorkshopApi workshop = GetGame().GetBackendApi().GetWorkshop();
		if (workshop && workshop.NeedAddonsScan() && m_iAttempts++ < 120)
		{
			GetGame().GetCallqueue().CallLater(Read, 500, false);
			return;
		}
		if (workshop && !m_bEnabledItems)
		{
			m_bEnabledItems = true;
			foreach (string addon : m_aOwners)
			{
				WorkshopItem item = workshop.FindItem(addon);
				if (!item) continue;
				item.Enable(true);
				Revision revision = item.GetActiveRevision();
				Print(string.Format("[AICF][METADATA_ADDON] id=%1 enabled=%2 revision=%3 downloaded=%4", addon, item.IsEnabled(), revision != null, revision && revision.IsDownloaded()));
			}
			GetGame().GetCallqueue().CallLater(Read, 1000, false);
			return;
		}
		array<string> loaded = {};
		GameProject.GetLoadedAddons(loaded);
		bool unloaded = true;
		foreach (string owner : m_aOwners)
			if (loaded.Find(owner) >= 0) unloaded = false;
		Check("ADDONS_UNLOADED", unloaded);
		Check("FRONTEND", GameStateTransitions.IsInsideMainMenu());
		Check("SCAN_READY", workshop && !workshop.NeedAddonsScan());
		array<MissionWorkshopItem> items = {};
		if (workshop) workshop.GetPageScenarios(items, 0, 1000);
		array<WorkshopItem> offline = {};
		if (workshop) workshop.GetOfflineItems(offline);
		Print(string.Format("[AICF][METADATA_SCAN] scenarios=%1 offline=%2", items.Count(), offline.Count()));
		int ownCount, setupCount, legacyCount;
		bool literal, stock;
		foreach (MissionWorkshopItem item : items)
		{
			Print(string.Format("[AICF][METADATA_RAW] owner=%1 target=%2", item.GetOwnerId(), item.Id()));
			if (item.Id() == "{C41618FD18E9D714}Missions/23_Campaign_Arland.conf") stock = true;
			if (m_aOwners.Find(item.GetOwnerId()) < 0) continue;
			ownCount++;
			Print(string.Format("[AICF][METADATA_ITEM] owner=%1 target=%2", item.GetOwnerId(), item.Id()));
			if (item.Id() == "{A1CF261019500001}Missions/AICF_Conflict_Arland_Setup.conf")
			{
				setupCount++;
				literal = item.Name() == "AI Conflict";
			}
			else legacyCount++;
		}
		Check("ONE_ENTRY", ownCount == 1);
		Check("SETUP_ONCE", setupCount == 1);
		Check("NO_LEGACY", legacyCount == 0);
		Check("LITERAL_NAME", literal);
		Check("STOCK_PRESERVED", stock);
		Print(string.Format("[AICF][METADATA_DONE] checks=%1 failed=%2 own=%3 setup=%4 legacy=%5", m_iChecks, m_iFailed, ownCount, setupCount, legacyCount));
		GetGame().GetCallqueue().CallLater(Close, 1000, false);
	}
	protected void Timeout() { Print("[AICF][METADATA_TIMEOUT]", LogLevel.ERROR); Close(); }
	protected void Close() { GetGame().RequestClose(); }
	void Stop()
	{
		GetGame().GetCallqueue().Remove(Read);
		GetGame().GetCallqueue().Remove(Close);
		GetGame().GetCallqueue().Remove(Timeout);
	}
}
modded class ArmaReforgerScripted
{
	protected ref AICF_WorkshopMetadataProbe m_AICFMetadataProbe;
	override bool OnGameStart()
	{
		bool result = super.OnGameStart();
		if (System.IsCLIParam("aicfMetadataProbe"))
		{
			m_AICFMetadataProbe = new AICF_WorkshopMetadataProbe();
			m_AICFMetadataProbe.Start();
		}
		return result;
	}
	override void OnGameEnd()
	{
		if (m_AICFMetadataProbe) m_AICFMetadataProbe.Stop();
		m_AICFMetadataProbe = null;
		super.OnGameEnd();
	}
}
