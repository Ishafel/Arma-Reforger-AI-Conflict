// Controlled native fault только в isolated retained repeat5: одна actual
// RUNNING entry вдали от машины временно переводится в native MaxLOD.
// Source actors/vehicle/health/pose не меняются. Исходный LOD возвращается
// до первой native retry либо при отмене/Stop; native boarding readback обязателен.
class AICF_ReboardNativeRetryProbe
{
	static AICF_VehicleBehaviorAction s_Token;
	static AIAgent s_Agent;
	static SCR_AIGetInVehicle s_Entry;
	static EntityID s_ActorId;
	static string s_sActorRpl;
	static vector s_vPausedAt;
	static int s_iOriginalPermanentLOD;
	static int s_iPausedAt;
	static int s_iOriginalDeadline;
	static int s_iStalledPolls;
	static bool s_bInjected;
	static bool s_bReleased;
	static bool s_bRetryIssued;
	static bool s_bSettled;
	static bool s_bInvalid;

	static bool Enabled()
	{
		string enabled;
		return Replication.IsServer() && System.GetCLIParam("aicfReboardNativeRetryProbe", enabled) && enabled == "1";
	}

	static void BeforeObservation(AICF_VehicleBehaviorAction token)
	{
		if (!Enabled() || !token || !token.ResolveOwner()) return;
		if (s_Token == token && !s_bReleased) return;
		if (s_bInjected || !token.m_Trip || token.m_Trip.GetPhase() != AICF_ETransportTripPhase.TACTICAL_DEPLOYMENT ||
			!token.m_Trip.GetBehaviorState().m_bReboarding || token.m_iRetries != 0 || token.m_iShortAttempts != 0 ||
			!token.m_Entry || token.m_Entry.GetActionState() != EAIActionState.RUNNING || !token.m_Seat || token.m_Seat.GetOccupant() ||
			token.m_Seat.GetType() != ECompartmentType.CARGO || token.m_Seat.GetAttachedTurret()) return;
		SCR_AIUtilityComponent utility = token.ResolveOwner();
		if (!token.OwnsAction(utility, token.m_Entry) || utility.GetCurrentAction() != token.m_Entry || utility.GetExecutedAction() != token.m_Entry) return;
		float distance = vector.Distance(token.m_Actor.GetOrigin(), token.m_Seat.GetPosition());
		if (distance < 30 || distance > 120 || AICF_VehicleShortRecovery.CanSeat(token.m_Trip, token.m_Actor, token.m_Seat)) return;
		ChimeraCharacter character = ChimeraCharacter.Cast(token.m_Actor);
		CompartmentAccessComponent access = character.GetCompartmentAccessComponent();
		AIAgent agent = character.GetAIControlComponent().GetControlAIAgent();
		if (!agent || !agent.IsAIActivated() || !access || access.IsInCompartment() || access.IsGettingIn() || access.IsGettingOut() ||
			access.IsSwitchingSeatsAnim() || character.IsInVehicle() || CompartmentAccessComponent.GetVehicleIn(character)) return;
		s_Token = token; s_Agent = agent; s_Entry = token.m_Entry;
		s_ActorId = token.m_ActorId; s_sActorRpl = token.m_sActorRpl; s_vPausedAt = character.GetOrigin();
		s_iOriginalPermanentLOD = agent.GetPermanentLOD(); s_iOriginalDeadline = token.m_Trip.GetBehaviorState().m_iReboardDeadline;
		s_iPausedAt = System.GetTickCount(); s_bInjected = true;
		agent.SetPermanentLOD(AIAgent.GetMaxLOD());
		agent.DeactivateAI();
		Print(string.Format("[AICF][REBOARD_NATIVE_RETRY_PAUSED] member=%1 actor_rpl=%2 seat=%3 distance_m=%4 before_lod=%5 active_after=%6 deadline_ms=%7 controlled_native_lod=1 pose_writes=0 health_writes=0",
			s_ActorId, s_sActorRpl, AICF_VehiclePreset.SeatKey(token.m_Seat), distance, s_iOriginalPermanentLOD, agent.IsAIActivated(), s_iOriginalDeadline));
	}

	// Observe the unmodified production decision and its actual progress clock.
	// Injection can follow native action selection after that clock has started.
	static void AfterObservation(AICF_VehicleBehaviorAction token, bool blocked)
	{
		if (!Enabled() || !blocked || token != s_Token || s_bReleased || !s_Agent || !token.ResolveOwner()) return;
		int nowMs = System.GetTickCount();
		int progressAge = nowMs - token.m_iLastProgressAt;
		if (nowMs <= s_iPausedAt || progressAge < 2000 || s_Agent.IsAIActivated() ||
			token.m_Entry != s_Entry || !token.OwnsAction(token.ResolveOwner(), s_Entry) ||
			s_Entry.GetActionState() != EAIActionState.RUNNING ||
			vector.Distance(token.m_Actor.GetOrigin(), s_vPausedAt) > 0.5 ||
			vector.Distance(token.m_Actor.GetOrigin(), token.m_Seat.GetPosition()) <= 25) return;
		s_iStalledPolls++;
		Print(string.Format("[AICF][REBOARD_NATIVE_RETRY_STALLED] member=%1 running=1 owned=1 motion_m=%2 distance_m=%3 elapsed_ms=%4 deadline_ms=%5 progress_age_ms=%6 actual_blocked=1",
			token.m_ActorId, vector.Distance(token.m_Actor.GetOrigin(), s_vPausedAt),
			vector.Distance(token.m_Actor.GetOrigin(), token.m_Seat.GetPosition()), nowMs - s_iPausedAt, s_iOriginalDeadline, progressAge));
	}

	static void Release(AICF_VehicleBehaviorAction token, string reason)
	{
		if (!s_bInjected || s_bReleased || !token || token != s_Token) return;
		if (!s_Agent || !token.ResolveOwner() || token.m_Actor.GetID() != s_ActorId ||
			AICF_VehicleBehaviorBinding.ComponentRpl(token.m_Actor) != s_sActorRpl || s_Agent.GetControlledEntity() != token.m_Actor)
		{
			s_bInvalid = true;
			return;
		}
		s_Agent.SetPermanentLOD(s_iOriginalPermanentLOD);
		bool activated = s_Agent.ActivateAI(false);
		s_bReleased = true;
		if (!activated || !s_Agent.IsAIActivated()) s_bInvalid = true;
		Print(string.Format("[AICF][REBOARD_NATIVE_RETRY_RELEASED] member=%1 reason=%2 native_retry=%3 restored_lod=%4 active=%5 stalled_polls=%6 deadline_unchanged=%7",
			s_ActorId, reason, token.m_iRetries, s_Agent.GetPermanentLOD(), s_Agent.IsAIActivated(), s_iStalledPolls,
			token.m_Trip.GetBehaviorState().m_iReboardDeadline == s_iOriginalDeadline));
	}
}

modded class AICF_VehicleBehaviorAction
{
	override bool EntryBlocked()
	{
		AICF_ReboardNativeRetryProbe.BeforeObservation(this);
		bool blocked = super.EntryBlocked();
		AICF_ReboardNativeRetryProbe.AfterObservation(this, blocked);
		return blocked;
	}
}

modded class AICF_VehicleBehaviorFlow
{
	override protected void IssueEntry(AICF_TransportTrip trip, AICF_VehicleBehaviorAction token, IEntity actor, BaseCompartmentSlot seat)
	{
		if (AICF_ReboardNativeRetryProbe.Enabled() && AICF_ReboardNativeRetryProbe.s_Token == token && token.m_iRetries > 0)
			AICF_ReboardNativeRetryProbe.Release(token, "BEFORE_NATIVE_RETRY");
		super.IssueEntry(trip, token, actor, seat);
		if (AICF_ReboardNativeRetryProbe.s_bReleased && AICF_ReboardNativeRetryProbe.s_Token == token && token.m_iRetries > 0 &&
			token.m_Entry && token.OwnsAction(token.ResolveOwner(), token.m_Entry))
		{
			AICF_ReboardNativeRetryProbe.s_bRetryIssued = true;
			Print(string.Format("[AICF][REBOARD_NATIVE_RETRY_ISSUED] member=%1 native_retry=%2 owned=1 distance_m=%3 physical_relocation=0 deadline_unchanged=%4",
				actor.GetID(), token.m_iRetries, vector.Distance(actor.GetOrigin(), seat.GetPosition()),
				trip.GetBehaviorState().m_iReboardDeadline == AICF_ReboardNativeRetryProbe.s_iOriginalDeadline));
		}
	}

	override void CancelEntries(AICF_TransportTrip trip)
	{
		if (AICF_ReboardNativeRetryProbe.s_Token && AICF_ReboardNativeRetryProbe.s_Token.m_Trip == trip)
			AICF_ReboardNativeRetryProbe.Release(AICF_ReboardNativeRetryProbe.s_Token, "PHASE_CANCEL");
		super.CancelEntries(trip);
	}
}

modded class AICF_MatchController
{
	override protected void Update()
	{
		super.Update();
		if (!AICF_ReboardNativeRetryProbe.Enabled() || m_bVSDone) return;
		AICF_VehicleBehaviorAction token = AICF_ReboardNativeRetryProbe.s_Token;
		if (token && AICF_ReboardNativeRetryProbe.s_bRetryIssued && token.IsSettled())
			AICF_ReboardNativeRetryProbe.s_bSettled = true;
		if (m_iVSStarted > 0 && System.GetTickCount() - m_iVSStarted > 600000)
		{
			AICF_VSCheck("NATIVE_RETRY_PROBE_DEADLINE", false);
			AICF_VSFinish();
		}
	}

	override protected void AICF_VSFinish()
	{
		if (AICF_ReboardNativeRetryProbe.Enabled() && !m_bVSDone)
		{
			AICF_VSCheck("NATIVE_RETRY_ACTUAL_FAR_RUNNING_STALL", AICF_ReboardNativeRetryProbe.s_bInjected && AICF_ReboardNativeRetryProbe.s_iStalledPolls > 0);
			AICF_VSCheck("NATIVE_RETRY_OWNED_REISSUE", AICF_ReboardNativeRetryProbe.s_bRetryIssued && AICF_ReboardNativeRetryProbe.s_bReleased);
			AICF_VSCheck("NATIVE_RETRY_SAME_ACTOR_SETTLED", AICF_ReboardNativeRetryProbe.s_bSettled && !AICF_ReboardNativeRetryProbe.s_bInvalid);
		}
		super.AICF_VSFinish();
	}
}
