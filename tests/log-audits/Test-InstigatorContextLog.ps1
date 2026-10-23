param(
    [Parameter(Mandatory=$true)][string]$LogPath,
    [Parameter(Mandatory=$true)][string]$LauncherPath,
    [Parameter(Mandatory=$true)][string]$NativeExitPath,
    [switch]$Player
)
$ErrorActionPreference='Stop'
$output=& (Join-Path $PSScriptRoot 'Test-CoopScenarioLog.ps1') -LogPath $LogPath -LauncherPath $LauncherPath -NativeExitPath $NativeExitPath
$baseExit=$LASTEXITCODE
$output
$log=[IO.File]::ReadAllText($LogPath)
$launcher=[IO.File]::ReadAllText($LauncherPath)
$failures=[Collections.Generic.List[string]]::new()
if($baseExit -ne 0){$failures.Add('BASE_RUNTIME')}
if($launcher -notmatch 'AICF_RUNTIME_MANIFEST_JSON=({[^\r\n]+})'){throw 'Missing runtime manifest'}
$manifest=$matches[1]|ConvertFrom-Json
$argsList=@($manifest.arguments)
foreach($flag in @('aicfInstigatorProbe')){
    $indices=@(for($i=0;$i -lt $argsList.Count;$i++){if($argsList[$i] -ceq "-$flag"){$i}})
    if($indices.Count -ne 1 -or $indices[0]+1 -ge $argsList.Count -or $argsList[$indices[0]+1] -cne '1'){$failures.Add('PROBE_ARGUMENT')}
}
$expected=@('REAL_OTHER_TYPE','OTHER_RELATION','OTHER_METADATA','OTHER_CONTROL','NATIVE_FRIENDLY_AI','REUSED_CONTEXT','OTHER_CHARACTER_OVERRIDE','NATIVE_SUICIDE','NATIVE_GM','NATIVE_NONE','NATIVE_DELETED','NATIVE_NON_CHARACTER','NATIVE_NULL_VICTIM','NATIVE_DEATH_EVENT','NATIVE_DEATH_RELATION','REAL_CHARACTER_DIED','DEATH_COMPLETED')
if($manifest.matchMode -eq 'Standard'){$expected+='NATIVE_ENEMY_AI'}
if($Player){
    $expected+=@('NATIVE_PLAYER','OTHER_PLAYER_OVERRIDE','OTHER_PLAYER_VICTIM')
    $index=[array]::IndexOf($argsList,'-aicfInstigatorPlayer')
    if($index -lt 0 -or $index+1 -ge $argsList.Count -or $argsList[$index+1] -cne '1'){$failures.Add('PLAYER_ARGUMENT')}
}
foreach($name in $expected){if([regex]::Matches($log,"\[INSTIGATOR_CASE\] case=$name pass=1\b").Count -ne 1){$failures.Add("CASE_$name")}}
if([regex]::Matches($log,'\[INSTIGATOR_CASE\]').Count -ne $expected.Count){$failures.Add('CASE_COUNT')}
if([regex]::Matches($log,"\[INSTIGATOR_DONE\] checks=$($expected.Count) failed=0\b").Count -ne 1){$failures.Add('COMPLETION')}
if($log -match '\[INSTIGATOR_CASE\][^\r\n]*pass=0'){$failures.Add('FAILED_CASE')}
if($failures.Count){$failures|ForEach-Object {"FAIL $_"};exit 1}
"Instigator full stopped log: PASS; cases=$($expected.Count)"
