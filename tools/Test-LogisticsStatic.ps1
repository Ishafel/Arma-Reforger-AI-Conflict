param([string]$RepositoryRoot = (Split-Path -Parent $PSScriptRoot))
$ErrorActionPreference = 'Stop'
$core = Join-Path $RepositoryRoot 'AIConflictCore/Scripts/Game/AIConflict'
$failures = [Collections.Generic.List[string]]::new()
function Read-Code([string]$path) { Get-Content -LiteralPath (Join-Path $core $path) -Raw }
function Require([string]$rule, [string]$code, [string]$pattern) {
    if ($code -notmatch $pattern) { $failures.Add($rule) }
}
function Forbid([string]$rule, [string]$code, [string]$pattern) {
    if ($code -match $pattern) { $failures.Add($rule) }
}
$config = Read-Code 'Config/AICF_LogisticsConfig.c'
$registry = Read-Code 'Economy/AICF_LogisticsDepotRegistry.c'
$planner = Read-Code 'Economy/AICF_LogisticsPlanner.c'
$ledger = Read-Code 'Economy/AICF_LogisticsLedger.c'
$resource = Read-Code 'Economy/AICF_LogisticsResourcePool.c'
$worker = Read-Code 'Economy/AICF_LogisticsJob.c'
$service = Read-Code 'Economy/AICF_LogisticsService.c'
$acquisitionFlow = Read-Code 'Vehicles/AICF_LogisticsAcquisitionFlow.c'
$acquisition = $acquisitionFlow + (Read-Code 'Vehicles/AICF_VehicleSpawner.c') + (Read-Code 'State/Vehicles/AICF_FactionFleet.c')
$flows = (Read-Code 'Vehicles/AICF_TransportTripController.c') + (Read-Code 'Vehicles/AICF_VehicleTaskHandoff.c')
$cleanup = Read-Code 'Vehicles/AICF_VehicleCleanupManager.c'
$delete = Read-Code 'Vehicles/AICF_VehicleCleanupManager.c'
$facade = Read-Code 'Economy/AICF_SupplyDeliverySystem.c'
Require 'CONFIG_STRICT_PARSE' $config 'Decimal\(text, false, parsed\)'
Require 'CONFIG_THRESHOLD_ORDER' $config 'm_fTargetPercent <= m_fDonorKeepPercent.*m_fDonorKeepPercent < m_fDonateAbovePercent'
Require 'DEPRECATION_EXPLICIT' $config 'LOGISTICS_CONFIG_DEPRECATED.*ignored=1'
Forbid 'NO_TIMER_DELIVERY' ($facade + $service + $ledger) 'AddSupplies\(|ArrivalMs|GetDeliveryBaseTravelMs\('
Require 'AUTHORITY_RUNNING_GATE' $service '!Replication.IsServer\(\).*!m_Campaign.IsMaster\(\).*!m_Campaign.IsRunning\(\)'
Require 'GRAPH_PENDING_GATE' $service 'if \(!graphReady\) return;'
Require 'DEPOT_EXACT_ROOT' $registry 'BuildingRoot\(w.m_Production.GetOwner\(\)\) != w.m_Depot'
Require 'DEPOT_EXACT_PROVIDER' $registry 'composition.GetProviderEntity\(\) != w.m_Provider.GetOwner\(\)'
Require 'DEPOT_FINISHED' $registry '!composition.IsCompositionSpawned\(\)'
Require 'DEPOT_STABLE_ORDINAL' $registry 'w.m_iOrdinal == ordinal'
Require 'DEPOT_CATALOG_MEMBERSHIP' $registry 'm_aAssetList.Contains\(entry\)'
Require 'DEPOT_ALLOWED_SLOT' $registry 'data.CanSpawnInSlot\(slot.GetSlotType\(\)\)'
Require 'PREFERENCE_SKIPS_INCOMPATIBLE_ENTRY' $registry '!w.m_Production.AICF_LogisticsSupports\(entry\)'
Forbid 'DEPOT_QUERY_NONDESTRUCTIVE' ($registry + $acquisitionFlow) '\.IsOccupied\(|\.GetFreeSlot\('
Require 'SHARED_FLEET_CAP' $acquisition 'GetActiveOrReservedCount\(\) >= m_iMaximumActiveOrReserved'
Require 'EMPTY_NEW_VEHICLE' $acquisition 'resources.EmptySpawnCargo\(w.m_Vehicle\)'
Require 'SINGLE_DRIVER' $acquisition 'HasExactFactionRoster\(w.m_Group, w.m_Faction.GetFactionKey\(\), 1,'
Require 'EXACT_PILOT' $worker 'm_Seat.GetOccupant\(\) != m_Driver'
Require 'PLAYER_MAIN_ENTITY' $worker 'GetPlayerIdFromMainEntity\(m_Driver\)'
Require 'OWN_DRIVER_NOT_FOREIGN_OCCUPANT' $worker 'occupant && occupant != m_Driver'
Forbid 'NO_CLEANUP_OCCUPANT_GATE_FOR_MOVEMENT' ($flows + (Read-Code 'Vehicles/AICF_VehicleTransitFlow.c') + (Read-Code 'Vehicles/AICF_VehicleDismountFlow.c')) 'HasProtectedOccupant\(w.m_Vehicle\)'
Require 'DIRECTED_HQ_PATH' $planner 'FindFriendlyPath\(faction.GetMainBase\(\), base, faction.GetFactionKey\(\), path\)'
Require 'THRESHOLD_STRICT' $planner 'actual < capacity \* below / 100'
Require 'DONOR_STRICT' $planner 'actual <= capacity \* donate / 100'
Require 'NEUTRAL_SOURCE_ONLY' $planner 'SCR_CampaignSourceBaseComponent'
Require 'RESUMABLE_SEARCH' $planner 'search.m_aCandidates\[search.m_iCursor\+\+\]'
Require 'RESUMABLE_CANDIDATE_PREPARATION' $planner 'search.m_aEndpoints\[search.m_iSourceCursor\+\+\]'
Require 'PREPARATION_PENDING_NOT_FAILURE' $planner '!search.m_bPrepared && !PrepareCandidates\(w\)'
Require 'SEARCH_PENDING_NOT_FAILURE' $service 'if \(w.m_Search\) return;\s*w.m_iReturnAttempts\+\+'
Require 'SHARED_POOL_RESERVATIONS' $ledger 'job.m_Destination.m_Pool.Overlaps\(pool\)'
Require 'TTL_TOKEN_GENERATION' $ledger 'job.m_iGeneration != w.m_iGeneration'
Require 'RECEIPT_EXACTLY_ONCE' $ledger 'if \(receipt.m_bAccounted\) return false;'
Require 'PAIR_READBACK' $resource 'm_fBeforeFrom - receipt.m_fAfterFrom - receipt.m_fAmount'
Require 'COMPENSATE_EXACT_DEBIT' $resource 'float escrow = debited - credited;'
Require 'OPERATION_EXACT_POOL' $resource 'actual.Valid\(\) && expected.Same\(actual\)'
Require 'OPERATION_RESOURCE_RIGHTS' $resource '!operation.CanInteractWith\(container\)'
Require 'OPERATION_CONSUMING_STATE' $resource '!consumer.IsConsuming\(\)'
Forbid 'PHYSICAL_TRANSFER_NOT_TRADE_PRICE' $resource 'consumer.GetBuyMultiplier\('
Require 'PHYSICAL_STATIONARY_GATE' $service 'physics.GetVelocity\(\).Length\(\) > m_Config.m_fStationarySpeedMps'
Require 'RETURN_SOURCE_PROVENANCE' $service 'job.m_OriginalSource.m_Pool == destination.m_Pool'
Require 'BATCH_UNKNOWN_EXTERNAL' $ledger 'original_source=UNKNOWN'
Require 'REAL_LOSS_ONLY' $ledger 'delta < 0 && damage && damage.IsDestroyed\(\)'
Require 'STOCK_FIRE_LOSS_ACCOUNTED' $ledger 'reason=STOCK_SUPPLIES_FIRE'
Require 'STOCK_FIRE_OBSERVATION' (Read-Code 'Economy/AICF_LogisticsDamageAccounting.c') 'super.UpdateSuppliesFireState\(fireRate, timeSlice\);'
Require 'STOCK_FIRE_CUSTODY_UNBOUND' $ledger 'damage.AICF_SetLogisticsCustody\(null\)'
Require 'SURVIVING_CARGO_RELEASE' $cleanup 'CanDeleteVehicle\(vehicle\).*ReleaseToWorldPool'
Require 'GLOBAL_CARGO_DELETE_GUARD' $delete 'if \(!AICF_LogisticsResourceAdapter.CanDeleteVehicle\(vehicle\)\)\s*return false;\s*RplComponent.DeleteRplEntity\(vehicle, false\);'
Require 'REMOVE_WAYPOINT_BEFORE_DELETE' $flows '(?s)RemoveWaypoint\(waypoint\).*?DeleteRplEntity\(waypoint, false\)'
Require 'EXTENDED_CONSERVATION' $worker 'm_fLoaded \+ m_fExternalIn - m_fDelivered - m_fReturned - cargo - m_fLost - m_fReleased - m_fExternalOut'
foreach ($callback in @('OwnerChanged','Placed','Removed')) {
    Require "EVENT_INSERT_$callback" $registry "\.Insert\($callback\)"
    Require "EVENT_REMOVE_$callback" $registry "\.Remove\($callback\)"
}
Require 'COMPOSITION_EVENT_CLEANUP' $registry 'GetOnCompositionSpawned\(\).Remove\(Changed\)'
Forbid 'FLOW_PHASE_OWNERSHIP' $acquisitionFlow 'm_ePhase\s*=\s*[^=]|LogisticsPhase\(|QueueLogisticsRelease\('
Forbid 'LOGISTICS_NO_INFANTRY_ASSIGNMENTS' ($service + $planner) 'SetAssignment|SetTarget|SetRole|RequestGroupConfiguration'
if ($failures.Count) { $failures | ForEach-Object { "FAIL $_" }; exit 1 }
'PASS Logistics static contracts (не заменяет Enforce compile/runtime)'
