// Controlled native wheel damage, isolated stage only. No production timer,
// counter, action or trip-state mutation; normal scenario verdict stays separate.
modded class AICF_VehicleCoordinator
{
	bool AICF_VMDReadLifecycleFailure(out bool latched, out int count)
	{
		latched = true;
		count = -1;
		if (!Replication.IsServer() || !m_Acceptance) return false;
		latched = m_Acceptance.HasFailureLatch();
		count = m_Acceptance.GetFailureCount();
		return true;
	}
}

modded class AICF_MatchController
{
	protected ref AICF_TransportTrip m_VMDTrip;
	protected int m_iVMDStep;
	protected int m_iVMDHoldAt;
	protected int m_iVMDInjectedAt;
	protected int m_iVMDMovingPolls;
	protected int m_iVMDNextSample;
	protected int m_iVMDIntent;
	protected bool m_bVMDInjected;
	protected bool m_bVMDRecoveryServiced;
	protected string m_sVMDOperation;
	protected EntityID m_VMDOriginalVehicleId;

	protected void AICF_VMDLog(string eventName, string details)
	{
		Print(string.Format("[AICF][TRANSPORT_DAMAGE_PROBE] event=%1 operation_id=%2 t_ms=%3 intent_revision=%4 classification=CONTROLLED_FAULT_INJECTION normal_arrival_proof=0 %5",
			eventName, m_sVMDOperation, System.GetTickCount(), m_iVMDIntent, details));
	}

	protected void AICF_VMDFinish()
	{
		AICF_VMDLog("DONE", string.Format("checks=%1 failures=%2 injected=%3 step=%4", m_iVSChecks, m_iVSFailures, m_bVMDInjected, m_iVMDStep));
		AICF_VSFinish();
	}

	protected bool AICF_VMDOriginalRoster(AICF_GroupSlot slot)
	{
		if (m_aVSMembers.Count() != 5 || AICF_GroupRuntime.CountAliveAgents(m_VSGroup) != 5) return false;
		for (int position; position < 5; position++)
		{
			IEntity member = slot.GetRosterMember(position);
			if (!AICF_GroupRuntime.IsAliveCharacter(member) || member.GetID() != m_aVSMembers[position]) return false;
			if (!AICF_VehicleBoardingMutationFence.IsAuthoritativeAIEntity(member)) return false;
		}
		return true;
	}

	override protected void AICF_VSServiceRecovery(AICF_GroupSlot slot)
	{
		string enabled;
		if (!System.GetCLIParam("aicfTransportDamageFallbackProbe", enabled) || enabled != "1") return;
		if (!slot || !m_VSState || slot != m_VSState.GetSlot(0) || !AICF_VMDOriginalRoster(slot)) return;
		if (m_iVMDStep == 5 && System.GetTickCount() >= m_iVMDNextSample)
		{
			m_iVMDNextSample = System.GetTickCount() + 10000;
			AICF_TransportTrip currentTrip = m_VSVehicles.GetTrip(slot);
			string currentOperation = "NONE";
			string currentPhase = "NONE";
			if (currentTrip)
			{
				currentOperation = currentTrip.GetOperationId();
				currentPhase = typename.EnumToString(AICF_ETransportTripPhase, currentTrip.GetPhase());
			}
			AICF_VMDLog("RETRY_WAIT", string.Format("current_operation=%1 current_phase=%2 pending_order=%3 recovery_serviced=%4 ready=%5 has_destination=%6 waypoint=%7 controlling_movement=%8 restore_pending=%9",
				currentOperation, currentPhase, slot.HasPendingOrderRecovery(), m_bVMDRecoveryServiced, slot.IsCombatReady(), slot.HasStrategicDestination(),
				AICF_GroupTrace.Entity(slot.GetWaypoint()), m_VSVehicles.IsControllingMovement(slot), m_VSVehicles.IsRestorePending(slot)));
		}
	}

	override protected void AICF_VSOnRecoveryVerified(AICF_GroupSlot slot, AIWaypoint waypoint, int stableMs, int stablePolls, int requiredStableMs)
	{
		string enabled;
		if (!System.GetCLIParam("aicfTransportDamageFallbackProbe", enabled) || enabled != "1") return;
		if (!AICF_VMDOriginalRoster(slot) || !m_VMDTrip || !m_VMDTrip.IsTerminal() || !m_VMDTrip.GetHandoffState().IsOrderRestored()) return;
		m_bVMDRecoveryServiced = true;
		AICF_VMDLog("RECOVERY_VERIFIED", string.Format("waypoint_id=%1 stable_ms=%2 stable_polls=%3 required_stable_ms=%4 pending_after=0 owner=ProcessPendingOrderRecovery direct_clear=0", waypoint.GetID(), stableMs, stablePolls, requiredStableMs));
	}

	protected bool AICF_VMDInjectWheels(AICF_TransportTrip trip)
	{
		if (m_bVMDInjected || !Replication.IsServer() || !trip || trip != m_VMDTrip ||
			trip.GetPhase() != AICF_ETransportTripPhase.TRANSIT || !AICF_VSExact(m_VSState.GetSlot(0))) return false;
		if (!trip.GetLease() || trip.GetLease().GetVehicle() != m_VSVehicle || m_VSVehicle.GetID() != m_VMDOriginalVehicleId) return false;
		SCR_WheeledDamageManagerComponent damage = SCR_WheeledDamageManagerComponent.Cast(SCR_DamageManagerComponent.GetDamageManager(m_VSVehicle));
		if (!damage || damage.GetMovementDamage() >= 1 || !SCR_AIVehicleUsability.VehicleCanMove(m_VSVehicle, damage)) return false;
		array<HitZone> zones = {};
		damage.GetAllHitZonesInHierarchy(zones);
		array<SCR_WheelHitZone> wheels = {};
		foreach (HitZone zone : zones)
		{
			SCR_WheelHitZone wheel = SCR_WheelHitZone.Cast(zone);
			if (wheel && !wheel.IsProxy() && wheel.GetWheelIndex() >= 0) wheels.Insert(wheel);
		}
		if (wheels.Count() < 4) return false;
		m_bVMDInjected = true;
		m_iVMDInjectedAt = System.GetTickCount();
		foreach (SCR_WheelHitZone damagedWheel : wheels) damagedWheel.SetHealthScaled(0);
		// Native recomputation from real wheel hitzones; never SetMovementDamage.
		damage.UpdateVehicleState();
		int destroyed;
		foreach (SCR_WheelHitZone observedWheel : wheels)
			if (observedWheel.GetDamageState() == EDamageState.DESTROYED) destroyed++;
		AICF_VMDLog("INJECTED", string.Format("vehicle=%1 wheels=%2 destroyed_wheels=%3 movement_damage=%4 can_move=%5 crew_recoveries=%6 alive=5 exact5_before=1 exact5_after=%7",
			AICF_GroupTrace.Entity(m_VSVehicle), wheels.Count(), destroyed, damage.GetMovementDamage(),
			SCR_AIVehicleUsability.VehicleCanMove(m_VSVehicle, damage), trip.GetMovementState().GetCrewRecoveryAttempts(), AICF_VSExact(m_VSState.GetSlot(0))));
		return destroyed == wheels.Count() && damage.GetMovementDamage() >= 1 && !SCR_AIVehicleUsability.VehicleCanMove(m_VSVehicle, damage);
	}

	protected void AICF_VMDFootPath(AICF_GroupSlot slot)
	{
		if (!AICF_VSOwnRecoveryRoster(slot)) return;
		int assignment = slot.GetStrategicAssignmentRevision();
		int intent = slot.GetStrategicIntentRevision();
		AIWaypoint waypoint = slot.GetWaypoint();
		AIGroupMovementComponent groupMovement = AIGroupMovementComponent.Cast(m_VSGroup.FindComponent(AIGroupMovementComponent));
		for (int position; position < m_aVSMembers.Count(); position++)
		{
			ChimeraCharacter member = ChimeraCharacter.Cast(slot.GetRosterMember(position));
			AIAgent agent = member.GetAIControlComponent().GetControlAIAgent();
			AIBaseMovementComponent movement = agent.GetMovementComponent();
			AIPathfindingComponent path;
			NavmeshWorldComponent nav;
			array<vector> points = {};
			if (movement)
			{
				movement.GetCurrentPath(points);
				path = movement.GetPathfindingComponent();
				if (path) nav = path.GetNavmeshComponent();
			}
			vector origin = member.GetOrigin();
			vector aheadDirection = m_vVSGoal - origin;
			aheadDirection.Normalize();
			vector ahead = origin + aheadDirection * 2;
			vector last;
			if (!points.IsEmpty()) last = points[points.Count() - 1];
			int handler = -999;
			int handlerCount = -1;
			int displacement = -1;
			string formation = "NONE";
			AIAgent handlerLeader;
			if (groupMovement)
			{
				handler = groupMovement.GetAgentMoveHandlerId(agent);
				if (handler >= 0)
				{
					handlerCount = groupMovement.GetMoveHandlerAgentCount(handler);
					handlerLeader = groupMovement.GetHandlerLeaderAgent(handler);
					AIFormationDefinition definition = groupMovement.GetFormationDefinition(handler);
					if (definition) formation = definition.GetName();
					displacement = groupMovement.GetFormationDisplacement(handler);
				}
			}
			bool actorLoaded, aheadLoaded, goalLoaded, lastLoaded, actorValid, aheadValid;
			bool actorProjection, goalProjection;
			vector actorNav, goalNav;
			if (nav)
			{
				actorLoaded = nav.IsTileLoaded(origin);
				aheadLoaded = nav.IsTileLoaded(ahead);
				goalLoaded = nav.IsTileLoaded(m_vVSGoal);
				if (!points.IsEmpty()) lastLoaded = nav.IsTileLoaded(last);
				actorValid = nav.IsTileValid(origin);
				aheadValid = nav.IsTileValid(ahead);
				actorProjection = path.GetClosestPositionOnNavmesh(origin, "1.5 4 1.5", actorNav);
				goalProjection = path.GetClosestPositionOnNavmesh(m_vVSGoal, "1.5 4 1.5", goalNav);
			}
			if (!AICF_VSOwnRecoveryRoster(slot) || slot.GetStrategicAssignmentRevision() != assignment || slot.GetStrategicIntentRevision() != intent || slot.GetWaypoint() != waypoint) return;
			if (slot.GetRosterMember(position) != member || member.GetID() != m_aVSMembers[position] || agent.GetMovementComponent() != movement) return;
			if (movement && movement.GetPathfindingComponent() != path) return;
			SCR_AIUtilityComponent utility = SCR_AIUtilityComponent.Cast(agent.FindComponent(SCR_AIUtilityComponent));
			string details = string.Format("position=%1 member=%2 member_pos=%3 goal=%4 agent=%5 agent_lod=%6 path_component=%7 nav_component=%8 path_count=%9",
				position, AICF_GroupTrace.Entity(member), origin, m_vVSGoal, AICF_GroupTrace.Entity(agent), agent.GetLOD(), path != null, nav != null, points.Count());
			details += string.Format(" path_last=%1 handler=%2 handler_count=%3 handler_leader=%4 formation=%5 displacement=%6 waypoint=%7 assignment_revision=%8",
				last, handler, handlerCount, AICF_GroupTrace.Entity(handlerLeader), formation, displacement, AICF_GroupTrace.Entity(waypoint), assignment);
			details += string.Format(" actor_tile_loaded=%1 ahead2m_tile_loaded=%2 goal_tile_loaded=%3 last_tile_loaded=%4 actor_tile_valid=%5 ahead2m_tile_valid=%6 actor_projection=%7 goal_projection=%8 observation_only=1 nav_load_requested=0",
				actorLoaded, aheadLoaded, goalLoaded, lastLoaded, actorValid, aheadValid, actorProjection, goalProjection);
			if (utility) details += string.Format(" current_action=%1 executed_action=%2", AICF_GroupTrace.Action(utility.GetCurrentAction()), AICF_GroupTrace.Action(utility.GetExecutedAction()));
			AICF_VMDLog("FOOT_PATH", details);
		}
	}

	protected bool AICF_VMDIsFailedExplicitRetry(AICF_GroupSlot slot, AICF_TransportTrip trip)
	{
		if (m_iVMDStep != 5 || !m_VMDTrip || !trip || trip == m_VMDTrip || !m_USSRFaction || !m_VSVehicles) return false;
		if (!AICF_VSOwnRecoveryRoster(slot) || m_VSVehicles.GetTrip(slot) != trip || trip.GetVehiclePresetSlot() != slot || !trip.GetAssignment()) return false;
		if (trip.GetAssignment().GetGroup() != m_VSGroup || trip.GetFactionKey() != m_USSRFaction.GetFactionKey() || trip.GetSlotId() != slot.GetSlotId()) return false;
		if (trip.GetGroupGeneration() != slot.GetSpawnGeneration() || trip.GetTripGeneration() != m_VMDTrip.GetTripGeneration() + 1) return false;
		if (trip.GetOperationId() == "" || trip.GetOperationId() == m_sVMDOperation || slot.GetStrategicIntentRevision() != m_iVMDIntent || !trip.IsTerminal()) return false;
		return !trip.GetLease() || !trip.GetLease().HasPhysicalAsset() || !trip.IsVehiclePresetDebited();
	}

	override protected bool AICF_VSAlternativeScenario(AICF_GroupSlot slot, AICF_TransportTrip trip)
	{
		string enabled;
		if (!System.GetCLIParam("aicfTransportDamageFallbackProbe", enabled) || enabled != "1") return false;
		int now = System.GetTickCount();
		if (!AICF_VMDOriginalRoster(slot)) { AICF_VSCheck("DAMAGE_ORIGINAL_ROSTER", false); AICF_VMDFinish(); return true; }
		if (m_iVMDStep == 0)
		{
			if (trip.GetPhase() != AICF_ETransportTripPhase.TRANSIT) return true;
			m_VMDTrip = trip;
			m_sVMDOperation = trip.GetOperationId();
			m_VSVehicle = trip.GetLease().GetVehicle();
			m_VSVehicleId = m_VSVehicle.GetID();
			m_VMDOriginalVehicleId = m_VSVehicleId;
			m_vVSStart = m_VSVehicle.GetOrigin();
			m_iVMDHoldAt = now;
			AICF_VSCheck("DAMAGE_EXACT_INITIAL_BOARDING", AICF_VSExact(slot) && trip.IsVehiclePresetDebited());
			AICF_VMDLog("BOARDED", string.Format("vehicle=%1 group=%2 exact5=%3 debited=%4", AICF_GroupTrace.Entity(m_VSVehicle), AICF_GroupTrace.Entity(m_VSGroup), AICF_VSExact(slot), trip.IsVehiclePresetDebited()));
			for (int boardedPosition; boardedPosition < 5; boardedPosition++)
				Print(string.Format("[AICF][TRANSPORT_DAMAGE_MEMBER] event=BOARD position=%1 member=%2", boardedPosition, AICF_GroupTrace.Entity(slot.GetRosterMember(boardedPosition))));
			if (!AICF_VSExact(slot) || !trip.IsVehiclePresetDebited()) { AICF_VMDFinish(); return true; }
			m_iVMDStep = 1;
			return true;
		}
		if (m_iVMDStep == 1)
		{
			if (!AICF_VSExact(slot) || !trip.IsPresetWaitingForDestination()) { AICF_VSCheck("DAMAGE_MOUNTED_HOLD", false); AICF_VMDFinish(); return true; }
			if (now - m_iVMDHoldAt < 30000) return true;
			vector goal;
			if (!AICF_VSTarget(goal)) return true;
			AICF_VSCheck("DAMAGE_MOUNTED_HOLD", true);
			bool accepted = AICF_VSOrder(slot, goal);
			AICF_VSCheck("DAMAGE_REMOTE_ORDER", accepted);
			if (!accepted) { AICF_VMDFinish(); return true; }
			m_iVMDIntent = slot.GetStrategicIntentRevision();
			m_iVMDStep = 2;
			return true;
		}
		if (m_iVMDStep == 2)
		{
			if (trip != m_VMDTrip || trip.GetPhase() != AICF_ETransportTripPhase.TRANSIT) { AICF_VSCheck("DAMAGE_PREINJECTION_TRANSIT", false); AICF_VMDFinish(); return true; }
			if (!AICF_VSExact(slot)) return true;
			if (m_VSVehicle.GetPhysics() && m_VSVehicle.GetPhysics().GetVelocity().Length() > 1) m_iVMDMovingPolls++;
			float displacement = vector.DistanceXZ(m_vVSStart, m_VSVehicle.GetOrigin());
			if (displacement < 15 || m_iVMDMovingPolls < 3) return true;
			AICF_VMDLog("MOVING", string.Format("vehicle=%1 displacement_m=%2 moving_polls=%3 exact5=1", AICF_GroupTrace.Entity(m_VSVehicle), displacement, m_iVMDMovingPolls));
			bool injected = AICF_VMDInjectWheels(trip);
			AICF_VSCheck("DAMAGE_NATIVE_WHEEL_INJECTION", injected);
			if (!injected) { AICF_VMDFinish(); return true; }
			m_iVMDStep = 3;
			return true;
		}
		if (m_iVMDStep == 3)
		{
			if (trip != m_VMDTrip || slot.GetStrategicIntentRevision() != m_iVMDIntent) { AICF_VSCheck("DAMAGE_UNEXPECTED_REPURCHASE_OR_INTENT", false); AICF_VMDFinish(); return true; }
			if (!trip.IsTerminal())
			{
				if (now - m_iVMDInjectedAt > 10000) { AICF_VSCheck("DAMAGE_BOUNDED_FALLBACK", false); AICF_VMDFinish(); }
				return true;
			}
			bool fallback = trip.GetPhase() == AICF_ETransportTripPhase.FALLBACK && trip.GetTerminalReason() == "VEHICLE_MOVEMENT_DESTROYED";
			AICF_VSCheck("DAMAGE_TYPED_FALLBACK", fallback);
			bool lifecycleLatched;
			int lifecycleCount;
			bool lifecycleRead = m_VSVehicles.AICF_VMDReadLifecycleFailure(lifecycleLatched, lifecycleCount);
			bool lifecycleClean = lifecycleRead && !lifecycleLatched && lifecycleCount == 0;
			AICF_VSCheck("DAMAGE_NO_FALSE_LIFECYCLE_FAILURE", lifecycleClean);
			AICF_VMDLog("FALLBACK", string.Format("reason=%1 phase=%2 latency_ms=%3 crew_recoveries=%4 order_restored=%5 lifecycle_failure_latched=%6 lifecycle_failure_count=%7", trip.GetTerminalReason(), typename.EnumToString(AICF_ETransportTripPhase, trip.GetPhase()), now - m_iVMDInjectedAt, trip.GetMovementState().GetCrewRecoveryAttempts(), trip.GetHandoffState().IsOrderRestored(), lifecycleLatched, lifecycleCount));
			if (!fallback || !lifecycleClean) { AICF_VMDFinish(); return true; }
			m_iVMDHoldAt = now;
			m_iVMDStep = 4;
			return true;
		}
		if (m_iVMDStep == 4)
		{
			if (trip != m_VMDTrip || trip.GetPhase() != AICF_ETransportTripPhase.FALLBACK || slot.GetStrategicIntentRevision() != m_iVMDIntent)
			{ AICF_VSCheck("DAMAGE_NO_REPURCHASE", false); AICF_VMDFinish(); return true; }
			if (!trip.GetHandoffState().IsOrderRestored() || m_VSVehicles.IsInfantryOrderSuspended(slot)) return true;
			float farthest;
			bool outside = true;
			for (int footPosition; footPosition < 5; footPosition++)
			{
				ChimeraCharacter foot = ChimeraCharacter.Cast(slot.GetRosterMember(footPosition));
				CompartmentAccessComponent access = foot.GetCompartmentAccessComponent();
				if (!access || foot.IsInVehicle() || access.IsInCompartment() || access.IsGettingIn() || access.IsGettingOut() || CompartmentAccessComponent.GetVehicleIn(foot)) outside = false;
				farthest = Math.Max(farthest, vector.DistanceXZ(foot.GetOrigin(), m_vVSGoal));
			}
			if (now >= m_iVMDNextSample)
			{
				m_iVMDNextSample = now + 5000;
				AICF_VMDLog("RETAINED", string.Format("held_ms=%1 outside5=%2 farthest_m=%3 order_restored=1 same_operation=1", now - m_iVMDHoldAt, outside, farthest));
				if (outside && farthest > 35) AICF_VMDFootPath(slot);
			}
			if (!outside || farthest > 35 || now - m_iVMDHoldAt < 30000 || !m_bVMDRecoveryServiced || slot.HasPendingOrderRecovery()) return true;
			AICF_VSCheck("DAMAGE_REAL_OWNER_RECOVERY_VERIFIED", true);
			AICF_VSCheck("DAMAGE_ORIGINAL_FOOT_ARRIVAL", true);
			AICF_VSCheck("DAMAGE_NO_REPURCHASE", true);
			AICF_VMDLog("FOOT", string.Format("held_ms=%1 outside5=1 farthest_m=%2 alive=5 order_restored=1 same_operation=1", now - m_iVMDHoldAt, farthest));
			for (int arrivedPosition; arrivedPosition < 5; arrivedPosition++)
				Print(string.Format("[AICF][TRANSPORT_DAMAGE_MEMBER] event=FOOT position=%1 member=%2", arrivedPosition, AICF_GroupTrace.Entity(slot.GetRosterMember(arrivedPosition))));
			bool retry = m_VSVehicles.RequestExplicitVehicleAdmission(slot, m_USSRFaction, "DAMAGE_FALLBACK_EXPLICIT_RETRY");
			AICF_VSCheck("DAMAGE_EXPLICIT_RETRY", retry);
			AICF_VMDLog("EXPLICIT", string.Format("accepted=%1 unchanged_intent=%2", retry, slot.GetStrategicIntentRevision() == m_iVMDIntent));
			if (!retry) { AICF_VMDFinish(); return true; }
			m_iVMDStep = 5;
			return true;
		}
		if (m_iVMDStep == 5)
		{
			if (AICF_VMDIsFailedExplicitRetry(slot, trip))
			{
				AICF_VSCheck("DAMAGE_EXPLICIT_RETRY_TERMINAL_BEFORE_PURCHASE", false);
				bool retryPhysical = trip.GetLease() && trip.GetLease().HasPhysicalAsset();
				AICF_VMDLog("RETRY_FAILED", string.Format("new_operation=%1 reason=%2 phase=%3 physical=%4 debited=%5 attempted=1", trip.GetOperationId(), trip.GetTerminalReason(), typename.EnumToString(AICF_ETransportTripPhase, trip.GetPhase()), retryPhysical, trip.IsVehiclePresetDebited()));
				AICF_VMDFinish();
				return true;
			}
			if (trip == m_VMDTrip || !trip.GetLease() || !trip.GetLease().HasPhysicalAsset() || !trip.IsVehiclePresetDebited()) return true;
			Vehicle retryVehicle = trip.GetLease().GetVehicle();
			bool retryIdentity = retryVehicle && retryVehicle.GetID() != m_VMDOriginalVehicleId && slot.GetStrategicIntentRevision() == m_iVMDIntent && AICF_VMDOriginalRoster(slot);
			AICF_VSCheck("DAMAGE_NEW_EXPLICIT_OPERATION_AND_DEBIT", retryIdentity);
			AICF_VMDLog("RETRY_PURCHASED", string.Format("new_operation=%1 vehicle=%2 different_vehicle=%3 debited=%4 original_roster5=%5 unchanged_intent=%6 injection_count=1",
				trip.GetOperationId(), AICF_GroupTrace.Entity(retryVehicle), retryVehicle && retryVehicle.GetID() != m_VMDOriginalVehicleId, trip.IsVehiclePresetDebited(), AICF_VMDOriginalRoster(slot), slot.GetStrategicIntentRevision() == m_iVMDIntent));
			m_iVMDStep = 6;
			AICF_VMDFinish();
			return true;
		}
		return true;
	}
}
