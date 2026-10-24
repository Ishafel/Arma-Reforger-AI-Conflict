// Только isolated Core/UI после QolProbe. Строит невидимое дерево widgets;
// не открывает меню, не меняет focus и не имитирует mouse/keyboard input.
class AICF_InspectorGroupModel : AICF_StrategicUIController
{
	override bool GetLoadoutGroup(int slotId, out string name, out int size, string targetFaction = "")
	{
		name = "Probe squad";
		size = 10;
		return targetFaction == "US" && slotId >= 0 && slotId < 10;
	}
}

modded class AICF_LoadoutItemGrid
{
	void AICF_InspectorScrollProbe(string label)
	{
		array<string> prefabs = {}, names = {}, details = {};
		for (int i; i < 37; i++)
		{
			prefabs.Insert(string.Empty);
			names.Insert("Row " + i.ToString());
			details.Insert(string.Empty);
		}
		SetItems(prefabs, names, details);
		Scroll(1);
		AICF_QolChecks.Check("INSPECTOR_SCROLL_ROW_" + label, m_iFirst == m_iColumns);
		AICF_QolChecks.Check("INSPECTOR_SCROLL_NATIVE_" + label, m_Scrollbar && m_Scrollbar.IsVertical() && m_Scrollbar.IsEnabled());
		AICF_QolChecks.Check("INSPECTOR_SCROLL_SYNC_" + label, m_Scrollbar.GetCurrent() == m_iFirst);
		Scroll(1000);
		int last = LastPageOffset(37, m_iColumns, m_aCards.Count());
		AICF_QolChecks.Check("INSPECTOR_SCROLL_LAST_" + label, m_iFirst == last);
		Select(0);
		AICF_QolChecks.Check("INSPECTOR_SCROLL_REVEAL_" + label, m_iFirst == 0 && m_iSelected == 0);
		SetEnabled(false);
		Scroll(1);
		AICF_QolChecks.Check("INSPECTOR_SCROLL_DISABLED_" + label, m_iFirst == 0 && !m_Scrollbar.IsEnabled());
		SetEnabled(true);
		array<string> empty = {};
		SetItems(empty, empty, empty);
		Render();
		AICF_QolChecks.Check("INSPECTOR_SCROLL_EMPTY_" + label, !m_Scrollbar.IsVisible() && !m_ScrollThumb.IsVisible());
	}
}

class AICF_InspectorProbeEditor : AICF_LoadoutEditor
{
	void AICF_InspectorProbeEditor(AICF_StrategicUIController controller) { m_Controller = controller; }

	void Run(Widget hiddenParent, SCR_PlayerController player)
	{
		m_Player = player;
		m_sTargetFaction = "US";
		m_iSlotId = 0;
		bool built = CreateInspector(hiddenParent);
		AICF_QolChecks.Check("INSPECTOR_HIDDEN_BUILD", built);
		if (!built) { Close(); return; }
		AICF_QolChecks.Check("INSPECTOR_HIDDEN_TREE", !m_wRoot.IsVisibleInHierarchy());
		vector left = FrameSlot.GetAnchorMin(m_wPreview);
		vector right = FrameSlot.GetAnchorMax(m_wPreview);
		AICF_QolChecks.Check("INSPECTOR_WIDTH_70_PERCENT", Math.AbsFloat((right[0] - left[0]) / 0.36 - 0.70) < 0.001);
		array<Widget> sections = {m_wLibrarySection, m_wMemberSaveSection, m_wSquadSaveSection, m_wConfirmSection};
		for (int i; i < sections.Count(); i++)
		{
			AICF_QolChecks.Check("INSPECTOR_SECTION_LAYER_" + i.ToString(), sections[i].GetZOrder() > 1);
			AICF_QolChecks.Check("INSPECTOR_SECTION_INPUT_" + i.ToString(), !(sections[i].GetFlags() & WidgetFlags.IGNORE_CURSOR));
		}
		AICF_QolChecks.Check("INSPECTOR_EDIT_INPUT", m_wName && !(m_wName.GetFlags() & WidgetFlags.IGNORE_CURSOR));
		AICF_QolChecks.Check("INSPECTOR_GROUPS_AVAILABLE", HasLoadoutGroups("US"));
		AICF_QolChecks.Check("INSPECTOR_PLAYER_SIDE_EMPTY", !HasLoadoutGroups("USSR"));
		Widget catalogRoot = m_wSearch;
		while (catalogRoot.GetParent() && catalogRoot.GetParent() != m_wPanel) catalogRoot = catalogRoot.GetParent();
		vector catalogLeft = FrameSlot.GetAnchorMin(catalogRoot);
		AICF_QolChecks.Check("INSPECTOR_CATALOG_EXPANDED", Math.AbsFloat(catalogLeft[0] - 0.532) < 0.001);
		m_Slots.AICF_InspectorScrollProbe("SLOTS");
		m_Items.AICF_InspectorScrollProbe("ITEMS");
		Close();
		AICF_QolChecks.Check("INSPECTOR_HIDDEN_CLEANUP", !m_wRoot && !m_wModal);
	}
}

modded class SCR_GameModeCampaign
{
	override protected void AICF_QolClose()
	{
		string enabled;
		if (System.GetCLIParam("aicfInspectorProbe", enabled) && enabled == "1")
		{
			Widget hidden = GetGame().GetWorkspace().CreateWidget(WidgetType.FrameWidgetTypeID, WidgetFlags.IGNORE_CURSOR, Color.White, 0);
			AICF_QolChecks.Check("INSPECTOR_HIDDEN_PARENT", hidden != null);
			if (hidden)
			{
				hidden.SetVisible(false);
				AICF_InspectorGroupModel model = new AICF_InspectorGroupModel();
				AICF_InspectorProbeEditor editor = new AICF_InspectorProbeEditor(model);
				editor.Run(hidden, SCR_PlayerController.Cast(GetGame().GetPlayerController()));
				hidden.RemoveFromHierarchy();
			}
		}
		super.AICF_QolClose();
	}
}
