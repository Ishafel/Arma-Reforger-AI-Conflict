param([string]$RepositoryRoot=(Split-Path -Parent (Split-Path -Parent $PSScriptRoot)))
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot '../lib/Stage3StaticAudit.Common.ps1')
$paths=@{
 receipt='AIConflictCore/Scripts/Game/AIConflict/Forces/AICF_RecruitmentMoveFailure.c'
 node='AIConflictCore/Scripts/Game/AIConflict/Forces/AICF_AINodeLifecycle.c'
 service='AIConflictCore/Scripts/Game/AIConflict/Forces/AICF_InfantryRecruitmentService.c'
 slot='AIConflictCore/Scripts/Game/AIConflict/State/AICF_GroupSlot.c'
}
$sources=@{};foreach($key in $paths.Keys){$sources[$key]=[IO.File]::ReadAllText((Join-Path $RepositoryRoot $paths[$key]))}
function Body($source,$method){Get-AICFMethodBody ([pscustomobject]@{Source=$source;Code=(ConvertTo-AICFCodeText $source)}) $method}
function Rules($set){
 $r=$set.receipt;$c=Body $r 'Capture';$v=Body $r 'IsCurrent';$t=Body $r 'TryRecover';$safe=Body $r 'SafeDestination';$dest=Body $r 'FindDestination'
 $hook=Body $set.node 'AICF_HandleRecruitmentFailedMovement';$ports=Body $set.node 'EOnTaskSimulate';$owner=Body $set.service 'IsCurrentOrder';$repair=Body $set.service 'CheckApproach'
 foreach($part in @('handler != AIGroupMovementComponent.DEFAULT_HANDLER_ID','result != EMoveError.STUCK && result != EMoveError.UNREACHABLE','!related','order.m_MoveFailure','agents.Count() != 1','action != activity','utility.GetExecutedAction()','activity.m_RelatedWaypoint != order.m_Waypoint','action.GetActionState() == EAIActionState.FAILED','action.GetActionState() == EAIActionState.COMPLETED','!receipt.IsCurrent(order, receipt.m_iRepair)')){if(!$c.Contains($part)){'CAPTURE_NATIVE_OWNER_RESULT';break}}
 foreach($part in @('order != m_Order','!order.m_RecruitmentService.IsCurrentOrder(order)','!order.m_Slot.MatchesRecruitmentOrder(order)','order.m_bAwaitingSupplyPlan','order.m_bDemandReleased','order.m_Donor','order.m_iArrivedAtMs > 0','!order.HasSafeBarracks()','age < 0 || age > 5000','m_iRepair >= 2','repair != order.m_iApproachRepairs','order.m_iGeneration != m_iGeneration','order.m_iAssignment != m_iAssignment','order.m_iIntent != m_iIntent','order.m_iGraphRevision != m_iGraph','order.m_iToken != m_iToken','m_Group.GetID() != m_GroupId','m_Waypoint.GetID() != m_WaypointId','m_Waypoint.GetOrigin() != m_vEndpoint','m_Waypoint.GetCompletionRadius() != m_fRadius','m_Group.GetCurrentWaypoint() != m_Waypoint','!queue.Contains(m_Waypoint)','agents.Count() != 1','agents[0] != m_Agent','m_Agent.GetParentGroup() != m_Group','m_Agent.GetControlledEntity() != m_Actor','m_Actor.GetID() != m_ActorId','GetControlAIAgent() != m_Agent','IsAuthoritativeAIEntity(m_Actor)','IsAuthoritativeReplicatedEntity(m_Group)','ToString() != m_sGroupRpl','ToString() != m_sActorRpl','vector.Distance(m_Actor.GetOrigin(), m_vOrigin) > 0.5','!controller.IsPlayerControlled()','faction.GetAffiliatedFaction() == order.m_Faction','!m_Actor.IsInVehicle()','!CompartmentAccessComponent.GetVehicleIn(m_Actor)','!access.IsInCompartment()','!access.IsGettingIn()','!access.IsGettingOut()','!access.IsSwitchingSeatsAnim()')){if(!$v.Contains($part)){'EXACT_LIVE_CONTEXT';break}}
 foreach($part in @('!Replication.IsServer()','m_bStopped','!m_Campaign.IsMaster()','!m_Campaign.IsRunning()','!m_aOrders.Contains(order)','order.m_RecruitmentService != this','m_Graph.GetRevision() != order.m_iGraphRevision','age >= 0 && age < timeout','PLAYER_APPROACH_TIMEOUT_MS','order.m_Slot.MatchesRecruitmentOrder(order)')){if(!$owner.Contains($part)){'SERVICE_DEADLINE_AUTHORITY';break}}
 if(!(Body $set.slot 'MatchesRecruitmentOrder').Contains('m_RecruitmentOrder == order')){'EXACT_SLOT_ORDER'}
 if(!$set.node.Contains('hasHandler && hasRelated && hasLocation && AICF_HandleRecruitmentFailedMovement')){'FULL_NATIVE_PORTS'}
 if($hook.IndexOf('AICF_RecruitmentMoveFailure.Capture(') -gt $hook.IndexOf('.OnMoveFailed(') -or $hook.IndexOf('order.m_MoveFailure = receipt;') -gt $hook.IndexOf('.OnMoveFailed(') -or !$hook.Contains('receipt.IsCapturedCurrent(order)') -or !$hook.Contains('GetExecutedAction() == action')){'CAPTURE_BEFORE_NATIVE_SAME_ACTION'}
 if(!$t.Contains('m_bConsumed = true;') -or !$t.Contains('IsCurrent(order, m_iRepair + 1)') -or !$t.Contains('order.m_MoveFailure != this') -or !$t.Contains('!order.m_bMovementFailed') -or $t.IndexOf('m_bConsumed = true;') -gt $t.IndexOf('.Teleport(') -or ([regex]::Matches($t,'\.Teleport\(')).Count -ne 1){'ONE_CONSUMED_BUDGETED_MUTATION'}
 if(([regex]::Matches($t,'!IsCurrent\(')).Count -ne 2 -or !$t.Contains('!SafeDestination(destination, checked)') -or !$t.Contains('vector.Distance(checked, destination) > 0.1') -or !$t.Contains('vector.Distance(m_Actor.GetOrigin(), destination) > 8')){'REPEATED_COMMIT_FENCE'}
 foreach($part in @('AICF_InfantrySpawnPlacement.IsUsable(m_Group, query, destination)','vector.DistanceSqXZ(query, destination) <= 0.25','vector.Distance(m_vOrigin, destination) >= 2','vector.Distance(m_vOrigin, destination) <= 8','Math.AbsFloat(m_vOrigin[1] - destination[1]) <= 1','ARRIVAL_METERS + 5','vector.DistanceXZ(m_vOrigin, m_vService) - 1')){if(!$safe.Contains($part)){'LOCAL_SAFE_STEP';break}}
 if(!$dest.Contains('ring < 3') -or !$dest.Contains('direction < 3') -or !$dest.Contains('(3 + ring * 2)') -or !$dest.Contains('ARRIVAL_METERS + 8')){'DETERMINISTIC_BOUNDED_SEARCH'}
 if(!$repair.Contains('order.m_iApproachRepairs >= 2') -or $repair.IndexOf('order.m_iApproachRepairs++;') -gt $repair.IndexOf('.TryRecover(order)') -or $repair.IndexOf('order.m_iApproachRepairs >= 2') -gt $repair.IndexOf('.TryRecover(order)') -or $repair.IndexOf('.TryRecover(order)') -gt $repair.IndexOf('RepairInfantryRecruitmentApproach(order)') -or !$repair.Contains('!order.m_bHiddenRecoveryPending') -or !$repair.Contains('order.m_MoveFailure = null;')){'EXISTING_REPAIR_BUDGET_AND_OWNER'}
 if((ConvertTo-AICFCodeText $r) -match '\b(?:CallLater|SetHealth|SetHealthScaled|SetLifeState|SetOrigin|SetVelocity|RepairInfantryRecruitmentApproach|AddAction|Fail|Complete)\s*\('){'NO_EXTRA_DOMAIN_MUTATIONS'}
}
$failures=@(Rules $sources);if($failures.Count){throw "Production failed: $($failures -join ',')"}
$mutants=@(
 @('receipt','handler != AIGroupMovementComponent.DEFAULT_HANDLER_ID','false'),
 @('receipt','result != EMoveError.STUCK && result != EMoveError.UNREACHABLE','false'),
 @('receipt','agents.Count() != 1','false'),@('receipt','action != activity','false'),
 @('receipt','order != m_Order','false'),@('receipt','order.m_iToken != m_iToken','false'),
 @('receipt','order.m_iGeneration != m_iGeneration','false'),@('receipt','order.m_iAssignment != m_iAssignment','false'),
 @('receipt','order.m_iIntent != m_iIntent','false'),@('receipt','order.m_iGraphRevision != m_iGraph','false'),
 @('receipt','m_Waypoint.GetID() != m_WaypointId','false'),@('receipt','m_Actor.GetID() != m_ActorId','false'),
 @('receipt','age > 5000','age > 999999'),@('receipt','m_iRepair >= 2','m_iRepair >= 99'),
 @('receipt','vector.Distance(m_Actor.GetOrigin(), m_vOrigin) > 0.5','false'),
 @('receipt','!controller.IsPlayerControlled()','true'),@('receipt','!access.IsGettingIn()','true'),
 @('receipt','m_bConsumed = true;',''),@('receipt','IsCurrent(order, m_iRepair + 1)','true'),
 @('receipt','vector.Distance(m_vOrigin, destination) <= 8','true'),
 @('receipt','vector.DistanceSqXZ(query, destination) <= 0.25','true'),
 @('receipt','Math.AbsFloat(m_vOrigin[1] - destination[1]) <= 1','true'),
 @('receipt','!SafeDestination(destination, checked)','false'),
 @('service','!m_aOrders.Contains(order)','false'),@('service','!m_Campaign.IsMaster()','false'),
 @('service','age >= 0 && age < timeout','true'),@('service','order.m_iApproachRepairs >= 2','false'),
 @('service','order.m_MoveFailure = null;',''),
 @('node','hasHandler && hasRelated && hasLocation && AICF_HandleRecruitmentFailedMovement','AICF_HandleRecruitmentFailedMovement'),
 @('node','receipt.IsCapturedCurrent(order)','true'),@('slot','m_RecruitmentOrder == order','true')
)
foreach($m in $mutants){$copy=@{};foreach($k in $sources.Keys){$copy[$k]=$sources[$k]};if(!$copy[$m[0]].Contains($m[1])){throw "Stale mutant $($m[1])"};$copy[$m[0]]=$copy[$m[0]].Replace($m[1],$m[2]);if(!@(Rules $copy).Count){throw "Escaped mutant: $($m[1])"}}
"PASS Recruitment short recovery contracts: production and $($mutants.Count) negative mutants"
