param(
    [string]$RepositoryRoot = (Split-Path -Parent $PSScriptRoot)
)

$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'Stage3StaticAudit.Common.ps1')

# Вызовы, comments и строки до объявления не должны подменять тело метода.
$parserFixture = @'
class AICF_ParserFixture
{
    bool Caller()
    {
        bool bound = ready &&
            IsWaypointBoundToGroup(group, waypoint);
        if (!bound) { return false; }
        return IsWaypointBoundToGroup(group, waypoint);
    }
    // bool IsWaypointBoundToGroup() { return false; }
    /* bool IsWaypointBoundToGroup() { return false; } */
    string label = "bool IsWaypointBoundToGroup() { return false; }";
    protected bool IsWaypointBoundToGroup(
        SCR_AIGroup group, AIWaypoint waypoint, string text = "brace { and escaped quote \"")
    // Ложная открывающая скобка: {
    {
        if (!group || !waypoint) { return false; }
        array<AIWaypoint> waypointQueue = {};
        group.GetWaypoints(waypointQueue);
        return waypointQueue.Contains(waypoint);
    }
    void After() { UnexpectedBody(); }
}
'@
$parserRecord = [pscustomobject]@{ Source = $parserFixture; Code = ConvertTo-AICFCodeText $parserFixture }
$body = Get-AICFMethodBody $parserRecord 'IsWaypointBoundToGroup'
if ($body -notmatch 'group\.GetWaypoints\(waypointQueue\)' -or
    $body -notmatch 'return waypointQueue\.Contains\(waypoint\)' -or
    $body -match 'UnexpectedBody|bool bound|Ложная') {
    throw 'Method parser did not isolate the declaration body'
}
foreach ($source in @(
    "class AICF_CallOnly {`n    bool Caller() {`n        return Target();`n    }`n    void After() { Wrong(); }`n}",
    "class AICF_Prototype {`n    bool Target();`n    void After() { Wrong(); }`n}",
    "class AICF_Comment {`n/*`n    bool Target() { Wrong(); }`n*/`n}",
    "class AICF_Unclosed {`n    bool Target() { return true;"
)) {
    $record = [pscustomobject]@{ Source = $source; Code = ConvertTo-AICFCodeText $source }
    if (Get-AICFMethodBody $record 'Target') { throw 'Method parser accepted a missing declaration or an unclosed body' }
}
Write-Output 'Method parser regression fixtures: PASS'

$tempBase = [System.IO.Path]::GetFullPath([System.IO.Path]::GetTempPath()).TrimEnd([char[]]"\/")
$fixtureRoot = Join-Path $tempBase ('aicf-static-contracts-' + [guid]::NewGuid().ToString('N'))
$corePath = 'AIConflictCore/Scripts/Game/AIConflict'
$utf8 = [System.Text.UTF8Encoding]::new($false)
$cases = @(
    @{ Name = 'timeout'; File = 'Config/AICF_Stage3Config.c'; Before = 'DEFAULT_OBJECTIVE_PROGRESS_TIMEOUT_MS = 120000;'; After = 'DEFAULT_OBJECTIVE_PROGRESS_TIMEOUT_MS = 300000;'; Audits = @{ Stage3 = 'STAGE3_PROGRESS_EVIDENCE' } },
    @{ Name = 'blocker'; File = 'Vehicles/AICF_VehicleCleanupManager.c'; Before = 'terminalReason + ":" + job.m_Scan.m_sBlockerSignature'; After = 'terminalReason'; Audits = @{ Stage3 = 'STAGE3_BOUNDED_PROTECTED_CLEARANCE'; Stage35 = 'STAGE35_BOUNDED_PROTECTED_CLEARANCE' } },
    @{ Name = 'grace'; File = 'Vehicles/AICF_VehicleCleanupManager.c'; Before = 'if (nowMs >= terminalAtMs)'; After = 'if (false)'; Audits = @{ Stage3 = 'STAGE3_BOUNDED_PROTECTED_CLEARANCE'; Stage35 = 'STAGE35_BOUNDED_PROTECTED_CLEARANCE' } },
    @{ Name = 'marker-state'; File = 'UI/AICF_GroupMapMarkers.c'; Before = 'vehicleCoordinator.GetSlotDisplayStatusText(slot)'; After = 'string.Empty'; Audits = @{ Stage3 = 'STAGE3_MARKER_STATE' } },
    @{ Name = 'marker-placeholder'; File = 'UI/AICF_GroupMapMarkers.c'; Before = '%5 | %6'; After = '| %6'; Audits = @{ Stage3 = 'STAGE3_MARKER_STATE' } },
    @{ Name = 'marker-render'; File = 'UI/AICF_GroupMapMarkers.c'; Before = 'vehicleState,'; After = 'string.Empty,'; Audits = @{ Stage3 = 'STAGE3_MARKER_STATE' } },
    @{ Name = 'queue-read'; File = 'Bootstrap/AICF_MatchController.c'; Before = 'group.GetWaypoints(waypointQueue);'; After = '// group.GetWaypoints(waypointQueue);'; Audits = @{ Stage35 = 'STAGE35_MEANINGFUL_TASK_PROOF' } },
    @{ Name = 'queue-membership'; File = 'Bootstrap/AICF_MatchController.c'; Before = 'return waypointQueue.Contains(waypoint);'; After = 'return true; // waypointQueue.Contains(waypoint)'; Audits = @{ Stage35 = 'STAGE35_MEANINGFUL_TASK_PROOF' } }
)
try {
    $targetCore = Join-Path $fixtureRoot $corePath
    [void](New-Item -ItemType Directory -Path (Split-Path -Parent $targetCore) -Force)
    Copy-Item -LiteralPath (Join-Path $RepositoryRoot $corePath) -Destination $targetCore -Recurse
    foreach ($case in $cases) {
        $path = Join-Path $targetCore $case.File
        $original = [System.IO.File]::ReadAllText($path)
        if (-not $original.Contains($case.Before)) { throw "Mutation target missing: $($case.Name)" }
        try {
            [System.IO.File]::WriteAllText($path, $original.Replace($case.Before, $case.After), $utf8)
            foreach ($auditName in ($case.Audits.Keys | Sort-Object)) {
                $output = & powershell.exe -NoProfile -ExecutionPolicy Bypass -File (Join-Path $PSScriptRoot "Test-${auditName}Static.ps1") -RepositoryRoot $fixtureRoot 2>&1
                $auditExit = $LASTEXITCODE
                $expectedRule = $case.Audits[$auditName]
                if ($auditExit -ne 1 -or ($output -join "`n") -notmatch ('\[' + [regex]::Escape($expectedRule) + '\]')) {
                    throw "Mutation $($case.Name) did not fail $expectedRule (exit=$auditExit): $output"
                }
                Write-Output "Negative contract fixture: PASS case=$($case.Name) audit=$auditName detected=$expectedRule"
            }
        }
        finally {
            [System.IO.File]::WriteAllText($path, $original, $utf8)
        }
    }
}
finally {
    $resolvedFixture = [System.IO.Path]::GetFullPath($fixtureRoot)
    if (-not $resolvedFixture.StartsWith($tempBase + [System.IO.Path]::DirectorySeparatorChar, [System.StringComparison]::OrdinalIgnoreCase) -or
        (Split-Path -Leaf $resolvedFixture) -notlike 'aicf-static-contracts-*') {
        throw "Unsafe fixture cleanup path: $resolvedFixture"
    }
    if (Test-Path -LiteralPath $resolvedFixture) { Remove-Item -LiteralPath $resolvedFixture -Recurse -Force }
}
Write-Output 'Stage 3/3.5 static contract regression: PASS'
