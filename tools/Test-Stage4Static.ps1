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
$mapMarkers = Read-Required 'UI\AICF_GroupMapMarkers.c'
$strategicRpc = Read-Required 'UI\AICF_StrategicCommandRpc.c'
$strategicUI = Read-Required 'UI\AICF_StrategicUI.c'

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

foreach ($field in @(
    'm_sAICFUSStrategicObjective', 'm_sAICFUSOrderTargets',
    'm_sAICFUSGroup0', 'm_sAICFUSGroup1', 'm_sAICFUSGroup2', 'm_sAICFUSGroup3',
    'm_iAICFUSCombatGroups', 'm_iAICFUSManagedAgents',
    'm_sAICFUSSRStrategicObjective', 'm_sAICFUSSROrderTargets',
    'm_sAICFUSSRGroup0', 'm_sAICFUSSRGroup1', 'm_sAICFUSSRGroup2', 'm_sAICFUSSRGroup3',
    'm_iAICFUSSRCombatGroups', 'm_iAICFUSSRManagedAgents'
)) {
    Assert-Contains 'STAGE4_STRATEGIC_REPLICATION' $campaignState ("RplProp[\s\S]{0,100}" + [regex]::Escape($field)) "Missing strategic replicated field $field"
}

Assert-Contains 'STAGE4_HUD' $strategicUI 'TICKETS\s+%1[\s\S]*SUPPLY\s+%2[\s\S]*SQUADS\s+%3[\s\S]*OBJECTIVE' 'Compact HUD must expose tickets, supply, squads, and the current objective'
Assert-Contains 'STAGE4_WIDGET_HIERARCHY' $strategicUI 'CreateRect[\s\S]*FrameWidgetTypeID[\s\S]*RECT_BACKGROUND_NAME' 'Text and controls must be siblings of a background image inside a FrameWidget container'
Assert-Contains 'STAGE4_WIDGET_RENDERING' $strategicUI 'ImageWidgetTypeID[\s\S]*WidgetFlags\.BLEND[\s\S]*background\.SetColor\s*\(\s*color\s*\)' 'Programmatic panel backgrounds must alpha-blend their explicit dark color'
Assert-Contains 'STAGE4_WIDGET_RENDERING' $strategicUI 'RefreshVisualStyles[\s\S]*SetRectColor\s*\(\s*m_wHUDRoot[\s\S]*SetRectColor\s*\(\s*m_wCommandPanel[\s\S]*foreach\s*\(\s*Widget\s+targetButton' 'Top-level and dynamic panel colors must be restored after Enfusion widget initialization'
Assert-Contains 'STAGE4_WIDGET_INPUT' $strategicUI 'ButtonWidgetTypeID[\s\S]*inputWidget\.SetName\s*\(\s*RECT_INPUT_NAME\s*\)' 'Every clickable rectangle must own a real ButtonWidget input surface'
Assert-Contains 'STAGE4_WIDGET_INPUT' $strategicUI 'inputWidget\.AddHandler\s*\(\s*handler\s*\)' 'Button input surfaces must receive the strategic action handler'
Assert-Contains 'STAGE4_COMMAND_SURFACE' $strategicUI 'ARMY / SELECT GROUP' 'Command surface must expose army composition'
Assert-Contains 'STAGE4_COMMAND_SURFACE' $strategicUI 'SELECT A READY GROUP[\s\S]*NO VALID TARGETS FOR THIS ROLE' 'Command surface must explain empty target states'
Assert-Contains 'STAGE4_COMMAND_SURFACE' $strategicUI 'REINFORCEMENTS[\s\S]*SHIPMENTS' 'Command surface must expose reinforcement and logistics state'
Assert-Contains 'STAGE4_COMMAND_SURFACE' $strategicUI 'FormatGroupSummary[\s\S]*POSTURE[\s\S]*VEH[\s\S]*REINF' 'Unit cards must expose role, state, posture, vehicle phase, and reinforcement ETA'
Assert-Contains 'STAGE4_COMMAND_SURFACE' $strategicUI 'AICF_RequestStrategicOrder\s*\(' 'Command target buttons must issue a strategic-order RPC'

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

if ($failures.Count -gt 0) {
    Write-Host "Stage 4 static audit: FAIL ($($failures.Count) issue(s))" -ForegroundColor Red
    $failures | ForEach-Object { Write-Host " - $_" }
    exit 1
}

Write-Host 'Stage 4 static audit: PASS' -ForegroundColor Green
Write-Host 'negative_fixture=PASS default_off=PASS transaction=PASS delivery_balance=PASS replication=PASS strategic_ui=PASS order_authority=PASS'
