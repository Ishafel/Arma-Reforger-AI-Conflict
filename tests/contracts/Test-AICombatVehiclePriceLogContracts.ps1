param([string]$RepositoryRoot=(Split-Path -Parent (Split-Path -Parent $PSScriptRoot)))
$ErrorActionPreference='Stop'
$auditor=Join-Path $RepositoryRoot 'tests/log-audits/Test-AICombatVehiclePriceLog.ps1'
$root=Join-Path $RepositoryRoot ('.codex-runtime/ai-price-parser-'+[guid]::NewGuid().ToString('N'))
[void][IO.Directory]::CreateDirectory($root)
$lines=@()
foreach($side in @('US','USSR')){foreach($role in 0..4){
 $raw=200;$ai=200;$armed=0;$armor=0;$apc=0
 if($role -eq 2){$raw=400;$ai=133;$armed=1}
 if($role -eq 3){$raw=1500;$ai=500;$armed=1;$apc=1}
 if($role -eq 4){$raw=3500;$ai=1167;$armed=1;$armor=1}
 $lines+="[AICF][AI_PRICE_CASE] faction=$side role=$role passed=1 raw=$raw ai=$ai armed=$armed armor=$armor apc=$apc unarmed=0 human=$raw prefab=$side-$role"
}}
$positive=($lines -join "`n")+@'

[AICF][AI_PRICE_DONE] checks=10 failures=0 profile=WCS_RHS_ARLAND
[AICF][AI_PRICE_DEBIT] operation=op1 raw=3500 quote=1167 paid=1167 before=2000 after=833 human_before=3500 human_after=3500 passed=1 prefab=USSR-4
[AICF][VEHICLE_PRESET_PURCHASE] operation=op1 prefab=USSR-4 supplies=1167 base=1
[AICF][VEHICLE_SPAWNED] lease_generation=1 vehicle_generation=1 vehicle_lifecycle_id=asset1 vehicle=0x123 prefab=USSR-4 operation_id=op1 reason=SPAWN_SUCCESS
[AICF][AI_PRICE_PURCHASE_DONE] production_debit_observed=1
Game destroyed.
'@
$cases=@(
 @{name='positive';text=$positive;expected=0},
 @{name='wrong_ai';text=$positive.Replace('raw=400 ai=133','raw=400 ai=134');expected=1},
 @{name='changed_human';text=$positive.Replace('human=400','human=133');expected=1},
 @{name='nonzero_exit';text=$positive;native=1;expected=1},
 @{name='missing_stop';text=$positive.Replace('Game destroyed.','');expected=1},
 @{name='missing_matrix';text=$positive.Replace('checks=10 failures=0','checks=9 failures=0');expected=1},
 @{name='missing_debit';text=($positive -replace '(?m)^.*\[AI_PRICE_DEBIT\].*$','');expected=1},
 @{name='wrong_paid';text=$positive.Replace('paid=1167 before','paid=3500 before');expected=1},
 @{name='foreign_lease';text=$positive.Replace('operation_id=op1','operation_id=op2');expected=1},
 @{name='foreign_catalog_case';text=$positive.Replace('human=3500 prefab=USSR-4','human=3500 prefab=FOREIGN');expected=1},
 @{name='missing_bound_lease';text=($positive -replace '(?m)^.*\[VEHICLE_SPAWNED\].*$','');expected=1},
 @{name='sibling_oom';text=$positive;sibling='MEMORY (E): Out of memory';expected=1},
 @{name='sibling_resource';text=$positive;sibling='RESOURCES (E): Missing';expected=1}
)
foreach($case in $cases){
 $dir=Join-Path $root $case.name;[void][IO.Directory]::CreateDirectory($dir)
 $log=Join-Path $dir 'console.log';$launch=Join-Path $dir 'launcher.txt';$native=Join-Path $dir 'native-exit.txt'
 [IO.File]::WriteAllText($log,$case.text)
 [IO.File]::WriteAllText($launch,'AICF_RUNTIME_MANIFEST_JSON={"arguments":["-aicfAIPriceProbe","1"]}')
 $code=0;if($case.ContainsKey('native')){$code=$case.native};[IO.File]::WriteAllText($native,[string]$code)
 if($case.ContainsKey('sibling')){[IO.File]::WriteAllText((Join-Path $dir 'error.log'),$case.sibling)}
 & powershell.exe -NoProfile -ExecutionPolicy Bypass -File $auditor -LogPath $log -LauncherPath $launch -NativeExitPath $native -RequirePurchase *> (Join-Path $dir 'audit.txt')
 if($LASTEXITCODE -ne $case.expected){throw "AI price parser $($case.name): exit=$LASTEXITCODE expected=$($case.expected) evidence=$dir"}
}
"PASS AI price log parser controls=$($cases.Count); synthetic parser validation does not replace native proof; evidence=$root"
