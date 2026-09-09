param([string]$RepositoryRoot = (Split-Path -Parent $PSScriptRoot))
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'Stage3StaticAudit.Common.ps1')
$records = @(Get-AICFSourceRecords $RepositoryRoot)
$system = Find-AICFClassRecord $records 'AICF_GroupMapMarkerSystem'
$entity = Find-AICFClassRecord $records 'SCR_MapMarkerEntity'
$widget = Find-AICFClassRecord $records 'SCR_MapMarkerDynamicWComponent'
$card = Find-AICFClassRecord $records 'AICF_MapMarkerCardWidget'
$failures = [System.Collections.Generic.List[string]]::new()
foreach ($record in @($system, $entity, $widget, $card)) {
    if (-not $record) { throw 'Missing unique marker component' }
}
function Require([string]$Rule, [string]$Code, [string]$Pattern) {
    Assert-AICFContains $failures $Rule $Code $Pattern 'Group marker presentation contract'
}
function Method($Record, [string]$Name) {
    ConvertTo-AICFCodeText (Get-AICFMethodBody $Record $Name)
}
$setter = Method $entity 'AICF_SetGroupMarkerData'
Require 'GROUP_MARKER_AUTHORITY' $setter '!Replication.IsServer\(\)'
Require 'GROUP_MARKER_CHANGE_ONLY' $setter 'm_sAICFGroupMarkerText == label && m_sAICFGroupDetails == details\)\) return;'
Require 'GROUP_MARKER_PUBLISH' $setter 'm_sAICFGroupMarkerText = label;\s*m_sAICFGroupDetails = details;\s*Replication.BumpMe\(\)'
Require 'GROUP_MARKER_JIP' $entity.Source '\[RplProp\(onRplName: "AICF_OnGroupDetailsReplicated"\)\]\s*protected string m_sAICFGroupDetails'
Require 'GROUP_MARKER_CALLBACK' (Method $entity 'AICF_OnGroupDetailsReplicated') 'm_MarkerWidgetComp.AICF_SetGroupDetails\(m_sAICFGroupDetails\)'
Require 'GROUP_MARKER_LATE_WIDGET' (Method $widget 'AICF_ConfigureGroupMarker') 'AICF_GetGroupMarkerText\(\)[\s\S]*AICF_GetGroupDetails\(\)'
Require 'GROUP_MARKER_CALLBACK_ISOLATION' (Method $widget 'AICF_SetGroupDetails') 'm_iAICFCardKind == 0\)'
Require 'LOGISTICS_MARKER_CALLBACK_ISOLATION' (Method $widget 'AICF_SetLogisticsDetails') 'm_iAICFCardKind == AICF_LogisticsMapMarkerSystem.MARKER_KIND\)'
$sync = Method $system 'SyncFaction'
Require 'GROUP_MARKER_LIVE_LEADER' $sync 'AICF_GroupRuntime.ResolveAliveLeader\(group\)'
Require 'GROUP_MARKER_RETARGET' $sync 'm_aTrackedGroups\[markerIndex\] != group \|\| m_aTrackedLeaders\[markerIndex\] != leader[\s\S]*marker.SetTarget\(leader\)'
Require 'GROUP_MARKER_FACTION' $sync 'marker.SetFaction\(markerFaction\);\s*marker.SetGlobalVisible\(true\)'
if ([regex]::Matches($sync, 'AICF_SetGroupMarkerData\s*\(\s*BuildMarkerText\([^;]+BuildMarkerDetails\(').Count -ne 2) {
    $failures.Add('[GROUP_MARKER_REFRESH] New and existing markers must receive live label and details')
}
$direction = Method $system 'DescribeDirection'
Require 'GROUP_MARKER_POSITION' $direction 'if \(!leader\) return[\s\S]*origin = leader.GetOrigin\(\)'
Assert-AICFNotContains $failures 'GROUP_MARKER_POSITION' $system.Code 'group.GetOrigin\(\)' 'Controller origin must not become a group position'
$presentation = (Method $system 'BuildMarkerText') + (Method $system 'BuildMarkerDetails') + (Method $system 'DescribeTask')
Assert-AICFNotContains $failures 'GROUP_MARKER_READ_ONLY' $presentation 'SpawnEntity|DeleteRplEntity|SetTarget|SetDecisionAuthority|CallLater' 'Card text must remain read-only'
Require 'GROUP_MARKER_HOVER_EXIT' (Method $widget 'OnMouseLeave') '!m_AICFCardWidget.Contains\(enterW\)[\s\S]*ShowDetails\(false\)'
Require 'GROUP_MARKER_HOVER_ZORDER' (Method $card 'ShowDetails') 'else m_Root.SetZOrder\(m_iOriginalZOrder\)'
if ($failures.Count) { $failures; exit 1 }
Write-Output 'PASS Group map markers: authority, JIP, callback isolation, live refresh, faction, leader identity and hover lifecycle.'
exit 0
