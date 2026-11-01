param([string]$RepositoryRoot=(Split-Path -Parent (Split-Path -Parent $PSScriptRoot)))
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot '../lib/Stage3StaticAudit.Common.ps1')
$base=Join-Path $RepositoryRoot 'AIConflictCore/Scripts/Game/AIConflict/Vehicles'
$helper=[IO.File]::ReadAllText((Join-Path $base 'AICF_VehicleStagingExhaustion.c'))
$flow=[IO.File]::ReadAllText((Join-Path $base 'AICF_VehicleAcquisitionFlow.c'))
$spawner=[IO.File]::ReadAllText((Join-Path $base 'AICF_VehicleSpawner.c'))
function Body($s,$m){Get-AICFMethodBody ([pscustomobject]@{Source=$s;Code=(ConvertTo-AICFCodeText $s)}) $m}
function Rules($h,$f,$s){
 $current=Body $h 'IsCurrent';$roster=Body $h 'IsRosterCurrent';$task=Body $h 'HasCurrentActorTask';$decision=Body $h 'ShouldEnd';$approach=Body $f 'ProcessApproachingSite'
 foreach($guard in @('!m_bCaptured','!Replication.IsServer()','m_Trip.GetPhase() != AICF_ETransportTripPhase.APPROACHING_SITE',
  'm_Trip.GetLease()','m_Trip.IsVehiclePresetDebited()','!m_Trip.IsVehiclePresetCurrent()',
  '!current.MatchesCurrent(','current.GetAssignmentRevision() != observed.GetAssignmentRevision()',
  'current.GetBaseRevision() != observed.GetBaseRevision()','current.GetStrategicIntentRevision() != observed.GetStrategicIntentRevision()',
  'm_Trip.GetTripGeneration() != m_iTripGeneration','m_Trip.GetGroupGeneration() != m_iGroupGeneration',
  'm_Trip.GetVehiclePresetRevision() != m_iPresetRevision','m_Trip.GetRequestState() != m_Request',
  'm_Request.GetRequestGeneration() != m_iRequestGeneration','m_Request.GetSpawnPlan() != m_Plan',
  'm_Slot.GetSpawnGeneration() != m_iGroupGeneration','m_Slot.GetStrategicAssignmentRevision() != observed.GetAssignmentRevision()',
  '!m_Plan.RequiresManualRally()','!spawner.IsSiteReservationCurrentReadOnly(','nowMs >= m_Plan.GetApproachDeadlineMs()',
  'm_Trip.IsDeadlineReached(nowMs)','m_Request.IsDeadlineReached(nowMs)')){if(!$current.Contains($guard)){'FULL_CURRENT_PLAN_IDENTITY';break}}
 foreach($guard in @('m_Group.GetID() != m_GroupId','IsAuthoritativeReplicatedEntity(m_Group)','ComponentRpl(m_Group) != m_sGroupRpl',
  'm_Slot.GetRosterMember(position) != actor','m_Trip.VehiclePresetMember(position) != actor','actor.GetID() != m_aMemberIds[position]',
  'ComponentRpl(actor) != m_aMemberRpls[position]','!agents.Contains(agent)','agent.GetParentGroup() != m_Group','agent.GetControlledEntity() != actor',
  'IsAuthoritativeAIEntity(actor)','!SCR_AIDamageHandling.IsConscious(actor)','controller.IsPlayerControlled()',
  'access.IsSwitchingSeatsAnim()','captured == assigned','captured == m_Slot.GetDesiredSize()','captured == agents.Count()')){
  if(!$roster.Contains($guard)){'IMMUTABLE_ORIGINAL_ROSTER_RPL_AUTHORITY';break}
 }
 foreach($guard in @('!token.Matches(m_Trip, m_iRequestGeneration, m_Group)','!token.IsActionOwnedByUtility()',
  'action.GetRemoveAction()','action.GetActionIsSuspended()','EAIActionState.FAILED','EAIActionState.COMPLETED',
  'utility.GetCurrentAction() == action','utility.GetExecutedAction() == action')){if(!$task.Contains($guard)){'ACTUAL_OWNED_SELECTED_EXECUTED_TASK';break}}
 foreach($guard in @('!IsCurrent(current, spawner, nowMs)','m_Plan.GetApproachWaypointReissueCount() != 1','HasCurrentActorTask()',
  '!queue.IsEmpty() || m_Group.GetCurrentWaypoint()','!m_Plan.WasStagingMemberRetried(actor)',
  '!m_Plan.IsStagingMemberExhausted(actor)','m_Plan.FindStagingActionToken(actor)','return outside > 0;')){
  if(!$decision.Contains($guard)){'SPENT_BUDGET_NO_REAL_TASK_NO_PHYSICAL_RALLY';break}
 }
 if(!$f.Contains('plan.CaptureManualStagingRoster(trip)') -or !$s.Contains('protected ref AICF_VehicleStagingExhaustion m_ManualExhaustion') -or
  $approach.IndexOf('MaintainStagingMemberActions(') -gt $approach.IndexOf('exhaustion.ShouldEnd(') -or
  !$approach.Contains('return EndRequest(trip, fleet, requestState, "SPAWN_STAGING_ACTIONS_EXHAUSTED", nowMs);')){'ORIGINAL_FLOW_TYPED_FALLBACK_ONLY'}
 foreach($body in @($current,$roster,$task,$decision,(Body $s 'IsSiteReservationCurrentReadOnly'))){
  if($body -match '\b(?:PurgeExpired|Set\w*|Mark\w*|AddAction|Teleport|Fail|Complete|Commit\w*|CallLater|Withdraw|RemoveWaypoint)\s*\('){'READ_ONLY_NO_NEW_RETRY_CLOCK_TASK_OR_HEALTH'}
 }
}
$bad=@(Rules $helper $flow $spawner);if($bad.Count){throw "Staging exhaustion source: $bad"}
# Execute the production cardinality guard: one retained Pilot is a complete
# manual roster, while every assigned/desired/native mismatch stays rejected.
$rosterBody=Body $helper 'IsRosterCurrent'
$cardinality=[regex]::Match($rosterBody,'return\s+(captured\s*>[^;]+);')
if(!$cardinality.Success){throw 'Missing production staging cardinality guard'}
$cardinalityExpression=$cardinality.Groups[1].Value.Replace('m_Slot.GetDesiredSize()','desired').Replace('agents.Count()','actual')
$cardinalityName='AICFStagingCardinality'+[guid]::NewGuid().ToString('N')
Add-Type "public static class $cardinalityName {public static bool Check(int captured,int assigned,int desired,int actual){return $cardinalityExpression;}}"
$cardinalityMethod=([type]$cardinalityName).GetMethod('Check')
$cardinalityCases=0
foreach($size in 1..10){
 foreach($kind in @('complete','missingAssignment','wrongDesired','extraNative','incompleteNative')){
  $assigned=$size;$desired=$size;$actual=$size
  switch($kind){missingAssignment{$assigned--}wrongDesired{$desired++}extraNative{$actual++}incompleteNative{$actual--}}
  if($cardinalityMethod.Invoke($null,@([int]$size,[int]$assigned,[int]$desired,[int]$actual)) -ne ($kind -eq 'complete')){throw "Staging cardinality rejected/accepted wrong roster: size=$size kind=$kind"}
  $cardinalityCases++
 }
}
if($cardinalityMethod.Invoke($null,@(0,0,0,0))){throw 'Empty staging roster accepted'}
$cardinalityCases++
Write-Output "PASS Staging cardinality: actual production guard cases=$cardinalityCases; single Pilot valid, incomplete/foreign/native-count mismatches rejected"
$mutants=@('!m_bCaptured','!m_Trip.IsVehiclePresetCurrent()','current.GetBaseRevision() != observed.GetBaseRevision()',
 'm_Request.GetRequestGeneration() != m_iRequestGeneration','m_Request.GetSpawnPlan() != m_Plan','nowMs >= m_Plan.GetApproachDeadlineMs()',
 'ComponentRpl(m_Group) != m_sGroupRpl','ComponentRpl(actor) != m_aMemberRpls[position]','m_Trip.VehiclePresetMember(position) != actor',
 'captured == agents.Count()','!token.IsActionOwnedByUtility()','utility.GetExecutedAction() == action','action.GetActionIsSuspended()',
 'm_Plan.GetApproachWaypointReissueCount() != 1','HasCurrentActorTask()','!queue.IsEmpty() || m_Group.GetCurrentWaypoint()',
 '!m_Plan.IsStagingMemberExhausted(actor)','m_Plan.FindStagingActionToken(actor)','return outside > 0;')
foreach($m in $mutants){if(!$helper.Contains($m)){throw "Missing mutant $m"};if(!@(Rules ($helper.Replace($m,'false')) $flow $spawner).Count){throw "Escaped source mutant $m"}}
$body=(Body $helper 'ShouldEnd').Replace('array<AIWaypoint> queue = {};','var queue = new AICFList<AIWaypoint>();').Replace('foreach (IEntity actor : m_aMembers)','foreach (IEntity actor in m_aMembers)').Replace('if (!actor)','if (actor == null)').Replace('int outside;','int outside = 0;')
$name='AICFExhaustionDecision'+[guid]::NewGuid().ToString('N')
$code=@'
using System; using System.Collections.Generic; namespace NAMESPACE_NAME {
public class AICFList<T>:List<T>{public bool IsEmpty(){return Count==0;}}
public class AIWaypoint{} public class AICF_StrategicAssignmentSnapshot{}
public class IEntity{public float Distance=28;public bool Retried=true,Exhausted=true,Token=false;public float GetOrigin(){return Distance;}}
public static class vector{public static float DistanceXZ(float origin,float target){return origin;}}
public class AICF_VehicleSpawner{public static bool IsStagingGeometryReady(float distance,float radius,float pad,float envelope,bool manual){return distance<=radius&&pad>=10&&pad<=envelope;}}
public class AICFPlan{public int Reissues=1;public int GetApproachWaypointReissueCount(){return Reissues;}public float GetStagingPosition(){return 0;}public float GetSpawnPosition(){return 0;}public bool WasStagingMemberRetried(IEntity actor){return actor.Retried;}public bool IsStagingMemberExhausted(IEntity actor){return actor.Exhausted;}public bool FindStagingActionToken(IEntity actor){return actor.Token;}}
public class AICFGroup{public bool Queued,Current;public void GetWaypoints(AICFList<AIWaypoint> q){if(Queued)q.Add(new AIWaypoint());}public bool GetCurrentWaypoint(){return Current;}}
public class Model{public bool CurrentProof=true,NativeTask;public AICFPlan m_Plan=new AICFPlan();public AICFGroup m_Group=new AICFGroup();public List<IEntity> m_aMembers=new List<IEntity>{new IEntity()};public bool IsCurrent(AICF_StrategicAssignmentSnapshot c,AICF_VehicleSpawner s,int now){return CurrentProof;}public bool HasCurrentActorTask(){return NativeTask;}public bool ShouldEnd(AICF_StrategicAssignmentSnapshot current,AICF_VehicleSpawner spawner,int nowMs,float radius) BODY}}
'@
Add-Type ($code.Replace('NAMESPACE_NAME',$name).Replace('BODY',$body))
$type=[type]($name+'.Model');$entityType=[type]($name+'.IEntity');$method=$type.GetMethod('ShouldEnd');$cases=0
foreach($kind in @('exhausted','stale','nativeTask','queued','currentWP','noReissue','extraReissue','notRetried','notExhausted','pendingToken','allPhysicalRally','zeroRadius','unknownRadius','secondUnexhausted')){
 $model=[Activator]::CreateInstance($type);$radius=[single]25
 switch($kind){
  stale{$model.CurrentProof=$false}nativeTask{$model.NativeTask=$true}queued{$model.m_Group.Queued=$true}currentWP{$model.m_Group.Current=$true}
  noReissue{$model.m_Plan.Reissues=0}extraReissue{$model.m_Plan.Reissues=2}notRetried{$model.m_aMembers[0].Retried=$false}
  notExhausted{$model.m_aMembers[0].Exhausted=$false}pendingToken{$model.m_aMembers[0].Token=$true}allPhysicalRally{$model.m_aMembers[0].Distance=20}
  zeroRadius{$radius=0}unknownRadius{$radius=[single]::NaN}secondUnexhausted{$actor=[Activator]::CreateInstance($entityType);$actor.Exhausted=$false;$model.m_aMembers.Add($actor)}
 }
 if($method.Invoke($model,@($null,$null,0,$radius)) -ne ($kind -eq 'exhausted')){throw "Executed decision escaped $kind"};$cases++
}
"PASS Staging exhaustion: $($mutants.Count) source mutants +$cases actual decision-body controls; native identity adapters are source contracts, no phase-only task proof"
