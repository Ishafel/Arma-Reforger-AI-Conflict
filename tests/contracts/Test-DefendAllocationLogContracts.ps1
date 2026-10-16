$ErrorActionPreference='Stop'
$repo=Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$root=Join-Path $repo '.codex-runtime/defend-allocation-log-contracts'
[void][IO.Directory]::CreateDirectory($root)
$audit=Join-Path $repo 'tests/log-audits/Test-DefendAllocationLog.ps1'
$launcher='AICF_RUNTIME_MANIFEST_JSON={"role":"Server","arguments":["-aicfDefendAllocationProbe","1"]}'
$log="ENGINE : CLI Params: -aicfDefendAllocationProbe 1`n[AICF][ROSTER_READY]"
foreach ($side in @('US','USSR')) {
    foreach ($name in @('SETUP','HOLD_CREATED','NATIVE_ALLOCATION_SUCCESS','NATIVE_ACTIVITY','MOVE_RESTORED','OLD_WAYPOINT_REMOVED','NATIVE_INPUT_EMPTY','STALE_64_FAILS','MOVE_UNCHANGED','GROUP_UNCHANGED','NEW_ORDER_CURRENT')) {$log+="`n[AICF][DEFEND_ALLOCATION_CHECK] side=$side name=$name passed=1"}
    $log+="`n[AICF][DEFEND_ALLOCATION_ATTEMPT] side=$side calls=64 input=REMOVED_BY_PLANNER`n[AICF][AI_DEFEND_ALLOCATION_CANCELLED] group=$side reason=INVALID_DEFEND_INPUT"
}
$log+="`n[AICF][DEFEND_ALLOCATION_FINISHED] checks=22 failures=0 attempted=2 returned=2`nENGINE : Game destroyed."
$baseline=$log+"`nSCRIPT (E): Virtual Machine Exception`nReason: Defend Waypoint not provided to the node!`nSCRIPT (E): Virtual Machine Exception`nReason: Defend Waypoint not provided to the node!"
function Check([string]$Name,[string]$Text,[string]$Rule,[string]$Launch=$launcher,[switch]$Reproduction) {
    $path=Join-Path $root "$Name.fixture"
    [IO.File]::WriteAllText($path,$Text)
    [IO.File]::WriteAllText((Join-Path $root 'launcher.fixture'),$Launch)
    [IO.File]::WriteAllText((Join-Path $root 'exit.fixture'),'0')
    $args=@('-NoProfile','-ExecutionPolicy','Bypass','-File',$audit,'-LogPath',$path,'-LauncherPath',(Join-Path $root 'launcher.fixture'),'-NativeExitPath',(Join-Path $root 'exit.fixture'))
    if($Reproduction){$args+='-ExpectReproduction'}
    $output=& powershell.exe @args 2>&1
    $expected=0;if($Rule){$expected=1}
    if($LASTEXITCODE -ne $expected -or ($Rule -and ($output -join "`n") -notmatch $Rule)){throw "$Name unexpected audit: $output"}
    Write-Output "PASS allocation log case=$Name exit=$LASTEXITCODE"
}
Check 'positive' $log ''
Check 'baseline' $baseline '' -Reproduction
Check 'native-defect-rejected' $baseline 'SCRIPT_FATAL'
Check 'reproduction-required' $log 'BASELINE_REPRODUCTION' -Reproduction
Check 'engine-fatal' ($log+"`nENGINE (F): Crashed") 'ENGINE_FATAL'
Check 'active-log' ($log.Replace('Game destroyed.','active')) 'FULL_STOPPED_LOG_REQUIRED'
Check 'missing-manifest' $log 'MANIFEST' 'missing'
Check 'wrong-probe-mode' $log 'PROBE_LAUNCH' ($launcher.Replace('"1"','"0"'))
Check 'wrong-cli' ($log.Replace('CLI Params: -aicfDefendAllocationProbe 1','CLI Params: -wrong 1')) 'EXACT_CLI'
Check 'native-positive-required' ($log.Replace('name=NATIVE_ALLOCATION_SUCCESS passed=1','name=NATIVE_ALLOCATION_SUCCESS passed=0')) 'CHECK_US_NATIVE_ALLOCATION_SUCCESS'
Check 'stale-check-required' ($log.Replace('name=STALE_64_FAILS passed=1','name=STALE_64_FAILS passed=0')) 'CHECK_US_STALE_64_FAILS'
Check 'duplicate-side' ($log.Replace('side=USSR','side=US')) 'CHECK_US_SETUP'
Check 'mutated-move' ($log.Replace('name=MOVE_UNCHANGED passed=1','name=MOVE_UNCHANGED passed=0')) 'CHECK_US_MOVE_UNCHANGED'
Check 'warning-bounded' ($log.Replace('[AI_DEFEND_ALLOCATION_CANCELLED]','[OTHER]')) 'BOUNDED_CANCELLATION'
Write-Output 'PASS Defend allocation log contracts: 2 positive / 12 negative cases; native runtime is a separate gate'
