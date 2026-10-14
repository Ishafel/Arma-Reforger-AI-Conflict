param(
    [string]$RepositoryRoot = (Split-Path -Parent (Split-Path -Parent $PSScriptRoot)),
    [string]$EvidenceRoot
)

$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot '../lib/Stage3StaticAudit.Common.ps1')
if (-not $EvidenceRoot) {
    $EvidenceRoot = Join-Path $RepositoryRoot ('.codex-runtime/audit-selection-' + [guid]::NewGuid().ToString('N'))
}
if (Test-Path -LiteralPath $EvidenceRoot) { throw "EvidenceRoot must be new: $EvidenceRoot" }
[void](New-Item -ItemType Directory -Path $EvidenceRoot)
$EvidenceRoot = (Resolve-Path -LiteralPath $EvidenceRoot).Path
$fixtureRoot = Join-Path $EvidenceRoot 'source-copy'
if (-not [IO.Path]::GetFullPath($fixtureRoot).StartsWith($EvidenceRoot + [IO.Path]::DirectorySeparatorChar, [StringComparison]::OrdinalIgnoreCase)) {
    throw "Unsafe fixture path: $fixtureRoot"
}
$commanderFile = 'AIConflictCore/Scripts/Game/AIConflict/Integration/AICF_CampaignState.c'
$greetingFile = 'AIConflictCore/Scripts/Game/AIConflict/Integration/AICF_GreetingAudio.c'
$serviceFile = 'AIConflictCore/Scripts/Game/AIConflict/Forces/AICF_InfantryRecruitmentService.c'
$utf8 = [Text.UTF8Encoding]::new($false)
$results = [System.Collections.Generic.List[object]]::new()

function Invoke-SelectionAudit {
    param([string]$Name, [string]$Audit, [int]$ExpectedExit, [string]$ExpectedRule)

    $auditPath = Join-Path $PSScriptRoot '../static/Test-AICommanderModeStatic.ps1'
    if ($Audit -eq 'Squad') { $auditPath = Join-Path $PSScriptRoot 'Test-SquadCommandsContracts.ps1' }
    $arguments = @('-NoProfile', '-ExecutionPolicy', 'Bypass', '-File', $auditPath, '-RepositoryRoot', $fixtureRoot)
    $output = & powershell.exe @arguments 2>&1
    $nativeExit = $LASTEXITCODE
    $log = Join-Path $EvidenceRoot ($Name + '.txt')
    $output | Set-Content -LiteralPath $log -Encoding UTF8
    $ruleFound = -not $ExpectedRule -or ($output -join "`n") -match ('(?m)^(?: - \[)?' + [regex]::Escape($ExpectedRule) + '(?:\]|\r?$)')
    $passed = $nativeExit -eq $ExpectedExit -and $ruleFound
    $results.Add([pscustomobject]@{
        case = $Name
        command = @('powershell.exe') + $arguments
        exit_code = $nativeExit
        expected_exit = $ExpectedExit
        expected_rule = $ExpectedRule
        passed = $passed
        log = $log
    })
    ConvertTo-Json -InputObject @($results.ToArray()) -Depth 5 | Set-Content -LiteralPath (Join-Path $EvidenceRoot 'summary.json') -Encoding UTF8
    if (-not $passed) { throw "Unexpected audit result for $Name (exit=$nativeExit, rule=$ExpectedRule): $output" }
    Write-Output "PASS case=$Name audit_exit=$nativeExit expected_rule=$ExpectedRule"
}

# Все входы берутся из текущего production source. Каждый случай меняет
# одну копию, запускает полный аудитор и проверяет конкретный rule ID.
$commanderCases = @(
    @{ Name='missing-owner'; Operation='MissingOwner'; Rule='AI_COMMANDER_COMPONENT' },
    @{ Name='wrong-class-owner'; Before='class SCR_GameModeCampaign'; After='class AICF_UnrelatedCampaign'; Rule='AI_COMMANDER_COMPONENT' },
    @{ Name='duplicate-owner-file'; Operation='DuplicateFile'; Rule='AI_COMMANDER_COMPONENT' },
    @{ Name='duplicate-owner-body'; Operation='DuplicateBody'; Rule='AI_COMMANDER_COMPONENT' },
    @{ Name='partial-second-owner'; Operation='PartialOwner'; Rule='AI_COMMANDER_COMPONENT' },
    @{ Name='us-rpl-removed'; Before='[RplProp(onRplName: "AICF_OnAICommanderStateReplicated")]'; After=''; Occurrence=0; Rule='AI_COMMANDER_REPLICATION' },
    @{ Name='ussr-rpl-removed'; Before='[RplProp(onRplName: "AICF_OnAICommanderStateReplicated")]'; After=''; Occurrence=1; Rule='AI_COMMANDER_REPLICATION' },
    @{ Name='us-rpl-comment'; Before='[RplProp(onRplName: "AICF_OnAICommanderStateReplicated")]'; After='/* [RplProp(onRplName: "AICF_OnAICommanderStateReplicated")] */'; Occurrence=0; Rule='AI_COMMANDER_REPLICATION' },
    @{ Name='ussr-callback-wrong'; Before='[RplProp(onRplName: "AICF_OnAICommanderStateReplicated")]'; After='[RplProp(onRplName: "AICF_OnTicketsReplicated")]'; Occurrence=1; Rule='AI_COMMANDER_REPLICATION' },
    @{ Name='callback-method-missing'; Before='void AICF_OnAICommanderStateReplicated()'; After='void AICF_UnusedCommanderCallback()'; Rule='AI_COMMANDER_REPLICATION' },
    @{ Name='server-guard'; Method='AICF_SetAICommanderState'; Before='!Replication.IsServer() || '; After=''; Rule='AI_COMMANDER_REPLICATION' },
    @{ Name='master-guard'; Method='AICF_SetAICommanderState'; Before=' || !IsMaster()'; After=''; Rule='AI_COMMANDER_REPLICATION' },
    @{ Name='authority-return'; Method='AICF_SetAICommanderState'; Before='return;'; After='/* return; */'; Occurrence=0; Rule='AI_COMMANDER_REPLICATION' },
    @{ Name='unchanged-return'; Method='AICF_SetAICommanderState'; Before='return;'; After='/* return; */'; Occurrence=1; Rule='AI_COMMANDER_REPLICATION' },
    @{ Name='unchanged-comparison'; Method='AICF_SetAICommanderState'; Before='m_bAICFUSAICommanderEnabled == usEnabled'; After='m_bAICFUSAICommanderEnabled != usEnabled'; Rule='AI_COMMANDER_REPLICATION' },
    @{ Name='premature-bump'; Method='AICF_SetAICommanderState'; Before='if (m_bAICFUSAICommanderEnabled == usEnabled'; After="Replication.BumpMe();`n`t`tif (m_bAICFUSAICommanderEnabled == usEnabled"; Rule='AI_COMMANDER_REPLICATION' },
    @{ Name='second-bump'; Method='AICF_SetAICommanderState'; Before='Replication.BumpMe();'; After='Replication.BumpMe(); Replication.BumpMe();'; Rule='AI_COMMANDER_REPLICATION' },
    @{ Name='wrong-assignment'; Method='AICF_SetAICommanderState'; Before='m_bAICFUSSRAICommanderEnabled = ussrEnabled;'; After='m_bAICFUSSRAICommanderEnabled = usEnabled;'; Rule='AI_COMMANDER_REPLICATION' },
    @{ Name='getter-faction'; Method='AICF_GetAICommanderEnabled'; Before='if (isUSSR)'; After='if (!isUSSR)'; Rule='AI_COMMANDER_REPLICATION' },
    @{ Name='getter-sentinel'; Method='AICF_HasAICommanderState'; Before='m_bAICFUSAICommanderEnabled || m_bAICFUSSRAICommanderEnabled'; After='true'; Rule='AI_COMMANDER_REPLICATION' }
)
$squadCases = @(
    @{ Name='nearest-equal'; Before='distance < nearestDistance'; After='distance <= nearestDistance'; Rule='NEAREST_GRAPH_STABLE_TIE' },
    @{ Name='nearest-worse'; Before='distance < nearestDistance'; After='distance > nearestDistance'; Rule='NEAREST_GRAPH_STABLE_TIE' },
    @{ Name='nearest-comment-decoy'; Before='distance < nearestDistance'; After='false /* distance < nearestDistance */'; Rule='NEAREST_GRAPH_STABLE_TIE' },
    @{ Name='nearest-distance-update'; Before='nearestDistance = distance;'; After='nearestDistance = 0;'; Rule='NEAREST_GRAPH_STABLE_TIE' },
    @{ Name='pending-equal'; Before='distanceSq < pendingDistance'; After='distanceSq <= pendingDistance'; Rule='PENDING_DISTANCE_STABLE_TIE' },
    @{ Name='pending-worse'; Before='distanceSq < pendingDistance'; After='distanceSq > pendingDistance'; Rule='PENDING_DISTANCE_STABLE_TIE' },
    @{ Name='pending-distance-update'; Before='pendingDistance = distanceSq;'; After='pendingDistance = 0;'; Rule='PENDING_DISTANCE_STABLE_TIE' },
    @{ Name='eta-equal'; Before='m_fCompletionSeconds >= bestCompletion'; After='m_fCompletionSeconds > bestCompletion'; Rule='ETA_SELECTION_STABLE_TIE' },
    @{ Name='eta-worse'; Before='m_fCompletionSeconds >= bestCompletion'; After='m_fCompletionSeconds <= bestCompletion'; Rule='ETA_SELECTION_STABLE_TIE' },
    @{ Name='distance-only'; Before='order.m_Forecast.m_fCompletionSeconds'; After='distanceSq'; Rule='ETA_SELECTION_STABLE_TIE' },
    @{ Name='eta-distance-update'; Before='bestCompletion = order.m_Forecast.m_fCompletionSeconds;'; After='bestCompletion = distanceSq;'; Rule='ETA_SELECTION_STABLE_TIE' },
    @{ Name='eta-comment-decoy'; Before='if (order.m_Forecast.m_fCompletionSeconds >= bestCompletion)'; After='if (false) /* if (order.m_Forecast.m_fCompletionSeconds >= bestCompletion) */'; Rule='ETA_SELECTION_STABLE_TIE' },
    @{ Name='graph-order'; Before='index++'; After='index--'; Rule='SELECTION_STABLE_ORDER' },
    @{ Name='candidate-order'; Before='nodeId++'; After='nodeId--'; Rule='SELECTION_STABLE_ORDER' },
    @{ Name='nearest-reversed-lookup'; Before='m_Graph.GetNode(index)'; After='m_Graph.GetNode(m_Graph.GetNodeCount() - 1 - index)'; Rule='SELECTION_STABLE_ORDER' },
    @{ Name='candidate-reversed-lookup'; Before='m_Graph.GetNode(nodeId)'; After='m_Graph.GetNode(m_Graph.GetNodeCount() - 1 - nodeId)'; Rule='SELECTION_STABLE_ORDER' },
    @{ Name='forecast-bypass'; Before='bool feasible = order.m_Forecast.Evaluate(Math.Sqrt(distanceSq), remainingSeconds);'; After='bool feasible = true;'; Rule='SELECTION_FEASIBILITY' },
    @{ Name='approach-bypass'; Before='!order.m_Forecast.CanApproach(playerRequested)'; After='false'; Rule='SELECTION_FEASIBILITY' },
    @{ Name='pending-beats-feasible'; Before='if (!best)'; After='if (pending)'; Rule='ETA_SELECTION_STABLE_TIE' }
)

try {
    foreach ($relative in @('AIConflictCore/Scripts', 'AIConflictCore/Language', 'AIConflictArland/Scripts', 'AIConflictEveron/Scripts', 'tests/log-audits')) {
        $destination = Join-Path $fixtureRoot $relative
        [void](New-Item -ItemType Directory -Path (Split-Path -Parent $destination) -Force)
        Copy-Item -LiteralPath (Join-Path $RepositoryRoot $relative) -Destination $destination -Recurse
    }
    Invoke-SelectionAudit 'commander-positive-before' 'Commander' 0
    Invoke-SelectionAudit 'squad-positive-before' 'Squad' 0

    # Владение не зависит от имени файла; нейтральное modded расширение допустимо.
    $originalPath = Join-Path $fixtureRoot $commanderFile
    $renamedPath = Join-Path (Split-Path -Parent $originalPath) 'AICF_RenamedCampaignState.c'
    Move-Item -LiteralPath $originalPath -Destination $renamedPath
    try { Invoke-SelectionAudit 'renamed-state-file' 'Commander' 0 }
    finally { Move-Item -LiteralPath $renamedPath -Destination $originalPath }
    $neutralPath = Join-Path (Split-Path -Parent $originalPath) 'AICF_NeutralCampaignExtension.c'
    [IO.File]::WriteAllText($neutralPath, 'modded class SCR_GameModeCampaign { protected bool m_bAICFNeutral; }', $utf8)
    try { Invoke-SelectionAudit 'unrelated-modded-extension' 'Commander' 0 }
    finally { Remove-Item -LiteralPath $neutralPath }

    foreach ($audit in @('Commander', 'Squad')) {
        $relative = $commanderFile
        $cases = $commanderCases
        if ($audit -eq 'Squad') { $relative = $serviceFile; $cases = $squadCases }
        foreach ($case in $cases) {
            $path = Join-Path $fixtureRoot $relative
            if ($case['Operation'] -eq 'PartialOwner') { $path = Join-Path $fixtureRoot $greetingFile }
            $original = [IO.File]::ReadAllText($path)
            $body = $original
            $mutated = ''
            $extraPath = $null
            if ($case['Operation'] -eq 'MissingOwner') {
                $mutated = 'modded class SCR_GameModeCampaign {}'
            } elseif ($case['Operation'] -eq 'DuplicateFile') {
                $extraPath = Join-Path (Split-Path -Parent $path) 'AICF_DuplicateCampaignState.c'
                [IO.File]::WriteAllText($extraPath, $original, $utf8)
                $mutated = $original
            } elseif ($case['Operation'] -eq 'DuplicateBody') {
                $mutated = $original + "`n" + $original
            } elseif ($case['Operation'] -eq 'PartialOwner') {
                $mutated = $original.Replace('protected int m_iAICFLastGreetingToken;', 'protected bool m_bAICFUSAICommanderEnabled; protected int m_iAICFLastGreetingToken;')
            } else {
                $method = $case['Method']
                if ($audit -eq 'Squad') { $method = 'SelectBarracks' }
                if ($method) {
                    $record = [pscustomobject]@{Source=$original; Code=(ConvertTo-AICFCodeText $original)}
                    $body = Get-AICFMethodBody $record $method
                }
                if (-not $body -or -not $body.Contains($case.Before)) { throw "Missing mutation target: $($case.Name)" }
                $changedBody = $body.Replace($case.Before, $case.After)
                if ($case.ContainsKey('Occurrence')) {
                    $matches = @([regex]::Matches($body, [regex]::Escape($case.Before)))
                    if ($matches.Count -ne 2) { throw "Expected two mutation targets: $($case.Name)" }
                    $match = $matches[$case.Occurrence]
                    $changedBody = $body.Remove($match.Index, $match.Length).Insert($match.Index, $case.After)
                }
                $mutated = $original.Replace($body, $changedBody)
            }
            if ($mutated -eq $original -and -not $extraPath) { throw "Unchanged mutation: $($case.Name)" }
            try {
                [IO.File]::WriteAllText($path, $mutated, $utf8)
                Invoke-SelectionAudit $case.Name $audit 1 $case.Rule
            }
            finally {
                [IO.File]::WriteAllText($path, $original, $utf8)
                if ($extraPath) { Remove-Item -LiteralPath $extraPath }
            }
        }
    }
    Invoke-SelectionAudit 'commander-positive-after' 'Commander' 0
    Invoke-SelectionAudit 'squad-positive-after' 'Squad' 0
}
finally {
    $resolvedFixture = [IO.Path]::GetFullPath($fixtureRoot)
    if (-not $resolvedFixture.StartsWith($EvidenceRoot + [IO.Path]::DirectorySeparatorChar, [StringComparison]::OrdinalIgnoreCase) -or
        (Split-Path -Leaf $resolvedFixture) -ne 'source-copy') {
        throw "Unsafe fixture cleanup path: $resolvedFixture"
    }
    if (Test-Path -LiteralPath $resolvedFixture) { Remove-Item -LiteralPath $resolvedFixture -Recurse -Force }
}
Write-Output "Audit selection contracts: PASS; positive controls=6; commander mutations=$($commanderCases.Count); recruitment mutations=$($squadCases.Count)"
Write-Output "AICF_AUDIT_SELECTION_EVIDENCE=$EvidenceRoot"
exit 0
