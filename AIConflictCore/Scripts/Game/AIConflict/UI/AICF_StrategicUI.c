enum AICF_EStrategicUIButtonAction
{
	TOGGLE_COMMAND = 0,
	CLOSE_COMMAND,
	SELECT_GROUP,
	ISSUE_TARGET,
	SET_ROLE,
	SET_UNIT_TYPE,
	ADJUST_SIZE
}

class AICF_StrategicUIButtonHandler : ScriptedWidgetEventHandler
{
	protected AICF_StrategicUIController m_Controller;
	protected AICF_EStrategicUIButtonAction m_Action;
	protected int m_iValue;

	void AICF_StrategicUIButtonHandler(
		AICF_StrategicUIController controller,
		AICF_EStrategicUIButtonAction action,
		int value = 0)
	{
		m_Controller = controller;
		m_Action = action;
		m_iValue = value;
	}

	override bool OnClick(Widget w, int x, int y, int button)
	{
		if (!m_Controller)
			return false;
		m_Controller.HandleButton(m_Action, m_iValue);
		return true;
	}
}

// Client-only presentation over the replicated SCR_GameModeCampaign state.
// The compact HUD is always read-only. The full command surface is attached to
// the stock map, which already owns cursor/input focus and a full-screen layer.
class AICF_StrategicUIController
{
	protected static const int UPDATE_INTERVAL_MS = 500;
	protected static const ResourceName FONT =
		"{3E7733BAC8C831F6}UI/Fonts/RobotoCondensed/RobotoCondensed_Regular.fnt";
	protected static const string RECT_BACKGROUND_NAME = "AICF_RectBackground";
	protected static const string RECT_INPUT_NAME = "AICF_RectInput";

	protected SCR_GameModeCampaign m_Campaign;
	protected Widget m_wHUDRoot;
	protected Widget m_wHUDAccent;
	protected TextWidget m_wHUDText;
	protected TextWidget m_wHUDObjective;
	protected Widget m_wMapToggle;
	protected Widget m_wMapToggleAccent;
	protected TextWidget m_wMapToggleText;
	protected Widget m_wCommandScrim;
	protected Widget m_wCommandPanel;
	protected Widget m_wCommandAccent;
	protected Widget m_wCloseButton;
	protected TextWidget m_wCommandOverview;
	protected TextWidget m_wCommandStatus;
	protected TextWidget m_wTargetTitle;
	protected TextWidget m_wTargetEmptyState;
	protected TextWidget m_wSizeValue;
	protected ref array<Widget> m_aGroupButtons = {};
	protected ref array<TextWidget> m_aGroupTexts = {};
	protected ref array<Widget> m_aRoleButtons = {};
	protected ref array<Widget> m_aUnitTypeButtons = {};
	protected ref array<Widget> m_aSizeButtons = {};
	protected ref array<Widget> m_aTargetButtons = {};
	protected ref array<ref AICF_StrategicUIButtonHandler> m_aHandlers = {};
	protected ref array<ref AICF_StrategicUIButtonHandler> m_aTargetHandlers = {};
	protected bool m_bStarted;
	protected bool m_bCommandOpen;
	protected bool m_bLocalUSSR;
	protected int m_iSelectedSlot;
	protected int m_iPendingConfigSlot = -1;
	protected int m_iPendingConfigRole;
	protected int m_iPendingConfigUnitType;
	protected int m_iPendingConfigSize;
	protected int m_iPendingConfigAtMs;
	protected string m_sRenderedTargets;
	protected string m_sRenderedTargetMode;

	void Start(SCR_GameModeCampaign campaign)
	{
		if (m_bStarted || !campaign || !GetGame().InPlayMode())
			return;
		m_bStarted = true;
		m_Campaign = campaign;
		SCR_MapEntity.GetOnMapOpenComplete().Insert(OnMapOpen);
		SCR_MapEntity.GetOnMapClose().Insert(OnMapClose);
		GetGame().GetCallqueue().CallLater(Update, UPDATE_INTERVAL_MS, true);
	}

	void Stop()
	{
		if (!m_bStarted)
			return;
		m_bStarted = false;
		GetGame().GetCallqueue().Remove(Update);
		SCR_MapEntity.GetOnMapOpenComplete().Remove(OnMapOpen);
		SCR_MapEntity.GetOnMapClose().Remove(OnMapClose);
		RemoveMapUI();
		if (m_wHUDRoot)
			m_wHUDRoot.RemoveFromHierarchy();
		m_wHUDRoot = null;
		m_wHUDAccent = null;
		m_wHUDText = null;
		m_wHUDObjective = null;
		m_Campaign = null;
	}

	void HandleButton(AICF_EStrategicUIButtonAction action, int value)
	{
		AICF_Stage4Diagnostics.Info(
			"STRATEGIC_UI_CLICK",
			string.Format(
				"action=%1 value=%2",
				typename.EnumToString(AICF_EStrategicUIButtonAction, action),
				value));
		switch (action)
		{
			case AICF_EStrategicUIButtonAction.TOGGLE_COMMAND:
				SetCommandOpen(!m_bCommandOpen);
				break;
			case AICF_EStrategicUIButtonAction.CLOSE_COMMAND:
				SetCommandOpen(false);
				break;
			case AICF_EStrategicUIButtonAction.SELECT_GROUP:
				m_iSelectedSlot = Math.ClampInt(
					value,
					0,
					AICF_Stage1Config.GROUP_SLOTS_PER_FACTION - 1);
				m_sRenderedTargetMode = string.Empty;
				RefreshCommandPanel();
				break;
			case AICF_EStrategicUIButtonAction.ISSUE_TARGET:
				IssueOrder(value);
				break;
			case AICF_EStrategicUIButtonAction.SET_ROLE:
				SubmitGroupConfiguration(value, -1, 0);
				break;
			case AICF_EStrategicUIButtonAction.SET_UNIT_TYPE:
				SubmitGroupConfiguration(-1, value, 0);
				break;
			case AICF_EStrategicUIButtonAction.ADJUST_SIZE:
				SubmitGroupConfiguration(-1, -1, value);
				break;
		}
	}

	protected void Update()
	{
		if (!m_bStarted || !m_Campaign)
			return;
		Faction localFaction = SCR_FactionManager.SGetLocalPlayerFaction();
		if (!localFaction)
		{
			if (m_wHUDRoot)
				m_wHUDRoot.SetVisible(false);
			return;
		}
		FactionKey key = localFaction.GetFactionKey();
		if (key != "US" && key != "USSR")
		{
			if (m_wHUDRoot)
				m_wHUDRoot.SetVisible(false);
			return;
		}
		m_bLocalUSSR = key == "USSR";
		EnsureHUD();
		RefreshHUD();
		if (m_wMapToggleText)
		{
			m_wMapToggleText.SetText(string.Format(
				"%1  [OPEN]",
				GetCommandAuthorityLabel()));
		}
		RefreshVisualStyles();
		if (m_bCommandOpen)
			RefreshCommandPanel();
	}

	protected void EnsureHUD()
	{
		if (m_wHUDRoot)
		{
			m_wHUDRoot.SetVisible(true);
			return;
		}
		SCR_HUDManagerComponent hudManager = SCR_HUDManagerComponent.GetHUDManager();
		if (!hudManager || !hudManager.GetHUDRootWidget())
			return;

		m_wHUDRoot = CreateRect(
			hudManager.GetHUDRootWidget(),
			0.015, 0.025, 0.275, 0.092,
			Color.FromSRGBA(5, 10, 14, 218),
			false);
		if (!m_wHUDRoot)
			return;
		m_wHUDRoot.SetName("AICF_StrategicHUD");
		m_wHUDRoot.SetZOrder(30);
		m_wHUDAccent = CreateRect(
			m_wHUDRoot,
			0, 0, 1, 0.045,
			Color.FromSRGBA(226, 167, 79, 255),
			false);
		m_wHUDText = CreateText(
			m_wHUDRoot,
			0.025, 0.12, 0.975, 0.52,
			"TICKETS  --    SUPPLY  --    SQUADS  --    TROOPS  --",
			16,
			Color.FromSRGBA(232, 241, 247, 255));
		m_wHUDObjective = CreateText(
			m_wHUDRoot,
			0.025, 0.52, 0.975, 0.94,
			"ORDERS // AWAITING ORDERS",
			16,
			Color.FromSRGBA(226, 167, 79, 255));
	}

	protected void RefreshHUD()
	{
		if (!m_wHUDText || !m_wHUDObjective)
			return;
		int tickets = m_Campaign.AICF_GetUSTickets();
		int totalSupplies = m_Campaign.AICF_GetUSTotalSupplies();
		int connectedSupplies = m_Campaign.AICF_GetUSConnectedSupplies();
		if (m_bLocalUSSR)
		{
			tickets = m_Campaign.AICF_GetUSSRTickets();
			totalSupplies = m_Campaign.AICF_GetUSSRTotalSupplies();
			connectedSupplies = m_Campaign.AICF_GetUSSRConnectedSupplies();
		}
		string supply = "SYNC";
		if (m_Campaign.AICF_GetStage4Enabled())
			supply = string.Format("%1/%2", connectedSupplies, totalSupplies);
		string objective = m_Campaign.AICF_GetStrategicObjective(m_bLocalUSSR);
		if (objective.IsEmpty())
			objective = "AWAITING ORDERS";
		m_wHUDText.SetText(string.Format(
			"TICKETS  %1    SUPPLY  %2    SQUADS  %3    TROOPS  %4",
			tickets,
			supply,
			m_Campaign.AICF_GetCombatGroups(m_bLocalUSSR),
			m_Campaign.AICF_GetManagedAgents(m_bLocalUSSR)));
		m_wHUDObjective.SetText(string.Format(
			"%1 // %2",
			GetCommandAuthorityLabel(),
			objective));
	}

	protected void OnMapOpen(MapConfiguration config)
	{
		RemoveMapUI();
		Faction localFaction = SCR_FactionManager.SGetLocalPlayerFaction();
		if (!localFaction)
			return;
		FactionKey localFactionKey = localFaction.GetFactionKey();
		if (localFactionKey != "US" && localFactionKey != "USSR")
			return;
		m_bLocalUSSR = localFactionKey == "USSR";

		SCR_MapEntity mapEntity = SCR_MapEntity.GetMapInstance();
		if (!mapEntity || !mapEntity.GetMapMenuRoot())
			return;
		Widget mapRoot = mapEntity.GetMapMenuRoot();

		m_wMapToggle = CreateRect(
			mapRoot,
			0.052, 0.025, 0.189, 0.075,
			Color.FromSRGBA(12, 25, 34, 238),
			true);
		m_wMapToggle.SetName("AICF_CommandToggle");
		m_wMapToggle.SetZOrder(220);
		m_wMapToggleText = CreateText(
			m_wMapToggle, 0, 0, 1, 1,
			string.Format("%1  [OPEN]", GetCommandAuthorityLabel()),
			20,
			Color.FromSRGBA(236, 242, 245, 255),
			true);
		m_wMapToggleAccent = CreateRect(
			m_wMapToggle,
			0, 0.92, 1, 1,
			Color.FromSRGBA(226, 167, 79, 255),
			false);
		AttachHandler(m_wMapToggle, AICF_EStrategicUIButtonAction.TOGGLE_COMMAND);

		m_wCommandScrim = CreateRect(
			mapRoot,
			0, 0, 1, 1,
			Color.FromSRGBA(0, 0, 0, 155),
			false);
		m_wCommandScrim.SetZOrder(240);

		m_wCommandPanel = CreateRect(
			mapRoot,
			0.055, 0.12, 0.945, 0.93,
			Color.FromSRGBA(4, 8, 11, 242),
			false);
		m_wCommandPanel.SetZOrder(250);
		m_wCommandAccent = CreateRect(
			m_wCommandPanel,
			0, 0, 1, 0.006,
			Color.FromSRGBA(226, 167, 79, 255),
			false);
		CreateText(
			m_wCommandPanel, 0.03, 0.018, 0.7, 0.088,
			"AI CONFLICT — COMMAND",
			26,
			Color.FromSRGBA(236, 244, 248, 255));

		m_wCloseButton = CreateRect(
			m_wCommandPanel,
			0.86, 0.022, 0.97, 0.082,
			Color.FromSRGBA(91, 34, 34, 240),
			true);
		CreateText(
			m_wCloseButton, 0, 0, 1, 1,
			"CLOSE  [X]", 18,
			Color.FromSRGBA(255, 232, 232, 255),
			true);
		AttachHandler(m_wCloseButton, AICF_EStrategicUIButtonAction.CLOSE_COMMAND);

		m_wCommandOverview = CreateText(
			m_wCommandPanel, 0.03, 0.082, 0.97, 0.158,
			string.Empty, 17, Color.FromSRGBA(202, 218, 227, 255));
		CreateText(
			m_wCommandPanel, 0.03, 0.16, 0.49, 0.205,
			"ARMY / SELECT GROUP",
			18, Color.FromSRGBA(116, 198, 239, 255));
		CreateText(
			m_wCommandPanel, 0.515, 0.16, 0.97, 0.205,
			"GROUP CONFIGURATION",
			18, Color.FromSRGBA(239, 187, 104, 255));

		for (int slotId = 0; slotId < AICF_Stage1Config.GROUP_SLOTS_PER_FACTION; slotId++)
		{
			int column = slotId % 2;
			int row = slotId / 2;
			float left = 0.03 + column * 0.23;
			float top = 0.21 + row * 0.12;
			Widget groupButton = CreateRect(
				m_wCommandPanel,
				left, top, left + 0.215, top + 0.098,
				Color.FromSRGBA(13, 28, 38, 232),
				true);
			TextWidget groupText = CreateText(
				groupButton, 0, 0, 1, 1,
				string.Empty, 14, Color.FromSRGBA(231, 239, 244, 255));
			FrameSlot.SetOffsets(groupText, 12, 4, -12, -4);
			m_aGroupButtons.Insert(groupButton);
			m_aGroupTexts.Insert(groupText);
			AttachHandler(groupButton, AICF_EStrategicUIButtonAction.SELECT_GROUP, slotId);
		}

		CreateText(
			m_wCommandPanel, 0.515, 0.21, 0.97, 0.245,
			"ROLE",
			14, Color.FromSRGBA(173, 190, 200, 255));
		array<string> roleLabels = {"ATTACK", "DEFEND", "RESERVE"};
		for (int roleIndex = 0; roleIndex < roleLabels.Count(); roleIndex++)
		{
			float roleLeft = 0.515 + roleIndex * 0.15;
			Widget roleButton = CreateRect(
				m_wCommandPanel,
				roleLeft, 0.247, roleLeft + 0.135, 0.298,
				Color.FromSRGBA(34, 45, 52, 245),
				true);
			CreateText(
				roleButton, 0, 0, 1, 1,
				roleLabels[roleIndex],
				14, Color.FromSRGBA(235, 241, 245, 255), true);
			m_aRoleButtons.Insert(roleButton);
			AttachHandler(roleButton, AICF_EStrategicUIButtonAction.SET_ROLE, roleIndex);
		}

		CreateText(
			m_wCommandPanel, 0.515, 0.312, 0.97, 0.347,
			"UNIT TYPE / NO HEAVY ARMOR",
			14, Color.FromSRGBA(173, 190, 200, 255));
		array<string> typeLabels = {"INFANTRY", "LIGHT 4X4", "TRUCK", "ARMED 4X4"};
		for (int typeIndex = 0; typeIndex < typeLabels.Count(); typeIndex++)
		{
			float typeLeft = 0.515 + typeIndex * 0.113;
			Widget typeButton = CreateRect(
				m_wCommandPanel,
				typeLeft, 0.349, typeLeft + 0.105, 0.4,
				Color.FromSRGBA(34, 45, 52, 245),
				true);
			CreateText(
				typeButton, 0, 0, 1, 1,
				typeLabels[typeIndex],
				14, Color.FromSRGBA(235, 241, 245, 255), true);
			m_aUnitTypeButtons.Insert(typeButton);
			AttachHandler(
				typeButton,
				AICF_EStrategicUIButtonAction.SET_UNIT_TYPE,
				typeIndex);
		}

		CreateText(
			m_wCommandPanel, 0.515, 0.414, 0.69, 0.449,
			"NEXT DEPLOYMENT SIZE",
			14, Color.FromSRGBA(173, 190, 200, 255));
		Widget sizeDown = CreateRect(
			m_wCommandPanel,
			0.705, 0.41, 0.76, 0.461,
			Color.FromSRGBA(34, 45, 52, 245),
			true);
		CreateText(
			sizeDown, 0, 0, 1, 1,
			"-", 20, Color.FromSRGBA(235, 241, 245, 255), true);
		m_aSizeButtons.Insert(sizeDown);
		AttachHandler(sizeDown, AICF_EStrategicUIButtonAction.ADJUST_SIZE, -1);
		m_wSizeValue = CreateText(
			m_wCommandPanel, 0.765, 0.41, 0.825, 0.461,
			"4", 18, Color.FromSRGBA(239, 187, 104, 255), true);
		Widget sizeUp = CreateRect(
			m_wCommandPanel,
			0.83, 0.41, 0.885, 0.461,
			Color.FromSRGBA(34, 45, 52, 245),
			true);
		CreateText(
			sizeUp, 0, 0, 1, 1,
			"+", 20, Color.FromSRGBA(235, 241, 245, 255), true);
		m_aSizeButtons.Insert(sizeUp);
		AttachHandler(sizeUp, AICF_EStrategicUIButtonAction.ADJUST_SIZE, 1);
		CreateText(
			m_wCommandPanel, 0.895, 0.41, 0.97, 0.461,
			"MAX 10", 13, Color.FromSRGBA(132, 149, 159, 255), true);

		m_wTargetTitle = CreateText(
			m_wCommandPanel, 0.515, 0.475, 0.97, 0.515,
			"ORDER TARGETS",
			17, Color.FromSRGBA(239, 187, 104, 255));
		m_wTargetEmptyState = CreateText(
			m_wCommandPanel, 0.515, 0.525, 0.97, 0.605,
			"SELECT A READY GROUP",
			15, Color.FromSRGBA(132, 149, 159, 255));

		m_wCommandStatus = CreateText(
			m_wCommandPanel, 0.03, 0.84, 0.97, 0.925,
			"HOW TO USE // SELECT GROUP   CONFIGURE ROLE / TYPE / NEXT SIZE   ISSUE TARGET",
			15, Color.FromSRGBA(173, 190, 200, 255));
		AICF_Stage4Diagnostics.Info(
			"STRATEGIC_UI_READY",
			string.Format("faction=%1 interaction=BUTTON_WIDGET", localFactionKey));
		SetCommandOpen(false);
	}

	protected void OnMapClose(MapConfiguration config)
	{
		RemoveMapUI();
	}

	protected void SetCommandOpen(bool open)
	{
		m_bCommandOpen = open && m_wCommandPanel;
		if (m_wCommandScrim)
			m_wCommandScrim.SetVisible(m_bCommandOpen);
		if (m_wCommandPanel)
			m_wCommandPanel.SetVisible(m_bCommandOpen);
		if (m_wMapToggle)
			m_wMapToggle.SetVisible(!m_bCommandOpen);
		RefreshVisualStyles();
		AICF_Stage4Diagnostics.Info(
			"STRATEGIC_UI_TOGGLED",
			string.Format("open=%1", m_bCommandOpen));
		if (m_bCommandOpen)
			RefreshCommandPanel();
	}

	protected void RefreshCommandPanel()
	{
		if (!m_wCommandPanel || !m_bCommandOpen || !m_Campaign)
			return;
		RefreshVisualStyles();
		int tier = m_Campaign.AICF_GetUSLogisticsTier();
		int pending = m_Campaign.AICF_GetUSPendingReinforcements();
		int shipments = m_Campaign.AICF_GetUSShipmentsInTransit();
		int totalSupplies = m_Campaign.AICF_GetUSTotalSupplies();
		int connectedSupplies = m_Campaign.AICF_GetUSConnectedSupplies();
		if (m_bLocalUSSR)
		{
			tier = m_Campaign.AICF_GetUSSRLogisticsTier();
			pending = m_Campaign.AICF_GetUSSRPendingReinforcements();
			shipments = m_Campaign.AICF_GetUSSRShipmentsInTransit();
			totalSupplies = m_Campaign.AICF_GetUSSRTotalSupplies();
			connectedSupplies = m_Campaign.AICF_GetUSSRConnectedSupplies();
		}
		string logistics = "ECONOMY SYNC";
		if (m_Campaign.AICF_GetStage4Enabled())
			logistics = GetTierName(tier);
		m_wCommandOverview.SetText(string.Format(
			"AUTHORITY  %1     OBJECTIVE  %2\nFORCE  %3 squads / %4 personnel     LOGISTICS  %5     SUPPLY  %6/%7     REINFORCEMENTS  %8     SHIPMENTS  %9",
			GetCommandAuthorityLabel(),
			m_Campaign.AICF_GetStrategicObjective(m_bLocalUSSR),
			m_Campaign.AICF_GetCombatGroups(m_bLocalUSSR),
			m_Campaign.AICF_GetManagedAgents(m_bLocalUSSR),
			logistics,
			connectedSupplies,
			totalSupplies,
			pending,
			shipments));

		for (int slotId = 0; slotId < m_aGroupTexts.Count(); slotId++)
		{
			string summary = m_Campaign.AICF_GetStrategicGroupSummary(m_bLocalUSSR, slotId);
			m_aGroupTexts[slotId].SetText(FormatGroupSummary(summary));
			if (slotId == m_iSelectedSlot)
				SetRectColor(m_aGroupButtons[slotId], Color.FromSRGBA(25, 92, 125, 245));
			else
				SetRectColor(m_aGroupButtons[slotId], Color.FromSRGBA(13, 28, 38, 232));
		}

		string selectedSummary = m_Campaign.AICF_GetStrategicGroupSummary(
			m_bLocalUSSR, m_iSelectedSlot);
		RefreshConfigurationButtons(selectedSummary);
		string targetMode = GetTargetMode(selectedSummary);
		string targets = m_Campaign.AICF_GetOrderTargets(m_bLocalUSSR);
		if (targets != m_sRenderedTargets || targetMode != m_sRenderedTargetMode)
		{
			m_sRenderedTargets = targets;
			m_sRenderedTargetMode = targetMode;
			RebuildTargetButtons(targets, targetMode);
		}
		m_wTargetTitle.SetText(string.Format(
			"ORDER TARGETS — %1 (%2)",
			GetSlotKey(selectedSummary),
			GetRoleName(selectedSummary)));
	}

	protected void RebuildTargetButtons(string encodedTargets, string targetMode)
	{
		foreach (Widget targetButton : m_aTargetButtons)
		{
			if (targetButton)
				targetButton.RemoveFromHierarchy();
		}
		m_aTargetButtons.Clear();
		m_aTargetHandlers.Clear();
		if (targetMode.IsEmpty())
		{
			if (m_wTargetEmptyState)
			{
				m_wTargetEmptyState.SetText("SELECT A READY GROUP");
				m_wTargetEmptyState.SetVisible(true);
			}
			return;
		}

		array<string> entries = {};
		encodedTargets.Split(";", entries, true);
		int visibleIndex;
		foreach (string entry : entries)
		{
			array<string> fields = {};
			entry.Split("~", fields, false);
			if (fields.Count() < 3 || fields[1] != targetMode)
				continue;

			int column = visibleIndex % 2;
			int row = visibleIndex / 2;
			if (visibleIndex >= 10)
				continue;
			float left = 0.515 + column * 0.23;
			float top = 0.525 + row * 0.06;
			Widget targetButton = CreateRect(
				m_wCommandPanel,
				left, top, left + 0.215, top + 0.05,
				Color.FromSRGBA(66, 48, 19, 238),
				true);
			CreateText(
				targetButton, 0, 0, 1, 1,
				string.Format("ORDER  >  %1", fields[2]),
				14, Color.FromSRGBA(255, 235, 199, 255), true);
			int callsign = fields[0].ToInt();
			AICF_StrategicUIButtonHandler handler = new AICF_StrategicUIButtonHandler(
				this, AICF_EStrategicUIButtonAction.ISSUE_TARGET, callsign);
			Widget inputWidget = GetRectInputWidget(targetButton);
			inputWidget.AddHandler(handler);
			m_aTargetHandlers.Insert(handler);
			m_aTargetButtons.Insert(targetButton);
			visibleIndex++;
		}
		if (m_wTargetEmptyState)
		{
			if (visibleIndex > 0)
			{
				m_wTargetEmptyState.SetVisible(false);
			}
			else
			{
				m_wTargetEmptyState.SetText("NO VALID TARGETS FOR THIS ROLE");
				m_wTargetEmptyState.SetVisible(true);
			}
		}
	}

	protected void IssueOrder(int targetCallsign)
	{
		SCR_PlayerController playerController = SCR_PlayerController.Cast(GetGame().GetPlayerController());
		if (!playerController)
		{
			if (m_wCommandStatus)
				m_wCommandStatus.SetText("ORDER NOT SENT: local player controller is unavailable.");
			return;
		}
		playerController.AICF_RequestStrategicOrder(m_iSelectedSlot, targetCallsign);
		if (m_wCommandStatus)
		{
			m_wCommandStatus.SetText(string.Format(
				"ORDER SENT: slot %1 -> base callsign %2. Awaiting authoritative state update.",
				m_iSelectedSlot,
				targetCallsign));
		}
	}

	protected void SubmitGroupConfiguration(
		int requestedRole,
		int requestedUnitType,
		int sizeDelta)
	{
		if (!m_Campaign)
			return;
		string summary = m_Campaign.AICF_GetStrategicGroupSummary(
			m_bLocalUSSR,
			m_iSelectedSlot);
		array<string> fields = {};
		summary.Split("|", fields, false);
		if (fields.Count() < 10)
		{
			if (m_wCommandStatus)
				m_wCommandStatus.SetText("CONFIG NOT SENT: selected group state is unavailable.");
			return;
		}

		int roleCode = GetRoleCode(fields[1]);
		int unitTypeCode = GetUnitTypeCode(fields[8]);
		int desiredSize = fields[9].ToInt();
		if (m_iPendingConfigSlot == m_iSelectedSlot &&
			System.GetTickCount(m_iPendingConfigAtMs) < 1500)
		{
			roleCode = m_iPendingConfigRole;
			unitTypeCode = m_iPendingConfigUnitType;
			desiredSize = m_iPendingConfigSize;
		}
		desiredSize = Math.ClampInt(
			desiredSize + sizeDelta,
			AICF_Stage1Config.MIN_GROUP_SIZE,
			AICF_Stage1Config.MAX_GROUP_SIZE);
		if (requestedRole >= 0)
			roleCode = requestedRole;
		if (requestedUnitType >= 0)
			unitTypeCode = requestedUnitType;

		SCR_PlayerController playerController = SCR_PlayerController.Cast(
			GetGame().GetPlayerController());
		if (!playerController)
		{
			if (m_wCommandStatus)
				m_wCommandStatus.SetText("CONFIG NOT SENT: local player controller is unavailable.");
			return;
		}

		playerController.AICF_RequestGroupConfiguration(
			m_iSelectedSlot,
			roleCode,
			unitTypeCode,
			desiredSize);
		m_iPendingConfigSlot = m_iSelectedSlot;
		m_iPendingConfigRole = roleCode;
		m_iPendingConfigUnitType = unitTypeCode;
		m_iPendingConfigSize = desiredSize;
		m_iPendingConfigAtMs = System.GetTickCount();
		if (m_wSizeValue)
			m_wSizeValue.SetText(desiredSize.ToString());
		m_sRenderedTargetMode = string.Empty;
		if (m_wCommandStatus)
		{
			m_wCommandStatus.SetText(string.Format(
				"CONFIG SENT: slot %1 role %2 type %3 next size %4. Awaiting server state.",
				m_iSelectedSlot,
				roleCode,
				unitTypeCode,
				desiredSize));
		}
	}

	protected void RefreshConfigurationButtons(string summary)
	{
		array<string> fields = {};
		summary.Split("|", fields, false);
		if (fields.Count() < 10)
			return;

		int roleCode = GetRoleCode(fields[1]);
		int unitTypeCode = GetUnitTypeCode(fields[8]);
		int desiredSize = fields[9].ToInt();
		if (m_iPendingConfigSlot == m_iSelectedSlot)
		{
			if (roleCode == m_iPendingConfigRole &&
				unitTypeCode == m_iPendingConfigUnitType &&
				desiredSize == m_iPendingConfigSize)
			{
				m_iPendingConfigSlot = -1;
			}
			else if (System.GetTickCount(m_iPendingConfigAtMs) < 1500)
			{
				roleCode = m_iPendingConfigRole;
				unitTypeCode = m_iPendingConfigUnitType;
				desiredSize = m_iPendingConfigSize;
			}
			else
			{
				m_iPendingConfigSlot = -1;
			}
		}
		for (int roleIndex = 0; roleIndex < m_aRoleButtons.Count(); roleIndex++)
		{
			if (roleIndex == roleCode)
				SetRectColor(m_aRoleButtons[roleIndex], Color.FromSRGBA(25, 92, 125, 250));
			else
				SetRectColor(m_aRoleButtons[roleIndex], Color.FromSRGBA(34, 45, 52, 245));
		}
		for (int typeIndex = 0; typeIndex < m_aUnitTypeButtons.Count(); typeIndex++)
		{
			if (typeIndex == unitTypeCode)
				SetRectColor(m_aUnitTypeButtons[typeIndex], Color.FromSRGBA(113, 72, 20, 250));
			else
				SetRectColor(m_aUnitTypeButtons[typeIndex], Color.FromSRGBA(34, 45, 52, 245));
		}
		if (m_wSizeValue)
			m_wSizeValue.SetText(desiredSize.ToString());
	}

	protected string FormatGroupSummary(string summary)
	{
		array<string> fields = {};
		summary.Split("|", fields, false);
		if (fields.Count() < 10)
			return "UNAVAILABLE";
		string result = string.Format(
			"%1  %2  %3  %4/%5",
			fields[0], fields[1], fields[2], fields[3], fields[9]);
		result += string.Format(
			"\n%1  |  %2\nЗАДАЧА %3  |  ТЕХНИКА %4  |  ПОПОЛН. %5",
			fields[8], fields[4], fields[5], fields[6], fields[7]);
		return result;
	}

	protected int GetRoleCode(string role)
	{
		if (role == "DEFEND") return AICF_EGroupRole.DEFEND;
		if (role == "RESERVE") return AICF_EGroupRole.RESERVE;
		return AICF_EGroupRole.ATTACK;
	}

	protected int GetUnitTypeCode(string unitType)
	{
		if (unitType == "MOTORIZED_LIGHT")
			return AICF_EGroupUnitType.MOTORIZED_LIGHT;
		if (unitType == "MOTORIZED_TRUCK")
			return AICF_EGroupUnitType.MOTORIZED_TRUCK;
		if (unitType == "MOTORIZED_ARMED_LIGHT")
			return AICF_EGroupUnitType.MOTORIZED_ARMED_LIGHT;
		return AICF_EGroupUnitType.INFANTRY;
	}

	protected string GetTargetMode(string summary)
	{
		array<string> fields = {};
		summary.Split("|", fields, false);
		if (fields.Count() < 3 ||
			(fields[2] != "READY" && fields[2] != "AWAITING_PLAYER_COMMAND"))
			return string.Empty;
		string role = GetRoleName(summary);
		if (role == "ATTACK") return "A";
		if (role == "DEFEND") return "D";
		if (role == "RESERVE") return "R";
		return string.Empty;
	}

	protected string GetCommandAuthorityLabel()
	{
		if (!m_Campaign || !m_Campaign.AICF_HasAICommanderState())
			return "COMMAND SYNC";
		if (m_Campaign.AICF_GetAICommanderEnabled(m_bLocalUSSR))
			return "AI COMMANDER";
		return "PLAYER COMMAND";
	}

	protected string GetSlotKey(string summary)
	{
		array<string> fields = {};
		summary.Split("|", fields, false);
		if (fields.IsEmpty())
			return "-";
		return fields[0];
	}

	protected string GetRoleName(string summary)
	{
		array<string> fields = {};
		summary.Split("|", fields, false);
		if (fields.Count() < 2)
			return "UNAVAILABLE";
		return fields[1];
	}

	protected string GetTierName(int tier)
	{
		switch (tier)
		{
			case AICF_ESupplyNetworkTier.HEALTHY: return "HEALTHY";
			case AICF_ESupplyNetworkTier.STRAINED: return "STRAINED";
			case AICF_ESupplyNetworkTier.ISOLATED: return "ISOLATED";
			case AICF_ESupplyNetworkTier.BLOCKED: return "BLOCKED";
		}
		return "UNKNOWN";
	}

	protected void RemoveMapUI()
	{
		m_bCommandOpen = false;
		if (m_wCommandPanel)
			m_wCommandPanel.RemoveFromHierarchy();
		if (m_wCommandScrim)
			m_wCommandScrim.RemoveFromHierarchy();
		if (m_wMapToggle)
			m_wMapToggle.RemoveFromHierarchy();
		m_wCommandPanel = null;
		m_wMapToggle = null;
		m_wMapToggleAccent = null;
		m_wMapToggleText = null;
		m_wCommandScrim = null;
		m_wCommandAccent = null;
		m_wCloseButton = null;
		m_wCommandOverview = null;
		m_wCommandStatus = null;
		m_wTargetTitle = null;
		m_wTargetEmptyState = null;
		m_wSizeValue = null;
		m_aGroupButtons.Clear();
		m_aGroupTexts.Clear();
		m_aRoleButtons.Clear();
		m_aUnitTypeButtons.Clear();
		m_aSizeButtons.Clear();
		m_aTargetButtons.Clear();
		m_aHandlers.Clear();
		m_aTargetHandlers.Clear();
		m_iPendingConfigSlot = -1;
		m_sRenderedTargets = string.Empty;
		m_sRenderedTargetMode = string.Empty;
	}

	protected void RefreshVisualStyles()
	{
		SetRectColor(m_wHUDRoot, Color.FromSRGBA(5, 10, 14, 235));
		SetRectColor(m_wHUDAccent, Color.FromSRGBA(226, 167, 79, 255));
		SetRectColor(m_wMapToggle, Color.FromSRGBA(12, 25, 34, 248));
		SetRectColor(m_wMapToggleAccent, Color.FromSRGBA(226, 167, 79, 255));
		SetRectColor(m_wCommandScrim, Color.FromSRGBA(0, 0, 0, 165));
		SetRectColor(m_wCommandPanel, Color.FromSRGBA(4, 8, 11, 250));
		SetRectColor(m_wCommandAccent, Color.FromSRGBA(226, 167, 79, 255));
		SetRectColor(m_wCloseButton, Color.FromSRGBA(91, 34, 34, 250));
		foreach (Widget roleButton : m_aRoleButtons)
			SetRectColor(roleButton, Color.FromSRGBA(34, 45, 52, 245));
		foreach (Widget unitTypeButton : m_aUnitTypeButtons)
			SetRectColor(unitTypeButton, Color.FromSRGBA(34, 45, 52, 245));
		foreach (Widget sizeButton : m_aSizeButtons)
			SetRectColor(sizeButton, Color.FromSRGBA(34, 45, 52, 245));
		foreach (Widget targetButton : m_aTargetButtons)
			SetRectColor(targetButton, Color.FromSRGBA(66, 48, 19, 248));
	}

	protected void AttachHandler(
		Widget widget,
		AICF_EStrategicUIButtonAction action,
		int value = 0)
	{
		if (!widget)
			return;
		AICF_StrategicUIButtonHandler handler = new AICF_StrategicUIButtonHandler(
			this, action, value);
		Widget inputWidget = GetRectInputWidget(widget);
		inputWidget.AddHandler(handler);
		m_aHandlers.Insert(handler);
	}

	protected Widget GetRectInputWidget(Widget widget)
	{
		if (!widget)
			return null;
		Widget inputWidget = widget.FindAnyWidget(RECT_INPUT_NAME);
		if (!inputWidget)
			inputWidget = widget;
		inputWidget.ClearFlags(WidgetFlags.IGNORE_CURSOR);
		inputWidget.SetOpacity(0);
		return inputWidget;
	}

	protected Widget CreateRect(
		Widget parent,
		float left,
		float top,
		float right,
		float bottom,
		Color color,
		bool clickable)
	{
		if (!parent)
			return null;
		WidgetFlags flags = WidgetFlags.VISIBLE;
		if (!clickable)
			flags = SCR_Enum.SetFlag(flags, WidgetFlags.IGNORE_CURSOR);
		Widget widget = GetGame().GetWorkspace().CreateWidget(
			WidgetType.FrameWidgetTypeID,
			flags,
			Color.FromSRGBA(255, 255, 255, 0),
			0,
			parent);
		if (!widget)
			return null;
		FrameSlot.SetAnchorMin(widget, left, top);
		FrameSlot.SetAnchorMax(widget, right, bottom);
		FrameSlot.SetOffsets(widget, 0, 0, 0, 0);

		ImageWidget background = ImageWidget.Cast(GetGame().GetWorkspace().CreateWidget(
			WidgetType.ImageWidgetTypeID,
			WidgetFlags.VISIBLE | WidgetFlags.IGNORE_CURSOR |
				WidgetFlags.BLEND | WidgetFlags.STRETCH | WidgetFlags.NOWRAP,
			color,
			0,
			widget));
		if (!background)
		{
			widget.RemoveFromHierarchy();
			return null;
		}
		background.SetName(RECT_BACKGROUND_NAME);
		FrameSlot.SetAnchorMin(background, 0, 0);
		FrameSlot.SetAnchorMax(background, 1, 1);
		FrameSlot.SetOffsets(background, 0, 0, 0, 0);

		if (clickable)
		{
			ButtonWidget inputWidget = ButtonWidget.Cast(
				GetGame().GetWorkspace().CreateWidget(
					WidgetType.ButtonWidgetTypeID,
					WidgetFlags.VISIBLE | WidgetFlags.BLEND,
					Color.FromSRGBA(255, 255, 255, 0),
					1,
					widget));
			if (!inputWidget)
			{
				widget.RemoveFromHierarchy();
				return null;
			}
			inputWidget.SetName(RECT_INPUT_NAME);
			inputWidget.SetColor(Color.FromSRGBA(255, 255, 255, 0));
			inputWidget.SetOpacity(0);
			FrameSlot.SetAnchorMin(inputWidget, 0, 0);
			FrameSlot.SetAnchorMax(inputWidget, 1, 1);
			FrameSlot.SetOffsets(inputWidget, 0, 0, 0, 0);
		}
		return widget;
	}

	protected void SetRectColor(Widget widget, Color color)
	{
		if (!widget)
			return;
		ImageWidget background = ImageWidget.Cast(
			widget.FindAnyWidget(RECT_BACKGROUND_NAME));
		if (background)
			background.SetColor(color);
	}

	protected TextWidget CreateText(
		Widget parent,
		float left,
		float top,
		float right,
		float bottom,
		string text,
		int fontSize,
		Color color,
		bool centered = false)
	{
		if (!parent)
			return null;
		WidgetFlags flags = WidgetFlags.VISIBLE | WidgetFlags.IGNORE_CURSOR |
			WidgetFlags.BLEND | WidgetFlags.VCENTER | WidgetFlags.NO_LOCALIZATION;
		if (centered)
			flags = SCR_Enum.SetFlag(flags, WidgetFlags.CENTER);
		TextWidget widget = TextWidget.Cast(GetGame().GetWorkspace().CreateWidget(
			WidgetType.TextWidgetTypeID,
			flags,
			color,
			2,
			parent));
		if (!widget)
			return null;
		widget.SetFlags(flags);
		FrameSlot.SetAnchorMin(widget, left, top);
		FrameSlot.SetAnchorMax(widget, right, bottom);
		FrameSlot.SetOffsets(widget, 0, 0, 0, 0);
		widget.SetColor(color);
		widget.SetFont(FONT);
		widget.SetExactFontSize(fontSize);
		widget.SetText(text);
		return widget;
	}
}
