param([string]$RepositoryRoot=(Split-Path -Parent (Split-Path -Parent $PSScriptRoot)))
$ErrorActionPreference='Stop'
$work=Join-Path ([IO.Path]::GetTempPath()) ('aicf-recruit-short-log-'+[guid]::NewGuid().ToString('N'))
[void][IO.Directory]::CreateDirectory($work)
$argv=@('-gproj','C:\source path\addon.gproj','-profile','C:\profile','-addonsDir','C:\source path','-addons','CORE,MAP','-aicfRecruitmentShortRecoveryProbe','1')
$manifest='AICF_RUNTIME_MANIFEST_JSON='+(@{arguments=$argv}|ConvertTo-Json -Compress)
$names=@('CURRENT_NATIVE_MOVE_OWNER','UNKNOWN_NO_CAPTURE','FOREIGN_HANDLER_NO_CAPTURE','UNRELATED_NO_CAPTURE','STALE_TOKEN_NO_MUTATION','STALE_GENERATION_NO_MUTATION','STALE_GRAPH_NO_MUTATION','FOREIGN_ORDER_NO_MUTATION','NO_SAFE_DESTINATION_NO_MUTATION','CAPTURE_BEFORE_NATIVE_CALLBACK','NATIVE_CALLBACK_ONCE','PHYSICAL_SHORT_HOP','RECEIPT_CONSUMED_ONCE','ABSOLUTE_DEADLINE_UNCHANGED','REPAIR_BUDGET_TWO','CONTINUED_NATIVE_APPROACH','REAL_ARRIVAL','PAID_RECRUIT_JOIN','VEHICLE_NO_MUTATION','INCAP_NO_MUTATION')
$rows=@(('CLI Params: '+($argv -join ' ')),'[ROSTER_READY]')
$rows+=@($names|ForEach-Object{"[AICF][RECRUITMENT_SHORT_CASE] case=$_ passed=1 details=[native_readback=1]"})
$rows+='[AICF][RECRUITMENT_SHORT_FINISHED] checks=20 failures=0'
$rows+='Game destroyed.'
$good=$rows -join "`n"
$checks=0
function Check([string]$name,[string]$log,[int]$expected,[string]$rule='', [string]$launcher=$manifest,[string]$nativeExit='0',[string]$sibling=''){
 [IO.File]::WriteAllText((Join-Path $work 'console.log'),$log)
 [IO.File]::WriteAllText((Join-Path $work 'script.log'),$sibling)
 [IO.File]::WriteAllText((Join-Path $work 'launcher.txt'),$launcher)
 [IO.File]::WriteAllText((Join-Path $work 'exit.txt'),$nativeExit)
 $output=@(& powershell.exe -NoProfile -ExecutionPolicy Bypass -File (Join-Path $RepositoryRoot 'tests/log-audits/Test-RecruitmentShortRecoveryLog.ps1') -LogPath (Join-Path $work 'console.log') -LauncherPath (Join-Path $work 'launcher.txt') -NativeExitPath (Join-Path $work 'exit.txt') 2>&1)
 if($LASTEXITCODE -ne $expected -or ($rule -and ($output -join "`n") -notmatch $rule)){throw "$name unexpected audit: $($output -join ' ')"}
 $script:checks++
}
try{
 Check complete $good 0 'TARGETED_CONTROLLED_FUNCTIONAL_ONLY.*checks=20'
 foreach($name in $names){Check ('missing-'+$name) ($good.Replace(('case='+$name+' '),'case=UNPROVEN ')) 1 ('CASE_'+$name)}
 Check failed-hop ($good.Replace('PHYSICAL_SHORT_HOP passed=1','PHYSICAL_SHORT_HOP passed=0')) 1 'FAILED_CASE'
 Check malformed-hop ($good.Replace('PHYSICAL_SHORT_HOP passed=1 details=[native_readback=1]','PHYSICAL_SHORT_HOP passed=1')) 1 'MALFORMED_CASE'
 Check duplicate ($good.Replace('case=REAL_ARRIVAL ','case=PHYSICAL_SHORT_HOP ')) 1 'UNIQUE_CASES'
 Check unfinished ($good.Replace('Game destroyed.','')) 1 'FULL_STOPPED_ROSTER'
 Check native-failure $good 1 'NATIVE_EXIT' $manifest '-1'
 Check no-roster ($good.Replace('[ROSTER_READY]','')) 1 'FULL_STOPPED_ROSTER'
 Check wrong-count ($good.Replace('checks=20 failures=0','checks=19 failures=0')) 1 'FINISHED_COUNTS'
 Check failed-summary ($good.Replace('checks=20 failures=0','checks=20 failures=1')) 1 'FINISHED_COUNTS'
 Check duplicate-finished ($good+"`n[RECRUITMENT_SHORT_FINISHED] checks=20 failures=0") 1 'UNIQUE_FINISHED'
 Check early-summary ($good.Replace('[AICF][RECRUITMENT_SHORT_FINISHED] checks=20 failures=0','').Replace('[ROSTER_READY]',"[ROSTER_READY]`n[RECRUITMENT_SHORT_FINISHED] checks=20 failures=0")) 1 'FINISHED_BEFORE_STOP'
 Check no-manifest $good 1 'MANIFEST' ''
 Check duplicate-manifest $good 1 'MANIFEST' ($manifest+"`n"+$manifest)
 Check malformed-manifest $good 1 'MALFORMED_MANIFEST' 'AICF_RUNTIME_MANIFEST_JSON=bad'
 Check wrong-cli ($good.Replace('-addons CORE,MAP','-addons OTHER')) 1 'EXACT_CLI'
 Check zero-probe ($good.Replace('ShortRecoveryProbe 1','ShortRecoveryProbe 0')) 1 'PROBE_VALUE' ($manifest.Replace('Probe","1"','Probe","0"'))
 Check sibling-fatal $good 1 'FULL_LOG_FATAL' $manifest '0' 'SCRIPT (E): injected exception'
 Check resources-independent $good 0 'gate=strict_native_resources verdict=FAIL' $manifest '0' 'RESOURCES (E): missing texture'
}finally{
 $resolved=[IO.Path]::GetFullPath($work)
 $allowed=[IO.Path]::GetFullPath([IO.Path]::GetTempPath()).TrimEnd('\')+'\'
 if(!$resolved.StartsWith($allowed,[StringComparison]::OrdinalIgnoreCase) -or (Split-Path -Leaf $resolved) -notlike 'aicf-recruit-short-log-*'){throw 'Unsafe contract cleanup path'}
 Remove-Item -LiteralPath $resolved -Recurse -Force
}
"PASS Recruitment short recovery log controls checks=$checks"
