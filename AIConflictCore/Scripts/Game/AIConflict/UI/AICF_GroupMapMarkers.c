// Gameplay map markers for managed allied groups and their current objectives.
// Stock marker streaming keeps each faction's operational picture private.

modded class SCR_MapMarkerDynamicWComponent
{
	static const string AICF_ATTACK_BADGE_TEXT_NAME = "AICF_AttackBadgeText";

	void AICF_SetLiveText(string text)
	{
		Widget markerRoot = GetRootWidget();
		TextWidget attackBadgeText;
		if (markerRoot)
		{
			attackBadgeText = TextWidget.Cast(
				markerRoot.FindAnyWidget(AICF_ATTACK_BADGE_TEXT_NAME));
		}

		if (attackBadgeText)
		{
			attackBadgeText.SetText(text);
			SetTextVisible(false);
			return;
		}

		SetText(text);
		SetTextVisible(true);
	}
}

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

		m_MarkerWidgetComp.AICF_SetLiveText(m_sAICFGroupMarkerText);
	}
}

// DYNAMIC_EXAMPLE is deliberately reused because stock reserves it as an unconfigured example.
// The replicated marker config ID carries all visual metadata needed by the client.
[BaseContainerProps(), SCR_MapMarkerTitle()]
class AICF_GroupMapMarkerEntry : SCR_MapMarkerEntryDynamic
{
	static const ResourceName MARKER_PREFAB = "{DD74BE2BBAE07192}Prefabs/Markers/MapMarkerEntityBase.et";
	static const ResourceName MARKER_LAYOUT = "{3E27127E86F84A12}UI/layouts/Map/MapMarkerDynamicBase.layout";
	static const ResourceName ATTACK_BADGE_FONT =
		"{3E7733BAC8C831F6}UI/Fonts/RobotoCondensed/RobotoCondensed_Regular.fnt";
	static const string ATTACK_BADGE_NAME = "AICF_AttackBadge";

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
		int markerKind = remainder % 10;

		FactionKey factionKey = "US";
		Color markerColor = Color.FromSRGBA(44, 126, 255, 255);
		if (factionCode == 1)
		{
			factionKey = "USSR";
			markerColor = Color.FromSRGBA(230, 66, 66, 255);
		}
		if (markerKind == 1)
		{
			if (factionCode == 1)
				markerColor = Color.FromSRGBA(255, 132, 84, 255);
			else
				markerColor = Color.FromSRGBA(232, 143, 38, 255);
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

		if (faction && markerKind == 0)
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
		if (markerKind == 1)
			ConfigureAttackBadge(widgetComp, markerColor);
		widgetComp.AICF_SetLiveText(markerText);
	}

	protected void ConfigureAttackBadge(
		notnull SCR_MapMarkerDynamicWComponent widgetComp,
		Color markerColor)
	{
		Widget markerRoot = widgetComp.GetRootWidget();
		if (!markerRoot)
			return;
		markerRoot.ClearFlags(WidgetFlags.CLIPCHILDREN);

		ImageWidget markerIcon = ImageWidget.Cast(
			markerRoot.FindAnyWidget("MarkerIcon"));
		if (markerIcon)
			markerIcon.SetVisible(false);
		TextWidget stockMarkerLabel = TextWidget.Cast(
			markerRoot.FindAnyWidget("MarkerText"));
		if (stockMarkerLabel)
			stockMarkerLabel.SetVisible(false);

		ImageWidget attackBadge = ImageWidget.Cast(
			GetGame().GetWorkspace().CreateWidget(
				WidgetType.ImageWidgetTypeID,
				WidgetFlags.VISIBLE | WidgetFlags.IGNORE_CURSOR |
					WidgetFlags.BLEND | WidgetFlags.STRETCH |
					WidgetFlags.NOWRAP,
				Color.FromSRGBA(5, 10, 14, 235),
				1,
				markerRoot));
		if (attackBadge)
		{
			attackBadge.SetName(ATTACK_BADGE_NAME);
			attackBadge.SetColor(Color.FromSRGBA(5, 10, 14, 235));
			FrameSlot.SetAnchor(attackBadge, 0.5, 1);
			FrameSlot.SetAlignment(attackBadge, 0.5, 0);
			FrameSlot.SetSize(attackBadge, 150, 24);
			FrameSlot.SetPos(attackBadge, 0, 18);
			attackBadge.SetZOrder(1);
		}

		TextWidget attackBadgeText = TextWidget.Cast(
			GetGame().GetWorkspace().CreateWidget(
				WidgetType.TextWidgetTypeID,
				WidgetFlags.VISIBLE | WidgetFlags.IGNORE_CURSOR |
					WidgetFlags.BLEND | WidgetFlags.CENTER |
					WidgetFlags.VCENTER | WidgetFlags.NO_LOCALIZATION,
				markerColor,
				2,
				markerRoot));
		if (!attackBadgeText)
			return;
		attackBadgeText.SetName(
			SCR_MapMarkerDynamicWComponent.AICF_ATTACK_BADGE_TEXT_NAME);
		attackBadgeText.SetColor(markerColor);
		attackBadgeText.SetFont(ATTACK_BADGE_FONT);
		attackBadgeText.SetExactFontSize(14);
		FrameSlot.SetAnchor(attackBadgeText, 0.5, 1);
		FrameSlot.SetAlignment(attackBadgeText, 0.5, 0);
		FrameSlot.SetSize(attackBadgeText, 150, 24);
		FrameSlot.SetPos(attackBadgeText, 0, 18);
		attackBadgeText.SetZOrder(2);
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
	protected ref array<SCR_MapMarkerEntity> m_aObjectiveMarkers = {};
	protected ref array<SCR_CampaignMilitaryBaseComponent> m_aTrackedObjectives = {};
	protected bool m_bReadyLogged;

	void AICF_GroupMapMarkerSystem()
	{
		for (int i = 0; i < TOTAL_SLOTS; i++)
		{
			m_aMarkers.Insert(null);
			m_aTrackedGroups.Insert(null);
			m_aTrackedLeaders.Insert(null);
			m_aObjectiveMarkers.Insert(null);
			m_aTrackedObjectives.Insert(null);
		}
	}

	void Sync(
		AICF_FactionState usState,
		AICF_FactionState ussrState,
		AICF_VehicleCoordinator vehicleCoordinator)
	{
		if (!Replication.IsServer())
			return;

		if (!m_MarkerManager)
			m_MarkerManager = SCR_MapMarkerManagerComponent.GetInstance();
		if (!m_MarkerManager || !m_MarkerManager.GetMarkerConfig() ||
			!m_MarkerManager.GetMarkerConfig().GetMarkerEntryConfigByType(SCR_EMapMarkerType.DYNAMIC_EXAMPLE))
			return;

		SyncFaction(usState, false, 0, vehicleCoordinator);
		SyncFaction(ussrState, true, SLOTS_PER_FACTION, vehicleCoordinator);

		if (!m_bReadyLogged && CountMarkers() == TOTAL_SLOTS)
		{
			m_bReadyLogged = true;
			AICF_Stage1Diagnostics.Info(
				"GROUP_MAP_MARKERS_READY",
				string.Format("groups=%1 visibility=ALLIED tracking=LEADER", TOTAL_SLOTS));
		}
	}

	void Stop()
	{
		for (int i = 0; i < m_aMarkers.Count(); i++)
		{
			RemoveMarker(i);
			RemoveObjectiveMarker(i);
		}

		m_MarkerManager = null;
	}

	protected void SyncFaction(
		AICF_FactionState factionState,
		bool isUSSR,
		int offset,
		AICF_VehicleCoordinator vehicleCoordinator)
	{
		if (!factionState)
			return;
		FactionManager factionManager = GetGame().GetFactionManager();
		SCR_Faction markerFaction;
		if (factionManager)
			markerFaction = SCR_Faction.Cast(
				factionManager.GetFactionByKey(factionState.GetFactionKey()));
		if (!markerFaction)
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

				marker.AICF_SetGroupMarkerText(BuildMarkerText(
					factionState,
					slot,
					group,
					vehicleCoordinator));
				continue;
			}

			marker = m_MarkerManager.InsertDynamicMarker(
				SCR_EMapMarkerType.DYNAMIC_EXAMPLE,
				leader,
				PackStableConfig(isUSSR, slot));
			if (!marker)
				continue;

			marker.AICF_SetGroupMarkerText(BuildMarkerText(
				factionState,
				slot,
				group,
				vehicleCoordinator));

			// Apply stream rules immediately for clients that were already connected.
			// Markers created before player spawn are covered by stock
			// SetStreamRulesForPlayer; replacement markers are not unless SetFaction
			// explicitly refreshes the current connection nodes.
			marker.SetFaction(markerFaction);
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

		SyncFactionObjectiveMarkers(factionState, isUSSR, offset, markerFaction);
	}

	protected string BuildMarkerText(
		AICF_FactionState factionState,
		AICF_GroupSlot slot,
		SCR_AIGroup group,
		AICF_VehicleCoordinator vehicleCoordinator)
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
		AICF_VehicleSlotView vehicleView;
		if (vehicleCoordinator)
			vehicleView = vehicleCoordinator.GetSlotView(slot);
		if (vehicleView)
			vehicleState = vehicleView.GetStatusText();
		if (vehicleState.IsEmpty() || vehicleState == "NONE")
			vehicleState = "Пешком";

		return string.Format(
			"%1 | %2 | %3 | БОЙЦОВ %4 | ТЕХНИКА %5 | %6",
			identity,
			role,
			task,
			alive,
			vehicleState,
			DescribeDirection(group, slot.GetTargetBase()));
	}

	protected string DescribeDirection(
		SCR_AIGroup group,
		SCR_CampaignMilitaryBaseComponent target)
	{
		if (!group || !target || !target.GetOwner())
			return "DIR -";
		IEntity leader = AICF_GroupRuntime.ResolveAliveLeader(group);
		vector origin = group.GetOrigin();
		if (leader)
			origin = leader.GetOrigin();
		vector destination = target.GetOwner().GetOrigin();
		vector direction = vector.Direction(origin, destination);
		float bearing = Math.Atan2(direction[0], direction[2]) * Math.RAD2DEG;
		if (bearing < 0)
			bearing += 360;
		int sector = Math.Floor((bearing + 22.5) / 45.0);
		if (sector >= 8)
			sector = 0;
		string compass = "N";
		switch (sector)
		{
			case 1: compass = "NE"; break;
			case 2: compass = "E"; break;
			case 3: compass = "SE"; break;
			case 4: compass = "S"; break;
			case 5: compass = "SW"; break;
			case 6: compass = "W"; break;
			case 7: compass = "NW"; break;
		}
		return string.Format(
			"DIR %1 %2m",
			compass,
			Math.Round(vector.DistanceXZ(origin, destination)));
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

	// Marker identity is role-local, while slotId remains the stable internal
	// identity used by lifecycle and replication. Commander role changes reindex
	// all same-role callsigns in AICF_FactionState.
	protected string GetRoleLocalMarkerKey(AICF_GroupSlot slot)
	{
		if (!slot)
			return "?";

		return string.Format(
			"%1%2",
			GetShortRole(slot.GetRole()),
			slot.GetRoleIndex());
	}

	protected int PackStableConfig(bool isUSSR, AICF_GroupSlot slot, bool objective = false)
	{
		int factionCode;
		if (isUSSR)
			factionCode = 1;
		int markerKind;
		if (objective)
			markerKind = 1;

		return factionCode * 100000 +
			slot.GetSlotId() * 10000 +
			((int)slot.GetRole()) * 1000 +
			slot.GetRoleIndex() * 100 + markerKind;
	}

	// One target base owns one marker per faction. Multiple ATTACK slots are
	// folded into the same label so co-located replicated widgets cannot overlap.
	protected void SyncFactionObjectiveMarkers(
		AICF_FactionState factionState,
		bool isUSSR,
		int offset,
		SCR_Faction markerFaction)
	{
		array<SCR_CampaignMilitaryBaseComponent> targets = {};
		array<AICF_GroupSlot> representativeSlots = {};
		array<string> attackers = {};
		for (int slotId = 0; slotId < factionState.GetSlotCount(); slotId++)
		{
			AICF_GroupSlot slot = factionState.GetSlot(slotId);
			if (!slot || !slot.IsCombatReady() ||
				slot.GetRole() != AICF_EGroupRole.ATTACK)
			{
				continue;
			}

			SCR_CampaignMilitaryBaseComponent target = slot.GetTargetBase();
			if (!target || !target.GetOwner())
				continue;

			int targetIndex = targets.Find(target);
			if (targetIndex < 0)
			{
				targets.Insert(target);
				representativeSlots.Insert(slot);
				attackers.Insert(slot.GetSlotKey());
			}
			else
			{
				string attackerList = attackers[targetIndex];
				attackerList += string.Format("+%1", slot.GetSlotKey());
				attackers.Set(targetIndex, attackerList);
			}
		}

		for (int objectiveIndex = 0; objectiveIndex < SLOTS_PER_FACTION; objectiveIndex++)
		{
			int markerIndex = offset + objectiveIndex;
			if (objectiveIndex >= targets.Count())
			{
				RemoveObjectiveMarker(markerIndex);
				continue;
			}

			SCR_CampaignMilitaryBaseComponent target = targets[objectiveIndex];
			SCR_MapMarkerEntity objectiveMarker = m_aObjectiveMarkers[markerIndex];
			if (!objectiveMarker || m_aTrackedObjectives[markerIndex] != target)
			{
				RemoveObjectiveMarker(markerIndex);
				objectiveMarker = m_MarkerManager.InsertDynamicMarker(
					SCR_EMapMarkerType.DYNAMIC_EXAMPLE,
					target.GetOwner(),
					PackStableConfig(
						isUSSR,
						representativeSlots[objectiveIndex],
						true));
				if (!objectiveMarker)
					continue;
				objectiveMarker.SetFaction(markerFaction);
				objectiveMarker.SetGlobalVisible(true);
				m_aObjectiveMarkers[markerIndex] = objectiveMarker;
				m_aTrackedObjectives[markerIndex] = target;
			}

			objectiveMarker.AICF_SetGroupMarkerText(string.Format(
				"ATK  %1",
				attackers[objectiveIndex]));
		}
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

	protected void RemoveObjectiveMarker(int markerIndex)
	{
		if (markerIndex < 0 || markerIndex >= m_aObjectiveMarkers.Count())
			return;
		SCR_MapMarkerEntity marker = m_aObjectiveMarkers[markerIndex];
		if (marker && m_MarkerManager)
			m_MarkerManager.RemoveDynamicMarker(marker);
		m_aObjectiveMarkers[markerIndex] = null;
		m_aTrackedObjectives[markerIndex] = null;
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
