param([string]$RepositoryRoot=(Split-Path -Parent (Split-Path -Parent $PSScriptRoot)),[string]$BeforeSource)
$ErrorActionPreference='Stop'
# Reuse native value adapters only, before the vehicle extension/its cases.
# BuildCounts, custody, binding, Body and Ledger remain actual extracted bodies.
$adapter=[IO.File]::ReadAllText((Join-Path $PSScriptRoot 'Test-AIQuotaVehicleBackendContracts.ps1'))
$cut=$adapter.IndexOf("foreach(`$file in @('AICF_AIQuotaVehicleOwner.c'")
if($cut-lt0){throw 'Native adapter boundary absent'}
$prefix=$adapter.Substring(0,$cut)
$prefix=[regex]::Replace($prefix,'\Aparam\([^\r\n]+\)\r?\n','')
$custodyScriptRoot=$PSScriptRoot
$prefix=$prefix.Replace('$PSScriptRoot','$custodyScriptRoot')
Invoke-Expression $prefix
if($BeforeSource){
 $old=[IO.File]::ReadAllText($BeforeSource)
 $oldBody=Get-AICFMethodBody ([pscustomobject]@{Source=$old;Code=(ConvertTo-AICFCodeText $old)}) 'BuildCounts'
 $currentBody=Convert-VehicleReplay (ServiceBody 'BuildCounts')
 $oldBody=Convert-VehicleReplay $oldBody
 if(!$replay.Contains($currentBody)){throw 'Actual BuildCounts replay not found'}
 $replay=$replay.Replace($currentBody,$oldBody)
}
$replay=$replay.Replace('QuotaVehicleBackendReplay','QuotaCharacterCustodyReplay')
$replay+=@'
public class AICF_AIWorldQuotaService:QuotaBase
{
 public override bool ClassifyQuotaVehicleRow(AICF_AIQuotaAgentRow row,out int bucket,out bool charge){bucket=5;charge=false;return true;}
 public override bool AddQuotaVehicleHolds(AICF_AIQuotaCensus s,AICF_QuotaInts counts,EArray<IEntity> counted){return true;}
 public override bool InitializeQuotaVehicleBindings(AICF_AIQuotaCensus s){return true;}
}
public static class CharacterCases
{
 static int N;static AICF_AIWorldQuotaService S;static SCR_AIGroup G;static ChimeraCharacter A;static long Next;
 static void Need(bool ok,string name){N++;if(!ok)throw new Exception(name);}
 static T Add<T>(T e)where T:IEntity {e.Id=++Next;e.World=Runtime.Game.World;Runtime.Game.World.Entities[e.Id]=e;return e;}
 static void Setup(bool counted=false,bool active=false)
 {
  Replication.Server=true;SCR_PossessingManagerComponent.MainId=0;Runtime.Game=new Game();Next=1;
  S=new AICF_AIWorldQuotaService();S.m_World=Add(new ChimeraAIWorld());G=Add(new SCR_AIGroup());A=Add(new ChimeraCharacter());
  A.Control.Agent=Add(new AIAgent{Actor=A,Group=G,Counted=counted,Activated=active});S.m_World.Agents.Add(A.Control.Agent);
  S.m_Limits=new AICF_AIQuotaLimits();Need(S.m_Limits.CaptureStockSnapshot(128,0,0)&&S.m_Limits.Configure(220,0,20,0),"configured original stock128 preserved");S.m_Ledger=new AICF_AIQuotaLedger(S.m_Limits,1);
 }
 static bool Read(out AICF_QuotaInts counts,out AICF_AIQuotaCensus census)
 {counts=new AICF_QuotaInts();census=new AICF_AIQuotaCensus();return census.Capture(S.m_World)&&S.BuildCounts(census,counts);}
 static void Apply(AICF_QuotaInts counts,AICF_AIQuotaCensus census)
 {Need(S.m_Ledger.ApplyCensus(counts,true)&&S.InitializeQuotaCharacterBindings(census),"fresh zero-increment exact body binding");}
 public static int Run()
 {
  N=0;AICF_QuotaInts counts;AICF_AIQuotaCensus census;
  Setup();Need(Read(out counts,out census)&&counts[1]==1,"existing ALIVE uncounted inactive body costs1 BEFORE future native activation");Apply(counts,census);
  Need(S.m_aBodies.Count()==1&&S.m_Ledger.GetPhysical(1)==1&&S.m_Ledger.GetReserved(1)==0,"actual existing body neither free nor double reserved");
  Need(S.InitializeQuotaCharacterBindings(census)&&S.m_aBodies.Count()==1,"repeated binding deduplicates same exact body");
  A.Control.Agent.Counted=true;A.Control.Agent.Activated=true;Need(Read(out counts,out census)&&counts[1]==1,"late native activation replaces hold exactly1");Apply(counts,census);
  A.Control.Agent.Counted=false;A.Control.Agent.Activated=false;Need(Read(out counts,out census)&&counts[1]==1,"inactive ALIVE never releases unit");
  S.m_World.Agents.Clear();Need(Read(out counts,out census)&&counts[1]==1,"registered-agent disappearance retains actual living body custody");
  A.Controller.Life=ECharacterLifeState.INCAPACITATED;Need(Read(out counts,out census)&&counts[1]==1,"incap body retains future activation room");
  A.Controller.Life=ECharacterLifeState.DEAD;Need(Read(out counts,out census)&&counts[1]==0&&S.m_aBodies.Count()==0,"nativeDEAD inactive releases binding");
  Setup();A.Controller.Life=ECharacterLifeState.DEAD;Need(Read(out counts,out census)&&counts[1]==0,"dead uncounted body not seeded");
  Setup();A.Controller.Player=true;Need(Read(out counts,out census)&&counts[1]==0,"current human does not get AI body hold");
  Setup();Runtime.Game.Players.PlayerId=12;Need(Read(out counts,out census)&&counts[1]==0,"PlayerManager ownership veto independent of nativeplayer flag");
  Setup();SCR_PossessingManagerComponent.MainId=12;Need(Read(out counts,out census)&&counts[1]==0,"possessing owner veto independent of nativeplayer flag");
  Setup();A.Control.Agent=new AIAgent();Need(!Read(out counts,out census),"foreign control agent fails census closed");
  Setup();A.Controller=null;Need(!Read(out counts,out census),"missing actual life state is UNKNOWN not free unit");
  Setup();A.Controller.Life=999;Need(!Read(out counts,out census),"unknown life state cannot masqueradeDEAD");
  Setup();Need(Read(out counts,out census),"actual existing body capture");Apply(counts,census);A.Rpl.R.Value++;
  Need(!Read(out counts,out census),"replaced captured component Rpl cannot free or reuse old body");
  Setup();Need(Read(out counts,out census),"actual before human proxy transition capture");Apply(counts,census);A.Controller.Player=true;Runtime.Game.Players.PlayerId=1;A.Rpl.Master=false;
  Need(!Read(out counts,out census)&&S.m_aBodies.Count()==1&&S.m_Ledger.GetPhysical(1)==1,"conditional AI-to-human proxy transition is UNKNOWN and retains original charged custody; native topology NOT RUN");
  Setup();Need(Read(out counts,out census),"actual existing absence capture");Apply(counts,census);S.m_World.Agents.Clear();Runtime.Game.World.Entities.Remove(A.Id);S.m_aBodies[0].m_Entity=null;
  Need(Read(out counts,out census)&&counts[1]==0&&S.m_aBodies.Count()==0,"exact world absence releases bound existing body");
  Setup();Need(Read(out counts,out census),"actual existing registered count");Apply(counts,census);S.m_World.Agents.Add(Add(new AIAgent()));
  Need(Read(out counts,out census)&&counts[1]==1,"empty uncounted controller costs0 without deleting existingbody");
  return N;
 }
}
}
'@
Add-Type $replay
$actual=[QuotaCharacterCustodyReplay.CharacterCases]::Run()
$unsafe=0
foreach($variant in @(
 @('SkipInactive','if (!row.m_bCounted && !characterHold) continue;','if (!row.m_bCounted) continue;'),
 @('NoLivingHold','hold = !human && life != ECharacterLifeState.DEAD;','hold = false;'),
 @('HumanHold','hold = !human && life != ECharacterLifeState.DEAD;','hold = life != ECharacterLifeState.DEAD;'),
 @('ReleaseInactiveAlive','body.IsDefinitelyGone() || body.IsNativeDeadAndInactive()','body.IsDefinitelyGone() || true'),
 @('NoBodySeed','m_aBodies.Insert(body);','')
)){
 if(!$replay.Contains($variant[1])){throw "Unsafe target missing $($variant[0])"}
 $ns='CharacterUnsafe'+$variant[0]
 $mutated=$replay.Replace('QuotaCharacterCustodyReplay',$ns).Replace($variant[1],$variant[2])
 Add-Type $mutated
 $caught=$false;try{[void]([type]("$ns.CharacterCases")).GetMethod('Run').Invoke($null,@())}catch{$caught=$true}
 if(!$caught){throw "Unsafe character custody survived: $($variant[0])"};$unsafe++
}
"PASS quota character custody actual=$actual executed_unsafe=$unsafe raw_native_counters_unchanged=1 native=NOT_RUN"
