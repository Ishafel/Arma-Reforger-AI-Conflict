param([string]$RepositoryRoot = (Split-Path -Parent $PSScriptRoot))
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
Assert-Contract 'Bootstrap/AICF_MatchController.c' 'm_InfantryRecruitment.Stop\(\)[\s\S]*m_EconomySystem.Stop' 'STOP_BEFORE_ECONOMY'
$service = [IO.File]::ReadAllText((Join-Path $core 'Forces/AICF_InfantryRecruitmentService.c'))
if ($service -match 'CallLater\s*\(|GetOn\w+\(\)\.Insert') { $failures.Add('UNOWNED_CALLBACK') }
if (Test-Path (Join-Path $core 'Forces/AICF_InfantryRecruitmentRuntimeProbe.c')) { $failures.Add('FIXTURE_IN_PRODUCTION') }
if ($failures.Count) {
    Write-Output 'Infantry recruitment static: FAIL'
    $failures | ForEach-Object { Write-Output " - $_" }
    exit 1
}
Write-Output 'Infantry recruitment static: PASS (seed, identity, locality, payment, readiness, cleanup, authority)'
