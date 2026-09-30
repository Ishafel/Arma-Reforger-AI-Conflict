param([string]$RepositoryRoot = (Split-Path -Parent $PSScriptRoot))
$ErrorActionPreference='Stop'
. (Join-Path $RepositoryRoot 'tools/Stage3StaticAudit.Common.ps1')
$root=Join-Path $RepositoryRoot 'AIConflictCore/Scripts/Game/AIConflict'
$sources=@{
 state=Get-Content (Join-Path $root 'State/Vehicles/AICF_FIAPatrol.c') -Raw
 service=Get-Content (Join-Path $root 'Vehicles/AICF_FIAPatrolService.c') -Raw
 node=Get-Content (Join-Path $root 'Forces/AICF_AINodeLifecycle.c') -Raw
 planner=Get-Content (Join-Path $root 'Orders/AICF_OrderPlanner.c') -Raw
 route=Get-Content (Join-Path $root 'Orders/AICF_InfantryApproachRoute.c') -Raw
}
function Test-Contracts($s) {
 $errors=[Collections.Generic.List[string]]::new()
 $rules=@(
  @('GENERATION','state','p.m_iGeneration != m_iGeneration'),
  @('LEG','state','p.m_iLeg != m_iLeg'),
  @('GRAPH_TOKEN','state','p.m_iGraphRevision != m_iGraphRevision'),
  @('VEHICLE_TOKEN','state','p.m_VehicleId != m_VehicleId'),
  @('WAYPOINT_TOKEN','state','m_Waypoint.GetID() == m_WaypointId'),
  @('CREW','state','!p.CrewIdentity()'),
  @('GRAPH_OWNER','service','p.m_iGraphRevision != m_Graph.GetRevision()'),
  @('CURRENT_OWNER','service','group.GetCurrentWaypoint() != waypoint'),
  @('FOREIGN_VEHICLE','service','vehicle && vehicle != p.m_Vehicle'),
  @('BOUND','service','p.m_iRouteRetries >= 2'),
  @('PROGRESS','service','p.m_fBestEndpointDistance - endpointDistance >= 15'),
  @('CALLBACK_FENCE','node','failure.IsCurrent(p)'),
  @('ACTION_FENCE','node','utility.GetExecutedAction() == failedAction'),
  @('UNKNOWN_ONLY','node','result != EMoveError.UNKNOWN'),
  @('TILE_IDENTITY','route','navmesh.IsTileLoaded(m_vTileCandidate)')
 )
 foreach($r in $rules){if(!$s[$r[1]].Contains($r[2])){$errors.Add($r[0])}}
 $advance=Get-AICFMethodBody ([pscustomobject]@{Source=$s.planner;Code=(ConvertTo-AICFCodeText $s.planner)}) 'TryAdvanceStuckRoute'
 if($advance -notmatch '(?s)!slot.IsStuckRouteContextCurrent.*?CanResumeTileWait.*?INFANTRY_APPROACH_TILE_READY.*?BeginStuckRouteCompletionWait'){$errors.Add('TILE_RESUME_ORDER')}
 $recover=Get-AICFMethodBody ([pscustomobject]@{Source=$s.service;Code=(ConvertTo-AICFCodeText $s.service)}) 'RecoverRoute'
 if($recover -match 'MoveFIAPatrol\(' -or $recover -notmatch 'ClearFIAPatrolWaypoint\(p\)'){$errors.Add('TEARDOWN_TICK')}
 $routeBody=Get-AICFMethodBody ([pscustomobject]@{Source=$s.service;Code=(ConvertTo-AICFCodeText $s.service)}) 'Route'
 if($routeBody -notmatch '(?s)if \(p.m_MoveFailure.IsCurrent\(p\)\).*?RecoverRoute\(p, now, "UNKNOWN_MOVE"\);\s*return;'){$errors.Add('DEFERRED_IDENTITY')}
 return $errors.ToArray()
}
$failures=@(Test-Contracts $sources)
if($failures.Count){$failures;exit 1}
$mutations=@(
 @('GENERATION','state','p.m_iGeneration != m_iGeneration'),
 @('LEG','state','p.m_iLeg != m_iLeg'),
 @('GRAPH_TOKEN','state','p.m_iGraphRevision != m_iGraphRevision'),
 @('VEHICLE_TOKEN','state','p.m_VehicleId != m_VehicleId'),
 @('WAYPOINT_TOKEN','state','m_Waypoint.GetID() == m_WaypointId'),
 @('CREW','state','!p.CrewIdentity()'),
 @('GRAPH_OWNER','service','p.m_iGraphRevision != m_Graph.GetRevision()'),
 @('CURRENT_OWNER','service','group.GetCurrentWaypoint() != waypoint'),
 @('FOREIGN_VEHICLE','service','vehicle && vehicle != p.m_Vehicle'),
 @('BOUND','service','p.m_iRouteRetries >= 2'),
 @('PROGRESS','service','p.m_fBestEndpointDistance - endpointDistance >= 15'),
 @('CALLBACK_FENCE','node','failure.IsCurrent(p)'),
 @('ACTION_FENCE','node','utility.GetExecutedAction() == failedAction'),
 @('UNKNOWN_ONLY','node','result != EMoveError.UNKNOWN'),
 @('TILE_IDENTITY','route','navmesh.IsTileLoaded(m_vTileCandidate)'),
 @('TILE_RESUME_ORDER','planner','route.CanResumeTileWait(slot.GetGroup())'),
 @('DEFERRED_IDENTITY','service','p.m_MoveFailure.IsCurrent(p)')
)
foreach($m in $mutations){
 $copy=$sources.Clone();$copy[$m[1]]=$copy[$m[1]].Replace($m[2],'false')
 if(-not (@(Test-Contracts $copy) -contains $m[0])){throw "Mutation escaped: $($m[0])"}
}
"Movement recovery contracts: PASS; negative mutations=$($mutations.Count); runtime=NOT_RUN"
