param([string]$RepositoryRoot=(Split-Path -Parent (Split-Path -Parent $PSScriptRoot)))
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot '../lib/Stage3StaticAudit.Common.ps1')
$source=[IO.File]::ReadAllText((Join-Path $RepositoryRoot 'tests/fixtures/AICF_RecruitmentShortRecoveryProbe.c'))
function Body($s,$method){Get-AICFMethodBody ([pscustomobject]@{Source=$s;Code=(ConvertTo-AICFCodeText $s)}) $method}
function Rules($s){
 $code=ConvertTo-AICFCodeText $s
 foreach($name in @('CURRENT_NATIVE_MOVE_OWNER','UNKNOWN_NO_CAPTURE','FOREIGN_HANDLER_NO_CAPTURE','UNRELATED_NO_CAPTURE','STALE_TOKEN_NO_MUTATION','STALE_GENERATION_NO_MUTATION','STALE_GRAPH_NO_MUTATION','FOREIGN_ORDER_NO_MUTATION','NO_SAFE_DESTINATION_NO_MUTATION','CAPTURE_BEFORE_NATIVE_CALLBACK','NATIVE_CALLBACK_ONCE','PHYSICAL_SHORT_HOP','RECEIPT_CONSUMED_ONCE','ABSOLUTE_DEADLINE_UNCHANGED','REPAIR_BUDGET_TWO','CONTINUED_NATIVE_APPROACH','REAL_ARRIVAL','PAID_RECRUIT_JOIN','VEHICLE_NO_MUTATION','INCAP_NO_MUTATION')){if(([regex]::Matches($s,[regex]::Escape('"'+$name+'"'))).Count -ne 1){'MANDATORY_CASES';break}}
 if(!$s.Contains('aicfRecruitmentShortRecoveryProbe') -or !$s.Contains('flag == "1"')){'EXPLICIT_OPT_IN'}
 $inject=Body $s 'Inject';if(!$inject.Contains('OnInit(group)') -or !$inject.Contains('AICF_HandleRecruitmentFailedMovement(result, handler, related,')){'ACTUAL_PRODUCTION_NODE_HANDLER'}
 $negative=Body $s 'Negatives';$attempt=Body $s 'NoMutation';$nativeAttempt=Body $s 'AICF_RSPNegativeAttempt'
 foreach($part in @('order.m_iToken++','order.m_iToken--','order.m_iGeneration++','order.m_iGeneration--','order.m_iGraphRevision++','order.m_iGraphRevision--','s_bRejectPlacement = true','s_bRejectPlacement = false','order.m_iApproachRepairs++','order.m_iApproachRepairs--')){if(!$negative.Contains($part)){'MEANINGFUL_RESTORED_NEGATIVE_CONTEXT';break}}
 foreach($body in @($attempt,$nativeAttempt)){if(!$body.Contains('receipt.TryRecover(order)') -or !$body.Contains('!submitted &&') -or !$body.Contains('GetOrigin() == before')){'NO_SUBMISSION_AND_NATIVE_POSITION_NEGATIVES';break}}
 if(!$s.Contains('displacement >= 2 && displacement <= 8') -or !$s.Contains('now - AICF_RecruitmentShortProbeState.s_iSubmitted >= 1000')){'ACTUAL_PHYSICAL_HOP'}
 if(!$s.Contains('s_fAfterDistance - 5') -or !$s.Contains('vector.DistanceXZ(actor.GetOrigin(), AICF_RecruitmentShortProbeState.s_vAfter) >= 5') -or !$s.Contains('order.IsPhysicallyPresent() && AICF_RecruitmentShortProbeState.s_bApproached')){'POST_HOP_NATIVE_MOTION_AND_ARRIVAL'}
 if(!$s.Contains('before == 1 && AICF_GroupRuntime.CountAliveAgents(order.m_Group) == 2') -or !$s.Contains('order.m_Base.GetSupplies() < stock')){'REAL_PAID_JOIN'}
 if(!$s.Contains('actor.IsInVehicle() && access.IsInCompartment()') -or !$s.Contains('GetLifeState() == ECharacterLifeState.INCAPACITATED') -or !$s.Contains('damage.ForceUnconsciousness(0)') -or !$s.Contains('s_NegativeReceipt = fresh')){'NATIVE_VEHICLE_AND_INCAP_STATES'}
 if(!$s.Contains('utility.GetOnMoveFailed().Insert(AICF_RecruitmentShortProbeState.Callback)') -or !$s.Contains('utility.GetOnMoveFailed().Remove(AICF_RecruitmentShortProbeState.Callback)') -or !$s.Contains('s_iCallbacks == 1') -or !(Body $s 'Callback').Contains('s_Order.m_MoveFailure.IsCapturedCurrent(s_Order)')){'REAL_CALLBACK_EXACTLY_ONCE_RECEIPT_PRESENT'}
 if(!$s.Contains('>= 240000') -or !$s.Contains('Check("TIMEOUT", false)') -or !(Body $s 'AICF_RSPFinish').Contains('Stop(true)') -or !(Body $s 'AICF_RSPClose').Contains('RequestClose()') -or !(Body $s 'Stop').Contains('Remove(AICF_RSPClose)')){'BOUNDED_STOPPED_LIFECYCLE'}
 if($code -match '\b(?:Teleport|SetOrigin|SetWorldTransform|SetHealth|SetHealthScaled|SetLifeState|SetResilience)\s*\('){'NO_SETUP_TELEPORT_HEAL_OR_REVIVE'}
}
$bad=@(Rules $source);if($bad.Count){throw ($bad -join ',')}
$mutants=@(
 @('displacement <= 8','true'),@('order.m_Base.GetSupplies() < stock','true'),
 @('s_fAfterDistance - 5','s_fAfterDistance + 5'),@('s_iCallbacks == 1','s_iCallbacks >= 0'),
 @('order.m_iToken++',''),@('order.m_iToken--',''),@('order.m_iGeneration++',''),@('order.m_iGraphRevision++',''),
 @('GetLifeState() == ECharacterLifeState.INCAPACITATED','GetLifeState() == ECharacterLifeState.ALIVE'),
 @('actor.IsInVehicle() && access.IsInCompartment()','true'),@('s_NegativeReceipt = fresh','s_NegativeReceipt = null'),
 @('>= 240000','>= 999999999'),@('Check("TIMEOUT", false)','Check("TIMEOUT", true)'),
 @('s_iCallbacks == 1','true'),@('return !submitted && actor.GetOrigin() == before','return true'),
 @('Stop(true)',''),@('GetGame().RequestClose()',''),@('damage.ForceUnconsciousness(0)','')
)
foreach($m in $mutants){if(!$source.Contains($m[0])){throw "Stale mutant $($m[0])"};if(!@(Rules $source.Replace($m[0],$m[1])).Count){throw "Escaped mutant $($m[0])"}}
"PASS Recruitment short native fixture contracts: production and $($mutants.Count) negative mutants"
