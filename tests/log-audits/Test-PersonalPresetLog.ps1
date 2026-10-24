param(
    [Parameter(Mandatory=$true)][string]$LogPath,
    [Parameter(Mandatory=$true)][string]$LauncherPath,
    [Parameter(Mandatory=$true)][string]$NativeExitPath,
    [switch]$Client,
    [switch]$Reload
)
$ErrorActionPreference='Stop'
if($Client -and $Reload){throw 'Reload is a server-only probe'}
$failures=[Collections.Generic.List[string]]::new()
$log=Get-Content -LiteralPath $LogPath -Raw
$launcher=Get-Content -LiteralPath $LauncherPath -Raw
$peer='Server'; if($Client){$peer='Client'}
if(-not $Reload){
    $base=@{LogPath=$LogPath;LauncherPath=$LauncherPath;NativeExitPath=$NativeExitPath;Files=$true;Client=$Client}
    & (Join-Path $PSScriptRoot 'Test-LoadoutQolLog.ps1') @base
    if($LASTEXITCODE -ne 0){exit 1}
}
if((Get-Content -LiteralPath $NativeExitPath -Raw).Trim() -ne '0'){$failures.Add('NATIVE_EXIT')}
if($log -notmatch 'Game destroyed\.'){ $failures.Add('INCOMPLETE_LOG') }
foreach($file in @(Get-ChildItem -LiteralPath (Split-Path -Parent $LogPath) -File -Filter '*.log')){
    if((Get-Content -LiteralPath $file.FullName -Raw) -match 'SCRIPT\s+\([EF]\):|ENGINE\s+\(F\):|NULL pointer|Virtual Machine Exception|Unhandled exception'){$failures.Add("SCRIPT_FATAL:$($file.Name)")}
}
$manifestMatch=[regex]::Match($launcher,'AICF_RUNTIME_MANIFEST_JSON=([^\r\n]+)')
if(!$manifestMatch.Success){$failures.Add('MANIFEST')}
else{
    $manifest=$manifestMatch.Groups[1].Value | ConvertFrom-Json
    if($manifest.role -ne $peer -or $manifest.arguments -notcontains '-aicfPersonalLibraryProbe'){$failures.Add('WRONG_LAUNCH')}
    if($Reload -and $manifest.arguments -notcontains '-aicfPersonalLibraryReload'){$failures.Add('RELOAD_LAUNCH')}
    $cli=[regex]::Match($log,'CLI Params:([^\r\n]+)').Groups[1].Value.Replace('"','').Replace('/','\')
    foreach($argument in $manifest.arguments){if($cli.IndexOf(([string]$argument).Replace('"','').Replace('/','\'),[StringComparison]::OrdinalIgnoreCase) -lt 0){$failures.Add("CLI_MISMATCH:$argument")}}
}
$required=@()
if($Reload){
    foreach($side in @('US','USSR')){foreach($rule in @('RESTART_MEMBER','RESTART_TOMBSTONE','RESTART_ACTIVE','RESTART_LIST')){$required+="PERSONAL_STORE_${side}_$rule"}}
    if(@([regex]::Matches($log,'\[PERSONAL_RELOAD_DONE\] failures=0')).Count -ne 1){$failures.Add('RELOAD_DONE')}
}
elseif(!$Client){
    foreach($side in @('US','USSR')){foreach($rule in @('LEGACY_ACTIVE','SCALAR_RECIPE_REJECTED','SAVE_A','DEDUPE','UPDATE_A','SAVE_B','PARTIAL_RECOVERY','DELETE_A','RELOAD_TOMBSTONE','SESSION_NO_DISK_READ','SESSION_SAVE','SESSION_ISOLATION','CONTEXT_SCOPE','OLD_ACTIVE_BYTES','AI_FILES_UNCHANGED')){$required+="PERSONAL_STORE_${side}_$rule"}}
}
else{
    $required+=@(0..15 | ForEach-Object {"PERSONAL_RPC_STEP_$_"})
    $required+=@('PERSONAL_RPC_BASE','PERSONAL_RPC_LOAD_DRAFT','PERSONAL_RPC_DEFAULT','PERSONAL_RPC_SELECT')
    foreach($step in @((2..9)+@(13,14))){$required+="PERSONAL_RPC_ACTIVE_REVISION_$step"; if($step -ne 4){$required+="PERSONAL_RPC_ACTIVE_DATA_$step"}}
    foreach($suffix in @('HIDDEN_BUILD','HIDDEN_TREE','WIDTH_70_PERCENT','EDIT_INPUT','SIDE_HIDDEN','GROUP_HIDDEN','COST_VISIBLE','SINGLE_RECIPIENT','CATALOG_EXPANDED','HIDDEN_CLEANUP')){$required+="PERSONAL_INSPECTOR_$suffix"}
    foreach($section in 0..3){$required+="PERSONAL_INSPECTOR_SECTION_LAYER_$section";$required+="PERSONAL_INSPECTOR_SECTION_INPUT_$section"}
    foreach($part in @('SLOTS','ITEMS')){foreach($rule in @('ROW','NATIVE','SYNC','LAST','REVEAL','DISABLED','EMPTY')){$required+="PERSONAL_INSPECTOR_SCROLL_${rule}_$part"}}
    if(@([regex]::Matches($log,'\[PERSONAL_RPC_DONE\] failures=0 steps=16')).Count -ne 1){$failures.Add('PERSONAL_RPC_DONE')}
    $negative=@{6='REVISION_CONFLICT';7='TEMPLATE_UNAVAILABLE';8='INVALID_RECIPE';15='REVISION_CONFLICT'}
    foreach($step in $negative.Keys){if($log -notmatch ('\[PERSONAL_RPC\] step='+$step+' accepted=0 reason='+$negative[$step])){$failures.Add("NEGATIVE_$step")}}
}
$checks=@([regex]::Matches($log,'\[QOL_CHECK\] rule=(PERSONAL_\w+) passed=([01])'))
if($checks.Count -ne $required.Count){$failures.Add('CASE_COUNT')}
foreach($rule in $required){if(@($checks | Where-Object {$_.Groups[1].Value -ceq $rule -and $_.Groups[2].Value -eq '1'}).Count -ne 1){$failures.Add("CASE:$rule")}}
if($failures.Count){$failures | ForEach-Object {"FAIL $_"};exit 1}
Write-Output "PASS personal_presets peer=$peer reload=$Reload cases=$($required.Count) full_stopped_log=True native_exit=0 physical_input=NOT_RUN"
exit 0
