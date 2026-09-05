[CmdletBinding()]
param([string]$EvidenceRoot)
$ErrorActionPreference = 'Stop'
$repo = Split-Path -Parent $PSScriptRoot
if (-not $EvidenceRoot) { $EvidenceRoot = Join-Path $repo ('.codex-runtime/construction-contracts-' + (Get-Date -Format 'yyyyMMdd-HHmmss')) }
New-Item -ItemType Directory -Path $EvidenceRoot -Force | Out-Null
$failures = [System.Collections.Generic.List[string]]::new()
$common = 'token=construction-1 faction=US base=base1 provider=provider1 type=SMALL_BARRACKS cost=250 supplies_before=1000 supplies_after=750 reserve=500 layout=layout1 props_cost=71 props_before=10 props_after=81'
function Event([string]$Name, [int]$Time, [string]$Extra='') { return "SCRIPT : [AICF][STAGE1][INFO][$Name] t_ms=$Time $common $Extra" }
$positive = @(
    'SCRIPT : [AICF][ROSTER_READY]',
    (Event 'CONSTRUCTION_DECISION' 60000),
    (Event 'CONSTRUCTION_SITE_SELECTED' 61000),
    (Event 'CONSTRUCTION_RESERVED' 61000 'supply_debited=0'),
    (Event 'CONSTRUCTION_PAYMENT' 61000 'debit_count=1 tickets=0 reservation_released=1'),
    (Event 'CONSTRUCTION_PLACED' 61000 'service_online=0'),
    'SCRIPT : [AICF][BUILDER_PROGRESS] t_ms=90000 target=layout1 tool_active=1 item_using=1',
    'SCRIPT : [AICF][BUILDER_COMPLETED] t_ms=93000 target=layout1 tool_active=1 item_using=1',
    (Event 'CONSTRUCTION_COMPLETED' 94000 'service_online=1'),
    'ENGINE : Game destroyed.'
) -join "`n"
function CheckLog([string]$Name, [string]$Content, [string]$ExpectedRule='', [string]$Mode='BOTH', [bool]$Completion=$true) {
    $path = Join-Path $EvidenceRoot ($Name + '.log')
    [IO.File]::WriteAllText($path, $Content)
    $arguments = @('-NoProfile','-ExecutionPolicy','Bypass','-File', (Join-Path $PSScriptRoot 'Test-ConstructionLog.ps1'), '-LogPath', $path, '-ExpectedMode', $Mode)
    if ($Completion) { $arguments += '-RequireCompletion' }
    $result = & powershell.exe @arguments 2>&1
    $code = $LASTEXITCODE
    $result | Set-Content -LiteralPath (Join-Path $EvidenceRoot ($Name + '.txt'))
    $valid = $code -eq 0
    if ($ExpectedRule) { $valid = $code -eq 1 -and ($result -join "`n") -match [regex]::Escape("[$ExpectedRule]") }
    if (-not $valid) { $failures.Add($Name) }
    Write-Output "$Name exit=$code expected_rule=$ExpectedRule matched=$valid"
}
CheckLog 'positive-worker-service' $positive
CheckLog 'negative-double-payment' ($positive.Replace((Event 'CONSTRUCTION_PLACED' 61000 'service_online=0'), (Event 'CONSTRUCTION_PAYMENT' 61000 'debit_count=1 tickets=0 reservation_released=1') + "`n" + (Event 'CONSTRUCTION_PLACED' 61000 'service_online=0'))) 'CONSTRUCTION_PAYMENT_WITHOUT_RESERVATION'
CheckLog 'negative-debit-balance' ($positive.Replace('supplies_after=750','supplies_after=749')) 'CONSTRUCTION_DEBIT_BALANCE'
CheckLog 'negative-prop-balance' ($positive.Replace('props_after=81','props_after=152')) 'CONSTRUCTION_PROP_BALANCE'
CheckLog 'negative-deferred-prop-balance' ($positive.Replace('ENGINE : Game destroyed.', "SCRIPT : [AICF][CONSTRUCTION_DEFERRED_PROBE] accepted=1 props_current=152 props_expected=81 root_present=1`nENGINE : Game destroyed.")) 'CONSTRUCTION_DEFERRED_PROP_BALANCE'
CheckLog 'negative-premature-service' ($positive.Replace('service_online=0','service_online=1')) 'CONSTRUCTION_PREMATURE_SERVICE'
CheckLog 'negative-before-roster' ($positive.Replace('[ROSTER_READY]','[NOT_READY]')) 'CONSTRUCTION_BEFORE_ROSTER'
CheckLog 'negative-player-side' $positive 'CONSTRUCTION_PLAYER_SIDE' 'USSR'
CheckLog 'negative-unconfirmed-tool' ($positive.Replace('tool_active=1','tool_active=0')) 'CONSTRUCTION_NO_BUILDER_COMPLETION'
CheckLog 'negative-layout-identity' ($positive.Replace((Event 'CONSTRUCTION_COMPLETED' 94000 'service_online=1'), (Event 'CONSTRUCTION_COMPLETED' 94000 'service_online=1').Replace('layout=layout1','layout=layout2'))) 'CONSTRUCTION_LAYOUT_IDENTITY'
CheckLog 'negative-live-log' ($positive.Replace('Game destroyed.','Still running.')) 'CONSTRUCTION_LOG_NOT_STOPPED'
$rollback = @(
    'SCRIPT : [AICF][ROSTER_READY]', (Event 'CONSTRUCTION_DECISION' 60000),
    (Event 'CONSTRUCTION_SITE_SELECTED' 61000), (Event 'CONSTRUCTION_RESERVED' 61000 'supply_debited=0'),
    (Event 'CONSTRUCTION_ROLLBACK' 61000 'restored=1 stock_refund=0 reservation_released=1').Replace('supplies_after=750','supplies_after=1000').Replace('props_after=81','props_after=10'),
    (Event 'CONSTRUCTION_CANCELLED' 61000 'reservation_released=1'), 'ENGINE : Game destroyed.'
) -join "`n"
CheckLog 'positive-rollback' $rollback '' 'BOTH' $false
CheckLog 'negative-refund-balance' ($rollback.Replace('supplies_after=1000','supplies_after=999')) 'CONSTRUCTION_ROLLBACK_BALANCE' 'BOTH' $false
# Негативный static input — отдельная копия исходников; рабочая реализация не меняется.
$fixtureRepo = Join-Path $EvidenceRoot 'static-input'
$fixtureCore = Join-Path $fixtureRepo 'AIConflictCore/Scripts/Game/AIConflict'
New-Item -ItemType Directory -Path $fixtureCore -Force | Out-Null
foreach ($domain in @('Construction','Economy','Config','Command','Bootstrap','Vehicles')) {
    Copy-Item -LiteralPath (Join-Path $repo "AIConflictCore/Scripts/Game/AIConflict/$domain") -Destination $fixtureCore -Recurse -Force
}
$staticTool = Join-Path $PSScriptRoot 'Test-ConstructionStatic.ps1'
$result = & powershell.exe -NoProfile -ExecutionPolicy Bypass -File $staticTool -RepositoryRoot $fixtureRepo
if ($LASTEXITCODE -ne 0) { $failures.Add('static-positive') }
$result | Set-Content -LiteralPath (Join-Path $EvidenceRoot 'static-positive.txt')
$orderPath = Join-Path $fixtureCore 'Construction/AICF_ConstructionOrder.c'
$order = [IO.File]::ReadAllText($orderPath)
[IO.File]::WriteAllText($orderPath, $order.Replace('m_Provider.GetOwner().GetID() == m_ProviderId','true'))
$result = & powershell.exe -NoProfile -ExecutionPolicy Bypass -File $staticTool -RepositoryRoot $fixtureRepo
if ($LASTEXITCODE -ne 1 -or ($result -join "`n") -notmatch '\[CONSTRUCTION_PROVIDER_IDENTITY\]') { $failures.Add('static-negative-identity') }
$result | Set-Content -LiteralPath (Join-Path $EvidenceRoot 'static-negative-identity.txt')
if ($failures.Count) { Write-Output "Construction contract inputs: FAIL $($failures -join ',')"; exit 1 }
Write-Output 'Construction contract inputs: PASS (13 log inputs + positive/negative static inputs)'
