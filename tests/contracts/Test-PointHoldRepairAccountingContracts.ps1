param(
    [string]$RepositoryRoot = (Split-Path -Parent (Split-Path -Parent $PSScriptRoot)),
    [string]$ControllerSourcePath
)
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot '../lib/Stage3StaticAudit.Common.ps1')
if (!$ControllerSourcePath) {
    $ControllerSourcePath = Join-Path $RepositoryRoot 'AIConflictCore/Scripts/Game/AIConflict/Bootstrap/AICF_MatchController.c'
}
$source = [IO.File]::ReadAllText($ControllerSourcePath)
function Body([string]$text, [string]$method) {
    Get-AICFMethodBody ([pscustomobject]@{Source=$text; Code=(ConvertTo-AICFCodeText $text)}) $method
}
function Rules([string]$text) {
    $proof = Body $text 'TryResolveCompletedPointRepairReplacement'
    $process = Body $text 'ProcessPendingOrderRecovery'
    $terminal = Body $text 'RecordPendingOrderRepairTerminal'
    $audit = Body $text 'AuditOrderRepairAccounting'
    if (!$proof -or !$process -or !$terminal -or !$audit) { 'MISSING_METHOD'; return }
    foreach ($guard in @(
        '!Replication.IsServer()', '!slot || !faction || attemptId <= 0',
        '!slot.HasPendingOrderRecovery()', '!slot.PendingOrderRecoveryCountsAsReliabilityAttempt()',
        'slot.GetPendingOrderRecoveryReliabilityAttemptId() != attemptId',
        '!slot.IsPendingOrderRecoveryContextCurrent()',
        'slot.GetTargetKind() != AICF_EOrderTargetKind.POSITION',
        'group.GetFaction() != faction',
        'slot.GetOwnedWaypointTerminalOutcome(waypoint) != "GROUP_CALLBACK_COMPLETED"',
        'distanceMeters > POINT_DESTINATION_RADIUS_METERS',
        'slot.GetPendingOrderRecoveryReliabilityAttemptId() == attemptId',
        'slot.IsPendingOrderRecoveryContextCurrent()',
        'slot.GetGroup() == group && group.GetID() == groupId',
        'slot.GetWaypoint() == waypoint', 'waypoint.GetID() == waypointId',
        'slot.GetSpawnGeneration() == generation',
        'slot.GetStrategicAssignmentRevision() == assignmentRevision',
        'slot.GetStrategicIntentRevision() == intentRevision',
        'slot.GetTargetPosition() == targetPosition',
        'AICF_GroupRuntime.ResolveAliveLeader(group) == leader',
        'vector.DistanceSqXZ(leader.GetOrigin(), targetPosition) <='
    )) {
        if (!$proof.Contains($guard)) { 'POINT_IDENTITY_AND_PHYSICAL_PROOF' }
    }
    if ((ConvertTo-AICFCodeText $proof) -match '\b(?:Clear\w*|Record\w*|Refresh\w*|Rebuild\w*|Set\w*|Delete\w*)\s*\(') {
        'READ_ONLY_PROOF'
    }
    foreach ($group in @('pendingCurrent','groupCurrent','waypointCurrent','strategicCurrent','completedAtLeader')) {
        if ($proof -notmatch ('if \(!' + $group + '\)\s*return false;')) { 'CONSUMED_IDENTITY_PROOF' }
    }
    # Enforce rejects long boolean formulas even when a textual contract passes.
    # Bound each expression independently; source strings/comments do not count.
    foreach ($expression in ([regex]::Matches((ConvertTo-AICFCodeText $proof), '[^;{}]+[;{]'))) {
        if ([regex]::Matches($expression.Value, '&&|\|\|').Count -gt 4) { 'BOUNDED_NATIVE_FORMULAS' }
    }
    $match = [regex]::Match($process, '(?s)bool completedPointRepair =.*?(?=// Proximity is confirmation)')
    if (!$match.Success) { 'MISSING_DEDICATED_BRANCH'; return }
    $branch = $match.Value
    if ($branch -notmatch 'bool completedPointRepair = countsAsReliabilityRepair &&\s*slot.GetTargetKind\(\) == AICF_EOrderTargetKind.POSITION;' -or
        $branch -notmatch 'completedPointRepair && failureReason == "WAYPOINT_NOT_CURRENT"' -or
        $branch -notmatch 'terminalOutcome == "GROUP_CALLBACK_COMPLETED"' -or
        $branch -notmatch 'TryResolveCompletedPointRepairReplacement\(\s*slot, faction, repairAttemptId, completedPointDistanceMeters\)') {
        'EXACT_POINT_REPLACEMENT_BRANCH'
    }
    if ($process -notmatch 'if \(!completedPointRepair && terminalOutcome == "GROUP_CALLBACK_COMPLETED" &&\s*TryHoldCompletedOrderAtObjective') {
        'NO_GENERIC_PROOF_BYPASS'
    }
    $record = $branch.IndexOf('if (!RecordPendingOrderRepairTerminal(')
    $clear = $branch.IndexOf('slot.ClearPendingOrderRecovery();')
    $replace = $branch.IndexOf('RefreshCompletedPointHold(slot, faction, completedPointDistanceMeters);')
    if ($record -lt 0 -or $clear -le $record -or $replace -le $clear -or
        $branch -notmatch '"SUPERSEDED", "POINT_HOLD_REFRESH",\s*"COMPLETED_AT_POINT_REPLACEMENT"') {
        'TERMINAL_BEFORE_REPLACEMENT'
    }
    if ([regex]::Matches($branch, 'RecordPendingOrderRepairTerminal\(').Count -ne 1 -or
        [regex]::Matches($branch, 'slot.ClearPendingOrderRecovery\(\);').Count -ne 1 -or
        $branch -notmatch 'RefreshCompletedPointHold\(slot, faction, completedPointDistanceMeters\);\s*return;\s*\}') {
        'SINGLE_TERMINAL_NO_FALLTHROUGH'
    }
    if ($branch -match 'ORDER_RECOVERED|"CONFIRMED"|m_iOrderRecoveries\+\+|m_iOrderBindingVerifications\+\+') {
        'NO_UNVERIFIED_SUCCESS'
    }
    if ($match.Index -lt $process.IndexOf('if (!durabilitySatisfied)') -or
        $process -notmatch '"CONFIRMED",\s*"DURABILITY_CONFIRMED",\s*"DURABILITY_WINDOW"') {
        'EXISTING_DURABILITY_CONFIRMATION'
    }
    if ($terminal -notmatch 'outcome == "SUPERSEDED"\)\s*m_iOrderRecoverySuperseded\+\+' -or
        $audit -notmatch 'm_iOrderRecoveryAttempts - terminalCount - pendingCount' -or
        $audit -notmatch 'ORDER_REPAIR_ACCOUNTING_INVARIANT_FAILED') {
        'STRICT_ACCOUNTING_OWNER'
    }
}
$issues = @(Rules $source)
if ($issues.Count) { throw "Point hold repair accounting: $($issues -join ',')" }
$controls = 1
foreach ($mutation in @(
    @('attemptId <= 0', 'attemptId < 0', 'POINT_IDENTITY_AND_PHYSICAL_PROOF'),
    @('slot.GetPendingOrderRecoveryReliabilityAttemptId() != attemptId', 'false', 'POINT_IDENTITY_AND_PHYSICAL_PROOF'),
    @('!slot.IsPendingOrderRecoveryContextCurrent()', 'false', 'POINT_IDENTITY_AND_PHYSICAL_PROOF'),
    @('slot.GetTargetKind() != AICF_EOrderTargetKind.POSITION', 'false', 'POINT_IDENTITY_AND_PHYSICAL_PROOF'),
    @('group.GetFaction() != faction', 'false', 'POINT_IDENTITY_AND_PHYSICAL_PROOF'),
    @('slot.GetOwnedWaypointTerminalOutcome(waypoint) != "GROUP_CALLBACK_COMPLETED"', 'false', 'POINT_IDENTITY_AND_PHYSICAL_PROOF'),
    @('distanceMeters > POINT_DESTINATION_RADIUS_METERS', 'false', 'POINT_IDENTITY_AND_PHYSICAL_PROOF'),
    @('slot.GetPendingOrderRecoveryReliabilityAttemptId() == attemptId', 'true', 'POINT_IDENTITY_AND_PHYSICAL_PROOF'),
    @('group.GetID() == groupId', 'true', 'POINT_IDENTITY_AND_PHYSICAL_PROOF'),
    @('waypoint.GetID() == waypointId', 'true', 'POINT_IDENTITY_AND_PHYSICAL_PROOF'),
    @('slot.GetSpawnGeneration() == generation', 'true', 'POINT_IDENTITY_AND_PHYSICAL_PROOF'),
    @('slot.GetStrategicAssignmentRevision() == assignmentRevision', 'true', 'POINT_IDENTITY_AND_PHYSICAL_PROOF'),
    @('slot.GetStrategicIntentRevision() == intentRevision', 'true', 'POINT_IDENTITY_AND_PHYSICAL_PROOF'),
    @('slot.GetTargetPosition() == targetPosition', 'true', 'POINT_IDENTITY_AND_PHYSICAL_PROOF'),
    @('AICF_GroupRuntime.ResolveAliveLeader(group) == leader', 'true', 'POINT_IDENTITY_AND_PHYSICAL_PROOF'),
    @('completedPointRepair && failureReason == "WAYPOINT_NOT_CURRENT"', 'completedPointRepair', 'EXACT_POINT_REPLACEMENT_BRANCH'),
    @('!completedPointRepair && terminalOutcome == "GROUP_CALLBACK_COMPLETED"', 'terminalOutcome == "GROUP_CALLBACK_COMPLETED"', 'NO_GENERIC_PROOF_BYPASS'),
    @('"SUPERSEDED", "POINT_HOLD_REFRESH"', '"CONFIRMED", "POINT_HOLD_REFRESH"', 'NO_UNVERIFIED_SUCCESS'),
    @('m_iOrderRecoveryAttempts - terminalCount - pendingCount', '0', 'STRICT_ACCOUNTING_OWNER'),
    @('if (!pendingCurrent)', 'if (false)', 'CONSUMED_IDENTITY_PROOF'),
    @('if (!groupCurrent)', 'if (false)', 'CONSUMED_IDENTITY_PROOF'),
    @('if (!waypointCurrent)', 'if (false)', 'CONSUMED_IDENTITY_PROOF'),
    @('if (!strategicCurrent)', 'if (false)', 'CONSUMED_IDENTITY_PROOF'),
    @('if (!completedAtLeader)', 'if (false)', 'CONSUMED_IDENTITY_PROOF'),
    @('bool strategicCurrent =', 'bool strategicCurrent = true && true && true && true && true &&', 'BOUNDED_NATIVE_FORMULAS')
)) {
    $mutated = $source.Replace($mutation[0], $mutation[1])
    if ($mutated -ceq $source) { throw "Missing mutation target: $($mutation[0])" }
    if ($mutation[2] -notin @(Rules $mutated)) { throw "Mutation survived: $($mutation[2])" }
    $controls++
}
$process = Body $source 'ProcessPendingOrderRecovery'
$branch = [regex]::Match($process, '(?s)bool completedPointRepair =.*?(?=// Proximity is confirmation)').Value
foreach ($mutation in @(
    @($branch.Replace('if (!RecordPendingOrderRepairTerminal(', 'slot.ClearPendingOrderRecovery(); if (!RecordPendingOrderRepairTerminal('), 'TERMINAL_BEFORE_REPLACEMENT'),
    @($branch.Replace('slot.ClearPendingOrderRecovery();', 'RecordPendingOrderRepairTerminal(slot, faction, "SUPERSEDED", "DUPLICATE", "DUPLICATE"); slot.ClearPendingOrderRecovery();'), 'SINGLE_TERMINAL_NO_FALLTHROUGH'),
    @($branch.Replace('RefreshCompletedPointHold(slot, faction, completedPointDistanceMeters);', 'RefreshCompletedPointHold(slot, faction, completedPointDistanceMeters); AICF_Stage2Diagnostics.Info("ORDER_RECOVERED", "false");'), 'NO_UNVERIFIED_SUCCESS'),
    @($branch.Replace("RefreshCompletedPointHold(slot, faction, completedPointDistanceMeters);`r`n`t`t`treturn;", 'RefreshCompletedPointHold(slot, faction, completedPointDistanceMeters);'), 'SINGLE_TERMINAL_NO_FALLTHROUGH')
)) {
    $mutated = $source.Replace($branch, $mutation[0])
    if ($mutated -ceq $source) { throw "Missing branch mutation target: $($mutation[1])" }
    if ($mutation[1] -notin @(Rules $mutated)) { throw "Branch mutation survived: $($mutation[1])" }
    $controls++
}
$proof = Body $source 'TryResolveCompletedPointRepairReplacement'
$mutated = $source.Replace($proof, $proof.Insert(1, 'slot.ClearPendingOrderRecovery();'))
if ('READ_ONLY_PROOF' -notin @(Rules $mutated)) { throw 'Side-effectful proof survived' }
$controls++
"PASS Point hold repair accounting: exact completed point superseded before replacement; fresh hold receives no success; strict terminal accounting, identity and physical fences; controls=$controls"
