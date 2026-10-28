param([string]$RepositoryRoot=(Split-Path -Parent (Split-Path -Parent $PSScriptRoot)))
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot '../lib/Stage3StaticAudit.Common.ps1')
$base=Join-Path $RepositoryRoot 'AIConflictCore/Scripts/Game/AIConflict/Vehicles'
$helper=[IO.File]::ReadAllText((Join-Path $base 'AICF_VehicleManualStagingRecovery.c'))
$flow=[IO.File]::ReadAllText((Join-Path $base 'AICF_VehicleAcquisitionFlow.c'))
$tokens=[IO.File]::ReadAllText((Join-Path $base 'AICF_VehicleSpawner.c'))
function Body($s,$m){Get-AICFMethodBody ([pscustomobject]@{Source=$s;Code=(ConvertTo-AICFCodeText $s)}) $m}
function Rules($h,$f,$t){
 $current=Body $h 'IsCurrent';$roster=Body $h 'IsExactFootRoster';$fit=Body $h 'IsFitFootActor';$move=Body $h 'TryRecover';$safe=Body $h 'SafeDestination';$maintain=Body $f 'MaintainStagingMemberActions'
 foreach($guard in @('!Replication.IsServer()','m_Trip.GetPhase() != AICF_ETransportTripPhase.APPROACHING_SITE','m_Trip.GetLease()',
  '!m_Trip.IsVehiclePresetCurrent()','m_Trip.IsVehiclePresetDebited()','m_Trip.GetAssignment() != m_Assignment',
  '!m_CurrentAssignment.MatchesCurrent(', 'm_CurrentAssignment.GetAssignmentRevision() != m_Assignment.GetAssignmentRevision()',
  'm_CurrentAssignment.GetStrategicIntentRevision() != m_Assignment.GetStrategicIntentRevision()',
  'm_CurrentAssignment.GetBaseRevision() != m_Assignment.GetBaseRevision()', 'slot.GetSpawnGeneration() != m_Trip.GetGroupGeneration()',
  'm_Trip.GetVehiclePresetRevision() != m_iPresetRevision','m_Trip.GetRequestState() != m_Request','m_Request.GetRequestGeneration() != m_iRequestGeneration',
  'm_Request.GetSpawnPlan() != m_Plan','!m_Plan.RequiresManualRally()','!m_Spawner.IsSiteReservationCurrent(',
  'm_Trip.IsDeadlineReached(nowMs)','m_Request.IsDeadlineReached(nowMs)','nowMs >= m_Plan.GetApproachDeadlineMs()',
  'm_Plan.GetStagingPosition() != m_vStaging','m_Plan.GetApproachWaypoint() != m_Waypoint','m_Waypoint.GetOrigin() != m_vWaypoint',
  'm_Group.GetCurrentWaypoint() != m_Waypoint','m_Group.GetID() != m_GroupId','IsAuthoritativeReplicatedEntity(m_Group)',
  'ComponentRpl(m_Group) != m_sGroupRpl','!waypoints.Contains(m_Waypoint)','activity.m_RelatedWaypoint != m_Waypoint',
  'groupUtility.GetExecutedAction() != activity','m_Plan.FindStagingActionToken(m_Actor) != m_Token','!m_Token.Matches(',
  'm_Token.GetRetryCount() != 1','!m_Plan.WasStagingMemberRetried(m_Actor)','!m_Plan.IsStagingMemberExhausted(m_Actor)',
  'm_Plan.WasStagingMemberRelocated(m_Actor)','!m_Token.HasNoPhysicalMotion(nowMs, 5000)',
  'nowMs - m_Token.GetLastProgressAtMs() < 12000','!m_Token.IsActionOwnedByUtility()',
  'utility.GetCurrentAction() != m_Token.GetAction()','utility.GetExecutedAction() != m_Token.GetAction()',
  'vector.Distance(m_Actor.GetOrigin(), m_vOrigin) > 0.5')){if(!$current.Contains($guard)){'CURRENT_FULL_NATIVE_OWNER';break}}
 foreach($guard in @('slot.GetRosterMember(position) != actor','m_Trip.VehiclePresetMember(position)','!agents.Contains(agent)',
  '!IsFitFootActor(actor, agent)','captured == assigned','captured == slot.GetDesiredSize()','captured == agents.Count()','outside == 1')){
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
 if(!$move.Contains('m_bConsumed = true;') -or !$move.Contains('m_Plan.MarkStagingMemberRelocated(m_Actor);') -or
  $move.IndexOf('MarkStagingMemberRelocated') -gt $move.IndexOf('.Teleport(') -or
  ([regex]::Matches($move,'!IsCurrent\(System.GetTickCount\(\)\)')).Count -ne 2 -or
  !$move.Contains('!SafeDestination(destination, checked)') -or !$move.Contains('vector.Distance(checked, destination) > 0.1') -or
  !$move.Contains('!(vector.Distance(m_Actor.GetOrigin(), destination) <= MAX_SHIFT_METERS)')){'CONSUME_BEFORE_SINGLE_REPEAT_FENCED_TELEPORT'}
 if(([regex]::Matches($h,'\.Teleport\(')).Count -ne 1 -or $h -match '\b(?:CommitTransition|AddAction|SetOrigin|SetHealth|SetLifeState|SetVelocity|CallLater|MarkVehiclePresetDebited|Set.*Deadline)\s*\('){'NO_NEW_TASK_BUDGET_HEALTH_OR_PHASE_MUTATIONS'}
 if(!$safe.Contains('AICF_InfantrySpawnPlacement.IsUsable(m_Group, query, destination)') -or
  !$safe.Contains('IsBoundedStep(vector.Distance(m_vOrigin, destination)') -or !$safe.Contains('vector.DistanceSqXZ(query, destination)')){'NATIVE_GROUND_ACTUAL_3D'}
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
 'captured == assigned','captured == agents.Count()','outside == 1','!SCR_AIDamageHandling.IsConscious(actor)','!controller || controller.IsPlayerControlled()',
 '!access.IsSwitchingSeatsAnim()','m_Plan.MarkStagingMemberRelocated(m_Actor);')
foreach($m in $mutants){if(!$helper.Contains($m)){throw "Missing mutant $m"};if(!@(Rules ($helper.Replace($m,'false')) $flow $tokens).Count){throw "Escaped mutant $m"}}
$body=Body $helper 'IsBoundedStep'
$name='AICFManualStagingGeometry'+[guid]::NewGuid().ToString('N')
Add-Type "public static class $name { const float MAX_SHIFT_METERS=8; public static bool Check(float shift,float vertical,float projectionSq,float before,float after,bool rallyReady) {$body} }"
$method=([type]$name).GetMethod('Check')
$cases=@(@(5,1,.25,28,23,$true,$true),@(8,1,.25,28,20,$true,$true),@(8.001,0,0,28,20,$true,$false),
 @(7.99,1.001,0,28,20,$true,$false),@(5,0,.2501,28,23,$true,$false),@(1.999,0,0,28,26,$true,$false),
 @(5,0,0,28,27,$true,$false),@(5,0,0,28,23,$false,$false),@([float]::NaN,0,0,28,23,$true,$false),
 @(5,[float]::NaN,0,28,23,$true,$false),@(5,0,[float]::NaN,28,23,$true,$false),@(5,0,0,28,[float]::NaN,$true,$false))
foreach($case in $cases){$args=@([single]$case[0],[single]$case[1],[single]$case[2],[single]$case[3],[single]$case[4],[bool]$case[5]);if($method.Invoke($null,$args) -ne $case[6]){throw "Geometry control escaped $case"}}
"PASS Manual staging short recovery: full owner/roster/native-action budget contracts; $($mutants.Count) source mutants +$($cases.Count) executed production geometry controls"
