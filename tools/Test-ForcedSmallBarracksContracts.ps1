$ErrorActionPreference='Stop'
$repoRoot=Split-Path -Parent $PSScriptRoot
$output=Join-Path $repoRoot ('.codex-runtime/forced-small-contracts-'+[guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $output -Force | Out-Null
$lines=@('[AICF][FORCED_SMALL_CONTRACTS] passed=7 total=7','[AICF][ROSTER_READY]')
foreach($side in @('US','USSR')) {
 $identity="token=$side faction=$side base=$side provider=$side type=SMALL_BARRACKS"
 $lines+="[AICF][CONSTRUCTION_DECISION] t_ms=1000 $identity duration_ms=0"
 $lines+="[AICF][CONSTRUCTION_FORCED_SMALL_STARTED] t_ms=121000 $identity"
 $lines+="[AICF][CONSTRUCTION_PAYMENT] t_ms=122000 $identity debit_count=1 reservation_released=1"
 $lines+="[AICF][CONSTRUCTION_PLACED] t_ms=122001 $identity layout=layout-$side"
 $lines+="[AICF][CONSTRUCTION_FORCED_SMALL_COMPLETION] t_ms=123000 $identity layout=layout-$side"
 $lines+="[AICF][CONSTRUCTION_COMPLETED] t_ms=124000 $identity layout=layout-$side service_online=1"
 $lines+="[AICF][INFANTRY_RECRUIT_JOINED] faction=$side slot=0"
}
$lines+='Game destroyed.'
$valid=$lines -join "`n"
$cases=[ordered]@{
 valid=$valid
 early=$valid.Replace('t_ms=121000','t_ms=120999')
 wrong_type=$valid.Replace('type=SMALL_BARRACKS','type=LARGE_BARRACKS')
 no_payment=($valid -replace '(?m)^.*\[CONSTRUCTION_PAYMENT\].*\n','')
 duplicate_payment=($valid -replace '(?m)^(.*\[CONSTRUCTION_PAYMENT\].*)$',('$1' + "`n" + '$1'))
 changed_provider=$valid.Replace('t_ms=124000 token=US faction=US base=US provider=US','t_ms=124000 token=US faction=US base=US provider=STALE')
 no_service=$valid.Replace('service_online=1','service_online=0')
 wrong_layout=$valid.Replace('t_ms=124000 token=US faction=US base=US provider=US type=SMALL_BARRACKS layout=layout-US','t_ms=124000 token=US faction=US base=US provider=US type=SMALL_BARRACKS layout=other')
 no_request=($valid -replace '(?m)^.*\[CONSTRUCTION_FORCED_SMALL_COMPLETION\].*\n','')
 no_recruitment=($valid -replace '(?m)^.*\[INFANTRY_RECRUIT_JOINED\].*\n','')
 no_shutdown=$valid.Replace('Game destroyed.','')
}
foreach($name in $cases.Keys) {
 $path=Join-Path $output "$name.log"
 $cases[$name] | Set-Content -LiteralPath $path
 & powershell.exe -NoProfile -ExecutionPolicy Bypass -File "$PSScriptRoot/Test-ForcedSmallBarracksLog.ps1" -LogPath $path -RequireProbe -RequireRecruitment *> (Join-Path $output "$name.txt")
 $expected=1; if($name -eq 'valid') {$expected=0}
 if($LASTEXITCODE -ne $expected) {throw "Unexpected verdict $name exit=$LASTEXITCODE expected=$expected; $output"}
 "PASS $name"
}
"Forced small analyzer contracts: PASS ($($cases.Count) cases); evidence=$output"
