param([string]$RepositoryRoot=(Split-Path -Parent (Split-Path -Parent $PSScriptRoot)))
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot '../lib/Stage3StaticAudit.Common.ps1')
$core=Join-Path $RepositoryRoot 'AIConflictCore/Scripts/Game/AIConflict'
$basket=[IO.File]::ReadAllText((Join-Path $core 'Forces/AICF_AIQuotaCatalogBasket.c'))
$component=[IO.File]::ReadAllText((Join-Path $core 'Integration/AICF_AIQuotaCatalog.c'))
$queue=[IO.File]::ReadAllText((Join-Path $core 'Integration/AICF_AIQuotaCatalogQueue.c'))
$pinned=[IO.File]::ReadAllText((Join-Path $RepositoryRoot '.cache/reforger-api/Arma-Reforger-Script-Diff-1.8.0.13/scripts/Game/Components/Spawner/CatalogSpawner/SCR_CatalogEntitySpawnerComponent.c'))
function Body($text,$name){Get-AICFMethodBody ([pscustomobject]@{Source=$text;Code=(ConvertTo-AICFCodeText $text)}) $name}
function Tokens($s){[regex]::Replace((ConvertTo-AICFCodeText $s),'\s+','')}
function StripHooks($s){[regex]::Replace($s,'(?ms)// AICF_CATALOG_BEGIN (\w+).*?// AICF_CATALOG_END \1','')}
$perform=StripHooks (Body $component AICF_PerformBoundCatalog)
$perform=$perform.Replace('AICF_QuotaCatalogFactory(basket, spawnerData.GetRandomDefaultOrVariantPrefab(), slot)','SpawnEntity(spawnerData.GetRandomDefaultOrVariantPrefab(), slot.GetOwner())').Replace('AICF_OnBoundCatalogGroup(basket, SCR_AIGroup.Cast(m_SpawnedEntity), user, slot.GetOwner(), slot.GetRallyPoint())','OnAIGroupSpawned(SCR_AIGroup.Cast(m_SpawnedEntity), user, slot.GetOwner(), slot.GetRallyPoint())')
if((Tokens $perform)-cne(Tokens (Body $pinned PerformSpawn))){throw 'Pinned PerformSpawn projection drift'}
$group=Body $component AICF_OnBoundCatalogGroup
$group=[regex]::Replace($group,'(?ms)// AICF_CATALOG_BEGIN QUEUE_EXPLICIT.*?// AICF_CATALOG_END QUEUE_EXPLICIT','groupSpawningManager.QueueSpawn(this, resName, user, slotEntity, rallyPoint);')
if((Tokens (StripHooks $group))-cne(Tokens (Body $pinned OnAIGroupSpawned))){throw 'Pinned OnAIGroupSpawned projection drift'}
if((Tokens (StripHooks (Body $component AICF_QuotaPinnedCatalogFactory)))-cne(Tokens (Body $pinned SpawnEntity))){throw 'Pinned SpawnEntity projection drift'}
$sourceControls=3
function Adapt($s){
 $s=$s.Replace('notnull ','').Replace('ref ','').Replace('GetGame()','Game.Current').Replace('super.','base.')
 $s=[regex]::Replace($s,'\bparams\b','spawnParams')
 $s=[regex]::Replace($s,'array<([^>]+)>','NativeArray<$1>')
 $s=[regex]::Replace($s,'NativeArray<([^>]+)> (\w+) = \{\};','NativeArray<$1> $2 = new NativeArray<$1>();')
 $s=[regex]::Replace($s,'foreach \(([^:]+) : ([^)]+)\)','foreach ($1 in $2)')
 $s=[regex]::Replace($s,'(?m)^\t((?:NativeArray<[^>]+>|\w+) m_\w+(?: =[^;]+)?;)',' public $1')
 $s=[regex]::Replace($s,'(?m)^\t((?:static )?(?:bool|void|FactionKey|AICF_\w+) \w+\()',' public $1')
 $s=$s.Replace('class AICF_','public class AICF_').Replace('override bool','public override bool').Replace('override FactionKey','public override FactionKey')
 $s=[regex]::Replace($s,'public class (AICF_AIQuotaCatalog(?:Identity|Basket|Request))\s*\{','public class $1:NativeObject{')
 foreach($type in @('RplComponent','SCR_SpawnerRequestComponent','SCR_EntityCatalogSpawnerData','SCR_PlayerControllerGroupComponent')){$s=$s.Replace('FindComponent('+$type+')','FindComponent(typeof('+$type+'))').Replace('GetEntityDataOfType('+$type+')','GetEntityDataOfType(typeof('+$type+'))')}
 return $s.Replace('int removed;','int removed = 0;').Replace('AICF_AIQuotaCatalogRequest context;','AICF_AIQuotaCatalogRequest context = null;').Replace('bool nativeCalled;','bool nativeCalled = false;').Replace('string reason;','string reason = null;').Replace('CaptureWithReason(service, spawner, entry, user, reason)','CaptureWithReason(service, spawner, entry, user, out reason)').Replace('slot.GetOwner(), nativeCalled)','slot.GetOwner(), out nativeCalled)').Replace('.GetClassName().ToType().IsInherited(Vehicle)','.IsVehicle')
}
$code=@'
using System;
using System.Collections.Generic;
using ResourceName=System.String;
using FactionKey=System.String;
using EntityID=System.Int32;
public class NativeObject{public bool Deleted;public static implicit operator bool(NativeObject o){return !Object.ReferenceEquals(o,null)&&!o.Deleted;}}
public class NativeArray<T>:List<T>{public static implicit operator bool(NativeArray<T> a){return a!=null;}public new int Count(){return base.Count;}public bool IsEmpty(){return base.Count==0;}public int Find(T x){return IndexOf(x);}public bool IsIndexValid(int i){return i>=0&&i<base.Count;}public void Insert(T x){Add(x);}public void RemoveOrdered(int i){RemoveAt(i);}public void RemoveItem(T x){Remove(x);}public void Copy(NativeArray<T> x){Clear();AddRange(x);}}
public static class NativeString{public static bool IsEmpty(this string s){return String.IsNullOrEmpty(s);}}
public static class Replication{public static bool Server=true;public static bool IsServer(){return Server;}}
public class BaseWorld:NativeObject{public Dictionary<int,IEntity> Entities=new Dictionary<int,IEntity>();public IEntity FindEntityByID(int i){IEntity e;return Entities.TryGetValue(i,out e)?e:null;}}
public class RplId{public string Value="rpl";public bool Valid=true;public bool IsValid(){return Valid;}public override string ToString(){return Value;}}
public class RplComponent:NativeObject{public bool Master=true;public RplId Value=new RplId();public bool IsMaster(){return Master;}public RplId Id(){return Value;}public static RplComponent Cast(object o){return o as RplComponent;}}
public class IEntity:NativeObject{public int Id;public BaseWorld World;public RplComponent Rpl=new RplComponent();public BaseWorld GetWorld(){return World;}public int GetID(){if(Deleted)throw new Exception("deleted entity getter");return Id;}public virtual object FindComponent(Type t){return Rpl;}public void GetTransform(vector[] x){x[3]=new vector();}}
public class SCR_ChimeraCharacter:IEntity{public static SCR_ChimeraCharacter Cast(IEntity e){return e as SCR_ChimeraCharacter;}}
public class SCR_AIGroup:IEntity{public NativeArray<string> m_aUnitPrefabSlots=new NativeArray<string>();public SCR_AIGroup Slave;public SCR_AIGroup GetSlave(){return Slave;}public int Agents,Players;public NativeArray<IEntity> GetAIMembers(){var a=new NativeArray<IEntity>();for(int i=0;i<Agents;i++)a.Add(new IEntity());return a;}public int GetAgentsCount(){return Agents;}public int GetPlayerCount(){return Players;}public static SCR_AIGroup Cast(IEntity e){return e as SCR_AIGroup;}}
public class Faction:NativeObject{public string Key="US";public string GetFactionKey(){return Key;}}
public enum EEntityCatalogType{CHARACTER,GROUP,VEHICLE}
public class SCR_EntityCatalog:NativeObject{public EEntityCatalogType Type=EEntityCatalogType.GROUP;public EEntityCatalogType GetCatalogType(){return Type;}}
public class SCR_EntityCatalogSpawnerData:NativeObject{public int N=5,Cost=200;public NativeArray<string> Variants=new NativeArray<string>();public int GetEntityCount(){return N;}public int GetSupplyCost(){return Cost;}public void GetVariantPrefabList(NativeArray<string> a){a.AddRange(Variants);}public static SCR_EntityCatalogSpawnerData Cast(object o){return o as SCR_EntityCatalogSpawnerData;}}
public class SCR_EntityCatalogEntry:NativeObject{public string Prefab="group.et";public SCR_EntityCatalog Catalog=new SCR_EntityCatalog();public SCR_EntityCatalogSpawnerData Data=new SCR_EntityCatalogSpawnerData();public string GetPrefab(){return Prefab;}public SCR_EntityCatalog GetCatalogParent(){return Catalog;}public object GetEntityDataOfType(Type t){return Data;}}
public class SCR_SpawnerRequestComponent:NativeObject{public static SCR_SpawnerRequestComponent Cast(object o){return o as SCR_SpawnerRequestComponent;}public int Queued,Writes;public void AddQueuedAI(int n){Queued=Math.Max(0,Queued+n);Writes++;}public int GetQueuedAIs(){return Queued;}}
public class SCR_CommandingManagerComponent:NativeObject{public static int Maximum=5;public static SCR_CommandingManagerComponent GetInstance(){return new SCR_CommandingManagerComponent();}public int GetMaxAIPerGroup(){return Maximum;}}
public class SCR_PlayerControllerGroupComponent:NativeObject{public SCR_AIGroup Group;public bool Leader=true;public SCR_AIGroup GetPlayersGroup(){return Group;}public bool IsPlayerLeaderOwnGroup(){return Leader;}public static SCR_PlayerControllerGroupComponent Cast(object o){return o as SCR_PlayerControllerGroupComponent;}}public class SCR_PlayerController:IEntity{public SCR_PlayerControllerGroupComponent GroupComponent=new SCR_PlayerControllerGroupComponent();public int PlayerId=1;public IEntity Controlled;public SCR_SpawnerRequestComponent Request=new SCR_SpawnerRequestComponent();public int GetPlayerId(){return PlayerId;}public IEntity GetControlledEntity(){return Controlled;}public override object FindComponent(Type t){if(t==typeof(SCR_SpawnerRequestComponent))return Request;if(t==typeof(SCR_PlayerControllerGroupComponent))return GroupComponent;return Rpl;}}
public class PlayerManager:NativeObject{public SCR_PlayerController Controller;public SCR_PlayerController GetPlayerController(int id){return Controller!=null&&id==Controller.PlayerId?Controller:null;}public int GetPlayerIdFromControlledEntity(IEntity e){return Controller!=null&&Controller.Controlled==e?Controller.PlayerId:0;}}
public class BaseGameMode:IEntity{public bool Master=true;public bool IsMaster(){return Master;}}
public class Game:NativeObject{public static Game Current;public BaseWorld World;public BaseGameMode Mode;public PlayerManager Players=new PlayerManager();public AICF_AIWorldQuotaService Service;public Action OnSpawn;public IEntity SpawnResult;public int NativeFactories;public BaseWorld GetWorld(){return World;}public BaseGameMode GetGameMode(){return Mode;}public PlayerManager GetPlayerManager(){return Players;}public IEntity SpawnEntityPrefab(Resource r,BaseWorld w,EntitySpawnParams p){NativeFactories++;if(OnSpawn!=null)OnSpawn();return SpawnResult;}}
public static class SCR_StringHelper{public static bool IsEmptyOrWhiteSpace(string s){return String.IsNullOrWhiteSpace(s);}}
public enum AICF_EAIQuotaBucket{STOCK}
public static class AICF_AIQuotaMath{public const int MAX_TOTAL_UNITS=4096;}
public class AICF_AIQuotaReservation:NativeObject{public int m_iAvailable,Serial;public bool Cancelled;}
public enum AICF_EAIQuotaBirthStatus{VETO,IN_NATIVE_FACTORY,NATIVE_NULL,BOUND,ROLLED_BACK,QUARANTINED}
public class AICF_AIQuotaCreationGuard:NativeObject{public virtual bool IsCurrent(){return false;}public virtual string GetFactionKey(){return "";}}
public class AICF_AIQuotaBody:NativeObject{public string Key(){return "body";}public IEntity Entity;public bool MatchesIdentity(){return Entity&&!Entity.Deleted;}}
public class AICF_AIQuotaCreation:NativeObject{public AICF_AIQuotaCreationGuard Guard;public AICF_AIWorldQuotaService m_Service;public IEntity m_Entity;public AICF_AIQuotaBody m_Body;public AICF_EAIQuotaBirthStatus m_Status=AICF_EAIQuotaBirthStatus.IN_NATIVE_FACTORY;public bool m_bFinished,Header,Started;public int m_iNativeActiveBefore,m_iNativeIntendedBefore;public int N;public bool IsProducerCurrent(){return Guard.IsCurrent();}}
public class AICF_AIWorldQuotaService:NativeObject{public bool Owner=true,Configured=true,Policy=true,Busy,BindFailure;public int Epoch=1,Next=1,Limit=20,Reserved,ReserveCalls,CancelCalls,Bodies;public Action AfterReserve;public AICF_AIQuotaCreation Current;public bool IsOwnerCurrent(){return Owner;}public bool IsConfigured(){return Configured;}public bool IsCurrentWorld(){return true;}public int GetEpoch(){return Epoch;}public int AllocateContextSerial(){return Busy?0:Next++;}public bool CanReserve(AICF_EAIQuotaBucket b,int n){return Configured&&!Busy&&n>0&&Reserved+Bodies+n<=Limit;}public AICF_AIQuotaReservation ReserveUnits(AICF_EAIQuotaBucket b,int n,int s){ReserveCalls++;if(!CanReserve(b,n))return null;Reserved+=n;var r=new AICF_AIQuotaReservation{m_iAvailable=n,Serial=s};if(AfterReserve!=null)AfterReserve();return r;}public bool CancelReservation(AICF_AIQuotaReservation r,int s){if(r==null||r.Serial!=s||r.Cancelled)return false;Reserved-=r.m_iAvailable;r.m_iAvailable=0;r.Cancelled=true;CancelCalls++;return true;}public static AICF_AIWorldQuotaService GetAuthorityForWorld(BaseWorld w){return Replication.Server&&Game.Current&&Game.Current.World==w&&Game.Current.Service.Policy?Game.Current.Service:null;}
public AICF_AIQuotaCreation BeginExternalCreation(AICF_AIQuotaReservation r,int serial,string prefab,IEntity producer,bool header,AICF_AIQuotaCreationGuard guard){if(!Configured||Busy||r==null||r.Cancelled||r.m_iAvailable<=0||!guard.IsCurrent())return null;var c=new AICF_AIQuotaCreation{Guard=guard,m_Service=this,Header=header,N=r.m_iAvailable};if(!header)r.m_iAvailable--;Current=c;Busy=true;return c;}
public bool MarkExternalNativeCallStarted(AICF_AIQuotaCreation c){if(Current!=c||c.m_bFinished||c.Started||!c.Guard.IsCurrent())return false;c.Started=true;return true;}public bool CancelExternalCreationBeforeFactory(AICF_AIQuotaCreation c){if(Current!=c||c.m_bFinished||c.Started)return false;c.m_Status=AICF_EAIQuotaBirthStatus.VETO;if(!c.Header)Reserved--;return EndExternalCreation(c);}
public void ObserveExternalFactory(AICF_AIQuotaCreation c,IEntity e){if(Current!=c||c.m_bFinished||!c.Started)return;c.m_Entity=e;c.m_Body=new AICF_AIQuotaBody{Entity=e};if(e==null){c.m_Status=AICF_EAIQuotaBirthStatus.NATIVE_NULL;if(!c.Header)Reserved--;return;}var g=e as SCR_AIGroup;bool shape=!c.Header||(g!=null&&g.Agents==0&&g.Players==0&&g.m_aUnitPrefabSlots.Count()==c.N);if(!shape||!c.Guard.IsCurrent()||BindFailure){c.m_Status=AICF_EAIQuotaBirthStatus.QUARANTINED;return;}c.m_Status=AICF_EAIQuotaBirthStatus.BOUND;if(!c.Header){Reserved--;Bodies++;}}
public bool EndExternalCreation(AICF_AIQuotaCreation c){if(Current!=c||c.m_bFinished||c.m_Status==AICF_EAIQuotaBirthStatus.IN_NATIVE_FACTORY)return false;c.m_bFinished=true;Current=null;Busy=false;return true;}public bool CanRollbackExternalCreation(AICF_AIQuotaCreation c){if(Current!=c||c.m_bFinished||!c.m_Entity)return false;var g=c.m_Entity as SCR_AIGroup;return c.Header?g!=null&&g.Agents==0&&g.Players==0&&(g.m_aUnitPrefabSlots.Count()!=c.N||!c.Guard.IsCurrent()):!c.Guard.IsCurrent();}public bool ObserveExternalAbsence(AICF_AIQuotaCreation c){if(c.m_Entity&&!c.m_Entity.Deleted)return false;if(!c.Header)Reserved--;c.m_Status=AICF_EAIQuotaBirthStatus.ROLLED_BACK;return EndExternalCreation(c);}public void RejectExternalCreation(AICF_AIQuotaCreation c,string r){Configured=false;c.m_Status=AICF_EAIQuotaBirthStatus.QUARANTINED;EndExternalCreation(c);}}
public class SCR_EntityLabelPointComponent:NativeObject{public IEntity Owner;public IEntity GetOwner(){return Owner;}}
public class SCR_EntitySpawnerSlotComponent:NativeObject{public IEntity Owner;public SCR_EntityLabelPointComponent Rally;public IEntity GetOwner(){return Owner;}public SCR_EntityLabelPointComponent GetRallyPoint(){return Rally;}}
public class vector{}
public class EntitySpawnParams{public ETransformMode TransformMode;public vector[] Transform=new vector[4];}
public enum ETransformMode{WORLD}
public class IEntitySource:NativeObject{public bool IsVehicle;}
public class ResourceData:NativeObject{public IEntitySource ToEntitySource(){return Resource.SourceMissing?null:new IEntitySource{IsVehicle=Resource.VehiclePrefab};}}
public class Resource:NativeObject{public static bool Valid=true,VehiclePrefab,SourceMissing;public static Action OnLoad;public static Resource Load(string s){if(OnLoad!=null)OnLoad();return new Resource();}public ResourceData GetResource(){return new ResourceData();}public bool IsValid(){return Valid;}}
public enum LogLevel{ERROR}
public enum SCR_EEntityRequestStatus{CAN_SPAWN,NOT_AVAILABLE,AI_LIMIT_REACHED}
public class NativeSpawnerBase:NativeObject{public static void Print(string s,LogLevel l){}protected virtual void PerformSpawn(SCR_EntityCatalogEntry e,IEntity u,SCR_EntitySpawnerSlotComponent s){}public virtual IEntity SpawnEntity(string r,IEntity s){return Game.Current.SpawnEntityPrefab(Resource.Load(r),Game.Current.World,new EntitySpawnParams());}}
public static class SCR_EntityHelper{public static void DeleteEntityAndChildren(IEntity e){e.Deleted=true;e.World.Entities.Remove(e.Id);}}
public static class AICF_Stage3Diagnostics{public static void Info(string e,string d){}public static void Warning(string e,string d){}}
public enum EntityEvent{FRAME}
public class SCR_SpawnerAIRequest:NativeObject{public SCR_CatalogEntitySpawnerComponent m_Spawner;public string m_ResourceName;public IEntity m_UserEntity,m_SlotEntity;public SCR_EntityLabelPointComponent m_RallyPoint;public AICF_AIQuotaCatalogRequest m_AICFQuotaRequest;public SCR_SpawnerAIRequest(SCR_CatalogEntitySpawnerComponent s,string p,IEntity u,IEntity o,SCR_EntityLabelPointComponent r){m_Spawner=s;m_ResourceName=p;m_UserEntity=u;m_SlotEntity=o;m_RallyPoint=r;}}
public class NativeManagerBase:NativeObject{public NativeArray<SCR_SpawnerAIRequest> m_aAIQueue;public float m_fCurrentGroupMemberSpawnDelay,m_fGroupMemberSpawnDelay=1.5f;public int SetFrames,ClearFrames,NativeQueueCalls,NativeFrameCalls;public IEntity Owner;public IEntity GetOwner(){return Owner;}public BaseGameMode GetGameMode(){return Game.Current.Mode;}public void SetEventMask(IEntity e,EntityEvent ev){SetFrames++;}public void ClearEventMask(IEntity e,EntityEvent ev){ClearFrames++;}public virtual void QueueSpawn(SCR_CatalogEntitySpawnerComponent s,string p,IEntity u,IEntity o,SCR_EntityLabelPointComponent r){NativeQueueCalls++;}public virtual void EOnFrame(IEntity o,float d){NativeFrameCalls++;}}
public class SCR_CatalogEntitySpawnerComponent:NativeSpawnerBase{public static NativeArray<SCR_CatalogEntitySpawnerComponent> INSTANCES=new NativeArray<SCR_CatalogEntitySpawnerComponent>();public IEntity Owner;public Faction Faction=new Faction();public SCR_EntityCatalogEntry Entry;public SCR_EntitySpawnerSlotComponent Slot;public int Forget,Activated,DispatchCalls;public Func<AICF_AIQuotaCatalogRequest,bool> OnDispatch;public bool Wait;public IEntity GetOwner(){return Owner;}public Faction GetFaction(){return Faction;}public Faction GetRequesterFaction(IEntity u){return Faction;}public SCR_PlayerController GetPlayerControllerFromEntity(IEntity u){return Game.Current.Players.Controller;}public SCR_SpawnerRequestComponent GetRequestComponentFromPlayerEntity(IEntity u){return Game.Current.Players.Controller.Request;}public bool AICF_QuotaCatalogContains(SCR_EntityCatalogEntry e){return Entry==e;}public bool AICF_QuotaCatalogSlot(SCR_EntityCatalogEntry e,SCR_EntitySpawnerSlotComponent s){return Entry==e&&Slot==s;}public void AICF_ForgetCatalogBasket(AICF_AIQuotaCatalogBasket b){Forget++;}public bool AICF_DispatchBoundCatalogRequest(AICF_AIQuotaCatalogRequest r){DispatchCalls++;if(OnDispatch!=null)return OnDispatch(r);if(Wait)return false;r.m_bFactoryCalled=true;r.m_bMaterialized=true;return true;}
FACTORY_METHODS
}
public class SCR_SpawnerAIGroupManagerComponent:NativeManagerBase{
MANAGER_METHODS
}
BASKET_SOURCE
public static class Fixture{public static SCR_CatalogEntitySpawnerComponent Shop;public static SCR_EntityCatalogEntry Entry;public static SCR_EntitySpawnerSlotComponent Slot;public static SCR_SpawnerAIGroupManagerComponent Manager;public static int NextId;public static T Entity<T>()where T:IEntity,new(){var e=new T{Id=NextId++,World=Game.Current.World};e.Rpl.Value.Value="rpl"+e.Id;Game.Current.World.Entities[e.Id]=e;return e;}public static SCR_ChimeraCharacter NewActor(){return Entity<SCR_ChimeraCharacter>();}public static SCR_AIGroup NewGroup(){return Entity<SCR_AIGroup>();}public static void Reset(){NextId=1;Game.Current=new Game{World=new BaseWorld(),Service=new AICF_AIWorldQuotaService()};Game.Current.Mode=Entity<BaseGameMode>();var c=Entity<SCR_PlayerController>();c.Rpl.Master=false;c.Controlled=Entity<SCR_ChimeraCharacter>();c.GroupComponent.Group=Entity<SCR_AIGroup>();c.GroupComponent.Group.Slave=Entity<SCR_AIGroup>();Game.Current.Players.Controller=c;Shop=new SCR_CatalogEntitySpawnerComponent{Owner=Entity<IEntity>()};SCR_CatalogEntitySpawnerComponent.INSTANCES.Clear();SCR_CatalogEntitySpawnerComponent.INSTANCES.Add(Shop);Entry=new SCR_EntityCatalogEntry();Shop.Entry=Entry;Slot=new SCR_EntitySpawnerSlotComponent{Owner=Entity<IEntity>(),Rally=new SCR_EntityLabelPointComponent{Owner=Entity<IEntity>()}};Slot.Owner.Rpl=null;Slot.Rally.Owner.Rpl=null;Shop.Slot=Slot;Manager=new SCR_SpawnerAIGroupManagerComponent{Owner=Game.Current.Mode};Resource.Valid=true;Replication.Server=true;}public static AICF_AIQuotaCatalogBasket Capture(){return AICF_AIQuotaCatalogBasket.Capture(Game.Current.Service,Shop,Entry,Game.Current.Players.Controller.Controlled);}public static AICF_AIQuotaCatalogBasket Queue(int n){Entry.Data.N=n;var b=Capture();b.BindSlot(Slot);b.m_bHeaderObserved=true;for(int i=0;i<n;i++)b.m_aMemberPrefabs.Add("member"+i+".et");for(int i=0;i<n;i++)if(!Manager.AICF_QueueBoundCatalog(b,i,b.m_aMemberPrefabs[i],b.m_User.m_Entity,Slot.Owner,Slot.Rally))throw new Exception("queue setup");b.m_bCounterAdded=true;b.m_RequestComponent.Queued=n;return b;}}
public static class UnsafeChecks{
 public static bool UserMissingRpl(){Fixture.Reset();Game.Current.Players.Controller.Controlled.Rpl=null;return Fixture.Capture()!=null;}
 public static bool UserInvalidRpl(){Fixture.Reset();Game.Current.Players.Controller.Controlled.Rpl.Value.Valid=false;return Fixture.Capture()!=null;}
 public static bool UserProxyRejected(){Fixture.Reset();Game.Current.Players.Controller.Controlled.Rpl.Master=false;return Fixture.Capture()==null;}
 public static bool PartialBasket(){Fixture.Reset();Game.Current.Service.Limit=4;return Fixture.Capture()!=null;}
 public static bool QuarantineIdentityLoss(){Fixture.Reset();var b=Fixture.Capture();Game.Current.Service.Configured=false;return !b.IsCurrent();}
 public static bool ForeignHeadPop(){Fixture.Reset();var b=Fixture.Queue(3);var m=Fixture.Manager;var old=m.m_aAIQueue[0];Fixture.Shop.OnDispatch=r=>{m.m_aAIQueue.RemoveOrdered(0);m.m_aAIQueue.Insert(old);r.m_Request.m_AICFQuotaRequest=null;return true;};m.EOnFrame(m.Owner,2);return m.m_aAIQueue.Count()!=3||b.m_iPending!=3;}
 public static bool UnboundBorrow(){Fixture.Reset();Fixture.Manager.QueueSpawn(Fixture.Shop,"foreign.et",Game.Current.Players.Controller.Controlled,Fixture.Slot.Owner,Fixture.Slot.Rally);return Fixture.Manager.NativeQueueCalls!=0;}
 public static bool ConstructorRefund(){Fixture.Reset();Fixture.Entry.Catalog.Type=EEntityCatalogType.CHARACTER;Fixture.Entry.Data.N=1;var b=Fixture.Capture();b.BindSlot(Fixture.Slot);Game.Current.SpawnResult=Fixture.NewActor();bool cancelled=false;Game.Current.OnSpawn=()=>{cancelled=Game.Current.Service.CancelExternalCreationBeforeFactory(Game.Current.Service.Current);};Fixture.Shop.AICF_QuotaCatalogFactory(b,"group.et",Fixture.Slot);return cancelled||Game.Current.Service.Bodies!=1;}
 public static bool ParentCounterWindow(){Fixture.Reset();var b=Fixture.Capture();if(!AICF_AIQuotaCatalogParents.Enter(b))throw new Exception("parent enter");b.CancelRemaining("callback");bool allowed=Fixture.Capture()!=null;AICF_AIQuotaCatalogParents.Leave(b);return allowed;}
 public static bool VehicleKindBypass(){Fixture.Reset();Resource.VehiclePrefab=false;Resource.SourceMissing=false;Resource.OnLoad=null;Fixture.Shop.m_aChildSlots.Add(Fixture.Slot);Game.Current.SpawnResult=Fixture.NewActor();Fixture.Shop.SpawnEntity("character.et",Fixture.Slot.Owner);return Game.Current.NativeFactories!=0;}
 public static bool VehicleStaleMetadata(){Fixture.Reset();Resource.VehiclePrefab=true;Resource.SourceMissing=false;Fixture.Shop.m_aChildSlots.Add(Fixture.Slot);Game.Current.SpawnResult=Fixture.NewActor();Resource.OnLoad=()=>{Fixture.Shop.Owner.Rpl.Value.Value="changed";};Fixture.Shop.SpawnEntity("vehicle.et",Fixture.Slot.Owner);Resource.OnLoad=null;Resource.VehiclePrefab=false;return Game.Current.NativeFactories!=0;}
}
'@
$methods=@()
$code+=@'
public static class ParentPipelineCases{
 public static bool CrossProducerCancel(){Fixture.Reset();var first=Fixture.Shop;var second=new SCR_CatalogEntitySpawnerComponent{Owner=Fixture.Entity<IEntity>(),Entry=Fixture.Entry,Slot=Fixture.Slot};SCR_CatalogEntitySpawnerComponent.INSTANCES.Add(second);var c=Game.Current.Players.Controller;first.OnParent=b=>{b.CancelRemaining("inside-native-callback");second.InvokeActualParent(Fixture.Entry,c.Controlled);};first.InvokeActualParent(Fixture.Entry,c.Controlled);return first.ParentBodyCalls==1&&second.ParentBodyCalls==0&&second.Notifications==1&&Game.Current.NativeFactories==0&&c.Request.Writes==0&&!AICF_AIQuotaCatalogParents.HasParent(c,c.Request,c.PlayerId,Game.Current.World);}
 public static bool Sequential(){Fixture.Reset();var shop=Fixture.Shop;var c=Game.Current.Players.Controller;shop.OnParent=b=>{b.m_iPending=0;};shop.InvokeActualParent(Fixture.Entry,c.Controlled);shop.InvokeActualParent(Fixture.Entry,c.Controlled);return shop.ParentBodyCalls==2&&shop.m_aAICFQuotaBaskets.Count()==0&&!AICF_AIQuotaCatalogParents.HasParent(c,c.Request,c.PlayerId,Game.Current.World);}
 public static bool OtherPlayer(){Fixture.Reset();var shop=Fixture.Shop;var original=Game.Current.Players.Controller;int depth=0;shop.OnParent=b=>{depth++;if(depth==1){var other=Fixture.Entity<SCR_PlayerController>();other.PlayerId=2;other.Controlled=Fixture.NewActor();other.GroupComponent.Group=Fixture.NewGroup();other.GroupComponent.Group.Slave=Fixture.NewGroup();Game.Current.Players.Controller=other;shop.InvokeActualParent(Fixture.Entry,other.Controlled);Game.Current.Players.Controller=original;}b.m_iPending=0;depth--;};shop.InvokeActualParent(Fixture.Entry,original.Controlled);return shop.ParentBodyCalls==2&&shop.Notifications==0&&shop.m_aAICFQuotaBaskets.Count()==0&&!AICF_AIQuotaCatalogParents.HasParent(original,original.Request,original.PlayerId,Game.Current.World);}
}
'@
foreach($spec in @(
 @('bool','AICF_QuotaManagerCurrent',''),@('bool','AICF_QuotaHasNativeRequests',''),@('bool','AICF_IsNativeQuotaQueueEmpty',''),@('bool','AICF_QuotaIsHead','SCR_SpawnerAIRequest request'),
 @('bool','AICF_RemoveBoundCatalogRequest','AICF_AIQuotaCatalogRequest context'),@('bool','AICF_QueueBoundCatalog','AICF_AIQuotaCatalogBasket basket,int index,string resName,IEntity user,IEntity slotEntity,SCR_EntityLabelPointComponent rallyPoint'),
 @('override void','QueueSpawn','SCR_CatalogEntitySpawnerComponent spawner,string resName,IEntity user,IEntity slotEntity,SCR_EntityLabelPointComponent rallyPoint'),@('override void','EOnFrame','IEntity owner,float timeSlice')
)){$methods+='public '+$spec[0]+' '+$spec[1]+'('+$spec[2]+')'+(Adapt (Body $queue $spec[1]))}
$factory=@()
foreach($spec in @(@('IEntity','AICF_QuotaCatalogFactory','AICF_AIQuotaCatalogBasket basket,string entityResourceName,SCR_EntitySpawnerSlotComponent slot,AICF_AIQuotaCatalogRequest request=null'),@('bool','AICF_QuotaFactoryCurrent','AICF_AIQuotaCreation creation'),@('IEntity','AICF_QuotaPinnedCatalogFactory','AICF_AIQuotaCreation creation,string entityResourceName,IEntity slotOwner,out bool nativeCalled'))){$factory+='public '+$spec[0]+' '+$spec[1]+'('+$spec[2]+')'+(Adapt (Body $component $spec[1]))}
$factory+='public bool CanRequestAI(IEntity user,int aiCount=1)'+(Adapt (Body $pinned CanRequestAI))
$factory+='public override IEntity SpawnEntity(string entityResourceName,IEntity slotOwner)'+(Adapt (Body $component SpawnEntity))
$factory+='public bool AICF_QuotaVehicleFactoryCurrent(AICF_AIWorldQuotaService quota,int epoch,AICF_AIQuotaCatalogIdentity producer,AICF_AIQuotaCatalogIdentity slot)'+(Adapt (Body $component AICF_QuotaVehicleFactoryCurrent))
$factory+=@'
public bool m_bAICFCatalogStopping;
public NativeArray<SCR_EntitySpawnerSlotComponent> m_aChildSlots=new NativeArray<SCR_EntitySpawnerSlotComponent>(),m_aNearSlots=new NativeArray<SCR_EntitySpawnerSlotComponent>();
public NativeArray<AICF_AIQuotaCatalogBasket> m_aAICFQuotaBaskets=new NativeArray<AICF_AIQuotaCatalogBasket>();
public Action<AICF_AIQuotaCatalogBasket> OnParent;
public int ParentBodyCalls,Notifications;
public bool AICF_QuotaCharacterCatalog(SCR_EntityCatalogEntry e){return e!=null&&(e.Catalog.Type==EEntityCatalogType.CHARACTER||e.Catalog.Type==EEntityCatalogType.GROUP);}
public SCR_EEntityRequestStatus GetRequestState(SCR_EntityCatalogEntry e,IEntity u){return CanRequestAI(u,e.Data.N)?SCR_EEntityRequestStatus.CAN_SPAWN:SCR_EEntityRequestStatus.NOT_AVAILABLE;}
public void SendNotification(SCR_EEntityRequestStatus s,IEntity u){Notifications++;}
public void AICF_PerformBoundCatalog(AICF_AIQuotaCatalogBasket b,SCR_EntityCatalogEntry e,IEntity u,SCR_EntitySpawnerSlotComponent s){ParentBodyCalls++;if(OnParent!=null)OnParent(b);}
'@
$factory+='public void InvokeActualParent(SCR_EntityCatalogEntry entityEntry,IEntity user=null,SCR_EntitySpawnerSlotComponent preferredSlot=null)'+(Adapt (Body $component PerformSpawn)).Replace('string quotaReason;','string quotaReason = null;').Replace('entityEntry, user, quotaReason)','entityEntry, user, out quotaReason)')
$compiled=$code.Replace('BASKET_SOURCE',(Adapt $basket)).Replace('MANAGER_METHODS',($methods-join"`n")).Replace('FACTORY_METHODS',($factory-join"`n"))
Add-Type -TypeDefinition $compiled
$unsafe=@(
 @('PartialBasket',$compiled.Replace('service.ReserveUnits(AICF_EAIQuotaBucket.STOCK, basket.m_iQuoted, basket.m_iSerial)','service.ReserveUnits(AICF_EAIQuotaBucket.STOCK, 1, basket.m_iSerial)')),
 @('QuarantineIdentityLoss',$compiled.Replace('if (m_bCancelled || !m_Service || !m_Service.IsOwnerCurrent() || m_Service.GetEpoch() != m_iEpoch)','if (m_bCancelled || !m_Service || !m_Service.IsConfigured() || !m_Service.IsOwnerCurrent() || m_Service.GetEpoch() != m_iEpoch)')),
 @('ForeignHeadPop',$compiled.Replace('if (AICF_QuotaIsHead(head) && context.MatchesNativeRequest())','if (m_aAIQueue && !m_aAIQueue.IsEmpty())')),
 @('UnboundBorrow',$compiled.Replace((Adapt (Body $queue QueueSpawn)),((Adapt (Body $queue QueueSpawn)).Replace('if (!quota)','if (Game.Current != null)'))))
 ,@('ConstructorRefund',$compiled.Replace('if (!creation.m_Service.MarkExternalNativeCallStarted(creation)) return null;',''))
 ,@('ParentCounterWindow',$compiled.Replace('if (parent.m_Controller == controller || parent.m_RequestComponent == request || parent.m_iPlayerId == playerId) return true;','if (parent.m_Controller == controller || parent.m_RequestComponent == request || parent.m_iPlayerId == playerId) return false;'))
 ,@('VehicleKindBypass',$compiled.Replace('source && source.IsVehicle && AICF_QuotaVehicleFactoryCurrent','source && AICF_QuotaVehicleFactoryCurrent'))
 ,@('VehicleStaleMetadata',$compiled.Replace('source && source.IsVehicle && AICF_QuotaVehicleFactoryCurrent(quota, epoch, producer, slot)','source && source.IsVehicle'))
 ,@('UserMissingRpl',$compiled.Replace('if (!m_User.m_Rpl || !m_User.m_Rpl.Id().IsValid()) return false;',''))
 ,@('UserInvalidRpl',$compiled.Replace('!m_User.m_Rpl.Id().IsValid()','false'))
 ,@('UserProxyRejected',$compiled.Replace('AICF_AIQuotaCatalogIdentity.Capture(user, false)','AICF_AIQuotaCatalogIdentity.Capture(user, true)'))
)
$executedUnsafe=0
foreach($mutation in $unsafe){
 if([UnsafeChecks].GetMethod($mutation[0]).Invoke($null,@())){throw "Unmodified source failed ownership boundary $($mutation[0])"}
 if($mutation[1]-ceq$compiled){throw "Missing executable source mutation $($mutation[0])"}
 $ns='UnsafeCatalog'+$mutation[0]
 Add-Type -TypeDefinition ('namespace '+$ns+' {'+"`n"+$mutation[1]+"`n"+'}')
 $type=[type]($ns+'.UnsafeChecks')
 if(!$type.GetMethod($mutation[0]).Invoke($null,@())){throw "Unsafe catalog mutation did not demonstrate violation $($mutation[0])"};$executedUnsafe++
}
$cases=0
function Check($ok,$name){if(!$ok){throw "Catalog actual $name"};$script:cases++}
[Fixture]::Reset();$b=[Fixture]::Capture();Check ($b-and$b.m_iQuoted-eq5-and[Game]::Current.Service.Reserved-eq5) 'reserve whole authored5';Check ($b.BindSlot([Fixture]::Slot)) 'real static slot metadata through exact master producer';Check $b.IsSlotCurrent() 'controller optionalRpl role0 remains valid'
foreach($test in @('budget4','authority','producerProxy','userMissingRpl','userInvalidRpl','controllerChanged','catalogChanged','countChanged','priceChanged','prefabChanged','worldChanged','reserveCallback')){
 [Fixture]::Reset();$s=[Game]::Current.Service
 switch($test){budget4{$s.Limit=4} authority{[Replication]::Server=$false} producerProxy{[Fixture]::Shop.Owner.Rpl.Master=$false} userMissingRpl{[Game]::Current.Players.Controller.Controlled.Rpl=$null} userInvalidRpl{[Game]::Current.Players.Controller.Controlled.Rpl.Value.Valid=$false} reserveCallback{$s.AfterReserve=[Action]{[Fixture]::Shop.Owner.Deleted=$true}}}
 $b=[Fixture]::Capture()
 if($test-in@('budget4','authority','producerProxy','userMissingRpl','userInvalidRpl','reserveCallback')){Check (!$b) $test;continue}
 switch($test){controllerChanged{[Game]::Current.Players.Controller.PlayerId=2} catalogChanged{[Fixture]::Shop.Entry=New-Object SCR_EntityCatalogEntry} countChanged{[Fixture]::Entry.Data.N=4} priceChanged{[Fixture]::Entry.Data.Cost=201} prefabChanged{[Fixture]::Entry.Prefab='other.et'} worldChanged{[Fixture]::Shop.Owner.World=New-Object BaseWorld}}
 Check (!$b.IsCurrent()) $test
}
foreach($master in @($false,$true)){
 [Fixture]::Reset();[Game]::Current.Players.Controller.Controlled.Rpl.Master=$master;$b=[Fixture]::Capture()
 Check ([bool]$b-and$b.IsCurrent()) ('actual full player capture supports native master role '+$master)
 foreach($change in @('absentRpl','invalidRpl','replacedRpl','changedRplId','otherBody','playerId','otherController','groupChanged','lostLeader','slaveChanged')){
  [Fixture]::Reset();$c=[Game]::Current.Players.Controller;$c.Controlled.Rpl.Master=$master;$b=[Fixture]::Capture()
  switch($change){
   absentRpl {$c.Controlled.Rpl=$null}
   invalidRpl {$c.Controlled.Rpl.Value.Valid=$false}
   replacedRpl {$c.Controlled.Rpl=New-Object RplComponent}
   changedRplId {$c.Controlled.Rpl.Value.Value='changed'}
   otherBody {$c.Controlled=[Fixture]::NewActor()}
   playerId {$c.PlayerId=2}
   otherController {[Game]::Current.Players.Controller=New-Object SCR_PlayerController}
   groupChanged {$c.GroupComponent.Group=[Fixture]::NewGroup()}
   lostLeader {$c.GroupComponent.Leader=$false}
   slaveChanged {$c.GroupComponent.Group.Slave=[Fixture]::NewGroup()}
  }
  Check (!$b.IsCurrent()) ('exact player identity remains required role '+$master+' '+$change)
 }
}
[Fixture]::Reset();$b=[Fixture]::Capture();[Game]::Current.Service.Configured=$false;Check $b.IsCurrent() 'quota quarantine/readiness is not request identity loss'
[Fixture]::Reset();[Fixture]::Entry.Catalog.Type=[EEntityCatalogType]::CHARACTER;Check (![Fixture]::Capture()) 'unproven character multiple-body shape rejected';[Fixture]::Entry.Data.N=1;Check ([bool][Fixture]::Capture()) 'character authored1 admitted'
foreach($test in @('fifo','transient','headReplaced','pointerTag','userChanged','counterNotAdded','inactivePolicy','unboundQueue','cancelAll','producerDeleted')){
 [Fixture]::Reset();$b=[Fixture]::Queue(3);$m=[Fixture]::Manager;$head=$m.m_aAIQueue[0]
 switch($test){
  transient{[Fixture]::Shop.Wait=$true}
  headReplaced{[Fixture]::Shop.OnDispatch=[Func[AICF_AIQuotaCatalogRequest,bool]]{param($r)$m.m_aAIQueue.RemoveOrdered(0);$m.m_aAIQueue.Insert($head);$r.m_Request.m_AICFQuotaRequest=$null;return $true}.GetNewClosure()}
  pointerTag{$head.m_AICFQuotaRequest=$null}
  userChanged{[Game]::Current.Players.Controller.Controlled=[Fixture]::NewActor()}
  counterNotAdded{$b.m_bCounterAdded=$false}
  inactivePolicy{[Game]::Current.Service.Policy=$false}
  cancelAll{$b.CancelRemaining('controlled')}
  producerDeleted{[Fixture]::Shop.Owner.Deleted=$true;$b.CancelRemaining('controlled')}
  unboundQueue{$m.QueueSpawn([Fixture]::Shop,'foreign.et',$b.m_User.m_Entity,[Fixture]::Slot.Owner,[Fixture]::Slot.Rally)}
 }
 if($test-notin@('cancelAll','producerDeleted')){$m.EOnFrame($m.Owner,2)}
 switch($test){
  fifo{Check ($m.m_aAIQueue.Count-eq2-and$b.m_iPending-eq2-and[Fixture]::Shop.DispatchCalls-eq1) 'FIFO one exacthead only'}
  transient{Check ($m.m_aAIQueue[0]-eq$head-and$b.m_iPending-eq3-and[Fixture]::Shop.DispatchCalls-eq1) 'transient retains samehead/fullremaining'}
  headReplaced{Check ($m.m_aAIQueue.Count-eq3-and$m.m_aAIQueue[2]-eq$head-and$b.m_iPending-eq3) 'foreign successor not popped after callback'}
  pointerTag{Check ($m.m_aAIQueue[0]-eq$head-and[Fixture]::Shop.DispatchCalls-eq0) 'unknown request not adopted'}
  userChanged{Check ($m.m_aAIQueue.Count-eq0-and[Fixture]::Shop.DispatchCalls-eq0-and$b.m_RequestComponent.Queued-eq0) 'old ownedpending cancelled after userreplacement'}
  counterNotAdded{Check ([Fixture]::Shop.DispatchCalls-eq0-and$m.m_aAIQueue.Count-eq3) 'no body before native queued counter commit'}
  inactivePolicy{Check ($m.NativeFrameCalls-eq1-and[Fixture]::Shop.DispatchCalls-eq0) 'inactive policy delegatesnative frame'}
  unboundQueue{Check ($m.NativeQueueCalls-eq0-and$m.m_aAIQueue.Count-eq2) 'unbound doesnot inheritbasket'}
  cancelAll{Check ($m.m_aAIQueue.Count-eq0-and$b.m_RequestComponent.Queued-eq0-and[Game]::Current.Service.Reserved-eq0) 'cancel exact fullbasket'}
  producerDeleted{Check ($m.m_aAIQueue.Count-eq0-and$b.m_RequestComponent.Queued-eq0) 'producer deletion removes only knownrequests'}
 }
}
foreach($test in @('header5','header4','character','nativeNull','invalidResource','staleDuringNative','validBindFailure','nestedUnbound')){
 [Fixture]::Reset();$g=[Fixture]::NewGroup();for($i=0;$i-lt5;$i++){$g.m_aUnitPrefabSlots.Insert("m$i.et")};[Game]::Current.SpawnResult=$g
 if($test-in@('character','nativeNull','invalidResource','staleDuringNative','validBindFailure','nestedUnbound')){[Fixture]::Entry.Catalog.Type=[EEntityCatalogType]::CHARACTER;[Fixture]::Entry.Data.N=1;[Game]::Current.SpawnResult=[Fixture]::NewActor()}
 switch($test){header4{$g.m_aUnitPrefabSlots.RemoveOrdered(4)} nativeNull{[Game]::Current.SpawnResult=$null} invalidResource{[Resource]::Valid=$false} staleDuringNative{[Game]::Current.OnSpawn=[Action]{[Fixture]::Shop.Owner.Deleted=$true}} validBindFailure{[Game]::Current.Service.BindFailure=$true} nestedUnbound{[Game]::Current.OnSpawn=[Action]{[Fixture]::Manager.QueueSpawn([Fixture]::Shop,'nested.et',[Game]::Current.Players.Controller.Controlled,[Fixture]::Slot.Owner,[Fixture]::Slot.Rally)}}}
 $b=[Fixture]::Capture();$null=$b.BindSlot([Fixture]::Slot);$entity=[Fixture]::Shop.AICF_QuotaCatalogFactory($b,'group.et',[Fixture]::Slot,$null)
 switch($test){
  header5{Check ($entity-eq$g-and$b.m_bHeaderObserved-and$b.m_aMemberPrefabs.Count-eq5-and[Game]::Current.Service.Reserved-eq5) 'empty header validatesfullN withoutconsumingunit'}
  header4{Check (!$entity-and$g.Deleted-and![Game]::Current.Service.Busy) 'mismatched newemptyheader rolledback beforecash'}
  character{Check ($entity-and[Game]::Current.Service.Bodies-eq1-and![Game]::Current.Service.Busy) 'exactbody bound before activationcallback'}
  nativeNull{Check (!$entity-and![Game]::Current.Service.Busy) 'actual nativeNULL balanced'}
  invalidResource{Check (!$entity-and[Game]::Current.NativeFactories-eq0-and![Game]::Current.Service.Busy) 'invalid resource cancels beforeactualnativecall'}
  staleDuringNative{Check (!$entity-and[Game]::Current.SpawnResult.Deleted-and![Game]::Current.Service.Busy) 'stale exactunassignedbody rolledback'}
  validBindFailure{Check (!$entity-and![Game]::Current.SpawnResult.Deleted-and![Game]::Current.Service.Configured) 'validcontext bindfailure quarantined neverdelete-to-fit'}
  nestedUnbound{Check ($entity-and[Fixture]::Manager.NativeQueueCalls-eq0) 'nested unboundrequest denied whilebirthbusy'}
 }
}
for($n=1;$n-le10;$n++){
 [Fixture]::Reset();[Fixture]::Entry.Data.N=$n;[Game]::Current.Service.Limit=$n;$b=[Fixture]::Capture()
 Check ($b-and$b.m_iQuoted-eq$n-and[Game]::Current.Service.Reserved-eq$n-and[Game]::Current.NativeFactories-eq0) ('full group basket exactbudget '+$n)
 [Fixture]::Reset();[Fixture]::Entry.Data.N=$n;[Game]::Current.Service.Limit=$n-1
 Check (![Fixture]::Capture()) ('no partial group basket budgetminus1 '+$n)
}
foreach($bad in @(0,-1,4097,[int]::MaxValue)){
 [Fixture]::Reset();[Fixture]::Entry.Data.N=$bad
 Check (![Fixture]::Capture()) ('bounded invalid authored cardinality '+$bad)
 Check ([Game]::Current.Service.ReserveCalls-eq0) ('no reservation for invalid cardinality '+$bad)
}
foreach($change in @('groupComponent','playerGroup','slaveGroup','leader','controllerRpl','producerRpl','userRpl','slotRpl','rallyRpl','groupRpl','slaveRpl','roleType','requestComponent')){
 [Fixture]::Reset();$b=[Fixture]::Capture();$null=$b.BindSlot([Fixture]::Slot);$c=[Game]::Current.Players.Controller
 switch($change){
  groupComponent {$c.GroupComponent=New-Object SCR_PlayerControllerGroupComponent}
  playerGroup {$c.GroupComponent.Group=[Fixture]::NewGroup()}
  slaveGroup {$c.GroupComponent.Group.Slave=[Fixture]::NewGroup()}
  leader {$c.GroupComponent.Leader=$false}
  controllerRpl {$c.Rpl.Value.Value='reused'}
  producerRpl {[Fixture]::Shop.Owner.Rpl.Value.Value='reused'}
  userRpl {$c.Controlled.Rpl.Value.Value='reused'}
  slotRpl {[Fixture]::Slot.Owner.Rpl=New-Object RplComponent}
  rallyRpl {[Fixture]::Slot.Rally.Owner.Rpl=New-Object RplComponent}
  groupRpl {$c.GroupComponent.Group.Rpl.Value.Value='reused'}
  slaveRpl {$c.GroupComponent.Group.Slave.Rpl.Value.Value='reused'}
  roleType {[Fixture]::Entry.Catalog.Type=[EEntityCatalogType]::CHARACTER}
  requestComponent {$c.Request=New-Object SCR_SpawnerRequestComponent}
 }
 Check (!$b.IsSlotCurrent()) ('exact user/group/controller/slot metadata fence '+$change)
}
[Fixture]::Reset();$b=[Fixture]::Queue(3);$b.m_RequestComponent.Queued+=2;$b.CancelRemaining('owned-only')
Check ($b.m_RequestComponent.Queued-eq2-and[Fixture]::Manager.m_aAIQueue.Count-eq0) 'cancellation retains other baskets nativecounter'
[Fixture]::Reset();$b=[Fixture]::Queue(3);$r=$b.m_aRequests[0];$r.m_bDispatching=$true;$r.m_bMaterialized=$true;$b.CancelRemaining('during-native')
Check ($b.m_RequestComponent.Queued-eq1-and[Fixture]::Manager.m_aAIQueue.Count-eq1) 'inflight materialized member is not counted as cancelled unspawned'
$r.m_bDispatching=$false;$null=[Fixture]::Manager.AICF_RemoveBoundCatalogRequest($r);$b.RequestFinished($r,$true)
Check ($b.m_iPending-eq0-and$b.m_RequestComponent.Queued-eq1) 'materialized native member retains normal enroute counter'
[Fixture]::Reset();Check ([Fixture]::Manager.AICF_IsNativeQuotaQueueEmpty()) 'bootstrap only empty nativequeue ready';$b=[Fixture]::Queue(1)
Check (![Fixture]::Manager.AICF_IsNativeQuotaQueueEmpty()) 'prepolicy paid queue must drain before publish'
Check (![UnsafeChecks]::ConstructorRefund()) 'constructor cannot refund already started native creation'
[Fixture]::Reset();$controller=[Game]::Current.Players.Controller;$user=$controller.Controlled;$shop=[Fixture]::Shop
Check ($shop.CanRequestAI($user,5)) 'actual pinned native initial group capacity allows five'
$parent=[Fixture]::Capture();Check ([AICF_AIQuotaCatalogParents]::Enter($parent)) 'exact parent frame acquires counter lock'
$reserveCalls=[Game]::Current.Service.ReserveCalls
Check ($shop.CanRequestAI($user,5)) 'actual native callback window still has zero queued counter'
Check (![Fixture]::Capture()) 'same player nested full basket veto before factory or debit'
Check ([Game]::Current.Service.ReserveCalls-eq$reserveCalls-and[Game]::Current.NativeFactories-eq0) 'nested parent veto has no reservation or native creation side effects'
$parent.CancelRemaining('native-callback-cancel')
Check (![Fixture]::Capture()) 'cancelled parent retains lock until actual parent return'
[SCR_CatalogEntitySpawnerComponent]::INSTANCES.Clear()
Check ([AICF_AIQuotaCatalogParents]::HasParent($controller,$controller.Request,1,[Game]::Current.World)) 'producer deletion registry removal does not release live parent frame'
[SCR_CatalogEntitySpawnerComponent]::INSTANCES.Add($shop)
$other=New-Object SCR_PlayerController;$other.Id=[Fixture]::NextId;[Fixture]::NextId++;$other.World=[Game]::Current.World;$other.PlayerId=2
$other.Rpl.Value.Value='other-controller';[Game]::Current.World.Entities.Add($other.Id,$other)
$other.Controlled=[Fixture]::NewActor();$other.GroupComponent.Group=[Fixture]::NewGroup();$other.GroupComponent.Group.Slave=[Fixture]::NewGroup()
[Game]::Current.Players.Controller=$other;$independent=[Fixture]::Capture()
Check ([bool]$independent) 'different player native counter remains independent'
$independent.CancelRemaining('independent-finished');[Game]::Current.Players.Controller=$controller
$controller.Request.AddQueuedAI(5);[AICF_AIQuotaCatalogParents]::Leave($parent)
Check (![AICF_AIQuotaCatalogParents]::HasParent($controller,$controller.Request,1,[Game]::Current.World)) 'only outer return releases frame lock'
Check (!$shop.CanRequestAI($user,5)) 'after return actual pinned native queued counter prevents second full five'
$controller.Request.AddQueuedAI(-5)
Check ($shop.CanRequestAI($user,5)-and[bool][Fixture]::Capture()) 'sequential request after normal native capacity frees remains allowed'
Check ([ParentPipelineCases]::CrossProducerCancel()) 'actual Perform wrapper blocks cancelled-parent cross-producer nested body before factory or debit'
Check ([ParentPipelineCases]::Sequential()) 'actual Perform wrapper all early parent returns release marker and basket'
Check ([ParentPipelineCases]::OtherPlayer()) 'actual Perform wrapper allows independent player nested parent and balances both markers'
function VehicleFactorySetup {
 [Fixture]::Reset();[Resource]::VehiclePrefab=$true;[Resource]::SourceMissing=$false;[Resource]::OnLoad=$null
 [Fixture]::Shop.m_aChildSlots.Add([Fixture]::Slot);[Game]::Current.SpawnResult=[Fixture]::NewActor()
}
VehicleFactorySetup;[Game]::Current.Service.Limit=0;[Game]::Current.Service.Configured=$false
$vehicleResult=[Fixture]::Shop.SpawnEntity('verified-vehicle.et',[Fixture]::Slot.Owner)
Check ([bool]$vehicleResult-and[Game]::Current.NativeFactories-eq1-and[Game]::Current.Service.ReserveCalls-eq0) 'verified vehicle metadata delegates without character basket even when character quota is full or quarantined'
foreach($case in @('characterMetadata','sourceMissing','invalidResource','producerProxy','slotProxy','unknownSlot','stopped','producerReplacedDuringMetadata','epochChangedDuringMetadata','slotRemovedDuringMetadata','policyChangedDuringMetadata')){
 VehicleFactorySetup
 switch($case){
  characterMetadata {[Resource]::VehiclePrefab=$false}
  sourceMissing {[Resource]::SourceMissing=$true}
  invalidResource {[Resource]::Valid=$false}
  producerProxy {[Fixture]::Shop.Owner.Rpl.Master=$false}
  slotProxy {[Fixture]::Slot.Owner.Rpl=New-Object RplComponent;[Fixture]::Slot.Owner.Rpl.Master=$false}
  unknownSlot {[Fixture]::Shop.m_aChildSlots.Clear()}
  stopped {[Fixture]::Shop.m_bAICFCatalogStopping=$true}
  producerReplacedDuringMetadata {[Resource]::OnLoad={[Fixture]::Shop.Owner.Rpl.Value.Value='changed'}}
  epochChangedDuringMetadata {[Resource]::OnLoad={[Game]::Current.Service.Epoch++}}
  slotRemovedDuringMetadata {[Resource]::OnLoad={[Fixture]::Shop.m_aChildSlots.Clear()}}
  policyChangedDuringMetadata {[Resource]::OnLoad={[Game]::Current.Service.Policy=$false}}
 }
 $result=[Fixture]::Shop.SpawnEntity('metadata.et',[Fixture]::Slot.Owner)
 Check (!$result-and[Game]::Current.NativeFactories-eq0-and[Game]::Current.Service.ReserveCalls-eq0) ('metadata vehicle delegate guard '+$case)
}
[Resource]::OnLoad=$null;[Resource]::VehiclePrefab=$false;[Resource]::SourceMissing=$false
"PASS Catalog quota: pinned_projections=$sourceControls actual_context_fifo_factory_cases=$cases executed_unsafe_mutants=$executedUnsafe native_profiles=NOT_RUN"
