param([string]$RepositoryRoot=(Split-Path -Parent (Split-Path -Parent $PSScriptRoot)))
$ErrorActionPreference='Stop'
$work=Join-Path ([IO.Path]::GetTempPath()) ('aicf-vehicle-gc-log-'+[guid]::NewGuid().ToString('N'))
[void][IO.Directory]::CreateDirectory($work)
$argv=@('-gproj','C:\source path\addon.gproj','-profile','C:\profile','-addonsDir','C:\source path','-addons','CORE,MAP','-aicfVehicleGarbageProbe','1')
$manifest='AICF_RUNTIME_MANIFEST_JSON='+(@{arguments=$argv}|ConvertTo-Json -Compress)
$names=@('REAL_FLEET_ACTIVE_BIND','PRELEASE_NATIVE_TTL_FIVE','OWNED_DELETE_VETO','OWNED_SURVIVES_NATIVE_TTL','OWNED_TRACKING_ENDED','OWNED_INSERT_VETO','RELEASE_PENDING_PROTECTED','FAILED_CLOSED_PROTECTED','STALE_GROUP_GENERATION_REJECTED','STALE_TRIP_GENERATION_REJECTED','STALE_LEASE_GENERATION_REJECTED','STALE_VEHICLE_GENERATION_REJECTED','FAKE_ENTITY_REJECTED','LOCAL_NATIVE_AUTHORITY_READBACK','RELEASE_TO_WORLD_POOL','RELEASED_NATIVE_TRACKING_RESTORED','RELEASED_NO_PERMISSION','RELEASED_NATIVE_TTL_DELETE','UNOWNED_NATIVE_TTL_DELETE','STOCK_CALLBACKS_PRESERVED','LEASE_TEARDOWN_TRACKING_RESTORED')
$rows=@(('CLI Params: '+($argv -join ' ')),'[ROSTER_READY]')
$rows+=@($names|ForEach-Object{"[AICF][VEHICLE_GC_CASE] case=$_ passed=1 details=[native_readback=1]"})
$rows+='[AICF][VEHICLE_GC_FINISHED] checks=21 failures=0'
$rows+='Game destroyed.'
$good=$rows -join "`n"
$checks=0
function Check([string]$name,[string]$log,[int]$expected,[string]$rule='', [string]$launcher=$manifest,[string]$nativeExit='0',[string]$sibling=''){
 [IO.File]::WriteAllText((Join-Path $work 'console.log'),$log)
 [IO.File]::WriteAllText((Join-Path $work 'script.log'),$sibling)
 [IO.File]::WriteAllText((Join-Path $work 'launcher.txt'),$launcher)
 [IO.File]::WriteAllText((Join-Path $work 'exit.txt'),$nativeExit)
 $output=@(& powershell.exe -NoProfile -ExecutionPolicy Bypass -File (Join-Path $RepositoryRoot 'tests/log-audits/Test-VehicleGarbageLog.ps1') -LogPath (Join-Path $work 'console.log') -LauncherPath (Join-Path $work 'launcher.txt') -NativeExitPath (Join-Path $work 'exit.txt') 2>&1)
 if($LASTEXITCODE -ne $expected -or ($rule -and ($output -join "`n") -notmatch $rule)){throw "$name unexpected audit: $($output -join ' ')"}
 $script:checks++
}
try{
 Check complete $good 0 'TARGETED_CONTROLLED_FUNCTIONAL_ONLY.*checks=21'
 foreach($name in $names){Check ('missing-'+$name) ($good.Replace(('case='+$name+' '),'case=UNPROVEN ')) 1 ('CASE_'+$name)}
 Check failed-hop ($good.Replace('OWNED_SURVIVES_NATIVE_TTL passed=1','OWNED_SURVIVES_NATIVE_TTL passed=0')) 1 'FAILED_CASE'
 Check malformed-hop ($good.Replace('OWNED_SURVIVES_NATIVE_TTL passed=1 details=[native_readback=1]','OWNED_SURVIVES_NATIVE_TTL passed=1')) 1 'MALFORMED_CASE'
 Check duplicate ($good.Replace('case=STOCK_CALLBACKS_PRESERVED ','case=OWNED_SURVIVES_NATIVE_TTL ')) 1 'UNIQUE_CASES'
 Check unfinished ($good.Replace('Game destroyed.','')) 1 'FULL_STOPPED_ROSTER'
 Check native-failure $good 1 'NATIVE_EXIT' $manifest '-1'
 Check no-roster ($good.Replace('[ROSTER_READY]','')) 1 'FULL_STOPPED_ROSTER'
 Check wrong-count ($good.Replace('checks=21 failures=0','checks=19 failures=0')) 1 'FINISHED_COUNTS'
 Check failed-summary ($good.Replace('checks=21 failures=0','checks=21 failures=1')) 1 'FINISHED_COUNTS'
 Check duplicate-finished ($good+"`n[VEHICLE_GC_FINISHED] checks=21 failures=0") 1 'UNIQUE_FINISHED'
 Check early-summary ($good.Replace('[AICF][VEHICLE_GC_FINISHED] checks=21 failures=0','').Replace('[ROSTER_READY]',"[ROSTER_READY]`n[VEHICLE_GC_FINISHED] checks=21 failures=0")) 1 'FINISHED_BEFORE_STOP'
 Check no-manifest $good 1 'MANIFEST' ''
 Check duplicate-manifest $good 1 'MANIFEST' ($manifest+"`n"+$manifest)
 Check malformed-manifest $good 1 'MALFORMED_MANIFEST' 'AICF_RUNTIME_MANIFEST_JSON=bad'
 Check wrong-cli ($good.Replace('-addons CORE,MAP','-addons OTHER')) 1 'EXACT_CLI'
 Check zero-probe ($good.Replace('VehicleGarbageProbe 1','VehicleGarbageProbe 0')) 1 'PROBE_VALUE' ($manifest.Replace('Probe","1"','Probe","0"'))
 Check sibling-fatal $good 1 'FULL_LOG_FATAL' $manifest '0' 'SCRIPT (E): injected exception'
 Check resources-independent $good 0 'gate=strict_native_resources verdict=FAIL' $manifest '0' 'RESOURCES (E): missing texture'
}finally{
 $resolved=[IO.Path]::GetFullPath($work)
 $allowed=[IO.Path]::GetFullPath([IO.Path]::GetTempPath()).TrimEnd('\')+'\'
 if(!$resolved.StartsWith($allowed,[StringComparison]::OrdinalIgnoreCase) -or (Split-Path -Leaf $resolved) -notlike 'aicf-vehicle-gc-log-*'){throw 'Unsafe contract cleanup path'}
 Remove-Item -LiteralPath $resolved -Recurse -Force
}
"PASS Vehicle garbage log controls checks=$checks"
