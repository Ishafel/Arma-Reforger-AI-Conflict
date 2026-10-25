// Hidden widget tree + настоящие owner RPC. Не открывает карту/окна,
// не захватывает focus и не имитирует физический ввод.
class AICF_VehiclePresetProbeUIModel : AICF_StrategicUIController
{
	protected int m_iSliderRequest = -1;
	override protected void SubmitGroupConfiguration(int requestedRole, int requestedUnitType, int sizeDelta, int absoluteSize = -1)
	{
		m_iSliderRequest = absoluteSize;
	}
	bool CheckCommanderControls(Widget hidden)
	{
		m_wCommandPanel = CreateRect(hidden, 0, 0, 1, 1, Color.Black, false);
		CreateSizeSlider();
		if (!m_SizeSlider || !m_wSizeValue) return false;
		m_SizeSlider.SetValue(0);
		bool lower = m_SizeSlider.GetValue() == 1;
		m_SizeSlider.SetValue(11);
		bool upper = m_SizeSlider.GetValue() == 10;
		OnSizeChanged(m_SizeSlider, 7);
		RefreshConfigurationButtons("A0|ATTACK|READY|1|NONE|NONE|0|0|INFANTRY|4");
		bool drag = m_bDraggingSizeSlider && m_wSizeValue.GetText() == "7" && m_SizeSlider.GetValue() == 10;
		OnSizeFinal(m_SizeSlider, 7);
		bool request = m_iSliderRequest == 7 && !m_bDraggingSizeSlider;
		Widget button = CreateRect(m_wCommandPanel, 0, 0, 1, 1, Color.Black, true);
		AttachHandler(button, AICF_EStrategicUIButtonAction.CLOSE_COMMAND);
		AICF_StrategicUIButtonHandler handler = m_aHandlers[m_aHandlers.Count() - 1];
		ImageWidget background = ImageWidget.Cast(button.FindAnyWidget("AICF_RectBackground"));
		handler.OnMouseEnter(button, 0, 0);
		int hover = background.GetColor().PackToInt();
		SetRectColor(button, Color.Black);
		bool held = hover == background.GetColor().PackToInt();
		handler.OnMouseLeave(button, null, 0, 0);
		bool restored = background.GetColor().PackToInt() != hover;
		RemoveMapUI();
		return lower && upper && drag && request && held && restored;
	}

	override bool GetLoadoutGroup(int slotId, out string name, out int size, string targetFaction = "")
	{
		name = "Alpha";
		size = 1;
		return slotId == 0;
	}
}

class AICF_VehiclePresetProbeEditor : AICF_VehiclePresetEditor
{
	protected int m_iProbeStep;
	protected bool m_bProbeDone;
	protected bool m_bProbeJIP;
	protected int m_iProbeChecks;
	protected int m_iProbeFailures;
	protected int m_iProbeExpectedRevision;
	protected bool m_bProbeCatalogFilters;
	protected ResourceName m_sProbePrefab;
	void AICF_VehiclePresetProbeEditor(AICF_StrategicUIController controller) { m_Controller = controller; }
	bool Done() { return m_bProbeDone; }
	int Failures() { return m_iProbeFailures; }

	protected void Check(string name, bool passed)
	{
		m_iProbeChecks++;
		if (!passed) m_iProbeFailures++;
		Print(string.Format("[AICF][VEHICLE_UI_CHECK] rule=%1 passed=%2", name, passed));
	}

	void Build(Widget hidden, SCR_PlayerController player, bool jip, bool noRightsPassed)
	{
		m_Player = player;
		m_bProbeJIP = jip;
		string catalogFlag;
		m_bProbeCatalogFilters = System.GetCLIParam("aicfVehicleCatalogFilters", catalogFlag);
		if (!jip) Check("NO_FACTION_REJECTED", noRightsPassed);
		Check("ALPHA_VEHICLE_FORM", Create(hidden, 0) && !m_wRoot.IsVisibleInHierarchy());
		Request(AICF_EVehiclePresetOperation.READ, "");
	}

	protected void Click(string name)
	{
		foreach (Widget button : m_aButtons)
			if (button.GetName() == name) { OnClick(button, 0, 0, SCR_EMouseButtons.LEFT); return; }
		Check("BUTTON_" + name, false);
	}

	protected void Filter(int source, int category)
	{
		m_bRendering = true;
		m_SourceFilter.SetCurrentItem(source, false, false, false);
		m_CategoryFilter.SetCurrentItem(category, false, false, false);
		m_bRendering = false;
		OnChanged(m_SourceFilter, source);
	}

	protected bool VisibleMatches(int source, int category)
	{
		if (m_aVisibleVehicles.IsEmpty()) return false;
		foreach (int index : m_aVisibleVehicles)
			if ((source > 0 && m_aCatalogSources[index] != source) || (category > 0 && m_aCatalogCategories[index] != category)) return false;
		return true;
	}

	void Tick()
	{
		Refresh();
		if (m_bProbeDone || m_bPending || m_bQueued || m_sView.IsEmpty() || !m_Player) return;
		JsonLoadContext json = new JsonLoadContext();
		string assigned, faction;
		int slot;
		json.LoadFromString(m_sView);
		json.ReadValue("assigned", assigned);
		json.ReadValue("faction", faction);
		json.ReadValue("slot", slot);
		string crewFlag;
		if (System.GetCLIParam("aicfVehicleCrewReturnProbe", crewFlag)) { TickCrew(assigned); return; }
		if (m_bProbeJIP)
		{
			AICF_VehiclePreset confirmed = AICF_VehiclePreset.Decode(assigned);
			Check("JIP_CONFIRMED_ASSIGNMENT", confirmed && confirmed.m_sFaction == "USSR" && confirmed.m_sName == "UI vehicle" && confirmed.m_sPrefab.Contains("UAZ469"));
			Check("JIP_STABLE_SLOT", slot == 0 && m_iRevision > 0 && faction == "USSR");
			if (m_bProbeCatalogFilters)
			{
				int confirmedIndex = m_aPrefabs.Find(confirmed.m_sPrefab);
				Check("JIP_CIVILIAN_AVAILABLE", confirmedIndex >= 0 && m_aCatalogSources[confirmedIndex] == AICF_EVehicleCatalogSource.CIVILIAN);
				Filter(AICF_EVehicleCatalogSource.FACTION, AICF_EVehicleCatalogCategory.TRUCKS);
				RevealDraft();
				Check("JIP_FILTERED_SELECTION", SelectedPrefab() == confirmed.m_sPrefab && m_SourceFilter.GetCurrentIndex() == AICF_EVehicleCatalogSource.CIVILIAN);
			}
			Finish();
			return;
		}
		switch (m_iProbeStep)
		{
			case 0:
				Check("CONTEXT", slot == 0 && faction == "USSR");
				if (m_bProbeCatalogFilters)
				{
					Check("ALL_NATIVE_CATALOG", m_aVisibleVehicles.Count() == m_aPrefabs.Count());
					Filter(AICF_EVehicleCatalogSource.CIVILIAN, AICF_EVehicleCatalogCategory.ALL);
					Check("CIVILIAN_FILTER", VisibleMatches(AICF_EVehicleCatalogSource.CIVILIAN, 0));
					Filter(AICF_EVehicleCatalogSource.CIVILIAN, AICF_EVehicleCatalogCategory.CARS);
					Check("CAR_CATEGORY_FILTER", VisibleMatches(AICF_EVehicleCatalogSource.CIVILIAN, AICF_EVehicleCatalogCategory.CARS));
					Filter(AICF_EVehicleCatalogSource.FACTION, AICF_EVehicleCatalogCategory.ALL);
					Check("FACTION_FILTER", VisibleMatches(AICF_EVehicleCatalogSource.FACTION, 0));
					Filter(AICF_EVehicleCatalogSource.CIVILIAN, AICF_EVehicleCatalogCategory.ARMOR);
					Check("EMPTY_FILTER_SAFE", m_aVisibleVehicles.IsEmpty() && SelectedPrefab().IsEmpty());
					int emptyToken = m_iToken;
					Click("select");
					Check("EMPTY_SELECT_NO_RPC", m_iToken == emptyToken && !m_bQueued && !m_bPending);
					Filter(AICF_EVehicleCatalogSource.CIVILIAN, AICF_EVehicleCatalogCategory.CARS);
				}
				int selected = -1;
				for (int i; i < m_aPrefabs.Count(); i++)
				{
					if (m_bProbeCatalogFilters)
					{
						if (m_aPrefabs[i].Contains("UAZ469_uncovered_CIV_blue.et")) selected = i;
					}
					else if (m_aPrefabs[i].Contains("UAZ469_uncovered.et")) selected = i;
				}
				Check("UAZ_WHITELIST", selected >= 0);
				if (selected < 0) { Finish(); return; }
				m_sProbePrefab = m_aPrefabs[selected];
				m_Catalog.SetCurrentItem(m_aVisibleVehicles.Find(selected), false, false, false);
				if (m_bProbeCatalogFilters) Check("FILTERED_PREFAB_MAPPING", SelectedPrefab() == m_sProbePrefab);
				Click("select");
				break;
			case 1:
				Check("SELECT_HANDLER", m_Player.AICF_VehiclePresetAccepted() && m_Draft && m_Draft.m_sPrefab == m_sProbePrefab);
				if (!m_Draft) { Finish(); return; }
				Check("NATIVE_SEAT_LABELS", m_aSeatNames.Count() == m_Draft.m_aSeats.Count());
				int driver = -1;
				for (int seat; seat < m_Draft.m_aSeats.Count(); seat++)
				{
					array<string> parts = {};
					m_Draft.m_aSeats[seat].Split(":", parts, false);
					if (parts[2].ToInt() == ECompartmentType.PILOT) driver = seat;
					Check("NO_FAKE_TURRET_" + seat.ToString(), parts[2].ToInt() != ECompartmentType.TURRET);
				}
				Check("DRIVER_SEAT", driver >= 0 && driver < 8);
				if (driver < 0 || driver >= 8) { Finish(); return; }
				OnChanged(m_aSeats[driver], 1);
				Check("ASSIGN_HANDLER", m_Draft.m_aMembers[driver] == 0);
				m_wName.SetText("UI vehicle");
				m_iProbeExpectedRevision = m_iRevision;
				Click("save");
				break;
			case 2:
				Check("SAVE_HANDLER", m_Player.AICF_VehiclePresetAccepted() && m_iRevision == m_iProbeExpectedRevision && assigned.IsEmpty() && m_aLibraryIds.Count() > 0);
				Click("apply");
				break;
			case 3:
				Check("APPLY_HANDLER", m_Player.AICF_VehiclePresetAccepted() && m_iRevision == m_iProbeExpectedRevision + 1 && AICF_VehiclePreset.Decode(assigned));
				m_iProbeExpectedRevision = m_iRevision;
				Click("apply");
				break;
			case 4:
				Check("REPEAT_APPLY_IDEMPOTENT", m_Player.AICF_VehiclePresetAccepted() && m_iRevision == m_iProbeExpectedRevision);
				m_iToken = m_Player.AICF_RequestVehiclePreset(m_iSlot, m_iRevision - 1, m_iGeneration, m_iLibraryRevision, AICF_EVehiclePresetOperation.APPLY, m_Draft.Encode());
				m_bPending = true;
				break;
			case 5:
				Check("STALE_REVISION_REJECTED", !m_Player.AICF_VehiclePresetAccepted() && m_iRevision == m_iProbeExpectedRevision && m_Player.AICF_VehiclePresetReason() == "REVISION_CONFLICT");
				AICF_VehiclePreset foreign = AICF_VehiclePreset.Decode(m_Draft.Encode());
				foreign.m_sFaction = "US";
				Request(AICF_EVehiclePresetOperation.APPLY, foreign.Encode());
				break;
			case 6:
				Check("FOREIGN_FACTION_REJECTED", !m_Player.AICF_VehiclePresetAccepted() && m_iRevision == m_iProbeExpectedRevision);
				if (m_bProbeCatalogFilters) Filter(AICF_EVehicleCatalogSource.FACTION, AICF_EVehicleCatalogCategory.TRUCKS);
				m_Library.SetCurrentItem(0, false, false, false);
				Click("load");
				break;
			case 7:
				Check("LOAD_HANDLER", m_Player.AICF_VehiclePresetAccepted() && m_Draft && m_Draft.m_sName == "UI vehicle" && m_iRevision == m_iProbeExpectedRevision);
				if (m_bProbeCatalogFilters) Check("LOAD_REVEALS_DRAFT", SelectedPrefab() == m_sProbePrefab && m_SourceFilter.GetCurrentIndex() == AICF_EVehicleCatalogSource.CIVILIAN);
				m_Library.SetCurrentItem(0, false, false, false);
				Click("delete");
				break;
			case 8:
				Check("DELETE_HANDLER", m_Player.AICF_VehiclePresetAccepted() && m_aLibraryIds.IsEmpty() && !assigned.IsEmpty() && m_iRevision == m_iProbeExpectedRevision);
				Finish();
				return;
		}
		m_iProbeStep++;
	}

	protected void CheckPresentation()
	{
		RefreshPreview();
		Check("COMMANDER_SLIDER_AND_PERSISTENT_HOVER", AICF_VehiclePresetProbeUIModel.Cast(m_Controller).CheckCommanderControls(m_wRoot.GetParent()));
		Check("PRIVATE_VEHICLE_PREVIEW", m_Preview && m_Preview.IsReady() && m_Preview.GetWorld() != GetGame().GetWorld() && m_wPreview.IsVisible());
		Check("UNSEATED_TEXT_RESOLVED", !m_wUnseated.GetText().Contains("{AICF:"));
		Widget button = m_aButtons[0];
		ImageWidget background = ImageWidget.Cast(m_aButtonFrames[0].FindAnyWidget("AICF_RectBackground"));
		int normal = background.GetColor().PackToInt();
		OnMouseEnter(button, 0, 0);
		Check("BUTTON_HOVER_COLOR", normal != background.GetColor().PackToInt());
		OnMouseLeave(button, null, 0, 0);
		Check("BUTTON_HOVER_RESTORED", normal == background.GetColor().PackToInt());
	}

	protected void TickCrew(string assigned)
	{
		switch (m_iProbeStep)
		{
			case 0:
				if (m_aActiveMembers.Count() != 10) return;
				for (int index; index < m_aPrefabs.Count(); index++)
					if (m_aPrefabs[index].Contains("UAZ469_uncovered_CIV_blue.et")) { m_sProbePrefab = m_aPrefabs[index]; m_Catalog.SetCurrentItem(m_aVisibleVehicles.Find(index), false, false, false); }
				Check("GROUP10_READY", m_iSize == 10 && !m_sProbePrefab.IsEmpty());
				Click("select");
				break;
			case 1:
				Check("UAZ_SELECTED", m_Draft && m_Draft.m_sPrefab == m_sProbePrefab);
				if (!m_Draft) { Finish(); return; }
				CheckPresentation();
				for (int seat; seat < m_Draft.m_aSeats.Count(); seat++)
				{
					array<string> parts = {};
					m_Draft.m_aSeats[seat].Split(":", parts, false);
					if (parts[2].ToInt() == ECompartmentType.PILOT) OnChanged(m_aSeats[seat], 10);
				}
				m_wName.SetText("Nonprefix driver");
				Click("reset");
				break;
			case 2:
				Check("AUTOMATIC_PRESERVES_UNSAVED_SEATS", m_Draft && m_Draft.m_aMembers.Contains(9) && m_wName.GetText() == "Nonprefix driver" && assigned.IsEmpty() && m_aSeatLabels[0].IsVisible());
				Click("apply");
				break;
			case 3:
				Check("CONFIRMATION_WITHOUT_SIDE_EFFECTS", !m_Player.AICF_VehiclePresetAccepted() && m_iConfirmation > 0 && m_iSize == 10 && m_aActiveMembers.Count() == 10 && assigned.IsEmpty());
				Click("cancelCrew");
				break;
			case 4:
				Check("CANCEL_PRESERVES_SQUAD_AND_DRAFT", m_iConfirmation == 0 && m_iSize == 10 && m_aActiveMembers.Count() == 10 && assigned.IsEmpty() && m_Draft.m_aMembers.Contains(9));
				Click("apply");
				break;
			case 5:
				Check("CONFIRM_AGAIN", m_iConfirmation > 0 && m_wConfirmation.IsVisible());
				Request(AICF_EVehiclePresetOperation.CONFIRM_CREW, (m_iConfirmation + 1).ToString());
				break;
			case 6:
				Check("WRONG_CONFIRMATION_REFUSED", !m_Player.AICF_VehiclePresetAccepted() && m_iConfirmation == 0 && m_iSize == 10 && assigned.IsEmpty());
				Click("apply");
				break;
			case 7:
				Click("confirmCrew");
				break;
			case 8:
				AICF_VehiclePreset applied = AICF_VehiclePreset.Decode(assigned);
				Check("CONFIRMED_10_TO_1", m_Player.AICF_VehiclePresetAccepted() && m_iConfirmation == 0 && m_iSize == 1 && m_aActiveMembers.Count() == 1 && applied && applied.m_aMembers.Contains(0) && !applied.m_aMembers.Contains(9));
				Finish();
				return;
		}
		m_iProbeStep++;
	}

	protected void Finish()
	{
		m_bProbeDone = true;
		Print(string.Format("[AICF][VEHICLE_UI_DONE] checks=%1 failures=%2 jip=%3", m_iProbeChecks, m_iProbeFailures, m_bProbeJIP));
		Close();
	}
}

modded class SCR_PlayerController
{
	void AICF_VehicleProbeClientFinished(int failures, bool jip) { Rpc(RpcAsk_AICFVehicleProbeClientFinished, failures, jip); }
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_AICFVehicleProbeClientFinished(int failures, bool jip)
	{
		string flag;
		if (!Replication.IsServer() || !System.GetCLIParam("aicfVehiclePresetClient", flag)) return;
		Print(string.Format("[AICF][VEHICLE_UI_SERVER_READBACK] player=%1 failures=%2 jip=%3", GetPlayerId(), failures, jip));
		SCR_GameModeCampaign campaign = SCR_GameModeCampaign.Cast(GetGame().GetGameMode());
		if (jip && campaign) campaign.AICF_VehicleProbeScheduleServerClose();
	}
}

modded class AICF_MatchController
{
	protected bool m_bVehicleClientConfigured;
	override protected void Update()
	{
		super.Update();
		string flag;
		if (!m_bRosterReady || !System.GetCLIParam("aicfVehiclePresetClient", flag)) return;
		array<int> connected = {};
		GetGame().GetPlayerManager().GetPlayers(connected);
		foreach (int playerId : connected)
		{
			SCR_PlayerController player = SCR_PlayerController.Cast(GetGame().GetPlayerManager().GetPlayerController(playerId));
			if (!player || player.GetControlledEntity() || SCR_FactionManager.SGetPlayerFaction(playerId) != m_USSRFaction) continue;
			// Реальный avatar даёт cleanup проверяемую позицию игрока.
			// Передаём существующего бойца другого отряда, новый crew не создаётся.
			for (int index = 9; index > 0; index--)
			{
				IEntity member = m_USSRState.GetSlot(index).GetRosterMember(0);
				if (!AICF_VehicleBoardingMutationFence.IsAuthoritativeAIEntity(member)) continue;
				player.SetInitialMainEntity(member);
				Print(string.Format("[AICF][VEHICLE_UI_AVATAR] player=%1 member=%2", playerId, member.GetID()));
				break;
			}
		}
		string crewFlag;
		if (System.GetCLIParam("aicfVehicleCrewReturnProbe", crewFlag)) return;
		if (m_bVehicleClientConfigured) return;
		m_bVehicleClientConfigured = true;
		AICF_GroupSlot slot = m_USSRState.GetSlot(0);
		slot.SetDesiredSize(1);
		m_OrderPlanner.AssignPlayerPointOrder(slot, m_USSRFaction, AICF_GroupRuntime.ResolveAliveLeader(slot.GetGroup()).GetOrigin() + "400 0 0", false);
	}
}

modded class SCR_GameModeCampaign
{
	void AICF_VehicleProbeScheduleServerClose()
	{
		GetGame().GetCallqueue().CallLater(AICF_VehicleUIClose, 10000, false);
	}
	protected ref AICF_VehiclePresetProbeEditor m_VehicleUIProbe;
	protected ref AICF_VehiclePresetProbeUIModel m_VehicleUIModel;
	protected Widget m_wVehicleUIHidden;
	protected int m_iVehicleUITime;
	protected int m_iVehicleRightsToken;
	protected bool m_bVehicleRightsChecked;
	protected bool m_bVehicleRightsPassed;
	override void OnGameStart()
	{
		super.OnGameStart();
		string flag;
		if (Replication.IsServer() || !System.GetCLIParam("aicfVehiclePresetClient", flag)) return;
		m_iVehicleUITime = System.GetTickCount();
		GetGame().GetCallqueue().CallLater(AICF_VehicleUITick, 1200, true);
	}
	protected void AICF_VehicleUITick()
	{
		if (System.GetTickCount(m_iVehicleUITime) > 180000) { Print("[AICF][VEHICLE_UI_TIMEOUT] failed=1"); GetGame().RequestClose(); return; }
		SCR_PlayerController player = SCR_PlayerController.Cast(GetGame().GetPlayerController());
		if (!player) return;
		Faction faction = GetGame().GetFactionManager().GetFactionByKey(AICF_ContentProfile.GetActive().GetRuntimeFactionKey("USSR"));
		SCR_PlayerFactionAffiliationComponent affiliation = SCR_PlayerFactionAffiliationComponent.Cast(player.FindComponent(SCR_PlayerFactionAffiliationComponent));
		if (!affiliation || !faction) return;
		if (!m_bVehicleRightsChecked)
		{
			if (m_iVehicleRightsToken > 0)
			{
				if (player.AICF_VehiclePresetToken() != m_iVehicleRightsToken) return;
				m_bVehicleRightsPassed = !player.AICF_VehiclePresetAccepted() && player.AICF_VehiclePresetView().IsEmpty();
				m_bVehicleRightsChecked = true;
			}
			else if (!affiliation.GetAffiliatedFaction())
			{
				m_iVehicleRightsToken = player.AICF_RequestVehiclePreset(0, -1, -1, -1, AICF_EVehiclePresetOperation.READ, "");
				return;
			}
			else m_bVehicleRightsChecked = true;
		}
		if (affiliation.GetAffiliatedFaction() != faction) { affiliation.RequestFaction(faction); return; }
		if (!m_VehicleUIProbe)
		{
			m_wVehicleUIHidden = GetGame().GetWorkspace().CreateWidget(WidgetType.FrameWidgetTypeID, WidgetFlags.IGNORE_CURSOR, Color.White, 0);
			m_wVehicleUIHidden.SetVisible(false);
			m_VehicleUIModel = new AICF_VehiclePresetProbeUIModel();
			m_VehicleUIProbe = new AICF_VehiclePresetProbeEditor(m_VehicleUIModel);
			string jip;
			m_VehicleUIProbe.Build(m_wVehicleUIHidden, player, System.GetCLIParam("aicfVehiclePresetJIP", jip), m_bVehicleRightsPassed);
			return;
		}
		m_VehicleUIProbe.Tick();
		if (!m_VehicleUIProbe.Done()) return;
		string late;
		player.AICF_VehicleProbeClientFinished(m_VehicleUIProbe.Failures(), System.GetCLIParam("aicfVehiclePresetJIP", late));
		GetGame().GetCallqueue().Remove(AICF_VehicleUITick);
		GetGame().GetCallqueue().CallLater(AICF_VehicleUIClose, 2000, false);
	}
	protected void AICF_VehicleUIClose() { GetGame().RequestClose(); }
	override void OnGameEnd()
	{
		GetGame().GetCallqueue().Remove(AICF_VehicleUITick);
		GetGame().GetCallqueue().Remove(AICF_VehicleUIClose);
		if (m_VehicleUIProbe) m_VehicleUIProbe.Close();
		m_VehicleUIProbe = null;
		m_VehicleUIModel = null;
		if (m_wVehicleUIHidden) m_wVehicleUIHidden.RemoveFromHierarchy();
		m_wVehicleUIHidden = null;
		super.OnGameEnd();
	}
}
