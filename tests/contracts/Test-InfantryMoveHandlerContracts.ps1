param(
    [string]$RepositoryRoot = (Split-Path -Parent (Split-Path -Parent $PSScriptRoot)),
    [string]$RuntimeLogPath,
    [string]$ManifestPath
)
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot '../lib/Stage3StaticAudit.Common.ps1')
$source = Get-Content -LiteralPath (Join-Path $RepositoryRoot 'AIConflictCore/Scripts/Game/AIConflict/Forces/AICF_AINodeLifecycle.c') -Raw -Encoding UTF8
$record = [pscustomobject]@{ Source=$source; Code=(ConvertTo-AICFCodeText $source) }
$body = ConvertTo-AICFCodeText (Get-AICFMethodBody $record 'AICF_IsInfantryMoveHandler')
$rules = @(
    @('DEFAULT_HANDLER', 'if \(handler == AIGroupMovementComponent.DEFAULT_HANDLER_ID\)\s*return true;'),
    @('SENTINEL_SCOPE', 'if \(handler != -1 \|\| !m_GroupUtilityComponent.m_VehicleMgr\)\s*return false;'),
    @('UNASSIGNED_MOVE', 'if \(registered && !SCR_AIMoveActivity.Cast\(m_GroupUtilityComponent.GetCurrentAction\(\)\)\)\s*return false;'),
    @('LIVE_ROSTER', 'if \(!AICF_GroupRuntime.IsAliveCharacter\(member\)\)\s*continue;'),
    @('MEMBER_IDENTITY', 'if \(agent.GetParentGroup\(\) != m_Group\)\s*return false;'),
    @('PHYSICAL_FOOT', '!access \|\| access.IsInCompartment\(\) \|\| access.IsGettingIn\(\) \|\| access.IsGettingOut\(\) \|\|\s*CompartmentAccessComponent.GetVehicleIn\(member\)'),
    @('NONEMPTY_ROSTER', 'return alive > 0;')
)
$failures = [Collections.Generic.List[string]]::new()
foreach ($rule in $rules) {
    Assert-AICFContains $failures $rule[0] $body $rule[1] 'Infantry handler guard missing'
}
if ($failures.Count) { $failures; exit 1 }
foreach ($rule in $rules) {
    $mutated = [regex]::Replace($body, $rule[1], '')
    $rejected = [Collections.Generic.List[string]]::new()
    Assert-AICFContains $rejected $rule[0] $mutated $rule[1] 'Removed guard'
    if (!$rejected.Count) { throw "Mutation escaped: $($rule[0])" }
    "PASS negative mutation=$($rule[0])"
}
if ($RuntimeLogPath) {
    if (!$ManifestPath) { throw 'Runtime requires the actual launcher manifest' }
    $log = Get-Content -LiteralPath $RuntimeLogPath -Raw -Encoding UTF8
    $manifest = Get-Content -LiteralPath $ManifestPath -Raw -Encoding UTF8 | ConvertFrom-Json
    $cli = [regex]::Match($log, '(?m)^[^\r\n]*CLI Params:\s*([^\r\n]*)')
    if (!$cli.Success -or $cli.Groups[1].Value.Trim() -cne ($manifest.arguments -join ' ')) { throw 'Runtime CLI mismatch' }
    if ($log -match 'SCRIPT\s*\((E|F)\)|ENGINE\s*\(F\)|Virtual Machine Exception') { throw 'Runtime script/VM/fatal failure' }
    if (!$log.Contains('[AICF][STAGE1][INFO][ROSTER_READY]')) { throw 'Roster never became ready' }
    $expected = @('ROSTER_NINE','MUSTER_COMPLETE','POINT_ORDER','CURRENT_MOVE_ACTIVITY','FOOT_SENTINEL','FOREIGN_HANDLER','UNRELATED_REJECTED','UNASSIGNED_REGISTRY','FOOT_WITH_UNUSED_VEHICLE','ASSIGNED_VEHICLE_REJECTED','BOARD_REQUEST','PHYSICAL_OCCUPANT_REJECTED','EXIT_REQUEST','POINT_RESTORED_AFTER_EXIT','FOOT_AFTER_EXIT','FOREIGN_WAYPOINT_REJECTED')
    $expected += @('FAILURE_REPORTED','FAILURE_VISIBLE','STALE_FAILURE_FENCES')
    $checks = @([regex]::Matches($log, '\[INFANTRY_HANDLER_CHECK\] name=(\w+) passed=([01])'))
    if ($checks.Count -ne $expected.Count) { throw 'Unexpected runtime check count' }
    foreach ($name in $expected) {
        $found = @($checks | Where-Object { $_.Groups[1].Value -ceq $name })
        if ($found.Count -ne 1 -or $found[0].Groups[2].Value -ne '1') { throw "Runtime check failed or missing: $name" }
    }
    $end = @([regex]::Matches($log, '\[INFANTRY_HANDLER_FINISHED\] checks=(\d+) failures=(\d+)'))
    if ($end.Count -ne 1 -or [int]$end[0].Groups[1].Value -ne $expected.Count -or $end[0].Groups[2].Value -ne '0') { throw 'Missing successful probe completion' }
    "Infantry handler runtime: PASS; checks=$($checks.Count)"
}
"Infantry handler contracts: PASS; guards=$($rules.Count); runtime=$([bool]$RuntimeLogPath)"
