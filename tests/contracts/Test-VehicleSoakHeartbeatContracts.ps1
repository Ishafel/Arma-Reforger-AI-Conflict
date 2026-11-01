param([string]$RepositoryRoot=(Split-Path -Parent (Split-Path -Parent $PSScriptRoot)))
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot '../lib/Stage3StaticAudit.Common.ps1')
$source=[IO.File]::ReadAllText((Join-Path $RepositoryRoot 'tests/fixtures/AICF_VehicleSoakProbe.c'))
$record=[pscustomobject]@{Source=$source;Code=(ConvertTo-AICFCodeText $source)}
$helper=Get-AICFMethodBody $record 'AICF_VehicleSoakHeartbeat'
$update=Get-AICFMethodBody $record 'Update'
$branch=[regex]::Match($update,'(?s)if \(!slot.GetGroup\(\)\)\s*\{(?<body>[^{}]+)\}').Groups['body'].Value
if(!$branch.Contains('AICF_VehicleSoakHeartbeat(slot, faction, waitingTrip, now);') -or $branch.IndexOf('AICF_VehicleSoakHeartbeat(') -gt $branch.IndexOf('AICF_VehicleSoakFail("GROUP_UNAVAILABLE")')){throw 'Actual nullgroup Update skips heartbeat or changes failure order'}
foreach($required in @('now < m_iVehicleSoakNextSample','now + 10000','CountAliveAgents(slot.GetGroup())','slot.GetSpawnGeneration()','AICF_GroupTrace.Entity(slot.GetGroup())','slot.GetState()','slot.HasNativeSpawnCapacityReport()','trip.ObserveCapturedDeadRetirement()','retirement_status=','group_present=','AICF_VehicleSoakLog("HEARTBEAT", details)')){if(!$helper.Contains($required)){throw "Missing heartbeat guard $required"}}
if($helper -match '\b(?:MarkReady|MarkComplete|SetHealth|SetLifeState|Record.*Progress|ClearLatched|Teleport)\s*\('){throw 'Heartbeat mutates gameplay/proof'}
$writes=@([regex]::Matches($helper,'\b(m_[A-Za-z0-9_]+)\s*='));if(@($writes|Where-Object{$_.Groups[1].Value -ne 'm_iVehicleSoakNextSample'}).Count){throw 'Heartbeat modified route/stage/readiness'}
function Cs($s){return $s.Replace('!slot || !faction','slot == null || faction == null').Replace('if (trip)','if (trip != null)').Replace('if (evidence)','if (evidence != null)').Replace('if (slot.GetGroup())','if (slot.GetGroup() != null)').Replace('if (m_VehicleCoordinator)','if (m_VehicleCoordinator != null)').Replace('AICF_TransportTrip waitingTrip;','AICF_TransportTrip waitingTrip = null;').Replace('string.Format(','Fmt(').Replace('typename.EnumToString(AICF_EGroupSlotState,','EnumName(').Replace('typename.EnumToString(AICF_ETripRetirementObservation,','EnumName(')}
$space='AICFHeartbeatActual'+[guid]::NewGuid().ToString('N')
$template=@'
using System;using System.Collections.Generic;using System.Globalization;
namespace NAMESPACE {
 public enum AICF_EGroupSlotState { WAITING_FOR_RESPAWN, READY }
 public enum AICF_ETripRetirementObservation { NONE, PENDING, CONFIRMED }
 public class Group { public int Alive=5; }
 public class AICF_GroupSlot { public Group Group; public int Generation=2;public bool Capacity=true;public AICF_EGroupSlotState State=AICF_EGroupSlotState.WAITING_FOR_RESPAWN;public Group GetGroup(){return Group;}public int GetDesiredSize(){return 5;}public int GetSpawnGeneration(){return Generation;}public bool IsRecruitingInfantry(){return false;}public AICF_EGroupSlotState GetState(){return State;}public bool HasNativeSpawnCapacityReport(){return Capacity;} }
 public class SCR_CampaignFaction { public string GetFactionKey(){return "USSR";} }
 public class AICF_TripRetirementObservation {public AICF_ETripRetirementObservation GetStatus(){return AICF_ETripRetirementObservation.CONFIRMED;} }
 public class AICF_TransportTrip {public string GetPhase(){return "FAILED_CLOSED";}public string GetLastTransitionReason(){return "GROUP_UNAVAILABLE";}public AICF_TripRetirementObservation ObserveCapturedDeadRetirement(){return new AICF_TripRetirementObservation();} }
 public class Coordinator {public AICF_TransportTrip GetTrip(AICF_GroupSlot s){return new AICF_TransportTrip();} }
 public static class AICF_GroupRuntime {public static int CountAliveAgents(Group g){return g==null?0:g.Alive;} }
 public static class AICF_Stage3Diagnostics {public static string TripPhaseToString(string p){return p;} }
 public static class AICF_GroupTrace {public static int Snapshots;public static string Entity(Group g){return g==null?"NONE":"LIVE";}public static void Snapshot(AICF_GroupSlot s,string f,AICF_TransportTrip t,int x,string z){Snapshots++;} }
 public class Harness {
 public int m_iVehicleSoakNextSample;public int m_iVehicleSoakPhase=9;public bool m_bVehicleSoakFootArrived;public Coordinator m_VehicleCoordinator=new Coordinator();public List<string> Logs=new List<string>();public int Failures;
 static string EnumName(object x){return x.ToString();}
 static string Fmt(string fmt,params object[] args){for(int i=9;i>=1;i--)fmt=fmt.Replace("%"+i,"{"+(i-1)+"}");for(int i=0;i<args.Length;i++)if(args[i] is bool)args[i]=((bool)args[i])?"1":"0";return string.Format(CultureInfo.InvariantCulture,fmt,args);}
 void AICF_VehicleSoakLog(string e,string d){Logs.Add(e+" "+d);}void AICF_VehicleSoakFail(string r){if(r!="GROUP_UNAVAILABLE")throw new Exception("failure classification changed");Failures++;}
 public void AICF_VehicleSoakHeartbeat(AICF_GroupSlot slot,SCR_CampaignFaction faction,AICF_TransportTrip trip,int now) HELPER
 public void Tick(AICF_GroupSlot slot,SCR_CampaignFaction faction,int now){if(slot.GetGroup()==null){BRANCH}}
 public void OldTick(AICF_GroupSlot slot,SCR_CampaignFaction faction,int now){if(slot.GetGroup()==null){OLDBRANCH}}
 }
 public static class Check {public static int Run(){int checks=0;var s=new AICF_GroupSlot();var f=new SCR_CampaignFaction();var h=new Harness();for(int now=0;now<=140000;now+=1000)h.Tick(s,f,now);if(h.Logs.Count!=15||h.Failures!=141)throw new Exception("nullgroup lacks timed heartbeat/original failure invocation");checks++;
 foreach(string line in h.Logs)if(!line.Contains("alive=0")||!line.Contains("generation=2")||!line.Contains("group=NONE")||!line.Contains("route_complete=0")||!line.Contains("stage=9")||!line.Contains("native_capacity_wait=1")||!line.Contains("retirement_status=CONFIRMED")||line.Contains("slot_state=READY"))throw new Exception("fabricated READY/actor/route progress");checks++;
 if(h.m_bVehicleSoakFootArrived||h.m_iVehicleSoakPhase!=9||s.State!=AICF_EGroupSlotState.WAITING_FOR_RESPAWN||AICF_GroupTrace.Snapshots!=0)throw new Exception("heartbeat mutated gameplay/proof or snapshotted absent group");checks++;
 int count=h.Logs.Count;h.Tick(s,f,140000);if(h.Logs.Count!=count)throw new Exception("same tick heartbeat duplicate");checks++;
 var old=new Harness();for(int now=0;now<=140000;now+=1000)old.OldTick(s,f,now);if(old.Logs.Count!=0||old.Failures!=141)throw new Exception("old null branch no longer reproduces heartbeat silence");checks++;
 s.Group=new Group();s.State=AICF_EGroupSlotState.READY;s.Capacity=false;h.AICF_VehicleSoakHeartbeat(s,f,new AICF_TransportTrip(),150000);if(!h.Logs[h.Logs.Count-1].Contains("alive=5")||!h.Logs[h.Logs.Count-1].Contains("group=LIVE")||AICF_GroupTrace.Snapshots!=1)throw new Exception("present group actual state lost");checks++;
 return checks;} }
}
'@
$code=$template.Replace('NAMESPACE',$space).Replace('HELPER',(Cs $helper)).Replace('OLDBRANCH',(Cs $branch.Replace('AICF_VehicleSoakHeartbeat(slot, faction, waitingTrip, now);',''))).Replace('BRANCH',(Cs $branch))
Add-Type $code
$checks=([type]($space+'.Check')).GetMethod('Run').Invoke($null,@())
# Execute the unchanged frozen auditor's actual liveness block.
$auditor=[IO.File]::ReadAllText((Join-Path $RepositoryRoot 'tests/log-audits/Test-VehicleSoakObservationLog.ps1'))
$clockStart=$auditor.IndexOf(' $previous=$start');$clockEnd=$auditor.IndexOf(' Require stopped_native',$clockStart)
if($clockStart -lt 0 -or $clockEnd -lt $clockStart){throw 'Frozen liveness path not found'}
$clock=[scriptblock]::Create($auditor.Substring($clockStart,$clockEnd-$clockStart))
function Number($v){return [double]$v}
function Require($gate,[bool]$condition,$reason){if(!$condition){$script:clockErrors.Add($reason)}}
foreach($hung in @($false,$true)){
 $script:clockErrors=[Collections.Generic.List[string]]::new();$maxGap=0
 $start=[pscustomobject]@{index=0;fields=@{elapsed_ms=0;t_ms=1000}}
 $done=[pscustomobject]@{index=9999;fields=@{elapsed_ms=180000;t_ms=181000}}
 $heartbeats=@(for($t=10000;$t -lt180000;$t+=10000){if(!$hung -or $t -lt 30000 -or $t -gt110000){[pscustomobject]@{index=$t/100;fields=@{elapsed_ms=$t;t_ms=$t+1000}}}})
 & $clock
 if(!$hung -and $clockErrors.Count){throw 'Real WAIT heartbeat rejected by frozen60s clock'}
 if($hung -and !$clockErrors.Contains('HEARTBEAT_GAP_MAX_60S')){throw 'Truly hung server passed frozen60s clock'}
}
"PASS Soak nullgroup heartbeat: $checks actual Update/helper controls +2 actual frozen-auditor liveness controls; no synthetic READY/route/nativefatal waiver"
