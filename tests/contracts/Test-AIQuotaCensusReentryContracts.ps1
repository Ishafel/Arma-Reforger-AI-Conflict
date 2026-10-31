param([string]$RepositoryRoot=(Split-Path -Parent (Split-Path -Parent $PSScriptRoot)))
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot '../lib/Stage3StaticAudit.Common.ps1')
$source=[IO.File]::ReadAllText((Join-Path $RepositoryRoot 'AIConflictCore/Scripts/Game/AIConflict/Forces/AICF_AIWorldQuotaService.c'))
function Extract([string]$name){Get-AICFMethodBody ([pscustomobject]@{Source=$source;Code=(ConvertTo-AICFCodeText $source)}) $name}
$methods='public bool IsQuotaAdmissionBusy()'+(Extract 'IsQuotaAdmissionBusy')
$methods+='public bool Initialize(object controller,int managedUnits,int builderUnits,int fleetUnits,int fiaUnits)'+(Extract 'Initialize')
$methods+='public bool RefreshCensus()'+(Extract 'RefreshCensus')
$preamble=@'
using System;
public class Service {public bool m_bNativeBirthBusy,m_bQuotaCensusBusy,Result=true;public int Calls;public Action<Service> Callback;
public bool InitializeCurrent(object root,int a,int b,int c,int d){return Inner();}public bool RefreshCurrentCensus(){return Inner();}
private bool Inner(){Calls++;var cb=Callback;Callback=null;if(cb!=null)cb(this);return Result;}
'@
$cases=@'
}
public static class Cases {public static int Execute(){int n=0;Action<bool,string> need=(ok,label)=>{if(!ok)throw new Exception(label);n++;};
var s=new Service();bool nested=true,seen=false;s.Callback=owner=>{seen=owner.IsQuotaAdmissionBusy();nested=owner.RefreshCensus();};need(s.Initialize(new object(),220,8,20,0)&&seen&&!nested&&s.Calls==1&&!s.IsQuotaAdmissionBusy(),"initialize blocks nested census until all bindings complete");
s=new Service();s.Callback=owner=>{seen=owner.IsQuotaAdmissionBusy();nested=owner.Initialize(new object(),220,8,20,0);};need(s.RefreshCensus()&&seen&&!nested&&s.Calls==1&&!s.IsQuotaAdmissionBusy(),"refresh blocks nested initialization");
s=new Service{Result=false};need(!s.RefreshCensus()&&!s.IsQuotaAdmissionBusy(),"failed capture balances guard");need(!s.Initialize(new object(),220,8,20,0)&&!s.IsQuotaAdmissionBusy(),"failed init balances guard");
s=new Service{m_bNativeBirthBusy=true};need(!s.RefreshCensus()&&s.Calls==0&&s.m_bNativeBirthBusy,"native birth not reclassified during pending factory");need(!s.Initialize(new object(),220,8,20,0)&&s.Calls==0&&s.m_bNativeBirthBusy,"initialize cannot erase factory owner");
s=new Service{m_bQuotaCensusBusy=true};need(!s.RefreshCensus()&&s.m_bQuotaCensusBusy&&s.Calls==0,"nested callback cannot clear outer census guard");need(!s.Initialize(new object(),220,8,20,0)&&s.m_bQuotaCensusBusy&&s.Calls==0,"nested init cannot clear outer guard");
s=new Service();need(s.RefreshCensus()&&s.RefreshCensus()&&s.Calls==2&&!s.IsQuotaAdmissionBusy(),"sequential legitimate refresh preserved");return n;}}
'@
$code=$preamble+$methods+$cases
Add-Type $code
$actual=[Cases]::Execute()
$unsafe=0
foreach($mutation in @(
 @('guard-not-entered','m_bQuotaCensusBusy = true;',''),
 @('guard-not-balanced','m_bQuotaCensusBusy = false;',''),
 @('native-busy-ignored','m_bNativeBirthBusy || m_bQuotaCensusBusy','m_bQuotaCensusBusy')
)){
 if(!$code.Contains($mutation[1])){throw "Missing census reentry target $($mutation[0])"}
 $name='CensusUnsafe'+[guid]::NewGuid().ToString('N')
 $mutated=$code.Replace($mutation[1],$mutation[2]).Replace('public class Service {',"namespace $name {public class Service {")+'}'
 Add-Type $mutated
 $failed=$false;try{$null=([type]("$name.Cases")).GetMethod('Execute').Invoke($null,@())}catch{$failed=$true}
 if(!$failed){throw "Unsafe census reentry survived $($mutation[0])"};$unsafe++
}
"PASS AI quota actual census reentry=$actual executed_unsafe=$unsafe runtime=NOT_RUN"
