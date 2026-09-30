param([Parameter(Mandatory)][string]$Profile, [int]$DeadlineSeconds=650)
$ErrorActionPreference='Stop'
$profilePattern='(?i)(?:^|\s)-profile\s+(?:"'+[regex]::Escape($Profile.TrimEnd('\'))+'"|'+[regex]::Escape($Profile.TrimEnd('\'))+')(?=\s|$)'
$started=Get-Date
$seen=$false
while (((Get-Date)-$started).TotalSeconds -lt $DeadlineSeconds) {
    $processes=@(Get-CimInstance Win32_Process | Where-Object { $_.Name -eq 'ArmaReforgerServerDiag.exe' -and $_.CommandLine -match $profilePattern })
    if($processes.Count){$seen=$true}
    $logs=@(Get-ChildItem -LiteralPath $Profile -Recurse -Filter console.log -ErrorAction SilentlyContinue)
    $errorFound=$false
    foreach($log in $logs) {
        $stream=[IO.File]::Open($log.FullName,'Open','Read','ReadWrite')
        $reader=[IO.StreamReader]::new($stream)
        try{$raw=$reader.ReadToEnd()}finally{$reader.Dispose()}
        if($raw -match 'Virtual Machine Exception|Unhandled exception|NULL pointer|Failed to compile|SCRIPT\s+\(F\)'){$errorFound=$true}
    }
    if($errorFound) {
        foreach($proc in $processes) {
            $again=Get-CimInstance Win32_Process -Filter "ProcessId=$($proc.ProcessId)"
            if($again -and $again.Name -eq 'ArmaReforgerServerDiag.exe' -and $again.CommandLine -match $profilePattern) {
                "DIAGNOSTIC_STOP pid=$($proc.ProcessId) profile=$Profile time=$(Get-Date -Format o)" | Tee-Object -FilePath (Join-Path $Profile 'watchdog-stop.txt')
                Stop-Process -Id $proc.ProcessId -Force
            }
        }
        exit 2
    }
    if($seen -and !$processes.Count){'SERVER_EXIT_OBSERVED'; exit 0}
    Start-Sleep -Seconds 5
}
foreach($proc in @(Get-CimInstance Win32_Process | Where-Object { $_.Name -eq 'ArmaReforgerServerDiag.exe' -and $_.CommandLine -match $profilePattern })) {
    "DEADLINE_STOP pid=$($proc.ProcessId) profile=$Profile" | Tee-Object -FilePath (Join-Path $Profile 'watchdog-stop.txt')
    Stop-Process -Id $proc.ProcessId -Force
}
exit 3
