param([string]$RepositoryRoot=(Split-Path -Parent (Split-Path -Parent $PSScriptRoot)))
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot '../lib/Stage3StaticAudit.Common.ps1')
$s=[IO.File]::ReadAllText((Join-Path $RepositoryRoot 'tests/fixtures/AICF_AIQuotaCatalogProbe.c'))
function Body($name){Get-AICFMethodBody ([pscustomobject]@{Source=$s;Code=(ConvertTo-AICFCodeText $s)}) $name}
# Pinned 1.8.0.13 SCR_EntityCatalogSpawnerData.c SHA256:
# 76D8618626FC0095C5919C24F98B452DD4F4E693AE34CFE4904356B592FF8806.
# Native BaseContainer registration is separate from script compilation.
$catalogDataNativeHeader='[BaseContainerProps(configRoot: true), BaseContainerCustomCheckIntTitleField("m_bEnabled", "SpawnerData", "DISABLED - SpawnerData", 1)]'
function Assert-AICFCatalogDataRegistration([string]$source){
 $pattern='(?m)^[\t ]*'+[regex]::Escape($catalogDataNativeHeader)+'\s*modded[\t ]+class[\t ]+SCR_EntityCatalogSpawnerData\b'
 $match=[regex]::Match($source,$pattern)
 if(!$match.Success){throw 'Serialized catalog observer requires exact adjacent original configRoot/title annotations'}
 $code=ConvertTo-AICFCodeText $source
 $actual=$code.Substring($match.Index,$match.Length)
 if($actual -notmatch '\bBaseContainerProps\s*\(configRoot:\s*true\)' -or
  $actual -notmatch '\bBaseContainerCustomCheckIntTitleField\s*\(' -or
  $actual -notmatch '\bmodded\s+class\s+SCR_EntityCatalogSpawnerData\b'){
  throw 'Catalog registration header must be real code, not a comment/string'
 }
}
Assert-AICFCatalogDataRegistration $s
$registrationUnsafe=@(
 $s.Replace($catalogDataNativeHeader,''),
 $s.Replace($catalogDataNativeHeader,$catalogDataNativeHeader.Replace('configRoot: true','configRoot: false')),
 $s.Replace($catalogDataNativeHeader,'[BaseContainerProps(configRoot: true)]'),
 $s.Replace($catalogDataNativeHeader+([regex]::Match($s,[regex]::Escape($catalogDataNativeHeader)+'(\r?\n)').Groups[1].Value)+'modded class SCR_EntityCatalogSpawnerData',
  '/*'+$catalogDataNativeHeader+([Environment]::NewLine)+'modded class SCR_EntityCatalogSpawnerData*/'+([Environment]::NewLine)+'modded class SCR_EntityCatalogSpawnerData')
)
foreach($mutant in $registrationUnsafe){$rejected=$false;try{Assert-AICFCatalogDataRegistration $mutant}catch{$rejected=$true};if(!$rejected){throw 'Unsafe serialized-class registration mutation escaped'}}
'PASS catalog serialized_data_registration exact_pinned_header=1 unsafe_source_controls=4 native_reflection=NOT_RUN'
$rules=@('300000','InitiateSpawn(entry, GetGame().GetPlayerManager().GetPlayerIdFromControlledEntity(user), slot);','s_Service.ReserveUnits(AICF_EAIQuotaBucket.STOCK, pressure, s_Serial)','s_Service.CancelReservation(s_Pressure, s_Serial)','state != SCR_EEntityRequestStatus.AI_LIMIT_REACHED','GetPlayerManager().GetPlayerController(s_Player.GetPlayerId()) == s_Player','!AICF_GroupRuntime.IsAliveCharacter(liveBody.m_Entity)','Math.AbsFloat(s_Debited - s_Cost) <= 0.01','GetControlAIAgent().CountTowardsAIWorldLimit()','seat.GetOccupant() || seat.IsReserved()','GetCallqueue().Remove(Tick)','GetCallqueue().Remove(Close)')
foreach($r in $rules){if(!$s.Contains($r)){throw "Native catalog fixture boundary missing: $r"}}
if(!$s.Contains('AICF_AIQuotaCatalogIdentity.Capture(player.GetControlledEntity(), false)')-or!$s.Contains('if (!s_User.m_Rpl || !s_User.m_Rpl.Id().IsValid()) return false;')){throw 'Actual network player requires present valid Rpl without assuming server master role'}
if(!$s.Contains('donePlayer.GetPlayerId() == s_ClientReadyPlayer')){throw 'Failure cleanup RPC must retain exact ready player identity'}
if($s-match'\.(SetHealth\w*|SetLifeState|SetWorldTransform|SetOrigin|SetInitialMainEntity|SetLimitOfActiveAIs|SetDormant\w*)\s*\('){throw 'Fixture changes protected native simulation or player identity'}
$method=Body ObserveCallback
$method=$method.Replace('bool bound;','bool bound=false;').Replace('foreach (AICF_AIQuotaCreation creation : s_Creations)','foreach (AICF_AIQuotaCreation creation in s_Creations)')
$code=@'
using System;using System.Collections.Generic;
public class N {public static implicit operator bool(N n){return n!=null;}}
public enum AICF_EAIQuotaBirthStatus{BOUND,QUARANTINED,IN_NATIVE_FACTORY}
public class Ticket:N{public bool m_bClosed=true;}
public class Body:N{public bool Current=true;public bool MatchesIdentity(){return Current;}}
public class AICF_AIQuotaCreation:N{public N m_Entity;public AICF_EAIQuotaBirthStatus m_Status=AICF_EAIQuotaBirthStatus.BOUND;public bool m_bFinished=true;public Ticket m_Ticket=new Ticket();public Body m_Body=new Body();}
public class Probe {public static List<AICF_AIQuotaCreation> s_Creations=new List<AICF_AIQuotaCreation>();public static bool s_CallbackInvalid;public static int s_Callbacks;public static void Log(string e,string d){}public static void ObserveCallback(N actor)BODY}
public static class Cases {public static int Run(){int count=0;foreach(string kind in new[]{"valid","status","unfinished","ticket","body","bodyIdentity","actor","absent"}){var actor=new N();var c=new AICF_AIQuotaCreation{m_Entity=actor};Probe.s_Creations.Clear();Probe.s_Callbacks=0;Probe.s_CallbackInvalid=false;switch(kind){case"status":c.m_Status=AICF_EAIQuotaBirthStatus.QUARANTINED;break;case"unfinished":c.m_bFinished=false;break;case"ticket":c.m_Ticket.m_bClosed=false;break;case"body":c.m_Body=null;break;case"bodyIdentity":c.m_Body.Current=false;break;case"actor":c.m_Entity=new N();break;}if(kind!="absent")Probe.s_Creations.Add(c);Probe.ObserveCallback(actor);if(Probe.s_Callbacks!=1||Probe.s_CallbackInvalid!=(kind!="valid"))throw new Exception(kind);count++;}return count;}}
'@
$code=$code.Replace('BODY',$method)
Add-Type $code;$actual=[Cases]::Run();$unsafe=0
foreach($m in @(@('finished','&& creation.m_bFinished',''),@('ticket','creation.m_Ticket.m_bClosed','true'),@('identity','creation.m_Body.MatchesIdentity()','true'))){
 if(!$code.Contains($m[1])){throw "No mutation target $($m[0])"};$ns='UnsafeCatalogProbe'+$m[0];Add-Type ('namespace '+$ns+' {'+$code.Replace($m[1],$m[2])+'}');$rejected=$false
 try{$null=([type]($ns+'.Cases')).GetMethod('Run').Invoke($null,@())}catch{$rejected=$true};if(!$rejected){throw "Unsafe callback proof survived $($m[0])"};$unsafe++
}
"PASS catalog native fixture source=$($rules.Count+2) actual_callback_cases=$actual executed_unsafe=$unsafe native_runtime=NOT_RUN"
. (Join-Path $RepositoryRoot 'tests/log-audits/Test-AIQuotaCatalogLog.ps1') -LibraryOnly
$log=@'
[AICF][QUOTA_CATALOG_PROBE][SELECTED] mode=group actual_avatar=1 actual_group_leader=1 faction=USSR prefab=group.et authored=2 cost=100
[AICF][QUOTA_CATALOG_PROBE][FULL_BASKET_NEGATIVE] mode=group remaining=1 requested=2 native_factories=0 headers=0 callbacks=0 debits=0
[AICF][QUOTA_CATALOG_PROBE][NATIVE_HEADER] mode=group actual_native_header=1 nonnull=1
[AICF][QUOTA_CATALOG_PROBE][PAYMENT] mode=group value=-100 actual_delta=100 actual_native_debit=1 native_consumer=1
[AICF][QUOTA_CATALOG_PROBE][BODY_IDENTITY] mode=group entity=E1 rpl=R1
[AICF][QUOTA_CATALOG_PROBE][NATIVE_CALLBACK] mode=group index=1 bound_receipt_before_native_activation=1
[AICF][QUOTA_CATALOG_PROBE][BODY_IDENTITY] mode=group entity=E2 rpl=R2
[AICF][QUOTA_CATALOG_PROBE][NATIVE_CALLBACK] mode=group index=2 bound_receipt_before_native_activation=1
[AICF][QUOTA_CATALOG_PROBE][DONE] mode=group passed=1 reason=ACTUAL_NATIVE_CATALOG_TAIL expected=2 factories=2 callbacks=2 debits=1 debited=100 headers=1 elapsed_ms=30000
[AICF][QUOTA_CATALOG_PROBE][TAIL] mode=group actual_tail_ms=25000 retained_original_body_count=2
'@
if(@(Test-AICFQuotaCatalogProof ($log-split"`n") group).Count){throw 'Positive strict catalog log rejected'}
$logs=1
foreach($m in @(@('passed=1','passed=0'),@('entity=E2','entity=E1'),@('rpl=R2','rpl=R1'),@('rpl=R2','rpl=NONE'),@('bound_receipt_before_native_activation=1','bound_receipt_before_native_activation=0'),@('index=2','index=1'),@('debited=100','debited=99'),@('debited=100','debited=NaN'),@('cost=100','cost=NaN'),@('cost=100','cost=Infinity'),@('headers=1 elapsed','headers=2 elapsed'),@('headers=0 callbacks','headers=1 callbacks'),@('remaining=1','remaining=2'),@('actual_tail_ms=25000','actual_tail_ms=24999'),@('retained_original_body_count=2','retained_original_body_count=1'),@('actual_avatar=1','actual_avatar=0'))){
 if(@(Test-AICFQuotaCatalogProof (($log.Replace($m[0],$m[1]))-split"`n") group).Count-eq0){throw "Unsafe log accepted $($m[0])"};$logs++
}
"PASS catalog native strict log cases=$logs"
$catalogCliRoot=Join-Path $RepositoryRoot ('.codex-runtime/catalog-probe-cli-'+[guid]::NewGuid().ToString('N'))
$catalogAudit=Join-Path $RepositoryRoot 'tests/log-audits/Test-AIQuotaCatalogLog.ps1'
$cli=0
foreach($case in @(@('positive with spaces','',0,$true,0),@('enginefatal','ENGINE (F): Crashed',0,$true,1),@('guierror','GUI (E): Invalid widget',0,$true,1),@('scriptfatal','SCRIPT (E): NULL pointer',0,$true,1),@('resource','RESOURCES (E): Missing resource',0,$true,1),@('access','Access violation',0,$true,1),@('nativeexit','',-1,$true,1),@('incomplete','',0,$false,1),@('vm','VM : Uncaught exception',0,$true,1),@('nullinstance','Null instance',0,$true,1),@('applicationcrash','Application crashed with exit code C0000005',0,$true,1),@('unknownmodule','FOOBAR (E): invalid state',0,$true,1),@('unknownmarker','native-subsystem/1 (E): unclassified native error',0,$true,1))){
 $dir=Join-Path $catalogCliRoot $case[0];[IO.Directory]::CreateDirectory($dir)|Out-Null;$path=Join-Path $dir 'console.log';$text=$log
 if($case[3]){$text+="`nENGINE : Game destroyed."};[IO.File]::WriteAllText($path,$text);[IO.File]::WriteAllText((Join-Path $dir 'error.log'),$case[1])
 $out=@(& powershell.exe -NoProfile -ExecutionPolicy Bypass -File $catalogAudit -LogPath $path -Mode group -NativeExitCode $case[2] -EvidenceRoot (Join-Path $dir 'audit') 2>&1);$exit=$LASTEXITCODE
 $out|Set-Content -LiteralPath (Join-Path $dir 'actual-cli-output.txt')
 if($exit-ne$case[4]){throw "Full catalog CLI case $($case[0]) returned $exit expected $($case[4]) $out"};$cli++
}
"PASS catalog actual separate_process_cli_cases=$cli full_siblings_strict=1"
$clientCli=0
$clientPositive='[AICF][QUOTA_CLIENT_BOOTSTRAP] phase=4 actual_actor=1 actual_faction=USSR player=1'+"`nENGINE : Game destroyed."
foreach($case in @(@('clientpositive',$clientPositive,'',0,0),@('clientnoplayer',$clientPositive.Replace('player=1','player=unknown'),'',0,1),@('clientmissingavatar','ENGINE : Game destroyed.','',0,1),@('clientvm',$clientPositive,'VM : Uncaught exception',0,1),@('clientgui',$clientPositive,'GUI (E): invalid widget',0,1),@('clientresource',$clientPositive,'RESOURCES (E): absent',0,1),@('clientnative',$clientPositive,'',-1,1))){
 $dir=Join-Path $catalogCliRoot $case[0];[IO.Directory]::CreateDirectory($dir)|Out-Null;$path=Join-Path $dir 'console.log';[IO.File]::WriteAllText($path,$case[1]);[IO.File]::WriteAllText((Join-Path $dir 'error.log'),$case[2])
 $out=@(& powershell.exe -NoProfile -ExecutionPolicy Bypass -File $catalogAudit -LogPath $path -Mode group -NativeExitCode $case[3] -LogRole Client -EvidenceRoot (Join-Path $dir 'audit') 2>&1);$exit=$LASTEXITCODE
 $out|Set-Content -LiteralPath (Join-Path $dir 'actual-cli-output.txt')
 if($exit-ne$case[4]){throw "Client role CLI case $($case[0]) returned $exit expected $($case[4]) $out"};$clientCli++
}
"PASS catalog client_role_separate_cli_cases=$clientCli no_server_proof_waiver=1"
$stopCli=0
foreach($role in @('Server','Client')){
 $dir=Join-Path $catalogCliRoot ('stop-before-'+$role);[IO.Directory]::CreateDirectory($dir)|Out-Null;$path=Join-Path $dir 'console.log'
 $content="ENGINE : Game destroyed.`n"+$log;if($role-eq'Client'){$content=$clientPositive.Replace("`nENGINE : Game destroyed.",'');$content="ENGINE : Game destroyed.`n"+$content}
 [IO.File]::WriteAllText($path,$content)
 $output=@(& powershell.exe -NoProfile -ExecutionPolicy Bypass -File $catalogAudit -LogPath $path -Mode group -NativeExitCode 0 -LogRole $role -EvidenceRoot (Join-Path $dir 'audit') 2>&1)
 if($LASTEXITCODE-ne1){throw ('Actual CLI accepted stop before proof '+$role)};$stopCli++
}
"PASS catalog stopped_after_proof_actual_cli_cases=$stopCli"

# Execute actual wrappers, retaining their single super result and out value.
$diagMethods=@{}
$censusWrapper=Get-AICFBracedBody -Source $s -StartPattern '\bmodded\s+class\s+AICF_AIQuotaCensus\b'
$serviceWrapper=Get-AICFBracedBody -Source $s -StartPattern '\bmodded\s+class\s+AICF_AIWorldQuotaService\b'
foreach($name in @('Capture','ReadQuotaCharacterCustody','BuildCounts')){
 $wrapperSource=$serviceWrapper;if($name-eq'Capture'){$wrapperSource=$censusWrapper}
 $body=Get-AICFMethodBody ([pscustomobject]@{Source=$wrapperSource;Code=(ConvertTo-AICFCodeText $wrapperSource)}) $name
 if(@([regex]::Matches($body,'super\.'+$name+'\(')).Count-ne1-or@([regex]::Matches($body,'return actual;')).Count-ne1){throw "Diagnostic wrapper is not transparent: $name"}
 if($body-match '\breturn\s+(true|false);|\.\s*(Set\w*|RefreshCensus|ReserveUnits|CancelReservation|Register\w*|Begin\w*)\s*\('){throw "Diagnostic wrapper mutates/replaces decision: $name"}
 $body=$body.Replace('super.','base.').Replace('string.Format','String.Format').Replace('int inspected;','int inspected=0;').Replace('foreach (AICF_AIQuotaBody body : m_aBodies)','foreach (AICF_AIQuotaBody body in m_aBodies)').Replace('foreach (AICF_AIQuotaAgentRow row : m_aRows)','foreach (AICF_AIQuotaAgentRow row in m_aRows)').Replace('ReadQuotaCharacterCustody(row, hold)','ReadQuotaCharacterCustody(row, out hold)')
 $diagMethods[$name]=$body
}
$entityState=Body EntityState
foreach($guard in @('if (entity) actualId = entity.GetID();','if (entity) rpl =','if (entity) control =','if (rpl) master =','if (rpl) actualRpl =','if (control) actualAgent =','if (actualAgent) agentId =','if (actualAgent) nativeActive =','if (actualAgent) nativeCounted =')){if(!$entityState.Contains($guard)){throw "Diagnostic weak identity getter unguarded $guard"}}
$diagCode=@'
using System;using System.Collections.Generic;
namespace CatalogCensusDiag {
 public class N {public static implicit operator bool(N n){return n!=null;}}
 public class NativeList<T>:List<T>{public new int Count(){return base.Count;}}
 public class SCR_AIWorld:N{public int GetCurrentNumOfActiveAIs(){return 1;}public int GetActiveGroupIntendedCount(){return 1;}}
 public static class EntityID{public const int INVALID=0;}
 public static class AICF_AIQuotaMath{public const int MAX_TOTAL_UNITS=4096;}
 public class AICF_AIQuotaCensus:N{}
 public class AICF_AIQuotaAgentRow:N{public int m_AgentId,m_EntityId;public bool m_bCounted,m_bActivated;public N m_Entity,m_Agent;}
 public class AICF_AIQuotaBody:N{public bool Current;public bool m_bReleased,m_bQuarantined;public int m_Bucket;public N m_Entity,m_Rpl;public string Key(){return "captured";}public bool MatchesIdentity(){return Current;}}
 public static class SystemTick {public static int GetTickCount(){return 100000;}}
 public static class AICF_AIQuotaCatalogCensusObservation {public static int s_CaptureAt,s_CustodyAt,s_CountsAt,Telemetry;public static bool Enabled=true;public static bool Due(int at){return Enabled;}public static void EntityState(N e,string c,N expected=null,N rpl=null){Telemetry++;}}
 public class Base {public bool Result,Hold;public int Calls;protected NativeList<AICF_AIQuotaAgentRow> m_aRows=new NativeList<AICF_AIQuotaAgentRow>();public NativeList<AICF_AIQuotaBody> m_aBodies=new NativeList<AICF_AIQuotaBody>();protected static void Print(string s){}protected string GetReason(){return "ACTUAL";}protected int GetNativeActive(){return -1;}protected int GetNativeIntended(){return -1;}public virtual bool Capture(SCR_AIWorld w){Calls++;return Result;}public virtual bool ReadQuotaCharacterCustody(AICF_AIQuotaAgentRow r,out bool h){Calls++;h=Hold;return Result;}public virtual bool BuildCounts(AICF_AIQuotaCensus s,int[] c){Calls++;return Result;}}
 public class Wrapper:Base {public override bool Capture(SCR_AIWorld world)CAPTURE public override bool ReadQuotaCharacterCustody(AICF_AIQuotaAgentRow row,out bool hold)CUSTODY public override bool BuildCounts(AICF_AIQuotaCensus snapshot,int[] counts)COUNTS }
 public static class Cases {public static int Run(){int n=0;foreach(bool enabled in new[]{false,true})foreach(bool result in new[]{false,true}){AICF_AIQuotaCatalogCensusObservation.Enabled=enabled;var w=new Wrapper{Result=result,Hold=!result};bool hold;bool actual=w.Capture(null);if(actual!=result||w.Calls!=1)throw new Exception("Capture transparent");n++;w.Calls=0;actual=w.ReadQuotaCharacterCustody(new AICF_AIQuotaAgentRow(),out hold);if(actual!=result||hold!=!result||w.Calls!=1)throw new Exception("Custody transparent");n++;w.Calls=0;w.m_aBodies.Add(null);w.m_aBodies.Add(new AICF_AIQuotaBody());actual=w.BuildCounts(null,null);if(actual!=result||w.Calls!=1)throw new Exception("Counts transparent");n++;}return n;}}
}
'@
$diagCode=$diagCode.Replace('CAPTURE',$diagMethods.Capture.Replace('System.GetTickCount()','SystemTick.GetTickCount()')).Replace('CUSTODY',$diagMethods.ReadQuotaCharacterCustody.Replace('System.GetTickCount()','SystemTick.GetTickCount()')).Replace('COUNTS',$diagMethods.BuildCounts.Replace('System.GetTickCount()','SystemTick.GetTickCount()'))
Add-Type -TypeDefinition $diagCode
$diagCases=[CatalogCensusDiag.Cases]::Run()
"PASS catalog transparent_census_wrappers_actual=$diagCases source_wrappers=3 guarded_entity_getters=9 native=NOT_RUN"

# Execute the exact read-only enumeration, including an unexamined suffix.
$enumeration=Body AICF_QuotaProbeDescribe
if($enumeration-match '\b(GetRequestState|GetFreeSlot|Set\w*|Spawn\w*|Reserve\w*|InitiateSpawn)\s*\('){throw 'Metadata enumeration performs admission or mutations'}
foreach($guard in @('if (examined >= 256) break;','examined == m_aAssetList.Count()','data.GetEntityCount()','entry.IsEnabled()','actualFaction == requesterFaction')){if(!$enumeration.Contains($guard)){throw "Metadata enumeration guard missing $guard"}}
$enumeration=$enumeration.Replace('string.Format','Shim.Format').Replace('m_aAssetList.Count()','m_aAssetList.Count').Replace('foreach (EEntityCatalogType allowedType : m_aCatalogTypes)','foreach (EEntityCatalogType allowedType in m_aCatalogTypes)').Replace('foreach (SCR_EntityCatalogEntry entry : m_aAssetList)','foreach (SCR_EntityCatalogEntry entry in m_aAssetList)')
foreach($declaration in @('EntityID ownerId;','string allowedTypes;','int examined;','int wantedCount;','int enabledData;','int eligibleCount;','int count;','int cost;')){
 $replacement=$declaration.TrimEnd(';')+' = 0;';if($declaration.StartsWith('string ')){$replacement='string allowedTypes = "";'};if($declaration.StartsWith('EntityID ')){$replacement='EntityID ownerId = null;'}
 $enumeration=$enumeration.Replace($declaration,$replacement)
}
$enumCode=@'
using System;using System.Collections.Generic;
namespace CatalogEnumerationReplay {
 public class N {public static implicit operator bool(N n){return n!=null;}}
 public class EntityID:N {public override string ToString(){return "OWNER";}}
 public class IEntity:N {public EntityID GetID(){return new EntityID();}}
 public class Faction:N {public string GetFactionKey(){return "US";}}
 public enum EEntityCatalogType{CHARACTER=0,GROUP=1,VEHICLE=2}
 public class SCR_EntityCatalog:N {public EEntityCatalogType Type=EEntityCatalogType.GROUP;public int GetCatalogType(){return (int)Type;}}
 public class SCR_EntityCatalogSpawnerData:N {public int Count=5,Cost=100;public int GetEntityCount(){return Count;}public int GetSupplyCost(){return Cost;}public static SCR_EntityCatalogSpawnerData Cast(SCR_EntityCatalogSpawnerData d){return d;}}
 public class SCR_EntityCatalogEntry:N {public SCR_EntityCatalog Parent=new SCR_EntityCatalog();public SCR_EntityCatalogSpawnerData Data=new SCR_EntityCatalogSpawnerData();public SCR_EntityCatalog GetCatalogParent(){return Parent;}public SCR_EntityCatalogSpawnerData GetEntityDataOfType(Type t){return Data;}public string GetPrefab(){return "group.et";}public bool IsEnabled(){return true;}}
 public static class Shim {public static string Format(string f,params object[] args){for(int i=args.Length;i>0;i--)f=f.Replace("%"+i,args[i-1]==null?"NONE":args[i-1].ToString());return f;}}
 public static class AICF_AIQuotaCatalogProbe {public static List<string> Logs=new List<string>();public static void Log(string e,string d){Logs.Add(e+" "+d);}}
 public class Probe {public Faction F=new Faction();public List<EEntityCatalogType> m_aCatalogTypes=new List<EEntityCatalogType>{EEntityCatalogType.GROUP};public List<SCR_EntityCatalogEntry> m_aAssetList=new List<SCR_EntityCatalogEntry>();public IEntity GetOwner(){return new IEntity();}public Faction GetFaction(){return F;}public void AICF_QuotaProbeDescribe(EEntityCatalogType wanted,Faction requesterFaction)BODY}
 public static class Cases {public static int Run(){int n=0;foreach(string kind in new[]{"valid","wrongType","disabledData","zeroCost","oneMember","unexaminedSuffix","foreignFaction"}){var p=new Probe();var e=new SCR_EntityCatalogEntry();p.m_aAssetList.Add(e);switch(kind){case"wrongType":e.Parent.Type=EEntityCatalogType.VEHICLE;break;case"disabledData":e.Data=null;break;case"zeroCost":e.Data.Cost=0;break;case"oneMember":e.Data.Count=1;break;case"unexaminedSuffix":for(int i=1;i<257;i++)p.m_aAssetList.Add(null);break;}AICF_AIQuotaCatalogProbe.Logs.Clear();p.AICF_QuotaProbeDescribe(EEntityCatalogType.GROUP,kind=="foreignFaction"?new Faction():p.F);string last=AICF_AIQuotaCatalogProbe.Logs[AICF_AIQuotaCatalogProbe.Logs.Count-1];bool eligible=kind=="valid"||kind=="unexaminedSuffix"||kind=="foreignFaction";if(!last.Contains("eligible="+(eligible?"1":"0"))||!last.Contains("scan_complete="+(kind=="unexaminedSuffix"?"False":"True"))||!last.Contains("faction_exact="+(kind=="foreignFaction"?"False":"True")))throw new Exception(kind+last);n++;}return n;}}
}
'@
$enumCode=$enumCode.Replace('BODY',$enumeration.Replace('actualType == wanted','actualType == (int)wanted').Replace('entry.GetEntityDataOfType(SCR_EntityCatalogSpawnerData)','entry.GetEntityDataOfType(typeof(SCR_EntityCatalogSpawnerData))'))
Add-Type -TypeDefinition $enumCode
$enumCases=[CatalogEnumerationReplay.Cases]::Run()
"PASS catalog actual_readonly_enumeration_cases=$enumCases bound=256 no_admission_or_mutation=1"

$payment=Body AddSpawnerSupplies
if($payment.Contains('GetSpawnerSupplies(')-or!$payment.Contains('GetSpawnerResourceValue()')){throw 'Payment proof must read the same native resource consumer as stock admission'}
$paymentCode=@'
using System;
namespace CatalogPaymentReplay {
 public static class AICF_AIQuotaCatalogProbe {public static object s_Spawner;public static int s_Debits;public static float s_Debited;public static bool s_CallbackInvalid;public static void Log(string e,string d){}}
 public class Native {public float Resource=100,Legacy=100;public bool Consume=true,NaNAfter;public float GetSpawnerResourceValue(){return Resource;}public float GetSpawnerSupplies(){return Legacy;}public virtual void AddSpawnerSupplies(float value){Legacy+=value;if(Consume)Resource+=value;if(NaNAfter)Resource=float.NaN;}}
 public class Probe:Native {public override void AddSpawnerSupplies(float supplies)BODY}
 public static class Cases {public static int Run(){int n=0;foreach(string kind in new[]{"paid","nativeNoop","credit","foreign","beforeNaN","afterNaN"}){var p=new Probe{Consume=kind!="nativeNoop",NaNAfter=kind=="afterNaN"};if(kind=="beforeNaN")p.Resource=float.NaN;AICF_AIQuotaCatalogProbe.s_Spawner=kind=="foreign"?new object():p;AICF_AIQuotaCatalogProbe.s_Debits=0;AICF_AIQuotaCatalogProbe.s_Debited=0;AICF_AIQuotaCatalogProbe.s_CallbackInvalid=false;p.AddSpawnerSupplies(kind=="credit"?10:-10);bool observed=kind!="credit"&&kind!="foreign";if(AICF_AIQuotaCatalogProbe.s_Debits!=(observed?1:0)||AICF_AIQuotaCatalogProbe.s_CallbackInvalid!=(kind=="nativeNoop"||kind=="beforeNaN"||kind=="afterNaN")||((kind=="beforeNaN"||kind=="afterNaN")?!float.IsNaN(AICF_AIQuotaCatalogProbe.s_Debited):AICF_AIQuotaCatalogProbe.s_Debited!=(observed&&kind!="nativeNoop"?10:0)))throw new Exception(kind+" native="+p.Resource+" legacy="+p.Legacy+" invalid="+AICF_AIQuotaCatalogProbe.s_CallbackInvalid);n++;}return n;}}
}
'@
$paymentCode=$paymentCode.Replace('BODY',$payment.Replace('super.','base.').Replace('Math.AbsFloat','Math.Abs'))
Add-Type -TypeDefinition $paymentCode
$paymentCases=[CatalogPaymentReplay.Cases]::Run()
# Keep both getters distinct: mutate only the actual wrapper body, not declarations.
$unsafePayment=$paymentCode.Replace('namespace CatalogPaymentReplay','namespace CatalogPaymentUnsafe').Replace($payment.Replace('super.','base.').Replace('Math.AbsFloat','Math.Abs'),$payment.Replace('super.','base.').Replace('Math.AbsFloat','Math.Abs').Replace('GetSpawnerResourceValue()','GetSpawnerSupplies()'))
Add-Type -TypeDefinition $unsafePayment
$caught=$false;try{[CatalogPaymentUnsafe.Cases]::Run()|Out-Null}catch{$caught=$true};if(!$caught){throw 'Legacy custom supply balance falsely proves native payment'}
"PASS catalog actual_native_resource_payment_cases=$paymentCases executed_unsafe_legacy_balance=1"

$fund=Body AICF_QuotaProbeFund
$fundCurrent=Body AICF_QuotaProbeFundingCurrent
foreach($guard in @('producer.IsCurrent()','GetSpawnerResourceComponent() != resource','resource.GetConsumer(','resource.GetGenerator(','AICF_AIQuotaCatalogProbe.CurrentUser()','GetRequesterFaction(user) != GetFaction()','data.GetSupplyCost() != quote','!IsSuppliesConsumptionEnabled()')){if(!$fundCurrent.Contains($guard)){throw "Controlled native funding owner guard missing $guard"}}
if($fund-match'm_iCustomSupplies|Set\w*\s*\(|ReserveUnits|InitiateSpawn|SetLimit'){throw 'Controlled funding bypasses native resources'}
$fundCode=@'
using System;
namespace CatalogFundingReplay {
 public class N{public static implicit operator bool(N n){return n!=null;}}
 public class RplId:N{public bool IsValid(){return true;}} public class Rpl:N{public RplId Id(){return new RplId();}}
 public class IEntity:N{public EntityID GetID(){return new EntityID();}}
 public class EntityID:N{}
 public static class Replication{public static RplId FindItemId(N n){return new RplId();}}
 public static class AICF_AIQuotaCatalogResourceClient{public static void Trace(string s,string d){}} public class Service{public int AllocateContextSerial(){return 7;}}
 public class Player:N{public int Calls;public void AICF_QuotaCatalogResourceSubscribe(RplId p,RplId r,int s){Calls++;}}
 public class SCR_EntityCatalogEntry:N{} public class SCR_MilitaryBaseComponent:N{public IEntity GetOwner(){return new IEntity();}} public static class AICF_AIQuotaCatalogResourceObservation{public static SCR_ResourceGenerator s_Generator;public static SCR_ResourceConsumer s_Consumer;public static void Stop(){s_Generator=null;s_Consumer=null;}public static void Links(SCR_ResourceGenerator g,SCR_ResourceConsumer c,string stage){}}
 public class SCR_ResourceConsumer:N{}
 public class SCR_ResourceGenerator:N{public Type Type(){return GetType();}}
 public enum EResourceGeneratorID{DEFAULT} public enum EResourceType{SUPPLIES}
 public class SCR_ResourceComponent:N{public SCR_ResourceConsumer Consumer=new SCR_ResourceConsumer();public SCR_ResourceGenerator Generator=new SCR_ResourceGenerator();public SCR_ResourceConsumer GetConsumer(EResourceGeneratorID i,EResourceType t){return Consumer;}public SCR_ResourceGenerator GetGenerator(EResourceGeneratorID i,EResourceType t){return Generator;}}
 public class AICF_AIQuotaCatalogIdentity:N{public EntityID m_Id=new EntityID();public Rpl m_Rpl=new Rpl();public IEntity m_Entity=new IEntity();public static AICF_AIQuotaCatalogIdentity Capture(IEntity e,bool m){return e?new AICF_AIQuotaCatalogIdentity():null;}}
 public static class AICF_AIQuotaCatalogProbe{public static bool s_FundingAttempted,s_FundingConfirmed,s_Finished,UserCurrent=true;public static Probe s_FundingSpawner;public static SCR_EntityCatalogEntry s_FundingEntry;public static AICF_AIQuotaCatalogIdentity s_FundingProducer,s_User=new AICF_AIQuotaCatalogIdentity();public static SCR_ResourceComponent s_FundingResource;public static SCR_ResourceConsumer s_FundingConsumer;public static SCR_ResourceGenerator s_FundingGenerator;public static int s_FundingQuote,s_ResourceSerial,s_ResourceAck,s_ResourceLast,s_RequestedAt,Logs,FundingLogs;public static float s_FundingBefore,s_FundingShortfall;public static string s_FundingGeneratorType;public static Service s_Service=new Service();public static Player s_Player=new Player();public static bool CurrentUser(){return UserCurrent;}public static void Finish(bool passed,string why){s_Finished=true;}public static void Log(string e,string d){Logs++;if(e=="CONTROLLED_RESOURCE_FUNDING")FundingLogs++;}public static void Reset(){s_FundingAttempted=s_FundingConfirmed=s_Finished=false;UserCurrent=true;s_FundingSpawner=null;s_FundingEntry=null;s_ResourceSerial=s_ResourceAck=s_ResourceLast=Logs=FundingLogs=0;s_Player=new Player();}}
 public class Probe:N{public SCR_ResourceComponent Resource=new SCR_ResourceComponent();public bool Guard=true,Noop,Overfund,InvalidateAfter,NaNAfter,InvalidateReadback,SwapOnReadback,Reading;public int Calls,GuardCalls;public void GetBases(System.Collections.Generic.List<SCR_MilitaryBaseComponent> bases){}public float Balance=50;public IEntity GetOwner(){return new IEntity();}public SCR_ResourceComponent GetSpawnerResourceComponent(){return Resource;}public float GetSpawnerResourceValue(){if(Calls>0)Reading=true;if(InvalidateReadback&&Calls>0)Guard=false;return Balance;}public bool AICF_QuotaProbeFundingCurrent(AICF_AIQuotaCatalogIdentity p,SCR_ResourceComponent r,SCR_ResourceConsumer c,SCR_ResourceGenerator g,SCR_EntityCatalogEntry e,IEntity u,int q){GuardCalls++;if(Reading&&SwapOnReadback){AICF_AIQuotaCatalogProbe.s_Player=new Player();SwapOnReadback=false;}return Guard&&p&&r&&c&&g&&!AICF_AIQuotaCatalogProbe.s_Finished;}public void AddSpawnerSupplies(float amount){Calls++;if(!Noop)Balance+=amount;if(Overfund)Balance++;if(NaNAfter)Balance=float.NaN;if(InvalidateAfter)Guard=false;}public bool AICF_QuotaProbeFund(SCR_EntityCatalogEntry entry,IEntity user,int quote)BODY public bool AICF_QuotaProbeFundingReadback()READBACK public bool AICF_QuotaProbeConfirmAlreadyFunded()ALREADY}
 public static class Cases{public static int Run(){int n=0;foreach(string kind in new[]{"valid","delayed","missingResource","missingGenerator","staleBefore","staleAfter","nativeNoop","overfund","duplicate","alreadyFull","beforeNaN","beforePositiveInfinity","beforeNegativeInfinity","afterNaN","readbackIdentity","readbackPlayerSwap","foreignEntry","foreignSpawner","quoteChanged","waitAck","playerChanged","timeoutStopped"}){AICF_AIQuotaCatalogProbe.Reset();var p=new Probe();var e=new SCR_EntityCatalogEntry();var u=new IEntity();switch(kind){case"missingResource":p.Resource=null;break;case"missingGenerator":p.Resource.Generator=null;break;case"staleBefore":p.Guard=false;break;case"staleAfter":p.InvalidateAfter=true;break;case"nativeNoop":case"delayed":p.Noop=true;break;case"overfund":p.Overfund=true;break;case"alreadyFull":p.Balance=100;break;case"beforeNaN":p.Balance=float.NaN;break;case"beforePositiveInfinity":p.Balance=float.PositiveInfinity;break;case"beforeNegativeInfinity":p.Balance=float.NegativeInfinity;break;case"afterNaN":p.NaNAfter=true;break;case"duplicate":AICF_AIQuotaCatalogProbe.s_FundingAttempted=true;break;case"readbackIdentity":p.InvalidateReadback=true;break;case"readbackPlayerSwap":p.SwapOnReadback=true;break;}
 bool first=p.AICF_QuotaProbeFund(e,u,100);if(first||p.Calls!=0)throw new Exception("premature write "+kind);bool prepared=AICF_AIQuotaCatalogProbe.s_FundingSpawner!=null;if(!prepared){if(AICF_AIQuotaCatalogProbe.s_Player.Calls!=0)throw new Exception("bad setup "+kind);n++;continue;}if(AICF_AIQuotaCatalogProbe.s_Player.Calls!=1)throw new Exception("subscription "+kind);for(int i=0;i<3;i++){if(p.AICF_QuotaProbeFund(e,u,100)||p.Calls!=0)throw new Exception("write before handle ACK "+kind);}if(kind!="waitAck")AICF_AIQuotaCatalogProbe.s_ResourceAck=AICF_AIQuotaCatalogProbe.s_ResourceSerial;
 if(kind=="foreignEntry")e=new SCR_EntityCatalogEntry();if(kind=="foreignSpawner")AICF_AIQuotaCatalogProbe.s_FundingSpawner=new Probe();if(kind=="playerChanged"){p.Guard=false;AICF_AIQuotaCatalogProbe.UserCurrent=false;}if(kind=="timeoutStopped")AICF_AIQuotaCatalogProbe.s_Finished=true;
 bool result=p.AICF_QuotaProbeFund(e,u,kind=="quoteChanged"?101:100);bool called=kind=="valid"||kind=="delayed"||kind=="nativeNoop"||kind=="overfund"||kind=="afterNaN"||kind=="staleAfter"||kind=="readbackIdentity"||kind=="readbackPlayerSwap";if(p.Calls!=(called?1:0)||result!=(kind=="valid"))throw new Exception("dispatch "+kind);bool failure=kind=="foreignEntry"||kind=="foreignSpawner"||kind=="quoteChanged"||kind=="playerChanged"||kind=="timeoutStopped"||kind=="overfund"||kind=="afterNaN"||kind=="staleAfter"||kind=="readbackIdentity"||kind=="readbackPlayerSwap";if(AICF_AIQuotaCatalogProbe.s_Finished!=failure)throw new Exception("failure "+kind);
 if(kind=="delayed"){for(int i=0;i<4;i++)if(p.AICF_QuotaProbeFund(e,u,100)||p.Calls!=1)throw new Exception("pending repeated generation");p.Balance=100;if(!p.AICF_QuotaProbeFund(e,u,100)||p.Calls!=1)throw new Exception("delayed confirmation");}if(kind=="nativeNoop"){for(int i=0;i<4;i++)if(p.AICF_QuotaProbeFund(e,u,100)||p.Calls!=1||AICF_AIQuotaCatalogProbe.s_Finished)throw new Exception("cached no-op pending");AICF_AIQuotaCatalogProbe.s_Finished=true;if(p.AICF_QuotaProbeFund(e,u,100)||p.Calls!=1)throw new Exception("restart after timeout");}if(kind=="valid"&&(!p.AICF_QuotaProbeFund(e,u,100)||p.Calls!=1||AICF_AIQuotaCatalogProbe.FundingLogs!=1))throw new Exception("confirmed repeat");n++;}return n;}}
}
'@
$fundBody=$fund.Replace('Math.AbsFloat','Math.Abs').Replace('array<SCR_MilitaryBaseComponent> bases = {};','System.Collections.Generic.List<SCR_MilitaryBaseComponent> bases = new System.Collections.Generic.List<SCR_MilitaryBaseComponent>();').Replace('bases.Count()','bases.Count').Replace('EntityID baseId;','EntityID baseId=null;').Replace('IEntity baseOwner;','IEntity baseOwner=null;').Replace('for (int baseIndex;','for (int baseIndex=0;')
$readbackBody=(Body AICF_QuotaProbeFundingReadback).Replace('Math.AbsFloat','Math.Abs').Replace('SCR_PlayerController','Player')
$alreadyFundedBody=(Body AICF_QuotaProbeConfirmAlreadyFunded).Replace('SCR_PlayerController','Player').Replace('AICF_AIQuotaCatalogProbe.s_RequestedAt ||','AICF_AIQuotaCatalogProbe.s_RequestedAt != 0 ||')
$fundCode=$fundCode.Replace('BODY',$fundBody).Replace('READBACK',$readbackBody).Replace('ALREADY',$alreadyFundedBody)
Add-Type -TypeDefinition $fundCode
$fundCases=[CatalogFundingReplay.Cases]::Run()
$fundUnsafe=0
foreach($mutation in @(@('once','if (AICF_AIQuotaCatalogProbe.s_FundingAttempted) return AICF_QuotaProbeFundingReadback();',''),@('pending','if (after < quote) return false;',''),@('identity','AICF_AIQuotaCatalogProbe.s_FundingEntry != entry','false'),@('finalPlayer','AICF_AIQuotaCatalogProbe.s_Player != player','false'),@('ack','if (AICF_AIQuotaCatalogProbe.s_ResourceAck != AICF_AIQuotaCatalogProbe.s_ResourceSerial)','if (AICF_AIQuotaCatalogProbe.s_ResourceSerial < 0)'))){
 if(!$fundCode.Contains($mutation[1])){throw "Missing actual funding mutant $($mutation[0])"};$ns='CatalogFundingUnsafe'+$mutation[0];$mutant=$fundCode.Replace('namespace CatalogFundingReplay','namespace '+$ns).Replace($mutation[1],$mutation[2]);Add-Type -TypeDefinition $mutant;$caught=$false;try{([type]($ns+'.Cases')).GetMethod('Run').Invoke($null,@())|Out-Null}catch{$caught=$true};if(!$caught){throw "Controlled funding unsafe survived $($mutation[0])"};$fundUnsafe++
}
"PASS catalog actual_controlled_funding_cases=$fundCases executed_unsafe=$fundUnsafe native_generator_only=1"

# Recover the old Fund control flow by removing only the two new branch calls.
$alreadyCalls='(?m)^[\t ]*if \(AICF_QuotaProbeConfirmAlreadyFunded\(\)\) return true;\r?\n[\t ]*if \(AICF_AIQuotaCatalogProbe.s_Finished\) return false;\r?\n'
if(@([regex]::Matches($fundBody,$alreadyCalls)).Count-ne1){throw 'Already-funded calls cannot be isolated from original Fund'}
$legacyFundBody=[regex]::Replace($fundBody,$alreadyCalls,'')
if($alreadyFundedBody-match'AddSpawnerSupplies|RequestGeneration|RequestAvailability|RequestConsumtion|UpdateInteractor|SetResourceValue|InitiateSpawn|\.s_FundingAttempted\s*='){throw 'Already-funded confirmation changes native resources or generation state'}
$alreadyCode=$fundCode.Replace('namespace CatalogFundingReplay','namespace CatalogAlreadyFundedReplay')
$alreadyCode=$alreadyCode.Replace('public int Calls,GuardCalls;','public Action DuringRead,DuringGuard;public int Calls,GuardCalls;')
$alreadyCode=$alreadyCode.Replace('return Balance;}','float actual=Balance;var callback=DuringRead;DuringRead=null;if(callback!=null)callback();return actual;}')
$alreadyCode=$alreadyCode.Replace('GuardCalls++;','GuardCalls++;var callback=DuringGuard;DuringGuard=null;if(callback!=null)callback();')
$alreadyCode=$alreadyCode.Replace('public bool AICF_QuotaProbeConfirmAlreadyFunded()','public bool LegacyFund(SCR_EntityCatalogEntry entry,IEntity user,int quote)LEGACY public bool AICF_QuotaProbeConfirmAlreadyFunded()').Replace('LEGACY',$legacyFundBody)
$alreadyCasesCode=@'
 public static class AlreadyCases{
 static Probe Setup(){AICF_AIQuotaCatalogProbe.Reset();AICF_AIQuotaCatalogProbe.s_RequestedAt=0;var p=new Probe();if(p.AICF_QuotaProbeFund(new SCR_EntityCatalogEntry(),AICF_AIQuotaCatalogProbe.s_User.m_Entity,200)||p.Calls!=0||AICF_AIQuotaCatalogProbe.s_FundingSpawner!=p)throw new Exception("pending setup");p.Balance=15600;return p;}
 static void Change(string kind,Probe p){switch(kind){case"player":AICF_AIQuotaCatalogProbe.s_Player=new Player();break;case"user":AICF_AIQuotaCatalogProbe.s_User=new AICF_AIQuotaCatalogIdentity();break;case"producer":AICF_AIQuotaCatalogProbe.s_FundingProducer=new AICF_AIQuotaCatalogIdentity();break;case"entry":AICF_AIQuotaCatalogProbe.s_FundingEntry=new SCR_EntityCatalogEntry();break;case"resource":AICF_AIQuotaCatalogProbe.s_FundingResource=new SCR_ResourceComponent();break;case"consumer":AICF_AIQuotaCatalogProbe.s_FundingConsumer=new SCR_ResourceConsumer();break;case"generator":AICF_AIQuotaCatalogProbe.s_FundingGenerator=new SCR_ResourceGenerator();break;case"spawner":AICF_AIQuotaCatalogProbe.s_FundingSpawner=new Probe();break;case"serial":AICF_AIQuotaCatalogProbe.s_ResourceSerial++;break;case"ack":AICF_AIQuotaCatalogProbe.s_ResourceAck++;break;case"quote":AICF_AIQuotaCatalogProbe.s_FundingQuote++;break;case"finished":AICF_AIQuotaCatalogProbe.s_Finished=true;break;case"requested":AICF_AIQuotaCatalogProbe.s_RequestedAt=1;break;case"attempted":AICF_AIQuotaCatalogProbe.s_FundingAttempted=true;break;case"confirmed":AICF_AIQuotaCatalogProbe.s_FundingConfirmed=true;break;case"identityInvalid":p.Guard=false;break;}}
 public static int Run(){int n=0;foreach(bool ack in new[]{false,true}){var p=Setup();if(ack)AICF_AIQuotaCatalogProbe.s_ResourceAck=AICF_AIQuotaCatalogProbe.s_ResourceSerial;var entry=AICF_AIQuotaCatalogProbe.s_FundingEntry;var user=AICF_AIQuotaCatalogProbe.s_User.m_Entity;if(p.LegacyFund(entry,user,200)||p.Calls!=0)throw new Exception("old branch did not reproduce15600/200");if(p.AICF_QuotaProbeFund(entry,user,200)!=ack||p.Calls!=0||AICF_AIQuotaCatalogProbe.s_FundingAttempted||AICF_AIQuotaCatalogProbe.FundingLogs!=0)throw new Exception("new funded ACK/zero-generation");if(ack)for(int i=0;i<3;i++)if(!p.AICF_QuotaProbeFund(entry,user,200)||p.Calls!=0)throw new Exception("confirmed repeat generated");n++;}
 foreach(float balance in new[]{200f,15600f,199f,0f,-1f,float.NaN,float.PositiveInfinity,float.NegativeInfinity}){var p=Setup();AICF_AIQuotaCatalogProbe.s_ResourceAck=AICF_AIQuotaCatalogProbe.s_ResourceSerial;p.Balance=balance;bool expected=balance==200f||balance==15600f;if(p.AICF_QuotaProbeConfirmAlreadyFunded()!=expected||p.Calls!=0||AICF_AIQuotaCatalogProbe.s_FundingAttempted)throw new Exception("finite funded readback "+balance);n++;}
 foreach(string boundary in new[]{"read","guard"})foreach(string kind in new[]{"player","user","producer","entry","resource","consumer","generator","spawner","serial","ack","quote","finished","requested","attempted","confirmed","identityInvalid"}){var p=Setup();AICF_AIQuotaCatalogProbe.s_ResourceAck=AICF_AIQuotaCatalogProbe.s_ResourceSerial;Action change=()=>Change(kind,p);if(boundary=="read")p.DuringRead=change;else p.DuringGuard=change;if(p.AICF_QuotaProbeConfirmAlreadyFunded()||p.Calls!=0)throw new Exception("stale accepted "+boundary+kind);n++;}
 foreach(string kind in new[]{"ack","serial","attempted","finished","requested","confirmed"}){var p=Setup();AICF_AIQuotaCatalogProbe.s_ResourceAck=AICF_AIQuotaCatalogProbe.s_ResourceSerial;if(kind=="serial"){AICF_AIQuotaCatalogProbe.s_ResourceSerial=0;AICF_AIQuotaCatalogProbe.s_ResourceAck=0;}else Change(kind,p);if(p.AICF_QuotaProbeConfirmAlreadyFunded()||p.Calls!=0)throw new Exception("unavailable accepted "+kind);n++;}
 var generated=Setup();AICF_AIQuotaCatalogProbe.s_ResourceAck=AICF_AIQuotaCatalogProbe.s_ResourceSerial;generated.Balance=50;generated.Noop=true;var selected=AICF_AIQuotaCatalogProbe.s_FundingEntry;var actor=AICF_AIQuotaCatalogProbe.s_User.m_Entity;if(generated.AICF_QuotaProbeFund(selected,actor,200)||generated.Calls!=1)throw new Exception("normal generation pending");generated.Balance=15600;if(generated.AICF_QuotaProbeFund(selected,actor,200)||!AICF_AIQuotaCatalogProbe.s_Finished||generated.Calls!=1||AICF_AIQuotaCatalogProbe.s_FundingConfirmed)throw new Exception("pending generated delta bypassed by available funds");n++;return n;}}
'@
$alreadyCode=[regex]::Replace($alreadyCode,'\}\s*$',$alreadyCasesCode+[Environment]::NewLine+'}')
Add-Type -TypeDefinition $alreadyCode
$alreadyCases=[CatalogAlreadyFundedReplay.AlreadyCases]::Run()
$alreadyUnsafe=0
foreach($mutation in @(@('ack','AICF_AIQuotaCatalogProbe.s_ResourceAck != serial','serial < 0'),@('finite','available <= 1000000000','available >= 0'),@('balance','available >= quote','available >= 0'),@('player','AICF_AIQuotaCatalogProbe.s_Player != player','player == null'),@('producer','AICF_AIQuotaCatalogProbe.s_FundingProducer != producer','producer == null'),@('serial','AICF_AIQuotaCatalogProbe.s_ResourceSerial != serial','serial < 0'),@('attempted','AICF_AIQuotaCatalogProbe.s_FundingAttempted ||','false ||'))){
 if(!$alreadyFundedBody.Contains($mutation[1])){throw "Missing already-funded mutant $($mutation[0])"};$ns='CatalogAlreadyFundedUnsafe'+$mutation[0];$changed=$alreadyCode.Replace('namespace CatalogAlreadyFundedReplay','namespace '+$ns).Replace($alreadyFundedBody,$alreadyFundedBody.Replace($mutation[1],$mutation[2]));Add-Type -TypeDefinition $changed;$caught=$false;try{([type]($ns+'.AlreadyCases')).GetMethod('Run').Invoke($null,@())|Out-Null}catch{$caught=$true};if(!$caught){throw "Already-funded unsafe survived $($mutation[0])"};$alreadyUnsafe++
}
"PASS catalog actual_already_funded_cases=$alreadyCases executed_unsafe=$alreadyUnsafe old_15600_200_rejected=1 zero_generation_confirmation=1 native_payment_still_required=1"

$clientSource=$s.Substring($s.IndexOf('class AICF_AIQuotaCatalogResourceClient'))
$clientAudit=[pscustomobject]@{Source=$clientSource;Code=(ConvertTo-AICFCodeText $clientSource)}
$clientTick=Get-AICFMethodBody $clientAudit Tick
$clientStop=Get-AICFMethodBody $clientAudit Stop
$clientExact=(Get-AICFMethodBody $clientAudit ExactHandle).Replace('typename ','Type ')
$clientStart=Get-AICFMethodBody $clientAudit Start
if(@([regex]::Matches($clientStart,'RequestSubscriptionListenerHandleGraceful\(')).Count-ne2-or!$clientStart.Contains('player.AICF_QuotaCatalogResourceReady(serial);')-or!$clientStart.Contains('s_ConsumerHandle != consumerHandle')){throw 'Client setup must request both exact normal handles and fence ACK/timer after Tick'}
if($clientTick-match'RequestSubscription|UpdateInteractor|RequestAvailability|RequestGeneration'){throw 'Client subscription keepalive performs resource mutation'}
$clientCode=@'
using System;
namespace CatalogResourceClientReplay { public static class GetterOrder{public static System.Collections.Generic.List<string> Calls=new System.Collections.Generic.List<string>();public static void Add(string s){Calls.Add(s);}}
 public class N{public static implicit operator bool(N n){return n!=null;}}
 public class RplId:N{} public enum EResourceType{SUPPLIES,OTHER} public enum EResourceGeneratorID{DEFAULT,OTHER}
 public class SCR_ResourceInteractor:N{public Type Type(){GetterOrder.Add("EXPECTED_TYPE");return GetType();}}
 public class SCR_ResourceConsumer:SCR_ResourceInteractor{} public class SCR_ResourceGenerator:SCR_ResourceInteractor{}
 public class SCR_ResourceComponent:N{}
 public class SCR_ResourcePlayerControllerInventoryComponent:N{public int Calls;public void RequestUnsubscription(RplId r,Type t,EResourceType e,EResourceGeneratorID i){Calls++;}}
 public class SCR_ResourceSystemSubscriptionHandleBase:N{public RplId Owner,Resource;public Type Kind;public EResourceType ResourceType;public EResourceGeneratorID Identifier;public Action During;public int Pokes;public RplId GetOwnerRplId(){GetterOrder.Add("OWNER");return Owner;}public RplId GetResourceComponentRplId(){GetterOrder.Add("RESOURCE");return Resource;}public Type GetInteractorType(){GetterOrder.Add("ACTUAL_TYPE");return Kind;}public EResourceType GetResourceType(){GetterOrder.Add("RESOURCE_TYPE");return ResourceType;}public EResourceGeneratorID GetResourceIdentifier(){GetterOrder.Add("IDENTIFIER");return Identifier;}public void Poke(){Pokes++;if(During!=null)During();}}
 public class Queue{public int Removes;public void Remove(Action a){Removes++;}}
 public class Game{public Queue Queue=new Queue();public Queue GetCallqueue(){return Queue;}}
 public class Client{public static N s_Player,s_User,s_Producer;public static SCR_ResourceComponent s_Resource;public static SCR_ResourceConsumer s_Consumer;public static SCR_ResourceGenerator s_Generator;public static SCR_ResourcePlayerControllerInventoryComponent s_Inventory;public static RplId s_ResourceId,s_InventoryId;public static int s_Serial;public static bool Context=true;public static SCR_ResourceSystemSubscriptionHandleBase s_ConsumerHandle,s_GeneratorHandle;public static Game Game=new Game();public static Game GetGame(){return Game;}public static void Trace(string s,string d){}public static bool Current(){return s_Player&&Context;}public static bool ExactHandle(SCR_ResourceSystemSubscriptionHandleBase handle,SCR_ResourceInteractor interactor)EXACT public static void Tick()TICK public static void Stop()STOP
 public static void Reset(){Context=true;s_Player=new N();s_User=new N();s_Producer=new N();s_Resource=new SCR_ResourceComponent();s_Consumer=new SCR_ResourceConsumer();s_Generator=new SCR_ResourceGenerator();s_Inventory=new SCR_ResourcePlayerControllerInventoryComponent();s_ResourceId=new RplId();s_InventoryId=new RplId();s_Serial=7;Game=new Game();s_ConsumerHandle=new SCR_ResourceSystemSubscriptionHandleBase{Owner=s_InventoryId,Resource=s_ResourceId,Kind=s_Consumer.Type()};s_GeneratorHandle=new SCR_ResourceSystemSubscriptionHandleBase{Owner=s_InventoryId,Resource=s_ResourceId,Kind=s_Generator.Type()};}}
 public static class Cases{public static int Run(){int n=0;foreach(string kind in new[]{"valid","owner","resource","kind","resourceType","identifier","null"}){Client.Reset();var h=Client.s_ConsumerHandle;switch(kind){case"owner":h.Owner=new RplId();break;case"resource":h.Resource=new RplId();break;case"kind":h.Kind=typeof(SCR_ResourceGenerator);break;case"resourceType":h.ResourceType=EResourceType.OTHER;break;case"identifier":h.Identifier=EResourceGeneratorID.OTHER;break;case"null":h=null;break;}GetterOrder.Calls.Clear();if(Client.ExactHandle(h,Client.s_Consumer)!=(kind=="valid"))throw new Exception("exact "+kind);int expected=kind=="null"?0:kind=="owner"?1:kind=="resource"?2:kind=="kind"?4:kind=="resourceType"?5:6;string[] order={"OWNER","RESOURCE","ACTUAL_TYPE","EXPECTED_TYPE","RESOURCE_TYPE","IDENTIFIER"};if(GetterOrder.Calls.Count!=expected)throw new Exception("extra handle getters "+kind);for(int j=0;j<expected;j++)if(GetterOrder.Calls[j]!=order[j])throw new Exception("handle getter order "+kind);n++;}
 foreach(string kind in new[]{"valid","stopDuringFirst","replaceDuringFirst","serialDuringFirst","vetoDuringFirst","staleBefore"}){Client.Reset();var c=Client.s_ConsumerHandle;var g=Client.s_GeneratorHandle;var inv=Client.s_Inventory;switch(kind){case"stopDuringFirst":c.During=()=>Client.Stop();break;case"replaceDuringFirst":c.During=()=>Client.s_GeneratorHandle=new SCR_ResourceSystemSubscriptionHandleBase();break;case"serialDuringFirst":c.During=()=>Client.s_Serial++;break;case"vetoDuringFirst":c.During=()=>Client.Context=false;break;case"staleBefore":Client.Context=false;break;}Client.Tick();if(c.Pokes!=(kind=="staleBefore"?0:1)||g.Pokes!=(kind=="valid"?1:0))throw new Exception("tick "+kind);Client.Stop();Client.Stop();if(Client.Game.Queue.Removes<2||Client.s_Player||Client.s_ConsumerHandle||Client.s_GeneratorHandle||Client.s_Inventory||Client.s_Serial!=0)throw new Exception("stop cleanup "+kind);int expected=kind=="vetoDuringFirst"||kind=="staleBefore"?0:2;if(inv.Calls!=expected)throw new Exception("unsubscription "+kind+" "+inv.Calls);n++;}return n;}}
}
'@
$clientCode=$clientCode.Replace('EXACT',$clientExact).Replace('TICK',$clientTick).Replace('STOP',$clientStop)
Add-Type -TypeDefinition $clientCode
$clientCases=[CatalogResourceClientReplay.Cases]::Run()
$clientUnsafe=0
foreach($mutation in @(@('postPoke','if (!Current() || s_Serial != serial || s_ConsumerHandle != consumerHandle || s_GeneratorHandle != generatorHandle) { Trace("TICK_POST_FIRST_POKE_REJECTED", "cached_branch=1"); return; }',''),@('owner','ownerId != s_InventoryId','s_Serial < 0'),@('remove','GetGame().GetCallqueue().Remove(Tick);',''))){
 if(!$clientCode.Contains($mutation[1])){throw "Missing client mutation $($mutation[0])"};$ns='CatalogResourceClientUnsafe'+$mutation[0];Add-Type -TypeDefinition $clientCode.Replace('namespace CatalogResourceClientReplay','namespace '+$ns).Replace($mutation[1],$mutation[2]);$caught=$false;try{([type]($ns+'.Cases')).GetMethod('Run').Invoke($null,@())|Out-Null}catch{$caught=$true};if(!$caught){throw "Unsafe client lifecycle survived $($mutation[0])"};$clientUnsafe++
}
"PASS catalog actual_resource_handle_lifecycle_cases=$clientCases executed_unsafe=$clientUnsafe ack_server_registration=UNOBSERVED handle_short_circuit_getter_order=1"

# Diagnostics must preserve the original Current short circuit, including getter order.
$resourceCurrent=(Get-AICFMethodBody $clientAudit Current).Replace('s_Player.FindComponent(SCR_ResourcePlayerControllerInventoryComponent)','s_Player.FindComponent(typeof(SCR_ResourcePlayerControllerInventoryComponent))')
$resourceTrace=Get-AICFMethodBody $clientAudit Trace
if($resourceTrace-match'\b(?:GetPlayer\w*|GetOwner|FindItem\w*|GetConsumer|GetGenerator|Request\w*|Current)\s*\('){throw 'Resource notes re-evaluate native gameplay getters or decisions'}
foreach($guard in @('s_Notes.Count() >= 64','s_Notes.Contains(key)','s_Notes.Insert(key)','aicfQuotaCatalogProbe')){if(!$resourceTrace.Contains($guard)){throw "Bounded opt-in resource notes missing $guard"}}
$resourceCurrentCode=@'
using System;using System.Collections.Generic;
namespace CatalogResourceCurrentReplay {
 public class N{public static implicit operator bool(N n){return n!=null;}}
 public static class Native{public static string Failure;public static List<string> Calls=new List<string>();public static void Note(string s){Calls.Add(s);}}
 public class IEntity:N{}
 public class RplId:N{public string Kind;public bool IsValid(){Native.Note(Kind+"_ID_VALID");return Native.Failure!=Kind+"_ID_INVALID";}}
 public class Identity:N{public string Kind;public IEntity m_Entity=new IEntity();public bool IsCurrent(){Native.Note(Kind+"_IDENTITY");return Native.Failure!=Kind+"_IDENTITY";}}
 public enum EResourceGeneratorID{DEFAULT}public enum EResourceType{SUPPLIES}
 public class SCR_ResourceConsumer:N{}public class SCR_ResourceGenerator:N{}public class SCR_ResourcePlayerControllerInventoryComponent:N{}
 public class SCR_ResourceComponent:N{public IEntity Owner;public SCR_ResourceConsumer Consumer;public SCR_ResourceGenerator Generator;public IEntity GetOwner(){Native.Note("RESOURCE_OWNER");return Native.Failure=="RESOURCE_OWNER"?new IEntity():Owner;}public SCR_ResourceConsumer GetConsumer(EResourceGeneratorID i,EResourceType t){Native.Note("CONSUMER_POINTER");return Native.Failure=="CONSUMER_POINTER"?new SCR_ResourceConsumer():Consumer;}public SCR_ResourceGenerator GetGenerator(EResourceGeneratorID i,EResourceType t){Native.Note("GENERATOR_POINTER");return Native.Failure=="GENERATOR_POINTER"?new SCR_ResourceGenerator():Generator;}}
 public class Player:N{public IEntity GetControlledEntity(){Native.Note("CONTROLLED_ENTITY");return Native.Failure=="CONTROLLED_ENTITY"?new IEntity():Client.s_User.m_Entity;}public int GetPlayerId(){Native.Note("PLAYER_ID");return Native.Failure=="PLAYER_ID"?0:1;}public SCR_ResourcePlayerControllerInventoryComponent FindComponent(Type t){Native.Note("INVENTORY_COMPONENT");return Native.Failure=="INVENTORY_COMPONENT"?new SCR_ResourcePlayerControllerInventoryComponent():Client.s_Inventory;}}
 public class Manager{public Player GetPlayerController(int id){Native.Note("PLAYER_MANAGER_CONTROLLER");return Native.Failure=="PLAYER_MANAGER"?new Player():Client.s_Player;}}
 public class Game{public Player GetPlayerController(){Native.Note("LOCAL_CONTROLLER");return Native.Failure=="PLAYER_NOT_LOCAL"?new Player():Client.s_Player;}public Manager GetPlayerManager(){Native.Note("PLAYER_MANAGER_GET");return new Manager();}}
 public static class Replication{public static N FindItem(RplId id){Native.Note("RESOURCE_REPLICATION");return Native.Failure=="RESOURCE_REPLICATION"?new N():Client.s_Resource;}public static RplId FindItemId(N n){Native.Note("INVENTORY_REPLICATION");return Native.Failure=="INVENTORY_REPLICATION"?new RplId():Client.s_InventoryId;}}
 public static class Client{public static Player s_Player;public static Identity s_User,s_Producer;public static SCR_ResourceComponent s_Resource;public static SCR_ResourceConsumer s_Consumer;public static SCR_ResourceGenerator s_Generator;public static SCR_ResourcePlayerControllerInventoryComponent s_Inventory;public static RplId s_ResourceId,s_InventoryId;public static string Rejected;public static Game GetGame(){return new Game();}public static bool Reject(string s){Rejected=s;return false;}public static bool Current()CURRENT
 public static bool Legacy(){
 if (!s_Player || GetGame().GetPlayerController() != s_Player || !s_User || !s_User.IsCurrent()) return false;
 if (s_Player.GetControlledEntity() != s_User.m_Entity || s_Player.GetPlayerId() <= 0) return false;
 if (GetGame().GetPlayerManager().GetPlayerController(s_Player.GetPlayerId()) != s_Player) return false;
 if (!s_Producer || !s_Producer.IsCurrent() || !s_Resource || !s_ResourceId.IsValid()) return false;
 if (Replication.FindItem(s_ResourceId) != s_Resource || s_Resource.GetOwner() != s_Producer.m_Entity) return false;
 if (!s_Inventory || s_Player.FindComponent(typeof(SCR_ResourcePlayerControllerInventoryComponent)) != s_Inventory) return false;
 if (!s_InventoryId.IsValid() || Replication.FindItemId(s_Inventory) != s_InventoryId) return false;
 if (s_Resource.GetConsumer(EResourceGeneratorID.DEFAULT, EResourceType.SUPPLIES) != s_Consumer) return false;
 return s_Consumer && s_Generator && s_Resource.GetGenerator(EResourceGeneratorID.DEFAULT, EResourceType.SUPPLIES) == s_Generator;
 }
 public static void Setup(string failure){Native.Failure=failure;Native.Calls.Clear();Rejected=null;s_Player=new Player();s_User=new Identity{Kind="USER"};s_Producer=new Identity{Kind="PRODUCER"};s_Consumer=new SCR_ResourceConsumer();s_Generator=new SCR_ResourceGenerator();s_Resource=new SCR_ResourceComponent{Owner=s_Producer.m_Entity,Consumer=s_Consumer,Generator=s_Generator};s_Inventory=new SCR_ResourcePlayerControllerInventoryComponent();s_ResourceId=new RplId{Kind="RESOURCE"};s_InventoryId=new RplId{Kind="INVENTORY"};switch(failure){case"PLAYER_NULL":s_Player=null;break;case"USER_NULL":s_User=null;break;case"PRODUCER_NULL":s_Producer=null;break;case"RESOURCE_NULL":s_Resource=null;break;case"INVENTORY_NULL":s_Inventory=null;break;case"CONSUMER_NULL":s_Consumer=null;s_Resource.Consumer=null;break;case"GENERATOR_NULL":s_Generator=null;s_Resource.Generator=null;break;}}}
 public static class Cases{public static int Run(){int n=0;string[] reasons={"VALID","PLAYER_NULL","PLAYER_NOT_LOCAL","USER_NULL","USER_IDENTITY","CONTROLLED_ENTITY","PLAYER_ID","PLAYER_MANAGER","PRODUCER_NULL","PRODUCER_IDENTITY","RESOURCE_NULL","RESOURCE_ID_INVALID","RESOURCE_REPLICATION","RESOURCE_OWNER","INVENTORY_NULL","INVENTORY_COMPONENT","INVENTORY_ID_INVALID","INVENTORY_REPLICATION","CONSUMER_POINTER","CONSUMER_NULL","GENERATOR_NULL","GENERATOR_POINTER"};int[] expectedCounts={16,0,1,1,2,3,4,7,7,8,8,9,10,11,11,12,13,14,15,15,15,16};string[] complete={"LOCAL_CONTROLLER","USER_IDENTITY","CONTROLLED_ENTITY","PLAYER_ID","PLAYER_MANAGER_GET","PLAYER_ID","PLAYER_MANAGER_CONTROLLER","PRODUCER_IDENTITY","RESOURCE_ID_VALID","RESOURCE_REPLICATION","RESOURCE_OWNER","INVENTORY_COMPONENT","INVENTORY_ID_VALID","INVENTORY_REPLICATION","CONSUMER_POINTER","GENERATOR_POINTER"};for(int i=0;i<reasons.Length;i++){string r=reasons[i];Client.Setup(r);bool before=Client.Legacy();string legacy=String.Join("/",Native.Calls);Client.Setup(r);bool after=Client.Current();string actual=String.Join("/",Native.Calls);if(before!=after||after!=(r=="VALID")||actual!=legacy||Native.Calls.Count!=expectedCounts[i]||Client.Rejected!=(r=="VALID"?null:r))throw new Exception("changed branch/getters "+r+" "+actual);for(int j=0;j<Native.Calls.Count;j++)if(Native.Calls[j]!=complete[j])throw new Exception("getter sequence "+r);n++;}return n;}}
}
'@
$resourceCurrentCode=$resourceCurrentCode.Replace('CURRENT',$resourceCurrent)
Add-Type -TypeDefinition $resourceCurrentCode
$resourceCurrentCases=[CatalogResourceCurrentReplay.Cases]::Run()
$resourceCurrentUnsafe=0
foreach($mutation in @(@('identity','if (!s_Producer.IsCurrent()) return Reject("PRODUCER_IDENTITY");',''),@('replication','if (Replication.FindItemId(s_Inventory) != s_InventoryId) return Reject("INVENTORY_REPLICATION");',''),@('consumer','if (s_Resource.GetConsumer(EResourceGeneratorID.DEFAULT, EResourceType.SUPPLIES) != s_Consumer) return Reject("CONSUMER_POINTER");',''))){
 if(!$resourceCurrent.Contains($mutation[1])){throw "Missing Current native-order mutation $($mutation[0])"};$ns='CatalogResourceCurrentUnsafe'+$mutation[0];Add-Type -TypeDefinition $resourceCurrentCode.Replace('namespace CatalogResourceCurrentReplay','namespace '+$ns).Replace($resourceCurrent,$resourceCurrent.Replace($mutation[1],$mutation[2]));$caught=$false;try{([type]($ns+'.Cases')).GetMethod('Run').Invoke($null,@())|Out-Null}catch{$caught=$true};if(!$caught){throw "Current native-order unsafe survived $($mutation[0])"};$resourceCurrentUnsafe++
}
"PASS catalog actual_resource_current_short_circuit_cases=$resourceCurrentCases executed_unsafe=$resourceCurrentUnsafe original_native_getter_order=1 extra_native_getters=0"

# Execute both real identity helpers. A server-only identity must never be mocked client-valid.
$clientIdentitySource=Get-AICFBracedBody -Source $s -StartPattern '\bclass\s+AICF_QuotaCatalogClientIdentity\b'
$serverIdentityFile=Join-Path $RepositoryRoot 'AIConflictCore/Scripts/Game/AIConflict/Forces/AICF_AIQuotaCatalogBasket.c'
$serverIdentitySource=Get-AICFBracedBody -Source ([IO.File]::ReadAllText($serverIdentityFile)) -StartPattern '\bclass\s+AICF_AIQuotaCatalogIdentity\b'
function IdentityBody([string]$Text,[string]$Name){Get-AICFMethodBody ([pscustomobject]@{Source=$Text;Code=(ConvertTo-AICFCodeText $Text)}) $Name}
function IdentityReplay([string]$Text){$Text.Replace('FindComponent(RplComponent)','FindComponent(typeof(RplComponent))').Replace('EntityID.INVALID','0').Replace('!m_sRpl.IsEmpty()','!string.IsNullOrEmpty(m_sRpl)')}
$clientCapture=IdentityReplay (IdentityBody $clientIdentitySource Capture)
$clientIdentityCurrent=IdentityReplay (IdentityBody $clientIdentitySource IsCurrent)
$serverCapture=IdentityReplay (IdentityBody $serverIdentitySource Capture)
$serverIdentityCurrent=IdentityReplay (IdentityBody $serverIdentitySource IsCurrent)
if(!$clientStart.Contains('AICF_QuotaCatalogClientIdentity.Capture(player.GetControlledEntity())')-or!$clientStart.Contains('AICF_QuotaCatalogClientIdentity.Capture(s_Resource.GetOwner())')-or$clientStart.Contains('AICF_AIQuotaCatalogIdentity.Capture(')){throw 'Owner client must capture the actual client-only identity helper'}
if(!$clientSource.Contains('static ref AICF_QuotaCatalogClientIdentity s_User;')-or!$clientSource.Contains('static ref AICF_QuotaCatalogClientIdentity s_Producer;')){throw 'Owner-client identity fields must retain the client-only type'}
if(!$serverCapture.Contains('!Replication.IsServer()')-or!$serverIdentityCurrent.Contains('!Replication.IsServer()')){throw 'Production Catalog identity lost server-only authority'}
$clientIdentityCode=@'
using System;using System.Collections.Generic;
namespace CatalogClientIdentityReplay {
 public class N{public static implicit operator bool(N n){return n!=null;}}
 public static class Replication{public static bool Server;public static bool IsServer(){return Server;}}
 public struct RplId{public int Value;public bool IsValid(){return Value!=-1;}public override string ToString(){return Value.ToString();}}
 public class RplComponent:N{public bool Master;public int Value=42;public bool IsMaster(){return Master;}public RplId Id(){return new RplId{Value=Value};}public static RplComponent Cast(N n){return n as RplComponent;}}
 public class BaseWorld:N{public Dictionary<int,IEntity> Rows=new Dictionary<int,IEntity>();public IEntity FindEntityByID(int id){IEntity e;return Rows.TryGetValue(id,out e)?e:null;}}
 public class IEntity:N{public int Id=1;public BaseWorld World;public RplComponent Rpl=new RplComponent();public Action OnFind;public int GetID(){return Id;}public BaseWorld GetWorld(){return World;}public N FindComponent(Type t){if(OnFind!=null)OnFind();return Rpl;}}
 public class Game:N{public BaseWorld World;public BaseWorld GetWorld(){return World;}}
 public static class Hub{public static Game Game;public static IEntity Entity;public static void Reset(bool server=false,bool master=false){Replication.Server=server;Game=new Game{World=new BaseWorld()};Entity=new IEntity{World=Game.World};Entity.Rpl.Master=master;Game.World.Rows[1]=Entity;}}
 public class AICF_QuotaCatalogClientIdentity:N{public IEntity m_Entity;public int m_Id;public BaseWorld m_World;public RplComponent m_Rpl;public string m_sRpl="";public static Game GetGame(){return Hub.Game;}public static AICF_QuotaCatalogClientIdentity Capture(IEntity entity)CLIENTCAPTURE public bool IsCurrent()CLIENTCURRENT}
 public class AICF_AIQuotaCatalogIdentity:N{public IEntity m_Entity;public int m_Id;public BaseWorld m_World;public RplComponent m_Rpl;public string m_sRpl="";public bool m_bMaster;public static Game GetGame(){return Hub.Game;}public static AICF_AIQuotaCatalogIdentity Capture(IEntity entity,bool requireMaster)SERVERCAPTURE public bool IsCurrent()SERVERCURRENT}
 public static class Cases{public static int Run(){int n=0;foreach(bool master in new[]{false,true}){Hub.Reset(false,master);if(AICF_AIQuotaCatalogIdentity.Capture(Hub.Entity,false)!=null)throw new Exception("actual old server helper accepted a client");var current=AICF_QuotaCatalogClientIdentity.Capture(Hub.Entity);if(current==null||!current.IsCurrent())throw new Exception("actual client helper rejected valid client "+master);n++;}
 foreach(bool master in new[]{false,true}){Hub.Reset(true,master);if(AICF_QuotaCatalogClientIdentity.Capture(Hub.Entity)!=null||AICF_AIQuotaCatalogIdentity.Capture(Hub.Entity,false)==null||(AICF_AIQuotaCatalogIdentity.Capture(Hub.Entity,true)!=null)!=master)throw new Exception("realm/requireMaster flags changed");n++;}Hub.Reset(true);Hub.Entity.Rpl=null;if(AICF_AIQuotaCatalogIdentity.Capture(Hub.Entity,false)==null||AICF_AIQuotaCatalogIdentity.Capture(Hub.Entity,true)!=null||AICF_QuotaCatalogClientIdentity.Capture(Hub.Entity)!=null)throw new Exception("original optional server-Rpl contract changed");n++;
 foreach(string kind in new[]{"noGame","nullEntity","foreignWorld","lookupMissing","lookupOther","invalidEntityId","rplMissing","invalidRplId","rplSwapDuringCapture"}){Hub.Reset();var e=Hub.Entity;switch(kind){case"noGame":Hub.Game=null;break;case"nullEntity":e=null;break;case"foreignWorld":e.World=new BaseWorld();e.World.Rows[1]=e;break;case"lookupMissing":Hub.Game.World.Rows.Clear();break;case"lookupOther":Hub.Game.World.Rows[1]=new IEntity();break;case"invalidEntityId":e.Id=0;Hub.Game.World.Rows[0]=e;break;case"rplMissing":e.Rpl=null;break;case"invalidRplId":e.Rpl.Value=-1;break;case"rplSwapDuringCapture":e.OnFind=()=>e.Rpl=new RplComponent();break;}if(AICF_QuotaCatalogClientIdentity.Capture(e)!=null)throw new Exception("client Capture accepted "+kind);n++;}
 foreach(string kind in new[]{"serverFlip","noGame","gameWorldSwap","entityWorldSwap","entityIdSwap","lookupMissing","lookupOther","rplRemoved","rplComponentSwap","rplIdSwap","rplIdInvalid","storedWorldNull","storedEntityNull","storedRplNull","storedRplTextEmpty","storedIdInvalid"}){Hub.Reset();var e=Hub.Entity;var identity=AICF_QuotaCatalogClientIdentity.Capture(e);if(identity==null)throw new Exception("setup");switch(kind){case"serverFlip":Replication.Server=true;break;case"noGame":Hub.Game=null;break;case"gameWorldSwap":Hub.Game.World=new BaseWorld();break;case"entityWorldSwap":e.World=new BaseWorld();break;case"entityIdSwap":e.Id=2;break;case"lookupMissing":Hub.Game.World.Rows.Clear();break;case"lookupOther":Hub.Game.World.Rows[1]=new IEntity();break;case"rplRemoved":e.Rpl=null;break;case"rplComponentSwap":e.Rpl=new RplComponent();break;case"rplIdSwap":e.Rpl.Value=43;break;case"rplIdInvalid":e.Rpl.Value=-1;break;case"storedWorldNull":identity.m_World=null;break;case"storedEntityNull":identity.m_Entity=null;break;case"storedRplNull":identity.m_Rpl=null;break;case"storedRplTextEmpty":identity.m_sRpl="";break;case"storedIdInvalid":identity.m_Id=0;break;}if(identity.IsCurrent())throw new Exception("client stale identity accepted "+kind);n++;}return n;}}
}
'@
$clientIdentityCode=$clientIdentityCode.Replace('CLIENTCAPTURE',$clientCapture).Replace('CLIENTCURRENT',$clientIdentityCurrent).Replace('SERVERCAPTURE',$serverCapture).Replace('SERVERCURRENT',$serverIdentityCurrent)
Add-Type -TypeDefinition $clientIdentityCode
$clientIdentityCases=[CatalogClientIdentityReplay.Cases]::Run()
$clientIdentityUnsafe=0
$clientIdentityPair=$clientCapture+$clientIdentityCurrent
foreach($mutation in @(@('realm','Replication.IsServer() ||',''),@('lookup','m_World.FindEntityByID(m_Id) != m_Entity','m_Entity == null'),@('rplComponent','m_Entity.FindComponent(typeof(RplComponent)) != m_Rpl','m_Rpl == null'),@('entityId','m_Entity.GetID() != m_Id','m_Entity == null'),@('world','GetGame().GetWorld() != m_World','m_World == null'),@('rplId','rplId.ToString() == m_sRpl','m_sRpl != null'),@('captureCurrent','if (!identity.IsCurrent()) return null;',''))){
 if(!$clientIdentityPair.Contains($mutation[1])){throw "Missing client identity unsafe $($mutation[0])"};$ns='CatalogClientIdentityUnsafe'+$mutation[0];$changed=$clientIdentityCode.Replace('namespace CatalogClientIdentityReplay','namespace '+$ns).Replace($clientCapture,$clientCapture.Replace($mutation[1],$mutation[2])).Replace($clientIdentityCurrent,$clientIdentityCurrent.Replace($mutation[1],$mutation[2]));Add-Type -TypeDefinition $changed;$caught=$false;try{([type]($ns+'.Cases')).GetMethod('Run').Invoke($null,@())|Out-Null}catch{$caught=$true};if(!$caught){throw "Client identity unsafe survived $($mutation[0])"};$clientIdentityUnsafe++
}
"PASS catalog actual_client_identity_cases=$clientIdentityCases executed_unsafe=$clientIdentityUnsafe actual_old_server_identity_rejects_client=1 production_server_identity_unchanged=1 native=NOT_RUN"

$vehicleBody=Body SpawnEntity
if(@([regex]::Matches($s,'override\s+protected\s+IEntity\s+SpawnEntity\s*\(')).Count-ne1){throw 'Exactly one fixture vehicle factory override required'}
if($vehicleBody.Contains('AICF_AIQuotaVehicleEntity.Capture(raw,')){throw 'Vehicle identity Capture must retain the current one-argument native-domain API'}
if(@([regex]::Matches($vehicleBody,'super\.SpawnEntity\(')).Count-ne1-or$vehicleBody-match'\b(?:Set\w*|ReserveUnits|BeginExternalCreation|AddSpawnerSupplies)\s*\('){throw 'Vehicle factory observer changes native dispatch'}
$vehicleBody=$vehicleBody.Replace('super.','base.').Replace('AICF_AIQuotaCatalogIdentity producer;','AICF_AIQuotaCatalogIdentity producer=null;').Replace('AICF_AIQuotaCatalogIdentity slot;','AICF_AIQuotaCatalogIdentity slot=null;')
$vehicleCode=@'
using System;using System.Collections.Generic;
namespace CatalogVehicleFactoryReplay {
 public class N{public static implicit operator bool(N n){return n!=null;}}
 public class IEntity:N{public bool Current=true;}
 public class Vehicle:IEntity{public static Vehicle Cast(IEntity e){return e as Vehicle;}}
 public class AICF_AIQuotaCatalogIdentity:N{public IEntity Entity;public bool IsCurrent(){return Entity.Current;}public static AICF_AIQuotaCatalogIdentity Capture(IEntity e,bool m){return e?new AICF_AIQuotaCatalogIdentity{Entity=e}:null;}}
 public class AICF_AIQuotaVehicleEntity:N{public string m_Id="E",m_sRpl="R";public static Action OnCapture;public static AICF_AIQuotaVehicleEntity Capture(IEntity e){if(OnCapture!=null)OnCapture();return e?new AICF_AIQuotaVehicleEntity():null;}}
 public class NativeList<T>:List<T>{public void Insert(T e){Add(e);}}
 public static class AICF_AIQuotaCatalogProbe{public static object s_Spawner;public static string s_Mode="vehicle";public static bool s_Finished,s_CallbackInvalid;public static int s_Factories;public static NativeList<AICF_AIQuotaVehicleEntity> s_Bodies=new NativeList<AICF_AIQuotaVehicleEntity>();public static void Log(string e,string d){}}
 public class Native{public IEntity Owner=new IEntity(),Raw=new Vehicle();public Action During;public int Calls;public IEntity GetOwner(){return Owner;}public virtual IEntity SpawnEntity(string res,IEntity slot){Calls++;if(During!=null)During();return Raw;}}
 public class Probe:Native{public override IEntity SpawnEntity(string entityResourceName,IEntity slotOwner)BODY}
 public static class Cases{public static int Run(){int n=0;foreach(string kind in new[]{"valid","foreign","character","nativeNull","wrongKind","stopped","producerDeleted","slotDeleted","captureReentry"}){var p=new Probe();var slot=new IEntity();AICF_AIQuotaCatalogProbe.s_Spawner=kind=="foreign"?new object():p;AICF_AIQuotaCatalogProbe.s_Mode=kind=="character"?"group":"vehicle";AICF_AIQuotaCatalogProbe.s_Finished=false;AICF_AIQuotaCatalogProbe.s_CallbackInvalid=false;AICF_AIQuotaCatalogProbe.s_Factories=0;AICF_AIQuotaCatalogProbe.s_Bodies.Clear();AICF_AIQuotaVehicleEntity.OnCapture=null;switch(kind){case"nativeNull":p.Raw=null;break;case"wrongKind":p.Raw=new IEntity();break;case"stopped":p.During=()=>AICF_AIQuotaCatalogProbe.s_Finished=true;break;case"producerDeleted":p.During=()=>p.Owner.Current=false;break;case"slotDeleted":p.During=()=>slot.Current=false;break;case"captureReentry":AICF_AIQuotaVehicleEntity.OnCapture=()=>AICF_AIQuotaCatalogProbe.s_Spawner=new object();break;}var raw=p.SpawnEntity("vehicle.et",slot);bool target=kind!="foreign"&&kind!="character";bool passed=kind=="valid";if(raw!=p.Raw||p.Calls!=1||AICF_AIQuotaCatalogProbe.s_Factories!=(passed?1:0)||AICF_AIQuotaCatalogProbe.s_Bodies.Count!=(passed?1:0)||AICF_AIQuotaCatalogProbe.s_CallbackInvalid!=(target&&!passed))throw new Exception(kind);n++;}return n;}}
}
'@
$vehicleCode=$vehicleCode.Replace('BODY',$vehicleBody)
Add-Type -TypeDefinition $vehicleCode
$vehicleCases=[CatalogVehicleFactoryReplay.Cases]::Run()
"PASS catalog actual_vehicle_factory_observer_cases=$vehicleCases native_super_once=1 return_unchanged=1"

$fundingRow='[AICF][QUOTA_CATALOG_PROBE][CONTROLLED_RESOURCE_FUNDING] mode=group producer=P1 generator_type=NativeGenerator before=0 amount=100 after=100 effective_cost=100 native_generator=1 fixture_only=1'
$fundedLog=$fundingRow+"`n"+$log
if(@(Test-AICFQuotaCatalogProof ($fundedLog-split"`n") group).Count){throw 'Funded native consumer positive rejected'}
$fundedLogs=1
foreach($mutation in @(@('amount=100','amount=NaN'),@('after=100','after=0'),@('amount=100','amount=99'),@('effective_cost=100','effective_cost=99'),@('native_generator=1','native_generator=0'),@('fixture_only=1','fixture_only=0'),@('generator_type=NativeGenerator','generator_type='),@('native_consumer=1','native_consumer=0'),@('actual_delta=100','actual_delta=0'),@('value=-100','value=100'),@('value=-100','value=NaN'),@('actual_delta=100','actual_delta=Infinity'),@('[PAYMENT] mode=group','[PAYMENT] mode=character'))){if(@(Test-AICFQuotaCatalogProof (($fundedLog.Replace($mutation[0],$mutation[1]))-split"`n") group).Count-eq0){throw "Unsafe funding/payment log accepted $($mutation[0])"};$fundedLogs++}
foreach($bad in @(($fundingRow+"`n"+$fundedLog),($log+"`n"+$fundingRow),(($fundedLog-split"`n"|Where-Object{$_-notmatch'\[PAYMENT\]'})-join"`n"))){if(@(Test-AICFQuotaCatalogProof @($bad-split"`n") group).Count-eq0){throw 'Duplicate/late funding or missing actual payment accepted'};$fundedLogs++}
$vehicleLog=@'
[AICF][QUOTA_CATALOG_PROBE][SELECTED] mode=vehicle actual_avatar=1 actual_group_leader=1 faction=USSR prefab=vehicle.et authored=1 cost=100
[AICF][QUOTA_CATALOG_PROBE][BODY_IDENTITY] mode=vehicle entity=E1 rpl=R1
[AICF][QUOTA_CATALOG_PROBE][NATIVE_VEHICLE_FACTORY] mode=vehicle actual_protected_factory_return=1 actual_vehicle=1 retained_physical_identity=1 character_basket=0
[AICF][QUOTA_CATALOG_PROBE][PAYMENT] mode=vehicle value=-100 actual_delta=100 actual_native_debit=1 native_consumer=1
[AICF][QUOTA_CATALOG_PROBE][EMPTY_VEHICLE] mode=vehicle actual_compartments_read=1 occupants=0 reservations=0 counted_vehicle_agent=0 character_bucket_full=1
[AICF][QUOTA_CATALOG_PROBE][DONE] mode=vehicle passed=1 reason=ACTUAL_NATIVE_CATALOG_TAIL expected=1 factories=1 callbacks=0 debits=1 debited=100 headers=0 elapsed_ms=30000
[AICF][QUOTA_CATALOG_PROBE][TAIL] mode=vehicle actual_tail_ms=25000 retained_original_body_count=1
'@
if(@(Test-AICFQuotaCatalogProof ($vehicleLog-split"`n") vehicle).Count){throw 'Actual empty vehicle proof rejected'}
$vehicleLogs=1
foreach($mutation in @(@('actual_vehicle=1','actual_vehicle=0'),@('character_basket=0','character_basket=1'),@('retained_physical_identity=1','retained_physical_identity=0'),@('actual_protected_factory_return=1','actual_protected_factory_return=0'),@('callbacks=0','callbacks=1'),@('occupants=0','occupants=1'),@('reservations=0','reservations=1'),@('character_bucket_full=1','character_bucket_full=0'))){if(@(Test-AICFQuotaCatalogProof (($vehicleLog.Replace($mutation[0],$mutation[1]))-split"`n") vehicle).Count-eq0){throw "Unsafe vehicle proof accepted $($mutation[0])"};$vehicleLogs++}
if(@(Test-AICFQuotaCatalogProof @($vehicleLog-split"`n"|Where-Object{$_-notmatch'\[NATIVE_VEHICLE_FACTORY\]'}) vehicle).Count-eq0){throw 'Missing actual native vehicle factory receipt accepted'};$vehicleLogs++
"PASS catalog funded_consumer_log_cases=$fundedLogs empty_native_vehicle_log_cases=$vehicleLogs"

$generation=Body RequestGeneration
if(@([regex]::Matches($generation,'super\.RequestGeneration\(')).Count-ne1-or$generation-match'RequestAvailability|\.\s*(Register\w*|Update\w*|Set\w*|PerformAction|RequestConsumtion)\s*\('){throw 'Generation diagnostic repeats or mutates native resource decision'}
$links=Body Links
if($links-match'\.\s*(Register\w*|Update\w*|Set\w*|Request\w*|Spawn\w*)\s*\('){throw 'Container linkage diagnostic mutates resource state'}
foreach($guard in @('g < 32','c < 32','if (!container) complete = false;','if (!consumedContainer) complete = false;','generatorContainers.Contains(consumedContainer)','generated && consumed')){if(!$links.Contains($guard)){throw "Bounded exact resource linkage observation missing $guard"}}
$dump=Body DumpKnownSources
if(@([regex]::Matches($dump,'"Prefabs/[^"\r\n]+\.et"')).Count-ne4-or!$dump.Contains('"$profile:AICF_CatalogMetadata_%1.et"')-or$dump-match'SpawnEntity|\.\s*(Set\w*|Register\w*|RequestGeneration|RequestConsumtion)\s*\('){throw 'Resource source export must be bounded/read-only/fixed own profile path'}
if(!$fund.Contains('AICF_AIQuotaCatalogResourceObservation.s_Generator = generator;')-or!$fund.Contains('if (AICF_AIQuotaCatalogResourceObservation.s_Generator == generator) AICF_AIQuotaCatalogResourceObservation.Stop();')-or!(Body Stop).Contains('AICF_AIQuotaCatalogResourceObservation.Stop();')){throw 'Diagnostic generation target lifetime unbalanced'}
$generation=$generation.Replace('super.','base.').Replace('foreach (SCR_ResourceGeneratorActionBase action : m_aActions)','foreach (SCR_ResourceGeneratorActionBase action in m_aActions)').Replace('float storageRange;','float storageRange=0;').Replace('float multiplier;','float multiplier=0;').Replace('string actions;','string actions="";').Replace('int actionIndex;','int actionIndex=0;')
$generationCode=@'
using System;using System.Collections.Generic;
namespace CatalogGenerationReplay {
 public class N{public static implicit operator bool(N n){return n!=null;}}
 public class SCR_ResourceConsumer:N{}
 public class SCR_ResourceGenerationResponse:N{public int GetReason(){return 2;}public float GetAvailableResourceSpace(){return 0;}}
 public class SCR_ResourceGeneratorActionBase:N{public Type Type(){return GetType();}}
 public static class AICF_AIQuotaCatalogResourceObservation{public static SCR_ResourceGenerator s_Generator;public static SCR_ResourceConsumer s_Consumer;public static int LinksCalls;public static SCR_ResourceConsumer ObservedConsumer;public static void Links(SCR_ResourceGenerator g,SCR_ResourceConsumer c,string stage){LinksCalls++;ObservedConsumer=c;}}
 public static class AICF_AIQuotaCatalogProbe{public static int Logs;public static void Log(string e,string d){Logs++;}}
 public class Native:N{public List<SCR_ResourceGeneratorActionBase> m_aActions=new List<SCR_ResourceGeneratorActionBase>{new SCR_ResourceGeneratorActionBase()};public SCR_ResourceGenerationResponse Response=new SCR_ResourceGenerationResponse();public Action During;public int Calls;public float Amount;public SCR_ResourceGenerator Argument;public int GetResourceType(){return 0;}public float GetStorageRange(){return 50;}public float GetResourceMultiplier(){return 1;}public virtual SCR_ResourceGenerationResponse RequestGeneration(float amount,SCR_ResourceGenerator arg=null){Calls++;Amount=amount;Argument=arg;if(During!=null)During();return Response;}}
 public class SCR_ResourceGenerator:Native{public override SCR_ResourceGenerationResponse RequestGeneration(float resourceAmount,SCR_ResourceGenerator generator=null)BODY}
 public static class Cases{public static int Run(){int n=0;foreach(string kind in new[]{"target","foreign","nullResponse","nullActions","nullActionEntry","clearedDuringNative","argument"}){var g=new SCR_ResourceGenerator();var consumer=new SCR_ResourceConsumer();var argument=kind=="argument"?new SCR_ResourceGenerator():null;AICF_AIQuotaCatalogResourceObservation.s_Generator=kind=="foreign"?new SCR_ResourceGenerator():g;AICF_AIQuotaCatalogResourceObservation.s_Consumer=consumer;AICF_AIQuotaCatalogResourceObservation.LinksCalls=0;AICF_AIQuotaCatalogProbe.Logs=0;switch(kind){case"nullResponse":g.Response=null;break;case"nullActions":g.m_aActions=null;break;case"nullActionEntry":g.m_aActions.Add(null);break;case"clearedDuringNative":g.During=()=>{AICF_AIQuotaCatalogResourceObservation.s_Generator=null;AICF_AIQuotaCatalogResourceObservation.s_Consumer=null;};break;}var actual=g.RequestGeneration(200,argument);bool target=kind!="foreign";if(actual!=g.Response||g.Calls!=1||g.Amount!=200||g.Argument!=argument||AICF_AIQuotaCatalogProbe.Logs!=(target?1:0)||AICF_AIQuotaCatalogResourceObservation.LinksCalls!=(target?1:0)||(target&&AICF_AIQuotaCatalogResourceObservation.ObservedConsumer!=consumer))throw new Exception(kind);n++;}return n;}}
}
'@
Add-Type -TypeDefinition ($generationCode.Replace('BODY',$generation))
$generationCases=[CatalogGenerationReplay.Cases]::Run()
$links=$links.Replace('array<SCR_ResourceContainer> generatorContainers = {};','List<SCR_ResourceContainer> generatorContainers = new List<SCR_ResourceContainer>();').Replace('generatorContainers.Insert(container);','generatorContainers.Add(container);').Replace('int generatorCount;','int generatorCount=0;').Replace('int consumerCount;','int consumerCount=0;').Replace('int intersection;','int intersection=0;').Replace('SCR_ResourceContainerQueueBase generated;','SCR_ResourceContainerQueueBase generated=null;').Replace('SCR_ResourceContainerQueueBase consumed;','SCR_ResourceContainerQueueBase consumed=null;').Replace('for (int g;','for (int g=0;').Replace('for (int c;','for (int c=0;').Replace('string.Format','Shim.Format')
$linksCode=@'
using System;using System.Collections.Generic;
namespace CatalogResourceLinksReplay {
 public class N{public static implicit operator bool(N n){return n!=null;}}
 public class SCR_ResourceContainer:N{}
 public class SCR_ResourceContainerQueueBase:N{public List<SCR_ResourceContainer> Items=new List<SCR_ResourceContainer>();public int GetContainerCount(){return Items.Count;}public SCR_ResourceContainer GetContainerAt(int i){return Items[i];}}
 public class SCR_ResourceGenerator:N{public SCR_ResourceContainerQueueBase Queue=new SCR_ResourceContainerQueueBase();public SCR_ResourceContainerQueueBase GetContainerQueue(){return Queue;}}
 public class SCR_ResourceConsumer:N{public SCR_ResourceContainerQueueBase Queue=new SCR_ResourceContainerQueueBase();public SCR_ResourceContainerQueueBase GetContainerQueue(){return Queue;}}
 public static class Shim{public static string Format(string f,params object[] a){for(int i=a.Length;i>0;i--)f=f.Replace("%"+i,a[i-1]==null?"NONE":a[i-1].ToString());return f;}}
 public static class AICF_AIQuotaCatalogProbe{public static string Last;public static void Log(string e,string d){Last=d;}}
 public static class Probe{public static int Rows;public static void Container(SCR_ResourceContainer c,SCR_ResourceGenerator g,SCR_ResourceConsumer s,string stage,string queue,int i){Rows++;}public static void Links(SCR_ResourceGenerator generator,SCR_ResourceConsumer consumer,string stage)BODY}
 public static class Cases{public static int Run(){int n=0;foreach(string kind in new[]{"shared","disjoint","missingGeneratorQueue","missingConsumerQueue","generatorSuffix","consumerSuffix","nullContainer","emptyQueues"}){var g=new SCR_ResourceGenerator();var c=new SCR_ResourceConsumer();var shared=new SCR_ResourceContainer();if(kind!="emptyQueues"){g.Queue.Items.Add(shared);c.Queue.Items.Add(kind=="disjoint"?new SCR_ResourceContainer():shared);}switch(kind){case"missingGeneratorQueue":g.Queue=null;break;case"missingConsumerQueue":c.Queue=null;break;case"generatorSuffix":g.Queue.Items.Clear();for(int i=0;i<32;i++)g.Queue.Items.Add(new SCR_ResourceContainer());g.Queue.Items.Add(shared);break;case"consumerSuffix":c.Queue.Items.Clear();for(int i=0;i<32;i++)c.Queue.Items.Add(new SCR_ResourceContainer());c.Queue.Items.Add(shared);break;case"nullContainer":g.Queue.Items[0]=null;c.Queue.Items[0]=null;break;}Probe.Rows=0;Probe.Links(g,c,"TEST");bool complete=kind=="shared"||kind=="disjoint"||kind=="emptyQueues";int intersection=kind=="shared"?1:0;if(!AICF_AIQuotaCatalogProbe.Last.Contains("exact_pointer_intersection="+intersection)||!AICF_AIQuotaCatalogProbe.Last.Contains("scan_complete="+(complete?"True":"False"))||Probe.Rows>64)throw new Exception(kind+AICF_AIQuotaCatalogProbe.Last);n++;}return n;}}
}
'@
Add-Type -TypeDefinition ($linksCode.Replace('BODY',$links))
$linkCases=[CatalogResourceLinksReplay.Cases]::Run()
"PASS catalog generation_super_once_cases=$generationCases exact_container_link_cases=$linkCases metadata_sources=4 native_mutation=0"

# Vehicle-only eligible native slot/captured pending identity regressions.
& {
param([string]$RepositoryRoot=(Split-Path -Parent (Split-Path -Parent $PSScriptRoot)),[string]$Mutation='')
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot '../lib/Stage3StaticAudit.Common.ps1')
$s=[IO.File]::ReadAllText((Join-Path $RepositoryRoot 'tests/fixtures/AICF_AIQuotaCatalogProbe.c'))
function Body($name){Get-AICFMethodBody ([pscustomobject]@{Source=$s;Code=(ConvertTo-AICFCodeText $s)}) $name}
function Class($name){Get-AICFBracedBody -Source $s -StartPattern ('\bclass\s+'+$name+'\b')}
function Tx($s){
 $s=$s -replace '\bprotected\b','public' -replace '\bref\s+','' -replace '\bnotnull\s+','' -replace '\bEntityID\b','int' -replace '\bResourceName\b','string' -replace '\bchecked\b','examined'
 $s=$s -replace 'FindComponent\((RplComponent|SCR_EntitySpawnerSlotComponent|SCR_SlotServiceComponent|SCR_CatalogEntitySpawnerComponent)\)','FindComponent(typeof($1))'
 $s=$s -replace 'GetEntityDataOfType\(SCR_EntityCatalogSpawnerData\)','GetEntityDataOfType(typeof(SCR_EntityCatalogSpawnerData))'
 $s=$s -replace 'array<([^>]+)> (\w+) = \{\};','EArray<$1> $2 = new EArray<$1>();' -replace '\barray<','EArray<'
 $s=$s -replace 'foreach \(([^:]+) : ([^)]+)\)','foreach ($1 in $2)'
 $s=$s -replace '(?m)^(\s*)(int|float|bool|string|IEntity) (\w+);','$1$2 $3 = default($2);'
 $s=$s -replace 'string.Format\(','Fmt('
 $s=$s -replace 'super\.','base.'
 $s=$s -replace '(?m)^\t(static |bool |void |AICF_QuotaCatalogSlotFrame )',"`tpublic `$1"
 $s=$s -replace '(?m)^\t(?!public\b)([A-Za-z])',"`tpublic `$1"
 return $s
}
$selector=Tx (Body 'AICF_QuotaProbeSelectVehicle');$selector=$selector.Insert($selector.IndexOf('{')+1,'selected=null;slot=null;')
$candidate=Tx (Class 'AICF_QuotaCatalogVehicleSelection')
if($Mutation -eq 'producer-component'){$candidate=$candidate.Replace('if (m_Producer.m_Entity.FindComponent(typeof(SCR_CatalogEntitySpawnerComponent)) != m_Spawner) return false;','').Replace('m_Producer.m_Entity.FindComponent(typeof(SCR_CatalogEntitySpawnerComponent)) != m_Spawner || ','')}
$frame=Tx (Class 'AICF_QuotaCatalogSlotFrame')
$observer=Tx (Class 'AICF_QuotaCatalogSlotObserver')
if($Mutation -eq 'observer-scope'){$observer=$observer.Replace('if (s_Frame != frame) { Lost(frame); return; }','').Replace('if (s_Frame != frame || !current) { Lost(frame); return; }','').Replace('if (s_Frame != frame || !frame.IsCurrent() || s_Frame != frame) { Lost(frame); return; }','')}
if($Mutation -eq 'unknown-ent-first'){$observer=$observer.Replace('if (!CurrentUnknownSlot(frame, slot) || !ent)','if (!ent || !CurrentUnknownSlot(frame, slot))')}
$identity=[IO.File]::ReadAllText((Join-Path $RepositoryRoot 'AIConflictCore/Scripts/Game/AIConflict/Forces/AICF_AIQuotaCatalogBasket.c'))
$identity=Tx (Get-AICFBracedBody -Source $identity -StartPattern '\bclass\s+AICF_AIQuotaCatalogIdentity\b')
$identity=$identity -replace '(?m)^\t(IEntity|int|BaseWorld|RplComponent|string|bool) ',"`tpublic `$1 "
$identity=$identity -replace '(?m)^\t(static AICF_AIQuotaCatalogIdentity )',"`tpublic `$1"
$fundGate=[regex]::Match((Body 'AICF_QuotaProbeFundingCurrent'),'if \(AICF_AIQuotaCatalogProbe.s_Mode == "vehicle"[\s\S]+?return false;').Value
if(!$fundGate){throw 'Missing actual vehicle funding identity gate'}
$sourceRequired=@('GetFreeSlot(data)','AICF_QuotaProbeFund(entry, user, data.GetSupplyCost())','candidate.m_Slot != freeSlot','!RankCheck(entry, user) || !CooldownCheck(user)','BOUND_NATIVE_SLOT_CHANGED','AICF_QuotaCatalogSlotObserver.Stop();')
foreach($part in $sourceRequired){if(!$s.Contains($part)){throw "Missing source fence $part"}}
if((Body 'AICF_QuotaProbeSelectVehicle').IndexOf('GetFreeSlot(data)') -gt (Body 'AICF_QuotaProbeSelectVehicle').IndexOf('AICF_QuotaProbeFund(entry')){throw 'Slot must precede funding'}
foreach($method in @('GetSlotType','IsOccupied','CanSpawnInSlot','GetFreeSlotFromArray','GetLastKnownSuitableSlot')){if(([regex]::Matches((Body $method),'super\.')).Count -ne 1 -or !(Body $method).Contains('return result;')){throw "Single-super/rawreturn $method"}}
$unknownBody=Body 'UnknownHit'
if($unknownBody-match 'GetPhysics\(|IsOccupied\(|TracePosition\(|GetFreeSlot\(' -or !$unknownBody.Contains('frame.m_LastSlot != slot') -or !$unknownBody.Contains('metadata_after_decision=1 atomic_snapshot=0') -or !(Body 'AICF_ObserveUnknownOccupancyHit').Contains('AICF_QuotaCatalogSlotObserver.UnknownHit(this, ent, reason);')){throw 'Unknown hit observation changed native policy/scope'}
$code=@'
using System;using System.Collections.Generic;
public class N{public static implicit operator bool(N x){return x!=null;}public static Game GetGame(){return Env.Game;}public static string Fmt(string f,params object[] p){for(int i=9;i>0;i--)f=f.Replace("%"+i,"{"+(i-1)+"}");return string.Format(f,p);}}
public class EArray<T>:List<T>{public static implicit operator bool(EArray<T> xs){return xs!=null;}public new int Count(){return base.Count;}public void Insert(T x){Add(x);}}
public static class Text{public static bool IsValid(this int x){return x>0;}}
public class vector:N{public float X,Z;public static float DistanceSqXZ(vector a,vector b){return (a.X-b.X)*(a.X-b.X)+(a.Z-b.Z)*(a.Z-b.Z);}public override string ToString(){return X+","+Z;}}
public class BaseWorld:N{public Dictionary<int,IEntity> Map=new Dictionary<int,IEntity>();public IEntity FindEntityByID(int id){return Map.ContainsKey(id)?Map[id]:null;}}
public class Game:N{public BaseWorld World=new BaseWorld();public BaseWorld GetWorld(){return World;}}
public class RplComponent:N{public int Value=1;public bool Master=true;public int Id(){return Value;}public bool IsMaster(){return Master;}public static RplComponent Cast(object x){return x as RplComponent;}}
public class EntityPrefabData:N{public Action Read;public string GetPrefabName(){if(Read!=null)Read();return "fixture-hit.et";}}
public class IEntity:N{public int Id;public BaseWorld World;public RplComponent Rpl=new RplComponent();public SCR_EntitySpawnerSlotComponent Slot;public SCR_CatalogEntitySpawnerComponent Spawner;public IEntity Parent;public bool Removed;public static implicit operator bool(IEntity e){return !Object.ReferenceEquals(e,null)&&!e.Removed;}public EntityPrefabData Prefab=new EntityPrefabData();public Action<string> MetadataRead;public void Snap(string n){if(Removed)throw new Exception("Native getter after removal: "+n);if(MetadataRead!=null)MetadataRead(n);}public int GetID(){Snap("id");return Id;}public BaseWorld GetWorld(){Snap("world");return World;}public string ClassName(){Snap("class");return "FixtureHit";}public EntityPrefabData GetPrefabData(){Snap("prefabdata");return Prefab;}public object FindComponent(Type t){if(t==typeof(RplComponent))return Rpl;if(t==typeof(SCR_EntitySpawnerSlotComponent))return Slot;if(t==typeof(SCR_CatalogEntitySpawnerComponent))return Spawner;return null;}public vector GetOrigin(){return new vector();}public IEntity GetParent(){return Parent;}}
public class SCR_SlotServiceComponent:N{}public class SCR_SlotServiceComponentClass:N{public static SCR_SlotServiceComponentClass Cast(object o){return o as SCR_SlotServiceComponentClass;}public float GetMaxSlotDistance(){return 5;}}
public enum SCR_EEntitySpawnerSlotType{INVALID=0,SMALL=1,LARGE=2}
public enum EEntityCatalogType{VEHICLE,GROUP,CHARACTER}
public enum SCR_EEntityRequestStatus{CAN_SPAWN,NOT_ENOUGH_SUPPLIES,RANK_LOW}
public class Catalog:N{public EEntityCatalogType Type=EEntityCatalogType.VEHICLE;public EEntityCatalogType GetCatalogType(){return Type;}}
public class BaseData:N{public int Mask=1;public int CompatibilityCalls;public virtual bool CanSpawnInSlot(SCR_EEntitySpawnerSlotType t){CompatibilityCalls++;return (Mask&(int)t)!=0;}}
public class SCR_EntityCatalogSpawnerData:BaseData{public int Cost=200,Count=1;public Action CostRead;public int CostReads,CostReadAt=1;public int GetSupplyCost(){CostReads++;if(CostReads==CostReadAt){var r=CostRead;CostRead=null;if(r!=null)r();}return Cost;}public int GetEntityCount(){return Count;}public static SCR_EntityCatalogSpawnerData Cast(object x){return x as SCR_EntityCatalogSpawnerData;}public override bool CanSpawnInSlot(SCR_EEntitySpawnerSlotType slotSize) COMPAT}
public class SCR_EntityCatalogEntry:N{public SCR_EntityCatalogSpawnerData Data=new SCR_EntityCatalogSpawnerData();public Catalog Parent=new Catalog();public string Name="car";public SCR_EEntityRequestStatus State=SCR_EEntityRequestStatus.CAN_SPAWN;public Catalog GetCatalogParent(){return Parent;}public Action DataRead;public object GetEntityDataOfType(Type t){var r=DataRead;DataRead=null;if(r!=null)r();return Data;}public string GetPrefab(){return Name;}}
public class BaseSlot:N{public IEntity Owner;public bool Occupied;public SCR_EEntitySpawnerSlotType Type=SCR_EEntitySpawnerSlotType.SMALL;public int TypeCalls,OccupiedCalls;public Action OnType,OnOccupied;public IEntity GetOwner(){return Owner;}public virtual void AICF_ObserveUnknownOccupancyHit(IEntity ent,string reason){}public virtual SCR_EEntitySpawnerSlotType GetSlotType(){TypeCalls++;var r=OnType;OnType=null;if(r!=null)r();return Type;}public virtual bool IsOccupied(){OccupiedCalls++;var r=OnOccupied;OnOccupied=null;if(r!=null)r();return Occupied;}}
public class SCR_EntitySpawnerSlotComponent:BaseSlot{public override void AICF_ObserveUnknownOccupancyHit(IEntity ent,string reason) UNKNOWNHOOK public override SCR_EEntitySpawnerSlotType GetSlotType() TYPE public override bool IsOccupied() OCCUPIED}
public class SCR_GameModeCampaign:N{public static bool Enabled=true;public static SCR_GameModeCampaign GetInstance(){return Enabled?new SCR_GameModeCampaign():null;}}
public class BaseSpawner:N{public object GetComponentData(IEntity owner){return new SCR_SlotServiceComponentClass();}public IEntity Owner;public EArray<SCR_EntitySpawnerSlotComponent> m_aChildSlots=new EArray<SCR_EntitySpawnerSlotComponent>(),m_aNearSlots=new EArray<SCR_EntitySpawnerSlotComponent>(),CacheOccupied=new EArray<SCR_EntitySpawnerSlotComponent>();public SCR_EntitySpawnerSlotComponent CacheFree;public int ArraySuperCalls,CacheSuperCalls,FreeCalls;public Action QueryRead;public IEntity GetOwner(){return Owner;}public SCR_EntitySpawnerSlotComponent GetFreeSlot(SCR_EntityCatalogSpawnerData data){FreeCalls++;var x=GetLastKnownSuitableSlot(data);if(!x)x=GetFreeSlotFromArray(m_aChildSlots,data,CacheOccupied);if(!x)x=GetFreeSlotFromArray(m_aNearSlots,data,CacheOccupied);var r=QueryRead;QueryRead=null;if(r!=null)r();return x;}public virtual SCR_EntitySpawnerSlotComponent GetFreeSlotFromArray(EArray<SCR_EntitySpawnerSlotComponent> slots,SCR_EntityCatalogSpawnerData data,EArray<SCR_EntitySpawnerSlotComponent> occupied){ArraySuperCalls++;foreach(var slot in slots)if(data.CanSpawnInSlot(slot.GetSlotType())&&!occupied.Contains(slot)&&!slot.IsOccupied())return slot;return null;}public virtual SCR_EntitySpawnerSlotComponent GetLastKnownSuitableSlot(SCR_EntityCatalogSpawnerData d){CacheSuperCalls++;if(CacheFree&&d.CanSpawnInSlot(CacheFree.GetSlotType()))return CacheFree;return null;}}
public class SCR_CatalogEntitySpawnerComponent:BaseSpawner{public static EArray<SCR_CatalogEntitySpawnerComponent> INSTANCES=new EArray<SCR_CatalogEntitySpawnerComponent>();public EArray<SCR_EntityCatalogEntry> m_aAssetList=new EArray<SCR_EntityCatalogEntry>();public bool Rank=true,Cooldown=true,Faction=true,Ack=true;public int FundCalls,Bindings;public object GetFaction(){return Env.Faction;}public object GetRequesterFaction(IEntity u){return Faction?Env.Faction:new object();}public bool RankCheck(SCR_EntityCatalogEntry e,IEntity u){return Rank;}public bool CooldownCheck(IEntity u){return Cooldown;}public SCR_EEntityRequestStatus GetRequestState(SCR_EntityCatalogEntry e,IEntity u){return e.State;}public bool FundingGate(SCR_EntityCatalogEntry entry,IEntity user,int quote){FUNDGATE return true;}public bool AICF_QuotaProbeFund(SCR_EntityCatalogEntry e,IEntity u,int q){FundCalls++;if(!FundingGate(e,u,q))return false;if(!AICF_AIQuotaCatalogProbe.s_FundingSpawner){Bindings++;AICF_AIQuotaCatalogProbe.s_FundingSpawner=this;AICF_AIQuotaCatalogProbe.s_FundingEntry=e;AICF_AIQuotaCatalogProbe.s_ResourceSerial=42;}if(!Ack)return false;AICF_AIQuotaCatalogProbe.s_FundingConfirmed=true;e.State=SCR_EEntityRequestStatus.CAN_SPAWN;return true;}public bool AICF_QuotaProbeSlotCurrent(SCR_EntitySpawnerSlotComponent slot) SLOTCURRENT public bool AICF_QuotaProbeUnfundedVehicleEligible(SCR_EntityCatalogEntry entry,IEntity user) ELIGIBLE public bool Select(IEntity user,out SCR_EntityCatalogEntry selected,out SCR_EntitySpawnerSlotComponent slot) SELECTOR public override SCR_EntitySpawnerSlotComponent GetFreeSlotFromArray(EArray<SCR_EntitySpawnerSlotComponent> slots,SCR_EntityCatalogSpawnerData spawnerData,EArray<SCR_EntitySpawnerSlotComponent> occupiedSlots) ARRAYHOOK public override SCR_EntitySpawnerSlotComponent GetLastKnownSuitableSlot(SCR_EntityCatalogSpawnerData spawnerData) CACHEHOOK }
public static class Replication{public static bool Server=true;public static bool IsServer(){return Server;}}
public class AICF_AIQuotaCatalogIdentity:N IDENTITY
public class AICF_QuotaCatalogVehicleSelection:N CANDIDATE
public class AICF_QuotaCatalogSlotFrame:N FRAME
public class AICF_QuotaCatalogSlotObserver:N OBSERVER
public class AICF_AIQuotaCatalogProbe:N{public static bool s_Finished,s_FundingAttempted,s_FundingConfirmed;public static int s_ResourceSerial,s_RequestedAt,Done,Logs;public static string s_Mode="vehicle";public static AICF_AIQuotaCatalogIdentity s_User;public static SCR_CatalogEntitySpawnerComponent s_FundingSpawner;public static SCR_EntityCatalogEntry s_FundingEntry;public static AICF_QuotaCatalogVehicleSelection s_VehicleSelection;public static bool CurrentUser(){return s_User&&s_User.IsCurrent()&&!s_Finished;}public static void Finish(bool p,string r){s_Finished=true;Done++;AICF_QuotaCatalogSlotObserver.Stop();}public static string LastLog;public static void Log(string e,string d){Logs++;LastLog=e;}public static void Reset(){s_Finished=s_FundingAttempted=s_FundingConfirmed=false;s_ResourceSerial=s_RequestedAt=Done=Logs=0;s_Mode="vehicle";s_User=null;s_FundingSpawner=null;s_FundingEntry=null;s_VehicleSelection=null;AICF_QuotaCatalogSlotObserver.Stop();SCR_CatalogEntitySpawnerComponent.INSTANCES.Clear();Replication.Server=true;}}
public class Env{public static Game Game;public static object Faction=new object();public IEntity User;public SCR_CatalogEntitySpawnerComponent Spawner;public SCR_EntityCatalogEntry Entry;public SCR_EntitySpawnerSlotComponent Slot;public Env(){AICF_AIQuotaCatalogProbe.Reset();Game=new Game();User=Entity(1);AICF_AIQuotaCatalogProbe.s_User=AICF_AIQuotaCatalogIdentity.Capture(User,false);Spawner=NewSpawner(10);Entry=new SCR_EntityCatalogEntry();Spawner.m_aAssetList.Add(Entry);Slot=NewSlot(20);Spawner.m_aNearSlots.Add(Slot);}public static IEntity Entity(int id){var e=new IEntity{Id=id,World=Game.World};Game.World.Map[id]=e;return e;}public static SCR_CatalogEntitySpawnerComponent NewSpawner(int id){var s=new SCR_CatalogEntitySpawnerComponent{Owner=Entity(id)};s.Owner.Spawner=s;SCR_CatalogEntitySpawnerComponent.INSTANCES.Add(s);return s;}public static SCR_EntitySpawnerSlotComponent NewSlot(int id){var s=new SCR_EntitySpawnerSlotComponent{Owner=Entity(id)};s.Owner.Slot=s;return s;}public bool Select(){SCR_EntityCatalogEntry e;SCR_EntitySpawnerSlotComponent s;return Spawner.Select(User,out e,out s);}}
'@
$parts=@{SELECTOR=$selector;CANDIDATE=$candidate;FRAME=$frame;OBSERVER=$observer;IDENTITY=$identity;FUNDGATE=(Tx $fundGate);SLOTCURRENT=(Tx (Body 'AICF_QuotaProbeSlotCurrent'));ELIGIBLE=(Tx (Body 'AICF_QuotaProbeUnfundedVehicleEligible'));UNKNOWNHOOK=(Tx (Body 'AICF_ObserveUnknownOccupancyHit'));TYPE=(Tx (Body 'GetSlotType'));OCCUPIED=(Tx (Body 'IsOccupied'));COMPAT=(Tx (Body 'CanSpawnInSlot'));ARRAYHOOK=(Tx (Body 'GetFreeSlotFromArray'));CACHEHOOK=(Tx (Body 'GetLastKnownSuitableSlot'))}
$code=[regex]::Replace($code,'\b(SELECTOR|CANDIDATE|FRAME|OBSERVER|IDENTITY|FUNDGATE|SLOTCURRENT|ELIGIBLE|UNKNOWNHOOK|TYPE|OCCUPIED|COMPAT|ARRAYHOOK|CACHEHOOK)\b',{param($match) $parts[$match.Value]})
$oldBody=Body 'AICF_QuotaProbeSelect'
$delegate='if (type == EEntityCatalogType.VEHICLE) return AICF_QuotaProbeSelectVehicle(user, selected, slot);'
if(!$oldBody.Contains($delegate)){throw 'Original generic selector vehicle delegation missing'}
$oldBody=$oldBody.Replace($delegate,'')
$oldBody=(Tx $oldBody).Replace('AICF_QuotaProbeFund(','LegacyFund(')
$oldBody=$oldBody.Insert($oldBody.IndexOf('{')+1,'selected=null;slot=null;')
$code=$code.Replace('public bool Select(IEntity user,','public bool LegacyFund(SCR_EntityCatalogEntry e,IEntity u,int q){Bindings++;AICF_AIQuotaCatalogProbe.s_FundingSpawner=this;AICF_AIQuotaCatalogProbe.s_FundingEntry=e;AICF_AIQuotaCatalogProbe.s_FundingConfirmed=true;e.State=SCR_EEntityRequestStatus.CAN_SPAWN;return true;}public bool LegacySelect(EEntityCatalogType type,IEntity user,out SCR_EntityCatalogEntry selected,out SCR_EntitySpawnerSlotComponent slot)'+$oldBody+'public bool Select(IEntity user,')
$request=(Tx (Body 'AICF_QuotaProbeRequest')).Replace('AICF_QuotaCatalogSlotFrame frame;','AICF_QuotaCatalogSlotFrame frame=null;')
$code=$code.Replace('public bool Select(IEntity user,','public IEntity m_SpawnedEntity;public int RequestCalls,Factories,Debits;public void InitiateSpawn(SCR_EntityCatalogEntry e,int user,SCR_EntitySpawnerSlotComponent slot){RequestCalls++;if(!Replication.Server||user<=0||e.State!=SCR_EEntityRequestStatus.CAN_SPAWN||!Rank||!Cooldown||!Faction)return;if(slot&&e.Data.CanSpawnInSlot(slot.GetSlotType())&&!slot.IsOccupied()){Factories++;Debits++;m_SpawnedEntity=Env.Entity(90);}}public void Request(SCR_EntityCatalogEntry entry,IEntity user,SCR_EntitySpawnerSlotComponent slot)'+$request+'public bool Select(IEntity user,')
$code=$code.Replace('public class Game:N{','public class PlayerManager:N{public int GetPlayerIdFromControlledEntity(IEntity user){return user?1:0;}}public class Game:N{public PlayerManager PM=new PlayerManager();public PlayerManager GetPlayerManager(){return PM;}')
$code=$code.Replace('public static bool s_Finished,s_FundingAttempted,s_FundingConfirmed;','public static IEntity s_Produced;public static bool s_Finished,s_FundingAttempted,s_FundingConfirmed;')
$code=$code.Replace('public string GetPrefab(){return Name;}','public Action PrefabRead;public string GetPrefab(){var r=PrefabRead;PrefabRead=null;if(r!=null)r();return Name;}')
$code=$code.Replace('public static string LastLog;','public static string LastLog,LastDetail;').Replace('LastLog=e;','LastLog=e;LastDetail=d;')
$code='namespace AICF_CatalogVehicleCandidateScope {'+$code+'}'
Add-Type -TypeDefinition $code
[AICF_CatalogVehicleCandidateScope.AICF_QuotaCatalogSlotObserver]::End($null,$null)
$legacy=[AICF_CatalogVehicleCandidateScope.Env]::new();$legacy.Entry.Data.Mask=2;$legacy.Entry.State=[AICF_CatalogVehicleCandidateScope.SCR_EEntityRequestStatus]::NOT_ENOUGH_SUPPLIES;$second=[AICF_CatalogVehicleCandidateScope.SCR_EntityCatalogEntry]::new();$legacy.Spawner.m_aAssetList.Add($second);$picked=$null;$pickedSlot=$null
if($legacy.Spawner.LegacySelect([AICF_CatalogVehicleCandidateScope.EEntityCatalogType]::VEHICLE,$legacy.User,[ref]$picked,[ref]$pickedSlot) -or $legacy.Spawner.Bindings -ne 1 -or [AICF_CatalogVehicleCandidateScope.AICF_AIQuotaCatalogProbe]::s_FundingEntry -ne $legacy.Entry){throw 'Actual old selector did not reproduce first null-slot funding pin'}
if($legacy.Spawner.LegacySelect([AICF_CatalogVehicleCandidateScope.EEntityCatalogType]::VEHICLE,$legacy.User,[ref]$picked,[ref]$pickedSlot)){throw 'Actual old selector unexpectedly escaped pinned wrong-type entry'}
$cases=@('positive','first-entry-wrong-type','first-producer-no-slot','all-no-slot','occupied','cached-occupied','cache-free','lowrank','cooldown','foreign','wrong-kind','invalid-cost','invalid-count','query-stop','type-world','occupancy-stop','pending-ack','bound-slot-swapped','bound-world','bound-user','bound-producer-component','bound-slot-component','bound-entry-data','bound-quote','bound-slot-unregistered','bound-funding-gate','bound-direct-producer-gate','slot-no-rpl','slot-invalid-rpl','slot-not-master','producer-no-rpl','producer-invalid-rpl','producer-not-master','late-quote-producer-component','late-quote-slot-component','query-slot-unregistered','query-producer-replacement','observer-stale-write','observer-overflow')
$count=0
foreach($case in $cases){$e=[AICF_CatalogVehicleCandidateScope.Env]::new();$expected=$case -in @('positive','first-entry-wrong-type','first-producer-no-slot','cache-free','pending-ack');if($case -in @('lowrank','cooldown','foreign','wrong-kind','invalid-cost','invalid-count','all-no-slot','occupied','cached-occupied')){$e.Entry.State=[AICF_CatalogVehicleCandidateScope.SCR_EEntityRequestStatus]::NOT_ENOUGH_SUPPLIES};switch($case){
 'first-entry-wrong-type'{$e.Entry.Data.Mask=2;$e.Entry.State=[AICF_CatalogVehicleCandidateScope.SCR_EEntityRequestStatus]::NOT_ENOUGH_SUPPLIES;$next=[AICF_CatalogVehicleCandidateScope.SCR_EntityCatalogEntry]::new();$next.State=[AICF_CatalogVehicleCandidateScope.SCR_EEntityRequestStatus]::NOT_ENOUGH_SUPPLIES;$e.Spawner.m_aAssetList.Add($next)}
 'first-producer-no-slot'{$e.Entry.State=[AICF_CatalogVehicleCandidateScope.SCR_EEntityRequestStatus]::NOT_ENOUGH_SUPPLIES;$e.Spawner.m_aNearSlots.Clear();if($e.Select() -or $e.Spawner.Bindings -ne 0){throw 'No-slot native producer must not bind funding'};$other=[AICF_CatalogVehicleCandidateScope.Env]::NewSpawner(11);$next=[AICF_CatalogVehicleCandidateScope.SCR_EntityCatalogEntry]::new();$next.State=[AICF_CatalogVehicleCandidateScope.SCR_EEntityRequestStatus]::NOT_ENOUGH_SUPPLIES;$other.m_aAssetList.Add($next);$other.m_aNearSlots.Add($e.Slot);$e.Spawner=$other}
 'all-no-slot'{$e.Spawner.m_aNearSlots.Clear()}
 'occupied'{$e.Slot.Occupied=$true}
 'cached-occupied'{$e.Spawner.CacheOccupied.Add($e.Slot)}
 'cache-free'{$e.Spawner.CacheFree=$e.Slot}
 'lowrank'{$e.Spawner.Rank=$false}
 'cooldown'{$e.Spawner.Cooldown=$false}
 'foreign'{$e.Spawner.Faction=$false}
 'wrong-kind'{$e.Entry.Parent.Type=[AICF_CatalogVehicleCandidateScope.EEntityCatalogType]::GROUP}
 'invalid-cost'{$e.Entry.Data.Cost=0}
 'invalid-count'{$e.Entry.Data.Count=21}
 'query-stop'{$e.Spawner.QueryRead={[AICF_CatalogVehicleCandidateScope.AICF_AIQuotaCatalogProbe]::Finish($false,'injected')}}
 'type-world'{$e.Slot.OnType={[AICF_CatalogVehicleCandidateScope.Env]::Game.World=[AICF_CatalogVehicleCandidateScope.BaseWorld]::new()}}
 'occupancy-stop'{$e.Slot.OnOccupied={[AICF_CatalogVehicleCandidateScope.AICF_AIQuotaCatalogProbe]::Finish($false,'injected')}}
 'pending-ack'{$e.Entry.State=[AICF_CatalogVehicleCandidateScope.SCR_EEntityRequestStatus]::NOT_ENOUGH_SUPPLIES;$e.Spawner.Ack=$false;if($e.Select()){throw 'Before ACK selected'};if($e.Spawner.Bindings -ne 1){throw 'Preflight before funding did not bind exact candidate'};$e.Spawner.Ack=$true}
 'slot-no-rpl'{$e.Slot.Owner.Rpl=$null}
 'slot-invalid-rpl'{$e.Slot.Owner.Rpl.Value=0}
 'slot-not-master'{$e.Slot.Owner.Rpl.Master=$false}
 'producer-no-rpl'{$e.Spawner.Owner.Rpl=$null}
 'producer-invalid-rpl'{$e.Spawner.Owner.Rpl.Value=0}
 'producer-not-master'{$e.Spawner.Owner.Rpl.Master=$false}
 'late-quote-producer-component'{$x=$e;$e.Entry.Data.CostReadAt=3;$e.Entry.Data.CostRead={$x.Spawner.Owner.Spawner=[AICF_CatalogVehicleCandidateScope.Env]::NewSpawner(40)}.GetNewClosure()}
 'late-quote-slot-component'{$x=$e;$e.Entry.Data.CostReadAt=3;$e.Entry.Data.CostRead={$x.Slot.Owner.Slot=[AICF_CatalogVehicleCandidateScope.Env]::NewSlot(41)}.GetNewClosure()}
 'query-slot-unregistered'{$x=$e;$e.Spawner.QueryRead={$x.Spawner.m_aNearSlots.Clear()}.GetNewClosure()}
 'query-producer-replacement'{$x=$e;$e.Spawner.QueryRead={$x.Spawner.Owner.Spawner=[AICF_CatalogVehicleCandidateScope.Env]::NewSpawner(42)}.GetNewClosure()}
 'observer-stale-write'{$f=[AICF_CatalogVehicleCandidateScope.AICF_QuotaCatalogSlotObserver]::Begin($e.Spawner,$e.Entry,$e.Entry.Data,$e.User,'TEST');$e.Entry.DataRead={[AICF_CatalogVehicleCandidateScope.AICF_QuotaCatalogSlotObserver]::Stop()};[AICF_CatalogVehicleCandidateScope.AICF_QuotaCatalogSlotObserver]::Write($f,'test','stale');if([AICF_CatalogVehicleCandidateScope.AICF_AIQuotaCatalogProbe]::LastLog -ne 'SLOT_OBSERVER_SCOPE_LOST' -or [AICF_CatalogVehicleCandidateScope.AICF_QuotaCatalogSlotObserver]::s_Keys.ToArray().Length -ne 0){throw 'Stale observer mutated successor'};continue}
 'observer-overflow'{$f=[AICF_CatalogVehicleCandidateScope.AICF_QuotaCatalogSlotObserver]::Begin($e.Spawner,$e.Entry,$e.Entry.Data,$e.User,'TEST');0..511|ForEach-Object {[AICF_CatalogVehicleCandidateScope.AICF_QuotaCatalogSlotObserver]::s_Keys.Add($_.ToString())};[AICF_CatalogVehicleCandidateScope.AICF_QuotaCatalogSlotObserver]::Write($f,'new','overflow');if(![AICF_CatalogVehicleCandidateScope.AICF_QuotaCatalogSlotObserver]::s_bOverflow -or [AICF_CatalogVehicleCandidateScope.AICF_QuotaCatalogSlotObserver]::s_Keys.ToArray().Length -ne 512){throw 'Observer overflow unbounded'};continue}
 default {if($case.StartsWith('bound-')){$e.Entry.State=[AICF_CatalogVehicleCandidateScope.SCR_EEntityRequestStatus]::NOT_ENOUGH_SUPPLIES;$e.Spawner.Ack=$false;[void]$e.Select();$e.Spawner.Ack=$true;switch($case){'bound-slot-swapped'{$e.Spawner.m_aNearSlots.Clear();$e.Spawner.m_aNearSlots.Add([AICF_CatalogVehicleCandidateScope.Env]::NewSlot(21))}'bound-world'{[AICF_CatalogVehicleCandidateScope.Env]::Game.World=[AICF_CatalogVehicleCandidateScope.BaseWorld]::new()}'bound-user'{[AICF_CatalogVehicleCandidateScope.AICF_AIQuotaCatalogProbe]::s_User=[AICF_CatalogVehicleCandidateScope.AICF_AIQuotaCatalogIdentity]::Capture([AICF_CatalogVehicleCandidateScope.Env]::Entity(2),$false)}'bound-producer-component'{$e.Spawner.Owner.Spawner=[AICF_CatalogVehicleCandidateScope.Env]::NewSpawner(30)}'bound-slot-component'{$e.Slot.Owner.Slot=[AICF_CatalogVehicleCandidateScope.Env]::NewSlot(31)}'bound-entry-data'{$e.Entry.Data=[AICF_CatalogVehicleCandidateScope.SCR_EntityCatalogSpawnerData]::new()}'bound-quote'{$e.Entry.Data.Cost++}'bound-slot-unregistered'{$e.Spawner.m_aNearSlots.Clear()}'bound-funding-gate'{$e.Slot.Owner.Rpl.Value++;if($e.Spawner.FundingGate($e.Entry,$e.User,200)){throw 'Actual funding gate accepted stale slot identity'}}'bound-direct-producer-gate'{$e.Spawner.Owner.Spawner=[AICF_CatalogVehicleCandidateScope.Env]::NewSpawner(32);if($e.Spawner.FundingGate($e.Entry,$e.User,200)){throw 'Actual funding gate accepted replacement producer component'}}}}}
 };if($case.StartsWith('observer-')){$count++;continue};$actual=$e.Select();if($actual -ne $expected){throw "Actual selector mismatch case=$case actual=$actual expected=$expected"};if(!$expected -and $e.Spawner.Bindings -gt 0 -and !$case.StartsWith('bound-')){throw "Rejected candidate funded $case"};if($case -eq 'pending-ack' -and $e.Spawner.Bindings -ne 1){throw 'Repeated subscription binding'};if($case -eq 'cache-free' -and $e.Slot.OccupiedCalls -ne 0){throw 'Observer added occupancy query to native cache hit'};if($case -eq 'positive' -and ($e.Spawner.FreeCalls -ne 1 -or $e.Spawner.CacheSuperCalls -ne 1 -or $e.Spawner.ArraySuperCalls -ne 2 -or $e.Slot.TypeCalls -ne 1 -or $e.Slot.OccupiedCalls -ne 1 -or $e.Entry.Data.CompatibilityCalls -ne 1)){throw 'Observer duplicated native lookup/compatibility/occupancy call'};$count++}
$requests=0
foreach($case in @('request-positive','request-slot-stale','request-scope-stop','request-occupied','request-cache-occupied','request-user-stale')){
 $e=[AICF_CatalogVehicleCandidateScope.Env]::new();if($case -eq 'request-cache-occupied'){$e.Spawner.CacheFree=$e.Slot;$e.Slot.Occupied=$true}
 if(!$e.Select()){throw "Request setup selection failed $case"};[AICF_CatalogVehicleCandidateScope.AICF_AIQuotaCatalogProbe]::s_RequestedAt=123
 switch($case){
  'request-slot-stale'{$e.Slot.Owner.Rpl.Value++}
  'request-scope-stop'{$e.Entry.PrefabRead={[AICF_CatalogVehicleCandidateScope.AICF_AIQuotaCatalogProbe]::Finish($false,'injected-request-scope')}}
  'request-occupied'{$e.Slot.Occupied=$true}
  'request-user-stale'{[AICF_CatalogVehicleCandidateScope.AICF_AIQuotaCatalogProbe]::s_User=[AICF_CatalogVehicleCandidateScope.AICF_AIQuotaCatalogIdentity]::Capture([AICF_CatalogVehicleCandidateScope.Env]::Entity(99),$false)}
 }
 $e.Spawner.Request($e.Entry,$e.User,$e.Slot);$expected=0;if($case -eq 'request-positive'){$expected=1}
 if($e.Spawner.Factories -ne $expected -or $e.Spawner.Debits -ne $expected){throw "Actual request wrapper bypassed stale/native space veto $case"}
 if($case -in @('request-slot-stale','request-scope-stop','request-user-stale') -and $e.Spawner.RequestCalls -ne 0){throw "Stale wrapper reached native boundary $case"};$requests++
}
"PASS actual_vehicle_cases=$count includes_observer_cases=2 old_pinned_selector_replays=2 actual_request_cases=$requests source_scope=C1FF_ignored native_boundaries=SIMULATED WinPS_CSharp5=1"

$unknownCases=0
foreach($case in @('null-entity','null-physics','inactive','different-slot','foreign-world','id-stop','world-stop','class-stop','prefabdata-stop','prefabname-stop','id-frame-delete','class-frame-delete','prefabdata-frame-delete')){
 $e=[AICF_CatalogVehicleCandidateScope.Env]::new()
 $hit=[AICF_CatalogVehicleCandidateScope.Env]::Entity(99)
 $f=[AICF_CatalogVehicleCandidateScope.AICF_QuotaCatalogSlotObserver]::Begin($e.Spawner,$e.Entry,$e.Entry.Data,$e.User,'UNKNOWN_TEST');$f.m_LastSlot=$e.Slot
 $reason='NULL_PHYSICS';if($case-eq'null-entity'){$hit=$null;$reason='NULL_ENTITY'}
 if($case-eq'inactive'){[AICF_CatalogVehicleCandidateScope.AICF_QuotaCatalogSlotObserver]::Stop()}
 if($case-eq'different-slot'){$f.m_LastSlot=[AICF_CatalogVehicleCandidateScope.Env]::NewSlot(98)}
 if($case-eq'foreign-world'){$hit.World=[AICF_CatalogVehicleCandidateScope.BaseWorld]::new()}
 $reads=[Collections.Generic.List[string]]::new();$readsLocal=$reads;$caseLocal=$case
 $hitLocal=$hit;$entryLocal=$e.Entry
 $deleteHitLocal={$hitLocal.Removed=$true}.GetNewClosure()
 if($hit){$hit.MetadataRead={param($n) $readsLocal.Add($n);if($caseLocal-eq($n+'-stop')){[AICF_CatalogVehicleCandidateScope.AICF_QuotaCatalogSlotObserver]::Stop()};if($caseLocal-eq($n+'-frame-delete')){$entryLocal.DataRead=$deleteHitLocal}}.GetNewClosure();if($case-eq'prefabname-stop'){$hit.Prefab.Read={[AICF_CatalogVehicleCandidateScope.AICF_QuotaCatalogSlotObserver]::Stop()}}}
 $e.Slot.AICF_ObserveUnknownOccupancyHit($hit,$reason)
 if($case-in@('inactive','different-slot')){if([AICF_CatalogVehicleCandidateScope.AICF_AIQuotaCatalogProbe]::Logs-ne0){throw "Inactive/foreign slot observer logged $case"}}
 elseif($case.EndsWith('-stop') -or $case.EndsWith('-frame-delete')){
  if([AICF_CatalogVehicleCandidateScope.AICF_AIQuotaCatalogProbe]::LastLog-ne'SLOT_OBSERVER_SCOPE_LOST' -or [AICF_CatalogVehicleCandidateScope.AICF_QuotaCatalogSlotObserver]::s_Keys.ToArray().Length-ne0){throw "Unknown hit stale observer touched keys $case"}
  if($case-eq'class-stop' -and $reads.Contains('prefabdata')){throw 'Unknown hit read metadata after scope lost'}
 }else{
  $detail=[AICF_CatalogVehicleCandidateScope.AICF_AIQuotaCatalogProbe]::LastDetail
  if($detail-notmatch'kind=UNKNOWN_OCCUPANCY_HIT' -or !$detail.Contains('reason='+$reason) -or !$detail.Contains('metadata_after_decision=1 atomic_snapshot=0')){throw "Missing actual unknown hit reason/decision metadata $case"}
  if($case-eq'null-physics' -and (!$detail.Contains('class_after_decision=FixtureHit') -or !$detail.Contains('prefab_after_decision=fixture-hit.et'))){throw 'Unknown hit entity metadata missing'}
  if($case-eq'foreign-world' -and !$detail.Contains('metadata_available_snapshot=False')){throw 'Foreign world metadata accepted'}
 }
 if($e.Slot.OccupiedCalls-ne0 -or $e.Spawner.FreeCalls-ne0){throw 'Unknown observer repeated native occupancy/search'}
 $unknownCases++
}
"PASS actual_unknown_hit_cases=$unknownCases native_boundaries=SIMULATED"

} -RepositoryRoot $RepositoryRoot
