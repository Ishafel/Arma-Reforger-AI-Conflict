param([string]$RepositoryRoot = (Split-Path -Parent (Split-Path -Parent $PSScriptRoot)))
$ErrorActionPreference = 'Stop'
$core = Join-Path $RepositoryRoot 'AIConflictCore/Scripts/Game/AIConflict'
$sources = @{}
foreach ($name in @('Forces/AICF_RecruitmentPriority.c','Forces/AICF_InfantryRecruitmentService.c','Forces/AICF_InfantryRecruitmentOrder.c','Economy/AICF_InfantryRecruitmentEconomy.c')) {
    $sources[(Split-Path $name -Leaf)] = [IO.File]::ReadAllText((Join-Path $core $name))
}
$rules = @(
    @('AICF_RecruitmentPriority.c','(?s)if \(playerRequested\)\s*return 0;.*?slot.GetRole\(\) == AICF_EGroupRole.ATTACK.*?return 1;.*?return 2;', 'SOURCE_AND_LIVE_ROLE'),
    @('AICF_RecruitmentPriority.c','first.m_iQueueSequence < second.m_iQueueSequence', 'FIFO'),
    @('AICF_InfantryRecruitmentService.c','(?s)ConsiderFaction\(us, usFaction\);.*?SortOrders\(\);.*?Tick\(order, graphReady\)', 'ADMIT_BEFORE_PURCHASE'),
    @('AICF_InfantryRecruitmentService.c','(?s)void ConsiderFaction.*?FindMissingMember\(slot, faction, prefab, role, memberIndex\).*?ScheduleRetry\(slot\);', 'NO_FULL_ROSTER_RETRY'),
    @('AICF_InfantryRecruitmentService.c','(?s)if \(reason == "ROSTER_COMPLETE_OR_EMPTY"\).*?m_aRetrySlots.Remove\(retry\);.*?m_aRetryAtMs.Remove\(retry\);', 'COMPLETE_RETRY_RELEASE'),
    @('AICF_InfantryRecruitmentService.c','(?s)active.m_bPlayerRequested = true;.*?INFANTRY_RECRUITMENT_PRIORITY_CHANGED', 'EXPLICIT_PROMOTION'),
    @('AICF_InfantryRecruitmentService.c','(?s)active.m_bPlayerRequested = true;.*?slot.BeginPlayerStrategicOrder\(\);.*?slot.SetRecruitmentOrder\(active\);', 'PENDING_MANUAL_AUTHORITY'),
    @('AICF_InfantryRecruitmentService.c','!AICF_RecruitmentPriority.Precedes\(order, requester\)', 'PRIORITY_DEMAND'),
    @('AICF_InfantryRecruitmentService.c','(?s)pending.m_bAwaitingSupplyPlan = true;.*?if \(!best\)\s*return pending;', 'DEFERRED_DEMAND'),
    @('AICF_InfantryRecruitmentService.c','(?s)OtherDemand.*?order.m_bDemandReleased.*?!order.IsCurrent\(order.m_Slot\).*?!order.HasSafeBarracks\(\).*?!IsDemandWithinDeadline\(order\)', 'LIVE_DEMAND'),
    @('AICF_InfantryRecruitmentService.c','(?s)remaining, order.m_iQueueSequence, false\).*?alternative.m_iStartedAtMs = order.m_iStartedAtMs', 'REPLAN_FIFO'),
    @('AICF_InfantryRecruitmentService.c','(?s)ahead > 0 && order.m_Base.GetSupplies\(\) - ahead < order.m_iCost.*?if \(order.m_Donor\).*?!m_Spawner.ClearRecruit\(order\).*?INFANTRY_RECRUIT_SPAWN_CANCELLED.*?SUPPLY_WAIT_TIMEOUT', 'PREEMPT_UNPAID_DONOR'),
    @('AICF_InfantryRecruitmentService.c','(?s)void Finish.*?m_bDemandReleased = true;.*?if \(!order.m_bAwaitingSupplyPlan\)\s*m_Planner.EndInfantryRecruitment', 'PENDING_WAYPOINT_OWNERSHIP'),
    @('AICF_InfantryRecruitmentEconomy.c','(?s)QuoteInfantryRecruit.*?order.m_RecruitmentService && order.m_RecruitmentService.CanFund\(order\).*?DebitInfantryRecruit.*?!QuoteInfantryRecruit\(order\) \|\| !order.IsPhysicallyPresent\(\).*?AddSupplies\(-order.m_iCost\)', 'DEBIT_PRIORITY_GATE'),
    @('AICF_InfantryRecruitmentOrder.c','source=%2 priority=%3 queue=%4', 'DIAGNOSTICS')
)
function Test-Rules($inputSources) {
    foreach ($rule in $rules) {
        if ($inputSources[$rule[0]] -notmatch $rule[1]) { $rule[2] }
    }
}
$failures = @(Test-Rules $sources)
if ($failures.Count) { $failures; exit 1 }
$mutations = @(
    @('AICF_RecruitmentPriority.c','if (playerRequested)','if (slot.HasPlayerStrategicIntent())','SOURCE_AND_LIVE_ROLE'),
    @('AICF_RecruitmentPriority.c','first.m_iQueueSequence < second.m_iQueueSequence','first.m_iQueueSequence > second.m_iQueueSequence','FIFO'),
    @('AICF_InfantryRecruitmentService.c','ConsiderFaction(us, usFaction);','','ADMIT_BEFORE_PURCHASE'),
    @('AICF_InfantryRecruitmentService.c','FindMissingMember(slot, faction, prefab, role, memberIndex)','REMOVED','NO_FULL_ROSTER_RETRY'),
    @('AICF_InfantryRecruitmentService.c','m_aRetrySlots.Remove(retry);','','COMPLETE_RETRY_RELEASE'),
    @('AICF_InfantryRecruitmentService.c','active.m_bPlayerRequested = true;','','EXPLICIT_PROMOTION'),
    @('AICF_InfantryRecruitmentService.c','slot.SetRecruitmentOrder(active);','','PENDING_MANUAL_AUTHORITY'),
    @('AICF_InfantryRecruitmentService.c','!AICF_RecruitmentPriority.Precedes(order, requester)','false','PRIORITY_DEMAND'),
    @('AICF_InfantryRecruitmentService.c','pending.m_bAwaitingSupplyPlan = true;','','DEFERRED_DEMAND'),
    @('AICF_InfantryRecruitmentService.c','!order.IsCurrent(order.m_Slot)','false','LIVE_DEMAND'),
    @('AICF_InfantryRecruitmentService.c','!IsDemandWithinDeadline(order)','false','LIVE_DEMAND'),
    @('AICF_InfantryRecruitmentService.c','remaining, order.m_iQueueSequence, false','remaining, 0, false','REPLAN_FIFO'),
    @('AICF_InfantryRecruitmentService.c','!m_Spawner.ClearRecruit(order)','false','PREEMPT_UNPAID_DONOR'),
    @('AICF_InfantryRecruitmentService.c','if (!order.m_bAwaitingSupplyPlan)','if (true)','PENDING_WAYPOINT_OWNERSHIP'),
    @('AICF_InfantryRecruitmentEconomy.c','order.m_RecruitmentService.CanFund(order)','true','DEBIT_PRIORITY_GATE')
)
foreach ($mutation in $mutations) {
    $changed = $sources.Clone()
    if (!$changed[$mutation[0]].Contains($mutation[1])) { throw "Missing mutation input: $($mutation[3])" }
    $changed[$mutation[0]] = $changed[$mutation[0]].Replace($mutation[1], $mutation[2])
    if ($mutation[3] -notin @(Test-Rules $changed)) { throw "Mutation escaped: $($mutation[3])" }
}
Write-Output "PASS recruitment priority contracts=$($rules.Count) negative_mutations=$($mutations.Count); runtime=NOT_RUN"
