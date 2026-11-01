param([string]$RepositoryRoot=(Split-Path -Parent (Split-Path -Parent $PSScriptRoot)))
$ErrorActionPreference='Stop'
$root=Join-Path $RepositoryRoot 'AIConflictCore/Scripts/Game/AIConflict/Forces'
$math=[IO.File]::ReadAllText((Join-Path $root 'AICF_AIQuotaMath.c'))
$limits=[IO.File]::ReadAllText((Join-Path $root 'AICF_AIQuotaLimits.c'))
$ledger=[IO.File]::ReadAllText((Join-Path $root 'AICF_AIQuotaLedger.c'))
if($ledger-match'\b(SpawnEntity|RequestSpawn|SetLimitOfActiveAIs|DeleteRplEntity|SetLOD|ActivateAI|CallLater)\s*\('){throw 'Unit ledger changes native gameplay'}
function Adapt([string]$source){
	$source=[regex]::Replace($source,'(?m)^(\s*)ref (AICF_AIQuota\w+ m_\w+);','$1$2;')
 $source=[regex]::Replace($source,'(?s)enum AICF_EAIQuotaBucket\s*\{.*?\}','')
 $source=$source.Replace('AICF_EAIQuotaBucket bucket','int bucket').Replace('AICF_EAIQuotaBucket m_Bucket','int m_Bucket')
 $source=$source.Replace('AICF_EAIQuotaBucket fromBucket','int fromBucket').Replace('AICF_EAIQuotaBucket toBucket','int toBucket').Replace('AICF_EAIQuotaBucket target','int target')
 $source=$source.Replace('array<int>','AICF_QuotaInts').Replace('array<string>','AICF_QuotaStrings').Replace('protected ref ','protected ')
 $source=$source.Replace('static const int','public const int').Replace('static bool','public static bool')
 $source=[regex]::Replace($source,'(?m)^(\s*)(bool|int|void|AICF_AIQuotaReservation\w*|AICF_AIQuotaDispatch\w*) (\w+)\(','$1public $2 $3(')
 $source=[regex]::Replace($source,'public void (AICF_AIQuotaLedger\w*)\(','public $1(')
 $source=[regex]::Replace($source,'(?m)^(\s*)(AICF_AIQuota\w*|AICF_EAIQuotaBucket|int|bool) (m_\w+);','$1public $2 $3;')
 $source=[regex]::Replace($source,'(?m)^(class AICF_AIQuota\w*)\s*$','public $1 : AICF_QuotaNative')
 $source=$source.Replace('= {};','= new AICF_QuotaInts();')
 $source=$source.Replace('m_aBoundKeys = new AICF_QuotaInts();','m_aBoundKeys = new AICF_QuotaStrings();')
 $source=$source.Replace('int sum;','int sum = 0;').Replace('int total;','int total = 0;')
 $source=$source.Replace('for (int index;','for (int index = 0;').Replace('for (int copyIndex;','for (int copyIndex = 0;').Replace('foreach (int value : limits)','foreach (int value in limits)')
 $source=$source.Replace('AddBounded(sum, value, MAX_TOTAL_UNITS, next)','AddBounded(sum, value, MAX_TOTAL_UNITS, out next)').Replace('TotalLimits(limits, total)','TotalLimits(limits, out total)').Replace('TotalLimits(candidate, total)','TotalLimits(candidate, out total)').Replace('AddBounded(physical[index], reserved[index], limits[index], occupied)','AddBounded(physical[index], reserved[index], limits[index], out occupied)').Replace('AddBounded(counts[index], m_aReserved[index], m_Limits.GetLimit(index), occupied)','AddBounded(counts[index], m_aReserved[index], m_Limits.GetLimit(index), out occupied)')
 $source=$source.Replace('entityKey.IsEmpty()','string.IsNullOrEmpty(entityKey)').Replace('m_aBoundKeys.Remove(index)','m_aBoundKeys.RemoveAt(index)').Replace('m_aBoundBuckets.Remove(index)','m_aBoundBuckets.RemoveAt(index)').Replace('foreach (int assignedBucket : m_aBoundBuckets)','foreach (int assignedBucket in m_aBoundBuckets)').Replace('int bound;','int bound = 0;')
 return $source
}
$variants=@(
 [pscustomobject]@{name='Actual';source=$ledger},
 [pscustomobject]@{name='IgnorePending';source=$ledger.Replace('m_aPhysical[bucket], m_aReserved[bucket], m_Limits.GetLimit(bucket), units','m_aPhysical[bucket], 0, m_Limits.GetLimit(bucket), units')},
 [pscustomobject]@{name='ReuseDispatch';source=$ledger.Replace('reservation.m_iAvailable--;','')},
 [pscustomobject]@{name='ReuseTicket';source=$ledger.Replace('&& !ticket.m_bClosed','')},
 [pscustomobject]@{name='DoubleCount';source=$ledger.Replace('m_aReserved[bucket] = m_aReserved[bucket] - 1;','')},
 [pscustomobject]@{name='ForeignOwner';source=$ledger.Replace('reservation.m_iOwnerSerial == ownerSerial','true')},
 [pscustomobject]@{name='UnknownCensus';source=$ledger.Replace('!complete','false')},
 [pscustomobject]@{name='DuplicateEntity';source=$ledger.Replace('m_aBoundKeys.Contains(entityKey)','false')},
 [pscustomobject]@{name='PrematureRelease';source=$ledger.Replace('m_aBoundKeys.Remove(index);','m_aPhysical[0]--; m_aBoundKeys.Remove(index);')}
)
$code=@'
#pragma warning disable 0162,0168
using System;
using System.Collections.Generic;
public class AICF_QuotaNative {public static implicit operator bool(AICF_QuotaNative value){return value!=null;}}
public class AICF_QuotaInts:List<int>
{
 public new int Count(){return base.Count;}
 public void Insert(int value){Add(value);}
 public void Resize(int count){while(base.Count<count)Add(0);if(base.Count>count)RemoveRange(count,base.Count-count);}
 public static bool operator !(AICF_QuotaInts value){return value==null;}
}
public class AICF_QuotaStrings:List<string>
{
 public new int Count(){return base.Count;}
 public int Find(string key){return IndexOf(key);}
 public void Insert(string key){Add(key);}
}
public static class AICF_EAIQuotaBucket {public const int STOCK=0,COMBAT=1,BUILDERS=2,FLEET=3,FIA_EXTRA=4,STOCK_VEHICLES=5,COUNT=6;}
'@
$code+=Adapt $math
$code+=Adapt $limits
foreach($variant in $variants){
 $body=$variant.source.Replace('AICF_AIQuotaLedger','AICF_AIQuotaLedger_'+$variant.name).Replace('AICF_AIQuotaReservation','AICF_AIQuotaReservation_'+$variant.name).Replace('AICF_AIQuotaDispatch','AICF_AIQuotaDispatch_'+$variant.name)
 $code+=Adapt $body
}
$code+=@'
public static class AICF_QuotaLedgerCases
{
 static int Cases;
 static void Need(bool valid,string message){Cases++;if(!valid)throw new Exception(message);}
 static object Call(object target,string name,params object[] args){return target.GetType().GetMethod(name).Invoke(target,args);}
 static int Count(object target,string name,int bucket){return (int)Call(target,name,bucket);}
 static bool Bool(object target,string name,params object[] args){return (bool)Call(target,name,args);}
 static object New(Type type){var l=new AICF_AIQuotaLimits();Need(l.CaptureStockSnapshot(128,0,0)&&l.Configure(220,8,20,0),"real six-bucket config");return Activator.CreateInstance(type,new object[]{l,7});}
 static AICF_QuotaInts Counts(int stock,int combat=0,int vehicles=0){return new AICF_QuotaInts{stock,combat,0,0,0,vehicles};}
 static void Census(object q,int stock=0,int combat=0,int vehicles=0){Need(Bool(q,"ApplyCensus",Counts(stock,combat,vehicles),true),"complete census admitted");}
 public static int Run(Type type)
 {
  Cases=0;var q=New(type);Need(!Bool(q,"CanReserve",0,1),"unknown census cannot admit");
  Need(!Bool(q,"ApplyCensus",Counts(0),false),"incomplete census cannot open gate");Census(q);
  var full=Call(q,"Reserve",0,128,11);Need(full!=null,"full stock queued basket reserved");
  Need(Count(q,"GetPhysical",0)==0&&Count(q,"GetReserved",0)==128,"queued not fabricated physical");
  Need(Call(q,"Reserve",0,1,12)==null,"stock queued basket cannot borrow managed reserve");
  var own=Call(q,"Reserve",1,200,15);Need(own!=null,"same-faction managed owner independent of stock full");
  var vehicles=Call(q,"Reserve",5,128,16);Need(vehicles!=null,"stock vehicle pool independent of stock character pool");
  Need(Call(q,"BeginDispatch",full,99)==null,"foreign callback cannot take basket unit");
  var first=Call(q,"BeginDispatch",full,11);Need(first!=null,"exact dispatch consumes one available unit");
  Need(Bool(q,"Bind",first,11,"entity1/component1/agent1"),"native identity bound once");
  Need(Count(q,"GetPhysical",0)==1&&Count(q,"GetReserved",0)==127,"pending to materialized has no double count");
  Need(!Bool(q,"Bind",first,11,"entity2/component2/agent2"),"closed ticket cannot spawn second actor");
  Census(q,1);Need(Count(q,"GetPhysical",0)==1&&Count(q,"GetReserved",0)==127,"fresh census already includes bound actor");
  var second=Call(q,"BeginDispatch",full,11);Need(!Bool(q,"Bind",second,11,"entity1/component1/agent1"),"duplicate actual identity rejected");
  Need(Bool(q,"FailDispatch",second,11),"failed unused dispatch releases only own unit");
  Need(!Bool(q,"FailDispatch",second,11),"duplicate completion cannot refund twice");
  Need(Count(q,"GetPhysical",0)==1&&Count(q,"GetReserved",0)==126,"failed dispatch refund conserves units");
  Need(Bool(q,"CancelAvailable",full,11),"cancel remaining queue basket");
  Need(Count(q,"GetReserved",0)==0,"only undelivered units cancelled");
  Need(Call(q,"BeginDispatch",full,11)==null,"cancelled basket cannot reopen");
  Need(Bool(q,"RetireBinding","entity1/component1/agent1"),"exact binding retired");
  Need(Count(q,"GetPhysical",0)==1,"retirement never fabricates new capacity before fresh census");Census(q);
  var single=Call(q,"Reserve",0,1,21);var t=Call(q,"BeginDispatch",single,21);Need(Call(q,"BeginDispatch",single,21)==null,"nested callback cannot inherit consumed unit");
  Need(Bool(q,"CancelAvailable",single,21),"cancel basket with inflight unit");
  Need(Count(q,"GetReserved",0)==1,"inflight survives cancellation");
  Need(Bool(q,"Bind",t,21,"exactInflight"),"already dispatched actual result can bind after available cancelled");
  Need(!Bool(q,"ApplyCensus",Counts(129),true),"over-limit census fails closed");
  Need(Count(q,"GetPhysical",0)==-1&&!Bool(q,"CanReserve",1,1),"unknown census closes all new buckets");
  q=New(type);Census(q,1,0,128);
  Need(Bool(q,"RegisterExistingBinding","existingCar",0),"initial existing identity binds without increment");
  Need(Count(q,"GetPhysical",0)==1&&Count(q,"GetReserved",0)==0,"existing body not charged twice");
  Need(Bool(q,"RegisterExistingBinding","existingCar",0)&&!Bool(q,"RegisterExistingBinding","existingCar",1),"existing identity cannot be stolen by another bucket");
  Need(!Bool(q,"TransferBinding","existingCar",0,5),"destination full retains source custody");
  Need(Count(q,"GetPhysical",0)==1&&Count(q,"GetPhysical",5)==128&&(int)Call(q,"GetBindingBucket","existingCar")==0,"failed transfer never frees source");
  Census(q,1,0,127);Need(Bool(q,"TransferBinding","existingCar",0,5),"released destination capacity admits atomic transfer");
  Need(Count(q,"GetPhysical",0)==0&&Count(q,"GetPhysical",5)==128,"transfer preserves physical total");
  Need(!Bool(q,"TransferBinding","existingCar",0,1),"stale source bucket cannot transfer new ownership");
  var fleet=Call(q,"Reserve",3,20,71);Need(fleet!=null&&!Bool(q,"TransferBinding","existingCar",5,3),"reserved destination also prevents borrowing");
  var moved=Call(q,"BeginDispatch",fleet,71);Need(Bool(q,"BindTransferred",moved,71,"existingCar",5),"reserved transaction reuses existing native car");
  Need(Count(q,"GetPhysical",3)==1&&Count(q,"GetPhysical",5)==127&&Count(q,"GetReserved",3)==19,"reuse consumes target reservation and source physical once");
  Need(!Bool(q,"BindTransferred",moved,71,"existingCar",3),"transfer ticket cannot be reused");
  Need(Bool(q,"CancelAvailable",fleet,71)&&Count(q,"GetReserved",3)==0&&Count(q,"GetPhysical",3)==1,"only new outstanding delta cancelled, existing car preserved");
  return Cases;
 }
 public static int Matrix(Type type)
 {
  Cases=0;
  for(int count=1;count<=128;count++)
  {
   var q=New(type);Census(q,128-count);var basket=Call(q,"Reserve",0,count,1);Need(basket!=null,"exact remaining basket fits");
   Need(Call(q,"Reserve",0,1,2)==null,"all exact pending capacity protects other buckets");
   for(int i=0;i<count;i++){var t=Call(q,"BeginDispatch",basket,1);Need(t!=null&&Bool(q,"Bind",t,1,"physical"+i),"one ticket one exact native identity");}
   Need(Count(q,"GetReserved",0)==0&&Count(q,"GetPhysical",0)==128,"all pending converted once");
   Need(Call(q,"BeginDispatch",basket,1)==null,"basket exhausted after N callbacks");
  }
  return Cases;
 }
}
'@
Add-Type -TypeDefinition $code -Language CSharp
$actual=('AICF_AIQuotaLedger_Actual'-as[type]);$cases=[AICF_QuotaLedgerCases]::Run($actual)+[AICF_QuotaLedgerCases]::Matrix($actual)
$mutants=0
foreach($variant in $variants | Select-Object -Skip 1){
 $caught=$false;try{[void][AICF_QuotaLedgerCases]::Run(('AICF_AIQuotaLedger_'+$variant.name)-as[type])}catch{$caught=$true}
 if(!$caught){throw ('Unsafe actual ledger mutant survived: '+$variant.name)};$mutants++
}
"PASS AI quota ledger actual_extracted_cases=$cases executed_unsafe_mutants=$mutants native_identity_adapter=NOT_IMPLEMENTED runtime=NOT_RUN"
