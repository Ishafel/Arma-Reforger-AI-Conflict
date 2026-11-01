param(
 [Parameter(Mandatory)][string]$LogPath,
 [Parameter(Mandatory)][string]$LauncherPath,
 [Parameter(Mandatory)][string]$NativeExitPath
)
$ErrorActionPreference='Stop'
$full=[IO.File]::ReadAllText((Resolve-Path -LiteralPath $LogPath).Path)
$launcher=[IO.File]::ReadAllText((Resolve-Path -LiteralPath $LauncherPath).Path)
$exitCode=[int]([IO.File]::ReadAllText((Resolve-Path -LiteralPath $NativeExitPath).Path).Trim())
$failures=[Collections.Generic.List[string]]::new()
function Require([bool]$condition,[string]$rule){if(!$condition){$failures.Add($rule)}}
Require ($exitCode -eq 0) 'NATIVE_EXIT_ZERO'
Require ($full.Contains('Game destroyed.')) 'FULL_STOPPED_CONSOLE'
$manifestMatch=[regex]::Match($launcher,'AICF_RUNTIME_MANIFEST_JSON=([^\r\n]+)')
Require $manifestMatch.Success 'CANONICAL_MANIFEST'
if($manifestMatch.Success){
 $manifest=$manifestMatch.Groups[1].Value|ConvertFrom-Json
 $argsList=@($manifest.arguments)
 $flag=[array]::IndexOf($argsList,'-aicfHealthyNativeAbandonProbe')
 Require ($flag -ge 0 -and $argsList[$flag+1] -eq '1') 'OPT_IN_EXACT_NATIVE_PROBE'
 $canonical=[array]::IndexOf($argsList,'-aicfVehicleScenarioCanonicalOwner')
 Require ($canonical -ge 0 -and $argsList[$canonical+1] -eq '1') 'CANONICAL_OWNER_OPT_IN'
}
Require ($full -match '\[HEALTHY_NATIVE_ABANDON_PROBE\] event=CANONICAL_READY actual=5 initial_generation=1 unit_type=INFANTRY before_coordinator_tick=1') 'CANONICAL_ROOT_READY'
Require ($full -match '\[VEHICLE_SCENARIO_CHECK\][^\r\n]*rule=CANONICAL_OWNER passed=1') 'CANONICAL_QUOTA_OWNER_READBACK'
Require ($full -notmatch '\[VEHICLE_SCENARIO_CHECK\][^\r\n]*passed=0') 'SCAFFOLD_NO_FAILED_CHECKS'
$start=[regex]::Match($full,'\[HEALTHY_NATIVE_ABANDON_PROBE\] event=INJECTED operation_id=(\S+) vehicle=(\S+) route_generation=(\d+) attempts=(\d+) deadline=(\d+) health_mutation=0 pose_mutation=0 controlled_fault=1')
$decision=[regex]::Match($full,'\[HEALTHY_NATIVE_ABANDON_PROBE\] event=DECISION operation_id=(\S+) observed=1 recovered=1 attempts=(\d+) deadline=(\d+) controlled_fault=1 native_selection=1 production_outcome_unchanged=1')
$done=[regex]::Match($full,'\[HEALTHY_NATIVE_ABANDON_PROBE\] event=DONE operation_id=(\S+) vehicle=(\S+) displacement_m=([\d.]+) moving_polls=(\d+) attempts=(\d+) deadline=(\d+) original_roster=1 controlled_fault=1 natural_failure_proof=0')
Require $start.Success 'ACTUAL_STOCK_ACTIVITY_QUEUED_AFTER_MOVEMENT'
Require $decision.Success 'ACTUAL_PRODUCTION_RECOVERY_AND_NATIVE_CANCEL'
Require $done.Success 'SAME_ASSET_PHYSICAL_MOVEMENT_RECEIPT'
if($start.Success -and $decision.Success -and $done.Success){
 Require ($start.Index -lt $decision.Index -and $decision.Index -lt $done.Index) 'CAUSAL_QUEUE_DECISION_MOVED_ORDER'
 Require ($start.Groups[1].Value -eq $decision.Groups[1].Value -and $start.Groups[1].Value -eq $done.Groups[1].Value) 'SAME_OPERATION'
 Require ($start.Groups[2].Value -eq $done.Groups[2].Value) 'SAME_VEHICLE'
 Require ([int]$decision.Groups[2].Value -eq [int]$start.Groups[4].Value+1 -and [int]$done.Groups[5].Value -eq [int]$decision.Groups[2].Value) 'ONE_COUNTED_RECOVERY'
 Require ($start.Groups[5].Value -eq $decision.Groups[3].Value -and $start.Groups[5].Value -eq $done.Groups[6].Value) 'UNCHANGED_ABSOLUTE_DEADLINE'
 Require ([double]::Parse($done.Groups[3].Value,[Globalization.CultureInfo]::InvariantCulture) -ge 25 -and [int]$done.Groups[4].Value -ge 3) 'PHYSICAL_25M_THREE_VELOCITY_POLLS'
 $operation=[regex]::Escape($start.Groups[1].Value)
 Require ($full -match ('\[TRANSPORT_NATIVE_ABANDON_RECOVERY\][^\r\n]*operation_id='+$operation+'\s[^\r\n]*healthy=1\|full_roster=1\|settled=1\|local_combat=0[^\r\n]*native_get_out_cancelled=1 recovery_mode=ROUTE_REBUILD_ONLY health_unchanged=1 deadline_renewed=0')) 'CURRENT_HEALTH_SCOPE_AND_ROUTE_ONLY_OWNER'
}
foreach($rule in @('HNA_PRODUCTION_RECOVERY','HNA_SAME_VEHICLE_PHYSICAL_MOVEMENT')){
 Require ($full -match ('\[VEHICLE_SCENARIO_CHECK\][^\r\n]*rule='+$rule+' passed=1')) $rule
}
$siblingLogs=@(Get-ChildItem -LiteralPath (Split-Path -Parent $LogPath) -Filter *.log -File)
$fatal=0;$resource=0
foreach($file in $siblingLogs){
 $text=[IO.File]::ReadAllText($file.FullName)
 $fatal+=([regex]::Matches($text,'(?m)^.*(?:SCRIPT\s+\([EF]\)|ENGINE\s+\(F\)|Out of memory|Unhandled exception|NULL pointer|Virtual Machine Exception).*$')).Count
 $resource+=([regex]::Matches($text,'(?m)^.*\(E\).*$')).Count
}
$functional='PASS';if($failures.Count){$functional='FAIL'}
$fatalVerdict='PASS';if($fatal){$fatalVerdict='FAIL'}
$strict='PASS';if($resource){$strict='FAIL'}
"FUNCTIONAL=$functional failures=$($failures -join ',')"
"FULL_FATAL=$fatalVerdict count=$fatal"
"STRICT_FULL_ERRORS=$strict raw_sibling_lines=$resource"
if($failures.Count -or $fatal -or $resource){exit 1}
exit 0
