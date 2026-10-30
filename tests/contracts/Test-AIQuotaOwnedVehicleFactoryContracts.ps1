param([string]$RepositoryRoot=(Split-Path -Parent (Split-Path -Parent $PSScriptRoot)))
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot '../lib/Stage3StaticAudit.Common.ps1')
$prefix=Join-Path $RepositoryRoot 'AIConflictCore/Scripts/Game/AIConflict'
$common=[IO.File]::ReadAllText((Join-Path $prefix 'Forces/AICF_AIQuotaVehicleCreationGuard.c'))
$trip=[IO.File]::ReadAllText((Join-Path $prefix 'Forces/AICF_AIQuotaTripVehicleCreation.c'))
$fia=[IO.File]::ReadAllText((Join-Path $prefix 'Forces/AICF_AIQuotaFIAVehicleCreation.c'))
$spawner=[IO.File]::ReadAllText((Join-Path $prefix 'Vehicles/AICF_VehicleSpawner.c'))
function Body([string]$source,[string]$name){Get-AICFMethodBody ([pscustomobject]@{Source=$source;Code=(ConvertTo-AICFCodeText $source)}) $name}
$factory=Body $spawner 'SpawnSelectedPrefab'
$mark=$factory.IndexOf('MarkExternalNativeCallStarted(creation, true)')
$raw=$factory.IndexOf('IEntity raw = GetGame().SpawnEntityPrefabEx')
$observe=$factory.IndexOf('ObserveExternalFactory(creation, raw)')
if($mark-lt0-or$raw-lt$mark-or$observe-lt$raw){throw 'Raw factory identity must be observed before Vehicle.Cast'}
if($factory.Substring($mark,$raw-$mark)-match 'Diagnostics|GetOrigin|FindComponent|CommitFactoryAttempt'){throw 'No callbacks after atomic attempt commit'}
if(!(Body $common 'CancelAvailable').Contains('!m_bReservationAdopted')){throw 'Adopted driver reservation may not be refunded by producer'}
if(!(Body $common 'OwnsVehicleLease').Contains('m_bBound')){throw 'An arbitrary fleet owner must not adopt provisional vehicle custody'}
function CompileActual([string]$commonText,[string]$tripText,[string]$factoryText){
 $source=$commonText+"`n"+$tripText+"`n"+$fia
 $source=$source.Replace('ref ','')
 $source=[regex]::Replace($source,'\bFactionKey\b|\bResourceName\b','string')
 $source=[regex]::Replace($source,'\bEntityID\b','int')
 $source=[regex]::Replace($source,'(?m)^class (\w+)(\s*:\s*[^\r\n]+)?\s*$',{param($m) if($m.Groups[2].Success){'public class '+$m.Groups[1].Value+$m.Groups[2].Value}else{'public class '+$m.Groups[1].Value+' : N'}})
 $source=$source.Replace('protected ','public ')
 $source=[regex]::Replace($source,'(?m)^(\s*)(AICF_\w+|int|bool|string) (m_\w+);','$1public $2 $3;')
 $source=[regex]::Replace($source,'(?m)^(\s*)(override\s+)?(static\s+)?(bool|void|AICF_\w+|string) (\w+)\(','$1public $2$3$4 $5(')
 $source=$source.Replace('public public ','public ').Replace('void ~AICF_AIQuotaVehicleCreationGuard()','public void Destroy()')
 $source=$source.Replace('public bool IsDomainCurrent()','public virtual bool IsDomainCurrent()')
 $factoryText=[regex]::Replace($factoryText,'\bparams\b','spawn')
 $factoryText=$factoryText.Replace('AICF_AIQuotaCreation creation;','AICF_AIQuotaCreation creation = null;')
 $adapters=@'
using System;using System.Collections.Generic;
public class N {public static implicit operator bool(N n){return n!=null;}public static Game GetGame(){return Env.Game;}}
public static class StringEx {public static bool IsEmpty(this string s){return string.IsNullOrEmpty(s);}}
public enum AICF_EAIQuotaBucket{STOCK,FLEET,COMBAT,FIA_EXTRA}
public enum AICF_EAIQuotaBirthStatus{IN_NATIVE_FACTORY,NATIVE_NULL,BOUND,QUARANTINED}
public enum AICF_EVehicleLeaseState{RESERVED,ACTIVE,RELEASE_PENDING}
public enum AICF_ETransportTripPhase{SPAWN_COMMIT,BOARDING}
public static class Clock{public static int Now=1000;}
public static class System{public static int GetTickCount(){return Clock.Now;}}
public class World:N{}
public class IEntity:N{public int Id=1;public string Rpl="rpl1";public bool Current=true;public int GetID(){return Id;}public World GetWorld(){return Env.Game.World;}}
public class Vehicle:IEntity{public static Vehicle Cast(IEntity e){return e as Vehicle;}}
public class SCR_AIGroup:IEntity{}
public class Game:N{public World World=new World();public int NativeCalls;public IEntity Result=new Vehicle();public Action OnNative;public World GetWorld(){return World;}public IEntity SpawnEntityPrefabEx(string p,bool b,EntitySpawnParams spawn){NativeCalls++;if(OnNative!=null)OnNative();return Result;}}
public static class Replication{public static bool IsServer(){return true;}}
public static class ETransformMode{public const int WORLD=1;}
public class EntitySpawnParams:N{public int TransformMode;public int[] Transform=new int[4];}
public class AICF_MatchController:N{public bool TripCurrent=true,FIAOwner=true;public bool IsAIQuotaVehicleTripCurrent(AICF_TransportTrip t){return TripCurrent;}public bool OwnsAIQuotaGarrison(AICF_FIAGarrison g){return FIAOwner&&g==Env.Garrison;}public bool OwnsAIQuotaPatrol(AICF_FIAPatrol p){return FIAOwner&&p==Env.Patrol;}}
public class SCR_CampaignFaction:N{public string Key="FIA";public string GetFactionKey(){return Key;}}
public class SCR_CampaignMilitaryBaseComponent:N{public IEntity Owner=new IEntity();public SCR_CampaignFaction Faction=new SCR_CampaignFaction();public IEntity GetOwner(){return Owner;}public SCR_CampaignFaction GetFaction(){return Faction;}}
public class AICF_FIAGarrison:N{public int m_iGeneration=1,m_BaseId=1;public bool m_bRetired;public SCR_CampaignMilitaryBaseComponent m_Base=new SCR_CampaignMilitaryBaseComponent();public SCR_CampaignFaction m_Faction;public AICF_VehicleLease m_Lease;public bool BaseIdentity(){return m_Base!=null&&m_Base.Owner.Id==m_BaseId;}}
public class AICF_FIAPatrol:N{public int m_iGeneration=1;public bool m_bRetired;public SCR_CampaignMilitaryBaseComponent m_Current=new SCR_CampaignMilitaryBaseComponent();public SCR_CampaignFaction m_Faction;public AICF_VehicleLease m_Lease;}
public class AICF_AIQuotaCreationGuard:N{public virtual bool IsCurrent(){return false;}public virtual string GetFactionKey(){return "";}public virtual AICF_EAIQuotaBucket GetBucket(){return AICF_EAIQuotaBucket.STOCK;}public virtual bool ExpectsVehicle(){return false;}public virtual bool OwnsVehicleLease(AICF_VehicleLease l,IEntity e){return false;}public virtual bool CommitNativeAttempt(){return true;}}
public class AICF_AIQuotaReservation:N{public int Available;}
public class AICF_AIQuotaBody:N{public IEntity Entity;public bool MatchesIdentity(){return Entity!=null&&Entity.Current;}}
public class AICF_AIQuotaCreation:N{public AICF_AIWorldQuotaService m_Service;public AICF_AIQuotaCreationGuard m_Guard;public IEntity m_Entity;public AICF_AIQuotaBody m_Body;public AICF_EAIQuotaBirthStatus m_Status;}
public class AICF_AIQuotaCatalogIdentity:N{public IEntity m_Entity;public static AICF_AIQuotaCatalogIdentity Capture(IEntity e,bool master){return e==null?null:new AICF_AIQuotaCatalogIdentity{m_Entity=e};}public bool IsCurrent(){return m_Entity.Current;}}
public class AICF_AIQuotaVehicleEntity:N{public IEntity m_Entity;public int m_Id;public string m_sRpl;public static AICF_AIQuotaVehicleEntity Capture(IEntity e){if(!(e is Vehicle)||!e.Current)return null;return new AICF_AIQuotaVehicleEntity{m_Entity=e,m_Id=e.Id,m_sRpl=e.Rpl};}public bool IsCurrent(){return m_Entity.Current&&m_Entity.Id==m_Id&&m_Entity.Rpl==m_sRpl;}}
public class AICF_VehicleLease:N{public int LeaseGeneration=1,VehicleGeneration;public string Lifecycle="";public Vehicle Car;public AICF_EVehicleLeaseState State;public bool HasPhysicalAsset(){return Car!=null&&State==AICF_EVehicleLeaseState.ACTIVE;}public AICF_EVehicleLeaseState GetState(){return State;}public int GetVehicleGeneration(){return VehicleGeneration;}public int GetLeaseGeneration(){return LeaseGeneration;}public string GetVehicleLifecycleId(){return Lifecycle;}public bool MatchesEntityIdentity(Vehicle v,int id,string rpl){return v!=null&&v==Car&&v.Id==id&&v.Rpl==rpl;}}
public class AICF_FactionFleet:N{public AICF_VehicleLease Lease=new AICF_VehicleLease();public bool Current=true;public bool OwnsVehicleLease(AICF_VehicleLease l){return Current&&l==Lease;}public bool Bind(Vehicle v){if(!Current||Lease.HasPhysicalAsset())return false;Lease.Car=v;Lease.VehicleGeneration=7;Lease.Lifecycle="accepted7";Lease.State=AICF_EVehicleLeaseState.ACTIVE;return true;}}
public static class AICF_AIQuotaVehicleOwner{public static string Fingerprint(AICF_VehicleLease l){return l.LeaseGeneration+"/"+l.VehicleGeneration+"/"+l.Lifecycle+"/"+l.State;}}
public class AICF_AIQuotaGroupOwner:N{}
public class AICF_AIWorldQuotaService:N{
 public AICF_MatchController Controller=new AICF_MatchController();public int Epoch=1,Serial,Reserved,Cancels,Adopts,Ends,Rejects,Marks;public bool Current=true,Configured=true,Admission=true,MarkAllowed=true,AdoptAllowed=true;public Action OnReserve;
 public static AICF_AIWorldQuotaService GetAuthorityForWorld(World world){return Env.Service;}
 public bool IsConfigured(){return Configured;}public bool IsCurrentWorld(){return Current;}public AICF_MatchController GetQuotaController(){return Controller;}public int GetEpoch(){return Epoch;}public int AllocateContextSerial(){return ++Serial;}
 public AICF_AIQuotaReservation ReserveUnits(AICF_EAIQuotaBucket bucket,int units,int serial){if(!Admission)return null;Reserved+=units;var r=new AICF_AIQuotaReservation{Available=units};if(OnReserve!=null)OnReserve();return r;}
 public bool CancelReservation(AICF_AIQuotaReservation r,int serial){Cancels+=r.Available;r.Available=0;return true;}
 public AICF_AIQuotaCreation BeginExternalCreation(AICF_AIQuotaReservation r,int serial,string prefab,IEntity producer,bool header,AICF_AIQuotaCreationGuard guard){if(!Admission||!guard.IsCurrent()||r.Available<=0)return null;return new AICF_AIQuotaCreation{m_Service=this,m_Guard=guard};}
 public bool MarkExternalNativeCallStarted(AICF_AIQuotaCreation c,bool commit){if(!MarkAllowed||!c.m_Guard.IsCurrent())return false;if(commit&&!c.m_Guard.CommitNativeAttempt())return false;Marks++;return true;}
 public bool CancelExternalCreationBeforeFactory(AICF_AIQuotaCreation c){Ends++;return true;}
 public void ObserveExternalFactory(AICF_AIQuotaCreation c,IEntity raw){c.m_Entity=raw;if(raw==null){c.m_Status=AICF_EAIQuotaBirthStatus.NATIVE_NULL;return;}c.m_Body=new AICF_AIQuotaBody{Entity=raw};c.m_Status=(raw is Vehicle)&&c.m_Guard.IsCurrent()?AICF_EAIQuotaBirthStatus.BOUND:AICF_EAIQuotaBirthStatus.QUARANTINED;}
 public bool EndExternalCreation(AICF_AIQuotaCreation c){Ends++;return true;}public void RejectExternalCreation(AICF_AIQuotaCreation c,string why){Rejects++;c.m_Status=AICF_EAIQuotaBirthStatus.QUARANTINED;}
 public bool AdoptExternalGroupReservation(SCR_AIGroup g,AICF_AIQuotaGroupOwner owner,AICF_AIQuotaReservation r,int serial){if(!AdoptAllowed)return false;Adopts++;return true;}
}
public class AICF_VehicleSpawnPlan:N{public bool Valid=true;public bool IsValid(){return Valid;}}
public class AICF_VehicleRequestState:N{public int Generation=1,Attempts,Total,Deadline=9000;public AICF_VehicleSpawnPlan Plan=new AICF_VehicleSpawnPlan();public int GetRequestGeneration(){return Generation;}public AICF_VehicleSpawnPlan GetSpawnPlan(){return Plan;}public int GetAbsoluteDeadlineMs(){return Deadline;}public bool IsDeadlineReached(int now){return now>=Deadline;}public int GetAttemptCount(){return Attempts;}public int GetTotalAttemptCount(){return Total;}public bool CanAttempt(int now){return now<Deadline&&Attempts<4;}public bool BeginAttempt(int now){if(!CanAttempt(now))return false;Attempts++;Total++;return true;}}
public class Assignment:N{public SCR_AIGroup Group=new SCR_AIGroup();public SCR_AIGroup GetGroup(){return Group;}}
public class AICF_TransportTrip:N{public AICF_VehicleRequestState Request=new AICF_VehicleRequestState();public Assignment Assignment=new Assignment();public int TripGeneration=1,GroupGeneration=1,Transitions;public AICF_ETransportTripPhase Phase;public AICF_ETransportTripPhase GetPhase(){return Phase;}public int GetTransitionCount(){return Transitions;}public AICF_VehicleRequestState GetRequestState(){return Request;}public Assignment GetAssignment(){return Assignment;}public int GetTripGeneration(){return TripGeneration;}public int GetGroupGeneration(){return GroupGeneration;}public string GetFactionKey(){return "US";}}
public static class Env{public static Game Game;public static AICF_AIWorldQuotaService Service;public static AICF_TransportTrip Trip;public static AICF_FactionFleet Fleet;public static AICF_FIAGarrison Garrison;public static AICF_FIAPatrol Patrol;public static AICF_AIQuotaTripVehicleCreation Fresh(){Clock.Now=1000;Game=new Game();Service=new AICF_AIWorldQuotaService();Trip=new AICF_TransportTrip();Fleet=new AICF_FactionFleet();Garrison=new AICF_FIAGarrison{m_Lease=Fleet.Lease};Garrison.m_Faction=Garrison.m_Base.Faction;Patrol=new AICF_FIAPatrol{m_Lease=Fleet.Lease};Patrol.m_Faction=Patrol.m_Current.Faction;return AICF_AIQuotaTripVehicleCreation.Capture(Trip,Trip.Request);}}
__SOURCE__
public class Factory:N{public Vehicle SpawnSelectedPrefab(string prefab,int position,EntitySpawnParams spawnParams=null,AICF_AIQuotaVehicleCreationGuard quotaGuard=null){__FACTORY__}}
public class TestGuard:AICF_AIQuotaVehicleCreationGuard{public override bool IsDomainCurrent(){return true;}}
public static class Cases{
 static int Count;static void Need(bool yes,string name){Count++;if(!yes)throw new Exception(name);}static Vehicle Spawn(AICF_AIQuotaTripVehicleCreation g){return new Factory().SpawnSelectedPrefab("car",0,null,g);}
 public static int Run(){Count=0;
 var g=Env.Fresh();Need(g!=null&&Env.Service.Reserved==1&&Env.Trip.Request.Attempts==0,"full1 reserve before lease and attempt");Need(g.GetBucket()==AICF_EAIQuotaBucket.FLEET,"managed vehicle bucket");Need(g.AttachPendingLease(Env.Fleet,Env.Fleet.Lease)&&g.PrepareAttempt(1000),"exact pending lease and attempt prepared");var car=Spawn(g);Need(car!=null&&Env.Game.NativeCalls==1&&Env.Trip.Request.Attempts==1,"native creation spends exactly one attempt");Need(!g.OwnsVehicleLease(Env.Fleet.Lease,car)&&!g.RecordOwnLeaseBind(car),"unbound receipt cannot release provisional custody");Need(Env.Fleet.Bind(car)&&g.RecordOwnLeaseBind(car)&&g.OwnsVehicleLease(Env.Fleet.Lease,car),"actual own bind receipt releases custody");g.CancelAvailable();Need(g.OwnsVehicleLease(Env.Fleet.Lease,car),"request teardown preserves accepted bind proof");Need(!g.OwnsVehicleLease(new AICF_VehicleLease(),car)&&!g.OwnsVehicleLease(Env.Fleet.Lease,new Vehicle()),"foreign lease or car cannot consume receipt");Env.Fleet.Lease.VehicleGeneration++;Need(!g.OwnsVehicleLease(Env.Fleet.Lease,car),"ABA generation invalidates bind receipt");
 g=Env.Fresh();g.AttachPendingLease(Env.Fleet,Env.Fleet.Lease);g.PrepareAttempt(1000);Env.Service.MarkAllowed=false;Need(Spawn(g)==null&&Env.Trip.Request.Attempts==0&&Env.Game.NativeCalls==0&&!g.WasNativeNull(),"quota veto zero native and attempt, not native NULL");
 g=Env.Fresh();g.AttachPendingLease(Env.Fleet,Env.Fleet.Lease);g.PrepareAttempt(1000);Env.Trip.Request.Total++;Need(Spawn(g)==null&&Env.Trip.Request.Attempts==0&&Env.Game.NativeCalls==0,"foreign attempt mutation cannot be overwritten");
 g=Env.Fresh();g.AttachPendingLease(Env.Fleet,Env.Fleet.Lease);g.PrepareAttempt(1000);Env.Game.Result=null;Need(Spawn(g)==null&&g.WasNativeNull()&&!g.IsQuarantined()&&Env.Trip.Request.Attempts==1,"genuine native NULL still spends one actual factory attempt");Need(Spawn(g)==null&&Env.Trip.Request.Attempts==1&&Env.Game.NativeCalls==2,"multiple catalog candidates share one attempt receipt");
 g=Env.Fresh();g.AttachPendingLease(Env.Fleet,Env.Fleet.Lease);g.PrepareAttempt(1000);Env.Game.Result=new IEntity();Need(Spawn(g)==null&&g.IsQuarantined()&&!g.WasNativeNull()&&Env.Service.Rejects==1,"wrong NONNULL type quarantined, never fake native NULL");Need(Spawn(g)==null&&Env.Game.NativeCalls==1,"quarantined newborn cannot retry factory");
 g=Env.Fresh();g.AttachPendingLease(Env.Fleet,Env.Fleet.Lease);g.PrepareAttempt(1000);Env.Game.OnNative=()=>Env.Trip.TripGeneration++;Need(Spawn(g)==null&&g.IsQuarantined()&&Env.Service.Rejects==1,"constructor changes exact trip identity, raw body retained by service quarantine");
 g=Env.Fresh();g.AttachPendingLease(Env.Fleet,Env.Fleet.Lease);g.PrepareAttempt(1000);Env.Trip.Request.Generation++;Need(Spawn(g)==null&&Env.Game.NativeCalls==0&&Env.Trip.Request.Attempts==0,"stale request cannot borrow old attempt");
 g=Env.Fresh();g.AttachPendingLease(Env.Fleet,Env.Fleet.Lease);g.PrepareAttempt(1000);Env.Fleet.Lease.LeaseGeneration++;Need(Spawn(g)==null&&Env.Game.NativeCalls==0,"pending lease fingerprint rechecked");
 g=Env.Fresh();g.AttachPendingLease(Env.Fleet,Env.Fleet.Lease);g.PrepareAttempt(1000);Env.Service.Epoch++;Need(Spawn(g)==null&&Env.Game.NativeCalls==0,"world epoch replacement veto");
 g=Env.Fresh();g.AttachPendingLease(Env.Fleet,Env.Fleet.Lease);g.PrepareAttempt(1000);Env.Service.Controller.TripCurrent=false;Need(Spawn(g)==null&&Env.Game.NativeCalls==0,"actual coordinator membership required");
 g=Env.Fresh();g.AttachPendingLease(Env.Fleet,Env.Fleet.Lease);g.PrepareAttempt(1000);car=Spawn(g);Env.Fleet.Bind(car);g.RecordOwnLeaseBind(car);Env.Fleet.Lease.Lifecycle="replacement";Need(!g.OwnsVehicleLease(Env.Fleet.Lease,car),"lifecycle ABA proof rejected");
 g=Env.Fresh();g.CancelAvailable();Need(Env.Service.Cancels==1&&!g.IsCurrent(),"unused exact reservation cancelled");g.Destroy();Need(Env.Service.Cancels==1,"destructor cannot double-refund");
 g=Env.Fresh();var owner=Env.Service;owner.Admission=false;Need(AICF_AIQuotaTripVehicleCreation.Capture(Env.Trip,Env.Trip.Request)==null&&Env.Trip.Request.Attempts==0,"full quota admission does not consume request attempt");
 g=Env.Fresh();var service=Env.Service;service.OnReserve=()=>service.Epoch++;Need(AICF_AIQuotaTripVehicleCreation.Capture(Env.Trip,Env.Trip.Request)==null&&service.Cancels==1,"reservation callback invalidation cancels only new available unit");
 g=Env.Fresh();g.AttachPendingLease(Env.Fleet,Env.Fleet.Lease);g.PrepareAttempt(1000);Clock.Now=9000;Need(Spawn(g)==null&&Env.Game.NativeCalls==0&&Env.Trip.Request.Attempts==0,"actual deadline cannot borrow captured old time");
 g=Env.Fresh();g.AttachPendingLease(Env.Fleet,Env.Fleet.Lease);g.PrepareAttempt(1000);Env.Trip.Phase=AICF_ETransportTripPhase.BOARDING;Need(Spawn(g)==null&&Env.Game.NativeCalls==0,"phase exit cancels factory permission");
 g=Env.Fresh();g.AttachPendingLease(Env.Fleet,Env.Fleet.Lease);g.PrepareAttempt(1000);Env.Trip.Transitions+=2;Need(Spawn(g)==null&&Env.Game.NativeCalls==0,"same-phase ABA cancels factory permission");
 g=Env.Fresh();g.AttachPendingLease(Env.Fleet,Env.Fleet.Lease);g.PrepareAttempt(1000);Env.Trip.Request.Deadline++;Need(Spawn(g)==null&&Env.Game.NativeCalls==0,"changed deadline is a new immutable request context");
 g=Env.Fresh();g.AttachPendingLease(Env.Fleet,Env.Fleet.Lease);g.PrepareAttempt(1000);g.m_Lease=null;Need(!g.IsCurrent()&&!g.AttachPendingLease(Env.Fleet,new AICF_VehicleLease()),"cleared captured weak lease cannot become an unattached permit");
 Env.Fresh();var f=AICF_AIQuotaFIAVehicleCreation.CaptureGarrison(Env.Garrison,Env.Fleet);Need(f!=null&&f.GetBucket()==AICF_EAIQuotaBucket.FIA_EXTRA,"actual registered garrison has separate FIA basket");car=new Factory().SpawnSelectedPrefab("fia",0,null,f);Need(car!=null&&Env.Fleet.Bind(car)&&f.RecordOwnLeaseBind(car)&&f.OwnsVehicleLease(Env.Fleet.Lease,car),"garrison exact own bind receipt");
 Env.Fresh();f=AICF_AIQuotaFIAVehicleCreation.CapturePatrol(Env.Patrol,Env.Fleet);Need(f!=null&&f.GetBucket()==AICF_EAIQuotaBucket.FIA_EXTRA,"registered patrol reserve");Env.Patrol.m_Current=new SCR_CampaignMilitaryBaseComponent();Need(new Factory().SpawnSelectedPrefab("fia",0,null,f)==null&&Env.Game.NativeCalls==0,"patrol base replacement before native veto");
 Env.Fresh();Env.Service.Controller.FIAOwner=false;Need(AICF_AIQuotaFIAVehicleCreation.CaptureGarrison(Env.Garrison,Env.Fleet)==null&&Env.Service.Reserved==1,"same FIA faction without actual service membership cannot reserve");
 Env.Fresh();f=AICF_AIQuotaFIAVehicleCreation.CaptureGarrison(Env.Garrison,Env.Fleet);Env.Garrison.m_iGeneration++;Need(!f.IsCurrent(),"garrison generation invalidates guard");
 Env.Fresh();f=AICF_AIQuotaFIAVehicleCreation.CapturePatrol(Env.Patrol,Env.Fleet);Env.Patrol.m_bRetired=true;Need(!f.IsCurrent(),"retired patrol cannot create another asset");
 Env.Fresh();Env.Garrison.m_Base.Faction=new SCR_CampaignFaction{Key="US"};Need(AICF_AIQuotaFIAVehicleCreation.CaptureGarrison(Env.Garrison,Env.Fleet)==null,"lost FIA base cannot create asset");
 return Count;
 }
}
'@
 $ns='AICFOwnedFactory'+[guid]::NewGuid().ToString('N')
 $code=$adapters.Replace('__SOURCE__',$source).Replace('__FACTORY__',$factoryText).Replace('public class N {',"namespace $ns {public class N {")+'}'
 Add-Type -TypeDefinition $code -Language CSharp
 return [type]("$ns.Cases")
}
$type=CompileActual $common $trip $factory
$cases=$type.GetMethod('Run').Invoke($null,@())
$unsafe=0
foreach($mutation in @(
 @('vehicle generation','lease.GetVehicleGeneration() == m_iBoundGeneration','true'),
 @('lifecycle','lease.GetVehicleLifecycleId() == m_sBoundLifecycle','true'),
 @('pending lease','AICF_AIQuotaVehicleOwner.Fingerprint(m_Lease) == m_sPendingLease','true'),
 @('lost captured lease','if (!m_Lease) return !m_bLeaseAttached;','if (!m_Lease) return true;'),
 @('native null','m_bNativeNull = true;','m_bNativeNull = false;')
)){
 if(!$common.Contains($mutation[1])){throw "Missing mutation $($mutation[0])"}
 $mutant=CompileActual ($common.Replace($mutation[1],$mutation[2])) $trip $factory
 $caught=$false;try{$null=$mutant.GetMethod('Run').Invoke($null,@())}catch{$caught=$true}
 if(!$caught){throw "Unsafe factory mutation survived $($mutation[0])"};$unsafe++
}
foreach($mutation in @(
 @('fresh deadline','m_Request.IsDeadlineReached(System.GetTickCount())','false'),
 @('phase exit','m_Trip.GetPhase() != AICF_ETransportTripPhase.SPAWN_COMMIT','false'),
 @('phase ABA','m_Trip.GetTransitionCount() != m_iTransitions','false'),
 @('deadline replacement','m_Request.GetAbsoluteDeadlineMs() != m_iDeadline','false')
)){
 if(!$trip.Contains($mutation[1])){throw "Missing trip mutation $($mutation[0])"}
 $mutant=CompileActual $common ($trip.Replace($mutation[1],$mutation[2])) $factory
 $caught=$false;try{$null=$mutant.GetMethod('Run').Invoke($null,@())}catch{$caught=$true}
 if(!$caught){throw "Unsafe trip mutation survived $($mutation[0])"};$unsafe++
}
"PASS owned vehicle actual_guard_trip_factory_cases=$cases unsafe_executed=$unsafe native_service_adapter=BOUNDARY_ONLY native_runtime=NOT_RUN"
