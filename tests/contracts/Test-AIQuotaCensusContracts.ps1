param([string]$RepositoryRoot=(Split-Path -Parent (Split-Path -Parent $PSScriptRoot)))
$ErrorActionPreference='Stop'
$source=[IO.File]::ReadAllText((Join-Path $RepositoryRoot 'AIConflictCore/Scripts/Game/AIConflict/Forces/AICF_AIQuotaCensus.c'))
if($source-match'\b(SetLimitOfActiveAIs|SpawnEntity|ActivateAI|SetLOD|DeleteRplEntity|CallLater|SetReserved)\s*\('){throw 'Read-only census performs gameplay mutation'}
function Adapt([string]$text){
 $text=$text.Replace('EntityID','long').Replace('GetGame()','AICF_CensusRuntime.Game').Replace('protected ref ','protected ')
 $text=[regex]::Replace($text,'array<(?:ref )?(\w+)>','AICF_CensusList<$1>')
 $text=[regex]::Replace($text,'(AICF_CensusList<\w+>) (\w+) = \{\};','$1 $2 = new $1();')
 $text=[regex]::Replace($text,'foreach \((\w+) (\w+) : (\w+)\)','foreach ($1 $2 in $3)')
 $text=[regex]::Replace($text,'(?m)^(\s*)(static bool|bool|int|string|AICF_AIQuotaAgentRow) (\w+)\(','$1public $2 $3(')
 $text=[regex]::Replace($text,'(?m)^(class AICF_AIQuota\w*)\s*$','public $1 : AICF_CensusNative')
 $text=[regex]::Replace($text,'(?m)^(\s*)(AIAgent|IEntity|AIGroup|long|bool) (m_\w+);','$1public $2 $3;')
 $text=$text.Replace('int invalid;','int invalid = 0;').Replace('int examined;','int examined = 0;').Replace('int countedActive;','int countedActive = 0;')
 return $text
}
$variants=@(
 [pscustomobject]@{name='Actual';source=$source},
 [pscustomobject]@{name='NullEntry';source=$source.Replace('invalid++;','')},
 [pscustomobject]@{name='IgnoreActive';source=$source.Replace('countedActive == afterActive','true')},
 [pscustomobject]@{name='IgnoreIntended';source=$source.Replace('beforeIntended == afterIntended','true')},
 [pscustomobject]@{name='IgnoreAgentOwner';source=$source.Replace('m_Agent.GetParentGroup() != m_Group','false')},
 [pscustomobject]@{name='IgnoreWorldLookup';source=$source.Replace('world.FindEntityByID(m_EntityId) != m_Entity','false')},
 [pscustomobject]@{name='SkipRepeated';source=$source.Replace('if (!IsCurrent())','if (false)')}
)
$code=@'
#pragma warning disable 0162,0168
using System;
using System.Collections.Generic;
public class AICF_CensusNative {public static implicit operator bool(AICF_CensusNative value){return value!=null;}}
public class AICF_CensusList<T>:List<T>{public new int Count(){return base.Count;}public void Insert(T value){Add(value);}}
public static class Replication {public static bool Server=true;public static bool IsServer(){return Server;}}
public static class AICF_AIQuotaMath {public const int MAX_TOTAL_UNITS=4096;}
public class BaseWorld:AICF_CensusNative {public Dictionary<long,IEntity> Entities=new Dictionary<long,IEntity>();public IEntity FindEntityByID(long id){IEntity e;return Entities.TryGetValue(id,out e)?e:null;}}
public class IEntity:AICF_CensusNative {public BaseWorld World;public long Id;public BaseWorld GetWorld(){return World;}public long GetID(){return Id;}}
public class Vehicle:IEntity {public static Vehicle Cast(IEntity e){return e as Vehicle;}}
public class AIGroup:IEntity {}
public class AIAgent:IEntity
{
 public IEntity Entity;public AIGroup Parent;public bool Counted,Active;
 public IEntity GetControlledEntity(){return Entity;}public AIGroup GetParentGroup(){return Parent;}
 public bool CountTowardsAIWorldLimit(){return Counted;}public bool IsAIActivated(){return Active;}
}
public class SCR_AIWorld:IEntity
{
 public int Active,Intended,ReadCalls;public Action<int> OnRead;public AICF_CensusList<AIAgent> Agents=new AICF_CensusList<AIAgent>();
 public int GetCurrentNumOfActiveAIs(){return Active;}public int GetActiveGroupIntendedCount(){return Intended;}
 public void GetAIAgents(AICF_CensusList<AIAgent> output){ReadCalls++;if(OnRead!=null)OnRead(ReadCalls);output.AddRange(Agents);}
}
public class AICF_CensusGame:AICF_CensusNative {public SCR_AIWorld AI;public BaseWorld Main;public BaseWorld GetWorld(){return Main;}public SCR_AIWorld GetAIWorld(){return AI;}}
public static class AICF_CensusRuntime {public static AICF_CensusGame Game;}
'@
foreach($variant in $variants){$code+=Adapt ($variant.source.Replace('AICF_AIQuotaCensus','AICF_AIQuotaCensus_'+$variant.name).Replace('AICF_AIQuotaAgentRow','AICF_AIQuotaAgentRow_'+$variant.name))}
$code+=@'
public static class AICF_CensusCases
{
 static int Cases;
 static void Need(bool value,string label){Cases++;if(!value)throw new Exception(label);}
 static SCR_AIWorld Setup()
 {
  Replication.Server=true;var main=new BaseWorld();var world=new SCR_AIWorld{World=main,Id=1,Active=2,Intended=2};AICF_CensusRuntime.Game=new AICF_CensusGame{Main=main,AI=world};
  var g=new AIGroup{World=main,Id=20};main.Entities.Add(20,g);
  for(int n=0;n<3;n++){IEntity e=n==1?(IEntity)new Vehicle():new IEntity();e.World=main;e.Id=30+n;main.Entities.Add(e.Id,e);world.Agents.Add(new AIAgent{World=main,Id=40+n,Entity=e,Parent=g,Counted=true,Active=n<2});}
  return world;
 }
 static bool Capture(object census,SCR_AIWorld world){return (bool)census.GetType().GetMethod("Capture").Invoke(census,new object[]{world});}
 static bool Current(object census){return (bool)census.GetType().GetMethod("IsCurrent").Invoke(census,null);}
 public static int Run(Type type)
 {
  Cases=0;var w=Setup();var c=Activator.CreateInstance(type);Need(Capture(c,w),"all exact counted identities/native counters valid");Need(Current(c),"fresh second registered set/row predicates");
  Need((int)type.GetMethod("Count").Invoke(c,null)==3,"inactive counted body retained separately from nativeActive2");
  w.Agents[0].Parent=new AIGroup();Need(!Current(c),"native current group change rejected");
  w=Setup();c=Activator.CreateInstance(type);w.World.Entities.Remove(30);Need(!Capture(c,w),"missing actual world identity rejected");
  w=Setup();c=Activator.CreateInstance(type);w.World.Entities.Remove(20);Need(!Capture(c,w),"stale native parent group world identity rejected");
  w=Setup();c=Activator.CreateInstance(type);w.Agents.Add(null);Need(!Capture(c,w),"null native array entry is incomplete");
  w=Setup();c=Activator.CreateInstance(type);w.Agents.Add(w.Agents[0]);Need(!Capture(c,w),"duplicate native agent rejected");
  w=Setup();c=Activator.CreateInstance(type);w.Active=3;Need(!Capture(c,w),"native active discrepancy rejected");
  w=Setup();c=Activator.CreateInstance(type);w.OnRead=n=>{if(n==1)w.Intended=3;};Need(!Capture(c,w),"native intended change during capture rejected");
  w=Setup();c=Activator.CreateInstance(type);w.OnRead=n=>{if(n==2)w.Agents[2].Counted=false;};Need(!Capture(c,w),"changed inactive row on repeated set rejected");
  w=Setup();c=Activator.CreateInstance(type);w.OnRead=n=>{if(n==2)w.Agents.Add(new AIAgent());};Need(!Capture(c,w),"new uncounted native identity cannot hide behind unchanged counters");
  w=Setup();c=Activator.CreateInstance(type);w.Agents[2].Entity=w.Agents[0].Entity;Need(!Capture(c,w),"duplicate controlled entity rejected");
  w=Setup();c=Activator.CreateInstance(type);w.Agents[2].Entity=null;Need(!Capture(c,w),"counted missing entity unknown");
  w=Setup();c=Activator.CreateInstance(type);Replication.Server=false;Need(!Capture(c,w),"client cannot create authoritative census");
  w=Setup();c=Activator.CreateInstance(type);AICF_CensusRuntime.Game.Main=new BaseWorld();Need(!Capture(c,w),"private/stale world rejected");
  w=Setup();c=Activator.CreateInstance(type);w.Agents.Clear();w.Active=0;w.Intended=0;Need(Capture(c,w),"legitimate actual empty native world succeeds");
  return Cases;
 }
 public static int Complete(Type type)
 {
  Cases=0;var m=type.GetMethod("IsComplete");
  for(int n=0;n<=4097;n++){bool v=(bool)m.Invoke(null,new object[]{n,n,0,0,0,0,0,0});Need(v==(n<=4096),"bounded complete native list");}
  foreach(var input in new[]{new[]{2,1,0,2,2,2,2,2},new[]{2,2,1,2,2,2,2,2},new[]{2,2,0,1,2,2,2,2},new[]{2,2,0,2,2,3,2,2},new[]{2,2,0,2,2,2,2,3}})
  {object[] args=Array.ConvertAll(input,x=>(object)x);Need(!(bool)m.Invoke(null,args),"incomplete/unreconciled native census cannot become known");}
  return Cases;
 }
}
'@
# Native C# access/default adapters preserve actual bodies; variants remain distinct.
$code=[regex]::Replace($code,'(?m)^(\s*)(bool|int|string|AICF_AIQuotaAgentRow\w*) (\w+)\(','$1public $2 $3(')
Add-Type -TypeDefinition $code -Language CSharp
$actual='AICF_AIQuotaCensus_Actual'-as[type];$cases=[AICF_CensusCases]::Run($actual)+[AICF_CensusCases]::Complete($actual)
$mutants=0
foreach($variant in $variants|Select-Object -Skip 1){$caught=$false;try{[void][AICF_CensusCases]::Run(('AICF_AIQuotaCensus_'+$variant.name)-as[type])}catch{$caught=$true};if(!$caught){throw ('Unsafe census mutant survived: '+$variant.name)};$mutants++}
"PASS AI quota census actual_extracted_cases=$cases executed_unsafe_mutants=$mutants runtime=NOT_RUN"
