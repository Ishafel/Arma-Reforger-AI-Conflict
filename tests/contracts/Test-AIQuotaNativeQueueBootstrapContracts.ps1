param([string]$RepositoryRoot=(Split-Path -Parent (Split-Path -Parent $PSScriptRoot)))
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot '../lib/Stage3StaticAudit.Common.ps1')
$source=[IO.File]::ReadAllText((Join-Path $RepositoryRoot 'AIConflictCore/Scripts/Game/AIConflict/Forces/AICF_AIWorldQuotaService.c'))
$body=Get-AICFMethodBody ([pscustomobject]@{Source=$source;Code=(ConvertTo-AICFCodeText $source)}) 'CapturePrepolicyNativeRequests'
if($body-match'\b(Reserve|BindEntity|RegisterExistingBinding|CaptureStockSnapshot|SetLimitOfActiveAIs|ActivateAI|DeleteRplEntity)\s*\('){throw 'Pending journal invents charged bodies or modifies native policy'}
$actual=$body.Replace('ref ','').Replace('array<AICF_AIQuotaNativeQueueRecord> records = {};','List<AICF_AIQuotaNativeQueueRecord> records = new List<AICF_AIQuotaNativeQueueRecord>();').Replace('foreach (AICF_AIQuotaNativeQueueRecord record : records)','foreach (AICF_AIQuotaNativeQueueRecord record in records)').Replace('EntityID','long').Replace('records.Count()','records.Count').Replace('int waitingUnits;','int waitingUnits=0;').Replace('int sum;','int sum=0;').Replace('metadataCeiling, sum)','metadataCeiling, out sum)').Replace('string.Format','Fmt.Format')
$preamble=@'
using System;using System.Collections.Generic;
public class N {public static implicit operator bool(N v){return v!=null;}}
public static class AICF_AIQuotaMath {public const int MAX_TOTAL_UNITS=4096;public static bool AddBounded(int a,int b,int max,out int sum){sum=0;if(a<0||b<0||a>max-b)return false;sum=a+b;return true;}}
public static class Fmt {public static string Format(string s,params object[] args){for(int n=args.Length;n>0;n--)s=s.Replace("%"+n,Convert.ToString(args[n-1]));return s;}}
public static class AICF_Stage1Diagnostics {public static List<string> Logs=new List<string>();public static void Info(string name,string s){Logs.Add(name+" "+s);}}
public class Slots {public int N=10;public int Count(){return N;}}
public class SCR_AIGroup:N {public int Actual=1,Intended=1;public Slots m_aUnitPrefabSlots=new Slots();public int GetAgentsCount(){return Actual;}public int GetIntendedActiveCount(){return Intended;}}
public class AICF_AIQuotaGroupQueue:N {public SCR_AIGroup m_Group=new SCR_AIGroup();public long m_GroupId=25;public string m_sRpl="rpl25";public static int RequestedUnits(SCR_AIGroup group,int slots){if(slots==0)return 0;if(group.Actual>group.m_aUnitPrefabSlots.N)return -1;return group.m_aUnitPrefabSlots.N-group.Actual;}}
public class AICF_AIQuotaNativeQueueRecord:N {public bool Current=true;public AICF_AIQuotaGroupQueue m_Identity=new AICF_AIQuotaGroupQueue();public int m_iSlotsWanted=9,m_iSequence=1;public bool IsCurrent(){return Current;}}
public class AICF_AIQuotaNativeQueueJournal:N {public bool Current=true,Complete=true;public List<AICF_AIQuotaNativeQueueRecord> Records=new List<AICF_AIQuotaNativeQueueRecord>();public bool Snapshot(List<AICF_AIQuotaNativeQueueRecord> output){output.AddRange(Records);return Complete;}public bool IsCurrent(){return Current;}}
public class World {public int NativeActive=1,NativeIntended=1,Hard=128;public AICF_AIQuotaNativeQueueJournal Journal=new AICF_AIQuotaNativeQueueJournal();public AICF_AIQuotaNativeQueueJournal AICF_GetNativeQuotaJournal(){return Journal;}}
public class Service {public World m_World=new World();public int m_iCapturedNativeRequests,m_iCapturedNativeWaitingUnits;public bool Capture()
'@
$cases=@'
}
public static class Cases {
 public static int Run(){int n=0;Action<bool,string> need=(ok,label)=>{if(!ok)throw new Exception(label);n++;};AICF_Stage1Diagnostics.Logs.Clear();var s=new Service();var r=new AICF_AIQuotaNativeQueueRecord();s.m_World.Journal.Records.Add(r);
 need(s.Capture(),"exact real Request9 journal capture");need(s.m_iCapturedNativeRequests==1&&s.m_iCapturedNativeWaitingUnits==9,"one actual body versus nine unmaterialized metadata units");
 need(s.m_World.NativeActive==1&&s.m_World.NativeIntended==1&&s.m_World.Hard==128&&r.m_Identity.m_Group.Actual==1,"journal does not fake grandfather9/native intended10/raised floor");
 need(AICF_Stage1Diagnostics.Logs.Exists(x=>x.Contains("group_native_intended=1 ")&&x.Contains("pending_charged_as_body=0")),"actual partial native counter retained");
 need(AICF_Stage1Diagnostics.Logs.Exists(x=>x.Contains("native_queue_membership=UNPROVEN")&&x.Contains("native_priority_unchanged=1")),"journal metadata is not native queue-enumeration proof");
 r.Current=false;need(!s.Capture(),"stale exact request does not become absent/paid/free metadata");r.Current=true;s.m_World.Journal.Complete=false;need(!s.Capture(),"incomplete native request journal blocks bootstrap");s.m_World.Journal.Complete=true;s.m_World.Journal.Current=false;need(!s.Capture(),"changed journal world rejects postread");
 s=new Service();need(s.Capture()&&s.m_iCapturedNativeRequests==0&&s.m_iCapturedNativeWaitingUnits==0,"actual empty prepolicy journal succeeds without fake promises");
 s=new Service();r=new AICF_AIQuotaNativeQueueRecord();r.m_Identity.m_Group.Actual=11;s.m_World.Journal.Records.Add(r);need(!s.Capture(),"invalid actual versus authored count blocks bootstrap");return n;
 }
}
'@
$code=$preamble+$actual+$cases
Add-Type $code
$count=[Cases]::Run()
$caught=0
foreach($variant in @(
 @('MetadataAsPhysical','pending_charged_as_body=0','pending_charged_as_body=1'),
 @('InventNativeIntended','group.GetIntendedActiveCount()','10'),
 @('ClaimQueueMembership','native_queue_membership=UNPROVEN','native_queue_membership=PROVEN'),
 @('IgnoreExactRecord','if (!record || !record.IsCurrent()) return false;','if (!record) return false;'),
 @('IgnoreSnapshot','if (!journal || !journal.Snapshot(records)) return false;','if (!journal) return false; journal.Snapshot(records);')
)){
 if(!$actual.Contains($variant[1])){throw "Missing bootstrap mutation $($variant[0])"}
 $ns='BootstrapUnsafe'+$variant[0];$mutated=($preamble+$actual.Replace($variant[1],$variant[2])+$cases).Replace('public class N {',"namespace $ns {public class N {")+'}'
 Add-Type $mutated
 $failed=$false;try{[void]([type]("$ns.Cases")).GetMethod('Run').Invoke($null,@())}catch{$failed=$true}
 if(!$failed){throw "Unsafe bootstrap survived $($variant[0])"};$caught++
}
"PASS native queue bootstrap actual=$count executed_unsafe=$caught partial_physical1_pending9_not_charged=1 runtime=NOT_RUN"
