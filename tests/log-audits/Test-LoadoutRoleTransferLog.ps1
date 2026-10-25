param(
    [Parameter(Mandatory=$true)][string]$LogPath,
    [Parameter(Mandatory=$true)][string]$LauncherPath,
    [Parameter(Mandatory=$true)][string]$NativeExitPath,
    [switch]$Client
)
$ErrorActionPreference='Stop'
$base=@{LogPath=$LogPath;LauncherPath=$LauncherPath;NativeExitPath=$NativeExitPath;Client=$Client}
& (Join-Path $PSScriptRoot 'Test-LoadoutLibraryPreviewLog.ps1') @base
if($LASTEXITCODE -ne 0){exit 1}
$log=Get-Content -LiteralPath $LogPath -Raw
$launcher=Get-Content -LiteralPath $LauncherPath -Raw
$manifest=[regex]::Match($launcher,'AICF_RUNTIME_MANIFEST_JSON=([^\r\n]+)').Groups[1].Value|ConvertFrom-Json
if($manifest.arguments -notcontains '-aicfRoleTransferProbe'){throw 'Role transfer probe not enabled'}
$required=@()
$checks=@([regex]::Matches($log,'\[QOL_CHECK\] rule=(ROLETRANSFER_\S+) passed=([01])'))
if($Client){
    $required+=@(0..14|ForEach-Object {"ROLETRANSFER_RPC_$_"})
    $required+=@(0..5|ForEach-Object {"ROLETRANSFER_EDITOR_$_"})
    $required+=@('ROLETRANSFER_VISIBLE','ROLETRANSFER_LOAD','ROLETRANSFER_UNCHANGED_10','ROLETRANSFER_UNCHANGED_11','ROLETRANSFER_EDITOR_BUILD','ROLETRANSFER_RETRY','ROLETRANSFER_CAMERA','ROLETRANSFER_THIN')
    if(@([regex]::Matches($log,'\[ROLETRANSFER_DONE\] failures=0 steps=15 phases=6')).Count -ne 1){throw 'Role transfer completion missing or failed'}
    foreach($entry in @{10='REVISION_CONFLICT';11='INVALID_RECIPE';13='INVALID_REQUEST'}.GetEnumerator()){
        if($log -notmatch ('\[ROLETRANSFER_RPC\] step='+$entry.Key+' accepted=0 reason='+$entry.Value)){throw "Negative result missing: $($entry.Key)"}
    }
}
else{
    foreach($side in @('US','USSR')){
        foreach($rule in @('SOURCE','LIBRARY','LABELS')){$required+="ROLETRANSFER_${side}_$rule"}
        $required+=@(0..9|ForEach-Object {"ROLETRANSFER_${side}_FITS_$_"})
    }
    $required+=@(0..9|ForEach-Object {"ROLETRANSFER_MEMBER_$_"})
    $required+='ROLETRANSFER_BAD_LAST_ATOMIC'
    # Runtime faction keys differ between content profiles; both sides must
    # independently construct a recipe with a hole in the cargo slot IDs.
    $sparse=@($checks|Where-Object {$_.Groups[1].Value -match '^ROLETRANSFER_\S+_SPARSE$'})
    $files=@($checks|Where-Object {$_.Groups[1].Value -match '^ROLETRANSFER_FILES_\d+$'})
    if($sparse.Count -ne 2 -or $files.Count -ne 5){throw 'Sparse source or disk isolation case count mismatch'}
    $required+=@($sparse+$files|ForEach-Object {$_.Groups[1].Value})
}
if($checks.Count -ne $required.Count){throw "Role transfer case count $($checks.Count), expected $($required.Count)"}
if(@($required|Sort-Object -Unique).Count -ne $required.Count){throw 'Duplicate required role transfer case'}
foreach($rule in $required){
    if(@($checks|Where-Object {$_.Groups[1].Value -ceq $rule -and $_.Groups[2].Value -eq '1'}).Count -ne 1){throw "Missing, failed or duplicate: $rule"}
}
Write-Output "PASS role_transfer client=$Client cases=$($required.Count) full_stopped_log=True physical_input=NOT_RUN"
exit 0
