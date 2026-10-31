param([string]$RepositoryRoot=(Split-Path -Parent (Split-Path -Parent $PSScriptRoot)))
$ErrorActionPreference='Stop'
$math=[IO.File]::ReadAllText((Join-Path $RepositoryRoot 'AIConflictCore/Scripts/Game/AIConflict/Forces/AICF_AIQuotaMath.c'))
$limits=[IO.File]::ReadAllText((Join-Path $RepositoryRoot 'AIConflictCore/Scripts/Game/AIConflict/Forces/AICF_AIQuotaLimits.c'))
foreach($source in @($math,$limits)){
 if($source-match'\b(SetLimitOfActiveAIs|RequestSpawn|ActivateAI|DeactivateAI|SetLOD|SetHealth|DeleteRplEntity|CallLater)\s*\('){throw 'Quota value object has gameplay side effects'}
}
$variants=@(
 [pscustomobject]@{name='Actual';source=$limits},
 [pscustomobject]@{name='IgnoreOriginal';source=$limits.Replace('return originalNativeLimit == m_iOriginalStock;','return true;')},
 [pscustomobject]@{name='Ratcheting';source=$limits.Replace('if (m_bStockCaptured)','if (false)')},
 [pscustomobject]@{name='DiscardGrandfather';source=$limits.Replace('Math.Max(originalNativeLimit, initialStockUnits)','originalNativeLimit')},
 [pscustomobject]@{name='Reconfigure';source=$limits.Replace('!m_bStockCaptured || m_bConfigured ||','!m_bStockCaptured || false ||')},
 [pscustomobject]@{name='FleetOverflow';source=$limits.Replace('fleetUnits > MAX_FLEET_UNITS','fleetUnits > 1000')},
 [pscustomobject]@{name='PartialCommit';source=$limits.Replace('array<int> candidate = {};','m_bConfigured = true; array<int> candidate = {};')},
 [pscustomobject]@{name='DiscardVehicleGrandfather';source=$limits.Replace('Math.Max(originalNativeLimit, initialStockVehicleUnits)','originalNativeLimit')}
)
function Adapt([string]$source){
 $source=[regex]::Replace($source,'(?s)enum AICF_EAIQuotaBucket\s*\{.*?\}','')
 $source=$source.Replace('AICF_EAIQuotaBucket bucket','int bucket').Replace('array<int>','AICF_QuotaLimitsArray').Replace('protected ref ','protected ')
 $source=$source.Replace('static const int','public const int').Replace('static bool','public static bool')
 $source=[regex]::Replace($source,'(?m)^(\s*)(bool|int) (\w+)\(','$1public $2 $3(')
 $source=$source.Replace('m_aLimits = {};','m_aLimits = new AICF_QuotaLimitsArray();').Replace('AICF_QuotaLimitsArray candidate = {};','AICF_QuotaLimitsArray candidate = new AICF_QuotaLimitsArray();')
 $source=$source.Replace('int sum;','int sum = 0;').Replace('int total;','int total = 0;').Replace('for (int index;','for (int index = 0;')
 $source=$source.Replace('foreach (int value : limits)','foreach (int value in limits)').Replace('AddBounded(sum, value, MAX_TOTAL_UNITS, next)','AddBounded(sum, value, MAX_TOTAL_UNITS, out next)')
 $source=$source.Replace('TotalLimits(limits, total)','TotalLimits(limits, out total)').Replace('TotalLimits(candidate, total)','TotalLimits(candidate, out total)').Replace('AddBounded(physical[index], reserved[index], limits[index], occupied)','AddBounded(physical[index], reserved[index], limits[index], out occupied)')
 return $source
}
$code=@'
#pragma warning disable 0162,0168
using System;
using System.Collections.Generic;
public class AICF_QuotaLimitsArray:List<int>
{
 public new int Count(){return base.Count;}
 public void Resize(int size){while(base.Count<size)Add(0);if(base.Count>size)RemoveRange(size,base.Count-size);}
 public static bool operator !(AICF_QuotaLimitsArray value){return value==null;}
}
public static class AICF_EAIQuotaBucket
{
 public const int STOCK=0,COMBAT=1,BUILDERS=2,FLEET=3,FIA_EXTRA=4,STOCK_VEHICLES=5,COUNT=6;
}
'@
$code+="`n"+(Adapt $math).Replace('class AICF_AIQuotaMath','public class AICF_AIQuotaMath')
foreach($variant in $variants){$code+="`n"+(Adapt $variant.source).Replace('class AICF_AIQuotaLimits',('public class AICF_AIQuotaLimits_'+$variant.name))}
$code+=@'
public static class AICF_QuotaLimitsCases
{
 static int Count;
 static void Need(bool value,string reason){Count++;if(!value)throw new Exception(reason);}
 public static int Run(Type type)
 {
  Count=0;
  var capture=type.GetMethod("CaptureStockSnapshot");var configure=type.GetMethod("Configure");
  Func<object,int,int,bool> Capture=(instance,original,initial)=>(bool)capture.Invoke(instance,new object[]{original,initial,0});
  Func<object,int,int,int,int,bool> Configure=(instance,managed,builder,fleet,fia)=>(bool)configure.Invoke(instance,new object[]{managed,builder,fleet,fia});
  Func<object,string,int> Get=(instance,name)=>(int)type.GetMethod(name).Invoke(instance,null);
  Func<object,bool> Ready=instance=>(bool)type.GetMethod("IsConfigured").Invoke(instance,null);
  var fresh=Activator.CreateInstance(type);
  Need(!Ready(fresh)&&Get(fresh,"GetTotalLimit")==-1,"Unknown initial limits");
  Need(!Configure(fresh,220,8,20,0),"No configuration without original snapshot");
  foreach(int value in new[]{-1,0,4097,int.MaxValue})Need(!Capture(fresh,value,0),"Invalid original native budget");
  foreach(int value in new[]{-1,4097,int.MaxValue})Need(!Capture(fresh,128,value),"Invalid initial census");
  Need(Capture(fresh,128,189),"Capture original and grandfathered population");
  Need(Get(fresh,"GetOriginalStockLimit")==128&&Get(fresh,"GetInitialStockUnits")==189&&Get(fresh,"GetStockCeiling")==189,"Original distinct from effective stock ceiling");
  Need(Capture(fresh,128,300)&&Get(fresh,"GetStockCeiling")==189&&Get(fresh,"GetInitialStockUnits")==189,"Same original cannot ratchet grandfather allowance");
  Need(!Capture(fresh,376,300)&&Get(fresh,"GetOriginalStockLimit")==128,"Raised world budget cannot replace original");
  Need(Configure(fresh,220,8,20,0)&&Ready(fresh)&&Get(fresh,"GetTotalLimit")==565,"Exact grandfathered composition");
  Need(!Configure(fresh,220,9,20,0)&&Get(fresh,"GetTotalLimit")==565,"Immutable composed session");
  Need((int)type.GetMethod("GetLimit").Invoke(fresh,new object[]{-1})==-1&&(int)type.GetMethod("GetLimit").Invoke(fresh,new object[]{6})==-1,"Unknown bucket getter");
  Need((int)type.GetMethod("GetLimit").Invoke(fresh,new object[]{0})==189&&(int)type.GetMethod("GetLimit").Invoke(fresh,new object[]{1})==220,"Exact individual quota getters");
  Need(Get(fresh,"GetStockVehicleCeiling")==128&&(int)type.GetMethod("GetLimit").Invoke(fresh,new object[]{5})==128,"Vehicle pool independent from grandfathered stock characters");
  var cars=Activator.CreateInstance(type);
  Need(!(bool)capture.Invoke(cars,new object[]{128,0,-1}),"Unknown initial vehicle units rejected");
  Need((bool)capture.Invoke(cars,new object[]{128,0,189})&&Get(cars,"GetInitialStockVehicleUnits")==189&&Get(cars,"GetStockVehicleCeiling")==189,"Existing active stock vehicles grandfathered once");
  Need((bool)capture.Invoke(cars,new object[]{128,300,300})&&Get(cars,"GetStockVehicleCeiling")==189&&Get(cars,"GetStockCeiling")==128,"Neither pool ratchets on same-world restart");
  Need(Configure(cars,220,8,20,0)&&Get(cars,"GetTotalLimit")==565,"Exact additional vehicle headroom composition");
  int[] manageds={-1,0,1,100,200,220,256,257,int.MaxValue};
  int[] builders={-1,0,8,512,513,int.MaxValue};
  int[] fleets={-1,0,10,20,21,int.MaxValue};
  int[] fias={-1,0,128,3000,4096,int.MaxValue};
  foreach(int managed in manageds)foreach(int builder in builders)foreach(int fleet in fleets)foreach(int fia in fias)
  {
   var instance=Activator.CreateInstance(type);Need(Capture(instance,128,0),"Fresh snapshot capture");
   bool expected=managed>0&&managed<=256&&builder>=0&&builder<=512&&fleet>=0&&fleet<=20&&fia>=0&&256L+managed+builder+fleet+fia<=4096;
   bool actual=Configure(instance,managed,builder,fleet,fia);
   Need(actual==expected,"Bounded complete plan admission");
   Need(Ready(instance)==expected,"No partial configured state on rejected plan");
   Need(Get(instance,"GetTotalLimit")== (expected?(int)(256L+managed+builder+fleet+fia):-1),"No partial total on rejected plan");
  }
  return Count;
 }
}
'@
Add-Type -TypeDefinition $code -Language CSharp
$cases=[AICF_QuotaLimitsCases]::Run([AICF_AIQuotaLimits_Actual]);$mutants=0
foreach($variant in ($variants|Select-Object -Skip 1)){
 $caught=$false;try{[void][AICF_QuotaLimitsCases]::Run(('AICF_AIQuotaLimits_'+$variant.name)-as[type])}catch{$caught=$true}
 if(!$caught){throw "Unsafe limits mutant escaped: $($variant.name)"};$mutants++
}
"PASS AI quota immutable limits: actual_extracted_cases=$cases executed_unsafe_mutants=$mutants world_binding=NOT_IMPLEMENTED runtime=NOT_RUN"
