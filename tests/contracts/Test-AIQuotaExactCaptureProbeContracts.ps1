param([string]$RepositoryRoot=(Split-Path -Parent (Split-Path -Parent $PSScriptRoot)))
$PrototypeRoot=$RepositoryRoot
$ErrorActionPreference='Stop'
$core=[IO.File]::ReadAllText((Join-Path $PrototypeRoot 'AIConflictCore/Scripts/Game/AIConflict/Forces/AICF_AIQuotaCensus.c'))
$fixture=[IO.File]::ReadAllText((Join-Path $PrototypeRoot 'tests/fixtures/AICF_AIQuotaExactCaptureProbe.c'))
if($fixture-match '\b(GetCurrentNumOfActiveAIs|GetActiveGroupIntendedCount|GetAIAgents|GetLOD|GetLifeState|GetControlledEntity|IsAIActivated|CountTowardsAIWorldLimit|CallLater|DeleteRplEntity|SetLOD|ActivateAI)\s*\('){throw 'Observer introduced extra native reads/actions'}
$variants=@(
 @{Name='Actual';Source=$fixture},
 @{Name='WrongActual';Source=$fixture.Replace('frame.m_bActual = actual;','frame.m_bActual = !actual;')},
 @{Name='WrongInvalid';Source=$fixture.Replace('frame.m_iInvalid = invalid;','frame.m_iInvalid = 0;')},
 @{Name='WrongBefore';Source=$fixture.Replace('frame.m_iBeforeActive = beforeActive;','frame.m_iBeforeActive = afterActive;')},
 @{Name='WrongRow';Source=$fixture.Replace('counted, active, result);','counted, active, !result);')},
 @{Name='WrongReturn';Source=$fixture.Replace('return result;','return !result;')},
 @{Name='DoubleCapture';Source=$fixture.Replace('bool result = super.Capture(world);','super.Capture(world); bool result = super.Capture(world);')},
 @{Name='SkipCapture';Source=$fixture.Replace('bool result = super.Capture(world);','bool result = false;')},
 @{Name='NoBound';Source=$fixture.Replace('s_Keys.Count() >= MAX_RECORDS','false')},
 @{Name='NoSampleBound';Source=$fixture.Replace('frame.m_iQ10Samples < MAX_SAMPLES','true')},
 @{Name='NoDedup';Source=$fixture.Replace('if (s_Keys.Contains(data)) return;','')},
 @{Name='SnapshotFromDynamic';Source=$fixture.Replace('frame.m_iSnapshotQ10 = q10;','frame.m_iSnapshotQ10 = frame.m_iQ10;')},
 @{Name='SkipSampler';Source=$fixture.Replace('AICF_ExactSampleSnapshot(frame);','')},
 @{Name='NoSnapshotInstanceFence';Source=$fixture.Replace('if (!AICF_ExactOwnsFrame(frame) || frame.m_Snapshot != this) return;','')},
 @{Name='WrongQ10';Source=$fixture.Replace('if (counted && !active) frame.m_iQ10++;','if (counted && active) frame.m_iQ10++;')},
 @{Name='StaleFrame';Source=$fixture.Replace('s_Frame == frame && !frame.m_bSuperseded','true')},
 @{Name='NoInstanceFence';Source=$fixture.Replace('frame.m_Snapshot.AICF_ExactOwnsFrame(frame) && ','')},
 @{Name='NoStopFlag';Source=$fixture.Replace('s_bStopped = true;','')},
 @{Name='StartClearsBoot';Source=$fixture.Replace('if (!s_bStopped) return;','s_Keys.Clear(); if (!s_bStopped) return;')},
 @{Name='LateStart';Source=$fixture.Replace('AICF_QuotaExactCaptureProbe.Start();'+[char]10+[char]9+[char]9+'return super.OnGameStart();','bool result = super.OnGameStart(); AICF_QuotaExactCaptureProbe.Start(); return result;')},
 @{Name='StopLeak';Source=$fixture.Replace('s_Keys.Clear();','')},
 @{Name='SkipRow';Source=$fixture.Replace('bool result = super.IsCurrent(world);','bool result = true;')}
)
function Adapt([string]$s,[bool]$isCore){
 $s=$s.Replace('EntityID','long').Replace('GetGame()','AICF_CensusRuntime.Game').Replace('protected ref ','protected ').Replace('static ref ','static ').Replace('static const ','const ')
 $s=[regex]::Replace($s,'array<(?:ref )?(\w+)>','AICF_CensusList<$1>')
 $s=[regex]::Replace($s,'(AICF_CensusList<\w+>) (\w+) = \{\};','$1 $2 = new $1();')
 $s=[regex]::Replace($s,'foreach \((\w+) (\w+) : (\w+)\)','foreach ($1 $2 in $3)')
 $s=$s.Replace('string.Format(','AICF_CensusRuntime.Format(').Replace('System.GetCLIParam("aicfQuotaCensusExactCaptureProbe", value)','DiagCLI.GetCLIParam("aicfQuotaCensusExactCaptureProbe", out value)')
 $s=$s.Replace('override protected void','protected override void').Replace('super.','base.')
 if($isCore){
  $s=$s.Replace('class AICF_AIQuotaCensus','public class AICF_AIQuotaCensusBase : AICF_CensusNative').Replace('class AICF_AIQuotaAgentRow','public class AICF_AIQuotaAgentRowBase : AICF_CensusNative')
  $s=$s.Replace('bool Capture(SCR_AIWorld world)','virtual bool Capture(SCR_AIWorld world)').Replace('bool IsCurrent(BaseWorld world)','virtual bool IsCurrent(BaseWorld world)').Replace('protected void AICF_ObserveCaptureDecision','protected virtual void AICF_ObserveCaptureDecision')
 }else{
  $s=$s.Replace('modded class AICF_AIQuotaCensus','public class AICF_AIQuotaCensus : AICF_AIQuotaCensusBase').Replace('modded class AICF_AIQuotaAgentRow','public class AICF_AIQuotaAgentRow : AICF_AIQuotaAgentRowBase').Replace('modded class ArmaReforgerScripted','public class ArmaReforgerScripted : ArmaReforgerScriptedBase')
  $s=[regex]::Replace($s,'(?m)^class (AICF_QuotaExact\w+)\s*$','public class $1 : AICF_CensusNative')
 }
 $s=[regex]::Replace($s,'(?m)^(\s*)(static |override |virtual |const )?(bool|void|int|string|BaseWorld|AICF_\w+) (\w+)\(','$1public $2$3 $4(')
 $s=[regex]::Replace($s,'(?m)^(\s*)(static |const )?(AICF_CensusList<\w+>|AICF_\w+|AIAgent|IEntity|AIGroup|BaseWorld|long|bool|int|string) (m_\w+|s_\w+|MAX_\w+)([; =])','$1public $2$3 $4$5')
 $s=$s.Replace('int invalid;','int invalid = 0;').Replace('int examined;','int examined = 0;').Replace('int countedActive;','int countedActive = 0;')
  foreach($name in @('scanned','nullRows','q10','samples','index')){$s=$s.Replace(('int '+$name+';'),('int '+$name+' = 0;'))}
 $s=$s.Replace('string ids;','string ids = "";')
 return $s
}
$common=@'
#pragma warning disable 0162,0168
using System;
using System.Collections.Generic;
'@
$stub=@'
public class AICF_CensusNative {public static implicit operator bool(AICF_CensusNative v){return v!=null;}public static void Print(string s){AICF_CensusRuntime.Print(s);}}
public class AICF_CensusList<T>:List<T>{public new int Count(){return base.Count;}public void Insert(T value){Add(value);}}
public static class Replication {public static bool Server=true;public static bool IsServer(){return Server;}}
public static class AICF_AIQuotaMath {public const int MAX_TOTAL_UNITS=4096;}
public static class DiagCLI {public static bool Enabled=true;public static bool GetCLIParam(string k,out string v){v=Enabled?"1":"0";return true;}}
public class BaseWorld:AICF_CensusNative {public Dictionary<long,IEntity> Entities=new Dictionary<long,IEntity>();public IEntity FindEntityByID(long id){IEntity e;return Entities.TryGetValue(id,out e)?e:null;}}
public class IEntity:AICF_CensusNative {public BaseWorld World;public long Id;public virtual BaseWorld GetWorld(){return World;}public long GetID(){return Id;}}
public class Vehicle:IEntity {public static Vehicle Cast(IEntity e){return e as Vehicle;}}
public class AIGroup:IEntity {}
public class AIAgent:IEntity {public IEntity Entity;public AIGroup Parent;public bool Counted,Active;public Action OnControlled;public int ControlledReads;public IEntity GetControlledEntity(){ControlledReads++;if(OnControlled!=null)OnControlled();return Entity;}public AIGroup GetParentGroup(){return Parent;}public bool CountTowardsAIWorldLimit(){return Counted;}public bool IsAIActivated(){return Active;}}
public class SCR_AIWorld:IEntity {public int Active,Intended,ListReads,ActiveReads,IntendedReads;public Action<int> OnList;public Action<int> OnActive;public Exception Failure;public AICF_CensusList<AIAgent> Agents=new AICF_CensusList<AIAgent>();public int GetCurrentNumOfActiveAIs(){ActiveReads++;if(OnActive!=null)OnActive(ActiveReads);return Active;}public int GetActiveGroupIntendedCount(){IntendedReads++;return Intended;}public void GetAIAgents(AICF_CensusList<AIAgent> output){ListReads++;if(Failure!=null)throw Failure;if(OnList!=null)OnList(ListReads);output.AddRange(Agents);}}
public class AICF_CensusGame:AICF_CensusNative {public SCR_AIWorld AI;public BaseWorld Main;public BaseWorld GetWorld(){return Main;}public SCR_AIWorld GetAIWorld(){return AI;}}
public static class AICF_CensusRuntime {public static AICF_CensusGame Game;public static List<string> Logs=new List<string>();public static Action<string> OnPrint;public static string Format(string f,params object[] args){for(int n=9;n>=1;n--)f=f.Replace("%"+n,"{"+(n-1)+"}");return string.Format(f,args);}public static void Print(string s){Logs.Add(s);if(OnPrint!=null)OnPrint(s);}}
public class ArmaReforgerScriptedBase:AICF_CensusNative {public static int EndCalls,StartCalls;public static Action OnEnd,OnStart;public static bool StartResult=true;public virtual bool OnGameStart(){StartCalls++;if(OnStart!=null)OnStart();return StartResult;}public virtual void OnGameEnd(){EndCalls++;if(OnEnd!=null)OnEnd();}}
public class PlainCensus:AICF_AIQuotaCensusBase {}
'@
$cases=@'
public static class Cases
{
 static int count;
 static void Need(bool v,string s){count++;if(!v)throw new Exception(s);}
 static SCR_AIWorld Setup(int bodies=3)
 {
  AICF_QuotaExactCaptureProbe.Stop();AICF_QuotaExactCaptureProbe.Start();DiagCLI.Enabled=true;Replication.Server=true;AICF_CensusRuntime.Logs.Clear();AICF_CensusRuntime.OnPrint=null;
  var main=new BaseWorld();var w=new SCR_AIWorld{World=main,Id=1,Active=bodies-1,Intended=2};AICF_CensusRuntime.Game=new AICF_CensusGame{AI=w,Main=main};var g=new AIGroup{Id=20,World=main};main.Entities.Add(g.Id,g);
  for(int n=0;n<bodies;n++){var e=new IEntity{World=main,Id=30+n};var a=new AIAgent{World=main,Id=100+n,Entity=e,Parent=g,Counted=true,Active=n<bodies-1};main.Entities.Add(e.Id,e);main.Entities.Add(a.Id,a);w.Agents.Add(a);}return w;
 }
 static string Last(){return AICF_CensusRuntime.Logs.Count==0?"":AICF_CensusRuntime.Logs[AICF_CensusRuntime.Logs.Count-1];}
 static string Trace(SCR_AIWorld w){string s=w.ActiveReads+"/"+w.IntendedReads+"/"+w.ListReads;foreach(var a in w.Agents)if(a!=null)s+="/"+a.ControlledReads;return s;}
 static void Has(string token){Need(Last().Contains(token),token+" missing from "+Last());}
 public static int Run()
 {
  count=0;var w=Setup();var plain=new PlainCensus();Need(plain.Capture(w),"original exact world succeeds");string originalTrace=Trace(w);
  w=Setup();DiagCLI.Enabled=false;var c=new AICF_AIQuotaCensus();Need(c.Capture(w),"disabled exact transparent");Need(Trace(w)==originalTrace,"disabled native getter trace unchanged");Need(AICF_CensusRuntime.Logs.Count==0&&AICF_QuotaExactCaptureProbe.s_Frame==null,"disabled no frame/log");
  w=Setup();c=new AICF_AIQuotaCensus();Need(c.Capture(w),"enabled exact succeeds");Need(Trace(w)==originalTrace,"enabled exact native getter trace unchanged");Has("predicate_seen=True predicate_actual=True returned=3 examined=3 invalid=0 counted_active=2 before_active=2 after_active=2");Has("before_intended=2 after_intended=2 capture_result=True capture_reason=COMPLETE");Has("initial_row_checks=3 initial_row_false=0 repeated_row_checks=3 repeated_row_false=0");Has("q11=2 q10=1 q01=0 q00=0");Has("102/32/countedTrue/activeFalse/row_currentTrue;");Has("life_lod_rpl=NOT_OBSERVED");Has("snapshot_scope=CAPTURE_SNAPSHOT_CACHED_AFTER_RETURN snapshot_seen=True snapshot_rows_scanned=3 snapshot_null_rows=0 snapshot_q10=1 snapshot_q10_samples=[102/32/countedTrue/activeFalse;]");Need(AICF_QuotaExactCaptureProbe.s_Frame==null,"frame released");
  int logs=AICF_CensusRuntime.Logs.Count;Need(c.Capture(w),"same identity repeat valid");Need(AICF_CensusRuntime.Logs.Count==logs,"value identical dedupe excludes serial");w.Agents[2].Id++;Need(c.Capture(w),"new cached agent ID repeat valid");Need(AICF_CensusRuntime.Logs.Count==logs+1,"new q10 identity not hidden by dedupe");
  w=Setup();w.Active=3;c=new AICF_AIQuotaCensus();Need(!c.Capture(w),"q10 never made known");Has("predicate_actual=False");Has("counted_active=2 before_active=3 after_active=3");Has("q11=2 q10=1");Need(w.ActiveReads==2&&w.IntendedReads==2&&w.ListReads==1,"failure no extra world counts");
  w=Setup();w.Agents.Add(null);c=new AICF_AIQuotaCensus();Need(!c.Capture(w),"null entry fails");Has("returned=4 examined=4 invalid=1");Has("initial_row_checks=3 initial_row_false=0");
  w=Setup();w.World.Entities.Remove(30);c=new AICF_AIQuotaCensus();Need(!c.Capture(w),"actual invalid row fails");Has("invalid=1");Has("initial_row_false=1");Has("100/30/countedTrue/activeTrue/row_currentFalse;");
  w=Setup();w.Agents[2].Entity=null;c=new AICF_AIQuotaCensus();Need(!c.Capture(w),"missing body remains unknown");Has("invalid=1");Has("102/0/countedTrue/activeFalse/row_currentTrue;");
  w=Setup();var foreignAgent=new AIAgent{World=w.World,Id=900,Entity=new IEntity{World=w.World,Id=800},Parent=w.Agents[0].Parent,Counted=true,Active=false};w.World.Entities.Add(800,foreignAgent.Entity);var foreignRow=new AICF_AIQuotaAgentRow{m_Agent=foreignAgent,m_AgentId=900,m_Entity=foreignAgent.Entity,m_EntityId=800,m_Group=foreignAgent.Parent,m_bCounted=true,m_bActivated=false};w.OnList=n=>{if(n==1)Need(foreignRow.IsCurrent(w.World),"foreign same-world dynamic row actual valid");};c=new AICF_AIQuotaCensus();Need(c.Capture(w),"foreign dynamic row does not alter actual Capture");Has("q11=2 q10=2");Has("900/800/countedTrue/activeFalse/row_currentTrue;");Has("snapshot_rows_scanned=3 snapshot_null_rows=0 snapshot_q10=1 snapshot_q10_samples=[102/32/countedTrue/activeFalse;]");Need(!Last().Substring(Last().IndexOf("snapshot_q10_samples=[")).Contains("900/800"),"dedicated snapshot q10 excludes foreign dynamic row");
  w=Setup();w.OnList=n=>{if(n==1){w.Active=1;w.Intended=3;}};c=new AICF_AIQuotaCensus();Need(!c.Capture(w),"changed original counters fail");Has("before_active=2 after_active=1");Has("before_intended=2 after_intended=3");
  w=Setup();w.OnList=n=>{if(n==2)w.Agents[2].Counted=false;};c=new AICF_AIQuotaCensus();Need(!c.Capture(w),"predicate true final capture false retained");Has("predicate_actual=True");Has("capture_result=False capture_reason=REPEATED_CENSUS_CHANGED");Has("repeated_row_checks=3 repeated_row_false=1");
  w=Setup();Replication.Server=false;c=new AICF_AIQuotaCensus();Need(!c.Capture(w),"early world fail");Has("predicate_seen=False");Has("capture_reason=WORLD_IDENTITY");Has("predicate_unseen_values=UNOBSERVED");Need(w.ActiveReads==0&&w.ListReads==0,"early failure does not read counters");
  w=Setup(20);for(int i=0;i<20;i++)w.Agents[i].Active=false;w.Active=0;c=new AICF_AIQuotaCensus();Need(c.Capture(w),"many inactive body capture unchanged");Has("q10=20");Has("snapshot_q10=20");Has("snapshot_samples_truncated=True");Has("q10_samples_truncated=True");Need(!Last().Contains("108/38/"),"q10 samples hard bound8");
  w=Setup();c=new AICF_AIQuotaCensus();AICF_QuotaExactCaptureFrame old=null,successor=null;bool nested=false;w.OnList=n=>{if(!nested){nested=true;old=AICF_QuotaExactCaptureProbe.s_Frame;Need(c.Capture(w),"same-instance nested native capture");successor=AICF_QuotaExactCaptureProbe.s_Frame;}};Need(!c.Capture(w),"outer original same-instance shared-row reentry fails closed");Need(old.m_bSuperseded,"old unique object marked superseded");Need(AICF_CensusRuntime.Logs.Count==1,"stale outer may not publish or mutate dedupe");Need(AICF_QuotaExactCaptureProbe.s_iPublished==1&&AICF_QuotaExactCaptureProbe.s_Keys.Count()==1,"successor budget unchanged by outer");Has("observations_incomplete=True");
  w=Setup();c=new AICF_AIQuotaCensus();nested=false;w.OnList=n=>{if(!nested){nested=true;old=AICF_QuotaExactCaptureProbe.s_Frame;AICF_QuotaExactCaptureProbe.Stop();AICF_QuotaExactCaptureProbe.Start();var next=AICF_QuotaExactCaptureProbe.Begin(c);Need(next.m_iSerial==old.m_iSerial,"Stop/new sequence ABA constructed");AICF_QuotaExactCaptureProbe.Row(next,999,999,true,false,true);successor=next;}};Need(c.Capture(w),"ABA outer actual return retained");Need(AICF_QuotaExactCaptureProbe.s_Frame==successor,"ABA successor object retained");Need(successor.m_iInitialChecks==1,"ABA stale outer cannot append row checks");Need(AICF_QuotaExactCaptureProbe.s_Keys.Count()==0&&AICF_QuotaExactCaptureProbe.s_iPublished==0,"ABA stale outer cannot consume record budget");c.AICF_ExactSampleSnapshot(successor);Need(!successor.m_bSnapshotSeen,"snapshot sampler rejects foreign instance-frame token despite same snapshot");
  w=Setup();c=new AICF_AIQuotaCensus();w.Failure=new Exception("original sentinel");Exception caught=null;try{c.Capture(w);}catch(Exception e){caught=e;}Need(caught==w.Failure,"same original exception object propagated");Need(AICF_CensusRuntime.Logs.Count==0,"exception not fabricated Capture result");old=AICF_QuotaExactCaptureProbe.s_Frame;Need(old!=null,"exception pending frame explicit until next Begin/Stop");new ArmaReforgerScripted().OnGameEnd();Need(old.m_bSuperseded&&AICF_QuotaExactCaptureProbe.s_Frame==null,"Stop supersedes exception frame");Need(!c.AICF_ExactOwnsFrame(old),"Stop clears snapshot observer token");Has("event=STOP published=0 pending_capture=True observations_incomplete=True");Need(ArmaReforgerScriptedBase.EndCalls==1,"game end delegates once");
  w=Setup();c=new AICF_AIQuotaCensus();for(int n=0;n<AICF_QuotaExactCaptureProbe.MAX_RECORDS+3;n++){w.Agents[2].Id=1000+n;Need(c.Capture(w),"bounded diagnostic capture result unchanged");}Need(AICF_QuotaExactCaptureProbe.s_Keys.Count()==384&&AICF_QuotaExactCaptureProbe.s_iPublished==384,"hard detail record bound384");Need(AICF_CensusRuntime.Logs.Count==385&&AICF_QuotaExactCaptureProbe.s_bOverflow,"one explicit overflow");Has("event=OVERFLOW observations_incomplete=1");AICF_QuotaExactCaptureProbe.Stop();Need(AICF_QuotaExactCaptureProbe.s_Keys.Count()==0&&AICF_QuotaExactCaptureProbe.s_iPublished==0&&!AICF_QuotaExactCaptureProbe.s_bOverflow,"Stop clears value cache/budget");
  w=Setup();c=new AICF_AIQuotaCensus();Need(c.Capture(w),"pre-Start boot capture");int bootKeys=AICF_QuotaExactCaptureProbe.s_Keys.Count();AICF_QuotaExactCaptureProbe.Start();Need(AICF_QuotaExactCaptureProbe.s_Keys.Count()==bootKeys,"Start while active preserves boot records");AICF_QuotaExactCaptureProbe.Stop();int stopLogs=AICF_CensusRuntime.Logs.Count;int listsBefore=w.ListReads;Need(c.Capture(w),"stopped Capture still delegates original return");Need(w.ListReads==listsBefore+2,"stopped Capture delegates once");Need(AICF_CensusRuntime.Logs.Count==stopLogs&&AICF_QuotaExactCaptureProbe.s_Frame==null&&AICF_QuotaExactCaptureProbe.s_Keys.Count()==0,"stopped Capture creates no logs/pending/cache");Need(AICF_QuotaExactCaptureProbe.Begin(c)==null,"stopped Begin cannot restart implicitly");ArmaReforgerScriptedBase.OnStart=()=>{Need(!AICF_QuotaExactCaptureProbe.s_bStopped,"Start precedes single superclass callback");Need(c.Capture(w),"new lifecycle superclass capture valid");};ArmaReforgerScriptedBase.StartResult=false;int starts=ArmaReforgerScriptedBase.StartCalls;Need(!new ArmaReforgerScripted().OnGameStart(),"original OnGameStart bool retained");Need(ArmaReforgerScriptedBase.StartCalls==starts+1&&AICF_QuotaExactCaptureProbe.s_Keys.Count()==1,"explicit Start enables next lifecycle once");ArmaReforgerScriptedBase.OnStart=null;ArmaReforgerScriptedBase.StartResult=true;ArmaReforgerScriptedBase.OnEnd=()=>{int afterStopLogs=AICF_CensusRuntime.Logs.Count;Need(c.Capture(w),"end-super cleanup original Capture delegates");Need(AICF_QuotaExactCaptureProbe.s_bStopped&&AICF_QuotaExactCaptureProbe.s_Frame==null&&AICF_CensusRuntime.Logs.Count==afterStopLogs,"end-super cleanup cannot restart observer");};new ArmaReforgerScripted().OnGameEnd();ArmaReforgerScriptedBase.OnEnd=null;
  return count;
 }
}
'@
$code=$common
foreach($v in $variants){$code+="`nnamespace V$($v.Name) {`n"+$stub+(Adapt $core $true)+(Adapt $v.Source $false)+$cases+"`n}`n"}
Add-Type -TypeDefinition $code -Language CSharp -WarningAction SilentlyContinue
$actual=[VActual.Cases]::Run();$mutants=0
foreach($v in $variants|Select-Object -Skip 1){$caught=$false;try{[void](('V'+$v.Name+'.Cases'-as[type])::Run())}catch{$caught=$true};if(!$caught){throw ('Unsafe observer survived: '+$v.Name)};$mutants++}
"PASS exact Capture prototype actual_cases=$actual executed_unsafe=$mutants host=$($PSVersionTable.PSVersion) runtime=NOT_RUN compile=NOT_RUN"
