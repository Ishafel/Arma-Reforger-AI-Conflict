// Намерение проходит через принадлежащий игроку controller. Снимок ответа
// хранится на authority и переживает закрытие карты/повторный stream controller.
modded class SCR_PlayerController
{
	[RplProp(condition: RplCondition.OwnerOnly)]
	protected int m_iAICFSupplyRequest;
	[RplProp(condition: RplCondition.OwnerOnly)]
	protected bool m_bAICFSupplyBusy;
	[RplProp(condition: RplCondition.OwnerOnly)]
	protected string m_sAICFSupplyStatus;
	[RplProp(condition: RplCondition.OwnerOnly)]
	protected string m_sAICFSupplySource;
	[RplProp(condition: RplCondition.OwnerOnly)]
	protected string m_sAICFSupplyDestination;
	protected int m_iAICFSupplySent;
	protected int m_iAICFSupplyRateAtMs;

	bool AICF_IsSupplyBusy()
	{
		return m_bAICFSupplyBusy || AICF_IsSupplyRequestPending();
	}

	// Ожидание ответа блокирует повторный клик; принятый рейс — только статус.
	bool AICF_IsSupplyRequestPending()
	{
		return m_iAICFSupplySent > m_iAICFSupplyRequest;
	}

	string AICF_GetSupplyStatus()
	{
		if (m_iAICFSupplySent > m_iAICFSupplyRequest)
			return "{AICF:AICF_UI_Waiting_for_server_response_ded60968}";
		if (m_sAICFSupplySource.IsEmpty()) return m_sAICFSupplyStatus;
		return WidgetManager.Translate(m_sAICFSupplySource) + " → " + WidgetManager.Translate(m_sAICFSupplyDestination) + "\n" + m_sAICFSupplyStatus;
	}

	void AICF_SetSupplyRoute(string source, string destination)
	{
		if (!Replication.IsServer() || (source == m_sAICFSupplySource && destination == m_sAICFSupplyDestination)) return;
		m_sAICFSupplySource = source;
		m_sAICFSupplyDestination = destination;
		Replication.BumpMe();
	}

	void AICF_RequestSupplyTransport(RplId source, RplId destination, int amount)
	{
		if (this != GetGame().GetPlayerController() || AICF_IsSupplyRequestPending()) return;
		m_iAICFSupplySent = m_iAICFSupplyRequest + 1;
		Rpc(RpcAsk_AICFSupplyTransport, m_iAICFSupplySent, source, destination, amount);
	}

	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_AICFSupplyTransport(int request, RplId source, RplId destination, int amount)
	{
		if (!Replication.IsServer() || request <= m_iAICFSupplyRequest) return;
		AICF_SetSupplyRoute(string.Empty, string.Empty);
		int now = System.GetTickCount();
		if (now < m_iAICFSupplyRateAtMs)
		{
			AICF_SetSupplyStatus(request, false, "{AICF:AICF_UI_Too_many_requests_Try_again_in_a_few_secon_6deb233f}");
			return;
		}
		m_iAICFSupplyRateAtMs = now + 2000;
		AICF_MatchController match = AICF_MatchController.GetActiveController();
		string reason = "{AICF:AICF_UI_Supply_service_is_not_ready_yet_f2e2a91b}";
		if (match && match.RequestSupplyTransport(this, request, source, destination, amount, reason)) return;
		AICF_SetSupplyStatus(request, false, reason);
	}

	void AICF_SetSupplyStatus(int request, bool busy, string status)
	{
		// Форма показывает последнюю заявку. Старые рейсы продолжают работу,
		// но их обновления и завершение не подменяют её ответ.
		if (!Replication.IsServer() || request < m_iAICFSupplyRequest) return;
		if (request == m_iAICFSupplyRequest && busy == m_bAICFSupplyBusy && status == m_sAICFSupplyStatus) return;
		m_iAICFSupplyRequest = request;
		m_bAICFSupplyBusy = busy;
		m_sAICFSupplyStatus = status;
		Replication.BumpMe();
	}
}
