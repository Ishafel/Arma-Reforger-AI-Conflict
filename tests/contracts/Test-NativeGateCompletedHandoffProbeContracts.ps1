param([string]$RepositoryRoot=(Split-Path -Parent (Split-Path -Parent $PSScriptRoot)))
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot '../lib/Stage3StaticAudit.Common.ps1')
$fixture=[IO.File]::ReadAllText((Join-Path $RepositoryRoot 'tests/fixtures/AICF_NativeGateCompletedHandoffProbe.c'))
function Body($s,$n){Get-AICFMethodBody ([pscustomobject]@{Source=$s;Code=(ConvertTo-AICFCodeText $s)}) $n}
function Rules($s){
 $log=Body $s 'Log';$observe=Body $s 'AICF_G3ObserveCompletedPerform';$stop=Body $s 'Stop'
 foreach($guard in @('super.Log(eventName);','aicfNativeGateCompletedHandoffProbe','eventName != "TRANSPORT_NATIVE_GATE_STARTED"','m_bG3ProbeAttached','AICF_GateCrewProbe.s_Gate != this','m_Perform.m_OnActionCompleted.Insert(AICF_G3ObserveCompletedPerform);')){if(!$log.Contains($guard)){'AFTER_ACTUAL_PRODUCTION_LISTENER';break}}
 foreach($guard in @('action != m_Perform','m_bStopped','m_Trip.GetMovementState().GetNativeGate() != this','OwnsTemporaryCrewExit(m_Trip, m_Seat, m_Member)','current == m_Perform','actions.Contains(m_Return)','GetActionIsSuspended','GetRelatedGroupActivity() == m_Activity','EntityID actualMemberId = m_Member.GetID();','EntityID actualVehicleId = m_Vehicle.GetID();','NATIVE_COMPLETION_INVOKER_DIAG_ONLY','actual_controller_decision=0')){if(!$observe.Contains($guard)){'PURE_ACTUAL_COMPLETION_BOUNDARY_READBACK';break}}
 if($log.IndexOf('super.Log(eventName);')-gt$log.IndexOf('.Insert(')){'AFTER_ACTUAL_PRODUCTION_LISTENER'}
 if(!$stop.Contains('m_Perform.m_OnActionCompleted.Remove(AICF_G3ObserveCompletedPerform);')-or$stop.IndexOf('.Remove(')-gt$stop.IndexOf('super.Stop();')){'BALANCED_OBSERVER_REMOVAL'}
 if($observe-match'\b(?:Set\w*|InspectTransit|RequestClose|Mark\w*|Register\w*|Insert|CallLater|AddAction|Commit\w*)\s*\('-or$observe-match'if\s*\(\s*!?accepted'){'BOOL_ONLY_LOGGED_NO_OUTCOME'}
}
if(@(Rules $fixture).Count){throw "Completed handoff fixture: $(Rules $fixture)"}
$sourceControls=0
foreach($guard in @('super.Log(eventName);','eventName != "TRANSPORT_NATIVE_GATE_STARTED"','action != m_Perform','m_Trip.GetMovementState().GetNativeGate() != this','actions.Contains(m_Return)','EntityID actualMemberId = m_Member.GetID();','actual_controller_decision=0','m_Perform.m_OnActionCompleted.Remove(AICF_G3ObserveCompletedPerform);')){
 if(!@(Rules ($fixture.Replace($guard,'MUTANT'))).Count){throw "Source mutation escaped $guard"};$sourceControls++
}
if(!@(Rules ($fixture.Replace('bool accepted = OwnsTemporaryCrewExit(m_Trip, m_Seat, m_Member);','bool accepted = OwnsTemporaryCrewExit(m_Trip, m_Seat, m_Member); if (accepted) GetGame().RequestClose();'))).Count){throw 'Outcome mutation escaped'};$sourceControls++
. (Join-Path $RepositoryRoot 'tests/log-audits/Test-NativeGateCompletedHandoffLog.ps1') -LogPath UNUSED -EvidenceRoot UNUSED
$prior=[IO.File]::ReadAllText((Join-Path $PSScriptRoot 'Test-NativeGateCrewExitProbeContracts.ps1'))
$raw=[regex]::Match($prior,"(?s)\`$raw=@'\r?\n(.*?)\r?\n'@").Groups[1].Value
if(!$raw){throw 'Existing strict field oracle example missing'}
$boundary='[GATE_CREW_PROBE] event=COMPLETED_HANDOFF_INVOKER mode=AFTER classification=CONTROLLED_GATE_SITE_ONLY accepted=1 operation=OP observed_ms=9000 window_start=1000 window_deadline=61000 exact_current_perform=1 current=SCR_AIPerformActionBehavior:COMPLETED current_removed=1 exact_return_queued=1 native_return=SCR_AIGetInVehicle:EVALUATED return_live=1 return_suspended=0 return_related_exact=1 observed_exit=1 member=A member_rpl=AR vehicle=V vehicle_rpl=VR actor_health=1 vehicle_health=1 current_deadline=100000 attempts=0 original_seat_empty=1 observer_scope=NATIVE_COMPLETION_INVOKER_DIAG_ONLY actual_controller_decision=0 successful_native_completion_invoker=1'
$receipt='[TRANSPORT_NATIVE_GATE_PERFORM_COMPLETED] operation_id=OP perform_completed_receipt=1 perform_listener_attached=1 perform_completed_at_ms=8000'
$insertion='[GATE_CREW_PROBE] event=NATIVE_COMPLETED_RETURN'
function Sample($b,$p){return $raw.Replace($insertion,($p+"`n"+$b+"`n"+$insertion))}
$good=Sample $boundary $receipt
if(@(Test-AICFCompletedHandoffProof $good $true).Count){throw "Positive handoff rejected: $(Test-AICFCompletedHandoffProof $good $true)"}
$logCases=1
foreach($pair in @(@('accepted=1','accepted=0'),@('exact_current_perform=1','exact_current_perform=0'),@('current=SCR_AIPerformActionBehavior:COMPLETED','current=SCR_AIPerformActionBehavior:EVALUATED'),
 @('current_removed=1','current_removed=0'),@('exact_return_queued=1','exact_return_queued=0'),@('return_live=1','return_live=0'),@('return_suspended=0','return_suspended=1'),@('return_related_exact=1','return_related_exact=0'),
 @('native_return=SCR_AIGetInVehicle:EVALUATED','native_return=SCR_AIIdleBehavior:RUNNING'),@('observed_ms=9000','observed_ms=61000'),@('window_deadline=61000','window_deadline=62000'),
 @('member=A','member=FOREIGN'),@('member_rpl=AR','member_rpl=NONE'),@('vehicle=V','vehicle=FOREIGN'),@('vehicle_rpl=VR','vehicle_rpl=NONE'),@('actor_health=1','actor_health=0'),@('vehicle_health=1','vehicle_health=0'),
 @('original_seat_empty=1','original_seat_empty=0'),@('current_deadline=100000','current_deadline=100001'),@('attempts=0','attempts=1'),@('actual_controller_decision=0','actual_controller_decision=1'),@('successful_native_completion_invoker=1','successful_native_completion_invoker=0'))){
 if(!@(Test-AICFCompletedHandoffProof (Sample ($boundary.Replace($pair[0],$pair[1])) $receipt) $true).Count){throw "Boundary log mutation escaped $($pair[0])"};$logCases++
}
foreach($p in @('',($receipt+"`n"+$receipt),$receipt.Replace('operation_id=OP','operation_id=FOREIGN'),$receipt.Replace('perform_completed_at_ms=8000','perform_completed_at_ms=62000'))){if(!@(Test-AICFCompletedHandoffProof (Sample $boundary $p) $true).Count){throw 'Invalid native receipt escaped'};$logCases++}
foreach($extra in @('[ACCEPTANCE_FAILURE_LATCHED] operation_id=FOREIGN reason=OLD_FAILURE',"SCRIPT (E): Unexpected error")) {if(!@(Test-AICFCompletedHandoffProof ($good+"`n"+$extra) $true).Count){throw 'Previous/fatal error masked'};$logCases++}
$before=Sample ($boundary.Replace('accepted=1','accepted=0')) ''
if(@(Test-AICFCompletedHandoffProof $before $false).Count){throw 'Actual G2 boundary rejection rejected'};$logCases++
if(!@(Test-AICFCompletedHandoffProof ($before.Replace('accepted=0','accepted=1')) $false).Count){throw 'Before unknown positive accepted'};$logCases++
if(!@(Test-AICFCompletedHandoffProof ($good+"`n"+$boundary) $true).Count){throw 'Duplicate boundary accepted'};$logCases++
$audit=Join-Path $RepositoryRoot 'tests/log-audits/Test-NativeGateCompletedHandoffLog.ps1'
$temp=Join-Path ([IO.Path]::GetTempPath()) ('AICF G3 Audit '+[guid]::NewGuid().ToString('N'))
[void][IO.Directory]::CreateDirectory($temp)
try{
 foreach($mode in @('BEFORE','AFTER')){
  $rawForMode=$good;if($mode-eq'BEFORE'){$rawForMode=$before}
  [IO.File]::WriteAllText((Join-Path $temp 'full console.log'),$rawForMode)
  [ordered]@{proof='PASS';fatal='PASS';resources='PASS'}|ConvertTo-Json|Set-Content (Join-Path $temp 'gate-crew-audit.json')
  & powershell -NoProfile -ExecutionPolicy Bypass -File $audit -LogPath (Join-Path $temp 'full console.log') -EvidenceRoot $temp -Side $mode -FieldAuditScriptPath (Join-Path $RepositoryRoot 'tests/log-audits/Test-NativeGateCrewExitLog.ps1') *> (Join-Path $temp 'entrypoint.txt')
  if($LASTEXITCODE-ne0){throw "Actual auditor entrypoint failed: $(Get-Content (Join-Path $temp 'entrypoint.txt') -Raw)"}
  $receipt=Get-Content (Join-Path $temp 'completed-handoff-audit.json') -Raw|ConvertFrom-Json
  if($receipt.boundary_proof-ne'PASS'-or$receipt.side-ne$mode){throw 'Actual entrypoint lost caller input paths or side'}
 }
 foreach($bad in @('proof','fatal','resources')){
  $field=@{proof='PASS';fatal='PASS';resources='PASS'};$field[$bad]='FAIL'
  $field|ConvertTo-Json|Set-Content (Join-Path $temp 'gate-crew-audit.json')
  & powershell -NoProfile -ExecutionPolicy Bypass -File $audit -LogPath (Join-Path $temp 'full console.log') -EvidenceRoot $temp -Side AFTER -FieldAuditScriptPath (Join-Path $RepositoryRoot 'tests/log-audits/Test-NativeGateCrewExitLog.ps1') *> (Join-Path $temp 'entrypoint.txt')
  if($LASTEXITCODE-eq0){throw "Actual entrypoint masked field $bad FAIL"}
 }
 foreach($missing in @('log','base')){
  if(Test-Path (Join-Path $temp 'completed-handoff-audit.json')){Remove-Item -LiteralPath (Join-Path $temp 'completed-handoff-audit.json') -Force}
  $inputLog=Join-Path $temp 'full console.log';$inputBase=Join-Path $RepositoryRoot 'tests/log-audits/Test-NativeGateCrewExitLog.ps1'
  if($missing-eq'log'){$inputLog=Join-Path $temp 'missing.log'}else{$inputBase=Join-Path $temp 'missing-base.ps1'}
  $priorErrors=$ErrorActionPreference
  try{$ErrorActionPreference='Continue'; & powershell -NoProfile -ExecutionPolicy Bypass -File $audit -LogPath $inputLog -EvidenceRoot $temp -Side AFTER -FieldAuditScriptPath $inputBase *> (Join-Path $temp 'entrypoint.txt')}
  finally{$ErrorActionPreference=$priorErrors}
  if($LASTEXITCODE-eq0-or(Test-Path (Join-Path $temp 'completed-handoff-audit.json'))){throw "Actual entrypoint accepted missing $missing"}
 }
}finally{
 $resolved=(Resolve-Path -LiteralPath $temp).Path
 $expected=[IO.Path]::GetFullPath($temp)
 if($resolved-ne$expected-or(Get-Item -LiteralPath $resolved).Attributes-band[IO.FileAttributes]::ReparsePoint){throw 'Unexpected temporary test cleanup target'}
 Remove-Item -LiteralPath $resolved -Recurse -Force
}
"PASS completed handoff probe: source_negative_controls=$sourceControls actual_log_cases=$logCases actual_ps51_entrypoints=7 native=NOT_RUN"
