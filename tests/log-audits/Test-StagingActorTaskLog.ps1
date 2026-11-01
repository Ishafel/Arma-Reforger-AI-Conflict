param([Parameter(Mandatory)][string]$LogPath,[Parameter(Mandatory)][string]$LauncherPath,[Parameter(Mandatory)][string]$NativeExitPath,[string]$EvidenceRoot)
$ErrorActionPreference='Stop'
$full=Get-Content -LiteralPath $LogPath -Raw
$launcher=Get-Content -LiteralPath $LauncherPath -Raw
$failures=[Collections.Generic.List[string]]::new()
function Require($condition,[string]$rule){if(!$condition){$failures.Add($rule)}}
Require ((Get-Content -LiteralPath $NativeExitPath -Raw).Trim() -eq '0') 'NATIVE_EXIT'
Require ($full.Contains('CLI Params:') -and $full.Contains('[ROSTER_READY]') -and $full.Contains('Game destroyed.')) 'FULL_STOPPED_LOG'
Require ($full -notmatch 'SCRIPT\s+\([EF]\)|Virtual Machine Exception|ENGINE\s+\(F\)|NULL pointer') 'FULL_FATAL'
Require ($launcher.Contains('AICF_RUNTIME_MANIFEST_JSON=') -and $launcher.Contains('-aicfStagingTaskProbe')) 'CANONICAL_MANIFEST'
$rows=@([regex]::Matches($full,'\[STAGING_TASK_CASE\] rule=(?<rule>\w+) passed=(?<passed>[01]) details=\[(?<details>[^\r\n]*)\]'))
$expected=@('SHORT_SETUP_ONLY','ACTUAL_RUNNING_ACTOR_WITHOUT_GROUP_WAYPOINT','EXPIRED_FALSE','STALE_GRAPH_FALSE','STALE_REQUEST_FALSE','FOREIGN_GOAL_FALSE','STALE_ASSIGNMENT_FALSE','NO_ACTIVITY_FALSE','UNOWNED_NATIVE_ACTIVITY_FALSE','SUSPENDED_NATIVE_ACTIVITY_FALSE','CANCELLED_NATIVE_ACTIVITY_FALSE','RESTORED_ORIGINAL_RUNNING_PROOF','FAILED_ACTIVITY_FALSE','ACTUAL_STAGING_COMPLETION','DEADLINES_REVISIONS_HEALTH_UNCHANGED')
Require (($rows|ForEach-Object{$_.Groups['rule'].Value}) -join ',' -ceq ($expected -join ',')) 'EXACT_ORDERED_RULES'
Require (@($rows|Where-Object{$_.Groups['passed'].Value -ne '1'}).Count -eq 0) 'NATIVE_CHECK_FAILURE'
$setup=[regex]::Match($full,'rule=SHORT_SETUP_ONLY passed=1 details=\[shift_m=(?<shift>[0-9.]+)\]')
Require ($setup.Success -and [double]$setup.Groups['shift'].Value -le 8) 'SHORT_SETUP_BOUND'
$active=[regex]::Match($full,'rule=ACTUAL_RUNNING_ACTOR_WITHOUT_GROUP_WAYPOINT passed=1 details=\[removed=(?<removed>[1-9][0-9]*) waypoint_current=0 waypoint_queued=0 actor_tokens=(?<tokens>[1-9][0-9]*)\]')
Require $active.Success 'ACTUAL_NATIVE_TASK_WITH_GROUP_WAYPOINT_WITHDRAWN'
$completion=[regex]::Match($full,'rule=ACTUAL_STAGING_COMPLETION passed=1 details=\[elapsed_ms=(?<elapsed>[0-9]+) moved_m=(?<moved>[0-9.]+) phase=(?<phase>[0-9]+)\]')
Require ($completion.Success -and [int]$completion.Groups['elapsed'].Value -lt 30000 -and [double]$completion.Groups['moved'].Value -ge 2) 'ACTUAL_COMPLETION_BEFORE_UNCHANGED_DEADLINE'
Require (@([regex]::Matches($full,'\[STAGING_TASK_FINISHED\] checks=15 failures=0 classification=CONTROLLED_ACTOR_TASK_ONLY')).Count -eq 1) 'EXACT_FINISH'
$resources=@([regex]::Matches($full,'(?m)^.*(?:RESOURCES|WORLD|ENTITY|ANIMATION|DEFAULT)\s+\(E\).*$')).Count
$targeted='PASS';if($failures.Count){$targeted='FAIL'}
$resourceVerdict='PASS';if($resources){$resourceVerdict='FAIL'}
$overall='PASS';if($targeted -ne 'PASS' -or $resourceVerdict -ne 'PASS'){$overall='FAIL'}
$result=[pscustomobject]@{classification='CONTROLLED_ACTOR_TASK_ONLY';targeted=$targeted;failures=@($failures);checks=$rows.Count;strict_native_resources=$resourceVerdict;native_resource_errors=$resources;overall=$overall;full_log=(Resolve-Path $LogPath).Path}
if($EvidenceRoot){[void][IO.Directory]::CreateDirectory($EvidenceRoot);$result|ConvertTo-Json -Depth 4|Set-Content (Join-Path $EvidenceRoot 'summary.json')}
$result|ConvertTo-Json -Depth 4
if($failures.Count){exit 1};exit 0
