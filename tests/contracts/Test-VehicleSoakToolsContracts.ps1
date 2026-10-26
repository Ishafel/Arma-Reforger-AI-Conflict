param([string]$RepositoryRoot=(Split-Path -Parent (Split-Path -Parent $PSScriptRoot)))
$ErrorActionPreference='Stop'
. (Join-Path $RepositoryRoot 'tools/AICF_VehicleSoakSources.ps1')
$taskRoot=Join-Path $RepositoryRoot ('.codex-runtime/soak-tools-contract-'+[guid]::NewGuid().ToString('N'))
[void][IO.Directory]::CreateDirectory($taskRoot)
$taskStage=Join-Path $taskRoot 'stage'
[void][IO.Directory]::CreateDirectory($taskStage)
$taskControls=0
function Reject([scriptblock]$Action){
    $rejected=$false
    try{& $Action}catch{$rejected=$true}
    if(!$rejected){throw 'Source mutation was accepted'}
    $script:taskControls++
}
try{
    $taskSource=Join-Path $taskStage 'actor.c'
    Set-Content -LiteralPath $taskSource 'original source'
    $taskSnapshot=@(Get-AICFVehicleSoakSources $taskStage)
    Assert-AICFVehicleSoakSources $taskStage $taskSnapshot
    $taskControls++
    Set-Content -LiteralPath (Join-Path $taskStage 'resourceDatabase.rdb') 'generated'
    Set-Content -LiteralPath (Join-Path $taskStage 'console.log') 'native output'
    Assert-AICFVehicleSoakSources $taskStage $taskSnapshot
    $taskControls++
    Set-Content -LiteralPath $taskSource 'changed source'
    Reject {Assert-AICFVehicleSoakSources $taskStage $taskSnapshot}
    Set-Content -LiteralPath $taskSource 'original source'
    $taskAdded=Join-Path $taskStage 'added.c'
    Set-Content -LiteralPath $taskAdded 'added source'
    Reject {Assert-AICFVehicleSoakSources $taskStage $taskSnapshot}
    Remove-Item -LiteralPath $taskAdded
    Move-Item -LiteralPath $taskSource -Destination (Join-Path $taskStage 'renamed.c')
    Reject {Assert-AICFVehicleSoakSources $taskStage $taskSnapshot}
    Move-Item -LiteralPath (Join-Path $taskStage 'renamed.c') -Destination $taskSource
    Reject {Assert-AICFVehicleSoakSources $taskStage @($taskSnapshot[0],$taskSnapshot[0])}
    $taskEscaped=[pscustomobject]@{path=(Join-Path $taskRoot 'outside.c');sha256=$taskSnapshot[0].sha256}
    Reject {Assert-AICFVehicleSoakSources $taskStage @($taskEscaped)}
    Remove-Item -LiteralPath $taskSource
    Reject {Assert-AICFVehicleSoakSources $taskStage $taskSnapshot}
    # Реальный watcher повторно читает остановленный лог, сохраняя единый индекс.
    $taskWatch=Join-Path $taskRoot 'watch'
    $taskLogs=Join-Path $taskWatch 'profile/logs/native'
    [void][IO.Directory]::CreateDirectory($taskLogs)
    $taskAlert='12:00:00 SCRIPT    (E): synthetic watcher failure'
    @($taskAlert,'12:00:01 ENGINE       : Game destroyed.')|Set-Content -LiteralPath (Join-Path $taskLogs 'console.log')
    foreach($taskAttempt in 1..2){
        $taskShell=(Get-Command powershell.exe -CommandType Application).Source
        & $taskShell -NoProfile -File (Join-Path $RepositoryRoot 'tools/Watch-AICFVehicleSoak.ps1') -EvidenceRoot $taskWatch *> (Join-Path $taskRoot "watch-$taskAttempt.txt")
        if($LASTEXITCODE -ne 0){throw "Watcher attempt failed: $taskAttempt"}
        $taskEvents=@(Get-Content -LiteralPath (Join-Path $taskWatch 'analysis-events.jsonl'))
        $taskState=Get-Content -LiteralPath (Join-Path $taskWatch 'analysis-live.json') -Raw|ConvertFrom-Json
        if($taskEvents.Count -ne 1 -or $taskState.alerts -ne 1 -or !$taskState.stopped){throw 'Restart duplicated watcher events or lost their count'}
        $taskControls++
    }
    @('Assertion failed','Reason: Entity out of world bounds. synthetic crash')|Set-Content -LiteralPath (Join-Path $taskLogs 'crash.log')
    foreach($taskAttempt in 3..4){
        & $taskShell -NoProfile -File (Join-Path $RepositoryRoot 'tools/Watch-AICFVehicleSoak.ps1') -EvidenceRoot $taskWatch *> (Join-Path $taskRoot "watch-$taskAttempt.txt")
        if($LASTEXITCODE -ne 0){throw 'Crash observer failed'}
        $taskEvents=@(Get-Content -LiteralPath (Join-Path $taskWatch 'analysis-events.jsonl')|ForEach-Object{$_|ConvertFrom-Json})
        $taskState=Get-Content -LiteralPath (Join-Path $taskWatch 'analysis-live.json') -Raw|ConvertFrom-Json
        if($taskEvents.Count -ne 2 -or @($taskEvents|Where-Object severity -eq 'NATIVE_CRASH').Count -ne 1 -or $taskState.alerts -ne 2){throw 'Sibling native crash missing or duplicated'}
        $taskControls++
    }
    $taskMultiWatch=Join-Path $taskRoot 'multi-watch'
    $taskMultiLogs=Join-Path $taskMultiWatch 'profile/logs/native'
    [void][IO.Directory]::CreateDirectory($taskMultiLogs)
    $taskMultiHeartbeat='12:01:00 SCRIPT : [AICF][VEHICLE_MULTI_HEARTBEAT] mode=CAPACITY cohort=4 us_exact_active=4 ussr_exact_active=4'
    @($taskMultiHeartbeat,'12:01:01 SCRIPT : [AICF][VEHICLE_MULTI_FAILURE] reason=SYNTHETIC_FAILURE','12:01:02 Game destroyed.')|Set-Content -LiteralPath (Join-Path $taskMultiLogs 'console.log')
    & $taskShell -NoProfile -File (Join-Path $RepositoryRoot 'tools/Watch-AICFVehicleSoak.ps1') -EvidenceRoot $taskMultiWatch *> (Join-Path $taskRoot 'watch-multi.txt')
    if($LASTEXITCODE -ne 0){throw 'Multi-squad watcher failed'}
    $taskMultiState=Get-Content -LiteralPath (Join-Path $taskMultiWatch 'analysis-live.json') -Raw|ConvertFrom-Json
    $taskMultiEvents=@(Get-Content -LiteralPath (Join-Path $taskMultiWatch 'analysis-events.jsonl')|ForEach-Object{$_|ConvertFrom-Json})
    if($taskMultiState.latest_heartbeat -ne $taskMultiHeartbeat -or $taskMultiEvents.Count -ne 1 -or $taskMultiEvents[0].severity -ne 'FAILURE'){throw 'Multi-squad heartbeat or failure missing'}
    $taskControls++
    # Реальная блокировка файла и фактическое условие остановки из AST watcher.
    $taskTokens=$null;$taskParseErrors=$null
    $taskWatcherAst=[Management.Automation.Language.Parser]::ParseFile((Join-Path $RepositoryRoot 'tools/Watch-AICFVehicleSoak.ps1'),[ref]$taskTokens,[ref]$taskParseErrors)
    if($taskParseErrors){throw 'Watcher parse error'}
    $taskAlertFunction=$taskWatcherAst.Find({param($node) $node -is [Management.Automation.Language.FunctionDefinitionAst] -and $node.Name -eq 'Add-AICFSoakAlert'},$true)
    . ([scriptblock]::Create($taskAlertFunction.Extent.Text))
    $taskStopIf=$taskWatcherAst.Find({param($node) $node -is [Management.Automation.Language.IfStatementAst] -and $node.Clauses[0].Item1.Extent.Text.Contains("Contains('Game destroyed.')")},$true)
    $taskStopCondition=[scriptblock]::Create($taskStopIf.Clauses[0].Item1.Extent.Text)
    $script:taskSeenAlerts=New-Object 'System.Collections.Generic.HashSet[string]'
    $script:taskAlerts=@();$script:taskEventPath=Join-Path $taskRoot 'locked-events.jsonl'
    $script:taskOffset=99;$script:taskCarry='partial';$script:taskPendingAlertWrite=$false
    $taskLock=[IO.File]::Open($script:taskEventPath,[IO.FileMode]::Create,[IO.FileAccess]::ReadWrite,[IO.FileShare]::Read)
    try{
        $null=Add-AICFSoakAlert 'NATIVE_CRASH' 'synthetic pending crash'
        $taskText='Game destroyed.'
        if(!$taskPendingAlertWrite -or $taskOffset -ne 0 -or $taskCarry -ne '' -or $taskSeenAlerts.Count -ne 0 -or (& $taskStopCondition)){throw 'Locked final alert was dropped or falsely allowed successful stop'}
        $taskControls++
    }finally{$taskLock.Dispose()}
    $script:taskPendingAlertWrite=$false
    $null=Add-AICFSoakAlert 'NATIVE_CRASH' 'synthetic pending crash'
    if($taskPendingAlertWrite -or $taskSeenAlerts.Count -ne 1 -or @((Get-Content -LiteralPath $taskEventPath)).Count -ne 1 -or !(& $taskStopCondition)){throw 'Pending alert did not recover before normal stop'}
    $taskControls++
    "PASS Vehicle soak source integrity and watcher restart: controls=$taskControls"
}finally{
    $taskResolved=[IO.Path]::GetFullPath($taskRoot)
    $taskAllowed=[IO.Path]::GetFullPath((Join-Path $RepositoryRoot '.codex-runtime')).TrimEnd([IO.Path]::DirectorySeparatorChar)+[IO.Path]::DirectorySeparatorChar
    if(!$taskResolved.StartsWith($taskAllowed,[StringComparison]::OrdinalIgnoreCase)){throw 'Unsafe contract cleanup path'}
    Remove-Item -LiteralPath $taskResolved -Recurse -Force
}
