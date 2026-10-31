param([string]$RepositoryRoot=(Split-Path -Parent (Split-Path -Parent $PSScriptRoot)))
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot '../lib/Stage3StaticAudit.Common.ps1')
$root=Join-Path $RepositoryRoot 'AIConflictCore/Scripts/Game/AIConflict'
$source=[IO.File]::ReadAllText((Join-Path $root 'Vehicles/AICF_VehicleAcquisitionFlow.c'))
$plan=[IO.File]::ReadAllText((Join-Path $root 'Vehicles/AICF_VehicleSpawner.c'))
function Body([string]$text,[string]$name){
 Get-AICFMethodBody ([pscustomobject]@{Source=$text;Code=(ConvertTo-AICFCodeText $text)}) $name
}
function Rules([string]$text){
 $budget=Body $text 'CalculateSpawnApproachBudgetMs'
 $measure=Body $text 'MeasureSpawnApproachDistance'
 $planning=Body $text 'ProcessWaitingForSite'
 $approach=Body $text 'ProcessApproachingSite'
 $planned=Body $text 'ProcessSitePlanned'
 if(!$budget -or !$measure -or !$planning -or !$approach -or !$planned){'MISSING_METHOD';return}
 if($text -notmatch 'SPAWN_APPROACH_WALK_METERS_PER_SECOND = 2.0' -or $budget -notmatch 'Math.Ceil\(Math.Max\(0.0, farthestDistanceMeters\) /\s*SPAWN_APPROACH_WALK_METERS_PER_SECOND \* 1000.0\)'){'WALK_ALLOWANCE'}
 if($text -notmatch 'MAX_DISTANCE_APPROACH_BUDGET_MS = 600000' -or $text -notmatch 'SPAWN_APPROACH_STAGING_RESERVE_MS = 60000' -or $budget -notmatch 'Math.Min\(MAX_DISTANCE_APPROACH_BUDGET_MS,\s*SPAWN_APPROACH_STAGING_RESERVE_MS \+ travelMs\)'){'DISTANCE_CAP'}
 if($budget -notmatch 'Math.Max\(configuredTimeoutMs, distanceBudgetMs\)'){'CONFIGURED_MINIMUM'}
 if($measure -notmatch 'group.GetAgents\(agents\)' -or $measure -notmatch 'foreach \(AIAgent agent : agents\)' -or $measure -notmatch 'agent.GetControlledEntity\(\)' -or $measure -notmatch 'Math.Max\(farthestDistanceMeters,\s*vector.DistanceXZ\(member.GetOrigin\(\), stagingPosition\)\)' -or $measure -match 'group.GetOrigin\(|ResolveAliveLeader|GetLeaderEntity'){'ALL_LIVING_POSITIONS'}
 if($measure -notmatch '!AICF_GroupRuntime.IsAliveCharacter\(member\)' -or $measure -notmatch 'if \(agent.GetParentGroup\(\) != group\)\s*return false' -or $measure -notmatch 'return aliveCount > 0'){'ROSTER_IDENTITY'}
 if($planning.IndexOf('ResolveStagingPosition') -gt $planning.IndexOf('MeasureSpawnApproachDistance') -or $planning.IndexOf('MeasureSpawnApproachDistance') -gt $planning.IndexOf('TryReserveSelectedSite') -or $planning -notmatch 'approachAliveCount < MinimumAgents\(trip\)'){'PREFLIGHT_BEFORE_RESERVATION'}
 if($planning -notmatch 'Math.Min\(trip.GetAbsoluteDeadlineMs\(\), nowMs \+ approachBudgetMs\)'){'TRIP_DEADLINE_CAP'}
 foreach($poll in @($approach,$planned)){
  if($poll -match 'CalculateSpawnApproachBudgetMs|approachDeadlineMs\s*=|SetSpawnPlan\(' -or $poll -notmatch 'nowMs >= plan.GetApproachDeadlineMs\(\)'){'NO_POLL_REARM'}
 }
 if($budget -match 'CallLater|SetOrigin|SpawnEntity|AddAction|CommitTransition' -or $measure -match 'CallLater|SetOrigin|SpawnEntity|AddAction|CommitTransition'){'READ_ONLY_POLICY'}
}
$failures=@(Rules $source)
if($failures.Count){throw "Acquisition deadline: $failures"}
if(@([regex]::Matches($plan,'m_iApproachDeadlineMs\s*=')).Count -ne 1){throw 'Plan deadline must remain constructor-only'}
$controls=1
foreach($mutation in @(
 @('Math.Ceil(Math.Max(0.0, farthestDistanceMeters) /','Math.Ceil(farthestDistanceMeters /','WALK_ALLOWANCE'),
 @('Math.Min(MAX_DISTANCE_APPROACH_BUDGET_MS,','Math.Max(MAX_DISTANCE_APPROACH_BUDGET_MS,','DISTANCE_CAP'),
 @('return Math.Max(configuredTimeoutMs, distanceBudgetMs);','return distanceBudgetMs;','CONFIGURED_MINIMUM'),
 @('vector.DistanceXZ(member.GetOrigin(), stagingPosition)','vector.DistanceXZ(group.GetOrigin(), stagingPosition)','ALL_LIVING_POSITIONS'),
 @('if (agent.GetParentGroup() != group)','if (false)','ROSTER_IDENTITY'),
 @('Math.Min(trip.GetAbsoluteDeadlineMs(), nowMs + approachBudgetMs)','nowMs + approachBudgetMs','TRIP_DEADLINE_CAP')
)){
 if(!$source.Contains($mutation[0])){throw "Mutation target absent: $($mutation[2])"}
 if($mutation[2] -notin @(Rules ($source.Replace($mutation[0],$mutation[1])))){throw "Mutation survived: $($mutation[2])"}
 $controls++
}
$approach=Body $source 'ProcessApproachingSite'
$rearmed=$source.Replace($approach,$approach.Insert(1,"`n CalculateSpawnApproachBudgetMs(90000, 500);"))
if('NO_POLL_REARM' -notin @(Rules $rearmed)){throw 'Rearm mutation survived'}
$controls++
$measure=Body $source 'MeasureSpawnApproachDistance'
$mutating=$source.Replace($measure,$measure.Insert(1,"`n group.SetOrigin(stagingPosition);"))
if('READ_ONLY_POLICY' -notin @(Rules $mutating)){throw 'Mutation side effect survived'}
$controls++
"PASS Vehicle acquisition deadline: fixed living-member distance allowance, configured floor, ten-minute dynamic cap, trip cap, roster fences and no rearm; controls=$controls"
