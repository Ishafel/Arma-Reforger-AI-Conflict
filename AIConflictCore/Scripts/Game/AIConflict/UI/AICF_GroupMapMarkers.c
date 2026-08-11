// Gameplay map markers for the leaders of all managed AI groups. The current
// visibility policy is intentionally global; a later stage will filter them to
// the local player's allied faction without changing marker identity or text.

// Replicate the live group label independently from the marker config ID. The
// config ID keeps the stable faction/slot visuals, while this text can change
// as casualties occur and the commander assigns a new objective.
modded class SCR_MapMarkerEntity
{
	[RplProp(onRplName: "AICF_OnGroupMarkerTextReplicated")]
	protected string m_sAICFGroupMarkerText;

	void AICF_SetGroupMarkerText(string text)
	{
		if (!Replication.IsServer() || m_sAICFGroupMarkerText == text)
			return;

		m_sAICFGroupMarkerText = text;
		Replication.BumpMe();
		AICF_ApplyGroupMarkerText();
	}

	string AICF_GetGroupMarkerText()
	{
		return m_sAICFGroupMarkerText;
	}

	protected void AICF_OnGroupMarkerTextReplicated()
	{
		AICF_ApplyGroupMarkerText();
	}

	protected void AICF_ApplyGroupMarkerText()
	{
		if (!m_MarkerWidgetComp || m_sAICFGroupMarkerText.IsEmpty())
			return;

		m_MarkerWidgetComp.SetText(m_sAICFGroupMarkerText);
		m_MarkerWidgetComp.SetTextVisible(true);
	}
}

// DYNAMIC_EXAMPLE is deliberately reused because stock reserves it as an unconfigured example.
// The replicated marker config ID carries all visual metadata needed by the client.
[BaseContainerProps(), SCR_MapMarkerTitle()]
class AICF_GroupMapMarkerEntry : SCR_MapMarkerEntryDynamic
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
		remainder = remainder % 1000;
		int roleLocalIndex = remainder / 100;

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

		string markerText = marker.AICF_GetGroupMarkerText();
		if (markerText.IsEmpty())
			markerText = string.Format("%1 %2%3", factionKey, role, roleLocalIndex);

		widgetComp.SetColor(markerColor);
		widgetComp.SetText(markerText);
		widgetComp.SetTextVisible(true);
	}
}

modded class SCR_MapMarkerManagerComponent
{
	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);
		if (!m_MarkerCfg)
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

		entries.Insert(new AICF_GroupMapMarkerEntry());
	}
}

class AICF_GroupMapMarkerSystem
{
	static const int SLOTS_PER_FACTION = AICF_Stage1Config.GROUP_SLOTS_PER_FACTION;
	static const int TOTAL_SLOTS = SLOTS_PER_FACTION * 2;
	static const float AT_OBJECTIVE_RADIUS_METERS = 75;

	protected SCR_MapMarkerManagerComponent m_MarkerManager;
	protected ref array<SCR_MapMarkerEntity> m_aMarkers = {};
	protected ref array<SCR_AIGroup> m_aTrackedGroups = {};
	protected ref array<IEntity> m_aTrackedLeaders = {};
	protected bool m_bReadyLogged;

	void AICF_GroupMapMarkerSystem()
	{
		for (int i = 0; i < TOTAL_SLOTS; i++)
		{
			m_aMarkers.Insert(null);
			m_aTrackedGroups.Insert(null);
			m_aTrackedLeaders.Insert(null);
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
				"GROUP_MAP_MARKERS_READY",
				string.Format("groups=%1 visibility=GLOBAL tracking=LEADER", TOTAL_SLOTS));
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
			IEntity leader = AICF_GroupRuntime.ResolveAliveLeader(group);

			if (!group || !leader)
			{
				// Visibility-only updates can leave an already-created client widget on the
				// map. Delete the replicated marker entity when a slot stops being combat
				// ready; a successful replacement receives a fresh entity below.
				if (m_aMarkers[markerIndex])
				{
					FactionKey factionKey = "US";
					if (isUSSR)
						factionKey = "USSR";
					string removalReason = "GROUP_NOT_COMBAT_READY";
					if (group)
						removalReason = "NO_ALIVE_LEADER";

					AICF_Stage1Diagnostics.Info(
						"GROUP_MAP_MARKER_REMOVED",
						string.Format(
							"faction=%1 slot=%2 reason=%3",
							factionKey,
							slotId,
							removalReason));
					RemoveMarker(markerIndex);
				}
				continue;
			}

			SCR_MapMarkerEntity marker = m_aMarkers[markerIndex];
			if (marker)
			{
				if (m_aTrackedGroups[markerIndex] != group || m_aTrackedLeaders[markerIndex] != leader)
				{
					bool groupChanged = m_aTrackedGroups[markerIndex] != group;
					marker.SetTarget(leader);
					marker.SetGlobalVisible(true);
					m_aTrackedGroups[markerIndex] = group;
					m_aTrackedLeaders[markerIndex] = leader;

					FactionKey factionKey = "US";
					if (isUSSR)
						factionKey = "USSR";
					string retargetReason = "LEADER_CHANGED";
					if (groupChanged)
						retargetReason = "GROUP_REPLACED";
					AICF_Stage1Diagnostics.Info(
						"GROUP_MAP_MARKER_RETARGETED",
						string.Format(
							"faction=%1 slot=%2 reason=%3",
							factionKey,
							slotId,
							retargetReason));
				}

				marker.AICF_SetGroupMarkerText(BuildMarkerText(factionState, slot, group));
				continue;
			}

			marker = m_MarkerManager.InsertDynamicMarker(
				SCR_EMapMarkerType.DYNAMIC_EXAMPLE,
				leader,
				PackStableConfig(isUSSR, slot));
			if (!marker)
				continue;

			marker.AICF_SetGroupMarkerText(BuildMarkerText(factionState, slot, group));

			// Apply stream rules immediately for clients that were already connected.
			// Markers created before player spawn are covered by stock
			// SetStreamRulesForPlayer; replacement markers are not unless SetFaction
			// explicitly refreshes the current connection nodes. A null faction keeps
			// the current gameplay policy globally visible to both sides.
			marker.SetFaction(null);
			marker.SetGlobalVisible(true);
			m_aMarkers[markerIndex] = marker;
			m_aTrackedGroups[markerIndex] = group;
			m_aTrackedLeaders[markerIndex] = leader;

			FactionKey factionKey = "US";
			if (isUSSR)
				factionKey = "USSR";
			AICF_Stage1Diagnostics.Info(
				"GROUP_MAP_MARKER_CREATED",
				string.Format("faction=%1 slot=%2 tracking=LEADER", factionKey, slotId));
		}
	}

	protected string BuildMarkerText(
		AICF_FactionState factionState,
		AICF_GroupSlot slot,
		SCR_AIGroup group)
	{
		string factionKey = factionState.GetFactionKey();
		string role = AICF_Stage1Diagnostics.RoleToString(slot.GetRole());
		string identity = string.Format(
			"%1 %2",
			factionKey,
			GetRoleLocalMarkerKey(slot));
		string task = DescribeTask(factionState, slot, group);
		if (slot.IsRecoveringFromStuck())
		{
			task = string.Format(
				"ROUTE RECOVERY %1 | %2",
				slot.GetStuckRecoveryCount(),
				task);
		}
		int alive = AICF_GroupRuntime.CountAliveAgents(group);
		string vehicleState;
		AICF_VehicleRuntime vehicleRuntime = slot.GetVehicleRuntime();
		if (vehicleRuntime)
			vehicleState = AICF_Stage3Diagnostics.StateToString(vehicleRuntime.GetState());
		if (vehicleState.IsEmpty() || vehicleState == "NONE")
			vehicleState = "ON_FOOT";

		return string.Format(
			"%1 | %2 | %3 | ALIVE %4 | VEH %5",
			identity,
			role,
			task,
			alive,
			vehicleState);
	}

	protected string DescribeTask(
		AICF_FactionState factionState,
		AICF_GroupSlot slot,
		SCR_AIGroup group)
	{
		SCR_CampaignMilitaryBaseComponent target = slot.GetTargetBase();
		if (!target || !target.GetOwner())
			return "AWAITING ORDER";

		string targetName = WidgetManager.Translate(target.GetBaseName());
		if (targetName.IsEmpty())
			targetName = "BASE";

		string targetLabel = string.Format(
			"%1 [%2]",
			targetName,
			target.GetCallsign());
		IEntity leader = AICF_GroupRuntime.ResolveAliveLeader(group);
		vector groupPosition = group.GetOrigin();
		if (leader)
			groupPosition = leader.GetOrigin();
		bool atObjective = vector.DistanceSqXZ(
			groupPosition,
			target.GetOwner().GetOrigin()) <=
			AT_OBJECTIVE_RADIUS_METERS * AT_OBJECTIVE_RADIUS_METERS;

		switch (slot.GetRole())
		{
			case AICF_EGroupRole.ATTACK:
				Faction targetFaction = target.GetFaction();
				if (targetFaction && targetFaction.GetFactionKey() == factionState.GetFactionKey())
					return string.Format("AWAITING RETASK AT %1", targetLabel);

				if (target.GetType() == SCR_ECampaignBaseType.RELAY)
				{
					if (atObjective)
						return string.Format("CAPTURING RELAY %1", targetLabel);

					return string.Format("MOVING TO RELAY %1", targetLabel);
				}

				if (atObjective)
					return string.Format("CAPTURING %1", targetLabel);

				return string.Format("MOVING TO %1", targetLabel);

			case AICF_EGroupRole.DEFEND:
				if (slot.GetOperationalPosture() == "QRF")
				{
					if (atObjective)
						return string.Format("QRF AT %1", targetLabel);

					return string.Format("QRF TO %1", targetLabel);
				}

				if (atObjective)
					return string.Format("FORWARD DEFEND %1", targetLabel);

				return string.Format("MOVING TO FORWARD DEFEND %1", targetLabel);

			case AICF_EGroupRole.RESERVE:
				if (atObjective)
					return string.Format("HOLDING RESERVE AT %1", targetLabel);

				return string.Format("MOVING TO RESERVE %1", targetLabel);
		}

		return "AWAITING ORDER";
	}

	protected string GetShortRole(AICF_EGroupRole role)
	{
		switch (role)
		{
			case AICF_EGroupRole.ATTACK:
				return "A";
			case AICF_EGroupRole.DEFEND:
				return "D";
			case AICF_EGroupRole.RESERVE:
				return "R";
		}

		return "?";
	}

	// Marker identity is role-local (A0/A1/A2/D0), while slotId remains the
	// stable internal identity used by lifecycle and replication. The boundary
	// fallback also keeps old 2/1/1 baseline layouts readable.
	protected string GetRoleLocalMarkerKey(AICF_GroupSlot slot)
	{
		if (!slot)
			return "?";

		int roleLocalIndex = slot.GetRoleIndex();
		if (slot.GetRole() == AICF_EGroupRole.DEFEND &&
			slot.GetSlotId() >= AICF_Stage1Config.ATTACK_SLOTS_PER_FACTION)
		{
			roleLocalIndex = slot.GetSlotId() - AICF_Stage1Config.ATTACK_SLOTS_PER_FACTION;
		}

		return string.Format("%1%2", GetShortRole(slot.GetRole()), roleLocalIndex);
	}

	protected int PackStableConfig(bool isUSSR, AICF_GroupSlot slot)
	{
		int factionCode;
		if (isUSSR)
			factionCode = 1;

		return factionCode * 100000 +
			slot.GetSlotId() * 10000 +
			((int)slot.GetRole()) * 1000 +
			slot.GetRoleIndex() * 100;
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
		m_aTrackedLeaders[markerIndex] = null;
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
