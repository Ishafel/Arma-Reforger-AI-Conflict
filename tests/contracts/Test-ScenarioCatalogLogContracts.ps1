param([string]$RepositoryRoot=(Split-Path -Parent (Split-Path -Parent $PSScriptRoot)))
$ErrorActionPreference='Stop'
$tempBase=[IO.Path]::GetFullPath([IO.Path]::GetTempPath())
$work=Join-Path $tempBase ('aicf-catalog-log-'+[guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $work|Out-Null
$audit=Join-Path $RepositoryRoot 'tests/log-audits/Test-ScenarioCatalogLog.ps1'
$manifest=@{profile='C:\fresh profile';addonsDir='C:\source,C:\game\addons';arguments=@('-gproj','C:\source\AIConflictArland\addon.gproj','-profile','C:\fresh profile','-addonsDir','C:\source,C:\game\addons','-aicfCatalogProbe','1')}|ConvertTo-Json -Compress
$names=@('EXAMPLE','VISIBLE_NAME','REPEAT','STOCK_PRESERVED','HIDDEN_PRESETS')
for($mode=0;$mode -lt 3;$mode++){for($difficulty=0;$difficulty -lt 3;$difficulty++){$names+="TARGET_${mode}_${difficulty}"}}
$names+=@('FILTER_TYPE','FILTER_MANW')
$log=(@('CLI Params: -gproj C:\source\AIConflictArland\addon.gproj -profile C:\fresh profile -addonsDir C:\source,C:\game\addons -aicfCatalogProbe 1')+@($names|ForEach-Object {"[AICF][CATALOG_CASE] case=$_ pass=1"})+@('[AICF][CATALOG_DONE] checks=16 failed=0','ENGINE : Game destroyed.'))-join "`n"
$count=0
function Check([string]$Name,[string]$Log,[string]$Manifest,[int]$Exit,[int]$Expected,[string]$Rule){
    [IO.File]::WriteAllText("$work/log.txt",$Log)
    [IO.File]::WriteAllText("$work/manifest.json",$Manifest)
    [IO.File]::WriteAllText("$work/exit.txt","$Exit")
    $output=& powershell.exe -NoProfile -ExecutionPolicy Bypass -File $audit -LogPath "$work/log.txt" -ManifestPath "$work/manifest.json" -NativeExitPath "$work/exit.txt" 2>&1
    if($LASTEXITCODE -ne $Expected -or ($Rule -and ($output-join "`n") -notmatch $Rule)){throw "Unexpected ${Name}: $output"}
    $script:count++
    "PASS $Name"
}
try{
    Check 'positive' $log $manifest 0 0 ''
    Check 'missing-case' ($log.Replace('[AICF][CATALOG_CASE] case=EXAMPLE pass=1','')) $manifest 0 1 'CASE_EXAMPLE'
    Check 'failed-case' ($log.Replace('case=EXAMPLE pass=1','case=EXAMPLE pass=0')) $manifest 0 1 'CASE_EXAMPLE'
    Check 'duplicate-case' ($log+"`n[AICF][CATALOG_CASE] case=EXAMPLE pass=1") $manifest 0 1 'CASE_EXAMPLE'
    Check 'missing-hidden-presets' ($log.Replace('[AICF][CATALOG_CASE] case=HIDDEN_PRESETS pass=1','')) $manifest 0 1 'CASE_HIDDEN_PRESETS'
    Check 'visible-presets' ($log.Replace('case=HIDDEN_PRESETS pass=1','case=HIDDEN_PRESETS pass=0')) $manifest 0 1 'CASE_HIDDEN_PRESETS'
    Check 'missing-done' ($log.Replace('[AICF][CATALOG_DONE] checks=16 failed=0','')) $manifest 0 1 'COMPLETION'
    Check 'failed-done' ($log.Replace('checks=16 failed=0','checks=16 failed=1')) $manifest 0 1 'COMPLETION'
    Check 'duplicate-done' ($log+"`n[AICF][CATALOG_DONE] checks=16 failed=0") $manifest 0 1 'COMPLETION'
    Check 'script-error' ($log+"`nSCRIPT (E): constructor is not public") $manifest 0 1 'SCRIPT_VM_FATAL'
    Check 'fatal' ($log+"`nENGINE (F): Crashed") $manifest 0 1 'SCRIPT_VM_FATAL'
    Check 'vm' ($log+"`nNULL pointer to instance") $manifest 0 1 'SCRIPT_VM_FATAL'
    Check 'running' ($log.Replace('Game destroyed.','running')) $manifest 0 1 'STOPPED_LOG'
    Check 'wrong-cli' ($log.Replace('C:\fresh profile','C:\old profile')) $manifest 0 1 'CLI_MISMATCH'
    Check 'missing-probe' $log ($manifest.Replace('-aicfCatalogProbe','-unknown')) 0 1 'PROBE_CLI'
    Check 'bad-native-exit' $log $manifest 1 1 'NATIVE_EXIT'
}
finally{
    $target=[IO.Path]::GetFullPath($work)
    if(!$target.StartsWith($tempBase,[StringComparison]::OrdinalIgnoreCase) -or (Split-Path -Leaf $target) -notlike 'aicf-catalog-log-*'){throw 'Unsafe cleanup'}
    Remove-Item -LiteralPath $target -Recurse -Force
}
"Scenario catalog log contracts: PASS; cases=$count"
