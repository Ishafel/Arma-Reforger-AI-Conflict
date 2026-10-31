param([string]$RepositoryRoot=(Split-Path -Parent (Split-Path -Parent $PSScriptRoot)))
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot '../lib/Stage3StaticAudit.Common.ps1')
. (Join-Path $PSScriptRoot 'Test-AIQuotaVehicleEntryContracts.ps1') -RepositoryRoot $RepositoryRoot
$entry=[regex]::Replace($code,'(?m)^using [^\r\n]+;\s*','')
$entry=$entry.Replace('public class PlayerManager:Native {public int PlayerId;public int GetPlayerIdFromControlledEntity(IEntity e){return PlayerId;}}','public class PlayerController:Native {public int Id=1;public IEntity Actor;public Action OnRead;public int GetPlayerId(){return Id;}public IEntity GetControlledEntity(){if(OnRead!=null)OnRead();return Actor;}} public class PlayerManager:Native {public int PlayerId;public PlayerController Controller;public Action OnLookup;public int GetPlayerIdFromControlledEntity(IEntity e){if(OnLookup!=null)OnLookup();return PlayerId;}public PlayerController GetPlayerController(int id){return Controller;}}')
$force=Join-Path $RepositoryRoot 'AIConflictCore/Scripts/Game/AIConflict/Forces'
function Convert-VehicleReplay([string]$s){
 $s=$s.Replace('ref ','').Replace('GetGame()','Runtime.Game').Replace('EntityID','long').Replace('int.MAX','int.MaxValue').Replace('string.Format','Fmt.Format')
 $s=$s.Replace('long.INVALID','0L').Replace('System.GetTickCount()','QuotaDiagnosticClock.GetTickCount()')
 $s=$s.Replace('active = agent.IsAIActivated();','active = agent.IsAIActivated()?1:0;').Replace('counted = agent.CountTowardsAIWorldLimit();','counted = agent.CountTowardsAIWorldLimit()?1:0;')
 $s=[regex]::Replace($s,'(?s)enum AICF_EAIQuotaBucket\s*\{.*?\}','')
 $s=[regex]::Replace($s,'\bAICF_EAIQuotaBucket\b(?=\s+\w+)','int')
 $s=$s.Replace('array<AICF_EAIQuotaBucket>','EArray<int>').Replace('array<int>','AICF_QuotaInts').Replace('array<string>','EArray<string>')
 $s=[regex]::Replace($s,'array<([^>]+)>','EArray<$1>')
 $s=[regex]::Replace($s,'(EArray<([^>]+)>\s+\w+\s*=\s*)\{\};','$1new EArray<$2>();')
 $s=$s.Replace('EArray<IEntity> nodes = {vehicle};','EArray<IEntity> nodes = new EArray<IEntity>{vehicle};').Replace('= {};','= new AICF_QuotaInts();')
 $s=$s.Replace('override protected ','public override ').Replace('protected ','public ').Replace('static const int','public const int')
 $s=[regex]::Replace($s,'(?m)^(\s*)(static (?:bool|string|AICF_\w+)|bool|void|int|string|AICF_\w+) (\w+)\(','$1public $2 $3(')
 $s=[regex]::Replace($s,'(?m)^(\s*)(?!return\b)(\w+(?:<[^>]+>)?) (m_\w+)(?=;|\s*=)','$1public $2 $3')
 $s=[regex]::Replace($s,'(?m)^class (AICF_\w+)\s*$','public class $1 : Native')
 $s=$s.Replace('modded class AICF_AIWorldQuotaService','public class AICF_AIWorldQuotaService:QuotaBase')
 $s=$s.Replace('public void AICF_AIQuotaLedger(','public AICF_AIQuotaLedger(')
 $s=$s.Replace('FindComponent(RplComponent)','FindComponent(typeof(RplComponent))').Replace('FindComponent(BaseCompartmentManagerComponent)','FindComponent(typeof(BaseCompartmentManagerComponent))')
 $s=[regex]::Replace($s,'([\w]+\.[\w]+\(\)|[\w]+)\.IsEmpty\(\)','string.IsNullOrEmpty($1)')
 $s=[regex]::Replace($s,'string\.IsNullOrEmpty\((m_a\w+)\)','$1.IsEmpty()')
 $s=[regex]::Replace($s,'for \(int (\w+);','for (int $1 = 0;')
 $s=[regex]::Replace($s,'foreach \(([^:]+) : ([^)]+)\)','foreach ($1 in $2)')
 $s=[regex]::Replace($s,'(?m)^(\s*)(int|bool|long) (\w+);','$1$2 $3 = default($2);')
 $s=[regex]::Replace($s,'(?m)^(\s*)(AICF_\w+|SCR_\w+|AIAgent|IEntity|Faction|AIControlComponent|CharacterControllerComponent|PlayerManager|PlayerController) (?!m_)(\w+);','$1$2 $3 = null;')
 $s=$s.Replace('ECharacterLifeState m_Life','int m_Life').Replace('ECharacterLifeState life','int life')
 $s=$s.Replace('AddBounded(sum, value, MAX_TOTAL_UNITS, next)','AddBounded(sum, value, MAX_TOTAL_UNITS, out next)').Replace('TotalLimits(candidate, total)','TotalLimits(candidate, out total)').Replace('TotalLimits(limits, total)','TotalLimits(limits, out total)')
 $s=$s.Replace('AddBounded(physical[index], reserved[index], limits[index], occupied)','AddBounded(physical[index], reserved[index], limits[index], out occupied)').Replace('AddBounded(counts[index], m_aReserved[index], m_Limits.GetLimit(index), occupied)','AddBounded(counts[index], m_aReserved[index], m_Limits.GetLimit(index), out occupied)')
 $s=$s.Replace(', owner.m_Fleet, owner.m_Lease, owner.m_Role, owner.m_Bucket)',', out owner.m_Fleet, out owner.m_Lease, out owner.m_Role, out owner.m_Bucket)').Replace(', fleet, lease, role, bucket)',', out fleet, out lease, out role, out bucket)')
 $s=$s.Replace('ClassifyQuotaVehicleRow(row, bucket, charge)','ClassifyQuotaVehicleRow(row, out bucket, out charge)').Replace('ClassifyQuotaVehicleRow(row, vehicleBucket, vehicleCharge)','ClassifyQuotaVehicleRow(row, out vehicleBucket, out vehicleCharge)')
 $s=$s.Replace('VehicleCharge(owner, use, FindBody(row.m_Entity), bucket, charge)','VehicleCharge(owner, use, FindBody(row.m_Entity), out bucket, out charge)').Replace('VehicleCharge(owner, use, body, bucket, charge)','VehicleCharge(owner, use, body, out bucket, out charge)')
 $s=$s.Replace('ClassifyRow(row, bucket)','ClassifyRow(row, out bucket)').Replace('ClassifyCapturedActor(body, SCR_AIGroup.Cast(row.m_Group), bucket)','ClassifyCapturedActor(body, SCR_AIGroup.Cast(row.m_Group), out bucket)').Replace('m_Owners.ClassifyGroup(SCR_AIGroup.Cast(row.m_Group), bucket)','m_Owners.ClassifyGroup(SCR_AIGroup.Cast(row.m_Group), out bucket)').Replace('m_Owners.ClassifyGroup(parent, bucket)','m_Owners.ClassifyGroup(parent, out bucket)')
 $s=$s.Replace('ClassifyCapturedActor(body, parent, bodyBucket)','ClassifyCapturedActor(body, parent, out bodyBucket)')
 $s=$s.Replace('.Remove(index)','.RemoveAt(index)').Replace('.Remove(bodyIndex)','.RemoveAt(bodyIndex)').Replace('.Remove(sourceIndex)','.RemoveAt(sourceIndex)').Replace('CompartmentAccessComponent access;','CompartmentAccessComponent access = null;')
 $s=$s.Replace('ReadQuotaCharacterCustody(row, hold)','ReadQuotaCharacterCustody(row, out hold)').Replace('ReadQuotaCharacterCustody(row, characterHold)','ReadQuotaCharacterCustody(row, out characterHold)')
 return $s
}
$service=[IO.File]::ReadAllText((Join-Path $force 'AICF_AIWorldQuotaService.c'))
function ServiceBody([string]$method){Get-AICFMethodBody ([pscustomobject]@{Source=$service;Code=(ConvertTo-AICFCodeText $service)}) $method}
$replay="using System;using System.Collections.Generic;namespace QuotaVehicleBackendReplay {`n"+$entry
$replay+=@'
public class EArray<T>:List<T>{public new int Count(){return base.Count;}public void Insert(T v){Add(v);}public bool IsEmpty(){return base.Count==0;}public int Find(T v){return IndexOf(v);}public bool IsIndexValid(int i){return i>=0&&i<base.Count;}public static implicit operator bool(EArray<T> v){return v!=null;}}
public static class QuotaDiagnosticClock {public static int GetTickCount(){return 20000;}}
public class AICF_QuotaInts:EArray<int>{public void Resize(int n){while(base.Count()<n)Add(0);if(base.Count()>n)RemoveRange(n,base.Count()-n);}}
public static class Fmt {public static string Format(string format,params object[] values){for(int i=values.Length;i>=1;i--)format=format.Replace("%"+i,Convert.ToString(values[i-1]));return format;}}
public static class AICF_EAIQuotaBucket {public const int STOCK=0,COMBAT=1,BUILDERS=2,FLEET=3,FIA_EXTRA=4,STOCK_VEHICLES=5,COUNT=6;}
public static class AICF_EVehicleLeaseState {public const int ACTIVE=1,RELEASE_PENDING=2,FAILED_CLOSED=3;}
public static class AICF_LogisticsConfig {public const int SERVICE_SLOT_FIRST=1000;}
public class Vehicle:IEntity {public static Vehicle Cast(object v){return v as Vehicle;}}
public class AICF_AIQuotaCreationGuard:Native {public bool Current,Bound;public int Bucket;public AICF_VehicleLease Expected;public IEntity Entity;public bool IsCurrent(){return Current;}public int GetBucket(){return Bucket;}public bool OwnsVehicleLease(AICF_VehicleLease l,IEntity e){return Bound&&l==Expected&&e==Entity;}}
// В этом replay новые producer methods проверяются отдельным actual contract.
// Здесь DTO нужен только для неизменных vehicle loops при пустом producer list.
public class AICF_AIQuotaCompartmentCreation:Native {public AICF_AIQuotaBody m_CarBody,m_CharacterBody;public AICF_AIQuotaVehicleEntity m_Newborn;public AICF_AIQuotaVehicleEntry m_SeatScope;public bool m_bClosed,m_bSourceEnded;public bool IsSeatScopeCurrent(){return m_SeatScope!=null&&m_SeatScope.IsSeatCurrent();}}
public class AICF_AIQuotaGroupOwner:Native {public SCR_AIGroup m_Group;public bool MatchesIdentity(){return true;}public int GetBucket(){return 1;}}
public class AICF_AIQuotaRegistry:Native {public bool UnownedStock;public AICF_AIQuotaGroupOwner Find(SCR_AIGroup group){return null;}public bool ClassifyGroup(SCR_AIGroup group,out int bucket){if(UnownedStock&&group==null){bucket=0;return true;}bucket=1;return group!=null;}}
public class AICF_VehicleLease:Native
{
 public Vehicle Car;public string Faction="US",Life="L1";public int Slot,Group=2,Trip=3,Lease=4,Generation=5,State=1;public bool Cap=true;
 public Vehicle GetVehicle(){return Car;}public long GetEntityId(){return Car==null?0:Car.Id;}public string GetRplId(){return Car==null?"":Car.Rpl.R.ToString();}public string GetFactionKey(){return Faction;}public int GetSlotId(){return Slot;}public int GetGroupGeneration(){return Group;}public int GetTripGeneration(){return Trip;}public int GetLeaseGeneration(){return Lease;}public int GetVehicleGeneration(){return Generation;}public string GetVehicleLifecycleId(){return Life;}public int GetState(){return State;}public bool IsCapActive(){return Cap;}public bool MatchesEntityIdentity(Vehicle car,long id,string rpl){return Car==car&&car.Id==id&&car.Rpl.R.ToString()==rpl;}
}
public class AICF_FactionFleet:Native {public EArray<AICF_VehicleLease> Leases=new EArray<AICF_VehicleLease>();public string Faction="US";public string GetFactionKey(){return Faction;}public int GetLeaseCount(){return Leases.Count();}public AICF_VehicleLease GetLease(int n){return Leases[n];}}
public class SCR_AIWorld:IEntity {public EArray<AIAgent> Agents=new EArray<AIAgent>();public int Intended;public int GetCurrentNumOfActiveAIs(){int n=0;foreach(var a in Agents)if(a.IsAIActivated()&&a.CountTowardsAIWorldLimit())n++;return n;}public int GetActiveGroupIntendedCount(){return Intended;}}
public class ChimeraAIWorld:SCR_AIWorld {public bool Hard=true,Faction=true;public bool CanAIBeActivated(){return Hard;}public bool CanActivateGroup(SCR_AIGroup g){return Hard;}public bool CanLimitedAIBeAddedForFaction(string key){return Faction;}public static ChimeraAIWorld Cast(object v){return v as ChimeraAIWorld;}}
public class AICF_MatchController:Native {public bool Active=true;public SCR_AIWorld World;public EArray<AICF_FactionFleet> Fleets=new EArray<AICF_FactionFleet>();public EArray<int> Roles=new EArray<int>();public bool IsAIQuotaOwnerCurrent(SCR_AIWorld w){return Active&&w==World;}public bool CollectAIQuotaFleets(EArray<AICF_FactionFleet> f,EArray<int> r){f.Clear();r.Clear();f.AddRange(Fleets);r.AddRange(Roles);return Active;}}
public class AICF_AIQuotaWorldState:Native {public int Epoch=1,Last=1;public int GetActiveEpoch(){return Epoch;}public int GetLastEpoch(){return Last;}}
public class AICF_AIQuotaAgentRow:Native {public AIAgent m_Agent;public long m_AgentId;public IEntity m_Entity;public SCR_AIGroup m_Group;public bool m_bVehicle,m_bCounted,m_bActivated;public bool IsCurrent(BaseWorld world){return m_Agent!=null&&m_Agent.World==world&&m_Agent.Actor==m_Entity&&m_Agent.Group==m_Group&&m_Agent.Activated==m_bActivated&&m_Agent.Counted==m_bCounted;}}
public class AICF_AIQuotaCensus:Native
{
 public EArray<AICF_AIQuotaAgentRow> Rows=new EArray<AICF_AIQuotaAgentRow>();public SCR_AIWorld World;public bool Known=true;
 public bool Capture(SCR_AIWorld world){World=world;Rows.Clear();foreach(var a in world.Agents)Rows.Add(new AICF_AIQuotaAgentRow{m_Agent=a,m_AgentId=a.Id,m_Entity=a.Actor,m_Group=a.Group,m_bVehicle=a.Actor is Vehicle,m_bCounted=a.Counted,m_bActivated=a.Activated});return IsCurrent();}
 public int Count(){return Rows.Count();}public bool HasCompleteCapture(){return Known;}public Action<int> OnRow;public AICF_AIQuotaAgentRow GetRow(int n){if(OnRow!=null)OnRow(n);return Known?Rows[n]:null;}public int GetNativeActive(){int n=0;foreach(var r in Rows)if(r.m_bCounted&&r.m_bActivated)n++;return n;}public int GetNativeIntended(){return World.Intended;}public bool IsCurrent(){if(!Known||World==null||World.Agents.Count()!=Rows.Count())return false;foreach(var r in Rows)if(!r.IsCurrent(World.World))return false;return true;}
}
public static class AICF_Stage1Diagnostics {public static List<string> Events=new List<string>();public static void Info(string name,string text){Events.Add(name+" "+text);}}
'@
foreach($file in @('AICF_AIQuotaMath.c','AICF_AIQuotaLimits.c','AICF_AIQuotaLedger.c','AICF_AIQuotaBody.c')){$replay+=Convert-VehicleReplay ([IO.File]::ReadAllText((Join-Path $force $file)))}
$replay+=@'
public class QuotaBase:Native
{
 public ChimeraAIWorld m_World;public AICF_MatchController m_Controller,m_InitializingController;public int m_iEpoch,m_iNextContext=1,m_iHumanRetirementDiagnosticAt;public AICF_AIQuotaLimits m_Limits;public AICF_AIQuotaLedger m_Ledger;public AICF_AIQuotaWorldState m_WorldState=new AICF_AIQuotaWorldState();public AICF_AIQuotaCensus m_Census;public AICF_AIQuotaRegistry m_Owners=new AICF_AIQuotaRegistry();public EArray<AICF_AIQuotaBody> m_aBodies=new EArray<AICF_AIQuotaBody>();public bool m_bNativeBirthBusy,m_bQuarantined,m_bQuotaCensusBusy;public string m_sLastReason;
 public bool IsQuotaAdmissionBusy(){return m_bNativeBirthBusy||m_bQuotaCensusBusy;}public void PruneGroupQueues(){}
 public bool IsCurrentWorld(){return Replication.Server&&Runtime.Game!=null&&m_World!=null&&m_World.World==Runtime.Game.World;}public bool IsOwnerCurrent(){return IsCurrentWorld()&&m_Controller!=null&&m_Controller.IsAIQuotaOwnerCurrent(m_World)&&m_iEpoch>0;}public bool IsConfigured(){return IsCurrentWorld()&&m_Limits!=null&&m_Limits.IsConfigured()&&m_Ledger!=null&&!m_bQuarantined;}public AICF_MatchController GetQuotaController(){return m_Controller;}public int GetEpoch(){return m_iEpoch;}public int AllocateContextSerial(){return IsConfigured()&&!m_bNativeBirthBusy?m_iNextContext++:0;}
 public virtual int BeginQuotaVehicleBootstrap(AICF_MatchController c){return 0;}public virtual bool EndQuotaVehicleBootstrap(AICF_MatchController c,int serial){return false;}public virtual bool ClassifyQuotaVehicleRow(AICF_AIQuotaAgentRow row,out int b,out bool charge){b=0;charge=false;return false;}public virtual bool AddQuotaVehicleHolds(AICF_AIQuotaCensus s,AICF_QuotaInts c,EArray<IEntity> counted){return false;}public virtual bool InitializeQuotaVehicleBindings(AICF_AIQuotaCensus s){return false;}
'@
foreach($method in @('FindBody','ClassifyRow','ClassifyCapturedActor','ReadQuotaCharacterCustody','InitializeQuotaCharacterBindings','BuildCounts','TryRetireHumanBody','ReportHumanRetirementVeto','RefreshCensus','RefreshCurrentCensus')){
 $type=@{FindBody='AICF_AIQuotaBody';ClassifyRow='bool';ClassifyCapturedActor='bool';ReadQuotaCharacterCustody='bool';InitializeQuotaCharacterBindings='bool';BuildCounts='bool';TryRetireHumanBody='bool';ReportHumanRetirementVeto='void';RefreshCensus='bool';RefreshCurrentCensus='bool'}[$method]
 $args=@{FindBody='IEntity entity';ClassifyRow='AICF_AIQuotaAgentRow row, out int bucket';ClassifyCapturedActor='AICF_AIQuotaBody body, SCR_AIGroup parent, out int bucket';ReadQuotaCharacterCustody='AICF_AIQuotaAgentRow row, out bool hold';InitializeQuotaCharacterBindings='AICF_AIQuotaCensus snapshot';BuildCounts='AICF_AIQuotaCensus snapshot, AICF_QuotaInts counts';TryRetireHumanBody='AICF_AIQuotaCensus snapshot,AICF_AIQuotaBody body,int index';ReportHumanRetirementVeto='AICF_AIQuotaBody body';RefreshCensus='';RefreshCurrentCensus=''}[$method]
 $replay+="public $type $method($args){`n"+(Convert-VehicleReplay (ServiceBody $method))+"`n}`n"
}
$replay+="}`n"
foreach($file in @('AICF_AIQuotaVehicleOwner.c','AICF_AIQuotaVehicleUse.c','AICF_AIQuotaVehicleAdmission.c')){
 $vehicleSource=[IO.File]::ReadAllText((Join-Path $force $file))
 if($file-eq'AICF_AIQuotaVehicleAdmission.c'){
  # Exact new producer methods + lifecycle replay live in their own contract;
  # do not substitute these native factory APIs into the existing116 cases.
  $start=$vehicleSource.IndexOf("`tAICF_AIQuotaCompartmentCreation BeginCompartmentCreation(")
  if($start-lt0){throw 'Named compartment producer boundary absent'}
  $vehicleSource=$vehicleSource.Substring(0,$start)+"}`n"
 }
 $replay+=Convert-VehicleReplay $vehicleSource
}
$replay+=@'
public static class BackendCases
{
 static int Cases;static long Id;static AICF_AIWorldQuotaService S;static AICF_MatchController C;static AICF_FactionFleet Fleet;static SCR_AIGroup Group;static ChimeraCharacter Actor;
 static void Need(bool yes,string why){Cases++;if(!yes)throw new Exception(why);}
 static T Register<T>(T e)where T:IEntity{e.Id=++Id;e.World=Runtime.Game.World;Runtime.Game.World.Entities[e.Id]=e;return e;}
 static Vehicle Car(bool active=false){var v=Register(new Vehicle());v.Manager=new BaseCompartmentManagerComponent();var seat=new BaseCompartmentSlot{Vehicle=v,Owner=v,Manager=v.Manager};v.Manager.Seats.Add(seat);if(active)S.m_World.Agents.Add(Register(new AIAgent{Actor=v,Activated=true,Counted=true}));return v;}
 static void Own(Vehicle v,int slot=0){Fleet.Leases.Add(new AICF_VehicleLease{Car=v,Slot=slot});}
 static ChimeraCharacter Member(){var a=Register(new ChimeraCharacter());a.Control.Agent=Register(new AIAgent{Actor=a,Group=Group,Activated=true,Counted=true});S.m_World.Agents.Add(a.Control.Agent);return a;}
 static AICF_AIQuotaVehicleEntry Entry(Vehicle v,ChimeraCharacter a=null,int seat=0){return AICF_AIQuotaVehicleEntry.Capture(a??Actor,v,v.Manager.Seats[seat]);}
 static void Cold()
 {
  Replication.Server=true;SCR_PossessingManagerComponent.MainId=0;Runtime.Game=new Game();Id=100;AICF_Stage1Diagnostics.Events.Clear();
  S=new AICF_AIWorldQuotaService();S.m_World=Register(new ChimeraAIWorld());C=new AICF_MatchController{World=S.m_World};Fleet=new AICF_FactionFleet();C.Fleets.Add(Fleet);C.Roles.Add(3);Group=Register(new SCR_AIGroup());Actor=Member();
 }
 static void Init()
 {
  int serial=S.BeginQuotaVehicleBootstrap(C);Need(serial>0,"typed bootstrap begin");var snap=new AICF_AIQuotaCensus();var counts=new AICF_QuotaInts();Need(snap.Capture(S.m_World)&&S.BuildCounts(snap,counts),"actual cold census with preexisting car, no configured service");Need(S.EndQuotaVehicleBootstrap(C,serial),"exact bootstrap end");
  S.m_Limits=new AICF_AIQuotaLimits();Need(S.m_Limits.CaptureStockSnapshot(1,counts[0],counts[5])&&S.m_Limits.Configure(220,0,1,4),"real sixbucket limits");S.m_Ledger=new AICF_AIQuotaLedger(S.m_Limits,1);S.m_Controller=C;S.m_iEpoch=1;S.m_Census=snap;Need(S.m_Ledger.ApplyCensus(counts,true)&&S.InitializeQuotaVehicleBindings(snap),"actual existing car binding after physical census");
 }
 static AICF_AIQuotaBody Body(Vehicle car){return S.FindBody(car);}
 public static int Run()
 {
  Cases=0;Cold();var stock=Car(true);Init();Need(S.m_Ledger.GetPhysical(5)==1&&S.m_aBodies.Count()==1,"initialstock car exact1, char separate");Need(S.InitializeQuotaVehicleBindings(S.m_Census)&&S.m_aBodies.Count()==1&&S.m_Ledger.GetPhysical(5)==1,"initial synchronise idempotent, no doublecount");
  Cold();var car=Car();Own(car);Init();var e=Entry(car);Need(S.PeekVehicleEntry(e)&&S.m_iNextContext==1&&S.m_Ledger.GetReserved(3)==0,"cached peek no serial/no reservation");var p=S.BeginVehicleEntry(e);Need(p!=null&&S.m_Ledger.GetPhysical(3)==1&&S.m_Ledger.GetReserved(3)==0,"new entry reserves and binds one actual car before native");
  var second=Member();car.Manager.Seats.Add(new BaseCompartmentSlot{Vehicle=car,Owner=car,Manager=car.Manager,SlotId=3});Need(S.RefreshCensus(),"new actual member census");var p2=S.BeginVehicleEntry(Entry(car,second,1));Need(p2!=null&&S.m_Ledger.GetPhysical(3)==1&&S.m_Ledger.GetReserved(3)==0,"two actual samecar occupants share exactly1unit");
  S.CancelVehicleEntry(p);S.CancelVehicleEntry(p2);Fleet.Leases.Clear();Need(S.RefreshCensus()&&S.m_Ledger.GetPhysical(3)==0&&Body(car)==null,"released empty inactive car retires0hold, fleet admission recovers");
  Cold();car=Car();Init();p=S.BeginVehicleEntry(Entry(car));Need(p!=null,"late activation car permit before agent exists");S.ObserveVehicleEntryResult(p,true);S.CancelVehicleEntry(p);Need(!p.m_bClosed&&S.RefreshCensus()&&S.m_Ledger.GetPhysical(5)==1,"accepted async unseen queue retains hold after caller end");
  var seat=car.Manager.Seats[0];seat.Occupant=Actor;Actor.Access.Seat=seat;Actor.Access.Vehicle=car;S.m_World.Agents.Add(Register(new AIAgent{Actor=car,Activated=true,Counted=true}));Need(S.RefreshCensus()&&p.m_bClosed&&S.m_Ledger.GetPhysical(5)==1,"actual exact settledseat closes pending, native agent replaces hold");
  seat.Occupant=null;Actor.Access.Seat=null;Actor.Access.Vehicle=null;foreach(var a in S.m_World.Agents)if(a.Actor==car)a.Activated=false;Need(S.RefreshCensus()&&S.m_Ledger.GetPhysical(5)==0&&Body(car)==null,"inactive empty and no pending eventually releases actual stock car");
  Cold();stock=Car(true);car=Car(true);Own(car);Init();Fleet.Leases.Clear();Need(S.RefreshCensus()&&S.m_Ledger.GetPhysical(3)==1&&S.m_Ledger.GetPhysical(5)==1,"fullstock releasedfleet retains old source cost");Need(!S.PeekVehicleEntry(Entry(car))&&S.BeginVehicleEntry(Entry(car))==null&&Body(car).m_Bucket==3,"new stock entry cannot borrow retained fleet unit");Need(AICF_Stage1Diagnostics.Events.Exists(x=>x.StartsWith("AI_QUOTA_VEHICLE_TRANSFER_WAIT")),"explicit transfer wait receipt");
  foreach(var a in S.m_World.Agents)if(a.Actor==stock)a.Activated=false;Need(S.RefreshCensus()&&S.RefreshCensus(),"fresh census replaces old occupied stock before transfer");Need(Body(car).m_Bucket==5&&S.m_Ledger.GetPhysical(3)==0&&S.m_Ledger.GetPhysical(5)==1,"atomic logical transfer frees source only with target room");
  var fresh=Car();Own(fresh);Need(S.RefreshCensus()&&S.BeginVehicleEntry(Entry(fresh))!=null,"new ownfleet entry resumes after positive transfer");
  Cold();car=Car();Own(car,1000);Init();p=S.BeginVehicleEntry(Entry(car));Need(p!=null&&Body(car).m_Bucket==1,"service car explicit registered role uses COMBAT, not capped Fleet");
  Cold();car=Car();Own(car);Init();e=Entry(car);Fleet.Leases[0].State=2;Need(S.BeginVehicleEntry(e)==null,"releasepending lease cannot receive new entry");
  Cold();car=Car();Own(car);Init();var owner=AICF_AIQuotaVehicleOwner.Capture(S,car);Fleet.Leases[0].Trip++;Need(!owner.IsCurrent(),"lease generation replacement invalidates exact owner");
  Cold();car=Car();Init();int serial2=S.BeginQuotaVehicleBootstrap(C);var censusOwner=AICF_AIQuotaVehicleOwner.CaptureForCensus(S,car);Need(censusOwner!=null&&!censusOwner.CanEnter(),"bootstrap cannot issue entry permission");Need(!S.EndQuotaVehicleBootstrap(C,serial2+1)&&S.IsQuotaVehicleBootstrapCurrent(C,serial2),"stale bootstrap end cannot clear current scope");S.EndQuotaVehicleBootstrap(C,serial2);Need(!censusOwner.IsCurrent(),"ended bootstrap receipts expire");
  Cold();car=Car();Init();e=Entry(car);S.m_bQuotaCensusBusy=true;Need(!S.PeekVehicleEntry(e)&&S.BeginVehicleEntry(e)==null&&S.m_iNextContext==1,"reentrant census cannot evaluate/admit a new permit");S.m_bQuotaCensusBusy=false;
  S.m_World.Hard=false;Need(S.BeginVehicleEntry(e)==null&&S.m_iNextContext==1,"native hard gate preserved before new vehicle activation");
  Cold();car=Car(true);Init();S.m_World.Hard=false;Need(S.BeginVehicleEntry(Entry(car))!=null,"already active/counted samecar consumes no additional native unit at hard cap");
  Cold();car=Car();Init();p=S.BeginVehicleEntry(Entry(car));S.MarkVehicleEntryDispatched(p);Need(p.m_bDispatched&&!p.m_bHasNativeResult&&!p.m_bNativeAccepted,"void selected dispatch is not bool native acceptance");S.CancelVehicleEntry(p);Need(!p.m_bClosed&&S.RefreshCensus()&&S.m_Ledger.GetPhysical(5)==1,"unproven queue abort never releases a dispatched claim");
  Cold();car=Car();Own(car);Init();p=S.BeginVehicleEntry(Entry(car));S.CancelVehicleEntry(p);var held=Body(car);held.m_CreationGuard=new AICF_AIQuotaCreationGuard{Current=true,Bucket=3};Fleet.Leases.Clear();Need(S.RefreshCensus()&&Body(car)==held&&held.m_Bucket==3&&held.m_CreationGuard!=null,"exact provisional factory guard retains original source before lease binding");Need(!S.PeekVehicleEntry(Entry(car))&&S.BeginVehicleEntry(Entry(car))==null,"provisional owned factory car cannot borrow unleased stock permission");Own(car);Need(S.RefreshCensus()&&held.m_CreationGuard!=null&&S.BeginVehicleEntry(Entry(car))==null,"any currentlease alone is not the captured positive creation bind receipt");held.m_CreationGuard.Expected=Fleet.Leases[0];held.m_CreationGuard.Entity=car;held.m_CreationGuard.Bound=true;Need(S.RefreshCensus()&&held.m_CreationGuard==null,"positive actual lease bind ends provisional guard custody");
  Cold();car=Car();Own(car);Init();p=S.BeginVehicleEntry(Entry(car));S.CancelVehicleEntry(p);held=Body(car);held.m_CreationGuard=new AICF_AIQuotaCreationGuard{Current=true,Bucket=3};Fleet.Leases.Clear();Need(S.RefreshCensus(),"provisional charge established");held.m_CreationGuard.Current=false;Need(S.RefreshCensus()&&Body(car)==held&&held.m_CreationGuard!=null&&S.m_Ledger.GetPhysical(3)==1,"invalidated unbound factory guard retains alive empty inactive source custody");Need(!S.PeekVehicleEntry(Entry(car))&&S.BeginVehicleEntry(Entry(car))==null&&Body(car)==held,"stale factory guard cannot become reusable stock admission");Runtime.Game.World.Entities.Remove(car.Id);held.m_Entity=null;Need(S.RefreshCensus()&&held.m_bReleased&&held.m_CreationGuard==null&&S.m_Ledger.GetPhysical(3)==0,"positive actual newborn absence retires stale custody, no invented permanent leak");
  Cold();car=Car(true);var snap2=new AICF_AIQuotaCensus();Need(snap2.Capture(S.m_World),"raw native snapshot known");car.Manager.Seats.Add(null);Need(AICF_AIQuotaVehicleUse.Capture(car,snap2)==null,"missing compartment row does not become empty proof");
  Cold();car=Car();Own(car);Init();Fleet.Leases.Add(Fleet.Leases[0]);Need(AICF_AIQuotaVehicleOwner.Capture(S,car)==null&&S.BeginVehicleEntry(Entry(car))==null,"ambiguous multiple actual leases reject entry");
  Cold();car=Car();Own(car);Init();e=Entry(car);p=S.BeginVehicleEntry(e);S.m_Ledger.ApplyCensus(null,false);Need(!S.PeekVehicleEntry(e)&&!S.IsVehicleEntryPermitCurrent(p,e),"old complete snapshot and existing body cannot bypass unknown ledger");Need(S.RefreshCensus()&&S.PeekVehicleEntry(e),"fresh complete census restores entry after unknown ledger");
  return Cases;
 }
}
}
'@
Add-Type -TypeDefinition $replay -Language CSharp
$cases=[QuotaVehicleBackendReplay.BackendCases]::Run()
$unsafe=@(
 @{name='IgnorePending';from='&& !HasVehiclePending(body)';to=''},
 @{name='CancelFreesUnknownQueue';from='if (!permit.m_bDispatched || IsVehicleEntrySettled';to='if (permit.m_bCancelRequested || IsVehicleEntrySettled'},
 @{name='ForgetActiveNativeCar';from='charge = use.m_bActive || use.m_bHasAI;';to='charge = use.m_bHasAI;'},
 @{name='BootstrapEntryPermission';from='if (m_bCensusOnly || !IsCurrent())';to='if (!IsCurrent())'},
 @{name='StaleBootstrapEnd';from='serial != m_iVehicleBootstrapSerial';to='serial < 1'},
 @{name='BorrowRetainedSource';from='bool room = body && body.m_Bucket == owner.m_Bucket;';to='bool room = body != null;'},
 @{name='LoseCarDedupe';from='if (body && !body.m_bReleased && body.m_Entity == entity) return body;';to='if (body && !body.m_bReleased && body.m_Entity == entity) continue;'},
 @{name='DropProvisionalHold';from='bool provisional = body.m_CreationGuard != null;';to='bool provisional = body.m_CreationGuard && body.m_CreationGuard.IsCurrent();'}
)
$caught=0
foreach($variant in $unsafe){
 if(!$replay.Contains($variant.from)){throw ('Unsafe control source absent: '+$variant.name)}
 $ns='QuotaVehicleBackendUnsafe'+$variant.name
 $mutant=$replay.Replace('QuotaVehicleBackendReplay',$ns).Replace($variant.from,$variant.to)
 Add-Type -TypeDefinition $mutant -Language CSharp
 $type=($ns+'.BackendCases')-as[type]
 $rejected=$false
 try{[void]$type::Run()}catch{$rejected=$true}
 if(!$rejected){throw ('Actual backend unsafe mutant survived: '+$variant.name)}
 $caught++
}
"PASS AI quota vehicle backend actual_extracted_cases=$cases executed_unsafe_mutants=$caught actual_ledger=1 actual_refresh=1 native=NOT_RUN"
