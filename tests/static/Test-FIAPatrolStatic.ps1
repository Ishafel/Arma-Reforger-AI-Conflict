param([string]$RepositoryRoot = (Split-Path -Parent (Split-Path -Parent $PSScriptRoot)))
$ErrorActionPreference = 'Stop'
$core = Join-Path $RepositoryRoot 'AIConflictCore/Scripts/Game/AIConflict'
$files = @{
    service = 'Vehicles/AICF_FIAPatrolService.c'
    state = 'State/Vehicles/AICF_FIAPatrol.c'
    fleet = 'State/Vehicles/AICF_FactionFleet.c'
    spawn = 'Vehicles/AICF_VehicleSpawner.c'
    crew = 'Forces/AICF_FIAPatrolCrew.c'
    handoff = 'Vehicles/AICF_VehicleTaskHandoff.c'
    cleanup = 'Vehicles/AICF_VehicleCleanupManager.c'
    root = 'Bootstrap/AICF_MatchController.c'
}
$sources = @{}
foreach ($key in $files.Keys) { $sources[$key] = Get-Content (Join-Path $core $files[$key]) -Raw }
# Guards, а не runtime PASS: отрицательные мутации обязаны нарушать каждый guard.
$rules = @(
    @('SERVER', 'service', '!Replication.IsServer() || m_bStopped', '!Replication.IsServer() || m_bStopped'),
    @('COUNT', 'service', 'm_iObjectives / 2', 'm_iObjectives / 2'),
    @('HQ', 'service', '!node.GetBase().IsHQ()', '!node.GetBase().IsHQ()'),
    @('FLEET_CAP', 'fleet', 'GetLeaseCount() >= objectiveCount / 2', 'GetLeaseCount() >= objectiveCount / 2'),
    @('FLEET_SLOT', 'fleet', 'HasLeaseForSlot(patrol.m_iSlot)', 'HasLeaseForSlot(patrol.m_iSlot)'),
    @('ASYNC_ROSTER', 'crew', 'HasExactFactionRoster(p.m_Group, "FIA", 2,', 'HasExactFactionRoster(p.m_Group, "FIA", 2,'),
    @('GUNNER', 'state', 'm_TurretSeat.GetOccupant() == m_Gunner', 'm_TurretSeat.GetOccupant() == m_Gunner'),
    @('PLAYER_FENCE', 'state', 'IsAuthoritativeAIEntity(m_Driver)', 'IsAuthoritativeAIEntity(m_Driver)'),
    @('NO_FOREIGN_DRIVER', 'handoff', 'p.m_PilotSeat.GetOccupant() != p.m_Driver', 'p.m_PilotSeat.GetOccupant() != p.m_Driver'),
    @('ARRIVAL_REQUIRES_CREW', 'service', 'p.m_Target && p.Seated()', 'p.m_Target && p.Seated()'),
    @('DIRECTED_EDGE', 'service', 'source.GetOutgoingNodeIds()', 'source.GetOutgoingNodeIds()'),
    @('REVISION', 'service', 'p.m_iGraphRevision != revision', 'p.m_iGraphRevision != revision'),
    @('PHYSICAL_ARRIVAL', 'service', 'vector.DistanceXZ(p.m_Vehicle.GetOrigin(), p.m_vEndpoint) <= 30', 'vector.DistanceXZ(p.m_Vehicle.GetOrigin(), p.m_vEndpoint) <= 30'),
    @('SURFACE', 'spawn', 'AICF_LogisticsSpawnGeometry.FitToSurface(world, footprint, pose)', 'AICF_LogisticsSpawnGeometry.FitToSurface(world, footprint, pose)'),
    @('BODY', 'spawn', 'footprint.IsClear(world, pose, body)', 'footprint.IsClear(world, pose, body)'),
    @('EXIT', 'spawn', 'AICF_LogisticsSpawnGeometry.ExitClear(world, footprint, pose, exitPosition, exitTrace)', 'AICF_LogisticsSpawnGeometry.ExitClear(world, footprint, pose, exitPosition, exitTrace)'),
    @('PURGE', 'cleanup', 'world.PurgeSpawnRequestsForGroup(p.m_Group)', 'world.PurgeSpawnRequestsForGroup(p.m_Group)'),
    @('STOP', 'root', 'm_FIAPatrols.Stop()', 'm_FIAPatrols.Stop()')
)
$failures = [System.Collections.Generic.List[string]]::new()
foreach ($rule in $rules) {
    $text = $sources[$rule[1]]
    if (-not $text.Contains($rule[2])) { $failures.Add($rule[0]) }
    $mutant = $text.Replace($rule[3], 'REMOVED_GUARD')
    if ($mutant.Contains($rule[2])) { $failures.Add("NEGATIVE_$($rule[0])") }
}
if ($sources.handoff -notmatch '(?s)void ClearFIAPatrolWaypoint.*?RemoveWaypoint\(waypoint\).*?DeleteRplEntity\(waypoint') { $failures.Add('DETACH_BEFORE_DELETE') }
$detach = [regex]::Match($sources.handoff, '(?s)\tvoid DetachFIAPatrol\(.*?\n\t\}').Value
if (-not $detach.Contains('ClearFIAPatrolWaypoint(p);') -or $detach -match 'RemoveUsableVehicle\s*\(') { $failures.Add('RETAIN_NATIVE_VEHICLE_UNTIL_ENTITY_CLEANUP') }
$detachMutant = $detach.Replace('ClearFIAPatrolWaypoint(p);', 'ClearFIAPatrolWaypoint(p); utility.RemoveUsableVehicle(usage);')
if ($detachMutant -notmatch 'RemoveUsableVehicle\s*\(') { $failures.Add('NEGATIVE_RETAIN_NATIVE_VEHICLE') }
if ($sources.service -match '\.(AddWaypoint|RemoveWaypoint|SpawnEntityPrefabEx|DeleteRplEntity)\(') { $failures.Add('DOMAIN_OWNER') }
if ($sources.service -match 'CallLater\(|\.Insert\(On') { $failures.Add('NO_SECOND_LOOP') }
if ($sources.service.Contains('if (!p.Seated()) continue;')) { $failures.Add('NATIVE_REBOARD_ROUTE_BLOCKED') }
if (Test-Path (Join-Path $core 'Vehicles/AICF_FIAPatrolProbe.c')) { $failures.Add('FIXTURE_IN_PRODUCTION') }
if ($failures.Count) { $failures | ForEach-Object { Write-Output "FAIL $_" }; exit 1 }
Write-Output "FIA patrol static: PASS; negative guards=$($rules.Count); runtime=NOT_RUN"
