param([string]$RepositoryRoot=(Split-Path -Parent (Split-Path -Parent $PSScriptRoot)))
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot '../lib/Stage3StaticAudit.Common.ps1')
$core=Join-Path $RepositoryRoot 'AIConflictCore/Scripts/Game/AIConflict'
function Read([string]$path){[IO.File]::ReadAllText((Join-Path $core $path))}
function Body([string]$text,[string]$method){Get-AICFMethodBody ([pscustomobject]@{Source=$text;Code=(ConvertTo-AICFCodeText $text)}) $method}
$identity=Read 'State/Vehicles/AICF_TripRetirementIdentity.c'
$trip=Read 'State/Vehicles/AICF_TransportTrip.c'
$owner=Read 'Vehicles/AICF_TransportTripController.c'
$handoff=Read 'Vehicles/AICF_VehicleTaskHandoff.c'
$cleanup=Read 'Vehicles/AICF_VehicleCleanupManager.c'
$watchdog=Read 'Vehicles/AICF_VehicleWatchdog.c'
function Rules([string]$Identity,[string]$Trip,[string]$Owner,[string]$Handoff,[string]$Cleanup){
 $capture=Body $Identity 'AICF_TripRetirementIdentity';$absent=Body $Identity 'MatchesAbsentGroup';$asset=Body $Identity 'IsFleetAssetCurrent'
 foreach($guard in @('!Replication.IsServer()','!assignment.IsValid()','IsAuthoritativeReplicatedEntity(group)','!rpl.Id().IsValid()','m_GroupId = group.GetID();','m_GroupRpl = rpl.Id();','m_sOperation = operation;')){if(!$capture.Contains($guard)){'CAPTURED_AUTHORITATIVE_GROUP_IDENTITY';break}}
 foreach($guard in @('!m_bCaptured','!trip.GetAssignment()','trip.GetAssignment().GetGroup()','GetGame().GetWorld().FindEntityByID(m_GroupId)','Replication.FindItem(m_GroupRpl)','trip.MatchesIdentity(m_sFaction, m_iSlot, m_iGroupGeneration, m_iTripGeneration)','trip.GetOperationId() == m_sOperation')){if(!$absent.Contains($guard)){'EXACT_ABSENT_GROUP_IDENTITY';break}}
 if($absent.Contains('GetVehiclePresetSlot')){'NO_REPLACEMENT_GROUP_MUTATION'}
 foreach($guard in @('fleet.GetFactionKey() != trip.GetFactionKey()','lease.MatchesTripIdentity','fleet.FindLeaseForSlot(trip.GetSlotId(), trip.GetGroupGeneration()) != lease','IsAuthoritativeReplicatedEntity(vehicle)','GetGame().GetWorld().FindEntityByID(lease.GetEntityId()) != vehicle','lease.MatchesEntityIdentity(vehicle, vehicle.GetID(), rpl.Id().ToString())')){if(!$asset.Contains($guard)){'FLEET_AND_ACTUAL_ASSET_FENCE';break}}
 if((Body $Trip 'IsValid') -notmatch 'm_Assignment.IsValid\(\)' -or (Body $Trip 'IsValid').Contains('Retirement')){'LIVE_VALIDITY_NOT_WEAKENED'}
 foreach($method in @('CommitTransition','IsTransitionAllowedTo')){if(!(Body $Trip $method).Contains('nextPhase == AICF_ETransportTripPhase.FAILED_CLOSED && HasAbsentGroupRetirementIdentity()')){'ONLY_FAILED_CLOSED_TRANSITION';break}}
 $transition=Body $Owner 'TransitionTo';$exit=$transition.IndexOf('ExitPhaseEffects');$fences=@([regex]::Matches($transition,'AICF_TripRetirementIdentity.IsFleetAssetCurrent\(trip, retirementFleet\)'))
 if($fences.Count -ne 2 -or $fences[0].Index -gt $exit -or $fences[1].Index -lt $exit -or $fences[1].Index -gt $transition.IndexOf('trip.CommitTransition')){'REPEAT_NATIVE_FENCE_AROUND_EXIT_EFFECTS'}
 $owned=Body $Handoff 'IsExactTripOwnedVehicleWaypoint'
 foreach($guard in @('trip.HasAbsentGroupRetirementIdentity()','trip.GetLease().MatchesTripIdentity','waypoint == movement.GetRouteWaypoint()','waypoint == movement.GetSupersededRouteWaypoint()','waypoint == dismount.GetDismountWaypoint()')){if(!$owned.Contains($guard)){'EXACT_PHASE_POINTER_ONLY';break}}
 $queue=Body $Cleanup 'QueueLeaseRelease'
 foreach($guard in @('!trip.IsValidForRetirement()','trip.GetLease() != lease','trip.GetPhase() != AICF_ETransportTripPhase.FAILED_CLOSED','AICF_TripRetirementIdentity.IsFleetAssetCurrent(trip, fleet)','ProcessLeaseRelease(job, nowMs)')){if(!$queue.Contains($guard)){'ORIGINAL_CLEANUP_OWNER_FULL_SCAN';break}}
 if((Body $Cleanup 'ProcessLeaseRelease') -notmatch 'm_Scan.m_bSafe' -or $queue -match 'DeleteRplEntity|\.ReleaseLease\('){'NO_OCCUPANCY_OR_CLEARANCE_BYPASS'}
 $safety=Body $Cleanup 'InspectCleanupSafety';$release=Body $Cleanup 'ProcessLeaseRelease'
 if(!$safety.Contains('bool globalClear = m_Watchdog.InspectProtectedCleanupUse(') -or !$safety.Contains('scan.m_bSafe = scan.m_bPlayerScanAvailable && globalClear && managedClear;') -or
  ([regex]::Matches($release,'InspectCleanupSafety\(job, vehicle, job.m_Scan\)')).Count -lt 3 -or !$release.Contains('PROTECTED_CLEARANCE_RESCAN_BLOCKED')){'HUMAN_FOREIGN_UNKNOWN_FULL_RESCAN'}
 $dead=(Body $Identity 'ResolveCapturedDeadMember')+(Body $Identity 'ResolveDeadCapturedIdentity');$eject=Body $Cleanup 'TryDetachCapturedDeadOrphanMembers'
 foreach($guard in @('controller.GetLifeState() != ECharacterLifeState.DEAD','actorRpl.Id().ToString() != member.m_sRpl','faction.GetFactionKey() != m_sFaction','character.GetID() != member.m_Id','GetGame().GetWorld().FindEntityByID(member.m_Id) != character','job.m_Lease != m_CleanupLease','m_CleanupFleet.FindLeaseForSlot(m_iSlot, m_iGroupGeneration) != m_CleanupLease','agent.GetParentGroup() != m_CapturedGroup')){if(!$dead.Contains($guard)){'CAPTURED_DEAD_ONLY_ORPHAN_MEMBER';break}}
 foreach($boundary in @('ResolveCapturedDeadMember\(','IsAuthoritativeNonPlayerCharacter\(','ResolveExpectedVehicle\(','CanApplyHiddenRecovery\(')){
  if(([regex]::Matches($eject,$boundary)).Count -ne 2){'REPEATED_DEAD_SEAT_PLAYER_CAR_BOUNDARIES';break}
 }
 if(!$eject.Contains('seat.GetVehicle() != vehicle || seat.GetOccupant() != character') -or !$eject.Contains('access.KickFromVehicle(doorIndex)') -or !$eject.Contains('seat.PickDoorIndexForPoint(character.GetOrigin())') -or
  !$eject.Contains('job.m_iNonAliveDetachAttempts >= NON_ALIVE_DETACH_MAX_ATTEMPTS') -or !$eject.Contains('job.m_iNonAliveDetachAttempts++;') -or $eject -match 'SetOrigin|Teleport|SetHealth|\.Kill\(|DeleteRplEntity|m_ActorHistory'){'ONLY_BOUNDED_NATIVE_EXACT_DEAD_EJECT'}
 $memberCapture=Body $Identity 'CaptureMember'
 foreach($part in @('group.GetAgents(agents)','agent.GetParentGroup() == group')){if(!$capture.Contains($part)){'NONPRESET_ADMISSION_ROSTER_CAPTURE';break}}
 foreach($part in @('!AICF_VehicleBoardingMutationFence.IsAuthoritativeAIEntity(character)','!rpl.Id().IsValid()','faction.GetFactionKey() != m_sFaction','member.m_Character = character;','member.m_Id = character.GetID();','member.m_sRpl = rpl.Id().ToString();','m_bRosterTrusted = false;')){if(!$memberCapture.Contains($part)){'NONPRESET_ADMISSION_ROSTER_CAPTURE';break}}
 if((ConvertTo-AICFCodeText $Identity) -match '\.(SetHealth|Kill|SetOrigin|DeleteEntity|RequestSpawn|MarkReady)\s*\('){'READONLY_IDENTITY_HELPER'}
}
$issues=@(Rules $identity $trip $owner $handoff $cleanup);if($issues.Count){throw "Orphan source rules: $issues"}
$mutants=@(
 @('i','!assignment.IsValid()','false'),@('i','m_GroupId = group.GetID();','m_GroupId = EntityID.INVALID;'),
 @('i','!m_bCaptured','false'),@('i','trip.GetAssignment().GetGroup()','false'),
 @('i','GetGame().GetWorld().FindEntityByID(m_GroupId)','false'),@('i','Replication.FindItem(m_GroupRpl)','false'),
 @('i','trip.GetOperationId() == m_sOperation','true'),@('i','fleet.FindLeaseForSlot(trip.GetSlotId(), trip.GetGroupGeneration()) != lease','false'),
 @('i','lease.MatchesEntityIdentity(vehicle, vehicle.GetID(), rpl.Id().ToString())','true'),
 @('t','nextPhase == AICF_ETransportTripPhase.FAILED_CLOSED && HasAbsentGroupRetirementIdentity()','HasAbsentGroupRetirementIdentity()'),
 @('o','!AICF_TripRetirementIdentity.IsFleetAssetCurrent(trip, retirementFleet)','false'),
 @('h','waypoint == movement.GetRouteWaypoint()','true'),
 @('c','trip.GetPhase() != AICF_ETransportTripPhase.FAILED_CLOSED','false'),
 @('c','return ProcessLeaseRelease(job, nowMs);','return null;'),
 @('c','scan.m_bPlayerScanAvailable && globalClear && managedClear','managedClear'),
 @('c','InspectCleanupSafety(job, vehicle, job.m_Scan);','SkipScan();'),
 @('i','controller.GetLifeState() != ECharacterLifeState.DEAD','false'),
 @('i','actorRpl.Id().ToString() != member.m_sRpl','false'),
 @('i','agent.GetParentGroup() != m_CapturedGroup','false'),
 @('i','character.GetID() != member.m_Id','false'),
 @('i','faction.GetFactionKey() != m_sFaction','false'),
 @('c','m_Watchdog.IsAuthoritativeNonPlayerCharacter(character)','true'),
 @('c','seat.GetVehicle() != vehicle || seat.GetOccupant() != character','false'),
 @('c','access.KickFromVehicle(doorIndex)','character.Teleport(transform)'),
 @('c','job.m_iNonAliveDetachAttempts++;',''),
 @('i','!AICF_VehicleBoardingMutationFence.IsAuthoritativeAIEntity(character)','false'),
 @('i','member.m_sRpl = rpl.Id().ToString();','member.m_sRpl = "fabricated";')
)
foreach($m in $mutants){
 $i=$identity;$t=$trip;$o=$owner;$h=$handoff;$c=$cleanup
 switch($m[0]){i{$i=$i.Replace($m[1],$m[2])}t{$t=$t.Replace($m[1],$m[2])}o{$o=$o.Replace($m[1],$m[2])}h{$h=$h.Replace($m[1],$m[2])}c{$c=$c.Replace($m[1],$m[2])}}
 if(!(@(Rules $i $t $o $h $c).Count)){throw "Orphan source mutation survived: $($m[1])"}
}
$body=Body $identity 'MatchesAbsentGroup'
$body=$body.Replace('!trip ||','trip == null ||').Replace('!trip.GetAssignment()','trip.GetAssignment() == null').Replace('!GetGame().GetWorld()','GetGame().GetWorld() == null').Replace('!GetGame()','GetGame() == null').Replace('trip.GetAssignment().GetGroup() ||','trip.GetAssignment().GetGroup() != null ||').Replace('GetGame().GetWorld().FindEntityByID(m_GroupId) ||','GetGame().GetWorld().FindEntityByID(m_GroupId) != null ||').Replace('Replication.FindItem(m_GroupRpl))','Replication.FindItem(m_GroupRpl) != null)')
$body=$body.Replace('!m_sOperation.IsEmpty()','!string.IsNullOrEmpty(m_sOperation)')
$code=@'
using System;
public class EntityID { public const long INVALID=-1; }
public struct RplId { public int Value; public bool IsValid(){return Value>0;} }
public class World { public object Present; public object FindEntityByID(long id){return Present;} }
public class Game { public World World=new World(); public World GetWorld(){return World;} }
public static class Replication { public static bool Server=true; public static object Item; public static bool IsServer(){return Server;} public static object FindItem(RplId id){return Item;} }
public class Assignment { public object Group; public object GetGroup(){return Group;} }
public class AICF_TransportTrip { public Assignment Assignment=new Assignment(); public bool Identity=true; public string Operation="original-operation"; public Assignment GetAssignment(){return Assignment;} public bool MatchesIdentity(string faction,int slot,int generation,int trip){return Identity;} public string GetOperationId(){return Operation;} }
public class AICF_ExtractedRetirement { public bool m_bCaptured=true; public long m_GroupId=10; public RplId m_GroupRpl=new RplId{Value=20}; public string m_sFaction="USSR",m_sOperation="original-operation"; public int m_iSlot=0,m_iGroupGeneration=1,m_iTripGeneration=1; public Game Game=new Game(); public Game GetGame(){return Game;} public bool MatchesAbsentGroup(AICF_TransportTrip trip) BODY }
'@
Add-Type -TypeDefinition ($code.Replace('BODY',$body))
foreach($name in @('positive','not-server','uncaptured','null-trip','null-assignment','live-group','world-id-still-present','rpl-still-present','wrong-generation-or-faction-or-slot','wrong-operation','missing-native-world','invalid-captured-id','invalid-captured-rpl')){
 $proof=[AICF_ExtractedRetirement]::new();$ctx=[AICF_TransportTrip]::new();[Replication]::Server=$true;[Replication]::Item=$null
 switch($name){'not-server'{[Replication]::Server=$false}'uncaptured'{$proof.m_bCaptured=$false}'null-trip'{$ctx=$null}'null-assignment'{$ctx.Assignment=$null}'live-group'{$ctx.Assignment.Group=[object]::new()}'world-id-still-present'{$proof.Game.World.Present=[object]::new()}'rpl-still-present'{[Replication]::Item=[object]::new()}'wrong-generation-or-faction-or-slot'{$ctx.Identity=$false}'wrong-operation'{$ctx.Operation='foreign-operation'}'missing-native-world'{$proof.Game.World=$null}'invalid-captured-id'{$proof.m_GroupId=-1}'invalid-captured-rpl'{$proof.m_GroupRpl=[RplId]::new()}}
 $result=$proof.MatchesAbsentGroup($ctx)
 if($result -ne ($name -eq 'positive')){throw "Extracted absent-group predicate failed: $name"}
}
$nativeSafety=Body $watchdog 'InspectProtectedCleanupUse'
$nativeReturn=[regex]::Match($nativeSafety,'return !playerPositionUnknown && protectedOccupantCount == 0 && linkedPlayerCount == 0 &&\s*playerTransitionCount == 0 && nearbyPlayerCount == 0;').Value
if(!$nativeReturn){throw 'Missing actual global human/foreign/unknown occupancy predicate'}
Add-Type -TypeDefinition ('public static class AICF_OrphanGlobalSafety { public static bool Clear(bool playerPositionUnknown,int protectedOccupantCount,int linkedPlayerCount,int playerTransitionCount,int nearbyPlayerCount) {'+$nativeReturn+'} }')
foreach($mask in 0..31){
 $unknown=($mask -band 1) -ne 0;$foreign=[int](($mask -band 2) -ne 0);$human=[int](($mask -band 4) -ne 0);$transition=[int](($mask -band 8) -ne 0);$nearby=[int](($mask -band 16) -ne 0)
 if([AICF_OrphanGlobalSafety]::Clear($unknown,$foreign,$human,$transition,$nearby) -ne ($mask -eq 0)){throw "Orphan null-group global protection bypassed: $mask"}
}
$dead=Body $identity 'ResolveDeadCapturedIdentity'
$deadGuard=[regex]::Match($dead,'if \(!controller \|\| controller.GetLifeState\(\) != ECharacterLifeState.DEAD \|\| !actorRpl \|\| actorRpl.Id\(\).ToString\(\) != member.m_sRpl \|\|\s*!faction \|\| faction.GetFactionKey\(\) != m_sFaction\) return null;').Value
if(!$deadGuard){throw 'Missing actual DEAD/component Rpl/faction native commit guard'}
$deadGuard=$deadGuard.Replace('!controller ||','controller == null ||').Replace('!actorRpl ||','actorRpl == null ||').Replace('!faction ||','faction == null ||').Replace('ECharacterLifeState.DEAD','2').Replace('return null;','return false;')
Add-Type -TypeDefinition ('public class AICF_OrphanDeadController {public int Life; public int GetLifeState(){return Life;}} public class AICF_OrphanActorRpl {public string Value; public string Id(){return Value;}} public class AICF_OrphanFaction {public string Value; public string GetFactionKey(){return Value;}} public class AICF_OrphanMember {public string m_sRpl="captured";} public static class AICF_OrphanDeadGuard {public static bool Check(AICF_OrphanDeadController controller,AICF_OrphanActorRpl actorRpl,AICF_OrphanFaction faction,AICF_OrphanMember member,string m_sFaction) {'+$deadGuard+' return true;} }')
foreach($life in 0..2){foreach($mask in 0..31){
 $ctrl=[AICF_OrphanDeadController]::new();$ctrl.Life=$life;if($mask -band 1){$ctrl=$null}
 $rpl=[AICF_OrphanActorRpl]::new();$rpl.Value='captured';if($mask -band 2){$rpl=$null}elseif($mask -band 4){$rpl.Value='foreign'}
 $faction=[AICF_OrphanFaction]::new();$faction.Value='USSR';if($mask -band 8){$faction=$null}elseif($mask -band 16){$faction.Value='US'}
 if([AICF_OrphanDeadGuard]::Check($ctrl,$rpl,$faction,[AICF_OrphanMember]::new(),'USSR') -ne ($life -eq 2 -and $mask -eq 0)){throw "Actual DEAD-only identity guard failed life=$life mask=$mask"}
}}
$nonplayer=Body $watchdog 'IsAuthoritativeNonPlayerCharacter'
$playerReturn=[regex]::Match($nonplayer,'return playerManager && playerManager.GetPlayerIdFromControlledEntity\(entity\) <= 0 &&\s*SCR_PossessingManagerComponent.GetPlayerIdFromMainEntity\(entity\) <= 0;').Value
if(!$playerReturn){throw 'Missing actual controlled/main player ownership fence'}
$playerReturn=$playerReturn.Replace('playerManager &&','playerManager != null &&').Replace('SCR_PossessingManagerComponent.GetPlayerIdFromMainEntity(entity)','mainPlayer')
Add-Type -TypeDefinition ('public class AICF_OrphanPlayerManager {public int Controlled; public int GetPlayerIdFromControlledEntity(object entity){return Controlled;}} public static class AICF_OrphanNonplayerGuard {public static bool Check(AICF_OrphanPlayerManager playerManager,object entity,int mainPlayer){'+$playerReturn+'}}')
foreach($present in @($false,$true)){foreach($controlled in @(-1,0,1)){foreach($main in @(-1,0,1)){
 $manager=$null;if($present){$manager=[AICF_OrphanPlayerManager]::new();$manager.Controlled=$controlled}
 if([AICF_OrphanNonplayerGuard]::Check($manager,[object]::new(),$main) -ne ($present -and $controlled -le 0 -and $main -le 0)){throw 'Actual PlayerManager/main ownership veto failed'}
}}}
"PASS Orphan retirement: source_mutants=$($mutants.Count) absent_identity_cases=13 global_human_foreign_unknown_cases=32 actual_dead_rpl_faction_cases=96 actual_player_owned_cases=18; no live/INCAP/foreign/corpse-delete or global scan override"
