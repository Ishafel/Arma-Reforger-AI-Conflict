// Только ProofVersion4 stage. Наблюдение реальной боевой потери не подменяет full route.
class AICF_SoakObservationCleanup
{
	static ref array<ref AICF_SoakObservationCleanup> s_aReceipts = {};
	string m_sOperation;
	string m_sRpl;
	EntityID m_Id;
	int m_iGroupGeneration;
	int m_iTripGeneration;
	int m_iLeaseGeneration;
	int m_iVehicleGeneration;
	int m_iConfirmedAt;
	int m_iTerminalAt;
	bool m_bAbsent;
	bool m_bCompleted;

	static void Capture(AICF_VehicleCleanupJob job, int terminalAt)
	{
		if (!job || !job.m_Snapshot || !job.m_State || !GetGame() || !GetGame().GetWorld()) return;
		AICF_SoakObservationCleanup receipt = new AICF_SoakObservationCleanup();
		receipt.m_sOperation = job.m_sOperationId;
		receipt.m_sRpl = job.m_Snapshot.GetLastRplId();
		receipt.m_Id = job.m_Snapshot.GetLastEntityId();
		receipt.m_iGroupGeneration = job.m_Snapshot.GetGroupGeneration();
		receipt.m_iTripGeneration = job.m_Snapshot.GetTripGeneration();
		receipt.m_iLeaseGeneration = job.m_Snapshot.GetLeaseGeneration();
		receipt.m_iVehicleGeneration = job.m_Snapshot.GetVehicleGeneration();
		receipt.m_iConfirmedAt = System.GetTickCount();
		receipt.m_iTerminalAt = terminalAt;
		receipt.m_bAbsent = !GetGame().GetWorld().FindEntityByID(receipt.m_Id);
		receipt.m_bCompleted = job.m_bCompleted && !job.m_State.IsStoppedFailClosed();
		s_aReceipts.Insert(receipt);
	}
}

modded class AICF_VehicleCleanupManager
{
	override protected void ReportCleanupConfirmed(AICF_VehicleCleanupJob job, string reason)
	{
		super.ReportCleanupConfirmed(job, reason);
		string enabled;
		if (System.GetCLIParam("aicfVehicleSoakObservation4", enabled) && enabled == "1" && job)
			AICF_SoakObservationCleanup.Capture(job, job.m_iProtectedClearanceDeadlineAtMs + PROTECTED_CLEARANCE_TERMINAL_GRACE_MS);
	}
}

modded class AICF_MatchController
{
	protected bool m_bSoakObservationLoss;
	protected bool m_bSoakObservationComplete;
	protected bool m_bSoakObservationCleanup;
	protected bool m_bSoakObservationReplacement;
	protected bool m_bSoakObservationBroken;
	protected bool m_bSoakObservationCapturedDeath;
	protected int m_iSoakObservationDamageAt;
	protected int m_iSoakObservationIntent;
	protected vector m_vSoakObservationTarget;
	protected ref AICF_VehicleLease m_SoakObservationLease;
	protected string m_sSoakObservationCarRpl;
	protected ref array<ChimeraCharacter> m_aSoakObservationActors = {};
	protected ref array<string> m_aSoakObservationActorRpl = {};
	protected ref array<string> m_aSoakObservationComponentRpl = {};
	protected ref array<int> m_aSoakObservationLastLife = {};
	protected ref array<bool> m_aSoakObservationDetached = {};
	protected vector m_vSoakObservationDamageLeader;
	protected EntityID m_SoakObservationSurvivorLeaderId;
	protected vector m_vSoakObservationSurvivorStart;
	protected int m_iSoakObservationSurvivorAt;
	protected EntityID m_SoakObservationReplacementId;
	protected EntityID m_SoakObservationReplacementLeaderId;
	protected vector m_vSoakObservationReplacementStart;
	protected int m_iSoakObservationReplacementAt;
	protected ref array<EntityID> m_aSoakObservationReplacementMembers = {};

	protected bool AICF_SoakObservationEnabled()
	{
		string enabled;
		return System.GetCLIParam("aicfVehicleSoakObservation4", enabled) && enabled == "1";
	}

	protected AICF_GroupSlot AICF_SoakObservationSlot()
	{
		if (m_sVehicleSoakFaction == "US") return m_USState.GetSlot(0);
		if (m_sVehicleSoakFaction == "USSR") return m_USSRState.GetSlot(0);
		return null;
	}

	protected void AICF_SoakObservationCaptureActors()
	{
		AICF_GroupSlot slot = AICF_SoakObservationSlot();
		if (!slot || m_aVehicleSoakMembers.Count() != 5 || m_aSoakObservationActors.Count() != 0) return;
		for (int index; index < 5; index++)
		{
			ChimeraCharacter actor = ChimeraCharacter.Cast(slot.GetRosterMember(index));
			if (!actor || actor.GetID() != m_aVehicleSoakMembers[index]) { m_bSoakObservationBroken = true; return; }
			string rpl = Replication.FindItemId(actor).ToString();
			RplComponent component = RplComponent.Cast(actor.FindComponent(RplComponent));
			if (!component || !component.Id().IsValid()) { m_bSoakObservationBroken = true; return; }
			m_aSoakObservationActors.Insert(actor);
			m_aSoakObservationActorRpl.Insert(rpl);
			m_aSoakObservationComponentRpl.Insert(component.Id().ToString());
			m_aSoakObservationLastLife.Insert(-1);
			m_aSoakObservationDetached.Insert(false);
			AICF_VehicleSoakLog("OBS_ACTOR_CAPTURE", string.Format("position=%1 member=%2 actor_rpl=%3 native_capture=1 component_rpl=%4", index, actor.GetID(), rpl, component.Id()));
		}
	}

	override protected void AICF_VehicleSoakLog(string eventName, string details)
	{
		if (AICF_SoakObservationEnabled())
		{
			details.Replace("proof_version=3", "proof_version=4");
			if (eventName == "START")
			{
				details += " observation_policy=ACTUAL_DAMAGE_BOUNDED_RECOVERY original_full_route_separate=1 captured_death_policy=TYPED_NATIVE_ALL_DEAD_RELEASE_V1";
			}
			if (eventName == "RECRUITED") AICF_SoakObservationCaptureActors();
			if (eventName == "BOARDED" && m_VehicleSoakObservedTrip && m_VehicleSoakObservedTrip.GetLease())
			{
				m_SoakObservationLease = m_VehicleSoakObservedTrip.GetLease();
				m_sSoakObservationCarRpl = m_SoakObservationLease.GetRplId();
			}
			if (eventName == "DONE")
			{
				bool valid = !m_bSoakObservationBroken && (m_bVehicleSoakFootArrived || m_bSoakObservationComplete);
				string category = "UNPROVEN";
				if (m_bVehicleSoakFootArrived) category = "ORIGINAL_FULL_ROUTE";
				else if (m_bSoakObservationComplete && m_bSoakObservationCapturedDeath) category = "CONFIRMED_NATIVE_ALL_DEAD_RETIREMENT";
				else if (m_bSoakObservationComplete) category = "EXPLAINED_NATIVE_DAMAGE_RECOVERY";
				details += string.Format(" observation_complete=%1 observation_category=%2 original_full_route=%3 proof_version=4", valid, category, m_bVehicleSoakFootArrived);
			}
		}
		super.AICF_VehicleSoakLog(eventName, details);
	}

	// Typed production receipt owns the native DEAD/cause/cleanup proof. A missing
	// receipt, wrong member component identity or retained cleanup is never a loss PASS.
	protected bool AICF_SoakObservationAcceptCapturedDeath()
	{
		AICF_TransportTrip trip = m_VehicleSoakObservedTrip;
		if (!trip || !m_SoakObservationLease || m_aSoakObservationComponentRpl.Count() != 5 ||
			trip.GetOperationId() != m_sVehicleSoakTrip || trip.GetFactionKey() != m_sVehicleSoakFaction || trip.GetSlotId() != 0 ||
			trip.GetGroupGeneration() != m_iVehicleSoakGeneration || m_SoakObservationLease.GetEntityId() != m_VehicleSoakVehicleId ||
			m_SoakObservationLease.GetRplId() != m_sSoakObservationCarRpl) return false;
		AICF_TripRetirementObservation evidence = trip.ObserveCapturedDeadRetirement();
		if (!evidence || !evidence.Matches(trip) ||
			(evidence.GetStatus() != AICF_ETripRetirementObservation.PENDING && !evidence.IsConfirmed())) return false;
		for (int index; index < 5; index++)
			if (!evidence.MatchesCapturedActor(m_aVehicleSoakMembers[index], m_aSoakObservationComponentRpl[index])) return false;
		if (!m_bSoakObservationLoss)
		{
			m_bSoakObservationCapturedDeath = true;
			m_bSoakObservationLoss = true;
			m_iSoakObservationDamageAt = System.GetTickCount();
			m_iSoakObservationIntent = m_iVehicleSoakGoalRevision;
			m_vSoakObservationTarget = m_vVehicleSoakTarget;
			AICF_VehicleSoakLog("OBS_CAPTURED_DEATH", evidence.Details() + string.Format(" target=%1 intent_revision=%2 typed_members=5 generation=%3 group=%4", m_vSoakObservationTarget, m_iSoakObservationIntent, m_iVehicleSoakGeneration, m_VehicleSoakGroupId));
		}
		if (m_bSoakObservationCapturedDeath && !m_bSoakObservationCleanup && evidence.IsConfirmed())
		{
			m_bSoakObservationCleanup = true;
			AICF_VehicleSoakLog("OBS_CAPTURED_DEATH_RELEASE", evidence.Details() + " typed_confirmed=1");
		}
		return true;
	}

	protected bool AICF_SoakObservationAcceptDamage(string reason)
	{
		AICF_GroupSlot slot = AICF_SoakObservationSlot();
		AICF_TransportTrip trip = m_VehicleSoakObservedTrip;
		Vehicle car = m_VehicleSoakVehicle;
		if (!slot || !slot.GetGroup() || !trip || !car || !m_SoakObservationLease) return false;
		if (trip.GetLease() != m_SoakObservationLease || !AICF_VehicleBoardingMutationFence.IsAuthoritativeReplicatedEntity(car) ||
			trip.GetFactionKey() != m_sVehicleSoakFaction || trip.GetSlotId() != 0 ||
			!m_SoakObservationLease.MatchesSlotIdentity(m_sVehicleSoakFaction, 0, m_iVehicleSoakGeneration)) return false;
		if (car.GetID() != m_VehicleSoakVehicleId || m_SoakObservationLease.GetEntityId() != car.GetID() ||
			m_SoakObservationLease.GetRplId() != m_sSoakObservationCarRpl) return false;
		if (trip.GetGroupGeneration() != m_iVehicleSoakGeneration || trip.GetOperationId() != m_sVehicleSoakTrip ||
			!trip.IsTerminal() || trip.GetPhase() != AICF_ETransportTripPhase.FALLBACK || trip.GetTerminalReason() != reason ||
			m_aSoakObservationActors.Count() != 5 || slot.GetGroup().GetID() != m_VehicleSoakGroupId) return false;
		RplComponent rpl = RplComponent.Cast(car.FindComponent(RplComponent));
		SCR_DamageManagerComponent damage = SCR_DamageManagerComponent.GetDamageManager(car);
		if (!rpl || rpl.Id().ToString() != m_sSoakObservationCarRpl || !damage) return false;
		bool onFire = SCR_AIVehicleUsability.VehicleIsOnFire(car, damage);
		float movement = damage.GetMovementDamage();
		if (!onFire && (movement < 1 || SCR_AIVehicleUsability.VehicleCanMove(car, damage))) return false;
		AIWaypoint waypoint = slot.GetWaypoint();
		array<AIWaypoint> queue = {};
		slot.GetGroup().GetWaypoints(queue);
		if (!trip.GetHandoffState().IsOrderRestored() || !waypoint || !queue.Contains(waypoint) || slot.GetGroup().GetCurrentWaypoint() != waypoint) return false;
		m_bSoakObservationLoss = true;
		m_iSoakObservationDamageAt = System.GetTickCount();
		m_iSoakObservationIntent = slot.GetStrategicIntentRevision();
		m_vSoakObservationTarget = slot.GetTargetPosition();
		IEntity leader = AICF_GroupRuntime.ResolveAliveLeader(slot.GetGroup());
		if (leader) m_vSoakObservationDamageLeader = leader.GetOrigin();
		string details = string.Format("operation_id=%1 vehicle=%2 rpl_id=%3 group=%4 generation=%5 trip_generation=%6 lease_generation=%7 vehicle_generation=%8",
			m_sVehicleSoakTrip, car.GetID(), m_sSoakObservationCarRpl, m_VehicleSoakGroupId, m_iVehicleSoakGeneration,
			trip.GetTripGeneration(), m_SoakObservationLease.GetLeaseGeneration(), m_SoakObservationLease.GetVehicleGeneration());
		details += string.Format(" reason=%1 on_fire=%2 movement_damage=%3 native_health=1 handoff_restored=1 actual_current_waypoint=%4 target=%5 intent_revision=%6", reason, onFire, movement, waypoint.GetID(), m_vSoakObservationTarget, m_iSoakObservationIntent);
		AICF_VehicleSoakLog("OBS_DAMAGE", details);
		return true;
	}

	override protected void AICF_VehicleSoakFail(string reason)
	{
		if (AICF_SoakObservationEnabled())
		{
			if (!m_bSoakObservationLoss && AICF_SoakObservationAcceptCapturedDeath()) return;
			if (!m_bSoakObservationLoss && AICF_SoakObservationAcceptDamage(reason)) return;
			if (m_bSoakObservationLoss && (reason == "GROUP_UNAVAILABLE" || reason == "GROUP_REPLACED_BEFORE_ARRIVAL")) return;
			if (reason == "ROUTE_NOT_COMPLETE_AT_DURATION" && m_bSoakObservationComplete && !m_bSoakObservationBroken) return;
		}
		super.AICF_VehicleSoakFail(reason);
	}

	protected void AICF_SoakObservationActors()
	{
		for (int index; index < m_aSoakObservationActors.Count(); index++)
		{
			ChimeraCharacter actor = m_aSoakObservationActors[index];
			if (!actor || GetGame().GetWorld().FindEntityByID(actor.GetID()) != actor)
			{
				if (m_bSoakObservationCapturedDeath && m_VehicleSoakObservedTrip)
				{
					AICF_TripRetirementObservation deadEvidence = m_VehicleSoakObservedTrip.ObserveCapturedDeadRetirement();
					if (deadEvidence && deadEvidence.Matches(m_VehicleSoakObservedTrip) && deadEvidence.MatchesCapturedActor(m_aVehicleSoakMembers[index], m_aSoakObservationComponentRpl[index]))
						m_aSoakObservationLastLife[index] = ECharacterLifeState.DEAD;
				}
				if (m_aSoakObservationLastLife[index] != ECharacterLifeState.DEAD) m_bSoakObservationBroken = true;
				else if (!m_aSoakObservationDetached[index])
				{
					m_aSoakObservationDetached[index] = true;
					AICF_VehicleSoakLog("OBS_ACTOR_ABSENT", string.Format("position=%1 member=%2 actor_rpl=%3 previously_native_dead=1 exists=0 detached=1", index, m_aVehicleSoakMembers[index], m_aSoakObservationActorRpl[index]));
				}
				continue;
			}
			if (actor.GetID() != m_aVehicleSoakMembers[index] || Replication.FindItemId(actor).ToString() != m_aSoakObservationActorRpl[index] || !actor.GetCharacterController())
			{ m_bSoakObservationBroken = true; continue; }
			int life = actor.GetCharacterController().GetLifeState();
			if (m_bSoakObservationLoss && m_bSoakObservationReplacement && life == ECharacterLifeState.ALIVE) m_bSoakObservationBroken = true;
			CompartmentAccessComponent access = actor.GetCompartmentAccessComponent();
			bool detached = !actor.IsInVehicle() && !CompartmentAccessComponent.GetVehicleIn(actor) && access && !access.GetCompartment() && !access.IsInCompartment() && !access.IsGettingIn() && !access.IsGettingOut() && !access.IsSwitchingSeatsAnim();
			if (life != m_aSoakObservationLastLife[index] || detached != m_aSoakObservationDetached[index])
			{
				m_aSoakObservationLastLife[index] = life;
				m_aSoakObservationDetached[index] = detached;
				AICF_VehicleSoakLog("OBS_ACTOR_STATE", string.Format("position=%1 member=%2 actor_rpl=%3 life=%4 detached=%5 native_readback=1", index, actor.GetID(), m_aSoakObservationActorRpl[index], SCR_Enum.GetEnumName(ECharacterLifeState, life), detached));
			}
		}
	}

	protected void AICF_SoakObservationTick()
	{
		if (!m_bVehicleSoakStarted || m_bVehicleSoakDone || m_bStopped) return;
		if (!m_bSoakObservationLoss || m_bSoakObservationCapturedDeath)
		{
			bool accepted = AICF_SoakObservationAcceptCapturedDeath();
			if (m_bSoakObservationCapturedDeath && !accepted) { m_bSoakObservationBroken = true; return; }
		}
		AICF_SoakObservationActors();
		if (!m_bSoakObservationLoss || m_bSoakObservationBroken) return;
		if (!m_bSoakObservationCleanup && !m_bSoakObservationCapturedDeath)
		{
			foreach (AICF_SoakObservationCleanup receipt : AICF_SoakObservationCleanup.s_aReceipts)
			{
				if (receipt.m_sOperation != m_sVehicleSoakTrip || receipt.m_Id != m_VehicleSoakVehicleId || receipt.m_sRpl != m_sSoakObservationCarRpl ||
					receipt.m_iGroupGeneration != m_iVehicleSoakGeneration || receipt.m_iTripGeneration != m_VehicleSoakObservedTrip.GetTripGeneration() ||
					receipt.m_iLeaseGeneration != m_SoakObservationLease.GetLeaseGeneration() || receipt.m_iVehicleGeneration != m_SoakObservationLease.GetVehicleGeneration()) continue;
				if (!receipt.m_bCompleted || !receipt.m_bAbsent || receipt.m_iTerminalAt <= 0 || receipt.m_iConfirmedAt > receipt.m_iTerminalAt || m_SoakObservationLease.IsCapActive())
				{ m_bSoakObservationBroken = true; return; }
				m_bSoakObservationCleanup = true;
				AICF_VehicleSoakLog("OBS_CLEANUP", string.Format("operation_id=%1 vehicle=%2 rpl_id=%3 confirmed_at_ms=%4 terminal_at_ms=%5 absent=1 cap_held=0 native_completed=1", m_sVehicleSoakTrip, receipt.m_Id, receipt.m_sRpl, receipt.m_iConfirmedAt, receipt.m_iTerminalAt));
			}
		}
		AICF_GroupSlot slot = AICF_SoakObservationSlot();
		if (m_bSoakObservationCleanup && slot && slot.GetGroup() && slot.GetSpawnGeneration() == m_iVehicleSoakGeneration && slot.IsCombatReady())
		{
			AIWaypoint footWaypoint = slot.GetWaypoint();
			array<AIWaypoint> footQueue = {};
			slot.GetGroup().GetWaypoints(footQueue);
			IEntity footLeader = AICF_GroupRuntime.ResolveAliveLeader(slot.GetGroup());
			int fit;
			bool allDetached = true;
			for (int member; member < 5; member++)
			{
				if (!m_aSoakObservationDetached[member]) allDetached = false;
				ChimeraCharacter original = m_aSoakObservationActors[member];
				if (original && AICF_GroupRuntime.IsAliveCharacter(original))
				{
					if (slot.GetRosterMember(member) != original) { m_bSoakObservationBroken = true; return; }
					if (SCR_AIDamageHandling.IsConscious(original)) fit++;
				}
			}
			if (!m_bSoakObservationComplete && allDetached && fit > 0 && footLeader && footWaypoint && footQueue.Contains(footWaypoint) &&
				slot.GetGroup().GetCurrentWaypoint() == footWaypoint && slot.GetStrategicIntentRevision() == m_iSoakObservationIntent &&
				vector.DistanceXZ(footWaypoint.GetOrigin(), m_vSoakObservationTarget) <= 1)
			{
				if (m_iSoakObservationSurvivorAt == 0 || footLeader.GetID() != m_SoakObservationSurvivorLeaderId)
				{
					m_SoakObservationSurvivorLeaderId = footLeader.GetID();
					m_vSoakObservationSurvivorStart = footLeader.GetOrigin();
					m_iSoakObservationSurvivorAt = System.GetTickCount();
					AICF_VehicleSoakLog("OBS_SURVIVORS_READY", string.Format("operation_id=%1 group=%2 generation=%3 actual=%4 current_waypoint=%5 intent_revision=%6 leader=%7 leader_position=%8 actual_waypoint_position=%9", m_sVehicleSoakTrip, slot.GetGroup().GetID(), slot.GetSpawnGeneration(), fit, footWaypoint.GetID(), m_iSoakObservationIntent, footLeader.GetID(), footLeader.GetOrigin(), footWaypoint.GetOrigin()));
					return;
				}
				float survivorMoved = vector.DistanceXZ(footLeader.GetOrigin(), m_vSoakObservationSurvivorStart);
				if (survivorMoved < 10 || System.GetTickCount() - m_iSoakObservationSurvivorAt < 1000) return;
				m_bSoakObservationComplete = true;
				AICF_GroupTrace.Snapshot(slot, m_sVehicleSoakFaction, null, 0, "SOAK_OBSERVATION_RECOVERY");
				string survivorDetails = string.Format("operation_id=%1 group=%2 generation=%3 actual=%4 moved_m=%5 current_waypoint=%6 intent_revision=%7 cleanup_complete=1 original_full_route=0 recovery_kind=SURVIVORS", m_sVehicleSoakTrip, slot.GetGroup().GetID(), slot.GetSpawnGeneration(), fit, survivorMoved, footWaypoint.GetID(), m_iSoakObservationIntent);
				survivorDetails += string.Format(" leader=%1 leader_position=%2 actual_waypoint_position=%3", footLeader.GetID(), footLeader.GetOrigin(), footWaypoint.GetOrigin());
				AICF_VehicleSoakLog("OBS_RECOVERY_COMPLETE", survivorDetails);
			}
		}
		if (!slot || !slot.GetGroup() || !slot.IsCombatReady() || slot.GetSpawnGeneration() != m_iVehicleSoakGeneration + 1) return;
		int actual, wrong, nonAlive;
		if (!AICF_GroupRuntime.HasExactFactionRoster(slot.GetGroup(), m_sVehicleSoakFaction, 5, actual, wrong, nonAlive)) return;
		AIWaypoint waypoint = slot.GetWaypoint();
		array<AIWaypoint> queue = {};
		slot.GetGroup().GetWaypoints(queue);
		if (!waypoint || slot.GetGroup().GetCurrentWaypoint() != waypoint || !queue.Contains(waypoint) ||
			slot.GetStrategicIntentRevision() != m_iSoakObservationIntent || vector.DistanceXZ(waypoint.GetOrigin(), m_vSoakObservationTarget) > 1) return;
		IEntity leader = AICF_GroupRuntime.ResolveAliveLeader(slot.GetGroup());
		if (!leader) return;
		if (!m_bSoakObservationReplacement)
		{
			m_bSoakObservationReplacement = true;
			m_SoakObservationReplacementId = slot.GetGroup().GetID();
			m_SoakObservationReplacementLeaderId = leader.GetID();
			m_vSoakObservationReplacementStart = leader.GetOrigin();
			m_iSoakObservationReplacementAt = System.GetTickCount();
			for (int index; index < 5; index++)
			{
				IEntity member = slot.GetRosterMember(index);
				m_aSoakObservationReplacementMembers.Insert(member.GetID());
				AICF_VehicleSoakLog("OBS_REPLACEMENT_MEMBER", string.Format("position=%1 member=%2 actor_rpl=%3 group=%4 generation=%5", index, member.GetID(), Replication.FindItemId(member).ToString(), slot.GetGroup().GetID(), slot.GetSpawnGeneration()));
			}
			AICF_VehicleSoakLog("OBS_REPLACEMENT_READY", string.Format("operation_id=%1 group=%2 generation=%3 actual=5 faction_correct=1 current_waypoint=%4 intent_revision=%5 leader_position=%6 leader=%7 actual_waypoint_position=%8", m_sVehicleSoakTrip, slot.GetGroup().GetID(), slot.GetSpawnGeneration(), waypoint.GetID(), m_iSoakObservationIntent, leader.GetOrigin(), leader.GetID(), waypoint.GetOrigin()));
			return;
		}
		if (slot.GetGroup().GetID() != m_SoakObservationReplacementId || leader.GetID() != m_SoakObservationReplacementLeaderId) { m_bSoakObservationBroken = true; return; }
		for (int position; position < 5; position++)
			if (!slot.GetRosterMember(position) || slot.GetRosterMember(position).GetID() != m_aSoakObservationReplacementMembers[position]) { m_bSoakObservationBroken = true; return; }
		float moved = vector.DistanceXZ(leader.GetOrigin(), m_vSoakObservationReplacementStart);
		if (!m_bSoakObservationCleanup || moved < 10 || System.GetTickCount() - m_iSoakObservationReplacementAt < 1000 || m_bSoakObservationComplete) return;
		for (int member; member < 5; member++)
		{
			if (!m_aSoakObservationDetached[member] || m_aSoakObservationLastLife[member] == ECharacterLifeState.ALIVE) return;
			AICF_VehicleSoakLog("OBS_ACTOR_FINAL", string.Format("position=%1 member=%2 actor_rpl=%3 life=%4 detached=1", member, m_aVehicleSoakMembers[member], m_aSoakObservationActorRpl[member], SCR_Enum.GetEnumName(ECharacterLifeState, m_aSoakObservationLastLife[member])));
		}
		m_bSoakObservationComplete = true;
		AICF_GroupTrace.Snapshot(slot, m_sVehicleSoakFaction, null, 0, "SOAK_OBSERVATION_RECOVERY");
		AICF_VehicleSoakLog("OBS_RECOVERY_COMPLETE", string.Format("operation_id=%1 group=%2 generation=%3 actual=5 moved_m=%4 current_waypoint=%5 intent_revision=%6 cleanup_complete=1 original_full_route=0 recovery_kind=REPLACEMENT leader=%7 leader_position=%8 actual_waypoint_position=%9", m_sVehicleSoakTrip, slot.GetGroup().GetID(), slot.GetSpawnGeneration(), moved, waypoint.GetID(), m_iSoakObservationIntent, leader.GetID(), leader.GetOrigin(), waypoint.GetOrigin()));
	}

	override protected void Update()
	{
		if (AICF_SoakObservationEnabled()) AICF_SoakObservationTick();
		super.Update();
		if (AICF_SoakObservationEnabled()) AICF_SoakObservationTick();
	}

	override protected void Stop(bool cleanupEntities)
	{
		super.Stop(cleanupEntities);
		AICF_SoakObservationCleanup.s_aReceipts.Clear();
		m_aSoakObservationActors.Clear();
	}
}
