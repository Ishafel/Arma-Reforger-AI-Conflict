param([string]$RepositoryRoot=(Split-Path -Parent (Split-Path -Parent $PSScriptRoot)),[string]$BeforeService)
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot '../lib/Stage3StaticAudit.Common.ps1')
$forces=Join-Path $RepositoryRoot 'AIConflictCore/Scripts/Game/AIConflict/Forces'
$set=[IO.File]::ReadAllText((Join-Path $forces 'AICF_AIQuotaVariantSet.c'))
$service=[IO.File]::ReadAllText((Join-Path $forces 'AICF_AIWorldQuotaService.c'))
function Body([string]$source,[string]$name){Get-AICFMethodBody ([pscustomobject]@{Source=$source;Code=(ConvertTo-AICFCodeText $source)}) $name}
$observe=Body $service 'ObserveGroupFactory'
if($BeforeService){$observe=Body ([IO.File]::ReadAllText($BeforeService)) 'ObserveGroupFactory'}
$fence=Body $service 'IsGroupObservationCurrent'
$set=$set.Replace('protected ref array<ResourceName> m_aAllowed = {};','protected EArray<string> m_aAllowed = new EArray<string>();')
$set=$set.Replace('array<SCR_EditableEntityVariant> variants = {};','EArray<SCR_EditableEntityVariant> variants = new EArray<SCR_EditableEntityVariant>();')
$set=$set.Replace('ResourceName','string').Replace('static AICF_AIQuotaVariantSet Capture','public static AICF_AIQuotaVariantSet Capture').Replace('bool MatchesRoot','public bool MatchesRoot').Replace('bool Contains','public bool Contains')
$set=$set.Replace('source.Get("m_VariantData", data)','source.Get("m_VariantData", out data)').Replace('SCR_EditableEntityVariantData data;','SCR_EditableEntityVariantData data=null;').Replace('int validEntries;','int validEntries=0;')
$set=$set.Replace('foreach (SCR_EditableEntityVariant variant : variants)','foreach (SCR_EditableEntityVariant variant in variants)')
$observe=$observe.Replace('ResourceName','string').Replace('SCR_stringUtils','SCR_ResourceNameUtils').Replace('IEntity','Entity').Replace('AICF_AIQuotaBody','Body')
$fence=$fence.Replace('IEntity','Entity').Replace('AICF_AIQuotaBody','Body')
$replay=@'
using System;
using System.Collections.Generic;
public class N {public static implicit operator bool(N n){return n!=null;}public static bool operator !(N n){return n==null;}}
public class EArray<T>:List<T> {public new int Count(){return base.Count;}public void Insert(T v){Add(v);}public bool IsEmpty(){return base.Count==0;}}
public static class Text {public static bool IsEmpty(this string s){return string.IsNullOrEmpty(s);}}
public class Resource:N {public string Name;public bool Valid=true,Editable=true;public SCR_EditableEntityVariantData Data;public static Dictionary<string,Resource> All=new Dictionary<string,Resource>();public static Action<string> OnLoad;public static Resource Load(string n){if(OnLoad!=null)OnLoad(n);Resource r;return All.TryGetValue(n,out r)?r:new Resource{Valid=false};}public bool IsValid(){return Valid;}}
public class IEntityComponentSource:N {public Resource R;public void Get(string n,out SCR_EditableEntityVariantData d){d=R.Data;}}
public class SCR_EditableEntityComponent { }
public static class SCR_BaseContainerTools {public static IEntityComponentSource FindComponentSource(Resource r,Type t){return r.Editable?new IEntityComponentSource{R=r}:null;}}
public static class SCR_EditableEntityComponentClass {public static IEntityComponentSource GetEditableEntitySource(Resource r){return r.Editable?new IEntityComponentSource{R=r}:null;}}
public class SCR_EditableEntityVariantData:N {public bool m_bRandomizeDefaultVariant;public int m_iDefaultVariantRandomizerWeight;public EArray<SCR_EditableEntityVariant> Items=new EArray<SCR_EditableEntityVariant>();public int GetVariants(EArray<SCR_EditableEntityVariant> a){foreach(var v in Items)if(v!=null&&v.m_bEnabled)a.Add(v);return a.Count();}}
public class SCR_EditableEntityVariant:N {public string m_sVariantPrefab;public int m_iRandomizerWeight=1;public bool m_bEnabled=true;}
'@ + $set.Replace('class AICF_AIQuotaVariantSet','public class AICF_AIQuotaVariantSet:N').Replace('resource, SCR_EditableEntityComponent)','resource, typeof(SCR_EditableEntityComponent))') + @'
public enum AICF_EAIQuotaBirthStatus {IN_NATIVE_FACTORY,NATIVE_NULL,BOUND,QUARANTINED}
public class Entity:N {public string Prefab="variant";}
public class AICF_AIQuotaDispatch:N {public bool m_bClosed;}
public class AICF_AIQuotaGroupQueue:N {public int m_iSerial=1;public N m_Owner=new N();public int GetBucket(){return 0;}}
public class Body:N {public Entity m_Entity;public string Key(){return "exact";}public static Action OnCapture;public static Body Capture(Entity e,int b,N o){if(OnCapture!=null)OnCapture();return new Body{m_Entity=e};}}
public class AICF_AIQuotaBirth:N {public Service m_Service;public AICF_AIQuotaGroupQueue m_Queue=new AICF_AIQuotaGroupQueue();public AICF_AIQuotaDispatch m_Ticket=new AICF_AIQuotaDispatch();public AICF_AIQuotaVariantSet m_Variants;public Body m_Body;public Entity m_Entity;public int m_iSlotIndex;public string m_Prefab="root",m_ProducedPrefab,m_sRejectReason;public bool m_bFactoryObserved,m_bFinished;public AICF_EAIQuotaBirthStatus m_Status;public Action OnRequest;public bool RequestCurrent=true;public bool IsRequestCurrent(){if(OnRequest!=null)OnRequest();return RequestCurrent;}public static bool IsNonPlayerUnassigned(Entity e){return e!=null;}}
public static class AICF_Stage1Diagnostics {public static void Info(string e,string s){}}
public static class SCR_ResourceNameUtils {public static Action OnRead;public static string GetPrefabName(Entity e){if(OnRead!=null)OnRead();return e.Prefab;}}
public static class AICF_AIQuotaMath {public const int MAX_TOTAL_UNITS=4096;}
public class Ledger {public int Bound;public bool FailDispatch(AICF_AIQuotaDispatch t,int s){return true;}public bool Bind(AICF_AIQuotaDispatch t,int s,string key){Bound++;t.m_bClosed=true;return true;}}
public class Service:N {public bool m_bNativeBirthBusy=true;public AICF_AIQuotaBirth m_CurrentBirth,m_QuarantinedBirth;public EArray<Body> m_aBodies=new EArray<Body>();public Ledger m_Ledger=new Ledger();public bool IsCurrentWorld(){return true;}public Body FindBody(Entity e){return null;}public void QuarantineBirth(AICF_AIQuotaBirth b,string r){}public bool RetainQuarantinedGroupReturn(AICF_AIQuotaBirth b,Entity e){return false;}
'@ + 'public void ObserveGroupFactory(AICF_AIQuotaBirth birth,Entity entity)'+$observe+'public bool IsGroupObservationCurrent(AICF_AIQuotaBirth birth,AICF_AIQuotaGroupQueue queue,AICF_AIQuotaDispatch ticket,AICF_AIQuotaVariantSet variants,Body body,Entity entity)'+$fence + @'
}
public static class Cases {
static int n;static Resource R;static Service S;static AICF_AIQuotaBirth B;
static void Need(bool ok,string message){n++;if(!ok)throw new Exception(message);}
static void Reset(){Resource.All.Clear();Resource.OnLoad=null;Body.OnCapture=null;SCR_ResourceNameUtils.OnRead=null;R=new Resource{Name="root",Data=new SCR_EditableEntityVariantData()};Resource.All["root"]=R;Resource.All["variant"]=new Resource{Name="variant"};R.Data.Items.Add(new SCR_EditableEntityVariant{m_sVariantPrefab="variant"});}
static AICF_AIQuotaVariantSet Capture(){return AICF_AIQuotaVariantSet.Capture("root");}
static void Begin(){S=new Service();B=new AICF_AIQuotaBirth{m_Service=S,m_Variants=Capture()};S.m_CurrentBirth=B;}
public static int Run(){
Reset();var a=Capture();Need(a.Contains("variant")&&!a.Contains("root")&&!a.Contains("unknown"),"one-level enabled variant only, no unconditional root");
Need(a.MatchesRoot("root")&&!a.MatchesRoot("variant"),"immutable exact source root");
R.Data.m_bRandomizeDefaultVariant=true;R.Data.m_iDefaultVariantRandomizerWeight=1;a=Capture();Need(a.Contains("root")&&a.Contains("variant"),"positive weighted default allowed");
R.Data.m_iDefaultVariantRandomizerWeight=0;Need(!Capture().Contains("root"),"zero weighted default never allowed");
R.Data.m_iDefaultVariantRandomizerWeight=-1;Need(Capture()==null,"negative default unsupported, no guessed pool");
Reset();R.Data.Items[0].m_bEnabled=false;Need(Capture().Contains("root")&&!Capture().Contains("variant"),"all disabled means native early default fallback");
Reset();Resource.All["variant"].Valid=false;Need(Capture().Contains("root"),"no valid resource entries native fallback");
Reset();Resource.All["variant"].Editable=false;Need(Capture().Contains("root"),"noneditable entry excluded and empty weighted fallback");
Reset();R.Data.Items[0].m_iRandomizerWeight=0;Need(Capture()==null,"valid zero entry is not weighted-array-empty/default fallback");
Reset();R.Data.Items[0].m_iRandomizerWeight=-1;Need(Capture()==null,"negative entry unsupported");
Reset();R.Data=null;Need(Capture().Contains("root"),"missing variant metadata native root fallback");
Reset();R.Editable=false;Need(Capture().Contains("root"),"no editable source native root fallback");
Reset();Resource.All["variant"].Data=new SCR_EditableEntityVariantData();Resource.All["variant"].Data.Items.Add(new SCR_EditableEntityVariant{m_sVariantPrefab="grandchild"});Resource.All["grandchild"]=new Resource();Need(!Capture().Contains("grandchild"),"no recursive descendant family acceptance");
Reset();a=Capture();R.Data.Items.Clear();R.Data.Items.Add(new SCR_EditableEntityVariant{m_sVariantPrefab="other"});Need(a.Contains("variant")&&!a.Contains("other"),"metadata changes after capture cannot broaden immutable set");
Reset();for(int i=0;i<256;i++)R.Data.Items.Add(new SCR_EditableEntityVariant{m_sVariantPrefab="variant"});Need(Capture()==null,"bounded authored entries veto oversized metadata");
Reset();Begin();S.ObserveGroupFactory(B,new Entity());Need(B.m_Status==AICF_EAIQuotaBirthStatus.BOUND&&B.m_ProducedPrefab=="variant"&&S.m_Ledger.Bound==1&&S.m_aBodies.Count()==1,"actual observed native variant binds exactly once (old exact-root check fails)");
Reset();Begin();S.ObserveGroupFactory(B,new Entity{Prefab="unknown"});Need(B.m_Status==AICF_EAIQuotaBirthStatus.QUARANTINED&&S.m_Ledger.Bound==0,"unknown constructor result quarantined without bind");
Reset();Begin();S.ObserveGroupFactory(B,new Entity{Prefab="root"});Need(B.m_Status==AICF_EAIQuotaBirthStatus.QUARANTINED&&S.m_Ledger.Bound==0,"disabled default result quarantined");
Reset();Begin();var old=B;B.OnRequest=()=>{old.m_Variants=AICF_AIQuotaVariantSet.Capture("variant");};S.ObserveGroupFactory(B,new Entity());Need(S.m_Ledger.Bound==0,"variant set replacement during identity getter cannot bind");
Reset();Begin();old=B;B.OnRequest=()=>old.m_bFinished=true;S.ObserveGroupFactory(B,new Entity());Need(S.m_Ledger.Bound==0,"finished callback cannot late-bind");
Reset();Begin();old=B;B.OnRequest=()=>old.m_Status=AICF_EAIQuotaBirthStatus.QUARANTINED;S.ObserveGroupFactory(B,new Entity());Need(S.m_Ledger.Bound==0&&B.m_Status==AICF_EAIQuotaBirthStatus.QUARANTINED,"prior quarantine cannot be overwritten by BOUND");
Reset();Begin();old=B;B.OnRequest=()=>old.m_ProducedPrefab="other";S.ObserveGroupFactory(B,new Entity());Need(S.m_Ledger.Bound==0,"produced fingerprint replacement cannot bind");
Reset();Begin();var entity=new Entity();B.OnRequest=()=>entity.Prefab="other";S.ObserveGroupFactory(B,entity);Need(S.m_Ledger.Bound==0,"actual prefab changed by getter cannot bind");
Reset();Begin();B.RequestCurrent=false;S.ObserveGroupFactory(B,new Entity());Need(S.m_Ledger.Bound==0&&B.m_ProducedPrefab=="variant","stale request refuses bind but retains exact admitted produced fingerprint for existing safe rollback");
Reset();Begin();old=B;var sentinel=new Body();SCR_ResourceNameUtils.OnRead=()=>{old.m_bFinished=true;old.m_Body=sentinel;};S.ObserveGroupFactory(B,new Entity());Need(B.m_Body==sentinel&&S.m_Ledger.Bound==0,"prefab getter finished callback cannot overwrite body");
Reset();Begin();old=B;sentinel=new Body();Body.OnCapture=()=>{old.m_bFinished=true;old.m_Body=sentinel;};S.ObserveGroupFactory(B,new Entity());Need(B.m_Body==sentinel&&S.m_Ledger.Bound==0,"body constructor callback cannot overwrite successor body");
Reset();Begin();old=B;B.OnRequest=()=>{old.m_bFinished=true;old.m_Status=AICF_EAIQuotaBirthStatus.BOUND;old.RequestCurrent=false;};S.ObserveGroupFactory(B,new Entity());Need(B.m_Status==AICF_EAIQuotaBirthStatus.BOUND&&S.m_Ledger.Bound==0,"failure validation callback cannot overwrite finished status");
Reset();Begin();S.ObserveGroupFactory(B,null);Need(B.m_Status==AICF_EAIQuotaBirthStatus.NATIVE_NULL&&S.m_Ledger.Bound==0,"actual native null remains distinct");
return n;}}
'@
Add-Type -TypeDefinition $replay -Language CSharp
$count=[Cases]::Run()
$unsafe=0
foreach($mutation in @(
 @('universal','return !prefab.IsEmpty() && m_aAllowed.Contains(prefab);','return !prefab.IsEmpty();'),
 @('zero_fallback','if (validEntries == 0)','if (result.m_aAllowed.IsEmpty())'),
 @('zero_default','data.m_iDefaultVariantRandomizerWeight > 0','data.m_iDefaultVariantRandomizerWeight >= 0'),
 @('set_replacement','birth.m_Variants == variants &&',''),
 @('finished','birth.m_bFinished || birth.m_Status','false || birth.m_Status'),
 @('produced_replacement',' || birth.m_ProducedPrefab != produced',''),
 @('prefab_change','SCR_ResourceNameUtils.GetPrefabName(entity) != produced || ',''),
 @('late_body_write','if (!IsGroupObservationCurrent(birth, queue, ticket, variants, priorBody, entity)) return;','')
)){
 if(!$replay.Contains($mutation[1])){throw "Missing actual mutant anchor $($mutation[0])"}
 $unsafe++;$namespace='VariantUnsafe'+$unsafe
 $code='namespace '+$namespace+' {'+$replay.Replace($mutation[1],$mutation[2])+'}'
 $types=Add-Type -TypeDefinition $code -Language CSharp -PassThru
 $type=@($types|Where-Object FullName -eq ($namespace+'.Cases'))[0]
 $rejected=$false
 try{$null=$type.GetMethod('Run').Invoke($null,@())}catch{$rejected=$true}
 if(!$rejected){throw "Actual unsafe mutation survived $($mutation[0])"}
}
$begin=Body $service 'BeginGroupBirth';$rollback=Body $service 'CanRollbackGroupBirth';$current=Body $service 'IsGroupBirthCurrent'
if($begin.IndexOf('AICF_AIQuotaVariantSet.Capture(prefab)')-ge$begin.IndexOf('queue.BeginDispatch(m_Ledger)')){throw 'Variant capture must precede dispatch'}
if(@([regex]::Matches($begin,'nativeWorld.CanAIBeActivated\(\)')).Count-ne2-or$begin.LastIndexOf('nativeWorld.CanAIBeActivated()')-lt$begin.IndexOf('AICF_AIQuotaVariantSet.Capture(prefab)')){throw 'Native admission must repeat after metadata callbacks'}
if($rollback-notmatch'GetPrefabName\(birth.m_Entity\) == birth.m_ProducedPrefab'-or$current-notmatch'GetPrefabName\(birth.m_Entity\) != birth.m_ProducedPrefab'){throw 'Post-birth identity must retain exact actual produced prefab'}
$native=[IO.File]::ReadAllText((Join-Path $RepositoryRoot 'AIConflictCore/Scripts/Game/AIConflict/Integration/AICF_AIQuotaNativeGroup.c'))
if($native-notmatch'SpawnEntityPrefabEx\(res, true, world, spawnParams\)'){throw 'Pinned native randomizer chain changed'}
"PASS AI quota variants actual_cases=$count executed_unsafe=$unsafe source_fences=4 native_rng_draws=0 native=NOT_RUN"
