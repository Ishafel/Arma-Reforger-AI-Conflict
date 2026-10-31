param([string]$RepositoryRoot=(Split-Path -Parent (Split-Path -Parent $PSScriptRoot)))
$ErrorActionPreference='Stop'
$LogPath=$null;$OutputPath=$null
. (Join-Path $RepositoryRoot 'tests/log-audits/Test-AIQuotaStockPressureLog.ps1')
$golden=@'
[AICF][QUOTA_STOCK_PRESSURE][TARGET_HOLD_ONE] elapsed_ms=100 group=0x4000000000000001 generation=1 original_actor=0x4000000000000011
[AICF][QUOTA_STOCK_PRESSURE][FULL_STOCK_VETO] elapsed_ms=5000 stock_limit=128 stock_physical=128 stock_reserved=0 admitted=0 actual_agents=0 native_intended_before=148 native_intended_after=148
[AICF][QUOTA_STOCK_PRESSURE][MANAGED_FIVE_INTENT] elapsed_ms=5001 group=0x4000000000000001 generation=1
[AICF][QUOTA_STOCK_PRESSURE][RECRUIT_REQUEST] elapsed_ms=5002 accepted=1
'@
foreach($id in 18..21){
 $actor='0x{0:X16}'-f(0x4000000000000000L+$id)
 $golden+="`n[AICF][STAGE4][INFO][INFANTRY_RECRUIT_DEBITED] faction=US slot=0 group=0x4000000000000001 generation=1 token=1 cost=15 supplies_before=100 supplies_after=85"
 $golden+="`n[AICF][STAGE4][INFO][INFANTRY_RECRUIT_JOINED] faction=US slot=0 group=0x4000000000000001 generation=1 token=1 character=$actor"
}
foreach($id in 17..21){
 $actor='0x{0:X16}'-f(0x4000000000000000L+$id)
 $golden+="`n[AICF][QUOTA_STOCK_PRESSURE][MANAGED_MEMBER] elapsed_ms=40000 group=0x4000000000000001 generation=1 actor=$actor rpl=$id"
}
$golden+="`n[AICF][QUOTA_STOCK_PRESSURE][PROOF] elapsed_ms=40005 stock_physical=128 stock_limit=128 stock_reserved=0 actual_managed=5 extra_actual=0 veto_hold_ms=35005 group=0x4000000000000001 generation=1"
$golden+="`n[AICF][QUOTA_STOCK_PRESSURE][DONE] elapsed_ms=40006 passed=1 reason=STOCK_FULL_MANAGED_RECRUIT_FIVE"
if(@(Test-StockPressureText $golden).Count){throw 'Actual auditor positive failed'}
$cases=1
foreach($mutation in @(
 @('missing_done','[DONE]','[UNKNOWN]'),@('failed','passed=1','passed=0'),
 @('underfull','stock_physical=128','stock_physical=127'),@('reserved','stock_reserved=0','stock_reserved=1'),
 @('extra_admitted','admitted=0','admitted=1'),@('extra_body','actual_agents=0','actual_agents=1'),
 @('native_pending','native_intended_after=148','native_intended_after=149'),
 @('short_window','veto_hold_ms=35005','veto_hold_ms=34999'),@('invented_window','elapsed_ms=40005','elapsed_ms=6000'),
 @('wrong_group','[PROOF] elapsed_ms=40005 stock_physical=128 stock_limit=128 stock_reserved=0 actual_managed=5 extra_actual=0 veto_hold_ms=35005 group=0x4000000000000001','[PROOF] elapsed_ms=40005 stock_physical=128 stock_limit=128 stock_reserved=0 actual_managed=5 extra_actual=0 veto_hold_ms=35005 group=0x4000000000000002'),
 @('no_accept','accepted=1','accepted=0'),@('unpaid','supplies_after=85','supplies_after=100'),
 @('no_join','[INFANTRY_RECRUIT_JOINED]','[UNKNOWN_JOIN]'),@('bad_rpl','rpl=21','rpl=0'),
 @('original_missing','original_actor=0x4000000000000011','original_actor=0x4000000000000099')
)){
 if(!$golden.Contains($mutation[1])){throw "Mutation missing $($mutation[0])"}
 if(!@(Test-StockPressureText ($golden.Replace($mutation[1],$mutation[2]))).Count){throw "Unsafe log accepted $($mutation[0])"}
 $cases++
}
$root=Join-Path $RepositoryRoot ('.codex-runtime/pressure-log-contract-'+[guid]::NewGuid().ToString('N'))
[void][IO.Directory]::CreateDirectory($root)
try{
 $input=Join-Path $root 'input.txt';$output=Join-Path $root 'verdict.json';$audit=Join-Path $RepositoryRoot 'tests/log-audits/Test-AIQuotaStockPressureLog.ps1'
 $golden|Set-Content -LiteralPath $input
 & powershell.exe -NoProfile -ExecutionPolicy Bypass -File $audit -LogPath $input -OutputPath $output *> (Join-Path $root 'positive.txt')
 if($LASTEXITCODE-ne0-or!(Get-Content $output -Raw|ConvertFrom-Json).pass){throw 'Actual auditor CLI positive failed'}
 $golden.Replace('passed=1','passed=0')|Set-Content -LiteralPath $input
 & powershell.exe -NoProfile -ExecutionPolicy Bypass -File $audit -LogPath $input -OutputPath $output *> (Join-Path $root 'negative.txt')
 if($LASTEXITCODE-eq0-or(Get-Content $output -Raw|ConvertFrom-Json).pass){throw 'Actual auditor CLI failed-DONE accepted'}
}finally{
 $resolved=[IO.Path]::GetFullPath($root);$boundary=[IO.Path]::GetFullPath((Join-Path $RepositoryRoot '.codex-runtime'))+[IO.Path]::DirectorySeparatorChar
 if(!$resolved.StartsWith($boundary,[StringComparison]::OrdinalIgnoreCase)){throw 'Unexpected temporary target'}
 if(Test-Path -LiteralPath $resolved){Remove-Item -LiteralPath $resolved -Recurse -Force}
}
"PASS stock pressure functional auditor log_cases=$cases actual_winps_cli=2 native=NOT_RUN full_stopped_gates=RUNNER"
