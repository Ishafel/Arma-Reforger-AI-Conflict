param([Parameter(Mandatory)][string]$LogPath,[Parameter(Mandatory)][string]$LauncherPath,[Parameter(Mandatory)][string]$NativeExitPath,[string]$EvidenceRoot,[switch]$SeedOnly)
$ErrorActionPreference='Stop'
$full=Get-Content -LiteralPath $LogPath -Raw;$launcher=Get-Content -LiteralPath $LauncherPath -Raw
$failures=[Collections.Generic.List[string]]::new()
function Require($condition,[string]$reason){if(!$condition){$failures.Add($reason)}}
Require ((Get-Content -LiteralPath $NativeExitPath -Raw).Trim() -eq '0') 'NATIVE_EXIT'
Require ($full.Contains('Game destroyed.') -and $full.Contains('[ROSTER_READY]') -and $full.Contains('CLI Params:')) 'FULL_STOPPED_LOG'
Require ($full -notmatch 'SCRIPT\s+\([EF]\)|ENGINE\s+\(F\)|Virtual Machine Exception|NULL pointer') 'FULL_FATAL'
Require ($launcher.Contains('AICF_RUNTIME_MANIFEST_JSON=') -and $launcher.Contains('-aicfVehicleParkingProbe')) 'CANONICAL_MANIFEST'
$rows=@([regex]::Matches($full,'\[PARKING_CASE\] rule=(?<rule>\w+) passed=(?<passed>[01]) details=\[(?<details>[^\r\n]*)\]'))
$expected=@('INITIAL_REAL_ROUTE','REAL_NATIVE_SLOPE','ALL_FIVE_OUTSIDE_PARKED','STALE_CAR_NO_MUTATION','THIRTY_SECOND_SLOPE_HOLD','OWN_BRAKE_RELEASED_NATIVE_PILOT','REAL_RESUMED_MOVEMENT','ORIGINAL_HEALTH_UNCHANGED')
if($SeedOnly){$expected=@('PREEXISTING_NATIVE_BRAKE_SEEDED','INITIAL_REAL_ROUTE','NATIVE_PILOT_RELEASED_PREEXISTING','ORIGINAL_HEALTH_UNCHANGED')}
Require ((($rows|ForEach-Object{$_.Groups['rule'].Value}) -join ',') -ceq ($expected -join ',')) 'EXACT_NATIVE_RULES'
Require (@($rows|Where-Object{$_.Groups['passed'].Value -ne '1'}).Count -eq 0) 'NATIVE_CASE_FAILURE'
if(!$SeedOnly){
$slope=[regex]::Match($full,'rule=REAL_NATIVE_SLOPE passed=1 details=\[pitch_deg=(?<pitch>-?[0-9.]+) speed_mps=(?<speed>[0-9.]+) pre_brake=0 pos=<[^>]+> terrain_grade=(?<grade>[0-9.]+)\]')
Require ($slope.Success -and [Math]::Abs([double]$slope.Groups['pitch'].Value) -ge 6 -and [double]$slope.Groups['speed'].Value -ge 2 -and [double]$slope.Groups['grade'].Value -ge 0.1) 'REAL_TERRAIN_SLOPE_BEFORE_BRAKE'
$outside=[regex]::Match($full,'rule=ALL_FIVE_OUTSIDE_PARKED passed=1 details=\[speed_mps=(?<speed>[0-9.Ee+-]+) pitch_deg=(?<pitch>-?[0-9.]+) owns_brake=1\]')
Require ($outside.Success -and [double]$outside.Groups['speed'].Value -le 1) 'ACTUAL_DRIVERLESS_OWN_PARKING'
$hold=[regex]::Match($full,'rule=THIRTY_SECOND_SLOPE_HOLD passed=1 details=\[hold_ms=(?<elapsed>[0-9]+) polls=(?<polls>[0-9]+) drift_m=(?<drift>[0-9.Ee+-]+) pitch_deg=(?<pitch>-?[0-9.]+)\]')
Require ($hold.Success -and [int]$hold.Groups['elapsed'].Value -ge 30000 -and [int]$hold.Groups['polls'].Value -ge 20 -and [double]$hold.Groups['drift'].Value -le 1 -and [Math]::Abs([double]$hold.Groups['pitch'].Value) -ge 3) 'PHYSICAL_THIRTY_SECOND_SLOPE_HOLD'
$motion=[regex]::Match($full,'rule=REAL_RESUMED_MOVEMENT passed=1 details=\[moved_m=(?<moved>[0-9.]+) velocity_polls=(?<polls>[0-9]+)\]')
Require ($motion.Success -and [double]$motion.Groups['moved'].Value -ge 25 -and [int]$motion.Groups['polls'].Value -ge 3) 'ACTUAL_RESUMED_MOVEMENT'
Require ($full.Contains('[VEHICLE_PARKING_APPLIED]') -and $full.Contains('[VEHICLE_PARKING_RELEASED]') -and $full.Contains('direct_velocity_pose_mutation=0')) 'PRODUCTION_PARKING_RECEIPTS'
Require (@([regex]::Matches($full,'\[PARKING_FINISHED\] checks=8 failures=0 classification=CONTROLLED_NATIVE_SLOPE')).Count -eq 1) 'EXACT_FINISH'
}else{
 Require ($launcher.Contains('-aicfVehicleParkingSeedOnly')) 'EXPLICIT_SEED_ONLY_MANIFEST'
 Require ($full.Contains('rule=PREEXISTING_NATIVE_BRAKE_SEEDED passed=1 details=[persistent_brake=1 parking_token=0 controlled_native_brake_seed=1]')) 'ACTUAL_PREEXISTING_NATIVE_FLAG'
 $native=[regex]::Match($full,'rule=NATIVE_PILOT_RELEASED_PREEXISTING passed=1 details=\[persistent_brake=0 parking_token=0 moved_m=(?<moved>[0-9.]+) velocity_polls=(?<polls>[0-9]+) native_pilot_ready=1\]')
 Require ($native.Success -and [double]$native.Groups['moved'].Value -ge 25 -and [int]$native.Groups['polls'].Value -ge 3) 'NATIVE_RELEASE_AND_REAL_MOTION'
 Require ($full -notmatch '\[VEHICLE_PARKING_(?:APPLIED|RELEASED)\]') 'NO_PROJECT_PARKING_WRITE'
 Require (@([regex]::Matches($full,'\[PARKING_FINISHED\] checks=4 failures=0 classification=CONTROLLED_PREEXISTING_NATIVE_BRAKE')).Count -eq 1) 'EXACT_NATIVE_SEED_FINISH'
}
$errors=@([regex]::Matches($full,'(?m)^.*\s+\(E\).*$')).Count
$targeted='PASS';if($failures.Count){$targeted='FAIL'};$resources='PASS';if($errors){$resources='FAIL'};$overall='PASS';if($targeted -ne 'PASS' -or $resources -ne 'PASS'){$overall='FAIL'}
$classification='CONTROLLED_NATIVE_SLOPE';if($SeedOnly){$classification='CONTROLLED_PREEXISTING_NATIVE_BRAKE'}
$result=[pscustomobject]@{classification=$classification;targeted=$targeted;failures=@($failures);checks=$rows.Count;strict_native_resources=$resources;native_error_lines=$errors;overall=$overall;full_log=(Resolve-Path $LogPath).Path}
if($EvidenceRoot){[void][IO.Directory]::CreateDirectory($EvidenceRoot);$result|ConvertTo-Json -Depth 4|Set-Content (Join-Path $EvidenceRoot 'summary.json')}
$result|ConvertTo-Json -Depth 4
if($failures.Count){exit 1};exit 0
