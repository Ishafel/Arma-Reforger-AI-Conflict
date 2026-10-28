param([string]$RepositoryRoot=(Split-Path -Parent (Split-Path -Parent $PSScriptRoot)))
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot '../lib/Stage3StaticAudit.Common.ps1')
$fixture=[IO.File]::ReadAllText((Join-Path $RepositoryRoot 'tests/fixtures/AICF_ManualStagingProbe.c'))
function Body($s,$m){Get-AICFMethodBody ([pscustomobject]@{Source=$s;Code=(ConvertTo-AICFCodeText $s)}) $m}
function Rules($s){
 $maintain=Body $s 'MaintainStagingMemberActions'
 foreach($guard in @('aicfManualStagingProbe','flag == "1"','trip.GetAssignment().GetGroup() == AICF_ManualStagingProbeState.s_Group',
  '!AICF_ManualStagingProbeState.s_SetupAt','plan.RequiresManualRally()','vector.Distance(actor.GetOrigin(), destination) <= 25',
  'AICF_InfantrySpawnPlacement.IsUsable(','GetDisableMovementControls()','SetDisableMovementControls(true)',
  'SetDisableMovementControls(s_OldMovementDisabled)','super.MaintainStagingMemberActions(',
  'now - m_iManualStagingStarted > 180000','AICF_VBFinish();')){if(!$s.Contains($guard)){'OWNED_CONTROLLED_BOUNDED_LIFECYCLE';break}}
 $negative=Body $s 'AICF_MSNegative'
 foreach($guard in @('!recovered && !m_bConsumed','!m_Plan.WasStagingMemberRelocated(m_Actor)',
  'vector.Distance(before, m_Actor.GetOrigin()) < 0.01','GetHealthScaled() == health','GetLifeState() == ECharacterLifeState.ALIVE')){
  if(!$negative.Contains($guard)){'NEGATIVE_NO_TRANSFORM_HEALTH_LIFE_BUDGET_MUTATION';break}
 }
 foreach($name in @('STALE_REQUEST_NO_MUTATION','STALE_PRESET_NO_MUTATION','STALE_RPL_NO_MUTATION','NO_CURRENT_ASSIGNMENT_NO_MUTATION',
  'LOST_ACTION_NO_MUTATION','EXHAUSTED_TWO_OWNED_ATTEMPTS','SHORT_SUBMITTED_ONCE','DUPLICATE_NO_MUTATION','ACTUAL_SHORT_TELEPORT',
  'ALL_FIVE_PHYSICAL_RALLY_BEFORE_PAYMENT','ORIGINAL_IDENTITIES_HEALTH_UNCHANGED','DEADLINES_UNCHANGED','NO_BUY_OR_DEBIT')){
  if(!$s.Contains('"'+$name+'"')){'OBLIGATORY_CASE';break}
 }
 foreach($guard in @('m_Token.GetRetryCount() == 1','m_Plan.WasStagingMemberRetried(m_Actor)',
  'm_Plan.IsStagingMemberExhausted(m_Actor)','m_Token.IsActionOwnedByUtility()',
  'm_iManualStagingClearPolls >= 2 && shift <= 8','ready == 5 && !trip.GetLease() && !trip.IsVehiclePresetDebited()',
  'trip.VehiclePresetMember(member) == original','GetHealthScaled() == AICF_ManualStagingProbeState.s_Health',
  'plan.GetApproachDeadlineMs() == AICF_ManualStagingProbeState.s_ApproachDeadline',
  'trip.GetAbsoluteDeadlineMs() == AICF_ManualStagingProbeState.s_TripDeadline','now < plan.GetApproachDeadlineMs()',
  'GetSpawnedCount(m_VBFaction.GetFactionKey()) == 0','trip.GetRequestState().GetAttemptCount() == 0')){
  if(!$s.Contains($guard)){'NATIVE_ACTUAL_PHYSICAL_AND_EXISTING_BUDGET_PROOF';break}
 }
 if($s -match '\b(?:SetHealth|SetLifeState|SetHealthScaled|KillGM|MarkStagingMemberRetried|MarkStagingMemberExhausted|MarkStagingMemberRelocated)\s*\(' -or
  $s -match 'm_i(?:LastProgressAtMs|LastMotionAtMs|RetryCount)\s*='){'NO_HEALTH_OR_ATTEMPT_CLOCK_FABRICATION'}
 foreach($guard in @('aicfManualStagingExhaustionProbe','setupDistance = 40','EXHAUSTION_TWO_ATTEMPTS_SPENT',
  'EXHAUSTION_LIVE_WAYPOINT_NOT_FALLBACK','EXHAUSTION_MISSING_REISSUE_NOT_FALLBACK','EXHAUSTION_WAYPOINT_REMOVED_FIRST',
  'EXHAUSTION_WAYPOINT_REMOVED_REISSUE','EXHAUSTION_EARLY_TYPED_FALLBACK','EXHAUSTION_PLAN_RELEASED','EXHAUSTION_ORDER_RESTORED',
  'EXHAUSTION_NO_TELEPORT_DEBIT','EXHAUSTION_ORIGINAL_HEALTH','EXHAUSTION_DEADLINE_UNCHANGED',
  'plan.GetApproachWaypointReissueCount() == 1','trip.GetTerminalReason() == "SPAWN_STAGING_ACTIONS_EXHAUSTED"',
  'trip.GetPhaseStartedAtMs() < AICF_ManualStagingProbeState.s_ApproachDeadline','!trip.GetRequestState().GetSpawnPlan()',
  'handoff.IsOrderRestored()','!AICF_ManualStagingProbeState.s_SubmittedAt')){
  if(!$s.Contains($guard)){'EXHAUSTED_NATIVE_LIFECYCLE_AND_FALLBACK_PROOF';break}
 }
 if((Body $s 'Stop').IndexOf('AICF_ManualStagingProbeState.RestoreMovement()') -gt (Body $s 'Stop').IndexOf('super.Stop(')){'RESTORE_NATIVE_CONTROL_BEFORE_STOP'}
 if($s.Contains('plan.GetApproachWaypointReissueCount() != AICF_ManualStagingProbeState.s_WaypointRemovals') -or
  !$maintain.Contains('!AICF_ManualStagingProbeState.s_MissingReissueReported') -or
  !$maintain.Contains('AICF_ManualStagingProbeState.s_MissingReissueReported = true;') -or
  $maintain.IndexOf('"EXHAUSTION_MISSING_REISSUE_NOT_FALLBACK"') -gt $maintain.IndexOf('super.MaintainStagingMemberActions(')){'EARLY_NATIVE_COUNTER_ZERO_NO_COUNTER_COUPLING'}
 if(!$maintain.Contains('!AICF_ManualStagingProbeState.s_TwoAttemptsReported') -or
  $maintain.IndexOf('s_TwoAttemptsReported = true;') -gt $maintain.IndexOf('"EXHAUSTION_TWO_ATTEMPTS_SPENT"') -or
  !$maintain.Contains('AICF_ManualStagingProbeState.s_ReissueRemovalReported) return;')){'ONESHOT_RECEIPTS_INDEPENDENT_OF_NATIVE_REISSUE'}
 foreach($guard in @('firstQueue.Contains(first)','RemoveWaypoint(first)','bool replacementQueued = waypoint && queue.Contains(waypoint)',
  'factory.CreateSpawnStagingWaypoint(','AddWaypoint(temporary)','bool nativeQueued = temporary && queue.Contains(temporary)',
  'RemoveWaypoint(temporary)','bool detached = temporary && !queue.Contains(temporary)',
  'if (detached) { RplComponent.DeleteRplEntity(temporary, false);','plan_rebound=0','original_replacement_already_native_unqueued=1')){
  if(!$maintain.Contains($guard)){'ACTUAL_NATIVE_WAYPOINT_NEGATIVE_AND_DETACHED_DELETE';break}
 }
 $restore=Body $s 'AICF_MSExhaustionFinished'
 foreach($guard in @('if (!m_iManualStagingRestoreDeadline)','handoff.GetAbsoluteOrderDeadlineMs()',
  'Math.Min(nowMs + 30000, m_iManualStagingOriginalHandoffDeadline)','ProcessPendingOrderRecovery(m_VBState, slot, m_VBFaction)',
  'm_iManualStagingRestoreNextPoll = nowMs + m_Stage2Config.GetReliabilityIntervalMs()',
  'handoff.GetRestoredWaypoint() == infantryWaypoint','nativeQueue.Contains(infantryWaypoint)',
  'HasMeaningfulTask(slot, m_VBCoordinator.GetSlotView(slot))','m_OrderPlanner.GetOrderFailureReason(slot, m_VBFaction)',
  '!slot.HasPendingOrderRecovery()','if (!restored && nowMs < m_iManualStagingRestoreDeadline) return;',
  'handoff.GetAbsoluteOrderDeadlineMs() == m_iManualStagingOriginalHandoffDeadline')){
  if(!$restore.Contains($guard)){'REAL_DURABILITY_OWNER_ORIGINAL_BOUNDED_DEADLINE';break}
 }
 if($restore -match '\b(?:ClearPendingOrderRecovery|RecordPendingOrderRecoveryStablePoll|BeginOrderRecoveryVerification|Set\w*Deadline)\s*\('){'NO_FAKE_RESTORE_CONFIRMATION_OR_REARM'}
}
$bad=@(Rules $fixture);if($bad.Count){throw "Manual staging native fixture: $bad"}
$mutants=@('vector.Distance(actor.GetOrigin(), destination) <= 25','super.MaintainStagingMemberActions(',
 'SetDisableMovementControls(s_OldMovementDisabled)','!recovered && !m_bConsumed','vector.Distance(before, m_Actor.GetOrigin()) < 0.01',
 'm_Token.GetRetryCount() == 1','m_Plan.IsStagingMemberExhausted(m_Actor)','m_iManualStagingClearPolls >= 2 && shift <= 8',
 'ready == 5 && !trip.GetLease() && !trip.IsVehiclePresetDebited()','GetHealthScaled() == AICF_ManualStagingProbeState.s_Health',
 'trip.GetAbsoluteDeadlineMs() == AICF_ManualStagingProbeState.s_TripDeadline','trip.GetRequestState().GetAttemptCount() == 0',
 'trip.GetPhaseStartedAtMs() < AICF_ManualStagingProbeState.s_ApproachDeadline','handoff.IsOrderRestored()',
 'trip.GetTerminalReason() == "SPAWN_STAGING_ACTIONS_EXHAUSTED"','!AICF_ManualStagingProbeState.s_SubmittedAt',
 '!AICF_ManualStagingProbeState.s_MissingReissueReported','!AICF_ManualStagingProbeState.s_TwoAttemptsReported',
 'AICF_ManualStagingProbeState.s_ReissueRemovalReported) return;','bool nativeQueued = temporary && queue.Contains(temporary)',
 'if (detached) { RplComponent.DeleteRplEntity(temporary, false);',
 'ProcessPendingOrderRecovery(m_VBState, slot, m_VBFaction)','m_iManualStagingRestoreNextPoll = nowMs + m_Stage2Config.GetReliabilityIntervalMs()',
 'HasMeaningfulTask(slot, m_VBCoordinator.GetSlotView(slot))','!slot.HasPendingOrderRecovery()',
 'handoff.GetAbsoluteOrderDeadlineMs() == m_iManualStagingOriginalHandoffDeadline')
foreach($m in $mutants){if(!$fixture.Contains($m)){throw "Missing mutant $m"};if(!@(Rules ($fixture.Replace($m,'false'))).Count){throw "Escaped fixture mutant $m"}}
"PASS Manual staging native fixture contracts: $($mutants.Count) negative controls; controlled native block, no clock/budget fabrication, actual later physical rally/payment fence"
