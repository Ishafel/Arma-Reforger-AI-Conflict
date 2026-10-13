param([string]$RepositoryRoot = (Split-Path -Parent (Split-Path -Parent $PSScriptRoot)))
$ErrorActionPreference = 'Stop'
$core = Join-Path $RepositoryRoot 'AIConflictCore/Scripts/Game/AIConflict'
$sources = @{}
foreach ($path in @('Config/AICF_InfantryRecruitmentConfig.c','Forces/AICF_InfantryRecruitmentService.c','Forces/AICF_InfantryRecruitmentOrder.c','Forces/AICF_RecruitmentPriority.c','Forces/AICF_InfantryRecruitSpawner.c','Economy/AICF_RecruitmentSupplyForecast.c')) {
    $sources[(Split-Path $path -Leaf)] = [IO.File]::ReadAllText((Join-Path $core $path))
}
$rules = @(
    @('AICF_InfantryRecruitmentConfig.c','PURCHASE_INTERVAL_MS = 5000;', 'FIVE_SECONDS'),
    @('AICF_InfantryRecruitmentOrder.c','(?s)void DelayPurchase\(int now\).*?now \+ AICF_InfantryRecruitmentConfig.PURCHASE_INTERVAL_MS', 'NO_CATCHUP'),
    @('AICF_InfantryRecruitmentOrder.c','m_iNextPurchaseAtMs > 0 && now >= m_iNextPurchaseAtMs', 'CLOCK_BOUNDARY'),
    @('AICF_RecruitmentPriority.c','(?s)first.m_bPlayerRequested != second.m_bPlayerRequested.*?return first.m_bPlayerRequested;.*?first.m_bAutoServing != second.m_bAutoServing.*?return first.m_bAutoServing;', 'MANUAL_THEN_OWNER'),
    @('AICF_InfantryRecruitmentService.c','(?s)AutoServiceOwner\(SCR_CampaignFaction faction\).*?order.m_Faction == faction.*?order.m_bAutoServing && IsServiceEligible\(order\)', 'FACTION_OWNER'),
    @('AICF_InfantryRecruitmentService.c','(?s)RefreshAutoService\(\).*?existing.m_bPlayerRequested \|\| !IsServiceEligible\(existing\).*?SortOrders\(\);.*?AutoServiceOwner\(order.m_Faction\).*?order.m_bAutoServing = true;', 'RELEASE_AND_SELECT'),
    @('AICF_InfantryRecruitmentService.c','(?s)CanFund\(.*?m_aOrders.Contains\(order\).*?order.IsCurrent\(order.m_Slot\).*?AutoServiceOwner\(order.m_Faction\) == order', 'DEBIT_OWNER_FENCE'),
    @('AICF_InfantryRecruitmentService.c','(?s)if \(!order.m_bPlayerRequested && AutoServiceOwner\(order.m_Faction\) != order\).*?return string.Empty;.*?if \(order.m_iNextPurchaseAtMs == 0\).*?order.DelayPurchase\(now\);.*?QuoteInfantryRecruit\(order\)', 'FIRST_STEP_AFTER_ADMISSION'),
    @('AICF_InfantryRecruitmentService.c','(?s)RecordRecruitedMember.*?ClearRecruit\(order\).*?order.DelayPurchase\(System.GetTickCount\(\)\);', 'STEP_AFTER_TRANSFER'),
    @('AICF_InfantryRecruitmentService.c','serving.m_Slot == slot && serving.m_iQueueSequence == queueSequence', 'REPLAN_OWNER'),
    @('AICF_InfantryRecruitmentService.c','(?s)return "VISIT_TIMEOUT";.*?order.m_iSupplyWaitStartedAtMs > 0 &&.*?return "SUPPLY_WAIT_TIMEOUT";.*?if \(order.m_bAwaitingSupplyPlan\).*?AutoServiceOwner\(order.m_Faction\) != order', 'TIMEOUT_BEFORE_QUEUE_WAIT'),
    @('AICF_InfantryRecruitSpawner.c','CanFund\(order\) \|\| !order.IsPurchaseDue\(System.GetTickCount\(\)\)', 'SPAWN_OWNER_CLOCK_FENCE'),
    @('AICF_RecruitmentSupplyForecast.c','(?s)EvaluateSequential.*?EvaluateJob\(earlier, stocks, now, remainingSeconds\).*?m_fQueueSeconds = now;.*?EvaluateJob\(this, stocks, now, remainingSeconds\)', 'QUEUE_FORECAST'),
    @('AICF_RecruitmentSupplyForecast.c','existing.m_Base == job.m_Base', 'SHARED_BASE_LEDGER')
)
function Test-Rules($inputSources) {
    foreach ($rule in $rules) { if ($inputSources[$rule[0]] -notmatch $rule[1]) { $rule[2] } }
}
$failures = @(Test-Rules $sources)
if ($failures.Count) { $failures; exit 1 }
$mutations = @(
    @('AICF_InfantryRecruitmentConfig.c','PURCHASE_INTERVAL_MS = 5000;','PURCHASE_INTERVAL_MS = 3000;', 'FIVE_SECONDS'),
    @('AICF_InfantryRecruitmentOrder.c','now + AICF_InfantryRecruitmentConfig.PURCHASE_INTERVAL_MS','m_iNextPurchaseAtMs + 5000', 'NO_CATCHUP'),
    @('AICF_InfantryRecruitmentOrder.c','m_iNextPurchaseAtMs > 0 && now >= m_iNextPurchaseAtMs','true', 'CLOCK_BOUNDARY'),
    @('AICF_RecruitmentPriority.c','return first.m_bAutoServing;','return false;', 'MANUAL_THEN_OWNER'),
    @('AICF_InfantryRecruitmentService.c','order.m_Faction == faction','true', 'FACTION_OWNER'),
    @('AICF_InfantryRecruitmentService.c','!IsServiceEligible(existing)','false', 'RELEASE_AND_SELECT'),
    @('AICF_InfantryRecruitmentService.c','m_aOrders.Contains(order)','true', 'DEBIT_OWNER_FENCE'),
    @('AICF_InfantryRecruitmentService.c','if (order.m_iNextPurchaseAtMs == 0)','if (false)', 'FIRST_STEP_AFTER_ADMISSION'),
    @('AICF_InfantryRecruitmentService.c','order.DelayPurchase(System.GetTickCount());','', 'STEP_AFTER_TRANSFER'),
    @('AICF_InfantryRecruitmentService.c','serving.m_iQueueSequence == queueSequence','true', 'REPLAN_OWNER'),
    @('AICF_InfantryRecruitmentService.c','order.m_iSupplyWaitStartedAtMs > 0 &&','false &&', 'TIMEOUT_BEFORE_QUEUE_WAIT'),
    @('AICF_InfantryRecruitSpawner.c','!order.IsPurchaseDue(System.GetTickCount())','false', 'SPAWN_OWNER_CLOCK_FENCE'),
    @('AICF_RecruitmentSupplyForecast.c','EvaluateJob(earlier, stocks, now, remainingSeconds)','true', 'QUEUE_FORECAST'),
    @('AICF_RecruitmentSupplyForecast.c','existing.m_Base == job.m_Base','true', 'SHARED_BASE_LEDGER')
)
foreach ($mutation in $mutations) {
    $changed = $sources.Clone()
    if (!$changed[$mutation[0]].Contains($mutation[1])) { throw "Missing mutation: $($mutation[3])" }
    $changed[$mutation[0]] = $changed[$mutation[0]].Replace($mutation[1], $mutation[2])
    if ($mutation[3] -notin @(Test-Rules $changed)) { throw "Mutation escaped: $($mutation[3])" }
}
Write-Output "PASS sequence contracts=$($rules.Count) mutations=$($mutations.Count); runtime=NOT_RUN"
