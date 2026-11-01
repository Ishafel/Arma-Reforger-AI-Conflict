param([Parameter(Mandatory)][string]$LogPath,[Parameter(Mandatory)][string]$ManifestPath,[Parameter(Mandatory)][string]$NativeExitPath)
$ErrorActionPreference='Stop'
$transition=& (Join-Path $PSScriptRoot 'Test-UniversalScenarioLog.ps1') -LogPath $LogPath -ManifestPath $ManifestPath -NativeExitPath $NativeExitPath -Transition
if($LASTEXITCODE -ne 0){$transition;exit 1}
$log=[IO.File]::ReadAllText($LogPath)
$fullLogs=(@(Get-ChildItem -LiteralPath (Split-Path -Parent $LogPath) -File -Filter '*.log')|ForEach-Object {[IO.File]::ReadAllText($_.FullName)}) -join "`n"
$manifest=[IO.File]::ReadAllText($ManifestPath)|ConvertFrom-Json
$argv=@($manifest.arguments)
$index=[array]::IndexOf($argv,'-aicfCigaretteProbe')
$failures=@()
if($index -lt 0 -or $argv[$index+1] -ne '1' -or @($argv|Where-Object {$_ -eq '-aicfCigaretteProbe'}).Count -ne 1 -or $log -notmatch 'CLI Params:[^\r\n]*-aicfCigaretteProbe 1'){$failures+='PROBE_CLI'}
if($fullLogs -match 'Assertion failed|Attempt to modify locked entity|Access violation'){$failures+='NATIVE_ASSERT'}
if($fullLogs -match 'ParticleEffectComponent:\s*Suspicious parent velocity'){$failures+='PARTICLE_POSE_VELOCITY'}
if($fullLogs -match 'SCRIPT\s+\([EF]\)|ENGINE\s+\(F\)|NULL pointer|Virtual Machine Exception'){$failures+='FULL_LOG_FATAL'}
$cases=@([regex]::Matches($log,'\[CIGARETTE_CASE\] case=(\w+) cycle=(\d+) pass=([01])'))
$names=@('PARENT','PROP','COMPONENT','FRAME_MASK','PLAYBACK','DETACH','DELETE')
if($cases.Count -ne 42){$failures+='CASE_COUNT'}
for($cycle=0;$cycle -lt 6;$cycle++){
 foreach($name in $names){
  $found=@($cases|Where-Object {$_.Groups[1].Value -ceq $name -and $_.Groups[2].Value -eq "$cycle"})
  if($found.Count -ne 1 -or $found[0].Groups[3].Value -ne '1'){$failures+="CASE_$($name)_$cycle"}
 }
}
$done=@([regex]::Matches($log,'\[CIGARETTE_DONE\] checks=(\d+) failed=(\d+) cycles=(\d+)'))
if($done.Count -ne 1 -or $done[0].Value -cne '[CIGARETTE_DONE] checks=42 failed=0 cycles=6'){$failures+='COMPLETION'}
if($failures.Count){$failures;exit 1}
'Cigarette full stopped log: PASS; cycles=6; cases=42; native_transition=PASS'
