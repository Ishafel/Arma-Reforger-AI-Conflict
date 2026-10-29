param([string]$RepositoryRoot=(Split-Path -Parent (Split-Path -Parent $PSScriptRoot)))
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot '../lib/Stage3StaticAudit.Common.ps1')
$folder=Join-Path $RepositoryRoot 'AIConflictCore/Scripts/Game/AIConflict'
$helper=Get-Content (Join-Path $folder 'Forces/AICF_RosterSpawnBatch.c') -Raw
$spawner=Get-Content (Join-Path $folder 'Forces/AICF_GroupSpawner.c') -Raw
$controller=Get-Content (Join-Path $folder 'Bootstrap/AICF_MatchController.c') -Raw
function Body($source,$name){Get-AICFMethodBody ([pscustomobject]@{Source=$source;Code=ConvertTo-AICFCodeText $source}) $name}
$owner=Body $controller 'IsCurrentRosterSpawnOwner'
$bound=Body $spawner 'BeginBoundRosterSpawn'
$begin=Body $spawner 'BeginRosterSpawn'
$actual=$helper.Replace('class AICF_RosterSpawnBatch','public class AICF_RosterSpawnBatch : NativeObject').Replace('protected ref array<ResourceName> m_aPrefabs = {};','protected Bag<string> m_aPrefabs = new Bag<string>();').Replace('EntityID','int').Replace('void AICF_RosterSpawnBatch(','public AICF_RosterSpawnBatch(').Replace('for (int index;','for (int index=0;').Replace('int created;','int created=0;').Replace('System.GetTickCount','Clock.GetTickCount').Replace('m_aPrefabs[index].IsEmpty()','string.IsNullOrEmpty(m_aPrefabs[index])')
$actual=[regex]::Replace($actual,'(?m)^\t(bool|static bool) ',' public $1 ')
$harness=@'
using System;
using System.Collections.Generic;
public static class Clock {public static int GetTickCount(int start=0){return 10;}}
public class NativeObject {public static implicit operator bool(NativeObject o){return o!=null;}public static Game GetGame(){return Game.Current;}}
public class Bag<T>:NativeObject {public List<T> Items=new List<T>();public int Count(){return Items.Count;}public T this[int i]{get{return Items[i];}set{Items[i]=value;}}public void Copy(Bag<T> b){Items=new List<T>(b.Items);}}
public static class Replication {public static bool Server=true;public static bool IsServer(){return Server;}}
public class Game {public static Game Current;public ChimeraAIWorld World;public ChimeraAIWorld GetAIWorld(){return World;}}
public class Faction:NativeObject {public string Key="USSR";public string GetFactionKey(){return Key;}}
public class ChimeraAIWorld:NativeObject {
 public int Active=223,Intended=223,Limit=228,FactionCurrent=0,FactionLimit=-1,Purges;public bool CanAI=true,CanGroup=true,CanFaction=true;
 public static ChimeraAIWorld Cast(ChimeraAIWorld w){return w;}
 public bool CanAIBeActivated(){return CanAI;} public bool CanActivateGroup(SCR_AIGroup g){return CanGroup;}
 public bool CanLimitedAIBeAddedForFaction(string k){return CanFaction;}public int GetCurrentNumOfActiveAIs(){return Active;}
 public int GetActiveGroupIntendedCount(){return Intended;}public int GetLimitOfActiveAIs(){return Limit;}
 public int GetCurrentAmountOfLimitedAIsForFaction(string k){return FactionCurrent;}public int GetAILimitForFaction(string k){return FactionLimit;}
 public void PurgeSpawnRequestsForGroup(SCR_AIGroup g){Purges++;g.Purged++;}
}
public class SCR_AIGroup:NativeObject {
 public int Id=10,Count,Expands,Requests,Requested,Added,Completed,Purged,Members;public bool Authority=true,EmptyShape=true,ExpansionResult=true,Adds=true;public string Rpl="rpl10";public Faction Faction=new Faction();
 public Bag<string> m_aUnitPrefabSlots=new Bag<string>();public Action<SCR_AIGroup> Callback;
 public int GetID(){return Id;}public Faction GetFaction(){return Faction;}public int GetAgentsCount(){return Count;}
 public void AICF_RecordRosterBatchOwner(bool queue){}public bool AICF_HasEmptyRosterAttachments(){return EmptyShape;}public void SetNumberOfMembersToSpawn(int n){Members=n;}
 public bool ExpandOneMember(){Expands++;if(Adds){Count++;Added++;GetGame().World.Intended++;if(Count==m_aUnitPrefabSlots.Count())Completed++;}if(Callback!=null)Callback(this);return ExpansionResult;}
 public void RequestSpawn(int n){Requests++;Requested=n;}
}
public enum AICF_EGroupSlotState {WAITING,SPAWNING,READY}
public class AICF_GroupSlot:NativeObject {
 public int Id,Generation=2,Started=100,Expected=5;public bool Requested=true;public SCR_AIGroup Group;public AICF_EGroupSlotState State=AICF_EGroupSlotState.SPAWNING;
 public int GetSlotId(){return Id;}public int GetSpawnGeneration(){return Generation;}public int GetSpawnStartedAtMs(){return Started;}
 public bool IsRosterSpawnRequested(){return Requested;}public int GetRosterExpectedCount(){return Expected;}public int GetDeploymentSize(){return Expected;}
 public SCR_AIGroup GetGroup(){return Group;}public AICF_EGroupSlotState GetState(){return State;}
}
public class AICF_FactionState:NativeObject {public AICF_GroupSlot[] Slots;public AICF_GroupSlot GetSlot(int i){return i>=0&&i<Slots.Length?Slots[i]:null;}}
public class AICF_MatchController:NativeObject {
 public static AICF_MatchController s_ActiveController;public bool m_bStopped,m_bStarted=true;public AICF_FactionState m_USState,m_USSRState;
 public bool IsCurrentRosterSpawnOwner(AICF_FactionState state,AICF_GroupSlot slot,SCR_AIGroup group,int generation) __OWNER__
}
public static class AICF_VehicleBehaviorBinding {public static string ComponentRpl(SCR_AIGroup g){return g.Rpl;}}
public static class AICF_VehicleBoardingMutationFence {public static bool IsAuthoritativeReplicatedEntity(SCR_AIGroup g){return g.Authority;}}
public static class AICF_Stage1Diagnostics {public static void Info(string k,string v){}}
__ACTUAL__
public class AICF_GroupSpawner:NativeObject {
 protected AICF_RosterSpawnBatch m_RosterBatch;
 public bool BeginBoundRosterSpawn(AICF_MatchController controller,AICF_FactionState state,AICF_GroupSlot slot,SCR_AIGroup group,int expectedSize) __BOUND__
 public virtual bool BeginRosterSpawn(SCR_AIGroup group,int expectedSize) __BEGIN__
 public AICF_RosterSpawnBatch Current(){return m_RosterBatch;}
}
public static class BatchChecks {
 static int checks;
 static void Check(bool b,string reason){checks++;if(!b)throw new Exception(reason);}
 public class Case {
  public AICF_MatchController Owner=new AICF_MatchController();public AICF_FactionState State=new AICF_FactionState();public AICF_GroupSlot Slot=new AICF_GroupSlot();public SCR_AIGroup Group=new SCR_AIGroup();public ChimeraAIWorld World=new ChimeraAIWorld();public AICF_GroupSpawner Spawner=new AICF_GroupSpawner();
  public Case(int count=5){Slot.Expected=count;Slot.Group=Group;State.Slots=new[]{Slot};Owner.m_USSRState=State;AICF_MatchController.s_ActiveController=Owner;Game.Current=new Game{World=World};Replication.Server=true;for(int i=0;i<count;i++)Group.m_aUnitPrefabSlots.Items.Add("unit"+i);}
  public AICF_RosterSpawnBatch Context(){return new AICF_RosterSpawnBatch(Owner,State,Slot,Group,Slot.Expected);}
 }
 public static int Run(){
  foreach(int n in new[]{1,5,10}){var c=new Case(n);c.World.Limit=223+n;Check(c.Spawner.BeginBoundRosterSpawn(c.Owner,c.State,c.Slot,c.Group,n),"Accept batch");Check(c.Group.Count==n&&c.Group.Expands==n&&c.Group.Added==n&&c.Group.Completed==1&&c.Group.Requests==0&&c.Group.Purged==1,"Native exact batch+callbacks");Check(c.Spawner.Current()==null,"Outer context cleared");}
  int[] v={-1,0,1,4,5,223,228,int.MaxValue};
  foreach(int a in v)foreach(int i in v)foreach(int h in v)foreach(int f in new[]{-2,-1,0,4,5,int.MaxValue})foreach(int n in new[]{-1,0,1,5,10}){
   bool wanted=a>=0&&i>=0&&h>0&&n>0&&(long)h-Math.Max(a,i)>=n&&f>=-1&&(f==-1||(long)f-3>=n);
   Check(AICF_RosterSpawnBatch.FitsRemaining(a,i,h,3,f,n)==wanted,"Global/faction/overflow remaining boundary");
  }
  Action<Case>[] stale={c=>c.Owner.m_bStopped=true,c=>AICF_MatchController.s_ActiveController=new AICF_MatchController(),c=>c.Slot.Generation++,c=>c.Slot.Started++,c=>c.Slot.Requested=false,c=>c.Slot.Expected=4,c=>c.Slot.State=AICF_EGroupSlotState.READY,c=>c.Slot.Group=new SCR_AIGroup(),c=>c.State.Slots[0]=new AICF_GroupSlot(),c=>c.Group.Id++,c=>c.Group.Rpl="other",c=>c.Group.Faction=new Faction(),c=>c.Group.Authority=false,c=>c.Group.m_aUnitPrefabSlots[0]="wrong",c=>Replication.Server=false};
  foreach(var mutation in stale){var c=new Case();var context=c.Context();mutation(c);Check(!context.IsCurrent(c.Group,5),"Unknown/stale owner admitted");Check(!context.TryDispatch(c.Group,5)&&c.Group.Expands==0&&c.Group.Purged==0,"Stale context mutated");}
  Action<Case>[] transient={c=>c.World.CanAI=false,c=>c.World.CanGroup=false,c=>c.World.CanFaction=false,c=>c.World.Intended=224,c=>c.World.Limit=227,c=>c.World.FactionLimit=4,c=>c.Group.EmptyShape=false};
  foreach(var mutation in transient){var c=new Case();mutation(c);Check(c.Spawner.BeginBoundRosterSpawn(c.Owner,c.State,c.Slot,c.Group,5),"Queue fallback rejected");Check(c.Group.Purged==0&&c.Group.Expands==0&&c.Group.Requests==1&&c.Group.Requested==5,"Preflight fallback must keep stock side effects");}
  foreach(var mutation in stale){var c=new Case();c.Group.Callback=g=>mutation(c);Check(c.Spawner.BeginBoundRosterSpawn(c.Owner,c.State,c.Slot,c.Group,5),"Changed callback scope must return accepted");Check(c.Group.Expands==1&&c.Group.Requests==0,"Callback mutated successor or queued stale remainder");}
  {var c=new Case();c.Group.Adds=false;Check(c.Spawner.BeginBoundRosterSpawn(c.Owner,c.State,c.Slot,c.Group,5),"Transient no-agent success");Check(c.Group.Expands==1&&c.Group.Requests==1&&c.Group.Requested==5,"No increment repeated same prefab index");}
  {var c=new Case();c.Group.ExpansionResult=false;Check(c.Spawner.BeginBoundRosterSpawn(c.Owner,c.State,c.Slot,c.Group,5),"False with actual addition");Check(c.Group.Expands==1&&c.Group.Requested==4,"Fallback must use actual remaining count");}
  {var c=new Case();c.Group.Callback=g=>c.World.CanFaction=false;c.Spawner.BeginBoundRosterSpawn(c.Owner,c.State,c.Slot,c.Group,5);Check(c.Group.Expands==1&&c.Group.Requested==4,"Fresh faction veto per member");}
  {var c=new Case();c.Group.Callback=g=>c.World.Intended=228;c.Spawner.BeginBoundRosterSpawn(c.Owner,c.State,c.Slot,c.Group,5);Check(c.Group.Expands==1&&c.Group.Requested==4,"Fresh global veto per member");}
  {var c=new Case();c.World.Limit=250;var inner=new AICF_GroupSlot{Id=1,Group=new SCR_AIGroup{Id=20,Rpl="rpl20"}};for(int k=0;k<5;k++)inner.Group.m_aUnitPrefabSlots.Items.Add("unit"+k);c.State.Slots=new[]{c.Slot,inner};bool nested=false;c.Group.Callback=g=>{if(nested)return;nested=true;var prior=c.Spawner.Current();c.Spawner.BeginBoundRosterSpawn(c.Owner,c.State,inner,inner.Group,5);Check(c.Spawner.Current()==prior,"Nested context must restore captured outer");};c.Spawner.BeginBoundRosterSpawn(c.Owner,c.State,c.Slot,c.Group,5);Check(c.Group.Count==5&&inner.Group.Count==5&&c.Spawner.Current()==null,"Reentrant nested begin lost ownership");}
  {var c=new Case();c.Group.Callback=g=>{var donor=new SCR_AIGroup();donor.m_aUnitPrefabSlots.Items.Add("one");c.Spawner.BeginRosterSpawn(donor,1);Check(donor.Requests==1&&donor.Expands==0,"Unbound callback owner must retain queue");};c.Spawner.BeginBoundRosterSpawn(c.Owner,c.State,c.Slot,c.Group,5);Check(c.Group.Count==5,"Unbound nested queue changed outer");}
  {var c=new Case();c.Group.Callback=g=>c.Group.EmptyShape=false;c.Spawner.BeginBoundRosterSpawn(c.Owner,c.State,c.Slot,c.Group,5);Check(c.Group.Expands==1&&c.Group.Requests==0,"Changed authored shape must not initialize foreign assets");}
  return checks;
 }
}
'@
$code=$harness.Replace('__OWNER__',$owner).Replace('__ACTUAL__',$actual).Replace('__BOUND__',$bound).Replace('__BEGIN__',$begin)
Add-Type -TypeDefinition $code
$count=[BatchChecks]::Run()
if($helper-match'\b(SetLimitOfActiveAIs|SetImportance|SetDormantCounts|MarkReady|SetHealth|CallLater)\('){throw 'Forbidden ownership transfer'}
foreach($needle in @('context.TryDispatch(group, expectedSize)','m_RosterBatch = previous;','if (context && !context.IsCurrent(group, expectedSize)) return true;')){if(!$spawner.Contains($needle)){throw "Missing scope/reentry guard: $needle"}}
foreach($needle in @('ROSTER_SPAWN_SCOPE_CANCELLED','!IsCurrentRosterSpawnOwner(factionState, slot, group, replacementGeneration)','!IsCurrentRosterSpawnOwner(factionState, slot, group, initialGeneration)')){if(!$controller.Contains($needle)){throw "Missing outer callback guard: $needle"}}
Write-Output "PASS RosterSpawnBatch actual helper+owner+bound/default dispatch checks=$count; native runtime separate"

$mutations=@(
 @('group.GetID() != m_Id','false'),
 @('AICF_VehicleBehaviorBinding.ComponentRpl(group) != m_sRpl','false'),
 @('m_Slot.GetSpawnStartedAtMs() != m_iStartedAt','false'),
 @('m_Slot.IsRosterSpawnRequested()','true'),
 @('slot.GetSpawnGeneration() != generation','false'),
 @('s_ActiveController != this','false'),
 @('!world.CanActivateGroup(m_Group)','false'),
 @('!world.CanLimitedAIBeAddedForFaction(m_Faction.GetFactionKey())','false'),
 @('!CanDispatch(world, expected - before)','false'),
 @('!expanded || group.GetAgentsCount() != before + 1','!expanded'),
 @('m_RosterBatch = previous;','m_RosterBatch = null;'),
 @('if (context && !context.IsForGroup(group)) context = null;','')
)
$mutationIndex=0
foreach($mutation in $mutations){
 if(!$code.Contains($mutation[0])){throw "Mutation did not alter source: $($mutation[0])"}
 $changed=$code.Replace($mutation[0],$mutation[1]);$ns='AICFBatchMutation'+$mutationIndex
 Add-Type -TypeDefinition ("namespace $ns {"+$changed+'}')
 $type=(($ns+'.BatchChecks') -as [type]);$caught=$false
 try{[void]$type.GetMethod('Run').Invoke($null,@())}catch{$caught=$true}
 if(!$caught){throw "Escaped actual mutation: $($mutation[0])"}
 $mutationIndex++
}
"PASS actual helper/owner/dispatch mutants=$mutationIndex"
$boundary=Get-Content (Join-Path $RepositoryRoot 'tests/fixtures/AICF_RosterSpawnBatchBoundaryProbe.c') -Raw
if($boundary-match'\b(ExpandOneMember|RequestSpawn|MarkReady|SetHealth|SetOrigin|SetLimitOfActiveAIs|SetImportance)\('){throw 'Boundary fixture bypasses real production dispatch/readiness'}
foreach($needle in @('return super.SpawnInitialRoster(factionState, faction);','HasExactFactionRoster','AICF_EGroupSlotState.READY','m_aAICFBatchBoundaryActors','m_aAICFBatchBoundaryRpl','Remove(AICF_BatchBoundaryTick)')){if(!$boundary.Contains($needle)){throw "Boundary fixture missing strict proof: $needle"}}
"PASS boundary fixture real bound caller/readonly roster/identity/Stop contract"
