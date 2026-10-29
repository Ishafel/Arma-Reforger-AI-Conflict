param([string]$RepositoryRoot=(Split-Path -Parent (Split-Path -Parent $PSScriptRoot)))
$ErrorActionPreference='Stop'
$auditor=Join-Path $RepositoryRoot 'tests/log-audits/Test-StagingActorTaskLog.ps1'
$temporary=Join-Path ([IO.Path]::GetTempPath()) ('aicf-staging-task-'+[guid]::NewGuid().ToString('N'))
[void][IO.Directory]::CreateDirectory($temporary)
$rules=@('SHORT_SETUP_ONLY','ACTUAL_RUNNING_ACTOR_WITHOUT_GROUP_WAYPOINT','EXPIRED_FALSE','STALE_GRAPH_FALSE','STALE_REQUEST_FALSE','FOREIGN_GOAL_FALSE','STALE_ASSIGNMENT_FALSE','NO_ACTIVITY_FALSE','UNOWNED_NATIVE_ACTIVITY_FALSE','SUSPENDED_NATIVE_ACTIVITY_FALSE','CANCELLED_NATIVE_ACTIVITY_FALSE','RESTORED_ORIGINAL_RUNNING_PROOF','FAILED_ACTIVITY_FALSE','ACTUAL_STAGING_COMPLETION','DEADLINES_REVISIONS_HEALTH_UNCHANGED')
$rows=@('CLI Params: -aicfStagingTaskProbe 1','[ROSTER_READY]')
foreach($rule in $rules){$details='';switch($rule){'SHORT_SETUP_ONLY'{$details='shift_m=7.5'};'ACTUAL_RUNNING_ACTOR_WITHOUT_GROUP_WAYPOINT'{$details='removed=1 waypoint_current=0 waypoint_queued=0 actor_tokens=1'};'ACTUAL_STAGING_COMPLETION'{$details='elapsed_ms=5033 moved_m=11.5 phase=3'}};$rows+="[STAGING_TASK_CASE] rule=$rule passed=1 details=[$details]"}
$rows+='[STAGING_TASK_FINISHED] checks=15 failures=0 classification=CONTROLLED_ACTOR_TASK_ONLY';$rows+='Game destroyed.'
$valid=$rows -join "`n";$checks=0
function Run([string]$full,[bool]$expected,[string]$exit='0',[string]$manifest='AICF_RUNTIME_MANIFEST_JSON=-aicfStagingTaskProbe 1'){
 $log=Join-Path $temporary 'console.log';$launcher=Join-Path $temporary 'launcher.txt';$native=Join-Path $temporary 'native-exit.txt'
 Set-Content $log $full;Set-Content $launcher $manifest;Set-Content $native $exit
 $result=@(& (Get-Process -Id $PID).Path -NoProfile -File $auditor -LogPath $log -LauncherPath $launcher -NativeExitPath $native)
 if(($LASTEXITCODE -eq 0) -ne $expected){throw "Staging log oracle mismatch: $result"};$script:checks++
}
try{
 Run $valid $true
 Run ($valid.Replace('Game destroyed.','')) $false
 Run ($valid+"`nSCRIPT (E): test actual error") $false
 Run $valid $false '-1'
 Run $valid $false '0' 'missing canonical manifest'
 Run ($valid.Replace('shift_m=7.5','shift_m=8.1')) $false
 Run ($valid.Replace('waypoint_queued=0','waypoint_queued=1')) $false
 Run ($valid.Replace('elapsed_ms=5033','elapsed_ms=30000')) $false
 Run ($valid.Replace('moved_m=11.5','moved_m=1.9')) $false
 Run ($valid.Replace('rule=STALE_REQUEST_FALSE passed=1','rule=STALE_REQUEST_FALSE passed=0')) $false
 Run ($valid.Replace('rule=NO_ACTIVITY_FALSE','rule=MISSING_CASE')) $false
 Run ($valid.Replace('checks=15 failures=0','checks=14 failures=0')) $false
 Run ($valid+"`n[STAGING_TASK_CASE] rule=NO_ACTIVITY_FALSE passed=1 details=[]") $false
 "PASS Staging actor task log controls=$checks"
}finally{
 $resolved=(Resolve-Path -LiteralPath $temporary).Path
 if(!(Split-Path -Leaf $resolved).StartsWith('aicf-staging-task-') -or !(Split-Path -Parent $resolved).TrimEnd('\').Equals(([IO.Path]::GetTempPath()).TrimEnd('\'),[StringComparison]::OrdinalIgnoreCase)){throw 'Unsafe temporary cleanup target'}
 Remove-Item -LiteralPath $resolved -Recurse -Force
}
