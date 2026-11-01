param([string]$RepositoryRoot=(Split-Path -Parent (Split-Path -Parent $PSScriptRoot)))
$ErrorActionPreference='Stop'
$auditor=Join-Path $RepositoryRoot 'tests/log-audits/Test-TankPhysicalLog.ps1'
$evidence=Join-Path $RepositoryRoot ('.codex-runtime/tank-audit-contract-'+[guid]::NewGuid().ToString('N'))
[void][IO.Directory]::CreateDirectory($evidence)
$positive=@'
[AICF][TANK_PHYSICAL_PROBE] event=CANONICAL_READY faction=US crew=4 initial_generation=1 before_coordinator_tick=1
[AICF][VEHICLE_SCENARIO_SELECTION] scenario=tank faction=US actual_faction=RHS_USAF prefab=tank.et native_seats=4 native_cost=3750
[AICF][VEHICLE_PRESET_PURCHASE] operation=trip-US-0-1 prefab=tank.et supplies=3750 base=29
[AICF][TANK_PHYSICAL_PROBE] event=START faction=US operation_id=trip-US-0-1 vehicle=0x123{} crew=4 prefab=tank.et initial_position=<1,2,3> deadline=900000
[AICF][VEHICLE_STATE_CHANGED] operation_id=trip-US-0-1 reason=DESTINATION_REACHED state=TACTICAL_DEPLOYMENT
[AICF][TANK_PHYSICAL_PROBE] event=READBACK faction=US phase=12 reason=DESTINATION_REACHED displacement_m=100 moving_polls=12 exact_crew=1
[AICF][TANK_PHYSICAL_PROBE] event=DONE faction=US operation_id=trip-US-0-1 vehicle=0x123{} displacement_m=100 moving_polls=12 crew=4 loader_retained=1 cargo_deployed=0 controlled_truce=1 actual_combat_threat=0
Game destroyed.
'@
foreach($rule in @('CANONICAL_OWNER','EXACT_RECIPE','EXACT_BOARDING','WAITING_MOUNTED','REMOTE_ORDER','TANK_EXPECTED_COMBAT_CREW','TANK_EXACT_NATIVE_CREW','TANK_REAL_NATIVE_MOVEMENT','TANK_COMBAT_ARRIVAL_CREW_RETAINED')){$positive+="`n[AICF][VEHICLE_SCENARIO_CHECK] scenario=tank rule=$rule passed=1"}
$launcher='AICF_RUNTIME_MANIFEST_JSON={"arguments":["-aicfTankPhysicalProbe","1","-aicfVehicleScenarioCanonicalOwner","1","-aicfVehicleScenario","tank","-aicfVehicleScenarioFaction","US"]}'
$gateRecords=@'
[AICF][TRANSPORT_NATIVE_GATE_STARTED] operation_id=trip-US-0-1 member=0x10{} seat=0:2:0 vehicle=0x123{} elapsed_ms=0 route_generation=1 assignment_revision=3 window_start_ms=500 window_deadline_ms=60500
[AICF][TRANSPORT_NATIVE_GATE_RETURNED] operation_id=trip-US-0-1 member=0x10{} seat=0:2:0 vehicle=0x123{} elapsed_ms=44180 route_generation=1 assignment_revision=3 window_start_ms=500 window_deadline_ms=60500 observed_exit=1 return_completed_receipt=1 perform_completed_receipt=1
'@
$positiveGate=$positive.Replace('[AICF][TANK_PHYSICAL_PROBE] event=READBACK',$gateRecords+"`n[AICF][TANK_PHYSICAL_PROBE] event=READBACK")
$exitBoundary='[AICF][TANK_GATE_EXIT_HANDOFF_PROBE] event=EXIT_HANDOFF_BOUNDARY operation_id=trip-US-0-1 vehicle=0x123{} member=0x10{} accepted=1 current_is_exit=1 current_completed=1 current_removed=1 exit_receipt=1 exit_receipt_ms=1000 perform_queued=1 return_queued=1 window_start_ms=500 window_deadline_ms=60500 observer_readonly=1 production_outcome_unchanged=1'
$positiveExit=$positiveGate.Replace('[AICF][TRANSPORT_NATIVE_GATE_RETURNED]',$exitBoundary+"`n[AICF][TRANSPORT_NATIVE_GATE_RETURNED]")
$handlerRecords=@'
[AICF][TANK_POSITIVE_HANDLER_PROBE] event=INJECTED operation_id=trip-US-0-1 vehicle=0x123{} record=1 pilot_handler=2 deadline=900000 attempts=0 lease_generation=1 vehicle_generation=1 injected=1 healthy=1 full_settled=1 controlled_fault=1 physical_mutation=0
[AICF][VEHICLE_NATIVE_SUBGROUP_POSITIVE_RECONCILE] operation_id=trip-US-0-1 group=0x9{} vehicle=0x123{} record=1 pilot_handler=2 record_members=2 pilot_members=1 scope=EXACT_OWN_POSITIVE_HANDLERS physical_mutation=0 native_join_calls=1
[AICF][VEHICLE_NATIVE_SUBGROUP_BOUND] operation_id=trip-US-0-1 group=0x9{} vehicle=0x123{} pilot=0x10{} record_before=1 pilot_before=2 handler=1 pilot_handler=1 joined=0
[AICF][TANK_POSITIVE_HANDLER_PROBE] event=DECISION operation_id=trip-US-0-1 vehicle=0x123{} accepted=1 record=1 pilot_handler=1 deadline=900000 attempts=0 lease_generation=1 vehicle_generation=1 original_record=1 created_handler=2 same_identity=1 controlled_fault=1 production_outcome_unchanged=1 physical_mutation=0
'@
$positiveHandler=$positive.Replace('[AICF][TANK_PHYSICAL_PROBE] event=START',$handlerRecords+"`n[AICF][TANK_PHYSICAL_PROBE] event=START")
$handlerManifest=($launcher.Substring('AICF_RUNTIME_MANIFEST_JSON='.Length)|ConvertFrom-Json);$handlerManifest.arguments+=@('-aicfTankPositiveHandlerFault','1')
$handlerLauncher='AICF_RUNTIME_MANIFEST_JSON='+($handlerManifest|ConvertTo-Json -Compress)
$cases=@(
 @{name='positive';text=$positive;expected=0},
 @{name='foreign_asset';text=$positive.Replace('vehicle=0x123{} displacement_m','vehicle=0x456{} displacement_m');expected=1},
 @{name='foreign_operation';text=$positive.Replace('event=DONE faction=US operation_id=trip-US-0-1','event=DONE faction=US operation_id=other');expected=1},
 @{name='wrong_crew';text=$positive.Replace('moving_polls=12 crew=4','moving_polls=12 crew=3');expected=1},
 @{name='cargo_loader_dismounted';text=$positive.Replace('loader_retained=1','loader_retained=0');expected=1},
 @{name='short_motion';text=$positive.Replace('displacement_m=100 moving_polls=12 crew=4','displacement_m=49 moving_polls=12 crew=4');expected=1},
 @{name='few_velocity_polls';text=$positive.Replace('moving_polls=12 crew=4','moving_polls=2 crew=4');expected=1},
 @{name='no_destination';text=$positive.Replace('reason=DESTINATION_REACHED','reason=STILL_MOVING');expected=1},
 @{name='unpaid';text=$positive.Replace('[VEHICLE_PRESET_PURCHASE]','[UNPAID]');expected=1},
 @{name='discounted';text=$positive.Replace('supplies=3750','supplies=1');expected=1},
 @{name='failed_check';text=$positive.Replace('rule=EXACT_BOARDING passed=1','rule=EXACT_BOARDING passed=0');expected=1},
 @{name='premature_crew_fallback';text=$positive.Replace('[AICF][TANK_PHYSICAL_PROBE] event=READBACK',"[AICF][VEHICLE_STATE_CHANGED] operation_id=trip-US-0-1 reason=CREW_UNAVAILABLE`n[AICF][TANK_PHYSICAL_PROBE] event=READBACK");expected=1},
 @{name='not_stopped';text=$positive.Replace('Game destroyed.','');expected=1},
 @{name='native_nonzero';text=$positive;expected=1;native=1},
 @{name='alternate_owner';text=$positive;expected=1;launcher=$launcher.Replace('-aicfVehicleScenarioCanonicalOwner','-otherOwner')},
 @{name='sibling_oom';text=$positive;expected=1;sibling='MEMORY (E): Out of memory'},
 @{name='sibling_resource';text=$positive;expected=1;sibling='RESOURCES (E): missing resource'},
 @{name='positive_native_gate';text=$positiveGate;expected=0;gate=$true},
 @{name='gate_missing';text=$positive;expected=1;gate=$true},
 @{name='gate_foreign_tank';text=$positiveGate.Replace('vehicle=0x123{} elapsed_ms=44180','vehicle=0x456{} elapsed_ms=44180');expected=1;gate=$true},
 @{name='gate_expired';text=$positiveGate.Replace('elapsed_ms=44180','elapsed_ms=60001');expected=1;gate=$true},
 @{name='gate_window_extended';text=$positiveGate.Replace('window_deadline_ms=60500 observed_exit','window_deadline_ms=60501 observed_exit');expected=1;gate=$true},
 @{name='gate_no_return_receipt';text=$positiveGate.Replace('return_completed_receipt=1','return_completed_receipt=0');expected=1;gate=$true},
 @{name='gate_no_exit';text=$positiveGate.Replace('observed_exit=1','observed_exit=0');expected=1;gate=$true},
 @{name='gate_route_changed';text=$positiveGate.Replace('elapsed_ms=44180 route_generation=1','elapsed_ms=44180 route_generation=2');expected=1;gate=$true},
 @{name='positive_exit_handoff';text=$positiveExit;expected=0;exitHandoff=$true},
 @{name='missing_exit_handoff';text=$positiveGate;expected=1;exitHandoff=$true},
 @{name='exit_proof_rejected';text=$positiveExit.Replace('accepted=1','accepted=0');expected=1;exitHandoff=$true},
 @{name='exit_not_current';text=$positiveExit.Replace('current_is_exit=1','current_is_exit=0');expected=1;exitHandoff=$true},
 @{name='exit_receipt_missing';text=$positiveExit.Replace('exit_receipt=1','exit_receipt=0');expected=1;exitHandoff=$true},
 @{name='exit_perform_not_queued';text=$positiveExit.Replace('perform_queued=1','perform_queued=0');expected=1;exitHandoff=$true},
 @{name='exit_foreign_actor';text=$positiveExit.Replace('member=0x10{} accepted','member=0x11{} accepted');expected=1;exitHandoff=$true},
 @{name='exit_late_receipt';text=$positiveExit.Replace('exit_receipt_ms=1000','exit_receipt_ms=60500');expected=1;exitHandoff=$true},
 @{name='exit_window_replaced';text=$positiveExit.Replace('return_queued=1 window_start_ms=500','return_queued=1 window_start_ms=501');expected=1;exitHandoff=$true},
 @{name='positive_handler_repair';text=$positiveHandler;expected=0;positiveHandler=$true},
 @{name='missing_handler_repair';text=$positive;expected=1;positiveHandler=$true},
 @{name='handler_no_optin';text=$positiveHandler;launcher=$launcher;expected=1;positiveHandler=$true},
 @{name='handler_scope_missing';text=$positiveHandler.Replace('[VEHICLE_NATIVE_SUBGROUP_POSITIVE_RECONCILE]','[MISSING_SCOPE]');expected=1;positiveHandler=$true},
 @{name='handler_scope_other_record';text=$positiveHandler.Replace('record=1 pilot_handler=2 record_members','record=3 pilot_handler=2 record_members');expected=1;positiveHandler=$true},
 @{name='handler_scope_other_pilot';text=$positiveHandler.Replace('record=1 pilot_handler=2 record_members','record=1 pilot_handler=3 record_members');expected=1;positiveHandler=$true},
 @{name='handler_native_readback_missing';text=$positiveHandler.Replace('[VEHICLE_NATIVE_SUBGROUP_BOUND]','[MISSING_READBACK]');expected=1;positiveHandler=$true},
 @{name='handler_zero_claim';text=$positiveHandler.Replace('record=1 pilot_handler=2 deadline','record=0 pilot_handler=2 deadline');expected=1;positiveHandler=$true},
 @{name='handler_deadline_extended';text=$positiveHandler.Replace('deadline=900000 attempts=0 lease_generation=1 vehicle_generation=1 original_record','deadline=900001 attempts=0 lease_generation=1 vehicle_generation=1 original_record');expected=1;positiveHandler=$true},
 @{name='handler_budget_reset';text=$positiveHandler.Replace('deadline=900000 attempts=0 lease_generation=1 vehicle_generation=1 original_record','deadline=900000 attempts=1 lease_generation=1 vehicle_generation=1 original_record');expected=1;positiveHandler=$true},
 @{name='handler_foreign_asset';text=$positiveHandler.Replace('event=DECISION operation_id=trip-US-0-1 vehicle=0x123{}','event=DECISION operation_id=trip-US-0-1 vehicle=0x456{}');expected=1;positiveHandler=$true},
 @{name='handler_pilot_not_rejoined';text=$positiveHandler.Replace('accepted=1 record=1 pilot_handler=1','accepted=1 record=1 pilot_handler=2');expected=1;positiveHandler=$true}
)
foreach($case in $cases){
 $root=Join-Path $evidence $case.name;[void][IO.Directory]::CreateDirectory($root)
 $log=Join-Path $root 'console.log';$launch=Join-Path $root 'launcher.txt';$exitPath=Join-Path $root 'native-exit.txt'
 $native=0;if($case.ContainsKey('native')){$native=$case.native}
 $cli=$launcher;if($case.ContainsKey('launcher')){$cli=$case.launcher}
 if($case.ContainsKey('positiveHandler') -and !$case.ContainsKey('launcher')){$cli=$handlerLauncher}
 [IO.File]::WriteAllText($log,$case.text);[IO.File]::WriteAllText($launch,$cli);[IO.File]::WriteAllText($exitPath,[string]$native)
 if($case.ContainsKey('sibling')){[IO.File]::WriteAllText((Join-Path $root 'error.log'),$case.sibling)}
 $cliArgs=@('-NoProfile','-ExecutionPolicy','Bypass','-File',$auditor,'-LogPath',$log,'-LauncherPath',$launch,'-NativeExitPath',$exitPath,'-Faction','US')
 if($case.ContainsKey('gate')){$cliArgs+='-RequireNativeGate'}
 if($case.ContainsKey('exitHandoff')){$cliArgs+='-RequireExitHandoff'}
 if($case.ContainsKey('positiveHandler')){$cliArgs+='-RequirePositiveHandler'}
 & powershell.exe @cliArgs *> (Join-Path $root 'audit.txt')
 if($LASTEXITCODE -ne $case.expected){throw "Tank parser case=$($case.name) exit=$LASTEXITCODE expected=$($case.expected) evidence=$root"}
 if($case.name -eq 'sibling_oom' -and ([IO.File]::ReadAllText((Join-Path $root 'audit.txt'))) -notmatch 'fatal=FAIL'){throw 'Sibling OOM hidden'}
}
"PASS Tank physical parser: $($cases.Count) synthetic controls; actual paid quote, same asset/operation, exact crew/loader, physical movement, arrival, full stopped sibling logs. Synthetic PASS is not native proof. Evidence=$evidence"
