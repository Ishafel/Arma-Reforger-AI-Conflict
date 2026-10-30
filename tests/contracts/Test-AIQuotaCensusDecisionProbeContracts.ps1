param([string]$RepositoryRoot=(Split-Path -Parent (Split-Path -Parent $PSScriptRoot)))
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot '../lib/Stage3StaticAudit.Common.ps1')
$source=[IO.File]::ReadAllText((Join-Path $RepositoryRoot 'tests/fixtures/AICF_AIQuotaCensusDecisionProbe.c'))
$data=[pscustomobject]@{Source=$source;Code=(ConvertTo-AICFCodeText $source)}
function Body([string]$Name){Get-AICFMethodBody $data $Name}
function CSharp([string]$Text){
 $Text=$Text.Replace('ref AICF_QuotaCensusDecisionFrame','AICF_QuotaCensusDecisionFrame').Replace('super.','base.').Replace('string.Format','Shim.Format').Replace('System.GetTickCount()','Clock.Now').Replace('EntityID.INVALID','0').Replace('Print(', 'Shim.Print(')
 $Text=[regex]::Replace($Text,'\.Count\(\)','.Count')
 $Text=[regex]::Replace($Text,'foreach \(([^:]+) : ([^)]+)\)','foreach ($1 in $2)')
 $Text=$Text.Replace('array<EntityID> entities = {};','List<int> entities = new List<int>();').Replace('entities.Insert(row.m_EntityId);','entities.Add(row.m_EntityId);')
 $Text=[regex]::Replace($Text,'(?m)^(\s*)(int|bool|string|EntityID|AICF_QuotaCensusDecisionFrame)\s+(\w+);\s*$',{param($m)$value='null';switch($m.Groups[2].Value){int{$value='0'}bool{$value='false'}string{$value='""'}EntityID{$value='0'}};$m.Groups[1].Value+$m.Groups[2].Value+' '+$m.Groups[3].Value+'='+$value+';'})
 $Text=$Text.Replace('kind = body.m_GroupOwner.m_Kind;','kind = (int)body.m_GroupOwner.m_Kind;').Replace('System.GetCLIParam("aicfQuotaCensusDecisionProbe", value)','Cli.GetCLIParam("aicfQuotaCensusDecisionProbe", out value)')
 $Text=$Text.Replace('super.', 'base.').Replace('base.ReadQuotaCharacterCustody(row, hold)','base.ReadQuotaCharacterCustody(row, out hold)').Replace('base.ClassifyRow(row, bucket)','base.ClassifyRow(row, out bucket)').Replace('base.ClassifyCapturedActor(body, parent, bucket)','base.ClassifyCapturedActor(body, parent, out bucket)').Replace('base.ClassifyQuotaVehicleRow(row, bucket, charge)','base.ClassifyQuotaVehicleRow(row, out bucket, out charge)').Replace('base.VehicleCharge(owner, use, body, bucket, charge)','base.VehicleCharge(owner, use, body, out bucket, out charge)')
 return $Text
}
$signatures=[ordered]@{
 RefreshCensus='public override bool RefreshCensus()'
 RefreshCurrentCensus='public override bool RefreshCurrentCensus()'
 BuildCounts='public override bool BuildCounts(AICF_AIQuotaCensus snapshot,List<int> counts)'
 ReadQuotaCharacterCustody='public override bool ReadQuotaCharacterCustody(AICF_AIQuotaAgentRow row,out bool hold)'
 ClassifyRow='public override bool ClassifyRow(AICF_AIQuotaAgentRow row,out AICF_EAIQuotaBucket bucket)'
 ClassifyCapturedActor='public override bool ClassifyCapturedActor(AICF_AIQuotaBody body,SCR_AIGroup parent,out AICF_EAIQuotaBucket bucket)'
 ClassifyQuotaVehicleRow='public override bool ClassifyQuotaVehicleRow(AICF_AIQuotaAgentRow row,out AICF_EAIQuotaBucket bucket,out bool charge)'
 AddQuotaVehicleHolds='public override bool AddQuotaVehicleHolds(AICF_AIQuotaCensus snapshot,List<int> counts,List<IEntity> counted)'
 Capture='public override bool Capture(SCR_AIWorld world)'
 IsCurrent='public override bool IsCurrent()'
}
$actualOverrides=@([regex]::Matches($data.Code,'override\s+(?:protected\s+)?bool\s+(\w+)\(')|ForEach-Object{$_.Groups[1].Value})
if($actualOverrides.Count-ne$signatures.Count){throw 'Observer override inventory differs from original-base pinned list'}
foreach($name in $actualOverrides){if(!$signatures.Contains($name)){throw "Unsupported observer override $name"}}
$decisionPattern='\.(?:IsCurrent|Capture|BuildCounts|RefreshCensus|RefreshCurrentCensus|ClassifyRow|ClassifyCapturedActor|ClassifyQuotaVehicleRow|VehicleCharge|ReadQuotaCharacterCustody|AddQuotaVehicleHolds)\('
$wrappers=@{};$sourceRules=0
foreach($name in $signatures.Keys){
 $basePath='AIConflictCore/Scripts/Game/AIConflict/Forces/AICF_AIWorldQuotaService.c'
 if($name-in@('Capture','IsCurrent')){$basePath='AIConflictCore/Scripts/Game/AIConflict/Forces/AICF_AIQuotaCensus.c'}
 $declared=[IO.File]::ReadAllText((Join-Path $RepositoryRoot $basePath))
 $classStart=$declared.IndexOf('class AICF_')
 $nextModded=$declared.IndexOf('modded class ',$classStart)
 if($nextModded-ge0){$declared=$declared.Substring(0,$nextModded)}
 if($declared-notmatch('(?m)^\s*(?:protected\s+)?bool\s+'+[regex]::Escape($name)+'\(')){throw "Observer has no original-base declaration: $name"}
 $body=Body $name
 if(@([regex]::Matches($body,'super\.'+$name+'\(')).Count-ne1-or@([regex]::Matches($body,'return actual;')).Count-ne1){throw "Nontransparent delegate $name"}
 if(($body.Replace('super.'+$name+'(',''))-match$decisionPattern){throw "Observer repeats decision $name"}
 if($body-match'\.(?:Set\w*|Reserve\w*|Cancel\w*|ApplyCensus|Spawn\w*|Delete\w*|Activate\w*|Interrupt\w*)\('){throw "Observer mutates native/policy $name"}
 $wrappers[$name]=CSharp $body;$sourceRules++
}
foreach($name in @('Begin','End','Want','Write','ScopeLoss','ForService','ForWorld','RowKey','RowData','BodyKey','BodyData','GroupId','NativeAfter','AICF_DecisionContext')){
 if((Body $name)-match$decisionPattern){throw "Observer helper recurses into real decision $name"};$sourceRules++
}
if($source-match'ref\s+AICF_AIQuotaCensus\b|ref\s+AICF_AIQuotaAgentRow\b|ref\s+AICF_AIQuotaBody\b'){throw 'Probe adds a strong snapshot/body/row anchor'}
if(!$source.Contains('MAX_RECORDS = 96')-or!$source.Contains('observations_incomplete=1')-or!$source.Contains('successor_untouched=1')-or!$source.Contains('AFTER_DECISION_NOT_CAPTURED')){throw 'Bound/scope/observation labels missing'}
$helperSignatures=[ordered]@{Enabled='public static bool Enabled()';Begin='public static AICF_QuotaCensusDecisionFrame Begin(AICF_AIWorldQuotaService service,SCR_AIWorld world,string context,string before)';End='public static void End(AICF_QuotaCensusDecisionFrame frame,AICF_QuotaCensusDecisionFrame previous)';ScopeLoss='public static void ScopeLoss(AICF_QuotaCensusDecisionFrame frame)';Want='public static bool Want(AICF_QuotaCensusDecisionFrame frame,string stage,string identity)';Write='public static void Write(AICF_QuotaCensusDecisionFrame frame,string stage,string identity,string details)';ForService='public static AICF_QuotaCensusDecisionFrame ForService(AICF_AIWorldQuotaService service)';ForWorld='public static AICF_QuotaCensusDecisionFrame ForWorld(SCR_AIWorld world)';RowKey='public static string RowKey(AICF_AIQuotaAgentRow row)';RowData='public static string RowData(AICF_AIQuotaAgentRow row)';BodyKey='public static string BodyKey(AICF_AIQuotaBody body)';BodyData='public static string BodyData(AICF_AIQuotaBody body)';GroupId='public static string GroupId(AIGroup group)';NativeAfter='public static string NativeAfter(SCR_AIWorld world)';Stop='public static void Stop()'}
$helpers=@($helperSignatures.Keys|ForEach-Object{$helperSignatures[$_]+(CSharp (Body $_))})-join"`n"
$serviceMethods=@($signatures.Keys|Where-Object{$_-notin@('Capture','IsCurrent')}|ForEach-Object{$signatures[$_]+$wrappers[$_]})-join"`n"
$code=@'
using System;using System.Collections.Generic;using EntityID=System.Int32;
namespace CensusDecisionReplay {
 public class N{public static implicit operator bool(N n){return n!=null;}}
 public class NativeList<T>:List<T>{public void Insert(T v){Add(v);}}
 public static class Clock{public static int Now=10000;}
 public static class Cli{public static string Value="1";public static bool GetCLIParam(string n,out string v){v=Value;return v!="";}}
 public static class Shim{public static List<string> Logs=new List<string>();public static string Format(string f,params object[] a){for(int i=a.Length;i>0;i--)f=f.Replace("%"+i,a[i-1]==null?"NONE":a[i-1].ToString());return f;}public static void Print(string s){Logs.Add(s);}}
 public class IEntity:N{public int Id=11;public virtual int GetID(){return Id;}}
 public class AIGroup:IEntity{} public class SCR_AIGroup:AIGroup{}
 public class SCR_AIWorld:IEntity{public int Reads;public int GetCurrentNumOfActiveAIs(){Reads++;return 5;}public int GetActiveGroupIntendedCount(){Reads++;return 5;}}
 public enum AICF_EAIQuotaBucket{STOCK,COMBAT,BUILDERS,FLEET,FIA_EXTRA,STOCK_VEHICLES,COUNT}
 public class AICF_AIQuotaGroupOwner:N{public int m_GroupId=13,m_iGeneration=1,m_Kind=0;}
 public class AICF_AIQuotaBody:N{public int m_Id=9;public string m_sRpl="R";public AICF_EAIQuotaBucket m_Bucket=AICF_EAIQuotaBucket.COMBAT;public AICF_AIQuotaGroupOwner m_GroupOwner=new AICF_AIQuotaGroupOwner();public bool m_bVehicle,m_bReleased,m_bQuarantined;public string Key(){return m_Id+"/"+m_sRpl;}}
 public class AICF_AIQuotaAgentRow:N{public int m_EntityId=9,m_AgentId=10;public AIGroup m_Group=new AIGroup();public bool m_bCounted=true,m_bActivated=true,m_bVehicle;}
 public class AICF_AIQuotaVehicleOwner:N{public AICF_EAIQuotaBucket m_Bucket=AICF_EAIQuotaBucket.FLEET;public string m_sLease="L1";public bool m_bCensusOnly;}
 public class AICF_AIQuotaVehicleUse:N{public bool m_bActive=true,m_bCounted=true,m_bHasAI=true;}
 public static class AICF_AIQuotaMath{public const int MAX_TOTAL_UNITS=4096;}
 public class AICF_QuotaCensusDecisionFrame:N{public AICF_AIWorldQuotaService m_Service;public SCR_AIWorld m_World;public int m_iSerial;public string m_sContext,m_sBefore;}
 public static class AICF_QuotaCensusDecisionProbe{
 public const int MAX_RECORDS=96;public static AICF_QuotaCensusDecisionFrame s_Frame;public static NativeList<string> s_Keys=new NativeList<string>();public static int s_iSerial,s_iContextAt;public static bool s_bOverflow,s_bScopeLoss,s_bContextIncomplete;public static AICF_AIWorldQuotaService s_ContextService;public static string s_sContext="";__HELPERS__
 }
 public class NativeBase:N{public bool Result;public int Calls,Writes;public object[] Args;public Action During;public void DebugNativeWrite(){Writes++;}protected bool Run(params object[] args){Calls++;Args=args;if(During!=null)During();return Result;}}
 public class NativeCensus:NativeBase{public SCR_AIWorld m_World=new SCR_AIWorld();public string m_sReason="TEST_FALSE";public bool m_bComplete=true;public int m_iActive=5,m_iIntended=5;public NativeList<AICF_AIQuotaAgentRow> m_aRows=new NativeList<AICF_AIQuotaAgentRow>();public virtual bool Capture(SCR_AIWorld world){return Run(world);}public virtual bool IsCurrent(){return Run();}}
 public class AICF_AIQuotaCensus:NativeCensus{__CAPTURE__ __CURRENT__}
 public class NativeService:NativeBase{public SCR_AIWorld m_World=new SCR_AIWorld();public int m_iEpoch=1;public bool m_bNativeBirthBusy,m_bQuotaCensusBusy,m_bQuarantined,m_bPolicyActive=true;public N m_Limits=new N(),m_Ledger=new N(),m_WorldState=new N();public NativeList<AICF_AIQuotaBody> m_aBodies=new NativeList<AICF_AIQuotaBody>();public string m_sLastReason="NATIVE_CENSUS_UNKNOWN";
 public virtual bool RefreshCensus(){return Run();}public virtual bool RefreshCurrentCensus(){return Run();}public virtual bool BuildCounts(AICF_AIQuotaCensus s,List<int> c){return Run(s,c);}public virtual bool ReadQuotaCharacterCustody(AICF_AIQuotaAgentRow r,out bool h){h=true;return Run(r);}public virtual bool ClassifyRow(AICF_AIQuotaAgentRow r,out AICF_EAIQuotaBucket b){b=AICF_EAIQuotaBucket.FLEET;return Run(r);}public virtual bool ClassifyCapturedActor(AICF_AIQuotaBody b,SCR_AIGroup g,out AICF_EAIQuotaBucket k){k=AICF_EAIQuotaBucket.FLEET;return Run(b,g);}public virtual bool ClassifyQuotaVehicleRow(AICF_AIQuotaAgentRow r,out AICF_EAIQuotaBucket b,out bool c){b=AICF_EAIQuotaBucket.FLEET;c=true;return Run(r);}public virtual bool AddQuotaVehicleHolds(AICF_AIQuotaCensus s,List<int> c,List<IEntity> a){return Run(s,c,a);}}
 public class AICF_AIWorldQuotaService:NativeService{protected string AICF_DecisionContext()__CONTEXT__ __SERVICE__}
 public static class Cases{
 public static int Run(){int n=0;foreach(string name in new[]{"RefreshCensus","RefreshCurrentCensus","BuildCounts","ReadQuotaCharacterCustody","ClassifyRow","ClassifyCapturedActor","ClassifyQuotaVehicleRow","AddQuotaVehicleHolds","Capture","IsCurrent"})foreach(bool result in new[]{false,true})foreach(bool enabled in new[]{false,true})foreach(string callback in new[]{"none","replaceFrame","clearArgs"}){
 AICF_QuotaCensusDecisionProbe.Stop();Shim.Logs.Clear();Cli.Value=enabled?"1":"";var service=new AICF_AIWorldQuotaService{Result=result};var snapshot=new AICF_AIQuotaCensus{Result=result,m_World=service.m_World};var row=new AICF_AIQuotaAgentRow();var body=new AICF_AIQuotaBody();var parent=new SCR_AIGroup();var owner=new AICF_AIQuotaVehicleOwner();var use=new AICF_AIQuotaVehicleUse();var counts=new List<int>();var counted=new List<IEntity>();AICF_QuotaCensusDecisionFrame prior=null,successor=null;if(enabled)prior=AICF_QuotaCensusDecisionProbe.Begin(service,service.m_World,"PRIOR","before=1");NativeBase target=(name=="Capture"||name=="IsCurrent")?(NativeBase)snapshot:service;
 if(enabled&&callback=="replaceFrame")target.During=()=>{successor=AICF_QuotaCensusDecisionProbe.Begin(service,service.m_World,"SUCCESSOR","before=1");};if(callback=="clearArgs")target.During=()=>{row.m_Group=null;body.m_GroupOwner=null;service.m_Limits=null;service.m_Ledger=null;owner.m_sLease=null;};bool actual=false,hold=false,charge=false;AICF_EAIQuotaBucket bucket=AICF_EAIQuotaBucket.STOCK;object[] args=null;
 switch(name){case"RefreshCensus":actual=service.RefreshCensus();args=new object[0];break;case"RefreshCurrentCensus":actual=service.RefreshCurrentCensus();args=new object[0];break;case"BuildCounts":actual=service.BuildCounts(snapshot,counts);args=new object[]{snapshot,counts};break;case"ReadQuotaCharacterCustody":actual=service.ReadQuotaCharacterCustody(row,out hold);args=new object[]{row};if(!hold)throw new Exception("hold altered");break;case"ClassifyRow":actual=service.ClassifyRow(row,out bucket);args=new object[]{row};break;case"ClassifyCapturedActor":actual=service.ClassifyCapturedActor(body,parent,out bucket);args=new object[]{body,parent};break;case"ClassifyQuotaVehicleRow":actual=service.ClassifyQuotaVehicleRow(row,out bucket,out charge);args=new object[]{row};if(!charge)throw new Exception("charge altered");break;case"AddQuotaVehicleHolds":actual=service.AddQuotaVehicleHolds(snapshot,counts,counted);args=new object[]{snapshot,counts,counted};break;case"Capture":actual=snapshot.Capture(service.m_World);args=new object[]{service.m_World};break;case"IsCurrent":actual=snapshot.IsCurrent();args=new object[0];break;}
 if(name=="ClassifyRow"||name=="ClassifyCapturedActor"||name=="ClassifyQuotaVehicleRow")if(bucket!=AICF_EAIQuotaBucket.FLEET)throw new Exception("bucket altered");if(actual!=result||target.Calls!=1||target.Writes!=0||target.Args.Length!=args.Length)throw new Exception(name+" result/delegate/write");for(int i=0;i<args.Length;i++)if(target.Args[i]!=args[i])throw new Exception(name+" arg altered");if(enabled&&callback=="replaceFrame"&&AICF_QuotaCensusDecisionProbe.s_Frame!=successor)throw new Exception("successor erased");if(name=="RefreshCensus"&&enabled&&callback!="replaceFrame"&&AICF_QuotaCensusDecisionProbe.s_Frame!=prior)throw new Exception("prior frame lost");n++;}return n;}
 public static int Edges(){
 var service=new AICF_AIWorldQuotaService{Result=false};AICF_QuotaCensusDecisionProbe.Stop();Shim.Logs.Clear();Cli.Value="0";service.RefreshCensus();if(Shim.Logs.Count!=0||service.m_World.Reads!=0||service.Calls!=1)throw new Exception("disabled observer ran");
 AICF_QuotaCensusDecisionProbe.Stop();Shim.Logs.Clear();Cli.Value="1";service=new AICF_AIWorldQuotaService{Result=true};service.RefreshCensus();if(Shim.Logs.Count!=0||service.m_World.Reads!=0||service.Calls!=1)throw new Exception("positive decision observed");
 AICF_QuotaCensusDecisionProbe.Stop();Shim.Logs.Clear();var snapshot=new AICF_AIQuotaCensus{Result=false};AICF_QuotaCensusDecisionProbe.Begin(service,snapshot.m_World,"BOUND","");for(int i=0;i<4097;i++)snapshot.m_aRows.Insert(new AICF_AIQuotaAgentRow{m_EntityId=i+1});snapshot.Capture(snapshot.m_World);string text=string.Join("\n",Shim.Logs);if(!text.Contains("examined_rows=4096")||!text.Contains("cached_scan_complete=False")||snapshot.Calls!=1)throw new Exception("unexamined suffix reported complete");
 AICF_QuotaCensusDecisionProbe.Stop();Shim.Logs.Clear();snapshot=new AICF_AIQuotaCensus{Result=false};AICF_QuotaCensusDecisionProbe.Begin(service,snapshot.m_World,"ROWS","");snapshot.m_aRows.Insert(null);snapshot.m_aRows.Insert(new AICF_AIQuotaAgentRow{m_EntityId=0});snapshot.m_aRows.Insert(new AICF_AIQuotaAgentRow{m_EntityId=7});snapshot.m_aRows.Insert(new AICF_AIQuotaAgentRow{m_EntityId=7});snapshot.Capture(snapshot.m_World);text=string.Join("\n",Shim.Logs);if(!text.Contains("null_rows=1")||!text.Contains("cached_counted_no_entity=1")||!text.Contains("cached_duplicate_entities=1")||!text.Contains("cached_scan_complete=True"))throw new Exception("cached row facts lost");
 AICF_QuotaCensusDecisionProbe.Stop();Shim.Logs.Clear();var frame=AICF_QuotaCensusDecisionProbe.Begin(service,service.m_World,"CURRENT","");AICF_QuotaCensusDecisionProbe.Begin(service,service.m_World,"SUCCESSOR","");AICF_QuotaCensusDecisionProbe.Write(frame,"LATE","ID","none");if(Shim.Logs.Count!=1||!Shim.Logs[0].Contains("SCOPE_LOSS"))throw new Exception("old frame wrote attribution");
 AICF_QuotaCensusDecisionProbe.Stop();Shim.Logs.Clear();service=new AICF_AIWorldQuotaService{Result=false};Cli.Value="";service.RefreshCensus();if(Shim.Logs.Count!=0||AICF_QuotaCensusDecisionProbe.s_Frame!=null)throw new Exception("absent flag created frame");return 6;
 }
 public static int Frames(){AICF_QuotaCensusDecisionProbe.Stop();Shim.Logs.Clear();var service=new AICF_AIWorldQuotaService();var a=AICF_QuotaCensusDecisionProbe.Begin(service,service.m_World,"A","");var b=AICF_QuotaCensusDecisionProbe.Begin(service,service.m_World,"B","");AICF_QuotaCensusDecisionProbe.End(b,a);if(AICF_QuotaCensusDecisionProbe.s_Frame!=a)throw new Exception("nested restore");if(!AICF_QuotaCensusDecisionProbe.Want(a,"STAGE","ID")||AICF_QuotaCensusDecisionProbe.Want(a,"STAGE","ID"))throw new Exception("dedup");for(int i=1;i<96;i++)if(!AICF_QuotaCensusDecisionProbe.Want(a,"STAGE","ID"+i))throw new Exception("early bound");if(AICF_QuotaCensusDecisionProbe.Want(a,"STAGE","OVER")||!AICF_QuotaCensusDecisionProbe.s_bOverflow||AICF_QuotaCensusDecisionProbe.s_Keys.Count!=96)throw new Exception("overflow");var successor=AICF_QuotaCensusDecisionProbe.Begin(service,service.m_World,"S","");AICF_QuotaCensusDecisionProbe.End(a,null);if(AICF_QuotaCensusDecisionProbe.s_Frame!=successor||!AICF_QuotaCensusDecisionProbe.s_bScopeLoss)throw new Exception("late end erased successor");if(AICF_QuotaCensusDecisionProbe.ForService(new AICF_AIWorldQuotaService())!=null||AICF_QuotaCensusDecisionProbe.ForWorld(new SCR_AIWorld())!=null)throw new Exception("foreign scope accepted");AICF_QuotaCensusDecisionProbe.Stop();if(AICF_QuotaCensusDecisionProbe.s_Frame!=null||AICF_QuotaCensusDecisionProbe.s_Keys.Count!=0||AICF_QuotaCensusDecisionProbe.s_bOverflow||AICF_QuotaCensusDecisionProbe.s_bScopeLoss)throw new Exception("stop unbalanced");return 8;}
 }
}
'@
$code=$code.Replace('__HELPERS__',$helpers).Replace('__CAPTURE__',$signatures.Capture+$wrappers.Capture).Replace('__CURRENT__',$signatures.IsCurrent+$wrappers.IsCurrent).Replace('__CONTEXT__',(CSharp (Body AICF_DecisionContext))).Replace('__SERVICE__',$serviceMethods)
Add-Type -TypeDefinition $code
$actual=[CensusDecisionReplay.Cases]::Run();$frames=[CensusDecisionReplay.Cases]::Frames();$edges=[CensusDecisionReplay.Cases]::Edges()
$unsafe=0
$refresh=$wrappers.RefreshCensus
foreach($mutation in @(@('zero','bool actual = base.RefreshCensus();','bool actual = Result;'),@('double','bool actual = base.RefreshCensus();','bool actual = base.RefreshCensus(); actual = base.RefreshCensus();'),@('return','return actual;','return !actual;'),@('write','bool actual = base.RefreshCensus();','bool actual = base.RefreshCensus(); DebugNativeWrite();'))){
 if(!$refresh.Contains($mutation[1])){throw "Missing executable unsafe target $($mutation[0])"};$ns='CensusDecisionUnsafe'+$mutation[0];$mutant=$code.Replace('namespace CensusDecisionReplay','namespace '+$ns).Replace($refresh,$refresh.Replace($mutation[1],$mutation[2]));Add-Type -TypeDefinition $mutant;$rejected=$false;try{([type]($ns+'.Cases')).GetMethod('Run').Invoke($null,@())|Out-Null}catch{$rejected=$true};if(!$rejected){throw "Unsafe observer survived $($mutation[0])"};$unsafe++
}
"PASS census decision observer source=$sourceRules actual_transparency=$actual actual_frames=$frames actual_edges=$edges executed_unsafe=$unsafe native=NOT_RUN"
