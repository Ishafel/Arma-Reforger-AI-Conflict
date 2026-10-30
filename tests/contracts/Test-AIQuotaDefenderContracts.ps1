param([string]$RepositoryRoot=(Split-Path -Parent (Split-Path -Parent $PSScriptRoot)))
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot '../lib/Stage3StaticAudit.Common.ps1')
function Body([string]$text,[string]$name){Get-AICFMethodBody ([pscustomobject]@{Source=$text;Code=(ConvertTo-AICFCodeText $text)}) $name}
function Tokens([string]$text){@([regex]::Matches($text,'(?s)/\*.*?\*/|//[^\r\n]*|"(?:\\.|[^"\\])*"|[A-Za-z_]\w*|\d+(?:\.\d+)?|[^\s]')|Where-Object{!$_.Value.StartsWith('//')-and!$_.Value.StartsWith('/*')}|ForEach-Object{$_.Value}) -join "`n"}
$pinned=[IO.File]::ReadAllText((Join-Path $RepositoryRoot '.cache/reforger-api/Arma-Reforger-Script-Diff-1.8.0.13/scripts/Game/Components/Spawner/CatalogSpawner/SCR_DefenderSpawnerComponent.c')).Replace("`r`n","`n")
$adapter=[IO.File]::ReadAllText((Join-Path $RepositoryRoot 'AIConflictCore/Scripts/Game/AIConflict/Integration/AICF_AIQuotaDefender.c')).Replace("`r`n","`n")
$basket=[IO.File]::ReadAllText((Join-Path $RepositoryRoot 'AIConflictCore/Scripts/Game/AIConflict/Forces/AICF_AIQuotaDefenderBasket.c')).Replace("`r`n","`n")
function Projection([string]$text,[string]$name){
 $method=Body $text $name
 $expected=@('FACTORY_BEGIN','FACTORY_OBSERVE');if($name-eq'AICF_QuotaHandleDefenderGroup'){$expected=@('FREE_CREDIT','PAID_DISPATCH','DISTANT_CANCEL')}
 $found=@([regex]::Matches($method,'(?m)^\s*// AICF_QUOTA_BLOCK_BEGIN ([A-Z_]+)\s*$')|ForEach-Object{$_.Groups[1].Value})
 if(@(Compare-Object @($expected|Sort-Object) @($found|Sort-Object)).Count -or @($found|Sort-Object -Unique).Count-ne$expected.Count){throw 'Defender projection marker set changed'}
 $method=[regex]::Replace($method,'(?ms)^[\t ]*// AICF_QUOTA_BLOCK_BEGIN ([A-Z_]+)[^\r\n]*\r?\n.*?^[\t ]*// AICF_QUOTA_BLOCK_END \1[^\r\n]*(?:\r?\n|$)',{
  param($m)
  switch($m.Groups[1].Value){
   'FREE_CREDIT'{return "SpawnUnit(m_aRefillQueue[0], false);`nm_iDespawnedGroupMembers--;`n"}
   'PAID_DISPATCH'{return "SpawnUnit(m_aRefillQueue[0]);`n"}
   default{return ''}
  }
 })
 Tokens $method
}
foreach($pair in @(@('SpawnUnit','AICF_QuotaSpawnDefenderUnit'),@('HandleGroup','AICF_QuotaHandleDefenderGroup'))){if((Projection $adapter $pair[1])-cne(Tokens (Body $pinned $pair[0]))){throw "Pinned defender body drift: $($pair[0])"}}
$sourceCases=2
foreach($mutation in @(
 @('price division','entityData.GetSupplyCost() / m_AIgroup.m_aUnitPrefabSlots.Count()','entityData.GetSupplyCost()'),
 @('native activation','control.ActivateAI();','control.ActivateAI(false);'),
 @('native supply mutation','AddSupplies(-spawnCost);','AddSupplies(-1);'),
 @('formation','0.5 * formationDefiniton.GetOffsetPosition','1.5 * formationDefiniton.GetOffsetPosition'),
 @('native join callback','Insert(OnMoveFinished)','Insert(OnMovingCharacterDeath)'),
 @('native queue order','m_aRefillQueue.RemoveItem(unitResource);','m_aRefillQueue.Clear();'),
 @('original despawn','m_AIgroup.DespawnMembers();','Print("skipped");'),
 @('original schedule','replicationTime.PlusSeconds(m_fRespawnDelay)','replicationTime.PlusSeconds(0)')
)){
 if(!$adapter.Contains($mutation[1])){throw "Mutation missing: $($mutation[0])"}
 $changed=$adapter.Replace($mutation[1],$mutation[2]);$rejected=$false
 foreach($pair in @(@('SpawnUnit','AICF_QuotaSpawnDefenderUnit'),@('HandleGroup','AICF_QuotaHandleDefenderGroup'))){try{if((Projection $changed $pair[1])-cne(Tokens (Body $pinned $pair[0]))){$rejected=$true}}catch{$rejected=$true}}
 if(!$rejected){throw "Pinned mutation escaped: $($mutation[0])"};$sourceCases++
}
function Rules([string]$text,[string]$value){
 $spawn=Body $text 'AICF_QuotaSpawnDefenderUnit';$handle=Body $text 'AICF_QuotaHandleDefenderGroup'
 foreach($field in @('basket.EnsureRemaining()','AICF_AIQuotaDefenderAttempt.Capture','quota.BeginExternalCreation','if (!attempt.IsCurrent() || !quota.IsConfigured())','CancelExternalCreationBeforeFactory','ObserveExternalFactory(creation, ai)','quota.MarkExternalNativeCallStarted(creation)')){if(!$spawn.Contains($field)){'PRE_FACTORY_GUARD';break}}
 if($spawn.IndexOf('quota.BeginExternalCreation')-gt$spawn.IndexOf('IEntity ai = GetGame().SpawnEntityPrefab')){'ADMISSION_AFTER_NATIVE'}
 if($spawn.IndexOf('quota.MarkExternalNativeCallStarted')-gt$spawn.IndexOf('IEntity ai = GetGame().SpawnEntityPrefab')){'NATIVE_STARTED_AFTER_FACTORY'}
 if($spawn.IndexOf('ObserveExternalFactory(creation, ai)')-gt$spawn.IndexOf('AddSupplies(-spawnCost)')-or$spawn.IndexOf('ObserveExternalFactory(creation, ai)')-gt$spawn.IndexOf('m_aRefillQueue.RemoveItem')){'BOUND_AFTER_SIDE_EFFECT'}
 if(!$handle.Contains('if (attempt && attempt.CanConsumeFreeCredit()) m_iDespawnedGroupMembers--;')){'FREE_CREDIT_LOST_ON_VETO'}
 foreach($method in @('ReinforceGroup','OnFactionChanged','HandleGroup')){if(!(Body $text $method).Contains('AICF_CancelDefenderBasket()')){'AVAILABLE_LIFECYCLE_CANCEL';break}}
 if(!$text.Contains('void ~SCR_DefenderSpawnerComponent()')-or!$text.Contains('basket.Cancel();')){'DESTRUCTOR_CANCEL'}
 $ensure=Body $value 'EnsureRemaining'
 $identity=Body $value 'IsCurrent'
 if(!$identity.Contains('m_Service.IsCurrentWorld()')-or$identity.Contains('m_Service.IsConfigured()')-or!$ensure.Contains('m_Service.IsConfigured()')){'IDENTITY_NOT_QUOTA_AVAILABILITY'}
 if(!$ensure.Contains('m_Queue.Count() - available')-or!$ensure.Contains('AICF_EAIQuotaBucket.STOCK')-or!$ensure.Contains('CancelReservation(extra, m_iSerial)')){'FULL_REMAINING_NO_PARTIAL'}
 $attempt=Body $value 'CanConsumeFreeCredit'
 foreach($fence in @('!m_bBound','!OwnerCurrent()','m_aBeforeQueue.Count() - 1','!= m_iFreeCredit','m_aBeforeQueue[index + 1]')){if(!$attempt.Contains($fence)){'EXACT_SUCCESS_CREDIT_RECEIPT';break}}
}
if(@(Rules $adapter $basket).Count){throw "Defender guard failed: $(Rules $adapter $basket)"}
foreach($mutation in @(
 @('credit receipt','attempt.CanConsumeFreeCredit()','true'),
 @('pre-factory state','if (!attempt.IsCurrent() || !quota.IsConfigured())','if (false)'),
 @('bind exact body','ObserveExternalFactory(creation, ai)','ObserveExternalFactory(creation, null)'),
 @('remaining basket','basket.EnsureRemaining()','true'),
 @('reinforce cancellation','AICF_CancelDefenderBasket();'+"`n`t`tsuper.ReinforceGroup();",'super.ReinforceGroup();')
)){
 if(!$adapter.Contains($mutation[1])){throw "Missing guard mutant $($mutation[0])"}
 if(!@(Rules ($adapter.Replace($mutation[1],$mutation[2])) $basket).Count){throw "Guard mutant escaped: $($mutation[0])"};$sourceCases++
}
$attemptCode=$basket.Substring($basket.IndexOf('class AICF_AIQuotaDefenderAttempt'))
$attemptCode=$attemptCode.Replace('class AICF_AIQuotaDefenderAttempt','public class AICF_AIQuotaDefenderAttempt').Replace('ref array<ResourceName> m_aBeforeQueue = {};','NativeArray<string> m_aBeforeQueue = new NativeArray<string>();').Replace('ref AICF_AIQuotaCatalogIdentity','AICF_AIQuotaCatalogIdentity').Replace('ResourceName','string')
$attemptCode=[regex]::Replace($attemptCode,'\bFactionKey\b','string')
$attemptCode=[regex]::Replace($attemptCode,'(?m)^\t((?:AICF_\w+|SCR_\w+|string|float|int|bool|NativeArray<string>) \w+(?: = [^;]+)?;)',' public $1')
$attemptCode=$attemptCode.Replace('static AICF_AIQuotaDefenderAttempt Capture','public static AICF_AIQuotaDefenderAttempt Capture').Replace('override string GetFactionKey','public override string GetFactionKey').Replace('bool OwnerCurrent()','public bool OwnerCurrent()').Replace('override bool IsCurrent()','public override bool IsCurrent()').Replace('bool CanConsumeFreeCredit()','public bool CanConsumeFreeCredit()')
$available=(Body $basket 'Available').Replace('foreach (AICF_AIQuotaReservation reservation : m_aReservations)','foreach (AICF_AIQuotaReservation reservation in m_aReservations)').Replace('AICF_AIQuotaMath.MAX_TOTAL_UNITS, sum)','AICF_AIQuotaMath.MAX_TOTAL_UNITS, out sum)')
$ensure=Body $basket 'EnsureRemaining'
$next=(Body $basket 'NextReservation').Replace('foreach (AICF_AIQuotaReservation reservation : m_aReservations)','foreach (AICF_AIQuotaReservation reservation in m_aReservations)')
$cancel=(Body $basket 'Cancel').Replace('foreach (AICF_AIQuotaReservation reservation : m_aReservations)','foreach (AICF_AIQuotaReservation reservation in m_aReservations)')
$identity=(Body $basket 'IsCurrent').Replace('m_Entry.GetEntityDataOfType(SCR_EntityCatalogSpawnerData)','m_Entry.GetEntityDataOfType(typeof(SCR_EntityCatalogSpawnerData))')
$identity=$identity.Replace('{',"{`nif (!Current) return false;")
$actual=@'
using System;
using System.Collections.Generic;
public class N {public static implicit operator bool(N value){return !Object.ReferenceEquals(value,null);}}
public static class Text {public static bool IsEmpty(this string value){return String.IsNullOrEmpty(value);}}
public class NativeArray<T>:List<T>{public static implicit operator bool(NativeArray<T> value){return value!=null;}public new int Count(){return base.Count;}public bool IsEmpty(){return base.Count==0;}public void Copy(IEnumerable<T> values){Clear();AddRange(values);}public void Insert(T value){Add(value);}public void Remove(int index){RemoveAt(index);}}
public class Entity:N{}
public class Rpl:N {public bool Master=true;public bool IsMaster(){return Master;}}
public class AICF_AIQuotaCatalogIdentity:N {public Entity m_Entity;public Rpl m_Rpl;public bool Current=true;public bool IsCurrent(){return Current;}public static AICF_AIQuotaCatalogIdentity Capture(Entity entity,bool requireMaster){return entity==null?null:new AICF_AIQuotaCatalogIdentity{m_Entity=entity};}}
public class Faction:N {public string GetFactionKey(){return "US";}}
public class SCR_EntitySpawnerSlotComponent:N {public Entity Owner=new Entity();public Entity GetOwner(){return Owner;}}
public class Spawner:N {public Entity Owner=new Entity();public Faction Faction=new Faction();public float Supplies=100;public int Credit=2;public bool HasSlot=true,Owns=true;public float GetSpawnerSupplies(){return Supplies;}public int AICF_QuotaDefenderFreeCredit(){return Credit;}public bool AICF_QuotaDefenderHasSlot(SCR_EntitySpawnerSlotComponent slot){return HasSlot;}public Entity GetOwner(){return Owner;}public Faction GetFaction(){return Faction;}public bool AICF_QuotaDefenderOwns(AICF_AIQuotaDefenderBasket basket){return Owns;}}
public class Group:N {public Faction Faction;public NativeArray<string> m_aUnitPrefabSlots=new NativeArray<string>{"one","two","three"};public Faction GetFaction(){return Faction;}}
public class SCR_EntityCatalogSpawnerData:N {public int Cost=100;public int GetSupplyCost(){return Cost;}}
public class Entry:N {public SCR_EntityCatalogSpawnerData Data;public SCR_EntityCatalogSpawnerData GetEntityDataOfType(Type type){return Data;}}
public enum AICF_EAIQuotaBucket {STOCK}
public static class AICF_AIQuotaMath {public const int MAX_TOTAL_UNITS=4096;public static bool AddBounded(int a,int b,int bound,out int sum){sum=0;if(a<0||b<0||a>bound||b>bound-a)return false;sum=a+b;return true;}}
public class AICF_AIQuotaReservation:N {public int m_iAvailable;public bool m_bCancelled;}
public class AICF_AIWorldQuotaService:N {public bool Configured=true,WorldCurrent=true;public int Epoch=1;public bool IsConfigured(){return Configured;}public bool IsCurrentWorld(){return WorldCurrent;}public int GetEpoch(){return Epoch;}public int Remaining=10,ReserveCalls,CancelCalls;public Action OnReserve;public AICF_AIQuotaReservation ReserveUnits(AICF_EAIQuotaBucket b,int units,int serial){ReserveCalls++;if(units<=0||units>Remaining)return null;Remaining-=units;var r=new AICF_AIQuotaReservation{m_iAvailable=units};if(OnReserve!=null)OnReserve();return r;}public bool CancelReservation(AICF_AIQuotaReservation r,int serial){if(r==null||r.m_bCancelled)return false;CancelCalls++;Remaining+=r.m_iAvailable;r.m_iAvailable=0;r.m_bCancelled=true;return true;}}
public class AICF_AIQuotaCreationGuard:N {public virtual bool IsCurrent(){return false;}public virtual string GetFactionKey(){return "";}}
public class AICF_AIQuotaDefenderBasket:N {public AICF_AIWorldQuotaService m_Service=new AICF_AIWorldQuotaService();public Spawner m_Spawner;public Faction m_Faction;public Group m_Group=new Group();public AICF_AIQuotaCatalogIdentity m_Producer,m_GroupIdentity=new AICF_AIQuotaCatalogIdentity();public SCR_EntityCatalogSpawnerData m_Data=new SCR_EntityCatalogSpawnerData();public Entry m_Entry;public int m_iEpoch=1,m_iSupplyCost=100;public NativeArray<string> m_aAuthored=new NativeArray<string>{"one","two","three"};public NativeArray<string> m_Queue=new NativeArray<string>();public int m_iSerial=1;public bool Current=true,m_bCancelled;public NativeArray<AICF_AIQuotaReservation> m_aReservations=new NativeArray<AICF_AIQuotaReservation>();public AICF_AIQuotaDefenderBasket(){m_Entry=new Entry{Data=m_Data};AttachSpawner(new Spawner());}public void AttachSpawner(Spawner spawner){m_Spawner=spawner;m_Producer=new AICF_AIQuotaCatalogIdentity{m_Entity=spawner.Owner};m_Faction=spawner.Faction;m_Group.Faction=m_Faction;}public bool IsCurrent() __IDENTITY__ public int Available() __AVAILABLE__ public bool EnsureRemaining() __ENSURE__ public AICF_AIQuotaReservation NextReservation() __NEXT__ public void Cancel() __CANCEL__}
__ATTEMPT__
public static class Run {public static AICF_AIQuotaDefenderAttempt Fresh(){var b=new AICF_AIQuotaDefenderBasket();b.m_Queue.AddRange(new[]{"one","two","three"});return AICF_AIQuotaDefenderAttempt.Capture(b,new SCR_EntitySpawnerSlotComponent(),"one",100,2);}
public static int Execute(){int count=0;Action<bool,string> check=(ok,label)=>{if(!ok)throw new Exception(label);count++;};var a=Fresh();check(a!=null&&a.IsCurrent(),"exact native attempt captured");check(!a.CanConsumeFreeCredit(),"no credit before body");a.m_Basket.m_Queue.Remove(0);check(!a.IsCurrent(),"pre-factory context consumed");check(!a.CanConsumeFreeCredit(),"queue change alone not birth");a.m_bBound=true;check(a.CanConsumeFreeCredit(),"exact born body and consumed first queue unit");
Action<Action<AICF_AIQuotaDefenderAttempt>,string> reject=(change,label)=>{var x=Fresh();change(x);check(!x.IsCurrent(),label);};
reject(x=>x.m_Basket.Current=false,"basket retired");reject(x=>x.m_SlotOwner.Current=false,"slot identity stale");reject(x=>x.m_Slot.Owner=new Entity(),"slot owner replaced");reject(x=>x.m_SlotOwner.m_Rpl=new Rpl{Master=false},"proxy slot");reject(x=>x.m_Basket.m_Spawner.HasSlot=false,"foreign slot");reject(x=>x.m_Basket.m_Queue[0]="other","head changed");reject(x=>x.m_Basket.m_Queue[1]="other","same-size tail changed");reject(x=>x.m_Basket.m_Queue.Remove(2),"partial queue changed");reject(x=>x.m_Basket.m_Spawner.Supplies=101,"quote changed");reject(x=>x.m_Basket.m_Spawner.Credit=3,"free-credit context changed");
Action<Action<AICF_AIQuotaDefenderAttempt>,string> creditReject=(change,label)=>{var x=Fresh();x.m_bBound=true;x.m_Basket.m_Queue.Remove(0);change(x);check(!x.CanConsumeFreeCredit(),label);};
creditReject(x=>x.m_Basket.m_Queue[0]="other","replacement tail receipt");creditReject(x=>x.m_Basket.m_Spawner.Credit=3,"successor free-credit");creditReject(x=>x.m_Basket.Current=false,"successor owner");creditReject(x=>x.m_iFreeCredit=0,"paid birth has no free credit");creditReject(x=>x.m_Basket.m_Queue.Remove(0),"two removals not one receipt");
var b=Fresh().m_Basket;b.m_Service.Remaining=2;check(!b.EnsureRemaining()&&b.m_Service.Remaining==2&&b.Available()==0,"full remaining3 refused atomically with2free");b.m_Service.Remaining=3;check(b.EnsureRemaining()&&b.Available()==3&&b.m_Service.Remaining==0,"reserve full remaining3");int calls=b.m_Service.ReserveCalls;check(b.EnsureRemaining()&&b.m_Service.ReserveCalls==calls,"same basket no duplicate reservation");var r=b.NextReservation();r.m_iAvailable--;b.m_Queue.Remove(0);check(b.EnsureRemaining()&&b.Available()==2&&b.m_Service.ReserveCalls==calls,"bound unit replaces pending");b.Cancel();check(b.Available()==0&&b.m_Service.Remaining==2&&b.m_Service.CancelCalls==1,"cancel available only");b.Cancel();check(b.m_Service.CancelCalls==1,"cancel idempotent");
var c=Fresh().m_Basket;c.m_Service.Remaining=3;c.m_Service.OnReserve=()=>c.Current=false;check(!c.EnsureRemaining()&&c.m_Service.Remaining==3&&c.m_Service.CancelCalls==1,"reentry invalidates and refunds own full pending");
var q=Fresh();q.m_Basket.m_Service.Configured=false;check(q.IsCurrent(),"unrelated quota quarantine is not owner loss");check(!q.m_Basket.EnsureRemaining(),"unconfigured quota still vetoes new admission");q.m_Basket.m_Service.WorldCurrent=false;check(!q.IsCurrent(),"actual world loss invalidates immutable identity");q=Fresh();q.m_Basket.m_Service.Epoch++;check(!q.IsCurrent(),"actual epoch change invalidates immutable identity");q=Fresh();q.m_Basket.m_Group.m_aUnitPrefabSlots[1]="foreign";check(!q.IsCurrent(),"authored prefab change invalidates identity");q=Fresh();q.m_Basket.m_Spawner.Owns=false;check(!q.IsCurrent(),"actual producer ownership lost");return count;}}
'@
$actual=$actual.Replace('__IDENTITY__',$identity).Replace('__AVAILABLE__',$available).Replace('__ENSURE__',$ensure).Replace('__NEXT__',$next).Replace('__CANCEL__',$cancel).Replace('__ATTEMPT__',$attemptCode)
$actual=$actual.Replace('int result;','int result = 0;').Replace('for (int index;','for (int index = 0;')
$actualNamespace='AICFDefenderActual'+[guid]::NewGuid().ToString('N')
Add-Type ($actual.Replace('public class N {',"namespace $actualNamespace {public class N {")+'}')
$actualType=[type]("$actualNamespace.Run")
$actualCases=$actualType.GetMethod('Execute').Invoke($null,@())
"PASS quota defender source_cases=$sourceCases actual_attempt_and_remaining_basket=$actualCases inverse_token_equal=1 pinned_version=1.8.0.13 runtime=NOT_RUN compile_gate=SEPARATE"
