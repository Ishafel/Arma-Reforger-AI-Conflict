param([string]$RepositoryRoot = (Split-Path -Parent $PSScriptRoot))
$ErrorActionPreference = 'Stop'
$evidence = Join-Path $RepositoryRoot '.codex-runtime/logistics-contracts'
[IO.Directory]::CreateDirectory($evidence) | Out-Null
$fixture = Get-Content -LiteralPath (Join-Path $RepositoryRoot 'tools/fixtures/logistics/physical-delivery.fixture') -Raw
$logAudit = Join-Path $RepositoryRoot 'tools/Test-LogisticsLog.ps1'
$checks = 0
function Check-Log([string]$name, [string]$text, [int]$expected, [string]$rule) {
    $path = Join-Path $evidence "$name.fixture"
    [IO.File]::WriteAllText($path, $text)
    $result = & powershell.exe -NoProfile -ExecutionPolicy Bypass -File $logAudit -LogPath $path -RequireDelivery -RequirePolicy -RequireLedger -RequireGraph -RequireSearch 2>&1
    if ($LASTEXITCODE -ne $expected -or ($rule -and ($result -join "`n") -notmatch $rule)) { throw "Contract $name unexpected result: $result" }
    $script:checks++
}
Check-Log 'physical-fractional-partial-release' $fixture 0 ''
Check-Log 'nonfinite' ($fixture.Replace('loaded=100.125','loaded=NaN')) 1 'NONFINITE'
Check-Log 'double-release-hidden-zero' ($fixture.Replace('released=25.125','released=50.25')) 1 'BALANCE_CONSERVATION'
Check-Log 'unmatched-pair' ($fixture.Replace('from_after=800.375','from_after=800')) 1 'PAIR_CONSERVATION'
Check-Log 'pending-discrepancy' ($fixture.Replace('discrepancy=0','discrepancy=0.5')) 1 'DISCREPANCY'
Check-Log 'wrong-generation' ($fixture.Replace('phase=LOADING operation','phase=LOADING generation=2 operation')) 1 'WORK_BEFORE_EXACT_READY'
$load = ($fixture -split "`n" | Where-Object { $_ -match 'LOGISTICS_LOAD_COMMITTED' }) -join "`n"
Check-Log 'duplicate-operation' ($fixture.Replace('ENGINE : Game destroyed.', $load + "`nENGINE : Game destroyed.")) 1 'DUPLICATE_OPERATION'
Check-Log 'late-transfer' ($fixture.Replace('[AICF][STAGE4][INFO][LOGISTICS_JOB_RESERVED]', "[AICF][STAGE4][INFO][LOGISTICS_STOP]`n[AICF][STAGE4][INFO][LOGISTICS_JOB_RESERVED]")) 1 'WORK_AFTER_STOP'
Check-Log 'active-fragment' ($fixture.Replace('ENGINE : Game destroyed.','')) 1 'FULL_STOPPED_LOG_REQUIRED'
Check-Log 'missing-roster' ($fixture.Replace('[STAGE1][INFO][ROSTER_READY]','[STAGE1][INFO][ROSTER_PENDING]')) 1 'WORK_BEFORE_ROSTER_READY'
Check-Log 'unknown-resource' ($fixture.Replace('discrepancy=0','discrepancy=0 unknown_state=1')) 1 'UNKNOWN_RESOURCE_STATE'
Check-Log 'unknown-custody-with-zero-balance' ($fixture.Replace('discrepancy=0','discrepancy=0 fault=1')) 1 'CARGO_CUSTODY_FAULT'
Check-Log 'ledger-contract-missing' ($fixture.Replace('passed=18 total=18','passed=17 total=18')) 1 'PRODUCTION_LEDGER_CONTRACT_NOT_OBSERVED'
Check-Log 'graph-contract-missing' ($fixture.Replace('passed=12 total=12','passed=11 total=12')) 1 'PRODUCTION_GRAPH_CONTRACT_NOT_OBSERVED'
Check-Log 'search-contract-missing' ($fixture.Replace('passed=6 total=6','passed=5 total=6')) 1 'PRODUCTION_SEARCH_CONTRACT_NOT_OBSERVED'
Check-Log 'native-bind-error' ($fixture.Replace('ENGINE : Game destroyed.','NETWORK (E): Unable to start replication')) 1 'ENGINE_OR_AICF_ERROR'
Check-Log 'self-transfer' ($fixture.Replace('from_pool=A to_pool=V','from_pool=V to_pool=V')) 1 'SELF_TRANSFER'

$clientFixture = Get-Content -LiteralPath (Join-Path $RepositoryRoot 'tools/fixtures/logistics/client-loaded.fixture') -Raw
$serverFixturePath = Join-Path $evidence 'client-server.fixture'
[IO.File]::WriteAllText($serverFixturePath, $fixture)
$clientCases = @(
    @('client-loaded', $clientFixture, 0, ''),
    @('client-wrong-rpl', $clientFixture.Replace('vehicle_rpl=VR1','vehicle_rpl=VR2'), 1, 'CLIENT_EXACT_IDENTITY'),
    @('client-wrong-generation', $clientFixture.Replace('generation=1','generation=2'), 1, 'CLIENT_EXACT_IDENTITY'),
    @('client-nonfinite', $clientFixture.Replace('cargo=100.125','cargo=NaN'), 1, 'NONFINITE'),
    @('client-empty-only', $clientFixture.Replace('cargo=100.125','cargo=0'), 1, 'LOADED_CLIENT_REPLICA_NOT_OBSERVED'),
    @('client-active-fragment', $clientFixture.Replace('ENGINE : Game destroyed.',''), 1, 'FULL_STOPPED_CLIENT_LOG_REQUIRED')
)
foreach ($case in $clientCases) {
    $clientPath = Join-Path $evidence ($case[0] + '.fixture')
    [IO.File]::WriteAllText($clientPath, $case[1])
    $result = & powershell.exe -NoProfile -ExecutionPolicy Bypass -File $logAudit -LogPath $serverFixturePath -ClientLogPath $clientPath -RequireDelivery -RequireLoadedClient 2>&1
    if ($LASTEXITCODE -ne $case[2] -or ($case[3] -and ($result -join "`n") -notmatch $case[3])) { throw "Client contract $($case[0]) unexpected result: $result" }
    $checks++
}

# Отрицательные source fixtures вызывают тот же production static auditor.
$sourceRoot = Join-Path $evidence 'source'
$relativeCore = 'AIConflictCore/Scripts/Game/AIConflict'
$originalCore = Join-Path $RepositoryRoot $relativeCore
$targetCore = Join-Path $sourceRoot $relativeCore
foreach ($directory in @('Config','Economy','Vehicles','State/Vehicles')) {
    [IO.Directory]::CreateDirectory((Join-Path $targetCore $directory)) | Out-Null
    Get-ChildItem -LiteralPath (Join-Path $originalCore $directory) -Filter '*.c' -File | Copy-Item -Destination (Join-Path $targetCore $directory)
}
$mutations = @(
    @('Config/AICF_LogisticsConfig.c','Decimal(text, false, parsed)','true','CONFIG_STRICT_PARSE'),
    @('Economy/AICF_LogisticsPlanner.c','actual < capacity * below / 100','actual <= capacity * below / 100','THRESHOLD_STRICT'),
    @('Economy/AICF_LogisticsPlanner.c','search.m_aEndpoints[search.m_iSourceCursor++]','search.m_aEndpoints[0]','RESUMABLE_CANDIDATE_PREPARATION'),
    @('Economy/AICF_LogisticsPlanner.c','!search.m_bPrepared && !PrepareCandidates(w)','false','PREPARATION_PENDING_NOT_FAILURE'),
    @('Economy/AICF_LogisticsLedger.c','if (receipt.m_bAccounted) return false;','// removed','RECEIPT_EXACTLY_ONCE'),
    @('Economy/AICF_LogisticsJob.c','m_Seat.GetOccupant() != m_Driver','false','EXACT_PILOT'),
    @('Economy/AICF_LogisticsJob.c','occupant && occupant != m_Driver','occupant != null','OWN_DRIVER_NOT_FOREIGN_OCCUPANT'),
    @('Economy/AICF_LogisticsResourcePool.c','actual.Valid() && expected.Same(actual)','actual.Valid()','OPERATION_EXACT_POOL'),
    @('Economy/AICF_LogisticsResourcePool.c','!operation.CanInteractWith(container)','false','OPERATION_RESOURCE_RIGHTS'),
    @('Economy/AICF_LogisticsResourcePool.c','!consumer.IsConsuming()','false','OPERATION_CONSUMING_STATE'),
    @('Economy/AICF_LogisticsResourcePool.c','generator.GetResourceMultiplier() != 1','consumer.GetBuyMultiplier() != 1','PHYSICAL_TRANSFER_NOT_TRADE_PRICE'),
    @('Economy/AICF_LogisticsDepotRegistry.c','data.CanSpawnInSlot(slot.GetSlotType())','true','DEPOT_ALLOWED_SLOT'),
    @('Economy/AICF_LogisticsDepotRegistry.c','!w.m_Production.AICF_LogisticsSupports(entry)','false','PREFERENCE_SKIPS_INCOMPATIBLE_ENTRY'),
    @('Vehicles/AICF_VehicleCleanupManager.c','if (!AICF_LogisticsResourceAdapter.CanDeleteVehicle(vehicle))','if (false)','GLOBAL_CARGO_DELETE_GUARD')
)
foreach ($mutation in $mutations) {
    $path = Join-Path $targetCore $mutation[0]
    $original = Get-Content -LiteralPath $path -Raw
    if (-not $original.Contains($mutation[1])) { throw "Mutation target missing: $($mutation[3])" }
    [IO.File]::WriteAllText($path, $original.Replace($mutation[1],$mutation[2]))
    $result = & powershell.exe -NoProfile -ExecutionPolicy Bypass -File (Join-Path $RepositoryRoot 'tools/Test-LogisticsStatic.ps1') -RepositoryRoot $sourceRoot 2>&1
    $verdict = $LASTEXITCODE
    [IO.File]::WriteAllText($path, $original)
    if ($verdict -ne 1 -or ($result -join "`n") -notmatch $mutation[3]) { throw "Mutation survived: $($mutation[3]) $result" }
    $checks++
}
"PASS Logistics contracts: $checks positive/negative cases; planner formulas additionally require Enforce runtime policy probe"
