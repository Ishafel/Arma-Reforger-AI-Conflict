param([Parameter(Mandatory)][string]$LogPath,[Parameter(Mandatory)][string]$LauncherPath,[Parameter(Mandatory)][string]$NativeExitPath,[switch]$Client)
$ErrorActionPreference='Stop'
$base=@{LogPath=$LogPath;LauncherPath=$LauncherPath;NativeExitPath=$NativeExitPath}
if($Client){$base.Client=$true;$base.RequirePlayer=$true;$base.RequireWelcome=$true}
& (Join-Path $PSScriptRoot 'Test-CoopScenarioLog.ps1') @base
if($LASTEXITCODE -ne 0){exit 1}
$log=[IO.File]::ReadAllText($LogPath)
$launcher=[IO.File]::ReadAllText($LauncherPath)
$failures=@()
if($launcher -notmatch 'AICF_RUNTIME_MANIFEST_JSON=({[^\r\n]+})'){throw 'Missing native manifest'}
$manifest=$matches[1]|ConvertFrom-Json
$argv=@($manifest.arguments)
$indices=@(for($i=0;$i -lt $argv.Count;$i++){if($argv[$i] -ceq '-aicfSetupProbe'){$i}})
if($indices.Count -ne 1 -or $indices[0]+1 -ge $argv.Count -or $argv[$indices[0]+1] -cne '1'){$failures+='SETUP_CLI'}
$expected=@('HEADER','DEFAULT')
for($mode=0;$mode -lt 3;$mode++){for($difficulty=0;$difficulty -lt 3;$difficulty++){$expected+="PRESET_${mode}_${difficulty}"}}
$expected+=@('ACTIVE_PRESET','INVALID_BOUNDS','FAILURE_PRESERVES_SELECTION','NULL_HEADER','INVALID_MAP','INVALID_CONTENT','NO_RECURSION')
if($Client){$expected=@('UI_OPEN','UI_BOUNDS','UI_MODAL');for($mode=0;$mode -lt 3;$mode++){for($difficulty=0;$difficulty -lt 3;$difficulty++){$expected+="UI_${mode}_${difficulty}"}};$expected+=@('UI_INVALID','UI_CLOSED','UI_CLOSE_IDEMPOTENT')}
$cases=@([regex]::Matches($log,'\[SETUP_CASE\] case=(\w+) pass=([01])'))
if($cases.Count -ne $expected.Count){$failures+='CASE_COUNT'}
foreach($name in $expected){$found=@($cases|Where-Object {$_.Groups[1].Value -ceq $name});if($found.Count -ne 1 -or $found[0].Groups[2].Value -cne '1'){$failures+="CASE_$name"}}
$done=@([regex]::Matches($log,'\[SETUP_DONE\] checks=(\d+) failed=(\d+)'))
if($done.Count -ne 1 -or $done[0].Groups[1].Value -ne "$($expected.Count)" -or $done[0].Groups[2].Value -ne '0'){$failures+='COMPLETION'}
if($failures.Count){$failures;exit 1}
"Scenario setup full stopped log: PASS; cases=$($expected.Count)"
exit 0
