param([string]$RepositoryRoot=(Split-Path -Parent (Split-Path -Parent $PSScriptRoot)))
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot '../lib/Stage3StaticAudit.Common.ps1')
$s=Get-Content (Join-Path $RepositoryRoot 'tests/fixtures/AICF_AIQuotaStockPressureProbe.c') -Raw
$record=[pscustomobject]@{Source=$s;Code=(ConvertTo-AICFCodeText $s)}
$methods='public static bool Full(int physical,int reserved,int limit)'+(Get-AICFMethodBody $record 'Full')
$methods+='public static bool Veto(bool admitted,int agents,int beforeIntended,int afterIntended,bool full)'+(Get-AICFMethodBody $record 'Veto')
$methods+='public static bool Ready(bool current,int ageMs,bool full,bool original,bool recruited,bool exactRoster)'+(Get-AICFMethodBody $record 'Ready')
$cases=@'
public static class Cases {public static int Run(){int n=0;Action<bool,string> need=(ok,label)=>{if(!ok)throw new Exception(label);n++;};
need(Proof.Full(128,0,128),"physical full actual stock");foreach(int p in new[]{-1,0,127,129})need(!Proof.Full(p,0,128),"unknown/under/over stock is not full");foreach(int r in new[]{-1,1,128})need(!Proof.Full(128,r,128),"pending reservation is not physical-full proof");need(!Proof.Full(0,0,0)&&!Proof.Full(4097,0,4097),"invalid limits");
need(Proof.Veto(false,0,128,128,true),"zero body intended-stable actual veto");need(!Proof.Veto(true,0,128,128,true),"admitted extra request rejects");need(!Proof.Veto(false,1,128,128,true),"unexpected body rejects");need(!Proof.Veto(false,0,-1,-1,true),"unknown native counter rejects");need(!Proof.Veto(false,0,128,129,true),"native intended increment rejects");need(!Proof.Veto(false,0,128,128,false),"lost pressure rejects");
need(Proof.Ready(true,35000,true,true,true,true),"exact positive35s");need(!Proof.Ready(false,35000,true,true,true,true),"stale owner rejects");foreach(int age in new[]{-1,0,34999,600001})need(!Proof.Ready(true,age,true,true,true,true),"short or unbounded window rejects");need(!Proof.Ready(true,35000,false,true,true,true),"lost full pressure rejects");need(!Proof.Ready(true,35000,true,false,true,true),"original actor lost rejects");need(!Proof.Ready(true,35000,true,true,false,true),"missing accepted actual recruitment rejects");need(!Proof.Ready(true,35000,true,true,true,false),"fake roster rejects");return n;}}
'@
$code='using System;public static class Proof {'+$methods+'}'+$cases
Add-Type $code
$n=[Cases]::Run();$mutants=0
foreach($m in @(@('physical == limit','physical <= limit'),@('reserved == 0','reserved >= 0'),@('!admitted','true'),@('agents == 0','true'),@('beforeIntended == afterIntended','true'),@('ageMs >= 35000','ageMs >= 0'),@('current && ageMs','true && ageMs'),@('&& exactRoster','&& true'))){$ns='Unsafe'+[guid]::NewGuid().ToString('N');$bad='using System;namespace '+$ns+'{public static class Proof {'+$methods.Replace($m[0],$m[1])+'}'+$cases+'}';Add-Type $bad;$failed=$false;try{[void]([type]("$ns.Cases")).GetMethod('Run').Invoke($null,@())}catch{$failed=$true};if(!$failed){throw "Unsafe proof survived $($m[0])"};$mutants++}
foreach($forbidden in @('MarkReady\(','SetDormantCounts\(','SetLimitOfActiveAIs\(','SetOrigin\(','SetTransform\(','SetHealth\(','DeleteRplEntity\(','DespawnMembers\(','\.ActivateAI\(','\.DeactivateAI\(')){if($s-match$forbidden){throw "Fixture changes forbidden policy/body $forbidden"}}
if(([regex]::Matches($s,'m_iQuotaPressureVetoAt\s*=').Count)-ne1){throw 'Veto clock changed or retried'}
if($s-notmatch'CONTINUOUS_STOCK_PRESSURE_LOST'-or$s-notmatch'PRESSURE_BUDGET_UNKNOWN'){throw 'Unknown/discontinuous full pressure must fail'}
if($s-notmatch'AssignPlayerPointOrder\(slot, m_USFaction, m_vQuotaPressureHome, false\)'-or$s-notmatch'RequestPlayerRecruitment\(m_QuotaPressureSlot, m_USFaction, reason\)'){throw 'Normal hold/recruit API missing'}
if($s-notmatch'Remove\(AICF_QuotaPressureTimeout\)'){throw 'Timeout lifecycle removal missing'}
if($s-notmatch'bool exactRoster = AICF_GroupRuntime.HasExactFactionRoster\('-or$s-notmatch'm_bQuotaPressureRecruitAccepted, exactRoster\)'){throw 'Positive roster must come from actual native group read'}
"PASS actual_pressure_proof=$n executed_unsafe=$mutants source_policy_guards=15 runtime=NOT_RUN"
$profile=(Get-AICFMethodBody $record 'RefreshCensus').Replace('System.GetTickCount()','ProbeSystem.GetTickCount()').Replace('System.IsCLIParam','ProbeSystem.IsCLIParam').Replace('super.RefreshCensus()','base.RefreshCensus()')
$profileCode=@'
using System;
public static class ProbeSystem {public static int Now=1000;public static bool Enabled=true;public static int GetTickCount(){return Now;}public static bool IsCLIParam(string s){return Enabled;}}
public class World {public int GetCurrentNumOfActiveAIs(){return 128;}public int GetActiveGroupIntendedCount(){return 128;}}
public class Parent {public bool Result=true,Current=true;public int Calls,Duration=5;public World m_World=new World();public virtual bool RefreshCensus(){Calls++;ProbeSystem.Now+=Duration;return Result;}public bool IsCurrentWorld(){return Current;}public int Logs;public void Print(string s){Logs++;}}
public class Profile:Parent {public int m_iPressureCensusCalls,m_iPressureCensusMs,m_iPressureCensusMaxMs,m_iPressureCensusReportAt;public override bool RefreshCensus()
'@ + $profile + @'
}
public static class ProfileCases {public static int Run(){int n=0;Action<bool,string> need=(ok,s)=>{if(!ok)throw new Exception(s);n++;};ProbeSystem.Enabled=true;ProbeSystem.Now=1000;var p=new Profile();need(p.RefreshCensus()&&p.Calls==1&&p.m_iPressureCensusCalls==1&&p.m_iPressureCensusMs==5&&p.m_iPressureCensusMaxMs==5,"exact one true parent call, elapsed captured");p.Result=false;p.Duration=20;need(!p.RefreshCensus()&&p.Calls==2&&p.m_iPressureCensusCalls==2&&p.m_iPressureCensusMs==25&&p.m_iPressureCensusMaxMs==20,"false parent preserved and aggregate only");ProbeSystem.Now=15000;need(!p.RefreshCensus()&&p.Calls==3&&p.Logs==1&&p.m_iPressureCensusCalls==0&&p.m_iPressureCensusMs==0,"bounded15s report no extra refresh");ProbeSystem.Enabled=false;p=new Profile{Result=false};need(!p.RefreshCensus()&&p.Calls==1&&p.m_iPressureCensusCalls==0&&p.Logs==0,"outside test flag exact super and no metrics");ProbeSystem.Enabled=true;ProbeSystem.Now=15000;p=new Profile{Current=false};need(p.RefreshCensus()&&p.Calls==1&&p.Logs==0,"lost owner prevents post-native world report");return n;}}
'@
Add-Type $profileCode
$profileCases=[ProfileCases]::Run()
$profileMutants=0
foreach($m in @(@('bool result = base.RefreshCensus();','bool result = base.RefreshCensus(); base.RefreshCensus();'),@('return result;','return !result;'),@('if (!ProbeSystem.IsCLIParam("aicfQuotaStockPressure"))','if (ProbeSystem.IsCLIParam("aicfQuotaStockPressure"))'))){$ns='ProfileUnsafe'+[guid]::NewGuid().ToString('N');$bad=$profileCode.Replace('public static class ProbeSystem {',"namespace $ns {public static class ProbeSystem {").Replace($m[0],$m[1])+'}';Add-Type $bad;$caught=$false;try{[void]([type]("$ns.ProfileCases")).GetMethod('Run').Invoke($null,@())}catch{$caught=$true};if(!$caught){throw 'Profiler changes caller behavior'};$profileMutants++}
"PASS actual_profiler=$profileCases executed_unsafe=$profileMutants added_refresh_calls=0 native=NOT_RUN"
$boot=[IO.File]::ReadAllText((Join-Path $RepositoryRoot 'tests/fixtures/AICF_AIQuotaBootReadbackProbe.c'))
foreach($required in @('CallLater(Read, 15000, true)','GetCallqueue().Remove(Read)','Replication.IsServer() && System.IsCLIParam("aicfQuotaBootReadback")','agents.Count() <= 4096','if (sample >= 8) continue;','world.GetWorld() != GetGame().GetWorld()','snapshot.Capture(world)','alive_inactive_uncounted=%6')){
 if(!$boot.Contains($required)){throw "Read-only boot observer boundary missing $required"}
}
foreach($forbidden in @('MarkReady\(','SetDormantCounts\(','SetLimitOfActiveAIs\(','SetOrigin\(','SetTransform\(','SetHealth\(','DeleteRplEntity\(','DespawnMembers\(','\.ActivateAI\(','\.DeactivateAI\(','RefreshCensus\(')){
 if($boot-match$forbidden){throw "Read-only boot observer mutates policy/body/cadence $forbidden"}
}
"PASS boot readback source_guards=19 native=NOT_RUN diagnostic_capture_only=1"
# Removing only the named diagnostic blocks must restore the exact previously
# reviewed fixture tokens (full/veto/35s/timeout/roster/cleanup unchanged).
$projected=[regex]::Replace($s,'(?ms)^[\t ]*// AICF_PRESSURE_TRACE_BEGIN ([A-Z_]+)[^\r\n]*\r?\n.*?^[\t ]*// AICF_PRESSURE_TRACE_END \1[^\r\n]*(?:\r?\n|$)','')
$tokens=@([regex]::Matches($projected,'(?s)/\*.*?\*/|//[^\r\n]*|"(?:\\.|[^"\\])*"|[A-Za-z_]\w*|\d+(?:\.\d+)?|[^\s]')|Where-Object{!$_.Value.StartsWith('//')-and!$_.Value.StartsWith('/*')}|ForEach-Object{$_.Value})-join"`n"
$sha=[Security.Cryptography.SHA256]::Create()
$tokenHash=[BitConverter]::ToString($sha.ComputeHash([Text.Encoding]::UTF8.GetBytes($tokens))).Replace('-','')
if($tokenHash-ne'33EC2C8A52F0687C63714453ADC7604BEF266DDD815BAD5BF5EE59AD0D9FBA8D'){throw "Pressure diagnostic inverse projection changed original policy: $tokenHash"}
$traceStart=$s.IndexOf('class AICF_QuotaStockPressureTrace')
$traceEnd=$s.IndexOf('// AICF_PRESSURE_TRACE_END CONTEXT',$traceStart)
$traceSource=$s.Substring($traceStart,$traceEnd-$traceStart)
$traceSource=$traceSource.Replace('class AICF_QuotaStockPressureTrace','public class AICF_QuotaStockPressureTrace').Replace('static ','public static ').Replace('GetGame()','Runtime.Game').Replace('EntityID','long').Replace('System.GetTickCount()','ProbeClock.GetTickCount()').Replace('string.Format','Fmt.Format').Replace('Print(','Sink.Print(')
$wrappers=''
foreach($name in @('AdmitGroupRequest','BeginGroupBirth','ObserveGroupFactory','ReserveUnits','ObserveExternalFactory')){
 $signature=@{AdmitGroupRequest='public override bool AdmitGroupRequest(SCR_AIGroup group,int slotsWanted)';BeginGroupBirth='public override AICF_AIQuotaBirth BeginGroupBirth(SCR_AIGroup group,int index,string prefab)';ObserveGroupFactory='public override void ObserveGroupFactory(AICF_AIQuotaBirth birth,IEntity entity)';ReserveUnits='public override AICF_AIQuotaReservation ReserveUnits(int bucket,int units,int serial)';ObserveExternalFactory='public override void ObserveExternalFactory(AICF_AIQuotaCreation creation,IEntity entity)'}[$name]
 $body=Get-AICFMethodBody $record $name
 if(@([regex]::Matches($body,'super\.'+[regex]::Escape($name)+'\(')).Count-ne1){throw "Trace must call native parent once: $name"}
 if($body-match'\bRefreshCensus\s*\(|\bRequestSpawn\s*\(|\bBeginDispatch\s*\(|\bBind\s*\('){throw "Trace changes dispatch $name"}
 $body=$body.Replace('super.','base.').Replace('EntityID','long').Replace('ResourceName','string').Replace('string.Format','Fmt.Format')
 $body=[regex]::Replace($body,'(?m)^(\s*)(string|SCR_AIGroup) (\w+);','$1$2 $3=null;')
 $body=[regex]::Replace($body,'(?m)^(\s*)(long|bool) (\w+);','$1$2 $3=default($2);')
 $wrappers+=$signature+$body
}
$traceCode=@'
using System;using System.Collections.Generic;
public class N {public static implicit operator bool(N n){return n!=null;}}
public class BaseWorld:N {public SCR_AIGroup Group;public IEntity FindEntityByID(long id){return Group!=null&&Group.Id==id?Group:null;}}
public class IEntity:N {public long Id=42;public string Rpl="R42";public BaseWorld World;public long GetID(){return Id;}public BaseWorld GetWorld(){return World;}}
public class SCR_AIGroup:IEntity {}
public class AICF_MatchController:N {public static AICF_MatchController Active;public static AICF_MatchController GetActiveController(){return Active;}}
public class Game:N {public BaseWorld World;public BaseWorld GetWorld(){return World;}}
public static class Runtime {public static Game Game;}
public static class Replication {public static bool IsServer(){return true;}}
public static class ProbeClock {public static int GetTickCount(){return 1000;}}
public static class Sink {public static List<string> Lines=new List<string>();public static void Print(string s){Lines.Add(s);}}
public static class Fmt {public static string Format(string s,params object[] v){for(int i=v.Length;i>=1;i--)s=s.Replace("%"+i,Convert.ToString(v[i-1]));return s;}}
public static class AICF_VehicleBehaviorBinding {public static string ComponentRpl(IEntity e){return e==null?"NONE":e.Rpl;}}
public static class AICF_EAIQuotaBucket {public const int STOCK=0,COMBAT=1;}
public class Queue:N {public SCR_AIGroup m_Group;public int m_iSerial=9,Bucket;public int GetBucket(){return Bucket;}}
public class Ticket:N {public bool m_bClosed;}
public class AICF_AIQuotaBirth:N {public Queue m_Queue;public Ticket m_Ticket=new Ticket();public int m_Status;public string m_Prefab="root",m_ProducedPrefab;}
public class AICF_AIQuotaReservation:N {public int m_Bucket;}
public class AICF_AIQuotaCreation:N {public AICF_AIQuotaReservation m_Reservation;public int m_iSerial=10,m_Status;public bool m_bHeaderOnly;}
public class Parent:N {public AICF_MatchController Root;public bool Admit;public AICF_AIQuotaBirth Birth;public AICF_AIQuotaReservation Reservation;public int AdmitCalls,BeginCalls,ObserveCalls,ReserveCalls,ExternalCalls,BudgetCalls,RefreshCalls;public SCR_AIGroup LastGroup;public IEntity LastEntity;public int LastIndex,LastUnits,LastSerial;public string LastPrefab;public Action Callback;
public AICF_MatchController GetQuotaController(){return Root;}public string BudgetDetails(int b,int n){BudgetCalls++;return "bucket="+b+" requested="+n+" physical=128";}
public virtual bool AdmitGroupRequest(SCR_AIGroup g,int n){AdmitCalls++;LastGroup=g;LastUnits=n;if(Callback!=null)Callback();return Admit;}
public virtual AICF_AIQuotaBirth BeginGroupBirth(SCR_AIGroup g,int i,string p){BeginCalls++;LastGroup=g;LastIndex=i;LastPrefab=p;if(Callback!=null)Callback();return Birth;}
public virtual void ObserveGroupFactory(AICF_AIQuotaBirth b,IEntity e){ObserveCalls++;LastEntity=e;if(b!=null){b.m_Status=3;b.m_ProducedPrefab="variant";b.m_Ticket.m_bClosed=true;}if(Callback!=null)Callback();}
public virtual AICF_AIQuotaReservation ReserveUnits(int b,int n,int serial){ReserveCalls++;LastUnits=n;LastSerial=serial;if(Callback!=null)Callback();return Reservation;}
public virtual void ObserveExternalFactory(AICF_AIQuotaCreation c,IEntity e){ExternalCalls++;LastEntity=e;if(c!=null)c.m_Status=3;if(Callback!=null)Callback();}}
'@+$traceSource+'public class AICF_AIWorldQuotaService:Parent {'+$wrappers+@'
}
public static class TraceCases {
static int n;static AICF_AIWorldQuotaService S;static SCR_AIGroup G;static void Need(bool ok,string label){n++;if(!ok)throw new Exception(label);}
static void Setup(){Sink.Lines.Clear();S=new AICF_AIWorldQuotaService();G=new SCR_AIGroup();var world=new BaseWorld{Group=G};G.World=world;Runtime.Game=new Game{World=world};S.Root=new AICF_MatchController();AICF_MatchController.Active=S.Root;AICF_QuotaStockPressureTrace.Bind(G,S,S.Root,world);}
public static int Run(){
Setup();Need(!S.AdmitGroupRequest(G,1)&&S.AdmitCalls==1&&S.LastGroup==G&&S.LastUnits==1&&Sink.Lines.Count==1,"false admission single parent/original args unchanged");
S.Admit=true;Need(S.AdmitGroupRequest(G,2)&&S.AdmitCalls==2,"true admission unchanged");Need(Sink.Lines[1].Contains("result=True"),"actual successful admission logged");
Setup();var b=new AICF_AIQuotaBirth{m_Queue=new Queue{m_Group=G}};S.Birth=b;Need(S.BeginGroupBirth(G,4,"exact")==b&&S.BeginCalls==1&&S.LastIndex==4&&S.LastPrefab=="exact","same exact native birth receipt returned");
S.Birth=null;Need(S.BeginGroupBirth(G,5,"empty")==null&&S.BeginCalls==2,"null native birth preserved");
Setup();var e=new IEntity{Id=77};b=new AICF_AIQuotaBirth{m_Queue=new Queue{m_Group=G}};S.ObserveGroupFactory(b,e);Need(S.ObserveCalls==1&&S.LastEntity==e&&b.m_Status==3&&b.m_Ticket.m_bClosed,"observe preserves real parent effects once");Need(Sink.Lines[0].Contains("entity=77")&&Sink.Lines[0].Contains("status=3"),"exact returned actor and post-super status joined");
Setup();var other=new SCR_AIGroup{Id=99};b=new AICF_AIQuotaBirth{m_Queue=new Queue{m_Group=other,Bucket=0}};S.ObserveGroupFactory(b,e);Need(Sink.Lines.Count==1&&Sink.Lines[0].Contains("producer_group=99"),"other stock producer observed for adoption join");
Setup();b=new AICF_AIQuotaBirth{m_Queue=new Queue{m_Group=other,Bucket=1}};S.ObserveGroupFactory(b,e);Need(S.ObserveCalls==1&&Sink.Lines.Count==0,"unrelated managed producer parent still called without stock log");
Setup();S.Reservation=new AICF_AIQuotaReservation();Need(S.ReserveUnits(0,3,12)==S.Reservation&&S.ReserveCalls==1&&S.LastUnits==3&&S.LastSerial==12,"exact reservation returned and args unchanged");S.Reservation=null;Need(S.ReserveUnits(0,1,13)==null&&S.ReserveCalls==2,"reserve veto preserved");
Setup();S.ReserveUnits(1,2,8);Need(S.ReserveCalls==1&&Sink.Lines.Count==0,"nonstock reservation unaffected");
Setup();var c=new AICF_AIQuotaCreation{m_Reservation=new AICF_AIQuotaReservation()};S.ObserveExternalFactory(c,e);Need(S.ExternalCalls==1&&S.LastEntity==e&&c.m_Status==3&&Sink.Lines[0].Contains("entity=77"),"external stock constructor result logged after real parent");
Setup();c=new AICF_AIQuotaCreation{m_Reservation=new AICF_AIQuotaReservation{m_Bucket=1}};S.ObserveExternalFactory(c,e);Need(S.ExternalCalls==1&&Sink.Lines.Count==0,"managed external constructor not misclassified");
Setup();S.Callback=()=>AICF_MatchController.Active=null;S.Admit=true;Need(S.AdmitGroupRequest(G,1)&&AICF_QuotaStockPressureTrace.s_Lost==1,"lost diagnostic scope never changes actual success");
Setup();AICF_QuotaStockPressureTrace.Clear();S.Admit=true;Need(S.AdmitGroupRequest(G,1)&&S.AdmitCalls==1&&S.BudgetCalls==0&&Sink.Lines.Count==0,"outside exact bound trace no new budget reads/logs");
Setup();for(int i=0;i<2050;i++)AICF_QuotaStockPressureTrace.Emit("test","row");Need(Sink.Lines.Count==2048&&AICF_QuotaStockPressureTrace.s_Overflow,"bounded diagnostic overflow explicit");AICF_QuotaStockPressureTrace.Summary();Need(Sink.Lines.Count==2049&&Sink.Lines[2048].Contains("overflow=True"),"summary survives cap");
Need(S.RefreshCalls==0,"diagnostics add no refresh");return n;}}
'@
$traceCode='namespace StockTraceReplay {'+$traceCode+'}'
Add-Type $traceCode
$traceCases=[StockTraceReplay.TraceCases]::Run()
$traceUnsafe=0
foreach($mutation in @(
 @('bool result = base.AdmitGroupRequest(group, slotsWanted);','bool result = base.AdmitGroupRequest(group, slotsWanted); base.AdmitGroupRequest(group, slotsWanted);'),
 @('return result;','return !result;','BOOL_ONLY'),
 @('base.ObserveGroupFactory(birth, entity);','base.ObserveGroupFactory(birth, entity); base.ObserveGroupFactory(birth, entity);'),
 @('base.ObserveExternalFactory(creation, entity);',''),
 @('if (s_Logged >= 2048)','if (s_Logged >= 99999)')
)){
 if(!$traceCode.Contains($mutation[0])){throw 'Actual trace mutation anchor missing'}
 $traceUnsafe++;$namespace='StockTraceUnsafe'+$traceUnsafe
 if($mutation.Count-gt2){
  $boolWrapper=Get-AICFMethodBody $record 'AdmitGroupRequest'
  $boolWrapper=$boolWrapper.Replace('super.','base.').Replace('string.Format','Fmt.Format')
  $boolWrapper=[regex]::Replace($boolWrapper,'(?m)^(\s*)string (\w+);','$1string $2=null;')
  $changed=$traceCode.Replace($boolWrapper,$boolWrapper.Replace($mutation[0],$mutation[1]))
 }else{$changed=$traceCode.Replace($mutation[0],$mutation[1])}
 $changed=$changed.Replace('namespace StockTraceReplay','namespace '+$namespace)
 Add-Type $changed
 $rejected=$false;try{$null=([type]($namespace+'.TraceCases')).GetMethod('Run').Invoke($null,@())}catch{$rejected=$true}
 if(!$rejected){throw 'Actual unsafe trace mutation survived'}
}
"PASS actual trace wrappers=$traceCases executed_unsafe=$traceUnsafe single_super=5 policy_inverse_sha=$tokenHash native=NOT_RUN"
