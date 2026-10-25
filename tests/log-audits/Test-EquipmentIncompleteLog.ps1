param(
    [Parameter(Mandatory=$true)][string]$LogPath,
    [Parameter(Mandatory=$true)][string]$LauncherPath,
    [Parameter(Mandatory=$true)][string]$NativeExitPath
)
$ErrorActionPreference='Stop'
if((Get-Content -LiteralPath $NativeExitPath -Raw).Trim() -ne '0'){throw 'Native exit not zero'}
$log=Get-Content -LiteralPath $LogPath -Raw
if($log -notmatch 'Game destroyed\.' -or $log -notmatch '\[ROSTER_READY\]'){throw 'Incomplete stopped log or roster not ready'}
foreach($file in Get-ChildItem -LiteralPath (Split-Path -Parent $LogPath) -File -Filter '*.log'){
    if((Get-Content -LiteralPath $file.FullName -Raw) -match 'SCRIPT\s+\([EF]\):|ENGINE\s+\(F\):|NULL pointer|Virtual Machine Exception|EQUIPMENT_RECOVERY_TIMEOUT|READBACK_NOT_OWNED|ORIGINAL_RETAINED_AT_FEET'){throw "Script, fatal or recovery failure: $($file.Name)"}
}
$launcher=Get-Content -LiteralPath $LauncherPath -Raw
$manifest=([regex]::Match($launcher,'AICF_RUNTIME_MANIFEST_JSON=([^\r\n]+)').Groups[1].Value)|ConvertFrom-Json
$args=@($manifest.arguments)
$probeIndex=[array]::IndexOf($args,'-aicfEquipmentProbe')
if($manifest.role -ne 'Server' -or $probeIndex -lt 0 -or $args[$probeIndex+1] -cne 'incomplete'){throw 'Wrong native probe launch'}
$cli=[regex]::Match($log,'CLI Params:([^\r\n]+)').Groups[1].Value.Trim().Replace('"','').Replace('/','\')
if($cli -cne ($args -join ' ').Trim().Replace('"','').Replace('/','\')){throw 'Manifest/CLI mismatch'}
$required=@('STOP')
foreach($side in @('US','USSR')){foreach($kind in 0..1){foreach($rule in @('VALIDATE','PLAN','READBACK','NO_AMMO_NEED','BAD_READBACK')){$required+="${side}_${kind}_$rule"}}}
foreach($kind in 0..2){foreach($rule in @('ASSIGN','SERVICE_PLAN','APPLIED','INVENTORY','WEAPON','UNLOCKED','NO_LOOP','LEDGER')){$required+="LIVE_${kind}_$rule"}}
$checks=@([regex]::Matches($log,'\[INCOMPLETE_CHECK\] rule=(\S+) passed=([01])'))
if($checks.Count -ne $required.Count){throw "Case count $($checks.Count), expected $($required.Count)"}
foreach($rule in $required){if(@($checks|Where-Object {$_.Groups[1].Value -ceq $rule -and $_.Groups[2].Value -eq '1'}).Count -ne 1){throw "Missing, failed or duplicate: $rule"}}
if(@([regex]::Matches($log,'\[INCOMPLETE_DONE\] checks=45 failures=0 cases=3')).Count -ne 1){throw 'Completion missing or failed'}
Write-Output 'PASS equipment_incomplete cases=45 live_cases=3 native_exit=0 full_stopped_log=True all_native_errors=SEPARATE_GATE'
exit 0
