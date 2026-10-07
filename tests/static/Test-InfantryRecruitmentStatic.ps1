param([string]$RepositoryRoot = (Split-Path -Parent (Split-Path -Parent $PSScriptRoot)))
$ErrorActionPreference = 'Stop'
$core = Join-Path $RepositoryRoot 'AIConflictCore/Scripts/Game/AIConflict'
$failures = [System.Collections.Generic.List[string]]::new()
function Assert-Contract([string]$File, [string]$Pattern, [string]$Rule) {
    $source = [IO.File]::ReadAllText((Join-Path $core $File))
    if ($source -notmatch $Pattern) { $failures.Add($Rule) }
}
Assert-Contract 'State/AICF_GroupSlot.c' 'GetDeploymentSize\(\)[\s\S]*?INFANTRY[\s\S]*?return AICF_Stage1Config.MIN_GROUP_SIZE' 'INFANTRY_SEED_ONE'
Assert-Contract 'State/AICF_GroupSlot.c' 'HasRosterMember[\s\S]*?IsAliveCharacter[\s\S]*?GetParentGroup\(\) == m_Group' 'MEMBER_IDENTITY_AND_LIVENESS'
Assert-Contract 'Config/AICF_InfantryRecruitmentConfig.c' 'MAX_DISTANCE_METERS = 500' 'BOUNDED_DISTANCE'
Assert-Contract 'Config/AICF_InfantryRecruitmentConfig.c' 'm_iRiflemanCost = 10;[\s\S]*m_iMedicCost = 15;[\s\S]*m_iGrenadierCost = 20;' 'ROLE_PRICES'
Assert-Contract 'Forces/AICF_InfantryRecruitmentOrder.c' 'GetID\(\) == m_GroupId[\s\S]*GetSpawnGeneration\(\) == m_iGeneration[\s\S]*GetStrategicAssignmentRevision\(\) == m_iAssignment[\s\S]*GetStrategicIntentRevision\(\) == m_iIntent' 'STALE_REQUEST_FENCE'
Assert-Contract 'Forces/AICF_InfantryRecruitmentOrder.c' 'GetCaptureState\(\) != SCR_EBaseCaptureState.NONE[\s\S]*BARRACKS[\s\S]*ONLINE[\s\S]*services.Contains\(m_Service\)' 'LIVE_BARRACKS_REVALIDATION'
Assert-Contract 'Forces/AICF_InfantryRecruitmentOrder.c' 'ResolveAliveLeader[\s\S]*ARRIVAL_METERS[\s\S]*CountAliveAgentsInAnyVehicle' 'PHYSICAL_PRESENCE'
Assert-Contract 'Forces/AICF_InfantryRecruitmentService.c' 'GetHopDistance\(nearest, base\) != 1[\s\S]*distanceSq >= targetDistance' 'LOCAL_OR_NEIGHBOR_CLOSER_THAN_TARGET'
Assert-Contract 'Forces/AICF_InfantryRecruitmentService.c' 'm_Campaign.IsMaster\(\)[\s\S]*m_iGraphRevision != m_Graph.GetRevision\(\)' 'MASTER_AND_GRAPH_GUARD'
Assert-Contract 'Forces/AICF_InfantryRecruitmentService.c' 'HasExactFactionRoster[\s\S]*DebitInfantryRecruit[\s\S]*RemoveAgent\(recruit\)[\s\S]*AddAgent\(recruit\)[\s\S]*RefundInfantryRecruit[\s\S]*RecordRecruitedMember' 'READY_PAY_TRANSFER_ROLLBACK'
Assert-Contract 'Forces/AICF_InfantryRecruitmentService.c' 'VISIT_TIMEOUT_MS[\s\S]*APPROACH_TIMEOUT_MS[\s\S]*SPAWN_TIMEOUT_MS' 'BOUNDED_WAIT'
Assert-Contract 'Economy/AICF_InfantryRecruitmentEconomy.c' 'QuoteInfantryRecruit\(order\) \|\| !order.IsPhysicallyPresent\(\)[\s\S]*AddSupplies\(-order.m_iCost\)[\s\S]*before - after[\s\S]*RefundInfantryRecruit' 'LIVE_EXACT_DEBIT'
Assert-Contract 'Forces/AICF_InfantryRecruitSpawner.c' 'PurgeSpawnRequestsForGroup[\s\S]*IsPlayerControlled[\s\S]*IsMaster[\s\S]*DespawnMembers[\s\S]*DeleteRplEntity' 'OWNED_PENDING_CLEANUP'
Assert-Contract 'Orders/AICF_OrderPlanner.c' 'CanRecruitInfantry[\s\S]*HasPlayerStrategicIntent[\s\S]*IsAICommanderEnabled' 'PLAYER_ORDER_PRIORITY'
Assert-Contract 'Orders/AICF_OrderPlanner.c' 'bool CanRecruitInfantry\([^}]*!slot.HasPendingOrderRecovery\(\)' 'PENDING_RECOVERY_BEFORE_RECRUITMENT'
Assert-Contract 'Orders/AICF_OrderPlanner.c' '(?s)bool BeginInfantryRecruitment\(.*?if \(!navmesh.IsTileLoaded\(order.m_vPosition\) && !navmesh.IsTileRequested\(order.m_vPosition\)\)\s*navmesh.LoadTileIn\(order.m_vPosition\);\s*if \(!pathfinding.GetClosestPositionOnNavmesh' 'RECRUITMENT_NO_DUPLICATE_TILE_REQUEST'
Assert-Contract 'Bootstrap/AICF_MatchController.c' 'm_InfantryRecruitment.Stop\(\)[\s\S]*m_EconomySystem.Stop' 'STOP_BEFORE_ECONOMY'
$service = [IO.File]::ReadAllText((Join-Path $core 'Forces/AICF_InfantryRecruitmentService.c'))
$slotSource = [IO.File]::ReadAllText((Join-Path $core 'State/AICF_GroupSlot.c'))
$plannerSource = [IO.File]::ReadAllText((Join-Path $core 'Orders/AICF_OrderPlanner.c'))
function Test-MusterContracts([string]$Slot, [string]$Planner, [string]$Service) {
    $rules = @(
        @($Slot, 'CountAliveAgents\(m_Group\) < GetDesiredSize\(\)[\s\S]*?return false;[\s\S]*?m_bInfantryMusterComplete = true;', 'MUSTER_FULL_ROSTER'),
        @($Slot, '!m_bInfantryMusterComplete && GetDesiredSize\(\) > 1 && !HasPlayerStrategicIntent\(\)', 'MUSTER_PLAYER_PRIORITY'),
        @($Slot, 'void ClearRuntimeReferences\(\)[\s\S]*?m_bInfantryMusterComplete = false;', 'MUSTER_NEW_GENERATION'),
        @($Slot, 'return IsWaitingForInfantryMuster\(\) \|\|', 'MUSTER_RECOVERY_EXCLUSION'),
        @($Planner, 'decisionAuthority == AICF_EStrategicDecisionAuthority.AI_COMMANDER && slot.NeedsInfantryMuster\(\)[\s\S]*?return HoldInfantryForMuster\(slot, faction\);[\s\S]*?AIWaypoint newWaypoint;', 'MUSTER_BEFORE_ATTACK'),
        @($Planner, 'bool HoldInfantryForMuster[\s\S]*?ResolveAliveLeader\(slot.GetGroup\(\)\)[\s\S]*?CreatePositionWaypoint\(leader.GetOrigin\(\)\)', 'MUSTER_PHYSICAL_ORIGIN'),
        @($Service, 'RefreshInfantryMuster\(slot, faction, m_Graph, m_Selector\);', 'MUSTER_UPDATE'),
        @($Planner, 'bool ConfigureInfantryServiceWaypoint[^}]*SetUseTurrets\(false\);[^}]*SetFractionOfSA\(0\);', 'SERVICE_NO_TURRET_OR_SMART_ACTION'),
        @($Planner, 'bool HoldInfantryForMuster[\s\S]*?ConfigureInfantryServiceWaypoint\(waypoint\)[\s\S]*?ClearOrder\(slot\);', 'MUSTER_FOOT_POSTURE'),
        @($Planner, 'bool BeginInfantryRecruitment[\s\S]*?ConfigureInfantryServiceWaypoint\(waypoint\)', 'RECRUITMENT_FOOT_POSTURE'),
        @($Service, 'ScheduleRetry\(slot\);[\s\S]*?CountAliveAgentsInAnyVehicle[\s\S]*?if \(slot.IsWaitingForInfantryMuster\(\)\)\s*m_Planner.HoldInfantryForMuster\(slot, faction\);\s*continue;', 'MUSTER_TURRET_EXIT_RETRY')
    )
    foreach ($rule in $rules) { if ($rule[0] -notmatch $rule[1]) { $rule[2] } }
}
foreach ($failure in @(Test-MusterContracts $slotSource $plannerSource $service)) { $failures.Add($failure) }
$mutations = @(
    @('slot','CountAliveAgents(m_Group) < GetDesiredSize()', 'false', 'MUSTER_FULL_ROSTER'),
    @('slot','!m_bInfantryMusterComplete && GetDesiredSize() > 1 && !HasPlayerStrategicIntent()', 'true', 'MUSTER_PLAYER_PRIORITY'),
    @('slot','m_bInfantryMusterComplete = false;', 'm_bInfantryMusterComplete = true;', 'MUSTER_NEW_GENERATION'),
    @('slot','return IsWaitingForInfantryMuster() ||', 'return', 'MUSTER_RECOVERY_EXCLUSION'),
    @('planner','return HoldInfantryForMuster(slot, faction);', 'return false;', 'MUSTER_BEFORE_ATTACK'),
    @('planner','CreatePositionWaypoint(leader.GetOrigin())', 'CreatePositionWaypoint(slot.GetGroup().GetOrigin())', 'MUSTER_PHYSICAL_ORIGIN'),
    @('service','m_Planner.RefreshInfantryMuster(slot, faction, m_Graph, m_Selector);', '', 'MUSTER_UPDATE'),
    @('planner','preset.SetUseTurrets(false);', 'preset.SetUseTurrets(true);', 'SERVICE_NO_TURRET_OR_SMART_ACTION'),
    @('planner','preset.SetFractionOfSA(0);', 'preset.SetFractionOfSA(1);', 'SERVICE_NO_TURRET_OR_SMART_ACTION'),
    @('planner','!ConfigureInfantryServiceWaypoint(waypoint)', 'false', 'MUSTER_FOOT_POSTURE'),
    @('planner','!ConfigureInfantryServiceWaypoint(waypoint)', 'false', 'RECRUITMENT_FOOT_POSTURE'),
    @('service','m_Planner.HoldInfantryForMuster(slot, faction);', '', 'MUSTER_TURRET_EXIT_RETRY')
)
foreach ($mutation in $mutations) {
    $inputs = @{slot=$slotSource;planner=$plannerSource;service=$service}
    $inputs[$mutation[0]] = $inputs[$mutation[0]].Replace($mutation[1], $mutation[2])
    if ($mutation[3] -notin @(Test-MusterContracts $inputs.slot $inputs.planner $inputs.service)) { $failures.Add('MUSTER_MUTATION_ESCAPED: ' + $mutation[3]) }
}
if ($service -match 'CallLater\s*\(|GetOn\w+\(\)\.Insert') { $failures.Add('UNOWNED_CALLBACK') }
if (Test-Path (Join-Path $core 'Forces/AICF_InfantryRecruitmentRuntimeProbe.c')) { $failures.Add('FIXTURE_IN_PRODUCTION') }
if ($failures.Count) {
    Write-Output 'Infantry recruitment static: FAIL'
    $failures | ForEach-Object { Write-Output " - $_" }
    exit 1
}
Write-Output 'Infantry recruitment static: PASS (seed, identity, locality, payment, readiness, cleanup, authority)'
Write-Output ('Infantry muster contracts: PASS (' + $mutations.Count + ' negative mutations)')
