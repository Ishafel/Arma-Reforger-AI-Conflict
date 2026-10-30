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
