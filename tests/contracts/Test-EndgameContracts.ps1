param([string]$RepositoryRoot = (Split-Path -Parent (Split-Path -Parent $PSScriptRoot)))
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot '../lib/Stage3StaticAudit.Common.ps1')
$core = Join-Path $RepositoryRoot 'AIConflictCore/Scripts/Game/AIConflict'
$s = @{}
@{ planner='Orders/AICF_OrderPlanner.c'; policy='Command/AICF_AdaptiveCommanderPolicy.c'; slot='State/AICF_GroupSlot.c'; episode='State/AICF_RouteRecoveryEpisode.c'; controller='Bootstrap/AICF_MatchController.c'; diagnostic='Victory/AICF_VictoryDiagnostics.c'; construction='Construction/AICF_ConstructionPlanner.c' }.GetEnumerator() | ForEach-Object {
    $s[$_.Key] = Get-Content (Join-Path $core $_.Value) -Raw -Encoding UTF8
}
function Test-Endgame($sources) {
    $checks = @(
        @('planner','authority == AICF_EStrategicDecisionAuthority.AI_COMMANDER && posture == POSTURE_AREA_SECURITY','AUTHORITY_BOUNDARY'),
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
    # Adaptive policy owns selection since #42. Ordinary return from security
    # respects dwell/stable gates; urgency or invalid targets bypass them.
    $candidates = Get-AICFMethodBody ([pscustomobject]@{Source=$sources.policy;Code=$sources.policy}) 'BuildCandidates'
    $select = Get-AICFMethodBody ([pscustomobject]@{Source=$sources.policy;Code=$sources.policy}) 'Select'
    $reconcile = Get-AICFMethodBody ([pscustomobject]@{Source=$sources.planner;Code=$sources.planner}) 'ReconcileAICommanderOrder'
    if ($candidates -notmatch '(?s)m_Selector.CollectAttackNodes.*?!slot.IsRouteTargetDeferred.*?usableAttack = true;' -or
        $candidates -notmatch '(?s)slot.GetRole\(\) == AICF_EGroupRole.ATTACK.*?usableAttack && AICF_GroupRuntime.CountAliveAgents\(slot.GetGroup\(\)\) >= AICF_AdaptiveCommanderConfig.MIN_ATTACK_ALIVE.*?candidate.m_fFrontValue = -AICF_AdaptiveCommanderConfig.ATTACK_HOLD_COST;') { 'ATTACK_FIRST' }
    if ($candidates -notmatch 'candidate.m_bHQ = base == m_Faction.GetMainBase\(\);' -or
        $candidates -notmatch 'slot.GetRole\(\) != AICF_EGroupRole.DEFEND && !candidate.m_bHQ' -or
        $select -notmatch '(?s)AICF_AdaptiveScore.Choose\(.*?decision.m_sPosture = "AREA_SECURITY";.*?if \(best.m_bAttack\).*?decision.m_sPosture = "ATTACK_PRIMARY";') { 'SECURITY_FALLBACK' }
    if ($reconcile -notmatch 'string desiredPosture = decision.m_sPosture;' -or
        $reconcile -notmatch '(?s)bool urgentQRF = decision.m_bUrgent;.*?if \(!urgentQRF\).*?IsStrategicCandidateReady\(.*?desiredPosture,.*?minimumDwellMs,.*?stableCandidateMs\)' -or
        $reconcile -notmatch '(?s)return ReplaceOrder\(\s*slot,\s*faction,\s*desiredTarget,\s*reason,\s*desiredPosture,') { 'ATTACK_RESUME' }
}
$failures = @(Test-Endgame $s)
$mutations = @(
    @('policy','candidate.m_fFrontValue = -AICF_AdaptiveCommanderConfig.ATTACK_HOLD_COST;','ATTACK_FIRST'),
    @('policy','!slot.IsRouteTargetDeferred(m_Graph.GetNode(attackId).GetBase())','ATTACK_FIRST'),
    @('policy','decision.m_sPosture = "AREA_SECURITY";','SECURITY_FALLBACK'),
    @('policy','slot.GetRole() != AICF_EGroupRole.DEFEND && !candidate.m_bHQ','SECURITY_FALLBACK'),
    @('planner','authority == AICF_EStrategicDecisionAuthority.AI_COMMANDER && posture == POSTURE_AREA_SECURITY','AUTHORITY_BOUNDARY'),
    @('planner','string desiredPosture = decision.m_sPosture;','ATTACK_RESUME'),
    @('planner','bool urgentQRF = decision.m_bUrgent;','ATTACK_RESUME'),
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
