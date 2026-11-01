param([string]$RepositoryRoot=(Split-Path -Parent (Split-Path -Parent $PSScriptRoot)))
$ErrorActionPreference='Stop'
$path=Join-Path $RepositoryRoot 'AIConflictCore/Scripts/Game/AIConflict/Forces/AICF_AIQuotaVehicleEntry.c'
$source=[IO.File]::ReadAllText($path)
if($source -match '\b(?:SetReserved|GetInVehicle|ActivateAI|DeactivateAI|SetLOD|SetFaction|SetLifeState)\s*\('){throw 'Vehicle entry scope must remain read-only'}
$actual=$source.Replace('ref ','').Replace('EntityID','long').Replace('GetGame()','Runtime.Game')
$actual=[regex]::Replace($actual,'\bFactionKey\b','string')
$actual=$actual.Replace('m_sRpl.IsEmpty()','string.IsNullOrEmpty(m_sRpl)').Replace('m_sGroupFactionKey.IsEmpty()','string.IsNullOrEmpty(m_sGroupFactionKey)')
$actual=$actual.Replace('FindComponent(RplComponent)','FindComponent(typeof(RplComponent))').Replace('FindComponent(BaseCompartmentManagerComponent)','FindComponent(typeof(BaseCompartmentManagerComponent))')
$actual=$actual.Replace('FindComponent(FactionAffiliationComponent)','FindComponent(typeof(FactionAffiliationComponent))').Replace('Faction actorFaction;','Faction actorFaction = null;')
$actual=$actual.Replace('array<BaseCompartmentSlot> seats = {};','List<BaseCompartmentSlot> seats = new List<BaseCompartmentSlot>();').Replace('seats.Count()','seats.Count')
$actual=$actual.Replace('foreach (BaseCompartmentSlot seat : seats)','foreach (BaseCompartmentSlot seat in seats)')
$actual=$actual.Replace('class AICF_AIQuotaVehicleEntity','public class AICF_AIQuotaVehicleEntity : Native').Replace('class AICF_AIQuotaVehicleEntry','public class AICF_AIQuotaVehicleEntry : Native')
$actual=[regex]::Replace($actual,'(?m)^(\s*)(static const int) ','$1public const int ')
$actual=[regex]::Replace($actual,'(?m)^(\s*)(static AICF_\w+|bool) (\w+)\(','$1public $2 $3(')
$actual=[regex]::Replace($actual,'(?m)^(\s*)(IEntity|BaseWorld|long|RplComponent|string|AICF_\w+|ChimeraCharacter|AIControlComponent|AIAgent|SCR_AIGroup|Faction|FactionAffiliationComponent|BaseCompartmentSlot|BaseCompartmentManagerComponent|int) (m_\w+);','$1public $2 $3;')
$code=@'
using System;
using System.Collections.Generic;
public class Native { public static implicit operator bool(Native value){return value!=null;} }
public static class Replication {public static bool Server=true;public static bool IsServer(){return Server;}}
public class RplId {public int Value=1;public bool IsValid(){return Value>0;}public override string ToString(){return Value.ToString();}}
public class RplComponent:Native {public bool Master=true,Proxy;public RplId R=new RplId();public bool IsMaster(){return Master;}public bool IsProxy(){return Proxy;}public RplId Id(){return R;}public static RplComponent Cast(object value){return value as RplComponent;}}
public class BaseWorld:Native {public Dictionary<long,IEntity> Entities=new Dictionary<long,IEntity>();public IEntity FindEntityByID(long id){IEntity v;return Entities.TryGetValue(id,out v)?v:null;}}
public class IEntity:Native {public BaseWorld World;public long Id;public RplComponent Rpl=new RplComponent();public BaseCompartmentManagerComponent Manager;public FactionAffiliationComponent Affiliation;public IEntity Child,Sibling,Parent;public BaseWorld GetWorld(){return World;}public long GetID(){return Id;}public IEntity GetChildren(){return Child;}public IEntity GetSibling(){return Sibling;}public IEntity GetParent(){return Parent;}public object FindComponent(Type type){if(type==typeof(RplComponent))return Rpl;if(type==typeof(FactionAffiliationComponent))return Affiliation;return Manager;}}
public class Faction:Native {public string Key="US";public string GetFactionKey(){return Key;}}
public class FactionAffiliationComponent:Native {public Faction Faction;public Faction GetAffiliatedFaction(){return Faction;}public static FactionAffiliationComponent Cast(object v){return v as FactionAffiliationComponent;}}
public class SCR_AIGroup:IEntity {public Faction Faction=new Faction();public Faction GetFaction(){return Faction;}public static SCR_AIGroup Cast(object v){return v as SCR_AIGroup;}}
public class AIAgent:IEntity {public IEntity Actor;public SCR_AIGroup Group;public bool Activated,Counted;public IEntity GetControlledEntity(){return Actor;}public SCR_AIGroup GetParentGroup(){return Group;}public bool IsAIActivated(){return Activated;}public bool CountTowardsAIWorldLimit(){return Counted;}}
public class AIControlComponent:Native {public AIAgent Agent;public AIAgent GetControlAIAgent(){return Agent;}}
public static class ECharacterLifeState {public const int ALIVE=1,INCAPACITATED=2,DEAD=3;}
public class CharacterControllerComponent:Native {public bool Player;public int Life=1;public bool IsPlayerControlled(){return Player;}public int GetLifeState(){return Life;}}
public class ChimeraCharacter:IEntity {public AIControlComponent Control=new AIControlComponent();public CharacterControllerComponent Controller=new CharacterControllerComponent();public CompartmentAccessComponent Access=new CompartmentAccessComponent();public AIControlComponent GetAIControlComponent(){return Control;}public CharacterControllerComponent GetCharacterController(){return Controller;}public bool IsInVehicle(){return Access.Vehicle!=null;}public CompartmentAccessComponent GetCompartmentAccessComponent(){return Access;}public static ChimeraCharacter Cast(object v){return v as ChimeraCharacter;}}
public class CompartmentAccessComponent:Native {public IEntity Vehicle;public BaseCompartmentSlot Seat;public bool GettingIn,GettingOut,Switching;public BaseCompartmentSlot GetCompartment(){return Seat;}public bool IsGettingIn(){return GettingIn;}public bool IsGettingOut(){return GettingOut;}public bool IsSwitchingSeatsAnim(){return Switching;}public static IEntity GetVehicleIn(IEntity actor){var c=actor as ChimeraCharacter;return c==null?null:c.Access.Vehicle;}}
public class PlayerManager:Native {public int PlayerId;public int GetPlayerIdFromControlledEntity(IEntity e){return PlayerId;}}
public static class SCR_PossessingManagerComponent {public static int MainId;public static int GetPlayerIdFromMainEntity(IEntity e){return MainId;}}
public class Game:Native {public BaseWorld World=new BaseWorld();public PlayerManager Players=new PlayerManager();public BaseWorld GetWorld(){return World;}public PlayerManager GetPlayerManager(){return Players;}}
public static class Runtime {public static Game Game;}
public class BaseCompartmentSlot:Native {public IEntity Vehicle,Owner,Occupant;public BaseCompartmentManagerComponent Manager;public int ManagerId=1,SlotId=2;public IEntity GetVehicle(){return Vehicle;}public IEntity GetOwner(){return Owner;}public IEntity GetOccupant(){return Occupant;}public BaseCompartmentManagerComponent GetManager(){return Manager;}public int GetCompartmentMgrID(){return ManagerId;}public int GetCompartmentSlotID(){return SlotId;}}
public class BaseCompartmentManagerComponent:Native {public List<BaseCompartmentSlot> Seats=new List<BaseCompartmentSlot>();public Action Callback;public static BaseCompartmentManagerComponent Cast(object v){return v as BaseCompartmentManagerComponent;}public void GetCompartments(List<BaseCompartmentSlot> seats){seats.AddRange(Seats);if(Callback!=null)Callback();}}
'@
$code+=$actual
$code+=@'
public static class EntryCases
{
 static int Cases;
 static void Need(bool yes,string why){Cases++;if(!yes)throw new Exception(why);}
 static AICF_AIQuotaVehicleEntry Setup(bool child=false)
 {
  Replication.Server=true;SCR_PossessingManagerComponent.MainId=0;Runtime.Game=new Game();var w=Runtime.Game.World;
  var actor=new ChimeraCharacter{Id=1,World=w};var group=new SCR_AIGroup{Id=2,World=w};var agent=new AIAgent{Id=3,World=w,Actor=actor,Group=group};actor.Control.Agent=agent;
  var car=new IEntity{Id=4,World=w};var owner=child?new IEntity{Id=5,World=w}:car;var manager=new BaseCompartmentManagerComponent();owner.Manager=manager;
  var seat=new BaseCompartmentSlot{Vehicle=car,Owner=owner,Manager=manager};manager.Seats.Add(seat);
  foreach(var e in new IEntity[]{actor,group,agent,car,owner})w.Entities[e.Id]=e;
  var captured=AICF_AIQuotaVehicleEntry.Capture(actor,owner,seat);if(captured==null)throw new Exception("valid capture failed");return captured;
 }
 static void Reject(Action<AICF_AIQuotaVehicleEntry> change,string why){var e=Setup(true);change(e);Need(!e.IsCurrent(),why);}
 public static int Run()
 {
  Cases=0;Need(Setup().IsCurrent(),"root seat/current exact AI");Need(Setup(true).IsCurrent(),"nested compartment owner permitted");
  var e=Setup();e.m_NativeVehicle=e.m_Vehicle;Need(e.IsCurrent(),"native target root accepted");
  Need(AICF_AIQuotaVehicleEntry.Capture(null,e.m_Vehicle,e.m_Seat)==null,"null actor rejected");
  Need(AICF_AIQuotaVehicleEntry.Capture(e.m_Actor,null,e.m_Seat)==null,"null vehicle rejected");
  Need(AICF_AIQuotaVehicleEntry.Capture(e.m_Actor,e.m_Vehicle,null)==null,"null seat rejected");
  Reject(x=>Replication.Server=false,"client scope rejected");Reject(x=>Runtime.Game=null,"world ended");
  Reject(x=>Runtime.Game.World=new BaseWorld(),"different main world");Reject(x=>x.m_Actor.World=new BaseWorld(),"actor private world");
  Reject(x=>x.m_Actor.Id++,"actor id changed");Reject(x=>x.m_ActorIdentity.m_World.Entities.Remove(x.m_Actor.Id),"actor absent from native world");
  Reject(x=>x.m_Actor.Rpl=new RplComponent(),"replaced Rpl component");Reject(x=>x.m_Actor.Rpl.R.Value++,"Rpl epoch changed");
  Reject(x=>x.m_Actor.Rpl.R.Value=0,"invalid Rpl");Reject(x=>x.m_Actor.Rpl.Master=false,"proxy authority");Reject(x=>x.m_Actor.Rpl.Proxy=true,"proxy marker");
  Reject(x=>x.m_Actor.Control=new AIControlComponent{Agent=x.m_Agent},"same agent changed control component");
  Reject(x=>x.m_Control.Agent=new AIAgent{Actor=x.m_Actor,Group=x.m_Group},"current controlling agent changed");
  Reject(x=>x.m_Agent.Actor=new ChimeraCharacter(),"agent controls other actor");Reject(x=>x.m_Agent.Group=new SCR_AIGroup(),"foreign parent group");
  Reject(x=>x.m_Agent.Id++,"agent id changed");Reject(x=>x.m_Agent.World=new BaseWorld(),"agent world mismatch");
  Reject(x=>x.m_Group.Faction=new Faction(),"same group changed faction");Reject(x=>x.m_Group.Faction=null,"missing faction");
  Reject(x=>x.m_Group.Faction.Key="USSR","group faction key changed");Reject(x=>x.m_Group.Faction.Key="","empty group faction key");
  Reject(x=>x.m_Actor.Affiliation=new FactionAffiliationComponent(),"actor affiliation component changed");
  Reject(x=>x.m_Actor.Controller.Life=2,"incap actor");Reject(x=>x.m_Actor.Controller.Life=3,"dead actor");
  Reject(x=>x.m_Actor.Controller.Player=true,"player controller");Reject(x=>Runtime.Game.Players.PlayerId=8,"player controlled mapping");
  Reject(x=>SCR_PossessingManagerComponent.MainId=9,"player main entity");Reject(x=>Runtime.Game.Players=null,"missing player registry");
  Reject(x=>x.m_Seat.Vehicle=new IEntity(),"seat moved to foreign vehicle");Reject(x=>x.m_Seat.Owner=x.m_Vehicle,"seat owner changed");
  Reject(x=>x.m_Seat.Manager=new BaseCompartmentManagerComponent(),"seat manager swapped");Reject(x=>x.m_Seat.ManagerId++,"manager id changed");
  Reject(x=>x.m_Seat.SlotId++,"slot id changed");Reject(x=>x.m_SeatOwnerIdentity.m_Entity.Manager=new BaseCompartmentManagerComponent(),"owner component changed");
  Reject(x=>x.m_Manager.Seats.Clear(),"seat no longer in manager");Reject(x=>x.m_Manager.Seats.Add(x.m_Seat),"duplicate seat entry");
  Reject(x=>x.m_Manager.Seats.Add(null),"null seat snapshot");Reject(x=>{for(int i=0;i<256;i++)x.m_Manager.Seats.Add(new BaseCompartmentSlot());},"truncated seat snapshot");
  Reject(x=>x.m_NativeVehicle=new IEntity(),"unrelated native vehicle");Reject(x=>x.m_Vehicle.Rpl.R.Value++,"car replication identity changed");
  Reject(x=>x.m_Manager.Callback=()=>x.m_Control.Agent=new AIAgent(),"control callback after enumeration");
  Reject(x=>x.m_Manager.Callback=()=>x.m_Actor.Controller.Player=true,"human takeover callback");
  Reject(x=>x.m_Manager.Callback=()=>x.m_Seat=null,"weak seat cleared during enumeration");
  Reject(x=>x.m_Manager.Callback=()=>x.m_Seat.ManagerId++,"seat callback after enumeration");
  Reject(x=>x.m_Manager.Callback=()=>x.m_Group.Faction=new Faction(),"faction callback");
  e=Setup(true);e.m_NativeVehicle=e.m_Vehicle;Need(e.IsCurrent(),"root vehicle accepted for child seat");
  e=Setup(true);e.m_Actor.Affiliation=new FactionAffiliationComponent{Faction=new Faction{Key="CIV"}};
  e=AICF_AIQuotaVehicleEntry.Capture(e.m_Actor,e.m_NativeVehicle,e.m_Seat);Need(e!=null&&e.IsCurrent(),"actor affiliation captured independently, no invented actor/group equality");
  e.m_ActorFaction.Key="USSR";Need(!e.IsCurrent(),"actor affiliation key changed");
  e=Setup(true);e.m_Actor.Affiliation=new FactionAffiliationComponent{Faction=new Faction{Key="CIV"}};
  e=AICF_AIQuotaVehicleEntry.Capture(e.m_Actor,e.m_NativeVehicle,e.m_Seat);e.m_Actor.Affiliation.Faction=new Faction{Key="CIV"};Need(!e.IsCurrent(),"same faction key with replaced actor faction pointer");
  // Vehicle has deliberately no faction API. CIV/captured car identity does not
  // infer quota ownership from driver faction; service alone classifies cost.
  Need(Setup(true).IsCurrent(),"vehicle faction inference absent");return Cases;
 }
}
'@
Add-Type -TypeDefinition $code -Language CSharp
$cases=[EntryCases]::Run()
"PASS AI quota normal vehicle entry actual_extracted_cases=$cases read_only=1 quota_admission=NOT_CONNECTED native=NOT_RUN"
