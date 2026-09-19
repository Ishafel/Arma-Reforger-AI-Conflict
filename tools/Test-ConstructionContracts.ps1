[CmdletBinding()]
param([string]$EvidenceRoot)
$ErrorActionPreference = 'Stop'
$repo = Split-Path -Parent $PSScriptRoot
if (-not $EvidenceRoot) { $EvidenceRoot = Join-Path $repo ('.codex-runtime/construction-contracts-' + (Get-Date -Format 'yyyyMMdd-HHmmss')) }
New-Item -ItemType Directory -Path $EvidenceRoot -Force | Out-Null
$failures = [System.Collections.Generic.List[string]]::new()
$common = 'token=construction-1 faction=US base=base1 provider=provider1 type=SMALL_BARRACKS cost=250 supplies_before=1000 supplies_after=750 reserve=500 layout=layout1 props_cost=71 props_before=10 props_after=81'
function Event([string]$Name, [int]$Time, [string]$Extra='') { return "SCRIPT : [AICF][STAGE1][INFO][$Name] t_ms=$Time $common $Extra" }
$positive = @(
    'SCRIPT : [AICF][ROSTER_READY]',
    (Event 'CONSTRUCTION_DECISION' 60000),
    (Event 'CONSTRUCTION_SITE_SELECTED' 61000),
    (Event 'CONSTRUCTION_RESERVED' 61000 'supply_debited=0'),
    (Event 'CONSTRUCTION_PAYMENT' 61000 'debit_count=1 tickets=0 reservation_released=1'),
    (Event 'CONSTRUCTION_PLACED' 61000 'service_online=0'),
    'SCRIPT : [AICF][BUILDER_PROGRESS] t_ms=90000 target=layout1 tool_active=1 item_using=1',
    'SCRIPT : [AICF][BUILDER_COMPLETED] t_ms=93000 target=layout1 tool_active=1 item_using=1',
    (Event 'CONSTRUCTION_COMPLETED' 94000 'service_online=1'),
    'ENGINE : Game destroyed.'
) -join "`n"
function CheckLog([string]$Name, [string]$Content, [string]$ExpectedRule='', [string]$Mode='BOTH', [bool]$Completion=$true, [bool]$AllFactions=$false) {
    $path = Join-Path $EvidenceRoot ($Name + '.log')
    [IO.File]::WriteAllText($path, $Content)
    $arguments = @('-NoProfile','-ExecutionPolicy','Bypass','-File', (Join-Path $PSScriptRoot 'Test-ConstructionLog.ps1'), '-LogPath', $path, '-ExpectedMode', $Mode)
    if ($Completion) { $arguments += '-RequireCompletion' }
    if ($AllFactions) { $arguments += '-RequireAllFactions' }
    $result = & powershell.exe @arguments 2>&1
    $code = $LASTEXITCODE
    $result | Set-Content -LiteralPath (Join-Path $EvidenceRoot ($Name + '.txt'))
    $valid = $code -eq 0
    if ($ExpectedRule) { $valid = $code -eq 1 -and ($result -join "`n") -match [regex]::Escape("[$ExpectedRule]") }
    if (-not $valid) { $failures.Add($Name) }
    Write-Output "$Name exit=$code expected_rule=$ExpectedRule matched=$valid"
}
CheckLog 'positive-worker-service' $positive
CheckLog 'negative-double-payment' ($positive.Replace((Event 'CONSTRUCTION_PLACED' 61000 'service_online=0'), (Event 'CONSTRUCTION_PAYMENT' 61000 'debit_count=1 tickets=0 reservation_released=1') + "`n" + (Event 'CONSTRUCTION_PLACED' 61000 'service_online=0'))) 'CONSTRUCTION_PAYMENT_WITHOUT_RESERVATION'
CheckLog 'negative-debit-balance' ($positive.Replace('supplies_after=750','supplies_after=749')) 'CONSTRUCTION_DEBIT_BALANCE'
CheckLog 'negative-prop-balance' ($positive.Replace('props_after=81','props_after=152')) 'CONSTRUCTION_PROP_BALANCE'
CheckLog 'negative-deferred-prop-balance' ($positive.Replace('ENGINE : Game destroyed.', "SCRIPT : [AICF][CONSTRUCTION_DEFERRED_PROBE] accepted=1 props_current=152 props_expected=81 root_present=1`nENGINE : Game destroyed.")) 'CONSTRUCTION_DEFERRED_PROP_BALANCE'
CheckLog 'negative-premature-service' ($positive.Replace('service_online=0','service_online=1')) 'CONSTRUCTION_PREMATURE_SERVICE'
CheckLog 'negative-before-roster' ($positive.Replace('[ROSTER_READY]','[NOT_READY]')) 'CONSTRUCTION_BEFORE_ROSTER'
CheckLog 'negative-player-side' $positive 'CONSTRUCTION_PLAYER_SIDE' 'USSR'
CheckLog 'negative-unconfirmed-tool' ($positive.Replace('tool_active=1','tool_active=0')) 'CONSTRUCTION_NO_BUILDER_COMPLETION'
CheckLog 'negative-layout-identity' ($positive.Replace((Event 'CONSTRUCTION_COMPLETED' 94000 'service_online=1'), (Event 'CONSTRUCTION_COMPLETED' 94000 'service_online=1').Replace('layout=layout1','layout=layout2'))) 'CONSTRUCTION_LAYOUT_IDENTITY'
CheckLog 'negative-live-log' ($positive.Replace('Game destroyed.','Still running.')) 'CONSTRUCTION_LOG_NOT_STOPPED'
$rollback = @(
    'SCRIPT : [AICF][ROSTER_READY]', (Event 'CONSTRUCTION_DECISION' 60000),
    (Event 'CONSTRUCTION_SITE_SELECTED' 61000), (Event 'CONSTRUCTION_RESERVED' 61000 'supply_debited=0'),
    (Event 'CONSTRUCTION_ROLLBACK' 61000 'restored=1 stock_refund=0 reservation_released=1').Replace('supplies_after=750','supplies_after=1000').Replace('props_after=81','props_after=10'),
    (Event 'CONSTRUCTION_CANCELLED' 61000 'reservation_released=1'), 'ENGINE : Game destroyed.'
) -join "`n"
CheckLog 'positive-rollback' $rollback '' 'BOTH' $false
CheckLog 'negative-refund-balance' ($rollback.Replace('supplies_after=1000','supplies_after=999')) 'CONSTRUCTION_ROLLBACK_BALANCE' 'BOTH' $false
$matrixCases = [System.Collections.Generic.List[string]]::new()
$caseNumber = 0
foreach ($side in @('US','USSR')) {
    foreach ($type in @('SMALL_BARRACKS','ARMORY','LIGHT_DEPOT','LARGE_BARRACKS','HEAVY_DEPOT')) {
        $caseNumber++
        $case = $positive.Replace('token=construction-1', "token=matrix-$caseNumber").Replace('base=base1', "base=matrix-base-$caseNumber").Replace('layout1', "matrix-layout-$caseNumber").Replace('faction=US ', "faction=$side ").Replace('type=SMALL_BARRACKS', "type=$type")
        $offset = $caseNumber * 100000
        $case = [regex]::Replace($case, 't_ms=(\d+)', { param($match) 't_ms=' + ([int]$match.Groups[1].Value + $offset) })
        $matrixCases.Add($case)
    }
}
CheckLog 'positive-all-faction-types' ($matrixCases -join "`n") '' 'BOTH' $true $true
CheckLog 'negative-missing-faction-type' (($matrixCases | Select-Object -First 9) -join "`n") 'CONSTRUCTION_MISSING_FACTION_TYPE:USSR/HEAVY_DEPOT' 'BOTH' $true $true
CheckLog 'negative-completion-case-identity' ($positive.Replace((Event 'CONSTRUCTION_COMPLETED' 94000 'service_online=1'), (Event 'CONSTRUCTION_COMPLETED' 94000 'service_online=1').Replace('faction=US ', 'faction=USSR '))) 'CONSTRUCTION_COMPLETION_CASE_IDENTITY'
# Негативный static input — отдельная копия исходников; рабочая реализация не меняется.
$fixtureRepo = Join-Path $EvidenceRoot 'static-input'
$fixtureCore = Join-Path $fixtureRepo 'AIConflictCore/Scripts/Game/AIConflict'
New-Item -ItemType Directory -Path $fixtureCore -Force | Out-Null
foreach ($domain in @('Construction','Economy','Config','Command','Bootstrap','Vehicles')) {
    Copy-Item -LiteralPath (Join-Path $repo "AIConflictCore/Scripts/Game/AIConflict/$domain") -Destination $fixtureCore -Recurse -Force
}
$staticTool = Join-Path $PSScriptRoot 'Test-ConstructionStatic.ps1'
$result = & powershell.exe -NoProfile -ExecutionPolicy Bypass -File $staticTool -RepositoryRoot $fixtureRepo
if ($LASTEXITCODE -ne 0) { $failures.Add('static-positive') }
$result | Set-Content -LiteralPath (Join-Path $EvidenceRoot 'static-positive.txt')
$orderPath = Join-Path $fixtureCore 'Construction/AICF_ConstructionOrder.c'
$order = [IO.File]::ReadAllText($orderPath)
[IO.File]::WriteAllText($orderPath, $order.Replace('m_Provider.GetOwner().GetID() == m_ProviderId','true'))
$result = & powershell.exe -NoProfile -ExecutionPolicy Bypass -File $staticTool -RepositoryRoot $fixtureRepo
if ($LASTEXITCODE -ne 1 -or ($result -join "`n") -notmatch '\[CONSTRUCTION_PROVIDER_IDENTITY\]') { $failures.Add('static-negative-identity') }
$result | Set-Content -LiteralPath (Join-Path $EvidenceRoot 'static-negative-identity.txt')
[IO.File]::WriteAllText($orderPath, $order)
$plannerPath = Join-Path $fixtureCore 'Construction/AICF_ConstructionPlanner.c'
$planner = [IO.File]::ReadAllText($plannerPath)
foreach ($case in @(
    @{ Name='candidate-budget'; Before='m_iCandidatesThisTick < m_Config.m_iCandidatesPerTick'; After='true'; Rule='CONSTRUCTION_SHARED_CANDIDATE_BUDGET' },
    @{ Name='metadata-budget'; Before='if (m_bMetadataBatchThisTick)'; After='if (false)'; Rule='CONSTRUCTION_SHARED_METADATA_BUDGET' },
    @{ Name='placement-budget'; Before='if (m_bPlacementAttemptedThisTick)'; After='if (false)'; Rule='CONSTRUCTION_SINGLE_PLACEMENT_ATTEMPT' },
    @{ Name='checkpoint-bound'; Before='order.m_aPendingCandidates.Count() >= 8'; After='false'; Rule='CONSTRUCTION_CHECKPOINT_BOUND' },
    @{ Name='search-window-bound'; Before='order.m_iSearchWindows < 3'; After='true'; Rule='CONSTRUCTION_UNFINISHED_SEARCH_RETRY' },
    @{ Name='live-claim-refresh'; Before='AICF_ConstructionSiteSearch.RefreshClaim(order.m_sToken);'; After=''; Rule='CONSTRUCTION_COMMIT_CLAIM_LIFETIME' }
)) {
    [IO.File]::WriteAllText($plannerPath, $planner.Replace($case.Before, $case.After))
    $result = & powershell.exe -NoProfile -ExecutionPolicy Bypass -File $staticTool -RepositoryRoot $fixtureRepo
    if ($LASTEXITCODE -ne 1 -or ($result -join "`n") -notmatch [regex]::Escape('[' + $case.Rule + ']')) { $failures.Add('static-negative-' + $case.Name) }
    $result | Set-Content -LiteralPath (Join-Path $EvidenceRoot ('static-negative-' + $case.Name + '.txt'))
}
[IO.File]::WriteAllText($plannerPath, $planner)
$searchPath = Join-Path $fixtureCore 'Construction/AICF_ConstructionSiteSearch.c'
$search = [IO.File]::ReadAllText($searchPath)
foreach ($case in @(
    @{ Name='terrain-row'; Before='order.m_aTerrainHeights[sample - columns]'; After='point[1]'; Rule='CONSTRUCTION_LOCAL_TERRAIN_SLOPE' },
    @{ Name='terrain-completion-envelope'; Before='check.m_fMaxHeight = receipt.m_fMaxHeight;'; After='check.m_fMaxHeight = 0;'; Rule='CONSTRUCTION_TERRAIN_COLLISION_ENVELOPE' },
    @{ Name='candidate-resume'; Before='if (!order.m_bCandidateLiveChecked)'; After='if (false)'; Rule='CONSTRUCTION_CANDIDATE_BUDGET_RESUME' },
    @{ Name='live-budget-reserved'; Before='s_iQueries + count + reserved > s_iLimit'; After='false'; Rule='CONSTRUCTION_LIVE_BUDGET_RESERVATION' },
    @{ Name='commit-inventory-reserved'; Before='reservedCount++;'; After=''; Rule='CONSTRUCTION_COMMIT_INVENTORY_RESERVATION' }
)) {
    [IO.File]::WriteAllText($searchPath, $search.Replace($case.Before, $case.After))
    $result = & powershell.exe -NoProfile -ExecutionPolicy Bypass -File $staticTool -RepositoryRoot $fixtureRepo
    if ($LASTEXITCODE -ne 1 -or ($result -join "`n") -notmatch [regex]::Escape('[' + $case.Rule + ']')) { $failures.Add('static-negative-' + $case.Name) }
    $result | Set-Content -LiteralPath (Join-Path $EvidenceRoot ('static-negative-' + $case.Name + '.txt'))
}
[IO.File]::WriteAllText($searchPath, $search)
$pathPath = Join-Path $fixtureCore 'Construction/AICF_ConstructionPath.c'
$pathCode = [IO.File]::ReadAllText($pathPath)
foreach ($case in @(
    @{ Name='path-edge'; Before='return pathfinding.RayTrace(from, to, hit);'; After='return true;'; Rule='CONSTRUCTION_PATH_EDGES' },
    @{ Name='path-budget'; Before='AICF_ConstructionSiteSearch.TakeQueries(order, 3, false)'; After='true'; Rule='CONSTRUCTION_PATH_BOUNDED' },
    @{ Name='path-candidate-limit'; Before='order.m_iQueries - order.m_iPathQueriesAt >= MAX_QUERIES'; After='false'; Rule='CONSTRUCTION_PATH_CANDIDATE_LIMIT' }
)) {
    [IO.File]::WriteAllText($pathPath, $pathCode.Replace($case.Before, $case.After))
    $result = & powershell.exe -NoProfile -ExecutionPolicy Bypass -File $staticTool -RepositoryRoot $fixtureRepo
    if ($LASTEXITCODE -ne 1 -or ($result -join "`n") -notmatch [regex]::Escape('[' + $case.Rule + ']')) { $failures.Add('static-negative-' + $case.Name) }
    $result | Set-Content -LiteralPath (Join-Path $EvidenceRoot ('static-negative-' + $case.Name + '.txt'))
}
[IO.File]::WriteAllText($pathPath, $pathCode)
$checkpointPath = Join-Path $fixtureCore 'Construction/AICF_ConstructionCandidate.c'
$checkpointCode = [IO.File]::ReadAllText($checkpointPath)
[IO.File]::WriteAllText($checkpointPath, $checkpointCode.Replace('order.m_iPathQueriesAt = order.m_iQueries - m_iPathQueries;', 'order.m_iPathQueriesAt = order.m_iQueries;'))
$result = & powershell.exe -NoProfile -ExecutionPolicy Bypass -File $staticTool -RepositoryRoot $fixtureRepo
if ($LASTEXITCODE -ne 1 -or ($result -join "`n") -notmatch '\[CONSTRUCTION_CHECKPOINT_QUERY_IDENTITY\]') { $failures.Add('static-negative-checkpoint-queries') }
$result | Set-Content -LiteralPath (Join-Path $EvidenceRoot 'static-negative-checkpoint-queries.txt')
[IO.File]::WriteAllText($checkpointPath, $checkpointCode)
if ($failures.Count) { Write-Output "Construction contract inputs: FAIL $($failures -join ',')"; exit 1 }
Write-Output 'Construction contract inputs: PASS (16 log inputs + positive/16 negative static inputs)'
