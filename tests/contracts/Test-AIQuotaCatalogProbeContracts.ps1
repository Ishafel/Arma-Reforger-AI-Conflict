param([string]$RepositoryRoot=(Split-Path -Parent (Split-Path -Parent $PSScriptRoot)))
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot '../lib/Stage3StaticAudit.Common.ps1')
$s=[IO.File]::ReadAllText((Join-Path $RepositoryRoot 'tests/fixtures/AICF_AIQuotaCatalogProbe.c'))
function Body($name){Get-AICFMethodBody ([pscustomobject]@{Source=$s;Code=(ConvertTo-AICFCodeText $s)}) $name}
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
foreach($name in @('Capture','ReadQuotaCharacterCustody','BuildCounts')){
 $body=Body $name
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
 public class IEntity:N{public EntityID GetID(){return new EntityID();}}
 public class EntityID:N{}
 public class SCR_EntityCatalogEntry:N{} public class SCR_MilitaryBaseComponent:N{public IEntity GetOwner(){return new IEntity();}} public static class AICF_AIQuotaCatalogResourceObservation{public static SCR_ResourceGenerator s_Generator;public static SCR_ResourceConsumer s_Consumer;public static void Stop(){s_Generator=null;s_Consumer=null;}public static void Links(SCR_ResourceGenerator g,SCR_ResourceConsumer c,string stage){}}
 public class SCR_ResourceConsumer:N{}
 public class SCR_ResourceGenerator:N{public Type Type(){return GetType();}}
 public enum EResourceGeneratorID{DEFAULT} public enum EResourceType{SUPPLIES}
 public class SCR_ResourceComponent:N{public SCR_ResourceConsumer Consumer=new SCR_ResourceConsumer();public SCR_ResourceGenerator Generator=new SCR_ResourceGenerator();public SCR_ResourceConsumer GetConsumer(EResourceGeneratorID i,EResourceType t){return Consumer;}public SCR_ResourceGenerator GetGenerator(EResourceGeneratorID i,EResourceType t){return Generator;}}
 public class AICF_AIQuotaCatalogIdentity:N{public EntityID m_Id=new EntityID();public static AICF_AIQuotaCatalogIdentity Capture(IEntity e,bool m){return e?new AICF_AIQuotaCatalogIdentity():null;}}
 public static class AICF_AIQuotaCatalogProbe{public static bool s_FundingAttempted,Finished;public static int Logs;public static void Finish(bool passed,string why){Finished=true;}public static void Log(string e,string d){Logs++;}}
 public class Probe{public SCR_ResourceComponent Resource=new SCR_ResourceComponent();public bool Guard=true,Noop,Overfund,InvalidateAfter,NaNAfter;public int Calls,GuardCalls,RejectAt;public void GetBases(System.Collections.Generic.List<SCR_MilitaryBaseComponent> bases){}public float Balance=50;public IEntity GetOwner(){return new IEntity();}public SCR_ResourceComponent GetSpawnerResourceComponent(){return Resource;}public float GetSpawnerResourceValue(){return Balance;}public bool AICF_QuotaProbeFundingCurrent(AICF_AIQuotaCatalogIdentity p,SCR_ResourceComponent r,SCR_ResourceConsumer c,SCR_ResourceGenerator g,SCR_EntityCatalogEntry e,IEntity u,int q){GuardCalls++;return Guard&&p&&r&&c&&g&&GuardCalls!=RejectAt;}public void AddSpawnerSupplies(float amount){Calls++;if(!Noop)Balance+=amount;if(Overfund)Balance++;if(NaNAfter)Balance=float.NaN;if(InvalidateAfter)Guard=false;}public bool AICF_QuotaProbeFund(SCR_EntityCatalogEntry entry,IEntity user,int quote)BODY}
 public static class Cases{public static int Run(){int n=0;foreach(string kind in new[]{"valid","missingResource","missingGenerator","staleBefore","staleAfter","nativeNoop","overfund","duplicate","alreadyFull","lastGuard","beforeNaN","beforePositiveInfinity","beforeNegativeInfinity","afterNaN"}){var p=new Probe();AICF_AIQuotaCatalogProbe.s_FundingAttempted=kind=="duplicate";AICF_AIQuotaCatalogProbe.Finished=false;AICF_AIQuotaCatalogProbe.Logs=0;switch(kind){case"missingResource":p.Resource=null;break;case"missingGenerator":p.Resource.Generator=null;break;case"staleBefore":p.Guard=false;break;case"staleAfter":p.InvalidateAfter=true;break;case"nativeNoop":p.Noop=true;break;case"overfund":p.Overfund=true;break;case"alreadyFull":p.Balance=100;break;case"lastGuard":p.RejectAt=2;break;case"beforeNaN":p.Balance=float.NaN;break;case"beforePositiveInfinity":p.Balance=float.PositiveInfinity;break;case"beforeNegativeInfinity":p.Balance=float.NegativeInfinity;break;case"afterNaN":p.NaNAfter=true;break;}bool result=p.AICF_QuotaProbeFund(new SCR_EntityCatalogEntry(),new IEntity(),100);bool called=kind=="valid"||kind=="staleAfter"||kind=="nativeNoop"||kind=="overfund"||kind=="afterNaN";if(result!=(kind=="valid")||p.Calls!=(called?1:0)||AICF_AIQuotaCatalogProbe.Finished!=(kind=="staleAfter"||kind=="nativeNoop"||kind=="overfund"||kind=="afterNaN"))throw new Exception(kind);if(called){p.AICF_QuotaProbeFund(new SCR_EntityCatalogEntry(),new IEntity(),100);if(p.Calls!=1)throw new Exception("repeated "+kind);}n++;}return n;}}
}
'@
$fundBody=$fund.Replace('Math.AbsFloat','Math.Abs').Replace('array<SCR_MilitaryBaseComponent> bases = {};','System.Collections.Generic.List<SCR_MilitaryBaseComponent> bases = new System.Collections.Generic.List<SCR_MilitaryBaseComponent>();').Replace('bases.Count()','bases.Count').Replace('EntityID baseId;','EntityID baseId=null;').Replace('IEntity baseOwner;','IEntity baseOwner=null;').Replace('for (int baseIndex;','for (int baseIndex=0;')
$fundCode=$fundCode.Replace('BODY',$fundBody)
Add-Type -TypeDefinition $fundCode
$fundCases=[CatalogFundingReplay.Cases]::Run()
$fundUnsafe=0
foreach($mutation in @(@('once','if (AICF_AIQuotaCatalogProbe.s_FundingAttempted) return false;',''),@('readback','!(after >= quote && Math.Abs(after - before - shortfall) <= 0.01)','AICF_AIQuotaCatalogProbe.Finished'),@('identity','!AICF_QuotaProbeFundingCurrent(producer, resource, consumer, generator, entry, user, quote)','AICF_AIQuotaCatalogProbe.Finished'))){
 if(!$fundBody.Contains($mutation[1])){throw "Missing actual funding mutant $($mutation[0])"};$ns='CatalogFundingUnsafe'+$mutation[0];$mutant=$fundCode.Replace('namespace CatalogFundingReplay','namespace '+$ns).Replace($fundBody,$fundBody.Replace($mutation[1],$mutation[2]));Add-Type -TypeDefinition $mutant;$caught=$false;try{([type]($ns+'.Cases')).GetMethod('Run').Invoke($null,@())|Out-Null}catch{$caught=$true};if(!$caught){throw "Controlled funding unsafe survived $($mutation[0])"};$fundUnsafe++
}
"PASS catalog actual_controlled_funding_cases=$fundCases executed_unsafe=$fundUnsafe native_generator_only=1"

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
