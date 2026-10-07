param(
    [string]$RepositoryRoot = (Split-Path -Parent (Split-Path -Parent $PSScriptRoot)),
    [string]$EvidenceRoot
)

$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot '../lib/Stage3StaticAudit.Common.ps1')
$tempBase = [IO.Path]::GetFullPath([IO.Path]::GetTempPath()).TrimEnd([char[]]"\/")
$fixtureRoot = Join-Path $tempBase ('aicf-commander-ui-' + [guid]::NewGuid().ToString('N'))
$markerFile = 'AIConflictCore/Scripts/Game/AIConflict/UI/AICF_GroupMapMarkers.c'
$tableFile = 'AIConflictCore/Language/AICF_Localization.st'
$utf8 = [Text.UTF8Encoding]::new($false)
if ($EvidenceRoot) { [void](New-Item -ItemType Directory -Path $EvidenceRoot -Force) }

function Invoke-ContractAudit([string]$Name, [int]$ExpectedExit) {
    $audit = Join-Path $PSScriptRoot '../static/Test-AICommanderModeStatic.ps1'
    $output = & powershell.exe -NoProfile -ExecutionPolicy Bypass -File $audit -RepositoryRoot $fixtureRoot 2>&1
    $nativeExit = $LASTEXITCODE
    if ($EvidenceRoot) {
        $output | Set-Content -LiteralPath (Join-Path $EvidenceRoot "$Name.txt") -Encoding UTF8
        [pscustomobject]@{
            case = $Name
            command = @('powershell.exe', '-NoProfile', '-ExecutionPolicy', 'Bypass', '-File', $audit, '-RepositoryRoot', $fixtureRoot)
            exitCode = $nativeExit
        } | ConvertTo-Json -Compress | Add-Content -LiteralPath (Join-Path $EvidenceRoot 'commands.jsonl') -Encoding UTF8
    }
    if ($nativeExit -ne $ExpectedExit -or
        ($ExpectedExit -eq 1 -and ($output -join "`n") -notmatch '\[AI_COMMANDER_UI_STATE\]')) {
        throw "Unexpected audit result for $Name (exit=$nativeExit): $output"
    }
    Write-Output "PASS case=$Name audit_exit=$nativeExit"
}

# Каждая мутация меняет ровно одно тело метода в изолированной копии.
# Слова в comments не должны заменять исполняемую защиту.
$cases = @(
    @{ Name='awaiting-gate'; Method='IsAttackObjective'; Before='slot.IsAwaitingPlayerCommand() ||'; After='/* slot.IsAwaitingPlayerCommand() || */' },
    @{ Name='hold-gate'; Method='IsAttackObjective'; Before='slot.IsSystemHoldOrder() ||'; After='/* slot.IsSystemHoldOrder() || */' },
    @{ Name='recruitment-gate'; Method='IsAttackObjective'; Before='slot.IsRecruitingInfantry() ||'; After='' },
    @{ Name='ready-gate'; Method='IsAttackObjective'; Before='!slot.IsCombatReady() ||'; After='' },
    @{ Name='role-gate'; Method='IsAttackObjective'; Before='slot.GetRole() != AICF_EGroupRole.ATTACK'; After='false' },
    @{ Name='guard-return'; Method='IsAttackObjective'; Before='return false;'; After='return true;' },
    @{ Name='friendly-target'; Method='IsAttackObjective'; Before='target.GetFaction() != faction'; After='true /* target.GetFaction() != faction */' },
    @{ Name='helper-bypass'; Method='SyncFactionObjectiveMarkers'; Before='!IsAttackObjective(slot, markerFaction)'; After='false /* !IsAttackObjective(slot, markerFaction) */' },
    @{ Name='helper-inversion'; Method='SyncFactionObjectiveMarkers'; Before='!IsAttackObjective(slot, markerFaction)'; After='IsAttackObjective(slot, markerFaction)' },
    @{ Name='continue-removed'; Method='SyncFactionObjectiveMarkers'; Before='continue;'; After='/* continue; */' },
    @{ Name='task-awaiting'; Method='DescribeTask'; Before='slot.IsAwaitingPlayerCommand() || '; After='' },
    @{ Name='task-hold'; Method='DescribeTask'; Before=' || slot.IsSystemHoldOrder()'; After='' },
    @{ Name='task-key'; Method='DescribeTask'; Before='AICF_UI_Awaiting_orders_05004120'; After='AICF_MissingWaitingLabel' },
    @{ Name='details-key'; Method='BuildMarkerDetails'; Before='AICF_UI_Awaiting_player_orders_8d41bca5'; After='AICF_MissingAuthorityLabel' },
    @{ Name='details-render'; Method='BuildMarkerDetails'; Before='string.Format("%1", authority)'; After='string.Empty' },
    @{ Name='missing-key'; File=$tableFile; Before='Id "AICF_UI_Awaiting_orders_05004120"'; After='Id "AICF_UnusedWaitingLabel"' },
    @{ Name='wrong-russian'; File=$tableFile; Before='Target_ru_ru "Ожидает приказа"'; After='Target_ru_ru "Атакует"' },
    @{ Name='wrong-english'; File=$tableFile; Before='Target_en_us "Awaiting orders"'; After='Target_en_us "Attacking"' }
)
try {
    foreach ($relative in @('AIConflictCore/Scripts', 'AIConflictCore/Language', 'AIConflictArland/Scripts', 'AIConflictEveron/Scripts')) {
        $destination = Join-Path $fixtureRoot $relative
        [void](New-Item -ItemType Directory -Path (Split-Path -Parent $destination) -Force)
        Copy-Item -LiteralPath (Join-Path $RepositoryRoot $relative) -Destination $destination -Recurse
    }
    Invoke-ContractAudit 'positive-before' 0
    foreach ($case in $cases) {
        $relative = $markerFile
        if ($case.ContainsKey('File')) { $relative = $case.File }
        $path = Join-Path $fixtureRoot $relative
        $original = [IO.File]::ReadAllText($path)
        $body = $original
        if ($case.ContainsKey('Method')) {
            $record = [pscustomobject]@{Source=$original; Code=(ConvertTo-AICFCodeText $original)}
            $body = Get-AICFMethodBody $record $case.Method
        }
        if (-not $body -or -not $body.Contains($case.Before)) { throw "Missing mutation target: $($case.Name)" }
        $mutated = $original.Replace($body, $body.Replace($case.Before, $case.After))
        try {
            [IO.File]::WriteAllText($path, $mutated, $utf8)
            Invoke-ContractAudit $case.Name 1
        }
        finally { [IO.File]::WriteAllText($path, $original, $utf8) }
    }
    Invoke-ContractAudit 'positive-after' 0
}
finally {
    $resolvedFixture = [IO.Path]::GetFullPath($fixtureRoot)
    if (-not $resolvedFixture.StartsWith($tempBase + [IO.Path]::DirectorySeparatorChar, [StringComparison]::OrdinalIgnoreCase) -or
        (Split-Path -Leaf $resolvedFixture) -notlike 'aicf-commander-ui-*') {
        throw "Unsafe fixture cleanup path: $resolvedFixture"
    }
    if (Test-Path -LiteralPath $resolvedFixture) { Remove-Item -LiteralPath $resolvedFixture -Recurse -Force }
}
Write-Output "AI commander UI contracts: PASS; negative mutations=$($cases.Count)"
exit 0
