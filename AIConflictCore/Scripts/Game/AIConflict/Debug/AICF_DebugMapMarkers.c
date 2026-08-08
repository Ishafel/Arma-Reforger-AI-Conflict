// Opt-in, globally visible map markers for the eight Stage 1 managed AI groups.
// Both server and client must be started with -aicfDebugMapMarkers 1.
class AICF_DebugMapMarkerRuntime
{
	static bool IsEnabled()
	{
		string value;
		return System.GetCLIParam("aicfDebugMapMarkers", value) && value.ToInt() > 0;
	}
}

// DYNAMIC_EXAMPLE is deliberately reused because stock reserves it as an unconfigured example.
// The replicated marker config ID carries all visual metadata needed by the client.
[BaseContainerProps(), SCR_MapMarkerTitle()]
class AICF_DebugMapMarkerEntry : SCR_MapMarkerEntryDynamic
{
	static const ResourceName MARKER_PREFAB = "{DD74BE2BBAE07192}Prefabs/Markers/MapMarkerEntityBase.et";
	static const ResourceName MARKER_LAYOUT = "{3E27127E86F84A12}UI/layouts/Map/MapMarkerDynamicBase.layout";

	override SCR_EMapMarkerType GetMarkerType()
	{
		return SCR_EMapMarkerType.DYNAMIC_EXAMPLE;
	}

	override ResourceName GetMarkerPrefab()
	{
		return MARKER_PREFAB;
	}

	override ResourceName GetMarkerLayout()
	{
		return MARKER_LAYOUT;
	}

	override void InitClientSettingsDynamic(
		notnull SCR_MapMarkerEntity marker,
		notnull SCR_MapMarkerDynamicWComponent widgetComp)
	{
		int packed = marker.GetMarkerConfigID();
		int factionCode = packed / 100000;
		int remainder = packed % 100000;
		int slotId = remainder / 10000;
		remainder = remainder % 10000;
		int roleCode = remainder / 1000;
		int targetCode = remainder % 1000;

		FactionKey factionKey = "US";
		Color markerColor = Color.FromSRGBA(44, 126, 255, 255);
		if (factionCode == 1)
		{
			factionKey = "USSR";
			markerColor = Color.FromSRGBA(230, 66, 66, 255);
		}

		string role = "?";
		switch (roleCode)
		{
			case AICF_EGroupRole.ATTACK:
				role = "A";
				break;
			case AICF_EGroupRole.DEFEND:
				role = "D";
				break;
			case AICF_EGroupRole.RESERVE:
				role = "R";
				break;
		}

		string target = "-";
		if (targetCode > 0)
			target = (targetCode - 1).ToString();

		FactionManager factionManager = GetGame().GetFactionManager();
		SCR_Faction faction;
		if (factionManager)
			faction = SCR_Faction.Cast(factionManager.GetFactionByKey(factionKey));

		if (faction)
		{
			ResourceName imageSet = faction.GetGroupFlagImageSet();
			array<string> imageQuads = {};
			faction.GetFlagNames(imageQuads);
			if (!imageSet.IsEmpty() && !imageQuads.IsEmpty())
				widgetComp.SetImage(imageSet, imageQuads[0]);
		}

		widgetComp.SetColor(markerColor);
		widgetComp.SetText(string.Format("%1 %2%3 -> %4", factionKey, role, slotId, target));
		widgetComp.SetTextVisible(true);
	}
}

modded class SCR_MapMarkerManagerComponent
{
	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);
		if (!AICF_DebugMapMarkerRuntime.IsEnabled() || !m_MarkerCfg)
			return;

		// GetMarkerEntryConfigs returns the live config array. Mutate it through the
		// stock public API so SCR_MapMarkerConfig remains an untouched config-root type.
		array<ref SCR_MapMarkerEntryConfig> entries = m_MarkerCfg.GetMarkerEntryConfigs();
		if (!entries)
			return;

		for (int i = entries.Count() - 1; i >= 0; i--)
		{
			SCR_MapMarkerEntryConfig entry = entries[i];
			if (entry && entry.GetMarkerType() == SCR_EMapMarkerType.DYNAMIC_EXAMPLE)
				entries.Remove(i);
		}

		entries.Insert(new AICF_DebugMapMarkerEntry());
	}
}

class AICF_DebugMapMarkerSystem
{
	static const int SLOTS_PER_FACTION = AICF_Stage1Config.GROUP_SLOTS_PER_FACTION;
	static const int TOTAL_SLOTS = SLOTS_PER_FACTION * 2;

	protected SCR_MapMarkerManagerComponent m_MarkerManager;
	protected ref array<SCR_MapMarkerEntity> m_aMarkers = {};
	protected ref array<SCR_AIGroup> m_aTrackedGroups = {};
	protected ref array<int> m_aConfigIds = {};
	protected bool m_bReadyLogged;

	void AICF_DebugMapMarkerSystem()
	{
		for (int i = 0; i < TOTAL_SLOTS; i++)
		{
			m_aMarkers.Insert(null);
			m_aTrackedGroups.Insert(null);
			m_aConfigIds.Insert(-1);
		}
	}

	void Sync(AICF_FactionState usState, AICF_FactionState ussrState)
	{
		if (!Replication.IsServer())
			return;

		if (!m_MarkerManager)
			m_MarkerManager = SCR_MapMarkerManagerComponent.GetInstance();
		if (!m_MarkerManager || !m_MarkerManager.GetMarkerConfig() ||
			!m_MarkerManager.GetMarkerConfig().GetMarkerEntryConfigByType(SCR_EMapMarkerType.DYNAMIC_EXAMPLE))
			return;

		SyncFaction(usState, false, 0);
		SyncFaction(ussrState, true, SLOTS_PER_FACTION);

		if (!m_bReadyLogged && CountMarkers() == TOTAL_SLOTS)
		{
			m_bReadyLogged = true;
			AICF_Stage1Diagnostics.Info(
				"DEBUG_MAP_MARKERS_READY",
				string.Format("groups=%1 visibility=GLOBAL", TOTAL_SLOTS));
		}
	}

	void Stop()
	{
		for (int i = 0; i < m_aMarkers.Count(); i++)
			RemoveMarker(i);

		m_MarkerManager = null;
	}

	protected void SyncFaction(AICF_FactionState factionState, bool isUSSR, int offset)
	{
		if (!factionState)
			return;

		for (int slotId = 0; slotId < SLOTS_PER_FACTION; slotId++)
		{
			int markerIndex = offset + slotId;
			AICF_GroupSlot slot = factionState.GetSlot(slotId);
			SCR_AIGroup group;
			if (slot && slot.IsCombatReady())
				group = slot.GetGroup();

			if (!group)
			{
				RemoveMarker(markerIndex);
				continue;
			}

			int configId = PackConfig(isUSSR, slot);
			if (m_aMarkers[markerIndex] &&
				(m_aTrackedGroups[markerIndex] != group || m_aConfigIds[markerIndex] != configId))
			{
				RemoveMarker(markerIndex);
			}

			if (m_aMarkers[markerIndex])
				continue;

			SCR_MapMarkerEntity marker = m_MarkerManager.InsertDynamicMarker(
				SCR_EMapMarkerType.DYNAMIC_EXAMPLE,
				group,
				configId);
			if (!marker)
				continue;

			marker.SetGlobalVisible(true);
			m_aMarkers[markerIndex] = marker;
			m_aTrackedGroups[markerIndex] = group;
			m_aConfigIds[markerIndex] = configId;
		}
	}

	protected int PackConfig(bool isUSSR, AICF_GroupSlot slot)
	{
		int factionCode;
		if (isUSSR)
			factionCode = 1;

		int targetCode;
		SCR_CampaignMilitaryBaseComponent target = slot.GetTargetBase();
		if (target)
		{
			int callsign = target.GetCallsign();
			if (callsign < 0)
				callsign = 0;
			if (callsign > 998)
				callsign = 998;
			targetCode = callsign + 1;
		}

		return factionCode * 100000 +
			slot.GetSlotId() * 10000 +
			((int)slot.GetRole()) * 1000 +
			targetCode;
	}

	protected void RemoveMarker(int markerIndex)
	{
		if (markerIndex < 0 || markerIndex >= m_aMarkers.Count())
			return;

		SCR_MapMarkerEntity marker = m_aMarkers[markerIndex];
		if (marker && m_MarkerManager)
			m_MarkerManager.RemoveDynamicMarker(marker);

		m_aMarkers[markerIndex] = null;
		m_aTrackedGroups[markerIndex] = null;
		m_aConfigIds[markerIndex] = -1;
	}

	protected int CountMarkers()
	{
		int count;
		foreach (SCR_MapMarkerEntity marker : m_aMarkers)
		{
			if (marker)
				count++;
		}

		return count;
	}
}
