param([string]$RepositoryRoot=(Split-Path -Parent (Split-Path -Parent $PSScriptRoot)))
$ErrorActionPreference='Stop'
$auditor=Join-Path $RepositoryRoot 'tests/log-audits/Test-SupplyDepotConstructionLog.ps1'
$evidence=Join-Path $RepositoryRoot ('.codex-runtime/supply-construction-parser-'+[guid]::NewGuid().ToString('N'))
[void][IO.Directory]::CreateDirectory($evidence)
$positive=@'
[AICF][ROSTER_READY]
[AICF][SUPPLY_CONSTRUCTION_PROBE] event=METADATA faction=US valid=1 prefab=Supply.et reason=NONE
[AICF][SUPPLY_CONSTRUCTION_PROBE] event=START faction=US capacity=1000 quote=3750 human_veto=1 priority_controlled=1 economy_seed=0 capacity_write=0
[AICF][CONSTRUCTION_DECISION] t_ms=1000 token=c1 faction=US base=b1 type=SUPPLY_DEPOT cost=175 layout=NONE
[AICF][CONSTRUCTION_SITE_SELECTED] t_ms=2000 token=c1 faction=US base=b1 type=SUPPLY_DEPOT layout=NONE
[AICF][CONSTRUCTION_RESERVED] t_ms=3000 token=c1 faction=US base=b1 type=SUPPLY_DEPOT supply_debited=0 layout=NONE
[AICF][CONSTRUCTION_PAYMENT] t_ms=4000 token=c1 faction=US base=b1 type=SUPPLY_DEPOT cost=175 supplies_before=1000 supplies_after=825 reserve=500 debit_count=1 tickets=0 reservation_released=1 layout=L1
[AICF][CONSTRUCTION_PLACED] t_ms=4001 token=c1 faction=US base=b1 type=SUPPLY_DEPOT layout=L1 service_online=0
[AICF][SUPPLY_CONSTRUCTION_PROBE] event=PLACED faction=US accepted=1 paid=1 cost=175 before=1000 after=825 token=c1
[AICF][BUILDER_PROGRESS] target=L1 tool_active=1 item_using=1
[AICF][BUILDER_COMPLETED] target=L1 tool_active=1 item_using=1
[AICF][CONSTRUCTION_COMPLETED] t_ms=5000 token=c1 faction=US base=b1 type=SUPPLY_DEPOT layout=L1 service_online=1
[AICF][SUPPLY_CONSTRUCTION_PROBE] event=DONE passed=1 initial_capacity=1000 capacity=4000 quote=3750 online=1 human_veto=1
Game destroyed.
'@
$cases=@(
 @{name='positive';text=$positive;expected=0},
 @{name='missing_exit';text=$positive;expected=1;missingExit=$true},
 @{name='nonzero_exit';text=$positive;expected=1;native=1},
 @{name='missing_shutdown';text=$positive.Replace('Game destroyed.','');expected=1},
 @{name='foreign_probe_token';text=$positive.Replace('after=825 token=c1','after=825 token=c2');expected=1},
 @{name='foreign_completion_type';text=$positive.Replace('type=SUPPLY_DEPOT layout=L1 service_online=1','type=ARMORY layout=L1 service_online=1');expected=1},
 @{name='missing_payment';text=($positive -replace '(?m)^.*\[CONSTRUCTION_PAYMENT\].*$','');expected=1},
 @{name='capacity_not_grown';text=$positive.Replace('capacity=4000','capacity=1000');expected=1},
 @{name='capacity_under_quote';text=$positive.Replace('capacity=4000','capacity=3000');expected=1},
 @{name='human_veto_missing';text=$positive.Replace('human_veto=1','human_veto=0');expected=1},
 @{name='no_native_metadata';text=$positive.Replace('valid=1','valid=0');expected=1},
 @{name='sibling_oom';text=$positive;expected=1;sibling='MEMORY (E): Out of memory'},
 @{name='sibling_resource';text=$positive;expected=1;sibling='RESOURCES (E): Resource missing'}
)
foreach($case in $cases){
 $root=Join-Path $evidence $case.name;[void][IO.Directory]::CreateDirectory($root)
 $log=Join-Path $root 'console.log';$launch=Join-Path $root 'launcher.txt';$nativePath=Join-Path $root 'native-exit.txt'
 [IO.File]::WriteAllText($log,$case.text)
 [IO.File]::WriteAllText($launch,'AICF_RUNTIME_MANIFEST_JSON={"arguments":["-aicfSupplyDepotConstructionProbe","1"]}')
 if(!$case.missingExit){$native=0;if($case.ContainsKey('native')){$native=$case.native};[IO.File]::WriteAllText($nativePath,[string]$native)}
 if($case.ContainsKey('sibling')){[IO.File]::WriteAllText((Join-Path $root 'error.log'),$case.sibling)}
 & powershell.exe -NoProfile -ExecutionPolicy Bypass -File $auditor -LogPath $log -LauncherPath $launch -NativeExitPath $nativePath *> (Join-Path $root 'audit.txt')
 if($LASTEXITCODE -ne $case.expected){throw "Supply parser $($case.name) exit=$LASTEXITCODE expected=$($case.expected), evidence=$root"}
}
"PASS Supply construction parser: $($cases.Count) synthetic controls. This is parser validation, not native construction proof. Evidence=$evidence"
