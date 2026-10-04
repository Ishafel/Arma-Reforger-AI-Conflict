param([string]$RepositoryRoot = (Split-Path -Parent $PSScriptRoot))
$ErrorActionPreference = 'Stop'
$core = Join-Path $RepositoryRoot 'AIConflictCore/Scripts/Game/AIConflict'
$s = @{}
@{ planner='Orders/AICF_OrderPlanner.c'; slot='State/AICF_GroupSlot.c'; episode='State/AICF_RouteRecoveryEpisode.c'; controller='Bootstrap/AICF_MatchController.c'; diagnostic='Victory/AICF_VictoryDiagnostics.c'; construction='Construction/AICF_ConstructionPlanner.c' }.GetEnumerator() | ForEach-Object {
    $s[$_.Key] = Get-Content (Join-Path $core $_.Value) -Raw -Encoding UTF8
}
function Test-Endgame($sources) {
    $checks = @(
        @('planner','if (attackTarget)','ATTACK_FIRST'),
        @('planner','securityTarget = targetSelector.SelectDefendTarget','SECURITY_FALLBACK'),
        @('planner','authority == AICF_EStrategicDecisionAuthority.AI_COMMANDER && posture == POSTURE_AREA_SECURITY','AUTHORITY_BOUNDARY'),
        @('planner','currentPosture == POSTURE_AREA_SECURITY && desiredPosture != POSTURE_AREA_SECURITY','ATTACK_RESUME'),
        @('controller','AssignFactionStrategicOrder(slot, faction, "REPAIR_BUDGET_TARGET_INVALID")','BUDGET_REPLAN'),
        @('controller','slot.ConsumePersistentStuckReview()','REVIEW_LOOP'),
        @('controller','assignAfterContextChange && TryReviewPersistentStuckHold(slot, faction)','EXHAUSTED_REVIEW_PATH'),
        @('controller','episode.RearmAfterBoundedHold(slot)','EXHAUSTED_REARM'),
        @('episode','!IsBlocked(slot) || !slot.IsPersistentStuckContextCurrent()','REARM_IDENTITY'),
        @('slot','if (!IsPersistentStuckContextCurrent())','REVIEW_IDENTITY'),
        @('slot','Math.Min(1800000, holdMs * 2)','REVIEW_BACKOFF'),
        @('diagnostic','previous == details && now - reportedAt < 60000','DIAGNOSTIC_THROTTLE'),
        @('diagnostic','status = "NOT_READY"','READINESS_DISTINCT'),
        @('construction','now < state.m_aSearchRetryAt[type]','SEARCH_BACKOFF_ADMISSION'),
        @('construction','Math.Min(480000, retryMs)','SEARCH_BACKOFF_CAP')
    )
    foreach ($check in $checks) {
        if (!$sources[$check[0]].Contains($check[1])) { $check[2] }
    }
}
$failures = @(Test-Endgame $s)
$mutations = @(
    @('planner','if (attackTarget)','ATTACK_FIRST'),
    @('planner','securityTarget = targetSelector.SelectDefendTarget','SECURITY_FALLBACK'),
    @('planner','authority == AICF_EStrategicDecisionAuthority.AI_COMMANDER && posture == POSTURE_AREA_SECURITY','AUTHORITY_BOUNDARY'),
    @('planner','currentPosture == POSTURE_AREA_SECURITY && desiredPosture != POSTURE_AREA_SECURITY','ATTACK_RESUME'),
    @('controller','AssignFactionStrategicOrder(slot, faction, "REPAIR_BUDGET_TARGET_INVALID")','BUDGET_REPLAN'),
    @('slot','if (!IsPersistentStuckContextCurrent())','REVIEW_IDENTITY'),
    @('controller','assignAfterContextChange && TryReviewPersistentStuckHold(slot, faction)','EXHAUSTED_REVIEW_PATH'),
    @('controller','episode.RearmAfterBoundedHold(slot)','EXHAUSTED_REARM'),
    @('episode','!IsBlocked(slot) || !slot.IsPersistentStuckContextCurrent()','REARM_IDENTITY'),
    @('slot','Math.Min(1800000, holdMs * 2)','REVIEW_BACKOFF'),
    @('diagnostic','previous == details && now - reportedAt < 60000','DIAGNOSTIC_THROTTLE'),
    @('construction','now < state.m_aSearchRetryAt[type]','SEARCH_BACKOFF_ADMISSION')
)
foreach ($mutation in $mutations) {
    $copy = $s.Clone()
    $copy[$mutation[0]] = $copy[$mutation[0]].Replace($mutation[1], 'REMOVED_BY_MUTATION')
    if ($mutation[2] -notin @(Test-Endgame $copy)) { $failures += "MUTATION_ESCAPED $($mutation[2])" }
}
if (Test-Path (Join-Path $core 'Victory/AICF_EndgameProbe.c')) { $failures += 'FIXTURE_IN_PRODUCTION' }
if ($failures.Count) { $failures; exit 1 }
"Endgame contracts: PASS; mutations=$($mutations.Count)"
