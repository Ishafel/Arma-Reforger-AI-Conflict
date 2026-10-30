param([string]$RepositoryRoot=(Split-Path -Parent (Split-Path -Parent $PSScriptRoot)))
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot '../lib/Stage3StaticAudit.Common.ps1')
$taskForce=Join-Path $RepositoryRoot 'AIConflictCore/Scripts/Game/AIConflict/Forces'
$taskUse=[IO.File]::ReadAllText((Join-Path $taskForce 'AICF_AIQuotaVehicleUse.c')).Replace("`r`n","`n")
function Body([string]$Text,[string]$Name){Get-AICFMethodBody ([pscustomobject]@{Source=$Text;Code=(ConvertTo-AICFCodeText $Text)}) $Name}
function HashText([string]$Text){$h=[Security.Cryptography.SHA256]::Create();try{[BitConverter]::ToString($h.ComputeHash([Text.Encoding]::UTF8.GetBytes($Text.Replace("`r`n","`n")))).Replace('-','')}finally{$h.Dispose()}}
$taskCollect=Body $taskUse 'CollectSeats'
$taskNewEdges=@'
				if (child.GetParent() != node || child.GetWorld() != vehicle.GetWorld() ||
					siblings.Count() >= MAX_TOPOLOGY || siblings.Contains(child)) return false;
				siblings.Insert(child);
				if (ChimeraCharacter.Cast(child))
				{
					if (BaseCompartmentManagerComponent.Cast(child.FindComponent(BaseCompartmentManagerComponent))) return false;
				}
				else
				{
					if (nodes.Count() >= MAX_TOPOLOGY || nodes.Contains(child)) return false;
					nodes.Insert(child);
				}
'@
$taskOldEdges=@'
				if (nodes.Count() >= MAX_TOPOLOGY || nodes.Contains(child) || child.GetParent() != node) return false;
				nodes.Insert(child);
'@
$taskCollectForLegacy=[regex]::Replace($taskCollect,'(?m)^\t{4}// Seated characters[^\n]*\n\t{4}//[^\n]*\n','')
if(!$taskCollectForLegacy.Contains($taskNewEdges)){throw 'Named character boundary not found'}
$taskLegacyCollect=$taskCollectForLegacy.Replace("`t`t`tarray<IEntity> siblings = {};`n",'').Replace($taskNewEdges,$taskOldEdges)
if((HashText $taskLegacyCollect)-ne'C695E694436E8CA50C288260B7F1D1221D3522CA714E2FB4C5289EB9CC86EC44'){throw 'Before predicate no longer matches preserved actual baseline'}
if($taskCollect -match 'InventoryItemComponent|SetOrigin|Teleport|SpawnEntity|SetReserved|GetInVehicle|ActivateAI|SetLOD'){throw 'Topology boundary must only read structural/character hierarchy'}
if(!$taskUse.Contains('MAX_TOPOLOGY = 256')){throw 'Original topology bound changed'}
$taskLegacy=$taskUse.Replace($taskCollect,$taskLegacyCollect)
$taskIdentity=[IO.File]::ReadAllText((Join-Path $taskForce 'AICF_AIQuotaVehicleEntry.c'))
$taskIdentity=$taskIdentity.Substring(0,$taskIdentity.IndexOf('class AICF_AIQuotaVehicleEntry'))
$taskRow=[IO.File]::ReadAllText((Join-Path $taskForce 'AICF_AIQuotaCensus.c'))
$taskRow=$taskRow.Substring(0,$taskRow.IndexOf('class AICF_AIQuotaCensus'))
function Replay([string]$Text){
 $Text=$Text.Replace('ref ','').Replace('GetGame()','Runtime.Game').Replace('EntityID','long').Replace('static const int','public const int')
 $Text=$Text.Replace('long.INVALID','0L').Replace('m_sRpl.IsEmpty()','string.IsNullOrEmpty(m_sRpl)').Replace('ECharacterLifeState m_Life','int m_Life')
 $Text=[regex]::Replace($Text,'array<([^>]+)>','EArray<$1>')
 $Text=[regex]::Replace($Text,'(EArray<([^>]+)>\s+\w+\s*=\s*)\{\};','$1new EArray<$2>();')
 $Text=$Text.Replace('EArray<IEntity> nodes = {vehicle};','EArray<IEntity> nodes = new EArray<IEntity>{vehicle};')
 $Text=[regex]::Replace($Text,'(?m)^class (AICF_\w+)\s*$','public class $1:Native')
 $Text=[regex]::Replace($Text,'(?m)^(\s*)(static (?:bool|AICF_\w+)|bool|string) (\w+)\(','$1public $2 $3(')
 $Text=[regex]::Replace($Text,'(?m)^(\s*)(\w+(?:<[^>]+>)?) (m_\w+)(?=;|\s*=)','$1public $2 $3')
 foreach($type in @('RplComponent','BaseCompartmentManagerComponent')){$Text=$Text.Replace('FindComponent('+$type+')','FindComponent(typeof('+$type+'))')}
 $Text=[regex]::Replace($Text,'for \(int (\w+);','for (int $1 = 0;')
 $Text=[regex]::Replace($Text,'foreach \(([^:]+) : ([^)]+)\)','foreach ($1 in $2)')
 $Text=[regex]::Replace($Text,'(?m)^(\s*)(int|bool) (\w+);','$1$2 $3 = default($2);')
 $Text=[regex]::Replace($Text,'(?m)^(\s*)(AIAgent|AICF_\w+) (?!m_)(\w+);','$1$2 $3 = null;')
 return $Text
}
$taskShim=@'
using System;using System.Collections.Generic;
namespace VehicleTopologyReplay{
public class Native{public static implicit operator bool(Native n){return n!=null;}}
public class EArray<T>:List<T>{public static implicit operator bool(EArray<T> a){return a!=null;}public new int Count(){return base.Count;}public void Insert(T v){Add(v);}}
public static class Replication{public static bool Server=true;public static bool IsServer(){return Server;}}
public class RplId{public int Value=1;public bool IsValid(){return Value>0;}public override string ToString(){return Value.ToString();}}
public class RplComponent:Native{public bool Master=true,Proxy;public RplId Value=new RplId();public RplId Id(){return Value;}public bool IsMaster(){return Master;}public bool IsProxy(){return Proxy;}public static RplComponent Cast(object o){return o as RplComponent;}}
public class BaseWorld:Native{public Dictionary<long,IEntity> Entities=new Dictionary<long,IEntity>();public IEntity FindEntityByID(long id){IEntity e;return Entities.TryGetValue(id,out e)?e:null;}}
public class IEntity:Native{public BaseWorld World;public long Id;public IEntity Parent,Child,Sibling;public RplComponent Rpl=new RplComponent();public BaseCompartmentManagerComponent Manager;public int SiblingReads;public BaseWorld GetWorld(){return World;}public long GetID(){return Id;}public IEntity GetParent(){return Parent;}public IEntity GetChildren(){return Child;}public IEntity GetSibling(){SiblingReads++;return Sibling;}public object FindComponent(Type t){return t==typeof(RplComponent)?(object)Rpl:Manager;}}
public class Vehicle:IEntity{}public class AIGroup:IEntity{}
public class AIAgent:IEntity{public IEntity Actor;public AIGroup Group;public bool Activated=true,Counted=true;public IEntity GetControlledEntity(){return Actor;}public AIGroup GetParentGroup(){return Group;}public bool IsAIActivated(){return Activated;}public bool CountTowardsAIWorldLimit(){return Counted;}}
public class AIControlComponent:Native{public AIAgent Agent;public AIAgent GetControlAIAgent(){return Agent;}}
public static class ECharacterLifeState{public const int ALIVE=1,DEAD=3;}
public class CharacterControllerComponent:Native{public int Life=1;public bool Player;public int GetLifeState(){return Life;}public bool IsPlayerControlled(){return Player;}}
public class ChimeraCharacter:IEntity{public AIControlComponent Control=new AIControlComponent();public CharacterControllerComponent Controller=new CharacterControllerComponent();public CharacterControllerComponent GetCharacterController(){return Controller;}public AIControlComponent GetAIControlComponent(){return Control;}public static ChimeraCharacter Cast(object o){return o as ChimeraCharacter;}}
public class PlayerManager:Native{public int PlayerId;public int GetPlayerIdFromControlledEntity(IEntity e){return PlayerId;}}
public static class SCR_PossessingManagerComponent{public static int MainId;public static int GetPlayerIdFromMainEntity(IEntity e){return MainId;}}
public class Game:Native{public BaseWorld World=new BaseWorld();public PlayerManager Players=new PlayerManager();public BaseWorld GetWorld(){return World;}public PlayerManager GetPlayerManager(){return Players;}}
public static class Runtime{public static Game Game;}
public class BaseCompartmentSlot:Native{public IEntity Owner,Vehicle,Occupant;public BaseCompartmentManagerComponent Manager;public IEntity GetOwner(){return Owner;}public IEntity GetVehicle(){return Vehicle;}public IEntity GetOccupant(){return Occupant;}public BaseCompartmentManagerComponent GetManager(){return Manager;}}
public class BaseCompartmentManagerComponent:Native{public EArray<BaseCompartmentSlot> Seats=new EArray<BaseCompartmentSlot>();public static BaseCompartmentManagerComponent Cast(object o){return o as BaseCompartmentManagerComponent;}public void GetCompartments(EArray<BaseCompartmentSlot> seats){seats.AddRange(Seats);}}
public class AICF_AIQuotaCensus:Native{public EArray<AICF_AIQuotaAgentRow> Rows=new EArray<AICF_AIQuotaAgentRow>();public int Count(){return Rows.Count();}public AICF_AIQuotaAgentRow GetRow(int i){return Rows[i];}}
'@
$taskCases=@'
public static class Cases{
static long Id;static Vehicle Car;static IEntity ManagerNode;static List<ChimeraCharacter> Actors;static AICF_AIQuotaAgentRow Row;
static T Register<T>(T e)where T:IEntity{e.Id=++Id;e.World=Runtime.Game.World;Runtime.Game.World.Entities[e.Id]=e;return e;}
static void Attach(IEntity parent,IEntity child){child.Parent=parent;if(parent.Child==null)parent.Child=child;else{var s=parent.Child;while(s.Sibling!=null)s=s.Sibling;s.Sibling=child;}}
static void Setup(int actors=4,int inventory=100,int structural=112,bool human=false){Replication.Server=true;SCR_PossessingManagerComponent.MainId=0;Runtime.Game=new Game();Id=100;Car=Register(new Vehicle());Car.Manager=new BaseCompartmentManagerComponent();ManagerNode=Register(new IEntity{Manager=new BaseCompartmentManagerComponent()});Attach(Car,ManagerNode);for(int i=2;i<structural;i++)Attach(Car,Register(new IEntity()));Actors=new List<ChimeraCharacter>();for(int i=0;i<actors;i++){var a=Register(new ChimeraCharacter());a.Controller.Player=human;a.Control.Agent=Register(new AIAgent{Actor=a});Actors.Add(a);Attach(Car,a);IEntity parent=a;for(int j=0;j<inventory;j++){var item=Register(new IEntity());Attach(parent,item);if(j%3==0)parent=item;}}for(int i=0;i<5;i++){var owner=i<2?(IEntity)Car:ManagerNode;var manager=owner.Manager;manager.Seats.Add(new BaseCompartmentSlot{Owner=owner,Vehicle=Car,Manager=manager,Occupant=i<actors?Actors[i]:null});}var native=Register(new AIAgent{Actor=Car});Row=new AICF_AIQuotaAgentRow{m_Agent=native,m_AgentId=native.Id,m_Entity=Car,m_EntityId=Car.Id,m_bVehicle=true,m_bActivated=true,m_bCounted=true};}
static bool Collect(){return AICF_AIQuotaVehicleUse.CollectSeats(Car,new EArray<BaseCompartmentSlot>());}
public static int Run(){int n=0;foreach(int actors in new[]{3,4,5}){Setup(actors);var seats=new EArray<BaseCompartmentSlot>();if(!AICF_AIQuotaVehicleUse.CollectSeats(Car,seats)||seats.Count()!=5)throw new Exception("vehicle topology contaminated by seated inventory"+actors);var use=AICF_AIQuotaVehicleUse.CaptureRow(Car,Row);if(use==null||!use.IsCurrent()||!use.m_bHasAI||!use.m_bActive||!use.m_bCounted)throw new Exception("full CaptureRow/current with seated crew"+actors);var snapshot=new AICF_AIQuotaCensus();snapshot.Rows.Add(Row);if(AICF_AIQuotaVehicleUse.Capture(Car,snapshot)==null)throw new Exception("snapshot Capture path");Attach(Actors[0],Register(new IEntity()));if(!use.IsCurrent())throw new Exception("inventory change invalidates physical seat custody");n++;}
foreach(string k in new[]{"client","worldEnded","structural257","foreignStructural","foreignCharacter","wrongStructuralParent","wrongCharacterParent","characterSiblingCycle","structuralCycle","tooManyCharacters","characterManager","prunedSeatOwner","foreignSeatVehicle","nullSeatManager","nullSeatOwner","nullSeat","missingManager","tooManySeats"}){Setup();switch(k){case"client":Replication.Server=false;break;case"worldEnded":Runtime.Game=null;break;case"structural257":Setup(0,0,257);break;case"foreignStructural":ManagerNode.World=new BaseWorld();break;case"foreignCharacter":Actors[0].World=new BaseWorld();break;case"wrongStructuralParent":ManagerNode.Parent=Actors[0];break;case"wrongCharacterParent":Actors[0].Parent=ManagerNode;break;case"characterSiblingCycle":Actors[3].Sibling=Actors[3];break;case"structuralCycle":ManagerNode.Child=Car;Car.Parent=ManagerNode;break;case"tooManyCharacters":Setup(0,0,2);for(int i=0;i<257;i++)Attach(Car,Register(new ChimeraCharacter()));break;case"characterManager":Actors[0].Manager=new BaseCompartmentManagerComponent();break;case"prunedSeatOwner":Car.Manager.Seats[0].Owner=Actors[0];break;case"foreignSeatVehicle":Car.Manager.Seats[0].Vehicle=Register(new Vehicle());break;case"nullSeatManager":Car.Manager.Seats[0].Manager=null;break;case"nullSeatOwner":Car.Manager.Seats[0].Owner=null;break;case"nullSeat":Car.Manager.Seats.Add(null);break;case"missingManager":Car.Manager=null;ManagerNode.Manager=null;break;case"tooManySeats":for(int i=0;i<256;i++)Car.Manager.Seats.Add(Car.Manager.Seats[0]);break;}if(k=="client"){if(AICF_AIQuotaVehicleUse.CaptureRow(Car,Row)!=null)throw new Exception("client physical use accepted");n++;continue;}if(Collect())throw new Exception("unsafe topology accepted "+k);if(k=="characterSiblingCycle"&&Actors[3].SiblingReads>2)throw new Exception("pruned sibling cycle not immediately bounded");n++;}
foreach(string k in new[]{"occupantReplaced","occupantLife","occupantController","occupantControl","occupantAgent","occupantFlags","humanChanged","nativeFlags","nativeMissing","rootRpl","seatRemoved","seatReplaced","slotOwnerRemoved"}){Setup();var use=AICF_AIQuotaVehicleUse.CaptureRow(Car,Row);if(use==null)throw new Exception("stale setup");switch(k){case"occupantReplaced":Car.Manager.Seats[0].Occupant=Actors[1];break;case"occupantLife":Actors[0].Controller.Life=ECharacterLifeState.DEAD;break;case"occupantController":Actors[0].Controller=new CharacterControllerComponent();break;case"occupantControl":Actors[0].Control=new AIControlComponent();break;case"occupantAgent":Actors[0].Control.Agent=Register(new AIAgent{Actor=Actors[0]});break;case"occupantFlags":Actors[0].Control.Agent.Counted=false;break;case"humanChanged":Actors[0].Controller.Player=true;break;case"nativeFlags":Row.m_Agent.Counted=false;break;case"nativeMissing":use.m_Native=null;break;case"rootRpl":Car.Rpl.Value.Value++;break;case"seatRemoved":ManagerNode.Manager.Seats.RemoveAt(0);break;case"seatReplaced":Car.Manager.Seats[0]=new BaseCompartmentSlot{Owner=Car,Vehicle=Car,Manager=Car.Manager,Occupant=Actors[0]};break;case"slotOwnerRemoved":Car.Child=ManagerNode.Sibling;break;}if(use.IsCurrent())throw new Exception("stale physical custody accepted "+k);n++;}
Setup(5,100,112,true);var humanUse=AICF_AIQuotaVehicleUse.CaptureRow(Car,Row);if(humanUse==null||humanUse.m_bHasAI||!humanUse.IsCurrent())throw new Exception("human occupants must remain observed, not skipped");Actors[0].Controller.Player=false;if(humanUse.IsCurrent())throw new Exception("human custody change ignored");n++;
Setup();var one=Car.Manager.Seats[0];ManagerNode.Manager.Seats.Add(one);var dedupe=new EArray<BaseCompartmentSlot>();if(!AICF_AIQuotaVehicleUse.CollectSeats(Car,dedupe)||dedupe.Count()!=5)throw new Exception("same exact seat exposed by two managers counted twice");n++;return n;}
public static int Before(){int n=0;foreach(int actors in new[]{3,4,5}){Setup(actors);if(Collect()||AICF_AIQuotaVehicleUse.CaptureRow(Car,Row)!=null)throw new Exception("old actual bound unexpectedly accepts inventory subtree"+actors);n++;}return n;}
}
}
'@
$taskActual=$taskShim+(Replay ($taskIdentity+$taskRow+$taskUse))+$taskCases
$taskBefore=$taskShim+(Replay ($taskIdentity+$taskRow+$taskLegacy))+$taskCases
$taskBefore=$taskBefore.Replace('VehicleTopologyReplay','VehicleTopologyBefore')
Add-Type -TypeDefinition $taskBefore -Language CSharp
$taskBeforeCount=[VehicleTopologyBefore.Cases]::Before()
Add-Type -TypeDefinition $taskActual -Language CSharp
$taskCount=[VehicleTopologyReplay.Cases]::Run()
$taskUnsafe=@(
 @('bound','public const int MAX_TOPOLOGY = 256;','public const int MAX_TOPOLOGY = 1024;'),
 @('sibling',' || siblings.Contains(child)',''),
 @('parent','child.GetParent() != node','false'),
 @('world','child.GetWorld() != vehicle.GetWorld()','false'),
 @('characterManager','if (BaseCompartmentManagerComponent.Cast(child.FindComponent(typeof(BaseCompartmentManagerComponent)))) return false;',''),
 @('seatVehicle','seat.GetVehicle() != vehicle','false'),
 @('seatOwner','!nodes.Contains(seat.GetOwner())','false'),
 @('staleOccupant',' || !occupant.IsCurrent()','')
)
$taskCaught=0
foreach($taskMutation in $taskUnsafe){if(!$taskActual.Contains($taskMutation[1])){throw "Unsafe target missing $($taskMutation[0])"};$taskNS='VehicleTopologyUnsafe'+$taskMutation[0];$taskMutant=$taskActual.Replace('VehicleTopologyReplay',$taskNS).Replace($taskMutation[1],$taskMutation[2]);Add-Type -TypeDefinition $taskMutant -Language CSharp;$taskBad=$false;try{[void]([type]($taskNS+'.Cases')).GetMethod('Run').Invoke($null,@())}catch{$taskBad=$true};if(!$taskBad){throw "Unsafe survived $($taskMutation[0])"};$taskCaught++}
"PASS vehicle_census_topology actual_before_rejected=$taskBeforeCount actual_collect_capture_current_cases=$taskCount executed_unsafe=$taskCaught structural_limit=256 character_inventory_pruned=1 native=NOT_RUN"
