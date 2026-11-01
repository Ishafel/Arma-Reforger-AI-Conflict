param([string]$RepositoryRoot=(Split-Path -Parent (Split-Path -Parent $PSScriptRoot)))
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot '../lib/Stage3StaticAudit.Common.ps1')
$fixture=[IO.File]::ReadAllText((Join-Path $RepositoryRoot 'tests/fixtures/AICF_VehicleCrewReturnProbe.c'))
function Body([string]$text,[string]$method){Get-AICFMethodBody ([pscustomobject]@{Source=$text;Code=(ConvertTo-AICFCodeText $text)}) $method}
$spawn=Body $fixture 'SpawnInitialRoster'
$update=Body $fixture 'Update'
$close=Body $fixture 'AICF_CrewClose'
$process=Body $fixture 'ProcessFaction'
$reduced=Body $fixture 'AICF_CrewPrepareReducedOrder'
$checks=0
function Require([bool]$condition,[string]$label){if(!$condition){throw "FAIL $label"};$script:checks++}
Require ($fixture -notmatch 'm_CrewState|m_CrewVehicles|new AICF_FactionState|new AICF_VehicleCoordinator|ResolveVehiclePresetContext') 'NO_ALTERNATE_OWNER_OR_CONTEXT_REDIRECT'
Require ($fixture -notmatch '\.MarkReady\(|\.BeginInitialSpawn\(|\.BindSpawnedGroup\(|\.BeginRosterSpawn\(|\.RecordDeploymentMembers\(') 'NO_MANUAL_READINESS_OR_GENERATION'
Require ($update -match 'm_USSRState.GetSlot\(0\)' -and $update -match 'm_VehicleCoordinator.GetTrip\(slot\)' -and $update -match 'slot.GetGroup\(\) != m_CrewGroup') 'CANONICAL_SLOT_TRIP_AND_SUCCESSOR_FENCE'
Require ($update -notmatch '\.Update\(null|StopWithFactionContexts' -and $close -notmatch 'DeleteRplEntity|DespawnMembers|ClearOrder|\.Stop') 'NO_DUPLICATE_TICK_OR_FOREIGN_CLEANUP'
Require ($update -match 'AssignPlayerPointOrder\(slot, m_USSRFaction, position, false\)' -and $update -match 'bool resizeAllowed = CanResizeVehicleCrew\(slot\)' -and $reduced -match 'm_CrewKept.GetOrigin\(\) \+ "400 0 0"') 'REAL_PLAYER_HOLD_THEN_EXISTING_400M_ORDER'
Require ($update -match 'slot.GetRosterMember\(9\)' -and $reduced -match 'slot.GetRosterMember\(0\) == m_CrewKept' -and $update -match 'seat.GetOccupant\(\) == m_CrewKept') 'ORIGINAL_NONPREFIX_DRIVER_PHYSICAL_ORACLE'
Require ($reduced -match 'CountAgents\(\) == 9' -and $update -match 'AICF_CrewReturnCount\(\) == 0' -and $fixture -match 'before == after') 'RETURN_AND_INVENTORY_ORACLES_RETAINED'
$match=[IO.File]::ReadAllText((Join-Path $RepositoryRoot 'AIConflictCore/Scripts/Game/AIConflict/Bootstrap/AICF_MatchController.c'))
$originalSpawn=Body $match 'SpawnInitialRoster';$bind=Body $match 'BindManagedGroup'
Require ($originalSpawn -match 'slot.BeginInitialSpawn\(\)' -and $originalSpawn -match 'BindManagedGroup\(factionState, faction, slot, group' -and $bind -match 'RegisterSlot\(factionState, slot\)' -and $bind -match 'BindPlannedGroup\(quotaOwner\)' -and $bind -match 'BeginBoundRosterSpawn\(this, factionState, slot, group') 'DELEGATED_ACTUAL_CANONICAL_LIFECYCLE'
$slot=[IO.File]::ReadAllText((Join-Path $RepositoryRoot 'AIConflictCore/Scripts/Game/AIConflict/State/AICF_GroupSlot.c'))
Require ((Body $slot 'BeginInitialSpawn') -match 'm_iSpawnGeneration\+\+' -and (Body $slot 'CommitVehicleCrew') -match 'm_iSpawnGeneration\+\+') 'INITIAL1_THEN_CREW2_SOURCE_PROOF'
$bootstrapUpdate=Body $match 'Update'
Require ($bootstrapUpdate.IndexOf('ProcessFaction(m_USSRState, m_USSRFaction)') -lt $bootstrapUpdate.IndexOf('m_VehicleCoordinator.Update(') -and $process.IndexOf('super.ProcessFaction(') -lt $process.IndexOf('SetUnitType(')) 'READY_PROCESS_BEFORE_FIRST_COORDINATOR_TICK'
$deployment=(Body $slot 'GetDeploymentSize').Replace('m_UnitType','Unit').Replace('m_iDesiredSize','Size')
function Translate([string]$body){
 $body=$body.Replace('System.GetCLIParam("aicfVehicleCrewReturnProbe", flag)','Runtime.Cli("aicfVehicleCrewReturnProbe", out flag)')
 $body=$body.Replace('factionState &&','factionState != null &&').Replace('faction &&','faction != null &&')
 $body=$body.Replace('!slot ||','slot == null ||').Replace('|| slot.GetGroup())', '|| slot.GetGroup() != null)')
 $body=$body.Replace('!slot)', 'slot == null)')
 $body=$body.Replace('!factionState ||','factionState == null ||').Replace('!faction ||','faction == null ||').Replace('!group ||','group == null ||')
 $body=$body.Replace('!slot.GetRosterMember(9)','slot.GetRosterMember(9) == null').Replace('Print(', 'Runtime.Print(').Replace('slot &&','slot != null &&')
 return $body.Replace('super.SpawnInitialRoster(factionState, faction)','BaseSpawn(factionState, faction)').Replace('super.ProcessFaction(factionState, faction)','BaseProcess(factionState, faction)')
}
$variants=@(
 @{Name='Canonical';Body=$spawn},
 @{Name='UnsafeForeignState';Body=$spawn.Replace('factionState == m_USSRState &&','')},
 @{Name='UnsafeOccupied';Body=$spawn.Replace('if (!slot || slot.GetState() != AICF_EGroupSlotState.EMPTY || slot.GetGroup()) return false;','if (!slot) return false;')},
 @{Name='UnsafeInfantrySpawn';Body=$spawn.Replace('slot.SetUnitType(AICF_EGroupUnitType.MOTORIZED_LIGHT);','slot.SetUnitType(AICF_EGroupUnitType.INFANTRY);')}
)
foreach($variant in $variants | Select-Object -Skip 1){Require ($variant.Body -cne $spawn) ('UNSAFE_TARGET_'+$variant.Name)}
$code=@'
using System;
public enum AICF_EGroupSlotState {EMPTY,SPAWNING,READY}
public enum AICF_EGroupUnitType {INFANTRY,MOTORIZED_LIGHT}
public static class Replication {public static bool Server=true; public static bool IsServer(){return Server;}}
public static class Runtime {public static bool Flag=true;public static bool Cli(string key,out string value){value="1";return Flag;}public static void Print(string value){}}
public static class AICF_Stage1Config {public const int MIN_GROUP_SIZE=1;}
public class SCR_CampaignFaction {}
public class IEntity {public string GetOrigin(){return "origin";}}
public class SCR_AIGroup {public int Count;public Action OnCount;public int GetAgentsCount(){int old=Count;if(OnCount!=null)OnCount();return old;}}
public class AICF_GroupSlot {
 public AICF_EGroupSlotState State=AICF_EGroupSlotState.EMPTY;
 public AICF_EGroupUnitType Unit=AICF_EGroupUnitType.MOTORIZED_LIGHT;
 public int Size=1,Generation=0;public SCR_AIGroup Group;public object Roster9;public bool Replacement;
 public AICF_EGroupSlotState GetState(){return State;}public SCR_AIGroup GetGroup(){return Group;}
 public bool IsCombatReady(){return State==AICF_EGroupSlotState.READY;}public bool IsReplacementDeployment(){return Replacement;}
 public int GetSpawnGeneration(){return Generation;}public int GetDesiredSize(){return Size;}public AICF_EGroupUnitType GetUnitType(){return Unit;}
 public object GetRosterMember(int index){return Roster9;}
 public void SetDesiredSize(int size){Size=size;}public void SetUnitType(AICF_EGroupUnitType type){Unit=type;}
}
public class AICF_FactionState {public AICF_GroupSlot Slot=new AICF_GroupSlot();public AICF_GroupSlot GetSlot(int id){return Slot;}}
public abstract class FixtureBase {
 public AICF_FactionState m_USSRState=new AICF_FactionState();public SCR_CampaignFaction m_USSRFaction=new SCR_CampaignFaction();
 public int Calls,ProcessCalls,ReducedCalls;public bool Result=true,m_bCrewDone,m_bStopped,m_bCrewSpawnProfileRestored;public Action<AICF_FactionState> Callback,ProcessCallback;
 public bool BaseSpawn(AICF_FactionState state,SCR_CampaignFaction faction){Calls++;if(Callback!=null)Callback(state);return Result;}
 public void BaseProcess(AICF_FactionState state,SCR_CampaignFaction faction){ProcessCalls++;if(ProcessCallback!=null)ProcessCallback(state);}
 public void AICF_CrewPrepareReducedOrder(AICF_GroupSlot slot){ReducedCalls++;}
 public abstract bool Spawn(AICF_FactionState state,SCR_CampaignFaction faction);
 public abstract void Process(AICF_FactionState state,SCR_CampaignFaction faction);
}
'@
$code=$code.Replace('public object GetRosterMember(int index){return Roster9;}',('public object GetRosterMember(int index){return Roster9;} public int GetDeploymentSize() '+$deployment))
foreach($variant in $variants){$code+="`npublic class $($variant.Name) : FixtureBase {public override bool Spawn(AICF_FactionState factionState,SCR_CampaignFaction faction) "+(Translate $variant.Body)+" public override void Process(AICF_FactionState factionState,SCR_CampaignFaction faction) "+(Translate $process)+"}`n"}
$code+=@'
public static class CrewCanonicalReplay {
 static int count;static void Check(bool value,string label){if(!value)throw new Exception("FAIL "+label);count++;}
 public static string Run(){
  count=0;Replication.Server=true;Runtime.Flag=true;
  var c=new Canonical();var slot=c.m_USSRState.Slot;int beforeCalls=0;
  c.Callback=s=>{beforeCalls++;Check(s.Slot.Size==10 && s.Slot.Unit==AICF_EGroupUnitType.MOTORIZED_LIGHT && s.Slot.GetDeploymentSize()==10,"configured_before_super");s.Slot.Generation++;};
  Check(c.Spawn(c.m_USSRState,c.m_USSRFaction),"success");Check(c.Calls==1 && beforeCalls==1 && slot.Generation==1 && slot.State==AICF_EGroupSlotState.EMPTY,"single_super_no_ready");
  c=new Canonical();c.Result=false;Check(!c.Spawn(c.m_USSRState,c.m_USSRFaction) && c.Calls==1,"original_refusal_preserved");
  c=new Canonical();c.Callback=s=>{throw new InvalidOperationException("original");};bool threw=false;try{c.Spawn(c.m_USSRState,c.m_USSRFaction);}catch(InvalidOperationException e){threw=e.Message=="original";}Check(threw && c.Calls==1,"original_exception_preserved");
  c=new Canonical();Runtime.Flag=false;Check(c.Spawn(c.m_USSRState,c.m_USSRFaction) && c.Calls==1 && c.m_USSRState.Slot.Size==1,"disabled_transparent");Runtime.Flag=true;
  c=new Canonical();Replication.Server=false;Check(c.Spawn(c.m_USSRState,c.m_USSRFaction) && c.Calls==1 && c.m_USSRState.Slot.Size==1,"client_transparent");Replication.Server=true;
  c=new Canonical();var foreign=new AICF_FactionState();Check(c.Spawn(foreign,c.m_USSRFaction) && c.Calls==1 && foreign.Slot.Size==1,"foreign_state_transparent");
  c=new Canonical();Check(c.Spawn(c.m_USSRState,new SCR_CampaignFaction()) && c.Calls==1 && c.m_USSRState.Slot.Size==1,"foreign_faction_transparent");
  c=new Canonical();Check(c.Spawn(null,c.m_USSRFaction) && c.Calls==1,"null_state_original");
  c=new Canonical();Check(c.Spawn(c.m_USSRState,null) && c.Calls==1 && c.m_USSRState.Slot.Size==1,"null_faction_original");
  c=new Canonical();c.m_USSRState.Slot=null;Check(!c.Spawn(c.m_USSRState,c.m_USSRFaction) && c.Calls==0,"missing_slot_refused");
  c=new Canonical();c.m_USSRState.Slot.State=AICF_EGroupSlotState.SPAWNING;Check(!c.Spawn(c.m_USSRState,c.m_USSRFaction) && c.Calls==0 && c.m_USSRState.Slot.Size==1,"spawning_untouched");
  c=new Canonical();c.m_USSRState.Slot.State=AICF_EGroupSlotState.READY;Check(!c.Spawn(c.m_USSRState,c.m_USSRFaction) && c.Calls==0 && c.m_USSRState.Slot.Size==1,"ready_untouched");
  c=new Canonical();c.m_USSRState.Slot.Group=new SCR_AIGroup();Check(!c.Spawn(c.m_USSRState,c.m_USSRFaction) && c.Calls==0 && c.m_USSRState.Slot.Size==1,"occupied_untouched");
  c=new Canonical();c.Callback=s=>{c.m_USSRState=new AICF_FactionState();};slot=c.m_USSRState.Slot;Check(c.Spawn(c.m_USSRState,c.m_USSRFaction) && c.Calls==1 && c.m_USSRState.Slot.Size==1 && slot.Size==10,"callback_successor_not_overwritten");
  var u=new UnsafeForeignState();foreign=new AICF_FactionState();Check(u.Spawn(foreign,u.m_USSRFaction) && foreign.Slot.Size==10,"unsafe_foreign_detected");
  var o=new UnsafeOccupied();o.m_USSRState.Slot.Group=new SCR_AIGroup();Check(o.Spawn(o.m_USSRState,o.m_USSRFaction) && o.m_USSRState.Slot.Size==10,"unsafe_occupied_detected");
  slot=new AICF_GroupSlot();slot.Size=10;slot.Unit=AICF_EGroupUnitType.INFANTRY;Check(slot.GetDeploymentSize()==1,"old_infantry_setup_replays_native_one");slot.Unit=AICF_EGroupUnitType.MOTORIZED_LIGHT;Check(slot.GetDeploymentSize()==10,"motorized_uses_desired");
  c=new Canonical();c.Spawn(c.m_USSRState,c.m_USSRFaction);slot=c.m_USSRState.Slot;int requested=slot.GetDeploymentSize();
  c.ProcessCallback=s=>{s.Slot.Generation=1;s.Slot.State=AICF_EGroupSlotState.READY;s.Slot.Group=new SCR_AIGroup{Count=requested};s.Slot.Roster9=new object();};
  c.Process(c.m_USSRState,c.m_USSRFaction);Check(requested==10 && c.ProcessCalls==1 && c.m_bCrewSpawnProfileRestored && slot.Unit==AICF_EGroupUnitType.INFANTRY && slot.Group.Count==10,"composed_ready_then_infantry_before_coordinator");
  c.ProcessCallback=null;c.Process(c.m_USSRState,c.m_USSRFaction);Check(c.ReducedCalls==1 && c.ProcessCalls==2,"reduction_setup_at_pre_coordinator_seam");
  c=new Canonical();slot=c.m_USSRState.Slot;slot.Size=10;slot.Group=new SCR_AIGroup{Count=10};slot.Roster9=new object();slot.Generation=1;slot.State=AICF_EGroupSlotState.SPAWNING;c.Process(c.m_USSRState,c.m_USSRFaction);Check(slot.Unit==AICF_EGroupUnitType.MOTORIZED_LIGHT && !c.m_bCrewSpawnProfileRestored,"no_ready_no_switch");
  slot.State=AICF_EGroupSlotState.READY;slot.Group.Count=1;c.Process(c.m_USSRState,c.m_USSRFaction);Check(slot.Unit==AICF_EGroupUnitType.MOTORIZED_LIGHT && !c.m_bCrewSpawnProfileRestored,"actual_one_no_switch");
  slot.Group.Count=10;slot.Group.OnCount=()=>{c.m_bStopped=true;};c.Process(c.m_USSRState,c.m_USSRFaction);Check(slot.Unit==AICF_EGroupUnitType.MOTORIZED_LIGHT && !c.m_bCrewSpawnProfileRestored,"native_callback_stop_no_switch");
  c.m_bStopped=false;slot.Group.OnCount=()=>{c.m_USSRState=new AICF_FactionState();};var oldState=c.m_USSRState;c.Process(oldState,c.m_USSRFaction);Check(slot.Unit==AICF_EGroupUnitType.MOTORIZED_LIGHT && !c.m_bCrewSpawnProfileRestored && c.m_USSRState.Slot.Size==1,"native_callback_successor_untouched");
  var oldSetup=new UnsafeInfantrySpawn();oldSetup.Spawn(oldSetup.m_USSRState,oldSetup.m_USSRFaction);Check(oldSetup.m_USSRState.Slot.Size==10 && oldSetup.m_USSRState.Slot.GetDeploymentSize()==1,"unsafe_old_setup_native_replay");
  return "PASS Crew canonical fixture actual="+count+" unsafe=3 native_runtime=NOT_RUN";
 }
}
'@
Add-Type -TypeDefinition $code -Language CSharp
[CrewCanonicalReplay]::Run()
Write-Output "PASS Crew canonical source controls=$checks"
