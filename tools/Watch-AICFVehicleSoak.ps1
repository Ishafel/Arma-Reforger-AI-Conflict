[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$EvidenceRoot,
    [ValidateRange(1,60)][int]$PollSeconds=5,
    [ValidateRange(60,10800)][int]$MaximumSeconds=2400
)
$ErrorActionPreference='Stop'
$taskRoot=$ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($EvidenceRoot)
$taskStarted=[DateTimeOffset]::UtcNow
$taskOffset=0
$taskCarry=''
$taskAlerts=@()
$taskSeenAlerts=New-Object 'System.Collections.Generic.HashSet[string]'
$taskEventPath=Join-Path $taskRoot 'analysis-events.jsonl'
if(Test-Path -LiteralPath $taskEventPath){
    foreach($taskExistingLine in Get-Content -LiteralPath $taskEventPath){
        if(!$taskExistingLine.Trim()){continue}
        $taskExisting=$taskExistingLine|ConvertFrom-Json
        if(!$taskExisting.line -or !$taskExisting.severity){throw 'Invalid existing watcher event'}
        if($taskSeenAlerts.Add([string]$taskExisting.line)){$taskAlerts+=$taskExisting}
    }
}
$taskLastHeartbeat=''
$taskLastPrinted=[DateTimeOffset]::MinValue
$taskLogPath=''
$taskPendingAlertWrite=$false
function Save-AICFSoakLiveState([string]$Value,[string]$Path){
    for($writeAttempt=0;$writeAttempt -lt 5;$writeAttempt++){
        try{$Value|Set-Content -LiteralPath $Path;return}
        catch [IO.IOException]{
            if($writeAttempt -eq 4){Write-Warning 'Live state temporarily locked; full log observation continues';return}
            Start-Sleep -Milliseconds 100
        }
    }
}
function Add-AICFSoakAlert([string]$Severity,[string]$Line){
    if(!$script:taskSeenAlerts.Add($Line)){return}
    $event=[pscustomobject]@{observed_utc=[DateTimeOffset]::UtcNow.ToString('o');severity=$Severity;line=$Line}
    for($writeAttempt=0;$writeAttempt -lt 5;$writeAttempt++){
        try{
            $event|ConvertTo-Json -Compress|Add-Content -LiteralPath $script:taskEventPath
            $script:taskAlerts+=$event
            "SOAK_ALERT severity=$Severity $Line"
            return
        }catch [IO.IOException]{
            if($writeAttempt -eq 4){
                [void]$script:taskSeenAlerts.Remove($Line)
                $script:taskOffset=0;$script:taskCarry=''
                $script:taskPendingAlertWrite=$true
                Write-Warning 'Alert index temporarily locked; full log will be scanned again'
                return
            }
            Start-Sleep -Milliseconds 100
        }
    }
}
while(([DateTimeOffset]::UtcNow-$taskStarted).TotalSeconds -lt $MaximumSeconds){
    $taskPendingAlertWrite=$false
    if(!$taskLogPath){
        $taskLog=Get-ChildItem -LiteralPath (Join-Path $taskRoot 'profile/logs') -Recurse -Filter console.log -ErrorAction SilentlyContinue|Select-Object -First 1
        if($taskLog){$taskLogPath=$taskLog.FullName}
    }
    if($taskLogPath){
        # Native assert может писать только crash.log и остановить console.
        $taskCrashPath=Join-Path (Split-Path -Parent $taskLogPath) 'crash.log'
        if(Test-Path -LiteralPath $taskCrashPath){
            $taskCrash=[string](Get-Content -LiteralPath $taskCrashPath -Raw)
            $taskCrashFailure=[regex]::Match($taskCrash,'Assertion failed|Virtual Machine Exception|Access violation',[Text.RegularExpressions.RegexOptions]::IgnoreCase)
            if($taskCrashFailure.Success){
                $taskReason=[regex]::Match($taskCrash,'(?m)^Reason:\s*([^\r\n]+)').Groups[1].Value
                if(!$taskReason){$taskReason=$taskCrashFailure.Value}
                Add-AICFSoakAlert 'NATIVE_CRASH' ("file=$taskCrashPath reason=$taskReason")
            }
        }
        $taskText=[string](Get-Content -LiteralPath $taskLogPath -Raw)
        # Уникальный profile не перезаписывает console.log. Краткое чтение во
        # время native flush не должно повторно выпускать прежние события.
        if($taskText.Length -lt $taskOffset){Start-Sleep -Seconds $PollSeconds;continue}
        $taskNew=$taskCarry+$taskText.Substring($taskOffset)
        $taskOffset=$taskText.Length
        $taskLines=$taskNew -split "`n"
        $taskCarry=$taskLines[-1]
        for($taskIndex=0;$taskIndex -lt $taskLines.Count-1;$taskIndex++){
            $taskLine=$taskLines[$taskIndex].TrimEnd("`r")
            if($taskLine -match '\[VEHICLE_(?:SOAK|MULTI)_HEARTBEAT\]'){$taskLastHeartbeat=$taskLine}
            $taskSeverity=''
            if($taskLine -match 'SCRIPT\s+\([EF]\)|ENGINE\s+\(F\)|Virtual Machine Exception|Access violation|Unknown class .SCR_AIGoalReaction_GetOutVehicle'){$taskSeverity='FATAL'}
            elseif($taskLine -match '\[VEHICLE_(?:SOAK|MULTI)_FAILURE\]|\[ACCEPTANCE_FAILURE_LATCHED\]'){$taskSeverity='FAILURE'}
            elseif($taskLine -match '\[NATIVE_GROUP_MOVE_FAILED\]|\[TRANSPORT_NATIVE_GATE_REJECTED\]'){$taskSeverity='NATIVE_DECISION'}
            if($taskSeverity){Add-AICFSoakAlert $taskSeverity $taskLine}
        }
        $taskLiveJson=[pscustomobject]@{observed_utc=[DateTimeOffset]::UtcNow.ToString('o');log=$taskLogPath;characters_read=$taskOffset;alerts=$taskAlerts.Count;latest_heartbeat=$taskLastHeartbeat;stopped=$taskText.Contains('Game destroyed.');pending_alert_write=$taskPendingAlertWrite}|ConvertTo-Json -Depth 3
        Save-AICFSoakLiveState $taskLiveJson (Join-Path $taskRoot 'analysis-live.json')
        if(([DateTimeOffset]::UtcNow-$taskLastPrinted).TotalSeconds -ge 60){
            $taskLastPrinted=[DateTimeOffset]::UtcNow
            "SOAK_WATCH alerts=$($taskAlerts.Count) $taskLastHeartbeat"
        }
        if($taskText.Contains('Game destroyed.') -and !$taskPendingAlertWrite){'SOAK_WATCH_STOPPED_LOG';exit 0}
    }
    Start-Sleep -Seconds $PollSeconds
}
'SOAK_WATCH_LIMIT_REACHED_NO_PROCESS_MUTATION'
exit 2
