param([string]$RepositoryRoot=(Split-Path -Parent (Split-Path -Parent $PSScriptRoot)))
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot '../lib/Stage3StaticAudit.Common.ps1')
$taskCore=Join-Path $RepositoryRoot 'AIConflictCore/Scripts/Game/AIConflict'
$taskView=[IO.File]::ReadAllText((Join-Path $taskCore 'State/Vehicles/AICF_TransportTripRegistry.c'))
$taskController=[IO.File]::ReadAllText((Join-Path $taskCore 'Bootstrap/AICF_MatchController.c'))
function Body([string]$Source,[string]$Name){Get-AICFMethodBody ([pscustomobject]@{Source=$Source;Code=(ConvertTo-AICFCodeText $Source)}) $Name}
function Rules([string]$View,[string]$Controller){
 $constructor=Body $View 'AICF_VehicleSlotView'
 $meaningful=Body $Controller 'HasMeaningfulTask'
 $idle=Body $Controller 'IsBoundedVehicleLifecycle'
 if(!$constructor -or !$meaningful -or !$idle){'METHODS';return}
 if($constructor -notmatch 'm_bNativeCombatTask = currentBaseRevision >= 0 && assignment &&\s*assignment.GetBaseRevision\(\) == currentBaseRevision && AICF_TransportCombatHold.IsCurrentCombat\(trip\);'){'CURRENT_TYPED_COMBAT_PROOF'}
 if($View -notmatch 'bool HasNativeCombatTask\(\) \{ return m_bNativeCombatTask; \}'){'READ_ONLY_GETTER'}
 if($meaningful -notmatch 'if \(vehicleView && vehicleView.HasNativeCombatTask\(\)\)\s*return true;' -or $meaningful -notmatch '!slot \|\| !slot.HasStrategicDestination\(\)'){'TASK_USES_PROOF'}
 if($idle -notmatch 'if \(vehicleView && vehicleView.IsControllingMovement\(\) && vehicleView.HasNativeCombatTask\(\)\)\s*return true;'){'BOUNDED_IDLE_USES_SAME_PROOF'}
 if($Controller -notmatch 'native_vehicle_combat=%1' -or $Controller -notmatch 'vehicleView && vehicleView.HasNativeCombatTask\(\)\);'){'DECISION_READBACK'}
 if($constructor -match 'SetOrigin|CommitTransition|SetAbsoluteDeadline|SetPresetDestinationWaiting|AddAction|CallLater'){'READ_ONLY_PROJECTION'}
}
$taskFailures=@(Rules $taskView $taskController)
if($taskFailures.Count){throw "Combat task projection: $taskFailures"}
$taskExpression=[regex]::Match((Body $taskView 'AICF_VehicleSlotView'),'m_bNativeCombatTask = ([\s\S]*?);').Groups[1].Value
$taskExpression=$taskExpression.Replace('AICF_TransportCombatHold.IsCurrentCombat(trip)','$native').Replace('assignment.GetBaseRevision()','$snapshotRevision').Replace('currentBaseRevision','$currentRevision').Replace('assignment','$hasAssignment').Replace('>=','-ge').Replace('==','-eq').Replace('&&','-and')
$taskEvaluate=[scriptblock]::Create('param($currentRevision,$snapshotRevision,$hasAssignment,$native) '+$taskExpression)
$taskCases=@(@(1,1,$true,$true,$true),@(1,1,$true,$false,$false),@(2,1,$true,$true,$false),@(-1,-1,$true,$true,$false),@(1,1,$false,$true,$false))
foreach($taskCase in $taskCases){if([bool](& $taskEvaluate $taskCase[0] $taskCase[1] $taskCase[2] $taskCase[3]) -ne $taskCase[4]){throw 'Stale or missing combat context became a task'}}
$taskControls=1
foreach($taskMutation in @(
 @('view','currentBaseRevision >= 0','true','CURRENT_TYPED_COMBAT_PROOF'),
 @('view','assignment.GetBaseRevision() == currentBaseRevision','true','CURRENT_TYPED_COMBAT_PROOF'),
 @('view','AICF_TransportCombatHold.IsCurrentCombat(trip)','true','CURRENT_TYPED_COMBAT_PROOF'),
 @('view','return m_bNativeCombatTask;','return true;','READ_ONLY_GETTER'),
 @('controller','if (vehicleView && vehicleView.HasNativeCombatTask())','if (vehicleView && vehicleView.GetPhase() == "TRANSIT")','TASK_USES_PROOF'),
 @('controller','vehicleView.IsControllingMovement() && vehicleView.HasNativeCombatTask()','vehicleView.HasNativeCombatTask()','BOUNDED_IDLE_USES_SAME_PROOF'),
 @('controller','native_vehicle_combat=%1','combat_hidden=%1','DECISION_READBACK')
)){
 $view=$taskView;$controller=$taskController
 if($taskMutation[0] -eq 'view'){$view=$view.Replace($taskMutation[1],$taskMutation[2])}else{$controller=$controller.Replace($taskMutation[1],$taskMutation[2])}
 if($taskMutation[3] -notin @(Rules $view $controller)){throw "Mutation survived: $($taskMutation[3])"}
 $taskControls++
}
"PASS Native combat task projection: controls=$taskControls source_boolean_cases=$($taskCases.Count)"
