param(
    [string]$RepositoryRoot = (Split-Path -Parent $PSScriptRoot)
)

$ErrorActionPreference = 'Stop'
$failures = [System.Collections.Generic.List[string]]::new()

function Assert-FileContains {
    param(
        [string]$RelativePath,
        [string]$Pattern,
        [string]$Description
    )

    $path = Join-Path $RepositoryRoot $RelativePath
    if (-not (Test-Path -LiteralPath $path)) {
        $failures.Add("Missing file: $RelativePath")
        return
    }

    $content = Get-Content -LiteralPath $path -Raw
    if ($content -notmatch $Pattern) {
        $failures.Add("$Description ($RelativePath)")
    }
}

function Assert-FileNotContains {
    param(
        [string]$RelativePath,
        [string]$Pattern,
        [string]$Description
    )

    $path = Join-Path $RepositoryRoot $RelativePath
    if (-not (Test-Path -LiteralPath $path)) {
        $failures.Add("Missing file: $RelativePath")
        return
    }

    $content = Get-Content -LiteralPath $path -Raw
    if ($content -match $Pattern) {
        $failures.Add("$Description ($RelativePath)")
    }
}

$coordinator = 'AIConflictCore/Scripts/Game/AIConflict/Vehicles/AICF_VehicleCoordinator.c'
$runtime = 'AIConflictCore/Scripts/Game/AIConflict/State/AICF_VehicleRuntime.c'
$spawner = 'AIConflictCore/Scripts/Game/AIConflict/Vehicles/AICF_VehicleSpawner.c'
$waypoints = 'AIConflictCore/Scripts/Game/AIConflict/Vehicles/AICF_VehicleWaypointFactory.c'
$match = 'AIConflictCore/Scripts/Game/AIConflict/Bootstrap/AICF_MatchController.c'
$config = 'AIConflictCore/Scripts/Game/AIConflict/Config/AICF_Stage3Config.c'
$stage1Config = 'AIConflictCore/Scripts/Game/AIConflict/Config/AICF_Stage1Config.c'
$groupMarkers = 'AIConflictCore/Scripts/Game/AIConflict/UI/AICF_GroupMapMarkers.c'

Assert-FileContains $config 'm_bVehiclesEnabled\s*=\s*false' 'Stage 3 must preserve Stage 2 unless explicitly enabled'
Assert-FileContains $spawner '!Replication\.IsServer\(\).*campaign\.IsMaster\(\)' 'Vehicle spawning must be server/master-only'
Assert-FileContains $spawner 'SCR_WorldTools\.FindEmptyTerrainPosition' 'Vehicle spawning must use an empty-terrain query'
Assert-FileContains $coordinator 'runtimes\[slotId\]\s*=\s*runtime;\s*slot\.SetVehicleRuntime\(runtime\);' 'A slot must reserve cap capacity before spawning'
Assert-FileContains $coordinator 'slot\.GetSpawnGeneration\(\)\s*==\s*runtime\.GetGroupGeneration\(\)' 'Runtime callbacks/polls must be guarded by group generation'
Assert-FileContains $runtime 'm_iVehicleGeneration\+\+' 'Safe reuse must advance vehicle generation'
Assert-FileContains $coordinator 'RemoveUsableVehicle' 'Fallback/cleanup must detach the vehicle from group utility'
Assert-FileContains $waypoints 'group\.RemoveWaypoint\(waypoint\)[\s\S]*RplComponent\.DeleteRplEntity\(waypoint' 'Stage 3 waypoints must be removed and deleted'
Assert-FileContains $coordinator 'RplComponent\.DeleteRplEntity\(runtime\.GetVehicle\(\)' 'Terminal vehicle cleanup must delete the replicated entity'
Assert-FileContains $coordinator 'RestoreInfantryOrder' 'Every vehicle terminal path must preserve an infantry fallback'
Assert-FileContains $match 'm_VehicleCoordinator\.Stop\(cleanupEntities\)' 'Match shutdown must stop the vehicle coordinator'
Assert-FileContains $match 'm_VehicleCoordinator\.IsControllingMovement\(slot\)' 'Infantry reliability must not overwrite vehicle waypoints'
Assert-FileContains $match 'm_GroupMapMarkers\s*=\s*new AICF_GroupMapMarkerSystem\(\)' 'Gameplay group markers must always be created'
Assert-FileNotContains $stage1Config 'aicfDebugMapMarkers|DebugMapMarkers' 'Gameplay group markers must not depend on the removed debug CLI flag'
Assert-FileContains $groupMarkers 'GROUP_MAP_MARKERS_READY' 'Gameplay marker readiness must use the non-debug event contract'
Assert-FileContains $groupMarkers 'SetFaction\(null\)[\s\S]*SetGlobalVisible\(true\)' 'Current marker policy must show both factions globally'
Assert-FileNotContains 'AIConflictCore/Scripts/Game/AIConflict/Vehicles/AICF_VehicleCoordinator.c' 'CallLater|GetCallqueue' 'Vehicle lifecycle must not leave delayed callbacks'

$requiredEvents = @(
    'CONFIG', 'VEHICLE_STATE_CHANGED', 'VEHICLE_REQUESTED', 'VEHICLE_SPAWN_SITE_SELECTED',
    'VEHICLE_SPAWN_SITE_REJECTED', 'VEHICLE_SPAWNED', 'VEHICLE_ASSIGNED',
    'DRIVER_ASSIGNED', 'GUNNER_ASSIGNED', 'PASSENGERS_ASSIGNED', 'BOARDING_STARTED',
    'BOARDING_COMPLETE', 'BOARDING_TIMEOUT', 'VEHICLE_ROUTE_ASSIGNED', 'VEHICLE_PROGRESS', 'DISEMBARK_STARTED',
    'DISEMBARK_COMPLETE', 'VEHICLE_STUCK_DETECTED', 'VEHICLE_RECOVERY_STARTED',
    'VEHICLE_RECOVERY_SUCCEEDED', 'VEHICLE_RECOVERY_FAILED', 'DRIVER_LOST',
    'DRIVER_REASSIGNED', 'VEHICLE_ABANDONED', 'INFANTRY_FALLBACK',
    'VEHICLE_DESTROYED', 'VEHICLE_CAP_BLOCKED', 'VEHICLE_CLEANUP',
    'HEARTBEAT', 'RESULT'
)

$stage3Sources = Get-ChildItem (Join-Path $RepositoryRoot 'AIConflictCore/Scripts/Game/AIConflict') -Filter '*.c' -File -Recurse |
    Get-Content -Raw
$allStage3Source = $stage3Sources -join "`n"
foreach ($eventName in $requiredEvents) {
    if ($allStage3Source -notmatch [regex]::Escape('"' + $eventName + '"')) {
        $failures.Add("Missing Stage 3 diagnostic event: $eventName")
    }
}

if ($failures.Count -gt 0) {
    Write-Host "Stage 3 static audit: FAIL ($($failures.Count) issue(s))" -ForegroundColor Red
    $failures | ForEach-Object { Write-Host " - $_" -ForegroundColor Red }
    exit 1
}

Write-Host 'Stage 3 static audit: PASS'
Write-Host 'Checked: authority, disabled default, cap reservation, generations, waypoint/entity cleanup, infantry fallback, always-global group markers, callback absence, diagnostics contract.'
