param([string]$RepositoryRoot=(Split-Path -Parent (Split-Path -Parent $PSScriptRoot)))
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot '../lib/Stage3StaticAudit.Common.ps1')
$base=Join-Path $RepositoryRoot 'AIConflictCore/Scripts/Game/AIConflict/Vehicles'
$helper=[IO.File]::ReadAllText((Join-Path $base 'AICF_VehicleManualStagingRecovery.c'))
$flow=[IO.File]::ReadAllText((Join-Path $base 'AICF_VehicleAcquisitionFlow.c'))
$tokens=[IO.File]::ReadAllText((Join-Path $base 'AICF_VehicleSpawner.c'))
function Body($s,$m){Get-AICFMethodBody ([pscustomobject]@{Source=$s;Code=(ConvertTo-AICFCodeText $s)}) $m}
function Rules($h,$f,$t){
 $current=(Body $h 'IsCurrent')+(Body $h 'HasCurrentGuidance');$roster=Body $h 'IsExactFootRoster';$fit=Body $h 'IsFitFootActor';$move=Body $h 'TryRecover';$safe=Body $h 'SafeDestination';$maintain=Body $f 'MaintainStagingMemberActions'
 foreach($guard in @('!Replication.IsServer()','m_Trip.GetPhase() != AICF_ETransportTripPhase.APPROACHING_SITE','m_Trip.GetLease()',
  '!m_Trip.IsVehiclePresetCurrent()','m_Trip.IsVehiclePresetDebited()','m_Trip.GetAssignment() != m_Assignment',
  '!m_CurrentAssignment.MatchesCurrent(', 'm_CurrentAssignment.GetAssignmentRevision() != m_Assignment.GetAssignmentRevision()',
  'm_CurrentAssignment.GetStrategicIntentRevision() != m_Assignment.GetStrategicIntentRevision()',
  'm_CurrentAssignment.GetBaseRevision() != m_Assignment.GetBaseRevision()', 'slot.GetSpawnGeneration() != m_Trip.GetGroupGeneration()',
  'm_Trip.GetVehiclePresetRevision() != m_iPresetRevision','m_Trip.GetRequestState() != m_Request','m_Request.GetRequestGeneration() != m_iRequestGeneration',
  'm_Request.GetSpawnPlan() != m_Plan','!m_Plan.RequiresManualRally()','!m_Spawner.IsSiteReservationCurrent(',
  'm_Trip.IsDeadlineReached(nowMs)','m_Request.IsDeadlineReached(nowMs)','nowMs >= m_Plan.GetApproachDeadlineMs()',
  'm_Plan.GetStagingPosition() != m_vStaging','m_Plan.GetApproachWaypoint() != m_Waypoint','m_Waypoint.GetOrigin() != m_vWaypoint',
  'm_Group.GetCurrentWaypoint() == m_Waypoint','m_Group.GetID() != m_GroupId','IsAuthoritativeReplicatedEntity(m_Group)',
  'ComponentRpl(m_Group) != m_sGroupRpl','waypoints.Contains(m_Waypoint)','activity.m_RelatedWaypoint == m_Waypoint',
  'groupUtility.GetExecutedAction() == activity','m_Plan.FindStagingActionToken(m_Actor) != m_Token','!m_Token.Matches(',
  'm_Token.GetRetryCount() != 1','!m_Plan.WasStagingMemberRetried(m_Actor)','!m_Plan.IsStagingMemberExhausted(m_Actor)',
  'm_Plan.WasStagingMemberRelocated(m_Actor)','!m_Token.HasNoPhysicalMotion(nowMs, 5000)',
  'nowMs - m_Token.GetLastProgressAtMs() < 12000','!m_Token.IsActionOwnedByUtility()',
  'utility.GetCurrentAction() != m_Token.GetAction()','utility.GetExecutedAction() != m_Token.GetAction()',
  'vector.Distance(m_Actor.GetOrigin(), m_vOrigin) > 0.5','!m_Token.MatchesTaskAssignment(m_Assignment)','!(m_fRadius > 0)',
  'AICF_VehicleStagingTaskProof.HasCurrentTask(','waypoints.IsEmpty()','m_Group.GetCurrentWaypoint() != null')){if(!$current.Contains($guard)){'CURRENT_FULL_NATIVE_OWNER';break}}
 foreach($guard in @('slot.GetRosterMember(position) != actor','m_Trip.VehiclePresetMember(position)','!agents.Contains(agent)',
  '!IsFitFootActor(actor, agent)','captured > 0','captured == assigned','captured == slot.GetDesiredSize()','captured == agents.Count()','outside > 0',
  'actor != m_Actor && m_Plan.IsStagingMemberExhausted(actor)')){
  if(!$roster.Contains($guard)){'FULL_ORIGINAL_FIT_ROSTER';break}
 }
 foreach($guard in @('IsAuthoritativeAIEntity(actor)','!controller || controller.IsPlayerControlled()','!SCR_AIDamageHandling.IsConscious(actor)','agent.GetParentGroup() != m_Group',
  'GetControlAIAgent() != agent','!actor.IsInVehicle()','!CompartmentAccessComponent.GetVehicleIn(actor)',
  '!access.IsInCompartment()','!access.IsGettingIn()','!access.IsGettingOut()','!access.IsSwitchingSeatsAnim()')){
  if(!$fit.Contains($guard)){'NONPLAYER_FIT_FOOT_ONLY';break}
 }
 if(!$maintain.Contains('token.ObservePhysicalMotion(entity.GetOrigin(), nowMs);') -or
  !$maintain.Contains('plan.RequiresManualRally() && token.GetRetryCount() == 1') -or
  !$maintain.Contains('token.HasNoPhysicalMotion(nowMs, STAGING_MEMBER_ACTION_AUDIT_INTERVAL_MS)')){'REAL_NO_MOTION_SECOND_ATTEMPT'}
 $stall=$maintain.Substring($maintain.IndexOf('if (nowMs - token.GetLastProgressAtMs()'))
 if($stall.IndexOf('RecordStagingMemberAttemptOutcome(plan, entity)') -gt $stall.IndexOf('recovery.TryRecover()') -or
  $stall.IndexOf('recovery.TryRecover()') -gt $stall.IndexOf('CancelStagingMemberAction(plan, token)')){'OWNED_AFTER_SPENT_BEFORE_CANCEL'}
 if(!$stall.Contains('if (recovery.TryRecover()) return;')){'CONTINUE_EXACT_SAME_NATIVE_TASK_AFTER_UNBLOCK'}
 if(!$move.Contains('m_bConsumed = true;') -or !$move.Contains('m_Plan.MarkStagingMemberRelocated(m_Actor);') -or
  $move.IndexOf('MarkStagingMemberRelocated') -gt $move.IndexOf('.Teleport(') -or
  ([regex]::Matches($move,'!IsCurrent\(System.GetTickCount\(\)\)')).Count -ne 2 -or
  !$move.Contains('!SafeDestination(destination, checked)') -or !$move.Contains('vector.Distance(checked, destination) > 0.1') -or
  !$move.Contains('!(vector.Distance(m_Actor.GetOrigin(), destination) <= MAX_SHIFT_METERS)')){'CONSUME_BEFORE_SINGLE_REPEAT_FENCED_TELEPORT'}
 if(([regex]::Matches($h,'\.Teleport\(')).Count -ne 1 -or $h -match '\b(?:CommitTransition|AddAction|SetOrigin|SetHealth|SetLifeState|SetVelocity|CallLater|MarkVehiclePresetDebited|Set.*Deadline)\s*\('){'NO_NEW_TASK_BUDGET_HEALTH_OR_PHASE_MUTATIONS'}
 if(!$safe.Contains('AICF_InfantrySpawnPlacement.IsUsable(m_Group, query, destination)') -or
  !$safe.Contains('IsBoundedStep(vector.Distance(m_vOrigin, destination)') -or !$safe.Contains('vector.DistanceSqXZ(query, destination)') -or
  !$safe.Contains('padDistance >= 10 && padDistance <= 90')){'NATIVE_GROUND_ACTUAL_3D'}
 if(!(Body $t 'HasNoPhysicalMotion').Contains('vector.Distance(m_ReservedEntity.GetOrigin(), m_vLastMotionOrigin) <= 0.5') -or
  !(Body $t 'ObservePhysicalMotion').Contains('m_iLastMotionAtMs = nowMs')){'NATIVE_POSITION_HISTORY'}
}
$bad=@(Rules $helper $flow $tokens);if($bad.Count){throw "Manual staging short production: $bad"}
$mutants=@('m_Trip.GetLease()','!m_Trip.IsVehiclePresetCurrent()','m_Trip.IsVehiclePresetDebited()',
 'm_CurrentAssignment.GetBaseRevision() != m_Assignment.GetBaseRevision()','m_Request.GetSpawnPlan() != m_Plan',
 'm_Request.GetRequestGeneration() != m_iRequestGeneration','nowMs >= m_Plan.GetApproachDeadlineMs()',
 'm_Group.GetID() != m_GroupId','ComponentRpl(m_Group) != m_sGroupRpl','m_Token.GetRetryCount() != 1',
 '!m_Plan.IsStagingMemberExhausted(m_Actor)','m_Plan.WasStagingMemberRelocated(m_Actor)',
 '!m_Token.HasNoPhysicalMotion(nowMs, 5000)','utility.GetExecutedAction() != m_Token.GetAction()',
 'captured > 0','captured == assigned','captured == agents.Count()','outside > 0','actor != m_Actor && m_Plan.IsStagingMemberExhausted(actor)',
 '!m_Token.MatchesTaskAssignment(m_Assignment)','!(m_fRadius > 0)','!SCR_AIDamageHandling.IsConscious(actor)','!controller || controller.IsPlayerControlled()',
 '!access.IsSwitchingSeatsAnim()','m_Plan.MarkStagingMemberRelocated(m_Actor);')
foreach($m in $mutants){if(!$helper.Contains($m)){throw "Missing mutant $m"};if(!@(Rules ($helper.Replace($m,'false')) $flow $tokens).Count){throw "Escaped mutant $m"}}
$body=Body $helper 'IsBoundedStep'
$name='AICFManualStagingGeometry'+[guid]::NewGuid().ToString('N')
Add-Type "public static class $name { const float MAX_SHIFT_METERS=8; public static bool Check(float shift,float vertical,float projectionSq,float before,float after) {$body} }"
$method=([type]$name).GetMethod('Check')
$cases=@(@(5,1,.25,28,23,$true),@(8,1,.25,28,20,$true),@(8.001,0,0,28,20,$false),
 @(7.99,1.001,0,28,20,$false),@(5,0,.2501,28,23,$false),@(1.999,0,0,28,26,$false),
 @(5,0,0,28,27,$false),@(3,0,0,36.0178,33.0178,$true),@([float]::NaN,0,0,28,23,$false),
 @(5,[float]::NaN,0,28,23,$false),@(5,0,[float]::NaN,28,23,$false),@(5,0,0,28,[float]::NaN,$false))
foreach($case in $cases){$args=@([single]$case[0],[single]$case[1],[single]$case[2],[single]$case[3],[single]$case[4]);if($method.Invoke($null,$args) -ne $case[5]){throw "Geometry control escaped $case"}}
$guidance=Body $helper 'AcceptGuidanceShape'
$guidanceName='AICFManualStagingGuidance'+[guid]::NewGuid().ToString('N')
Add-Type "public static class $guidanceName {public static bool Check(bool exactGroupTask,bool queueEmpty,bool hasCurrentWaypoint,bool exactActorTask) $guidance }"
$guidanceMethod=([type]$guidanceName).GetMethod('Check')
foreach($mask in 0..15){$g=($mask -band 1) -ne 0;$empty=($mask -band 2) -ne 0;$wp=($mask -band 4) -ne 0;$actor=($mask -band 8) -ne 0;$expected=$g -or ($empty -and !$wp -and $actor);if($guidanceMethod.Invoke($null,@($g,$empty,$wp,$actor)) -ne $expected){throw 'Foreign/unknown group or actor task granted correction'}}
$cardinality=[regex]::Match((Body $helper 'IsExactFootRoster'),'return (?<expr>found &&[^;]+);').Groups['expr'].Value
$cardinality=$cardinality.Replace('slot.GetDesiredSize()','desired').Replace('agents.Count()','actual')
$targetIndex=$cardinality.IndexOf('!AICF_VehicleSpawner.IsStagingGeometryReady(')
if($targetIndex -lt 0){throw 'Missing actual final target-rally predicate'}
$cardinality=$cardinality.Substring(0,$targetIndex)+'!targetReady'
$cardinalityName='AICFManualStagingCardinality'+[guid]::NewGuid().ToString('N')
Add-Type "public static class $cardinalityName {public static bool Check(bool found,int captured,int assigned,int desired,int actual,int outside,bool targetReady){return $cardinality;} }"
$cm=([type]$cardinalityName).GetMethod('Check');$cardinalityCases=0
foreach($size in 1..10){foreach($kind in @('valid','assigned','desired','native')){$assigned=$size;$desired=$size;$actual=$size;switch($kind){assigned{$assigned++}desired{$desired++}native{$actual++}};if($cm.Invoke($null,@($true,[int]$size,[int]$assigned,[int]$desired,[int]$actual,1,$false)) -ne ($kind -eq 'valid')){throw "Manual short cardinality failed size=$size kind=$kind"};$cardinalityCases++}}
foreach($args in @(@($true,0,0,0,0,1,$false),@($false,1,1,1,1,1,$false),@($true,1,1,1,1,0,$false),@($true,1,1,1,1,1,$true))){if($cm.Invoke($null,$args)){throw 'Empty/unknown/ready actor manual correction permitted'};$cardinalityCases++}
if(!@(Rules $helper ($flow.Replace('if (recovery.TryRecover()) return;','recovery.TryRecover();')) $tokens).Contains('CONTINUE_EXACT_SAME_NATIVE_TASK_AFTER_UNBLOCK')){throw 'Cancelling same native action after short correction escaped'}
"PASS Manual staging short recovery: $($mutants.Count) source mutants +$($cases.Count) actual geometry +16 actual guidance +$cardinalityCases actual cardinality; single Pilot valid, foreign/unknown blocked"
