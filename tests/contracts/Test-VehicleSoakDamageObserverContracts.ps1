param([string]$RepositoryRoot=(Split-Path -Parent (Split-Path -Parent $PSScriptRoot)))
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot '../lib/Stage3StaticAudit.Common.ps1')
$source=[IO.File]::ReadAllText((Join-Path $RepositoryRoot 'tests/fixtures/AICF_VehicleSoakProbe.c'))
function Body([string]$text,[string]$name){Get-AICFMethodBody ([pscustomobject]@{Source=$text;Code=(ConvertTo-AICFCodeText $text)}) $name}
function Rules([string]$text){
 $bind=Body $text 'Bind';$current=Body $text 'IsCurrent';$stop=Body $text 'Stop';$callback=Body $text 'OnDamage'
 foreach($fence in @('m_Actor.GetID() == m_ActorId','m_Actor.GetDamageManager() == m_Damage','m_Group.GetID() == m_GroupId',
  'm_Slot.GetGroup() == m_Group','m_Slot.AICF_VehicleSoakRawRosterMember(m_iPosition) == m_Actor','Replication.FindItemId(m_Actor).ToString() == m_sActorRpl',
  'Replication.FindItemId(m_Group).ToString() == m_sGroupRpl','m_Slot.GetSpawnGeneration() == m_iGeneration')){
  if(!$current.Contains($fence)){'EXACT_ORIGINAL_IDENTITY';break}
 }
 if(!$bind.Contains('if (m_bBound') -or !$bind.Contains('m_Damage.GetOnDamage().Insert(OnDamage)') -or
  !$stop.Contains('m_Damage.GetOnDamage().Remove(OnDamage)') -or !$stop.Contains('m_bBound = false')){'PAIRED_SINGLE_BIND'}
 if(!$callback.Contains('if (!IsCurrent()) { Stop(); return; }') -or !$callback.Contains('context.hitEntity != m_Actor')){'CALLBACK_IDENTITY_FILTER'}
 foreach($fence in @('!Replication.IsServer()','m_Actor.GetWorld() != GetGame().GetWorld()',
  'FindEntityByID(m_ActorId) != m_Actor','FindEntityByID(m_GroupId) != m_Group',
  'agent.GetControlledEntity() != m_Actor','agent.GetParentGroup() != m_Group')){
  if(!$current.Contains($fence)){'CAPTURED_WORLD_AND_NATIVE_PARENT';break}
 }
 $raw=Body $text 'AICF_VehicleSoakRawRosterMember';$poll=Body $text 'Poll'
 if(!$raw.Contains('position < 0 || position >= m_aRosterMembers.Count()') -or
  !$raw.Contains('return m_aRosterMembers[position]') -or $raw.Contains('GetLifeState') -or
  $current.Contains('m_Slot.GetRosterMember(')){'INCAP_RAW_CAPTURE'}
 if(!$poll.Contains('if (!IsCurrent()) { Stop(); return; }') -or !$poll.Contains('life == m_sLastLife') -or
  !$poll.Contains('VEHICLE_SOAK_DAMAGE_LIFE') -or !$poll.Contains('cause=UNDETERMINED')){'LIFE_TRANSITION_READBACK'}
 foreach($field in @('context.damageType','context.damageValue','context.damageSource','context.hitPosition','context.impactVelocity',
  'context.instigator.GetInstigatorEntity()','context.instigator.GetInstigatorType()','m_Actor.GetOrigin()',
  'controller.GetLifeState()','agent.GetParentGroup()','CompartmentAccessComponent.GetVehicleIn(m_Actor)','cause=UNDETERMINED observer_only=1')){
  if(!$callback.Contains($field)){'ACTUAL_CONTEXT_ONLY';break}
 }
 $observer=$text.Substring($text.IndexOf('class AICF_VehicleSoakDamageObserver'),$text.IndexOf('modded class AICF_MatchController')-$text.IndexOf('class AICF_VehicleSoakDamageObserver'))
 if($observer -match '\.(?:SetHealth|SetHealthScaled|FullHeal|Heal|Kill|KillGM|ForceUnconsciousness|Teleport|SetOrigin|SetVelocity|SetDamage|Damage)\s*\('){'OBSERVER_MUTATES_GAMEPLAY'}
 if(@([regex]::Matches($text,'AICF_VehicleSoakStopDamageObservers\(\);')).Count -lt 4 -or
  !$text.Contains('damageObserver.Poll();') -or !$text.Contains('damageMember < 5') -or
  $text.IndexOf('observer.Bind(slot, damageMember') -lt $text.IndexOf('m_aVehicleSoakMembers.Insert')){'LIFECYCLE_ROSTER_BOUNDARY'}
}
$violations=@(Rules $source)
if($violations.Count){throw "Production observer failed: $violations"}
$mutants=@(
 @('actor ID','m_Actor.GetID() == m_ActorId','true'),
 @('actor Rpl','Replication.FindItemId(m_Actor).ToString() == m_sActorRpl','true'),
 @('group identity','m_Slot.GetGroup() == m_Group','true'),
 @('generation','m_Slot.GetSpawnGeneration() == m_iGeneration','true'),
 @('raw position','m_Slot.AICF_VehicleSoakRawRosterMember(m_iPosition) == m_Actor','true'),
 @('alive-only gap','m_Slot.AICF_VehicleSoakRawRosterMember(m_iPosition)','m_Slot.GetRosterMember(m_iPosition)'),
 @('world lookup','GetGame().GetWorld().FindEntityByID(m_ActorId) != m_Actor','false'),
 @('foreign parent','agent.GetParentGroup() != m_Group','false'),
 @('native hit entity','context.hitEntity != m_Actor','false'),
 @('remove','m_Damage.GetOnDamage().Remove(OnDamage)','Print("lost_remove")'),
 @('health mutation','Print(details);','m_Actor.GetDamageManager().SetHealthScaled(1); Print(details);')
)
foreach($mutant in $mutants){if(@(Rules ($source.Replace($mutant[1],$mutant[2]))).Count -eq 0){throw "Escaped mutant: $($mutant[0])"}}
$body=Body $source 'MatchesIdentity'
$type='AICFSoakDamageIdentity'+[guid]::NewGuid().ToString('N')
Add-Type "public static class $type { public static bool Check(bool exactActor, bool exactGroup, bool exactSlot, bool exactRpl, bool exactGeneration) {$body} }"
$method=([type]$type).GetMethod('Check')
for($mask=0;$mask -lt 32;$mask++){
 $args=@();for($bit=0;$bit -lt 5;$bit++){$args+= [bool]($mask -band (1 -shl $bit))}
 if($method.Invoke($null,$args) -ne ($mask -eq 31)){throw "Identity negative control escaped mask=$mask"}
}
# Actual IsCurrent и fixture-only raw getter исполняются с native read adapters.
# ALIVE-only mutant обязан разорвать ту же INCAP->ALIVE последовательность.
$currentBody=(Body $source 'IsCurrent').Replace('AIAgent agent;','AIAgent agent = null;')
$rawBody=Body $source 'AICF_VehicleSoakRawRosterMember'
$oldAliveBody=$currentBody.Replace('m_Slot.AICF_VehicleSoakRawRosterMember(m_iPosition)','m_Slot.GetRosterMember(m_iPosition)')
$actualCode=@'
using System;
using System.Collections.Generic;
public class N {public static implicit operator bool(N v){return !Object.ReferenceEquals(v,null);}}
public class NativeArray<T>:List<T>{public new int Count(){return base.Count;}}
public class World:N {public Dictionary<int,IEntity> Entities=new Dictionary<int,IEntity>();public IEntity FindEntityByID(int id){IEntity result;Entities.TryGetValue(id,out result);return result;}}
public class Game:N {public World World=new World();public World GetWorld(){return World;}}
public static class Replication {public static bool Server=true;public static bool IsServer(){return Server;}public static int FindItemId(IEntity e){return e.Rpl;}}
public class IEntity:N {public int Id,Rpl;public World World;public int GetID(){return Id;}public World GetWorld(){return World;}}
public class SCR_AIGroup:IEntity {}
public class Damage:N {}
public class AIControlComponent:N {public AIAgent Agent;public AIAgent GetControlAIAgent(){return Agent;}}
public class AIAgent:N {public Actor Actor;public SCR_AIGroup Parent;public Actor GetControlledEntity(){return Actor;}public SCR_AIGroup GetParentGroup(){return Parent;}}
public class Actor:IEntity {public bool Alive=true;public Damage Damage=new Damage();public AIControlComponent Control=new AIControlComponent();public Damage GetDamageManager(){return Damage;}public AIControlComponent GetAIControlComponent(){return Control;}}
public class AICF_GroupSlot:N {public SCR_AIGroup Group;public int Generation=1;public NativeArray<Actor> m_aRosterMembers=new NativeArray<Actor>();public SCR_AIGroup GetGroup(){return Group;}public int GetSpawnGeneration(){return Generation;}public Actor AICF_VehicleSoakRawRosterMember(int position) __RAW__ public Actor GetRosterMember(int position){Actor a=AICF_VehicleSoakRawRosterMember(position);return a!=null&&a.Alive&&a.Control.Agent!=null&&a.Control.Agent.Parent==Group?a:null;}}
public class Observer:N {public static Game Game;public Game GetGame(){return Game;}public bool m_bBound=true;public AICF_GroupSlot m_Slot;public Actor m_Actor;public SCR_AIGroup m_Group;public Damage m_Damage;public int m_ActorId=1,m_GroupId=2,m_iPosition=0,m_iGeneration=1;public string m_sActorRpl="10",m_sGroupRpl="20";public static bool MatchesIdentity(bool exactActor,bool exactGroup,bool exactSlot,bool exactRpl,bool exactGeneration) __IDENTITY__ public bool IsCurrent() __CURRENT__ public bool AliveOnlyMutant() __OLD__ }
public static class Run {public static Observer Fresh(){Observer.Game=new Game();Replication.Server=true;var world=Observer.Game.World;var group=new SCR_AIGroup{Id=2,Rpl=20,World=world};var actor=new Actor{Id=1,Rpl=10,World=world};actor.Control.Agent=new AIAgent{Actor=actor,Parent=group};var slot=new AICF_GroupSlot{Group=group};slot.m_aRosterMembers.Add(actor);world.Entities.Add(1,actor);world.Entities.Add(2,group);return new Observer{m_Slot=slot,m_Actor=actor,m_Group=group,m_Damage=actor.Damage};}
 public static int Execute(){int count=0;Action<bool,string> check=(ok,label)=>{if(!ok)throw new Exception(label);count++;};var o=Fresh();check(o.IsCurrent(),"initial captured ALIVE");o.m_Actor.Alive=false;check(o.IsCurrent(),"captured INCAP keeps subscription");check(!o.AliveOnlyMutant(),"old alive-only getter loses INCAP");o.m_Actor.Control.Agent.Parent=null;check(o.IsCurrent(),"captured INCAP without native parent");o.m_Actor.Control.Agent.Parent=o.m_Group;o.m_Actor.Alive=true;check(o.IsCurrent(),"same captured native wake");o.m_Actor.Alive=false;check(o.IsCurrent(),"same captured terminal DEAD");o.m_Actor.Control.Agent=null;check(o.IsCurrent(),"exact DEAD without agent");
 Action<Action<Observer>,string> reject=(change,label)=>{var x=Fresh();change(x);check(!x.IsCurrent(),label);};
 reject(x=>x.m_Actor.Id++,"actor ID changed");reject(x=>x.m_Actor.Rpl++,"actor Rpl changed");reject(x=>x.m_Actor.Damage=new Damage(),"damage component changed");reject(x=>x.m_Group.Id++,"group ID changed");reject(x=>x.m_Group.Rpl++,"group Rpl changed");reject(x=>x.m_Slot.Generation++,"generation changed");reject(x=>x.m_Slot.Group=new SCR_AIGroup(),"slot group replaced");reject(x=>x.m_Slot.m_aRosterMembers[0]=new Actor(),"raw position replaced");reject(x=>x.m_Actor.Control.Agent.Parent=new SCR_AIGroup(),"adopted foreign parent");reject(x=>x.m_Actor.Control.Agent.Actor=new Actor(),"agent controls another actor");reject(x=>x.m_Actor.World=new World(),"foreign actor world");reject(x=>x.m_Group.World=new World(),"foreign group world");reject(x=>Observer.Game.World.Entities[1]=new Actor(),"world ID reused");reject(x=>Observer.Game.World.Entities.Remove(2),"group absent");reject(x=>Replication.Server=false,"client rejected");reject(x=>x.m_iPosition=-1,"negative raw position");reject(x=>x.m_iPosition=1,"out-of-range raw position");reject(x=>x.m_bBound=false,"stopped observer");reject(x=>{Observer.Game.World=null;x.m_Actor.World=null;x.m_Group.World=null;},"teardown null world");return count;}}
'@
$actualCode=$actualCode.Replace('__RAW__',$rawBody).Replace('__IDENTITY__',$body).Replace('__CURRENT__',$currentBody).Replace('__OLD__',$oldAliveBody)
$actualNamespace='AICFSoakActual'+[guid]::NewGuid().ToString('N')
Add-Type ("namespace $actualNamespace {"+$actualCode.Replace('using System;','').Replace('using System.Collections.Generic;','').Replace('Dictionary<','System.Collections.Generic.Dictionary<').Replace(':List<T>',':System.Collections.Generic.List<T>').Replace('Action<','System.Action<').Replace('new Exception(','new System.Exception(').Replace('Object.ReferenceEquals','System.Object.ReferenceEquals')+'}')
$actualType=[type]("$actualNamespace.Run")
$actualCases=$actualType.GetMethod('Execute').Invoke($null,@())
$apiRoot=Join-Path $RepositoryRoot '.cache/reforger-api/Arma-Reforger-Script-Diff-1.8.0.13/scripts'
if(Test-Path $apiRoot){
 $damage=[IO.File]::ReadAllText((Join-Path $apiRoot 'GameCode/Components/SCR_DamageManagerComponent.c'))
 $context=[IO.File]::ReadAllText((Join-Path $apiRoot 'Game/generated/Damage/BaseDamageContext.c'))
 $instigator=[IO.File]::ReadAllText((Join-Path $apiRoot 'Game/generated/Damage/Instigator.c'))
 if($damage -notmatch 'invoker.Invoke\(damageContext\)' -or $context -notmatch 'IEntity hitEntity' -or
  $instigator -notmatch 'IEntity GetInstigatorEntity\(\)'){throw 'Pinned native callback/API contract changed'}
}
Write-Output "PASS VehicleSoak damage observer: read-only captured entity/Rpl/world/generation through INCAP/native wake/DEAD; lifecycle Remove; $($mutants.Count) source mutants +32 identity +$actualCases actual native-adapter controls; pinned API1.8.0.13"
