param(
 [Parameter(Mandatory)][string]$LogPath,
 [Parameter(Mandatory)][string]$LauncherPath,
 [Parameter(Mandatory)][string]$NativeExitPath
)
$ErrorActionPreference='Stop'
$log=[IO.File]::ReadAllText($LogPath)
$launcher=[IO.File]::ReadAllText($LauncherPath)
$failures=[Collections.Generic.List[string]]::new()
$manifest=@([regex]::Matches($launcher,'(?m)^AICF_RUNTIME_MANIFEST_JSON=([^\r\n]+)'))
if($manifest.Count -ne 1){throw 'One exact launcher manifest required'}
$argv=@(($manifest[0].Groups[1].Value|ConvertFrom-Json).arguments)
foreach($flag in @('-aicfTransitionProbe','-aicfProbeReturnToMenu')){
 $index=[array]::IndexOf($argv,$flag)
 if($index -lt 0 -or $index+1 -ge $argv.Count -or $argv[$index+1] -ne '1' -or !$log.Contains($flag+' 1')){$failures.Add('RETURN_CLI')}
}
# Keep the existing four exact production launch/header/graph cases as a separate gate.
$manifestFile=[IO.Path]::GetTempFileName()
try{
 [IO.File]::WriteAllText($manifestFile,$manifest[0].Groups[1].Value)
 & powershell.exe -NoProfile -ExecutionPolicy Bypass -File (Join-Path $PSScriptRoot 'Test-UniversalScenarioLog.ps1') -LogPath $LogPath -ManifestPath $manifestFile -NativeExitPath $NativeExitPath -Transition
 if($LASTEXITCODE -ne 0){$failures.Add('PRODUCTION_TRANSITION_GATE')}
}finally{Remove-Item -LiteralPath $manifestFile -Force}
$all=(@(Get-ChildItem (Split-Path $LogPath -Parent) -File -Filter '*.log')|ForEach-Object{[IO.File]::ReadAllText($_.FullName)}) -join "`n"
if($all -match 'SCRIPT\s+\([EF]\)|ENGINE\s+\(F\)|MEMORY\s+\([EF]\)|Virtual Machine Exception|Access violation|Out of memory|Assertion failed'){$failures.Add('FULL_FATAL_OR_OOM')}
if($log -match '\[TRANSITION_RETURN_FAIL\]|\[TRANSITION_TIMEOUT\]|\[TRANSITION_ABORT\]'){$failures.Add('RETURN_FAILURE')}
$requests=@([regex]::Matches($log,'\[TRANSITION_RETURN_REQUEST\] production_gameplay_end=1'))
$menus=@([regex]::Matches($log,'\[TRANSITION_RETURN_MAIN_MENU\] actual_menu=1 linger_ms=120000'))
$done=@([regex]::Matches($log,'\[TRANSITION_RETURN_DONE\] actual_menu=1 elapsed_ms=(\d+) relaunch=0'))
if($requests.Count -ne 1 -or $menus.Count -ne 1 -or $done.Count -ne 1){$failures.Add('EXACT_RETURN_COMPLETION')}
elseif($requests[0].Index -ge $menus[0].Index -or $menus[0].Index -ge $done[0].Index -or [int]$done[0].Groups[1].Value -lt 120000){$failures.Add('CAUSAL_RETURN_LINGER')}
foreach($phase in @(1,2)){
 if(@([regex]::Matches($log,'\[TRANSITION_RETURN_STATE\] phase='+$phase+' restored=1')).Count -ne 1){$failures.Add('PERSISTENT_PHASE_'+$phase)}
}
$samples=@([regex]::Matches($log,'\[TRANSITION_RETURN_LINGER\] elapsed_ms=(\d+)'))
if($samples.Count -lt 4){$failures.Add('LINGER_OBSERVATION')}
$previous=-1
foreach($sample in $samples){$elapsed=[int]$sample.Groups[1].Value;if($elapsed -le $previous){$failures.Add('LINGER_MONOTONIC')};$previous=$elapsed}
$raw=@([regex]::Matches($all,'(?m)^.*\(E\).*$')).Count
Write-Output "FULL_RAW_ERROR_COUNT=$raw"
if($raw -gt 0){Write-Output 'STRICT_FULL_ERROR_GATE=FAIL'}else{Write-Output 'STRICT_FULL_ERROR_GATE=PASS'}
if($failures.Count){$failures|Sort-Object -Unique;exit 1}
'PASS Scenario return: exact production Play/roster/header/graph, stock gameplay end, main menu120s, no relaunch/OOM, native0/stopped'
exit 0
