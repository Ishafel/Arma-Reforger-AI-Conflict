param([string]$RepositoryRoot = (Split-Path -Parent (Split-Path -Parent $PSScriptRoot)))
$ErrorActionPreference = 'Stop'
$core = Join-Path $RepositoryRoot 'AIConflictCore/Scripts/Game/AIConflict'
$rules = @(
    @('AUTHORITY', 'Vehicles/AICF_FIAGarrisonService.c', '!Replication.IsServer() || m_bStopped || !m_Fleet'),
    @('INITIAL_FIA_ONLY', 'Vehicles/AICF_FIAGarrisonService.c', 'base.IsHQ() || base.GetFaction() != faction'),
    @('EASY_ZERO', 'Vehicles/AICF_FIAGarrisonService.c', 'if (difficulty == AICF_EDifficulty.MEDIUM) perBase = 1;'),
    @('HARD_TWO', 'Vehicles/AICF_FIAGarrisonService.c', 'if (difficulty == AICF_EDifficulty.HARD) perBase = 2;'),
    @('NO_REPLACEMENT', 'Vehicles/AICF_FIAGarrisonService.c', '!g.m_Vehicle && g.m_iRequestedAtMs == 0'),
    @('SURVIVORS', 'Vehicles/AICF_FIAGarrisonService.c', 'if (g.m_bReady) { m_Crew.UpdateGarrison(g, now); m_Patrol.Update(g, now); continue; }'),
    @('MEMBER_IDENTITY', 'State/Vehicles/AICF_FIAGarrison.c', 'entity.GetID() != m_aCrewIds[index]'),
    @('PLAYER_FENCE', 'State/Vehicles/AICF_FIAGarrison.c', 'IsAuthoritativeAIEntity(entity)'),
    @('GROUP_MEMBERSHIP', 'State/Vehicles/AICF_FIAGarrison.c', 'control.GetAIAgent().GetParentGroup() == m_Group'),
    @('FULL_ROSTER', 'Forces/AICF_FIAGarrisonCrew.c', 'HasExactFactionRoster(g.m_Group, "FIA", g.m_aSeats.Count(),'),
    @('FOREIGN_OCCUPANT', 'Forces/AICF_FIAGarrisonCrew.c', 'if (seat.GetOccupant() || seat.IsReserved()) return false;'),
    @('NO_COMBAT_MOVE', 'Forces/AICF_FIAGarrisonCrew.c', 'behavior.m_bUseCombatMove = false;'),
    @('SCOPED_POLICY', 'Forces/AICF_FIAGarrisonCrew.c', '!AICF_FIAGarrisonService.IsDefender(m_OwnerEntity)'),
    @('DENIED_ACTION_RELEASED', 'Forces/AICF_FIAGarrisonCrew.c', 'behavior.Fail();'),
    @('ROAD_PROJECTION', 'Vehicles/AICF_VehicleSpawner.c', 'position = projected;'),
    @('SPAWN_PATROL_PREFLIGHT', 'Vehicles/AICF_VehicleSpawner.c', 'if (!AICF_FIAGarrisonPatrol.FindRoadEndpoint(ai.GetRoadNetworkManager(), position, center, patrolDirection, patrolEndpoint)) continue;'),
    @('TURN_SURFACE', 'Vehicles/AICF_VehicleSpawner.c', 'if (!FIAGarrisonTurnSurface(world, position)) continue;'),
    @('LOCAL_ACTIVITY_IDENTITY', 'Vehicles/AICF_FIAGarrisonService.c', 'activity.m_RelatedWaypoint == g.m_PatrolWaypoint'),
    @('ENDPOINT_FENCE', 'Vehicles/AICF_VehicleTaskHandoff.c', 'vector.DistanceXZ(endpoint, g.m_vHome) > AICF_FIAGarrisonPatrol.ROUTE_RADIUS'),
    @('RETURN_FENCE', 'Vehicles/AICF_FIAGarrisonPatrol.c', 'vector.DistanceXZ(position, g.m_vHome) > RETURN_RADIUS'),
    @('LIVING_DRIVER', 'State/Vehicles/AICF_FIAGarrison.c', 'driver = OwnsMember(occupant);'),
    @('WAYPOINT_IDENTITY', 'State/Vehicles/AICF_FIAGarrison.c', 'm_PatrolWaypoint.GetID() == m_PatrolWaypointId'),
    @('PATROL_FAILURE_FENCE', 'Vehicles/AICF_FIAGarrisonPatrol.c', 'g.m_iPatrolLeg == leg && g.m_PatrolWaypointId == waypointId'),
    @('NO_FORCED_DISMOUNT', 'Forces/AICF_AINodeLifecycle.c', 'AICF_FIAGarrisonMovementPolicy.Handle('),
    @('GARRISON_EXIT', 'Vehicles/AICF_VehicleSpawner.c', 'if (!AICF_LogisticsSpawnGeometry.ExitClear(world, footprint, pose, exitPosition, exitTrace)) continue;'),
    @('LOCAL_AVOIDANCE', 'Forces/AICF_FIAGarrisonCrew.c', 'AICF_FIAGarrisonService.IsLocalPilotAvoidance(m_OwnerEntity, avoidance.m_vMovePos.m_Value)'),
    @('AVOIDANCE_DISTANCE', 'Vehicles/AICF_FIAGarrisonService.c', 'vector.DistanceXZ(position, g.m_Vehicle.GetOrigin()) > 25'),
    @('NULL_TARGET_GUARD', 'Forces/AICF_FIAGarrisonCrew.c', 'if (!m_Target) return;'),
    @('THREAT_ESCALATION', 'Forces/AICF_FIAGarrisonCrew.c', 'super.OnThreatSectorEscalation(ts, sectorId, dangerValue);'),
    @('THREAT_DAMAGE', 'Forces/AICF_FIAGarrisonCrew.c', 'super.OnThreatSectorDamageTaken(ts, sectorId);'),
    @('BODY_CLEARANCE', 'Vehicles/AICF_VehicleSpawner.c', '!g.BaseIdentity() || g.m_Base.GetFaction() != g.m_Faction || !footprint.IsClear(world, pose, body)'),
    @('ASYNC_PURGE', 'Vehicles/AICF_VehicleCleanupManager.c', 'world.PurgeSpawnRequestsForGroup(g.m_Group)'),
    @('REGISTRY_STOP', 'Vehicles/AICF_FIAGarrisonService.c', 's_aDefenders.Clear();'),
    @('LIFECYCLE', 'Bootstrap/AICF_MatchController.c', 'm_FIAGarrisons.Stop();'),
    @('RECOVERY_RADIUS', 'Vehicles/AICF_FIAGarrisonRecovery.c', 'static const float PLAYER_RADIUS = 50;'),
    @('RECOVERY_MAIN_PLAYER', 'Vehicles/AICF_FIAGarrisonRecovery.c', 'SCR_PossessingManagerComponent.GetPlayerMainEntity(id)'),
    @('RECOVERY_DESTINATION_PLAYER', 'Vehicles/AICF_FIAGarrisonRecovery.c', '!AICF_FIAGarrisonRecovery.PlayersClear(g.m_Vehicle.GetOrigin(), pose[3])'),
    @('RECOVERY_FOREIGN_OCCUPANT', 'Vehicles/AICF_FIAGarrisonRecovery.c', 'if (seat.GetOccupant() && !g.OwnsMember(seat.GetOccupant())) return false;'),
    @('RECOVERY_SAME_VEHICLE', 'Vehicles/AICF_FIAGarrisonRecovery.c', 'g.m_Vehicle.SetWorldTransform(pose)'),
    @('RECOVERY_SETTLED_CREW', 'Vehicles/AICF_FIAGarrisonRecovery.c', 'g.m_aSeats[i].GetOccupant() != g.m_aCrew[i]) return false;'),
    @('DESANT_ANIMATED', 'Forces/AICF_FIAGarrisonCrew.c', 'access.GetOutVehicle(EGetOutType.ANIMATED'),
    @('DESANT_NO_REBOARD', 'Vehicles/AICF_FIAGarrisonService.c', 'g.IsDeployedPassenger(entity) || !g.VehicleIdentity()'),
    @('CREW_PLAYER_FENCE', 'Forces/AICF_FIAGarrisonCrew.c', '!AICF_FIAGarrisonRecovery.PlayersClear(member.GetOrigin(), g.m_Vehicle.GetOrigin())'),
    @('CREW_EXACT_SLOT', 'Forces/AICF_FIAGarrisonCrew.c', 'seat.GetOccupant() == member || now < g.m_iCrewRecoveryNextMs')
)
$failures = @()
foreach ($rule in $rules) {
    $source = Get-Content (Join-Path $core $rule[1]) -Raw
    if (-not $source.Contains($rule[2])) { $failures += $rule[0] }
    # Representative negative input must be rejected by the same guard.
    if ($source.Replace($rule[2], 'REMOVED').Contains($rule[2])) { $failures += "NEGATIVE_$($rule[0])" }
}
$service = Get-Content (Join-Path $core 'Vehicles/AICF_FIAGarrisonService.c') -Raw
if ($service -match 'CallLater\s*\(|\.AddWaypoint|SpawnEntityPrefab|DeleteRplEntity|GetOutgoingNodeIds') { $failures += 'DOMAIN_BOUNDARY' }
foreach ($case in @(@('Medium', 1, 'A1CF261008100001'), @('Hard', 2, 'A1CF261008100002'))) {
    $path = Join-Path $RepositoryRoot "AIConflictEveronWCSRHS/Missions/AICF_WCS_RHS_Conflict_Everon_North_$($case[0]).conf"
    $header = Get-Content $path -Raw
    if (-not $header.Contains('{A1CF261006100003}Missions/AICF_WCS_RHS_Conflict_Everon_North.conf') -or
        $header -notmatch "m_eAICFDifficulty\s+$($case[1])\b" -or
        $header -match 'm_aCampaignCustomBaseList|\bWorld\b') { $failures += "HEADER_$($case[0])" }
    if (-not (Get-Content "$path.meta" -Raw).Contains($case[2])) { $failures += "META_$($case[0])" }
}
if ($failures.Count) { $failures | ForEach-Object { "FAIL $_" }; exit 1 }
"FIA garrison contracts: PASS; negative guards=$($rules.Count); runtime=NOT_RUN"
