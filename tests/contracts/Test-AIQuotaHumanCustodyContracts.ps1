param([string]$RepositoryRoot=(Split-Path -Parent (Split-Path -Parent $PSScriptRoot)),[string]$BeforeService)
$ErrorActionPreference='Stop'
$adapter=[IO.File]::ReadAllText((Join-Path $PSScriptRoot 'Test-AIQuotaVehicleBackendContracts.ps1'))
$cut=$adapter.IndexOf("foreach(`$file in @('AICF_AIQuotaVehicleOwner.c'")
if($cut-lt0){throw 'Actual backend adapter boundary missing'}
$prefix=[regex]::Replace($adapter.Substring(0,$cut),'\Aparam\([^\r\n]+\)\r?\n','')
$humanScriptRoot=$PSScriptRoot
Invoke-Expression ($prefix.Replace('$PSScriptRoot','$humanScriptRoot'))
if($BeforeService){
 $before=[IO.File]::ReadAllText($BeforeService)
 $old=Get-AICFMethodBody ([pscustomobject]@{Source=$before;Code=(ConvertTo-AICFCodeText $before)}) 'BuildCounts'
 $actualBody=Convert-VehicleReplay (ServiceBody 'BuildCounts')
 if(!$replay.Contains($actualBody)){throw 'Actual BuildCounts substitution missing'}
 $replay=$replay.Replace($actualBody,(Convert-VehicleReplay $old))
}
$replay=$replay.Replace('QuotaVehicleBackendReplay','QuotaHumanCustodyReplay')
# Native weak objects can expire while their C# adapter reference is still held.
$replay=$replay.Replace('public class Native { public static implicit operator bool(Native value){return value!=null;} }','public class Native { public bool Exists=true;public static implicit operator bool(Native value){return value!=null&&value.Exists;} }')
$replay=$replay.Replace('public int GetPlayerId(){return Id;}','public int GetPlayerId(){if(!Exists)throw new Exception("expired player controller dereference");return Id;}')
$replay=$replay.Replace('public int GetPlayerIdFromControlledEntity(IEntity e){if(OnLookup!=null)OnLookup();return PlayerId;}','public int GetPlayerIdFromControlledEntity(IEntity e){if(!Exists)throw new Exception("expired PM dereference");if(OnLookup!=null)OnLookup();return PlayerId;}')
$replay=$replay.Replace('public AIAgent GetControlAIAgent(){return Agent;}','public AIAgent GetControlAIAgent(){if(!Exists)throw new Exception("deleted native control component dereference");return Agent;}')
$replay+=@'
public class AICF_AIWorldQuotaService:QuotaBase {
public override bool ClassifyQuotaVehicleRow(AICF_AIQuotaAgentRow row,out int bucket,out bool charge){bucket=5;charge=false;return true;}
public override bool AddQuotaVehicleHolds(AICF_AIQuotaCensus s,AICF_QuotaInts counts,EArray<IEntity> counted){return true;}
public override bool InitializeQuotaVehicleBindings(AICF_AIQuotaCensus s){return true;}}
public static class HumanCases {
static int N;static long Id;static AICF_AIWorldQuotaService S;static ChimeraCharacter A;static AICF_AIQuotaBody B;static PlayerController P;
static void Need(bool ok,string name){N++;if(!ok)throw new Exception(name);}
static T Add<T>(T e)where T:IEntity{e.Id=++Id;e.World=Runtime.Game.World;Runtime.Game.World.Entities[e.Id]=e;return e;}
static void Setup(){
Replication.Server=true;Runtime.Game=new Game();SCR_PossessingManagerComponent.MainId=0;Id=100;AICF_Stage1Diagnostics.Events.Clear();
S=new AICF_AIWorldQuotaService();S.m_World=Add(new ChimeraAIWorld());S.m_Controller=new AICF_MatchController{World=S.m_World};S.m_iEpoch=1;S.m_Owners.UnownedStock=true;
A=Add(new ChimeraCharacter());A.Control.Agent=Add(new AIAgent{Actor=A});S.m_World.Agents.Add(A.Control.Agent);
S.m_Limits=new AICF_AIQuotaLimits();if(!S.m_Limits.CaptureStockSnapshot(128,0,0)||!S.m_Limits.Configure(220,0,20,0))throw new Exception("limits");S.m_Ledger=new AICF_AIQuotaLedger(S.m_Limits,1);
AICF_QuotaInts counts;AICF_AIQuotaCensus census;if(!Read(out counts,out census)||counts[0]!=1||!S.m_Ledger.ApplyCensus(counts,true)||!S.InitializeQuotaCharacterBindings(census))throw new Exception("actual pre-human census seed");
B=S.m_aBodies[0];P=new PlayerController{Id=1,Actor=A};
}
static void Human(){A.Rpl.Master=false;A.Controller.Player=true;Runtime.Game.Players.PlayerId=1;Runtime.Game.Players.Controller=P;SCR_PossessingManagerComponent.MainId=1;}
static bool Read(out AICF_QuotaInts counts,out AICF_AIQuotaCensus c){counts=new AICF_QuotaInts();c=new AICF_AIQuotaCensus();return c.Capture(S.m_World)&&S.BuildCounts(c,counts);}
static AICF_AIQuotaCensus Snapshot(){var c=new AICF_AIQuotaCensus();if(!c.Capture(S.m_World))throw new Exception("snapshot");return c;}
static void Rejected(Action mutate,string reason){Setup();Human();mutate();AICF_QuotaInts counts;AICF_AIQuotaCensus c;Need(!Read(out counts,out c)&&S.m_aBodies.Count()==1&&!B.m_bReleased&&S.m_Ledger.GetBindingBucket(B.Key())==0,reason);}
public static int Run(){
N=0;AICF_QuotaInts counts;AICF_AIQuotaCensus c;
Setup();Human();Need(!B.MatchesIdentity(),"AI identity remains master strict");Need(Read(out counts,out c)&&counts[0]==0&&S.m_aBodies.Count()==0&&B.m_bReleased,"actual native human proxy transition retires only old stock custody (old BuildCounts fails)");
Need(S.m_Ledger.GetPhysical(0)==1&&S.m_Ledger.GetBindingBucket(B.Key())==-1,"retire removes binding, never decrements raw/physical census prematurely");Need(S.m_Ledger.ApplyCensus(counts,true)&&S.m_Ledger.GetPhysical(0)==0,"fresh census alone releases physical capacity");
Need(Runtime.Game.World.FindEntityByID(A.Id)==A&&P.GetControlledEntity()==A&&!A.Control.Agent.Activated&&!A.Control.Agent.Counted,"actor/player/AI flags untouched");
Need(AICF_Stage1Diagnostics.Events.Exists(x=>x.StartsWith("AI_QUOTA_HUMAN_BINDING_RETIRED")),"typed positive receipt");
Rejected(()=>A.Controller.Player=false,"proxy and PM identity without native player flag rejected");
Rejected(()=>Runtime.Game.Players.PlayerId=0,"missing authoritative PM owner rejected");
Rejected(()=>SCR_PossessingManagerComponent.MainId=2,"different possessing player rejected");
Rejected(()=>Runtime.Game.Players.Controller=null,"missing exact player controller rejected");
Rejected(()=>P.Actor=new ChimeraCharacter(),"foreign controlled body rejected");
Rejected(()=>P.Id=2,"different player-controller id rejected");
Rejected(()=>A.Rpl=new RplComponent{Master=false},"same Rpl numeric id on replaced component rejected");
Rejected(()=>A.Rpl.R.Value++,"changed captured Rpl id rejected");
Rejected(()=>B.m_GroupOwner=new AICF_AIQuotaGroupOwner(),"typed group owner never retired by stock bridge");
Rejected(()=>B.m_CreationGuard=new AICF_AIQuotaCreationGuard(),"external creation obligation never retired");
Rejected(()=>B.m_Bucket=1,"managed combat body remains UNKNOWN");
Rejected(()=>B.m_bQuarantined=true,"old quarantine never cleared");
Rejected(()=>A.Control.Agent.Activated=true,"active current AI agent rejected");
Rejected(()=>A.Control.Agent.Counted=true,"counted current AI row stays charged and UNKNOWN");
Need(AICF_Stage1Diagnostics.Events.Exists(x=>x.Contains("counted=1")&&x.Contains("observation_after_decision=1")),"veto diagnostics expose actual counted flag after negative decision");
Rejected(()=>A.Control.Agent.World=new BaseWorld(),"foreign agent world rejected");
Rejected(()=>Runtime.Game.World.Entities.Remove(A.Control.Agent.Id),"agent current world lookup identity required");
Setup();Human();S.m_World.Agents.Clear();A.Control.Agent=null;Need(Read(out counts,out c)&&counts[0]==0&&B.m_bReleased,"confirmed human with absent agent and no stale registered row");
Setup();Human();c=Snapshot();A.Control.Agent=null;Need(!B.HasConfirmedHumanCustody(c),"absent current agent cannot conceal old registered actor row");
Setup();Human();c=Snapshot();c.Known=false;Need(!S.TryRetireHumanBody(c,B,0)&&!B.m_bReleased,"incomplete census never retires body");
Setup();Human();c=Snapshot();c.Rows[0].m_AgentId++;Need(!B.HasConfirmedHumanCustody(c),"snapshot current-agent id must match captured current agent");
Setup();Human();c=Snapshot();P.OnRead=()=>Runtime.Game.Players=new PlayerManager();Need(!S.TryRetireHumanBody(c,B,0)&&!B.m_bReleased,"PM replacement during native read rejected");
Setup();Human();c=Snapshot();P.OnRead=()=>Runtime.Game.Players.Controller=new PlayerController{Id=1,Actor=A};Need(!S.TryRetireHumanBody(c,B,0)&&!B.m_bReleased,"controller replacement during native read rejected");
Setup();Human();c=Snapshot();P.OnRead=()=>A.Rpl=new RplComponent{Master=false};Need(!S.TryRetireHumanBody(c,B,0)&&!B.m_bReleased,"Rpl replacement during native read rejected");
Setup();Human();c=Snapshot();P.OnRead=()=>P.Exists=false;Need(!S.TryRetireHumanBody(c,B,0)&&!B.m_bReleased,"expired captured player controller rejected without dereference");
Setup();Human();c=Snapshot();P.OnRead=()=>Runtime.Game.Players.Exists=false;Need(!S.TryRetireHumanBody(c,B,0)&&!B.m_bReleased,"expired captured PM rejected without dereference");
Setup();Human();c=Snapshot();P.OnRead=()=>{A.Control.Exists=false;A.Control=null;};Need(!S.TryRetireHumanBody(c,B,0)&&!B.m_bReleased,"native control deletion during initial player callback rejects before GetControlAIAgent");
Setup();Human();c=Snapshot();c.OnRow=i=>{c.OnRow=null;A.Control.Agent.Exists=false;};Need(!S.TryRetireHumanBody(c,B,0)&&!B.m_bReleased,"present agent expiry cannot become absent-agent positive");
Setup();Human();c=Snapshot();c.OnRow=i=>{c.OnRow=null;A.Control.Exists=false;};Need(!S.TryRetireHumanBody(c,B,0)&&!B.m_bReleased,"captured control component expiry rejects");
Setup();Human();c=Snapshot();c.OnRow=i=>{c.OnRow=null;A.Control.Agent=Add(new AIAgent{Actor=A});};Need(!S.TryRetireHumanBody(c,B,0)&&!B.m_bReleased,"current AI agent replacement during row proof rejected");
Setup();Human();c=Snapshot();c.OnRow=i=>{c.OnRow=null;A.Control.Agent.Counted=true;};Need(!S.TryRetireHumanBody(c,B,0)&&!B.m_bReleased,"new counted obligation during row proof rejected");
Setup();Human();c=Snapshot();var oldLedger=S.m_Ledger;c.OnRow=i=>{c.OnRow=null;S.m_Ledger=new AICF_AIQuotaLedger(S.m_Limits,1);};Need(!S.TryRetireHumanBody(c,B,0)&&oldLedger.GetBindingBucket(B.Key())==0&&!B.m_bReleased,"replaced ledger preserves original binding");
Setup();Human();c=Snapshot();c.OnRow=i=>{c.OnRow=null;S.m_Controller=new AICF_MatchController{World=S.m_World};};Need(!S.TryRetireHumanBody(c,B,0)&&!B.m_bReleased,"replaced controller context rejected");
Setup();Human();c=Snapshot();c.OnRow=i=>{c.OnRow=null;S.m_iEpoch++;};Need(!S.TryRetireHumanBody(c,B,0)&&!B.m_bReleased,"replaced epoch rejected");
Setup();Human();c=Snapshot();c.OnRow=i=>{c.OnRow=null;S.m_aBodies[0]=new AICF_AIQuotaBody();};Need(!S.TryRetireHumanBody(c,B,0)&&!B.m_bReleased&&S.m_aBodies.Count()==1,"replaced body record not removed");
Setup();Human();c=Snapshot();c.OnRow=i=>{c.OnRow=null;S.m_WorldState.Last++;S.m_WorldState.Epoch=0;};Need(!S.TryRetireHumanBody(c,B,0)&&!B.m_bReleased,"successor session then End ABA rejected by monotonic world epoch");
Setup();Human();c=Snapshot();c.OnRow=i=>{c.OnRow=null;S.m_Controller.Active=false;S.m_WorldState.Epoch=0;};Need(S.TryRetireHumanBody(c,B,0)&&B.m_bReleased&&!S.IsOwnerCurrent(),"ending same-world root does not block confirmed physical bookkeeping");
Setup();Human();S.m_Controller=null;S.m_iEpoch=0;S.m_WorldState.Epoch=0;c=Snapshot();Need(S.TryRetireHumanBody(c,B,0)&&B.m_bReleased,"pre-BeginSession world-scoped confirmed custody may retire");
return N;}}
}
'@
Add-Type $replay
$actual=[QuotaHumanCustodyReplay.HumanCases]::Run()
$unsafe=0
foreach($mutation in @(
 @('global_master_waiver','m_Rpl && m_Rpl.IsMaster() &&','m_Rpl &&'),
 @('native_player_ignored','!character.IsPlayerControlled()','false'),
 @('rpl_component_ignored','m_Entity.FindComponent(typeof(RplComponent)) == m_Rpl','true'),
 @('record_replacement',' || m_aBodies[index] != body',''),
 @('epoch_replacement',' || m_iEpoch != epoch',''),
 @('successor_ABA','if (worldState.GetLastEpoch() != lastEpoch) return false;',''),
 @('native_control_after_callback','if (!actor || !character || !control || !players || !player) return false;','')
)){
 if(!$replay.Contains($mutation[1])){throw "Actual unsafe anchor missing $($mutation[0])"}
 $unsafe++;$namespace='HumanUnsafe'+$unsafe
 $bad=$replay.Replace('QuotaHumanCustodyReplay',$namespace).Replace($mutation[1],$mutation[2]);Add-Type $bad
 $rejected=$false;try{$null=([type]($namespace+'.HumanCases')).GetMethod('Run').Invoke($null,@())}catch{$rejected=$true}
 if(!$rejected){throw "Actual unsafe human transition survived $($mutation[0])"}
}
"PASS actual human custody=$actual executed_unsafe=$unsafe native=NOT_RUN no_actor_mutation=1"
