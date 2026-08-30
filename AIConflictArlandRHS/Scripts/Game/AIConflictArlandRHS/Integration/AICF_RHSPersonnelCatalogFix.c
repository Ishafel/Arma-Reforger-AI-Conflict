// Small LivingArea providers deliberately show only GROUPTYPE_ESSENTIAL groups.
// RHS groups visible in the large LivingArea omit that vanilla label. Treat one
// minimal, already-registered SentryTeam per supported faction as essential only
// in the client-side filter copy. Numeric prefab IDs, UI metadata, budgets and
// authoritative placement keeps all stock checks while receiving the same narrow
// label projection before validating provider traits.
class AICF_RHSPersonnelBuildingFix
{
	static bool IsActiveProfile()
	{
		AICF_ContentProfile contentProfile = AICF_ContentProfile.GetActive();
		return contentProfile &&
			contentProfile.GetProfileKey() == "RHS_USMC_MSV_0_16_5150";
	}

	static bool IsSupportedEssentialGroup(ResourceName prefab)
	{
		return prefab.Contains(
			"RHS_USAF_USMC_MEF/Group_USAF_USMC_MEF_SentryTeam.et") ||
			prefab.Contains(
				"MSV/VKPO_Demiseason/Group_RHS_RF_MSV_VKPO_DS_SentryTeam.et");
	}
}

modded class SCR_ContentBrowserEditorComponent
{
	protected bool m_bAICFEssentialGroupsLogged;

	override void FilterEntries()
	{
		if (GetExtendedEntity())
			return;

		AICF_ContentProfile contentProfile = AICF_ContentProfile.GetActive();
		bool isRHSProfile = AICF_RHSPersonnelBuildingFix.IsActiveProfile();
		SCR_PlacingEditorComponentClass placingPrefabData;
		array<int> essentialGroupPrefabIDs = {};
		string groups;
		if (isRHSProfile)
		{
			placingPrefabData = SCR_PlacingEditorComponentClass.Cast(
				SCR_PlacingEditorComponentClass.GetInstance(SCR_PlacingEditorComponent, true));
			if (placingPrefabData)
			{
				for (int prefabId = 0, infoCount = GetInfoCount(); prefabId < infoCount; prefabId++)
				{
					ResourceName prefab = placingPrefabData.GetPrefab(prefabId);
					if (!AICF_RHSPersonnelBuildingFix.IsSupportedEssentialGroup(prefab))
						continue;

					SCR_EditableEntityUIInfo groupInfo = GetInfo(prefabId);
					if (!groupInfo || !groupInfo.HasEntityLabel(EEditableEntityLabel.ENTITYTYPE_GROUP))
						continue;

					essentialGroupPrefabIDs.Insert(prefabId);
					if (!groups.IsEmpty())
						groups += ",";
					groups += prefab;
				}
			}
		}

		bool bindEssentialGroups = essentialGroupPrefabIDs.Count() == 2;

		// Stock FilterEntries implementation, with the narrow RHS label projection.
		m_aFilteredPrefabIDs.Clear();
		m_aLocalizationKeys.Clear();
		array<EEditableEntityLabel> entityLabels = {};

		array<EEditableEntityLabel> validBlackListLabels = {};
		GetValidBlackListedLabels(validBlackListLabels);

		bool isBlackListed;
		SCR_EditableEntityUIInfo info;
		int count = GetInfoCount();
		for (int i = 0; i < count; i++)
		{
			isBlackListed = false;
			entityLabels.Clear();
			info = GetInfo(i);

			if (!info)
				continue;

			info.GetEntityLabels(entityLabels);
			if (bindEssentialGroups && essentialGroupPrefabIDs.Contains(i))
				entityLabels.Insert(EEditableEntityLabel.GROUPTYPE_ESSENTIAL);

			if (!IsMatchingToggledLabels(entityLabels))
				continue;

			foreach (EEditableEntityLabel blackListLabel : validBlackListLabels)
			{
				if (entityLabels.Contains(blackListLabel))
				{
					isBlackListed = true;
					break;
				}
			}

			if (isBlackListed)
				continue;

			m_aFilteredPrefabIDs.Insert(i);
			m_aLocalizationKeys.Insert(info.GetName());
		}

		string currentSearch = GetCurrentSearch();
		if (!currentSearch.IsEmpty())
		{
			array<int> searchResultPrefabID = {}, searchResultIndices = {};
			WidgetManager.SearchLocalized(currentSearch, m_aLocalizationKeys, searchResultIndices);

			foreach (int searchResultIndex : searchResultIndices)
			{
				int prefabID = m_aFilteredPrefabIDs.Get(searchResultIndex);
				searchResultPrefabID.Insert(prefabID);
			}
			m_aFilteredPrefabIDs.Copy(searchResultPrefabID);
		}

		m_iFilteredPrefabIDsCount = m_aFilteredPrefabIDs.Count();
		Event_OnBrowserEntriesFiltered.Invoke();

		if (!isRHSProfile ||
			!IsLabelActive(EEditableEntityLabel.GROUPTYPE_ESSENTIAL) ||
			m_bAICFEssentialGroupsLogged)
		{
			return;
		}

		m_bAICFEssentialGroupsLogged = true;
		if (!bindEssentialGroups)
		{
			Print(
				string.Format(
					"[AICF][CONTENT][ERROR][PERSONNEL_BROWSER_BIND_FAILED] profile=%1 reason=ESSENTIAL_GROUPS_MISSING bound_count=%2 expected_count=2 info_count=%3 groups=%4",
					contentProfile.GetProfileKey(),
					essentialGroupPrefabIDs.Count(),
					GetInfoCount(),
					groups),
				LogLevel.ERROR);
			return;
		}

		Print(
			string.Format(
				"[AICF][CONTENT][INFO][PERSONNEL_BROWSER_BOUND] profile=%1 bound_count=%2 filtered_count=%3 groups=%4",
				contentProfile.GetProfileKey(),
				essentialGroupPrefabIDs.Count(),
				GetFilteredPrefabCount(),
				groups),
			LogLevel.NORMAL);
	}
}

// SCR_CampaignBuildingPlacingEditorComponent validates the original prefab
// labels again on authority. Project the same label only around the stock
// AreLabelsMatching call so faction, provider, blacklist and budget checks stay
// authoritative and unchanged.
modded class SCR_CampaignBuildingPlacingEditorComponent
{
	protected bool m_bAICFValidateSupportedEssentialGroup;
	protected bool m_bAICFEssentialGroupLabelProjected;

	override protected bool CanPlaceEntityServer(
		IEntityComponentSource editableEntitySource,
		out EEditableEntityBudget blockingBudget,
		bool updatePreview,
		bool showNotification,
		int prefabID = -1,
		int playerID = -1,
		SCR_EditorPreviewParams params = null)
	{
		ResourceName prefab;
		SCR_PlacingEditorComponentClass prefabData = SCR_PlacingEditorComponentClass.Cast(
			GetEditorComponentData());
		if (prefabData && prefabID >= 0)
			prefab = prefabData.GetPrefab(prefabID);

		m_bAICFValidateSupportedEssentialGroup =
			AICF_RHSPersonnelBuildingFix.IsActiveProfile() &&
			AICF_RHSPersonnelBuildingFix.IsSupportedEssentialGroup(prefab);
		m_bAICFEssentialGroupLabelProjected = false;

		bool canPlace = super.CanPlaceEntityServer(
			editableEntitySource,
			blockingBudget,
			updatePreview,
			showNotification,
			prefabID,
			playerID,
			params);

		if (m_bAICFValidateSupportedEssentialGroup && playerID >= 0)
		{
			AICF_ContentProfile contentProfile = AICF_ContentProfile.GetActive();
			Print(
				string.Format(
					"[AICF][CONTENT][INFO][PERSONNEL_SERVER_VALIDATED] profile=%1 prefab=%2 player_id=%3 essential_projected=%4 allowed=%5",
					contentProfile.GetProfileKey(),
					prefab,
					playerID,
					m_bAICFEssentialGroupLabelProjected,
					canPlace),
				LogLevel.NORMAL);
		}

		m_bAICFValidateSupportedEssentialGroup = false;
		m_bAICFEssentialGroupLabelProjected = false;
		return canPlace;
	}

	override protected bool AreLabelsMatching(notnull array<EEditableEntityLabel> entityLabels)
	{
		bool projected;
		if (m_bAICFValidateSupportedEssentialGroup &&
			!entityLabels.Contains(EEditableEntityLabel.GROUPTYPE_ESSENTIAL))
		{
			entityLabels.Insert(EEditableEntityLabel.GROUPTYPE_ESSENTIAL);
			projected = true;
			m_bAICFEssentialGroupLabelProjected = true;
		}

		bool matches = super.AreLabelsMatching(entityLabels);
		if (projected)
			entityLabels.RemoveItem(EEditableEntityLabel.GROUPTYPE_ESSENTIAL);

		return matches;
	}
}
