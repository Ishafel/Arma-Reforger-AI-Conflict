param([string]$RepositoryRoot=(Split-Path -Parent (Split-Path -Parent $PSScriptRoot)))
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot '../lib/Stage3StaticAudit.Common.ps1')
$source=[IO.File]::ReadAllText((Join-Path $RepositoryRoot 'AIConflictCore/Scripts/Game/AIConflict/Forces/AICF_AIQuotaStartup.c'))
function Body([string]$text,[string]$name){Get-AICFMethodBody ([pscustomobject]@{Source=$text;Code=(ConvertTo-AICFCodeText $text)}) $name}
function Rules([string]$text){
 $capture=Body $text 'Capture';$current=Body $text 'IsCurrent';$check=Body $text 'Check';$cancel=Body $text 'Cancel';$retry=Body $text 'Retry';$schedule=Body $text 'Schedule'
 foreach($guard in @('m_Controller.IsAIQuotaStartupCurrent(this)','m_Campaign.IsMaster()','GetGame().GetGameMode() != m_Campaign','GetGame().GetWorld() != m_MainWorld','GetGame().GetAIWorld() != m_World','m_WorldState.IsCurrentWorld()','m_Manager.GetOwner() == m_Campaign')){if(!$current.Contains($guard)){'CURRENT_NATIVE_OWNER';break}}
 if(!$capture.Contains('world.AICF_GetQuotaWorldState()')-or!$capture.Contains('GetOriginalNativeLimit()')){'CAPTURE_ORIGINAL_WORLD_FIRST'}
 if(!$text.Contains('protected ref AICF_Stage1Config m_Config;')-or!$text.Contains('protected ref AICF_ContentProfile m_Content;')){'RETRY_RETAINS_IMMUTABLE_INPUTS'}
 if(!$check.Contains('AICF_IsNativeQuotaQueueEmpty()')-or!$check.Contains('AICF_AIWorldQuotaService.GetAuthorityForWorld(m_MainWorld)')-or!$check.Contains('IsWithinWait(elapsedMs)')){'QUEUE_DRAIN_BOUNDED'}
 if(!$schedule.Contains('CallLater(Retry, RETRY_INTERVAL_MS, false)')-or!$cancel.Contains('Remove(Retry)')-or!$cancel.Contains('m_bCancelled = true')){'BALANCED_SINGLE_RETRY'}
 if(!$retry.Contains('if (!IsCurrent()) { Cancel(); return; }')-or!$retry.Contains('m_Controller.Start(m_Campaign, m_Config, m_Content, m_sMapKey)')){'RETRY_EXACT_START_ONLY'}
 if($text-match'\.(?:SetLimitOfActiveAIs|Spawn\w*|Delete\w*|RemoveItem|Clear|ActivateAI|SetHealth\w*)\s*\('){'NO_NATIVE_POLICY_OR_PAID_QUEUE_MUTATIONS'}
}
if(@(Rules $source).Count){throw "Startup source failed: $(Rules $source)"}
function CompileActual([string]$text){
 $code=$text.Replace('enum AICF_EAIQuotaStartup','public enum AICF_EAIQuotaStartup').Replace('class AICF_AIQuotaStartup','public class AICF_AIQuotaStartup:N').Replace('static const int','public const int').Replace('protected ref ','public ').Replace('protected ','public ')
 $code=$code.Replace('static AICF_AIQuotaStartup Capture','public static AICF_AIQuotaStartup Capture').Replace('string GetLastReason()','public string GetLastReason()').Replace('bool IsCurrent()','public bool IsCurrent()').Replace('static bool IsWithinWait','public static bool IsWithinWait').Replace('AICF_EAIQuotaStartup Check()','public AICF_EAIQuotaStartup Check()').Replace('void Cancel()','public void Cancel()').Replace('void ~AICF_AIQuotaStartup()','public void Destroy()')
 $code=$code.Replace('.FindComponent(SCR_SpawnerAIGroupManagerComponent)','.FindComponent(typeof(SCR_SpawnerAIGroupManagerComponent))').Replace('System.GetTickCount','Clock.GetTickCount')
 $code=[regex]::Replace($code,'%([1-9])',{param($m)'{'+([int]$m.Groups[1].Value-1)+'}'})
 $adapters=@'
using System;
public class N {public static implicit operator bool(N value){return !Object.ReferenceEquals(value,null);}public static Game GetGame(){return Env.Game;}}
public static class Clock {public static int Now;public static int GetTickCount(){return Now;}public static int GetTickCount(int before){return Now-before;}}
public static class Replication {public static bool Server=true;public static bool IsServer(){return Server;}}
public class BaseWorld:N {}
public class Queue:N {public Action Pending;public int Inserts,Removes;public void CallLater(Action action,int milliseconds,bool repeat){if(repeat||Pending!=null)throw new Exception("duplicate or repeating startup callback");Pending=action;Inserts++;}public void Remove(Action action){if(Pending==action)Pending=null;Removes++;}public void IgnoreRemove(Action action){}public void Fire(){Action action=Pending;Pending=null;if(action!=null)action();}}
public class Game:N {public bool Playing=true;public BaseWorld World=new BaseWorld();public SCR_AIWorld AIWorld;public SCR_GameModeCampaign Campaign;public Queue Queue=new Queue();public bool InPlayMode(){return Playing;}public BaseWorld GetWorld(){return World;}public SCR_AIWorld GetAIWorld(){return AIWorld;}public SCR_GameModeCampaign GetGameMode(){return Campaign;}public Queue GetCallqueue(){return Queue;}}
public class AICF_Stage1Config:N {} public class AICF_ContentProfile:N {}
public class AICF_AIQuotaWorldState:N {public bool Current=true;public int Original=128;public bool IsCurrentWorld(){return Current;}public int GetOriginalNativeLimit(){return Original;}}
public class SCR_AIWorld:N {public BaseWorld World;public int NativeLimit=128,StateReads;public AICF_AIQuotaWorldState State=new AICF_AIQuotaWorldState();public static SCR_AIWorld Cast(SCR_AIWorld world){return world;}public BaseWorld GetWorld(){return World;}public AICF_AIQuotaWorldState AICF_GetQuotaWorldState(){StateReads++;return State;}public int GetLimitOfActiveAIs(){return NativeLimit;}}
public class SCR_GameModeCampaign:N {public bool Master=true;public SCR_SpawnerAIGroupManagerComponent Manager;public bool IsMaster(){return Master;}public SCR_SpawnerAIGroupManagerComponent FindComponent(Type type){return Manager;}}
public class SCR_SpawnerAIGroupManagerComponent:N {public SCR_GameModeCampaign Owner;public bool Empty=true;public int Reads;public Action OnRead;public static SCR_SpawnerAIGroupManagerComponent Cast(SCR_SpawnerAIGroupManagerComponent manager){return manager;}public SCR_GameModeCampaign GetOwner(){return Owner;}public bool AICF_IsNativeQuotaQueueEmpty(){Reads++;if(OnRead!=null)OnRead();return Empty;}}
public class AICF_AIWorldQuotaService:N {public static bool Active;public static Action OnRead;public static AICF_AIWorldQuotaService GetAuthorityForWorld(BaseWorld world){if(OnRead!=null)OnRead();return Active?new AICF_AIWorldQuotaService():null;}}
public static class AICF_Stage1Diagnostics {public static int Logs;public static void Info(string name,string details){Logs++;}}
public class AICF_MatchController:N {public AICF_AIQuotaStartup Pending;public bool Started,Stopped,ActiveOwner=true;public int Starts;public AICF_Stage1Config LastConfig;public AICF_ContentProfile LastContent;public string LastMap;public bool IsAIQuotaStartupCurrent(AICF_AIQuotaStartup startup){return Pending==startup&&!Started&&!Stopped&&ActiveOwner;}public void Start(SCR_GameModeCampaign campaign,AICF_Stage1Config config,AICF_ContentProfile content,string map){Starts++;LastConfig=config;LastContent=content;LastMap=map;}}
public static class Env {public static Game Game;public static AICF_MatchController Owner;public static AICF_Stage1Config Config;public static AICF_ContentProfile Content;public static AICF_AIQuotaStartup Fresh(){Game=new Game();Game.AIWorld=new SCR_AIWorld{World=Game.World};Game.Campaign=new SCR_GameModeCampaign();Game.Campaign.Manager=new SCR_SpawnerAIGroupManagerComponent{Owner=Game.Campaign};Clock.Now=1000;Replication.Server=true;AICF_AIWorldQuotaService.Active=false;AICF_AIWorldQuotaService.OnRead=null;Owner=new AICF_MatchController();Config=new AICF_Stage1Config();Content=new AICF_ContentProfile();var s=AICF_AIQuotaStartup.Capture(Owner,Game.Campaign,Config,Content,"Arland");Owner.Pending=s;return s;}}
__SOURCE__
public static class Run {public static int Execute(){int count=0;Action<bool,string> check=(ok,label)=>{if(!ok)throw new Exception(label);count++;};var s=Env.Fresh();check(s!=null&&s.IsCurrent(),"captured current startup");check(Env.Game.AIWorld.StateReads==1&&Env.Game.AIWorld.NativeLimit==128,"readonly original capture before limit");check(s.Check()==AICF_EAIQuotaStartup.READY&&Env.Game.Queue.Inserts==0,"empty native queue ready without retry");s.Cancel();check(Env.Game.Queue.Pending==null,"ready owner cleanup balanced");
s=Env.Fresh();Env.Game.Campaign.Manager.Empty=false;check(s.Check()==AICF_EAIQuotaStartup.WAIT&&Env.Game.Queue.Inserts==1,"paid queue waiting single callback");check(s.Check()==AICF_EAIQuotaStartup.WAIT&&Env.Game.Queue.Inserts==1,"repeat Check cannot duplicate callback");Clock.Now+=1000;Env.Game.Queue.Fire();check(Env.Owner.Starts==1&&Env.Owner.LastConfig==Env.Config&&Env.Owner.LastContent==Env.Content&&Env.Owner.LastMap=="Arland","retry same owner and retained immutable args");check(!s.m_bScheduled,"one-shot callback cleared");check(s.Check()==AICF_EAIQuotaStartup.WAIT&&Env.Game.Queue.Inserts==2,"next retry requested by fresh Check only");Env.Game.Campaign.Manager.Empty=true;check(s.Check()==AICF_EAIQuotaStartup.READY,"old paid queue drained naturally");s.Cancel();check(Env.Game.Queue.Pending==null&&Env.Game.AIWorld.NativeLimit==128,"cancel pending retry and no floor mutation");
Action<Action<AICF_AIQuotaStartup>,string> reject=(change,label)=>{var x=Env.Fresh();change(x);check(!x.IsCurrent(),label);};reject(x=>Env.Owner.Stopped=true,"stopped root");reject(x=>Env.Owner.Started=true,"already started root");reject(x=>Env.Owner.Pending=new AICF_AIQuotaStartup(),"successor startup");reject(x=>Env.Owner.ActiveOwner=false,"another live controller");reject(x=>Env.Game.World=new BaseWorld(),"main world replaced");reject(x=>Env.Game.AIWorld=new SCR_AIWorld(),"AI world replaced");reject(x=>Env.Game.Campaign=new SCR_GameModeCampaign(),"campaign replaced");reject(x=>Env.Game.Campaign.Master=false,"authority lost");reject(x=>Env.Game.Campaign.Manager=new SCR_SpawnerAIGroupManagerComponent(),"queue manager replaced");reject(x=>Env.Game.AIWorld.State.Current=false,"world capture stale");reject(x=>Replication.Server=false,"client");
s=Env.Fresh();Env.Game.Campaign.Manager.Empty=false;s.Check();Env.Owner.Stopped=true;Env.Game.Queue.Fire();check(Env.Owner.Starts==0&&s.m_bCancelled,"stale retry cannot Start");s=Env.Fresh();Env.Game.Campaign.Manager.Empty=false;s.Check();s.Destroy();check(Env.Game.Queue.Pending==null&&s.m_bCancelled,"destructor uses same Cancel");
s=Env.Fresh();Env.Game.Campaign.Manager.Empty=false;Clock.Now=s.m_iStartedAtMs+119999;check(s.Check()==AICF_EAIQuotaStartup.WAIT,"last permitted millisecond");Clock.Now++;check(s.Check()==AICF_EAIQuotaStartup.BLOCKED&&Env.Game.Queue.Pending==null,"strict120s boundary without queue drop");s=Env.Fresh();Clock.Now=s.m_iStartedAtMs-1;check(s.Check()==AICF_EAIQuotaStartup.BLOCKED,"clock regression fail closed");
s=Env.Fresh();Env.Game.Campaign.Manager.Empty=false;AICF_AIWorldQuotaService.Active=true;Env.Game.AIWorld.NativeLimit=504;check(s.Check()==AICF_EAIQuotaStartup.READY&&Env.Game.Campaign.Manager.Reads==0&&Env.Game.AIWorld.State.Original==128,"existing protected world policy no adoption/ratchet");
s=Env.Fresh();Env.Game.Campaign.Manager.OnRead=()=>Env.Owner.Pending=new AICF_AIQuotaStartup();check(s.Check()==AICF_EAIQuotaStartup.BLOCKED&&Env.Game.Queue.Pending==null,"native queue read callback cannot resume successor");s=Env.Fresh();AICF_AIWorldQuotaService.Active=true;AICF_AIWorldQuotaService.OnRead=()=>Env.Game.World=new BaseWorld();check(s.Check()==AICF_EAIQuotaStartup.BLOCKED,"existing-policy read callback repeated identity");return count;}}
'@
 $namespace='AICFStartupActual'+[guid]::NewGuid().ToString('N')
 $result=$adapters.Replace('__SOURCE__',$code).Replace('public class N {',"namespace $namespace {public class N {")+'}'
 Add-Type $result
 return [type]("$namespace.Run")
}
$type=CompileActual $source
$cases=$type.GetMethod('Execute').Invoke($null,@())
$unsafe=0
foreach($mutation in @(
 @('root owner','m_Controller.IsAIQuotaStartupCurrent(this)','true'),
 @('strict deadline','elapsedMs < MAXIMUM_WAIT_MS','elapsedMs <= MAXIMUM_WAIT_MS'),
 @('cancel callback','GetCallqueue().Remove(Retry)','GetCallqueue().IgnoreRemove(Retry)')
)){
 if(!$source.Contains($mutation[1])){throw "Missing unsafe mutation $($mutation[0])"}
 $mutantType=CompileActual ($source.Replace($mutation[1],$mutation[2]));$rejected=$false
 try{$null=$mutantType.GetMethod('Execute').Invoke($null,@())}catch{$rejected=$true}
 if(!$rejected){throw "Unsafe startup survived $($mutation[0])"};$unsafe++
}
"PASS quota startup actual_cases=$cases unsafe_executed=$unsafe original_floor_readonly=1 native_runtime=NOT_RUN"
