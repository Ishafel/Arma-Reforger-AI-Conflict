param(
 [string]$RepositoryRoot=(Split-Path -Parent (Split-Path -Parent $PSScriptRoot)),
 [string]$AuditorPath
)
$ErrorActionPreference='Stop'
$audit=Join-Path $RepositoryRoot 'tests/log-audits/Test-SpawnApproachFailureLog.ps1'
if($AuditorPath){$audit=(Resolve-Path -LiteralPath $AuditorPath).Path}
$work=Join-Path $RepositoryRoot ('.codex-runtime/contract-probes/spawn-approach-log-'+[guid]::NewGuid().ToString('N'))
[void][IO.Directory]::CreateDirectory($work)
$argv=@('-gproj','test.gproj','-profile','profile','-addonsDir','addons','-addons','9178E5822AFE48EA','-addr','127.0.0.1:2588','-aicfVehicleScenario','uaz5','-aicfVehicleScenarioCombat','0','-aicfSpawnApproachFailureProbe','1')
$cli=$argv -join ' '
$operation='trip-USSR-0-1-1'
$log=@"
CLI Params: $cli
[AICF][STAGE1][INFO][ROSTER_READY] ready=1
[AICF][VEHICLE_SCENARIO_ENV] scenario=uaz5 combat=0 combat_mode=HOLD_FIRE damage_protection=1 truce=1
[AICF][STAGE3.5][WARN][VEHICLE_SPAWN_APPROACH_MOVE_FAILED] operation_id=$operation reason=SPAWN_SITE_APPROACH_MOVE_FAILED_UNKNOWN waypoint=123 location=<10,0,0> pending_tick=1
[AICF][SPAWN_APPROACH_FAILURE_PROBE] event=INJECTED t_ms=100 classification=CONTROLLED_FAULT_INJECTION normal_arrival_proof=0 operation_id=$operation group=0x4000000000001234{} waypoint_id=123 handler=-1 accepted=1 callbacks=1 phase=APPROACHING_SITE plan_pending=1 no_lease=1
[AICF][STAGE3][INFO][VEHICLE_STATE_CHANGED] operation_id=$operation reason=SPAWN_SITE_APPROACH_MOVE_FAILED_UNKNOWN previous_state=APPROACHING_SITE state=FALLBACK
[AICF][VEHICLE_SCENARIO_CHECK] scenario=uaz5 leg=1 rule=EXACT_APPROACH_REPORT_ACCEPTED passed=1
[AICF][VEHICLE_SCENARIO_CHECK] scenario=uaz5 leg=1 rule=APPROACH_NEXT_TICK_FALLBACK passed=1
[AICF][VEHICLE_SCENARIO_CHECK] scenario=uaz5 leg=1 rule=APPROACH_PLAN_RELEASED passed=1
[AICF][VEHICLE_SCENARIO_CHECK] scenario=uaz5 leg=1 rule=APPROACH_ORDER_RESTORED passed=1
[AICF][VEHICLE_SCENARIO_CHECK] scenario=uaz5 leg=1 rule=APPROACH_CALLBACK_ONCE passed=1
[AICF][SPAWN_APPROACH_FAILURE_PROBE] event=DONE t_ms=200 classification=CONTROLLED_FAULT_INJECTION normal_arrival_proof=0 operation_id=$operation elapsed_ms=100 phase=FALLBACK reason=SPAWN_SITE_APPROACH_MOVE_FAILED_UNKNOWN plan_released=1 order_restored=1 callbacks=1 failures=0 original_roster=1
[AICF][VEHICLE_SCENARIO_DONE] scenario=uaz5 checks=5 failures=0 legs=1
Game destroyed.
"@
$controls=0
function Run([string]$name,[string]$text,[int]$expected,[string]$sibling='', [string]$nativeExit='0', [string]$manifest=''){
 $dir=Join-Path $work $name;[void][IO.Directory]::CreateDirectory($dir)
 [IO.File]::WriteAllText((Join-Path $dir 'console.log'),$text)
 [IO.File]::WriteAllText((Join-Path $dir 'error.log'),$sibling)
 [IO.File]::WriteAllText((Join-Path $dir 'native-exit.txt'),$nativeExit)
 if(!$manifest){$manifest='AICF_RUNTIME_MANIFEST_JSON='+(@{arguments=$argv}|ConvertTo-Json -Compress)}
 [IO.File]::WriteAllText((Join-Path $dir 'launcher.txt'),$manifest)
 & powershell.exe -NoProfile -ExecutionPolicy Bypass -File $audit -LogPath (Join-Path $dir 'console.log') -LauncherPath (Join-Path $dir 'launcher.txt') -NativeExitPath (Join-Path $dir 'native-exit.txt') *> (Join-Path $dir 'audit.txt')
 if($LASTEXITCODE -ne $expected){throw "Wrong verdict case=$name expected=$expected actual=$LASTEXITCODE evidence=$dir"}
 $script:controls++
}
Run 'positive' $log 0
$nativeHexLog=$log.Replace('waypoint=123','waypoint=0x4000000000001682 {}').Replace('waypoint_id=123','waypoint_id=0x4000000000001682 {}')
Run 'native_fixed_hex_positive' $nativeHexLog 0
Run 'native_compact_braces_positive' ($nativeHexLog.Replace('0x4000000000001682 {}','0x4000000000001682{}')) 0
foreach($case in @(
 @('zero_decimal','123','0'),@('negative_decimal','123','-1'),@('decimal_garbage','123','123tail'),
 @('decimal_overflow','123','18446744073709551616'),@('decimal_leading_zero','123','0123')
)){
 $mutated=$log.Replace('waypoint=123','waypoint='+$case[2]).Replace('waypoint_id=123','waypoint_id='+$case[2])
 Run $case[0] $mutated 1
}
foreach($case in @(
 @('hex_zero','0x4000000000001682','0x0000000000000000'),
 @('hex_short','0x4000000000001682','0x400000000001682'),
 @('hex_long','0x4000000000001682','0x40000000000001682'),
 @('hex_garbage','0x4000000000001682','0x400000000000168G'),
 @('hex_trailing_garbage','0x4000000000001682','0x4000000000001682junk'),
 @('brace_truncated','{}','{'),@('brace_missing_open','{}','}'),
 @('brace_nested','{}','{{}}'),@('brace_trailing_garbage','{}','{}junk'),
 @('group_zero','0x4000000000001234','0x0000000000000000'),
 @('group_short','0x4000000000001234','0x1234'),
 @('group_garbage','0x4000000000001234','0x4000000000001234junk')
)){
 Run $case[0] ($nativeHexLog.Replace($case[1],$case[2])) 1
}
Run 'wrong_native_hex_identity' ($nativeHexLog.Replace('waypoint=0x4000000000001682','waypoint=0x4000000000001683')) 1
Run 'missing_report_identity' ($nativeHexLog.Replace(' waypoint=',' missing_waypoint=')) 1
Run 'missing_injected_identity' ($nativeHexLog.Replace(' waypoint_id=',' missing_waypoint_id=')) 1
foreach($case in @(
 @('no_manifest','AICF_RUNTIME_MANIFEST_JSON=','REMOVED='),
 @('wrong_cli','-aicfSpawnApproachFailureProbe 1','-aicfSpawnApproachFailureProbe 0'),
 @('not_stopped','Game destroyed.','still running'),
 @('missing_receipt','[VEHICLE_SPAWN_APPROACH_MOVE_FAILED]','[OTHER_EVENT]'),
 @('wrong_waypoint','waypoint=123','waypoint=124'),
 @('wrong_phase','phase=APPROACHING_SITE','phase=TRANSIT'),
 @('not_accepted','accepted=1','accepted=0'),
 @('no_pending','plan_pending=1','plan_pending=0'),
 @('multiple_callbacks','callbacks=1','callbacks=2'),
 @('lease_exists','no_lease=1','no_lease=0'),
 @('same_tick','event=DONE t_ms=200','event=DONE t_ms=100'),
 @('wrong_elapsed','elapsed_ms=100','elapsed_ms=99'),
 @('wrong_reason','reason=SPAWN_SITE_APPROACH_MOVE_FAILED_UNKNOWN','reason=UNKNOWN'),
 @('plan_retained','plan_released=1','plan_released=0'),
 @('not_restored','order_restored=1','order_restored=0'),
 @('roster_replaced','original_roster=1','original_roster=0'),
 @('failed_check','rule=APPROACH_PLAN_RELEASED passed=1','rule=APPROACH_PLAN_RELEASED passed=0'),
 @('missing_check','rule=APPROACH_CALLBACK_ONCE','rule=OTHER_CHECK'),
 @('failed_done','failures=0 legs=1','failures=1 legs=1'),
 @('wrong_classification','classification=CONTROLLED_FAULT_INJECTION','classification=NORMAL'),
 @('claims_arrival','normal_arrival_proof=0','normal_arrival_proof=1'),
 @('no_controller_fallback','state=FALLBACK','state=BOARDING'),
 @('wrong_controller_phase','previous_state=APPROACHING_SITE','previous_state=TRANSIT')
)){
 if($case[0] -eq 'no_manifest'){
  Run $case[0] $log 1 '' '0' ('REMOVED='+(@{arguments=$argv}|ConvertTo-Json -Compress));continue
 }
 if(!$log.Contains($case[1])){throw "Mutation target absent $($case[0])"}
 Run $case[0] ($log.Replace($case[1],$case[2])) 1
}
Run 'native_exit' $log 1 '' '-1'
Run 'sibling_vm' $log 1 'SCRIPT (E): Virtual Machine Exception'
Run 'sibling_fatal' $log 1 'ENGINE (F): fatal'
Run 'duplicate_event' ($log+"`n"+([regex]::Match($log,'\[SPAWN_APPROACH_FAILURE_PROBE\] event=INJECTED[^\r\n]*').Value)) 1
Run 'purchase_before_failure' ($log+"`n[VEHICLE_PRESET_PURCHASE] operation=$operation prefab=test.et supplies=100") 1
Run 'spawn_before_failure' ($log+"`n[VEHICLE_SPAWNED] operation_id=$operation vehicle=test") 1
# Resource failures remain an independent blocker, never erased by targeted success.
Run 'resource_independent_blocker' $log 0 'RESOURCES (E): missing resource'
$resource=[IO.File]::ReadAllText((Join-Path $work 'resource_independent_blocker/audit.txt'))
if($resource -notmatch 'gate=strict_native_resources verdict=FAIL' -or $resource -notmatch 'strict_native_resources=FAIL'){throw 'Resource blocker disappeared'}
$controls++
"Spawn approach failure log contracts: PASS; controls=$controls; evidence=$work"
