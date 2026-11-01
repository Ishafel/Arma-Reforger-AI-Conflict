param([Parameter(Mandatory)][string]$LogPath,[Parameter(Mandatory)][string]$EvidenceRoot,[ValidateSet('BEFORE','AFTER')][string]$Side='AFTER',[string]$FieldAuditScriptPath='')
$ErrorActionPreference='Stop'
if(!$FieldAuditScriptPath){$FieldAuditScriptPath=Join-Path (Split-Path -Parent $MyInvocation.MyCommand.Path) 'Test-NativeGateCrewExitLog.ps1'}
$handoffInputLogPath=$LogPath
$handoffInputEvidenceRoot=$EvidenceRoot
. $FieldAuditScriptPath -LogPath 'UNUSED' -LauncherPath 'UNUSED' -NativeExitPath 'UNUSED' -EvidenceRoot 'UNUSED'
$LogPath=$handoffInputLogPath
$EvidenceRoot=$handoffInputEvidenceRoot
function Test-AICFCompletedHandoffProof([string]$Raw,[bool]$ExpectedAccepted){
 $issues=New-Object 'System.Collections.Generic.List[string]'
 function Need([bool]$ok,[string]$why){if(!$ok-and!$issues.Contains($why)){$issues.Add($why)}}
 function Fields([string]$line){$f=@{};foreach($m in [regex]::Matches($line,'(?:^|\s)(\w+)=(.*?)(?=\s+\w+=|$)')){$f[$m.Groups[1].Value]=$m.Groups[2].Value.Trim()};return $f}
 $events=@([regex]::Matches($Raw,'\[GATE_CREW_PROBE\]([^\r\n]*)')|ForEach-Object{[pscustomobject]@{index=$_.Index;f=(Fields $_.Groups[1].Value)}})
 $boundary=@($events|Where-Object{$_.f.event-eq'COMPLETED_HANDOFF_INVOKER'});$chain=@($events|Where-Object{$_.f.event-eq'NATIVE_PILOT_CHAIN'});$ready=@($events|Where-Object{$_.f.event-eq'ACTUAL_READY_CLOSED'});$done=@($events|Where-Object{$_.f.event-eq'DONE'})
 Need ($boundary.Count-eq1-and$chain.Count-eq1-and$ready.Count-eq1-and$done.Count-eq1) 'ONE_REAL_NATIVE_COMPLETION_BOUNDARY'
 if($boundary.Count-ne1-or$chain.Count-ne1-or$ready.Count-ne1-or$done.Count-ne1){return $issues.ToArray()}
 $b=$boundary[0].f;$c=$chain[0].f;$r=$ready[0].f;$d=$done[0].f
 Need ($Raw.Contains('[ROSTER_READY]')-and$Raw.Contains('Game destroyed.')) 'STOPPED_REAL_ROSTER'
 Need ($Raw-notmatch'SCRIPT\s+\([EF]\)|ENGINE\s+\(F\)|NULL pointer|Virtual Machine Exception|Assertion failed|Access violation') 'NO_FULL_NATIVE_FATAL'
 Need ($b.mode-eq'AFTER'-and$b.classification-eq'CONTROLLED_GATE_SITE_ONLY'-and$b.observer_scope-eq'NATIVE_COMPLETION_INVOKER_DIAG_ONLY'-and$b.actual_controller_decision-eq'0'-and$b.successful_native_completion_invoker-eq'1') 'READONLY_ACTUAL_INVOKER_SCOPE'
 $wanted='0';if($ExpectedAccepted){$wanted='1'}
 Need ($b.accepted-eq$wanted) 'ACTUAL_PRODUCTION_OWNS_RESULT'
 Need ($chain[0].index-lt$boundary[0].index-and$boundary[0].index-lt$done[0].index) 'ACTUAL_BOUNDARY_CAUSAL_ORDER'
 Need ($b.operation-eq$c.operation-and$b.window_start-eq$c.window_start-and$b.window_deadline-eq$c.window_deadline-and[int]$c.window_start-gt0-and[int]$b.observed_ms-ge[int]$c.window_start-and[int]$b.observed_ms-lt[int]$c.window_deadline-and[int]$c.window_deadline-le[int]$c.window_start+60000) 'ORIGINAL_UNEXPIRED_NATIVE_WINDOW'
 Need ($r.actors-eq'1'-and$r.fake_ready-eq'0'-and$r.state-eq'0'-and$r.control-eq'0'-and$c.actual_original_pilot-eq'1'-and$c.closed_setup-eq'1'-and$ready[0].index-lt$chain[0].index) 'ACTUAL_ORIGINAL_PILOT_CLOSED_DOOR_SETUP'
 Need ($b.exact_current_perform-eq'1'-and$b.current-eq'SCR_AIPerformActionBehavior:COMPLETED'-and$b.current_removed-eq'1'-and$b.observed_exit-eq'1') 'ACTUAL_CACHED_COMPLETED_PERFORM'
 Need ($b.exact_return_queued-eq'1'-and$b.return_live-eq'1'-and$b.return_suspended-eq'0'-and$b.return_related_exact-eq'1'-and$b.native_return-match'^SCR_AIGetInVehicle:(EVALUATED|RUNNING)$') 'EXACT_QUEUED_LIVE_RETURN'
 Need ($b.member-eq$r.actor-and$b.member_rpl-eq$r.actor_rpl-and$b.member_rpl-and$b.member_rpl-ne'NONE'-and$b.vehicle-eq$c.vehicle-and$b.vehicle_rpl-eq$c.vehicle_rpl-and$b.vehicle_rpl-and$b.vehicle_rpl-ne'NONE') 'ORIGINAL_FRESH_ENTITY_RPL'
 Need ($b.actor_health-eq'1'-and$b.vehicle_health-eq'1'-and$b.original_seat_empty-eq'1'-and$b.current_deadline-eq$d.deadline-and$b.attempts-eq$d.captured_attempts) 'HEALTHY_EMPTY_SEAT_AND_OLD_DEADLINE_BUDGET'
 $receipts=@([regex]::Matches($Raw,'\[TRANSPORT_NATIVE_GATE_PERFORM_COMPLETED\]([^\r\n]*)')|ForEach-Object{[pscustomobject]@{index=$_.Index;f=(Fields $_.Groups[1].Value)}})
 if($ExpectedAccepted){
  Need ($receipts.Count-eq1) 'ONE_ACTUAL_PRODUCTION_COMPLETION_RECEIPT'
  if($receipts.Count-eq1){$p=$receipts[0].f;Need ($receipts[0].index-lt$boundary[0].index-and$p.operation_id-eq$c.operation-and$p.perform_completed_receipt-eq'1'-and$p.perform_listener_attached-eq'1'-and[int]$p.perform_completed_at_ms-ge[int]$c.window_start-and[int]$p.perform_completed_at_ms-le[int]$b.observed_ms-and[int]$p.perform_completed_at_ms-lt[int]$c.window_deadline) 'REAL_RECEIPT_PRECEDES_OBSERVER'}
  foreach($fieldIssue in @(Test-AICFGateCrewProof $Raw 'AFTER')){Need $false ('FIELD_'+$fieldIssue)}
 }else{Need ($receipts.Count-eq0) 'G2_PREDECESSOR_HAS_NO_COMPLETION_RECEIPT'}
 return $issues.ToArray()
}
if($MyInvocation.InvocationName-eq'.'){return}
$raw=[IO.File]::ReadAllText((Resolve-Path $LogPath).Path)
$issues=@(Test-AICFCompletedHandoffProof $raw ($Side-eq'AFTER'))
$field=Get-Content (Join-Path $EvidenceRoot 'gate-crew-audit.json') -Raw|ConvertFrom-Json
$proof='PASS';if($issues.Count){$proof='FAIL'}
[ordered]@{side=$Side;boundary_proof=$proof;scope='COMPLETION_HELPER_BOUNDARY_ONLY';actual_controller_decision=$false;field_proof=$field.proof;fatal=$field.fatal;resources=$field.resources;issues=$issues;raw_field_audit='gate-crew-audit.json';original_trip_completion_proven=$false}|ConvertTo-Json -Depth 5|Set-Content (Join-Path $EvidenceRoot 'completed-handoff-audit.json')
"G3_HANDOFF side=$Side boundary=$proof field=$($field.proof) fatal=$($field.fatal) resources=$($field.resources)"
if($issues.Count-or$field.proof-ne'PASS'-or$field.fatal-ne'PASS'-or$field.resources-ne'PASS'){exit 1};exit 0
