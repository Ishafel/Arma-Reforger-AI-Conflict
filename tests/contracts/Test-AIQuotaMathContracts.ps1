param([string]$RepositoryRoot=(Split-Path -Parent (Split-Path -Parent $PSScriptRoot)))
$ErrorActionPreference='Stop'
$path=Join-Path $RepositoryRoot 'AIConflictCore/Scripts/Game/AIConflict/Forces/AICF_AIQuotaMath.c'
$source=[IO.File]::ReadAllText($path)
if($source -match '\b(SetLimitOfActiveAIs|RequestSpawn|ActivateAI|DeactivateAI|SetLOD|SetHealth|DeleteRplEntity|CallLater)\s*\('){throw 'Quota arithmetic owns a gameplay side effect'}
$variants=@(
 [pscustomobject]@{name='Actual';source=$source},
 [pscustomobject]@{name='IgnoreReservation';source=$source.Replace('return reserved <= remaining && requested <= remaining - reserved;','return requested <= remaining;')},
 [pscustomobject]@{name='BorrowOtherBucket';source=$source.Replace('return FitsBucket(physical[bucket], reserved[bucket], limits[bucket], requested);','return true;')},
 [pscustomobject]@{name='UnknownOtherBucket';source=$source.Replace('if (!AddBounded(physical[index], reserved[index], limits[index], occupied))','if (false)')},
 [pscustomobject]@{name='UnboundedTotal';source=$source.Replace('MAX_TOTAL_UNITS = 4096','MAX_TOTAL_UNITS = 2147483647')},
 [pscustomobject]@{name='ZeroRequest';source=$source.Replace('requested <= 0','requested < 0')},
 [pscustomobject]@{name='LostAddition';source=$source.Replace('result = current + amount;','result = current;')}
)
$code=@'
#pragma warning disable 0162,0168
using System;
using System.Collections.Generic;
public class AICF_QuotaMathArray : List<int>
{
 public AICF_QuotaMathArray(params int[] values):base(values){}
 public new int Count(){return base.Count;}
 public static bool operator !(AICF_QuotaMathArray value){return value==null;}
}
public static class AICF_QuotaMathBucket
{
 public const int STOCK=0,COMBAT=1,BUILDERS=2,FLEET=3,FIA_EXTRA=4,STOCK_VEHICLES=5,COUNT=6;
}
'@
foreach($variant in $variants){
 $body=[regex]::Replace($variant.source,'(?s)enum AICF_EAIQuotaBucket\s*\{.*?\}','')
 $body=$body.Replace('AICF_EAIQuotaBucket bucket','int bucket').Replace('AICF_EAIQuotaBucket.','AICF_QuotaMathBucket.')
 $body=$body.Replace('array<int>','AICF_QuotaMathArray').Replace('static const int','public const int').Replace('static bool','public static bool')
 $body=$body.Replace('class AICF_AIQuotaMath',('public class AICF_AIQuotaMath_'+$variant.name))
 $body=$body.Replace('int sum;','int sum = 0;').Replace('for (int index;','for (int index = 0;').Replace('foreach (int value : limits)','foreach (int value in limits)')
 $body=$body.Replace('AddBounded(sum, value, MAX_TOTAL_UNITS, next)','AddBounded(sum, value, MAX_TOTAL_UNITS, out next)')
 $body=$body.Replace('TotalLimits(limits, total)','TotalLimits(limits, out total)').Replace('AddBounded(physical[index], reserved[index], limits[index], occupied)','AddBounded(physical[index], reserved[index], limits[index], out occupied)')
 $code+="`n"+$body
}
$code+=@'
public static class AICF_QuotaMathCases
{
 public delegate bool Add(int current,int amount,int ceiling,out int result);
 public delegate bool Partition(AICF_QuotaMathArray physical,AICF_QuotaMathArray reserved,AICF_QuotaMathArray limits,int bucket,int requested);
 public delegate bool Total(AICF_QuotaMathArray limits,out int result);
 static int Checks;
 static void Need(bool value,string reason){Checks++;if(!value)throw new Exception(reason);}
 public static int Run(Type type)
 {
  Checks=0;
  var add=(Add)Delegate.CreateDelegate(typeof(Add),type.GetMethod("AddBounded"));
  var fits=(Func<int,int,int,int,bool>)Delegate.CreateDelegate(typeof(Func<int,int,int,int,bool>),type.GetMethod("FitsBucket"));
  var partition=(Partition)Delegate.CreateDelegate(typeof(Partition),type.GetMethod("FitsPartition"));
  var total=(Total)Delegate.CreateDelegate(typeof(Total),type.GetMethod("TotalLimits"));
  int[] values={-1,0,1,4,5,127,128,219,220,4096,int.MaxValue};
  foreach(int a in values)foreach(int b in values)foreach(int cap in values)
  {
   int result;bool actual=add(a,b,cap,out result);
   bool expected=a>=0&&b>=0&&cap>=0&&(long)a+b<=cap;
   Need(actual==expected,"Addition eligibility/overflow");
   Need(result==(expected?(int)((long)a+b):-1),"Exact sum or unknown sentinel");
  }
  foreach(int physical in values)foreach(int pending in values)foreach(int cap in values)foreach(int request in values)
  {
   bool expected=physical>=0&&pending>=0&&cap>=0&&request>0&&(long)physical+pending+request<=cap;
   Need(fits(physical,pending,cap,request)==expected,"Bucket conservation and no borrowing");
  }
  int sum;
  var limits=new AICF_QuotaMathArray(128,220,8,20,0,128);
  Need(total(limits,out sum)&&sum==504,"Separated stock+combat+builder+fleet sum");
  Need(!total(null,out sum)&&sum==-1,"Missing limits unknown");
  Need(!total(new AICF_QuotaMathArray(128,220),out sum),"Incomplete partition unknown");
  Need(!total(new AICF_QuotaMathArray(128,220,-1,20,0,0),out sum),"Negative FIA/builder unknown");
  Need(!total(new AICF_QuotaMathArray(128,220,8,20,3721,0),out sum),"Bounded immutable FIA plan");
  Need(!total(new AICF_QuotaMathArray(int.MaxValue,220,8,20,0,0),out sum),"No total integer overflow");
  var empty=new AICF_QuotaMathArray(0,0,0,0,0,0);
  var stockFull=new AICF_QuotaMathArray(128,219,0,0,0,0);
  Need(!partition(stockFull,empty,limits,0,1),"Stock cannot consume free managed headroom");
  Need(partition(stockFull,empty,limits,1,1),"Managed reservation survives stock ceiling");
  Need(partition(stockFull,empty,limits,5,1),"Stock vehicle activation has independent bounded headroom");
  Need(!partition(new AICF_QuotaMathArray(0,0,0,0,0,128),empty,limits,5,1),"Stock vehicles cannot borrow managed headroom");
  var combatFull=new AICF_QuotaMathArray(127,220,0,0,0,0);
  Need(partition(combatFull,empty,limits,0,1),"Stock own slot remains independent");
  Need(!partition(combatFull,empty,limits,1,1),"Managed cannot borrow stock headroom");
  Need(partition(new AICF_QuotaMathArray(128,220,7,20,0,0),empty,limits,2,1),"Builder separate bounded slot");
  Need(!partition(new AICF_QuotaMathArray(128,220,8,20,0,128),empty,limits,3,1),"Fleet own quota exhausted");
  Need(!partition(empty,empty,limits,4,1),"No unplanned FIA reserve");
  Need(!partition(new AICF_QuotaMathArray(0,0,-1,0,0,0),empty,limits,0,1),"Unknown other bucket fails closed");
  Need(!partition(empty,new AICF_QuotaMathArray(0,0,0,-1,0,0),limits,0,1),"Unknown pending fails closed");
  Need(!partition(null,empty,limits,0,1),"Missing physical census unknown");
  Need(!partition(empty,null,limits,0,1),"Missing pending ledger unknown");
  Need(!partition(empty,empty,limits,-1,1)&&!partition(empty,empty,limits,6,1),"Invalid bucket");
  Need(!partition(empty,empty,limits,0,0),"No empty request permission");
  Need(!partition(new AICF_QuotaMathArray(127,0,0,0,0,0),new AICF_QuotaMathArray(1,0,0,0,0,0),limits,0,1),"In-flight reservation cannot be stolen");
  Need(partition(new AICF_QuotaMathArray(127,0,0,0,0,0),empty,limits,0,1),"Release allows next exact unit");
  Need(!partition(new AICF_QuotaMathArray(128,0,0,0,0,0),empty,limits,0,1),"Materialized reservation still consumes exact unit");
  return Checks;
 }
}
'@
Add-Type -TypeDefinition $code -Language CSharp
$actual=[AICF_QuotaMathCases]::Run([AICF_AIQuotaMath_Actual])
$mutants=0
foreach($variant in ($variants|Select-Object -Skip 1)){
 $caught=$false
 try{[void][AICF_QuotaMathCases]::Run(('AICF_AIQuotaMath_'+$variant.name)-as[type])}catch{$caught=$true}
 if(!$caught){throw "Unsafe actual math mutant escaped: $($variant.name)"}
 $mutants++
}
"PASS AI quota arithmetic: actual_extracted_cases=$actual executed_unsafe_mutants=$mutants runtime=NOT_RUN"
