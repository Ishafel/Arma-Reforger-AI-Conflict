param([string]$RepositoryRoot=(Split-Path -Parent (Split-Path -Parent $PSScriptRoot)))
$ErrorActionPreference='Stop'
$source=[IO.File]::ReadAllText((Join-Path $RepositoryRoot 'tests/fixtures/AICF_AIQuotaCompartmentProbe.c'))
$rules=@('aicfQuotaCompartmentProbe','AICF_MatchController.GetActiveController() != this','System.GetTickCount(m_iQCStarted) >= 300000','service.ReserveUnits(AICF_EAIQuotaBucket.STOCK, units, m_iQCSerial)','FULL_STOCK_ZERO_NATIVE','AICF_AIQuotaCompartmentProbe.s_Factories == 0','service.CancelReservation(m_QCPressure, m_iQCSerial)','m_QCFirst.SpawnCharacterInCompartment(m_sQCCharacter, m_QCGroup)','m_QCSecond.SpawnCharacterInCompartment(m_sQCCharacter, m_QCGroup)','source.m_Creation.m_Ticket.m_bClosed','OwnsCurrentCompartmentNewborn(source)','GetLedger().GetBindingBucket(source.m_CharacterBody.Key()) == AICF_EAIQuotaBucket.STOCK','m_iQCTailPolls >= 3','System.GetTickCount(m_iQCTailStarted) < 15000','GetCallqueue().Remove(AICF_QCClose)','super.Stop(cleanupEntities);','damage.GetDefaultHitZone().GetHealthScaled() != m_aQCHealth[index]','body.MatchesIdentity() && service.GetLedger().GetBindingBucket(body.Key()) == AICF_EAIQuotaBucket.STOCK','override bool CaptureCompartmentNewborn(AICF_AIQuotaCompartmentCreation context, IEntity raw)','override void ObserveCompartmentEntry(AICF_AIQuotaCompartmentCreation context, bool accepted)')
$rules+=@('bool current = super.IsCurrent();','return current;','native_row_present=%3','context_current=%3','can_group_null=%4')
foreach($rule in $rules){if(!$source.Contains($rule)){throw ('Compartment fixture boundary absent '+$rule)}}
$rules+=@('string actorId = source.m_Newborn.m_Id.ToString();','string actorRpl = source.m_Newborn.m_sRpl;','bool firstSettled = AICF_QCEntrySettled(0, m_QCFirst);','if (!firstSettled || !firstCensus || m_bQCFinished) return;','if (!firstReady || !secondReady || !bothCensus || m_bQCFinished) return;','m_iQCStage = 4;','bool captured = super.Capture(world);','bool built = super.BuildCounts(snapshot, counts);')
$rules+=@('m_iQCTailUnknownGeneration = AICF_AIQuotaCompartmentProbe.s_UnknownCaptures;','if (!tailCensus || m_iQCTailUnknownGeneration != AICF_AIQuotaCompartmentProbe.s_UnknownCaptures)','m_iQCTailPolls = 0;','TAIL_POST_CENSUS_IDENTITY_SEAT_HEALTH_FAILED','m_iQCStage = 3;')
$rules+=@('m_iQCInitialCensusWaits++;','no_reservation_or_factory=1')
$initialWait=$source.IndexOf('m_iQCInitialCensusWaits++;')
$firstReserve=$source.IndexOf('service.ReserveUnits(AICF_EAIQuotaBucket.STOCK, units, m_iQCSerial)')
if($initialWait-lt0-or$initialWait-ge$firstReserve-or$source.Substring($initialWait,$firstReserve-$initialWait)-notmatch'return;') {throw 'Initial UNKNOWN must return before reservation/factory without restarting300s'}
foreach($rule in $rules){if(!$source.Contains($rule)){throw ('Compartment asynchronous boundary absent '+$rule)}}
if($source-match'\b(?:ref\s+)?AICF_AIQuotaCensus\s+\w+\s*(?:=|;)'){throw 'Diagnostics must not retain an old census and hide weak-row expiry'}
# Actual observer boundary; native callback and final log sink are simulated.
$begin=$source.IndexOf('override void ObserveCompartmentEntry(')
$open=$source.IndexOf('{',$begin);$depth=1;$end=$open+1
while($depth-gt0){if($source[$end]-eq'{'){$depth++};if($source[$end]-eq'}'){$depth--};$end++}
$observer=$source.Substring($begin,$end-$begin).Replace('override void','public override void').Replace('super.','base.')
$observer=$observer.Replace('&& AICF_AIQuotaCompartmentProbe.s_Car &&','&& AICF_AIQuotaCompartmentProbe.s_Car != null &&').Replace('&& source &&','&& source != null &&').Replace('source.m_SeatScope &&','source.m_SeatScope != null &&').Replace('&& source.m_SeatScope.m_VehicleIdentity;','&& source.m_SeatScope.m_VehicleIdentity != null;').Replace('bool issued;','bool issued = false;').Replace('string carId;','string carId = null;')
$observer=[regex]::Replace($observer,'AICF_AIQuotaCompartmentProbe.Log\("GETIN_RESULT", string.Format\([\s\S]*?\)\);','Receipt = carId + ":" + issued + ":" + accepted;')
$shim=@'
using System;
public class Identity { public string m_Id="C1"; }
public class Scope { public object m_Vehicle; public Identity m_VehicleIdentity=new Identity(); }
public class AICF_AIQuotaCompartmentCreation { public Scope m_SeatScope; public bool m_bNativeIssued=true; }
public static class AICF_AIQuotaCompartmentProbe { public static Base s_Service; public static object s_Car; public static int s_Entries; }
public class Base { public Action<AICF_AIQuotaCompartmentCreation> Callback; public int Calls; public virtual void ObserveCompartmentEntry(AICF_AIQuotaCompartmentCreation context,bool accepted){Calls++;if(Callback!=null)Callback(context);} }
public class ActualObserver : Base { public string Receipt;
'@
$shim+=$observer+@'
public static void Run(){
 int controls=0;
 foreach(bool nativeAccepted in new[]{true,false}){
  var sut=new ActualObserver();var car=new object();var context=new AICF_AIQuotaCompartmentCreation{m_SeatScope=new Scope{m_Vehicle=car}};
  AICF_AIQuotaCompartmentProbe.s_Service=sut;AICF_AIQuotaCompartmentProbe.s_Car=car;AICF_AIQuotaCompartmentProbe.s_Entries=0;
  sut.Callback=c=>{c.m_SeatScope=null;AICF_AIQuotaCompartmentProbe.s_Car=null;};
  sut.ObserveCompartmentEntry(context,nativeAccepted);
  if(sut.Calls!=1||sut.Receipt!="C1:True:"+nativeAccepted||AICF_AIQuotaCompartmentProbe.s_Entries!=1)throw new Exception("postcallback observer dereference/result changed");controls++;
 }
 foreach(bool missingScope in new[]{true,false}){
  var sut=new ActualObserver();AICF_AIQuotaCompartmentProbe.s_Service=sut;AICF_AIQuotaCompartmentProbe.s_Car=null;AICF_AIQuotaCompartmentProbe.s_Entries=0;
  var context=new AICF_AIQuotaCompartmentCreation{m_SeatScope=missingScope?null:new Scope()};sut.ObserveCompartmentEntry(context,true);
  if(sut.Calls!=1||sut.Receipt!=null||AICF_AIQuotaCompartmentProbe.s_Entries!=0)throw new Exception("missing target logged");controls++;
 }
 Console.WriteLine("PASS actual observer callback controls="+controls);
}
}
'@
Add-Type -TypeDefinition $shim
[ActualObserver]::Run()
if($source-match'\.(SetHealth\w*|SetLifeState|SetOrigin|SetWorldTransform|ActivateAI|SetLimitOfActiveAIs|SetDormant\w*)\s*\('){throw 'Fixture mutates protected native state'}
. (Join-Path $RepositoryRoot 'tests/log-audits/Test-AIQuotaCompartmentLog.ps1') -LibraryOnly
$lines=@'
[AICF][QUOTA_COMPARTMENT_PROBE][SETUP] car=C1 car_rpl=CR1 character_prefab=char.et prefab_faction=US car_prefab=car.et
[AICF][QUOTA_COMPARTMENT_PROBE][CHECK] rule=FRESH_EMPTY_TWO_SEATS passed=1
[AICF][QUOTA_COMPARTMENT_PROBE][CHECK] rule=FULL_STOCK_ZERO_NATIVE passed=1
[AICF][QUOTA_COMPARTMENT_PROBE][CHECK] rule=FULL_STOCK_PRESERVED passed=1
[AICF][QUOTA_COMPARTMENT_PROBE][CHECK] rule=EXACT_PRESSURE_RELEASE passed=1
[AICF][QUOTA_COMPARTMENT_PROBE][BOUND] index=1 entity=A1 rpl=AR1 exact_current_creation=1 character_bound=1 car_bound=1 before_activate_getin=1
[AICF][QUOTA_COMPARTMENT_PROBE][GETIN_RESULT] index=1 accepted=1 marked_before_native=1 native_bool_observed=1 car=C1
[AICF][QUOTA_COMPARTMENT_PROBE][BODY] index=1 entity=A1 rpl=AR1 health=1 exact_seat=1 assigned_slot=0
[AICF][QUOTA_COMPARTMENT_PROBE][CHECK] rule=FIRST_ACTUAL_NEWBORN passed=1
[AICF][QUOTA_COMPARTMENT_PROBE][ENTRY_READINESS] index=1 entity=A1 captured_rpl=AR1 actual_rpl=AR1 exact_seat=1 getting_in=0 getting_out=0 stage=2
[AICF][QUOTA_COMPARTMENT_PROBE][CHECK] rule=FIRST_ENTRY_SETTLED passed=1
[AICF][QUOTA_COMPARTMENT_PROBE][BOUND] index=2 entity=A2 rpl=AR2 exact_current_creation=1 character_bound=1 car_bound=1 before_activate_getin=1
[AICF][QUOTA_COMPARTMENT_PROBE][GETIN_RESULT] index=2 accepted=1 marked_before_native=1 native_bool_observed=1 car=C1
[AICF][QUOTA_COMPARTMENT_PROBE][BODY] index=2 entity=A2 rpl=AR2 health=1 exact_seat=1 assigned_slot=1
[AICF][QUOTA_COMPARTMENT_PROBE][CHECK] rule=SECOND_SAME_CAR passed=1
[AICF][QUOTA_COMPARTMENT_PROBE][CHECK] rule=ACTUAL_FACTORY_ENTRY_COUNT passed=1
[AICF][QUOTA_COMPARTMENT_PROBE][ENTRY_READINESS] index=1 entity=A1 captured_rpl=AR1 actual_rpl=AR1 exact_seat=1 getting_in=0 getting_out=0 stage=3
[AICF][QUOTA_COMPARTMENT_PROBE][ENTRY_READINESS] index=2 entity=A2 captured_rpl=AR2 actual_rpl=AR2 exact_seat=1 getting_in=0 getting_out=0 stage=3
[AICF][QUOTA_COMPARTMENT_PROBE][CHECK] rule=BOTH_ENTRIES_SETTLED passed=1
[AICF][QUOTA_COMPARTMENT_PROBE][TAIL_WINDOW_STARTED] window=1 started_elapsed_ms=15000 unknown_generation=0 reset_count=0 known_census=1
[AICF][QUOTA_COMPARTMENT_PROBE][CHECK] rule=EXACT_TWO_STOCK_CHARACTERS passed=1
[AICF][QUOTA_COMPARTMENT_PROBE][CHECK] rule=ONE_STOCK_VEHICLE_HOLD passed=1
[AICF][QUOTA_COMPARTMENT_PROBE][CHECK] rule=SETTLED_TAIL passed=1
[AICF][QUOTA_COMPARTMENT_PROBE][TAIL] actual_tail_ms=15000 distinct_polls=3 exact_healthy_bodies=2 exact_seats=2 exact_character_bindings=2 exact_car_bodies=1
[AICF][QUOTA_COMPARTMENT_PROBE][TAIL_WINDOW_COMPLETED] window=1 unknown_generation=0 reset_count=0 elapsed_ms=30000 known_census=1
[AICF][QUOTA_COMPARTMENT_PROBE][DONE] passed=1 reason=ACTUAL_NEWBORN_HETEROGENEOUS_BASKET checks=10 failures=0 factories=2 births=2 entries=2 tail_polls=3 elapsed_ms=30000
'@
$cases=1
if(@(Test-AICFCompartmentProof ($lines-split"`n")).Count){throw 'Strict native compartment positive rejected'}
foreach($change in @(@('reason=ACTUAL_NEWBORN_HETEROGENEOUS_BASKET','reason=BOUNDED_TIMEOUT'),@('factories=2','factories=3'),@('entries=2','entries=1'),@('rule=FULL_STOCK_ZERO_NATIVE passed=1','rule=FULL_STOCK_ZERO_NATIVE passed=0'),@('rule=EXACT_PRESSURE_RELEASE passed=1','rule=EXACT_PRESSURE_RELEASE passed=0'),@('before_activate_getin=1','before_activate_getin=0'),@('car_bound=1','car_bound=0'),@('exact_current_creation=1','exact_current_creation=0'),@('accepted=1','accepted=0'),@('marked_before_native=1','marked_before_native=0'),@('native_bool_observed=1 car=C1','native_bool_observed=1 car=C9'),@('rpl=AR2','rpl=AR1'),@('entity=A2','entity=A1'),@('health=1','health=NaN'),@('health=1','health=Infinity'),@('health=1','health=0'),@('actual_tail_ms=15000','actual_tail_ms=14999'),@('distinct_polls=3','distinct_polls=2'),@('exact_car_bodies=1','exact_car_bodies=2'),@('exact_character_bindings=2','exact_character_bindings=1'),@('failures=0','failures=1'))){
 if(@(Test-AICFCompartmentProof (($lines.Replace($change[0],$change[1]))-split"`n")).Count-eq0){throw ('Unsafe native receipt accepted '+$change[0])};$cases++
}
$duplicate=$lines+"`n"+($lines-split"`n"|Where-Object{$_-match'\[DONE\]'})
if(@(Test-AICFCompartmentProof ($duplicate-split"`n")).Count-eq0){throw 'Duplicate DONE accepted'};$cases++
foreach($change in @(@('actual_rpl=AR1','actual_rpl=CHANGED'),@('exact_seat=1 getting_in=0 getting_out=0 stage=2','exact_seat=0 getting_in=1 getting_out=0 stage=2'),@('stage=2','stage=9'),@('rule=FIRST_ENTRY_SETTLED passed=1','rule=FIRST_ENTRY_SETTLED passed=0'),@('rule=BOTH_ENTRIES_SETTLED passed=1','rule=BOTH_ENTRIES_SETTLED passed=0'),@('exact_seat=1 getting_in=0 getting_out=0 stage=3','exact_seat=0 getting_in=1 getting_out=0 stage=3'))){
 if(@(Test-AICFCompartmentProof (($lines.Replace($change[0],$change[1]))-split"`n")).Count-eq0){throw ('Unsafe asynchronous receipt accepted '+$change[0])};$cases++
}
$resetPositive=$lines.Replace('[AICF][QUOTA_COMPARTMENT_PROBE][TAIL_WINDOW_STARTED] window=1 started_elapsed_ms=15000 unknown_generation=0 reset_count=0 known_census=1',@'
[AICF][QUOTA_COMPARTMENT_PROBE][TAIL_WINDOW_STARTED] window=1 started_elapsed_ms=10000 unknown_generation=0 reset_count=0 known_census=1
[AICF][QUOTA_COMPARTMENT_PROBE][CENSUS_REJECTED] reason=NATIVE_CENSUS_MISMATCH
[AICF][QUOTA_COMPARTMENT_PROBE][TAIL_RESET] window=1 held_ms=1000 unknown_before=0 unknown_after=1 reset_count=1 counted_active=32 native_active=31 exact_bodies_seats_health=1
[AICF][QUOTA_COMPARTMENT_PROBE][TAIL_WINDOW_STARTED] window=2 started_elapsed_ms=15000 unknown_generation=1 reset_count=1 known_census=1
'@).Replace('TAIL_WINDOW_COMPLETED] window=1 unknown_generation=0 reset_count=0','TAIL_WINDOW_COMPLETED] window=2 unknown_generation=1 reset_count=1')
if(@(Test-AICFCompartmentProof ($resetPositive-split"`n")).Count){throw 'Continuous known window after observer reset rejected'};$cases++
foreach($change in @(@('started_elapsed_ms=15000','started_elapsed_ms=16000'),@('TAIL_WINDOW_COMPLETED] window=1 unknown_generation=0','TAIL_WINDOW_COMPLETED] window=1 unknown_generation=1'),@('known_census=1','known_census=0'),@('reset_count=0 elapsed_ms=30000','reset_count=1 elapsed_ms=30000'))){
 if(@(Test-AICFCompartmentProof (($lines.Replace($change[0],$change[1]))-split"`n")).Count-eq0){throw ('Unsafe known-window receipt accepted '+$change[0])};$cases++
}
$interrupted=$lines.Replace('[AICF][QUOTA_COMPARTMENT_PROBE][TAIL]','[AICF][QUOTA_COMPARTMENT_PROBE][CENSUS_REJECTED] reason=NATIVE_CENSUS_MISMATCH'+"`n"+'[AICF][QUOTA_COMPARTMENT_PROBE][TAIL]')
if(@(Test-AICFCompartmentProof ($interrupted-split"`n")).Count-eq0){throw 'Unknown inside final claimed stable window accepted'};$cases++
if(@(Test-AICFCompartmentProof (($resetPositive.Replace('exact_bodies_seats_health=1','exact_bodies_seats_health=0'))-split"`n")).Count-eq0){throw 'Reset without actual unchanged physical proof accepted'};$cases++
$cliRoot=Join-Path $RepositoryRoot ('.codex-runtime/compartment-auditor-cli-'+[guid]::NewGuid().ToString('N'))
[void][IO.Directory]::CreateDirectory($cliRoot)
$auditor=Join-Path $RepositoryRoot 'tests/log-audits/Test-AIQuotaCompartmentLog.ps1'
$nativeCli='ENGINE : CLI Params: -gproj C:\fixture\AIConflictArland\addon.gproj -aicfQuotaCompartmentProbe 1'
$complete=$nativeCli+"`nSCRIPT : [AICF][ROSTER_READY] actual=1`n"+$lines+"`nENGINE : Game destroyed.`n"
$cliCases=@(
 @{name='positive';text=$complete;sibling='';exit=0;want=0},
 @{name='engineFatalSibling';text=$complete;sibling='ENGINE (F): Access violation. Illegal read by ffffffff';exit=0;want=1},
 @{name='guiErrorSibling';text=$complete;sibling='GUI (E): Missing font GUID';exit=0;want=1},
 @{name='unknownNativeError';text=$complete;sibling='FOOBAR (E): unknown native error';exit=0;want=1},
 @{name='nullException';text=$complete;sibling='NULL pointer to instance in script callback';exit=0;want=1},
 @{name='vmException';text=$complete;sibling='VM : Unhandled exception';exit=0;want=1},
 @{name='virtualMachineException';text=$complete;sibling='Virtual Machine Exception: invalid argument';exit=0;want=1},
 @{name='applicationCrash';text=$complete;sibling='Application crashed with exit code C0000005';exit=0;want=1},
 @{name='warningAllowed';text=$complete;sibling='ENGINE (W): optional native warning';exit=0;want=0},
 @{name='notStopped';text=$complete.Replace('ENGINE : Game destroyed.','');sibling='';exit=0;want=1},
 @{name='missingCli';text=$complete.Replace($nativeCli,'');sibling='';exit=0;want=1},
 @{name='wrongCli';text=$complete.Replace('-aicfQuotaCompartmentProbe 1','-aicfQuotaCompartmentProbe 0');sibling='';exit=0;want=1},
 @{name='notReady';text=$complete.Replace('[ROSTER_READY]','[NOT_READY]');sibling='';exit=0;want=1},
 @{name='nativeFailed';text=$complete;sibling='';exit=-1;want=1}
)
foreach($case in $cliCases){
 $folder=Join-Path $cliRoot $case.name;[void][IO.Directory]::CreateDirectory($folder)
 $log=Join-Path $folder 'console.log';[IO.File]::WriteAllText($log,$case.text)
 [IO.File]::WriteAllText((Join-Path $folder 'sibling.log'),$case.sibling)
 & C:/Windows/System32/WindowsPowerShell/v1.0/powershell.exe -NoProfile -ExecutionPolicy Bypass -File $auditor -LogPath $log -NativeExitCode $case.exit -EvidenceRoot $folder *> (Join-Path $folder 'output.txt')
 if($LASTEXITCODE-ne$case.want){throw ('Actual WinPS5.1 auditor CLI wrong exit '+$case.name+' at '+$folder)}
 $actual=Get-Content -LiteralPath (Join-Path $folder 'compartment-audit.json') -Raw|ConvertFrom-Json
 if($case.name-in@('engineFatalSibling','nullException','vmException','virtualMachineException','applicationCrash')-and$actual.full_sibling_fatal_count-lt1){throw ('Fatal row incorrectly classified '+$case.name)}
 if($case.name-in@('guiErrorSibling','unknownNativeError')-and$actual.resource_or_other_error_count-lt1){throw ('Native error incorrectly classified '+$case.name)}
}
"PASS compartment native fixture source=$($rules.Count) strict_log_cases=$cases actual_winps51_cli_cases=$($cliCases.Count) cli_evidence=$cliRoot native_runtime=NOT_RUN"
