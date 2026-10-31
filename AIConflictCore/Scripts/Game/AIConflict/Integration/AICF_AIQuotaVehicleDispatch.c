// Caller-facing admission: сервис владеет quota/body, этот синхронный token
// лишь связывает точный actor/car/seat с одним dispatch. Он не чистит seats.
class AICF_AIQuotaVehicleDispatch
{
	protected IEntity m_Actor;
	protected IEntity m_NativeVehicle;
	protected BaseCompartmentSlot m_Seat;
	protected bool m_bDelegated;
	protected bool m_bEnded;
	protected AICF_AIWorldQuotaService m_Service;
	protected ref AICF_AIQuotaVehicleEntry m_Entry;
	protected ref AICF_AIQuotaVehiclePermit m_Permit;

	static AICF_AIWorldQuotaService FindPolicy(IEntity actor)
	{
		if (!Replication.IsServer() || !GetGame() || !GetGame().GetWorld()) return null;
		if (actor && actor.GetWorld() != GetGame().GetWorld()) return null;
		AICF_AIWorldQuotaService service = AICF_AIWorldQuotaService.GetAuthorityForWorld(GetGame().GetWorld());
		if (!service) return null;
		if (!actor) return service;
		ChimeraCharacter character = ChimeraCharacter.Cast(actor);
		if (character && character.GetCharacterController() && character.GetCharacterController().IsPlayerControlled()) return null;
		PlayerManager players = GetGame().GetPlayerManager();
		if (players && players.GetPlayerIdFromControlledEntity(actor) > 0) return null;
		if (SCR_PossessingManagerComponent.GetPlayerIdFromMainEntity(actor) > 0) return null;
		return service;
	}

	static bool Peek(IEntity actor, IEntity vehicle, BaseCompartmentSlot seat)
	{
		AICF_AIWorldQuotaService service = FindPolicy(actor);
		if (!service) return true;
		AICF_AIQuotaVehicleEntry entry = AICF_AIQuotaVehicleEntry.Capture(actor, vehicle, seat);
		return entry && service.PeekVehicleEntry(entry) && entry.IsCurrent() && FindPolicy(actor) == service;
	}

	static AICF_AIQuotaVehicleDispatch Begin(IEntity actor, IEntity vehicle, BaseCompartmentSlot seat)
	{
		AICF_AIQuotaVehicleDispatch dispatch = new AICF_AIQuotaVehicleDispatch();
		dispatch.m_Actor = actor;
		dispatch.m_NativeVehicle = vehicle;
		dispatch.m_Seat = seat;
		dispatch.m_Service = FindPolicy(actor);
		if (!dispatch.m_Service)
		{
			dispatch.m_bDelegated = true;
			return dispatch;
		}
		dispatch.m_Entry = AICF_AIQuotaVehicleEntry.Capture(actor, vehicle, seat);
		if (!dispatch.m_Entry) return null;
		dispatch.m_Permit = dispatch.m_Service.BeginVehicleEntry(dispatch.m_Entry);
		if (!dispatch.m_Permit) return null;
		if (!dispatch.IsCurrent())
		{
			dispatch.Cancel();
			return null;
		}
		return dispatch;
	}

	bool IsCurrent()
	{
		if (m_bEnded) return false;
		if (m_bDelegated) return !FindPolicy(m_Actor);
		return m_Service && FindPolicy(m_Actor) == m_Service && m_Entry && m_Entry.IsCurrent() &&
			m_Permit && m_Service.IsVehicleEntryPermitCurrent(m_Permit, m_Entry);
	}

	bool Matches(IEntity actor, IEntity vehicle, BaseCompartmentSlot seat)
	{
		return actor == m_Actor && vehicle == m_NativeVehicle && seat == m_Seat && IsCurrent();
	}

	void MarkDispatched()
	{
		if (!m_bDelegated && !m_bEnded && m_Service && m_Permit)
			m_Service.MarkVehicleEntryDispatched(m_Permit);
	}

	void ObserveNativeResult(bool accepted)
	{
		if (!m_bDelegated && !m_bEnded && m_Service && m_Permit)
			m_Service.ObserveVehicleEntryResult(m_Permit, accepted);
	}

	void Cancel()
	{
		if (m_bEnded) return;
		m_bEnded = true;
		if (!m_bDelegated && m_Service && m_Permit) m_Service.CancelVehicleEntry(m_Permit);
	}
}
