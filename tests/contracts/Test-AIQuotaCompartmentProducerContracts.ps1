param([string]$RepositoryRoot=(Split-Path -Parent (Split-Path -Parent $PSScriptRoot)))
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'Test-AIQuotaVehicleBackendContracts.ps1') -RepositoryRoot $RepositoryRoot
$contextText=[IO.File]::ReadAllText((Join-Path $force 'AICF_AIQuotaCompartmentCreation.c'))
$producerText=[IO.File]::ReadAllText((Join-Path $RepositoryRoot 'AIConflictCore/Scripts/Game/AIConflict/Integration/AICF_AIQuotaCompartmentProducer.c'))
$admissionText=[IO.File]::ReadAllText((Join-Path $force 'AICF_AIQuotaVehicleAdmission.c'))
$pinned=[IO.File]::ReadAllText((Join-Path $RepositoryRoot '.cache/reforger-api/Arma-Reforger-Script-Diff-1.8.0.13/scripts/GameCode/Vehicle/BaseCompartmentSlot.c'))
foreach($token in @('IEntity SpawnCharacterInCompartment(ResourceName characterPrefab, inout AIGroup group','agentControlComponent.ActivateAI();','compartmentAccess.GetInVehicle(GetOwner(), this, true','group.AddAgent(agentControlComponent.GetControlAIAgent());')){if(!$pinned.Contains($token)){throw ('Pinned compartment seam drift: '+$token)}}
if($contextText-match'\bref\s+AICF_AIQuotaCreation\s+m_Creation'){throw 'Strong context->creation->guard cycle is forbidden'}
if($producerText-match'override\s+bool\s+GetInVehicle\('){throw 'Unsupported native GetIn override'}
function CompartmentConvert([string]$s){
 $s=Convert-VehicleReplay $s
 $s=[regex]::Replace($s,'\bResourceName\b|\bFactionKey\b','string')
 $s=$s.Replace('override bool ','public override bool ').Replace('override int ','public override int ').Replace('override string ','public override string ').Replace('override IEntity ','public override IEntity ').Replace('override void ','public override void ').Replace('public public ','public ')
 $s=$s.Replace('super.','base.').Replace('inout AIGroup group','ref AIGroup group').Replace('FindComponent(AIControlComponent)','FindComponent(typeof(AIControlComponent))').Replace('FindComponent(CompartmentAccessComponent)','FindComponent(typeof(CompartmentAccessComponent))').Replace('FindComponent(FactionAffiliationComponent)','FindComponent(typeof(FactionAffiliationComponent))')
 $s=$s.Replace('base.SpawnCharacterInCompartment(characterPrefab, group, groupPrefab)','base.SpawnCharacterInCompartment(characterPrefab, ref group, groupPrefab)')
 $s=$s.Replace('ReadCharacterMetadata(characterPrefab, context.m_sFaction)','ReadCharacterMetadata(characterPrefab, out context.m_sFaction)').Replace('affiliation.Get("faction affiliation", faction)','affiliation.Get("faction affiliation", out faction)').Replace('source.Get("m_aUnitPrefabSlots", units)','source.Get("m_aUnitPrefabSlots", out units)')
 $s=$s.Replace('m_Owners.ClassifyGroup(SCR_AIGroup.Cast(group), groupBucket)','m_Owners.ClassifyGroup(SCR_AIGroup.Cast(group), out groupBucket)')
 $s=$s.Replace('array<string> units','EArray<string> units').Replace('EArray<string> units = new AICF_QuotaInts();','EArray<string> units = new EArray<string>();').Replace('string.IsNullOrEmpty(units)','units.IsEmpty()')
 $s=$s.Replace('EArray<BaseCompartmentSlot> seats = new AICF_QuotaInts();','EArray<BaseCompartmentSlot> seats = new EArray<BaseCompartmentSlot>();')
 $s=$s.Replace('.IsInherited(ChimeraCharacter)','.IsInherited(typeof(ChimeraCharacter))').Replace('.IsInherited(SCR_AIGroup)','.IsInherited(typeof(SCR_AIGroup))').Replace('FindComponentSource(resource, FactionAffiliationComponent)','FindComponentSource(resource, typeof(FactionAffiliationComponent))')
 $s=$s.Replace('modded class BaseCompartmentSlot','public class BaseCompartmentSlot:CompartmentSlotNative').Replace('bool accepted = access.GetInVehicle','bool accepted = access.GetInVehicle')
 $s=$s.Replace('class AICF_AIQuotaCompartmentCreation :','public class AICF_AIQuotaCompartmentCreation :')
 $s=$s.Replace('EntitySpawnParams params =','EntitySpawnParams spawn =').Replace('params.Transform','spawn.Transform').Replace(', null, params)',', null, spawn)').Replace('spawn.Transform[3] + SPAWN_IN_VEHICLE_OFFSET;','GC.KeepAlive(spawn.Transform[3] + SPAWN_IN_VEHICLE_OFFSET);')
 $s=$s.Replace('ChimeraWorld world =','BaseWorld world =')
 $s=$s.Replace('public void ~AICF_AIWorldQuotaService()','public void Destroy()').Replace('void ~AICF_AIWorldQuotaService()','public void Destroy()')
 return $s
}
$actualContext=CompartmentConvert $contextText
$methodStart=$admissionText.IndexOf("`tAICF_AIQuotaCompartmentCreation BeginCompartmentCreation(")
$newMethods=CompartmentConvert ($admissionText.Substring($methodStart,$admissionText.LastIndexOf('}')-$methodStart))
$actualProducer=CompartmentConvert $producerText
$r=$replay.Replace('QuotaVehicleBackendReplay','QuotaCompartmentReplay')
$r=[regex]::Replace($r,'public class AICF_AIQuotaCompartmentCreation:Native \{[^\r\n]*\}',$actualContext)
$r=$r.Replace('public class AICF_AIWorldQuotaService:QuotaBase' + "`n{",'public class AICF_AIWorldQuotaService:QuotaBase' + "`n{`n"+$newMethods)
if(!$r.Contains('BeginCompartmentCreation(')){throw 'Actual producer methods were not inserted'}
$r=$r.Replace('public class Game:Native','public partial class Game:Native').Replace('public class QuotaBase:Native','public partial class QuotaBase:Native')
$r=$r.Replace('public class SCR_AIGroup:IEntity','public class SCR_AIGroup:AIGroup').Replace('public class BaseCompartmentSlot:Native','public class CompartmentSlotNative:Native')
$r=$r.Replace('public class AIControlComponent:Native','public partial class AIControlComponent:Native').Replace('public class CompartmentAccessComponent:Native','public partial class CompartmentAccessComponent:Native')
$r=$r.Replace('public class RplComponent:Native','public partial class RplComponent:Native')
$r=$r.Replace('public class BaseWorld:Native {','public class BaseWorld:Native {public bool IsGameTimePaused(){return false;}')
$r=$r.Replace('public AICF_AIQuotaAgentRow m_Native;','private AICF_AIQuotaAgentRow m_StrongNative;private WeakReference m_WeakNative;public AICF_AIQuotaAgentRow m_Native{get{return Probe.WeakRows?(m_WeakNative==null?null:m_WeakNative.Target as AICF_AIQuotaAgentRow):m_StrongNative;}set{if(Probe.WeakRows)m_WeakNative=new WeakReference(value);else m_StrongNative=value;}}')
$r=$r.Replace('public bool IsCurrent(){return Current;}public int GetBucket(){return Bucket;}','public virtual bool IsCurrent(){return Current;}public virtual int GetBucket(){return Bucket;}public virtual string GetFactionKey(){return "US";}')
# Explicit native registry boundary for this producer replay. The shared adapter
# also has an UnownedStock branch; replacing an old method literal silently left
# that adapter active and rejected the initial null-group before any factory.
$registryPattern='(?m)^public class AICF_AIQuotaRegistry:Native \{[^\r\n]*\}\r?$'
if([regex]::Matches($r,$registryPattern).Count-ne1){throw 'Exact producer registry adapter boundary missing/ambiguous'}
$r=[regex]::Replace($r,$registryPattern,'public class AICF_AIQuotaRegistry:Native {public bool UnownedStock;public AICF_AIQuotaGroupOwner Find(SCR_AIGroup group){return null;}public bool ClassifyGroup(SCR_AIGroup group,out int bucket){bucket=Probe.GroupBucket;return Probe.RegistryKnown;}}')
$r=$r.Replace('CanActivateGroup(SCR_AIGroup g)','CanActivateGroup(AIGroup g)').Replace('return Manager;}}','if(type==typeof(CompartmentAccessComponent))return (this as ChimeraCharacter)==null?null:(this as ChimeraCharacter).Access;if(type==typeof(AIControlComponent))return (this as ChimeraCharacter)==null?null:(this as ChimeraCharacter).Control;return Manager;}}')
$r=$r.Replace('public static class AICF_Stage1Diagnostics {','public static class AICF_Stage1Diagnostics {public static void Error(string name,string text){Events.Add(name+" "+text);}')
$r=$r.Replace('public IEntity GetOccupant(){return Occupant;}','public bool Accessible=true;public bool IsCompartmentAccessible(){return Accessible;}public IEntity GetOccupant(){return Occupant;}public static readonly V SPAWN_IN_VEHICLE_OFFSET=new V();public virtual IEntity SpawnCharacterInCompartment(string prefab,ref AIGroup group,string gp="base"){Probe.Delegated++;return null;}')
$r=$r.Replace('public long GetID(){return Id;}','public long GetID(){return Id;}public void GetTransform(V[] v){v[3]=new V();}')
$r=$r.Replace('public bool IsInVehicle(){return Access.Vehicle!=null;}','public bool IsInVehicle(){return Access!=null&&Access.Vehicle!=null;}')
$r=$r.Substring(0,$r.LastIndexOf('}'))
$identityText=[IO.File]::ReadAllText((Join-Path $force 'AICF_AIQuotaCatalogBasket.c'))
$identityText=$identityText.Substring(0,$identityText.IndexOf('class AICF_AIQuotaCatalogParents'))
$r+=CompartmentConvert $identityText
$r+=@'
public class AIGroup:IEntity {public List<AIAgent> Members=new List<AIAgent>();public void AddAgent(AIAgent a){Members.Add(a);a.Group=this as SCR_AIGroup;}public bool CountTowardsAIWorldLimit(){return false;}public int GetAgentsCount(){return Members.Count;}public int GetPlayerCount(){return 0;}public EArray<string> m_aUnitPrefabSlots=new EArray<string>();}
public static class GroupExtras {public static void InitFactionKey(this SCR_AIGroup g,string key){g.Faction.Key=key;}}
public class V{public static V operator +(V a,V b){return new V();}}public class EntitySpawnParams:Native{public V[] Transform=new V[]{new V(),new V(),new V(),new V()};}
public class MetaType{public Type Type;public bool IsInherited(Type t){return t.IsAssignableFrom(Type);}}public class MetaName{public Type Type;public MetaType ToType(){return new MetaType{Type=Type};}}
public class IEntitySource:Native{public Type Type;public bool NonEmpty;public MetaName GetClassName(){return new MetaName{Type=Type};}public bool Get(string key,out EArray<string> units){units=new EArray<string>();if(NonEmpty)units.Add("extra");return true;}}
public class IEntityComponentSource:Native{public bool Get(string key,out string faction){faction=Probe.Faction;return Probe.MetadataKnown;}}
public class ResourceData:Native{public IEntitySource Source;public IEntitySource ToEntitySource(){return Source;}}
public class Resource:Native{public string Name;public bool IsValid(){return Probe.MetadataKnown;}public ResourceData GetResource(){return new ResourceData{Source=new IEntitySource{Type=Name=="char"?typeof(ChimeraCharacter):typeof(SCR_AIGroup),NonEmpty=Probe.GroupNonEmpty}};}public static Resource Load(string name){return string.IsNullOrEmpty(name)?null:new Resource{Name=name};}}
public static class SCR_BaseContainerTools {public static IEntityComponentSource FindComponentSource(Resource r,Type t){return Probe.MetadataKnown?new IEntityComponentSource():null;}}
public class FactionManager:Native{public Faction GetFactionByKey(string key){return string.IsNullOrEmpty(key)?null:new Faction{Key=key};}}
public static class SCR_ResourceNameUtils{public static string GetPrefabName(IEntity e){return e is SCR_AIGroup?"base":"char";}}
public partial class Game {public FactionManager GetFactionManager(){return new FactionManager();}public bool IsGameTimePaused(){return false;}public IEntity SpawnEntityPrefab(Resource r,object unused=null,EntitySpawnParams p=null){return Probe.Spawn(r);}}
public class ChimeraWorld:Game{}
public partial class AIControlComponent{public static AIControlComponent Cast(object o){return o as AIControlComponent;}public bool ActivateAI(bool forced=true){Probe.Activations++;if(Probe.OnActivate!=null)Probe.OnActivate();if(Agent!=null){Agent.Activated=true;Agent.Counted=true;}return true;}}
public partial class CompartmentAccessComponent{
 public static CompartmentAccessComponent Cast(object o){return o as CompartmentAccessComponent;}
 public bool IsInCompartment(){return Vehicle!=null;}
 public bool GetInVehicle(IEntity owner,BaseCompartmentSlot seat,bool tp,int door,int close,bool paused){Probe.Entries++;if(Probe.OnEntry!=null)Probe.OnEntry();if(Probe.EntryAccepted){Vehicle=seat.Vehicle;Seat=seat;seat.Occupant=Probe.Actor;Probe.ActivateCar((Vehicle)seat.Vehicle);}return Probe.EntryAccepted;}}
public static class ECloseDoorAfterActions{public const int INVALID=0;}
public partial class RplComponent{public static void DeleteRplEntity(IEntity e,bool children){Probe.Deletes++;Runtime.Game.World.Entities.Remove(e.Id);Probe.S.m_World.Agents.RemoveAll(a=>a.Actor==e);foreach(var b in Probe.S.m_aBodies)if(b.m_Entity==e)b.m_Entity=null;}}
public static class AICF_AIQuotaBirth {public static bool IsNonPlayerUnassigned(IEntity e){var a=e as ChimeraCharacter;return a!=null&&a.Control!=null&&a.Controller!=null&&!a.Controller.Player&&Runtime.Game.Players.PlayerId==0&&SCR_PossessingManagerComponent.MainId==0&&a.Access!=null&&a.Access.Vehicle==null&&(a.Control.Agent==null||a.Control.Agent.Group==null);}}
public static class AICF_EAIQuotaBirthStatus{public const int IN_NATIVE_FACTORY=0,BOUND=1,NATIVE_NULL=2,QUARANTINED=3,ROLLED_BACK=4,VETO=5;}
public class AICF_AIQuotaCreation:Native{public AICF_AIWorldQuotaService m_Service;public AICF_AIQuotaCreationGuard m_Guard;public AICF_AIQuotaReservation m_Reservation;public AICF_AIQuotaDispatch Ticket;public int Serial,m_Status;public IEntity m_Entity;public AICF_AIQuotaBody m_Body;public bool m_bFinished,Started;}
public partial class QuotaBase{
 public static AICF_AIWorldQuotaService GetAuthorityForWorld(BaseWorld w){return Probe.S!=null&&Runtime.Game!=null&&w==Runtime.Game.World?Probe.S:null;}
 public AICF_AIQuotaCreation m_CurrentCreation;public virtual void StopOwner(AICF_MatchController c){if(c==m_Controller)c.Active=false;}
 public AICF_AIQuotaReservation ReserveUnits(int bucket,int units,int serial){if(!RefreshCensus())return null;if(Probe.WeakRows){GC.Collect();GC.WaitForPendingFinalizers();GC.Collect();}return m_Ledger.Reserve(bucket,units,serial);}public bool CancelReservation(AICF_AIQuotaReservation r,int s){return m_Ledger.CancelAvailable(r,s);}
 public AICF_AIQuotaCreation BeginExternalCreation(AICF_AIQuotaReservation r,int s,string prefab,IEntity producer,bool header,AICF_AIQuotaCreationGuard guard){if(IsQuotaAdmissionBusy()||!guard.IsCurrent()||!RefreshCensus())return null;var t=m_Ledger.BeginDispatch(r,s);if(t==null)return null;var c=new AICF_AIQuotaCreation{m_Service=this as AICF_AIWorldQuotaService,m_Guard=guard,m_Reservation=r,Ticket=t,Serial=s};m_CurrentCreation=c;m_bNativeBirthBusy=true;return c;}
 public bool MarkExternalNativeCallStarted(AICF_AIQuotaCreation c){if(c==null||c!=m_CurrentCreation||!c.m_Guard.IsCurrent())return false;c.Started=true;return true;}
 public void ObserveExternalFactory(AICF_AIQuotaCreation c,IEntity e){c.m_Entity=e;if(e==null){m_Ledger.FailDispatch(c.Ticket,c.Serial);c.m_Status=2;return;}c.m_Body=AICF_AIQuotaBody.Capture(e,0);if(c.m_Body!=null&&c.m_Guard.IsCurrent()&&AICF_AIQuotaBirth.IsNonPlayerUnassigned(e)&&m_Ledger.Bind(c.Ticket,c.Serial,c.m_Body.Key())){m_aBodies.Insert(c.m_Body);c.m_Status=1;}else{c.m_Status=3;m_bQuarantined=true;}}
 public bool EndExternalCreation(AICF_AIQuotaCreation c){if(c!=m_CurrentCreation)return false;c.m_bFinished=true;m_CurrentCreation=null;m_bNativeBirthBusy=false;return true;}public bool CancelExternalCreationBeforeFactory(AICF_AIQuotaCreation c){m_Ledger.FailDispatch(c.Ticket,c.Serial);return EndExternalCreation(c);}
 public void RejectExternalCreation(AICF_AIQuotaCreation c,string reason){m_bQuarantined=true;c.m_Status=3;EndExternalCreation(c);}public bool CanRollbackExternalCreation(AICF_AIQuotaCreation c){return c==m_CurrentCreation&&!c.m_bFinished&&c.m_Body!=null&&c.m_Body.MatchesIdentity()&&!c.m_Guard.IsCurrent()&&AICF_AIQuotaBirth.IsNonPlayerUnassigned(c.m_Entity);}public bool ObserveExternalAbsence(AICF_AIQuotaCreation c){if(!c.m_Body.IsDefinitelyGone())return false;c.m_Body.m_bReleased=true;m_Ledger.RetireBinding(c.m_Body.Key());c.m_Status=4;return EndExternalCreation(c);}
}
public static class Probe {
 public static AICF_AIWorldQuotaService S;public static Vehicle Car;public static ChimeraCharacter Actor;public static bool MetadataKnown=true,RegistryKnown=true,GroupNonEmpty,EntryAccepted=true,WeakRows;public static string Faction="US";public static int GroupBucket,SpawnCalls,Entries,Activations,Deletes,Delegated;public static Action OnSpawn,OnActivate,OnEntry;static long Id;
 public static T Add<T>(T e)where T:IEntity{e.Id=++Id;e.World=Runtime.Game.World;Runtime.Game.World.Entities[e.Id]=e;return e;}
 public static BaseCompartmentSlot Setup(int stock=4){Runtime.Game=new ChimeraWorld();Replication.Server=true;SCR_PossessingManagerComponent.MainId=0;Id=100;MetadataKnown=RegistryKnown=EntryAccepted=true;GroupNonEmpty=WeakRows=false;Faction="US";GroupBucket=SpawnCalls=Entries=Activations=Deletes=Delegated=0;OnSpawn=OnActivate=OnEntry=null;S=new AICF_AIWorldQuotaService();S.m_World=Add(new ChimeraAIWorld());S.m_Controller=new AICF_MatchController{World=S.m_World};S.m_iEpoch=1;S.m_Limits=new AICF_AIQuotaLimits();S.m_Limits.CaptureStockSnapshot(stock,0,0);S.m_Limits.Configure(220,0,20,0);S.m_Ledger=new AICF_AIQuotaLedger(S.m_Limits,1);Car=Add(new Vehicle());Car.Manager=new BaseCompartmentManagerComponent();var seat=new BaseCompartmentSlot{Vehicle=Car,Owner=Car,Manager=Car.Manager};Car.Manager.Seats.Add(seat);if(!S.RefreshCensus())throw new Exception("setup census");return seat;}
 public static IEntity Spawn(Resource resource){SpawnCalls++;if(OnSpawn!=null)OnSpawn();if(resource.Name=="base")return Add(new SCR_AIGroup());Actor=Add(new ChimeraCharacter());Actor.Affiliation=new FactionAffiliationComponent{Faction=new Faction{Key=Faction}};Actor.Control.Agent=Add(new AIAgent{Actor=Actor});S.m_World.Agents.Add(Actor.Control.Agent);return Actor;}
 public static void ActivateCar(Vehicle car){foreach(var a in S.m_World.Agents)if(a.Actor==car)return;S.m_World.Agents.Add(Add(new AIAgent{Actor=car,Activated=true,Counted=true}));}
}
'@
$r+=$actualProducer
$r+=@'
public static class CompartmentCases {
 static int N;static void Need(bool b,string why){N++;if(!b)throw new Exception(why);}
 public static int Run(){N=0;AIGroup group=null;var seat=Probe.Setup();var raw=seat.SpawnCharacterInCompartment("char",ref group,"base");Need(raw!=null&&Probe.SpawnCalls==2&&Probe.Entries==1&&Probe.Activations==1,"real scripted producer creates char then emptygroup and seats exactlyonce");Need(group!=null&&group.GetAgentsCount()==1&&Probe.S.RefreshCensus()&&Probe.S.m_Ledger.GetPhysical(0)==1&&Probe.S.m_Ledger.GetPhysical(5)==1,"actual ledger char1 and car1 after native readback, no doublecharge");Need(Probe.S.m_aCompartmentCreations.Count()==0,"settled producer custody retired without strongcycle");
 var seat2=new BaseCompartmentSlot{Vehicle=Probe.Car,Owner=Probe.Car,Manager=Probe.Car.Manager,SlotId=3};Probe.Car.Manager.Seats.Add(seat2);Need(seat2.SpawnCharacterInCompartment("char",ref group,"base")!=null&&Probe.S.RefreshCensus()&&Probe.S.m_Ledger.GetPhysical(0)==2&&Probe.S.m_Ledger.GetPhysical(5)==1,"multiple occupants one car body, each char full1");
 seat=Probe.Setup(1);group=null;int serial=Probe.S.AllocateContextSerial();var stockReservation=Probe.S.ReserveUnits(0,1,serial);Need(stockReservation!=null&&seat.SpawnCharacterInCompartment("char",ref group,"base")==null&&Probe.SpawnCalls==0&&Probe.Entries==0&&Probe.S.m_Ledger.GetReserved(0)==1,"full stock character basket veto zero body/seat and preserves existing reservation");Probe.S.CancelReservation(stockReservation,serial);Need(seat.SpawnCharacterInCompartment("char",ref group,"base")!=null,"released exact stock capacity admits next full producer");
 seat=Probe.Setup();group=null;Probe.MetadataKnown=false;Need(seat.SpawnCharacterInCompartment("char",ref group,"base")==null&&Probe.SpawnCalls==0&&Probe.Entries==0,"unknown metadata zero native");
 seat=Probe.Setup();group=null;Probe.GroupNonEmpty=true;Need(seat.SpawnCharacterInCompartment("char",ref group,"base")==null&&Probe.SpawnCalls==0,"nonempty new group metadata cannot hide additional authored spawn");
 seat=Probe.Setup();group=null;Probe.GroupBucket=1;Need(seat.SpawnCharacterInCompartment("char",ref group,"base")==null&&Probe.SpawnCalls==0,"foreign owned group bucket not borrowed as stock");
 seat=Probe.Setup();group=null;Probe.S.m_World.Hard=false;Need(seat.SpawnCharacterInCompartment("char",ref group,"base")==null&&Probe.SpawnCalls==0&&Probe.Entries==0,"native hard veto before both factories");
 seat=Probe.Setup(1);group=null;var other=Probe.Add(new Vehicle());other.Manager=new BaseCompartmentManagerComponent();Probe.ActivateCar(other);Need(Probe.S.RefreshCensus()&&Probe.S.m_Ledger.GetPhysical(5)==1,"preexisting active stock vehicle occupies its own actual bucket");Need(seat.SpawnCharacterInCompartment("char",ref group,"base")==null&&Probe.SpawnCalls==0&&Probe.Entries==0&&Probe.S.m_Ledger.GetReserved(0)==0,"full vehicle bucket blocks newborn before character reservation/factory");foreach(var agent in Probe.S.m_World.Agents)if(agent.Actor==other)agent.Activated=false;Need(Probe.S.RefreshCensus()&&seat.SpawnCharacterInCompartment("char",ref group,"base")!=null,"positive inactive empty stock car retirement restores next producer");
 seat=Probe.Setup();group=null;Probe.OnActivate=()=>Runtime.Game.Players.PlayerId=9;Need(seat.SpawnCharacterInCompartment("char",ref group,"base")==null&&Probe.Entries==0&&Probe.Deletes==0&&Probe.S.m_bQuarantined,"activation callback human takeover never seated/deleted/adopted");
 seat=Probe.Setup();group=null;Probe.OnActivate=()=>seat.Owner=Probe.Add(new IEntity());Need(seat.SpawnCharacterInCompartment("char",ref group,"base")==null&&Probe.Entries==0,"seat owner callback stale before GetIn");
 seat=Probe.Setup();group=null;Probe.EntryAccepted=false;Need(seat.SpawnCharacterInCompartment("char",ref group,"base")==null&&Probe.Entries==1&&Probe.Deletes==1&&!Probe.S.m_bQuarantined,"actual nativefalse allows only exact new unassigned body rollback");Need(Probe.S.RefreshCensus()&&Probe.S.m_Ledger.GetPhysical(0)==0&&Probe.S.m_Ledger.GetPhysical(5)==0,"positive rollback absence releases both costs on freshcensus");
 seat=Probe.Setup();var ctx=Probe.S.BeginCompartmentCreation(seat,"char",null,"base");Need(ctx!=null&&Probe.S.m_Ledger.GetPhysical(5)==1&&Probe.S.m_Ledger.GetReserved(0)==1,"heterogeneous fullbasket held before nativefactory");Need(Probe.S.RefreshCensus()&&Probe.S.m_Ledger.GetPhysical(5)==1,"empty inactive car survives intermediate character reservation refresh");ctx.m_CarBody.m_bReleased=true;Need(!ctx.IsCurrent(),"released car hold cannot back newborn admission even if entity remains");ctx.m_CarBody.m_bReleased=false;Probe.S.StopOwner(Probe.S.m_Controller);Need(ctx.m_bSourceEnded&&ctx.m_bClosed&&Probe.S.m_Ledger.GetReserved(0)==0,"exact owner Stop cancels unused sources with no native attempt");
 seat=Probe.Setup();ctx=Probe.S.BeginCompartmentCreation(seat,"char",null,"base");var creation=Probe.S.BeginCompartmentFactory(ctx);Probe.S.MarkExternalNativeCallStarted(creation);raw=Probe.Spawn(Resource.Load("char"));Probe.S.ObserveExternalFactory(creation,raw);Need(Probe.S.CaptureCompartmentNewborn(ctx,raw)&&Probe.S.OwnsCurrentCompartmentNewborn(ctx),"exact bound currentCreation newborn accepted");Probe.S.m_CurrentCreation=new AICF_AIQuotaCreation();Need(!Probe.S.OwnsCurrentCompartmentNewborn(ctx),"another current creation cannot authorize this newborn");Probe.S.m_CurrentCreation=creation;Probe.S.EndExternalCreation(creation);ctx.m_Creation=null;ctx.m_bSourceEnded=true;ctx.m_bNativeIssued=true;ctx.m_bHasNativeResult=true;ctx.m_bAccepted=true;Need(Probe.S.RefreshCensus()&&Probe.S.m_Ledger.GetPhysical(5)==1&&Probe.S.m_aCompartmentCreations.Count()==1,"weak Creation expiry does not release unresolved async car custody");RplComponent.DeleteRplEntity(raw,false);Need(Probe.S.RefreshCensus()&&ctx.m_bClosed&&Probe.S.m_aCompartmentCreations.Count()==0&&Probe.S.m_Ledger.GetPhysical(5)==0,"positive character absence retires pending even after weak Creation expired");
 seat=Probe.Setup();group=null;Probe.OnEntry=()=>Probe.S.StopOwner(Probe.S.m_Controller);Need(seat.SpawnCharacterInCompartment("char",ref group,"base")==null&&Probe.Entries==1&&Probe.S.m_aCompartmentCreations.Count()==1&&!Probe.S.m_aCompartmentCreations[0].m_bClosed,"Stop callback during actual native entry retains dispatched unknown custody");
 seat=Probe.Setup();group=null;Replication.Server=false;Need(seat.SpawnCharacterInCompartment("char",ref group,"base")==null&&Probe.Delegated==1&&Probe.SpawnCalls==0,"client preserves original method delegation without authority quota actions");
 seat=Probe.Setup();Probe.S.m_World.Agents.Add(Probe.Add(new AIAgent{Actor=Probe.Car,Activated=false,Counted=false}));Probe.WeakRows=true;ctx=Probe.S.BeginCompartmentCreation(seat,"char",null,"base");Need(ctx!=null&&ctx.IsCurrent()&&Probe.S.m_Ledger.GetPhysical(5)==1&&Probe.S.m_Ledger.GetReserved(0)==1,"registered inactive car weak census row must survive character Refresh until final fence");
 return N;}
}
}
'@
# Value/native boundary adapters are explicit above; production Entry, car owner,
# Use, Ledger, custody, compartment Context, Begin/Close and producer all execute.
Add-Type -TypeDefinition $r -Language CSharp
$count=[QuotaCompartmentReplay.CompartmentCases]::Run()
$mutants=@(
 @{name='PrematureCarRelease';from='if (creation && !creation.m_bClosed && creation.m_CarBody == body) return true;';to='if (creation && creation.m_bNativeIssued && !creation.m_bClosed && creation.m_CarBody == body) return true;'},
 @{name='NoCharacterReservation';from='context.m_CharacterReservation = ReserveUnits(AICF_EAIQuotaBucket.STOCK, 1, context.m_iSerial);';to='context.m_CharacterReservation = ReserveUnits(AICF_EAIQuotaBucket.STOCK_VEHICLES, 1, context.m_iSerial);'},
 @{name='NonemptyGroup';from='return source.Get("m_aUnitPrefabSlots", out units) && units.IsEmpty();';to='return source.Get("m_aUnitPrefabSlots", out units);'},
 @{name='BorrowForeignCreation';from='context.m_Creation == m_CurrentCreation &&';to='context.m_Creation != null &&'},
 @{name='ReleasedCarHold';from='body.m_bReleased || body.m_bQuarantined';to='body.m_bQuarantined'},
 @{name='UnmarkedNativeEntry';from='context.m_bNativeIssued = true;';to='/* no physical dispatch receipt */'},
 @{name='LostStopSource';from='if (source && source.m_iEpoch == m_iEpoch) CloseCompartmentCreation(source);';to='if (source && source.m_iEpoch < 0) CloseCompartmentCreation(source);'}
)
$anchorBody=CompartmentConvert (Get-AICFMethodBody ([pscustomobject]@{Source=$admissionText;Code=(ConvertTo-AICFCodeText $admissionText)}) 'BeginCompartmentCreation')
$dropAnchor=[regex]::Replace($anchorBody,'(?m)^\s*AICF_AIQuotaCensus capturedCensus = m_Census;\s*\r?\n','')
$dropAnchor=$dropAnchor.Replace(', capturedCensus);',', m_Census);').Replace(' || !capturedCensus.IsCurrent()','')
if($dropAnchor-eq$anchorBody){throw 'Explicit lifetime anchor missing'}
$mutants+=@{name='DropCensusLifetime';from=$anchorBody;to=$dropAnchor}
$caught=0
foreach($m in $mutants){
 if(!$r.Contains($m.from)){throw ('Unsafe mutation source absent '+$m.name)}
 $ns='QuotaCompartmentUnsafe'+$m.name
 Add-Type -TypeDefinition ($r.Replace('QuotaCompartmentReplay',$ns).Replace($m.from,$m.to)) -Language CSharp
 $type=($ns+'.CompartmentCases')-as[type];$bad=$false
 try{[void]$type::Run()}catch{$bad=$true}
 if(!$bad){throw ('Unsafe compartment mutation survived '+$m.name)};$caught++
}
"PASS quota compartment producer actual_cases=$count unsafe_executed=$caught actual_ledger=1 external_service_boundary=SIMULATED native_runtime=NOT_RUN"
