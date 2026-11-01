param([string]$RepositoryRoot=(Split-Path -Parent (Split-Path -Parent $PSScriptRoot)))
$ErrorActionPreference='Stop'
$auditor=Join-Path $RepositoryRoot 'tests/log-audits/Test-HealthyNativeAbandonLog.ps1'
$evidence=Join-Path $RepositoryRoot ('.codex-runtime/healthy-abandon-audit-contract-'+[guid]::NewGuid().ToString('N'))
[void][IO.Directory]::CreateDirectory($evidence)
$positive=@'
[AICF][HEALTHY_NATIVE_ABANDON_PROBE] event=CANONICAL_READY actual=5 initial_generation=1 unit_type=INFANTRY before_coordinator_tick=1
[AICF][VEHICLE_SCENARIO_CHECK] scenario=uaz5 rule=CANONICAL_OWNER passed=1
[AICF][VEHICLE_SCENARIO_CHECK] scenario=uaz5 rule=HNA_PRODUCTION_RECOVERY passed=1
[AICF][VEHICLE_SCENARIO_CHECK] scenario=uaz5 rule=HNA_SAME_VEHICLE_PHYSICAL_MOVEMENT passed=1
[AICF][HEALTHY_NATIVE_ABANDON_PROBE] event=INJECTED operation_id=trip-USSR-0-1-1 vehicle=0x123{} route_generation=1 attempts=0 deadline=900000 health_mutation=0 pose_mutation=0 controlled_fault=1
[AICF][STAGE3][INFO][TRANSPORT_NATIVE_ABANDON_RECOVERY] operation_id=trip-USSR-0-1-1 reason=HEALTHY_FULL_ROSTER predicates=[healthy=1|full_roster=1|settled=1|local_combat=0|attempts=0|maximum_attempts=2|expired=0] native_get_out_cancelled=1 recovery_mode=ROUTE_REBUILD_ONLY health_unchanged=1 deadline_renewed=0
[AICF][HEALTHY_NATIVE_ABANDON_PROBE] event=DECISION operation_id=trip-USSR-0-1-1 observed=1 recovered=1 attempts=1 deadline=900000 controlled_fault=1 native_selection=1 production_outcome_unchanged=1
[AICF][HEALTHY_NATIVE_ABANDON_PROBE] event=DONE operation_id=trip-USSR-0-1-1 vehicle=0x123{} displacement_m=27.5 moving_polls=4 attempts=1 deadline=900000 original_roster=1 controlled_fault=1 natural_failure_proof=0
Game destroyed.
'@
$launcher='AICF_RUNTIME_MANIFEST_JSON={"arguments":["-aicfHealthyNativeAbandonProbe","1","-aicfVehicleScenarioCanonicalOwner","1"]}'
$cases=@(
 [pscustomobject]@{name='positive_synthetic_parser_control';text=$positive;launcher=$launcher;native=0;expected=0},
 [pscustomobject]@{name='missing_decision';text=($positive -replace '(?m)^.*event=DECISION.*$','');launcher=$launcher;native=0;expected=1},
 [pscustomobject]@{name='missing_native_production_owner';text=($positive -replace '(?m)^.*\[TRANSPORT_NATIVE_ABANDON_RECOVERY\].*$','');launcher=$launcher;native=0;expected=1},
 [pscustomobject]@{name='foreign_vehicle';text=$positive.Replace('vehicle=0x123{} displacement_m=','vehicle=0x456{} displacement_m=');launcher=$launcher;native=0;expected=1},
 [pscustomobject]@{name='insufficient_motion';text=$positive.Replace('displacement_m=27.5','displacement_m=24.9');launcher=$launcher;native=0;expected=1},
 [pscustomobject]@{name='insufficient_velocity_polls';text=$positive.Replace('moving_polls=4','moving_polls=2');launcher=$launcher;native=0;expected=1},
 [pscustomobject]@{name='deadline_extended';text=$positive.Replace('deadline=900000 original_roster','deadline=900001 original_roster');launcher=$launcher;native=0;expected=1},
 [pscustomobject]@{name='missing_stop';text=$positive.Replace('Game destroyed.','');launcher=$launcher;native=0;expected=1},
 [pscustomobject]@{name='native_nonzero';text=$positive;launcher=$launcher;native=1;expected=1},
 [pscustomobject]@{name='flag_missing';text=$positive;launcher=$launcher.Replace('-aicfHealthyNativeAbandonProbe','-other');native=0;expected=1},
 [pscustomobject]@{name='alternate_owner';text=$positive;launcher=$launcher.Replace('-aicfVehicleScenarioCanonicalOwner','-otherOwner');native=0;expected=1},
 [pscustomobject]@{name='sibling_resource';text=$positive;launcher=$launcher;native=0;expected=1;sibling='RESOURCES (E): test resource failure'},
 [pscustomobject]@{name='sibling_oom';text=$positive;launcher=$launcher;native=0;expected=1;sibling='MEMORY (E): Out of memory'}
)
foreach($case in $cases){
 $root=Join-Path $evidence $case.name;[void][IO.Directory]::CreateDirectory($root)
 $log=Join-Path $root 'console.log';$launch=Join-Path $root 'launcher.txt';$exitPath=Join-Path $root 'native-exit.txt'
 [IO.File]::WriteAllText($log,$case.text);[IO.File]::WriteAllText($launch,$case.launcher);[IO.File]::WriteAllText($exitPath,[string]$case.native)
 if($case.PSObject.Properties.Name -contains 'sibling'){[IO.File]::WriteAllText((Join-Path $root 'error.log'),$case.sibling)}
 & powershell.exe -NoProfile -ExecutionPolicy Bypass -File $auditor -LogPath $log -LauncherPath $launch -NativeExitPath $exitPath *> (Join-Path $root 'audit.txt')
 $exit=$LASTEXITCODE
 if($exit -ne $case.expected){throw "Healthy abandon audit parser case $($case.name): exit=$exit expected=$($case.expected) evidence=$root"}
 if($case.name -eq 'sibling_oom' -and ([IO.File]::ReadAllText((Join-Path $root 'audit.txt'))) -notmatch 'FULL_FATAL=FAIL'){throw 'Sibling OOM hidden'}
}
"PASS Healthy native abandon log parser: $($cases.Count) synthetic controls, causal decision/identity/deadline/physical proof mandatory; stopped full sibling logs and native exit enforced. Synthetic parser PASS is not native recovery proof. Evidence=$evidence"
