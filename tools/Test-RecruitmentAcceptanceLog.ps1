param(
    [Parameter(Mandatory)][string]$LogPath,
    [Parameter(Mandatory)][ValidateSet('Lifecycle','Stability')][string]$Mode
)
$ErrorActionPreference = 'Stop'
$log = Get-Content -LiteralPath $LogPath -Raw
if ($log -notmatch 'Game destroyed\.') { throw 'Нужен полный остановленный server log.' }
if ($log -match 'SCRIPT\s+\((E|F)\)|ENGINE\s+\(F\)|NULL pointer|Virtual Machine Exception') { throw 'Script/VM error в полном логе.' }
$prefix = 'RECRUITMENT_' + $Mode.ToUpperInvariant() + '_PROBE'
if ($log -match "\[$prefix\] case=\S+ pass=0") { throw 'Fixture зафиксировала FAIL.' }
if ($Mode -eq 'Lifecycle') {
    $cases = @('LIVE_DEMAND','CANCEL_RELEASE','OWNER_CHANGED_RELEASE','OWNER_CHANGED_FINISHED',
        'DEATH_RELEASE','REPLACEMENT_IDENTITY','OLD_VISIT_RELEASED','REPLACEMENT_NEW_DEMAND',
        'RECRUIT_REDUCES_DEMAND','COMPLETION_RELEASE')
    if ($log -notmatch '\[RECRUITMENT_LIFECYCLE_PROBE\] finished=1 passed=10 total=10 phase=6') { throw 'Нет завершённой матрицы 10/10.' }
    if ($log -notmatch 'INFANTRY_RECRUIT_JOINED') { throw 'Нет фактической передачи recruits.' }
} else {
    $cases = @('TWO_REGISTERED_BASES','DETERMINISTIC_SAMPLES','SMALL_GAIN_HELD','LARGE_GAIN_REPLAN','REPLAN_DEMAND_TRANSFER')
    if ($log -notmatch '\[RECRUITMENT_STABILITY_PROBE\] finished=1 pass=1') { throw 'Нет успешного завершения stability.' }
    if ($log -match '\[RECRUITMENT_STABILITY_SAMPLE\][^\r\n]+(deterministic|token_stable)=0') { throw 'Нестабильный sample противоречит итоговому PASS.' }
    $samples = [regex]::Matches($log, '\[RECRUITMENT_STABILITY_SAMPLE\] elapsed_ms=(\d+) swing=(-?1) deterministic=1 token_stable=1 challenger=([01])')
    if ($samples.Count -lt 20 -or ($samples | Where-Object { [int]$_.Groups[1].Value -ge 120000 }).Count -lt 1) { throw 'Недостаточная длительность стабильного визита.' }
    if (($samples | Where-Object { $_.Groups[3].Value -eq '1' -and [int]$_.Groups[1].Value -ge 60000 }).Count -lt 3) { throw 'Нет более выгодной альтернативы после cooldown.' }
    if ([regex]::Matches($log,'INFANTRY_RECRUITMENT_REEVALUATED').Count -lt 4) { throw 'Недостаточно production reevaluations.' }
    if ([regex]::Matches($log,'RECRUITMENT_STABILITY_SAMPLE[^\r\n]+challenger_reevaluation=1').Count -lt 2) { throw 'Альтернатива должна выигрывать минимум в двух production reevaluations после cooldown.' }
    if ([regex]::Matches($log,'INFANTRY_RECRUITMENT_FINISHED[^\r\n]+reason=SUPPLY_REPLAN').Count -ne 1) { throw 'Ожидается ровно одно переключение после большого изменения.' }
}
foreach ($case in $cases) {
    if ([regex]::Matches($log,"\[$prefix\] case=$case pass=1\b").Count -ne 1) { throw "Нет однократного PASS: $case" }
}
Write-Output "PASS $Mode cases=$($cases.Count) stopped_log=$LogPath"
