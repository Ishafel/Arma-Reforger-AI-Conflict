param([string]$RepositoryRoot=(Split-Path -Parent (Split-Path -Parent $PSScriptRoot)))
$ErrorActionPreference='Stop'
$work=Join-Path ([IO.Path]::GetTempPath()) ('aicf-manual-staging-log-'+[guid]::NewGuid().ToString('N'))
[void][IO.Directory]::CreateDirectory($work)
$argv=@('-gproj','C:\source path\addon.gproj','-profile','C:\profile','-addonsDir','C:\source path','-addons','CORE,MAP','-aicfManualStagingProbe','1')
$manifest='AICF_RUNTIME_MANIFEST_JSON='+(@{arguments=$argv}|ConvertTo-Json -Compress)
$names=@('CONTROLLED_NATIVE_MOVEMENT_BLOCK','STALE_REQUEST_NO_MUTATION','STALE_PRESET_NO_MUTATION','STALE_RPL_NO_MUTATION','NO_CURRENT_ASSIGNMENT_NO_MUTATION','LOST_ACTION_NO_MUTATION','EXHAUSTED_TWO_OWNED_ATTEMPTS','SHORT_SUBMITTED_ONCE','DUPLICATE_NO_MUTATION','ACTUAL_SHORT_TELEPORT','ALL_FIVE_PHYSICAL_RALLY_BEFORE_PAYMENT','ORIGINAL_IDENTITIES_HEALTH_UNCHANGED','DEADLINES_UNCHANGED','NO_BUY_OR_DEBIT')
$rows=@(('CLI Params: '+($argv -join ' ')),'[ROSTER_READY]')
$rows+=@($names|ForEach-Object{"[AICF][MANUAL_STAGING_CASE] case=$_ passed=1 details=[native_readback=1]"})
$rows+='[AICF][MANUAL_STAGING_FINISHED] checks=14 failures=0'
$rows+='Game destroyed.'
$good=$rows -join "`n"
$checks=0
function Check([string]$name,[string]$log,[int]$expected,[string]$rule='', [string]$launcher=$manifest,[string]$nativeExit='0',[string]$sibling='',[string[]]$extraArgs=@()){
 [IO.File]::WriteAllText((Join-Path $work 'console.log'),$log)
 [IO.File]::WriteAllText((Join-Path $work 'script.log'),$sibling)
 [IO.File]::WriteAllText((Join-Path $work 'launcher.txt'),$launcher)
 [IO.File]::WriteAllText((Join-Path $work 'exit.txt'),$nativeExit)
 $output=@(& powershell.exe -NoProfile -ExecutionPolicy Bypass -File (Join-Path $RepositoryRoot 'tests/log-audits/Test-ManualStagingLog.ps1') -LogPath (Join-Path $work 'console.log') -LauncherPath (Join-Path $work 'launcher.txt') -NativeExitPath (Join-Path $work 'exit.txt') @extraArgs 2>&1)
 if($LASTEXITCODE -ne $expected -or ($rule -and ($output -join "`n") -notmatch $rule)){throw "$name unexpected audit: $($output -join ' ')"}
 $script:checks++
}
try{
 Check complete $good 0 'TARGETED_CONTROLLED_FUNCTIONAL_ONLY.*checks=14'
 foreach($name in $names){Check ('missing-'+$name) ($good.Replace(('case='+$name+' '),'case=UNPROVEN ')) 1 ('CASE_'+$name)}
 Check failed-hop ($good.Replace('ACTUAL_SHORT_TELEPORT passed=1','ACTUAL_SHORT_TELEPORT passed=0')) 1 'FAILED_CASE'
 Check malformed-hop ($good.Replace('ACTUAL_SHORT_TELEPORT passed=1 details=[native_readback=1]','ACTUAL_SHORT_TELEPORT passed=1')) 1 'MALFORMED_CASE'
 Check duplicate ($good.Replace('case=ALL_FIVE_PHYSICAL_RALLY_BEFORE_PAYMENT ','case=ACTUAL_SHORT_TELEPORT ')) 1 'UNIQUE_CASES'
 Check unfinished ($good.Replace('Game destroyed.','')) 1 'FULL_STOPPED_ROSTER'
 Check native-failure $good 1 'NATIVE_EXIT' $manifest '-1'
 Check no-roster ($good.Replace('[ROSTER_READY]','')) 1 'FULL_STOPPED_ROSTER'
 Check wrong-count ($good.Replace('checks=14 failures=0','checks=19 failures=0')) 1 'FINISHED_COUNTS'
 Check failed-summary ($good.Replace('checks=14 failures=0','checks=14 failures=1')) 1 'FINISHED_COUNTS'
 Check duplicate-finished ($good+"`n[MANUAL_STAGING_FINISHED] checks=14 failures=0") 1 'UNIQUE_FINISHED'
 Check early-summary ($good.Replace('[AICF][MANUAL_STAGING_FINISHED] checks=14 failures=0','').Replace('[ROSTER_READY]',"[ROSTER_READY]`n[MANUAL_STAGING_FINISHED] checks=14 failures=0")) 1 'FINISHED_BEFORE_STOP'
 Check no-manifest $good 1 'MANIFEST' ''
 Check duplicate-manifest $good 1 'MANIFEST' ($manifest+"`n"+$manifest)
 Check malformed-manifest $good 1 'MALFORMED_MANIFEST' 'AICF_RUNTIME_MANIFEST_JSON=bad'
 Check wrong-cli ($good.Replace('-addons CORE,MAP','-addons OTHER')) 1 'EXACT_CLI'
 Check zero-probe ($good.Replace('ManualStagingProbe 1','ManualStagingProbe 0')) 1 'PROBE_VALUE' ($manifest.Replace('Probe","1"','Probe","0"'))
 Check sibling-fatal $good 1 'FULL_LOG_FATAL' $manifest '0' 'SCRIPT (E): injected exception'
 Check resources-independent $good 0 'gate=strict_native_resources verdict=FAIL' $manifest '0' 'RESOURCES (E): missing texture'
 $exNames=@('CONTROLLED_NATIVE_MOVEMENT_BLOCK','EXHAUSTION_TWO_ATTEMPTS_SPENT','EXHAUSTION_LIVE_WAYPOINT_NOT_FALLBACK','EXHAUSTION_MISSING_REISSUE_NOT_FALLBACK','EXHAUSTION_WAYPOINT_REMOVED_FIRST','EXHAUSTION_WAYPOINT_REMOVED_REISSUE','EXHAUSTION_EARLY_TYPED_FALLBACK','EXHAUSTION_PLAN_RELEASED','EXHAUSTION_ORDER_RESTORED','EXHAUSTION_NO_TELEPORT_DEBIT','EXHAUSTION_ORIGINAL_HEALTH','EXHAUSTION_DEADLINE_UNCHANGED')
 $exArgv=$argv+@('-aicfManualStagingExhaustionProbe','1')
 $exManifest='AICF_RUNTIME_MANIFEST_JSON='+(@{arguments=$exArgv}|ConvertTo-Json -Compress)
 $exRows=@(('CLI Params: '+($exArgv -join ' ')),'[ROSTER_READY]')
 $exRows+=@($exNames|ForEach-Object {"[AICF][MANUAL_STAGING_CASE] case=$_ passed=1 details=[native_readback=1]"})
 $exRows+='[AICF][MANUAL_STAGING_FINISHED] checks=12 failures=0';$exRows+='Game destroyed.'
 $exGood=$exRows -join "`n"
 Check exhaustion-complete $exGood 0 'checks=12' $exManifest '0' '' @('-Exhaustion')
 foreach($name in $exNames){Check ('exhaustion-missing-'+$name) ($exGood.Replace(('case='+$name+' '),'case=UNPROVEN ')) 1 ('CASE_'+$name) $exManifest '0' '' @('-Exhaustion')}
 Check exhaustion-no-cli $good 1 'EXHAUSTION_CLI' $manifest '0' '' @('-Exhaustion')
 Check exhaustion-zero-cli ($exGood.Replace('ExhaustionProbe 1','ExhaustionProbe 0')) 1 'EXHAUSTION_VALUE' ($exManifest.Replace('ExhaustionProbe","1"','ExhaustionProbe","0"')) '0' '' @('-Exhaustion')
}finally{
 $resolved=[IO.Path]::GetFullPath($work)
 $allowed=[IO.Path]::GetFullPath([IO.Path]::GetTempPath()).TrimEnd('\')+'\'
 if(!$resolved.StartsWith($allowed,[StringComparison]::OrdinalIgnoreCase) -or (Split-Path -Leaf $resolved) -notlike 'aicf-manual-staging-log-*'){throw 'Unsafe contract cleanup path'}
 Remove-Item -LiteralPath $resolved -Recurse -Force
}
"PASS Manual staging log controls checks=$checks"
