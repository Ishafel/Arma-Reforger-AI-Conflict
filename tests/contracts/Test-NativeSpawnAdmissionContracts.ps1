param([string]$RepositoryRoot=(Split-Path -Parent (Split-Path -Parent $PSScriptRoot)))
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot '../lib/Stage3StaticAudit.Common.ps1')
$taskPolicy=Get-Content (Join-Path $RepositoryRoot 'AIConflictCore/Scripts/Game/AIConflict/Forces/AICF_SpawnAdmission.c') -Raw
$taskController=Get-Content (Join-Path $RepositoryRoot 'AIConflictCore/Scripts/Game/AIConflict/Bootstrap/AICF_MatchController.c') -Raw
$taskSlot=Get-Content (Join-Path $RepositoryRoot 'AIConflictCore/Scripts/Game/AIConflict/State/AICF_GroupSlot.c') -Raw
$taskRecruit=Get-Content (Join-Path $RepositoryRoot 'AIConflictCore/Scripts/Game/AIConflict/Forces/AICF_InfantryRecruitmentService.c') -Raw
function Body($source,$method){Get-AICFMethodBody ([pscustomobject]@{Source=$source;Code=(ConvertTo-AICFCodeText $source)}) $method}
$taskFits=Body $taskPolicy 'FitsCounts'
$taskProjection=(Body $taskPolicy 'ProjectedCounts').Replace('int.MAX','int.MaxValue')
$taskMissing=Body $taskPolicy 'MissingFaction'
$taskDonors=(Body $taskRecruit 'CountUnmaterializedAgents').Replace('order : m_aOrders','order in m_aOrders').Replace('int count;','int count = 0;')
$taskHarness=@'
using System;
public enum AICF_EGroupSlotState {WAITING,SPAWNING,READY}
public static class AICF_Stage1Config {public const int MAX_GROUP_SIZE=10;}
public class SCR_AIGroup {public int Count;public int GetAgentsCount(){return Count;}public static implicit operator bool(SCR_AIGroup value){return value!=null;}}
public class AICF_GroupSlot {
 public AICF_EGroupSlotState State;public int Expected;public int Deployment;public SCR_AIGroup Group;
 public AICF_EGroupSlotState GetState(){return State;}public int GetRosterExpectedCount(){return Expected;}
 public int GetDeploymentSize(){return Deployment;}public SCR_AIGroup GetGroup(){return Group;}
 public static implicit operator bool(AICF_GroupSlot value){return value!=null;}
}
public class AICF_FactionState {public AICF_GroupSlot[] Slots;public int GetSlotCount(){return Slots.Length;}public AICF_GroupSlot GetSlot(int index){return Slots[index];}public static implicit operator bool(AICF_FactionState value){return value!=null;}}
public class AICF_InfantryRecruitmentOrder {public SCR_AIGroup m_Donor;public static implicit operator bool(AICF_InfantryRecruitmentOrder value){return value!=null;}}
public static class AICFNativeAdmissionExtracted {
 public static bool FitsCounts(int active,int intended,int limit,int pending,int requested) __FITS__
 public static int ProjectedCounts(int active,int intended,int pending,int requested) __PROJECTION__
 public static int MissingFaction(AICF_FactionState state,AICF_GroupSlot requestedSlot) __MISSING__
 public static AICF_InfantryRecruitmentOrder[] m_aOrders;
 public static int CountUnmaterializedAgents() __DONORS__
 public static int Run(){
 int checks=0;int[] values={-1,0,1,4,5,83,84,220,228,int.MaxValue};
 foreach(int active in values)foreach(int intended in values)foreach(int limit in values)foreach(int pending in values)foreach(int requested in values){
  bool expected=active>=0&&intended>=0&&limit>0&&pending>=0&&requested>0&&Math.Max((long)active,intended)+pending+requested<=limit;
  if(FitsCounts(active,intended,limit,pending,requested)!=expected)throw new Exception("Capacity/overflow boundary");
  checks++;
 }
 foreach(int active in values)foreach(int intended in values)foreach(int pending in values)foreach(int requested in values){
  long exact=Math.Max((long)active,intended)+pending+requested;
  int expected=(active<0||intended<0||pending<0||requested<0)?-1:(int)Math.Min(int.MaxValue,exact);
  if(ProjectedCounts(active,intended,pending,requested)!=expected)throw new Exception("Projection overflow");checks++;
 }
 var candidate=new AICF_GroupSlot{State=AICF_EGroupSlotState.WAITING,Deployment=5};
 var state=new AICF_FactionState{Slots=new[]{candidate,
  new AICF_GroupSlot{State=AICF_EGroupSlotState.SPAWNING,Expected=5,Deployment=5,Group=new SCR_AIGroup{Count=2}},
  new AICF_GroupSlot{State=AICF_EGroupSlotState.SPAWNING,Expected=1,Deployment=10,Group=new SCR_AIGroup{Count=0}},
  new AICF_GroupSlot{State=AICF_EGroupSlotState.READY,Expected=5,Deployment=5,Group=new SCR_AIGroup{Count=5}},
  new AICF_GroupSlot{State=AICF_EGroupSlotState.SPAWNING,Expected=0,Deployment=1}}};
 if(MissingFaction(state,candidate)!=5)throw new Exception("Missing members double counted or seed1 read as desired10");checks++;
 state.Slots[1].Group.Count=5;if(MissingFaction(state,candidate)!=2)throw new Exception("Materialized actor remains reserved");checks++;
 state.Slots[1].Expected=11;if(MissingFaction(state,candidate)!=-1)throw new Exception("Invalid roster did not fail closed");checks++;
 m_aOrders=new[]{new AICF_InfantryRecruitmentOrder{m_Donor=new SCR_AIGroup{Count=1}},new AICF_InfantryRecruitmentOrder{m_Donor=new SCR_AIGroup{Count=0}},new AICF_InfantryRecruitmentOrder(),null};
 if(CountUnmaterializedAgents()!=1)throw new Exception("Alive donor counted twice");checks++;
 return checks;
 }
}
'@
Add-Type -TypeDefinition ($taskHarness.Replace('__FITS__',$taskFits).Replace('__PROJECTION__',$taskProjection).Replace('__MISSING__',$taskMissing).Replace('__DONORS__',$taskDonors))
$taskChecks=[AICFNativeAdmissionExtracted]::Run()
function Rules($controller,$policy,$slot){
 $start=Body $controller 'TryStartReplacement'
 $admit=$start.IndexOf('AICF_SpawnAdmission.Inspect(')
 if($admit -lt 0 -or $admit -gt $start.IndexOf('TryReserveDeployment(') -or $admit -gt $start.IndexOf('slot.BeginReplacementSpawn(')){'ADMISSION_BEFORE_RESERVATION_AND_GENERATION'}
 if($start -notmatch '(?s)if \(!nativeAdmission.CanAdmit\(\)\).*?PostponeReinforcementUntil\(nowMs \+ UPDATE_INTERVAL_MS\).*?GROUP_SPAWN_NATIVE_CAPACITY_WAIT.*?return;'){'CAPACITY_WAIT_NO_GENERATION'}
 if($start -notmatch 'CountUnmaterializedAgents\(\)' -or !$start.Contains('GROUP_SPAWN_NATIVE_CAPACITY_RELEASED')){'DONOR_RESERVATION_AND_RELEASE'}
 if((Body $policy 'CanAdmit') -notmatch 'm_bCanActivate && FitsCounts'){'GLOBAL_CAN_ACTIVATE_REQUIRED'}
 if((Body $slot 'ClearRuntimeReferences') -notmatch 'm_bNativeSpawnCapacityReported = false;'){'REPORT_LIFECYCLE_RESET'}
 if((ConvertTo-AICFCodeText $policy) -match '\b(SetLimitOfActiveAIs|SetAIGroupImportance|RequestSpawn|BeginReplacementSpawn|CallLater|SetHealth)\s*\('){'READ_ONLY_ADMISSION_POLICY'}
}
$taskFailures=@(Rules $taskController $taskPolicy $taskSlot);if($taskFailures.Count){throw ($taskFailures -join ',')}
foreach($taskMutant in @(
 @($taskController.Replace('if (!nativeAdmission.CanAdmit())','if (false)'),$taskPolicy,$taskSlot),
 @($taskController,$taskPolicy.Replace('m_bCanActivate && FitsCounts','FitsCounts'),$taskSlot),
 @($taskController,$taskPolicy,$taskSlot.Replace('m_bNativeSpawnCapacityReported = false;','')),
 @($taskController.Replace('AICF_SpawnAdmission.Inspect(','AICF_SpawnAdmission.Bypass('),$taskPolicy,$taskSlot)
)){if(!@(Rules $taskMutant[0] $taskMutant[1] $taskMutant[2]).Count){throw 'Admission lifecycle mutation escaped'}}
Write-Output "PASS NativeSpawnAdmission extracted cases=$taskChecks plus 4 control-flow mutants; native pressure remains separate"
