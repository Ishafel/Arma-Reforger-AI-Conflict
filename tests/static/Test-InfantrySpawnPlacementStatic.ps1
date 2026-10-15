param([string]$RepositoryRoot = (Split-Path -Parent (Split-Path -Parent $PSScriptRoot)))
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot '../lib/Stage3StaticAudit.Common.ps1')
$path = Join-Path $RepositoryRoot 'AIConflictCore/Scripts/Game/AIConflict/Forces/AICF_InfantrySpawnPlacement.c'
$source = Get-Content -LiteralPath $path -Raw -Encoding UTF8
$vehiclePath = Join-Path $RepositoryRoot 'AIConflictCore/Scripts/Game/AIConflict/Forces/AICF_InfantryVehicleClearance.c'
$vehicleSource = Get-Content -LiteralPath $vehiclePath -Raw -Encoding UTF8
function Test-Placement([string]$Text) {
    $code = ConvertTo-AICFCodeText $Text
    $issues = [Collections.Generic.List[string]]::new()
    Assert-AICFContains $issues 'NAVMESH_PENDING_REQUEST' $code 'if\s*\(!navmesh.IsTileLoaded\(query\)\)\s*\{\s*if\s*\(!navmesh.IsTileRequested\(query\)\)\s*navmesh.LoadTileIn\(query\);\s*return false;\s*\}' 'A pending tile must wait without duplicate native requests or premature placement'
    Assert-AICFContains $issues 'NAVMESH_READY_PROJECTION' $code '(?s)GetClosestPositionOnNavmesh\(query,.*?vector.DistanceSqXZ\(query, position\) > 2.*?Math.AbsFloat.*?> 1.*?return false;' 'Loaded tile still requires bounded, terrain-safe projection'
    Assert-AICFContains $issues 'PHYSICAL_VEHICLE_CLEARANCE' $code '(?s)if \(world.TracePosition\(body, null\) < 0\)\s*return false;\s*if \(!AICF_InfantryVehicleClearance.IsClear\(world, position\)\)\s*return false;' 'Infantry must also reject the buffered bounds of falling or moving vehicles'
    return $issues.ToArray()
}
function Test-VehicleClearance([string]$Text) {
    $code = ConvertTo-AICFCodeText $Text
    $issues = [Collections.Generic.List[string]]::new()
    Assert-AICFContains $issues 'VEHICLE_QUERY' $code '(?s)QUERY_RADIUS_METERS = 16;.*?if \(!world\)\s*return false;.*?bool completed = world.QueryEntitiesBySphere\(position, QUERY_RADIUS_METERS, query.Collect, null, EQueryEntitiesFlags.ALL\);\s*return completed && !query.m_bBlocked;' 'The bounded query must fail closed on cancellation or a blocking vehicle'
    Assert-AICFContains $issues 'VEHICLE_ROOT' $code '(?s)Vehicle.Cast\(entity\).*?Vehicle.Cast\(entity.GetRootParent\(\)\).*?m_aSeen.Find\(vehicle\).*?m_aSeen.Insert\(vehicle\)' 'Direct vehicles and vehicle children must share one bounds check'
    Assert-AICFContains $issues 'VEHICLE_BOUNDS' $code '(?s)VEHICLE_MARGIN_METERS = 2;.*?vehicle.GetWorldBounds\(mins, maxs\);.*?m_vPosition\[0\] >= mins\[0\] - VEHICLE_MARGIN_METERS && m_vPosition\[0\] <= maxs\[0\] \+ VEHICLE_MARGIN_METERS &&\s*m_vPosition\[2\] >= mins\[2\] - VEHICLE_MARGIN_METERS && m_vPosition\[2\] <= maxs\[2\] \+ VEHICLE_MARGIN_METERS' 'Fresh horizontal vehicle bounds need a two metre margin regardless of height'
    Assert-AICFContains $issues 'VEHICLE_VETO' $code '(?s)bool valid = maxs\[0\] > mins\[0\] && maxs\[2\] > mins\[2\];\s*if \(!valid \|\|.*?m_bBlocked = true;\s*return false;' 'Invalid bounds and occupied candidates must stop the query'
    return $issues.ToArray()
}
$issues = @(Test-Placement $source) + @(Test-VehicleClearance $vehicleSource)
if ($issues.Count) { $issues; exit 1 }
$negativeCount = 0
foreach ($mutation in @(
    @{name='pending-guard-removed'; before='if (!navmesh.IsTileRequested(query))'; after=''},
    @{name='pending-guard-inverted'; before='if (!navmesh.IsTileRequested(query))'; after='if (navmesh.IsTileRequested(query))'},
    @{name='waiting-returns-ready'; before="navmesh.LoadTileIn(query);`r`n`t`t`treturn false;"; after="navmesh.LoadTileIn(query);`r`n`t`t`treturn true;"},
    @{name='vehicle-guard-removed'; before='if (!AICF_InfantryVehicleClearance.IsClear(world, position))'; after='if (false)'; rule='PHYSICAL_VEHICLE_CLEARANCE'},
    @{name='vehicle-guard-inverted'; before='if (!AICF_InfantryVehicleClearance.IsClear(world, position))'; after='if (AICF_InfantryVehicleClearance.IsClear(world, position))'; rule='PHYSICAL_VEHICLE_CLEARANCE'}
)) {
    $normalized = $source.Replace("`r`n", "`n")
    $changed = $normalized.Replace($mutation.before.Replace("`r`n", "`n"), $mutation.after.Replace("`r`n", "`n"))
    $rule = 'NAVMESH_PENDING_REQUEST'
    if ($mutation.ContainsKey('rule')) { $rule = $mutation.rule }
    if ($changed -eq $normalized -or -not (@(Test-Placement $changed) -match ('\[' + $rule + '\]'))) {
        throw "Mutation escaped: $($mutation.name)"
    }
    $negativeCount++
    Write-Output "PASS negative mutation=$($mutation.name)"
}
foreach ($mutation in @(
    @{name='vehicle-query-cancel-ignored'; before='return completed && !query.m_bBlocked;'; after='return !query.m_bBlocked;'; rule='VEHICLE_QUERY'},
    @{name='vehicle-root-ignored'; before='Vehicle.Cast(entity.GetRootParent())'; after='null'; rule='VEHICLE_ROOT'},
    @{name='vehicle-margin-removed'; before='VEHICLE_MARGIN_METERS = 2;'; after='VEHICLE_MARGIN_METERS = 0;'; rule='VEHICLE_BOUNDS'},
    @{name='vehicle-invalid-bounds-ignored'; before='if (!valid ||'; after='if (valid &&'; rule='VEHICLE_VETO'},
    @{name='vehicle-veto-ignored'; before='m_bBlocked = true;'; after='m_bBlocked = false;'; rule='VEHICLE_VETO'}
)) {
    $changed = $vehicleSource.Replace($mutation.before, $mutation.after)
    if ($changed -eq $vehicleSource -or -not (@(Test-VehicleClearance $changed) -match ('\[' + $mutation.rule + '\]'))) {
        throw "Mutation escaped: $($mutation.name)"
    }
    $negativeCount++
    Write-Output "PASS negative mutation=$($mutation.name)"
}
Write-Output "Infantry spawn placement static: PASS; negative mutations=$negativeCount"
exit 0
