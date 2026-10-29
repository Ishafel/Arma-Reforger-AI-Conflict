param([string]$RepositoryRoot=(Split-Path -Parent (Split-Path -Parent $PSScriptRoot)))
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot '../lib/Stage3StaticAudit.Common.ps1')
$core=Join-Path $RepositoryRoot 'AIConflictCore/Scripts/Game/AIConflict/Forces'
$policy=Get-Content (Join-Path $core 'AICF_SpawnAdmission.c') -Raw
$service=Get-Content (Join-Path $core 'AICF_InfantryRecruitmentService.c') -Raw
function Body($s,$n){Get-AICFMethodBody ([pscustomobject]@{Source=$s;Code=(ConvertTo-AICFCodeText $s)}) $n}
function Rules($p,$s){
 $inspect=Body $p 'InspectRecruit';$ctx=Body $s 'InspectRecruitAdmission';$tick=Body $s 'Tick';$update=Body $s 'Update'
 foreach($x in @('result.m_iRequested = 1;','!world || !us || !ussr || unmaterializedRecruits < 0','world.CanAIBeActivated()','MissingFaction(us, null)','MissingFaction(ussr, null)','usPending + ussrPending + unmaterializedRecruits')){if(!$inspect.Contains($x)){'EXACT_ONE_SHARED_NATIVE_GLOBAL_BUDGET';break}}
 foreach($x in @('!Replication.IsServer()','!m_Campaign.IsMaster()','!m_Campaign.IsRunning()','!m_aOrders.Contains(order)','order.m_RecruitmentService != this','!order.IsCurrent(order.m_Slot)','order.m_iGraphRevision != m_Graph.GetRevision()','state.GetSlot(order.m_Slot.GetSlotId()) != order.m_Slot','CountUnmaterializedAgents()')){if(!$ctx.Contains($x)){'CURRENT_AUTHORITY_SLOT_ORDER_GRAPH_CONTEXT';break}}
 if($update.IndexOf('m_NativeUS = us;') -lt $update.IndexOf('!m_Campaign.IsMaster()') -or $update.IndexOf('m_NativeUS = us;') -gt $update.IndexOf('Tick(order,')){'SAME_UPDATE_FACTION_CONTEXT'}
 $begin=$tick.IndexOf('m_Spawner.BeginRecruit(order)');$gateStart=$tick.IndexOf('AICF_SpawnAdmission nativeAdmission = InspectRecruitAdmission(order);');$gateEnd=$tick.IndexOf('order.m_iLoadoutRevision =', $gateStart)
 if($gateStart -lt 0 -or $gateStart -gt $begin -or $gateEnd -gt $begin){'WAIT_BEFORE_DONOR_OR_REQUEST'}
 $gate=$tick.Substring($gateStart,$gateEnd-$gateStart)
 foreach($x in @('if (!nativeAdmission || !nativeAdmission.CanAdmit())','order.m_bNativeCapacityWaiting = true;','INFANTRY_RECRUIT_NATIVE_CAPACITY_WAIT','return string.Empty;','order.m_bNativeCapacityWaiting = false;','INFANTRY_RECRUIT_NATIVE_CAPACITY_RELEASED')){if(!$gate.Contains($x)){'WAIT_AND_RELEASE_WITHOUT_RESOURCE';break}}
 if($gate -match 'BeginRecruit\(|SpawnGroup|RequestSpawn|DelayPurchase|m_iStartedAtMs\s*=|m_iSpawnAtMs\s*=|m_iToken\s*[=+]|m_Donor\s*=|Debit|Refund|SetLimitOfActiveAIs|SetAIGroupImportance'){'NO_DONOR_TIMER_TOKEN_PAYMENT_OR_LIMIT_MUTATION'}
 if($tick.IndexOf('now - order.m_iStartedAtMs >= visitTimeout') -gt $gateStart -or !$tick.Contains('AICF_InfantryRecruitmentConfig.SPAWN_TIMEOUT_MS')){'ORIGINAL_TOTAL_VISIT_AND_NATIVE_DEADLINES'}
 $diag=Body $s 'LogRecruitNative'
 foreach($x in @('HasExactFactionRoster','world.GetCurrentNumOfActiveAIs()','world.GetActiveGroupIntendedCount()','world.GetLimitOfActiveAIs()','world.CanAIBeActivated()','donor.GetIntendedActiveCount()','donor.IsDormant()','donor.AICF_DescribeSpawnDispatch()','engine_queue_observable=0')){if(!$diag.Contains($x)){'NATIVE_GLOBAL_DORMANT_DISPATCH_DIAGNOSTICS';break}}
 if(!$tick.Contains('LogRecruitNative(order, "REQUEST")') -or !$tick.Contains('LogRecruitNative(order, "INVALID_OR_TIMEOUT")')){'REQUEST_AND_FAILURE_NATIVE_RECEIPTS'}
}
if(@(Rules $policy $service).Count){throw "Recruit native source: $(Rules $policy $service)"}
$mutants=@(
 @('p','result.m_iRequested = 1;','result.m_iRequested = 0;'),@('p','world.CanAIBeActivated()','true'),
 @('p','MissingFaction(us, null)','0'),@('p','+ unmaterializedRecruits',''),
 @('s','!m_aOrders.Contains(order)','false'),@('s','order.m_RecruitmentService != this','false'),
 @('s','!order.IsCurrent(order.m_Slot)','false'),@('s','order.m_iGraphRevision != m_Graph.GetRevision()','false'),
 @('s','state.GetSlot(order.m_Slot.GetSlotId()) != order.m_Slot','false'),
 @('s','if (!nativeAdmission || !nativeAdmission.CanAdmit())','if (false)'),
 @('s','order.m_bNativeCapacityWaiting = true;','order.m_iStartedAtMs = System.GetTickCount(); order.m_bNativeCapacityWaiting = true;'),
 @('s','return string.Empty;'+[Environment]::NewLine,'return string.Empty;'+[Environment]::NewLine),
 @('s','donor.IsDormant()','false'),@('s','LogRecruitNative(order, "INVALID_OR_TIMEOUT")','SkipDiagnostics(order)')
)
$mutants=@($mutants|Where-Object{$_[1] -ne $_[2]})
foreach($m in $mutants){$p=$policy;$s=$service;if($m[0] -eq 'p'){$p=$p.Replace($m[1],$m[2])}else{$s=$s.Replace($m[1],$m[2])};if(!@(Rules $p $s).Count){throw "Recruit admission mutation escaped $($m[1])"}}
$missing=(Body $policy 'MissingFaction').Replace('int missing = 0;','int missing = 0;')
$fit=Body $policy 'FitsCounts';$inspect=Body $policy 'InspectRecruit';$can=Body $policy 'CanAdmit'
$harness=@'
using System;
public enum AICF_EGroupSlotState {WAITING,SPAWNING,READY}
public static class AICF_Stage1Config {public const int MAX_GROUP_SIZE=10;}
public class SCR_AIGroup {public int Count;public bool Dormant;public int GetAgentsCount(){return Count;}public static implicit operator bool(SCR_AIGroup x){return x!=null;}}
public class AICF_GroupSlot {public SCR_AIGroup Group;public AICF_EGroupSlotState State;public int Expected,Deployment;public int GetRosterExpectedCount(){return Expected;}public int GetDeploymentSize(){return Deployment;}public SCR_AIGroup GetGroup(){return Group;}public AICF_EGroupSlotState GetState(){return State;}public static implicit operator bool(AICF_GroupSlot x){return x!=null;}}
public class AICF_FactionState {public AICF_GroupSlot[] Slots=new AICF_GroupSlot[0];public int GetSlotCount(){return Slots.Length;}public AICF_GroupSlot GetSlot(int i){return Slots[i];}public static implicit operator bool(AICF_FactionState x){return x!=null;}}
public class AIWorld {public int Active,Intended,Limit;public bool Can=true;public int GetCurrentNumOfActiveAIs(){return Active;}public int GetActiveGroupIntendedCount(){return Intended;}public int GetLimitOfActiveAIs(){return Limit;}public bool CanAIBeActivated(){return Can;}public static implicit operator bool(AIWorld x){return x!=null;}}
public class AICF_SpawnAdmission {
 int m_iActive=-1,m_iIntended=-1,m_iLimit=-1,m_iPending=-1,m_iRequested;bool m_bCanActivate;
 public static bool FitsCounts(int active,int intended,int limit,int pending,int requested) FIT
 public static int MissingFaction(AICF_FactionState state,AICF_GroupSlot requestedSlot) MISSING
 public static AICF_SpawnAdmission InspectRecruit(AIWorld world,AICF_FactionState us,AICF_FactionState ussr,int unmaterializedRecruits) INSPECT
 public bool CanAdmit() CAN
}
'@
Add-Type -TypeDefinition ($harness.Replace('FIT',$fit).Replace('MISSING',$missing).Replace('INSPECT',$inspect).Replace('CAN',$can))
$us=[AICF_FactionState]::new();$ussr=[AICF_FactionState]::new();$world=[AIWorld]::new();$world.Limit=228
$checks=0
foreach($active in @(-1,0,225,227,228,229)){foreach($intended in @(-1,0,225,227,228,229)){foreach($pending in @(-1,0,1,2,3)){foreach($flag in @($false,$true)){
 $world.Active=$active;$world.Intended=$intended;$world.Can=$flag
 $expected=$flag -and $active -ge 0 -and $intended -ge 0 -and $pending -ge 0 -and [Math]::Max($active,$intended)+$pending+1 -le 228
 if([AICF_SpawnAdmission]::InspectRecruit($world,$us,$ussr,$pending).CanAdmit() -ne $expected){throw 'Actual native request1 capacity boundary failed'};$checks++
}}}}
$world.Active=227;$world.Intended=227;$world.Can=$true
if(![AICF_SpawnAdmission]::InspectRecruit($world,$us,$ussr,0).CanAdmit() -or [AICF_SpawnAdmission]::InspectRecruit($world,$us,$ussr,1).CanAdmit()){throw 'Two simultaneous donor reservations overbooked or own waiting caused starvation'}
$slot=[AICF_GroupSlot]::new();$slot.State=[AICF_EGroupSlotState]::SPAWNING;$slot.Expected=5;$slot.Deployment=5;$slot.Group=[SCR_AIGroup]::new();$slot.Group.Count=2;$slot.Group.Dormant=$true;$us.Slots=@($slot)
$world.Active=224;$world.Intended=224
if([AICF_SpawnAdmission]::InspectRecruit($world,$us,$ussr,1).CanAdmit()){throw 'Missing group plus unmaterialized donor reservation ignored'}
$slot.Group.Count=5;$world.Active=227;$world.Intended=227
if(![AICF_SpawnAdmission]::InspectRecruit($world,$us,$ussr,0).CanAdmit()){throw 'Materialized group double counted or dormant flag bypassed native budget'}
$world.Active=229;$world.Intended=229
if([AICF_SpawnAdmission]::InspectRecruit($world,$us,$ussr,0).CanAdmit()){throw 'Dormant/empty-group native flag allowed actual overbudget'}
foreach($which in 0..2){$w=$world;$u=$us;$r=$ussr;switch($which){0{$w=$null}1{$u=$null}2{$r=$null}};if([AICF_SpawnAdmission]::InspectRecruit($w,$u,$r,0).CanAdmit()){throw 'Unknown current context admitted'}}
Write-Output "PASS Recruitment native admission: source_mutants=$($mutants.Count) actual_request1_boundaries=$checks simultaneous/missinggroups/donors/dormant_overbudget/unknown controls=7; no own-wait reservation, total visit deadline unchanged"
