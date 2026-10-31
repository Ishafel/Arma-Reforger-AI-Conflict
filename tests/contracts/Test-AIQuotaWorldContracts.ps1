param([string]$RepositoryRoot=(Split-Path -Parent (Split-Path -Parent $PSScriptRoot)))
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot '../lib/Stage3StaticAudit.Common.ps1')
$root=Join-Path $RepositoryRoot 'AIConflictCore/Scripts/Game/AIConflict'
$math=[IO.File]::ReadAllText((Join-Path $root 'Forces/AICF_AIQuotaMath.c'))
$limits=[IO.File]::ReadAllText((Join-Path $root 'Forces/AICF_AIQuotaLimits.c'))
$world=[IO.File]::ReadAllText((Join-Path $root 'Forces/AICF_AIQuotaWorldState.c'))
$controller=[IO.File]::ReadAllText((Join-Path $root 'Bootstrap/AICF_MatchController.c'))
$ownerBody=Get-AICFMethodBody ([pscustomobject]@{Source=$controller;Code=(ConvertTo-AICFCodeText $controller)}) 'IsAIQuotaOwnerCurrent'
if($world-match'\b(SetLimitOfActiveAIs|ActivateAI|DeactivateAI|SetLOD|SetHealth|DeleteRplEntity|CallLater)\s*\('){throw 'World identity object changes gameplay'}
$split=$world.IndexOf('modded class SCR_AIWorld')
if($split-lt0){throw 'Actual world-owned field missing'}
$stateSource=$world.Substring(0,$split)
$factorySource=$world.Substring($split)
$variants=@(
 [pscustomobject]@{name='Actual';source=$stateSource},
 [pscustomobject]@{name='RatchetingOriginal';source=$stateSource.Replace('return m_iOriginalNativeLimit;','return m_World.GetLimitOfActiveAIs();')},
 [pscustomobject]@{name='StoppedOwner';source=$stateSource.Replace('owner.IsAIQuotaOwnerCurrent(m_World)','true')},
 [pscustomobject]@{name='OldMainWorld';source=$stateSource.Replace('GetGame().GetWorld() == m_MainWorld','true')},
 [pscustomobject]@{name='ReusedEpoch';source=$stateSource.Replace('m_iLastEpoch++;','m_iLastEpoch = 1;')},
 [pscustomobject]@{name='RegrowGrandfather';source=$stateSource.Replace('if (!m_bStockCaptured)','if (true)')},
 [pscustomobject]@{name='WrongEndEpoch';source=$stateSource.Replace('epoch != m_iActiveEpoch','false')}
)
function Adapt([string]$source){
 $source=[regex]::Replace($source,'(?s)enum AICF_EAIQuotaBucket\s*\{.*?\}','')
 $source=$source.Replace('AICF_EAIQuotaBucket bucket','int bucket').Replace('array<int>','AICF_QuotaWorldArray').Replace('protected ref ','protected ')
 $source=$source.Replace('static const int','public const int').Replace('static bool','public static bool').Replace('EntityID','long').Replace('int.MAX','int.MaxValue')
 $source=$source.Replace('GetGame()','AICF_QuotaWorldRuntime.Game')
 $source=[regex]::Replace($source,'(?m)^(\s*)(bool|int|void|AICF_AIQuotaWorldState\w*) (\w+)\(','$1public $2 $3(')
 $source=[regex]::Replace($source,'public void (AICF_AIQuotaWorldState\w*)\(','public $1(')
 $source=$source.Replace('m_aLimits = {};','m_aLimits = new AICF_QuotaWorldArray();').Replace('AICF_QuotaWorldArray candidate = {};','AICF_QuotaWorldArray candidate = new AICF_QuotaWorldArray();')
 $source=$source.Replace('int sum;','int sum = 0;').Replace('int total;','int total = 0;').Replace('for (int index;','for (int index = 0;').Replace('foreach (int value : limits)','foreach (int value in limits)')
 $source=$source.Replace('AddBounded(sum, value, MAX_TOTAL_UNITS, next)','AddBounded(sum, value, MAX_TOTAL_UNITS, out next)').Replace('TotalLimits(limits, total)','TotalLimits(limits, out total)').Replace('TotalLimits(candidate, total)','TotalLimits(candidate, out total)').Replace('AddBounded(physical[index], reserved[index], limits[index], occupied)','AddBounded(physical[index], reserved[index], limits[index], out occupied)')
 return $source
}
$code=@'
#pragma warning disable 0162,0168
using System;
using System.Collections.Generic;
public class AICF_QuotaWorldNative
{
 public static implicit operator bool(AICF_QuotaWorldNative value){return value!=null;}
}
public class AICF_QuotaWorldArray:List<int>
{
 public new int Count(){return base.Count;}
 public void Resize(int size){while(base.Count<size)Add(0);if(base.Count>size)RemoveRange(size,base.Count-size);}
 public static bool operator !(AICF_QuotaWorldArray value){return value==null;}
}
public static class AICF_EAIQuotaBucket
{
 public const int STOCK=0,COMBAT=1,BUILDERS=2,FLEET=3,FIA_EXTRA=4,STOCK_VEHICLES=5,COUNT=6;
}
public static class Replication {public static bool Server=true;public static bool IsServer(){return Server;}}
public class BaseWorld:AICF_QuotaWorldNative {}
public class AICF_QuotaCampaign:AICF_QuotaWorldNative {public bool Master=true;public bool IsMaster(){return Master;}}
public class AICF_QuotaGame:AICF_QuotaWorldNative
{
 public BaseWorld Main;public SCR_AIWorld AI;
 public BaseWorld GetWorld(){return Main;}public SCR_AIWorld GetAIWorld(){return AI;}
}
public static class AICF_QuotaWorldRuntime {public static AICF_QuotaGame Game;}
public partial class SCR_AIWorld:AICF_QuotaWorldNative
{
 public BaseWorld Main;public long Id=7;public int Limit=128;
 public BaseWorld GetWorld(){return Main;}public long GetID(){return Id;}public int GetLimitOfActiveAIs(){return Limit;}
}
public class AICF_MatchController:AICF_QuotaWorldNative
{
 public bool m_bStarted=true,m_bStopped;
 public static AICF_MatchController s_ActiveController;
 public AICF_QuotaCampaign m_Campaign=new AICF_QuotaCampaign();
 public bool IsAIQuotaOwnerCurrent(SCR_AIWorld world)
 {
'@
$code+=(Adapt $ownerBody)+"`n }`n}`n"
$code+=(Adapt $math).Replace('class AICF_AIQuotaMath','public class AICF_AIQuotaMath')
$code+=(Adapt $limits).Replace('class AICF_AIQuotaLimits','public class AICF_AIQuotaLimits')
foreach($variant in $variants){
 $name='AICF_AIQuotaWorldState_'+$variant.name
 $body=Adapt ($variant.source.Replace('AICF_AIQuotaWorldState',$name))
 $body=$body.Replace(('class '+$name),('public class '+$name+':AICF_QuotaWorldNative'))
 $code+="`n"+$body
}
$factory=Adapt ($factorySource.Replace('AICF_AIQuotaWorldState','AICF_AIQuotaWorldState_Actual'))
$code+="`n"+$factory.Replace('modded class SCR_AIWorld','public partial class SCR_AIWorld')
$code+=@'
public static class AICF_QuotaWorldCases
{
 static int Count;
 static void Need(bool value,string reason){Count++;if(!value)throw new Exception(reason);}
 public static int Run(Type type)
 {
  Count=0;Replication.Server=true;
  var main=new BaseWorld();var ai=new SCR_AIWorld{Main=main};
  AICF_QuotaWorldRuntime.Game=new AICF_QuotaGame{Main=main,AI=ai};
  var owner=new AICF_MatchController();AICF_MatchController.s_ActiveController=owner;
  var state=Activator.CreateInstance(type,new object[]{ai});
  Func<bool> Current=()=>(bool)type.GetMethod("IsCurrentWorld").Invoke(state,null);
  Func<AICF_MatchController,int,bool> Session=(who,epoch)=>(bool)type.GetMethod("IsSessionCurrent").Invoke(state,new object[]{who,epoch});
  Func<AICF_MatchController,int,int,object[]> Begin=(who,stock,vehicles)=>{
   object[] args={who,stock,vehicles,null,0};bool accepted=(bool)type.GetMethod("BeginSession").Invoke(state,args);
   return new object[]{accepted,args[3],args[4]};
  };
  Action<AICF_MatchController,int> End=(who,epoch)=>type.GetMethod("EndSession").Invoke(state,new object[]{who,epoch});
  Need(Current(),"Current exact authoritative world");
  ai.Limit=504;
  Need((int)type.GetMethod("GetOriginalNativeLimit").Invoke(state,null)==128,"Original world limit does not ratchet");
  var invalid=Begin(owner,-1,0);Need(!(bool)invalid[0]&&invalid[1]==null&&(int)invalid[2]==0,"Unknown stock census cannot create epoch");
  var first=Begin(owner,189,2);Need((bool)first[0]&&(int)first[2]==1,"First captured epoch");
  var limits=(AICF_AIQuotaLimits)first[1];Need(limits.GetOriginalStockLimit()==128&&limits.GetStockCeiling()==189&&limits.GetStockVehicleCeiling()==128,"Exact first captured quotas");
  Need(Session(owner,1)&&!(bool)Begin(owner,300,300)[0],"Duplicate current owner cannot rebind epoch");
  owner.m_bStopped=true;Need(!Session(owner,1),"Stopped owner cannot use epoch");owner.m_bStopped=false;
  owner.m_bStarted=false;Need(!Session(owner,1),"Unstarted owner cannot use epoch");owner.m_bStarted=true;
  owner.m_Campaign.Master=false;Need(!Session(owner,1),"Nonmaster campaign cannot use epoch");owner.m_Campaign.Master=true;
  Replication.Server=false;Need(!Current()&&!Session(owner,1),"Client gets no quota owner");Replication.Server=true;
  var wrongWorld=new BaseWorld();AICF_QuotaWorldRuntime.Game.Main=wrongWorld;Need(!Current(),"New main world invalidates old capture");AICF_QuotaWorldRuntime.Game.Main=main;
  var otherAi=new SCR_AIWorld{Main=main};AICF_QuotaWorldRuntime.Game.AI=otherAi;Need(!Current(),"New AIWorld object invalidates old capture");AICF_QuotaWorldRuntime.Game.AI=ai;
  ai.Id=8;Need(!Current(),"AIWorld entity identity changed");ai.Id=7;
  End(owner,2);Need(Session(owner,1),"Wrong end epoch preserves live successor");
  var foreign=new AICF_MatchController();End(foreign,1);Need(Session(owner,1),"Foreign owner cannot end session");
  End(owner,1);Need(!Session(owner,1),"Exact end retires only own epoch");
  var second=Begin(owner,300,300);Need((bool)second[0]&&(int)second[2]==2,"Same-world reuse gets fresh monotonically increasing epoch");
  var secondLimits=(AICF_AIQuotaLimits)second[1];Need(secondLimits.GetStockCeiling()==189&&secondLimits.GetInitialStockUnits()==189&&secondLimits.GetStockVehicleCeiling()==128,"World grandfather snapshot does not regrow on restart");
  Need(!Session(owner,1)&&Session(owner,2),"Old callback cannot own restarted session");
  End(owner,1);Need(Session(owner,2),"Old same-owner end cannot erase new epoch");
  AICF_MatchController.s_ActiveController=foreign;Need(!Session(owner,2),"Owner replacement invalidates old callback");
  var third=Begin(foreign,300,300);Need((bool)third[0]&&(int)third[2]==3&&Session(foreign,3),"Current new controller can adopt same world without resetting stock snapshot");
  End(owner,2);Need(Session(foreign,3),"Prior controller cannot clear successor");
  return Count;
 }
 public static int Factory()
 {
  Count=0;Replication.Server=true;var main=new BaseWorld();var ai=new SCR_AIWorld{Main=main};AICF_QuotaWorldRuntime.Game=new AICF_QuotaGame{Main=main,AI=ai};
  var a=ai.AICF_GetQuotaWorldState();ai.Limit=504;var b=ai.AICF_GetQuotaWorldState();Need(a==b&&a.GetOriginalNativeLimit()==128,"Actual world field holds original across calls");
  Replication.Server=false;Need(ai.AICF_GetQuotaWorldState()==null,"Client world field getter has no authority capture");Replication.Server=true;
  AICF_QuotaWorldRuntime.Game.AI=new SCR_AIWorld{Main=main};Need(ai.AICF_GetQuotaWorldState()==null,"Wrong world cannot expose old field");
  return Count;
 }
}
'@
Add-Type -TypeDefinition $code -Language CSharp
$cases=[AICF_QuotaWorldCases]::Run([AICF_AIQuotaWorldState_Actual])+[AICF_QuotaWorldCases]::Factory();$mutants=0
foreach($variant in ($variants|Select-Object -Skip 1)){
 $caught=$false;try{[void][AICF_QuotaWorldCases]::Run(('AICF_AIQuotaWorldState_'+$variant.name)-as[type])}catch{$caught=$true}
 if(!$caught){throw "Unsafe world lifecycle mutant escaped: $($variant.name)"};$mutants++
}
"PASS AI quota world ownership: actual_extracted_cases=$cases executed_unsafe_mutants=$mutants floor_integration=NOT_IMPLEMENTED runtime=NOT_RUN"
