param([string]$RepositoryRoot = (Split-Path -Parent $PSScriptRoot))
$ErrorActionPreference = 'Stop'
$core = Join-Path $RepositoryRoot 'AIConflictCore/Scripts/Game/AIConflict'
$sources = @{
    Service = Get-Content (Join-Path $core 'Forces/AICF_InfantryRecruitmentService.c') -Raw
    Forecast = Get-Content (Join-Path $core 'Economy/AICF_RecruitmentSupplyForecast.c') -Raw
    Spawner = Get-Content (Join-Path $core 'Forces/AICF_InfantryRecruitSpawner.c') -Raw
    Economy = Get-Content (Join-Path $core 'Economy/AICF_InfantryRecruitmentEconomy.c') -Raw
}
function Test-Contracts($s) {
    $rules = @(
        @('Spawner', '(?s)QuoteMissingRoster.*?members < vacancies.*?HasRosterMember\(index\).*?ResolveRecruitPrefab\(faction, index, role\).*?prefab.IsEmpty\(\).*?return false;.*?cost \+= config.Cost\(role\)', 'FULL_ROLE_COST'),
        @('Service', '(?s)OtherDemand\(.*?order.m_bDemandReleased.*?order.m_Slot == excludedSlot.*?m_iGraphRevision != m_Graph.GetRevision\(\).*?!order.IsCurrent\(order.m_Slot\).*?!order.HasSafeBarracks\(\).*?CountAliveAgents\(order.m_Group\) <= 0.*?QuoteMissingRoster.*?total \+= cost', 'LIVE_DEMAND'),
        @('Service', '(?s)void Finish.*?m_bDemandReleased = true;.*?RefundInfantryRecruit.*?EndInfantryRecruitment.*?ClearRecruit', 'RELEASE_BEFORE_CLEANUP'),
        @('Service', '(?s)ReconsiderBarracks\(int index\).*?order.m_Donor.*?order.m_iSpawnAtMs > 0.*?REPLAN_COOLDOWN_MS.*?REPLAN_GAIN_SECONDS.*?REPLAN_GAIN_FRACTION.*?BeginInfantryRecruitment\(alternative\).*?alternative.m_iStartedAtMs = order.m_iStartedAtMs', 'BOUNDED_REPLAN'),
        @('Service', '(?s)if \(!order.IsPhysicallyPresent\(\)\).*?QuoteInfantryRecruit\(order\).*?SUPPLY_WAIT_TIMEOUT_MS.*?return "SUPPLY_WAIT_TIMEOUT"', 'BOUNDED_PHYSICAL_WAIT'),
        @('Service', '(?s)order.m_Forecast.Evaluate\(Math.Sqrt\(distanceSq\), remainingSeconds\).*?m_fCompletionSeconds >= bestCompletion', 'ETA_SELECTION_STABLE_TIE'),
        @('Forecast', 'm_iOwnDemand \+ m_iOtherDemand - m_fStock', 'SHARED_STOCK'),
        @('Forecast', 'Math.Ceil\(deficit / m_fIncome\)', 'DISCRETE_INCOME'),
        @('Forecast', 'm_fNextArrival \+ \(packages - 1\) \* m_fInterval', 'ACTUAL_ARRIVAL'),
        @('Forecast', 'Math.Max\(m_fTravelSeconds, m_fSupplySeconds\)', 'TRAVEL_AND_SUPPLIES'),
        @('Forecast', 'm_fIncome <= 0 \|\| m_fInterval <= 0', 'NO_INFINITE_INCOME'),
        @('Forecast', 'm_fWaitSeconds <= AICF_InfantryRecruitmentConfig.SUPPLY_HORIZON_SECONDS', 'HORIZON'),
        @('Forecast', '(?s)AICF_GetRecruitmentIncome.*?!Replication.IsServer\(\).*?IsHQRadioTrafficPossible.*?m_fRespawnAvailableSince.*?GetSuppliesReplenishThreshold.*?GetSuppliesIncome\(\).*?GetSuppliesArrivalTimer\(\).*?GetSuppliesArrivalTime\(\)', 'STOCK_READ_ONLY_INCOME'),
        @('Economy', '(?s)DebitInfantryRecruit.*?!QuoteInfantryRecruit\(order\) \|\| !order.IsPhysicallyPresent\(\).*?AddSupplies\(-order.m_iCost\)', 'TRANSACTION_UNCHANGED')
    )
    foreach ($rule in $rules) { if ($s[$rule[0]] -notmatch $rule[1]) { $rule[2] } }
    if ($s.Forecast -match '\b(AddSupplies|BumpMe|CalculateSupplyRegenerationAmount)\s*\(') { 'FORECAST_SIDE_EFFECT' }
}
$failures = @(Test-Contracts $sources)
if ($failures.Count) { $failures; exit 1 }
$mutations = @(
    @('Spawner','cost += config.Cost(role)','cost += 10','FULL_ROLE_COST'),
    @('Service','order.m_Slot == excludedSlot','false','LIVE_DEMAND'),
    @('Service','!order.IsCurrent(order.m_Slot)','false','LIVE_DEMAND'),
    @('Service','order.m_bDemandReleased = true;','','RELEASE_BEFORE_CLEANUP'),
    @('Service','alternative.m_iStartedAtMs = order.m_iStartedAtMs','alternative.m_iStartedAtMs = now','BOUNDED_REPLAN'),
    @('Forecast','m_iOwnDemand + m_iOtherDemand - m_fStock','m_iOwnDemand - m_fStock','SHARED_STOCK'),
    @('Forecast','Math.Ceil(deficit / m_fIncome)','deficit / m_fIncome','DISCRETE_INCOME'),
    @('Forecast','m_fNextArrival + (packages - 1) * m_fInterval','packages * m_fInterval','ACTUAL_ARRIVAL'),
    @('Forecast','m_fIncome <= 0 || m_fInterval <= 0','false','NO_INFINITE_INCOME'),
    @('Forecast','m_fWaitSeconds <= AICF_InfantryRecruitmentConfig.SUPPLY_HORIZON_SECONDS','true','HORIZON')
)
foreach ($mutation in $mutations) {
    $changed = $sources.Clone()
    $changed[$mutation[0]] = $changed[$mutation[0]].Replace($mutation[1], $mutation[2])
    if (@(Test-Contracts $changed) -notcontains $mutation[3]) { throw "Mutation escaped: $($mutation[3])" }
    Write-Output "PASS mutation=$($mutation[3])"
}
Write-Output 'Recruitment supply contracts: PASS; mutations=10; runtime=NOT_RUN'
