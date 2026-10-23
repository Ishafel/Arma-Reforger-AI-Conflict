param([Parameter(Mandatory)][string]$LogPath,[Parameter(Mandatory)][string]$ManifestPath,[Parameter(Mandatory)][string]$NativeExitPath)
$ErrorActionPreference='Stop'
$log=[IO.File]::ReadAllText($LogPath)
$manifest=[IO.File]::ReadAllText($ManifestPath) | ConvertFrom-Json
$failures=@()
if((Get-Content -LiteralPath $NativeExitPath -Raw).Trim() -ne '0'){$failures+='NATIVE_EXIT'}
if($log -notmatch 'Game destroyed\.'){$failures+='STOPPED_LOG'}
if($log -match 'SCRIPT\s*\([EF]\)|ENGINE\s*\(F\)|SCRIPT EXCEPTION|NULL pointer|VM exception|Unhandled exception'){$failures+='SCRIPT_VM_FATAL'}
$argv=@($manifest.arguments)
if(@($argv|Where-Object {$_ -ceq '-aicfCatalogProbe'}).Count -ne 1){$failures+='PROBE_CLI'}
$cli=@([regex]::Matches($log,'CLI Params: ([^\r\n]+)'))
if($cli.Count -ne 1){$failures+='CLI_COUNT'}
else {
    $fragments=@(
        ('-gproj '+$argv[[array]::IndexOf($argv,'-gproj')+1])
        ('-profile '+$manifest.profile)
        ('-addonsDir '+$manifest.addonsDir)
        '-aicfCatalogProbe'
    )
    foreach($fragment in $fragments){
        if(-not $cli[0].Groups[1].Value.Contains($fragment)){$failures+='CLI_MISMATCH'}
    }
}
$expected=@('EXAMPLE','VISIBLE_NAME','REPEAT','STOCK_PRESERVED')
for($mode=0;$mode -lt 3;$mode++){for($difficulty=0;$difficulty -lt 3;$difficulty++){$expected+="TARGET_${mode}_${difficulty}"}}
$expected+=@('FILTER_TYPE','FILTER_MANW')
$cases=@([regex]::Matches($log,'\[CATALOG_CASE\] case=(\w+) pass=([01])'))
if($cases.Count -ne $expected.Count){$failures+='CASE_COUNT'}
foreach($name in $expected){$found=@($cases|Where-Object {$_.Groups[1].Value -ceq $name});if($found.Count -ne 1 -or $found[0].Groups[2].Value -cne '1'){$failures+="CASE_$name"}}
$done=@([regex]::Matches($log,'\[CATALOG_DONE\] checks=(\d+) failed=(\d+)'))
if($done.Count -ne 1 -or $done[0].Groups[1].Value -ne "$($expected.Count)" -or $done[0].Groups[2].Value -ne '0'){$failures+='COMPLETION'}
if($failures.Count){$failures;exit 1}
"Scenario catalog full stopped log: PASS; cases=$($expected.Count)"
exit 0
