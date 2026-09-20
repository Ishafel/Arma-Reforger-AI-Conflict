[CmdletBinding()]
param([string]$RepositoryRoot)
$ErrorActionPreference = 'Stop'
if (-not $RepositoryRoot) { $RepositoryRoot = Split-Path -Parent $PSScriptRoot }
$core = Join-Path $RepositoryRoot 'AIConflictCore/Scripts/Game/AIConflict'
$files = @{}
foreach ($path in @('Construction/AICF_ConstructionPlanner.c','Construction/AICF_ConstructionOrder.c','Construction/AICF_ConstructionCandidate.c',
    'Construction/AICF_ConstructionMetadata.c','Construction/AICF_ConstructionSiteSearch.c','Construction/AICF_ConstructionPath.c','Construction/AICF_ConstructionNavigation.c',
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
Require 'CONSTRUCTION_CHEAP_BEFORE_GEOMETRY' 'Construction/AICF_ConstructionPlanner.c' '(?s)QuoteConstruction\(order, m_Config\).*?m_iStage = -1.*?StepGeometry\(m_Config.m_iMetadataEntriesPerTick, Math.Max\(1, m_Config.m_iSliceMs - \(System.GetTickCount\(\) - sliceStarted\)\)\)'
Require 'CONSTRUCTION_FINAL_REVALIDATION' 'Construction/AICF_ConstructionPlanner.c' '(?s)!order.IdentityValid\(\).*?!ScanInventory\(order\).*?Covered\(order, order.m_eType\).*?HasUnfinishedWork.*?LiveClear.*?QuoteConstruction.*?m_Adapter.Place'
Require 'CONSTRUCTION_TOKEN_GUARD' 'Construction/AICF_StockConstructionAdapter.c' '(?s)order.m_bCommitStarted.*?order.m_bCommitStarted = true;'
Require 'CONSTRUCTION_PROVIDER_IDENTITY' 'Construction/AICF_ConstructionOrder.c' '(?s)GetID\(\) == m_BaseId.*?GetFaction\(\) == m_Faction.*?GetID\(\) == m_ProviderId.*?GetMasterProvider\(\) == m_Provider'
Require 'CONSTRUCTION_PROVIDER_OWNER_GUARD' 'Construction/AICF_ConstructionOrder.c' '(?s)providerOwner = m_Provider.GetOwner\(\).*?if \(!baseOwner \|\| !providerOwner\).*?return false;.*?GetEntityFaction\(providerOwner\)'
Require 'CONSTRUCTION_RATE_LIMIT' 'Config/AICF_ConstructionConfig.c' 'm_iDecisionMs = 60000;'
Require 'CONSTRUCTION_LOAD_LIMIT' 'Construction/AICF_ConstructionSiteSearch.c' 's_iQueries \+ count > s_iLimit'
Require 'CONSTRUCTION_TRANSFORM_LIMIT' 'Construction/AICF_ConstructionPlanner.c' 'candidates\+\+ < m_Config.m_iCandidatesPerTick'
Require 'CONSTRUCTION_SHARED_CANDIDATE_BUDGET' 'Construction/AICF_ConstructionPlanner.c' '(?s)m_iCandidatesThisTick = 0;.*?m_iCandidatesThisTick < m_Config.m_iCandidatesPerTick.*?m_iCandidatesThisTick\+\+'
Require 'CONSTRUCTION_SHARED_METADATA_BUDGET' 'Construction/AICF_ConstructionPlanner.c' '(?s)m_bMetadataBatchThisTick = false;.*?if \(m_bMetadataBatchThisTick\)\s*return;.*?m_bMetadataBatchThisTick = true;'
Require 'CONSTRUCTION_ACTIVE_BASE_FAIRNESS' 'Construction/AICF_ConstructionPlanner.c' '(?s)if \(!visitedOrder\).*?m_iCursor = \(firstBase \+ visited \+ 1\) % m_aBases.Count\(\).*?Search\(selected, sliceStarted\)'
Require 'CONSTRUCTION_SINGLE_PLACEMENT_ATTEMPT' 'Construction/AICF_ConstructionPlanner.c' '(?s)m_bPlacementAttemptedThisTick = false;.*?if \(m_bPlacementAttemptedThisTick\)\s*break;.*?m_bPlacementAttemptedThisTick = true;\s*if \(!m_Adapter.Place'
Require 'CONSTRUCTION_OCCUPIED_SITE_RETRY' 'Construction/AICF_ConstructionPlanner.c' '(?s)if \(!AccessClear\(order\) \|\| !m_Search.LiveClear\(order, null\)\).*?QUERY_BUDGET.*?order.RejectCandidate\(\);\s*order.m_bSiteReserved = false;\s*order.m_iStage = 0;'
Require 'CONSTRUCTION_ORIENTED_BOUNDS' 'Construction/AICF_ConstructionSiteSearch.c' '(?s)InsideVolume\(order, order.m_Metadata.m_vMin - margin, order.m_Metadata.m_vMax \+ margin\).*?VolumeInsideBounds.*?point = transform\[3\] \+ transform\[0\].*?DistanceSqXZ\(point, providerPosition\)'
Require 'CONSTRUCTION_BOUNDED_METADATA_COMPACTION' 'Construction/AICF_ConstructionMetadata.c' '(?s)m_aCollisionVolumes.Count\(\) > 2048.*?m_aCollisionInput = m_aCollisionVolumes;.*?m_aCollisionVolumes.Count\(\) < 48.*?m_iCollisionCursor\+\+.*?return m_iCollisionCursor == m_aCollisionInput.Count\(\) && m_aCollisionVolumes.Count\(\) <= 24;'
Require 'CONSTRUCTION_PHYSICS' 'Construction/AICF_ConstructionSiteSearch.c' '(?s)QueryEntitiesByOBB.*?GetRoadsInAABB.*?TracePosition'
Require 'CONSTRUCTION_TERRAIN_GRID' 'Construction/AICF_ConstructionSiteSearch.c' '(?s)m_iSample.*?GetSurfaceY.*?TryGetWaterSurfaceSimple.*?m_fHeightDelta'
Require 'CONSTRUCTION_LOCAL_TERRAIN_SLOPE' 'Construction/AICF_ConstructionSiteSearch.c' '(?s)m_aTerrainHeights.Clear.*?sample - 1.*?sample - columns.*?m_aTerrainHeights.Insert.*?TerrainEdgeSupported.*?heightDelta \* distance / referenceStep'
Require 'CONSTRUCTION_TERRAIN_COLLISION_ENVELOPE' 'Construction/AICF_ConstructionSiteSearch.c' '(?s)lowerShift = Math.Min\(0, order.m_fMinHeight.*?upperShift = Math.Max\(0, order.m_fMaxHeight.*?trace.Mins\[1\] = trace.Mins\[1\] \+ lowerShift;.*?trace.Maxs\[1\] = trace.Maxs\[1\] \+ upperShift;.*?check.m_fMinHeight = receipt.m_fMinHeight;.*?check.m_fMaxHeight = receipt.m_fMaxHeight;'
Require 'CONSTRUCTION_NAV_RETRY' 'Construction/AICF_ConstructionSiteSearch.c' '(?s)IsTileLoaded.*?m_iNavRetry\+\+ >= 10.*?LoadTileIn'
Require 'CONSTRUCTION_FULL_METADATA' 'Construction/AICF_ConstructionMetadata.c' '(?s)entry.LoadTransform.*?MatrixScale.*?SetPreviewObject.*?GetBoneIndex.*?AddChild.*?Collect\(m_PreviewRoot\).*?DeleteEntityAndChildren\(m_PreviewRoot\).*?IGNORE_PREFAB'
Require 'CONSTRUCTION_METADATA_BATCH' 'Construction/AICF_ConstructionMetadata.c' 'count\+\+ < entriesPerTick'
Require 'CONSTRUCTION_SEARCH_DEADLINE_AFTER_METADATA' 'Construction/AICF_ConstructionPlanner.c' '(?s)order.m_iDeadline = now \+ m_Config.m_iDeadlineMs;.*?else if \(geometry > 0\).*?order.m_iDeadline = System.GetTickCount\(\) \+ m_Config.m_iDeadlineMs;\s*order.m_iStage = 0;'
Require 'CONSTRUCTION_METADATA_CLEANUP' 'Construction/AICF_ConstructionPlanner.c' 'metadata.ReleasePreview\(\)'
Require 'CONSTRUCTION_DEPOT_ENVELOPE' 'Construction/AICF_ConstructionMetadata.c' '(?s)SCR_EntitySpawnerSlotComponent.*?m_vMinBounds.*?m_vMaxBounds.*?m_iSpawnSlots\+\+'
Require 'CONSTRUCTION_PROGRESSIVE_SEARCH' 'Construction/AICF_ConstructionPlanner.c' '(?s)m_iSearchOffset = state.m_aSearchOffsets\[type\].*?state.m_aSearchOffsets\[order.m_eType\] = order.m_iSearchOffset \+ order.m_iAttempts'
Require 'CONSTRUCTION_UNFINISHED_SEARCH_RETRY' 'Construction/AICF_ConstructionPlanner.c' '(?s)order.m_iSearchWindows < 3.*?order.m_iResumeAt = now;.*?order.m_iDeadline = now \+ m_Config.m_iDeadlineMs.*?CONSTRUCTION_SEARCH_CONTINUED.*?SEARCH_BUDGET_EXHAUSTED.*?now >= selected.m_Order.m_iResumeAt'
Require 'CONSTRUCTION_CHECKPOINT_BOUND' 'Construction/AICF_ConstructionPlanner.c' '(?s)lifetime = 60000.*?m_bPathPending.*?lifetime = 15000.*?m_iStartedAt < lifetime.*?m_aPendingCandidates.Remove\(expired\).*?m_fRemainingDistance < order.m_aPendingCandidates\[best\].m_fRemainingDistance.*?m_aPendingCandidates\[best\].Restore\(order\).*?m_aPendingCandidates.Remove\(best\).*?m_iQueries - order.m_iPathSliceAt >= 128.*?m_aPendingCandidates.Count\(\) >= 32.*?m_aPendingCandidates.Remove\(0\).*?checkpoint.Save\(order\).*?m_aPendingCandidates.Insert\(checkpoint\)'
Require 'CONSTRUCTION_CHECKPOINT_QUERY_IDENTITY' 'Construction/AICF_ConstructionCandidate.c' '(?s)m_iPathQueries = order.m_iQueries - order.m_iPathQueriesAt.*?order.m_iPathQueriesAt = order.m_iQueries - m_iPathQueries.*?order.m_Path = m_Path'
Require 'CONSTRUCTION_SEPARATE_EXIT' 'Construction/AICF_ConstructionSiteSearch.c' '(?s)ValidateExits.*?ExitAvoidsComposition.*?m_aExitHeights.*?ClearExit.*?m_aExits.Insert'
Require 'CONSTRUCTION_EXIT_REVALIDATION' 'Construction/AICF_ConstructionSiteSearch.c' '(?s)bool LiveClear.*?foreach \(AICF_ConstructionVolume exitVolume : order.m_aExits\).*?ClearExit.*?bool CompletionClear.*?receipt.m_aExits.*?check.m_aExits.Insert.*?search.LiveClear'
Require 'CONSTRUCTION_WORKER_OUTSIDE_EXIT' 'Construction/AICF_ConstructionSiteSearch.c' '(?s)!WorkClearOfExits\(order, endpoint\) \|\| !WorkClearOfSolids\(order, endpoint\).*?continue;.*?m_aWorkCandidates.Insert\(endpoint\).*?static bool WorkClearOfExits.*?LocalPoint.*?exitVolume.m_vMin\[0\] - 2'
Require 'CONSTRUCTION_WORKER_PHYSICS' 'Construction/AICF_ConstructionSiteSearch.c' '(?s)!WorkerClear\(order, endpoint\).*?bool WorkerClear.*?TakeQueries\(order, 1\).*?TracePosition\(trace, TraceEntity\).*?WORKER_ENDPOINT_OBSTRUCTED.*?liveQueries\+\+.*?BeginLiveBudget\(order, liveQueries\).*?clear = WorkerClear\(order, order.m_vWork\)'
Require 'CONSTRUCTION_COMMIT_RADIUS' 'Construction/AICF_ConstructionSiteSearch.c' '(?s)bool LiveClear.*?order.m_iStage == 4 && !excludedRoot && !InsideBounds\(order\).*?return false;'
Require 'CONSTRUCTION_SEARCH_OUTCOMES' 'Construction/AICF_ConstructionOrder.c' '(?s)string SearchStatus.*?AREA_EXHAUSTED.*?COMPUTE_LIMIT.*?TEMPORARY_OBSTACLE.*?SITE_FOUND.*?COMPUTING.*?search_status='
Require 'CONSTRUCTION_SELECTED_REJECTIONS' 'Construction/AICF_ConstructionSiteSearch.c' '(?s)LAYOUT_TRANSFORM_CHANGED.*?RejectSelected\("COMPLETION"\).*?receipt.m_sObstacle = check.m_sObstacle.*?if \(!clear\).*?RejectSelected\("COMPLETION"\)'
Require 'CONSTRUCTION_PATH_BOUNDED' 'Construction/AICF_ConstructionPath.c' '(?s)MAX_NODES = 512.*?transitions\+\+ < 64.*?System.GetTickCount\(\) - started < sliceMs.*?m_iExpanded >= MAX_NODES.*?TakeQueries\(order, 1\).*?m_aNodes.Count\(\) >= MAX_NODES.*?m_Navigation.Project.*?m_Navigation.Edge'
Require 'CONSTRUCTION_PATH_PRUNE_BEFORE_QUERY' 'Construction/AICF_ConstructionPath.c' '(?s)if \(next && \(next.m_bClosed.*?m_iPathPruned\+\+;.*?continue;.*?m_Navigation.Project'
Require 'CONSTRUCTION_CANDIDATE_BUDGET_RESUME' 'Construction/AICF_ConstructionSiteSearch.c' '(?s)m_bCandidateLiveChecked = false;.*?if \(!order.m_bCandidateLiveChecked\).*?LiveClear\(order, null\).*?QUERY_BUDGET.*?return 0;.*?m_bCandidateLiveChecked = true;'
Require 'CONSTRUCTION_LIVE_BUDGET_RESERVATION' 'Construction/AICF_ConstructionSiteSearch.c' '(?s)s_iQueries \+ count \+ reserved > s_iLimit.*?BeginLiveBudget.*?s_aClaims\[0\] != claim.*?s_AtomicOrder = order;.*?TakeQueries\(order, count, false\).*?ReleaseClaim\(order.m_sToken\).*?bool LiveClear.*?BeginLiveBudget.*?LiveClearNow.*?s_AtomicOrder = null;'
Require 'CONSTRUCTION_COMMIT_INVENTORY_RESERVATION' 'Construction/AICF_ConstructionSiteSearch.c' '(?s)m_iQueryPhase == 6.*?m_sToken == order.m_sToken.*?reserved--;.*?reservedCount\+\+;.*?claim.m_iCount = reservedCount;'
Require 'CONSTRUCTION_COMMIT_CLAIM_LIFETIME' 'Construction/AICF_ConstructionPlanner.c' '(?s)order.m_iStage < 4 &&.*?state.m_Order == order && order.m_iStage == 4.*?RefreshClaim\(order.m_sToken\)'
Require 'CONSTRUCTION_PATH_EDGES' 'Construction/AICF_ConstructionPath.c' '(?s)SegmentIntersects.*?return pathfinding.RayTrace.*?!ClearSegment\(m_Validate.m_Previous.m_vPosition, m_Validate.m_vPosition, pathfinding\).*?m_Navigation.Invalidate.*?WORKER_PATH_CHANGED.*?m_Navigation.Project.*?m_Navigation.Edge'
Require 'CONSTRUCTION_NAVIGATION_BUDGET' 'Construction/AICF_ConstructionNavigation.c' '(?s)TakeQueries\(order, 2\).*?GetSurfaceY.*?GetClosestPositionOnNavmesh.*?m_mSamples.Count\(\) >= 2048.*?TakeQueries\(order, 1\).*?pathfinding.RayTrace.*?m_mEdges.Count\(\) >= 4096'
Require 'CONSTRUCTION_NAVIGATION_CONTEXT' 'Construction/AICF_ConstructionSiteSearch.c' '(?s)order.m_bPathContextSet && \(!order.m_PathContext \|\| order.m_PathContext != pathfinding\).*?NAVMESH_CONTEXT_CHANGED.*?return -1;.*?order.m_bPathContextSet = true;'
Require 'CONSTRUCTION_PENDING_CONTEXT' 'Construction/AICF_ConstructionPlanner.c' '(?s)order.m_bPathContextSet && \(!order.m_PathContext \|\| order.m_PathContext != Commander\(order.m_Faction\).GetConstructionPathfinding\(\)\).*?Cancel\(state, "NAVMESH_CONTEXT_CHANGED"\).*?now >= order.m_iDeadline'
Require 'CONSTRUCTION_PATH_CANDIDATE_RESET' 'Construction/AICF_ConstructionSiteSearch.c' '(?s)m_iNavPathCursor = 0;.*?m_Path = null;.*?m_aWorkCandidates.Clear\(\)'
Require 'CONSTRUCTION_PATH_START_BOUND' 'Construction/AICF_ConstructionSiteSearch.c' '(?s)if \(!order.m_bPathStartSampled\).*?m_vSpawnOrigin.*?m_bPathStartSampled = true.*?m_iPathStartOption < order.m_aSpawnSeeds.Count\(\).*?m_iPathStartOption >= order.m_aSpawnSeeds.Count\(\).*?m_iPathStartOption\+\+.*?m_Path.AddStarts\(order.m_aPathStarts\)'
Require 'CONSTRUCTION_AUTHORED_START_LIMIT' 'Construction/AICF_ConstructionNavigation.c' '(?s)GetSpawnPositionsInRange.*?GetID\(\).ToString\(\).Compare.*?positions.Count\(\) >= 16.*?direction < 8'
Require 'CONSTRUCTION_PATH_CANDIDATE_LIMIT' 'Construction/AICF_ConstructionPath.c' '(?s)MAX_QUERIES = 512;.*?order.m_iQueries - order.m_iPathQueriesAt >= MAX_QUERIES.*?WORKER_PATH_QUERY_LIMIT.*?return -1;'
Require 'CONSTRUCTION_PATH_START_SPAWN' 'Construction/AICF_BaseBuilderService.c' '(?s)firstTarget = SelectTarget.*?receipt.m_bAccepted && receipt.m_bPathStartReady.*?receipt.IdentityValid\(\) && receipt.PlacementUnchanged\(\).*?position = receipt.m_vPathStart;.*?m_Spawner.CreateBuilder\(faction, position\)'
Require 'CONSTRUCTION_EXIT_RESERVATION' 'Construction/AICF_ConstructionPlanner.c' '(?s)bool SpatialClear.*?order.m_aExits.*?bool VehicleAreaClear.*?order.m_aExits'
Require 'CONSTRUCTION_REJECTION_EVIDENCE' 'Construction/AICF_ConstructionOrder.c' '(?s)CONSTRUCTION_SITE_REJECTED.*?CONSTRUCTION_SEARCH_SUMMARY'
Require 'CONSTRUCTION_POINT_NARROW_PHASE' 'Construction/AICF_ConstructionSiteSearch.c' '(?s)bool CheckEntity.*?GetMinBoundsVector.*?SCR_SpawnPositionComponentClass.*?entity.GetOrigin\(\).*?vector.Dot.*?bool inside.*?blocked = blocked \|\| inside'
Require 'CONSTRUCTION_GEOMETRY_FRONTIER' 'Construction/AICF_ConstructionPlanner.c' '(?s)order.m_iNextPathAttempt = order.m_iAttempts \+ 4.*?result == 2.*?ready.Save\(order\).*?m_aPendingCandidates.Insert\(ready\).*?GEOMETRY_READY_PATH_QUEUED'
Require 'CONSTRUCTION_CACHE_EPOCH' 'Construction/AICF_ConstructionPath.c' '(?s)m_iSeedRevision = navigation.m_iRevision.*?m_Seeds.m_iRevision != m_iSeedRevision.*?NAVMESH_CACHE_INVALIDATED.*?return -1;'
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
foreach ($probe in @('AICF_ConstructionRuntimeProbe.c','AICF_ConstructionCatalogProbe.c','AICF_ConstructionManualProbe.c','AICF_ConstructionReplayProbe.c','AICF_ConstructionBudgetProbe.c','AICF_ConstructionSearchProbe.c')) {
    if (Test-Path -LiteralPath (Join-Path $core "Construction/$probe")) { $failures.Add('CONSTRUCTION_FIXTURE_IN_PRODUCTION') }
}
if ($files['Construction/AICF_ConstructionCandidate.c'] -match 'order.m_iPathStartOption\s*=') {
    $failures.Add('CONSTRUCTION_SHARED_START_CURSOR')
}
if ($failures.Count) {
    Write-Output "Construction static audit: FAIL ($($failures.Count) issues)"
    $failures | ForEach-Object { Write-Output " - [$_]" }
    exit 1
}
Write-Output 'Construction static audit: PASS'
