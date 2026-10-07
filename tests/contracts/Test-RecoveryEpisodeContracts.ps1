param(
    [string]$RepositoryRoot = (Split-Path -Parent (Split-Path -Parent $PSScriptRoot)),
    [string]$RuntimeLogPath,
    [switch]$RequireHiddenRecovery,
    [switch]$PlayerReleaseOnly,
    [switch]$HiddenContinuationOnly
)
$ErrorActionPreference = 'Stop'
$core = Join-Path $RepositoryRoot 'AIConflictCore/Scripts/Game/AIConflict'
$failures = [Collections.Generic.List[string]]::new()
function Require([string]$File, [string]$Pattern, [string]$Rule) {
    $source = [IO.File]::ReadAllText((Join-Path $core $File))
    if ($source -notmatch $Pattern) { $failures.Add($Rule) }
}
Require 'Forces/AICF_InfantryRecruitmentService.c' '(?s)if \(!order.IsPhysicallyPresent\(\)\).*?return CheckApproach\(order, leader.GetOrigin\(\), now\);' 'APPROACH_BEFORE_PURCHASE'
Require 'Forces/AICF_InfantryRecruitmentService.c' '(?s)GetWaypoints\(waypoints\).*?waypoints.Contains\(order.m_Waypoint\).*?GetCurrentWaypoint\(\) == order.m_Waypoint.*?m_iApproachRepairs >= 2.*?RepairInfantryRecruitmentApproach' 'BOUNDED_QUEUE_REPAIR'
Require 'Forces/AICF_InfantryRecruitmentService.c' '(?s)if \(IsApproachBlocked\(slot, service\)\)\s*continue;' 'EXCLUDE_FAILED_SERVICE'
Require 'Forces/AICF_InfantryRecruitmentService.c' '(?s)RememberApproachFailure\(order\);.*?EndInfantryRecruitment' 'REMEMBER_BEFORE_RESTORE'
Require 'Forces/AICF_RecruitmentApproachFailure.c' '(?s)GetID\(\) != m_GroupId.*?GetSpawnGeneration\(\) != m_iGeneration.*?graphRevision != m_iGraphRevision.*?GetID\(\) != m_ServiceId.*?DistanceSqXZ' 'FAILURE_IDENTITY_AND_CONTEXT'
Require 'Orders/AICF_OrderPlanner.c' '(?s)RefreshInfantryMuster.*?HasActiveRecruitmentOrder\(\)\)\s*return;' 'MUSTER_PRESERVES_VISIT'
Require 'Orders/AICF_OrderPlanner.c' '(?s)bool RepairInfantryRecruitmentApproach.*?Replication.IsServer\(\).*?IsCurrent\(order.m_Slot\).*?HasSafeBarracks\(\).*?m_Donor.*?CountAliveAgentsInAnyVehicle.*?BeginInfantryRecruitment' 'REPAIR_IDENTITY_AND_SAFETY'
Require 'State/AICF_RouteRecoveryEpisode.c' '(?s)m_bActive && m_GroupId == slot.GetGroup\(\).GetID\(\) && m_iGeneration == slot.GetSpawnGeneration\(\)\)\s*return;' 'EPISODE_SURVIVES_REASSIGNMENT'
Require 'State/AICF_RouteRecoveryEpisode.c' '(?s)GetAgeMs\(\) >= DEADLINE_MS.*?m_bExhausted = true;.*?int GetAgeMs\(\).*?System.GetTickCount\(m_iStartedAtMs\)' 'ABSOLUTE_DEADLINE'
Require 'State/AICF_RouteRecoveryEpisode.c' '(?s)GetCurrentWaypoint\(\) == slot.GetWaypoint\(\).*?DistanceSqXZ.*?>= 225.*?DistanceXZ.*?>= 5.*?return "PHYSICAL_PROGRESS";' 'PHYSICAL_ROUTE_PROOF'
Require 'Bootstrap/AICF_MatchController.c' '(?s)ProcessRouteRecoveryEpisode\(slot, faction\)\)\s*continue;\s*if \(slot.HasPendingOrderRecovery' 'EPISODE_BEFORE_LOCAL_TIMERS'
Require 'Bootstrap/AICF_MatchController.c' '(?s)m_OrderPlanner.ReleasePlayerCommand\(slot\);\s*//[^\r\n]*\s*ProcessRouteRecoveryEpisode\(slot, faction, false\);\s*bool assigned = commander.AssignOrder\(' 'PLAYER_RELEASE_BEFORE_AI_ASSIGNMENT'
Require 'Bootstrap/AICF_MatchController.c' '(?s)episode.TakeHoldAttempt\(\).*?HasPendingOrderRecovery\(\).*?SupersedePendingOrderRecovery\(slot, faction, "ROUTE_RECOVERY_EXHAUSTED"\).*?HoldPositionForPersistentStuck' 'EXHAUSTION_CLOSES_PENDING_VERIFICATION'
Require 'Forces/AICF_InfantryRecruitmentRecovery.c' '(?s)Replication.IsServer\(\).*?GetHiddenRecoveryEnabled\(\).*?IsCurrent\(order.m_Slot\).*?HasUsedIsolatedNavmeshRecovery\(\).*?HasSafeBarracks\(\)' 'HIDDEN_POLICY_IDENTITY_BUDGET'
Require 'Forces/AICF_InfantryRecruitmentRecovery.c' '(?s)IsMemberCurrent\(agents\[index\].*?GetID\(\) != identities\[index\].*?IsHiddenRecoveryCombatSafe.*?CanApplyHiddenRecovery.*?IsUsable.*?MarkIsolatedNavmeshRecoveryUsed\(\).*?Teleport\(transform\)' 'HIDDEN_RECHECK_BEFORE_MUTATION'
Require 'Forces/AICF_InfantryRecruitmentRecovery.c' '(?s)MAX_SHIFT_METERS = 8.*?DistanceXZ\(origin, candidate\) > MAX_SHIFT_METERS.*?ARRIVAL_METERS \+ 5' 'HIDDEN_LOCAL_STEP_REQUIRES_WALKING'
Require 'Forces/AICF_InfantryRecruitmentService.c' '(?s)if \(order.m_bHiddenRecoveryPending\).*?m_fApproachBestDistance = vector.DistanceXZ.*?return string.Empty;.*?int alive' 'HIDDEN_ASYNC_BASELINE_BEFORE_PURCHASE'
Require 'Forces/AICF_InfantryRecruitmentService.c' '(?s)m_iApproachRepairs == 1 && now - order.m_iApproachProgressAtMs >= 45000.*?TryRecover.*?RepairInfantryRecruitmentApproach' 'HIDDEN_ONLY_AFTER_STALLED_REPAIR'

$output = Join-Path $RepositoryRoot '.codex-runtime/issue-11/analyzer-tests'
New-Item -ItemType Directory -Force $output | Out-Null
$baseline = @()
foreach ($side in @('US','USSR')) {
    foreach ($slot in 0..3) { $baseline += "[AICF][STAGE2][INFO][SPAWN_BOUND] faction=$side slot=$slot generation=1 group=${side}_$slot" }
}
$baseline += '[AICF][STAGE2][INFO][RELIABILITY_HEARTBEAT]'
function Check-Log([string]$Name, [string[]]$Events, [int]$Expected, [string]$Reason = '') {
    $log = Join-Path $output "$Name.log"
    $baseline + $Events | Set-Content $log
    $result = & powershell.exe -NoProfile -ExecutionPolicy Bypass -File (Join-Path $RepositoryRoot 'tests/log-audits/Test-Stage2Log.ps1') -LogPath $log 2>&1
    $code = $LASTEXITCODE
    $result | Set-Content (Join-Path $output "$Name-result.txt")
    if ($code -ne $Expected -or ($Reason -and ($result -join "`n") -notmatch $Reason)) { $failures.Add("ANALYZER_${Name}: exit=$code expected=$Expected") }
}
Check-Log 'healthy' @() 0
$visits = 1..3 | ForEach-Object { "[AICF][STAGE4][INFO][INFANTRY_RECRUITMENT_FINISHED] faction=USSR slot=12 generation=1 token=$_ base=HQ service=42 reason=APPROACH_INTERRUPTED_OR_TIMEOUT" }
Check-Log 'barracks-cycle' $visits 1 'Repeated failed barracks approach'
$replans = 1..13 | ForEach-Object { "[AICF][STAGE2][INFO][FALSE_COMPLETION_ROUTE_REPLAN] faction=USSR slot=A4 numeric_slot=4 group_generation=1 assignment_revision=$_ replanned=$($_ % 2)" }
Check-Log 'route-cycle' $replans 1 'Route replan cycle'
$hold = '[AICF][STAGE35][INFO][ORDER_RESTORE_RESULT] faction=USSR numeric_slot=4 group_generation=1 success=1 fallback_action=TEMPORARY_ROUTE_REPLAN_HOLD'
Check-Log 'hold-is-not-motion' ($replans[0..6] + $hold + $replans[7..12]) 1 'Route replan cycle'
$motion = '[AICF][STAGE2][INFO][ROUTE_RECOVERY_EPISODE_FINISHED] faction=USSR numeric_slot=4 group_generation=1 outcome=PHYSICAL_PROGRESS'
Check-Log 'real-motion' ($replans[0..6] + $motion + $replans[7..12]) 0
$newGeneration = $replans[7..12] -replace 'group_generation=1','group_generation=2'
Check-Log 'new-generation' ($replans[0..6] + $newGeneration) 0
Check-Log 'arrived' ($visits[0..1] + '[AICF][STAGE4][INFO][INFANTRY_RECRUITMENT_ARRIVED] faction=USSR slot=12 generation=1 service=42 physical_presence=1' + $visits[2]) 0
Check-Log 'context-rearmed' ($visits[0..1] + '[AICF][STAGE4][INFO][INFANTRY_RECRUITMENT_APPROACH_REARMED] faction=USSR slot=12 generation=1 service=42 reason=CONTEXT_CHANGED' + $visits[2]) 0
Check-Log 'rejected-terminal-hold' @('[AICF][STAGE2][WARNING][ROUTE_RECOVERY_EXHAUSTED] faction=USSR numeric_slot=4 group_generation=1 hold_committed=0') 1 'Terminal route hold was rejected'
# Rearm живёт между визитами; таймер сам по себе не является причиной rearm.
foreach ($reason in @('CONTEXT_CHANGED','TIMER_ONLY')) {
    $rearmLog = Join-Path $output "rearm-$reason.log"
    @(
        '[INFANTRY_RECRUITMENT_STARTED] faction=USSR slot=0 generation=1 token=1 group=42 distance_m=50 alive=1 desired=10',
        '[INFANTRY_RECRUITMENT_FINISHED] faction=USSR slot=0 generation=1 token=1 group=42 reason=APPROACH_RECOVERY_EXHAUSTED',
        "[INFANTRY_RECRUITMENT_APPROACH_REARMED] faction=USSR slot=0 generation=1 service=99 reason=$reason movement_confirmation=NONE"
    ) | Set-Content $rearmLog
    $rearmResult = & powershell.exe -NoProfile -ExecutionPolicy Bypass -File (Join-Path $RepositoryRoot 'tests/log-audits/Test-InfantryRecruitmentLog.ps1') -LogPath $rearmLog 2>&1
    $expected = 0
    if ($reason -eq 'TIMER_ONLY') { $expected = 1 }
    if ($LASTEXITCODE -ne $expected) { $failures.Add("REARM_ANALYZER_$reason") }
    $rearmResult | Set-Content (Join-Path $output "rearm-$reason-result.txt")
}
if ($RuntimeLogPath) {
    $runtime = [IO.File]::ReadAllText((Resolve-Path -LiteralPath $RuntimeLogPath).Path)
    if ($RequireHiddenRecovery) {
        foreach ($case in @('HIDDEN_FENCES_AND_SUBMISSION','HIDDEN_OBSERVED_AND_BUDGET')) {
            if ($runtime -notmatch "\[EPISODE_PROBE\] case=$case passed=1") { $failures.Add("RUNTIME_MISSING_$case") }
        }
        foreach ($match in [regex]::Matches($runtime, '\[INFANTRY_RECRUITMENT_HIDDEN_MEMBER\][^\r\n]* shift_m=([\d.]+)')) {
            if ([double]::Parse($match.Groups[1].Value, [cultureinfo]::InvariantCulture) -gt 8) { $failures.Add('HIDDEN_SHIFT_EXCEEDS_8M') }
        }
        if ($runtime -notmatch '\[INFANTRY_RECRUITMENT_HIDDEN_MEMBER\]' -or $runtime -notmatch '\[INFANTRY_RECRUITMENT_HIDDEN_OBSERVED\].*paid=0') { $failures.Add('HIDDEN_NO_PHYSICAL_EVIDENCE') }
    }
    $cases = @('WAYPOINT_LOSS','REPLAN_EPISODE_BOUND','PLAYER_INTENT_CLEARED','AI_REPLAN_PRESERVES_EPISODE')
    if (-not $HiddenContinuationOnly) { $cases += @('APPROACH_EXHAUSTION','BARRACKS_RETRY_CONTEXT') }
    if ($PlayerReleaseOnly) { $cases = @('REPLAN_EPISODE_BOUND','PLAYER_INTENT_CLEARED','AI_REPLAN_PRESERVES_EPISODE') }
    foreach ($case in $cases) {
        if ($runtime -notmatch "\[EPISODE_PROBE\] case=$case passed=1") { $failures.Add("RUNTIME_MISSING_$case") }
    }
    if ($runtime -match '\[EPISODE_PROBE\].*passed=0') { $failures.Add('RUNTIME_PROBE_FAILURE') }
    $events = @('INFANTRY_RECRUITMENT_APPROACH_REPAIR','INFANTRY_RECRUITMENT_PROGRESS','INFANTRY_RECRUITMENT_ARRIVED','INFANTRY_RECRUIT_JOINED','ROUTE_RECOVERY_EXHAUSTED')
    if (-not $HiddenContinuationOnly) { $events += 'INFANTRY_RECRUITMENT_APPROACH_BLOCKED' }
    if ($PlayerReleaseOnly) { $events = @('ROUTE_RECOVERY_EXHAUSTED','ROUTE_RECOVERY_EPISODE_FINISHED') }
    foreach ($event in $events) {
        if ($runtime -notmatch "\[$event\]") { $failures.Add("RUNTIME_MISSING_$event") }
    }
    if (!$PlayerReleaseOnly -and ($runtime -notmatch '\[RECRUIT_PROBE\] full_rosters=1 stable_groups=1' -or
        $runtime -notmatch '\[RECRUIT_PROBE\] finished=1 full_rosters=1' -or
        $runtime -notmatch 'Game destroyed\.')) { $failures.Add('RUNTIME_INCOMPLETE_OR_UNSTOPPED') }
    if ($runtime -notmatch 'Game destroyed\.') { $failures.Add('RUNTIME_UNSTOPPED') }
    if ($runtime -match 'SCRIPT\s+\([EF]\)|ENGINE\s+\(F\)|NULL pointer|Unhandled exception|\[AICF\]\[STAGE[^\]]*\]\[ERROR\]') {
        $failures.Add('RUNTIME_SCRIPT_OR_AICF_ERROR')
    }
}
if ($failures.Count) { $failures | ForEach-Object { Write-Output "FAIL $_" }; exit 1 }
Write-Output 'Recovery episode contracts: PASS; route analyzer cases=9; rearm analyzer cases=2'
if ($RuntimeLogPath) {
    if ($PlayerReleaseOnly) { Write-Output 'Player release runtime: PASS; context release, executable AI order, identity and normal shutdown confirmed' }
    else { Write-Output "Recovery episode runtime: PASS; base fault cases=$($cases.Count); hidden cases=$([int]$RequireHiddenRecovery.IsPresent * 2); physical progress, full rosters and normal shutdown confirmed" }
}
