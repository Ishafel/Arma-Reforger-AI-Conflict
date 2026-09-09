param([string]$RepositoryRoot = (Split-Path -Parent $PSScriptRoot))
$ErrorActionPreference = 'Stop'
$root = Join-Path $RepositoryRoot 'AIConflictCore/Scripts/Game/AIConflict'
$model = Get-Content (Join-Path $root 'UI/AICF_LogisticsMapMarkers.c') -Raw
$wire = Get-Content (Join-Path $root 'UI/AICF_GroupMapMarkers.c') -Raw
$service = Get-Content (Join-Path $root 'Economy/AICF_LogisticsService.c') -Raw
$failures = [System.Collections.Generic.List[string]]::new()
function Require([string]$Name, [string]$Source, [string]$Pattern) {
    if ($Source -notmatch $Pattern) { $failures.Add($Name) }
}
Require 'AUTHORITY' $model 'void Sync[\s\S]*m_bStopped \|\| !Replication.IsServer\(\)'
Require 'LIVE_IDENTITY' $model 'static bool Trackable[\s\S]*m_bCustody[\s\S]*m_bCleanupComplete[\s\S]*w.VehicleIdentity\(\)[\s\S]*GetEntityFaction\(w.m_Vehicle\) != w.m_Faction[\s\S]*EDamageState.DESTROYED'
Require 'IMMUTABLE_RECORD' $model 'bool Matches[\s\S]*m_Faction == w.m_Faction[\s\S]*m_iSlot == w.m_iSlot[\s\S]*m_iGeneration == w.m_iGeneration[\s\S]*m_VehicleId == w.m_VehicleId[\s\S]*m_sVehicleRpl == w.m_sVehicleRpl[\s\S]*GetTarget\(\) == w.m_Vehicle'
Require 'REAL_VEHICLE_TARGET' $model 'InsertDynamicMarker\(SCR_EMapMarkerType.DYNAMIC_EXAMPLE, w.m_Vehicle, packed\)'
Require 'FACTION_BEFORE_VISIBLE' $model 'marker.SetFaction\(w.m_Faction\);\s*marker.SetGlobalVisible\(true\)'
Require 'RHS_FACTION_MAPPING' $model 'GetStableFactionKey\(w.m_Faction.GetFactionKey\(\)\)'
Require 'REPLACEMENT_REMOVAL' $model 'candidate.Matches\(w\)\) record = candidate;\s*else RemoveAt\(i\)'
Require 'STALE_REMOVAL' $model 'if \(!m_aMarkers\[stale\].m_bSeen\) RemoveAt\(stale\)'
Require 'STOP_REMOVAL' $model 'void Stop[\s\S]*m_bStopped = true;[\s\S]*RemoveAt\(i\)'
Require 'SERVICE_TICK' $service 'm_MapMarkers.Sync\(m_Registry.m_aWorkers, now\)'
Require 'SERVICE_STOP' $service 'void Stop[\s\S]*m_MapMarkers.Stop\(\);[\s\S]*m_Registry.Stop\(\)'
Require 'JIP_DETAILS' $wire '\[RplProp\(onRplName: "AICF_OnLogisticsDetailsReplicated"\)\]\s*protected string m_sAICFLogisticsDetails'
Require 'ONLY_CHANGED_BUMP' $wire 'void AICF_SetLogisticsMarkerData[\s\S]*!Replication.IsServer\(\)[\s\S]*m_sAICFGroupMarkerText == label && m_sAICFLogisticsDetails == details\)\) return;[\s\S]*Replication.BumpMe\(\)'
Require 'LATE_WIDGET_SNAPSHOT' $wire 'void AICF_ConfigureLogisticsMarker[\s\S]*marker.AICF_GetGroupMarkerText\(\)[\s\S]*marker.AICF_GetLogisticsDetails\(\)'
Require 'LIVE_CARGO' $model 'm_CargoPool && w.m_CargoPool.Valid\(\)[\s\S]*m_CargoPool.Value\(\)[\s\S]*m_CargoPool.Capacity\(\)'
Require 'JOB_GENERATION' $model '!w.m_Job.m_bCancelled && w.m_Job.m_iGeneration == w.m_iGeneration'
Require 'RECOVERY_STATE' $model 'm_RouteRecovery && w.m_RouteRecovery.m_bActive'
Require 'HOVER_OPEN' $wire 'override bool OnMouseEnter[\s\S]*ShowDetails\(true\)'
Require 'HOVER_CLOSE' $wire 'override bool OnMouseLeave[\s\S]*Contains\(enterW\)[\s\S]*ShowDetails\(false\)'
# A read model must not become another vehicle/economy/waypoint owner.
if ($model -match '(SpawnEntityPrefab|DeleteRplEntity|SetResourceValue|BeginLogisticsLeg|RetireLogistics|RpcAsk_|\.m_ePhase\s*=(?!=)|CallLater\()') {
    $failures.Add('READ_MODEL_SIDE_EFFECT')
}
if ($failures.Count) {
    $failures | ForEach-Object { Write-Output "FAIL $_" }
    exit 1
}
Write-Output 'PASS Logistics map markers: authority, identity, faction, JIP, cargo, hover and lifecycle contracts (20).'
exit 0
