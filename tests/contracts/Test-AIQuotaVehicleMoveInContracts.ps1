param([string]$RepositoryRoot=(Split-Path -Parent (Split-Path -Parent $PSScriptRoot)))
$ErrorActionPreference='Stop'
$root=Join-Path $RepositoryRoot 'AIConflictCore/Scripts/Game/AIConflict/Integration'
$source=[IO.File]::ReadAllText((Join-Path $root 'AICF_AIQuotaVehicleMoveIn.c'))
$pinned=Join-Path $RepositoryRoot '.cache/reforger-api/Arma-Reforger-Script-Diff-1.8.0.13/scripts/Game/Vehicle/SCR_CompartmentAccessComponent.c'
$native=[IO.File]::ReadAllText($pinned)
foreach($token in @('bool MoveInVehicle(IEntity vehicle, ECompartmentType compartmentType, bool performWhenPaused = false, BaseCompartmentSlot customSlot = null)','protected void MoveInVehicleOwner(RplId slotEntityID, int slotID, bool performWhenPaused)','CallLater(MoveInVehicleOwner, 1000, false','slot.SetCompartmentAccessible(false)')){
 if(!$native.Contains($token)){throw ('Pinned MoveIn seam drift: '+$token)}
}
if($source-match'override\s+bool\s+GetInVehicle\('){throw 'Native GetInVehicle is not a supported script override'}
$actual=$source.Replace('ref ','').Replace('GetGame()','Runtime.Game').Replace('super.','base.').Replace('protected ','public ')
$actual=$actual.Replace('modded class SCR_CompartmentAccessComponent','public class SCR_CompartmentAccessComponent:AccessBase')
$actual=[regex]::Replace($actual,'(?m)^(\s*)override ','$1public override ')
$actual=$actual.Replace('public override public ','public override ')
$actual=$actual.Replace('FindComponent(RplComponent)','FindComponent(typeof(RplComponent))').Replace('FindComponent(BaseCompartmentManagerComponent)','FindComponent(typeof(BaseCompartmentManagerComponent))')
$actual=$actual.Replace('RplComponent rpl;','RplComponent rpl = null;').Replace('BaseCompartmentManagerComponent manager;','BaseCompartmentManagerComponent manager = null;').Replace('BaseCompartmentSlot seat;','BaseCompartmentSlot seat = null;')
$replay=@'
using System;using System.Collections.Generic;
namespace QuotaMoveInReplay {
public class N{public static implicit operator bool(N o){return o!=null;}}
public class RplId{public int Value;public bool IsValid(){return Value>0;}public override string ToString(){return Value.ToString();}}
public class RplComponent:N{public RplId Value=new RplId{Value=4};public IEntity Entity;public Action OnId;public static RplComponent Cast(object o){return o as RplComponent;}public RplId Id(){if(OnId!=null)OnId();return Value;}public IEntity GetEntity(){return Entity;}}
public class IEntity:N{public RplComponent Rpl;public BaseCompartmentManagerComponent Manager;public object FindComponent(Type t){return t==typeof(RplComponent)?(object)Rpl:Manager;}}
public class BaseCompartmentSlot:N{public IEntity Owner;public int Id=7;public IEntity GetOwner(){return Owner;}public int GetCompartmentSlotID(){return Id;}}
public class BaseCompartmentManagerComponent:N{public BaseCompartmentSlot Seat;public Action OnFind;public static BaseCompartmentManagerComponent Cast(object o){return o as BaseCompartmentManagerComponent;}public BaseCompartmentSlot FindCompartment(int id){if(OnFind!=null)OnFind();return Seat!=null&&Seat.Id==id?Seat:null;}}
public enum ECompartmentType{PILOT,CARGO}public static class ECloseDoorAfterActions{public const int INVALID=0;}
public class Callqueue:N{public int Added,Removed;public Action<RplId,int,bool> Pending;public RplId Rpl;public int Slot;public bool Paused;public void CallLater(Action<RplId,int,bool> callback,int ms,bool repeat,RplId id,int slot,bool paused){Added++;Pending=callback;Rpl=id;Slot=slot;Paused=paused;}public void Remove(Action<RplId,int,bool> callback){Removed++;if(Pending==callback)Pending=null;}public void Run(){var p=Pending;Pending=null;if(p!=null)p(Rpl,Slot,Paused);}}
public class Game:N{public Callqueue Q=new Callqueue();public Callqueue GetCallqueue(){return Q;}}public static class Runtime{public static Game Game=new Game();}
public static class Replication{public static RplComponent Found;public static object FindItem(RplId id){return Found!=null&&Found.Value.Value==id.Value?Found:null;}}
public class AICF_AIQuotaVehicleDispatch:N{
 public static bool Policy=true,Allowed=true,Current=true;public static int Begins,Cancels,Marks,Results;public static bool Accepted;public static Action OnBegin;public IEntity Actor,Car;public BaseCompartmentSlot Seat;public bool Ended;
 public static N FindPolicy(IEntity actor){return Policy?new N():null;}public static AICF_AIQuotaVehicleDispatch Begin(IEntity actor,IEntity car,BaseCompartmentSlot seat){Begins++;if(OnBegin!=null)OnBegin();return Allowed&&actor!=null&&car!=null&&seat!=null?new AICF_AIQuotaVehicleDispatch{Actor=actor,Car=car,Seat=seat}:null;}
 public bool IsCurrent(){return Current&&!Ended;}public bool Matches(IEntity actor,IEntity car,BaseCompartmentSlot seat){return IsCurrent()&&Actor==actor&&Car==car&&Seat==seat;}public void MarkDispatched(){if(!Ended)Marks++;}public void ObserveNativeResult(bool result){Results++;Accepted=result;}public void Cancel(){if(!Ended){Ended=true;Cancels++;}}
 public static void Reset(){Policy=true;Allowed=true;Current=true;Begins=0;Cancels=0;Marks=0;Results=0;Accepted=false;OnBegin=null;}
}
public class AccessBase:N{
 public IEntity Actor;public BaseCompartmentSlot Selected;public int SuperMoves,SuperOwners,Physical,Deletes;public bool Accepted=true,Return=true,Immediate=true;public static int s_iWaitForVehicleTries;public const int WAIT_FOR_VEHICLE_MAX_TRIES=10;
 public IEntity GetOwner(){return Actor;}public BaseCompartmentSlot FindFreeAndAccessibleCompartment(IEntity car,ECompartmentType type,BaseCompartmentSlot custom){return custom??Selected;}
 public virtual bool MoveInVehicle(IEntity vehicle,ECompartmentType type,bool paused=false,BaseCompartmentSlot custom=null){SuperMoves++;var chosen=FindFreeAndAccessibleCompartment(vehicle,type,custom);if(Return&&Immediate&&chosen!=null)MoveInVehicleOwner(chosen.Owner.Rpl.Value,chosen.Id,paused);return Return;}
 public virtual void MoveInVehicleOwner(RplId id,int slot,bool paused){SuperOwners++;}public bool GetInVehicle(IEntity car,BaseCompartmentSlot slot,bool tp,int door,int close,bool paused){Physical++;return Accepted;}public virtual void OnDelete(IEntity owner){Deletes++;}
}
'@
$replay+=$actual
$replay+=@'
public static class Cases{
 static int Count;static SCR_CompartmentAccessComponent A;static IEntity Car;static BaseCompartmentSlot Seat;
 static void Need(bool yes,string why){Count++;if(!yes)throw new Exception(why);}
 static void Setup(){AICF_AIQuotaVehicleDispatch.Reset();Runtime.Game=new Game();AccessBase.s_iWaitForVehicleTries=0;Car=new IEntity();Seat=new BaseCompartmentSlot{Owner=Car};Car.Manager=new BaseCompartmentManagerComponent{Seat=Seat};Car.Rpl=new RplComponent{Entity=Car};Replication.Found=Car.Rpl;A=new SCR_CompartmentAccessComponent{Actor=new IEntity(),Selected=Seat};}
 public static int Run(){Count=0;
 Setup();Need(A.MoveInVehicle(Car,ECompartmentType.PILOT)&&A.Physical==1&&AICF_AIQuotaVehicleDispatch.Results==1&&AICF_AIQuotaVehicleDispatch.Accepted,"synchronous actual owner path uses one permit and actual bool");Need(AICF_AIQuotaVehicleDispatch.Begins==1&&AICF_AIQuotaVehicleDispatch.Cancels==1,"one current request/deduped claim");
 Setup();AICF_AIQuotaVehicleDispatch.Allowed=false;Need(!A.MoveInVehicle(Car,ECompartmentType.PILOT)&&A.SuperMoves==0&&A.Physical==0,"quota veto before native outer lock/RPC");
 Setup();AICF_AIQuotaVehicleDispatch.Policy=false;Need(A.MoveInVehicle(Car,ECompartmentType.PILOT)&&A.SuperMoves==1&&A.SuperOwners==1&&AICF_AIQuotaVehicleDispatch.Begins==0,"human/inactive policy delegates whole native path");
 Setup();A.Accepted=false;Need(A.MoveInVehicle(Car,ECompartmentType.PILOT)&&A.Physical==1&&AICF_AIQuotaVehicleDispatch.Results==1&&!AICF_AIQuotaVehicleDispatch.Accepted,"native bool false observed without fabricated acceptance");
 Setup();A.Return=false;Need(!A.MoveInVehicle(Car,ECompartmentType.PILOT)&&A.Physical==0&&AICF_AIQuotaVehicleDispatch.Cancels==1,"failed outer dispatch cancels unused permit");
 Setup();AICF_AIQuotaVehicleDispatch.OnBegin=()=>AICF_AIQuotaVehicleDispatch.Current=false;Need(!A.MoveInVehicle(Car,ECompartmentType.PILOT)&&A.Physical==0&&A.SuperMoves==0,"post Begin callback stale blocks native");
 Setup();A.Immediate=false;A.MoveInVehicle(Car,ECompartmentType.PILOT);A.MoveInVehicleOwner(new RplId{Value=99},Seat.Id,false);Need(A.Physical==0&&A.m_AICFMoveInQuota!=null,"stale Rpl tuple cannot borrow successor permit");A.MoveInVehicleOwner(Car.Rpl.Value,Seat.Id+1,false);Need(A.Physical==0&&A.m_AICFMoveInQuota!=null,"stale seat tuple cannot cancel successor permit");
 Setup();A.Immediate=false;A.MoveInVehicle(Car,ECompartmentType.PILOT);AICF_AIQuotaVehicleDispatch.Current=false;A.MoveInVehicleOwner(Car.Rpl.Value,Seat.Id,false);Need(A.Physical==0&&A.m_AICFMoveInQuota==null&&AICF_AIQuotaVehicleDispatch.Cancels==1,"late stale quota closes captured source without native");
 Setup();A.Immediate=false;A.MoveInVehicle(Car,ECompartmentType.PILOT);Replication.Found=null;A.MoveInVehicleOwner(Car.Rpl.Value,Seat.Id,false);Need(Runtime.Game.Q.Added==1&&A.Physical==0,"missing Rpl bounded own retry no physical");Replication.Found=Car.Rpl;Runtime.Game.Q.Run();Need(A.Physical==1&&AICF_AIQuotaVehicleDispatch.Begins==1,"retry retains exact permit and guarded path");
 Setup();A.Immediate=false;A.MoveInVehicle(Car,ECompartmentType.PILOT);Replication.Found=null;AccessBase.s_iWaitForVehicleTries=10;A.MoveInVehicleOwner(Car.Rpl.Value,Seat.Id,false);Need(Runtime.Game.Q.Added==0&&A.Physical==0&&A.m_AICFMoveInQuota==null,"pinned10 retry cap preserved");
 Setup();A.Immediate=false;A.MoveInVehicle(Car,ECompartmentType.PILOT);Replication.Found=null;A.MoveInVehicleOwner(Car.Rpl.Value,Seat.Id,false);A.OnDelete(A.Actor);Need(Runtime.Game.Q.Pending==null&&A.m_AICFMoveInQuota==null&&A.Deletes==1,"balanced own/native retry remove +source cancel on delete");
 Setup();A.Immediate=false;A.MoveInVehicle(Car,ECompartmentType.PILOT);Car.Manager.OnFind=()=>AICF_AIQuotaVehicleDispatch.Current=false;A.MoveInVehicleOwner(Car.Rpl.Value,Seat.Id,false);Need(A.Physical==0,"manager callback quota drift blocked");
 Setup();A.Immediate=false;A.MoveInVehicle(Car,ECompartmentType.PILOT);Car.Rpl.OnId=()=>AICF_AIQuotaVehicleDispatch.Current=false;A.MoveInVehicleOwner(Car.Rpl.Value,Seat.Id,false);Need(A.Physical==0,"last Rpl native getter callback drift blocked before physical");
 return Count;}
}
}
'@
Add-Type -TypeDefinition $replay -Language CSharp
$count=[QuotaMoveInReplay.Cases]::Run()
$mutants=@(
 @{name='LastNativeGetter';from='if (!quota.IsCurrent() || GetOwner() != actor || !quota.IsCurrent())';to='if (quota == null)'},
 @{name='OldSlotRPC';from=' || slotID != m_iAICFMoveInSlot';to=''},
 @{name='RetryCap';from='s_iWaitForVehicleTries >= WAIT_FOR_VEHICLE_MAX_TRIES';to='s_iWaitForVehicleTries > WAIT_FOR_VEHICLE_MAX_TRIES'},
 @{name='InventedAccepted';from='quota.ObserveNativeResult(accepted);';to='quota.ObserveNativeResult(true);'},
 @{name='UnbalancedRetry';from='Runtime.Game.GetCallqueue().Remove(AICF_RetryQuotaMoveInOwner);';to='/* missing own retry cleanup */'}
)
$caught=0
foreach($m in $mutants){
 if(!$replay.Contains($m.from)){throw ('Mutation source absent '+$m.name)}
 $ns='QuotaMoveInUnsafe'+$m.name
 Add-Type -TypeDefinition ($replay.Replace('QuotaMoveInReplay',$ns).Replace($m.from,$m.to)) -Language CSharp
 $type=($ns+'.Cases')-as[type];$bad=$false
 try{[void]$type::Run()}catch{$bad=$true}
 if(!$bad){throw ('Unsafe MoveIn mutation survived '+$m.name)};$caught++
}
"PASS quota MoveIn actual_adapter_cases=$count pinned_seams=4 unsafe_executed=$caught native_service_accounting=NOT_TESTED native=NOT_RUN"
