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
$orderPlanner = Read-Required 'Orders\AICF_OrderPlanner.c'
$groupSlot = Read-Required 'State\AICF_GroupSlot.c'
$groupRuntime = Read-Required 'State\AICF_GroupRuntime.c'
$mapMarkers = Read-Required 'UI\AICF_GroupMapMarkers.c'
$strategicRpc = Read-Required 'UI\AICF_StrategicCommandRpc.c'
$strategicUI = Read-Required 'UI\AICF_StrategicUI.c'
$createRectMatch = [regex]::Match(
    $strategicUI,
    'protected\s+Widget\s+CreateRect\s*\([\s\S]*?(?=\r?\n\s*protected\s+void\s+SetRectColor\s*\()'
)
$createRect = $createRectMatch.Value
$stage1Config = Read-Required 'Config\AICF_Stage1Config.c'
$factionState = Read-Required 'State\AICF_FactionState.c'
$groupSpawner = Read-Required 'Forces\AICF_GroupSpawner.c'
$contentProfile = Read-Required 'Content\AICF_ContentProfile.c'
$corpseRetention = Read-Required 'Forces\AICF_CorpseRetentionPolicy.c'
$unitTypes = Read-Required 'State\AICF_Stage1Enums.c'
$vehicleAcquisition = Read-Required 'Vehicles\AICF_VehicleAcquisitionFlow.c'
$vehicleSpawner = Read-Required 'Vehicles\AICF_VehicleSpawner.c'
$vehicleTripController = Read-Required 'Vehicles\AICF_TransportTripController.c'
$vehicleCoordinator = Read-Required 'Vehicles\AICF_VehicleCoordinator.c'
$vehicleBoarding = Read-Required 'Vehicles\AICF_VehicleBoardingFlow.c'
$vehicleTaskHandoff = Read-Required 'Vehicles\AICF_VehicleTaskHandoff.c'
$vehicleWaypointFactory = Read-Required 'Vehicles\AICF_VehicleWaypointFactory.c'
$vehicleTripEnums = Read-Required 'State\Vehicles\AICF_TransportTripEnums.c'
$vehicleTripView = Read-Required 'State\Vehicles\AICF_TransportTripRegistry.c'
$vehiclePhaseStates = Read-Required 'State\Vehicles\AICF_VehiclePhaseStates.c'
$vehicleBoardingTokens = Read-Required 'State\Vehicles\AICF_VehicleBoardingTokens.c'
$stage3Config = Read-Required 'Config\AICF_Stage3Config.c'
$factionFleet = Read-Required 'State\Vehicles\AICF_FactionFleet.c'
$logAudit = Get-Content -LiteralPath (Join-Path $RepositoryRoot 'tools\Test-Stage4Log.ps1') -Raw

Assert-Contains 'STAGE4_ALWAYS_ON' $config 'bool\s+GetEconomyEnabled\s*\(\s*\)\s*\{\s*return\s+true\s*;\s*\}' 'Economy compatibility accessor must remain permanently enabled'
Assert-NotContains 'STAGE4_ALWAYS_ON' $config 'm_bEconomyEnabled' 'Economy must not retain mutable enable state'
Assert-NotContains 'STAGE4_ALWAYS_ON' $config '"aicfEconomyEnabled"' 'Economy must not expose a CLI opt-out'
Assert-Contains 'STAGE4_ALWAYS_ON' $controller 'm_EconomySystem\s*=\s*new\s+AICF_EconomySystem' 'Match controller must always compose the economy subsystem'
Assert-Contains 'STAGE4_ALWAYS_ON' $economy 'bool\s+IsEnabled\s*\(\s*\)[\s\S]*m_Config\s*&&\s*m_Config\.GetEconomyEnabled\s*\(' 'Economy runtime gate must resolve through the permanently enabled configuration'
Assert-NotContains 'STAGE4_ALWAYS_ON' $strategicUI 'string\s+supply\s*=\s*"OFF"|ECONOMY OFF' 'Strategic UI must not expose a disabled economy state'
Assert-Contains 'STAGE4_VARIABLE_SUPPLY_COST' $config 'GetReplacementSupplyCostForSize[\s\S]*DEFAULT_GROUP_SIZE' 'Replacement supply cost must scale from the default ten-person roster'
Assert-Contains 'STAGE4_VARIABLE_SUPPLY_COST' $economy 'GetReplacementSupplyCostForSize\s*\(\s*slot\.GetDeploymentSize\(\)\s*\)' 'Replacement reservations must price the selected next-deployment size'
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

foreach ($field in @(
    'm_sAICFUSStrategicObjective', 'm_sAICFUSOrderTargets',
    'm_sAICFUSGroup0', 'm_sAICFUSGroup1', 'm_sAICFUSGroup2', 'm_sAICFUSGroup3', 'm_sAICFUSGroup4',
    'm_sAICFUSGroup5', 'm_sAICFUSGroup6', 'm_sAICFUSGroup7', 'm_sAICFUSGroup8', 'm_sAICFUSGroup9',
    'm_iAICFUSCombatGroups', 'm_iAICFUSManagedAgents',
    'm_sAICFUSSRStrategicObjective', 'm_sAICFUSSROrderTargets',
    'm_sAICFUSSRGroup0', 'm_sAICFUSSRGroup1', 'm_sAICFUSSRGroup2', 'm_sAICFUSSRGroup3', 'm_sAICFUSSRGroup4',
    'm_sAICFUSSRGroup5', 'm_sAICFUSSRGroup6', 'm_sAICFUSSRGroup7', 'm_sAICFUSSRGroup8', 'm_sAICFUSSRGroup9',
    'm_iAICFUSSRCombatGroups', 'm_iAICFUSSRManagedAgents'
)) {
    Assert-Contains 'STAGE4_STRATEGIC_REPLICATION' $campaignState ("RplProp[\s\S]{0,100}" + [regex]::Escape($field)) "Missing strategic replicated field $field"
}

Assert-Contains 'STAGE4_HUD' $strategicUI 'TICKETS\s+%1[\s\S]*SUPPLY\s+%2[\s\S]*SQUADS\s+%3[\s\S]*OBJECTIVE' 'Compact HUD must expose tickets, supply, squads, and the current objective'
Assert-Contains 'STAGE4_WIDGET_HIERARCHY' $strategicUI 'CreateRect[\s\S]*FrameWidgetTypeID[\s\S]*RECT_BACKGROUND_NAME' 'Text and controls must be siblings of a background image inside a FrameWidget container'
$createRectColorPattern = 'CreateWidget\s*\(\s*WidgetType\.ImageWidgetTypeID\s*,[\s\S]{0,320}?WidgetFlags\.BLEND[\s\S]{0,320}?\bcolor\s*,\s*0\s*,\s*widget\s*\)'
$redundantCreateRectColorPattern = 'background\.SetColor\s*\(\s*color\s*\)'
Assert-Contains 'STAGE4_WIDGET_RENDERING' $createRect $createRectColorPattern 'Programmatic panel backgrounds must receive their explicit dark color during widget creation'
Assert-NotContains 'STAGE4_WIDGET_RENDERING' $createRect $redundantCreateRectColorPattern 'CreateRect must not reuse its Color after CreateWidget has already applied it'
Assert-Contains 'STAGE4_WIDGET_RENDERING' $strategicUI 'RefreshVisualStyles[\s\S]*SetRectColor\s*\(\s*m_wHUDRoot[\s\S]*SetRectColor\s*\(\s*m_wCommandPanel[\s\S]*foreach\s*\(\s*Widget\s+targetButton' 'Top-level and dynamic panel colors must be restored after Enfusion widget initialization'
Assert-Contains 'STAGE4_WIDGET_INPUT' $strategicUI 'ButtonWidgetTypeID[\s\S]*inputWidget\.SetName\s*\(\s*RECT_INPUT_NAME\s*\)' 'Every clickable rectangle must own a real ButtonWidget input surface'
Assert-Contains 'STAGE4_WIDGET_INPUT' $strategicUI 'inputWidget\.AddHandler\s*\(\s*handler\s*\)' 'Button input surfaces must receive the strategic action handler'
Assert-Contains 'STAGE4_COMMAND_SURFACE' $strategicUI 'ARMY / SELECT GROUP' 'Command surface must expose army composition'
Assert-Contains 'STAGE4_COMMAND_SURFACE' $strategicUI 'SELECT A READY GROUP[\s\S]*NO VALID TARGETS FOR THIS ROLE' 'Command surface must explain empty target states'
Assert-Contains 'STAGE4_COMMAND_SURFACE' $strategicUI 'REINFORCEMENTS[\s\S]*SHIPMENTS' 'Command surface must expose reinforcement and logistics state'
Assert-Contains 'STAGE4_COMMAND_SURFACE' $strategicUI 'FormatGroupSummary[\s\S]*ЗАДАЧА[\s\S]*ТЕХНИКА[\s\S]*ПОПОЛН\.' 'Unit cards must expose role, state, posture, vehicle phase, and reinforcement ETA with user-facing Russian labels'
Assert-Contains 'STAGE4_COMMAND_SURFACE' $strategicUI 'AICF_RequestStrategicOrder\s*\(' 'Command target buttons must issue a strategic-order RPC'

Assert-Contains 'STAGE4_TEN_GROUPS' $stage1Config 'GROUP_SLOTS_PER_FACTION\s*=\s*10\s*;' 'Each faction must own ten stable group slots'
Assert-Contains 'STAGE4_TEN_GROUPS' $stage1Config 'DEFAULT_GROUP_SIZE\s*=\s*10\s*;' 'Every group must default to ten soldiers'
Assert-Contains 'STAGE4_TEN_GROUPS' $stage1Config 'MAX_GROUP_SIZE\s*=\s*10\s*;' 'Commander-selected group size must be capped at ten'
Assert-Contains 'STAGE4_DEFAULT_FULL_GROUPS' $stage1Config 'DEFAULT_FULL_SIZE_GROUPS_PER_FACTION\s*=\s*GROUP_SLOTS_PER_FACTION\s*;[\s\S]*GetDefaultGroupSizeForSlot[\s\S]*slotId\s*<\s*DEFAULT_FULL_SIZE_GROUPS_PER_FACTION[\s\S]*return\s+MAX_GROUP_SIZE' 'Every slot of each faction must default to the ten-agent limit'
Assert-Contains 'STAGE4_DEFAULT_FULL_GROUPS' $groupSlot 'm_iDesiredSize\s*=\s*AICF_Stage1Config\.GetDefaultGroupSizeForSlot\s*\(\s*slotId\s*\)' 'Each group slot must retain its slot-specific desired size for replacement spawns'
Assert-Contains 'STAGE4_DEFAULT_FULL_GROUPS' $stage1Config 'MIN_MANAGED_AGENTS\s*=\s*200\s*;' 'Admission cap must fit twenty ten-agent groups across both factions'
Assert-Contains 'STAGE4_TEN_GROUPS' $factionFleet 'HARD_MAX_ACTIVE_OR_RESERVED\s*=\s*10\s*;' 'Fleet hard cap must not silently clamp the ten configurable groups back to four'
Assert-Contains 'STAGE4_TEN_GROUPS' $factionState 'for\s*\([^)]*GROUP_SLOTS_PER_FACTION' 'Default faction state must construct every configured group slot'
Assert-Contains 'STAGE4_VARIABLE_ROSTER' $groupSpawner 'SpawnGroup\s*\([\s\S]*int\s+desiredSize[\s\S]*ConfigureManagedRoster\s*\(\s*group\s*,\s*faction\s*,\s*desiredSize' 'Spawner must shape the faction-correct roster to the selected size'
Assert-Contains 'STAGE4_ROLE_ROSTER' $groupSpawner 'EEntityCatalogType\.CHARACTER[\s\S]*BuildCharacterRoleCandidates[\s\S]*FindCharacterPrefab' 'Managed roster must resolve role candidates from the active faction character catalog'
Assert-Contains 'STAGE4_ROLE_ROSTER' $contentProfile 'SQUAD_LEADER[\s\S]*MEDIC[\s\S]*MACHINE_GUNNER[\s\S]*ANTI_TANK[\s\S]*GRENADIER[\s\S]*AUTOMATIC_RIFLEMAN[\s\S]*MACHINE_GUNNER_ASSISTANT[\s\S]*ANTI_TANK_ASSISTANT[\s\S]*RIFLEMAN' 'Stock content profile must retain the varied ten-position role table'
Assert-Contains 'STAGE4_GROUP_CONFIGURATION' $strategicUI 'ROLE[\s\S]*ATTACK[\s\S]*DEFEND[\s\S]*RESERVE' 'Commander panel must expose all group roles'
Assert-Contains 'STAGE4_GROUP_CONFIGURATION' $strategicUI 'UNIT TYPE / NO HEAVY ARMOR[\s\S]*INFANTRY[\s\S]*LIGHT 4X4[\s\S]*TRUCK[\s\S]*ARMED 4X4' 'Commander panel must expose infantry, transport, and armed-light profiles without heavy armor'
Assert-Contains 'STAGE4_GROUP_CONFIGURATION' $strategicUI 'NEXT DEPLOYMENT SIZE[\s\S]*MAX 10' 'Commander panel must label deferred roster-size changes and show the cap'
Assert-Contains 'STAGE4_GROUP_CONFIGURATION' $unitTypes 'AICF_EGroupUnitType[\s\S]*INFANTRY[\s\S]*MOTORIZED_LIGHT[\s\S]*MOTORIZED_TRUCK[\s\S]*MOTORIZED_ARMED_LIGHT' 'Model must contain infantry, transport, and armed-light profiles without heavy armor'
Assert-Contains 'STAGE4_GROUP_CONFIGURATION' $vehicleAcquisition 'GetUnitType[\s\S]*MOTORIZED_LIGHT[\s\S]*LIGHT_TRANSPORT[\s\S]*MOTORIZED_TRUCK[\s\S]*TRANSPORT[\s\S]*MOTORIZED_ARMED_LIGHT[\s\S]*ARMED_LIGHT' 'Vehicle acquisition must derive light, truck, and armed-light mobility from commander unit type'
Assert-Contains 'STAGE4_GROUP_CONFIGURATION' $strategicRpc 'AICF_RequestGroupConfiguration\s*\([\s\S]*RpcAsk_AICFGroupConfiguration' 'Client configuration controls must use a reliable server RPC'
Assert-Contains 'STAGE4_GROUP_CONFIGURATION' $controller 'RequestPlayerGroupConfiguration[\s\S]*SGetPlayerFaction\s*\(\s*playerId\s*\)' 'Server must derive group-configuration authority from the player faction'
Assert-Contains 'STAGE4_GROUP_CONFIGURATION' $controller 'ROSTER_SPAWN_ACTIVE' 'Size changes must be rejected while a roster spawn is already active'
Assert-Contains 'STAGE4_GROUP_CONFIGURATION' $controller 'MOTORIZED_ARMED_LIGHT[\s\S]*desiredSize\s*<\s*2[\s\S]*desiredSize\s*>\s*4' 'Armed-light configuration must stay within its conservative two-to-four-seat roster contract'
Assert-Contains 'STAGE4_DEFERRED_VEHICLE_SPAWN' $controller 'VEHICLE_TYPE_CHANGED[\s\S]*flow=TYPE_CHANGED desired_only=1 cap_reserved=0 entity_created=0' 'Commander type changes must persist desired state without allocating a lease or entity'
Assert-Contains 'STAGE4_GROUP_CONFIGURATION' $vehicleSpawner 'SPAWN_SEARCH_RADIUS_METERS\s*=\s*90\.0\s*;' 'Dense friendly bases must receive a wider bounded empty-terrain search before range-wait fallback'
Assert-Contains 'STAGE4_DEFERRED_VEHICLE_SPAWN' $vehicleTripEnums 'WAITING_FOR_SITE[\s\S]*SITE_PLANNED[\s\S]*APPROACHING_SITE[\s\S]*STAGING_CONFIRMED[\s\S]*SPAWN_COMMIT[\s\S]*BOARDING' 'Vehicle requests must use the explicit plan, approach, staging, commit, and boarding phases'
Assert-Contains 'STAGE4_DEFERRED_VEHICLE_SPAWN' $vehicleSpawner 'class AICF_VehicleSpawnSiteReservation[\s\S]*class AICF_VehicleSpawnPlan' 'A persisted plan and cap-free pad reservation must fence concurrent groups'
Assert-Contains 'STAGE4_DEFERRED_VEHICLE_SPAWN' $vehicleAcquisition 'TrySelectSiteForAcquisition\([\s\S]*GetMaximumSpawnDistanceMeters\(\),\s*0,' 'Site planning must not require the group to already be inside boarding range'
Assert-Contains 'STAGE4_DEFERRED_VEHICLE_SPAWN' $vehicleAcquisition 'ProcessApproachingSite[\s\S]*MeasureStagingReadiness[\s\S]*GetSpawnStagingHoldMs' 'All living members must remain staged for a stable hold before commit'
Assert-Contains 'STAGE4_DEFERRED_VEHICLE_SPAWN' $vehicleSpawner 'RevalidateSpawnCommit[\s\S]*GetSpawnRejectionReason[\s\S]*STAGING_NO_LONGER_CONFIRMED[\s\S]*TraceCilinderUtil[\s\S]*SPAWN_PAD_OCCUPIED' 'Spawn commit must recheck the persisted exact pad rather than resampling a terrain-search grid'
Assert-Contains 'STAGE4_SPAWN_PAD_DIAGNOSTICS' $vehicleSpawner 'VEHICLE_SPAWN_PAD_PROBE[\s\S]*EXACT_TRACE_CYLINDER[\s\S]*blocking_trace_count=[\s\S]*blocking_traces=[\s\S]*nearby_entity_count=[\s\S]*nearby_entities=' 'Occupied spawn pads must report exact trace hits, probe geometry and nearby blocker identities'
Assert-Contains 'STAGE4_DEFERRED_VEHICLE_SPAWN' $vehicleSpawner 'ObserveStaging[\s\S]*rosterChanged[\s\S]*m_iAllStagedSinceMs\s*=\s*nowMs' 'A changed living roster must earn a fresh stable staging hold'
Assert-Contains 'STAGE4_DEFERRED_VEHICLE_SPAWN' $vehicleSpawner 'RevalidateSpawnCommit[\s\S]*minimumAliveCount[\s\S]*GROUP_NOT_COMBAT_READY' 'Commit revalidation must reject a roster below the configured vehicle threshold'
Assert-Contains 'STAGE4_DEFERRED_VEHICLE_SPAWN' $vehicleAcquisition 'ProcessSpawnCommit[\s\S]*RevalidateSpawnCommit[\s\S]*EnsureReservedLease' 'Fleet cap may be reserved only after staging and commit revalidation'
Assert-Contains 'STAGE4_DEFERRED_VEHICLE_SPAWN' $vehicleAcquisition 'ProcessSpawnCommit[\s\S]*RevalidateSpawnCommit[\s\S]*GetNextAttemptAtMs\(\)' 'Cap waiting must continue to revalidate staging on every controller tick'
Assert-Contains 'STAGE4_DEFERRED_VEHICLE_SPAWN' $vehicleTripController 'CreateSpawnStagingWaypoint[\s\S]*BindVehicleWaypoint[\s\S]*ReleaseSpawnPlan' 'The controller and handoff boundary must own the temporary staging route lifecycle'
Assert-Contains 'STAGE4_DEFERRED_VEHICLE_SPAWN' $vehicleWaypointFactory 'CreateSpawnStagingWaypoint[\s\S]*SetCompletionType\(EAIWaypointCompletionType\.All\)' 'The engine must keep the staging waypoint active until every living member arrives'
Assert-Contains 'STAGE4_DEFERRED_VEHICLE_SPAWN' $vehicleAcquisition 'MAX_APPROACH_WAYPOINT_REISSUES\s*=\s*1[\s\S]*CanReissueApproachWaypoint[\s\S]*SPAWN_SITE_APPROACH_QUEUE_SETTLED' 'A missing staging waypoint must use one delayed retry instead of per-tick command churn'
Assert-Contains 'STAGE4_DEFERRED_VEHICLE_SPAWN' $logAudit 'Vehicle staging waypoint reissue churn[\s\S]*maximum=1' 'Runtime log audit must reject repeated staging command/voice churn per reservation'
Assert-Contains 'STAGE4_DEFERRED_VEHICLE_SPAWN' $vehicleTaskHandoff 'APPROACHING_SITE[\s\S]*GetApproachWaypoint' 'Waypoint removal must prove exact spawn-plan ownership'
Assert-Contains 'STAGE4_DEFERRED_VEHICLE_SPAWN' $stage3Config 'DEFAULT_SPAWN_STAGING_HOLD_MS\s*=\s*3000' 'Default staging confirmation must remain stable for three seconds'
Assert-Contains 'STAGE4_DEFERRED_VEHICLE_SPAWN' $stage3Config 'DEFAULT_SPAWN_STAGING_RADIUS_METERS\s*=\s*25\.0' 'Default vehicle staging radius must keep a staged squad inside the later boarding threshold'
Assert-Contains 'STAGE4_SPAWN_STAGING_CLEARANCE' $stage3Config 'DEFAULT_SPAWN_STAGING_OFFSET_METERS\s*=\s*43\.0' 'The staging center must keep the twenty-five-metre rally circle outside the eight-metre spawn cylinder with margin'
Assert-Contains 'STAGE4_SPAWN_STAGING_CLEARANCE' $stage3Config '18\.\.68 m pad distance[\s\S]*90 m boarding threshold[\s\S]*10\.\.90 m pad envelope' 'Default geometry must document both spawn-pad clearance and formation-tolerant boarding containment'
Assert-Contains 'STAGE4_SPAWN_STAGING_CLEARANCE' $vehicleAcquisition 'minimumOffsetMeters\s*=\s*m_Config\.GetSpawnStagingRadiusMeters\(\)[\s\S]*SPAWN_PAD_CLEARANCE_RADIUS_METERS[\s\S]*STAGING_TO_PAD_MARGIN_METERS' 'Runtime staging geometry must remain valid when radius is overridden by CLI'
Assert-Contains 'STAGE4_SPAWN_STAGING_CLEARANCE' $vehicleSpawner 'IsRequestingGroupClearOfSpawnPad[\s\S]*REQUESTING_GROUP_IN_SPAWN_CLEARANCE' 'Spawn commit must prove the requesting squad is outside the vehicle cylinder'
Assert-Contains 'STAGE4_DEFERRED_VEHICLE_SPAWN' $stage3Config 'aicfVehicleSpawnStagingRadiusMeters[\s\S]*ClampFloat\s*\(\s*value\.ToFloat\s*\(\s*\)\s*,\s*5\.0\s*,\s*100\.0\s*\)' 'CLI staging-radius override must permit the twenty-five-metre default and bounded tuning above it'
Assert-Contains 'STAGE4_DEFERRED_VEHICLE_SPAWN' $vehicleTripView 'Площадка выбрана[\s\S]*Следует к месту выдачи[\s\S]*Ожидание бойцов[\s\S]*Выдача техники[\s\S]*Посадка[\s\S]*Движение на технике[\s\S]*Высадка[\s\S]*Возврат к пешему приказу[\s\S]*Задача техники завершена[\s\S]*Переход на пеший порядок[\s\S]*Техника недоступна[\s\S]*Ожидание площадки' 'Commander and map projections must expose every vehicle lifecycle state in Russian'
Assert-NotContains 'STAGE4_DEFERRED_VEHICLE_SPAWN' $vehicleTripView 'return\s+typename\.EnumToString\s*\(\s*AICF_ETransportTripPhase' 'User-facing vehicle status must never fall back to an internal enum name'
Assert-Contains 'STAGE4_COMMAND_SURFACE' ($strategicUI + $mapMarkers) 'ТЕХНИКА' 'Commander cards and map markers must use a Russian vehicle-status label'
Assert-Contains 'STAGE4_PHYSICAL_VEHICLE_STATUS' ($groupRuntime + $vehicleCoordinator) 'CountAliveAgentsInAnyVehicle[\s\S]*GetSlotDisplayStatusText[\s\S]*В технике' 'Commander status must prefer physical vehicle occupancy when a terminal or retired Trip no longer describes the group'
Assert-Contains 'STAGE4_PHYSICAL_VEHICLE_STATUS' $controller 'GetSlotDisplayStatusText' 'Commander cards must use the physical mobility projection'
Assert-Contains 'STAGE4_PHYSICAL_VEHICLE_STATUS' $mapMarkers 'GetSlotDisplayStatusText' 'Allied map markers must use the physical mobility projection'
Assert-Contains 'STAGE4_PHYSICAL_VEHICLE_STATUS' $controller 'physical_vehicle_members=' 'Heartbeat diagnostics must expose physical occupants independently from Trip phase'
Assert-Contains 'STAGE4_STABLE_SLOT_IDENTITY' ($groupSlot + $vehicleAcquisition) 'GetStableSlotKey[\s\S]*stable_slot=' 'Dynamic role labels must be accompanied by a stable faction-slot identity'
Assert-Contains 'STAGE4_REPLACEMENT_AGENT_CAP' $controller 'CountProjectedManagedAgentsForSpawn\s*\(\s*slot\s*\)[\s\S]*projectedManagedAgents\s*>\s*managedAgentLimit[\s\S]*generation_unchanged=' 'Replacement capacity must use the configured roster projection before consuming a spawn generation'
Assert-Contains 'STAGE4_REPLACEMENT_AGENT_CAP' $logAudit 'AI_LIMIT block lacks exact capacity projection[\s\S]*False AI_LIMIT block' 'Runtime audit must reject incomplete or mathematically false AI-limit blocks'
Assert-Contains 'STAGE4_VEHICLE_ADMISSION_CAP' $vehicleTripView 'GetPreLeaseNonTerminalCount[\s\S]*!lease\.IsCapActive' 'Pre-lease vehicle operations must hold an admission token without double-counting fleet leases'
Assert-Contains 'STAGE4_VEHICLE_ADMISSION_CAP' $vehicleCoordinator 'GetActiveOrReservedCount\(\)\s*\+[\s\S]*GetPreLeaseNonTerminalCount[\s\S]*VEHICLE_CAP_UNAVAILABLE_PRE_ADMISSION' 'Vehicle lifecycle must remain on the infantry order until fleet admission is available'
Assert-Contains 'STAGE4_GUNNER_BOARDING' $vehiclePhaseStates 'BeginPhase[\s\S]*m_iHardDeadlineMs[\s\S]*m_iAbsoluteDeadlineMs\s*=\s*m_iPhaseDeadlineMs' 'A progressed crew phase must not starve the next gunner phase of its bounded timeout'
Assert-Contains 'STAGE4_GUNNER_BOARDING' ($vehicleBoarding + $vehicleBoardingTokens) 'IsReadyExactSeatWithoutTransition[\s\S]*CREW_ANIMATED_EXACT_ROLE_REISSUED' 'A mandatory crew GetIn action that never starts a transition must receive one exact animated retry'
Assert-Contains 'STAGE4_FALSE_COMPLETION_ROUTE' $orderPlanner 'GetReachablePoint[\s\S]*FALSE_COMPLETION_ROUTE_MIN_PROGRESS_METERS' 'False-completion recovery endpoints must be generated from the leader-connected navmesh island'
Assert-Contains 'STAGE4_FALSE_COMPLETION_ROUTE' ($controller + $groupSlot) 'RECOVERY_ROUTE_LEG_COMPLETED[\s\S]*GetFalseCompletionNoProgressCount' 'Physical progress on an intermediate route leg must not consume the repeated no-progress endpoint budget'

Assert-Contains 'STAGE4_CORPSE_RETENTION' $corpseRetention 'modded\s+class\s+SCR_GarbageSystem' 'Corpse retention must intercept the stock garbage system'
Assert-Contains 'STAGE4_CORPSE_RETENTION' $corpseRetention 'GetLifeState\(\)\s*==\s*ECharacterLifeState\.DEAD[\s\S]*OnInsertRequested[\s\S]*return\s+-1' 'Dead characters must be rejected before garbage tracking starts'
Assert-Contains 'STAGE4_CORPSE_RETENTION' $corpseRetention 'OnBeforeDelete[\s\S]*AICF_IsPersistentCorpse[\s\S]*return\s+false' 'Already tracked corpses must survive the final garbage deletion callback'
Assert-Contains 'STAGE4_CORPSE_RETENTION' $corpseRetention 'super\.OnInsertRequested[\s\S]*super\.OnBeforeDelete' 'Non-character garbage collection must retain the stock policy'

Assert-Contains 'STAGE4_ALLIED_MAP' $mapMarkers 'SetFaction\s*\(\s*markerFaction\s*\)' 'Group and objective markers must use allied faction stream rules'
Assert-Contains 'STAGE4_ALLIED_MAP' $mapMarkers 'visibility=ALLIED' 'Marker diagnostics must preserve the allied-only visibility policy'
Assert-Contains 'STAGE4_MAP_DIRECTION' $mapMarkers 'DescribeDirection[\s\S]*Math\.Atan2[\s\S]*DIR\s+%1\s+%2m' 'Group markers must expose movement bearing and distance'
Assert-Contains 'STAGE4_ATTACKED_BASES' $mapMarkers 'SyncFactionObjectiveMarkers[\s\S]*targets\.Find\s*\(\s*target\s*\)' 'Attack targets must be deduplicated before objective markers are created'
Assert-Contains 'STAGE4_ATTACKED_BASES' $mapMarkers 'markerKind\s*==\s*1[\s\S]*MarkerIcon[\s\S]*SetVisible\s*\(\s*false\s*\)' 'Attack targets must use a distinct text badge instead of a duplicate faction flag'
Assert-Contains 'STAGE4_ATTACKED_BASES' $mapMarkers 'AICF_ATTACK_BADGE_TEXT_NAME[\s\S]*SetTextVisible\s*\(\s*false\s*\)' 'Attack targets must use an independently positioned label instead of stock MarkerText'
Assert-Contains 'STAGE4_ATTACKED_BASES' $mapMarkers 'ATTACK_BADGE_NAME[\s\S]*FrameSlot\.SetAnchor\s*\(\s*attackBadge\s*,\s*0\.5\s*,\s*1\s*\)[\s\S]*FrameSlot\.SetPos\s*\(\s*attackBadge\s*,\s*0\s*,\s*18\s*\)' 'Attack badges must be placed below the stock base marker'
Assert-Contains 'STAGE4_ATTACKED_BASES' $mapMarkers 'attackerList\s*\+=\s*string\.Format\s*\(\s*"\+%1"[\s\S]*"ATK  %1"' 'One compact objective badge must aggregate all allied attacker slot keys'

Assert-Contains 'STAGE4_ORDER_AUTHORITY' $strategicRpc 'AICF_RequestStrategicOrder\s*\(\s*int\s+slotId\s*,\s*int\s+targetCallsign\s*\)' 'The client RPC may supply only slot identity and a base callsign'
Assert-Contains 'STAGE4_ORDER_AUTHORITY' $strategicRpc 'RplRcver\.Server[\s\S]*GetPlayerId\s*\(' 'Order requests must resolve player identity on the authoritative PlayerController'
Assert-NotContains 'STAGE4_ORDER_AUTHORITY' $strategicRpc 'AICF_RequestStrategicOrder\s*\([^)]*(?:Faction|playerId)' 'The public client request must not accept faction or player identity'
Assert-Contains 'STAGE4_ORDER_AUTHORITY' $controller 'RequestPlayerOrder[\s\S]*SGetPlayerFaction\s*\(\s*playerId\s*\)' 'Authority must derive the request faction from the player id'
Assert-Contains 'STAGE4_ORDER_AUTHORITY' $controller 'PLAYER_ORDER_RATE_LIMIT_MS[\s\S]*AssignPlayerOrder' 'Authority must rate-limit and validate player orders'
Assert-Contains 'STAGE4_ORDER_ROLE_GATE' $orderPlanner 'AssignPlayerOrder[\s\S]*IsTargetValidForRole' 'Player orders must use the existing role/target validity gate'
Assert-Contains 'STAGE4_ORDER_PERSISTENCE' $orderPlanner 'HasPlayerStrategicOrder\s*\(\)[\s\S]*ClearStrategicCandidate' 'A valid player order must survive routine commander reconciliation'
Assert-Contains 'STAGE4_ORDER_PERSISTENCE' $groupSlot 'm_bPlayerStrategicOrder[\s\S]*BeginPlayerStrategicOrder[\s\S]*ClearPlayerStrategicOrder' 'Group slots must own the lifecycle of explicit strategic orders'

Assert-Contains 'STAGE4_DIAGNOSTICS' $diagnostics '\[AICF\]\[STAGE4\]' 'Stage 4 must use its own log prefix'
Assert-NotContains 'STAGE4_NO_STATIC_PASS' ($config + $network + $selector + $economy + $delivery + $controller) 'status\s*=\s*PASS|\[RESULT\]\[PASS\]' 'Production/static code must not manufacture runtime acceptance'

# Negative-fixture self-check: removing the stock supply rollback must be caught.
$brokenEconomy = $economy -replace 'reservation\.GetBase\(\)\.AddSupplies\s*\(\s*reservation\.GetSupplyCost\(\)\s*\)\s*;', ''
$negativeDetected = $brokenEconomy -notmatch 'reservation\.GetBase\(\)\.AddSupplies\s*\(\s*reservation\.GetSupplyCost\(\)\s*\)'
if (-not $negativeDetected) {
    Add-Failure 'STAGE4_NEGATIVE_FIXTURE' 'Rollback negative fixture was not detected'
}

# Negative-fixture self-check: reusing CreateRect's Color after CreateWidget must be caught.
$brokenCreateRect = $createRect + "`r`n`tbackground.SetColor(color);"
$widgetRenderingNegativeDetected = $brokenCreateRect -match $redundantCreateRectColorPattern
if (-not $widgetRenderingNegativeDetected) {
    Add-Failure 'STAGE4_NEGATIVE_FIXTURE' 'CreateRect color-reuse negative fixture was not detected'
}

if ($failures.Count -gt 0) {
    Write-Host "Stage 4 static audit: FAIL ($($failures.Count) issue(s))" -ForegroundColor Red
    $failures | ForEach-Object { Write-Host " - $_" }
    exit 1
}

Write-Host 'Stage 4 static audit: PASS' -ForegroundColor Green
Write-Host 'negative_fixture=PASS widget_rendering_fixture=PASS always_on=PASS transaction=PASS delivery_balance=PASS replication=PASS strategic_ui=PASS order_authority=PASS'
