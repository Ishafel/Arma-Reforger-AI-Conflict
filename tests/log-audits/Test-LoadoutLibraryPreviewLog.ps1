param(
    [Parameter(Mandatory=$true)][string]$LogPath,
    [Parameter(Mandatory=$true)][string]$LauncherPath,
    [Parameter(Mandatory=$true)][string]$NativeExitPath,
    [switch]$Client
)
$ErrorActionPreference='Stop'
$base=@{LogPath=$LogPath;LauncherPath=$LauncherPath;NativeExitPath=$NativeExitPath;Client=$Client;LibraryPreview=$true}
& (Join-Path $PSScriptRoot 'Test-PersonalPresetLog.ps1') @base
if($LASTEXITCODE -ne 0){exit 1}
$log=Get-Content -LiteralPath $LogPath -Raw
$required=@()
if($Client){
    $required+=@(0..30|ForEach-Object {"LIBRARY_RPC_$_"})
    $inspect=@(2,6)+(9..19)+@(22,26,28,29)
    foreach($step in $inspect){
        $required+="LIBRARY_READ_ONLY_$step"
        if($step -notin @(19,28,29)){$required+="LIBRARY_STATUS_$step"}
    }
    foreach($part in @('AI','PERSONAL')){
        foreach($rule in @('BUILD','WORLD','SUMMARY','SCROLL','CLEAR','DRAFT_UNCHANGED','FEEDBACK_COLORS','DISABLED')){
            $required+="LIBRARY_WIDGET_${part}_$rule"
        }
    }
    if($log -notmatch '\[LIBRARY_RPC_DONE\] failures=0 steps=31'){throw 'Library completion missing or failed'}
    foreach($entry in @{19='LIBRARY_CONFLICT';28='REVISION_CONFLICT';29='TEMPLATE_UNAVAILABLE'}.GetEnumerator()){
        if($log -notmatch ('\[LIBRARY_RPC\] step='+$entry.Key+' accepted=0 reason='+$entry.Value)){throw "Negative result missing: $($entry.Key)"}
    }
}
else{
    foreach($side in @('US','USSR')){
        foreach($kind in @('UNARMED','NO_MAGAZINES')){
            foreach($rule in @('EMPTY','AI_VALIDATE','PERSONAL_VALIDATE')){$required+="LIBRARY_${side}_${kind}_$rule"}
        }
    }
}
$checks=@([regex]::Matches($log,'\[QOL_CHECK\] rule=(LIBRARY_\S+) passed=([01])'))
if($checks.Count -ne $required.Count){throw "Library case count $($checks.Count), expected $($required.Count)"}
foreach($rule in $required){
    if(@($checks|Where-Object {$_.Groups[1].Value -ceq $rule -and $_.Groups[2].Value -eq '1'}).Count -ne 1){throw "Missing, failed or duplicate: $rule"}
}
Write-Output "PASS library_preview client=$Client cases=$($required.Count) full_stopped_log=True physical_input=NOT_RUN"
exit 0
