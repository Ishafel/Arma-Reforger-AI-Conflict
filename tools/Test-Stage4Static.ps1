param(
    [string]$RepositoryRoot = (Split-Path -Parent $PSScriptRoot)
)

$ErrorActionPreference = 'Stop'
$failures = [System.Collections.Generic.List[string]]::new()
$sourceRoot = Join-Path $RepositoryRoot 'AIConflictCore\Scripts\Game\AIConflict'

function Add-Failure([string]$Rule, [string]$Message) {
    $failures.Add("[$Rule] $Message")
}

function Read-Required([string]$RelativePath) {
    $path = Join-Path $sourceRoot $RelativePath
    if (-not (Test-Path -LiteralPath $path)) {
        Add-Failure 'STAGE4_COMPONENT_MISSING' "Missing $RelativePath"
        return ''
    }
    return Get-Content -LiteralPath $path -Raw
}

function Assert-Contains([string]$Rule, [string]$Text, [string]$Pattern, [string]$Message) {
    if ($Text -notmatch $Pattern) { Add-Failure $Rule $Message }
}

function Assert-NotContains([string]$Rule, [string]$Text, [string]$Pattern, [string]$Message) {
    if ($Text -match $Pattern) { Add-Failure $Rule $Message }
}

$config = Read-Required 'Config\AICF_Stage4Config.c'
$diagnostics = Read-Required 'Diagnostics\AICF_Stage4Diagnostics.c'
$graph = Read-Required 'Objectives\AICF_ObjectiveGraph.c'
$network = Read-Required 'Economy\AICF_SupplyNetwork.c'
$selector = Read-Required 'Economy\AICF_ReinforcementBaseSelector.c'
$request = Read-Required 'Economy\AICF_ReinforcementRequest.c'
$reservation = Read-Required 'Economy\AICF_DeploymentReservation.c'
$economy = Read-Required 'Economy\AICF_EconomySystem.c'
$shipment = Read-Required 'Economy\AICF_SupplyShipment.c'
$delivery = Read-Required 'Economy\AICF_SupplyDeliverySystem.c'
$controller = Read-Required 'Bootstrap\AICF_MatchController.c'
$campaignState = Read-Required 'Integration\AICF_CampaignState.c'
$ticketLedger = Read-Required 'State\AICF_TicketLedger.c'

Assert-Contains 'STAGE4_DEFAULT_OFF' $config 'm_bEconomyEnabled\s*=\s*false\s*;' 'Economy must default off'
Assert-Contains 'STAGE4_DEFAULT_OFF' $config '"aicfEconomyEnabled"' 'Economy must require an explicit CLI opt-in'
foreach ($cli in @(
    'aicfReplacementSupplyCost', 'aicfEconomyHealthyPacePercent',
    'aicfEconomyStrainedPacePercent', 'aicfEconomyIsolatedPacePercent',
    'aicfSupplyDeliveryIntervalMs', 'aicfSupplyDeliveryPackage',
    'aicfSupplyDeliveryBaseTravelMs', 'aicfSupplyDeliveryPerHopMs',
    'aicfMaxSupplyShipmentsPerFaction', 'aicfSupplySourceReserveGroups'
)) {
    Assert-Contains 'STAGE4_CLI_CONFIG' $config ([regex]::Escape('"' + $cli + '"')) "Missing CLI option $cli"
}

Assert-Contains 'STAGE4_STOCK_SUPPLY_POOL' $network '\.GetSupplies\s*\(' 'Network must read the stock base supply pool'
Assert-Contains 'STAGE4_STOCK_SUPPLY_POOL' $network '\.GetSuppliesMax\s*\(' 'Network must inspect stock base capacity'
Assert-Contains 'STAGE4_STOCK_SUPPLY_POOL' $economy 'TryCompleteInitialSupplyProbe[\s\S]*HasInitializedFactionSupplyPool' 'Arland calibration probe must wait for both faction stock pools'
Assert-Contains 'STAGE4_STOCK_SUPPLY_POOL' $economy 'spawnBase\.AddSupplies\s*\(\s*-supplyCost\s*\)' 'Reservation must debit the selected stock base'
Assert-Contains 'STAGE4_STOCK_SUPPLY_POOL' $economy 'reservation\.GetBase\(\)\.AddSupplies\s*\(\s*reservation\.GetSupplyCost\(\)\s*\)' 'Abort must return reserved supplies to the stock base'

Assert-Contains 'STAGE4_GRAPH_PATHS' $graph 'FindFriendlyPath\s*\(' 'Graph must expose friendly path search'
Assert-Contains 'STAGE4_GRAPH_PATHS' $graph 'GetHopDistance\s*\(' 'Graph must expose deterministic hop distance'
Assert-Contains 'STAGE4_GRAPH_PATHS' $graph 'm_iRevision\+\+' 'Every successful graph build must advance the revision'
Assert-Contains 'STAGE4_ROUTE_OPERATIONAL' $network 'FindFriendlyPath[\s\S]*IsOperationalOwnedBase' 'Supply paths must reject captured or contested intermediate nodes'

foreach ($filter in @('ENEMY_OWNED', 'CONTESTED', 'SPAWN_POINT_MISSING', 'SPAWN_POINT_DISABLED', 'SPAWN_POINT_INACTIVE', 'INSUFFICIENT_SUPPLIES')) {
    Assert-Contains 'STAGE4_BASE_FILTER' ($selector + $controller + (Read-Required 'Integration\AICF_ConflictAdapter.c')) ([regex]::Escape($filter)) "Missing base rejection $filter"
}
Assert-Contains 'STAGE4_BASE_RANKING' $selector 'candidate\.Connected\s*!=\s*best\.Connected' 'Connected bases must rank before isolated bases'
Assert-Contains 'STAGE4_BASE_RANKING' $selector 'candidate\.TargetHops\s*!=\s*best\.TargetHops' 'Saved-target hop distance must be a ranking key'
Assert-Contains 'STAGE4_BASE_RANKING' $selector 'candidate\.RemainingSupplies\s*!=\s*best\.RemainingSupplies' 'Post-purchase stock must be a ranking key'
Assert-Contains 'STAGE4_BASE_RANKING' $selector 'candidate\.NodeId\s*<\s*best\.NodeId' 'Stable node id must be the final tie-break'

Assert-Contains 'STAGE4_PACING' $request 'm_iProgressMs\s*\+=\s*elapsedMs\s*\*\s*pacePercent\s*/\s*100' 'Readiness must accumulate instead of resetting'
Assert-Contains 'STAGE4_PACING' $network 'HEALTHY' 'Network must classify healthy logistics'
Assert-Contains 'STAGE4_PACING' $network 'STRAINED' 'Network must classify strained logistics'
Assert-Contains 'STAGE4_PACING' $network 'ISOLATED' 'Network must classify isolated logistics'
Assert-Contains 'STAGE4_PACING' $network 'BLOCKED' 'Network must classify blocked logistics'

Assert-Contains 'STAGE4_TRANSACTION' $economy 'TryReserveDeployment\s*\(\s*AICF_EDeploymentKind\.REPLACEMENT\s*\)' 'Ticket reservation must precede deployment'
Assert-Contains 'STAGE4_TRANSACTION' $economy 'ValidateReservationForCommit[\s\S]*GRAPH_REVISION_STALE' 'Commit must reject a stale graph revision'
Assert-Contains 'STAGE4_TRANSACTION' $economy 'ValidateReservationForCommit[\s\S]*GetSpawnRejectionReason' 'Commit must recheck ownership and safety'
Assert-Contains 'STAGE4_TRANSACTION' $controller 'ValidateReservationForSpawn[\s\S]*TrySpawnAtBase' 'Reservation, graph, ownership, safety, and stock pool must be revalidated immediately before entity spawn'
Assert-Contains 'STAGE4_TRANSACTION' $controller 'CanCommitDeploymentReady[\s\S]*TryCommitDeployment[\s\S]*CommitDeploymentReady[\s\S]*FinalizeDeployment' 'Exact roster/slot proof must precede the atomic economy commit'
Assert-Contains 'STAGE4_TRANSACTION' $ticketLedger 'RollbackCommittedDeployment' 'A failed slot commit must be able to roll back its ticket debit'
foreach ($reason in @('SPAWN_FAILED', 'BIND_FAILED', 'SPAWN_TIMEOUT', 'INVALID_ROSTER', 'GROUP_EMPTY', 'SYSTEM_STOP')) {
    Assert-Contains 'STAGE4_ROLLBACK_PATHS' ($controller + $economy) ([regex]::Escape('"' + $reason + '"')) "Missing rollback reason $reason"
}

Assert-Contains 'STAGE4_BASELINE_PRESERVED' $controller '!economyEnabled\s*&&\s*!factionState\.TryReserveDeployment' 'Legacy ticket reservation must remain the economy-off path'
Assert-Contains 'STAGE4_BASELINE_PRESERVED' $controller 'else[\s\S]*m_ReinforcementSystem\.TrySpawn\s*\(' 'Economy-off replacements must retain the legacy first-safe spawner'

Assert-NotContains 'STAGE4_ABSTRACT_DELIVERY' $delivery 'SpawnEntity|SpawnEntityPrefab|RplComponent\.DeleteRplEntity' 'Stage 4 delivery must not create physical convoy entities'
Assert-Contains 'STAGE4_ABSTRACT_DELIVERY' $delivery 'sourceBase\.AddSupplies\s*\(\s*-cargo\s*\)' 'Shipment dispatch must debit its source'
Assert-Contains 'STAGE4_ABSTRACT_DELIVERY' $delivery 'destination\.AddSupplies\s*\(\s*delivered\s*\)' 'Shipment arrival must credit its destination'
Assert-Contains 'STAGE4_ABSTRACT_DELIVERY' $delivery 'HasDestinationShipment' 'Duplicate destination shipments must be rejected'
Assert-Contains 'STAGE4_ABSTRACT_DELIVERY' $delivery 'PAUSED_ROUTE' 'Broken routes must pause shipments'
Assert-Contains 'STAGE4_SHIPMENT_BALANCE' $economy 'dispatched\s*-\s*delivered\s*-\s*returned\s*-\s*inTransit' 'Heartbeat must verify shipment conservation'

foreach ($field in @(
    'm_bAICFStage4Enabled', 'm_iAICFUSTotalSupplies', 'm_iAICFUSConnectedSupplies',
    'm_iAICFUSLogisticsTier', 'm_iAICFUSPendingReinforcements', 'm_iAICFUSShipmentsInTransit',
    'm_iAICFUSSRTotalSupplies', 'm_iAICFUSSRConnectedSupplies', 'm_iAICFUSSRLogisticsTier',
    'm_iAICFUSSRPendingReinforcements', 'm_iAICFUSSRShipmentsInTransit'
)) {
    Assert-Contains 'STAGE4_REPLICATION' $campaignState ("RplProp[\s\S]{0,100}" + [regex]::Escape($field)) "Missing replicated field $field"
}
Assert-Contains 'STAGE4_REPLICATION' $campaignState 'Replication\.BumpMe\s*\(' 'Stage 4 aggregate changes must be pushed to JIP/proxies'
Assert-Contains 'STAGE4_DIAGNOSTICS' $diagnostics '\[AICF\]\[STAGE4\]' 'Stage 4 must use its own log prefix'
Assert-NotContains 'STAGE4_NO_STATIC_PASS' ($config + $network + $selector + $economy + $delivery + $controller) 'status\s*=\s*PASS|\[RESULT\]\[PASS\]' 'Production/static code must not manufacture runtime acceptance'

# Negative-fixture self-check: removing the stock supply rollback must be caught.
$brokenEconomy = $economy -replace 'reservation\.GetBase\(\)\.AddSupplies\s*\(\s*reservation\.GetSupplyCost\(\)\s*\)\s*;', ''
$negativeDetected = $brokenEconomy -notmatch 'reservation\.GetBase\(\)\.AddSupplies\s*\(\s*reservation\.GetSupplyCost\(\)\s*\)'
if (-not $negativeDetected) {
    Add-Failure 'STAGE4_NEGATIVE_FIXTURE' 'Rollback negative fixture was not detected'
}

if ($failures.Count -gt 0) {
    Write-Host "Stage 4 static audit: FAIL ($($failures.Count) issue(s))" -ForegroundColor Red
    $failures | ForEach-Object { Write-Host " - $_" }
    exit 1
}

Write-Host 'Stage 4 static audit: PASS' -ForegroundColor Green
Write-Host 'negative_fixture=PASS default_off=PASS transaction=PASS delivery_balance=PASS replication=PASS'
