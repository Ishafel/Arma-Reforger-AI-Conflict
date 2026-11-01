param([string]$RepositoryRoot=(Split-Path -Parent (Split-Path -Parent $PSScriptRoot)))
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot '../lib/Stage3StaticAudit.Common.ps1')
$core=Join-Path $RepositoryRoot 'AIConflictCore/Scripts/Game/AIConflict'
$flow=[IO.File]::ReadAllText((Join-Path $core 'Vehicles/AICF_VehicleDismountFlow.c'))
$phases=[IO.File]::ReadAllText((Join-Path $core 'State/Vehicles/AICF_VehiclePhaseStates.c'))
$state=[regex]::Match($phases,'(?s)class AICF_VehicleDismountState\b.*?(?=\r?\nclass |\z)').Value
$fixture=[IO.File]::ReadAllText((Join-Path $RepositoryRoot 'tests/fixtures/AICF_DismountOperationBudgetProbe.c'))
$runner=[IO.File]::ReadAllText((Join-Path $RepositoryRoot 'tools/Invoke-AICFVehicleScenarios.ps1'))
function Body([string]$text,[string]$method){Get-AICFMethodBody ([pscustomobject]@{Source=$text;Code=(ConvertTo-AICFCodeText $text)}) $method}
function Rules([string]$f,[string]$s){
 $relocate=Body $f 'TryRelocateExactManagedMembers';$force=Body $f 'TryForceExactManagedExit';$terminal=Body $f 'ProcessTerminalClearance'
 if(!$terminal){$terminal=Body $f 'ProcessTerminalDismount'}
 if(!$terminal){$terminal=Body $f 'ProcessTerminal'}
 $physical=Body $s 'RecordPhysicalRelocationAttempt';$logical=Body $s 'RecordForceClearanceAttempt'
 if(!$relocate -or !$physical -or !$logical){'METHODS';return}
 foreach($method in @('Begin','BeginTerminal')){
  $begin=Body $s $method
  if(!$begin.Contains('m_iMaximumForceClearanceAttempts = Math.Max(0, forceBudget);') -or !$begin.Contains('m_iMaximumPhysicalRelocationAttempts = Math.Max(0, forceBudget);')){'TWO_FINITE_PHASE_BUDGETS'}
 }
 if(!(Body $s 'Reset').Contains('m_iPhysicalRelocationAttempts = 0;') -or !(Body $s 'Reset').Contains('m_iMaximumPhysicalRelocationAttempts = 0;')){'PHASE_RESET_ONLY'}
 if(!$physical.Contains('m_iLogicalOccupants > 0 || m_iTransitions > 0 || m_iInsideBounds <= 0') -or !$physical.Contains('m_iPhysicalRelocationAttempts >= m_iMaximumPhysicalRelocationAttempts') -or !$physical.Contains('m_iPhysicalRelocationAttempts++;')){'PHYSICAL_BUDGET_REQUIRES_EGRESS'}
 if(!$logical.Contains('m_iForceClearanceAttempts >= m_iMaximumForceClearanceAttempts') -or !$logical.Contains('m_iForceClearanceAttempts++;') -or ($physical+$logical) -match 'Reset\(|Begin\(|Deadline\w*\s*='){'NO_POLLING_REARM'}
 if(!(Body $s 'GetForceClearanceAttempts').Contains('m_iForceClearanceAttempts + m_iPhysicalRelocationAttempts') -or !(Body $s 'GetForceExitAttempts').Contains('return m_iForceClearanceAttempts;')){'AGGREGATE_TELEMETRY_DISTINCT_OWNER'}
 if(!$relocate.Contains('state.GetPhysicalRelocationAttempts() >= FORCE_MAX_ATTEMPTS') -or $relocate.Contains('state.GetForceClearanceAttempts() >=') -or $relocate.Contains('state.RecordForceClearanceAttempt()')){'RELOCATION_INDEPENDENT_OF_EXIT_BUDGET'}
 foreach($guard in @('!sample || sample.m_iLogicalOccupants > 0 || sample.m_iTransitions > 0 || sample.m_iInsideBounds <= 0',
  'AICF_DismountClearanceSample current = InspectCurrentManagedClearance(trip);',
  '!current || current.m_iLogicalOccupants > 0 || current.m_iTransitions > 0 || current.m_iInsideBounds <= 0',
  'state.RecordClearanceSample(current.m_iLogicalOccupants, current.m_iTransitions,',
  'current.m_iInsideBounds, commitNowMs, current.m_iPostEgressBlocked);')){if(!$relocate.Contains($guard)){'FRESH_WHOLE_GROUP_EGRESS_BEFORE_CHARGE';break}}
 $charge=$relocate.IndexOf('if (!state.RecordPhysicalRelocationAttempt())')
 if($charge -lt $relocate.IndexOf('if (candidates.IsEmpty())') -or $charge -lt $relocate.IndexOf('InspectCurrentManagedClearance(trip)') -or $charge -gt $relocate.IndexOf('m_Watchdog.ResetGroupVehicleActions(group)') -or $charge -gt $relocate.IndexOf('character.Teleport(transform)')){'NONEMPTY_PROBE_BEFORE_REAL_ATTEMPT'}
 foreach($guard in @('!IsAuthoritativeTripAssetCurrent(trip) || trip.GetAssignment().GetGroup() != group || trip.GetLease().GetVehicle() != vehicle',
  'commitNowMs >= state.GetTerminalDeadlineMs() || trip.IsDeadlineReached(commitNowMs)',
  '!IsExactAliveCurrentMember(group, agent)', '!AICF_VehicleBoardingMutationFence.IsAuthoritativeAIEntity(character)',
  '!IsPhysicalOnlyBlocker(character, vehicle, boundsMin, boundsMax)',
  'ChimeraWorldUtils.TryGetWaterSurfaceSimple(vehicle.GetWorld(), safePosition)',
  'CanApplyTerminalHiddenRecovery(')) {if(!$relocate.Contains($guard)){'REPEATED_IDENTITY_DISTANCE_NAV_DEADLINE_BOUNDARIES';break}}
 if(!$force.Contains('state.RecordForceClearanceAttempt()') -or $force.Contains('RecordPhysicalRelocationAttempt') -or !$force.Contains('state.GetForceExitAttempts()')){'LOGICAL_FORCE_OWNS_ONLY_EXIT_BUDGET'}
 if(!$terminal.Contains('activeBudgetExhausted = state.GetForceExitAttempts() >= FORCE_MAX_ATTEMPTS;') -or !$terminal.Contains('activeBudgetExhausted = state.GetPhysicalRelocationAttempts() >= FORCE_MAX_ATTEMPTS;') -or $terminal.Contains('state.GetForceClearanceAttempts() >= FORCE_MAX_ATTEMPTS')){'TERMINAL_ESCALATES_CURRENT_OPERATION_ONLY'}
 if($relocate -match '(?:Reset|BeginTerminal|SetAbsoluteDeadline)\(' -or $relocate -match '(?:Deadline\w*|m_i\w*Attempts)\s*(?:=|\+=)' -or !$f.Contains('FORCE_MAX_ATTEMPTS = 3;')){'NO_RETRY_OR_DEADLINE_EXPANSION'}
 if(!(Body $f 'FormatClearance').Contains('maximum_combined_batches_per_phase=%5')){'EXPLICIT_NEW_COMBINED_BOUND'}
}
$issues=@(Rules $flow $state);if($issues.Count){throw "Dismount operation budgets: $issues"}
$controls=1
function FixtureRules([string]$probe,[string]$production,[string]$launch){
 if($production.Contains('aicfVehicleScenarioDismountBudget') -or !$launch.Contains('if($DismountBudgetProbe){Copy-Item') -or !$launch.Contains('DismountBudgetProbe requires isolated non-combat ural5 only')){'ISOLATED_OPT_IN_ONLY'}
 if((ConvertTo-AICFCodeText $probe) -match 'RecordForceClearanceAttempt\(|RecordPhysicalRelocationAttempt\(|m_iForceClearanceAttempts\s*=|m_iPhysicalRelocationAttempts\s*='){'NO_DIRECT_STATE_COUNTER_PRIMING'}
 foreach($proof in @('batch < 3 && state.GetForceExitAttempts() < 3','super.TryForceExactManagedExit(trip, before);',
  'charged == 1 && nativeCalls > 0 && probe.m_iAcceptedCalls > oldAccepted','bool accepted = super.ForceOneManagedMemberOut(trip, character, forceAttempt);',
  'probe.m_iProvenBatches == 3 && !probe.m_bGhostInjected && egress.m_iLogicalOccupants == 0 && egress.m_iTransitions == 0',
  'state.GetNormalDeadlineMs() == m_iNormalDeadlineMs && state.GetTerminalDeadlineMs() == m_iTerminalDeadlineMs',
  'probe.m_bGhostInjected && !terminalOnly','super.TryRelocateExactManagedMembers(trip, sample, terminalOnly);',
  'displacement > 1 && !IsPhysicalOnlyBlocker','AICF_VSCheck("DISMOUNT_OPERATION_BUDGET_CAUSAL", probe && probe.Passed());',
  'AICF_VSDismountBudgetProbe.s_Current = null;')){if(!$probe.Contains($proof)){'ACTUAL_NATIVE_CAUSAL_BRANCH_ORACLE';break}}
 foreach($proof in @('trip != m_Trip','trip.GetGroupGeneration() != m_iGroupGeneration','trip.GetTripGeneration() != m_iTripGeneration',
  'm_Group.GetID() != m_GroupId','m_Vehicle.GetID() != m_VehicleId','Replication.FindItemId(m_Vehicle).ToString() != m_sVehicleRpl',
  'trip.GetLease().GetLeaseGeneration() != m_iLeaseGeneration','trip.GetLease().GetVehicleGeneration() != m_iVehicleGeneration',
  'member.GetID() != m_aMemberIds[position]','AICF_VehicleBoardingMutationFence.IsAuthoritativeAIEntity(member)')){if(!$probe.Contains($proof)){'EXACT_WEAK_NATIVE_PROBE_SCOPE';break}}
}
$issues=@(FixtureRules $fixture $flow $runner);if($issues.Count){throw "Dismount budget fixture: $issues"};$controls++
foreach($mutation in @(
 @('charged == 1 && nativeCalls > 0 && probe.m_iAcceptedCalls > oldAccepted','charged == 1 && nativeCalls > 0','ACTUAL_NATIVE_CAUSAL_BRANCH_ORACLE'),
 @('egress.m_iLogicalOccupants == 0 && egress.m_iTransitions == 0','true','ACTUAL_NATIVE_CAUSAL_BRANCH_ORACLE'),
 @('member.GetID() != m_aMemberIds[position]','false','EXACT_WEAK_NATIVE_PROBE_SCOPE'),
 @('trip.GetLease().GetLeaseGeneration() != m_iLeaseGeneration','false','EXACT_WEAK_NATIVE_PROBE_SCOPE'),
 @('AICF_VSDismountBudgetProbe.s_Current = null;','state.RecordForceClearanceAttempt();','NO_DIRECT_STATE_COUNTER_PRIMING')
)){
 $mutated=$fixture.Replace($mutation[0],$mutation[1])
 if($mutated -ceq $fixture -or $mutation[2] -notin @(FixtureRules $mutated $flow $runner)){throw "Fixture mutation survived: $($mutation[2])"};$controls++
}
foreach($mutation in @(
 @('f','state.GetPhysicalRelocationAttempts() >= FORCE_MAX_ATTEMPTS','state.GetForceClearanceAttempts() >= FORCE_MAX_ATTEMPTS','RELOCATION_INDEPENDENT_OF_EXIT_BUDGET'),
 @('f','if (!state.RecordPhysicalRelocationAttempt())','if (!state.RecordForceClearanceAttempt())','RELOCATION_INDEPENDENT_OF_EXIT_BUDGET'),
 @('f','!current || current.m_iLogicalOccupants > 0 || current.m_iTransitions > 0 || current.m_iInsideBounds <= 0','false','FRESH_WHOLE_GROUP_EGRESS_BEFORE_CHARGE'),
 @('f','commitNowMs >= state.GetTerminalDeadlineMs() || trip.IsDeadlineReached(commitNowMs)','false','REPEATED_IDENTITY_DISTANCE_NAV_DEADLINE_BOUNDARIES'),
 @('f','!IsPhysicalOnlyBlocker(character, vehicle, boundsMin, boundsMax)','false','REPEATED_IDENTITY_DISTANCE_NAV_DEADLINE_BOUNDARIES'),
 @('f','!AICF_VehicleBoardingMutationFence.IsAuthoritativeAIEntity(character)','false','REPEATED_IDENTITY_DISTANCE_NAV_DEADLINE_BOUNDARIES'),
 @('f','activeBudgetExhausted = state.GetPhysicalRelocationAttempts() >= FORCE_MAX_ATTEMPTS;','activeBudgetExhausted = state.GetForceExitAttempts() >= FORCE_MAX_ATTEMPTS;','TERMINAL_ESCALATES_CURRENT_OPERATION_ONLY'),
 @('f','FORCE_MAX_ATTEMPTS = 3;','FORCE_MAX_ATTEMPTS = 30;','NO_RETRY_OR_DEADLINE_EXPANSION'),
 @('s','m_iPhysicalRelocationAttempts >= m_iMaximumPhysicalRelocationAttempts','false','PHYSICAL_BUDGET_REQUIRES_EGRESS'),
 @('s','m_iLogicalOccupants > 0 || m_iTransitions > 0 || m_iInsideBounds <= 0','false','PHYSICAL_BUDGET_REQUIRES_EGRESS'),
 @('s','m_iMaximumPhysicalRelocationAttempts = Math.Max(0, forceBudget);','m_iMaximumPhysicalRelocationAttempts = 99;','TWO_FINITE_PHASE_BUDGETS'),
 @('s','m_iPhysicalRelocationAttempts++;','m_iPhysicalRelocationAttempts = 0;','PHYSICAL_BUDGET_REQUIRES_EGRESS')
)){
 $f=$flow;$s=$state
 if($mutation[0] -eq 'f'){$f=$f.Replace($mutation[1],$mutation[2])}else{$s=$s.Replace($mutation[1],$mutation[2])}
 if($f -ceq $flow -and $s -ceq $state){throw "Missing budget mutation: $($mutation[3])"}
 if($mutation[3] -notin @(Rules $f $s)){throw "Budget mutation survived: $($mutation[3])"};$controls++
}
# Run actual production state bodies. The adapter changes only native field
# declarations; counter/deadline/sample algorithms are compiled verbatim.
$fields=$state.Substring($state.IndexOf('{')+1,$state.IndexOf('void Reset()')-$state.IndexOf('{')-1)
$fields=$fields.Replace('protected ref array<ref AICF_VehicleDismountActionToken> m_aGuidanceTokens = {};','public System.Collections.Generic.List<object> m_aGuidanceTokens = new System.Collections.Generic.List<object>();').Replace('protected ','public ').Replace('AIWaypoint ','object ')
$code="using System; public class AICF_DismountBudgetState { $fields"
foreach($signature in @(
 @('Reset','void Reset()'), @('Begin','void Begin(int nowMs,int normalDeadlineMs,int terminalDeadlineMs,int guidanceBudget,int forceBudget)'),
 @('BeginTerminal','void BeginTerminal(int nowMs,int terminalDeadlineMs,int forceBudget)'),
 @('RecordForceClearanceAttempt','bool RecordForceClearanceAttempt()'), @('RecordPhysicalRelocationAttempt','bool RecordPhysicalRelocationAttempt()'),
 @('RecordClearanceSample','int RecordClearanceSample(int logicalOccupants,int transitions,int insideBounds,int nowMs,int additionalStabilityBlockers=0)'),
 @('GetForceClearanceAttempts','int GetForceClearanceAttempts()'),@('GetForceExitAttempts','int GetForceExitAttempts()'),@('GetPhysicalRelocationAttempts','int GetPhysicalRelocationAttempts()'),
 @('TryBeginExactRelocationProbe','bool TryBeginExactRelocationProbe(int nowMs,int backoffMs)')
)){$code+=' public '+$signature[1]+(Body $state $signature[0])}
$code+=' }'
Add-Type -TypeDefinition $code
$case=New-Object AICF_DismountBudgetState;$case.Begin(100,20100,40100,3,3)
foreach($attempt in 1..3){if(!$case.RecordForceClearanceAttempt()){throw 'Logical force budget prematurely exhausted'}}
if($case.RecordForceClearanceAttempt() -or $case.GetForceExitAttempts() -ne 3){throw 'Fourth logical attempt accepted'};$controls++
$case.RecordClearanceSample(0,0,1,10000)|Out-Null
foreach($attempt in 1..3){if(!$case.RecordPhysicalRelocationAttempt()){throw 'Logical exit budget masked later physical hull clearance'}}
if($case.RecordPhysicalRelocationAttempt() -or $case.GetPhysicalRelocationAttempts() -ne 3 -or $case.GetForceClearanceAttempts() -ne 6){throw 'New total bound is not finite6'};$controls++
foreach($bad in @(@(1,0,1),@(0,1,1),@(0,0,0))){
 $case.Begin(100,20100,40100,3,3);$case.RecordClearanceSample($bad[0],$bad[1],$bad[2],10000)|Out-Null
 if($case.RecordPhysicalRelocationAttempt() -or $case.GetPhysicalRelocationAttempts()){throw 'Logical/transition/empty sample consumed physical budget'};$controls++
}
$case.Begin(100,20100,40100,3,3);$case.RecordClearanceSample(0,0,1,10000)|Out-Null;$case.RecordPhysicalRelocationAttempt()|Out-Null
foreach($poll in 1..20){$case.RecordClearanceSample(0,0,1,10000+$poll)|Out-Null}
if($case.GetPhysicalRelocationAttempts() -ne 1 -or $case.m_iNormalDeadlineMs -ne 20100 -or $case.m_iTerminalDeadlineMs -ne 40100){throw 'Polling reset budget or extended deadline'};$controls++
if(!$case.TryBeginExactRelocationProbe(12000,2000) -or $case.TryBeginExactRelocationProbe(13000,2000) -or !$case.TryBeginExactRelocationProbe(14000,2000)){throw 'Read-only probe backoff changed'};$controls++
$case.BeginTerminal(22000,42000,3)
if($case.GetForceClearanceAttempts() -or $case.m_iNormalDeadlineMs -ne 22000 -or $case.m_iTerminalDeadlineMs -ne 42000 -or !$case.m_bTerminalClearanceStarted){throw 'Existing new-phase terminal initialization changed'};$controls++
"PASS Dismount operation budgets: independent logical3 then physical3 within unchanged deadlines; fresh whole-group egress, nonempty safe probe, repeat authority/short-distance/nav fences and current-operation terminal escalation; controls=$controls"
