// Кукла выбранного комплекта. Рецепт берётся из owner snapshot, а не из
// несохранённого редактора; inventory собирается только в собственном world.
class AICF_PersonalLoadoutPreview
{
	protected ref AICF_LoadoutDraft m_Draft = new AICF_LoadoutDraft();
	protected ItemPreviewWidget m_wPreview;
	protected ItemPreviewManagerEntity m_Manager;
	protected SCR_PlayerController m_Player;
	protected string m_sKey;
	protected string m_sName;
	protected bool m_bReady;

	string GetName() { return m_sName; }
	bool IsAttached() { return m_bReady && m_wPreview; }

	bool Update(ItemPreviewWidget widget, SCR_PlayerController player, AICF_LoadoutRecipe context, SCR_CampaignFaction faction, bool force = false)
	{
		if (!widget || !player || !context || !faction) { Clear(); return false; }
		AICF_LoadoutRecipe recipe = context;
		string rules;
		bool personal = player.AICF_PersonalSelected();
		if (personal)
		{
			recipe = AICF_LoadoutRecipe.Decode(player.AICF_LoadoutData());
			if (!player.AICF_PersonalAvailable() || player.AICF_LoadoutSlot() != AICF_PersonalLoadout.SLOT ||
				!AICF_PersonalLoadout.SameContext(AICF_LoadoutRecipe.Decode(player.AICF_PersonalContext()), context) ||
				!AICF_PersonalLoadout.SameContext(recipe, context) || !recipe.HasValidBounds())
			{
				Clear();
				return false;
			}
			rules = player.AICF_LoadoutLibrary();
		}
		string key = string.Format("%1|%2|%3", personal, recipe.Encode(), rules);
		if (m_Player != player || key != m_sKey)
		{
			Detach();
			m_Player = player;
			m_sKey = key;
			m_sName = "{AICF:AICF_UI_PersonalDefault}";
			if (personal) m_sName = recipe.m_sName;
			AICF_LoadoutCatalog catalog = new AICF_LoadoutCatalog(faction);
			if (personal) catalog.SetPersonalRules(rules);
			string reason;
			m_bReady = m_Draft.Build(recipe, catalog, reason);
			if (!m_bReady)
			{
				m_Draft.Clear();
				Print(string.Format("[AICF][PERSONAL_LOADOUT_PREVIEW] ready=0 personal=%1 reason=%2", personal, reason), LogLevel.WARNING);
			}
			force = true;
		}
		if (!m_bReady) return false;
		IEntity character = m_Draft.GetCharacter();
		if (!character || character.GetWorld() == GetGame().GetWorld()) { Clear(); return false; }
		if (m_wPreview != widget)
		{
			Detach();
			m_wPreview = widget;
			force = true;
		}
		if (force)
		{
			// Native ItemPreviewWidget обновляется ItemPreviewManager кампании.
			// SetWorld вручную сбрасывается его следующим обновлением.
			ChimeraWorld world = GetGame().GetWorld();
			m_Manager = world.GetItemPreviewManager();
			if (!m_Manager) { Detach(); return false; }
			// Источник из отдельного draft world. Native manager сам создаёт
			// render copy; ни живой inventory, ни prefab cache не изменяются.
			m_Manager.SetPreviewItem(m_wPreview, character, null, true);
			float width, height;
			m_wPreview.GetScreenSize(width, height);
			Print(string.Format("[AICF][PERSONAL_LOADOUT_PREVIEW] ready=1 personal=%1 revision=%2 isolated=1 renderer=NATIVE_MANAGER",
				personal, player.AICF_LoadoutRevision()));
			Print(string.Format("[AICF][PERSONAL_LOADOUT_PREVIEW] placement=NATIVE_WIDGET overlay=0 width=%1 height=%2", width, height));
		}
		return true;
	}

	void Detach()
	{
		// Native widget принадлежит меню; связь освобождаем тем же manager.
		if (m_Manager && m_wPreview) m_Manager.SetPreviewItem(m_wPreview, null);
		m_Manager = null;
		m_wPreview = null;
	}

	void Clear()
	{
		Detach();
		m_Draft.Clear();
		m_Player = null;
		m_sKey = string.Empty;
		m_sName = string.Empty;
		m_bReady = false;
	}

	void ~AICF_PersonalLoadoutPreview()
	{
		Clear();
	}
}
