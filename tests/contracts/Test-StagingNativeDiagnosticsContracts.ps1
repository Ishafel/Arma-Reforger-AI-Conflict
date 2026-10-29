param([string]$RepositoryRoot=(Split-Path -Parent (Split-Path -Parent $PSScriptRoot)))
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot '../lib/Stage3StaticAudit.Common.ps1')
$core=Join-Path $RepositoryRoot 'AIConflictCore/Scripts/Game/AIConflict'
$source=[IO.File]::ReadAllText((Join-Path $core 'Diagnostics/AICF_StagingNativeDiagnostics.c'))
$tokens=[IO.File]::ReadAllText((Join-Path $core 'Vehicles/AICF_VehicleSpawner.c'))
$flow=[IO.File]::ReadAllText((Join-Path $core 'Vehicles/AICF_VehicleAcquisitionFlow.c'))
function Body($s,$m){Get-AICFMethodBody ([pscustomobject]@{Source=$s;Code=(ConvertTo-AICFCodeText $s)}) $m}
function Rules($s,$t,$f){
 foreach($guard in @('!Replication.IsServer()','!trip.IsValid()','request.GetSpawnPlan() != plan',
  'agent.GetParentGroup() != group','agent.GetControlledEntity() != actor','!AICF_VehicleBoardingMutationFence.IsAuthoritativeAIEntity(actor)',
  'character.GetAIControlComponent().GetControlAIAgent() != agent','control_agent_exact=','ACTOR_CONTROL_AGENT_MISMATCH',
  'utility.m_OwnerEntity != actor','movement.GetAIAgent() != agent','token.GetAction() != expected',
  'utility.GetCurrentAction() != current || utility.GetExecutedAction() != executed','CONTEXT_CHANGED_DISCARDED',
  'actual_action_goal=','actual_action_radius=','current_is_expected=','executed_is_expected=',
  'expected.m_vPosition.m_Value','expected.m_Entity.m_Value','expected.m_Radius.m_Value',
  'MoveGoal("current", current, token.GetTargetPosition())','MoveGoal("executed", executed, token.GetTargetPosition())',
  'move.m_vPosition.m_Value == tokenGoal','individual.m_Entity.m_Value','individual.m_Radius.m_Value',
  'movement.GetCurrentPath(points)','movement.HasCompletedRequest(false)','points[count - 1].ToString()',
  'Math.Min(count, PATH_LOG_LIMIT)','Math.Min(count, PATH_ARC_LIMIT)','finite && count <= PATH_ARC_LIMIT',
  'path_finite_scope=EXAMINED_PREFIX path_all_finite_proven=','last_finite=',
  'raw_native_order=1 remaining_distance_proven=0')){if(!$s.Contains($guard)){'CURRENT_NATIVE_BOUNDED_RECEIPTS';break}}
 if(@([regex]::Matches($s,'token\.Matches\(')).Count -ne 2){'REPEATED_IDENTITY_AROUND_NATIVE_READ'}
 if($s -match '\b(?:Teleport|SetOrigin|SetActionState|Fail|Complete|SetRemoveAction|CallLater|Insert|SetHealth|SetLifeState)\s*\('){'NO_GAMEPLAY_MUTATION_OR_LISTENER'}
 $motion=Body $t 'ObserveNativeDiagnosticMotion'
 foreach($guard in @('physical_delta=','physical_xz_m=','radial_delta_m=','HasNoPhysicalMotion(nowMs, 5000)',
  'm_vDiagnosticOrigin = origin;','m_fDiagnosticDistance = m_fCurrentDistanceMeters;','m_iDiagnosticAtMs = nowMs;')){if(!$motion.Contains($guard)){'SEPARATE_DIAGNOSTIC_CLOCKS';break}}
 foreach($assignment in @([regex]::Matches($motion,'\b(m_[A-Za-z0-9_]+)\s*='))){if($assignment.Groups[1].Value -notin @('m_vDiagnosticOrigin','m_fDiagnosticDistance','m_iDiagnosticAtMs')){'NO_BEHAVIOR_CLOCK_OR_BUDGET_MUTATION';break}}
 $report=Body $f 'ReportStagingMemberAction'
 if(@([regex]::Matches($report,'AICF_StagingNativeDiagnostics.Report\(')).Count -ne 1 -or !$report.Contains('AICF_Stage4Diagnostics.Info(eventName, details);')){'EXISTING_EVENT_CADENCE_ONLY'}
}
$bad=@(Rules $source $tokens $flow);if($bad.Count){throw "Staging diagnostics: $bad"}
$mutants=@('!Replication.IsServer()','!trip.IsValid()','request.GetSpawnPlan() != plan','agent.GetParentGroup() != group',
 'character.GetAIControlComponent().GetControlAIAgent() != agent','control_agent_exact=','path_finite_scope=EXAMINED_PREFIX path_all_finite_proven=',
 'agent.GetControlledEntity() != actor','utility.m_OwnerEntity != actor','movement.GetAIAgent() != agent',
 'token.GetAction() != expected','utility.GetCurrentAction() != current || utility.GetExecutedAction() != executed',
 'actual_action_goal=','expected.m_vPosition.m_Value','expected.m_Entity.m_Value','expected.m_Radius.m_Value',
 'MoveGoal("current", current, token.GetTargetPosition())','MoveGoal("executed", executed, token.GetTargetPosition())',
 'move.m_vPosition.m_Value == tokenGoal',
 'movement.GetCurrentPath(points)','movement.HasCompletedRequest(false)','points[count - 1].ToString()',
 'Math.Min(count, PATH_LOG_LIMIT)','Math.Min(count, PATH_ARC_LIMIT)','raw_native_order=1 remaining_distance_proven=0')
foreach($m in $mutants){if(!@(Rules $source.Replace($m,'false') $tokens $flow).Count){throw "Escaped native diagnostic mutant $m"}}
if(!@(Rules $source $tokens.Replace('m_iDiagnosticAtMs = nowMs;','m_iLastProgressAtMs = nowMs;') $flow).Contains('NO_BEHAVIOR_CLOCK_OR_BUDGET_MUTATION')){throw 'Diagnostic sample reset behavior progress'}
$finite=(Body $source 'Finite').Replace('Math.AbsFloat','System.Math.Abs').Replace('float.MAX','float.MaxValue')
$name='AICFStagingNativeFinite'+[guid]::NewGuid().ToString('N');Add-Type "public static class $name {public static bool Check(float[] point) $finite}"
$method=([type]$name).GetMethod('Check');$cases=0
foreach($axis in 0..2){foreach($bad in @([single]::NaN,[single]::PositiveInfinity,[single]::NegativeInfinity,[single]::MaxValue)){[single[]]$point=@(1,2,3);$point[$axis]=$bad;if($method.Invoke($null,@(,$point))){throw 'Nonfinite native point escaped'};$cases++}}
foreach($point in @(@(0,0,0),@(1203.26,1.205,3266.16),@(-2000,400,-2000))){if(!$method.Invoke($null,@(,[single[]]$point))){throw 'Finite point rejected'};$cases++}
"PASS Staging native diagnostics: $($mutants.Count) source negatives +1 clock-negative +$cases actual finite cases; old cadence, bounded raw path, no progress credit"
