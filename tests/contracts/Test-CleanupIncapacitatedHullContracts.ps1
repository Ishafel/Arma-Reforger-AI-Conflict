param([string]$RepositoryRoot=(Split-Path -Parent (Split-Path -Parent $PSScriptRoot)))
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot '../lib/Stage3StaticAudit.Common.ps1')
$core=Join-Path $RepositoryRoot 'AIConflictCore/Scripts/Game/AIConflict'
$owner=[IO.File]::ReadAllText((Join-Path $core 'Vehicles/AICF_VehicleCleanupManager.c'))
$roster=[IO.File]::ReadAllText((Join-Path $core 'State/Vehicles/AICF_VehicleCleanupRecoveryRoster.c'))
$fixture=[IO.File]::ReadAllText((Join-Path $RepositoryRoot 'tests/fixtures/AICF_VehicleBehaviorProbe.c'))
$watchdog=[IO.File]::ReadAllText((Join-Path $core 'Vehicles/AICF_VehicleWatchdog.c'))
function Body([string]$text,[string]$name){Get-AICFMethodBody ([pscustomobject]@{Source=$text;Code=(ConvertTo-AICFCodeText $text)}) $name}
function Rules([string]$o,[string]$r,[string]$p){
 $capture=Body $r 'Capture';$current=Body $r 'IsCurrent';$member=Body $r 'Matches'
 $safe=Body $o 'IsIncapacitatedHullJobSafe';$eligible=Body $o 'IsCurrentDetachedHullMember';$move=Body $o 'TryRelocateIncapacitatedHullMembers';$queue=Body $o 'QueueLeaseRelease'
 if(!$move -or !$current -or !$eligible){'METHODS';return}
 if(!$queue.Contains('m_RecoveryRoster = AICF_VehicleCleanupRecoveryRoster.Capture(trip, lease, job.m_Group)') -or
  $queue.IndexOf('m_RecoveryRoster =') -gt $queue.IndexOf('m_aJobs.Insert(job)') -or
  $capture -notmatch '!Replication.IsServer\(\)|trip.GetLease\(\) != lease' -or
  !$capture.Contains('slot.GetGroup() != group || slot.GetSpawnGeneration() != lease.GetGroupGeneration()') -or
  !$capture.Contains('group.GetAgents(agents);') -or !$capture.Contains('new AICF_VehicleCleanupRecoveryMember(agent, character)')){'CAPTURE_PERMISSION_FROM_CURRENT_GROUP'}
 foreach($guard in @('job.m_Group != m_Group', 'm_Group.GetID() != m_GroupId', 'ComponentRpl(m_Group) != m_sGroupRpl',
  'm_Slot.GetGroup() != m_Group || m_Slot.GetSpawnGeneration() != m_iGroupGeneration',
  'vehicle.GetID() != m_VehicleId || ComponentRpl(vehicle) != m_sVehicleRpl',
  'job.m_Fence.GetGroupGeneration() != m_iGroupGeneration', 'job.m_Fence.GetTripGeneration() != m_iTripGeneration',
  'job.m_Fence.GetLeaseGeneration() != m_iLeaseGeneration', 'job.m_Fence.GetVehicleGeneration() != m_iVehicleGeneration',
  'agent.GetParentGroup() != m_Group', '!current.Contains(agent)', 'member.Matches(agent, character)')){
  if(!$current.Contains($guard)){'CURRENT_GROUP_ACTOR_LEASE_IDENTITY';break}
 }
 foreach($guard in @('agent == m_Agent && character == m_Character','character.GetID() == m_Id','ComponentRpl(character) == m_sRpl',
  'agent.GetControlledEntity() == character', 'character.GetAIControlComponent().GetControlAIAgent() == agent')){
  if(!$member.Contains($guard)){'CAPTURED_BODY_AGENT_AND_RPL';break}
 }
 foreach($guard in @('!IsJobCurrent(job)', '!job.m_RecoveryRoster.IsCurrent(job, vehicle, agent, character)',
  '!m_Watchdog.IsAuthoritativeNonPlayerCharacter(character)', 'currentVehicle != vehicle',
  'physics.GetVelocity().Length() > 0.25', 'controller.IsPlayerControlled()',
  'character.IsInVehicle() || CompartmentAccessComponent.GetVehicleIn(character)',
  'access.IsInCompartment() || access.IsGettingIn() || access.IsGettingOut() || access.IsSwitchingSeatsAnim()')){
  if(!$eligible.Contains($guard)){'ONLY_CURRENT_NONPLAYER_DETACHED_INCAP_HULL';break}
 }
 foreach($guard in @('job.m_bCompleted || job.m_bFailClosed || !IsJobCurrent(job)',
  'nowMs >= job.m_iProtectedClearanceDeadlineAtMs + PROTECTED_CLEARANCE_TERMINAL_GRACE_MS',
  'InspectCleanupSafety(job, vehicle, job.m_Scan)', 'job.m_Scan.m_bPlayerScanAvailable',
  'job.m_Scan.m_iProtectedOccupants == 0', 'job.m_Scan.m_iPlayerTransitions == 0',
  'job.m_Scan.m_iManagedLogicalOccupants == 0', 'job.m_Scan.m_iManagedTransitions == 0',
  '!job.m_Scan.m_sGlobalSamples.Contains("position_unknown")')){
  if(!$safe.Contains($guard)){'FRESH_WHOLE_JOB_FOREIGN_HUMAN_DEADLINE_FENCES';break}
 }
 if(($move+$safe) -match 'CanApplyHiddenRecovery|NearbyPlayers > 0|GetHiddenRecoveryEnabled|m_ActorHistory' -or
  @([regex]::Matches($o,'m_ActorHistory')).Count -ne 2){'VISIBLE_ALLOWED_NO_HISTORY_PERMISSIONS'}
 if(!$move.Contains('job.m_iManagedExactRecoveryAttempts >= MANAGED_EXACT_RECOVERY_MAX_ATTEMPTS') -or
  !$move.Contains('job.m_iManagedExactRecoveryAttempts++;') -or !$move.Contains('job.m_iLastManagedExactRecoveryAtMs = nowMs;') -or
  !$move.Contains('nowMs - job.m_iLastManagedExactRecoveryAtMs < MANAGED_EXACT_RECOVERY_INTERVAL_MS') -or
  $move -match 'm_i\w*(?:Attempts|Deadline)\w*\s*=\s*0|Begin\(|Reset\('){'ORIGINAL_SHARED_FINITE_BUDGET_NO_REARM'}
 if(@([regex]::Matches($move,'IsCurrentDetachedHullMember\(job, vehicle, agent, character\)')).Count -ne 2 -or
  @([regex]::Matches($move,'vector.Distance\(character.GetOrigin\(\), safePosition\) > AICF_VehicleShortRecovery.MAX_ACTOR_DISPLACEMENT_METERS')).Count -ne 2 -or
  !$move.Contains('IsInsideManagedClearanceBounds(vehicle.CoordToLocal(safePosition), minimum, maximum)') -or
  !$move.Contains('ChimeraWorldUtils.TryGetWaterSurfaceSimple(vehicle.GetWorld(), safePosition)')){'REPEATED_SHORT_SAFE_POSITION_AND_BODY_FENCE'}
 if((ConvertTo-AICFCodeText ($move+$eligible+$safe+$r)) -match 'SetHealth|ForceUnconsciousness|SetLifeState|Resurrect|EjectOccupant|GetOutVehicle|vehicle.Teleport|SetOrigin|m_ActorHistory'){'NO_HEALTH_LOGICAL_EXIT_VEHICLE_OR_HISTORY_MUTATION'}
 $native=Body $p 'AICF_VBCleanupIncapUpdate'
 foreach($proof in @('AICF_VBStartCleanup(slot, m_VBFaction)', 'victim.Teleport(transform);', 'damage.ForceUnconsciousness(0);',
  'CLEANUP_INCAP_HULL_GHOST_ESTABLISHED', 'CLEANUP_INCAP_HULL_PHYSICAL_CLEAR', 'CLEANUP_INCAP_LIFE_HEALTH_PRESERVED',
  'm_VBCoordinator.AICF_VBPollCleanup(m_VBFaction, nowMs)', 'attempts > 0 && attempts <= 3',
  'm_VBCoordinator.AICF_VBCleanupDeadline(m_VBCleanupLease) == m_iVBCleanupDeadline',
  'later_native_readback=1', 'CLEANUP_INCAP_FULL_RELEASE', 'query.IsClearanceSafe() && !query.IsRetainedFailClosed()')){
  if(!$native.Contains($proof)){'ACTUAL_CONTROLLED_NATIVE_QUEUE_AND_LATER_PROOF';break}
 }
 if($p -match 'm_iManagedExactRecoveryAttempts\s*(?:\+\+|=)|m_iProtectedClearanceDeadlineAtMs\s*='){'NO_FIXTURE_BUDGET_OR_DEADLINE_PRIMING'}
}
$issues=@(Rules $owner $roster $fixture);if($issues.Count){throw "Incapacitated hull cleanup: $issues"}
$controls=1
foreach($mutation in @(
 @('r','m_Group.GetID() != m_GroupId','false','CURRENT_GROUP_ACTOR_LEASE_IDENTITY'),
 @('r','ComponentRpl(character) == m_sRpl','true','CAPTURED_BODY_AGENT_AND_RPL'),
 @('r','job.m_Fence.GetLeaseGeneration() != m_iLeaseGeneration','false','CURRENT_GROUP_ACTOR_LEASE_IDENTITY'),
 @('o','!m_Watchdog.IsAuthoritativeNonPlayerCharacter(character)','false','ONLY_CURRENT_NONPLAYER_DETACHED_INCAP_HULL'),
 @('o','controller.IsPlayerControlled()','false','ONLY_CURRENT_NONPLAYER_DETACHED_INCAP_HULL'),
 @('o','job.m_Scan.m_iProtectedOccupants == 0','true','FRESH_WHOLE_JOB_FOREIGN_HUMAN_DEADLINE_FENCES'),
 @('o','nowMs >= job.m_iProtectedClearanceDeadlineAtMs + PROTECTED_CLEARANCE_TERMINAL_GRACE_MS','false','FRESH_WHOLE_JOB_FOREIGN_HUMAN_DEADLINE_FENCES'),
 @('o','job.m_iManagedExactRecoveryAttempts++;','job.m_iManagedExactRecoveryAttempts = 0;','ORIGINAL_SHARED_FINITE_BUDGET_NO_REARM'),
 @('o','vector.Distance(character.GetOrigin(), safePosition) > AICF_VehicleShortRecovery.MAX_ACTOR_DISPLACEMENT_METERS','false','REPEATED_SHORT_SAFE_POSITION_AND_BODY_FENCE'),
 @('p','attempts > 0 && attempts <= 3','attempts >= 0','ACTUAL_CONTROLLED_NATIVE_QUEUE_AND_LATER_PROOF')
)){
 $o=$owner;$r=$roster;$p=$fixture
 switch($mutation[0]){'o'{$o=$o.Replace($mutation[1],$mutation[2])}'r'{$r=$r.Replace($mutation[1],$mutation[2])}'p'{$p=$p.Replace($mutation[1],$mutation[2])}}
 if($o -ceq $owner -and $r -ceq $roster -and $p -ceq $fixture){throw "Missing mutation: $($mutation[3])"}
 if($mutation[3] -notin @(Rules $o $r $p)){throw "Mutation survived: $($mutation[3])"};$controls++
}
$body=Body $roster 'IsIncapacitatedPhysicalState'
$adapter='public enum ECharacterLifeState{ALIVE,INCAPACITATED,DEAD} public static class AICFIncapPhysicalState { public static bool Check(ECharacterLifeState life,bool characterInVehicle,bool linked,bool inCompartment,bool gettingIn,bool gettingOut,bool switching,bool inside)'+$body+'}'
Add-Type -TypeDefinition $adapter
$cases=0
foreach($life in 0..2){foreach($mask in 0..127){
 $flags=@(for($bit=0;$bit -lt 7;$bit++){[bool]($mask -band (1 -shl $bit))})
 $actual=[AICFIncapPhysicalState]::Check($life,$flags[0],$flags[1],$flags[2],$flags[3],$flags[4],$flags[5],$flags[6])
 $expected=$life -eq 1 -and $mask -eq 64
 if($actual -ne $expected){throw "Physical state predicate life=$life mask=$mask expected=$expected actual=$actual"};$cases++
}}
$player=Body $watchdog 'IsAuthoritativeNonPlayerCharacter'
if(!$player.Contains('playerManager.GetPlayerIdFromControlledEntity(entity) <= 0') -or !$player.Contains('SCR_PossessingManagerComponent.GetPlayerIdFromMainEntity(entity) <= 0')){throw 'Existing direct/main possession protection changed'}
"PASS Incapacitated hull cleanup: current permission roster independent of history, exact group/actor/lease fences, physical-only conscious-state matrix, existing3 budget/deadline, visible allowed with human/foreign veto and <=25m safe terrain; source_controls=$controls actual_predicate_cases=$cases native_fixture=REQUIRES_SEPARATE_RUNTIME"
