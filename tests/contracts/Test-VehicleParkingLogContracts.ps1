param([string]$RepositoryRoot=(Split-Path -Parent (Split-Path -Parent $PSScriptRoot)))
$ErrorActionPreference='Stop'
$auditor=Join-Path $RepositoryRoot 'tests/log-audits/Test-VehicleParkingLog.ps1'
$temporary=Join-Path ([IO.Path]::GetTempPath()) ('aicf-parking-log-'+[guid]::NewGuid().ToString('N'))
[void][IO.Directory]::CreateDirectory($temporary)
$rules=@('INITIAL_REAL_ROUTE','REAL_NATIVE_SLOPE','ALL_FIVE_OUTSIDE_PARKED','STALE_CAR_NO_MUTATION','THIRTY_SECOND_SLOPE_HOLD','OWN_BRAKE_RELEASED_NATIVE_PILOT','REAL_RESUMED_MOVEMENT','ORIGINAL_HEALTH_UNCHANGED')
$rows=@('CLI Params: -aicfVehicleParkingProbe 1','[ROSTER_READY]','[VEHICLE_PARKING_APPLIED] direct_velocity_pose_mutation=0')
foreach($rule in $rules){$details='';switch($rule){'REAL_NATIVE_SLOPE'{$details='pitch_deg=8 speed_mps=6 pre_brake=0 pos=<1,2,3> terrain_grade=0.12'};'ALL_FIVE_OUTSIDE_PARKED'{$details='speed_mps=0.001 pitch_deg=7 owns_brake=1'};'THIRTY_SECOND_SLOPE_HOLD'{$details='hold_ms=30148 polls=30 drift_m=0.003 pitch_deg=7'};'REAL_RESUMED_MOVEMENT'{$details='moved_m=26 velocity_polls=6'}};$rows+="[PARKING_CASE] rule=$rule passed=1 details=[$details]"}
$rows+='[VEHICLE_PARKING_RELEASED] direct_velocity_pose_mutation=0';$rows+='[PARKING_FINISHED] checks=8 failures=0 classification=CONTROLLED_NATIVE_SLOPE';$rows+='Game destroyed.'
$valid=$rows -join "`n";$checks=0
function Run([string]$full,[bool]$expected,[string]$exit='0',[bool]$seed=$false){
 $log=Join-Path $temporary 'console.log';$launcher=Join-Path $temporary 'launcher.txt';$native=Join-Path $temporary 'native-exit.txt'
 $manifest='AICF_RUNTIME_MANIFEST_JSON=-aicfVehicleParkingProbe 1';$options=@()
 if($seed){$manifest+=' -aicfVehicleParkingSeedOnly 1';$options+=@('-SeedOnly')}
 Set-Content $log $full;Set-Content $launcher $manifest;Set-Content $native $exit
 $result=@(& (Get-Process -Id $PID).Path -NoProfile -File $auditor -LogPath $log -LauncherPath $launcher -NativeExitPath $native @options)
 if(($LASTEXITCODE -eq 0) -ne $expected){throw "Parking log oracle mismatch: $result"};$script:checks++
}
try{
 Run $valid $true
 Run ($valid.Replace('Game destroyed.','')) $false
 Run ($valid+"`nSCRIPT (E): failure") $false
 Run $valid $false '-1'
 Run ($valid.Replace('terrain_grade=0.12','terrain_grade=0.09')) $false
 Run ($valid.Replace('pitch_deg=8','pitch_deg=0')) $false
 Run ($valid.Replace('speed_mps=0.001','speed_mps=1.01')) $false
 Run ($valid.Replace('owns_brake=1','owns_brake=0')) $false
 Run ($valid.Replace('hold_ms=30148','hold_ms=29999')) $false
 Run ($valid.Replace('polls=30','polls=19')) $false
 Run ($valid.Replace('drift_m=0.003','drift_m=1.01')) $false
 Run ($valid.Replace('pitch_deg=7','pitch_deg=0')) $false
 Run ($valid.Replace('moved_m=26','moved_m=24.99')) $false
 Run ($valid.Replace('velocity_polls=6','velocity_polls=2')) $false
 Run ($valid.Replace('rule=STALE_CAR_NO_MUTATION passed=1','rule=STALE_CAR_NO_MUTATION passed=0')) $false
 Run ($valid.Replace('[VEHICLE_PARKING_RELEASED]','[MISSING_RELEASE]')) $false
 Run ($valid.Replace('checks=8 failures=0','checks=7 failures=0')) $false
 $seedRows=@('CLI Params: -aicfVehicleParkingProbe 1 -aicfVehicleParkingSeedOnly 1','[ROSTER_READY]',
  '[PARKING_CASE] rule=PREEXISTING_NATIVE_BRAKE_SEEDED passed=1 details=[persistent_brake=1 parking_token=0 controlled_native_brake_seed=1]',
  '[PARKING_CASE] rule=INITIAL_REAL_ROUTE passed=1 details=[]',
  '[PARKING_CASE] rule=NATIVE_PILOT_RELEASED_PREEXISTING passed=1 details=[persistent_brake=0 parking_token=0 moved_m=30 velocity_polls=6 native_pilot_ready=1]',
  '[PARKING_CASE] rule=ORIGINAL_HEALTH_UNCHANGED passed=1 details=[]',
  '[PARKING_FINISHED] checks=4 failures=0 classification=CONTROLLED_PREEXISTING_NATIVE_BRAKE','Game destroyed.')
 $seedValid=$seedRows -join "`n"
 Run $seedValid $true '0' $true
 Run ($seedValid.Replace('native_pilot_ready=1','native_pilot_ready=0')) $false '0' $true
 Run ($seedValid.Replace('persistent_brake=0','persistent_brake=1')) $false '0' $true
 Run ($seedValid.Replace('parking_token=0 moved_m','parking_token=1 moved_m')) $false '0' $true
 Run ($seedValid.Replace('moved_m=30','moved_m=24.99')) $false '0' $true
 Run ($seedValid.Replace('velocity_polls=6','velocity_polls=2')) $false '0' $true
 Run ($seedValid+"`n[VEHICLE_PARKING_RELEASED]") $false '0' $true
 Run ($seedValid.Replace('classification=CONTROLLED_PREEXISTING_NATIVE_BRAKE','classification=CONTROLLED_NATIVE_SLOPE')) $false '0' $true
 "PASS Vehicle parking log controls=$checks"
}finally{
 $resolved=(Resolve-Path -LiteralPath $temporary).Path
 if(!(Split-Path -Leaf $resolved).StartsWith('aicf-parking-log-') -or !(Split-Path -Parent $resolved).TrimEnd('\').Equals(([IO.Path]::GetTempPath()).TrimEnd('\'),[StringComparison]::OrdinalIgnoreCase)){throw 'Unsafe temporary cleanup target'}
 Remove-Item -LiteralPath $resolved -Recurse -Force
}
