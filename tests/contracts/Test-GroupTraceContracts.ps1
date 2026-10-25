param([string]$RepositoryRoot=(Split-Path -Parent (Split-Path -Parent $PSScriptRoot)))
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot '../lib/Stage3StaticAudit.Common.ps1')
$path=Join-Path $RepositoryRoot 'AIConflictCore/Scripts/Game/AIConflict/Diagnostics/AICF_GroupTrace.c'
$trace=[IO.File]::ReadAllText($path)
$coordinator=[IO.File]::ReadAllText((Join-Path $RepositoryRoot 'AIConflictCore/Scripts/Game/AIConflict/Vehicles/AICF_VehicleCoordinator.c'))
function Rules([string]$source){
 $code=ConvertTo-AICFCodeText $source
 $record=[pscustomobject]@{Source=$source;Code=$code}
 $snapshot=Get-AICFMethodBody $record 'Snapshot'
 $update=Get-AICFMethodBody $record 'Update'
 $member=Get-AICFMethodBody $record 'Member'
 if($snapshot -notmatch '!Replication.IsServer\(\)'){ 'AUTHORITY' }
 if($code -match '\b(?:SetReserved|AddAction|GetInVehicle|GetOutVehicle|SetOrigin|CommitTransition|BumpMe|SpawnEntityPrefab|DeleteRplEntity|CallLater|Insert)\(' -and $code -notmatch '^$'){
  # Единственная Insert — собственный cursor, без удержания engine entities.
  if($code -match '\b(?:SetReserved|AddAction|GetInVehicle|GetOutVehicle|SetOrigin|CommitTransition|BumpMe|SpawnEntityPrefab|DeleteRplEntity|CallLater)\('){'SIDE_EFFECT'}
 }
 if($update -notmatch 'now < cursor.m_iNextMs' -or $source -notmatch 'Math.Clamp\(value.ToInt\(\), 1000, 60000\)'){'RATE_BOUND'}
 if($update -notmatch 'slot.IsCombatReady\(\)' -or $update -notmatch '!m_bAllGroups && !slot.HasVehiclePreset\(\)'){'SCOPE'}
 if($snapshot -notmatch 'AICF_GroupRuntime.ResolveAliveLeader\(group\)' -or $snapshot -match 'Position\(group\)'){'REAL_LEADER'}
 if($member -notmatch 'agent && boarding && boarding.GetTokens\(\)'){'UNSTARTED_BOARDING_SAFE'}
 foreach($field in @('selected_action=','executed_action=','expected_seat=','actual_seat=','reserved_by_member=','get_in_locked=','getting_in=','getting_out=','life=')){if(!$source.Contains($field)){'FIELD_'+$field}}
 if((Get-AICFMethodBody $record 'Stop') -notmatch 'm_aCursors.Clear\(\)'){'STOP'}
}
$failures=@(Rules $trace)
if($failures.Count){throw "Trace guards: $failures"}
if($coordinator -notmatch 'm_GroupTrace.Update\(usState, this\)' -or $coordinator -notmatch 'm_GroupTrace.Update\(ussrState, this\)' -or $coordinator -notmatch 'm_GroupTrace.Stop\(\)'){throw 'Coordinator lifecycle'}
$controls=1
$native=[IO.File]::ReadAllText((Join-Path $RepositoryRoot 'AIConflictCore/Scripts/Game/AIConflict/Diagnostics/AICF_NativeVehicleTrace.c'))
if(@([regex]::Matches($native,'super.OnActionSelected\(\);')).Count -ne 2 -or @([regex]::Matches($native,'super.WrapBehaviorOutsideOfVehicle\(action\);')).Count -ne 1 -or @([regex]::Matches($native,'super.PerformReaction\(utility, message\);')).Count -ne 1){throw 'Native callbacks must delegate exactly once'}
if($native -match '\b(?:AddAction|Fail|Complete|SetOrigin|GetOutVehicle|GetInVehicle)\('){throw 'Native observer changes behavior'}
if($native -notmatch '\[BaseContainerProps\(\)\]\s*modded class SCR_AIGoalReaction_GetOutVehicle'){throw 'Native serialized reaction must retain BaseContainerProps'}
if(@([regex]::Matches($native,'super.OnMoveFailed\(moveResult, vehicleUsed, isWaypointRelated, moveLocation\);')).Count -ne 1 -or !$native.Contains('NATIVE_GROUP_MOVE_FAILED')){throw 'Native movement failure observer must preserve original callback'}
$controls++
foreach($mutation in @(
 @('!Replication.IsServer()','false','AUTHORITY'),
 @('now < cursor.m_iNextMs','false','RATE_BOUND'),
 @('agent && boarding && boarding.GetTokens()','agent','UNSTARTED_BOARDING_SAFE'),
 @('AICF_GroupRuntime.ResolveAliveLeader(group)','group','REAL_LEADER'),
 @('AICF_Stage3Diagnostics.Info("GROUP_TRACE", details);','group.SetOrigin(vector.Zero);','SIDE_EFFECT')
)){
 if($mutation[2] -notin @(Rules ($trace.Replace($mutation[0],$mutation[1])))){throw "Mutation survived: $($mutation[2])"}
 $controls++
}
"PASS Group trace: authority, read-only, bounded scope/rate, real leader, unstarted tokens, lifecycle, fields; controls=$controls"
