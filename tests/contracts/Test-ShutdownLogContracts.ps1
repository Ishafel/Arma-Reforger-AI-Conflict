$ErrorActionPreference='Stop'
$repo=Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$root=Join-Path $repo '.codex-runtime/shutdown-log-contracts'
[void][IO.Directory]::CreateDirectory($root)
$audit=Join-Path $repo 'tests/log-audits/Test-ShutdownLog.ps1'
$launcher='AICF_RUNTIME_MANIFEST_JSON={"role":"Server","arguments":["-aicfShutdownMode","idle"]}'
$log=@('[AICF][SHUTDOWN_DONE] mode=idle steps=4 connected=0 elapsed_ms=55000')
foreach($phase in @('ready','cleanup_0','cleanup_1','cleanup_2','cleanup_3','rest')) {$log+="[AICF][SHUTDOWN_MEMORY] phase=$phase name=MemC value=123"}
$log+=@('[AICF][SHUTDOWN_OBJECTS] phase=after_stop drafts=0 created=2 destroyed=2','[AICF][SHUTDOWN_EQUIPMENT_CLEAR] items=75 alive=0','ENGINE : Game destroyed.')
$positive=$log -join "`n"
$positive+="`n[AICF][SHUTDOWN_RETAINED] phase=rest sampled=75 alive=0`n[AICF][SHUTDOWN_RETAINED] phase=after_stop sampled=75 alive=0"
function Check([string]$name,[string]$text,[string]$rule,[string]$launch=$launcher,[switch]$Client,[switch]$Batches) {
    $path=Join-Path $root "$name.fixture"
    [IO.File]::WriteAllText($path,$text)
    [IO.File]::WriteAllText((Join-Path $root 'launcher.fixture'),$launch)
    [IO.File]::WriteAllText((Join-Path $root 'exit.fixture'),'0')
    $arguments=@('-NoProfile','-ExecutionPolicy','Bypass','-File',$audit,'-LogPath',$path,'-LauncherPath',(Join-Path $root 'launcher.fixture'),'-NativeExitPath',(Join-Path $root 'exit.fixture'))
    if($Client){$arguments+='-Client'}else{$arguments+='-RequireStop'}
    if($Batches){$arguments+='-Batches'}
    $output=& powershell.exe @arguments 2>&1
    $expected=0;if($rule){$expected=1}
    if($LASTEXITCODE -ne $expected -or ($rule -and ($output -join "`n") -notmatch $rule)){throw "$name unexpected audit: $output"}
    Write-Output "PASS shutdown audit case=$name exit=$LASTEXITCODE"
}
Check 'positive-idle' $positive ''
Check 'native-leak' ($positive+"`nSCRIPT (E): Leaked 'PointInfo' script instance (45x)!") 'SCRIPT_FATAL'
Check 'active-log' ($positive.Replace('ENGINE : Game destroyed.','')) 'FULL_STOPPED_LOG_REQUIRED'
Check 'retained-items' ($positive.Replace('items=75 alive=0','items=75 alive=75')) 'NATIVE_ITEMS_RETAINED'
Check 'delayed-retained-items' ($positive.Replace('phase=rest sampled=75 alive=0','phase=rest sampled=75 alive=75')) 'DELAYED_NATIVE_ITEMS_RETAINED'
Check 'retained-draft' ($positive.Replace('drafts=0 created=2 destroyed=2','drafts=1 created=2 destroyed=1')) 'DRAFTS_RETAINED'
Check 'missing-readback' ($positive.Replace('phase=rest name=MemC','phase=other name=MemC')) 'MEMORY_rest'
Check 'missing-manifest' $positive 'MANIFEST' 'no manifest'
$batch=$positive.Replace('mode=idle','mode=batches')
$batchLauncher='AICF_RUNTIME_MANIFEST_JSON={"role":"Server","arguments":["-aicfShutdownMode","batches","-aicfShutdownEquipmentBatch","1"]}'
foreach($number in 1..3){
    foreach($iteration in 0..3){foreach($side in @('USSR','US')){$batch+="`n[AICF][SHUTDOWN_OPERATION] batch=$number iteration=$iteration side=$side passed=1"}}
    $batch+="`n[AICF][SHUTDOWN_BATCH] batch=$number operations=8 failures=0"
}
Check 'positive-batches' $batch '' $batchLauncher -Batches
Check 'idle-batch-manifest' $positive 'IDLE_MODE_REQUIRED' $batchLauncher
Check 'idle-batch-completion' $batch 'IDLE_COMPLETION_MODE'
Check 'idle-batch-run' $batch '(?s)IDLE_MODE_REQUIRED.*IDLE_COMPLETION_MODE' $batchLauncher
Check 'idle-wrong-mode' $positive 'IDLE_MODE_REQUIRED' ($launcher.Replace('"idle"','"other"'))
Check 'idle-missing-mode' $positive 'IDLE_MODE_REQUIRED' ($launcher.Replace('"-aicfShutdownMode","idle"',''))
Check 'idle-missing-mode-value' $positive 'IDLE_MODE_REQUIRED' ($launcher.Replace(',"idle"',''))
Check 'idle-unpaired-mode-value' $positive 'IDLE_MODE_REQUIRED' ($launcher.Replace('"-aicfShutdownMode","idle"','"-aicfShutdownMode","-other","idle"'))
Check 'idle-duplicate-mode' $positive 'IDLE_MODE_REQUIRED' ($launcher.Replace('"idle"]','"idle","-aicfShutdownMode","idle"]'))
Check 'idle-conflicting-mode' $positive 'IDLE_MODE_REQUIRED' ($launcher.Replace('"idle"]','"idle","-aicfShutdownMode","batches"]'))
Check 'idle-mode-case' $positive 'IDLE_MODE_REQUIRED' ($launcher.Replace('"idle"','"Idle"'))
Check 'batches-idle-manifest' $batch 'BATCH_MODE_REQUIRED' $launcher -Batches
Check 'batches-wrong-mode' $batch 'BATCH_MODE_REQUIRED' ($batchLauncher.Replace('"batches"','"idle"')) -Batches
Check 'batches-missing-mode' $batch 'BATCH_MODE_REQUIRED' ($batchLauncher.Replace('"-aicfShutdownMode","batches",','')) -Batches
Check 'batches-missing-equipment' $batch 'EQUIPMENT_BATCH_REQUIRED' ($batchLauncher.Replace(',"-aicfShutdownEquipmentBatch","1"','')) -Batches
Check 'batches-disabled-equipment' $batch 'EQUIPMENT_BATCH_REQUIRED' ($batchLauncher.Replace('"1"','"0"')) -Batches
Check 'batches-missing-mode-value' $batch 'BATCH_MODE_REQUIRED' ($batchLauncher.Replace('"batches",','')) -Batches
Check 'batches-missing-equipment-value' $batch 'EQUIPMENT_BATCH_REQUIRED' ($batchLauncher.Replace(',"1"','')) -Batches
Check 'batches-duplicate-mode' $batch 'BATCH_MODE_REQUIRED' ($batchLauncher.Replace('"batches",','"batches","-aicfShutdownMode","idle",')) -Batches
Check 'batches-duplicate-equipment' $batch 'EQUIPMENT_BATCH_REQUIRED' ($batchLauncher.Replace('"1"]','"1","-aicfShutdownEquipmentBatch","0"]')) -Batches
Check 'batches-unpaired-equipment-value' $batch 'EQUIPMENT_BATCH_REQUIRED' ($batchLauncher.Replace('"-aicfShutdownEquipmentBatch","1"','"-aicfShutdownEquipmentBatch","-other","1"')) -Batches
Check 'batches-wrong-completion-mode' ($batch.Replace('mode=batches','mode=idle')) 'BATCH_COMPLETION_MODE' $batchLauncher -Batches
Check 'missing-batch' ($batch.Replace('batch=3 operations=8 failures=0','batch=0 operations=8 failures=0')) 'BATCH_3' $batchLauncher -Batches
Check 'failed-operation' ($batch.Replace('side=US passed=1','side=US passed=0')) 'OPERATIONS' $batchLauncher -Batches
Check 'duplicate-operation' ($batch.Replace('iteration=3','iteration=0')) 'OPERATION_IDENTITIES' $batchLauncher -Batches
$client='[AICF][SHUTDOWN_CLIENT_DONE]'+"`nENGINE : Game destroyed."
$clientLauncher='AICF_RUNTIME_MANIFEST_JSON={"role":"Client","arguments":["-aicfShutdownMode","idle"]}'+"`n[AICF][RUNTIME_LAUNCHER][SERVER_READY] process_id=123 port=2361 log=fixture"
Check 'positive-client' $client '' $clientLauncher -Client
Check 'positive-batches-client' $client '' ($clientLauncher.Replace('"idle"','"batches"')) -Client
Check 'client-no-gate' $client 'READINESS_GATE' ($clientLauncher.Replace('[SERVER_READY]','[PENDING]')) -Client
Write-Output 'PASS Shutdown log contracts: 4 positive and 32 negative cases; real runtime is a separate gate'
exit 0
