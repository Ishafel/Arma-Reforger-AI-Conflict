// Только выбор варианта. Редактирование выполняет общий AICF_LoadoutEditor.
class AICF_PersonalLoadoutUI : ScriptedWidgetEventHandler
{
	protected ref AICF_StrategicUIController m_Style = new AICF_StrategicUIController();
	protected ref AICF_LoadoutEditor m_Editor;
	protected ref AICF_PersonalLoadoutPreview m_Preview;
	protected Widget m_wRoot;
	protected TextWidget m_wStatus;
	protected ref array<Widget> m_aButtons = {};
	protected ref array<TextWidget> m_aLabels = {};
	protected SCR_PlayerController m_Player;
	protected string m_sContext;
	protected int m_iToken;
	protected int m_iRequestedAt;
	protected int m_iRetryAt;
	protected bool m_bPending;
	protected int m_iOperation;

	bool IsBusy() { return (m_bPending && m_iOperation != 0) || (m_Editor && m_Editor.IsInputCaptured()); }
	bool IsPreviewVisible() { return m_Preview && m_Preview.IsAttached(); }
	bool SelectPreset(bool personal)
	{
		if (IsBusy()) return false;
		if (!m_Player) return !personal;
		if (personal == m_Player.AICF_PersonalSelected()) return true;
		if (m_bPending) return false;
		if (personal && !m_Player.AICF_PersonalAvailable()) return false;
		if (personal) Request(3);
		else Request(4);
		return true;
	}

	void Update(Widget parent, bool characterSelected, ItemPreviewWidget loadoutImage = null)
	{
		SCR_PlayerController player = SCR_PlayerController.Cast(GetGame().GetPlayerController());
		SCR_CampaignFaction faction;
		AICF_LoadoutRecipe context = AICF_PersonalLoadout.Context(player, faction);
		if (!context || AICF_GroupRuntime.IsAliveCharacter(player.GetControlledEntity())) { Close(); return; }
		if (m_Player != player || m_sContext != context.Encode())
		{
			Close();
			m_Player = player;
			m_sContext = context.Encode();
		}
		if (!m_wRoot)
		{
			m_wRoot = m_Style.CreateRect(parent, 0.32, 0.025, 0.74, 0.15, Color.FromSRGBA(9, 19, 28, 245), true);
			if (!m_wRoot) return;
			m_wRoot.SetZOrder(200);
			AddButton("edit", "{AICF:AICF_UI_PersonalCreate}", 0.01, 0.99);
			m_wStatus = m_Style.CreateText(m_wRoot, 0.02, 0.55, 0.98, 0.98, "", 16, Color.White);
			m_wStatus.SetTextWrapping(true);
			m_Editor = new AICF_LoadoutEditor(m_Style);
			Request(0);
		}
		m_wRoot.SetVisible(!characterSelected);
		m_Editor.Refresh();
		// Модель использует существующий viewport, не создавая новый слой UI.
		// Пока открыт редактор или выбран живой боец, его модель имеет приоритет.
		if (characterSelected || m_Editor.IsInputCaptured())
		{
			ClearPreview();
		}
		else
		{
			if (!m_Preview) m_Preview = new AICF_PersonalLoadoutPreview();
			m_Preview.Update(loadoutImage, player, context, faction);
		}
		if (m_bPending && m_Player.AICF_LoadoutToken() == m_iToken)
		{
			m_bPending = false;
			if (!m_Player.AICF_LoadoutAccepted())
			{
				m_wStatus.SetText(AICF_Localization.Resolve("{AICF:AICF_UI_PersonalUnavailable}"));
				m_iRetryAt = System.GetTickCount() + 2000;
			}
		}
		if (m_bPending && System.GetTickCount(m_iRequestedAt) > 15000)
		{
			m_bPending = false;
			m_wStatus.SetText(AICF_Localization.Resolve("{AICF:AICF_UI_PersonalUnavailable}"));
		}
		// JsonSaveContext пишет ResourceName как GUID на dedicated server и как
		// полный путь на клиенте. Сравниваем декодированную идентичность ресурса.
		bool sameContext = AICF_PersonalLoadout.SameContext(AICF_LoadoutRecipe.Decode(m_Player.AICF_PersonalContext()), context);
		if (!IsBusy() && !sameContext && System.GetTickCount() >= m_iRetryAt)
		{
			m_iRetryAt = System.GetTickCount() + 2000;
			Request(0);
		}
		bool ready = sameContext && !IsBusy() && !m_bPending && System.GetTickCount(m_iRequestedAt) > 800;
		m_aButtons[0].SetEnabled(ready);
		string editLabel = "{AICF:AICF_UI_PersonalCreate}";
		if (m_Player.AICF_PersonalAvailable()) editLabel = "{AICF:AICF_UI_PersonalEdit}";
		m_aLabels[0].SetText(AICF_Localization.Resolve(editLabel));
		if (ready && m_Player.AICF_LoadoutAccepted())
		{
			string status = "{AICF:AICF_UI_PersonalDefaultSelected}";
			if (m_Player.AICF_PersonalSelected()) status = "{AICF:AICF_UI_PersonalSelected}";
			m_wStatus.SetText(AICF_Localization.Resolve(status));
		}
	}

	protected void AddButton(string name, string text, float left, float right)
	{
		Widget frame = m_Style.CreateRect(m_wRoot, left, 0.04, right, 0.50, Color.FromSRGBA(46, 74, 91, 255), true);
		// Выше полноразмерного input панели (z=1), как в общем редакторе.
		frame.SetZOrder(12);
		TextWidget label = m_Style.CreateText(frame, 0, 0, 1, 1, text, 17, Color.White, true);
		m_aLabels.Insert(label);
		Widget input = frame.FindAnyWidget("AICF_RectInput");
		input.SetName(name);
		input.AddHandler(this);
		m_aButtons.Insert(input);
	}

	protected void Request(int operation)
	{
		if (!m_Player || m_bPending) return;
		m_iOperation = operation;
		m_iToken = m_Player.AICF_RequestLoadout(AICF_PersonalLoadout.SLOT, 0, m_Player.AICF_LoadoutRevision(), operation, m_Player.AICF_LoadoutData());
		m_bPending = m_iToken > 0;
		m_iRequestedAt = System.GetTickCount();
		m_wStatus.SetText(AICF_Localization.Resolve("{AICF:AICF_UI_Waiting_for_server_response_ded60968}"));
	}

	override bool OnClick(Widget w, int x, int y, int button)
	{
		if (button != 0 || IsBusy() || !m_aButtons.Contains(w)) return false;
		if (w.GetName() == "edit") m_Editor.Open(m_wRoot.GetParent(), AICF_PersonalLoadout.SLOT, true);
		return true;
	}

	void ClearPreview()
	{
		if (m_Preview) m_Preview.Clear();
		m_Preview = null;
	}

	void Close()
	{
		ClearPreview();
		if (m_Editor) m_Editor.Close();
		m_Editor = null;
		foreach (Widget button : m_aButtons) button.RemoveHandler(this);
		m_aButtons.Clear();
		m_aLabels.Clear();
		if (m_wRoot) m_wRoot.RemoveFromHierarchy();
		m_wRoot = null;
		m_Player = null;
		m_sContext = string.Empty;
		m_bPending = false;
	}
}
