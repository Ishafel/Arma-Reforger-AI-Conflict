[CmdletBinding(DefaultParameterSetName = 'Valid')]
param(
    [Parameter(Mandatory = $true)]
    [string]$ServerLogPath,

    [Parameter(Mandatory = $true, ParameterSetName = 'Valid')]
    [ValidateSet('BOTH', 'US', 'USSR')]
    [string]$ExpectedMode,

    [Parameter(Mandatory = $true, ParameterSetName = 'Invalid')]
    [AllowEmptyString()]
    [string]$ExpectedInvalidValue,

    [string]$ClientLogPath,

    [Parameter(ParameterSetName = 'Valid')]
    [switch]$RequireInitialCoverage
)

$ErrorActionPreference = 'Stop'
$resolvedServerLog = (Resolve-Path -LiteralPath $ServerLogPath).Path
$serverLines = Get-Content -LiteralPath $resolvedServerLog
$failures = [System.Collections.Generic.List[string]]::new()

function Get-AICFEventMatches {
    param(
        [string[]]$Lines,
        [string]$EventName
    )

    $eventPattern = '\[AICF\]\[[^\]]+\]\[(?:INFO|WARNING|ERROR)\]\[' +
        [regex]::Escape($EventName) + '\]'
    return @($Lines | Select-String -Pattern $eventPattern)
}

function Get-AICFFieldValue {
    param(
        [string]$Line,
        [string]$FieldName
    )

    $fieldMatch = [regex]::Match(
        $Line,
        '(?<!\S)' + [regex]::Escape($FieldName) + '=("[^"]*"|[^\s]+)')
    if (-not $fieldMatch.Success) {
        return $null
    }

    return $fieldMatch.Groups[1].Value.Trim('"')
}

function Test-AICFExpectedField {
    param(
        [string]$Line,
        [string]$FieldName,
        [string]$ExpectedValue
    )

    return (Get-AICFFieldValue $Line $FieldName) -ceq $ExpectedValue
}

function Test-AICFFactionAIControlled {
    param(
        [string]$Mode,
        [string]$Faction
    )

    if ($Mode -ceq 'BOTH') {
        return $Faction -ceq 'US' -or $Faction -ceq 'USSR'
    }

    return $Mode -ceq $Faction
}

function Add-AICFUnexpectedRuntimeFailures {
    param(
        [string[]]$Lines,
        [string]$Source,
        [switch]$AllowConfigInvalid
    )

    $aicfErrors = @($Lines | Select-String -Pattern '\[AICF\]\[[^\]]+\]\[ERROR\]')
    if ($AllowConfigInvalid) {
        $aicfErrors = @($aicfErrors | Where-Object {
            $_.Line -notmatch '\[CONFIG_INVALID\]'
        })
    }
    if ($aicfErrors) {
        $failures.Add("$Source AICF diagnostic ERROR events: $($aicfErrors.Count)")
    }

    $scriptErrors = @($Lines | Select-String -Pattern (
        'SCRIPT\s+\((?:E|F)\)|ENGINE\s+\(F\)|Virtual Machine Exception|NULL pointer'))
    if ($AllowConfigInvalid) {
        $scriptErrors = @($scriptErrors | Where-Object {
            $_.Line -notmatch '\[AICF\].*\[CONFIG_INVALID\]' -and
            $_.Line -notmatch '\[AICF\].*\[RESULT\]\[FAIL\].*reason=CONFIG_INVALID'
        })
    }
    if ($scriptErrors) {
        $failures.Add("$Source script/engine/VM failures: $($scriptErrors.Count)")
    }
}

$isInvalidRun = $PSCmdlet.ParameterSetName -eq 'Invalid'
$clientLines = $null
$hasClientLog = -not [string]::IsNullOrEmpty($ClientLogPath)
if ($hasClientLog) {
    $resolvedClientLog = (Resolve-Path -LiteralPath $ClientLogPath).Path
    $clientLines = Get-Content -LiteralPath $resolvedClientLog
}

if ($isInvalidRun) {
    Add-AICFUnexpectedRuntimeFailures `
        -Lines $serverLines `
        -Source 'Server' `
        -AllowConfigInvalid

    $invalidEvents = @(Get-AICFEventMatches $serverLines 'CONFIG_INVALID')
    $escapedInvalidValue = [regex]::Escape($ExpectedInvalidValue)
    $expectedInvalidPattern = 'parameter=aicfAICommanderMode\s+' +
        'value="' + $escapedInvalidValue + '"\s+' +
        'allowed=BOTH,US,USSR(?:\s|$)'
    $matchingInvalidEvents = @($invalidEvents | Where-Object {
        $_.Line -cmatch $expectedInvalidPattern
    })
    if ($invalidEvents.Count -ne 1) {
        $failures.Add(
            "Expected exactly one CONFIG_INVALID event, found $($invalidEvents.Count)")
    }
    elseif ($matchingInvalidEvents.Count -ne 1) {
        $failures.Add(
            "CONFIG_INVALID does not preserve exact value '$ExpectedInvalidValue' " +
            'or allowed=BOTH,US,USSR')
    }

    $resultFailures = @($serverLines | Select-String -Pattern (
        '\[AICF\]\[STAGE1\]\[RESULT\]\[FAIL\].*\breason=CONFIG_INVALID\b'))
    if ($resultFailures.Count -ne 1) {
        $failures.Add(
            "Expected exactly one Stage 1 RESULT FAIL for CONFIG_INVALID, " +
            "found $($resultFailures.Count)")
    }

    $forbiddenEvidence = @(
        [pscustomobject]@{
            Name = 'Stage 1 CONFIG'
            Pattern = '\[AICF\]\[STAGE1\]\[INFO\]\[CONFIG\]'
        },
        [pscustomobject]@{
            Name = 'MATCH_START'
            Pattern = '\[MATCH_START\]'
        },
        [pscustomobject]@{
            Name = 'CONFLICT_READY'
            Pattern = '\[CONFLICT_READY\]'
        },
        [pscustomobject]@{
            Name = 'radio bridge activity'
            Pattern = '\[RADIO_BRIDGE_[A-Z0-9_.-]+\]'
        },
        [pscustomobject]@{
            Name = 'command authority activity'
            Pattern = '\[COMMAND_AUTHORITY_(?:SET|REPLICATED)\]'
        },
        [pscustomobject]@{
            Name = 'strategic command activity'
            Pattern = '\[(?:STRATEGIC_ASSIGNMENT|COMMAND_WAITING)\]'
        },
        [pscustomobject]@{
            Name = 'roster/spawn activity'
            Pattern = '\[AICF\]\[[^\]]+\]\[(?:INFO|WARNING|ERROR)\]' +
                '\[[A-Z0-9_.-]*(?:ROSTER|SPAWN)[A-Z0-9_.-]*\]'
        },
        [pscustomobject]@{
            Name = 'heartbeat activity'
            Pattern = '\[AICF\]\[[^\]]+\]\[(?:INFO|WARNING|ERROR)\]' +
                '\[[A-Z0-9_.-]*HEARTBEAT\]'
        }
    )
    foreach ($forbidden in $forbiddenEvidence) {
        $matches = @($serverLines | Select-String -Pattern $forbidden.Pattern)
        if ($matches) {
            $failures.Add(
                "Invalid configuration reached $($forbidden.Name): $($matches.Count) event(s)")
        }
    }

    if ($hasClientLog) {
        Add-AICFUnexpectedRuntimeFailures -Lines $clientLines -Source 'Client'
        foreach ($forbidden in $forbiddenEvidence) {
            $matches = @($clientLines | Select-String -Pattern $forbidden.Pattern)
            if ($matches) {
                $failures.Add(
                    "Client log contains $($forbidden.Name) after invalid configuration: " +
                    "$($matches.Count) event(s)")
            }
        }
    }
}
else {
    Add-AICFUnexpectedRuntimeFailures -Lines $serverLines -Source 'Server'

    $expectedUSFlag = '0'
    $expectedUSSRFlag = '0'
    if (Test-AICFFactionAIControlled $ExpectedMode 'US') {
        $expectedUSFlag = '1'
    }
    if (Test-AICFFactionAIControlled $ExpectedMode 'USSR') {
        $expectedUSSRFlag = '1'
    }

    $configEvents = @($serverLines | Select-String -Pattern (
        '\[AICF\]\[STAGE1\]\[INFO\]\[CONFIG\]'))
    if ($configEvents.Count -ne 1) {
        $failures.Add("Expected exactly one Stage 1 CONFIG, found $($configEvents.Count)")
    }
    else {
        $configLine = $configEvents[0].Line
        if (-not (Test-AICFExpectedField $configLine 'ai_commander_mode' $ExpectedMode) -or
            -not (Test-AICFExpectedField $configLine 'ai_commander_us' $expectedUSFlag) -or
            -not (Test-AICFExpectedField $configLine 'ai_commander_ussr' $expectedUSSRFlag)) {
            $failures.Add(
                "Stage 1 CONFIG does not match mode=$ExpectedMode " +
                "us=$expectedUSFlag ussr=$expectedUSSRFlag")
        }
    }

    $authorityEvents = @(Get-AICFEventMatches $serverLines 'COMMAND_AUTHORITY_SET')
    if ($authorityEvents.Count -ne 2) {
        $failures.Add(
            "Expected exactly two COMMAND_AUTHORITY_SET events, found $($authorityEvents.Count)")
    }
    foreach ($faction in @('US', 'USSR')) {
        $expectedAuthority = 'PLAYER'
        if (Test-AICFFactionAIControlled $ExpectedMode $faction) {
            $expectedAuthority = 'AI'
        }
        $factionAuthority = @($authorityEvents | Where-Object {
            (Test-AICFExpectedField $_.Line 'faction' $faction) -and
            (Test-AICFExpectedField $_.Line 'authority' $expectedAuthority) -and
            (Test-AICFExpectedField $_.Line 'mode' $ExpectedMode)
        })
        if ($factionAuthority.Count -ne 1) {
            $failures.Add(
                "Expected one COMMAND_AUTHORITY_SET faction=$faction " +
                "authority=$expectedAuthority mode=$ExpectedMode, " +
                "found $($factionAuthority.Count)")
        }
    }

    $rosterReadyEvents = @(Get-AICFEventMatches $serverLines 'ROSTER_READY')
    $rosterReadyLineNumber = -1
    if ($rosterReadyEvents.Count -eq 1) {
        $rosterReadyLineNumber = $rosterReadyEvents[0].LineNumber
    }
    elseif ($RequireInitialCoverage) {
        $failures.Add(
            "Expected exactly one ROSTER_READY for initial coverage, " +
            "found $($rosterReadyEvents.Count)")
    }

    $assignments = @(Get-AICFEventMatches $serverLines 'STRATEGIC_ASSIGNMENT')
    $assignmentCoverage = [System.Collections.Generic.Dictionary[string, bool]]::new(
        [System.StringComparer]::Ordinal)
    $initialAssignmentCoverage = [System.Collections.Generic.Dictionary[string, bool]]::new(
        [System.StringComparer]::Ordinal)
    $systemHoldAssignmentLines = [System.Collections.Generic.Dictionary[string, int]]::new(
        [System.StringComparer]::Ordinal)
    foreach ($assignment in $assignments) {
        $faction = Get-AICFFieldValue $assignment.Line 'faction'
        $stableSlot = Get-AICFFieldValue $assignment.Line 'stable_slot'
        $numericSlot = Get-AICFFieldValue $assignment.Line 'numeric_slot'
        $decisionAuthority = Get-AICFFieldValue $assignment.Line 'decision_authority'
        $reason = Get-AICFFieldValue $assignment.Line 'reason'
        $groupGeneration = Get-AICFFieldValue $assignment.Line 'group_generation'
        $assignmentRevision = Get-AICFFieldValue $assignment.Line 'assignment_revision'
        if (@('US', 'USSR') -cnotcontains $faction) {
            $failures.Add(
                "STRATEGIC_ASSIGNMENT lacks a valid faction: $($assignment.LineNumber)")
            continue
        }
        if ($numericSlot -cnotmatch '^[0-9]$' -or
            $stableSlot -cnotmatch '^S([0-9])$' -or
            $stableSlot -cne "S$numericSlot") {
            $failures.Add(
                "STRATEGIC_ASSIGNMENT lacks matching stable/numeric identity: " +
                "$($assignment.LineNumber)")
            continue
        }
        if (@('AI_COMMANDER', 'PLAYER_COMMAND', 'SYSTEM_HOLD') -cnotcontains
            $decisionAuthority) {
            $failures.Add(
                "STRATEGIC_ASSIGNMENT has invalid decision authority: " +
                "$($assignment.LineNumber)")
            continue
        }

        $aiControlled = Test-AICFFactionAIControlled $ExpectedMode $faction
        if (-not $aiControlled -and $decisionAuthority -ceq 'AI_COMMANDER') {
            $failures.Add(
                "Hidden AI commander assignment on player-commanded faction: " +
                "faction=$faction stable_slot=$stableSlot line=$($assignment.LineNumber)")
        }
        if ($aiControlled -and $decisionAuthority -ceq 'SYSTEM_HOLD') {
            $failures.Add(
                "SYSTEM_HOLD assignment on AI-controlled faction: " +
                "faction=$faction stable_slot=$stableSlot line=$($assignment.LineNumber)")
        }

        $coverageKey = "$faction`:$stableSlot`:$decisionAuthority"
        $assignmentCoverage[$coverageKey] = $true
        $identityKey = "$faction`:$stableSlot`:$groupGeneration`:$assignmentRevision"
        if ($decisionAuthority -ceq 'SYSTEM_HOLD') {
            $systemHoldAssignmentLines[$identityKey] = $assignment.LineNumber
        }
        if ($reason -ceq 'INITIAL_DEPLOYMENT' -and
            $groupGeneration -ceq '1' -and
            $assignmentRevision -ceq '1' -and
            $rosterReadyLineNumber -gt 0 -and
            $assignment.LineNumber -lt $rosterReadyLineNumber) {
            $initialAssignmentCoverage[$coverageKey] = $true
        }
    }

    $commandWaitingEvents = @(Get-AICFEventMatches $serverLines 'COMMAND_WAITING')
    $waitingCoverage = [System.Collections.Generic.Dictionary[string, bool]]::new(
        [System.StringComparer]::Ordinal)
    $initialWaitingCoverage = [System.Collections.Generic.Dictionary[string, bool]]::new(
        [System.StringComparer]::Ordinal)
    foreach ($commandWaiting in $commandWaitingEvents) {
        $faction = Get-AICFFieldValue $commandWaiting.Line 'faction'
        $slotKey = Get-AICFFieldValue $commandWaiting.Line 'slot'
        $stableSlot = Get-AICFFieldValue $commandWaiting.Line 'stable_slot'
        $numericSlot = Get-AICFFieldValue $commandWaiting.Line 'numeric_slot'
        $reason = Get-AICFFieldValue $commandWaiting.Line 'reason'
        $groupGeneration = Get-AICFFieldValue `
            $commandWaiting.Line `
            'group_generation'
        $assignmentRevision = Get-AICFFieldValue `
            $commandWaiting.Line `
            'assignment_revision'
        $decisionAuthority = Get-AICFFieldValue `
            $commandWaiting.Line `
            'decision_authority'
        if (@('US', 'USSR') -cnotcontains $faction -or
            [string]::IsNullOrEmpty($slotKey) -or
            $numericSlot -cnotmatch '^[0-9]$' -or
            $stableSlot -cnotmatch '^S([0-9])$' -or
            $stableSlot -cne "S$numericSlot" -or
            $reason -cne 'NO_PLAYER_ORDER' -or
            $groupGeneration -cnotmatch '^\d+$' -or
            $assignmentRevision -cnotmatch '^\d+$' -or
            $decisionAuthority -cne 'SYSTEM_HOLD') {
            $failures.Add(
                "COMMAND_WAITING lacks exact identity/authority: " +
                "$($commandWaiting.LineNumber)")
            continue
        }
        if (Test-AICFFactionAIControlled $ExpectedMode $faction) {
            $failures.Add(
                "COMMAND_WAITING emitted for AI-controlled faction: " +
                "faction=$faction stable_slot=$stableSlot line=$($commandWaiting.LineNumber)")
        }
        $identityKey = "$faction`:$stableSlot`:$groupGeneration`:$assignmentRevision"
        if (-not $systemHoldAssignmentLines.ContainsKey($identityKey) -or
            $systemHoldAssignmentLines[$identityKey] -ge $commandWaiting.LineNumber) {
            $failures.Add(
                "COMMAND_WAITING has no earlier matching SYSTEM_HOLD assignment revision: " +
                "faction=$faction stable_slot=$stableSlot line=$($commandWaiting.LineNumber)")
        }
        $waitingCoverage["$faction`:$stableSlot"] = $true
        if ($groupGeneration -ceq '1' -and $assignmentRevision -ceq '1' -and
            $rosterReadyLineNumber -gt 0 -and
            $commandWaiting.LineNumber -lt $rosterReadyLineNumber -and
            $systemHoldAssignmentLines.ContainsKey($identityKey) -and
            $systemHoldAssignmentLines[$identityKey] -lt $commandWaiting.LineNumber) {
            $initialWaitingCoverage["$faction`:$stableSlot"] = $true
        }
    }

    if ($RequireInitialCoverage) {
        foreach ($faction in @('US', 'USSR')) {
            $aiControlled = Test-AICFFactionAIControlled $ExpectedMode $faction
            $requiredAuthority = 'SYSTEM_HOLD'
            if ($aiControlled) {
                $requiredAuthority = 'AI_COMMANDER'
            }
            for ($slotId = 0; $slotId -lt 10; $slotId++) {
                $stableSlot = "S$slotId"
                if (-not $initialAssignmentCoverage.ContainsKey(
                    "$faction`:$stableSlot`:$requiredAuthority")) {
                    $failures.Add(
                        "Initial authority coverage missing: faction=$faction " +
                        "stable_slot=$stableSlot authority=$requiredAuthority")
                }
                if (-not $aiControlled -and
                    -not $initialWaitingCoverage.ContainsKey("$faction`:$stableSlot")) {
                    $failures.Add(
                        "Initial COMMAND_WAITING coverage missing: " +
                        "faction=$faction stable_slot=$stableSlot")
                }
            }
        }
    }

    $clientReplicationCount = 0
    if ($hasClientLog) {
        Add-AICFUnexpectedRuntimeFailures -Lines $clientLines -Source 'Client'
        $replicatedEvents = @(Get-AICFEventMatches `
            $clientLines `
            'COMMAND_AUTHORITY_REPLICATED')
        $clientReplicationCount = $replicatedEvents.Count
        if (-not $replicatedEvents) {
            $failures.Add('Client log lacks COMMAND_AUTHORITY_REPLICATED JIP evidence')
        }
        else {
            $lastReplicatedLine = $replicatedEvents[-1].Line
            if (-not (Test-AICFExpectedField `
                    $lastReplicatedLine `
                    'ai_commander_us' `
                    $expectedUSFlag) -or
                -not (Test-AICFExpectedField `
                    $lastReplicatedLine `
                    'ai_commander_ussr' `
                    $expectedUSSRFlag)) {
                $failures.Add(
                    "Last client COMMAND_AUTHORITY_REPLICATED does not match " +
                    "mode=$ExpectedMode us=$expectedUSFlag ussr=$expectedUSSRFlag")
            }

            $clientConnections = @($clientLines | Select-String -Pattern (
                'ClientImpl event:\s+connected'))
            $connectionBeforeSnapshot = @($clientConnections | Where-Object {
                $_.LineNumber -lt $replicatedEvents[-1].LineNumber
            })
            if (-not $connectionBeforeSnapshot) {
                $failures.Add(
                    'Client authority snapshot is not preceded by a client connection marker')
            }

            $lateServerConnections = @()
            if ($rosterReadyLineNumber -gt 0) {
                $lateServerConnections = @($serverLines | Select-String -Pattern (
                    'ServerImpl event:\s+connected|Player connected:\s+connectionID=') |
                    Where-Object { $_.LineNumber -gt $rosterReadyLineNumber })
            }
            if (-not $lateServerConnections) {
                $failures.Add(
                    'Server log lacks a client connection after ROSTER_READY; JIP is unproven')
            }
        }
    }
}

if ($failures.Count -gt 0) {
    Write-Host "AI commander mode log audit: FAIL ($($failures.Count) issue(s))" `
        -ForegroundColor Red
    $failures | ForEach-Object { Write-Host " - $_" -ForegroundColor Red }
    exit 1
}

Write-Host 'AI commander mode log audit: PASS' -ForegroundColor Green
if ($isInvalidRun) {
    Write-Host "invalid_value=$ExpectedInvalidValue early_rejection=PASS"
    exit 0
}

$clientVerdict = 'NOT_RUN'
$jipVerdict = 'NOT_RUN'
if ($hasClientLog) {
    $clientVerdict = "PASS($clientReplicationCount event(s))"
    $jipVerdict = 'PASS'
}
Write-Host (
    "mode=$ExpectedMode authority_events=$($authorityEvents.Count) " +
    "assignments=$($assignments.Count) command_waiting=$($commandWaitingEvents.Count) " +
    "initial_coverage=$($RequireInitialCoverage.IsPresent) " +
    "client_replication=$clientVerdict jip=$jipVerdict")
