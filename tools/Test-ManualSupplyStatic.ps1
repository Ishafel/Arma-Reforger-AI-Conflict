param([string]$RepositoryRoot = (Split-Path -Parent $PSScriptRoot))
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'Stage3StaticAudit.Common.ps1')
$records = @(Get-AICFSourceRecords $RepositoryRoot)
$rpc = $records | Where-Object Name -eq 'AICF_SupplyTransportRpc.c'
$dispatch = Find-AICFClassRecord $records 'AICF_ManualSupplyDispatch'
$planner = Find-AICFClassRecord $records 'AICF_LogisticsPlanner'
$service = Find-AICFClassRecord $records 'AICF_LogisticsService'
$view = Find-AICFClassRecord $records 'AICF_SupplyMapUI'
$data = Find-AICFClassRecord $records 'AICF_SupplyMapBase'
$match = Find-AICFClassRecord $records 'AICF_MatchController'
$failures = [Collections.Generic.List[string]]::new()
$negativeCount = 0
function Method($Record, [string]$Name) {
    if (-not $Record) { throw "Missing record for $Name" }
    (ConvertTo-AICFCodeText (Get-AICFMethodBody $Record $Name)) -replace '\s+', ' '
}
function Require([string]$Rule, [string]$Code, [string]$Pattern, [string]$Mutation = '') {
    Assert-AICFContains $failures $Rule $Code $Pattern 'Manual supply transport contract'
    if ($Mutation) {
        if (-not $Code.Contains($Mutation)) { throw "Mutation absent: $Rule" }
        $broken = $Code.Replace($Mutation, '/* removed by negative case */')
        if ($broken -match $Pattern) { $failures.Add("[$Rule] Negative input was not rejected") }
        $script:negativeCount++
    }
}
$submit = Method $dispatch 'Submit'
$update = Method $dispatch 'Update'
$finish = Method $dispatch 'Finish'
$endpoint = Method $dispatch 'EndpointsValid'
$ask = Method $rpc 'RpcAsk_AICFSupplyTransport'
Require 'MANUAL_AUTHORITY' $submit '!Replication.IsServer\(\)' '!Replication.IsServer()'
Require 'MANUAL_CONTROLLER_OWNER' $submit 'GetPlayerController\(playerId\) != player' 'GetPlayerController(playerId) != player'
Require 'MANUAL_FACTION_AUTHORITY' $submit 'SGetPlayerFaction\(playerId\)' 'SGetPlayerFaction(playerId)'
Require 'MANUAL_SUPPORTED_SIDE' (Method $match 'RequestSupplyTransport') 'faction != m_USFaction && faction != m_USSRFaction'
Require 'MANUAL_MATCH_READINESS' (Method $match 'RequestSupplyTransport') '!m_bRosterReady[\s\S]*m_bReplanScheduled \|\| m_bGraphRebuildNeeded'
Require 'MANUAL_RPC_DEDUP' $ask 'request <= m_iAICFSupplyRequest \|\| m_bAICFSupplyBusy' 'request <= m_iAICFSupplyRequest'
Require 'MANUAL_RPC_RELIABLE' $rpc.Code '\[RplRpc\(RplChannel.Reliable, RplRcver.Server\)\]'
Require 'MANUAL_NETWORK_ID' (Method $dispatch 'ResolveBase') 'Replication.FindItem\(id\)' 'Replication.FindItem(id)'
Require 'MANUAL_NETWORK_ID_CLIENT' (Method $data 'NetworkId') 'm_EntityId[\s\S]*rpl.Id\(\)'
Assert-AICFNotContains $failures 'MANUAL_INTENT_ONLY' $rpc.Code 'RpcAsk_AICFSupplyTransport\([^)]*(?:Faction|playerId|EntityID)' 'RPC accepts intent, never authoritative identity'
Require 'MANUAL_BOTH_ENDPOINTS' $endpoint 'OwnedSafe\(source, faction\)[\s\S]*OwnedSafe\(destination, faction\)' 'OwnedSafe(destination, faction)'
Require 'MANUAL_NO_SELF_TRANSFER' $endpoint 'source.m_Pool.Overlaps\(destination.m_Pool\)' 'source.m_Pool.Overlaps(destination.m_Pool)'
Require 'MANUAL_AMOUNT_BOUNDS' $endpoint 'amount <= 0 \|\| amount > available' 'amount <= 0'
Require 'MANUAL_DESTINATION_CAPACITY' $endpoint 'amount > free' 'amount > free'
Require 'MANUAL_PRESERVE_COMMITMENTS' (Method $planner 'ManualAvailable') 'LogisticsObligations\(e.m_Base\)[\s\S]*Reserved\(e.m_Pool, false, exclude\)' 'LogisticsObligations(e.m_Base)'
Require 'MANUAL_PENDING_BOUND' $submit 'm_aRequests.Count\(\) >= 16[\s\S]*m_iNextAdmissionMs = now \+ 500'
Require 'MANUAL_FREE_EXACT_WORKER' $submit 'OwnsWorker\(w\)[\s\S]*w.m_Group \|\| w.m_Vehicle \|\| w.m_bCustody'
Require 'MANUAL_STABLE_WORKER' $submit 'w.m_iSlot >= selected.m_iSlot'
Require 'MANUAL_REPLACEMENT_COOLDOWN' $submit 'm_bCleanupComplete && now < w.m_iRetryAtMs \+ m_Planner.m_Config.m_iReplacementCooldownMs'
Require 'MANUAL_ROUTE_BEFORE_SPAWN' $submit 'Route\(load, destination, unload\)[\s\S]*BeginLogisticsSpawn\(selected\)[\s\S]*m_iGeneration = selected.m_iGeneration'
Require 'MANUAL_ASYNC_GENERATION' $update 'w.m_iGeneration != request.m_iGeneration' 'w.m_iGeneration != request.m_iGeneration'
Require 'MANUAL_PLAYER_RECHECK' (Method $dispatch 'PlayerValid') 'GetPlayerController\(request.m_iPlayer\) == request.m_Player[\s\S]*SGetPlayerFaction\(request.m_iPlayer\) == request.m_Worker.m_Faction'
Require 'MANUAL_READY_BEFORE_RESERVE' $update '!w.Ready\(\)[\s\S]*EndpointsValid\([\s\S]*m_CargoPool.Capacity\(\)[\s\S]*m_Book.Reserve\(' '!w.Ready()'
Require 'MANUAL_ACQUISITION_BEFORE_RESERVE' $update 'w.m_ePhase == AICF_ELogisticsPhase.SPAWN_PENDING \|\| !w.Ready\(\)\) continue;[\s\S]*m_Book.Reserve\(' 'w.m_ePhase == AICF_ELogisticsPhase.SPAWN_PENDING || '
Require 'MANUAL_EXACT_ENDPOINT_JOB' $update 'Reserve\(w, request.m_Source, request.m_Destination, request.m_iAmount'
Require 'MANUAL_JOB_BEFORE_MOVEMENT' $update 'request.m_Job = w.m_Job; request.m_Job.m_bManual = true;[\s\S]*BeginLogisticsLeg'
Require 'MANUAL_CANCEL_IDENTITY' $finish 'w.m_iGeneration == request.m_iGeneration[\s\S]*w.m_Job == request.m_Job[\s\S]*m_Book.Cancel\(w\)' 'w.m_Job == request.m_Job'
Require 'MANUAL_RESULT_ACTUAL_DELIVERY' $update 'w.m_fDelivered - request.m_fDeliveredBefore' 'w.m_fDelivered - request.m_fDeliveredBefore'
Require 'MANUAL_STATUS_OWNER_SNAPSHOT' $rpc.Code 'RplProp\(condition: RplCondition.OwnerOnly\)[\s\S]*m_sAICFSupplyStatus'
Require 'MANUAL_STATUS_AUTHORITY_CHANGE_ONLY' (Method $rpc 'AICF_SetSupplyStatus') '!Replication.IsServer\(\)[\s\S]*status == m_sAICFSupplyStatus\) return[\s\S]*Replication.BumpMe\(\)'
Require 'MANUAL_NO_IDLE_INTERFERENCE' (Method $service 'MaintainWorker') 'm_Manual.OwnsWorker\(w\)\) return' 'm_Manual.OwnsWorker(w)'
Require 'MANUAL_STOP' (Method $service 'Stop') 'm_Manual.Stop\(\)[\s\S]*m_Registry.Stop\(\)'
Require 'MANUAL_DESTINATION_EXPLICIT' (Method $view 'RebuildDestinations') 'm_Destination = null;[\s\S]*selectedIndex = m_aDestinations.Count\(\)'
Require 'MANUAL_SEND_RECHECK' (Method $view 'Send') 'Refresh\(\); if \(!m_bOpen \|\| !m_bCanSend\) return;[\s\S]*AICF_RequestSupplyTransport\(m_Selected.NetworkId\(\), m_Destination.NetworkId\(\), m_iAmount\)'
Require 'MANUAL_DESTINATION_LIFETIME' (Method $view 'Detach') 'm_DestinationCombo.m_OnChanged.Remove\(OnDestinationChanged\)' 'm_DestinationCombo.m_OnChanged.Remove(OnDestinationChanged)'
Assert-AICFNotContains $failures 'MANUAL_NO_RESOURCE_SIDE_EFFECT' ($view.Code + $rpc.Code + $dispatch.Code) 'AddSupplies\(|SpawnEntity|DeleteRplEntity|\.Transfer\(|SetResourceValue\(' 'Only existing owners perform physical/economic side effects'
if ($failures.Count) { $failures; exit 1 }
Write-Output "PASS Manual supply: authority, RPC/replicated result, bounds, exact route, generation, lifecycle; $negativeCount negative inputs. Runtime is a separate gate."
exit 0
