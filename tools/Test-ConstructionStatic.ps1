[CmdletBinding()]
param([string]$RepositoryRoot)
$ErrorActionPreference = 'Stop'
if (-not $RepositoryRoot) { $RepositoryRoot = Split-Path -Parent $PSScriptRoot }
$core = Join-Path $RepositoryRoot 'AIConflictCore/Scripts/Game/AIConflict'
$files = @{}
foreach ($path in @('Construction/AICF_ConstructionPlanner.c','Construction/AICF_ConstructionOrder.c',
    'Construction/AICF_ConstructionMetadata.c','Construction/AICF_ConstructionSiteSearch.c',
    'Construction/AICF_StockConstructionAdapter.c','Construction/AICF_BaseBuilderService.c',
    'Economy/AICF_ConstructionEconomy.c','Config/AICF_ConstructionConfig.c',
    'Command/AICF_AICommander.c','Bootstrap/AICF_MatchController.c','Vehicles/AICF_VehicleSpawner.c')) {
    $files[$path] = Get-Content -LiteralPath (Join-Path $core $path) -Raw
}
$failures = [System.Collections.Generic.List[string]]::new()
function Require([string]$rule, [string]$file, [string]$pattern) {
    if ($files[$file] -notmatch $pattern) { $failures.Add($rule) }
}
Require 'CONSTRUCTION_AUTHORITY' 'Construction/AICF_ConstructionPlanner.c' '!Replication.IsServer\(\).*?!m_Campaign.IsMaster\(\).*?!m_Campaign.IsRunning\(\)'
Require 'CONSTRUCTION_COMMANDER_POLICY' 'Command/AICF_AICommander.c' '(?s)SelectConstructionType.*?!Replication.IsServer\(\).*?!IsEnabled\(\)'
Require 'CONSTRUCTION_ROSTER_GATE' 'Bootstrap/AICF_MatchController.c' 'if \(m_bRosterReady && m_Construction\)\s*m_Construction.Update\(\)'
Require 'CONSTRUCTION_CHEAP_BEFORE_GEOMETRY' 'Construction/AICF_ConstructionPlanner.c' '(?s)QuoteConstruction\(order, m_Config\).*?m_iStage = -1.*?StepGeometry\(m_Config.m_iMetadataEntriesPerTick, m_Config.m_iSliceMs\)'
Require 'CONSTRUCTION_FINAL_REVALIDATION' 'Construction/AICF_ConstructionPlanner.c' '(?s)!order.IdentityValid\(\).*?!ScanInventory\(order\).*?Covered\(order, order.m_eType\).*?HasUnfinishedWork.*?LiveClear.*?QuoteConstruction.*?m_Adapter.Place'
Require 'CONSTRUCTION_TOKEN_GUARD' 'Construction/AICF_StockConstructionAdapter.c' '(?s)order.m_bCommitStarted.*?order.m_bCommitStarted = true;'
Require 'CONSTRUCTION_PROVIDER_IDENTITY' 'Construction/AICF_ConstructionOrder.c' '(?s)GetID\(\) == m_BaseId.*?GetFaction\(\) == m_Faction.*?GetID\(\) == m_ProviderId.*?GetMasterProvider\(\) == m_Provider'
Require 'CONSTRUCTION_RATE_LIMIT' 'Config/AICF_ConstructionConfig.c' 'm_iDecisionMs = 60000;'
Require 'CONSTRUCTION_LOAD_LIMIT' 'Construction/AICF_ConstructionSiteSearch.c' 's_iQueries \+ count > s_iLimit'
Require 'CONSTRUCTION_TRANSFORM_LIMIT' 'Construction/AICF_ConstructionPlanner.c' 'candidates\+\+ < m_Config.m_iCandidatesPerTick'
Require 'CONSTRUCTION_PHYSICS' 'Construction/AICF_ConstructionSiteSearch.c' '(?s)QueryEntitiesByOBB.*?GetRoadsInAABB.*?TracePosition'
Require 'CONSTRUCTION_TERRAIN_GRID' 'Construction/AICF_ConstructionSiteSearch.c' '(?s)m_iSample.*?GetSurfaceY.*?TryGetWaterSurfaceSimple.*?m_fHeightDelta'
Require 'CONSTRUCTION_NAV_RETRY' 'Construction/AICF_ConstructionSiteSearch.c' '(?s)IsTileLoaded.*?m_iNavRetry\+\+ >= 10.*?LoadTileIn'
Require 'CONSTRUCTION_FULL_METADATA' 'Construction/AICF_ConstructionMetadata.c' '(?s)entry.LoadTransform.*?MatrixScale.*?SetPreviewObject.*?GetBoneIndex.*?AddChild.*?Collect\(m_PreviewRoot\).*?DeleteEntityAndChildren\(m_PreviewRoot\).*?IGNORE_PREFAB'
Require 'CONSTRUCTION_METADATA_BATCH' 'Construction/AICF_ConstructionMetadata.c' 'count\+\+ < entriesPerTick'
Require 'CONSTRUCTION_METADATA_CLEANUP' 'Construction/AICF_ConstructionPlanner.c' 'metadata.ReleasePreview\(\)'
Require 'CONSTRUCTION_DEPOT_ENVELOPE' 'Construction/AICF_ConstructionMetadata.c' '(?s)SCR_EntitySpawnerSlotComponent.*?m_vMinBounds.*?m_vMaxBounds.*?m_iSpawnSlots\+\+'
Require 'CONSTRUCTION_PROGRESSIVE_SEARCH' 'Construction/AICF_ConstructionPlanner.c' '(?s)m_iSearchOffset = state.m_aSearchOffsets\[type\].*?state.m_aSearchOffsets\[order.m_eType\] = order.m_iSearchOffset \+ order.m_iAttempts'
Require 'CONSTRUCTION_UNFINISHED_SEARCH_RETRY' 'Construction/AICF_ConstructionPlanner.c' '(?s)order.m_iAttempts = previousAttempts.*?reason == "NO_SAFE_SITE" && order.m_iStage > 0 && !order.m_bAccepted && order.m_iAttempts > 0.*?state.m_aSearchOffsets\[order.m_eType\] - 1'
Require 'CONSTRUCTION_SEPARATE_EXIT' 'Construction/AICF_ConstructionSiteSearch.c' '(?s)ValidateExits.*?ExitAvoidsComposition.*?m_aExitHeights.*?ClearExit.*?m_aExits.Insert'
Require 'CONSTRUCTION_EXIT_REVALIDATION' 'Construction/AICF_ConstructionSiteSearch.c' '(?s)bool LiveClear.*?foreach \(AICF_ConstructionVolume exitVolume : order.m_aExits\).*?ClearExit.*?bool CompletionClear.*?receipt.m_aExits.*?check.m_aExits.Insert.*?search.LiveClear'
Require 'CONSTRUCTION_WORKER_OUTSIDE_EXIT' 'Construction/AICF_ConstructionSiteSearch.c' '(?s)!WorkClearOfExits\(order, endpoint\).*?m_iNavPathCursor = \(side \+ 1\) \* 25.*?static bool WorkClearOfExits.*?LocalPoint.*?exitVolume.m_vMin\[0\] - 2'
Require 'CONSTRUCTION_EXIT_RESERVATION' 'Construction/AICF_ConstructionPlanner.c' '(?s)bool SpatialClear.*?order.m_aExits.*?bool VehicleAreaClear.*?order.m_aExits'
Require 'CONSTRUCTION_REJECTION_EVIDENCE' 'Construction/AICF_ConstructionOrder.c' '(?s)CONSTRUCTION_SITE_REJECTED.*?CONSTRUCTION_SEARCH_SUMMARY'
Require 'CONSTRUCTION_POINT_NARROW_PHASE' 'Construction/AICF_ConstructionSiteSearch.c' '(?s)bool CheckEntity.*?GetMinBoundsVector.*?spawnPoint.GetPositionAndRotation.*?vector.Dot.*?bool inside.*?blocked = blocked \|\| inside'
Require 'CONSTRUCTION_ROAD_NARROW_PHASE' 'Construction/AICF_ConstructionSiteSearch.c' '(?s)GetRoadsInAABB.*?LocalPoint.*?m_Metadata.m_aSolids.*?roadChecks > 4096.*?solid.m_vMin - buffer'
Require 'CONSTRUCTION_STOCK_DEBIT' 'Economy/AICF_ConstructionEconomy.c' 'manager.AICF_ApplyConstructionBudget\(order\)'
Require 'CONSTRUCTION_DEFERRED_DEBIT_SUPPRESSED' 'Construction/AICF_StockConstructionAdapter.c' '(?s)m_AICFConstructionReceipt.*?budgetChange >= 0.*?return;'
Require 'CONSTRUCTION_ROLLBACK_IDENTITY' 'Economy/AICF_ConstructionEconomy.c' '(?s)RollbackConstruction.*?m_ConstructionReservation != order.*?order.IdentityValid\(\).*?m_OwnerId.*?if \(valid\)'
Require 'CONSTRUCTION_PROVIDER_JIP' 'Construction/AICF_StockConstructionAdapter.c' 'SetProviderEntityServer\(order.m_Provider.GetOwner\(\)\)'
Require 'CONSTRUCTION_UNFINISHED' 'Construction/AICF_StockConstructionAdapter.c' '(?s)IgnoreSpawning\(true\).*?SpawnAsOffline\(true\).*?SpawnEntityPrefabEx.*?IgnoreSpawning\(ignored\).*?SpawnAsOffline\(offline\).*?GetCurrentBuildValue\(\) == 0'
Require 'CONSTRUCTION_COMPLETION_GUARD' 'Construction/AICF_StockConstructionAdapter.c' '(?s)override void AddBuildingValue.*?!AICF_CompletionClear\(\).*?return;.*?super.AddBuildingValue'
Require 'CONSTRUCTION_BUILDER_REGISTRATION' 'Construction/AICF_BaseBuilderService.c' '(?s)bool RegisterConstruction.*?GetCampaignMilitaryBaseComponent\(\) != base.*?GetProviderEntity\(\) != provider.GetOwner\(\).*?m_aPlaced.Insert'
Require 'CONSTRUCTION_WORK_ENDPOINT' 'Construction/AICF_BaseBuilderService.c' 'builder.m_vWorkPosition = receipt.m_vWork'
Require 'CONSTRUCTION_FAILED_QUEUE_CLEANUP' 'Construction/AICF_StockConstructionAdapter.c' '(?s)RollbackConstruction\(order\).*?UnregisterFailedConstruction\(order.m_Composition\).*?DeleteRplEntity'
Require 'CONSTRUCTION_FAILED_TARGET_REJECTED' 'Construction/AICF_BaseBuilderService.c' '(?s)bool IsTargetValid.*?!composition.m_AICFConstructionReceipt.m_bAccepted.*?return false;'
Require 'CONSTRUCTION_MUTUAL_SPATIAL' 'Vehicles/AICF_VehicleSpawner.c' '(?s)ConstructionAreaClear.*?VehicleAreaClear.*?s_aConstructionSites.Insert\(reservation\)'
foreach ($callback in @('OnOwnerChanged','OnPlayerPlaced','OnRemoved')) {
    Require "CONSTRUCTION_SUBSCRIBE_$callback" 'Construction/AICF_ConstructionPlanner.c' ("\.Insert\($callback\)")
    Require "CONSTRUCTION_UNSUBSCRIBE_$callback" 'Construction/AICF_ConstructionPlanner.c' ("\.Remove\($callback\)")
}
foreach ($path in $files.Keys) {
    if ($path -match 'Construction' -and $files[$path] -match 'IsThereEnoughBudgetToSpawn\(|ClearAccumulatedBudgetChanges\(|AddSupplies\(') {
        $failures.Add("CONSTRUCTION_FORBIDDEN_BUDGET_SIDE_EFFECT:$path")
    }
}
foreach ($probe in @('AICF_ConstructionRuntimeProbe.c','AICF_ConstructionCatalogProbe.c')) {
    if (Test-Path -LiteralPath (Join-Path $core "Construction/$probe")) { $failures.Add('CONSTRUCTION_FIXTURE_IN_PRODUCTION') }
}
if ($failures.Count) {
    Write-Output "Construction static audit: FAIL ($($failures.Count) issues)"
    $failures | ForEach-Object { Write-Output " - [$_]" }
    exit 1
}
Write-Output 'Construction static audit: PASS'
