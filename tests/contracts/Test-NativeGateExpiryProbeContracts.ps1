param([string]$RepositoryRoot=(Split-Path -Parent (Split-Path -Parent $PSScriptRoot)))
$ErrorActionPreference='Stop'
$fixture=[IO.File]::ReadAllText((Join-Path $RepositoryRoot 'tests/fixtures/AICF_NativeGateExpiryProbe.c'))
function SourceRules([string]$source){
 if($source -match '\b(?:m_iStarted|m_iDeadline|m_bReturnCompleted|m_iReturnCompletedAtMs|m_iReturnedAtMs|m_Return|m_Exit|m_Perform)\s*=(?!=)|SetOrigin\(|SetWorldTransform\(|SetHealth\(|SetDamage\(|ChangeSimulationState\(|SetVelocity\(|TryGetOutVehicle\(|AddAction\(|new SCR_AI|\.BeginMobilityRecovery\('){'NO_FAKE_NATIVE_HISTORY_OR_PHYSICAL_EFFECT'}
 foreach($guard in @('if (m_bAICFGEPForcedExpiry) return true;','bool natural = super.IsExpired(now);','AICF_NativeGateExpiryProbe.s_iInjectedAt > 0','AICF_NativeGateExpiryProbe.s_Trip != m_Trip','!Matches(m_Trip)','!m_bObservedExit || !m_bReturnCompleted','m_iReturnCompletedAtMs <= 0','m_iReturnedAtMs < m_iReturnCompletedAtMs','m_iReturnedAtMs <= 0','m_iReturnedAtMs > now','!AICF_VehicleShortRecovery.IsSlowVehicle(m_Vehicle)')){if(!$source.Contains($guard)){'ACTUAL_RETURN_AND_ONE_STICKY_EXPIRY';break}}
 foreach($guard in @('state.GetMobilityRecoveryAttempts() == AICF_NativeGateExpiryProbe.s_iAttempts + 1','state.GetMaximumMobilityRecoveryAttempts() <= 2','trip.GetAbsoluteDeadlineMs() == AICF_NativeGateExpiryProbe.s_iDeadline','state.GetRouteGeneration() <= AICF_NativeGateExpiryProbe.s_iRoute','AICF_VSRetainedCurrent(slot, trip)','AICF_VSExact(slot)','now + 1000','physics.GetVelocity().Length() >= 0.5','distance < 25 || m_iGEPMovingPolls < 3')){if(!$source.Contains($guard)){'ACTUAL_BOUNDED_MOTION_AND_IDENTITY';break}}
 foreach($guard in @('System.GetCLIParam("aicfNativeGateExpiryGateHQ", setup)','faction.GetMainBase().GetSpawnPoint().GetOrigin()','smart.GetTags(tags)','tags.Contains(SCR_AIGoalReaction_OpenNavlinkDoor.SMART_ACTION_TAG)','door.SetControlValue(0, RplId.Invalid())','AICF_NativeGateExpiryProbe.s_bDoorCloseRequested = true;','Math.AbsFloat(door.GetNormalizedDoorState()) <= 0.01','m_SmartOwner != AICF_NativeGateExpiryProbe.s_SetupGate','!AICF_NativeGateExpiryProbe.s_SetupGate ||','AICF_NativeGateExpiryProbe.s_SetupGate = m_SmartOwner;')){if(!$source.Contains($guard)){'OPTIN_ACTUAL_NATIVE_DOOR_SETUP';break}}
 if(!$source.Contains('AICF_VehicleBehaviorBinding.ComponentRpl(m_Vehicle)') -or !$source.Contains('injected_at_ms=%7 gate_node_rpl=%8') -or !$source.Contains('position=%8 observed_at_ms=%9')){'EXPLICIT_NODE_COMPONENT_CLOCKS'}
}
$issues=@(SourceRules $fixture);if($issues.Count){throw "Gate expiry fixture: $issues"}
$sourceControls=1
foreach($item in @(
 @('if (m_bAICFGEPForcedExpiry) return true;','','ACTUAL_RETURN_AND_ONE_STICKY_EXPIRY'),
 @('AICF_NativeGateExpiryProbe.s_iInjectedAt > 0','false','ACTUAL_RETURN_AND_ONE_STICKY_EXPIRY'),
 @('!m_bObservedExit || !m_bReturnCompleted','false','ACTUAL_RETURN_AND_ONE_STICKY_EXPIRY'),
 @('m_iReturnedAtMs < m_iReturnCompletedAtMs','false','ACTUAL_RETURN_AND_ONE_STICKY_EXPIRY'),
 @('state.GetMaximumMobilityRecoveryAttempts() <= 2','true','ACTUAL_BOUNDED_MOTION_AND_IDENTITY'),
 @('distance < 25 || m_iGEPMovingPolls < 3','false','ACTUAL_BOUNDED_MOTION_AND_IDENTITY'),
 @('m_SmartOwner != AICF_NativeGateExpiryProbe.s_SetupGate','false','OPTIN_ACTUAL_NATIVE_DOOR_SETUP'),
 @('!AICF_NativeGateExpiryProbe.s_SetupGate ||','','OPTIN_ACTUAL_NATIVE_DOOR_SETUP'),
 @('AICF_VehicleBehaviorBinding.ComponentRpl(m_Vehicle)','m_sVehicleRpl','EXPLICIT_NODE_COMPONENT_CLOCKS')
)){$changed=$fixture.Replace($item[0],$item[1]);if($changed -ceq $fixture -or $item[2] -notin @(SourceRules $changed)){throw "Surviving fixture mutant $($item[2])"};$sourceControls++}
foreach($injection in @('m_iStarted = now;','m_bReturnCompleted = true;','m_Vehicle.SetOrigin(vector.Zero);')){if('NO_FAKE_NATIVE_HISTORY_OR_PHYSICAL_EFFECT' -notin @(SourceRules ($fixture+"`n"+$injection))){throw 'Mutation survived'};$sourceControls++}
$taskRoot=Join-Path $RepositoryRoot ('.codex-runtime/gate-expiry-contract-'+[guid]::NewGuid().ToString('N'))
[void][IO.Directory]::CreateDirectory($taskRoot)
$taskLog=Join-Path $taskRoot 'console.log';$taskLauncher=Join-Path $taskRoot 'launcher.txt';$taskExit=Join-Path $taskRoot 'native-exit.txt'
'AICF_RUNTIME_MANIFEST_JSON={"arguments":["-aicfVehicleScenarioRetainedBehavior","1","-aicfNativeGateExpiryProbe","1"]}'|Set-Content -LiteralPath $taskLauncher
'0'|Set-Content -LiteralPath $taskExit
$operation='trip-US-0-1-1';$identity="operation_id=$operation vehicle=0x10{} vehicle_rpl=42 gate_node_rpl=84"
$prefix='[AICF][NATIVE_GATE_EXPIRY_PROBE] classification=CONTROLLED_GATE_EXPIRY_ONLY natural_expiry_proof=0 '+$identity+' '
$gate="$identity member=0x20{} seat=0:3:2 smart_owner=0x30{} smart_tag=OpenGate window_start_ms=10000 window_deadline_ms=70000 progress_written=0 "
$history='observed_exit=1 return_completed_receipt=1 return_completed_at_ms=35000 returned_at_ms=36000'
$lines=@('CLI Params: -server','[ROSTER_READY]',('[VEHICLE_PRESET_PURCHASE] operation='+$operation),($prefix+'event=START t_ms=20000 trip_deadline_ms=1800000'))
$lines+='[TRANSPORT_NATIVE_GATE_STARTED] '+$gate+'observed_exit=0 return_completed_receipt=0 return_completed_at_ms=0 returned_at_ms=0'
$lines+=($prefix+'event=EXPIRED t_ms=40000 injected_at_ms=40000 member=0x20{} seat=0:3:2 smart_owner=0x30{} route_generation=1 attempts=0 maximum=2 trip_deadline_ms=1800000 window_start_ms=10000 window_deadline_ms=70000 '+$history+' native_history_actual=1 clocks_written=0 actions_issued=0 physical_correction=0')
$lines+='[TRANSPORT_NATIVE_GATE_CONTROLLED_EXPIRY] '+$gate+$history
$lines+='[TRANSPORT_NATIVE_GATE_RECOVERY_REQUESTED] '+$gate+$history
$lines+='[VEHICLE_RECOVERY_STARTED] '+$identity+' reason=NATIVE_GATE_WINDOW_EXHAUSTED mobility_attempt=1 unstuck_mode=ROUTE_REBUILD_ONLY'
$lines+=($prefix+'event=REPAIR t_ms=41000 observed_at_ms=41000 route_generation=2 attempts=1 maximum=2 trip_deadline_ms=1800000 delay_ms=1000 position=<0,0,0>')
$lines+=($prefix+'event=MOVED t_ms=50000 observed_at_ms=50000 displacement_m=30 moving_polls=3 attempts=1 maximum=2 trip_deadline_ms=1800000 original_roster=1 one_purchase=1 position=<30,0,0> speed_mps=1 exact_crew=1')
foreach($pass in 1,2){for($index=0;$index -lt 5;$index++){$lines+="[VEHICLE_SCENARIO_RETAINED_SEAT] leg=1 position=$index seat=0:3:$index member=0x2$index{} occupant=0x2$index{}"}}
foreach($rule in @('GEP_ONE_BOUNDED_ROUTE_REPAIR','GEP_NATIVE_25M_THREE_POLLS_SAME_ASSET')){$lines+="[VEHICLE_SCENARIO_CHECK] rule=$rule passed=1"}
$lines+='Game destroyed.'
$good=$lines -join "`n"
$logControls=0
function Control([string]$name,[string]$text,[int]$exitCode,[string]$reason='', [string]$sibling=''){
 $text|Set-Content -LiteralPath $taskLog
 $taskSibling=Join-Path $taskRoot 'error.log'
 if(Test-Path -LiteralPath $taskSibling){Remove-Item -LiteralPath $taskSibling}
 if($sibling){$sibling|Set-Content -LiteralPath $taskSibling}
 $output=& powershell.exe -NoProfile -ExecutionPolicy Bypass -File (Join-Path $RepositoryRoot 'tests/log-audits/Test-NativeGateExpiryLog.ps1') -LogPath $taskLog -LauncherPath $taskLauncher -NativeExitPath $taskExit 2>&1
 if($LASTEXITCODE -ne $exitCode){throw "Control $name exit=$LASTEXITCODE expected=$exitCode output=$output"}
 $result=($output -join "`n")|ConvertFrom-Json
 if($reason -and $reason -notin $result.failures){throw "Missing failure $reason in $name"}
 $script:logControls++
 return $result
}
try{
 $null=Control good $good 0
 $null=Control trace-clock-noise ($good.Replace('event=EXPIRED t_ms=40000','event=EXPIRED t_ms=40002').Replace('event=REPAIR t_ms=41000','event=REPAIR t_ms=41002')) 0
 foreach($case in @(
  @('node-component-confusion',$good.Replace(($identity+' event=EXPIRED'),($identity.Replace('vehicle_rpl=42','vehicle_rpl=84')+' event=EXPIRED')),'SAME_TRIP_CAR_RPL'),
  @('missing-history',$good.Replace('return_completed_receipt=1','return_completed_receipt=0'),'ACTUAL_NATIVE_RETURN_HISTORY'),
  @('reversed-history',$good.Replace('returned_at_ms=36000','returned_at_ms=34000'),'ACTUAL_NATIVE_RETURN_HISTORY'),
  @('extra-budget',$good.Replace('attempts=1 maximum=2','attempts=2 maximum=2'),'ONE_BOUNDED_ROUTE_REPAIR'),
  @('renew-trip',$good.Replace('trip_deadline_ms=1800000 original_roster=1','trip_deadline_ms=1900000 original_roster=1'),'UNCHANGED_TRIP_DEADLINE'),
  @('renew-window',$good.Replace('[TRANSPORT_NATIVE_GATE_RECOVERY_REQUESTED] '+$gate,'[TRANSPORT_NATIVE_GATE_RECOVERY_REQUESTED] '+$gate.Replace('window_deadline_ms=70000','window_deadline_ms=80000')),'UNCHANGED_ORIGINAL_GATE_WINDOW'),
  @('bad-polls',$good.Replace('moving_polls=3','moving_polls=2'),'ACTUAL_NATIVE_MOVEMENT'),
  @('fabricated-distance',$good.Replace('position=<30,0,0>','position=<10,0,0>'),'POSITION_READBACK_DISTANCE'),
  @('wrong-mode',$good.Replace('unstuck_mode=ROUTE_REBUILD_ONLY','unstuck_mode=RELOCATED'),'PRODUCTION_ROUTE_ONLY_RECOVERY'),
  @('second-purchase',($good+"`n[VEHICLE_PRESET_PURCHASE] operation=$operation"),'EXACT_ONE_PURCHASE'),
  @('second-fault',($good+"`n"+($lines|Where-Object{$_ -match 'event=EXPIRED'})),'ORDERED_ONE_EXPIRY_REPAIR')
 )){$null=Control $case[0] $case[1] 1 $case[2]}
 $null=Control sibling-crash $good 1 'FULL_SIBLING_FATAL' 'Assertion failed: Entity out of world bounds'
 $result=Control strict-resource ($good+"`nRESOURCES (E): missing resource") 0
 if($result.strict_resources -ne 'FAIL' -or $result.overall -ne 'FAIL'){throw 'Resources were hidden'}
 "PASS Native gate expiry probe: source_controls=$sourceControls log_controls=$logControls; actual native history, one sticky expiry, unchanged windows/deadline, counted route-only repair, same asset/roster/purchase, physical25m and sibling fatal/resource gates"
}finally{
 $resolved=[IO.Path]::GetFullPath($taskRoot);$allowed=[IO.Path]::GetFullPath((Join-Path $RepositoryRoot '.codex-runtime'))+'\'
 if(!$resolved.StartsWith($allowed,[StringComparison]::OrdinalIgnoreCase)){throw 'Cleanup escapes workspace evidence'}
 Remove-Item -LiteralPath $resolved -Recurse -Force
}
