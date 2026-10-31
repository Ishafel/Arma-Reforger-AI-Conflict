param([string]$RepositoryRoot=(Split-Path -Parent (Split-Path -Parent $PSScriptRoot)))
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot '../lib/Stage3StaticAudit.Common.ps1')
$root=Join-Path $RepositoryRoot 'AIConflictCore/Scripts/Game/AIConflict'
$controller=[IO.File]::ReadAllText((Join-Path $root 'Vehicles/AICF_TransportTripController.c'))
$handoff=[IO.File]::ReadAllText((Join-Path $root 'Vehicles/AICF_VehicleTaskHandoff.c'))
$slot=[IO.File]::ReadAllText((Join-Path $root 'State/AICF_GroupSlot.c'))
$owner=[IO.File]::ReadAllText((Join-Path $root 'Bootstrap/AICF_MatchController.c'))
function Body([string]$text,[string]$name){Get-AICFMethodBody ([pscustomobject]@{Source=$text;Code=(ConvertTo-AICFCodeText $text)}) $name}
function TacticalBranch([string]$text){[regex]::Match((Body $text 'DispatchCurrentPhase'),'(?s)case AICF_ETransportTripPhase.TACTICAL_DEPLOYMENT:(.*?)case AICF_ETransportTripPhase.DISMOUNT:').Groups[1].Value}
function Rules([string]$c,[string]$h,[string]$s,[string]$o){
 $tactical=TacticalBranch $c
 $report=Body $h 'ReportRestoredInfantryOrder';$current=Body $s 'IsPendingOrderRecoveryContextCurrent'
 $account=Body $o 'AuditOrderRepairAccounting';$terminal=Body $o 'RecordPendingOrderRepairTerminal'
 if(!$tactical -or !$report -or !$current -or !$account -or !$terminal){'METHODS';return}
 if($tactical -notmatch 'if \(slot.HasPendingOrderRecovery\(\) && slot.PendingOrderRecoveryCountsAsReliabilityAttempt\(\)\)\s*return AICF_TripOutcome.Wait\("TACTICAL_INFANTRY_VERIFICATION_PENDING", causationId\);' -or
  $tactical.IndexOf('slot.HasPendingOrderRecovery') -gt $tactical.IndexOf('m_BehaviorFlow.Tick') -or
  $tactical -match 'ApplyOutcome\(|SuspendInfantryOrder\(|RestoreInfantryOrder\(|ClearPendingOrderRecovery\('){'PENDING_REPAIR_WAITS_BEFORE_ANY_TACTICAL_MUTATION'}
 if($report -notmatch 'if \(!slot.IsSystemHoldOrder\(\) && !slot.IsPendingOrderRecoveryContextCurrent\(\)\)\s*slot.BeginOrderRecoveryVerification\("VEHICLE_HANDOFF"\);' -or
  ([regex]::Matches($report,'BeginOrderRecoveryVerification\(')).Count -ne 1 -or
  (ConvertTo-AICFCodeText $report) -match 'ClearPendingOrderRecovery|MarkPendingOrderRecoveryAsReliabilityAttempt|SetWaypoint|SuspendObjectiveWaypoint|RecordPendingOrderRepairTerminal'){'PROOF_ONLY_HANDOFF_PRESERVES_CURRENT_CANDIDATE'}
 foreach($predicate in @('HasPendingOrderRecovery()','m_Group == m_PendingOrderRecoveryGroup',
  'm_TargetKind == m_PendingOrderRecoveryTargetKind','m_TargetBase == m_PendingOrderRecoveryTargetBase',
  'm_vTargetPosition == m_vPendingOrderRecoveryTargetPosition','m_Waypoint == m_PendingOrderRecoveryWaypoint',
  'm_iSpawnGeneration == m_iPendingOrderRecoveryGroupGeneration','m_iStrategicAssignmentRevision == m_iPendingOrderRecoveryAssignmentRevision')){
  if(!$current.Contains($predicate)){'EXACT_EXISTING_CANDIDATE_IDENTITY';break}
 }
 if($account -notmatch 'm_iOrderRecoveryAttempts - terminalCount - pendingCount' -or
  $account -notmatch 'ORDER_REPAIR_ACCOUNTING_INVARIANT_FAILED' -or
  $terminal -notmatch 'outcome == "CONFIRMED"' -or $terminal -notmatch 'outcome == "FAILED"' -or
  $terminal -notmatch 'outcome == "SUPERSEDED"' -or
  (ConvertTo-AICFCodeText ($tactical+$report)) -match 'm_iOrderRecover(?:yAttempts|ies|yFailures|ySuperseded)\s*(?:\+\+|--|[+]?=)|"CONFIRMED"'){'COUNTERS_AND_TERMINALS_STAY_WITH_RELIABILITY_OWNER'}
}
$failures=@(Rules $controller $handoff $slot $owner)
if($failures.Count){throw "Tactical repair ownership: $failures"}
$controls=1
foreach($mutation in @(
 @('c','slot.HasPendingOrderRecovery() && slot.PendingOrderRecoveryCountsAsReliabilityAttempt()','slot.HasPendingOrderRecovery()','PENDING_REPAIR_WAITS_BEFORE_ANY_TACTICAL_MUTATION'),
 @('c','return AICF_TripOutcome.Wait("TACTICAL_INFANTRY_VERIFICATION_PENDING", causationId);','outcome = AICF_TripOutcome.Wait("TACTICAL_INFANTRY_VERIFICATION_PENDING", causationId);','PENDING_REPAIR_WAITS_BEFORE_ANY_TACTICAL_MUTATION'),
 @('h','!slot.IsSystemHoldOrder() && !slot.IsPendingOrderRecoveryContextCurrent()','!slot.IsSystemHoldOrder()','PROOF_ONLY_HANDOFF_PRESERVES_CURRENT_CANDIDATE'),
 @('h','!slot.IsSystemHoldOrder() && !slot.IsPendingOrderRecoveryContextCurrent()','!slot.IsPendingOrderRecoveryContextCurrent()','PROOF_ONLY_HANDOFF_PRESERVES_CURRENT_CANDIDATE'),
 @('s','m_Waypoint == m_PendingOrderRecoveryWaypoint','true','EXACT_EXISTING_CANDIDATE_IDENTITY'),
 @('s','m_iSpawnGeneration == m_iPendingOrderRecoveryGroupGeneration','true','EXACT_EXISTING_CANDIDATE_IDENTITY'),
 @('s','m_iStrategicAssignmentRevision == m_iPendingOrderRecoveryAssignmentRevision','true','EXACT_EXISTING_CANDIDATE_IDENTITY'),
 @('o','m_iOrderRecoveryAttempts - terminalCount - pendingCount','0','COUNTERS_AND_TERMINALS_STAY_WITH_RELIABILITY_OWNER')
)){
 $c=$controller;$h=$handoff;$s=$slot;$o=$owner
 $original=@{c=$c;h=$h;s=$s;o=$o}[$mutation[0]]
 if(!$original.Contains($mutation[1])){throw "Mutation target absent: $($mutation[3])"}
 $changed=$original.Replace($mutation[1],$mutation[2])
 switch($mutation[0]){'c'{$c=$changed}'h'{$h=$changed}'s'{$s=$changed}'o'{$o=$changed}}
 if($mutation[3] -notin @(Rules $c $h $s $o)){throw "Mutation survived: $($mutation[3])"}
 $controls++
}
# Исполняем production guards, в том числе сохранение исходного candidate clock
# при повторном Handoff proof независимо от его current verification owner.
$expression=[regex]::Match((TacticalBranch $controller),'if \((slot.HasPendingOrderRecovery\(\) && slot.PendingOrderRecoveryCountsAsReliabilityAttempt\(\))\)').Groups[1].Value.
 Replace('slot.HasPendingOrderRecovery()','$pending').Replace('slot.PendingOrderRecoveryCountsAsReliabilityAttempt()','$counted').Replace('&&','-and')
$wait=[scriptblock]::Create('param($pending,$counted) '+$expression)
$expression=[regex]::Match((Body $handoff 'ReportRestoredInfantryOrder'),'if \((!slot.IsSystemHoldOrder\(\) && !slot.IsPendingOrderRecoveryContextCurrent\(\))\)').Groups[1].Value.
 Replace('!slot.IsSystemHoldOrder()','(-not $systemHold)').Replace('!slot.IsPendingOrderRecoveryContextCurrent()','(-not $current)').Replace('&&','-and')
$begin=[scriptblock]::Create('param($systemHold,$current) '+$expression)
foreach($pending in @($false,$true)){foreach($counted in @($false,$true)){
 if([bool](& $wait $pending $counted) -ne ($pending -and $counted)){throw 'Tactical counted-owner wait truth table failed'}
 $controls++
}}
foreach($systemHold in @($false,$true)){foreach($current in @($false,$true)){
 $started=if([bool](& $begin $systemHold $current)){9999}else{1234}
 if($current -and $started -ne 1234){throw 'Current candidate clock overwritten'}
 if($systemHold -and [bool](& $begin $systemHold $current)){throw 'SYSTEM_HOLD created generic durability candidate'}
 if(!$systemHold -and !$current -and $started -ne 9999){throw 'Missing/replaced candidate failed to start new handoff verification'}
 $controls++
}}
$expression=[regex]::Match((Body $slot 'IsPendingOrderRecoveryContextCurrent'),'(?s)return\s+(.*?);').Groups[1].Value
$predicates=@('HasPendingOrderRecovery()','m_Group == m_PendingOrderRecoveryGroup','m_TargetKind == m_PendingOrderRecoveryTargetKind','m_TargetBase == m_PendingOrderRecoveryTargetBase','m_vTargetPosition == m_vPendingOrderRecoveryTargetPosition','m_Waypoint == m_PendingOrderRecoveryWaypoint','m_iSpawnGeneration == m_iPendingOrderRecoveryGroupGeneration','m_iStrategicAssignmentRevision == m_iPendingOrderRecoveryAssignmentRevision')
for($index=0;$index -lt $predicates.Count;$index++){$expression=$expression.Replace($predicates[$index],('$flags['+$index+']'))}
$current=[scriptblock]::Create('param($flags) '+$expression.Replace('&&','-and'))
for($changed=-1;$changed -lt $predicates.Count;$changed++){
 $flags=@($true,$true,$true,$true,$true,$true,$true,$true);if($changed -ge 0){$flags[$changed]=$false}
 if([bool](& $current $flags) -ne ($changed -lt 0)){throw "Candidate identity mutation accepted: $changed"}
 $controls++
}
"PASS Tactical repair ownership: counted reliability candidate owns its terminal receipt; proof-only handoff preserves current clock; context replacement and SYSTEM_HOLD remain separate; controls=$controls"
