// Живые ИИ используют штатные карточки в галерее сохранённых loadouts.
modded class SCR_LoadoutButton
{
	RplId m_AICFCharacter = RplId.Invalid();
	string m_sAICFCharacterName;
	bool m_bAICFPersonalPreset;

	void AICF_SetPersonalPreset(SCR_BasePlayerLoadout role)
	{
		m_bAICFPersonalPreset = true;
		SetLoadout(role);
		string name = AICF_Localization.Resolve("{AICF:AICF_UI_PersonalName}");
		SetPlayerName(name);
		if (m_wPlayerName) m_wPlayerName.SetVisible(true);
		if (m_wPlatformIcon) m_wPlatformIcon.SetVisible(false);
		if (m_wLeaderText) m_wLeaderText.SetVisible(false);
		if (m_wSuppliesLoadoutText) m_wSuppliesLoadoutText.SetText(name);
	}

	void AICF_SetCharacter(RplId character, string name, ResourceName prefab)
	{
		m_AICFCharacter = character;
		m_sAICFCharacterName = name;
		SetPlayerName(name);
		if (m_wPlayerName) m_wPlayerName.SetVisible(true);
		if (m_wPlatformIcon) m_wPlatformIcon.SetVisible(false);
		if (m_wLeaderText) m_wLeaderText.SetVisible(false);
		Resource resource = Resource.Load(prefab);
		if (resource && resource.IsValid())
		{
			IEntityComponentSource source = SCR_BaseContainerTools.FindComponentSource(resource, "SCR_EditableCharacterComponent");
			if (source)
			{
				SCR_EditableEntityUIInfo info = SCR_EditableEntityUIInfo.Cast(BaseContainerTools.CreateInstanceFromContainer(source.GetObject("m_UIInfo")));
				if (info && GetImageWidget())
				{
					info.SetIconTo(GetImageWidget());
					GetImageWidget().SetVisible(true);
				}
			}
		}
		if (m_wSuppliesLoadoutText)
			m_wSuppliesLoadoutText.SetText(AICF_Localization.Resolve("{AICF:AICF_UI_SquadRespawnTitle}"));
	}
}

modded class SCR_LoadoutGallery
{
	protected SCR_LoadoutButton m_AICFPersonalCard;
	protected string m_sAICFPersonalCardContext;
	protected bool m_bAICFPersonalCardSelected;
	protected int m_iAICFNativePageSize;

	void AICF_ClearPersonalCard()
	{
		if (m_AICFPersonalCard)
		{
			m_AICFPersonalCard.m_OnClicked.Remove(AICF_OnPersonalClicked);
			if (m_SelectedButton == m_AICFPersonalCard) m_SelectedButton = null;
			int index = m_aWidgets.Find(m_AICFPersonalCard.GetRootWidget());
			if (index >= 0) RemoveItem(index);
			if (m_iAICFNativePageSize > 0) m_iCountShownItems = m_iAICFNativePageSize;
			m_iSelectedItem = 0;
			ShowTiles(0, false);
			SetupHints(m_aWidgets.Count(), false);
			UpdatePagingButtons();
		}
		m_AICFPersonalCard = null;
		m_sAICFPersonalCardContext = string.Empty;
		m_bAICFPersonalCardSelected = false;
		m_iAICFNativePageSize = 0;
	}

	void AICF_SyncPersonalCard(SCR_PlayerController player, SCR_BasePlayerLoadout role, bool characterSelected, bool busy)
	{
		SCR_CampaignFaction faction;
		AICF_LoadoutRecipe context = AICF_PersonalLoadout.Context(player, faction);
		if (!context || !role || !player.AICF_PersonalAvailable() ||
			!AICF_PersonalLoadout.SameContext(AICF_LoadoutRecipe.Decode(player.AICF_PersonalContext()), context))
		{
			AICF_ClearPersonalCard();
			return;
		}
		if (m_sAICFPersonalCardContext != context.Encode() || (m_AICFPersonalCard && m_AICFPersonalCard.GetLoadout() != role))
			AICF_ClearPersonalCard();
		if (!m_AICFPersonalCard)
		{
			// Личный вариант идёт после native ролей, перед карточками живых ИИ.
			// Обычная синхронизация ниже восстановит roster в прежнем порядке.
			AICF_ClearCharacters();
			Widget card = GetGame().GetWorkspace().CreateWidgets(m_sLoadoutButton, GetContentRoot());
			if (!card) return;
			m_AICFPersonalCard = SCR_LoadoutButton.Cast(card.FindHandler(SCR_LoadoutButton));
			if (!m_AICFPersonalCard) { card.RemoveFromHierarchy(); return; }
			m_AICFPersonalCard.AICF_SetPersonalPreset(role);
			m_AICFPersonalCard.m_OnClicked.Insert(AICF_OnPersonalClicked);
			m_iAICFNativePageSize = m_iCountShownItems;
			m_iCountShownItems = Math.Max(2, m_iCountShownItems);
			AddItem(card);
			m_sAICFPersonalCardContext = context.Encode();
			UpdatePagingButtons();
			Print(string.Format("[AICF][PERSONAL_LOADOUT_CARD] state=CREATED native_gallery=1 visible_cards=%1", m_iCountShownItems));
		}
		bool selected = player.AICF_PersonalSelected() && !characterSelected;
		m_AICFPersonalCard.SetSelected(selected);
		m_AICFPersonalCard.SetEnabled(m_AICFPersonalCard.GetRootWidget().IsVisible() && !busy);
		foreach (SCR_LoadoutButton button : m_aLoadoutButtons)
			button.SetSelected(!selected && !characterSelected && button.GetLoadout() == role);
		if (selected) m_SelectedButton = m_AICFPersonalCard;
		else m_SelectedButton = GetButtonForLoadout(role);
		if (selected != m_bAICFPersonalCardSelected && !busy)
		{
			m_bAICFPersonalCardSelected = selected;
			if (!characterSelected && !busy && m_SelectedButton)
			{
				int index = m_aWidgets.Find(m_SelectedButton.GetRootWidget());
				if (index >= 0)
				{
					int pageStart = Math.Floor(index / m_iCountShownItems) * m_iCountShownItems;
					m_iSelectedItem = pageStart;
					ShowTiles(pageStart, false);
					SetupHints(m_aWidgets.Count(), false);
					UpdatePagingButtons();
				}
			}
			Print(string.Format("[AICF][PERSONAL_LOADOUT_CARD] selected=%1 revision=%2", selected, player.AICF_LoadoutRevision()));
		}
	}

	protected void AICF_OnPersonalClicked(SCR_LoadoutButton button)
	{
		SCR_DeployMenuMain menu = SCR_DeployMenuMain.GetDeployMenu();
		if (menu) menu.AICF_SelectPersonalPreset(true);
	}

	protected ref array<SCR_LoadoutButton> m_aAICFCharacters = {};
	protected ref array<RplId> m_aAICFCharacterIds = {};

	void AICF_ClearCharacters()
	{
		foreach (SCR_LoadoutButton button : m_aAICFCharacters)
		{
			if (!button) continue;
			button.m_OnClicked.Remove(AICF_OnCharacterClicked);
			int index = m_aWidgets.Find(button.GetRootWidget());
			if (index >= 0) RemoveItem(index);
		}
		m_aAICFCharacters.Clear();
		m_aAICFCharacterIds.Clear();
		// RemoveItem не сбрасывает индекс и видимость оставшихся пресетов.
		// После сокращения roster старая страница может оказаться за концом списка.
		m_iSelectedItem = 0;
		ShowTiles(0, false);
		SetupHints(m_aWidgets.Count(), false);
		UpdatePagingButtons();
	}

	override void ClearAll()
	{
		AICF_ClearPersonalCard();
		AICF_ClearCharacters();
		super.ClearAll();
	}

	void AICF_SyncCharacters(SCR_PlayerController player, RplId selected)
	{
		bool alive = AICF_GroupRuntime.IsAliveCharacter(player.GetControlledEntity());
		int count = player.m_aAICFSquadRespawnIds.Count();
		if (alive) count = 0;
		bool changed = count != m_aAICFCharacterIds.Count();
		for (int i = 0; i < count && !changed; i++)
			changed = m_aAICFCharacterIds[i] != player.m_aAICFSquadRespawnIds[i];
		if (changed)
		{
			AICF_ClearCharacters();
			for (int i = 0; i < count; i++)
			{
				Widget card = GetGame().GetWorkspace().CreateWidgets(m_sLoadoutButton, GetContentRoot());
				if (!card) continue;
				SCR_LoadoutButton button = SCR_LoadoutButton.Cast(card.FindHandler(SCR_LoadoutButton));
				if (!button) { card.RemoveFromHierarchy(); continue; }
				string name = player.m_aAICFSquadRespawnNames[i];
				if (name.IsEmpty()) name = AICF_Localization.Resolve("{AICF:AICF_UI_SquadRespawnSoldier}");
				button.AICF_SetCharacter(player.m_aAICFSquadRespawnIds[i], name, player.m_aAICFSquadRespawnPrefabs[i]);
				button.m_OnClicked.Insert(AICF_OnCharacterClicked);
				AddItem(card);
				m_aAICFCharacters.Insert(button);
				m_aAICFCharacterIds.Insert(button.m_AICFCharacter);
			}
			// AddItem обновляет paging до вставки: пересчитать по полному списку.
			UpdatePagingButtons();
		}
		foreach (SCR_LoadoutButton button : m_aAICFCharacters)
		{
			button.SetSelected(button.m_AICFCharacter == selected);
			// Native navigation раскрывает страницу только для disabled widget.
			// Нельзя включать скрытые карточки во время покадровой синхронизации.
			button.SetEnabled(button.GetRootWidget().IsVisible() && player.m_sAICFSquadRespawnResult != "PENDING");
		}
		if (selected.IsValid())
		{
			foreach (SCR_LoadoutButton preset : m_aLoadoutButtons) preset.SetSelected(false);
		}
	}

	protected void AICF_OnCharacterClicked(SCR_LoadoutButton button)
	{
		SCR_DeployMenuMain menu = SCR_DeployMenuMain.GetDeployMenu();
		if (menu && button) menu.AICF_SelectCharacter(button.m_AICFCharacter, button.m_sAICFCharacterName);
	}
}

modded class SCR_LoadoutRequestUIComponent
{
	protected bool m_bAICFCharacterPreview;
	protected IEntity m_AICFPreviewSource;
	protected ResourceName m_sAICFPreviewPrefab;
	protected bool m_bAICFPersonalPreviewActive;
	protected bool m_bAICFNativePreviewVisible;

	ItemPreviewWidget AICF_GetSelectedLoadoutImage()
	{
		// GetImageWidget у карточки возвращает маленький значок роли.
		// Геометрия куклы принадлежит отдельному native ItemPreviewWidget.
		if (m_PreviewComp) return m_PreviewComp.GetItemPreviewWidget();
		return null;
	}

	void AICF_SetPersonalPreviewActive(bool active)
	{
		bool restore = !active && m_bAICFPersonalPreviewActive;
		if (active && !m_bAICFPersonalPreviewActive && m_wLoadoutPreview)
			m_bAICFNativePreviewVisible = m_wLoadoutPreview.IsVisible();
		if (!active && m_bAICFPersonalPreviewActive && m_wLoadoutPreview)
			m_wLoadoutPreview.SetVisible(m_bAICFNativePreviewVisible);
		m_bAICFPersonalPreviewActive = active;
		if (active && m_wLoadoutPreview) m_wLoadoutPreview.SetVisible(true);
		if (restore && m_PlyLoadoutComp)
		{
			// Native Refresh пропускает preview для KEYBOARD; после освобождения
			// личной модели явно возвращаем штатный источник для любого ввода.
			SCR_BasePlayerLoadout role = m_PlyLoadoutComp.GetLoadout();
			if (role && m_PreviewComp) m_PreviewComp.SetPreviewedLoadout(role);
			RefreshLoadoutPreview();
		}
	}

	void AICF_SyncCharacters(SCR_PlayerController player, RplId selected)
	{
		if (m_LoadoutSelector) m_LoadoutSelector.AICF_SyncCharacters(player, selected);
	}

	void AICF_ClearCharacters()
	{
		if (m_LoadoutSelector) m_LoadoutSelector.AICF_ClearCharacters();
	}

	void AICF_ClearPersonalCard()
	{
		if (m_LoadoutSelector) m_LoadoutSelector.AICF_ClearPersonalCard();
	}

	void AICF_SyncPersonalCard(bool characterSelected, bool busy)
	{
		SCR_PlayerController player = SCR_PlayerController.Cast(GetGame().GetPlayerController());
		if (!m_LoadoutSelector || !m_PlyLoadoutComp || !player) return;
		m_LoadoutSelector.AICF_SyncPersonalCard(player, m_PlyLoadoutComp.GetLoadout(), characterSelected, busy);
		if (player.AICF_PersonalSelected() && !characterSelected)
		{
			string name = AICF_Localization.Resolve("{AICF:AICF_UI_PersonalName}");
			if (m_wExpandButtonName) m_wExpandButtonName.SetText(name);
			if (m_wLoadoutNameText) m_wLoadoutNameText.SetText(name);
		}
	}

	void AICF_ClearCharacterPreview()
	{
		m_bAICFCharacterPreview = false;
		m_AICFPreviewSource = null;
		m_sAICFPreviewPrefab = string.Empty;
	}

	override protected void SetLoadoutPreview(SCR_BasePlayerLoadout loadout)
	{
		if (m_bAICFCharacterPreview || m_bAICFPersonalPreviewActive) return;
		super.SetLoadoutPreview(loadout);
	}

	void AICF_ShowCharacter(RplId character, string name, string result)
	{
		AICF_SetPersonalPreviewActive(false);
		SCR_PlayerController player = SCR_PlayerController.Cast(GetGame().GetPlayerController());
		if (!player || !m_PreviewComp) return;
		int index = player.m_aAICFSquadRespawnIds.Find(character);
		if (!player.m_aAICFSquadRespawnPrefabs.IsIndexValid(index)) return;
		RplComponent rpl = RplComponent.Cast(Replication.FindItem(character));
		IEntity entity;
		if (rpl) entity = rpl.GetEntity();
		ResourceName prefab = player.m_aAICFSquadRespawnPrefabs[index];
		ChimeraWorld world = GetGame().GetWorld();
		ItemPreviewManagerEntity manager = world.GetItemPreviewManager();
		ItemPreviewWidget preview = m_PreviewComp.GetItemPreviewWidget();
		if (!manager || !preview) return;
		if (!m_bAICFCharacterPreview || entity != m_AICFPreviewSource || prefab != m_sAICFPreviewPrefab)
		{
			// Native manager создаёт visual copy и отслеживает hierarchy экипировки.
			if (entity) manager.SetPreviewItem(preview, entity);
			else manager.SetPreviewItemFromPrefab(preview, prefab);
			m_bAICFCharacterPreview = true;
			m_AICFPreviewSource = entity;
			m_sAICFPreviewPrefab = prefab;
		}
		if (m_wLoadoutPreview) m_wLoadoutPreview.SetVisible(true);
		if (m_wExpandButtonName) m_wExpandButtonName.SetText(name);
		if (m_wLoadoutNameText) m_wLoadoutNameText.SetText(name);
		if (m_wSupplies) m_wSupplies.SetVisible(true);
		if (result.IsEmpty() || result == "OK") result = "Hint";
		if (m_wSuppliesText) m_wSuppliesText.SetText(AICF_Localization.Resolve("{AICF:AICF_UI_SquadRespawn" + result + "}"));
	}

	override protected void OnRequestPlayerLoadout(SCR_LoadoutButton loadoutBtn)
	{
		SCR_DeployMenuMain menu = SCR_DeployMenuMain.GetDeployMenu();
		if (loadoutBtn && loadoutBtn.m_bAICFPersonalPreset)
		{
			if (menu) menu.AICF_SelectPersonalPreset(true);
			return;
		}
		if (menu && !menu.AICF_SelectPersonalPreset(false)) return;
		if (menu) menu.AICF_ClearCharacter();
		super.OnRequestPlayerLoadout(loadoutBtn);
	}
}

modded class SCR_DeployMenuMain
{
	protected RplId m_AICFSelectedCharacter = RplId.Invalid();
	protected int m_iAICFSelectedDeathRevision;
	protected string m_sAICFSelectedName;
	protected float m_fAICFRefresh;
	protected ref AICF_PersonalLoadoutUI m_AICFPersonalUI;
	protected float m_fAICFPersonalRefresh;

	bool AICF_SelectPersonalPreset(bool personal)
	{
		if (!m_AICFPersonalUI || !m_AICFPersonalUI.SelectPreset(personal)) return false;
		AICF_ClearCharacter();
		return true;
	}

	void AICF_SelectCharacter(RplId character, string name)
	{
		if (m_AICFPersonalUI && m_AICFPersonalUI.IsBusy()) return;
		SCR_PlayerController player = SCR_PlayerController.Cast(GetGame().GetPlayerController());
		if (!player || !player.m_aAICFSquadRespawnIds.Contains(character)) return;
		if (m_AICFPersonalUI) m_AICFPersonalUI.ClearPreview();
		if (player.m_sAICFSquadRespawnResult != "PENDING") player.m_sAICFSquadRespawnResult = string.Empty;
		m_AICFSelectedCharacter = character;
		m_iAICFSelectedDeathRevision = player.m_iAICFSquadListDeathRevision;
		m_sAICFSelectedName = name;
		UpdateRespawnButton();
	}

	void AICF_ClearCharacter()
	{
		m_AICFSelectedCharacter = RplId.Invalid();
		if (m_LoadoutRequestUIHandler) m_LoadoutRequestUIHandler.AICF_ClearCharacterPreview();
		if (m_LoadoutRequestUIHandler) m_LoadoutRequestUIHandler.RefreshLoadoutPreview();
		if (m_RespawnButton)
		{
			m_RespawnButton.SetSuppliesEnabled(m_bSuppliesEnabled);
			m_RespawnButton.SetText(m_bCanRespawnAtSpawnPoint, FALLBACK_DEPLOY_STRING);
			UpdateRespawnButton();
		}
	}

	override void OnMenuUpdate(float tDelta)
	{
		super.OnMenuUpdate(tDelta);
		if (!m_AICFPersonalUI) m_AICFPersonalUI = new AICF_PersonalLoadoutUI();
		m_fAICFPersonalRefresh -= tDelta;
		if (m_fAICFPersonalRefresh <= 0)
		{
			m_fAICFPersonalRefresh = 0.25;
			ItemPreviewWidget loadoutImage;
			if (m_LoadoutRequestUIHandler) loadoutImage = m_LoadoutRequestUIHandler.AICF_GetSelectedLoadoutImage();
			m_AICFPersonalUI.Update(GetRootWidget(), m_AICFSelectedCharacter.IsValid(), loadoutImage);
			if (m_LoadoutRequestUIHandler) m_LoadoutRequestUIHandler.AICF_SetPersonalPreviewActive(m_AICFPersonalUI.IsPreviewVisible());
			if (m_LoadoutRequestUIHandler) m_LoadoutRequestUIHandler.AICF_SyncPersonalCard(m_AICFSelectedCharacter.IsValid(), m_AICFPersonalUI.IsBusy());
		}
		SCR_PlayerController player = SCR_PlayerController.Cast(GetGame().GetPlayerController());
		if (!player || !m_LoadoutRequestUIHandler) return;
		m_fAICFRefresh -= tDelta;
		if (m_fAICFRefresh <= 0)
		{
			m_fAICFRefresh = 2;
			player.AICF_RequestSquadRespawnList();
		}
		if (m_AICFSelectedCharacter.IsValid() && (!player.m_aAICFSquadRespawnIds.Contains(m_AICFSelectedCharacter) || m_iAICFSelectedDeathRevision != player.m_iAICFSquadListDeathRevision))
			AICF_ClearCharacter();
		m_LoadoutRequestUIHandler.AICF_SyncCharacters(player, m_AICFSelectedCharacter);
		if (m_AICFSelectedCharacter.IsValid()) UpdateRespawnButton();
	}

	override protected void UpdateRespawnButton()
	{
		if (!m_AICFSelectedCharacter.IsValid()) { super.UpdateRespawnButton(); return; }
		SCR_PlayerController player = SCR_PlayerController.Cast(GetGame().GetPlayerController());
		if (!player || !m_RespawnButton) return;
		int remaining;
		if (m_PlayerRespawnTimer) remaining = m_PlayerRespawnTimer.GetPlayerRemainingTime(m_iPlayerId);
		bool enabled = remaining <= 0 && !m_bRespawnRequested && player.m_sAICFSquadRespawnResult != "PENDING" && !AICF_GroupRuntime.IsAliveCharacter(player.GetControlledEntity());
		m_RespawnButton.SetEnabled(enabled);
		m_RespawnButton.SetSuppliesEnabled(false);
		string label = AICF_Localization.Resolve("{AICF:AICF_UI_SquadRespawnTake}");
		if (remaining > 0) label = string.Format("%1 (%2)", label, remaining);
		m_RespawnButton.SetText(enabled, label);
		if (m_LoadoutRequestUIHandler) m_LoadoutRequestUIHandler.AICF_ShowCharacter(m_AICFSelectedCharacter, m_sAICFSelectedName, player.m_sAICFSquadRespawnResult);
	}

	override protected void RequestRespawn()
	{
		if (m_AICFPersonalUI && m_AICFPersonalUI.IsBusy()) return;
		if (!m_AICFSelectedCharacter.IsValid()) { super.RequestRespawn(); return; }
		SCR_PlayerController player = SCR_PlayerController.Cast(GetGame().GetPlayerController());
		UpdateRespawnButton();
		if (!player || !m_RespawnButton.IsEnabled()) return;
		if (!player.m_aAICFSquadRespawnIds.Contains(m_AICFSelectedCharacter) || m_iAICFSelectedDeathRevision != player.m_iAICFSquadListDeathRevision) return;
		player.AICF_RequestSquadRespawn(m_AICFSelectedCharacter, m_iAICFSelectedDeathRevision);
	}

	override void OnMenuClose()
	{
		if (m_AICFPersonalUI) m_AICFPersonalUI.Close();
		m_AICFPersonalUI = null;
		if (m_LoadoutRequestUIHandler) m_LoadoutRequestUIHandler.AICF_ClearPersonalCard();
		if (m_LoadoutRequestUIHandler) m_LoadoutRequestUIHandler.AICF_ClearCharacters();
		if (m_LoadoutRequestUIHandler) m_LoadoutRequestUIHandler.AICF_ClearCharacterPreview();
		m_AICFSelectedCharacter = RplId.Invalid();
		super.OnMenuClose();
	}
}
