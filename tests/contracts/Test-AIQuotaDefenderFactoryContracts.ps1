param([string]$RepositoryRoot=(Split-Path -Parent (Split-Path -Parent $PSScriptRoot)))
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'Test-AIQuotaDefenderContracts.ps1') -RepositoryRoot $RepositoryRoot
# Reuse the exact extracted value-object methods, with native factory adapters.
# This executes the actual factory boundary blocks; native engine is a separate gate.
function Block([string]$text,[string]$name){
 $match=[regex]::Match($text,('(?ms)^\s*// AICF_QUOTA_BLOCK_BEGIN '+$name+'\s*\r?\n(.*?)^\s*// AICF_QUOTA_BLOCK_END '+$name+'\s*$'))
 if(!$match.Success){throw "Missing actual boundary $name"};$match.Groups[1].Value
}
$before=Block $adapter 'FACTORY_BEGIN';$after=Block $adapter 'FACTORY_OBSERVE'
$free=(Block $adapter 'FREE_CREDIT').Replace('quota, attempt)','quota, out attempt)')
$native=Body $adapter 'AICF_QuotaSpawnDefenderUnit'
$sideStart=$native.IndexOf('if (m_aRefillQueue)');$sideEnd=$native.IndexOf('AIAgent agent =', $sideStart)
if($sideStart-lt0-or$sideEnd-lt$sideStart){throw 'Pinned post-bind stock side effects missing'}
$side=$native.Substring($sideStart,$sideEnd-$sideStart).Replace('.FindComponent(AIControlComponent)','.FindComponent(typeof(AIControlComponent))')
$baseActual=$actual.Replace('public class Entity:N{}','public class Entity:N{public AIControlComponent Control=new AIControlComponent();public object FindComponent(Type type){return Control;}}')
$baseActual=$baseActual.Replace('public class NativeArray<T>:List<T>{','public class NativeArray<T>:List<T>{public void RemoveItem(T value){base.Remove(value);}')
$baseActual=$baseActual.Replace('public int Credit=2;','private int credit=2;public virtual int Credit{get{return credit;}set{credit=value;}}')
$baseActual=$baseActual.Replace('return Configured;','return Configured&&!Quarantined;')
$serviceExtra=@'
public bool Busy,Quarantined,FailEnd,FailMark;public int Begins,Ends,Binds,Rejects,PrecallCancels,Marks;public AICF_AIQuotaCreation Current;public Action OnBegin;
public AICF_AIQuotaCreation BeginExternalCreation(AICF_AIQuotaReservation reservation,int serial,string prefab,Entity producer,bool headerOnly,AICF_AIQuotaCreationGuard guard){Begins++;if(Busy||Quarantined||reservation==null||reservation.m_iAvailable<=0||!guard.IsCurrent())return null;reservation.m_iAvailable--;Busy=true;Current=new AICF_AIQuotaCreation{Guard=guard,Reservation=reservation,m_Status=AICF_EAIQuotaBirthStatus.IN_NATIVE_FACTORY};if(OnBegin!=null)OnBegin();return Current;}
public bool MarkExternalNativeCallStarted(AICF_AIQuotaCreation creation){Marks++;if(FailMark||creation!=Current||!Busy||creation.Started||!creation.Guard.IsCurrent()||!IsConfigured())return false;creation.Started=true;return true;}public bool SkipExternalNativeCallStarted(AICF_AIQuotaCreation creation){return true;}
public void ObserveExternalFactory(AICF_AIQuotaCreation creation,Entity entity){if(creation!=Current||!Busy||!creation.Started){Quarantined=true;return;}if(entity==null){creation.m_Status=AICF_EAIQuotaBirthStatus.NATIVE_NULL;Remaining++;return;}if(!creation.Guard.IsCurrent()){creation.m_Status=AICF_EAIQuotaBirthStatus.QUARANTINED;return;}Binds++;creation.m_Status=AICF_EAIQuotaBirthStatus.BOUND;}
public bool EndExternalCreation(AICF_AIQuotaCreation creation){if(FailEnd||creation!=Current||!Busy)return false;Ends++;Busy=false;Current=null;return true;}
public bool CancelExternalCreationBeforeFactory(AICF_AIQuotaCreation creation){if(creation!=Current||creation.Started)return false;PrecallCancels++;Remaining++;return EndExternalCreation(creation);}
public void RejectExternalCreation(AICF_AIQuotaCreation creation,string reason){Rejects++;Quarantined=true;Busy=false;Current=null;}
'@
$baseActual=$baseActual.Replace('public class AICF_AIWorldQuotaService:N {','public class AICF_AIWorldQuotaService:N {'+$serviceExtra).Replace('ReserveCalls++;if(units<=0','ReserveCalls++;if(Busy||Quarantined||units<=0')
$harness=@'
public enum AICF_EAIQuotaBirthStatus {IN_NATIVE_FACTORY,NATIVE_NULL,BOUND,QUARANTINED}
public class AICF_AIQuotaCreation:N {public bool Started;public AICF_EAIQuotaBirthStatus m_Status;public AICF_AIQuotaCreationGuard Guard;public AICF_AIQuotaReservation Reservation;}
public class AIControlComponent:N {public int Activations;public static AIControlComponent Cast(object value){return value as AIControlComponent;}public void ActivateAI(){Activations++;}}
public class NativeGame:N {public int Factories;public bool ReturnNull;public Action OnFactory;public Entity Last;public Entity SpawnEntityPrefab(string resource,object world,object parameters){Factories++;if(OnFactory!=null)OnFactory();if(ReturnNull)return null;Last=new Entity();return Last;}}
public class FactoryHarness:Spawner {public NativeGame Game=new NativeGame();public AICF_AIQuotaDefenderBasket Basket;public NativeArray<string> m_aRefillQueue;public int m_iDespawnedGroupMembers=2,Debits;public bool m_bSuppliesConsumptionEnabled=true;public Entity Owner=new Entity();public NativeGame GetGame(){return Game;}public Entity GetOwner(){return Owner;}public void AddSupplies(int amount){Supplies+=amount;Debits++;}public AICF_AIQuotaDefenderBasket AICF_DefenderBasket(AICF_AIWorldQuotaService quota){return Basket;}public void AICF_RejectDefenderBirth(AICF_AIWorldQuotaService quota,AICF_AIQuotaCreation creation){quota.RejectExternalCreation(creation,"native context mismatch");}
public void Spawn(bool consumeSupplies,out AICF_AIQuotaDefenderAttempt attempt){string unitResource="one",res="one";var slot=new SCR_EntitySpawnerSlotComponent();object spawnParams=null;int spawnCost=50;var quota=Basket.m_Service;
__BEFORE__
Entity ai = GetGame().SpawnEntityPrefab(res, null, spawnParams);
__AFTER__
__SIDE__
}
public override int Credit{get{return m_iDespawnedGroupMembers;}set{m_iDespawnedGroupMembers=value;}}
public void AICF_QuotaSpawnDefenderUnit(string prefab,bool consume,AICF_AIWorldQuotaService quota,out AICF_AIQuotaDefenderAttempt attempt){Spawn(consume,out attempt);}
public void FreeRespawn(){var quota=Basket.m_Service;__FREE__}
}
public static class FactoryRun {public static FactoryHarness Fresh(){var h=new FactoryHarness();var b=new AICF_AIQuotaDefenderBasket();b.AttachSpawner(h);b.m_Queue.AddRange(new[]{"one","two","three"});h.Basket=b;h.m_aRefillQueue=b.m_Queue;return h;}
public static void CheckUnrelatedQuotaAfterBirth(){var h=Fresh();h.Game.OnFactory=()=>h.Basket.m_Service.Quarantined=true;AICF_AIQuotaDefenderAttempt attempt;h.Spawn(true,out attempt);if(h.Game.Factories!=1||h.Basket.m_Service.Binds!=1||h.Basket.m_Service.Rejects!=0||!attempt.m_bBound)throw new Exception("unrelated quota quarantine must not turn valid newborn into owner-loss rollback");}
public static int Execute(){int count=0;Action<bool,string> check=(ok,label)=>{if(!ok)throw new Exception(label);count++;};AICF_AIQuotaDefenderAttempt a;
var h=Fresh();h.Basket.m_Service.Remaining=2;h.Spawn(true,out a);check(h.Game.Factories==0&&h.Debits==0&&h.Supplies==100&&h.m_aRefillQueue.Count()==3,"quota-full0body0debit0queuechange");
h=Fresh();h.Game.ReturnNull=true;h.FreeRespawn();check(h.Game.Factories==1&&h.Basket.m_Service.Ends==1&&!h.Basket.m_Service.Busy,"actual native null closes context");check(h.Debits==0&&h.m_aRefillQueue.Count()==3&&h.Credit==2,"native null retains free credit and queue");
h=Fresh();h.Spawn(true,out a);check(h.Game.Factories==1&&h.Basket.m_Service.Binds==1&&h.Basket.m_Service.Ends==1,"one exact body bound and context closed");check(h.Debits==1&&h.Supplies==50&&h.Game.Last.Control.Activations==1&&h.m_aRefillQueue.Count()==2,"original paid stock side effects once");check(a.m_bBound&&a.CanConsumeFreeCredit(),"exact birth receipt bound before stock callbacks");
h=Fresh();h.FreeRespawn();check(h.Game.Factories==1&&h.Debits==0&&h.m_iDespawnedGroupMembers==1&&h.Credit==1,"free respawn consumes exactly one credit after birth");
h=Fresh();h.Basket.m_Service.Remaining=0;h.FreeRespawn();check(h.Game.Factories==0&&h.m_iDespawnedGroupMembers==2&&h.Credit==2,"quota veto retains free respawn credit");
var changed=Fresh();changed.Game.OnFactory=()=>changed.Supplies=101;changed.Spawn(true,out a);check(changed.Game.Factories==1&&changed.Debits==0&&changed.m_aRefillQueue.Count()==3&&changed.Basket.m_Service.Quarantined,"native callback quote change quarantined before debit");
var precall=Fresh();precall.Basket.m_Service.OnBegin=()=>precall.Supplies=101;precall.Spawn(true,out a);check(precall.Game.Factories==0&&precall.Debits==0&&precall.Basket.m_Service.PrecallCancels==1&&!precall.Basket.m_Service.Busy,"precall callback stale cancels before physical factory");
var unavailable=Fresh();unavailable.Basket.m_Service.OnBegin=()=>unavailable.Basket.m_Service.Quarantined=true;unavailable.Spawn(true,out a);check(unavailable.Game.Factories==0&&unavailable.Basket.m_Service.PrecallCancels==1&&unavailable.Basket.IsCurrent(),"precall unrelated quota veto cancels permission without owner loss");
var afterQuota=Fresh();afterQuota.Game.OnFactory=()=>afterQuota.Basket.m_Service.Quarantined=true;afterQuota.Spawn(true,out a);check(afterQuota.Game.Factories==1&&afterQuota.Basket.m_Service.Binds==1&&afterQuota.Basket.m_Service.Rejects==0&&afterQuota.Debits==1&&a.m_bBound,"postfactory unrelated quota quarantine keeps valid newborn receipt");
var ctor=Fresh();bool cancelled=false;ctor.Game.OnFactory=()=>cancelled=ctor.Basket.m_Service.CancelExternalCreationBeforeFactory(ctor.Basket.m_Service.Current);ctor.Spawn(true,out a);check(!cancelled&&ctor.Game.Factories==1&&ctor.Basket.m_Service.Binds==1&&ctor.Debits==1&&ctor.Basket.m_Service.Marks==1,"constructor callback cannot refund already-started factory");
h=Fresh();h.Basket.m_Service.FailMark=true;h.Spawn(true,out a);check(h.Game.Factories==0&&h.Basket.m_Service.PrecallCancels==1&&h.Debits==0,"failed native-start fence cancels before factory");
var nested=Fresh();nested.Game.OnFactory=()=>{AICF_AIQuotaDefenderAttempt inner;nested.Spawn(true,out inner);};nested.Spawn(true,out a);check(nested.Game.Factories==1&&nested.Debits==1&&nested.Basket.m_Service.Binds==1,"nested same-prefab cannot borrow factory permit");
h=Fresh();h.Basket.m_Service.FailEnd=true;h.Spawn(true,out a);check(h.Game.Factories==1&&h.Debits==0&&h.m_aRefillQueue.Count()==3&&h.Basket.m_Service.Quarantined&&!a.m_bBound,"changed finish context quarantines no debit/credit receipt");return count;}}
'@
$harness=$harness.Replace('__BEFORE__',$before).Replace('__AFTER__',$after).Replace('__SIDE__',$side).Replace('__FREE__',$free)
$harness=$harness.Replace('public Entity Owner=new Entity();','').Replace('public Entity GetOwner(){return Owner;}','')
$namespace='AICFDefenderFactory'+[guid]::NewGuid().ToString('N')
$combined=($baseActual+$harness).Replace('public class N {',"namespace $namespace {public class N {")+'}'
Add-Type $combined
$factoryType=[type]("$namespace.FactoryRun")
$factoryCases=$factoryType.GetMethod('Execute').Invoke($null,@())
$unsafeCases=0
foreach($mutation in @(
 @('free-credit receipt','attempt.CanConsumeFreeCredit()','true'),
 @('exact returned body','ObserveExternalFactory(creation, ai)','ObserveExternalFactory(creation, null)'),
 @('constructor started receipt','MarkExternalNativeCallStarted(creation)','SkipExternalNativeCallStarted(creation)')
)){
 if(!$harness.Contains($mutation[1])){throw "Unsafe source missing $($mutation[0])"}
 $unsafeNamespace='AICFDefenderUnsafe'+[guid]::NewGuid().ToString('N')
 $unsafeCode=($baseActual+$harness.Replace($mutation[1],$mutation[2])).Replace('public class N {',"namespace $unsafeNamespace {public class N {")+'}'
 Add-Type $unsafeCode
 $unsafeType=[type]("$unsafeNamespace.FactoryRun")
 $rejected=$false;try{$null=$unsafeType.GetMethod('Execute').Invoke($null,@())}catch{$rejected=$true}
 if(!$rejected){throw "Unsafe actual factory boundary survived: $($mutation[0])"};$unsafeCases++
}
$mixedNamespace='AICFDefenderMixedQuota'+[guid]::NewGuid().ToString('N')
$mixedCode=($baseActual.Replace('m_Service.IsCurrentWorld()','m_Service.IsConfigured()')+$harness).Replace('public class N {',"namespace $mixedNamespace {public class N {")+'}'
Add-Type $mixedCode
$mixedType=[type]("$mixedNamespace.FactoryRun")
$mixedRejected=$false;try{$null=$mixedType.GetMethod('CheckUnrelatedQuotaAfterBirth').Invoke($null,@())}catch{
 if($_.Exception.InnerException.Message -ne 'unrelated quota quarantine must not turn valid newborn into owner-loss rollback'){throw};$mixedRejected=$true
}
if(!$mixedRejected){throw 'Identity/budget confusion regression escaped actual post-factory quarantine case'}
$unsafeCases++
# New atomic Mark repeats the context/config guard. Removing only the earlier
# check is therefore a defended control, no longer an unsafe executed mutant.
$defendedNamespace='AICFDefenderMarkDefended'+[guid]::NewGuid().ToString('N')
$defendedCode=($baseActual+$harness.Replace('if (!attempt.IsCurrent() || !quota.IsConfigured())','if (quota == null)')).Replace('public class N {',"namespace $defendedNamespace {public class N {")+'}'
Add-Type $defendedCode
$defendedType=[type]("$defendedNamespace.FactoryRun")
if($defendedType.GetMethod('Execute').Invoke($null,@()) -ne $factoryCases){throw 'Atomic Mark did not independently defend the current context/config'}
"PASS quota defender actual factory boundaries=$factoryCases unsafe_executed=$unsafeCases redundant_precheck_defended_by_mark=1 native_adapters=only separate_native_runtime=NOT_RUN"
