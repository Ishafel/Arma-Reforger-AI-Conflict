param([string]$RepositoryRoot=(Split-Path -Parent (Split-Path -Parent $PSScriptRoot)),[string]$Mutation='')
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot '../lib/Stage3StaticAudit.Common.ps1')
$root=Join-Path $RepositoryRoot 'AIConflictCore/Scripts/Game/AIConflict'
function Read($p){[IO.File]::ReadAllText((Join-Path $root $p))}
function Body($s,$n){Get-AICFMethodBody ([pscustomobject]@{Source=$s;Code=(ConvertTo-AICFCodeText $s)}) $n}
$identity=Read 'State/Vehicles/AICF_TripRetirementIdentity.c'
$capability=Read 'State/Vehicles/AICF_CapturedDeadCleanupIdentity.c'
$manager=Read 'Vehicles/AICF_VehicleCleanupManager.c'
$trip=Read 'State/Vehicles/AICF_TransportTrip.c'
$handler=Body $manager 'TryDetachCapturedTerminalDeadMembers'
function Rules($i,$c,$h,$m,$t){
 $commit=Body $i 'ResolveTerminalDeadActorForCommit';$ownerCommit=Body $c 'ResolveForCommit'
 if($commit.IndexOf('expectedSeat.GetOccupant()') -gt $commit.IndexOf('CharacterControllerComponent controller = expectedActor.GetCharacterController()') -or
  $commit.LastIndexOf('GetWorld()') -gt $commit.LastIndexOf('controller.IsPlayerControlled()') -or
  $commit.IndexOf('ResolveTerminalDeadActor(index)') -gt $commit.IndexOf('expectedAccess.GetCompartment()')){'FINITE_BODY_SEAT_FINAL_HUMAN_ORDER'}
 $afterHuman=$h.Substring($h.IndexOf('identity.ResolveForCommit('),$h.IndexOf('access.KickFromVehicle')-$h.IndexOf('identity.ResolveForCommit('))
 if($afterHuman -match 'GetParentGroup|GetCompartment|GetVehicle\(|GetOccupant|FindComponent|GetCharacterController|GetWorld\(' -or
  !$ownerCommit.Contains('m_Admission.ResolveTerminalDeadActorForCommit(') -or !$commit.Contains('GetGame().GetWorld() != m_AdmissionWorld')){'NO_NATIVE_SEAT_PARENT_RPL_AFTER_FINAL_ACTOR_PROOF'}
 if(!(Body $i 'CaptureMember').Contains('member.m_World = character.GetWorld();')){'CAPTURE_ACTUAL_ADMISSION_ACTOR_WORLD'}
 foreach($s in @('character.GetWorld() != m_AdmissionWorld','character.FindComponent(RplComponent) != member.m_Rpl','character.GetCharacterController() != member.m_Controller','member.m_Controller.IsPlayerControlled()','playerManager.GetAllPlayers(playerIds)','playerController.GetControlledEntity() == character','!member.m_bAdmissionAlive')){if(!$i.Contains($s)){'ADMISSION_ACTOR_COMPONENT_WORLD_ALL_PLAYER';break}}
 foreach($s in @('trip.GetAssignment() != m_AdmissionAssignment','!trip.OwnsRetirementIdentity(this)','slot.GetSpawnGeneration() == m_iGroupGeneration')){if(!$i.Contains($s)){'EXACT_ADMISSION_TRIP_SLOT';break}}
 foreach($s in @('m_Fence.MatchesFence(job.m_Fence)','m_Fence.MatchesLease(m_Lease)','job.m_Fence != m_OriginalFence','m_Trip.GetTransitionCount() != m_iTransition','m_Assignment.GetAssignmentRevision() != m_iAssignmentRevision','m_Assignment.GetStrategicIntentRevision() != m_iIntentRevision','m_Assignment.GetBaseRevision() != m_iBaseRevision','vehicle.FindComponent(RplComponent) != m_VehicleRpl','m_World.FindEntityByID(m_Fence.GetEntityId()) != vehicle','!m_Lease.IsCapActive()','m_Fleet.FindLeaseForSlot')){if(!$c.Contains($s)){'EXACT_JOB_FLEET_LEASE_GENERATIONS_REVISIONS_WORLD';break}}
 if(([regex]::Matches($h,'identity.Resolve\(job, vehicle, index\)')).Count -lt 3 -or !$h.Contains('seat.GetOccupant() != character') -or !$h.Contains('character.GetCompartmentAccessComponent() != access') -or !$h.Contains('job.m_iNonAliveDetachAttempts++') -or $h.IndexOf('job.m_iNonAliveDetachAttempts++') -gt $h.IndexOf('access.KickFromVehicle(doorIndex)') -or !$h.Contains('!identity.IsCurrent(job, vehicle)')){'REPEAT_NATIVE_SEAT_CONTEXT_AND_SHARED_BUDGET'}
 if(!$m.Contains('NON_ALIVE_DETACH_MAX_ATTEMPTS = 3;') -or !$m.Contains('NON_ALIVE_DETACH_INTERVAL_MS = 1000;') -or ([regex]::Matches($m,'if \(!TryDetachCapturedTerminalDeadMembers\(job, vehicle, nowMs\)\)')).Count -ne 2){'EXISTING_BUDGET_BOTH_CLEANUP_ENTRIES'}
 if(!(Body $i 'ResolveCapturedDeadMember').Contains('job.m_Group ||') -or !(Body $i 'CaptureCleanup').Contains('AICF_ETransportTripPhase.FAILED_CLOSED')){'ORPHAN_SCOPE_UNCHANGED'}
 if((ConvertTo-AICFCodeText ($c+$h)) -match 'm_ActorHistory|SetHealth|SetLifeState|Teleport\(|SetOrigin\(|DeleteEntity|EjectOccupant|ReleaseLease\('){'CAPABILITY_PHYSICAL_DETACH_ONLY'}
 if(!(Body $t 'CaptureCapturedDeadCleanupIdentity').Contains('AICF_CapturedDeadCleanupIdentity.Capture(m_RetirementIdentity, this, job)')){'TRIP_DELEGATES_ONLY'}
}
$bad=@(Rules $identity $capability $handler $manager $trip);if($bad.Count){throw "Captured casualty source: $bad"}
$mutations=@(
 @('i','character.GetWorld() != m_AdmissionWorld','false'),@('i','character.FindComponent(RplComponent) != member.m_Rpl','false'),
 @('i','member.m_Controller.IsPlayerControlled()','false'),@('i','playerController.GetControlledEntity() == character','false'),
 @('i','!member.m_bAdmissionAlive','false'),@('i','!trip.OwnsRetirementIdentity(this)','false'),
 @('c','m_Trip.GetTransitionCount() != m_iTransition','false'),@('c','m_Assignment.GetAssignmentRevision() != m_iAssignmentRevision','false'),
 @('c','!m_Lease.IsCapActive()','false'),@('c','vehicle.FindComponent(RplComponent) != m_VehicleRpl','false'),
 @('h','job.m_iNonAliveDetachAttempts++;',''),@('h','!identity.IsCurrent(job, vehicle)','false'),
 @('m','NON_ALIVE_DETACH_MAX_ATTEMPTS = 3;','NON_ALIVE_DETACH_MAX_ATTEMPTS = 4;'),
 @('c','m_Admission.ResolveTerminalDeadActorForCommit(','m_Admission.ResolveTerminalDeadActor('),
 @('i','GetGame().GetWorld() != m_AdmissionWorld','false')
)
foreach($v in $mutations){$i=$identity;$c=$capability;$h=$handler;$m=$manager;switch($v[0]){i{$i=$i.Replace($v[1],$v[2])}c{$c=$c.Replace($v[1],$v[2])}h{$h=$h.Replace($v[1],$v[2])}m{$m=$m.Replace($v[1],$v[2])}};if(!@(Rules $i $c $h $m $trip).Count){throw "Escaped source mutation $($v[1])"}}
# Actual production method bodies run against explicit native-boundary shims.
# This is not Enfusion compile, physical detach, or a native release receipt.
function Translate($s){
 $s=$s -replace '\bprotected\b','public' -replace '\bref\s+','' -replace '\bFactionKey\b','string' -replace '\bEntityID\b','int'
 $s=$s -replace 'FindComponent\((RplComponent|FactionAffiliationComponent|AIControlComponent)\)','FindComponent(typeof($1))'
 $s=$s -replace 'foreach \(([^:]+) : ([^)]+)\)','foreach ($1 in $2)'
 $s=$s -replace 'array<int> playerIds = \{\};','EArray<int> playerIds = new EArray<int>();'
 $s=$s -replace 'for \(int index;','for (int index = 0;'
 $s=$s -replace '\b(float|string|BaseCompartmentSlot|AIAgent|Faction) (nearestPlayer|playerFence|seat|agent|faction);','$1 $2 = default($1);'
 $s=$s -replace 'CanApplyHiddenRecovery\(([^;]+), nearestPlayer, playerFence\)','CanApplyHiddenRecovery($1, out nearestPlayer, out playerFence)'
 return $s
}
$cap=Translate $capability
$cap=$cap.Replace('class AICF_CapturedDeadCleanupIdentity','public class AICF_CapturedDeadCleanupIdentity : Native')
$cap=$cap -replace '(?m)^\t(static |bool |int |ChimeraCharacter )',"`tpublic `$1"
$commitActor=Body $identity 'ResolveTerminalDeadActorForCommit'
if($Mutation -eq 'skip-final-human-pc'){$commitActor=[regex]::Replace($commitActor,'foreach \(int playerId : playerIds\)\s*\{[\s\S]+?\}', 'foreach (int playerId : playerIds) {}')}
if($Mutation -eq 'skip-final-actor'){$handler=$handler.Replace('ChimeraCharacter commitActor = identity.ResolveForCommit(job, vehicle, index, character, access, seat);','ChimeraCharacter commitActor = character;')}
$admissionMethods='public bool MatchesLiveCapturedGroup(AICF_TransportTrip trip)'+(Body $identity 'MatchesLiveCapturedGroup')+
 'public bool MatchesTerminalCleanupAdmission(AICF_TransportTrip trip)'+(Body $identity 'MatchesTerminalCleanupAdmission')+
 'public ChimeraCharacter ResolveDeadCapturedIdentity(int index)'+(Body $identity 'ResolveDeadCapturedIdentity')+
 'public ChimeraCharacter ResolveTerminalDeadActor(int index)'+(Body $identity 'ResolveTerminalDeadActor')+
 'public ChimeraCharacter ResolveTerminalDeadActorForCommit(int index, ChimeraCharacter expectedActor, Vehicle vehicle, CompartmentAccessComponent expectedAccess, BaseCompartmentSlot expectedSeat)'+$commitActor
$admissionMethods=Translate $admissionMethods
$handler=Translate $handler
$oldOrphan=Body $manager 'TryDetachCapturedDeadOrphanMembers'
$oldGuard=[regex]::Match($oldOrphan,'if \(!IsAuthority\(\)[\s\S]+?\) return;').Value.Replace('return;','return false;')
if(!$oldGuard){throw 'Missing exact old orphan first guard'}
$oldRoster=Body $manager 'TryDetachNonAliveExactOccupants'
$rosterStart=$oldRoster.IndexOf('array<AIAgent> agents = {};');$rosterEnd=$oldRoster.IndexOf('if (candidates <= 0)')
if($rosterStart -lt 0 -or $rosterEnd -le $rosterStart){throw 'Missing old exact current-group candidate census'}
$oldRoster=$oldRoster.Substring($rosterStart,$rosterEnd-$rosterStart)
$oldRoster=(Translate $oldRoster).Replace('array<AIAgent> agents = {};','EArray<AIAgent> agents = new EArray<AIAgent>();').Replace('int candidates;','int candidates=0;')
$code=@'
using System;
using System.Collections.Generic;
public class Native { public bool Removed;public static implicit operator bool(Native n){return n != null && !n.Removed;} public static Game GetGame(){return Env.Game;} }
public static class Text {public static bool IsEmpty(this string s){return string.IsNullOrEmpty(s);}}
public class EArray<T>:List<T>{public new int Count(){return base.Count;}public bool IsEmpty(){return Count()==0;}public bool IsIndexValid(int i){return i>=0&&i<Count();}}
public static class Replication{public static bool Server=true;public static bool IsServer(){return Server;}}
public enum ECharacterLifeState{ALIVE,INCAPACITATED,DEAD}
public enum AICF_EVehicleLeaseState{ACTIVE,FAILED_CLOSED,RELEASED}
public class BaseWorld:Native{public Dictionary<int,IEntity> Map=new Dictionary<int,IEntity>();public IEntity FindEntityByID(int id){return Map.ContainsKey(id)?Map[id]:null;}}
public class Game:Native{public BaseWorld World=new BaseWorld();public PlayerManager PM=new PlayerManager();public BaseWorld GetWorld(){return World;}public PlayerManager GetPlayerManager(){return PM;}}
public class RplComponent:Native{public int Value=7;public Action Read;public int Id(){var r=Read;Read=null;if(r != null)r.Invoke();return Value;}public static RplComponent Cast(object x){return x as RplComponent;}}
public class Faction:Native{public string GetFactionKey(){return "USSR";}}
public class FactionAffiliationComponent:Native{public Faction GetAffiliatedFaction(){return new Faction();}public static FactionAffiliationComponent Cast(object x){return x as FactionAffiliationComponent;}}
public class CharacterControllerComponent:Native{public ECharacterLifeState Life=ECharacterLifeState.DEAD;public bool Player;public ECharacterLifeState GetLifeState(){return Life;}public bool IsPlayerControlled(){return Player;}}
public class AIAgent:Native{public SCR_AIGroup Parent;public Action ParentRead;public int ParentReads,ParentReadAt=1;public BaseCompartmentSlot AssertSeat;public IEntity GetControlledEntity(){return null;}public SCR_AIGroup GetParentGroup(){if(AssertSeat&&AssertSeat.OccupantReads>=4)throw new System.Exception("Parent read after final seat proof");ParentReads++;if(ParentReads==ParentReadAt){var r=ParentRead;ParentRead=null;if(r!=null)r.Invoke();}return Parent;}}
public class AIControlComponent:Native{public AIAgent Agent=new AIAgent();public AIAgent GetAIAgent(){return Agent;}public static AIControlComponent Cast(object x){return x as AIControlComponent;}}
public class IEntity:Native{public int Id;public bool Master=true;public BaseWorld World;public RplComponent Rpl=new RplComponent();public int GetID(){return Id;}public BaseWorld GetWorld(){return World;}public virtual object FindComponent(Type t){return Rpl;}public int GetOrigin(){return 0;}}
public class SCR_AIGroup:IEntity{}
public class Vehicle:IEntity{}
public class ChimeraCharacter:IEntity{public CharacterControllerComponent Controller=new CharacterControllerComponent();public AIControlComponent AI=new AIControlComponent();public FactionAffiliationComponent Faction=new FactionAffiliationComponent();public CompartmentAccessComponent Access=new CompartmentAccessComponent();public CharacterControllerComponent GetCharacterController(){return Controller;}public CompartmentAccessComponent GetCompartmentAccessComponent(){return Access;}public override object FindComponent(Type t){if(t==typeof(AIControlComponent))return AI;if(t==typeof(FactionAffiliationComponent))return Faction;return Rpl;}}
public class PlayerController:Native{public IEntity Controlled;public IEntity GetControlledEntity(){return Controlled;}}
public class PlayerManager:Native{public int Controlled;public bool MissingPC,Overflow;public PlayerController PC=new PlayerController();public Action Read;public int Reads,ReadAt=1;public int GetPlayerIdFromControlledEntity(IEntity x){return Controlled;}public int GetAllPlayers(EArray<int> ids){ids.Add(1);return Overflow?257:1;}public PlayerController GetPlayerController(int id){Reads++;if(Reads==ReadAt){var r=Read;Read=null;if(r!=null)r.Invoke();}return MissingPC?null:PC;}}
public static class SCR_PossessingManagerComponent{public static int Player;public static int GetPlayerIdFromMainEntity(IEntity x){return Player;}}
public static class AICF_VehicleBoardingMutationFence{public static bool IsAuthoritativeReplicatedEntity(IEntity x){return x&&x.Master;}}
public class AICF_GroupSlot:Native{public SCR_AIGroup Group;public int Generation=1;public SCR_AIGroup GetGroup(){return Group;}public int GetSpawnGeneration(){return Generation;}}
public class AICF_StrategicAssignmentSnapshot:Native{public SCR_AIGroup Group;public int Assignment=3,Intent=4,Base=5;public SCR_AIGroup GetGroup(){return Group;}public int GetAssignmentRevision(){return Assignment;}public int GetStrategicIntentRevision(){return Intent;}public int GetBaseRevision(){return Base;}}
public class AICF_TransportTrip:Native{public AICF_StrategicAssignmentSnapshot Assignment;public AICF_TripRetirementIdentity Admission;public AICF_VehicleLease Lease;public AICF_GroupSlot Slot;public int Transition=6;public string Cause="terminal",Operation="trip-USSR-0-1-1";public bool Terminal=true,Identity=true;public bool IsTerminal(){return Terminal;}public bool OwnsRetirementIdentity(AICF_TripRetirementIdentity x){return x==Admission;}public AICF_StrategicAssignmentSnapshot GetAssignment(){return Assignment;}public AICF_GroupSlot GetVehiclePresetSlot(){return Slot;}public AICF_VehicleLease GetLease(){return Lease;}public int GetTransitionCount(){return Transition;}public string GetCausationId(){return Cause;}public string GetOperationId(){return Operation;}public string GetFactionKey(){return "USSR";}public int GetSlotId(){return 0;}public int GetGroupGeneration(){return 1;}public int GetTripGeneration(){return 1;}public bool MatchesIdentity(string f,int s,int g,int t){return Identity&&f=="USSR"&&s==0&&g==1&&t==1;}}
public class AICF_VehicleLease:Native{public Vehicle Vehicle;public bool Cap=true,Identity=true;public int LeaseGen=1,VehicleGen=1;public AICF_EVehicleLeaseState State=AICF_EVehicleLeaseState.ACTIVE;public Vehicle GetVehicle(){return Vehicle;}public bool IsCapActive(){return Cap;}public AICF_EVehicleLeaseState GetState(){return State;}public bool MatchesTripIdentity(string f,int s,int g,int t){return Identity;} }
public class AICF_FactionFleet:Native{public AICF_VehicleLease Lease;public string GetFactionKey(){return "USSR";}public AICF_VehicleLease FindLeaseForSlot(int s,int g){return Lease;}}
public class AICF_VehicleCleanupFence:Native{public AICF_VehicleLease Lease;public int LeaseGen,VehicleGen,Entity;public string Rpl,Token;public bool InitializeFromLease(AICF_VehicleLease l,string token){Lease=l;LeaseGen=l.LeaseGen;VehicleGen=l.VehicleGen;Entity=l.Vehicle.Id;Rpl=l.Vehicle.Rpl.Value.ToString();Token=token;return true;}public bool MatchesFence(AICF_VehicleCleanupFence f){return f&&f.Entity==Entity&&f.Rpl==Rpl&&f.Token==Token&&f.LeaseGen==LeaseGen&&f.VehicleGen==VehicleGen;}public bool MatchesLease(AICF_VehicleLease l){return l==Lease&&l.LeaseGen==LeaseGen&&l.VehicleGen==VehicleGen&&l.Vehicle.Id==Entity&&l.Vehicle.Rpl.Value.ToString()==Rpl;}public string GetActionToken(){return Token;}public int GetSlotId(){return 0;}public int GetGroupGeneration(){return 1;}public int GetEntityId(){return Entity;}public string GetRplId(){return Rpl;}}
public class AICF_VehicleCleanupJob:Native{public AICF_FactionFleet m_Fleet;public AICF_VehicleLease m_Lease;public SCR_AIGroup m_Group;public AICF_VehicleCleanupFence m_Fence;public AICF_CapturedDeadCleanupIdentity m_CapturedDeadCleanup;public string m_sOperationId="trip-USSR-0-1-1",m_sActionToken="cleanup-1";public bool m_bCompleted,m_bReleaseComplete;public int m_iNonAliveDetachAttempts,m_iLastNonAliveDetachAtMs;}
public class AICF_TripRetirementMember:Native{public ChimeraCharacter m_Character;public int m_Id;public string m_sRpl;public bool m_bAdmissionAlive=true;public BaseWorld m_World;public RplComponent m_Rpl;public CharacterControllerComponent m_Controller;}
public class AICF_TripRetirementIdentity:Native{public bool m_bCaptured=true,m_bRosterTrusted=true;public int m_GroupId,m_GroupRpl,m_iSlot=0,m_iGroupGeneration=1,m_iTripGeneration=1;public string m_sFaction="USSR",m_sOperation="trip-USSR-0-1-1";public SCR_AIGroup m_CapturedGroup;public BaseWorld m_AdmissionWorld;public RplComponent m_AdmissionGroupRpl;public AICF_StrategicAssignmentSnapshot m_AdmissionAssignment;public EArray<AICF_TripRetirementMember> m_aMembers=new EArray<AICF_TripRetirementMember>();public int GetCapturedMemberCount(){return m_aMembers.Count();} ADMISSION }
public static class IntId {public static bool IsValid(this int x){return x>0;}}
public class BaseCompartmentSlot:Native{public Vehicle Vehicle;public ChimeraCharacter Occupant;public Action DoorRead,OccupantRead;public int Door=0;public Vehicle GetVehicle(){return Vehicle;}public ChimeraCharacter GetOccupant(){var r=OccupantRead;OccupantRead=null;if(r != null)r.Invoke();return Occupant;}public int PickDoorIndexForPoint(int p){var r=DoorRead;DoorRead=null;if(r != null)r.Invoke();return Door;}}
public class CompartmentAccessComponent:Native{public BaseCompartmentSlot Seat;public Action Kick;public int Kicks;public BaseCompartmentSlot GetCompartment(){return Seat;}public void KickFromVehicle(int d){Kicks++;var r=Kick;Kick=null;if(r != null)r.Invoke();if(Seat)Seat.Occupant=null;}}
public class Config:Native{public bool Hidden=true;public bool GetHiddenRecoveryEnabled(){return Hidden;}public float GetHiddenRecoveryPlayerRadiusMeters(){return 100;}}
public class Watchdog:Native{public bool Allowed=true;public Action Read;public bool CanApplyHiddenRecovery(int a,int b,float radius,out float nearest,out string reason){nearest=200;reason="OK";var r=Read;Read=null;if(r != null)r.Invoke();return Allowed;}}
public static class AICF_VehicleCleanupRecoveryRoster{public static string ComponentRpl(ChimeraCharacter c){return c.Rpl.Value.ToString();}}
public static class AICF_Stage35Diagnostics{public static void Info(string e,string d){}}
public class Manager:Native{public Config m_Config=new Config();public Watchdog m_Watchdog=new Watchdog();public const int NON_ALIVE_DETACH_MAX_ATTEMPTS=3,NON_ALIVE_DETACH_INTERVAL_MS=1000;public bool IsAuthority(){return Replication.Server;}public bool IsJobCurrent(AICF_VehicleCleanupJob j){return j&&!j.m_bCompleted&&j.m_Fleet.Lease==j.m_Lease;}public bool IsExactCleanupOccupantOfVehicle(ChimeraCharacter c,Vehicle v){return c.Access&&c.Access.Seat&&c.Access.Seat.Vehicle==v&&c.Access.Seat.Occupant==c;}public string BuildJobIdentity(AICF_VehicleCleanupJob j){return "job";}public bool Run(AICF_VehicleCleanupJob job,Vehicle vehicle,int nowMs) HANDLER }
CAPABILITY
public class Env{public static Game Game;public ChimeraCharacter Actor;public Vehicle Vehicle;public SCR_AIGroup Group;public AICF_TransportTrip Trip;public AICF_TripRetirementIdentity Admission;public AICF_VehicleCleanupJob Job;public Manager Manager=new Manager();public Env(){Replication.Server=true;SCR_PossessingManagerComponent.Player=0;Game=new Game();Group=new SCR_AIGroup{Id=316,World=Game.World};Vehicle=new Vehicle{Id=10586,World=Game.World};Actor=new ChimeraCharacter{Id=3821,World=Game.World};Actor.Access.Seat=new BaseCompartmentSlot{Vehicle=Vehicle,Occupant=Actor};Game.World.Map[316]=Group;Game.World.Map[10586]=Vehicle;Game.World.Map[3821]=Actor;var assignment=new AICF_StrategicAssignmentSnapshot{Group=Group};var lease=new AICF_VehicleLease{Vehicle=Vehicle};var fleet=new AICF_FactionFleet{Lease=lease};Admission=new AICF_TripRetirementIdentity{m_CapturedGroup=Group,m_GroupId=316,m_GroupRpl=Group.Rpl.Value,m_AdmissionGroupRpl=Group.Rpl,m_AdmissionAssignment=assignment,m_AdmissionWorld=Game.World};Admission.m_aMembers.Add(new AICF_TripRetirementMember{m_Character=Actor,m_Id=3821,m_sRpl=Actor.Rpl.Value.ToString(),m_World=Game.World,m_Rpl=Actor.Rpl,m_Controller=Actor.Controller});Trip=new AICF_TransportTrip{Admission=Admission,Assignment=assignment,Lease=lease,Slot=new AICF_GroupSlot{Group=Group}};Job=new AICF_VehicleCleanupJob{m_Group=Group,m_Fleet=fleet,m_Lease=lease,m_Fence=new AICF_VehicleCleanupFence()};Job.m_Fence.InitializeFromLease(lease,Job.m_sActionToken);Job.m_CapturedDeadCleanup=AICF_CapturedDeadCleanupIdentity.Capture(Admission,Trip,Job);if(!Job.m_CapturedDeadCleanup)throw new Exception("positive capability capture failed");}}
'@
$code=$code.Replace('ADMISSION',$admissionMethods).Replace('HANDLER',$handler).Replace('CAPABILITY',$cap)
$code=$code.Replace('public bool m_bCaptured=true','public int m_iAdmissionAssignmentRevision=3,m_iAdmissionIntentRevision=4,m_iAdmissionBaseRevision=5;public bool m_bCaptured=true')
$code=$code.Replace('public class AICF_GroupSlot:Native{','public class AICF_GroupSlot:Native{public int GetSlotId(){return 0;}')
$code=$code.Replace('public Action DoorRead,OccupantRead;','public Action DoorRead,OccupantRead;public int OccupantReads,OccupantReadAt=1;')
$code=$code.Replace('var r=OccupantRead;OccupantRead=null;if(r != null)r.Invoke();return Occupant;','OccupantReads++;if(OccupantReads==OccupantReadAt){var r=OccupantRead;OccupantRead=null;if(r != null)r.Invoke();}return Occupant;')
$code=$code.Replace('public class SCR_AIGroup:IEntity{}','public class SCR_AIGroup:IEntity{public EArray<AIAgent> Agents=new EArray<AIAgent>();public void GetAgents(EArray<AIAgent> agents){agents.AddRange(Agents);}}')
$code=$code.Replace('public SCR_AIGroup GetParentGroup(){return Parent;}','public SCR_AIGroup GetParentGroup(){return Parent;}public IEntity GetControlledEntity(){return null;}')
$code=$code.Replace('public class ChimeraCharacter:IEntity{','public class ChimeraCharacter:IEntity{public static ChimeraCharacter Cast(object x){return x as ChimeraCharacter;}')
$code=$code.Replace('public AICF_CapturedDeadCleanupIdentity m_CapturedDeadCleanup;','public AICF_CapturedDeadCleanupIdentity m_CapturedDeadCleanup;public AICF_TripRetirementIdentity m_OrphanRetirement;')
$code=$code.Replace('public class Manager:Native{','public class Manager:Native{public bool OldOrphanCandidate(AICF_VehicleCleanupJob job,Vehicle vehicle){'+$oldGuard+'return true;}public int OldRosterCandidates(AICF_VehicleCleanupJob job,Vehicle vehicle){'+$oldRoster+'return candidates;}')
$code+='public static class AICF_GroupRuntime{public static bool IsAliveCharacter(ChimeraCharacter c){return c.Controller.Life==ECharacterLifeState.ALIVE;}}'
Add-Type -TypeDefinition $code
$oldCase=[Env]::new()
if($oldCase.Manager.OldOrphanCandidate($oldCase.Job,$oldCase.Vehicle) -or $oldCase.Manager.OldRosterCandidates($oldCase.Job,$oldCase.Vehicle) -ne 0){throw 'Actual old paths must reject original DEAD actor removed from surviving group roster'}
$cases=@('dead-removed-original','same-original-parent','terminal-lease-detached','living','incap','foreign-parent','player-pm','player-main','player-pc','player-controller','missing-pm','missing-pc','player-overflow','world','actor-world','actor-lookup','actor-rpl-id','actor-rpl-component','actor-controller','actor-authority','admission-not-alive','untrusted','group-generation','group-lookup','group-rpl-component','different-assignment','assignment-revision','intent-revision','base-revision','trip-generation','terminal-transition','terminal-cause','not-terminal','successor-lease','lease-generation','vehicle-generation','lease-cap','fleet-successor','vehicle-world','vehicle-rpl-component','vehicle-rpl-id','vehicle-authority','vehicle-lookup','job-fence','job-token','release-complete','completed','authority','foreign-seat','foreign-occupant','door-invalid','player-near','hidden-disabled','budget','interval','callback-door-life','callback-door-seat','callback-player-world','callback-player-successor','callback-pc-world','callback-rpl-release','callback-kick-successor','callback-seat-late-successor','callback-seat-late-world','callback-seat-late-budget','callback-seat-late-controller','callback-seat-late-pm','callback-seat-late-main','callback-seat-late-pc','callback-seat-late-controller-component','callback-seat-late-access','callback-seat-late-seat','callback-final-seat-controller','callback-final-seat-pm','callback-final-seat-main','callback-final-seat-pc','callback-final-pc-world','callback-final-pc-controller','callback-final-pc-main','callback-final-pc-pm','callback-final-pc-component','callback-final-parent-pc','final-parent-order','callback-final-seat-game-missing','callback-final-pc-game-missing','callback-final-pc-actor-removed','callback-final-pc-controller-removed','callback-final-pc-group-removed','callback-final-seat-actor-removed','callback-final-seat-controller-removed')
$count=0
foreach($case in $cases){
 $e=[Env]::new();$originalAccess=$e.Actor.Access;$expected=0;if($case -in @('dead-removed-original','same-original-parent','terminal-lease-detached','callback-kick-successor')){$expected=1}
 switch($case){
  'same-original-parent'{$e.Actor.AI.Agent.Parent=$e.Group}
  'terminal-lease-detached'{$e.Trip.Lease=$null}
  'living'{$e.Actor.Controller.Life=[ECharacterLifeState]::ALIVE}
  'incap'{$e.Actor.Controller.Life=[ECharacterLifeState]::INCAPACITATED}
  'foreign-parent'{$e.Actor.AI.Agent.Parent=[SCR_AIGroup]::new()}
  'player-pm'{[Env]::Game.PM.Controlled=1}
  'player-main'{[SCR_PossessingManagerComponent]::Player=1}
  'player-pc'{[Env]::Game.PM.PC.Controlled=$e.Actor}
  'player-controller'{$e.Actor.Controller.Player=$true}
  'missing-pm'{[Env]::Game.PM=$null}
  'missing-pc'{[Env]::Game.PM.MissingPC=$true}
  'player-overflow'{[Env]::Game.PM.Overflow=$true}
  'world'{[Env]::Game.World=[BaseWorld]::new()}
  'actor-world'{$e.Actor.World=[BaseWorld]::new()}
  'actor-lookup'{[Env]::Game.World.Map[3821]=[ChimeraCharacter]::new()}
  'actor-rpl-id'{$e.Actor.Rpl.Value=8}
  'actor-rpl-component'{$e.Actor.Rpl=[RplComponent]::new()}
  'actor-controller'{$e.Actor.Controller=[CharacterControllerComponent]::new()}
  'actor-authority'{$e.Actor.Master=$false}
  'admission-not-alive'{$e.Admission.m_aMembers[0].m_bAdmissionAlive=$false}
  'untrusted'{$e.Admission.m_bRosterTrusted=$false}
  'group-generation'{$e.Trip.Slot.Generation=2}
  'group-lookup'{[Env]::Game.World.Map[316]=[SCR_AIGroup]::new()}
  'group-rpl-component'{$e.Group.Rpl=[RplComponent]::new()}
  'different-assignment'{$e.Trip.Assignment=[AICF_StrategicAssignmentSnapshot]::new()}
  'assignment-revision'{$e.Trip.Assignment.Assignment++}
  'intent-revision'{$e.Trip.Assignment.Intent++}
  'base-revision'{$e.Trip.Assignment.Base++}
  'trip-generation'{$e.Trip.Identity=$false}
  'terminal-transition'{$e.Trip.Transition++}
  'terminal-cause'{$e.Trip.Cause='foreign'}
  'not-terminal'{$e.Trip.Terminal=$false}
  'successor-lease'{$e.Trip.Lease=[AICF_VehicleLease]::new()}
  'lease-generation'{$e.Job.m_Lease.LeaseGen++}
  'vehicle-generation'{$e.Job.m_Lease.VehicleGen++}
  'lease-cap'{$e.Job.m_Lease.Cap=$false}
  'fleet-successor'{$e.Job.m_Fleet.Lease=[AICF_VehicleLease]::new()}
  'vehicle-world'{$e.Vehicle.World=[BaseWorld]::new()}
  'vehicle-rpl-component'{$e.Vehicle.Rpl=[RplComponent]::new()}
  'vehicle-rpl-id'{$e.Vehicle.Rpl.Value=9}
  'vehicle-authority'{$e.Vehicle.Master=$false}
  'vehicle-lookup'{[Env]::Game.World.Map[10586]=[Vehicle]::new()}
  'job-fence'{$e.Job.m_Fence=[AICF_VehicleCleanupFence]::new()}
  'job-token'{$e.Job.m_sActionToken='foreign'}
  'release-complete'{$e.Job.m_bReleaseComplete=$true}
  'completed'{$e.Job.m_bCompleted=$true}
  'authority'{[Replication]::Server=$false}
  'foreign-seat'{$e.Actor.Access.Seat.Vehicle=[Vehicle]::new()}
  'foreign-occupant'{$e.Actor.Access.Seat.Occupant=[ChimeraCharacter]::new()}
  'door-invalid'{$e.Actor.Access.Seat.Door=-1}
  'player-near'{$e.Manager.m_Watchdog.Allowed=$false}
  'hidden-disabled'{$e.Manager.m_Config.Hidden=$false}
  'budget'{$e.Job.m_iNonAliveDetachAttempts=3}
  'interval'{$e.Job.m_iLastNonAliveDetachAtMs=9500}
  'callback-door-life'{$x=$e;$e.Actor.Access.Seat.DoorRead={$x.Actor.Controller.Life=[ECharacterLifeState]::ALIVE}.GetNewClosure()}
  'callback-door-seat'{$x=$e;$e.Actor.Access.Seat.DoorRead={$x.Actor.Access.Seat.Occupant=[ChimeraCharacter]::new()}.GetNewClosure()}
  'callback-player-world'{$e.Manager.m_Watchdog.Read={[Env]::Game.World=[BaseWorld]::new()}}
  'callback-player-successor'{$x=$e;$e.Manager.m_Watchdog.Read={$x.Job.m_Fleet.Lease=[AICF_VehicleLease]::new()}.GetNewClosure()}
  'callback-pc-world'{[Env]::Game.PM.Read={[Env]::Game.World=[BaseWorld]::new()}}
  'callback-rpl-release'{$x=$e;$e.Vehicle.Rpl.Read={$x.Job.m_bReleaseComplete=$true}.GetNewClosure()}
  'callback-kick-successor'{$x=$e;$e.Actor.Access.Kick={$x.Job.m_Fleet.Lease=[AICF_VehicleLease]::new()}.GetNewClosure()}
  'callback-seat-late-successor'{$x=$e;$e.Actor.Access.Seat.OccupantReadAt=3;$e.Actor.Access.Seat.OccupantRead={$x.Job.m_Fleet.Lease=[AICF_VehicleLease]::new()}.GetNewClosure()}
  'callback-seat-late-world'{$e.Actor.Access.Seat.OccupantReadAt=3;$e.Actor.Access.Seat.OccupantRead={[Env]::Game.World=[BaseWorld]::new()}}
  'callback-seat-late-controller'{$x=$e;$e.Actor.Access.Seat.OccupantReadAt=3;$e.Actor.Access.Seat.OccupantRead={$x.Actor.Controller.Player=$true}.GetNewClosure()}
  'callback-seat-late-pm'{$e.Actor.Access.Seat.OccupantReadAt=3;$e.Actor.Access.Seat.OccupantRead={[Env]::Game.PM.Controlled=1}}
  'callback-seat-late-main'{$e.Actor.Access.Seat.OccupantReadAt=3;$e.Actor.Access.Seat.OccupantRead={[SCR_PossessingManagerComponent]::Player=1}}
  'callback-seat-late-pc'{$x=$e;$e.Actor.Access.Seat.OccupantReadAt=3;$e.Actor.Access.Seat.OccupantRead={[Env]::Game.PM.PC.Controlled=$x.Actor}.GetNewClosure()}
  'callback-seat-late-controller-component'{$x=$e;$e.Actor.Access.Seat.OccupantReadAt=3;$e.Actor.Access.Seat.OccupantRead={$x.Actor.Controller=[CharacterControllerComponent]::new()}.GetNewClosure()}
  'callback-seat-late-access'{$x=$e;$e.Actor.Access.Seat.OccupantReadAt=3;$e.Actor.Access.Seat.OccupantRead={$x.Actor.Access=[CompartmentAccessComponent]::new()}.GetNewClosure()}
  'callback-seat-late-seat'{$x=$e;$e.Actor.Access.Seat.OccupantReadAt=3;$e.Actor.Access.Seat.OccupantRead={$x.Actor.Access.Seat=[BaseCompartmentSlot]::new()}.GetNewClosure()}
  'callback-final-seat-controller'{$x=$e;$e.Actor.Access.Seat.OccupantReadAt=4;$e.Actor.Access.Seat.OccupantRead={$x.Actor.Controller.Player=$true}.GetNewClosure()}
  'callback-final-seat-pm'{$e.Actor.Access.Seat.OccupantReadAt=4;$e.Actor.Access.Seat.OccupantRead={[Env]::Game.PM.Controlled=1}}
  'callback-final-seat-main'{$e.Actor.Access.Seat.OccupantReadAt=4;$e.Actor.Access.Seat.OccupantRead={[SCR_PossessingManagerComponent]::Player=1}}
  'callback-final-seat-pc'{$x=$e;$e.Actor.Access.Seat.OccupantReadAt=4;$e.Actor.Access.Seat.OccupantRead={[Env]::Game.PM.PC.Controlled=$x.Actor}.GetNewClosure()}
  'callback-final-pc-world'{[Env]::Game.PM.ReadAt=5;[Env]::Game.PM.Read={[Env]::Game.World=[BaseWorld]::new()}}
  'callback-final-pc-controller'{$x=$e;[Env]::Game.PM.ReadAt=5;[Env]::Game.PM.Read={$x.Actor.Controller.Player=$true}.GetNewClosure()}
  'callback-final-pc-main'{[Env]::Game.PM.ReadAt=5;[Env]::Game.PM.Read={[SCR_PossessingManagerComponent]::Player=1}}
  'callback-final-pc-pm'{[Env]::Game.PM.ReadAt=5;[Env]::Game.PM.Read={[Env]::Game.PM.Controlled=1}}
  'callback-final-pc-component'{$x=$e;[Env]::Game.PM.ReadAt=5;[Env]::Game.PM.Read={$x.Actor.Controller=[CharacterControllerComponent]::new()}.GetNewClosure()}
  'callback-final-parent-pc'{$x=$e;$e.Actor.AI.Agent.ParentReadAt=8;$e.Actor.AI.Agent.ParentRead={[Env]::Game.PM.PC.Controlled=$x.Actor}.GetNewClosure()}
  'callback-final-seat-game-missing'{$e.Actor.Access.Seat.OccupantReadAt=4;$e.Actor.Access.Seat.OccupantRead={[Env]::Game=$null}}
  'callback-final-pc-game-missing'{[Env]::Game.PM.ReadAt=5;[Env]::Game.PM.Read={[Env]::Game=$null}}
  'callback-final-pc-actor-removed'{$x=$e;[Env]::Game.PM.ReadAt=5;[Env]::Game.PM.Read={$x.Actor.Removed=$true}.GetNewClosure()}
  'callback-final-pc-controller-removed'{$x=$e;[Env]::Game.PM.ReadAt=5;[Env]::Game.PM.Read={$x.Actor.Controller.Removed=$true}.GetNewClosure()}
  'callback-final-seat-actor-removed'{$x=$e;$e.Actor.Access.Seat.OccupantReadAt=4;$e.Actor.Access.Seat.OccupantRead={$x.Actor.Removed=$true}.GetNewClosure()}
  'callback-final-seat-controller-removed'{$x=$e;$e.Actor.Access.Seat.OccupantReadAt=4;$e.Actor.Access.Seat.OccupantRead={$x.Actor.Controller.Removed=$true}.GetNewClosure()}
  'callback-final-pc-group-removed'{$x=$e;[Env]::Game.PM.ReadAt=5;[Env]::Game.PM.Read={$x.Group.Removed=$true}.GetNewClosure()}
  'final-parent-order'{$e.Actor.AI.Agent.AssertSeat=$e.Actor.Access.Seat;$expected=1}
  'callback-seat-late-budget'{$x=$e;$e.Actor.Access.Seat.OccupantReadAt=3;$e.Actor.Access.Seat.OccupantRead={$x.Job.m_iNonAliveDetachAttempts=3}.GetNewClosure()}
 }
 $before=$e.Job.m_iNonAliveDetachAttempts;$result=$e.Manager.Run($e.Job,$e.Vehicle,10000)
 if($originalAccess.Kicks -ne $expected){throw "Actual captured casualty replay case=$case kicks=$($originalAccess.Kicks) expected=$expected"}
 if($expected -eq 1 -and $e.Job.m_iNonAliveDetachAttempts -ne $before+1){throw "Shared budget not consumed before Kick: $case"}
 if($case -eq 'callback-kick-successor' -and $result){throw 'Native effect successor callback must stop caller before scan/release'}
 $count++
}
$budget=[Env]::new()
foreach($at in @(10000,11000,12000,13000)){$budget.Actor.Access.Seat.Occupant=$budget.Actor;[void]$budget.Manager.Run($budget.Job,$budget.Vehicle,$at)}
if($budget.Actor.Access.Kicks -ne 3 -or $budget.Job.m_iNonAliveDetachAttempts -ne 3){throw 'Original shared3 budget must stop fourth exact DEAD attempt'}
# The exact old candidate sources reject 0EED: group survives, actor removed.
$old=Body $manager 'TryDetachCapturedDeadOrphanMembers'
if(!$old.Contains('job.m_Group ||') -or !(Body $manager 'TryDetachNonAliveExactOccupants').Contains('job.m_Group.GetAgents(agents)')){throw 'Old native counterexample shape changed; investigate before replacing evidence'}
"PASS Captured casualty cleanup: actual_source_cases=$count source_mutants=$($mutations.Count); native0EED group-survives/dead-removed positive; native APIs SIMULATED; Workbench/runtime NOT RUN"
