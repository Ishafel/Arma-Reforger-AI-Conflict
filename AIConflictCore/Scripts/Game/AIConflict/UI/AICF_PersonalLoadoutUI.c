// Только выбор варианта. Редактирование выполняет общий AICF_LoadoutEditor.
class AICF_PersonalLoadoutUI : ScriptedWidgetEventHandler
{
	protected ref AICF_StrategicUIController m_Style = new AICF_StrategicUIController();
	protected ref AICF_LoadoutEditor m_Editor;
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

	void Update(Widget parent, bool characterSelected)
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
			AddButton("default", "{AICF:AICF_UI_PersonalDefault}", 0.01, 0.42);
			AddButton("personal", "{AICF:AICF_UI_PersonalName}", 0.43, 0.67);
			AddButton("edit", "{AICF:AICF_UI_PersonalCreate}", 0.68, 0.99);
			m_wStatus = m_Style.CreateText(m_wRoot, 0.02, 0.55, 0.98, 0.98, "", 16, Color.White);
			m_wStatus.SetTextWrapping(true);
			m_Editor = new AICF_LoadoutEditor(m_Style);
			Request(0);
		}
		m_wRoot.SetVisible(!characterSelected);
		m_Editor.Refresh();
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
		if (!IsBusy() && m_Player.AICF_PersonalContext() != m_sContext && System.GetTickCount() >= m_iRetryAt)
		{
			m_iRetryAt = System.GetTickCount() + 2000;
			Request(0);
		}
		bool ready = m_Player.AICF_PersonalContext() == m_sContext && !IsBusy() && !m_bPending && System.GetTickCount(m_iRequestedAt) > 800;
		m_aButtons[0].SetEnabled(!IsBusy() && !m_bPending);
		m_aButtons[1].SetEnabled(ready && m_Player.AICF_PersonalAvailable());
		m_aButtons[2].SetEnabled(ready);
		string editLabel = "{AICF:AICF_UI_PersonalCreate}";
		if (m_Player.AICF_PersonalAvailable()) editLabel = "{AICF:AICF_UI_PersonalEdit}";
		m_aLabels[2].SetText(AICF_Localization.Resolve(editLabel));
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
		else if (w.GetName() == "personal") Request(3);
		else if (w.GetName() == "default") Request(4);
		return true;
	}

	void Close()
	{
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
