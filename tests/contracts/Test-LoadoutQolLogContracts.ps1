param([string]$RepositoryRoot = (Split-Path -Parent (Split-Path -Parent $PSScriptRoot)))
$ErrorActionPreference='Stop'
$root=Join-Path ([IO.Path]::GetTempPath()) ('AICF-QolLog-'+[guid]::NewGuid().ToString('N'))
$auditor=Join-Path $RepositoryRoot 'tests/log-audits/Test-LoadoutQolLog.ps1'
$failures=[Collections.Generic.List[string]]::new()
$count=0
function Invoke-Case([string]$Name,[string]$Body,[string]$Role,[bool]$Expected,[string]$Exit='0',[string]$Extra='',[switch]$BadLauncher,[switch]$Files) {
    $folder=Join-Path $root $Name
    [void](New-Item -ItemType Directory -Path $folder)
    $log=Join-Path $folder 'console.log'
    $launch=Join-Path $folder 'launcher.txt'
    $exitPath=Join-Path $folder 'exit.txt'
    $nativeArguments=@('-aicfLoadoutQolProbe','1')
    if ($Files) { $nativeArguments+=@('-aicfLoadoutFilesProbe','1') }
    $manifest=@{role=$Role;arguments=$nativeArguments} | ConvertTo-Json -Compress
    $launcher="AICF_RUNTIME_MANIFEST_JSON=$manifest`n[AICF][RUNTIME_LAUNCHER][SERVER_READY] process_id=1 port=2001 log=test`n"
    if ($BadLauncher) { $launcher=$launcher.Replace('port=2001','port=2002') }
    [IO.File]::WriteAllText($log,"CLI Params: $($nativeArguments -join ' ')`n$Body`nGame destroyed.`n")
    [IO.File]::WriteAllText($launch,$launcher)
    [IO.File]::WriteAllText($exitPath,$Exit)
    if ($Extra) { [IO.File]::WriteAllText((Join-Path $folder 'error.log'),$Extra) }
    $arguments=@('-NoProfile','-ExecutionPolicy','Bypass','-File',$auditor,'-LogPath',$log,'-LauncherPath',$launch,'-NativeExitPath',$exitPath)
    if ($Role -eq 'Client') { $arguments+='-Client' }
    if ($Files) { $arguments+='-Files' }
    $output=@(& powershell.exe @arguments 2>&1 | ForEach-Object ToString)
    $passed=$LASTEXITCODE -eq 0
    if ($passed -ne $Expected) { $failures.Add("$Name expected=$Expected actual=$passed $($output -join ' ')") }
    $script:count++
}
try {
    [void](New-Item -ItemType Directory -Path $root)
    $server=''
    foreach ($side in @('US','USSR')) {
        foreach ($rule in @('STORAGE_CHANGED','STORAGE_VALIDATED','LEGACY_WRITE','LEGACY_READ','UPSERT','DEDUPE','RELOAD','PARTIAL_WRITE_PRESERVES','DELETE','NO_LEGACY_REVIVAL','BATCH_STALE_ATOMIC','BATCH_INVALID_ATOMIC','BATCH_COMMIT','BINDING_INDEPENDENCE')) { $server+="[AICF][QOL_CHECK] rule=${rule}_$side passed=1`n" }
    }
    foreach ($i in 0..9) { $server+="[AICF][QOL_CHECK] rule=ROLE_SLOT_$i passed=1`n[AICF][QOL_CHECK] rule=SQUAD_POSITION_$i passed=1`n[AICF][QOL_CHECK] rule=SQUAD_POSITION_$i passed=1`n[AICF][QOL_CHECK] rule=APPLY_NO_LIBRARY_WRITE_$i passed=1`n" }
    foreach ($rule in @('SQUAD_BAD_LAST_ATOMIC','ENEMY_DENIED','ADMIN_GRANT','ENEMY_ADMIN_READ','ADMIN_REVOKE','ENEMY_REVOKED','CLIENT_CONNECTED')) { $server+="[AICF][QOL_CHECK] rule=$rule passed=1`n" }
    $server+="[AICF][QOL_STORAGE_DONE] failures=0`n[AICF][QOL_SERVER_DONE] failures=0`n"
    $client=''
    foreach ($i in 1..28) { $client+="[AICF][QOL_CHECK] rule=RPC_STEP_$i passed=1`n" }
    foreach ($rule in @('CLIENT_CHANGED_DRAFT','REPEATED_APPLY_NO_REVISION','REPEATED_SAVE_NO_REVISION','UPDATE_SAME_ID','DELETED_FROM_LIST','SQUAD_DELETED_FROM_LIST')) { $client+="[AICF][QOL_CHECK] rule=$rule passed=1`n" }
    foreach ($pair in @(@(12,'LIBRARY_CONFLICT'),@(13,'INVALID_RECIPE'),@(20,'MATCH_OR_FACTION_UNAVAILABLE'),@(25,'TEMPLATE_UNAVAILABLE'),@(28,'MATCH_OR_FACTION_UNAVAILABLE'))) { $client+="[AICF][QOL_RPC] step=$($pair[0]) accepted=0 reason=$($pair[1]) revision=1 library=1`n" }
    $client+="[AICF][QOL_CLIENT_DONE] failures=0 step=29`n"
    Invoke-Case 'server-positive' $server 'Server' $true
    Invoke-Case 'client-positive' $client 'Client' $true
    Invoke-Case 'failed-case' ($server.Replace('rule=ROLE_SLOT_9 passed=1','rule=ROLE_SLOT_9 passed=0')) 'Server' $false
    Invoke-Case 'missing-partial-write' ($server.Replace('rule=PARTIAL_WRITE_PRESERVES_US passed=1','rule=OTHER passed=1')) 'Server' $false
    Invoke-Case 'missing-atomic' ($server.Replace('rule=SQUAD_BAD_LAST_ATOMIC passed=1','rule=OTHER passed=1')) 'Server' $false
    Invoke-Case 'missing-apply-write' ($server.Replace('rule=APPLY_NO_LIBRARY_WRITE_9 passed=1','rule=OTHER passed=1')) 'Server' $false
    Invoke-Case 'missing-rpc' ($client.Replace('rule=RPC_STEP_28 passed=1','rule=OTHER passed=1')) 'Client' $false
    Invoke-Case 'wrong-denial' ($client.Replace('reason=LIBRARY_CONFLICT','reason=OTHER')) 'Client' $false
    Invoke-Case 'native-failure' $client 'Client' $false '-1'
    Invoke-Case 'done-failure' ($client.Replace('failures=0 step=29','failures=1 step=29')) 'Client' $false
    Invoke-Case 'duplicate-done' ($server+"[AICF][QOL_SERVER_DONE] failures=0`n") 'Server' $false
    Invoke-Case 'neighbor-script-error' $server 'Server' $false '0' 'SCRIPT (E): Virtual Machine Exception'
    Invoke-Case 'readiness-port' $client 'Client' $false -BadLauncher
    $fileEvents=''
    foreach ($i in 0..9) { $fileEvents+="[AICF][QOL_CHECK] rule=FILE_IMAGE_UNCHANGED_$i passed=1`n" }
    $serverFiles=$server.Replace('[AICF][QOL_SERVER_DONE]',($fileEvents+'[AICF][QOL_SERVER_DONE]'))
    Invoke-Case 'files-positive' $serverFiles 'Server' $true -Files
    Invoke-Case 'files-missing' ($serverFiles.Replace('rule=FILE_IMAGE_UNCHANGED_9 passed=1','rule=OTHER passed=1')) 'Server' $false -Files
    Invoke-Case 'files-changed' ($serverFiles.Replace('rule=FILE_IMAGE_UNCHANGED_9 passed=1','rule=FILE_IMAGE_UNCHANGED_9 passed=0')) 'Server' $false -Files
    if ($failures.Count) { $failures | ForEach-Object { "FAIL $_" }; exit 1 }
    Write-Output "PASS Loadout QoL log contracts; cases=$count negative=13 runtime=NOT_RUN"
} finally {
    $full=[IO.Path]::GetFullPath($root)
    $temp=[IO.Path]::GetFullPath([IO.Path]::GetTempPath()).TrimEnd('\')+'\'
    if ($full.StartsWith($temp,[StringComparison]::OrdinalIgnoreCase) -and (Split-Path -Leaf $full).StartsWith('AICF-QolLog-')) { Remove-Item -LiteralPath $full -Recurse -Force }
}
