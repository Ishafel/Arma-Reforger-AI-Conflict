class AICF_VehicleNativeRouteProjection
{
	string m_sReason = "NO_PROJECTED_SEGMENT";
	float m_fArc;
	float m_fGap = float.MAX;
	float m_fHeight;
	int m_iSegment = -1;
}

// История содержит только значения. Старый frontier не исчезает при A -> B -> A.
class AICF_VehicleNativeRoutePathRecord
{
	ref array<vector> m_aPoints = {};
	float m_fHighWaterArc;
	float m_fCreditAnchorArc;
	float m_fPhysicalTravel;
	bool m_bHasSample;
	int m_iAtMs;
	vector m_vPosition;
	float m_fArc;
	float m_fSpeed;
}

// Наблюдает bounded продвижение по полному текущему native автомобильному пути.
// Ни один метод не пишет trip clocks, phase, waypoint, action или physics.
class AICF_VehicleNativeRouteProgress
{
	static const int MAXIMUM_POINTS = 512;
	static const int MAXIMUM_PATH_VARIANTS = 4;
	static const int MAXIMUM_SAMPLE_INTERVAL_MS = 5000;
	static const float MAXIMUM_OFF_PATH_METERS = 8;
	static const float MAXIMUM_HEIGHT_METERS = 2;
	static const float ENDPOINT_XZ_TOLERANCE_METERS = 3;
	static const float ENDPOINT_HEIGHT_TOLERANCE_METERS = 2;
	static const float NONLOCAL_ARC_METERS = 20;
	static const float AMBIGUITY_GAP_METERS = 2;
	static const float MAXIMUM_SPEED_MPS = 80;
	static const float MINIMUM_SPEED_MPS = 0.5;
	protected ref array<ref AICF_VehicleNativeRoutePathRecord> m_aRecords = {};
	protected string m_sScope;
	protected string m_sRouteIdentity;
	protected string m_sDetails = " native_route_progress=UNINITIALIZED";
	protected int m_iActiveRecord = -1;
	protected int m_iWaypointRplState = -1;

	void Reset()
	{
		m_aRecords.Clear();
		m_sScope = string.Empty;
		m_sRouteIdentity = string.Empty;
		m_iActiveRecord = -1;
		m_iWaypointRplState = -1;
		m_sDetails = " native_route_progress=RESET";
	}

	string GetDetails()
	{
		string opaqueScope = m_sScope;
		opaqueScope.Replace("=", ":");
		opaqueScope.Replace(" ", "|");
		return m_sDetails + string.Format(" native_waypoint_has_rpl=%1", m_iWaypointRplState) +
			" native_route_scope=" + opaqueScope;
	}

	static bool FiniteNumber(float value)
	{
		return value == value && Math.AbsFloat(value) < 1000000;
	}

	static bool FinitePoint(vector value)
	{
		return FiniteNumber(value[0]) && FiniteNumber(value[1]) && FiniteNumber(value[2]);
	}

	static bool SamePath(array<vector> first, array<vector> second)
	{
		if (!first || !second || first.Count() != second.Count()) return false;
		for (int index; index < first.Count(); index++)
			if (first[index] != second[index]) return false;
		return true;
	}

	static bool ValidatePath(array<vector> points, vector endpoint)
	{
		if (!points || points.Count() < 2 || points.Count() > MAXIMUM_POINTS || !FinitePoint(endpoint)) return false;
		foreach (vector point : points)
			if (!FinitePoint(point)) return false;
		vector last = points[points.Count() - 1];
		return vector.DistanceXZ(last, endpoint) <= ENDPOINT_XZ_TOLERANCE_METERS &&
			Math.AbsFloat(last[1] - endpoint[1]) <= ENDPOINT_HEIGHT_TOLERANCE_METERS;
	}

	protected static bool SegmentProjection(vector position, vector first, vector last, float prefix,
		out float arc, out float gap, out float height)
	{
		float dx = last[0] - first[0];
		float dz = last[2] - first[2];
		float lengthSq = dx * dx + dz * dz;
		if (lengthSq <= 0.0001) return false;
		float numerator = (position[0] - first[0]) * dx + (position[2] - first[2]) * dz;
		float fraction = Math.Clamp(numerator / lengthSq, 0, 1);
		vector projected = first + (last - first) * fraction;
		gap = vector.DistanceXZ(position, projected);
		height = Math.AbsFloat(position[1] - projected[1]);
		arc = prefix + Math.Sqrt(lengthSq) * fraction;
		return true;
	}

	static AICF_VehicleNativeRouteProjection ProjectUnique(array<vector> points, vector position)
	{
		AICF_VehicleNativeRouteProjection result = new AICF_VehicleNativeRouteProjection();
		if (!points || points.Count() < 2 || points.Count() > MAXIMUM_POINTS || !FinitePoint(position)) return result;
		float prefix;
		for (int index; index < points.Count() - 1; index++)
		{
			float arc;
			float gap;
			float height;
			if (SegmentProjection(position, points[index], points[index + 1], prefix, arc, gap, height) &&
				height <= MAXIMUM_HEIGHT_METERS && gap < result.m_fGap)
			{
				result.m_fArc = arc;
				result.m_fGap = gap;
				result.m_fHeight = height;
				result.m_iSegment = index;
			}
			prefix += vector.DistanceXZ(points[index], points[index + 1]);
		}
		if (result.m_iSegment < 0 || result.m_fGap > MAXIMUM_OFF_PATH_METERS) return result;
		prefix = 0;
		for (int other; other < points.Count() - 1; other++)
		{
			float otherArc;
			float otherGap;
			float otherHeight;
			if (SegmentProjection(position, points[other], points[other + 1], prefix, otherArc, otherGap, otherHeight) &&
				otherHeight <= MAXIMUM_HEIGHT_METERS && Math.AbsFloat(otherArc - result.m_fArc) > NONLOCAL_ARC_METERS &&
				otherGap <= result.m_fGap + AMBIGUITY_GAP_METERS)
			{
				result.m_sReason = "AMBIGUOUS_NONLOCAL_SEGMENT";
				return result;
			}
			prefix += vector.DistanceXZ(points[other], points[other + 1]);
		}
		result.m_sReason = string.Empty;
		return result;
	}

	protected static bool ReadContext(AICF_TransportTrip trip, out AICarMovementComponent car,
		out Vehicle vehicle, out string scope, out string routeIdentity, out vector endpoint, out string reason,
		out int waypointRplState)
	{
		waypointRplState = -1;
		reason = "TRIP_SCOPE";
		if (!Replication.IsServer() || !trip || !trip.IsValid() || trip.IsTerminal()) return false;
		if (trip.GetPhase() != AICF_ETransportTripPhase.TRANSIT || !trip.GetAssignment()) return false;
		reason = "PRESET_SCOPE";
		AICF_GroupSlot slot = trip.GetVehiclePresetSlot();
		if (!slot || !slot.IsCombatReady() || !trip.GetVehiclePreset() || !trip.IsVehiclePresetCurrent()) return false;
		AICF_StrategicAssignmentSnapshot assignment = trip.GetAssignment();
		SCR_AIGroup group = assignment.GetGroup();
		reason = "GROUP_ASSIGNMENT_SCOPE";
		if (!AICF_VehicleBoardingMutationFence.IsAuthoritativeReplicatedEntity(group) || slot.GetGroup() != group) return false;
		if (!group.GetFaction() || assignment.GetFactionKey() != group.GetFaction().GetFactionKey()) return false;
		if (assignment.GetSlotId() != slot.GetSlotId() || assignment.GetGroupGeneration() != slot.GetSpawnGeneration()) return false;
		if (assignment.GetAssignmentRevision() != slot.GetStrategicAssignmentRevision() ||
			assignment.GetStrategicIntentRevision() != slot.GetStrategicIntentRevision()) return false;
		if (assignment.GetTargetKind() != slot.GetTargetKind()) return false;
		if (assignment.GetTargetKind() == AICF_EOrderTargetKind.POSITION && assignment.GetTargetPosition() != slot.GetTargetPosition()) return false;
		if (assignment.GetTargetKind() == AICF_EOrderTargetKind.BASE && assignment.GetTargetBase() != slot.GetTargetBase()) return false;
		AICF_VehicleLease lease = trip.GetLease();
		reason = "LEASE_SCOPE";
		if (!lease || lease.GetState() != AICF_EVehicleLeaseState.ACTIVE || !lease.HasPhysicalAsset()) return false;
		if (!lease.MatchesTripIdentity(trip.GetFactionKey(), trip.GetSlotId(), trip.GetGroupGeneration(), trip.GetTripGeneration())) return false;
		vehicle = lease.GetVehicle();
		reason = "VEHICLE_IDENTITY";
		if (!AICF_VehicleBoardingMutationFence.IsAuthoritativeReplicatedEntity(vehicle)) return false;
		RplComponent vehicleRpl = RplComponent.Cast(vehicle.FindComponent(RplComponent));
		if (!vehicleRpl || !lease.MatchesEntityIdentity(vehicle, vehicle.GetID(), vehicleRpl.Id().ToString())) return false;
		AICF_VehicleMovementState movement = trip.GetMovementState();
		reason = "MOVEMENT_ROUTE_SCOPE";
		if (!movement || !movement.IsRouteWaypointBound() || movement.GetRouteGeneration() <= 0) return false;
		if (movement.GetCrewRecoveryToken() || movement.IsCrewRecoveryRoutePending()) return false;
		if (movement.GetAssignmentRevision() != assignment.GetAssignmentRevision()) return false;
		AIWaypoint waypoint = movement.GetRouteWaypoint();
		reason = "CURRENT_WAYPOINT_IDENTITY";
		if (!waypoint || group.GetCurrentWaypoint() != waypoint) return false;
		EntityID waypointId = waypoint.GetID();
		// Native transient waypoints may be server-only. Their exact trip-owned
		// pointer/EntityID, current queue and Move relation are the ownership proof.
		// A component which does exist must still be authoritative.
		RplComponent waypointRpl = RplComponent.Cast(waypoint.FindComponent(RplComponent));
		string waypointRplId = "NONE";
		string waypointComponentId = "NONE";
		waypointRplState = 0;
		reason = "CURRENT_WAYPOINT_AUTHORITY";
		if (waypointRpl)
		{
			waypointRplState = 1;
			reason += string.Format("_HAS_RPL_1_MASTER_%1_PROXY_%2", waypointRpl.IsMaster(), waypointRpl.IsProxy());
			if (!waypointRpl.IsMaster() || waypointRpl.IsProxy()) return false;
			waypointRplId = waypointRpl.Id().ToString();
			waypointComponentId = waypointRpl.ToString();
		}
		array<AIWaypoint> queue = {};
		group.GetWaypoints(queue);
		reason = "CURRENT_WAYPOINT_QUEUE";
		if (!queue.Contains(waypoint)) return false;
		SCR_AIGroupUtilityComponent utility = group.GetGroupUtilityComponent();
		reason = "GROUP_UTILITY_OWNER";
		if (!utility || utility.m_Owner != group || !utility.m_VehicleMgr) return false;
		SCR_AIMoveActivity move = SCR_AIMoveActivity.Cast(utility.GetCurrentAction());
		reason = "CURRENT_MOVE_ACTION";
		if (!move || move.m_Utility != utility || move.m_RelatedWaypoint != waypoint) return false;
		if (move.GetRemoveAction() || move.GetActionIsSuspended()) return false;
		EAIActionState actionState = move.GetActionState();
		if (actionState != EAIActionState.RUNNING && actionState != EAIActionState.EVALUATED) return false;
		SCR_AIGroupVehicle registered = utility.m_VehicleMgr.FindVehicle(vehicle);
		reason = "REGISTERED_GROUP_VEHICLE";
		if (!registered || registered.GetEntity() != vehicle) return false;
		ChimeraCharacter driver = ChimeraCharacter.Cast(registered.GetDriver());
		reason = "REGISTERED_AI_DRIVER";
		if (!AICF_VehicleBoardingMutationFence.IsAuthoritativeAIEntity(driver) || !SCR_AIDamageHandling.IsConscious(driver)) return false;
		if (!driver.GetAIControlComponent()) return false;
		AIAgent driverAgent = driver.GetAIControlComponent().GetControlAIAgent();
		if (!driverAgent || driverAgent.GetControlledEntity() != driver || driverAgent.GetParentGroup() != group) return false;
		array<AIAgent> agents = {};
		group.GetAgents(agents);
		if (!agents.Contains(driverAgent)) return false;
		CompartmentAccessComponent access = driver.GetCompartmentAccessComponent();
		reason = "SETTLED_PILOT";
		if (!access || !driver.IsInVehicle() || !access.IsInCompartment()) return false;
		if (access.IsGettingIn() || access.IsGettingOut() || access.IsSwitchingSeatsAnim()) return false;
		PilotCompartmentSlot pilot = PilotCompartmentSlot.Cast(access.GetCompartment());
		if (!pilot || pilot.GetVehicle() != vehicle || pilot.GetOccupant() != driver) return false;
		if (CompartmentAccessComponent.GetVehicleIn(driver) != vehicle) return false;
		car = AICarMovementComponent.Cast(driverAgent.GetMovementComponent());
		reason = "CURRENT_CAR_MOVEMENT";
		if (!car || !car.GetAIAgent() || car.GetAIAgent().GetControlledEntity() != vehicle) return false;
		RplComponent groupRpl = RplComponent.Cast(group.FindComponent(RplComponent));
		RplComponent driverRpl = RplComponent.Cast(driver.FindComponent(RplComponent));
		reason = "CONTEXT_REPLICATION";
		if (!groupRpl || !driverRpl) return false;
		reason = "CURRENT_WAYPOINT_IDENTITY_CHANGED";
		if (movement.GetRouteWaypoint() != waypoint || waypoint.GetID() != waypointId) return false;
		if (RplComponent.Cast(waypoint.FindComponent(RplComponent)) != waypointRpl) return false;
		if (waypointRpl)
		{
			reason = "CURRENT_WAYPOINT_AUTHORITY_CHANGED";
			if (!waypointRpl.IsMaster() || waypointRpl.IsProxy()) return false;
			if (waypointRpl.Id().ToString() != waypointRplId) return false;
		}
		endpoint = movement.GetRouteEndpoint();
		scope = string.Format("operation=%1 group=%2 group_rpl=%3 driver=%4 driver_rpl=%5 vehicle=%6 vehicle_rpl=%7",
			trip.GetOperationId(), group.GetID(), groupRpl.Id(), driver.GetID(), driverRpl.Id(), vehicle.GetID(), vehicleRpl.Id());
		scope += string.Format(" faction=%1 slot=%2 group_generation=%3 trip_generation=%4 lease_generation=%5 vehicle_generation=%6 assignment=%7 intent=%8",
			trip.GetFactionKey(), trip.GetSlotId(), trip.GetGroupGeneration(), trip.GetTripGeneration(), lease.GetLeaseGeneration(),
			lease.GetVehicleGeneration(), assignment.GetAssignmentRevision(), assignment.GetStrategicIntentRevision());
		scope += string.Format(" driver_agent=%1 car_agent=%2 car_component=%3", driverAgent.GetID(), car.GetAIAgent().GetID(), car.ToString());
		routeIdentity = string.Format("waypoint=%1 waypoint_rpl=%2 route_generation=%3 endpoint=%4 move=%5 waypoint_component=%6",
			waypointId, waypointRplId, movement.GetRouteGeneration(), endpoint, move.ToString(), waypointComponentId);
		reason = string.Empty;
		return true;
	}

	protected bool Invalidate(string reason)
	{
		foreach (AICF_VehicleNativeRoutePathRecord record : m_aRecords) record.m_bHasSample = false;
		m_iActiveRecord = -1;
		m_sDetails = " native_route_progress=" + reason;
		return false;
	}

	protected void Baseline(AICF_VehicleNativeRoutePathRecord record, AICF_VehicleNativeRouteProjection projection,
		vector position, float speed, int nowMs, string routeIdentity)
	{
		record.m_fHighWaterArc = Math.Max(record.m_fHighWaterArc, projection.m_fArc);
		record.m_fCreditAnchorArc = record.m_fHighWaterArc;
		record.m_fPhysicalTravel = 0;
		record.m_vPosition = position;
		record.m_fArc = projection.m_fArc;
		record.m_fSpeed = speed;
		record.m_iAtMs = nowMs;
		record.m_bHasSample = true;
		m_sRouteIdentity = routeIdentity;
	}

	static bool PlausibleStep(float arcDelta, float displacement, float previousSpeed, float speed, int elapsedMs)
	{
		if (elapsedMs <= 0 || elapsedMs > MAXIMUM_SAMPLE_INTERVAL_MS) return false;
		if (speed > MAXIMUM_SPEED_MPS || previousSpeed > MAXIMUM_SPEED_MPS) return false;
		float seconds = elapsedMs * 0.001;
		float velocityBudget = Math.Max(previousSpeed, speed) * seconds * 1.5 + 1;
		if (displacement > velocityBudget || displacement > MAXIMUM_SPEED_MPS * seconds + 1) return false;
		return arcDelta <= displacement * 2 + 1 && arcDelta <= MAXIMUM_SPEED_MPS * seconds + 1;
	}

	bool Observe(AICF_TransportTrip trip, int nowMs, float minimumAdvanceMeters)
	{
		if (!FiniteNumber(minimumAdvanceMeters) || minimumAdvanceMeters <= 0 || nowMs <= 0) return Invalidate("INVALID_INPUT");
		AICarMovementComponent car;
		Vehicle vehicle;
		string scope;
		string routeIdentity;
		vector endpoint;
		string contextReason;
		int waypointRplState;
		bool contextCurrent = ReadContext(trip, car, vehicle, scope, routeIdentity, endpoint, contextReason, waypointRplState);
		m_iWaypointRplState = waypointRplState;
		if (!contextCurrent) return Invalidate("NO_EXACT_CURRENT_NATIVE_CAR_ROUTE_" + contextReason);
		Physics physics = vehicle.GetPhysics();
		if (!physics) return Invalidate("NO_PHYSICS");
		vector position = vehicle.GetOrigin();
		vector velocity = physics.GetVelocity();
		if (!FinitePoint(position) || !FinitePoint(velocity)) return Invalidate("NONFINITE_PHYSICS");
		float speed = velocity.Length();
		if (speed > MAXIMUM_SPEED_MPS) return Invalidate("IMPLAUSIBLE_SPEED");
		array<vector> points = {};
		array<vector> repeated = {};
		car.GetCurrentPath(points);
		car.GetCurrentPath(repeated);
		AICarMovementComponent repeatedCar;
		Vehicle repeatedVehicle;
		string repeatedScope;
		string repeatedRoute;
		vector repeatedEndpoint;
		bool repeatedCurrent = ReadContext(trip, repeatedCar, repeatedVehicle, repeatedScope, repeatedRoute, repeatedEndpoint, contextReason, waypointRplState);
		m_iWaypointRplState = waypointRplState;
		if (!repeatedCurrent) return Invalidate("CONTEXT_CHANGED_DURING_READ_" + contextReason);
		if (car != repeatedCar || vehicle != repeatedVehicle || scope != repeatedScope || routeIdentity != repeatedRoute) return Invalidate("CONTEXT_CHANGED_DURING_READ");
		if (endpoint != repeatedEndpoint || vehicle.GetPhysics() != physics || vector.Distance(vehicle.GetOrigin(), position) > 0.1) return Invalidate("PHYSICS_CHANGED_DURING_READ");
		if (!SamePath(points, repeated)) return Invalidate("PATH_CHANGED_DURING_READ");
		if (!ValidatePath(points, endpoint)) return Invalidate("INCOMPLETE_OR_NONFINITE_NATIVE_PATH");
		AICF_VehicleNativeRouteProjection projection = ProjectUnique(points, position);
		if (!projection.m_sReason.IsEmpty()) return Invalidate(projection.m_sReason);
		if (scope != m_sScope)
		{
			Reset();
			m_sScope = scope;
			m_iWaypointRplState = waypointRplState;
		}
		int selected = -1;
		for (int index; index < m_aRecords.Count(); index++)
			if (SamePath(points, m_aRecords[index].m_aPoints)) selected = index;
		if (selected < 0)
		{
			if (m_aRecords.Count() >= MAXIMUM_PATH_VARIANTS) return Invalidate("PATH_HISTORY_EXHAUSTED");
			AICF_VehicleNativeRoutePathRecord created = new AICF_VehicleNativeRoutePathRecord();
			foreach (vector point : points) created.m_aPoints.Insert(point);
			selected = m_aRecords.Count();
			m_aRecords.Insert(created);
		}
		AICF_VehicleNativeRoutePathRecord record = m_aRecords[selected];
		if (selected != m_iActiveRecord || !record.m_bHasSample || routeIdentity != m_sRouteIdentity)
		{
			Invalidate("BASELINE_ONLY");
			m_iActiveRecord = selected;
			Baseline(record, projection, position, speed, nowMs, routeIdentity);
			return false;
		}
		int elapsed = nowMs - record.m_iAtMs;
		float displacement = vector.DistanceXZ(position, record.m_vPosition);
		float arcDelta = projection.m_fArc - record.m_fArc;
		if (!PlausibleStep(arcDelta, displacement, record.m_fSpeed, speed, elapsed) || arcDelta < -0.5)
		{
			Baseline(record, projection, position, speed, nowMs, routeIdentity);
			m_sDetails = " native_route_progress=REBASELINE_IMPLAUSIBLE_OR_REVERSE";
			return false;
		}
		bool physical = displacement >= 0.1 && Math.Max(speed, record.m_fSpeed) >= MINIMUM_SPEED_MPS && arcDelta > 0;
		if (physical) record.m_fPhysicalTravel += displacement;
		record.m_fHighWaterArc = Math.Max(record.m_fHighWaterArc, projection.m_fArc);
		float advance = record.m_fHighWaterArc - record.m_fCreditAnchorArc;
		bool credited = physical && advance >= minimumAdvanceMeters && record.m_fPhysicalTravel >= minimumAdvanceMeters * 0.5;
		m_sDetails = string.Format(" native_route_progress=OBSERVED native_path_points=%1 native_path_variant=%2 native_arc_m=%3 native_arc_advance_m=%4 native_physical_travel_m=%5 native_off_path_m=%6 native_height_delta_m=%7 native_segment=%8 native_progress_credited=%9",
			points.Count(), selected, projection.m_fArc, advance, record.m_fPhysicalTravel, projection.m_fGap, projection.m_fHeight, projection.m_iSegment, credited);
		m_sDetails += string.Format(" native_step_arc_m=%1 native_step_displacement_m=%2 native_sample_elapsed_ms=%3 native_speed_mps=%4", arcDelta, displacement, elapsed, speed);
		if (credited)
		{
			record.m_fCreditAnchorArc = record.m_fHighWaterArc;
			record.m_fPhysicalTravel = 0;
		}
		record.m_vPosition = position;
		record.m_fArc = projection.m_fArc;
		record.m_fSpeed = speed;
		record.m_iAtMs = nowMs;
		return credited;
	}
}
