param([string]$RepositoryRoot=(Split-Path -Parent (Split-Path -Parent $PSScriptRoot)))
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot '../lib/Stage3StaticAudit.Common.ps1')
$root=Join-Path $RepositoryRoot 'AIConflictCore/Scripts/Game/AIConflict'
$trace=[IO.File]::ReadAllText((Join-Path $root 'Diagnostics/AICF_VehicleCleanupTrace.c'))
$manager=[IO.File]::ReadAllText((Join-Path $root 'Vehicles/AICF_VehicleCleanupManager.c'))
function Body([string]$text,[string]$name){Get-AICFMethodBody ([pscustomobject]@{Source=$text;Code=(ConvertTo-AICFCodeText $text)}) $name}
function Rules([string]$source,[string]$owner){
 $identityMatch=Body $source 'Matches'
 $capture=Body $source 'Capture'
 $fence=Body $source 'MatchesFence'
 $report=Body $source 'Report'
 $actor=Body $source 'ReportActor'
 $retain=Body $owner 'RetainFailClosed'
 $queue=Body $owner 'QueueLeaseRelease'
 if(!$identityMatch -or !$capture -or !$fence -or !$report -or !$actor -or !$retain -or !$queue){'MISSING_METHOD';return}
 if($identityMatch -notmatch 'entity == m_Entity' -or $identityMatch -notmatch 'entity.GetID\(\) == m_Id' -or $identityMatch -notmatch 'm_sRplId != "NONE"' -or $identityMatch -notmatch 'RplId\(entity\) == m_sRplId'){'HISTORY_ACTOR_IDENTITY'}
 if($capture -notmatch '!Replication.IsServer\(\)' -or $capture -notmatch 'trip.GetLease\(\) != lease' -or $capture -notmatch '!lease.MatchesTripIdentity' -or $capture -notmatch 'trip.VehiclePresetMember\(index\), "TRIP_PRESET"' -or $capture -notmatch 'agent.GetControlledEntity\(\), "RELEASE_GROUP"'){'EXPLICIT_HISTORY_SCOPE'}
 if($fence -notmatch 'fence.IsComplete\(\)' -or $fence -notmatch 'fence.GetEntityId\(\) == m_VehicleId' -or $fence -notmatch 'fence.GetRplId\(\) == m_sVehicleRplId' -or $fence -notmatch 'fence.GetLeaseGeneration\(\) == m_iLeaseGeneration' -or $fence -notmatch 'fence.GetGroupGeneration\(\) == m_iGroupGeneration' -or $fence -notmatch 'fence.GetTripGeneration\(\) == m_iTripGeneration'){'HISTORY_LEASE_FENCE'}
 if($report -notmatch '!Replication.IsServer\(\)' -or $report -notmatch '!job.m_Fence.IsComplete\(\)' -or $report -notmatch 'job.m_Fence.GetEntityId\(\) != vehicle.GetID\(\)' -or $report -notmatch 'job.m_Fence.GetRplId\(\) != RplId\(vehicle\)'){'OBSERVED_VEHICLE_IDENTITY'}
 if($report -notmatch 'seat.GetVehicle\(\) == vehicle && seat.GetOccupant\(\)' -or $report -notmatch '"EXACT_SEAT"' -or $report -notmatch '"MANAGED_GROUP"' -or $report -notmatch 'GetAllPlayers\(ids\)' -or $report -notmatch 'GetPlayerControlledEntity\(id\)' -or $report -notmatch 'GetPlayerMainEntity\(id\)'){'ALL_BLOCKER_SOURCES'}
 if($actor -notmatch 'currentManaged = "UNKNOWN"' -or $actor -notmatch 'historical = "UNKNOWN"' -or $actor -notmatch 'history.MatchesFence\(job.m_Fence\)' -or $actor -notmatch 'history.IsGroupCurrent\(job.m_Group\)' -or $actor -notmatch 'parent == job.m_Group && members.Contains\(agent\) && agent.GetControlledEntity\(\) == entity'){'CURRENT_VS_HISTORICAL'}
 if($actor -notmatch 'playerStatus = "UNKNOWN"' -or $actor -notmatch 'GetPlayerIdFromControlledEntity\(entity\)' -or $actor -notmatch 'GetPlayerIdFromMainEntity\(entity\)' -or $actor -notmatch 'controlledPlayer > 0 \|\| mainPlayer > 0' -or $report -notmatch 'PLAYER_MANAGER_UNAVAILABLE|PLAYER_POSITION_UNKNOWN'){'PLAYER_UNKNOWN_AND_POSSESSION'}
 if($actor -notmatch 'int mainPlayer;' -or $actor -notmatch 'if \(players\)\s*\{\s*controlledPlayer = players.GetPlayerIdFromControlledEntity\(entity\);\s*mainPlayer = SCR_PossessingManagerComponent.GetPlayerIdFromMainEntity\(entity\);'){'PLAYER_MANAGER_GUARDS_BOTH_LOOKUPS'}
 foreach($field in @('actor_entity_id','actor_rpl_id','life_state','player_status','current_parent_group','current_managed','historical_assignment','history_capture_ms','actor_rpl_available','seat_owner_vehicle','seat_occupant','exact_seat_occupant','linked_vehicle','getting_in','getting_out','switching_seat','readonly=1')){
  if($source -notmatch [regex]::Escape($field)){'ACTOR_RECEIPT_FIELDS';break}
 }
 if($actor -notmatch 'entity.GetID\(\) != actorId \|\| RplId\(entity\) != actorRpl' -or $actor.IndexOf('entity.GetID() != actorId') -gt $actor.IndexOf('AICF_Stage3Diagnostics.Info')){'LATE_ACTOR_REVALIDATION'}
 if($retain.IndexOf('AICF_VehicleCleanupTrace.Report') -lt $retain.IndexOf('Error("VEHICLE_CLEANUP_RETAINED"') -or $retain -notmatch 'if \(ResolveExpectedVehicle\(job, observedVehicle, observedRplId, observationFailure\)\)\s*AICF_VehicleCleanupTrace.Report'){'AFTER_VERDICT_ONLY'}
 if($queue -notmatch 'm_ActorHistory = AICF_VehicleCleanupActorHistory.Capture\(trip, lease, job.m_Group, nowMs\)' -or $queue.IndexOf('m_ActorHistory =') -gt $queue.IndexOf('m_aJobs.Insert(job)')){'CAPTURE_BEFORE_TRIP_DETACH'}
 if(@([regex]::Matches($owner,'m_ActorHistory')).Count -ne 2){'HISTORY_NOT_A_SAFETY_INPUT'}
 if($source -match '\.Fail\(|\.Complete\(|Teleport|EjectOccupant|GetOutVehicle\(|InterruptVehicleActionQueue|SetOrigin|SpawnEntity|DeleteEntity|SetReserved|CallLater|CommitTransition|BumpMe|AddSupplies|m_Scan\.[A-Za-z_]+\s*='){'READ_ONLY'}
}
$failures=@(Rules $trace $manager)
if($failures.Count){throw "Cleanup trace: $failures"}
$controls=1
foreach($mutation in @(
 @('entity == m_Entity','true','HISTORY_ACTOR_IDENTITY'),
 @('entity.GetID() == m_Id','true','HISTORY_ACTOR_IDENTITY'),
 @('AICF_VehicleCleanupTrace.RplId(entity) == m_sRplId','true','HISTORY_ACTOR_IDENTITY'),
 @('fence.GetLeaseGeneration() == m_iLeaseGeneration','true','HISTORY_LEASE_FENCE'),
 @('job.m_Fence.GetEntityId() != vehicle.GetID()','false','OBSERVED_VEHICLE_IDENTITY'),
 @('seat.GetVehicle() == vehicle && seat.GetOccupant()','seat.GetOccupant()','ALL_BLOCKER_SOURCES'),
 @('parent == job.m_Group && members.Contains(agent) && agent.GetControlledEntity() == entity','true','CURRENT_VS_HISTORICAL'),
 @('string playerStatus = "UNKNOWN"','string playerStatus = "NON_PLAYER"','PLAYER_UNKNOWN_AND_POSSESSION'),
 @('int mainPlayer;','int mainPlayer = SCR_PossessingManagerComponent.GetPlayerIdFromMainEntity(entity);','PLAYER_MANAGER_GUARDS_BOTH_LOOKUPS'),
 @('entity.GetID() != actorId || RplId(entity) != actorRpl','false','LATE_ACTOR_REVALIDATION')
)){
 if(!$trace.Contains($mutation[0])){throw "Mutation target absent: $($mutation[2])"}
 if($mutation[2] -notin @(Rules ($trace.Replace($mutation[0],$mutation[1])) $manager)){throw "Mutation survived: $($mutation[2])"}
 $controls++
}
$actor=Body $trace 'ReportActor'
$mutating=$actor.Insert(1,"`n entity.SetOrigin(vector.Zero);")
if('READ_ONLY' -notin @(Rules ($trace.Replace($actor,$mutating)) $manager)){throw 'Mutation side effect survived'}
$controls++
$unsafeHistory=$manager+"`n if (job.m_ActorHistory) DeleteEntity(vehicle);"
if('HISTORY_NOT_A_SAFETY_INPUT' -notin @(Rules $trace $unsafeHistory)){throw 'History safety input mutation survived'}
$controls++
"PASS Vehicle cleanup trace: exact actor/vehicle/history identities, current vs historical scope, player possession/unknown evidence and readonly integration after retained verdict; controls=$controls"
