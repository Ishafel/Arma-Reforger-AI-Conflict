// Gameplay map markers for managed allied groups and their current objectives.
// Stock marker streaming keeps each faction's operational picture private.

modded class SCR_MapMarkerDynamicWComponent
{
	static const string AICF_ATTACK_BADGE_TEXT_NAME = "AICF_AttackBadgeText";
	protected ref AICF_MapMarkerCardWidget m_AICFCardWidget;
	protected int m_iAICFCardKind = -1;

	void AICF_ConfigureLogisticsMarker(Color color, SCR_MapMarkerEntity marker)
	{
		m_iAICFCardKind = AICF_LogisticsMapMarkerSystem.MARKER_KIND;
		m_AICFCardWidget = new AICF_MapMarkerCardWidget();
		m_AICFCardWidget.Init(GetRootWidget(), color, "Л");
		AICF_SetLiveText(marker.AICF_GetGroupMarkerText());
		AICF_SetLogisticsDetails(marker.AICF_GetLogisticsDetails());
	}

	void AICF_ConfigureGroupMarker(Color color, SCR_MapMarkerEntity marker)
	{
		m_iAICFCardKind = 0;
		m_AICFCardWidget = new AICF_MapMarkerCardWidget();
		m_AICFCardWidget.Init(GetRootWidget(), color, "О");
		AICF_SetLiveText(marker.AICF_GetGroupMarkerText());
		AICF_SetGroupDetails(marker.AICF_GetGroupDetails());
	}

	void AICF_SetGroupDetails(string details)
	{
		if (m_AICFCardWidget && m_iAICFCardKind == 0) m_AICFCardWidget.SetDetails(details);
	}

	void AICF_SetLogisticsDetails(string details)
	{
		if (m_AICFCardWidget && m_iAICFCardKind == AICF_LogisticsMapMarkerSystem.MARKER_KIND)
			m_AICFCardWidget.SetDetails(details);
	}

	override bool OnMouseEnter(Widget w, int x, int y)
	{
		if (m_AICFCardWidget) m_AICFCardWidget.ShowDetails(true);
		return super.OnMouseEnter(w, x, y);
	}

	override bool OnMouseLeave(Widget w, Widget enterW, int x, int y)
	{
		if (m_AICFCardWidget && !m_AICFCardWidget.Contains(enterW))
			m_AICFCardWidget.ShowDetails(false);
		return super.OnMouseLeave(w, enterW, x, y);
	}

	void AICF_SetLiveText(string text)
	{
		if (m_AICFCardWidget)
		{
			m_AICFCardWidget.SetLabel(text);
			return;
		}
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
	[RplProp(onRplName: "AICF_OnLogisticsDetailsReplicated")]
	protected string m_sAICFLogisticsDetails;
	[RplProp(onRplName: "AICF_OnGroupDetailsReplicated")]
	protected string m_sAICFGroupDetails;

	void AICF_SetGroupMarkerData(string label, string details)
	{
		if (!Replication.IsServer() ||
			(m_sAICFGroupMarkerText == label && m_sAICFGroupDetails == details)) return;
		m_sAICFGroupMarkerText = label;
		m_sAICFGroupDetails = details;
		Replication.BumpMe();
		AICF_ApplyGroupMarkerText();
		AICF_OnGroupDetailsReplicated();
	}

	string AICF_GetGroupDetails()
	{
		return m_sAICFGroupDetails;
	}

	protected void AICF_OnGroupDetailsReplicated()
	{
		if (m_MarkerWidgetComp) m_MarkerWidgetComp.AICF_SetGroupDetails(m_sAICFGroupDetails);
	}

	void AICF_SetLogisticsMarkerData(string label, string details)
	{
		if (!Replication.IsServer() ||
			(m_sAICFGroupMarkerText == label && m_sAICFLogisticsDetails == details)) return;
		m_sAICFGroupMarkerText = label;
		m_sAICFLogisticsDetails = details;
		Replication.BumpMe();
		AICF_ApplyGroupMarkerText();
		AICF_OnLogisticsDetailsReplicated();
	}

	string AICF_GetLogisticsDetails()
	{
		return m_sAICFLogisticsDetails;
	}

	protected void AICF_OnLogisticsDetailsReplicated()
	{
		if (m_MarkerWidgetComp) m_MarkerWidgetComp.AICF_SetLogisticsDetails(m_sAICFLogisticsDetails);
	}

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
		if (markerKind == AICF_LogisticsMapMarkerSystem.MARKER_KIND)
		{
			widgetComp.AICF_ConfigureLogisticsMarker(markerColor, marker);
			return;
		}
		if (markerKind == 0)
		{
			widgetComp.AICF_ConfigureGroupMarker(markerColor, marker);
			return;
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
	static const float POINT_AT_OBJECTIVE_RADIUS_METERS = 20;

	protected SCR_MapMarkerManagerComponent m_MarkerManager;
	protected ref array<SCR_MapMarkerEntity> m_aMarkers = {};
	protected ref array<SCR_AIGroup> m_aTrackedGroups = {};
	protected ref array<IEntity> m_aTrackedLeaders = {};
	protected ref array<SCR_MapMarkerEntity> m_aObjectiveMarkers = {};
	protected ref array<SCR_CampaignMilitaryBaseComponent> m_aTrackedObjectives = {};
	protected ref array<ref SCR_MapMarkerBase> m_aDestinationMarkers = {};
	protected ref array<int> m_aDestinationIntentRevisions = {};
	protected ref array<vector> m_aDestinationPositions = {};
	protected ref array<string> m_aDestinationSlotKeys = {};
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
			m_aDestinationMarkers.Insert(null);
			m_aDestinationIntentRevisions.Insert(-1);
			m_aDestinationPositions.Insert(vector.Zero);
			m_aDestinationSlotKeys.Insert(string.Empty);
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
			RemoveDestinationMarker(i);
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
			SyncDestinationMarker(
				slot,
				markerIndex,
				isUSSR,
				markerFaction,
				factionManager);
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

				marker.AICF_SetGroupMarkerData(
					BuildMarkerText(factionState, slot, group),
					BuildMarkerDetails(factionState, slot, group, vehicleCoordinator));
				continue;
			}

			marker = m_MarkerManager.InsertDynamicMarker(
				SCR_EMapMarkerType.DYNAMIC_EXAMPLE,
				leader,
				PackStableConfig(isUSSR, slot));
			if (!marker)
				continue;

			marker.AICF_SetGroupMarkerData(
				BuildMarkerText(factionState, slot, group),
				BuildMarkerDetails(factionState, slot, group, vehicleCoordinator));

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
		SCR_AIGroup group)
	{
		return string.Format("%1 · %2 · %3 чел.\n%4",
			GetRoleLocalMarkerKey(slot), DescribeRole(slot),
			AICF_GroupRuntime.CountAliveAgents(group), DescribeTask(factionState, slot, group));
	}

	protected string BuildMarkerDetails(
		AICF_FactionState factionState,
		AICF_GroupSlot slot,
		SCR_AIGroup group,
		AICF_VehicleCoordinator vehicleCoordinator)
	{
		string side = "США";
		if (AICF_ContentProfile.GetActive().GetStableFactionKey(factionState.GetFactionKey()) == "USSR")
			side = "СССР";
		string authority = "Не назначен";
		if (slot.IsAwaitingPlayerCommand() || slot.IsSystemHoldOrder())
			authority = "Ожидает приказа игрока";
		else if (slot.HasPlayerStrategicOrder())
			authority = "Игрок";
		else if (slot.GetDecisionAuthority() == AICF_EStrategicDecisionAuthority.AI_COMMANDER)
			authority = "AI-командир";
		int alive = AICF_GroupRuntime.CountAliveAgents(group);
		int inVehicle = AICF_GroupRuntime.CountAliveAgentsInAnyVehicle(group);
		string vehicleState;
		if (vehicleCoordinator)
			vehicleState = vehicleCoordinator.GetSlotDisplayStatusText(slot);
		if (vehicleState.IsEmpty() || vehicleState == "NONE")
		{
			vehicleState = "Пешком";
			if (inVehicle > 0)
				vehicleState = string.Format("В технике %1/%2", inVehicle, alive);
		}
		string details = string.Format("Отряд %1 · %2 · %3\nСостояние: %4\nБойцов: %5 · Плановый состав: %6",
			GetRoleLocalMarkerKey(slot), side, DescribeRole(slot),
			DescribeTask(factionState, slot, group), alive, slot.GetDesiredSize());
		details += string.Format("\nТехника: %1\nЦель: %2\nДо цели по прямой: %3\nПриказ: %4",
			vehicleState, DescribeObjective(slot), DescribeDirection(group, slot), authority);
		return details;
	}

	protected string DescribeRole(AICF_GroupSlot slot)
	{
		switch (slot.GetRole())
		{
			case AICF_EGroupRole.ATTACK: return "Атака";
			case AICF_EGroupRole.DEFEND: return "Оборона";
			case AICF_EGroupRole.RESERVE: return "Резерв";
		}
		return "Отряд";
	}

	protected string DescribeObjective(AICF_GroupSlot slot)
	{
		if (!slot.HasStrategicDestination()) return "Не назначена";
		if (slot.GetTargetKind() == AICF_EOrderTargetKind.POSITION)
		{
			vector position = slot.GetTargetPosition();
			return string.Format("Точка %1 / %2", Math.Round(position[0]), Math.Round(position[2]));
		}
		SCR_CampaignMilitaryBaseComponent target = slot.GetTargetBase();
		if (!target || !target.GetOwner()) return "Не назначена";
		string name = WidgetManager.Translate(target.GetBaseName());
		if (name.IsEmpty()) name = "База";
		return string.Format("%1 [%2]", name, target.GetCallsign());
	}

	protected string DescribeDirection(SCR_AIGroup group, AICF_GroupSlot slot)
	{
		if (!group || !slot || !slot.HasStrategicDestination()) return "—";
		IEntity leader = AICF_GroupRuntime.ResolveAliveLeader(group);
		if (!leader) return "—";
		vector origin = leader.GetOrigin();
		vector destination = slot.GetTargetPosition();
		SCR_CampaignMilitaryBaseComponent target = slot.GetTargetBase();
		if (slot.GetTargetKind() == AICF_EOrderTargetKind.BASE)
		{
			if (!target || !target.GetOwner()) return "—";
			destination = target.GetOwner().GetOrigin();
		}
		vector direction = vector.Direction(origin, destination);
		float bearing = Math.Atan2(direction[0], direction[2]) * Math.RAD2DEG;
		if (bearing < 0) bearing += 360;
		int sector = Math.Floor((bearing + 22.5) / 45.0);
		if (sector >= 8) sector = 0;
		string compass = "С";
		switch (sector)
		{
			case 1: compass = "СВ"; break;
			case 2: compass = "В"; break;
			case 3: compass = "ЮВ"; break;
			case 4: compass = "Ю"; break;
			case 5: compass = "ЮЗ"; break;
			case 6: compass = "З"; break;
			case 7: compass = "СЗ"; break;
		}
		return string.Format("%1 м · %2", Math.Round(vector.DistanceXZ(origin, destination)), compass);
	}

	// Краткое действие следует приказу и расстоянию; оно не утверждает,
	// что отряд прямо сейчас стреляет или имеет фактический путь до цели.
	protected string DescribeTask(
		AICF_FactionState factionState,
		AICF_GroupSlot slot,
		SCR_AIGroup group)
	{
		if (slot.IsAwaitingPlayerCommand() || slot.IsSystemHoldOrder())
			return "Ожидает приказа";
		if (slot.IsRecoveringFromStuck())
			return "Обходит препятствие";
		if (!slot.HasStrategicDestination()) return "Ожидает задачи";
		IEntity leader = AICF_GroupRuntime.ResolveAliveLeader(group);
		if (!leader) return "Позиция неизвестна";
		if (slot.GetTargetKind() == AICF_EOrderTargetKind.POSITION)
		{
			if (vector.DistanceSqXZ(leader.GetOrigin(), slot.GetTargetPosition()) <=
				POINT_AT_OBJECTIVE_RADIUS_METERS * POINT_AT_OBJECTIVE_RADIUS_METERS)
				return "Удерживает точку";
			return "Идёт к точке";
		}

		SCR_CampaignMilitaryBaseComponent target = slot.GetTargetBase();
		if (!target || !target.GetOwner()) return "Ожидает задачи";
		bool atObjective = vector.DistanceSqXZ(leader.GetOrigin(), target.GetOwner().GetOrigin()) <=
			AT_OBJECTIVE_RADIUS_METERS * AT_OBJECTIVE_RADIUS_METERS;
		switch (slot.GetRole())
		{
			case AICF_EGroupRole.ATTACK:
				Faction targetFaction = target.GetFaction();
				if (targetFaction && targetFaction.GetFactionKey() == factionState.GetFactionKey())
					return "Цель занята союзниками";
				if (atObjective) return "Захватывает цель";
				return "Движется к цели";
			case AICF_EGroupRole.DEFEND:
				if (slot.GetOperationalPosture() == "QRF")
				{
					if (atObjective) return "Усиливает оборону";
					return "Спешит на помощь";
				}
				if (atObjective) return "Обороняет цель";
				return "Занимает оборону";
			case AICF_EGroupRole.RESERVE:
				if (atObjective) return "Держит резерв";
				return "Следует в резерв";
		}
		return "Ожидает задачи";
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

	protected void SyncDestinationMarker(
		AICF_GroupSlot slot,
		int markerIndex,
		bool isUSSR,
		SCR_Faction markerFaction,
		FactionManager factionManager)
	{
		bool shouldExist = slot && slot.HasPlayerStrategicIntent() &&
			slot.GetPlayerStrategicIntentTargetKind() ==
				AICF_EOrderTargetKind.POSITION;
		if (!shouldExist)
		{
			RemoveDestinationMarker(markerIndex);
			return;
		}

		vector targetPosition = slot.GetPlayerStrategicIntentTargetPosition();
		int intentRevision = slot.GetPlayerStrategicIntentRevision();
		string slotKey = slot.GetSlotKey();
		if (m_aDestinationMarkers[markerIndex] &&
			m_aDestinationIntentRevisions[markerIndex] == intentRevision &&
			m_aDestinationPositions[markerIndex] == targetPosition &&
			m_aDestinationSlotKeys[markerIndex] == slotKey)
		{
			return;
		}

		RemoveDestinationMarker(markerIndex);
		EMilitarySymbolIdentity identity = EMilitarySymbolIdentity.BLUFOR;
		if (isUSSR)
			identity = EMilitarySymbolIdentity.OPFOR;
		SCR_MapMarkerBase marker = m_MarkerManager.PrepareMilitaryMarker(
			identity,
			EMilitarySymbolDimension.LAND,
			EMilitarySymbolIcon.INFANTRY);
		if (!marker)
			return;
		marker.SetWorldPos(
			Math.Round(targetPosition[0]),
			Math.Round(targetPosition[2]));
		marker.SetCustomText(string.Format("MOVE %1", slotKey));
		marker.SetCanBeRemovedByOwner(false);
		marker.AddMarkerFactionFlags(
			factionManager.GetFactionIndex(markerFaction));
		m_MarkerManager.InsertStaticMarker(marker, false, true);
		m_aDestinationMarkers[markerIndex] = marker;
		m_aDestinationIntentRevisions[markerIndex] = intentRevision;
		m_aDestinationPositions[markerIndex] = targetPosition;
		m_aDestinationSlotKeys[markerIndex] = slotKey;
		AICF_Stage4Diagnostics.Info(
			"PLAYER_POINT_MARKER_CREATED",
			string.Format(
				"faction=%1 slot=%2 stable_slot=%3 numeric_slot=%4 target_kind=POSITION target_x=%5 target_z=%6 intent_revision=%7 visibility=ALLIED jip=STATIC_SERVER_MARKER",
				markerFaction.GetFactionKey(),
				slot.GetSlotKey(),
				slot.GetStableSlotKey(),
				slot.GetSlotId(),
				Math.Round(targetPosition[0]),
				Math.Round(targetPosition[2]),
				intentRevision));
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
				slot.IsAwaitingPlayerCommand() || slot.IsSystemHoldOrder() ||
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

	protected void RemoveDestinationMarker(int markerIndex)
	{
		if (markerIndex < 0 || markerIndex >= m_aDestinationMarkers.Count())
			return;
		SCR_MapMarkerBase marker = m_aDestinationMarkers[markerIndex];
		if (marker && m_MarkerManager)
			m_MarkerManager.RemoveStaticMarker(marker);
		m_aDestinationMarkers[markerIndex] = null;
		m_aDestinationIntentRevisions[markerIndex] = -1;
		m_aDestinationPositions[markerIndex] = vector.Zero;
		m_aDestinationSlotKeys[markerIndex] = string.Empty;
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
