param([string]$RepositoryRoot=(Split-Path -Parent (Split-Path -Parent $PSScriptRoot)))
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot '../lib/Stage3StaticAudit.Common.ps1')
$root=Join-Path $RepositoryRoot 'AIConflictCore/Scripts/Game/AIConflict'
$policy=[IO.File]::ReadAllText((Join-Path $root 'Orders/AICF_PlayerMobHoldPolicy.c'))
$match=[IO.File]::ReadAllText((Join-Path $root 'Bootstrap/AICF_MatchController.c'))
$arrival=[IO.File]::ReadAllText((Join-Path $root 'Orders/AICF_DefendArrivalHandoff.c'))
function Body([string]$text,[string]$name){
 Get-AICFMethodBody ([pscustomobject]@{Source=$text;Code=(ConvertTo-AICFCodeText $text)}) $name
}
function Rules([string]$source,[string]$controller,[string]$physical){
 $hold=Body $source 'IsReachedCurrentHold'
 $idle=Body $controller 'ResolveAllowedIdleReason'
 $formation=Body $physical 'IsFormationAtWaypoint'
 if(!$hold -or !$idle -or !$formation){'MISSING_METHOD';return}
 if($hold -notmatch '!Replication.IsServer\(\)' -or $hold -notmatch '!slot.IsCombatReady\(\)' -or $hold -notmatch '!mainBase.GetOwner\(\)' -or $hold -notmatch 'mobRadiusMeters <= 0'){'AUTHORITY_READY_INPUTS'}
 if($hold -notmatch 'slot.GetDecisionAuthority\(\) != AICF_EStrategicDecisionAuthority.PLAYER_COMMAND' -or $hold -notmatch '!slot.HasPlayerStrategicIntent\(\)'){'CURRENT_PLAYER_AUTHORITY'}
 if($hold -notmatch 'slot.GetPlayerStrategicIntentTargetKind\(\) != AICF_EOrderTargetKind.POSITION' -or $hold -notmatch 'slot.GetTargetKind\(\) != AICF_EOrderTargetKind.POSITION'){'POINT_ONLY'}
 if($hold -notmatch 'slot.GetStrategicIntentPosture\(\) != "MOVE_AND_HOLD"' -or $hold -notmatch 'slot.GetOperationalPosture\(\) != "MOVE_AND_HOLD"'){'CURRENT_HOLD_POSTURE'}
 if($hold -notmatch 'slot.GetTargetPosition\(\) != slot.GetPlayerStrategicIntentTargetPosition\(\)'){'EXACT_CURRENT_INTENT'}
 if($hold -notmatch 'slot.HasPendingOrderRecovery\(\) \|\| slot.HasFailedMovement\(\)' -or $hold -notmatch 'slot.IsApproachRouteWaypoint\(\)'){'NO_RECOVERY_OR_ROUTE_LEG'}
 if($hold -notmatch 'SCR_DefendWaypoint.Cast\(slot.GetWaypoint\(\)\)' -or $hold -notmatch 'group.GetCurrentWaypoint\(\) != waypoint' -or $hold -notmatch 'group.GetWaypoints\(waypoints\)' -or $hold -notmatch '!waypoints.Contains\(waypoint\)'){'CURRENT_BOUND_DEFEND'}
 if($hold -notmatch 'vector.DistanceSqXZ\(targetPosition, mobOrigin\) > mobRadiusMeters \* mobRadiusMeters' -or $hold -notmatch 'vector.DistanceSqXZ\(waypoint.GetOrigin\(\), mobOrigin\) > mobRadiusMeters \* mobRadiusMeters'){'TARGET_INSIDE_MOB'}
 if($hold -notmatch 'vector.DistanceSqXZ\(waypoint.GetOrigin\(\), targetPosition\) > 1.0'){'WAYPOINT_AT_CURRENT_TARGET'}
 if($hold -notmatch 'return AICF_DefendArrivalHandoff.IsFormationAtWaypoint\(group, waypoint\);'){'PHYSICAL_ALL_MEMBER_ARRIVAL'}
 if($formation -notmatch 'AICF_GroupRuntime.IsAliveCharacter\(member\)' -or $formation -notmatch 'agent.GetParentGroup\(\) != group' -or $formation -notmatch 'access.IsInCompartment\(\) \|\| access.IsGettingIn\(\) \|\| access.IsGettingOut\(\)' -or $formation -notmatch 'vector.DistanceSqXZ\(member.GetOrigin\(\), waypoint.GetOrigin\(\)\) > radius \* radius' -or $formation -notmatch 'return alive > 0;'){'ALIVE_FOOT_ROSTER_RADIUS'}
 if($idle -notmatch 'if \(AICF_PlayerMobHoldPolicy.IsReachedCurrentHold\(slot, mainBase, STUCK_WATCHDOG_IGNORE_RADIUS_METERS\)\)\s*return "PLAYER_POSITION_HOLD_AT_MOB";'){'NARROW_IDLE_INTEGRATION'}
 if($source -match 'CallLater|Insert\(|SetOrigin|Teleport|SpawnEntity|DeleteEntity|CommitTransition|ResetMob|ObserveUnexplainedMobIdle|DeadlineMs\s*='){'READ_ONLY_NO_DEADLINE_MUTATION'}
}
$failures=@(Rules $policy $match $arrival)
if($failures.Count){throw "Player MOB hold: $failures"}
$controls=1
foreach($mutation in @(
 @('!Replication.IsServer()','false','AUTHORITY_READY_INPUTS'),
 @('slot.GetDecisionAuthority() != AICF_EStrategicDecisionAuthority.PLAYER_COMMAND','false','CURRENT_PLAYER_AUTHORITY'),
 @('!slot.HasPlayerStrategicIntent()','false','CURRENT_PLAYER_AUTHORITY'),
 @('slot.GetPlayerStrategicIntentTargetKind() != AICF_EOrderTargetKind.POSITION','false','POINT_ONLY'),
 @('slot.GetTargetKind() != AICF_EOrderTargetKind.POSITION','false','POINT_ONLY'),
 @('slot.GetStrategicIntentPosture() != "MOVE_AND_HOLD"','false','CURRENT_HOLD_POSTURE'),
 @('slot.GetTargetPosition() != slot.GetPlayerStrategicIntentTargetPosition()','false','EXACT_CURRENT_INTENT'),
 @('slot.HasPendingOrderRecovery() || slot.HasFailedMovement()','false','NO_RECOVERY_OR_ROUTE_LEG'),
 @('group.GetCurrentWaypoint() != waypoint','false','CURRENT_BOUND_DEFEND'),
 @('!waypoints.Contains(waypoint)','false','CURRENT_BOUND_DEFEND'),
 @('vector.DistanceSqXZ(targetPosition, mobOrigin) > mobRadiusMeters * mobRadiusMeters','false','TARGET_INSIDE_MOB'),
 @('vector.DistanceSqXZ(waypoint.GetOrigin(), mobOrigin) > mobRadiusMeters * mobRadiusMeters','false','TARGET_INSIDE_MOB'),
 @('vector.DistanceSqXZ(waypoint.GetOrigin(), targetPosition) > 1.0','false','WAYPOINT_AT_CURRENT_TARGET'),
 @('return AICF_DefendArrivalHandoff.IsFormationAtWaypoint(group, waypoint);','return true;','PHYSICAL_ALL_MEMBER_ARRIVAL')
)){
 if(!$policy.Contains($mutation[0])){throw "Mutation target absent: $($mutation[2])"}
 if($mutation[2] -notin @(Rules ($policy.Replace($mutation[0],$mutation[1])) $match $arrival)){throw "Mutation survived: $($mutation[2])"}
 $controls++
}
$broad=$match.Replace('if (AICF_PlayerMobHoldPolicy.IsReachedCurrentHold(slot, mainBase, STUCK_WATCHDOG_IGNORE_RADIUS_METERS))','if (slot.HasPlayerStrategicIntent())')
if('NARROW_IDLE_INTEGRATION' -notin @(Rules $policy $broad $arrival)){throw 'Broad player exemption mutation survived'}
$controls++
$leaderOnly=$arrival.Replace('vector.DistanceSqXZ(member.GetOrigin(), waypoint.GetOrigin()) > radius * radius','false')
if('ALIVE_FOOT_ROSTER_RADIUS' -notin @(Rules $policy $match $leaderOnly)){throw 'Missing physical member radius mutation survived'}
$controls++
$mutating=$policy.Replace('vector targetPosition = slot.GetTargetPosition();','slot.ResetMobEgressRecovery(); vector targetPosition = slot.GetTargetPosition();')
if('READ_ONLY_NO_DEADLINE_MUTATION' -notin @(Rules $mutating $match $arrival)){throw 'Watchdog reset mutation survived'}
$controls++
"PASS Player MOB hold: current PLAYER POSITION MOVE_AND_HOLD, exact bound target inside MOB, all living members physically arrived on foot, no broad exemption or deadline mutation; controls=$controls"
