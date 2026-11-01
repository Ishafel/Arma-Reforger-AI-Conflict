param([string]$RepositoryRoot=(Split-Path -Parent (Split-Path -Parent $PSScriptRoot)))
$ErrorActionPreference='Stop'
$work=Join-Path ([IO.Path]::GetTempPath()) ('aicf-preset-transit-'+[guid]::NewGuid().ToString('N'))
[void][IO.Directory]::CreateDirectory($work)
$argv=@('-gproj','source','-profile','profile','-addonsDir','addons','-addons','CORE,MAP','-aicfVehiclePresetTransitProbe','1')
$launcher='AICF_RUNTIME_MANIFEST_JSON='+(@{arguments=$argv}|ConvertTo-Json -Compress)
$rules=@('GROUP5_CREATED','UAZ_FIVE_REAL_SEATS','COMPATIBLE_APPLY','NEARBY_INITIAL_ORDER','NO_IMMEDIATE_DISMOUNT','PARKED_EXACT_FIVE','IDLE_BUDGET_PAUSED','FIVE_STAY_MOUNTED_30_SECONDS','WAIT_STATUS','NAVMESH_DESTINATION','ONWARD_ORDER','SAME_VEHICLE_REAL_MOTION','ROUTE_BUDGET_RESTORED','ARRIVAL_RETARGET','ARRIVAL_FULL_DISMOUNT','ONE_PURCHASE')
$good='CLI Params: '+($argv -join ' ')+"`n[ROSTER_READY]`n"
foreach($rule in $rules){$good+="[PRESET_TRANSIT_CHECK] rule=$rule passed=1`n"}
for($i=0;$i -lt 5;$i++){$good+="[VEHICLE_PRESET_OCCUPANT] position=$i seat=0:$i`:0 member=$i occupant=$i`n"}
$good+="[VEHICLE_PRESET_DESTINATION_WAIT] waiting=1 waited_ms=0`n[VEHICLE_PRESET_DESTINATION_WAIT] waiting=0 waited_ms=30000`n"
$motion='[PRESET_TRANSIT_MOTION] displacement_m=25 moving_polls=5 same_vehicle=1 exact_occupants=5'
$dismount='[VEHICLE_STATE_CHANGED] previous_state=TRANSIT state=DISMOUNT'
$purchase='[VEHICLE_PRESET_PURCHASE] prefab=UAZ469_uncovered.et supplies=125'
$completion='[DISEMBARK_COMPLETE] logical_occupants=0 transitions=0 inside_bounds=0'
$good+="$motion`n$dismount`n$completion`n[VEHICLE_STATE_CHANGED] previous_state=HANDOFF state=COMPLETE`n$purchase`n[PRESET_TRANSIT_DONE] checks=16 failures=0`nGame destroyed."
$checks=0
function Run([string]$name,[string]$body,[int]$expected,[string]$exitCode='0',[string]$sibling=''){
 [IO.File]::WriteAllText("$work/console.log",$body)
 [IO.File]::WriteAllText("$work/script.log",$sibling)
 [IO.File]::WriteAllText("$work/launcher.txt",$launcher)
 [IO.File]::WriteAllText("$work/exit.txt",$exitCode)
 $result=& (Join-Path $RepositoryRoot 'tests/log-audits/Test-VehiclePresetTransitLog.ps1') -LogPath "$work/console.log" -LauncherPath "$work/launcher.txt" -NativeExitPath "$work/exit.txt"
 if($LASTEXITCODE -ne $expected){throw "$name unexpected verdict: $result"}
 $script:checks++
}
try{
 Run complete $good 0
 foreach($rule in $rules){Run $rule ($good.Replace("rule=$rule passed=1","rule=$rule passed=0")) 1}
 Run intent-without-motion ($good.Replace($motion,'')) 1
 Run teleport-without-motion ($good.Replace('moving_polls=5','moving_polls=0')) 1
 Run insufficient-travel ($good.Replace('displacement_m=25','displacement_m=2')) 1
 Run another-vehicle ($good.Replace('same_vehicle=1','same_vehicle=0')) 1
 Run premature-exit ($good.Replace($motion,$dismount+"`n"+$motion)) 1
 Run missing-physical-exit ($good.Replace($completion,'')) 1
 Run timeout-instead-of-complete ($good.Replace('previous_state=HANDOFF state=COMPLETE','previous_state=DISMOUNT state=FALLBACK')) 1
 Run latched-failure ($good+"`n[ACCEPTANCE_FAILURE_LATCHED] reason=DISEMBARK_TIMEOUT") 1
 Run short-wait ($good.Replace('waited_ms=30000','waited_ms=500')) 1
 Run wrong-occupant ($good.Replace('member=2 occupant=2','member=2 occupant=3')) 1
 Run missing-member ($good.Replace('position=4 seat=','MISSING=4 seat=')) 1
 Run duplicate-debit ($good+"`n"+$purchase) 1
 Run not-stopped ($good.Replace('Game destroyed.','')) 1
 Run native-failure $good 1 '1'
 Run full-sibling-fatal $good 1 '0' 'SCRIPT (E): failure'
}
finally{
 $resolved=[IO.Path]::GetFullPath($work)
 if(!$resolved.StartsWith([IO.Path]::GetTempPath(),[StringComparison]::OrdinalIgnoreCase) -or (Split-Path -Leaf $resolved) -notlike 'aicf-preset-transit-*'){throw 'Unsafe cleanup'}
 Remove-Item -LiteralPath $resolved -Recurse -Force
}
"PASS Preset transit log contracts checks=$checks"
