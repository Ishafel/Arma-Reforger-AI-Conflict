// Данные конкретных карт и зависимостей добавляет integration, не Core.
class AICF_ScenarioVariant
{
	string m_sKey;
	int m_iMap;
	int m_iContent;
	string m_sMapLabel;
	ResourceName m_sPreviewHeader;
	string m_sWestLabel;
	string m_sEastLabel;
	string m_sOwner;
	ref array<string> m_aAddons = {};
	// Только внешние content dependencies; собственные integrations загружает transition.
	ref array<string> m_aContentAddons = {};
	ref array<ResourceName> m_aPresets = {};

	ResourceName Resolve(int mode, int difficulty)
	{
		if (mode < 0 || mode > 2 || difficulty < 0 || difficulty > 2) return "";
		if (m_aPresets.Count() != 9 || m_aAddons.IsEmpty() || m_aAddons.Find(m_sOwner) < 0) return "";
		return m_aPresets[mode * 3 + difficulty];
	}

	bool HasDependencies(array<string> available, out string missing)
	{
		missing = "";
		if (!available || m_aAddons.IsEmpty()) return false;
		foreach (string id : m_aAddons)
		{
			if (available.Find(id) >= 0) continue;
			missing = id;
			return false;
		}
		return true;
	}

	bool IsAvailable(out string missing)
	{
		array<string> available = {};
		GameProject.GetAvailableAddons(available);
		if (!HasDependencies(available, missing)) return false;
		WorkshopApi workshop = GetGame().GetBackendApi().GetWorkshop();
		if (!workshop || workshop.NeedAddonsScan()) return false;
		foreach (string id : m_aAddons)
		{
			WorkshopItem item = workshop.FindItem(id);
			// Source projects have no WorkshopItem; their presence was checked above.
			if (!item) continue;
			Revision revision = item.GetActiveRevision();
			if (revision)
			{
				// Запускаем именно downloaded revision, не selected backend revision.
				// IsReadyToRun также зависит от online/selected metadata и может быть
				// false для целого offline пакета; полный локальный граф проверен выше.
				if (revision.IsDownloaded() && !revision.IsCorrupted()) continue;
				missing = id;
				return false;
			}
			revision = item.GetLocalRevision();
			// CLI/source roots can have a placeholder item with no backend revision.
			// In that case native GameProject availability is the local evidence.
			if (!revision) continue;
			if (revision.IsDownloaded() && !revision.IsCorrupted() && item.IsReadyToRun()) continue;
			missing = id;
			return false;
		}
		return true;
	}

	bool HasEnabledContent(array<string> enabled)
	{
		if (!enabled) return false;
		foreach (string id : m_aContentAddons)
			if (enabled.Find(id) < 0) return false;
		return true;
	}

	bool IsContentEnabled()
	{
		if (m_aContentAddons.IsEmpty()) return true;
		WorkshopApi workshop = GetGame().GetBackendApi().GetWorkshop();
		if (!workshop || workshop.NeedAddonsScan()) return false;
		array<string> loaded = {};
		GameProject.GetLoadedAddons(loaded);
		array<string> enabled = {};
		foreach (string id : m_aContentAddons)
		{
			WorkshopItem item = workshop.FindItem(id);
			if (item && item.IsEnabled()) enabled.Insert(id);
			// Source addon без WorkshopItem должен быть действительно загружен.
			else if (!item && loaded.Find(id) >= 0) enabled.Insert(id);
		}
		return HasEnabledContent(enabled);
	}

	string GetAddonList()
	{
		string result;
		foreach (string id : m_aAddons)
		{
			if (!result.IsEmpty()) result += ",";
			result += id;
		}
		return result;
	}

	array<ref DSMod> BuildHostMods()
	{
		array<ref DSMod> mods = {};
		foreach (string id : m_aAddons)
		{
			DSMod mod = new DSMod();
			mod.modId = id;
			mod.name = GameProject.GetAddonTitle(id);
			WorkshopItem item = GetGame().GetBackendApi().GetWorkshop().FindItem(id);
			if (item)
			{
				Revision revision = item.GetActiveRevision();
				if (!revision) revision = item.GetLocalRevision();
				if (revision) mod.version = revision.GetVersion();
			}
			mods.Insert(mod);
		}
		return mods;
	}
}

class AICF_ScenarioRegistry
{
	protected ref array<ref AICF_ScenarioVariant> m_aVariants = {};

	void AICF_ScenarioRegistry() { Build(m_aVariants); }
	// Instance virtual dispatch is required when integrations extend this catalog.
	void Build(array<ref AICF_ScenarioVariant> variants) {}
	// Integration maps its known source project suffixes to immutable addon GUIDs.
	string ResolveSourceProject(string path) { return ""; }
	array<ref AICF_ScenarioVariant> GetVariants() { return m_aVariants; }
	AICF_ScenarioVariant Find(int mapIndex, int content)
	{
		foreach (AICF_ScenarioVariant variant : m_aVariants)
			if (variant.m_iMap == mapIndex && variant.m_iContent == content) return variant;
		return null;
	}
	bool IsLeaf(ResourceName resource)
	{
		foreach (AICF_ScenarioVariant variant : m_aVariants)
			if (variant.m_aPresets.Find(resource) >= 0) return true;
		return false;
	}
}
