// Hidden widget tree + настоящие owner RPC. Не открывает карту/окна,
// не захватывает focus и не имитирует физический ввод.
modded class AICF_FactionFleet
{
	// Private client-only test state; never touches the authoritative fleet.
	AICF_VehicleLease AICF_ClientGCBind(Vehicle vehicle, string rpl)
	{
		if (Replication.IsServer() || !vehicle || rpl.IsEmpty() || rpl == "NONE") return null;
		AICF_VehicleLease lease = new AICF_VehicleLease(m_sFactionKey, 0, 1, 17, ++m_iNextLeaseGeneration);
		m_aLeases.Insert(lease);
		if (!BindReservedLeaseVehicle(lease, vehicle, rpl,
			"{259EE7B78C51B624}Prefabs/Vehicles/Wheeled/UAZ469/UAZ469.et", AICF_EVehicleKind.LIGHT_TRANSPORT, 5, vehicle.GetOrigin()))
		{
			m_aLeases.RemoveItem(lease);
			return null;
		}
		return lease;
	}
}

class AICF_VehicleClientGarbageProbe
{
	static bool Run(SCR_PlayerController player, Vehicle vehicle, RplId expected, bool jip)
	{
		IEntity avatar;
		if (player) avatar = player.GetControlledEntity();
		BaseWorld world = GetGame().GetWorld();
		RplComponent component;
		if (vehicle) component = RplComponent.Cast(vehicle.FindComponent(RplComponent));
		ref AICF_FactionFleet fleet = new AICF_FactionFleet("USSR", 10);
		string rpl = AICF_VehicleBehaviorBinding.ComponentRpl(vehicle);
		AICF_VehicleLease lease = fleet.AICF_ClientGCBind(vehicle, rpl);
		bool prepared = !Replication.IsServer() && vehicle && component && component.IsProxy() && !component.IsMaster();
		if (prepared) prepared = expected != RplId.Invalid() && component.Id() == expected && Replication.FindItem(expected) == component;
		if (prepared) prepared = lease && fleet.OwnsVehicleLease(lease);
		if (prepared) prepared = lease.HasPhysicalAsset() && vehicle.GetWorld() == GetGame().GetWorld();
		if (prepared) prepared = lease.MatchesIdentity("USSR", 0, 1, lease.GetLeaseGeneration(), lease.GetVehicleGeneration(), vehicle.GetID(), rpl);
		if (prepared) prepared = lease.MatchesTripIdentity("USSR", 0, 1, 17);
		bool registerDenied;
		bool protectDenied;
		if (prepared)
		{
			registerDenied = !AICF_VehicleGarbagePolicy.Register(fleet, lease);
			ref AICF_VehicleGarbagePermission permission = new AICF_VehicleGarbagePermission(fleet, lease);
			protectDenied = !permission.CanProtect(vehicle);
		}
		bool passed = prepared && registerDenied && protectDenied;
		string details = string.Format("jip=%1 actual_server=%2 prepared=%3 register_denied=%4 protect_denied=%5 passed=%6",
			jip, Replication.IsServer(), prepared, registerDenied, protectDenied, passed);
		if (vehicle)
		{
			details += string.Format(" entity=%1 rpl=%2", AICF_GroupTrace.Entity(vehicle), rpl);
			if (component) details += string.Format(" native_master=%1 native_proxy=%2 native_role=%3", component.IsMaster(), component.IsProxy(), component.Role());
		}
		details += string.Format(" actor_present=%1 world_present=%2 vehicle_present=%3 fleet_owns=%4 lease_present=%5 expected_rpl=%6 setup_reason=%7",
			avatar != null, world != null, vehicle != null, fleet.OwnsVehicleLease(lease), lease != null, expected, player.AICF_VehicleGCProbeReason());
		if (lease) details += string.Format(" lease_state=%1 group_generation=%2 trip_generation=%3 lease_generation=%4 vehicle_generation=%5",
			lease.GetState(), lease.GetGroupGeneration(), lease.GetTripGeneration(), lease.GetLeaseGeneration(), lease.GetVehicleGeneration());
		Print("[AICF][VEHICLE_UI_GC_AUTHORITY] " + details);
		// This is a streamed proxy. Only the owning server fixture may clean it up.
		return passed;
	}
}

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
	protected bool m_bProbeBehaviorSettings;
	protected bool m_bProbeGarbageChecked;
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
		string behaviorFlag;
		m_bProbeBehaviorSettings = System.GetCLIParam("aicfVehicleBehaviorReadback", behaviorFlag) && behaviorFlag == "1";
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
		if (m_bProbeBehaviorSettings && !m_bProbeGarbageChecked)
			Print(string.Format("[AICF][VEHICLE_UI_GC_CLIENT_TICK] source=proxy_diag_v1 done=%1 pending=%2 queued=%3 view_bytes=%4 player_present=%5",
				m_bProbeDone, m_bPending, m_bQueued, m_sView.Length(), m_Player != null));
		if (m_bProbeDone || m_bPending || m_bQueued || m_sView.IsEmpty() || !m_Player) return;
		if (m_bProbeBehaviorSettings && !m_bProbeGarbageChecked)
		{
			m_Player.AICF_RequestVehicleGCProbe();
			if (!m_Player.AICF_VehicleGCProbeReady()) return;
			Check("GC_NO_AUTHORITY_REJECTED", AICF_VehicleClientGarbageProbe.Run(m_Player,
				m_Player.AICF_VehicleGCProbeEntity(), m_Player.AICF_VehicleGCProbeIdentity(), m_bProbeJIP));
			m_bProbeGarbageChecked = true;
		}
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
			// Неудачный первый Apply должен завершить probe с FAIL, без null dereference.
			if (!confirmed) { Finish(); return; }
			Check("JIP_STABLE_SLOT", slot == 0 && m_iRevision > 0 && faction == "USSR");
			if (m_bProbeBehaviorSettings)
			{
				Check("JIP_BEHAVIOR_CONFIRMED", confirmed && confirmed.m_iBehavior == AICF_EVehicleBehavior.COMBAT && confirmed.m_iReturnDistance == 437);
				Check("JIP_BEHAVIOR_UI_READBACK", m_Draft && m_Draft.m_iBehavior == AICF_EVehicleBehavior.COMBAT &&
					m_Behavior.GetCurrentIndex() == AICF_EVehicleBehavior.COMBAT && m_wReturnDistance.GetText() == "437");
			}
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
				if (m_bProbeBehaviorSettings)
				{
					m_Behavior.SetCurrentItem(AICF_EVehicleBehavior.COMBAT, false, false, false);
					OnChanged(m_Behavior, AICF_EVehicleBehavior.COMBAT);
					m_wReturnDistance.SetText("49");
					int beforeInvalid = m_iToken;
					Click("save");
					Check("BEHAVIOR_INVALID_DISTANCE_NO_RPC", m_iToken == beforeInvalid && !m_bPending && !m_bQueued);
					m_wReturnDistance.SetText("437");
					Check("BEHAVIOR_UI_CONTROLS", m_Draft.m_iBehavior == AICF_EVehicleBehavior.COMBAT &&
						m_Behavior.GetCurrentIndex() == AICF_EVehicleBehavior.COMBAT && m_wReturnDistance.GetText() == "437");
				}
				m_iProbeExpectedRevision = m_iRevision;
				Click("save");
				break;
			case 2:
				Check("SAVE_HANDLER", m_Player.AICF_VehiclePresetAccepted() && m_iRevision == m_iProbeExpectedRevision && assigned.IsEmpty() && m_aLibraryIds.Count() > 0);
				if (m_bProbeBehaviorSettings) Check("BEHAVIOR_SAVE_READBACK", m_Draft && m_Draft.m_iBehavior == AICF_EVehicleBehavior.COMBAT && m_Draft.m_iReturnDistance == 437 && assigned.IsEmpty());
				Click("apply");
				break;
			case 3:
				Check("APPLY_HANDLER", m_Player.AICF_VehiclePresetAccepted() && m_iRevision == m_iProbeExpectedRevision + 1 && AICF_VehiclePreset.Decode(assigned));
				if (m_bProbeBehaviorSettings)
				{
					AICF_VehiclePreset behaviorApplied = AICF_VehiclePreset.Decode(assigned);
					Check("BEHAVIOR_APPLY_READBACK", behaviorApplied && behaviorApplied.m_iBehavior == AICF_EVehicleBehavior.COMBAT && behaviorApplied.m_iReturnDistance == 437);
				}
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
				if (m_bProbeBehaviorSettings)
				{
					m_Behavior.SetCurrentItem(AICF_EVehicleBehavior.TRANSPORT, false, false, false);
					OnChanged(m_Behavior, AICF_EVehicleBehavior.TRANSPORT);
					m_wReturnDistance.SetText("50");
				}
				m_Library.SetCurrentItem(0, false, false, false);
				Click("load");
				break;
			case 7:
				Check("LOAD_HANDLER", m_Player.AICF_VehiclePresetAccepted() && m_Draft && m_Draft.m_sName == "UI vehicle" && m_iRevision == m_iProbeExpectedRevision);
				if (m_bProbeBehaviorSettings) Check("BEHAVIOR_LIBRARY_LOAD_READBACK", m_Draft &&
					m_Draft.m_iBehavior == AICF_EVehicleBehavior.COMBAT && m_Draft.m_iReturnDistance == 437 &&
					m_Behavior.GetCurrentIndex() == AICF_EVehicleBehavior.COMBAT && m_wReturnDistance.GetText() == "437");
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
	protected Vehicle m_AICFGCProbeVehicle;
	protected EntityID m_AICFGCProbeVehicleId;
	protected RplId m_AICFGCProbeServerRpl = RplId.Invalid();
	protected RplId m_AICFGCProbeClientRpl = RplId.Invalid();
	protected bool m_bAICFGCProbeSpawnAttempted;
	protected bool m_bAICFGCProbeSetupFailed;
	protected bool m_bAICFGCProbeNearbyCharacter;
	protected string m_sAICFGCProbeReason = "WAIT_REQUEST";
	protected int m_iAICFGCProbeRequestAt;

	void AICF_RequestVehicleGCProbe()
	{
		Print(string.Format("[AICF][VEHICLE_UI_GC_CLIENT_REQUEST] source=proxy_diag_v1 actual_server=%1 owner_exact=%2 setup_failed=%3 expected_rpl=%4 invalid_rpl=%5 now=%6 request_at=%7 controller_item=%8",
			Replication.IsServer(), GetGame().GetPlayerController() == this, m_bAICFGCProbeSetupFailed,
			m_AICFGCProbeClientRpl, RplId.Invalid(), System.GetTickCount(), m_iAICFGCProbeRequestAt, Replication.FindItemId(this)));
		if (Replication.IsServer() || GetGame().GetPlayerController() != this || m_bAICFGCProbeSetupFailed ||
			m_AICFGCProbeClientRpl != RplId.Invalid() || System.GetTickCount() < m_iAICFGCProbeRequestAt) return;
		m_iAICFGCProbeRequestAt = System.GetTickCount() + 2000;
		Print("[AICF][VEHICLE_UI_GC_CLIENT_SEND] source=proxy_diag_v1 rpc=SERVER");
		Rpc(RpcAsk_AICFVehicleGCProbe);
	}

	Vehicle AICF_VehicleGCProbeEntity()
	{
		RplComponent component = RplComponent.Cast(Replication.FindItem(m_AICFGCProbeClientRpl));
		if (!component || component.Id() != m_AICFGCProbeClientRpl) return null;
		return Vehicle.Cast(component.GetEntity());
	}
	RplId AICF_VehicleGCProbeIdentity() { return m_AICFGCProbeClientRpl; }
	string AICF_VehicleGCProbeReason() { return m_sAICFGCProbeReason; }
	bool AICF_VehicleGCProbeReady()
	{
		if (m_bAICFGCProbeSetupFailed) return true;
		Vehicle vehicle = AICF_VehicleGCProbeEntity();
		if (!vehicle) return false;
		RplComponent component = RplComponent.Cast(vehicle.FindComponent(RplComponent));
		if (component) Print(string.Format("[AICF][VEHICLE_UI_GC_CLIENT_NATIVE] actual_server=%1 entity=%2 expected_rpl=%3 actual_rpl=%4 native_master=%5 native_proxy=%6 native_role=%7",
			Replication.IsServer(), vehicle.GetID(), m_AICFGCProbeClientRpl, component.Id(), component.IsMaster(), component.IsProxy(), component.Role()));
		return component && component.Id() == m_AICFGCProbeClientRpl && component.IsProxy() && !component.IsMaster();
	}

	protected bool AICF_VehicleGCProbeServerEnabled()
	{
		string clientFlag, behaviorFlag;
		bool hasClientFlag = System.GetCLIParam("aicfVehiclePresetClient", clientFlag);
		bool hasBehaviorFlag = System.GetCLIParam("aicfVehicleBehaviorReadback", behaviorFlag);
		PlayerManager manager;
		if (GetGame()) manager = GetGame().GetPlayerManager();
		bool exactOwner = manager && manager.GetPlayerController(GetPlayerId()) == this;
		RplComponent component = RplComponent.Cast(FindComponent(RplComponent));
		string details = string.Format("source=proxy_diag_v1 actual_server=%1 client_flag_present=%2 client_flag=[%3] behavior_flag_present=%4 behavior_flag=[%5] player=%6 owner_exact=%7 controller_component=%8 component_authority=%9",
			Replication.IsServer(), hasClientFlag, clientFlag, hasBehaviorFlag, behaviorFlag, GetPlayerId(), exactOwner,
			component != null, AICF_VehicleBoardingMutationFence.IsAuthoritativeReplicatedEntity(this));
		details += string.Format(" manager_present=%1 controller_item=%2", manager != null, Replication.FindItemId(this));
		if (component) details += string.Format(" native_master=%1 native_proxy=%2 native_role=%3", component.IsMaster(), component.IsProxy(), component.Role());
		Print("[AICF][VEHICLE_UI_GC_SERVER_GUARDS] " + details);
		if (!manager) return false;
		// PlayerController RPC authority belongs to the server connection/owner.
		// Its native component can be neither master nor proxy on the server.
		return Replication.IsServer() && hasClientFlag && clientFlag == "1" && hasBehaviorFlag && behaviorFlag == "1" &&
			GetPlayerId() > 0 && manager.GetPlayerController(GetPlayerId()) == this;
	}

	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_AICFVehicleGCProbe()
	{
		Print(string.Format("[AICF][VEHICLE_UI_GC_SERVER_RECEIVED] source=proxy_diag_v1 actual_server=%1 player=%2", Replication.IsServer(), GetPlayerId()));
		if (!AICF_VehicleGCProbeServerEnabled()) return;
		IEntity avatar = GetControlledEntity();
		if (!avatar || GetGame().GetPlayerManager().GetPlayerIdFromControlledEntity(avatar) != GetPlayerId())
		{
			AICF_VehicleGCProbeReply("WAIT_AVATAR", false);
			return;
		}
		if (!m_bAICFGCProbeSpawnAttempted)
		{
			m_bAICFGCProbeSpawnAttempted = true;
			BaseWorld world = GetGame().GetWorld();
			Resource prefab = Resource.Load("{259EE7B78C51B624}Prefabs/Vehicles/Wheeled/UAZ469/UAZ469.et");
			bool validResource = prefab && prefab.IsValid();
			bool safe;
			vector position;
			if (world && validResource)
			{
				// Две ограниченные окружности: четыре cardinal samples могут все
				// попасть в постройки HQ. Расстояние10..20 и все safety gates прежние.
				array<vector> offsets = {"15 0 0", "13 0 7.5", "7.5 0 13", "0 0 15", "-7.5 0 13", "-13 0 7.5", "-15 0 0", "-13 0 -7.5", "-7.5 0 -13", "0 0 -15", "7.5 0 -13", "13 0 -7.5",
					"18 0 0", "15.5 0 9", "9 0 15.5", "0 0 18", "-9 0 15.5", "-15.5 0 9", "-18 0 0", "-15.5 0 -9", "-9 0 -15.5", "0 0 -18", "9 0 -15.5", "15.5 0 -9"};
				int candidate;
				foreach (vector offset : offsets)
				{
					candidate++;
					vector center = avatar.GetOrigin() + offset;
					if (!SCR_WorldTools.FindEmptyTerrainPosition(position, center, 4, 3, 3, TraceFlags.ENTS | TraceFlags.OCEAN, world))
					{
						Print(string.Format("[AICF][VEHICLE_UI_GC_CANDIDATE] player=%1 candidate=%2 center=%3 accepted=0 reason=NO_EMPTY_TERRAIN readonly=1", GetPlayerId(), candidate, center));
						continue;
					}
					float distance = vector.Distance(position, avatar.GetOrigin());
					bool water = ChimeraWorldUtils.TryGetWaterSurfaceSimple(world, position);
					if (distance < 10 || distance > 20 || water)
					{
						Print(string.Format("[AICF][VEHICLE_UI_GC_CANDIDATE] player=%1 candidate=%2 position=%3 distance_m=%4 water=%5 accepted=0 reason=DISTANCE_OR_WATER readonly=1", GetPlayerId(), candidate, position, distance, water));
						continue;
					}
					m_bAICFGCProbeNearbyCharacter = false;
					world.QueryEntitiesBySphere(position + "0 0.2 0", 5, AICF_VehicleGCProbeCollectNearby, null, EQueryEntitiesFlags.ALL);
					Print(string.Format("[AICF][VEHICLE_UI_GC_CANDIDATE] player=%1 candidate=%2 position=%3 distance_m=%4 nearby_character=%5 accepted=%6 readonly=1", GetPlayerId(), candidate, position, distance, m_bAICFGCProbeNearbyCharacter, !m_bAICFGCProbeNearbyCharacter));
					if (m_bAICFGCProbeNearbyCharacter) continue;
					safe = true;
					break;
				}
			}
			if (safe)
			{
				EntitySpawnParams params = new EntitySpawnParams();
				params.TransformMode = ETransformMode.WORLD;
				params.Transform[3] = position + "0 0.2 0";
				m_AICFGCProbeVehicle = Vehicle.Cast(GetGame().SpawnEntityPrefab(prefab, world, params));
				if (m_AICFGCProbeVehicle) m_AICFGCProbeVehicleId = m_AICFGCProbeVehicle.GetID();
			}
			Print(string.Format("[AICF][VEHICLE_UI_GC_SERVER_SETUP] player=%1 avatar_present=%2 world_present=%3 resource_valid=%4 safe_spawn=%5 vehicle_present=%6 distance=%7",
				GetPlayerId(), avatar != null, world != null, validResource, safe, m_AICFGCProbeVehicle != null, vector.Distance(position, avatar.GetOrigin())));
		}
		if (!m_AICFGCProbeVehicle)
		{
			AICF_VehicleGCProbeReply("SPAWN_FAILED", true);
			return;
		}
		RplComponent component = RplComponent.Cast(m_AICFGCProbeVehicle.FindComponent(RplComponent));
		if (!component || !component.IsMaster() || component.IsProxy() || m_AICFGCProbeVehicle.GetID() != m_AICFGCProbeVehicleId)
		{
			AICF_VehicleGCProbeReply("SERVER_IDENTITY_FAILED", true);
			return;
		}
		m_AICFGCProbeServerRpl = component.Id();
		if (m_AICFGCProbeServerRpl == RplId.Invalid()) { AICF_VehicleGCProbeReply("WAIT_RPL", false); return; }
		if (!AICF_VehicleGCProbeSafeDelete()) { AICF_VehicleGCProbeDescribeSafety(); AICF_VehicleGCProbeReply("NOT_EMPTY_OR_UNSAFE", true); return; }
		AICF_VehicleGCProbeReply("SERVER_SPAWNED", false);
	}

	protected void AICF_VehicleGCProbeReply(string reason, bool failed)
	{
		Print(string.Format("[AICF][VEHICLE_UI_GC_SERVER_REPLY] player=%1 reason=%2 failed=%3 entity=%4 rpl=%5",
			GetPlayerId(), reason, failed, m_AICFGCProbeVehicleId, m_AICFGCProbeServerRpl));
		Rpc(RpcDo_AICFVehicleGCProbe, m_AICFGCProbeServerRpl, reason, failed);
	}

	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	protected void RpcDo_AICFVehicleGCProbe(RplId identity, string reason, bool failed)
	{
		if (Replication.IsServer() || GetGame().GetPlayerController() != this) return;
		m_AICFGCProbeClientRpl = identity;
		m_sAICFGCProbeReason = reason;
		m_bAICFGCProbeSetupFailed = failed;
		Print(string.Format("[AICF][VEHICLE_UI_GC_CLIENT_REPLY] actual_server=%1 reason=%2 failed=%3 rpl=%4", Replication.IsServer(), reason, failed, identity));
	}

	protected bool AICF_VehicleGCProbeCollectNearby(IEntity entity)
	{
		if (ChimeraCharacter.Cast(entity)) m_bAICFGCProbeNearbyCharacter = true;
		return true;
	}

	protected bool AICF_VehicleGCProbeSafeDelete()
	{
		if (!GetGame()) return false;
		BaseWorld world = GetGame().GetWorld();
		PlayerManager manager = GetGame().GetPlayerManager();
		if (!world || !manager) return false;
		Vehicle vehicle = m_AICFGCProbeVehicle;
		if (!Replication.IsServer() || !vehicle || !vehicle.GetWorld() || vehicle.GetID() != m_AICFGCProbeVehicleId ||
			!AICF_VehicleBoardingMutationFence.IsAuthoritativeReplicatedEntity(vehicle)) return false;
		RplComponent component = RplComponent.Cast(vehicle.FindComponent(RplComponent));
		if (!component || component.Id() != m_AICFGCProbeServerRpl) return false;
		if (manager.GetPlayerIdFromControlledEntity(vehicle) > 0 ||
			SCR_PossessingManagerComponent.GetPlayerIdFromMainEntity(vehicle) > 0) return false;
		array<BaseCompartmentSlot> seats = {};
		AICF_FIAPatrolCrew.Seats(vehicle, seats);
		if (seats.IsEmpty()) return false;
		foreach (BaseCompartmentSlot seat : seats) if (!seat || seat.GetOccupant()) return false;
		m_bAICFGCProbeNearbyCharacter = false;
		vehicle.GetWorld().QueryEntitiesBySphere(vehicle.GetOrigin(), 5, AICF_VehicleGCProbeCollectNearby, null, EQueryEntitiesFlags.ALL);
		if (m_bAICFGCProbeNearbyCharacter) return false;
		array<int> players = {};
		manager.GetPlayers(players);
		foreach (int playerId : players)
		{
			SCR_PlayerController player = SCR_PlayerController.Cast(manager.GetPlayerController(playerId));
			if (!player) return false;
			IEntity actor = player.GetControlledEntity();
			if (actor == vehicle || (actor && CompartmentAccessComponent.GetVehicleIn(actor) == vehicle)) return false;
		}
		return AICF_LogisticsResourceAdapter.CanDeleteVehicle(vehicle);
	}

	protected void AICF_VehicleGCProbeDescribeSafety()
	{
		if (!GetGame()) return;
		Vehicle vehicle = m_AICFGCProbeVehicle;
		BaseWorld world = GetGame().GetWorld();
		PlayerManager manager = GetGame().GetPlayerManager();
		Print(string.Format("[AICF][VEHICLE_UI_GC_SERVER_SAFETY] realm=%1 world=%2 manager=%3 vehicle=%4",
			Replication.IsServer(), world != null, manager != null, vehicle != null));
		if (!vehicle || !world || !manager) return;
		RplComponent component = RplComponent.Cast(vehicle.FindComponent(RplComponent));
		string details = string.Format(" entity=%1 expected_entity=%2 rpl=%3 expected_rpl=%4 vehicle_authority=%5 direct_player=%6 main_player=%7",
			vehicle.GetID(), m_AICFGCProbeVehicleId, AICF_VehicleBehaviorBinding.ComponentRpl(vehicle), m_AICFGCProbeServerRpl,
			AICF_VehicleBoardingMutationFence.IsAuthoritativeReplicatedEntity(vehicle), manager.GetPlayerIdFromControlledEntity(vehicle),
			SCR_PossessingManagerComponent.GetPlayerIdFromMainEntity(vehicle));
		if (component) details += string.Format(" native_master=%1 native_proxy=%2 native_role=%3", component.IsMaster(), component.IsProxy(), component.Role());
		array<BaseCompartmentSlot> seats = {};
		AICF_FIAPatrolCrew.Seats(vehicle, seats);
		int occupied, unknown;
		foreach (BaseCompartmentSlot seat : seats) { if (!seat) unknown++; else if (seat.GetOccupant()) occupied++; }
		m_bAICFGCProbeNearbyCharacter = false;
		vehicle.GetWorld().QueryEntitiesBySphere(vehicle.GetOrigin(), 5, AICF_VehicleGCProbeCollectNearby, null, EQueryEntitiesFlags.ALL);
		details += string.Format(" seats=%1 occupied=%2 unknown=%3 nearby_character=%4 resources_clear=%5",
			seats.Count(), occupied, unknown, m_bAICFGCProbeNearbyCharacter, AICF_LogisticsResourceAdapter.CanDeleteVehicle(vehicle));
		array<int> players = {};
		manager.GetPlayers(players);
		foreach (int playerId : players)
		{
			SCR_PlayerController player = SCR_PlayerController.Cast(manager.GetPlayerController(playerId));
			IEntity actor;
			if (player) actor = player.GetControlledEntity();
			IEntity linked;
			if (actor) linked = CompartmentAccessComponent.GetVehicleIn(actor);
			details += string.Format(" player=%1 controller_present=%2 actor=%3 actor_link=%4", playerId, player != null,
				AICF_GroupTrace.Entity(actor), AICF_GroupTrace.Entity(linked));
		}
		Print("[AICF][VEHICLE_UI_GC_SERVER_SAFETY_FIELDS]" + details);
	}

	protected void AICF_VehicleGCProbeCleanup()
	{
		if (!AICF_VehicleGCProbeSafeDelete() || !AICF_VehicleGCProbeSafeDelete()) { AICF_VehicleGCProbeDescribeSafety(); return; }
		Vehicle vehicle = m_AICFGCProbeVehicle;
		m_AICFGCProbeVehicle = null;
		RplComponent.DeleteRplEntity(vehicle, false);
		Print(string.Format("[AICF][VEHICLE_UI_GC_SERVER_CLEANUP] player=%1 rpl=%2 exact_empty_delete_requested=1", GetPlayerId(), m_AICFGCProbeServerRpl));
	}

	void ~SCR_PlayerController() { if (GetGame()) AICF_VehicleGCProbeCleanup(); }

	void AICF_VehicleProbeClientFinished(int failures, bool jip) { Rpc(RpcAsk_AICFVehicleProbeClientFinished, failures, jip); }
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_AICFVehicleProbeClientFinished(int failures, bool jip)
	{
		string flag;
		if (!Replication.IsServer() || !System.GetCLIParam("aicfVehiclePresetClient", flag)) return;
		AICF_VehicleGCProbeCleanup();
		Print(string.Format("[AICF][VEHICLE_UI_SERVER_READBACK] player=%1 failures=%2 jip=%3", GetPlayerId(), failures, jip));
		string behaviorFlag;
		if (System.GetCLIParam("aicfVehicleBehaviorReadback", behaviorFlag) && behaviorFlag == "1")
		{
			AICF_MatchController match = AICF_MatchController.GetActiveController();
			AICF_GroupSlot slot;
			SCR_CampaignFaction faction;
			AICF_VehiclePreset actual;
			int schema;
			int revision;
			if (match && match.ResolveVehiclePresetContext(this, 0, slot, faction))
			{
				string encoded = slot.GetVehiclePresetData();
				actual = AICF_VehiclePreset.Decode(encoded);
				JsonLoadContext json = new JsonLoadContext();
				if (json.LoadFromString(encoded)) json.ReadValue("schemaVersion", schema);
				revision = slot.GetVehiclePresetRevision();
			}
			int behavior = -1;
			int distance = -1;
			if (actual) { behavior = actual.m_iBehavior; distance = actual.m_iReturnDistance; }
			Print(string.Format("[AICF][VEHICLE_UI_SERVER_BEHAVIOR_READBACK] player=%1 valid=%2 behavior=%3 return_distance=%4 schema=%5 revision=%6 jip=%7",
				GetPlayerId(), actual != null, behavior, distance, schema, revision, jip));
		}
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
		vector point = AICF_GroupRuntime.ResolveAliveLeader(slot.GetGroup()).GetOrigin() + "400 0 0";
		point[1] = GetGame().GetWorld().GetSurfaceY(point[0], point[2]);
		m_OrderPlanner.AssignPlayerPointOrder(slot, m_USSRFaction, point, false);
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
