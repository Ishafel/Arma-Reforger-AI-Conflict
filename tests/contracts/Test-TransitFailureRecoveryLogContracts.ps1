param([string]$RepositoryRoot=(Split-Path -Parent (Split-Path -Parent $PSScriptRoot)))
$ErrorActionPreference='Stop'
$taskWork=Join-Path $RepositoryRoot ('.codex-runtime/transit-failure-log-contract-'+[guid]::NewGuid().ToString('N'))
[void][IO.Directory]::CreateDirectory($taskWork)
$taskLauncher=Join-Path $taskWork 'launcher.txt';$taskExit=Join-Path $taskWork 'native-exit.txt';$taskLog=Join-Path $taskWork 'console.log'
'AICF_RUNTIME_MANIFEST_JSON={"arguments":["-aicfVehicleScenarioRetainedBehavior","1"]}'|Set-Content -LiteralPath $taskLauncher
'0'|Set-Content -LiteralPath $taskExit
$prefix='[AICF][TRANSIT_FAILURE_RECOVERY_PROBE] classification=CONTROLLED_FAILURE_RECOVERY_ONLY natural_failure_proof=0 operation_id=trip-USSR-0-1-1 vehicle=0x4000000000001234 {} '
$lines=@('CLI Params: -server','[ROSTER_READY]',($prefix+'event=START t_ms=1000 route_generation=1 deadline=100000'))
foreach($step in 1,2){
 $at=1000+$step*10000;$repairAt=$at+1000;$movedAt=$at+5000;$result='STUCK';if($step -eq 2){$result='UNKNOWN'}
 $lines+=($prefix+"event=INJECTED t_ms=$at step=$step native_result=$result handler=1 accepted=1 repeated=1 callbacks=$step route_generation=$step")
 $lines+=('[TRANSPORT_NATIVE_MOVE_FAILURE_CAPTURED] operation_id=trip-USSR-0-1-1 vehicle=0x4000000000001234 {} native_result=NATIVE_'+$result+' native_callback_delivered=1')
 $lines+=('[TRANSPORT_NATIVE_MOVE_FAILURE_RECOVERY] operation_id=trip-USSR-0-1-1 vehicle=0x4000000000001234 {} native_result=NATIVE_'+$result+' native_callback_delivered=1')
 $lines+=($prefix+"event=REPAIR t_ms=$repairAt step=$step route_generation=$($step+1) attempts=$step maximum=2 delay_ms=1000 deadline=100000")
 $lines+=($prefix+"event=MOVED t_ms=$movedAt step=$step displacement_m=30 moving_polls=4 attempts=$step deadline=100000")
 foreach($rule in @('TFR_UNRELATED_REJECTED','TFR_DEFAULT_HANDLER_REJECTED','TFR_MINUS_ONE_HANDLER_REJECTED','TFR_MINUS_TWO_HANDLER_REJECTED','TFR_FOREIGN_HANDLER_REJECTED','TFR_STOPPED_PRESERVES_STOCK','TFR_EXACT_NATIVE_FAILURE_CAPTURED','TFR_NEXT_TICK_BOUNDED_RECOVERY','TFR_REAL_MOVEMENT_AFTER_REPAIR')){
  $lines+="[AICF][VEHICLE_SCENARIO_CHECK] rule=$rule passed=1"
 }
}
$lines+='[AICF][VEHICLE_SCENARIO_CHECK] rule=TFR_INITIAL_BUDGET_TWO passed=1'
$lines+='[AICF][VEHICLE_SCENARIO_CHECK] rule=TFR_NEW_ACTION_SAME_LOCATION_NOT_DUPLICATE passed=1'
$lines+='[AICF][VEHICLE_SCENARIO_CHECK] rule=TFR_EXHAUSTED_PRESERVES_STOCK passed=1'
$lines+=($prefix+'event=DONE t_ms=30000 attempts=2 maximum=2 callbacks=2 original_roster=1 deadline=100000')
$lines+='Game destroyed.'
$good=$lines -join "`n"
$taskControls=0
function Run([string]$name,[string]$text,[int]$expected,[string]$reason=''){
 $text|Set-Content -LiteralPath $taskLog
 $taskOutput=& powershell.exe -NoProfile -ExecutionPolicy Bypass -File (Join-Path $RepositoryRoot 'tests/log-audits/Test-TransitFailureRecoveryLog.ps1') -LogPath $taskLog -LauncherPath $taskLauncher -NativeExitPath $taskExit 2>&1
 $taskActual=$LASTEXITCODE
 if($taskActual -ne $expected){throw "Control $name exit=$taskActual expected=$expected output=$taskOutput"}
 $taskParsed=($taskOutput -join "`n")|ConvertFrom-Json
 if($reason -and $reason -notin $taskParsed.failures){throw "Control $name missing reason $reason"}
 $script:taskControls++
 return $taskParsed
}
try{
 $null=Run good $good 0
 $null=Run decimal-identity ($good.Replace('0x4000000000001234 {}','4660')) 0
 $null=Run missing-done ($good -replace '(?m)^.*event=DONE[^\r\n]*','') 1 'ORDERED_TWO_RECOVERIES'
 $null=Run unstopped ($good.Replace('Game destroyed.','')) 1 'FULL_STOPPED_LOG'
 $null=Run fatal-before ($good.Replace('event=START','SCRIPT (E): failure' + "`n"+'event=START')) 1 'FULL_FATAL'
 $null=Run fatal-after ($good+"`nSCRIPT (E): teardown") 1 'FULL_FATAL'
 $null=Run wrong-identity ($good.Replace('event=REPAIR','vehicle=0x4000000000009999 event=REPAIR')) 1 'SAME_TRIP_AND_VEHICLE'
 $null=Run no-callback ($good.Replace('callbacks=1','callbacks=0')) 1 'EXACT_ONCE_NATIVE_CALLBACK'
 $null=Run duplicate-callback ($good.Replace('callbacks=2','callbacks=3')) 1 'EXACT_ONCE_NATIVE_CALLBACK'
 $null=Run immediate-same-tick ($good.Replace('event=REPAIR t_ms=12000','event=REPAIR t_ms=11000')) 1 'ACTUAL_NEXT_TICK_DELAY'
 $null=Run deadline-extension ($good.Replace('event=REPAIR t_ms=12000 step=1 route_generation=2 attempts=1 maximum=2 delay_ms=1000 deadline=100000','event=REPAIR t_ms=12000 step=1 route_generation=2 attempts=1 maximum=2 delay_ms=1000 deadline=100001')) 1 'UNCHANGED_ABSOLUTE_DEADLINE'
 $null=Run no-movement ($good.Replace('displacement_m=30','displacement_m=0')) 1 'PHYSICAL_MOVEMENT_AFTER_REPAIR'
 $null=Run budget-raised ($good.Replace('maximum=2','maximum=3')) 1 'COUNTED_BOUNDED_REPAIR'
 $null=Run wrong-production ($good.Replace('[TRANSPORT_NATIVE_MOVE_FAILURE_CAPTURED] operation_id=trip-USSR-0-1-1','[TRANSPORT_NATIVE_MOVE_FAILURE_CAPTURED] operation_id=trip-USSR-1-1-1')) 1 'PRODUCTION_IDENTITY_BOUND_RECEIPTS'
 $null=Run missing-negative ($good.Replace('rule=TFR_FOREIGN_HANDLER_REJECTED passed=1','rule=OTHER passed=1')) 1 'NATIVE_CHECK_TFR_FOREIGN_HANDLER_REJECTED'
 $null=Run new-action-hidden ($good.Replace('rule=TFR_NEW_ACTION_SAME_LOCATION_NOT_DUPLICATE passed=1','rule=OTHER passed=1')) 1 'NEW_ACTION_NOT_DEDUPLICATED'
 $null=Run weak-classification ($good.Replace('CONTROLLED_FAILURE_RECOVERY_ONLY','NORMAL_CAMPAIGN')) 1 'EXPLICIT_CONTROLLED_CLASSIFICATION'
 $resource=Run strict-resource ($good+"`nRESOURCES (E): native resource error") 0
 if($resource.strict_native_resources -ne 'FAIL' -or $resource.overall -ne 'FAIL'){throw 'Resource error was hidden'}
 '1'|Set-Content -LiteralPath $taskExit
 $null=Run native-exit $good 1 'NATIVE_EXIT'
 Write-Output "PASS TransitFailureRecoveryLogContracts controls=$taskControls"
}finally{
 $taskResolved=(Resolve-Path -LiteralPath $taskWork).Path
 $taskEvidenceRoot=(Resolve-Path -LiteralPath (Join-Path $RepositoryRoot '.codex-runtime')).Path
 if(!$taskResolved.StartsWith($taskEvidenceRoot+[IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase)){throw 'Unsafe contract cleanup target'}
 Remove-Item -LiteralPath $taskResolved -Recurse -Force
}
