param(
    [string]$RepositoryRoot = (Split-Path -Parent $PSScriptRoot)
)

$ErrorActionPreference = 'Stop'
$failures = [System.Collections.Generic.List[string]]::new()
$sourceCache = @{}
$missingFiles = @{}
$methodCache = @{}

function Get-RepositorySource {
    param([string]$RelativePath)

    if ($sourceCache.ContainsKey($RelativePath)) {
        return $sourceCache[$RelativePath]
    }

    $path = Join-Path $RepositoryRoot $RelativePath
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
        if (-not $missingFiles.ContainsKey($RelativePath)) {
            $missingFiles[$RelativePath] = $true
            $failures.Add("Missing file: $RelativePath")
        }
        $sourceCache[$RelativePath] = ''
        return ''
    }

    $sourceCache[$RelativePath] = Get-Content -LiteralPath $path -Raw
    return $sourceCache[$RelativePath]
}

function Assert-FileContains {
    param(
        [string]$RelativePath,
        [string]$Pattern,
        [string]$Description
    )

    $source = Get-RepositorySource $RelativePath
    if (-not $source -or $source -notmatch $Pattern) {
        $failures.Add("$Description ($RelativePath)")
    }
}

function Assert-FileNotContains {
    param(
        [string]$RelativePath,
        [string]$Pattern,
        [string]$Description
    )

    $source = Get-RepositorySource $RelativePath
    if ($source -and $source -match $Pattern) {
        $failures.Add("$Description ($RelativePath)")
    }
}

function Assert-CombinedContains {
    param(
        [string[]]$RelativePaths,
        [string]$Pattern,
        [string]$Description
    )

    $parts = foreach ($relativePath in $RelativePaths) {
        Get-RepositorySource $relativePath
    }
    $combined = $parts -join "`n"
    if ($combined -notmatch $Pattern) {
        $failures.Add($Description)
    }
}

function Get-EnforceMethodBody {
    param(
        [string]$RelativePath,
        [string]$MethodName
    )

    $cacheKey = "$RelativePath::$MethodName"
    if ($methodCache.ContainsKey($cacheKey)) {
        return $methodCache[$cacheKey]
    }

    $source = Get-RepositorySource $RelativePath
    if (-not $source) {
        $methodCache[$cacheKey] = ''
        return ''
    }

    $escapedName = [regex]::Escape($MethodName)
    $signaturePattern = "(?m)^\s*(?!return\b)(?:(?:override|protected|private|static)\s+)*[A-Za-z_][A-Za-z0-9_<>.,]*\s+$escapedName\s*\("
    $matches = [regex]::Matches($source, $signaturePattern)
    if ($matches.Count -ne 1) {
        $failures.Add("Method $MethodName must have one unique definition; found $($matches.Count) ($RelativePath)")
        $methodCache[$cacheKey] = ''
        return ''
    }

    $openBrace = $source.IndexOf('{', $matches[0].Index + $matches[0].Length)
    if ($openBrace -lt 0) {
        $failures.Add("Method $MethodName has no body ($RelativePath)")
        $methodCache[$cacheKey] = ''
        return ''
    }

    $depth = 0
    $inString = $false
    $inLineComment = $false
    $inBlockComment = $false
    for ($i = $openBrace; $i -lt $source.Length; $i++) {
        $character = $source[$i]
        $nextCharacter = [char]0
        if ($i + 1 -lt $source.Length) {
            $nextCharacter = $source[$i + 1]
        }

        if ($inLineComment) {
            if ($character -eq "`n") {
                $inLineComment = $false
            }
            continue
        }
        if ($inBlockComment) {
            if ($character -eq '*' -and $nextCharacter -eq '/') {
                $inBlockComment = $false
                $i++
            }
            continue
        }
        if ($inString) {
            if ($character -eq '\') {
                $i++
                continue
            }
            if ($character -eq '"') {
                $inString = $false
            }
            continue
        }

        if ($character -eq '/' -and $nextCharacter -eq '/') {
            $inLineComment = $true
            $i++
            continue
        }
        if ($character -eq '/' -and $nextCharacter -eq '*') {
            $inBlockComment = $true
            $i++
            continue
        }
        if ($character -eq '"') {
            $inString = $true
            continue
        }
        if ($character -eq '{') {
            $depth++
            continue
        }
        if ($character -eq '}') {
            $depth--
            if ($depth -eq 0) {
                $body = $source.Substring($openBrace, $i - $openBrace + 1)
                $methodCache[$cacheKey] = $body
                return $body
            }
        }
    }

    $failures.Add("Method $MethodName has unbalanced braces ($RelativePath)")
    $methodCache[$cacheKey] = ''
    return ''
}

function Assert-MethodContains {
    param(
        [string]$RelativePath,
        [string]$MethodName,
        [string]$Pattern,
        [string]$Description
    )

    $body = Get-EnforceMethodBody $RelativePath $MethodName
    if ($body -and $body -notmatch $Pattern) {
        $failures.Add("$Description ($RelativePath::$MethodName)")
    }
}

function Assert-MethodNotContains {
    param(
        [string]$RelativePath,
        [string]$MethodName,
        [string]$Pattern,
        [string]$Description
    )

    $body = Get-EnforceMethodBody $RelativePath $MethodName
    if ($body -and $body -match $Pattern) {
        $failures.Add("$Description ($RelativePath::$MethodName)")
    }
}

function Test-EnforceIdentifierCharacter {
    param([char]$Character)

    return [char]::IsLetterOrDigit($Character) -or $Character -eq '_'
}

function Get-SourceLineNumber {
    param(
        [string]$Source,
        [int]$Offset
    )

    if ($Offset -le 0) {
        return 1
    }

    return 1 + [regex]::Matches($Source.Substring(0, $Offset), "`n").Count
}

function Get-DirectFormatLiteral {
    param(
        [string]$Source,
        [int]$OpenParenOffset
    )

    $offset = $OpenParenOffset + 1
    while ($offset -lt $Source.Length) {
        $character = $Source[$offset]
        $nextCharacter = [char]0
        if ($offset + 1 -lt $Source.Length) {
            $nextCharacter = $Source[$offset + 1]
        }

        if ([char]::IsWhiteSpace($character)) {
            $offset++
            continue
        }
        if ($character -eq '/' -and $nextCharacter -eq '/') {
            $offset += 2
            while ($offset -lt $Source.Length -and $Source[$offset] -ne "`n") {
                $offset++
            }
            continue
        }
        if ($character -eq '/' -and $nextCharacter -eq '*') {
            $offset += 2
            while ($offset + 1 -lt $Source.Length) {
                if ($Source[$offset] -eq '*' -and $Source[$offset + 1] -eq '/') {
                    $offset += 2
                    break
                }
                $offset++
            }
            continue
        }
        break
    }

    if ($offset -ge $Source.Length -or $Source[$offset] -ne '"') {
        return ''
    }

    $literal = [System.Text.StringBuilder]::new()
    for ($offset = $offset + 1; $offset -lt $Source.Length; $offset++) {
        $character = $Source[$offset]
        if ($character -eq '\') {
            [void]$literal.Append($character)
            if ($offset + 1 -lt $Source.Length) {
                $offset++
                [void]$literal.Append($Source[$offset])
            }
            continue
        }
        if ($character -eq '"') {
            return $literal.ToString()
        }
        [void]$literal.Append($character)
    }

    return ''
}

function Get-StringFormatCallInfo {
    param(
        [string]$Source,
        [int]$OpenParenOffset
    )

    $depth = 1
    $argumentCount = 1
    $hasArgumentContent = $false
    $inString = $false
    $inLineComment = $false
    $inBlockComment = $false

    for ($offset = $OpenParenOffset + 1; $offset -lt $Source.Length; $offset++) {
        $character = $Source[$offset]
        $nextCharacter = [char]0
        if ($offset + 1 -lt $Source.Length) {
            $nextCharacter = $Source[$offset + 1]
        }

        if ($inLineComment) {
            if ($character -eq "`n") {
                $inLineComment = $false
            }
            continue
        }
        if ($inBlockComment) {
            if ($character -eq '*' -and $nextCharacter -eq '/') {
                $inBlockComment = $false
                $offset++
            }
            continue
        }
        if ($inString) {
            if ($character -eq '\') {
                $offset++
                continue
            }
            if ($character -eq '"') {
                $inString = $false
            }
            continue
        }

        if ($character -eq '/' -and $nextCharacter -eq '/') {
            $inLineComment = $true
            $offset++
            continue
        }
        if ($character -eq '/' -and $nextCharacter -eq '*') {
            $inBlockComment = $true
            $offset++
            continue
        }
        if ($character -eq '"') {
            $inString = $true
            $hasArgumentContent = $true
            continue
        }
        if ($character -eq '(' -or $character -eq '[' -or $character -eq '{') {
            $depth++
            $hasArgumentContent = $true
            continue
        }
        if ($character -eq ')' -or $character -eq ']' -or $character -eq '}') {
            $depth--
            if ($depth -eq 0) {
                if (-not $hasArgumentContent) {
                    $argumentCount = 0
                }
                return [pscustomobject]@{
                    Closed = $true
                    ArgumentCount = $argumentCount
                    Literal = (Get-DirectFormatLiteral $Source $OpenParenOffset)
                }
            }
            continue
        }
        if ($character -eq ',' -and $depth -eq 1) {
            $argumentCount++
            continue
        }
        if (-not [char]::IsWhiteSpace($character)) {
            $hasArgumentContent = $true
        }
    }

    return [pscustomobject]@{
        Closed = $false
        ArgumentCount = $argumentCount
        Literal = (Get-DirectFormatLiteral $Source $OpenParenOffset)
    }
}

function Assert-StringFormatContracts {
    param([string[]]$ProductionRoots)

    $repositoryPath = (Resolve-Path -LiteralPath $RepositoryRoot).Path.TrimEnd([char[]]"\/")
    $formatToken = 'string.Format'

    foreach ($productionRoot in $ProductionRoots) {
        $productionPath = Join-Path $repositoryPath $productionRoot
        if (-not (Test-Path -LiteralPath $productionPath -PathType Container)) {
            $failures.Add("Missing production root for string.Format audit: $productionRoot")
            continue
        }

        foreach ($file in (Get-ChildItem -LiteralPath $productionPath -Recurse -Filter '*.c' -File)) {
            $source = Get-Content -LiteralPath $file.FullName -Raw
            if (-not $source) {
                continue
            }

            $relativePath = $file.FullName.Substring($repositoryPath.Length).TrimStart([char[]]"\/").Replace('\', '/')
            $inString = $false
            $inLineComment = $false
            $inBlockComment = $false

            for ($offset = 0; $offset -lt $source.Length; $offset++) {
                $character = $source[$offset]
                $nextCharacter = [char]0
                if ($offset + 1 -lt $source.Length) {
                    $nextCharacter = $source[$offset + 1]
                }

                if ($inLineComment) {
                    if ($character -eq "`n") {
                        $inLineComment = $false
                    }
                    continue
                }
                if ($inBlockComment) {
                    if ($character -eq '*' -and $nextCharacter -eq '/') {
                        $inBlockComment = $false
                        $offset++
                    }
                    continue
                }
                if ($inString) {
                    if ($character -eq '\') {
                        $offset++
                        continue
                    }
                    if ($character -eq '"') {
                        $inString = $false
                    }
                    continue
                }

                if ($character -eq '/' -and $nextCharacter -eq '/') {
                    $inLineComment = $true
                    $offset++
                    continue
                }
                if ($character -eq '/' -and $nextCharacter -eq '*') {
                    $inBlockComment = $true
                    $offset++
                    continue
                }
                if ($character -eq '"') {
                    $inString = $true
                    continue
                }

                if ($offset + $formatToken.Length -gt $source.Length) {
                    continue
                }
                if ($source.Substring($offset, $formatToken.Length) -cne $formatToken) {
                    continue
                }

                $previousCharacter = [char]0
                if ($offset -gt 0) {
                    $previousCharacter = $source[$offset - 1]
                }
                $afterTokenCharacter = [char]0
                if ($offset + $formatToken.Length -lt $source.Length) {
                    $afterTokenCharacter = $source[$offset + $formatToken.Length]
                }
                if ((Test-EnforceIdentifierCharacter $previousCharacter) -or (Test-EnforceIdentifierCharacter $afterTokenCharacter)) {
                    continue
                }

                $openParenOffset = $offset + $formatToken.Length
                while ($openParenOffset -lt $source.Length -and [char]::IsWhiteSpace($source[$openParenOffset])) {
                    $openParenOffset++
                }
                if ($openParenOffset -ge $source.Length -or $source[$openParenOffset] -ne '(') {
                    continue
                }

                $lineNumber = Get-SourceLineNumber $source $offset
                $callInfo = Get-StringFormatCallInfo $source $openParenOffset
                if (-not $callInfo.Closed) {
                    $failures.Add("Unclosed string.Format call ($relativePath`:$lineNumber)")
                    continue
                }

                $substitutionCount = [Math]::Max(0, $callInfo.ArgumentCount - 1)
                if ($substitutionCount -gt 9) {
                    $failures.Add("string.Format has $substitutionCount substitutions; Enforce supports at most 9 ($relativePath`:$lineNumber)")
                }

                $invalidPlaceholders = [System.Collections.Generic.HashSet[int]]::new()
                foreach ($placeholderMatch in [regex]::Matches($callInfo.Literal, '%([0-9]{2,})')) {
                    $placeholder = 0
                    if ([int]::TryParse($placeholderMatch.Groups[1].Value, [ref]$placeholder) -and $placeholder -gt 9) {
                        [void]$invalidPlaceholders.Add($placeholder)
                    }
                }
                if ($invalidPlaceholders.Count -gt 0) {
                    $placeholderList = ($invalidPlaceholders | Sort-Object) -join ', '
                    $failures.Add("string.Format literal uses unsupported placeholder(s) $placeholderList; maximum is %9 ($relativePath`:$lineNumber)")
                }
            }
        }
    }
}

function Assert-NoCStyleTernary {
    param([string[]]$ProductionRoots)

    $repositoryPath = (Resolve-Path -LiteralPath $RepositoryRoot).Path.TrimEnd([char[]]"\/")

    foreach ($productionRoot in $ProductionRoots) {
        $productionPath = Join-Path $repositoryPath $productionRoot
        if (-not (Test-Path -LiteralPath $productionPath -PathType Container)) {
            continue
        }

        foreach ($file in (Get-ChildItem -LiteralPath $productionPath -Recurse -Filter '*.c' -File)) {
            $source = Get-Content -LiteralPath $file.FullName -Raw
            if (-not $source) {
                continue
            }

            $relativePath = $file.FullName.Substring($repositoryPath.Length).TrimStart([char[]]"\/").Replace('\', '/')
            $inString = $false
            $inLineComment = $false
            $inBlockComment = $false

            for ($offset = 0; $offset -lt $source.Length; $offset++) {
                $character = $source[$offset]
                $nextCharacter = [char]0
                if ($offset + 1 -lt $source.Length) {
                    $nextCharacter = $source[$offset + 1]
                }

                if ($inLineComment) {
                    if ($character -eq "`n") {
                        $inLineComment = $false
                    }
                    continue
                }
                if ($inBlockComment) {
                    if ($character -eq '*' -and $nextCharacter -eq '/') {
                        $inBlockComment = $false
                        $offset++
                    }
                    continue
                }
                if ($inString) {
                    if ($character -eq '\') {
                        $offset++
                        continue
                    }
                    if ($character -eq '"') {
                        $inString = $false
                    }
                    continue
                }

                if ($character -eq '/' -and $nextCharacter -eq '/') {
                    $inLineComment = $true
                    $offset++
                    continue
                }
                if ($character -eq '/' -and $nextCharacter -eq '*') {
                    $inBlockComment = $true
                    $offset++
                    continue
                }
                if ($character -eq '"') {
                    $inString = $true
                    continue
                }
                if ($character -ne '?') {
                    continue
                }

                $lineNumber = Get-SourceLineNumber $source $offset
                $failures.Add("C-style ternary token '?' is not supported in production Enforce code ($relativePath`:$lineNumber)")
            }
        }
    }
}

$stage1Config = 'AIConflictCore/Scripts/Game/AIConflict/Config/AICF_Stage1Config.c'
$stage3Config = 'AIConflictCore/Scripts/Game/AIConflict/Config/AICF_Stage3Config.c'
$stage3Enums = 'AIConflictCore/Scripts/Game/AIConflict/State/AICF_Stage3Enums.c'
$factionState = 'AIConflictCore/Scripts/Game/AIConflict/State/AICF_FactionState.c'
$groupSlot = 'AIConflictCore/Scripts/Game/AIConflict/State/AICF_GroupSlot.c'
$groupRuntime = 'AIConflictCore/Scripts/Game/AIConflict/State/AICF_GroupRuntime.c'
$groupSpawner = 'AIConflictCore/Scripts/Game/AIConflict/Forces/AICF_GroupSpawner.c'
$matchController = 'AIConflictCore/Scripts/Game/AIConflict/Bootstrap/AICF_MatchController.c'
$stage35Diagnostics = 'AIConflictCore/Scripts/Game/AIConflict/Diagnostics/AICF_Stage35Diagnostics.c'
$targetSelector = 'AIConflictCore/Scripts/Game/AIConflict/Objectives/AICF_TargetSelector.c'
$orderPlanner = 'AIConflictCore/Scripts/Game/AIConflict/Orders/AICF_OrderPlanner.c'
$vehicleCatalog = 'AIConflictCore/Scripts/Game/AIConflict/Vehicles/AICF_VehicleCatalog.c'
$vehicleSpawner = 'AIConflictCore/Scripts/Game/AIConflict/Vehicles/AICF_VehicleSpawner.c'
$vehicleCoordinator = 'AIConflictCore/Scripts/Game/AIConflict/Vehicles/AICF_VehicleCoordinator.c'
$vehicleRuntime = 'AIConflictCore/Scripts/Game/AIConflict/State/AICF_VehicleRuntime.c'
$vehiclePassengerBoarding = 'AIConflictCore/Scripts/Game/AIConflict/Vehicles/AICF_VehiclePassengerBoarding.c'
$vehicleWatchdog = 'AIConflictCore/Scripts/Game/AIConflict/Vehicles/AICF_VehicleWatchdog.c'
$groupMarkers = 'AIConflictCore/Scripts/Game/AIConflict/UI/AICF_GroupMapMarkers.c'

# Force structure and load budget.
Assert-FileContains $stage1Config '\bMANAGED_GROUP_SIZE\s*=\s*5\s*;' 'Managed groups must have exactly five members'
Assert-FileContains $stage1Config '\bGROUP_SLOTS_PER_FACTION\s*=\s*4\s*;' 'Stage 3.5 must retain four stable slots per faction'
Assert-FileContains $stage1Config '\bATTACK_SLOTS_PER_FACTION\s*=\s*3\s*;' 'Each faction must expose three ATTACK slots'
Assert-FileContains $stage1Config '\bDEFEND_SLOTS_PER_FACTION\s*=\s*1\s*;' 'Each faction must expose one DEFEND/QRF slot'
Assert-FileContains $stage1Config '\bRESERVE_SLOTS_PER_FACTION\s*=\s*0\s*;' 'Stage 3.5 must not keep an idle reserve slot'
Assert-FileContains $stage1Config '\bMIN_MANAGED_AGENTS\s*=\s*48\s*;' 'The managed-agent CLI floor must cover 40 live agents plus a conservative pending projection'
Assert-FileContains $stage1Config '\bDEFAULT_MAX_MANAGED_AGENTS\s*=\s*64\s*;' 'The standard managed-agent budget must retain the 64-agent margin'
Assert-FileContains $stage1Config 'System\.GetCLIParam\s*\(\s*"aicfActiveForcesRolesEnabled"\s*,' 'The active-force role policy must have an explicit CLI switch'
Assert-FileContains $stage1Config '(?:Get|Is)ActiveForcesRolesEnabled\s*\(' 'The active-force CLI switch must be exposed to runtime planning'
Assert-CombinedContains @($factionState, $matchController, $targetSelector, $orderPlanner) '(?:Get|Is)ActiveForcesRolesEnabled\s*\(' 'The active-force CLI switch must be consumed outside its config class'
Assert-MethodContains $factionState 'BuildDefaultSlots' 'activeForcesRolesEnabled[\s\S]*LEGACY_ATTACK_SLOTS_PER_FACTION[\s\S]*LEGACY_DEFEND_SLOTS_PER_FACTION' 'The CLI switch must select the legacy role boundaries for the infantry baseline'
Assert-MethodContains $matchController 'CountProjectedFactionAgents' 'PENDING_GROUP_AGENT_BUDGET' 'Managed-agent projection must reserve the conservative pending-group budget'

# Dedicated diagnostics are part of the acceptance contract, not optional aliases.
Assert-FileContains $stage35Diagnostics 'PREFIX\s*=\s*"\[AICF\]\[STAGE3\.5\]"' 'Stage 3.5 diagnostics must expose the documented log prefix'
Assert-FileContains $stage35Diagnostics 'RecordExternalError\s*\(\s*"STAGE3\.5"' 'Stage 3.5 errors must invalidate the shared Stage 1 result'
Assert-MethodContains $matchController 'Start' 'AICF_Stage35Diagnostics\.Configure\s*\(' 'Match startup must reset the Stage 3.5 diagnostic error latch'
Assert-MethodContains $matchController 'FinalizeResult' 'AICF_Stage35Diagnostics\.HasErrors\s*\(' 'Stage 3.5 errors must invalidate the final shared result candidate'
foreach ($eventName in @('CONFIG', 'GROUP_ENTITY_SPAWNED', 'GROUP_ROSTER_READY', 'STRATEGIC_ASSIGNMENT', 'STRATEGIC_CANDIDATE_HELD', 'DEFEND_POSTURE_CHANGED', 'VEHICLE_CAPACITY_PREFLIGHT', 'VEHICLE_TRANSPORT_FALLBACK', 'VEHICLE_REQUEST_INELIGIBLE', 'VEHICLE_SPAWN_CANDIDATE_REJECTED', 'FORCE_HEARTBEAT', 'SLOT_ACTIVITY', 'MEANINGFUL_TASK_LOST', 'MEANINGFUL_TASK_RECOVERED', 'ORDER_RESTORE_REQUESTED', 'ORDER_RESTORE_RESULT', 'WAYPOINT_REMOVED', 'WAYPOINT_BIND_MISMATCH', 'ABANDONED_EXIT_AUDIT', 'IDLE_DEADLINE_SUPPRESSED', 'FORCE_DISEMBARK_MEMBER', 'COHESION_OUTCOME', 'WAITING_FOR_SITE_EXIT')) {
    Assert-CombinedContains @($groupSpawner, $matchController, $orderPlanner, $vehicleCoordinator, $vehicleSpawner) ('"' + [regex]::Escape($eventName) + '"') "Missing Stage 3.5 diagnostic event: $eventName"
}
foreach ($errorEventName in @('GROUP_ROSTER_CONFIG_INVALID', 'GROUP_ROSTER_REJECTED', 'MOB_IDLE_DEADLINE_MISSED', 'MEANINGFUL_TASK_DEADLINE_MISSED')) {
    Assert-CombinedContains @($groupSpawner, $matchController) ('"' + [regex]::Escape($errorEventName) + '"') "Missing Stage 3.5 error event: $errorEventName"
}
Assert-MethodContains $groupSpawner 'SpawnGroup' 'AICF_Stage35Diagnostics\.Error\s*\(\s*"GROUP_ROSTER_CONFIG_INVALID"' 'Roster configuration rejection must use Stage 3.5 ERROR severity'
Assert-MethodContains $matchController 'HandleInvalidRoster' 'AICF_Stage35Diagnostics\.Error\s*\(\s*"GROUP_ROSTER_REJECTED"' 'Authoritative roster rejection must use Stage 3.5 ERROR severity'
Assert-MethodContains $matchController 'AuditActiveFactionTasking' 'AICF_Stage35Diagnostics\.Error\s*\(\s*"MEANINGFUL_TASK_DEADLINE_MISSED"[\s\S]*AICF_Stage35Diagnostics\.Error\s*\(\s*"MOB_IDLE_DEADLINE_MISSED"' 'Both active-force task deadlines must use Stage 3.5 ERROR severity'

# Exact five-member initial and replacement deployment.
Assert-FileContains $groupSpawner 'SCR_AIGroup\s*\.\s*IgnoreSpawning\s*\(' 'Roster shaping must use the stock SCR_AIGroup spawn guard'
Assert-FileContains $groupSpawner '\bm_aUnitPrefabSlots\b' 'Roster shaping must operate on the group prefab unit slots'
Assert-FileContains $groupSpawner 'AICF_Stage1Config\.MANAGED_GROUP_SIZE' 'GroupSpawner must derive its target size from MANAGED_GROUP_SIZE'
Assert-MethodContains $groupSpawner 'SpawnGroup' 'SpawnUnits\s*\(' 'The shaped faction-correct roster must still use normal unit spawning'
Assert-MethodContains $groupSpawner 'SpawnGroup' 'IgnoreSpawning\s*\(\s*true\s*\)[\s\S]*SpawnEntityPrefabEx[\s\S]*IgnoreSpawning\s*\(\s*false\s*\)[\s\S]*ConfigureManagedRoster[\s\S]*SpawnUnits\s*\(' 'Roster shaping must be ordered across the one-shot spawn guard before normal member spawning'
Assert-MethodContains $groupSpawner 'SpawnGroup' 'GROUP_ENTITY_SPAWNED[\s\S]*expected_agents=%6[\s\S]*AICF_Stage1Config\.MANAGED_GROUP_SIZE' 'Pending group diagnostics must bind expected_agents to the configured five-member roster'
Assert-MethodContains $matchController 'CompleteReadyDeployment' 'GROUP_SPAWNED[\s\S]*initial_agents=%6[\s\S]*GetAgentsCount\s*\(' 'Authoritative GROUP_SPAWNED diagnostics must report the settled roster count at READY'
Assert-FileContains $matchController 'GetAgentsCount\s*\(\s*\)\s*==\s*AICF_Stage1Config\.MANAGED_GROUP_SIZE' 'A slot may become ready only at the exact five-member gate'
Assert-MethodContains $matchController 'ProcessFaction' 'GetAgentsCount\s*\(\s*\)\s*==\s*AICF_Stage1Config\.MANAGED_GROUP_SIZE[\s\S]*GetSpawnQueueSize\s*\(\s*\)\s*==\s*0[\s\S]*HasExactFactionRoster[\s\S]*slot\.MarkReady' 'Readiness must wait for the settled exact roster and faction validation before READY'
Assert-MethodContains $groupRuntime 'HasExactFactionRoster' 'GetAgents[\s\S]*IsAliveCharacter[\s\S]*FactionAffiliationComponent[\s\S]*GetAffiliatedFaction\s*\(\s*\)[\s\S]*actualCount\s*==\s*expectedCount[\s\S]*factionMismatchCount\s*==\s*0[\s\S]*nonAliveCount\s*==\s*0' 'Exact readiness must verify every spawned member is alive and faction-correct as well as count'
Assert-FileNotContains $matchController 'GetAgentsCount\s*\(\s*\)\s*>\s*0\s*&&\s*slot\.MarkReady' 'The legacy non-empty readiness gate must not survive Stage 3.5'
Assert-MethodContains $matchController 'TryLogRosterReady' 'm_USState\.CountSlotsByState\s*\(\s*AICF_EGroupSlotState\.READY\s*\)[\s\S]*m_USSRState\.CountSlotsByState\s*\(\s*AICF_EGroupSlotState\.READY\s*\)' 'ROSTER_READY must require the complete faction rosters'
Assert-MethodContains $matchController 'TryLogRosterReady' 'CountSlotsByRole\s*\(\s*AICF_EGroupRole\.ATTACK\s*\)[\s\S]*CountSlotsByRole\s*\(\s*AICF_EGroupRole\.DEFEND\s*\)[\s\S]*CountSlotsByRole\s*\(\s*AICF_EGroupRole\.RESERVE\s*\)' 'ROSTER_READY must expose the 3/1/0 role split'
Assert-MethodContains $factionState 'BuildDefaultSlots' 'attackSlots\s*=\s*AICF_Stage1Config\.ATTACK_SLOTS_PER_FACTION' 'Faction slot construction must load the active ATTACK boundary'
Assert-MethodContains $factionState 'BuildDefaultSlots' 'defendSlots\s*=\s*AICF_Stage1Config\.DEFEND_SLOTS_PER_FACTION' 'Faction slot construction must load the active DEFEND/QRF count'
Assert-MethodContains $factionState 'BuildDefaultSlots' 'slotId\s*<\s*attackSlots[\s\S]*slotId\s*<\s*attackSlots\s*\+\s*defendSlots' 'Faction slot construction must map the fourth active-force slot to DEFEND/QRF'
Assert-MethodContains $factionState 'BuildDefaultSlots' 'roleIndex' 'Faction slots must retain a role-local ordinal for A0/A1/A2 and D0'

# Four-slot motorization and light-transport fallback.
Assert-FileContains $stage3Config '\bDEFAULT_MAX_VEHICLES_PER_FACTION\s*=\s*4\s*;' 'The active vehicle cap must default to four per faction'
Assert-FileContains $stage3Config '\bDEFAULT_COHESION_WAIT_TIMEOUT_MS\s*=\s*300000\s*;' 'Fragmented-group vehicle-site waiting must have a finite five-minute default deadline'
Assert-FileContains $stage3Config 'System\.GetCLIParam\s*\(\s*"aicfVehicleCohesionWaitTimeoutMs"[\s\S]{0,180}ClampInt\s*\(value\.ToInt\(\), 60000, 1800000\)' 'The cohesion-wait deadline must expose a bounded repeat-test CLI override'
Assert-FileContains $stage3Config 'aicfTransportVehiclesPerFaction"[\s\S]{0,240}AICF_Stage1Config\.GROUP_SLOTS_PER_FACTION' 'Transport CLI capacity must cover every managed slot, not ATTACK slots only'
Assert-FileContains $stage3Config 'aicfMaxVehiclesPerFaction"[\s\S]{0,240}AICF_Stage1Config\.GROUP_SLOTS_PER_FACTION' 'The active vehicle cap must remain clamped to the four stable slots'
Assert-MethodContains $stage3Config 'NormalizeVehicleCounts' 'AICF_Stage1Config\.GROUP_SLOTS_PER_FACTION' 'Vehicle-count normalization must use all managed slots'
Assert-FileContains $stage3Enums '\bLIGHT_TRANSPORT\b' 'Stage 3.5 must distinguish unarmed light transport from trucks and armed light vehicles'
Assert-FileContains $vehicleCatalog 'AICF_EVehicleKind\.LIGHT_TRANSPORT' 'The faction catalog must resolve LIGHT_TRANSPORT explicitly'
Assert-FileContains $vehicleCatalog 'M998[\\/]M998_covered_long\.et' 'The US light-transport candidate must come from the faction catalog'
Assert-FileContains $vehicleCatalog 'UAZ452[\\/]UAZ452_transport\.et' 'The USSR light-transport candidate must come from the faction catalog'
Assert-MethodNotContains $vehicleCoordinator 'CanStartVehicleTrip' 'GetRole\s*\(\s*\)\s*!=\s*AICF_EGroupRole\.ATTACK' 'DEFEND/QRF must not be rejected by the vehicle eligibility gate'
Assert-MethodNotContains $vehicleCoordinator 'TryGetDesiredKind' 'GetRole\s*\(\s*\)\s*!=\s*AICF_EGroupRole\.ATTACK' 'Vehicle kind selection must not be ATTACK-only'
Assert-MethodContains $vehicleCoordinator 'TryGetDesiredKind' 'AICF_Stage1Config\.GROUP_SLOTS_PER_FACTION' 'Vehicle kind selection must bound all four managed slots'
Assert-MethodContains $vehicleCoordinator 'TryGetDesiredKind' 'AICF_EVehicleKind\.TRANSPORT' 'A0/A1 must retain truck transport selection'
Assert-MethodContains $vehicleCoordinator 'TryGetDesiredKind' 'AICF_EVehicleKind\.LIGHT_TRANSPORT' 'A2/D0 must request light transport before capacity fallback'
Assert-MethodContains $vehicleCoordinator 'TryGetDesiredKind' 'GetSlotKey\s*\(\s*\)\s*==\s*"A0"[\s\S]*GetSlotKey\s*\(\s*\)\s*==\s*"A1"[\s\S]*AICF_EVehicleKind\.TRANSPORT[\s\S]*AICF_EVehicleKind\.LIGHT_TRANSPORT' 'Vehicle kind mapping must make A0/A1 trucks and the remaining transport slots light-first'
Assert-FileContains $vehicleCoordinator 'CountReserved\s*\([^\)]*\)\s*>=\s*m_Config\.GetMaxVehiclesPerFaction\s*\(' 'Coordinator admission must enforce the configured active/reserved cap'
Assert-CombinedContains @($vehicleCoordinator, $vehicleSpawner, $vehicleWatchdog) 'CountAccessibleSeats|availableCapacity' 'Every vehicle candidate must expose accessible compartment capacity'
Assert-FileContains $vehicleCoordinator 'availableCapacity\s*<\s*aliveAgents' 'Boarding must reject a vehicle that cannot carry every living member'
Assert-CombinedContains @($vehicleCoordinator, $vehicleCatalog, $vehicleSpawner) '(?:Capacity|Compartment)[A-Za-z0-9_]*(?:Fallback|Retry)|(?:Fallback|Retry)[A-Za-z0-9_]*(?:Truck|Transport)|VEHICLE_(?:CAPACITY_)?(?:CANDIDATE_REJECTED|TRANSPORT_FALLBACK)|INSUFFICIENT_COMPARTMENTS[\s\S]{0,1200}AICF_EVehicleKind\.TRANSPORT' 'An undersized light candidate must retry/fall back to truck transport before bounded infantry fallback'
Assert-MethodContains $vehicleCoordinator 'ProcessRequested' 'GetCandidatePrefabs[\s\S]*for\s*\(int candidateIndex[\s\S]*m_Spawner\.TrySpawn[\s\S]*InspectVehicleCapacity[\s\S]*if\s*\(!capacityAccepted\)[\s\S]*continue;[\s\S]*spawned\s*=\s*true' 'Every ordered vehicle candidate must be spawned, capacity-checked, rejected safely, and advanced before acceptance'
Assert-MethodContains $vehicleCoordinator 'ProcessRequested' 'requiredSeats\s*=\s*AICF_GroupRuntime\.CountAliveAgents[\s\S]*InspectVehicleCapacity\s*\([\s\S]*requiredSeats' 'Capacity preflight must use the complete living managed roster for each candidate'
Assert-MethodContains $vehicleCoordinator 'ProcessRequested' 'if\s*\(!capacityAccepted\)[\s\S]*DeleteRplEntity\s*\(\s*vehicle[\s\S]*vehicle\s*=\s*null[\s\S]*continue;' 'A rejected live capacity candidate must be deleted and cleared before the next candidate'
Assert-MethodContains $vehicleCoordinator 'ProcessRequested' 'INSUFFICIENT_COMPARTMENTS[\s\S]*VEHICLE_CAPACITY_UNAVAILABLE[\s\S]*BOUNDED_INFANTRY_FALLBACK[\s\S]*BeginFallback' 'Exhausted capacity candidates must enter an observable bounded foot fallback without a spawn loop'
Assert-FileContains $vehicleCoordinator 'BOARDING_COMPLETE[\s\S]{0,500}mounted=' 'Full-group boarding completion must remain observable'

# Transport T repeat contracts: safe wheeled surface, minimum new-request roster, and exact member evidence.
Assert-FileContains $stage3Config '\bDEFAULT_MINIMUM_VEHICLE_REQUEST_AGENTS\s*=\s*3\s*;' 'A new vehicle request must require the accepted three-member combat-ready majority'
Assert-FileContains $stage3Config 'System\.GetCLIParam\s*\(\s*"aicfVehicleMinimumRequestAgents"' 'The new-request combat-ready threshold must expose its documented CLI override'
Assert-MethodContains $vehicleCoordinator 'HasMinimumRosterForNewVehicleRequest' 'CountAliveAgents[\s\S]*GetMinimumVehicleRequestAgents' 'New vehicle eligibility must compare the living roster with the configured threshold'
Assert-MethodContains $vehicleCoordinator 'ProcessFaction' 'TryGetDesiredKind[\s\S]*if\s*\(!CanStartVehicleTrip\s*\(\s*slot\s*\)\)[\s\S]*if\s*\(!HasMinimumRosterForNewVehicleRequest[\s\S]*ReportVehicleRequestIneligible[\s\S]*SuppressVehicleTripForAssignment[\s\S]*continue;[\s\S]*System\.GetTickCount[\s\S]*runtime\s*=\s*new AICF_VehicleRuntime' 'Only an otherwise eligible undersized slot may be reported once and assignment-suppressed before runtime reservation or entity creation'
Assert-MethodContains $vehicleCoordinator 'ProcessRuntime' '!runtime\.GetVehicle\s*\(\s*\)[\s\S]*!HasMinimumRosterForNewVehicleRequest[\s\S]*ReportVehicleRequestIneligible[\s\S]*BeginFallback\s*\([^\)]*"GROUP_NOT_COMBAT_READY"' 'The combat-ready gate must affect only an unassigned new request and enter bounded fallback observably'
Assert-MethodContains $vehicleCoordinator 'ReportVehicleRequestIneligible' 'runtime\s*&&\s*runtime\.GetVehicle[\s\S]*alive=%5[\s\S]*required_minimum=%6[\s\S]*policy=NEW_REQUEST_ONLY[\s\S]*assigned_vehicle_present=%7[\s\S]*assigned_vehicle_policy=PRESERVE_EXISTING[\s\S]*VEHICLE_REQUEST_INELIGIBLE' 'Rejected new requests must report actual assignment presence separately from the preserve-existing policy'
Assert-MethodContains $vehicleSpawner 'TrySpawn' 'foreach\s*\(SCR_CampaignMilitaryBaseComponent safeCandidate[\s\S]*EvaluateBaseSpawnPositions\s*\([\s\S]*site\.m_sResult\s*==\s*"SELECTED"[\s\S]*selectedSite\s*=\s*site[\s\S]*spawnBase\s*=\s*safeCandidate[\s\S]*if\s*\(!spawnBase\)[\s\S]*return false;[\s\S]*if\s*\(preflightOnly\)[\s\S]*return true;[\s\S]*VEHICLE_SPAWN_SITE_SELECTED[\s\S]*SpawnEntityPrefabEx' 'TrySpawn must select a validated multi-position base result and finish preflight before the only vehicle entity-creation boundary'
Assert-MethodContains $vehicleSpawner 'EvaluateBaseSpawnPositions' 'FindAllEmptyTerrainPositions[\s\S]*for\s*\(int candidatePositionIndex[\s\S]*candidatePositions\[candidatePositionIndex\][\s\S]*if\s*\(!IsWheeledSpawnSurfaceSuitable[\s\S]*ReportSurfaceCandidateRejected[\s\S]*continue;[\s\S]*MeasureAliveGroupDistancesToPosition[\s\S]*farthestDistanceMeters\s*>\s*maximumBoardingDistanceMeters[\s\S]*continue;[\s\S]*m_sResult\s*=\s*"SELECTED"[\s\S]*m_vPosition\s*=\s*candidatePosition[\s\S]*return result;' 'Every empty-terrain position must pass wheeled-surface and all-member boarding checks before it can be selected'
Assert-MethodNotContains $vehicleSpawner 'EvaluateBaseSpawnPositions' 'SpawnEntityPrefabEx' 'Spawn-site evaluation must remain side-effect free and never create a vehicle entity'
Assert-MethodContains $vehicleSpawner 'ReportSurfaceCandidateRejected' 'WATER_OR_UNDRIVABLE_SURFACE[\s\S]*candidate_index=%1[\s\S]*origin=\[%1,%2,%3\][\s\S]*surface=%4[\s\S]*water=%5[\s\S]*footprint_delta_m=%1[\s\S]*probes=%2[\s\S]*VEHICLE_SPAWN_CANDIDATE_REJECTED' 'Rejected surface diagnostics must carry the repeat-T causal telemetry'
Assert-MethodContains $vehicleSpawner 'FormatSelectedSurfaceTelemetry' 'origin=\[%1,%2,%3\][\s\S]*surface=%4[\s\S]*water=0[\s\S]*footprint_delta_m=%1[\s\S]*probes=%2[\s\S]*candidate_index=%3[\s\S]*candidates=%4' 'Accepted spawn sites must repeat the validated surface telemetry'
Assert-MethodContains $vehicleWatchdog 'InspectBoardingProgress' 'GetAgents[\s\S]*distance_m=%2[\s\S]*linked=%3[\s\S]*compartment=%4[\s\S]*getting_in=%5[\s\S]*getting_out=%6[\s\S]*character_vehicle=%7[\s\S]*settled=%8[\s\S]*target_scope=%9[\s\S]*ai_action=%1[\s\S]*ai_action_state=%2' 'Boarding diagnostics must retain exact evidence for every living member'
Assert-FileContains $vehicleRuntime 'class\s+AICF_VehiclePassengerActionToken[\s\S]*SCR_AIGetInVehicle\s+m_Action[\s\S]*BaseCompartmentSlot\s+m_Compartment[\s\S]*IEntity\s+m_ReservedEntity[\s\S]*GetCompartment\(\)[\s\S]*GetReservedEntity\(\)' 'Runtime passenger tokens must retain the exact action, cargo compartment, and reservation owner together'
Assert-MethodContains $vehicleRuntime 'TrackPassengerBoardingAction' 'agent[\s\S]*action[\s\S]*compartment[\s\S]*reservedEntity[\s\S]*m_aPassengerBoardingActions\.Insert\s*\(\s*new AICF_VehiclePassengerActionToken' 'Every exact passenger action must be persisted in its vehicle runtime'
Assert-MethodContains $vehiclePassengerBoarding 'Start' 'array<BaseCompartmentSlot>\s+assignedCompartments[\s\S]*FindAvailableCargoCompartment[\s\S]*assignedCompartments\.Insert[\s\S]*for\s*\(int reserveIndex[\s\S]*SetReserved\s*\(\s*reservedEntity\s*\)[\s\S]*IsReservedBy\s*\(\s*reservedEntity\s*\)[\s\S]*RollbackReservations[\s\S]*for\s*\(int actionIndex[\s\S]*new SCR_AIGetInVehicle[\s\S]*assignedCompartments\[actionIndex\][\s\S]*EAICompartmentType\.Cargo[\s\S]*AddAction[\s\S]*TrackPassengerBoardingAction' 'Passenger start must reserve the complete exact cargo plan atomically before adding the first action'
Assert-MethodContains $vehiclePassengerBoarding 'Start' 'GetVehicleIn\s*\(\s*entity\s*\)\s*==\s*runtime\.GetVehicle\(\)[\s\S]*!IsSupportedSettledCompartment\s*\(\s*runtime,\s*entity\s*\)[\s\S]*return false;[\s\S]*continue;' 'An already-linked member must be role-compatible and physically settled before passenger planning skips it'
Assert-MethodContains $vehiclePassengerBoarding 'FindAvailableCargoCompartment' 'GetCompartments[\s\S]*CargoCompartmentSlot\.Cast[\s\S]*!cargo[\s\S]*excluded\.Contains[\s\S]*!compartment\.IsCompartmentAccessible[\s\S]*compartment\.GetOccupant[\s\S]*compartment\.IsReserved[\s\S]*compartment\.IsGetInLockedFor[\s\S]*selected\s*=\s*compartment' 'Exact passenger assignment must select only an accessible, unoccupied, unreserved Cargo compartment'
Assert-MethodContains $vehiclePassengerBoarding 'Maintain' 'IsMemberSettledInVehicle[\s\S]*access\.GetCompartment\(\)\s*!=\s*token\.GetCompartment\(\)[\s\S]*WRONG_COMPARTMENT[\s\S]*GetVehicleIn\s*\(\s*tokenEntity\s*\)\s*==\s*vehicle[\s\S]*continue;[\s\S]*access\.IsGettingIn\(\)\s*\|\|\s*access\.IsGettingOut\(\)[\s\S]*continue;[\s\S]*GetActionState[\s\S]*Cancel\s*\(\s*runtime,\s*token\s*\)[\s\S]*IssueOne[\s\S]*transition_fenced=1' 'Passenger polling must require the reserved exact compartment and fence transitions before any retry or cancellation'
Assert-MethodContains $vehiclePassengerBoarding 'Maintain' 'trackedToken\s*=\s*runtime\.FindPassengerBoardingAction[\s\S]*trackedToken\s*\|\|\s*\(access\s*&&\s*\(access\.IsGettingIn\(\)\s*\|\|\s*access\.IsGettingOut\(\)\)\)[\s\S]*continue;[\s\S]*GetVehicleIn\s*\(\s*entity\s*\)\s*==\s*vehicle[\s\S]*!IsSupportedSettledCompartment\s*\(\s*runtime,\s*entity\s*\)[\s\S]*UNSUPPORTED_COMPARTMENT[\s\S]*return false;[\s\S]*IssueOne' 'Passenger maintenance must fence tracked transitions, then fail closed only on an untracked linked unsupported or unsettled compartment'
Assert-MethodContains $vehiclePassengerBoarding 'IsSupportedSettledCompartment' 'IsMemberSettledInVehicle[\s\S]*ResolveAccess[\s\S]*GetCompartment[\s\S]*PilotCompartmentSlot\.Cast[\s\S]*CargoCompartmentSlot\.Cast[\s\S]*return true;[\s\S]*AICF_EVehicleKind\.ARMED_LIGHT[\s\S]*TurretCompartmentSlot\.Cast' 'Already-linked members must occupy a physically settled pilot, Cargo, or armed-only turret compartment'
Assert-MethodContains $vehicleWatchdog 'CountAccessibleSeatsForVehicle' 'GetCompartments[\s\S]*!compartment\.IsCompartmentAccessible[\s\S]*compartment\.GetOccupant[\s\S]*compartment\.IsReserved[\s\S]*PilotCompartmentSlot\.Cast[\s\S]*supportedSeat\s*=\s*true[\s\S]*CargoCompartmentSlot\.Cast[\s\S]*supportedSeat\s*=\s*true[\s\S]*TurretCompartmentSlot\.Cast[\s\S]*supportedSeat\s*=\s*kind\s*==\s*AICF_EVehicleKind\.ARMED_LIGHT[\s\S]*count\+\+' 'Capacity fallback must count only role-compatible pilot, Cargo, and armed-only turret compartments'
Assert-MethodContains $vehicleWatchdog 'InspectVehicleCapacity' 'CountAccessibleSeatsForVehicle[\s\S]*availableSeats\s*<\s*requiredSeats[\s\S]*!hasPilot[\s\S]*return false;[\s\S]*hasTurret\s*&&\s*requiredSeats\s*>=\s*2' 'Vehicle capacity acceptance must apply the supported-seat count and mandatory crew roles before a light candidate is accepted'
Assert-MethodContains $vehiclePassengerBoarding 'Cancel' 'GetActionState[\s\S]*state\s*!=\s*EAIActionState\.COMPLETED[\s\S]*state\s*!=\s*EAIActionState\.FAILED[\s\S]*!linked[\s\S]*IsAliveCharacter[\s\S]*utility\.m_OwnerEntity\s*==\s*currentEntity[\s\S]*action\.Fail\(\)[\s\S]*ReleaseReservation\s*\(\s*token\s*\)[\s\S]*RemovePassengerBoardingAction\s*\(\s*token\s*\)' 'Passenger cancellation must fail only a live owned action, then release its reservation and runtime token'
Assert-MethodContains $vehiclePassengerBoarding 'ReleaseReservation' 'IsReservedBy\s*\(\s*token\.GetReservedEntity\(\)\s*\)[\s\S]*SetReserved\s*\(\s*null\s*\)' 'Passenger cleanup must release only the reservation owned by the tracked member'
Assert-MethodContains $vehiclePassengerBoarding 'CancelAll' 'GetPassengerBoardingActionCount[\s\S]*for\s*\(int index[\s\S]*Cancel\s*\(\s*runtime,\s*runtime\.GetPassengerBoardingAction\(index\)\s*\)' 'Fallback cleanup must cancel every tracked passenger action'
Assert-MethodContains $vehiclePassengerBoarding 'ClearTracking' 'GetPassengerBoardingActionCount[\s\S]*ReleaseTracking[\s\S]*ClearPassengerBoardingActions' 'Successful boarding cleanup must release every reservation before clearing runtime tracking'
Assert-MethodContains $vehicleCoordinator 'StartBoardingPhase' 'phase\s*==\s*AICF_EVehicleBoardingPhase\.PASSENGERS[\s\S]*m_PassengerBoarding\.Start[\s\S]*EXACT_PER_MEMBER_CARGO' 'The passenger phase must delegate to exact per-member cargo actions rather than a generic group boarding waypoint'
Assert-MethodNotContains $vehicleCoordinator 'StartBoardingPhase' 'CreatePassengerBoardingWaypoint|AttachVehicleToGroup\s*\(' 'Passenger actions must start while generic boarding and group vehicle utility remain unavailable'
Assert-MethodContains $vehicleCoordinator 'ProcessBoarding' '(?m)^[ \t]*if\s*\(phase\s*==\s*AICF_EVehicleBoardingPhase\.PASSENGERS\)[ \t]*\r?$[\s\S]*!driverSettled[\s\S]*!gunnerSettled[\s\S]*CREW_ROLE_LOST_DURING_BOARDING[\s\S]*BeginFallback[\s\S]*return;[\s\S]*m_PassengerBoarding\.Maintain' 'Mandatory crew loss must enter fallback before passenger maintenance can allocate or retry Cargo actions'
Assert-MethodContains $vehicleCoordinator 'BeginFallback' 'm_PassengerBoarding\.CancelAll\s*\(\s*runtime\s*\)[\s\S]*SetState\s*\(\s*AICF_EVehicleState\.INFANTRY_FALLBACK' 'Infantry fallback must clean exact passenger actions and reservations before the terminal transition'

# Deterministic attack distribution, forward defense/QRF and bounded role churn.
Assert-FileContains $targetSelector 'ForwardDefend|FORWARD_DEFEND' 'TargetSelector must implement a forward friendly defense target'
Assert-FileContains $targetSelector 'attack(?:Slot|Index|Ordinal)|preferredIndex|slotId|direction' 'Attack selection must vary deterministically by managed attack slot'
Assert-MethodContains $targetSelector 'SelectAttackTarget' 'InsertRankedAttackNode[\s\S]*SelectAttackPlanNode' 'Attack target distribution must rank reachable nodes deterministically before applying the role-local plan'
Assert-MethodContains $targetSelector 'SelectAttackPlanNode' 'PRIMARY_RANKED_REACHABLE[\s\S]*ADJACENT_TO_PRIMARY[\s\S]*SUPPORT_ADJACENT_DIRECTION[\s\S]*SUPPORT_SECONDARY_DIRECTION' 'A0/A1/A2 must map to primary, adjacent direction and support with bounded deterministic fallbacks'
Assert-MethodContains $targetSelector 'SelectAttackPlanNode' 'AreAttackNodesAdjacent' 'Secondary and support attack choices must be causally tied to adjacent reachable directions'
Assert-FileContains $orderPlanner 'ForwardDefend|FORWARD_DEFEND' 'OrderPlanner must route D0 through forward defense planning'
Assert-CombinedContains @($targetSelector, $orderPlanner, $groupSlot, $matchController) '\bQRF\b|Qrf' 'The forward defender must expose a QRF posture'
Assert-CombinedContains @($targetSelector, $orderPlanner, $groupSlot, $matchController) 'HYSTERESIS|Hysteresis|MIN(?:IMUM)?_?DWELL|Min(?:imum)?Dwell' 'Forward-defense/QRF retargeting must have hysteresis or minimum dwell'
Assert-CombinedContains @($targetSelector, $orderPlanner, $matchController) 'IsBeingCaptured\s*\(|AreEnemiesPresent\s*\(|CONTESTED' 'QRF planning must react to a contested or threatened friendly base'
Assert-CombinedContains @($targetSelector, $orderPlanner, $matchController) 'BASE_OWNER_CHANGED|OWNER_CHANGED' 'Active-force planning must remain connected to owner-change replanning'
Assert-MethodContains $targetSelector 'SelectDefendTarget' 'IsThreatened[\s\S]*posture\s*=\s*"QRF"[\s\S]*HQ_THREAT[\s\S]*CONTESTED' 'D0 must prioritize threatened friendly bases with explicit QRF triggers'
Assert-MethodContains $orderPlanner 'ReconcileStrategicOrder' 'urgentQRF[\s\S]*IsStrategicCandidateReady[\s\S]*minimumDwellMs[\s\S]*stableCandidateMs[\s\S]*STRATEGIC_CANDIDATE_HELD' 'QRF escalation and stabilized return must use explicit hysteresis/minimum dwell evidence'
Assert-MethodContains $matchController 'ReplanFactionAfterBaseChange' 'IsDefendLossResponseRelevant[\s\S]*AssignLossResponseOrder' 'A relevant neighboring base loss must trigger the D0 loss-response QRF path'
Assert-MethodContains $matchController 'AuditActiveFactionTasking' 'unexplainedMobIdle\s*=\s*atMob\s*&&\s*!allowedException[\s\S]*idleAgeMs\s*<\s*2\s*\*\s*m_Config\.GetCommanderIntervalMs\(\)[\s\S]*MOB_IDLE_DEADLINE_MISSED' 'Continuous MOB presence without a permitted exception must fail observably at the two-interval deadline'
Assert-MethodContains $groupSlot 'ObserveMeaningfulTaskLoss' '!taskLost[\s\S]*m_iMeaningfulTaskLostStartedAtMs\s*=\s*0[\s\S]*m_bMeaningfulTaskLossReported\s*=\s*false[\s\S]*m_bMeaningfulTaskDeadlineReported\s*=\s*false[\s\S]*nowMs\s*-\s*Math\.Max\(0, observationSlackMs\)' 'Task-loss timing must be continuous, reset on recovery, and include first-sample slack'
Assert-MethodContains $groupSlot 'ObserveMobIdleSuppression' 'suppressionCleared\s*=\s*!m_sMobIdleSuppressionReason\.IsEmpty\(\)[\s\S]*m_sMobIdleSuppressionReason\s*=\s*string\.Empty[\s\S]*return suppressionCleared[\s\S]*m_sMobIdleSuppressionReason\s*==\s*reason' 'MOB suppression telemetry must report both reason changes and the transition back to an active deadline'
Assert-MethodContains $matchController 'AuditActiveFactionTasking' 'HasMeaningfulTask[\s\S]*ObserveMeaningfulTaskLoss\(\s*!meaningfulTask[\s\S]*MEANINGFUL_TASK_LOST[\s\S]*TryRecoverOrder\(slot, faction, "MEANINGFUL_TASK_LOST"\)[\s\S]*taskDeadlineMs\s*=\s*2\s*\*\s*m_Config\.GetCommanderIntervalMs\(\)[\s\S]*MEANINGFUL_TASK_DEADLINE_MISSED' 'Every combat-ready slot must report, repair, and hard-fail a globally taskless state independently of MOB distance'
Assert-MethodContains $matchController 'AuditActiveFactionTasking' 'MEANINGFUL_TASK_RECOVERED[\s\S]*taskless_age_ms=%7[\s\S]*alive=%1 role=%2 posture=%3 at_mob=%4' 'Task recovery telemetry must retain the roster, role/posture and MOB context of the recovered slot'
Assert-MethodContains $matchController 'HasMeaningfulTask' 'IsWaypointBoundToGroup\(slot\.GetGroup\(\), slot\.GetWaypoint\(\)\)[\s\S]*IsWaypointBoundToGroup\(slot\.GetGroup\(\), vehicleRuntime\.GetActiveWaypoint\(\)\)' 'Meaningful tasking must require an infantry or vehicle waypoint actually bound to the group queue'
Assert-MethodContains $matchController 'IsWaypointBoundToGroup' 'GetWaypoints\(waypointQueue\)[\s\S]*waypointQueue\.Contains\(waypoint\)' 'Waypoint task evidence must be verified against the authoritative group queue'
Assert-MethodContains $matchController 'AuditActiveFactionTasking' 'suppressionRule\s*=\s*"OUTSIDE_MOB"[\s\S]*ObserveMobIdleSuppression[\s\S]*IDLE_DEADLINE_SUPPRESSED[\s\S]*distance_to_mob_m=[\s\S]*taskless_age_ms=[\s\S]*suppression_active=' 'MOB-deadline suppression and its clearing transition must be observable without hiding the independent global taskless timer'
Assert-MethodContains $matchController 'TryRecoverOrder' 'requestedAtMs\s*=\s*System\.GetTickCount\(\)[\s\S]*ORDER_RESTORE_REQUESTED[\s\S]*RecoverOrder[\s\S]*GetWaypoints[\s\S]*Contains\(newWaypoint\)[\s\S]*ORDER_RESTORE_RESULT[\s\S]*bound_to_group=%8[\s\S]*postcondition_meaningful_task=%2[\s\S]*latency_ms=%4[\s\S]*WAYPOINT_BIND_MISMATCH' 'Reliability order repair must log the request, latency, and verify the waypoint was actually bound to the group queue'
Assert-MethodContains $orderPlanner 'LogWaypointRemoved' 'WAYPOINT_REMOVED[\s\S]*waypoint_kind=INFANTRY[\s\S]*owner=ORDER_PLANNER[\s\S]*remove_trigger=%6[\s\S]*remove_reason=%7' 'Infantry waypoint removal must retain its owner, trigger, reason, and stable slot identity'
Assert-MethodContains $matchController 'Update' 'AuditActiveFactionTasking\s*\(\s*m_USState[\s\S]*AuditActiveFactionTasking\s*\(\s*m_USSRState' 'MOB presence must be sampled at the one-second authority update cadence, not only on commander ticks'
Assert-MethodContains $matchController 'IsSafeVehicleSpawnWait' 'WAITING_FOR_SITE[\s\S]*GetLastSpawnFailureReason[\s\S]*SPAWN_POINT_DISABLED[\s\S]*NO_SAFE_SPAWN_AVAILABLE[\s\S]*NO_BOARDING_SITE_WITHIN_RANGE' 'Only an explicit whitelist of safe-site wait reasons may exempt a slot from the MOB deadline'
Assert-MethodNotContains $matchController 'IsSafeVehicleSpawnWait' 'case\s+"(?:VEHICLE_CAP_UNAVAILABLE|TRIP_CONTEXT_NOT_READY|POST_APPROACH_COHESION_WAIT)"' 'Cap, invalid trip context and cohesion wait must not masquerade as the safe-spawn MOB exception'
Assert-MethodContains $matchController 'AuditActiveFactionTasking' 'safeVehicleSpawnWait[\s\S]*allowedException' 'The MOB audit must consume the reason-qualified safe vehicle spawn exception'
Assert-MethodContains $matchController 'RecordPendingOwnerChange' 'm_aPendingOwnerChangedBases\.Find[\s\S]*m_aPendingOwnerChangeNewOwners[\s\S]*Insert\(base\)' 'Coalesced owner changes must retain a bounded per-base net-change set'
Assert-MethodContains $matchController 'ReplanFactionAfterBaseChange' 'm_aPendingOwnerChangedBases[\s\S]*relevantLostBases[\s\S]*foreach[\s\S]*AssignLossResponseOrder' 'Every relevant neighboring loss in one rebuild window must reach D0 QRF planning'
Assert-MethodContains $vehicleCoordinator 'ProcessWaitingForSite' 'CountReservedExcluding[\s\S]*GetMaxVehiclesPerFaction[\s\S]*VEHICLE_CAP_UNAVAILABLE[\s\S]*return;[\s\S]*ResetSpawnRequestContext' 'A cap-free waiting request must recheck cap before waking into the reserved REQUESTED state'
Assert-MethodContains $vehicleCoordinator 'ProcessWaitingForSite' 'ProcessBoundedCohesionWait\(runtime, faction, slot\)[\s\S]*GetNextAttemptAtMs' 'Fragmented-group waiting must be audited every coordinator update rather than only on the minute probe'
Assert-MethodContains $vehicleWatchdog 'MeasureAliveGroupSpread' 'ResolveAliveLeader[\s\S]*IsAliveCharacter[\s\S]*farthestFromLeaderMeters[\s\S]*for \(int firstIndex[\s\S]*for \(int secondIndex[\s\S]*maximumPairDistanceMeters' 'Cohesion waiting must measure all living members and maximum pair spread, not merely leader distance'
Assert-MethodContains $vehicleRuntime 'ObserveCohesionWait' '!fragmented[\s\S]*m_iCohesionWaitStartedAtMs\s*=\s*0[\s\S]*m_bCohesionWaitRecoveryAttempted\s*=\s*false[\s\S]*System\.GetTickCount\(m_iCohesionWaitStartedAtMs\)' 'The fragmented-group wait timer must reset only after cohesion is restored'
Assert-MethodContains $vehicleCoordinator 'ProcessBoundedCohesionWait' 'NO_BOARDING_SITE_WITHIN_RANGE[\s\S]*POST_APPROACH_COHESION_WAIT[\s\S]*MeasureAliveGroupSpread[\s\S]*maximumPairDistanceMeters\s*>\s*m_Config\.GetCohesionDistanceMeters\(\)[\s\S]*deadlineMs / 2[\s\S]*NormalizeAfterMovementFailure[\s\S]*RebuildCurrentOrder[\s\S]*outcome=RECOVERY_ISSUED[\s\S]*cohesionWaitAgeMs < deadlineMs[\s\S]*SuppressVehicleTripForAssignment[\s\S]*BOARDING_RANGE_WAIT_EXHAUSTED[\s\S]*RestoreInfantryOrder[\s\S]*COHESION_OUTCOME[\s\S]*WAITING_FOR_SITE_EXIT' 'Only a genuinely fragmented group may receive one half-deadline recovery and a full-deadline bounded infantry fallback from vehicle-site waiting'
Assert-MethodContains $vehicleCoordinator 'ProcessBoundedCohesionWait' 'outcome=VEHICLE_REQUEST_ENDED[\s\S]*wait_age_ms=%6[\s\S]*deadline_ms=%4' 'The terminal cohesion outcome must retain the configured deadline as well as observed wait age'
Assert-MethodContains $vehicleCoordinator 'ProcessBoundedCohesionWait' 'infantryOrderActive\s*=\s*RestoreInfantryOrder[\s\S]*if \(!infantryOrderActive\)[\s\S]*MarkInfantryFallbackRestorePending' 'A bounded cohesion fallback must verify a real infantry-order bind even when the slot retains a stale waypoint pointer'
Assert-MethodContains $vehicleCoordinator 'IsControllingMovement' 'case AICF_EVehicleState\.ABANDONED:[\s\S]*case AICF_EVehicleState\.DESTROYED:[\s\S]*return false;' 'Terminal vehicle cleanup must never suppress the living group strategic infantry order'
Assert-MethodContains $vehicleRuntime 'BeginForceDismountAttempt' 'm_iForceDismountAttempts\s*>=\s*maximumAttempts[\s\S]*m_iLastForceDismountAttemptAtMs[\s\S]*retryIntervalMs[\s\S]*m_iForceDismountAttempts\+\+' 'Owner-directed terminal dismount retries must have both an attempt cap and retry interval'
Assert-MethodContains $vehicleCoordinator 'ForceAliveGroupMembersOut' 'GetVehicleIn\(character\)\s*!=\s*vehicle[\s\S]*InterruptVehicleActionQueue[\s\S]*forceAttempt\s*=\s*runtime\.GetForceDismountAttempts\(\)[\s\S]*exactEscalation\s*=\s*forceAttempt\s*>\s*1[\s\S]*if \(!exactEscalation\)[\s\S]*GetOutVehicle_NoDoor[\s\S]*access\.GetCompartment\(\)[\s\S]*GetOccupant\(\)\s*==\s*character[\s\S]*exactEjectNeeded\s*=\s*exactEscalation\s*\|\|[\s\S]*EjectOccupant\(true, false, ejectedImmediately, false\)[\s\S]*GetCompartmentSlotID\(\)[\s\S]*GetCompartmentMgrID\(\)[\s\S]*FORCE_DISEMBARK_MEMBER[\s\S]*compartment_manager=%1[\s\S]*exact_escalation=%5' 'A direct get-out may run once, but a still-linked exact manager/slot owner must reach bounded compartment ejection no later than the next force attempt'
Assert-MethodContains $vehicleCoordinator 'EmitAbandonedExitAudit' 'InspectProtectedMemberDismountClearance[\s\S]*MarkTerminalAuditDue[\s\S]*WAIT_PROTECTED_CLEARANCE[\s\S]*GetForceDismountAttempts\(\)\s*<\s*FORCE_DISEMBARK_MAX_ATTEMPTS[\s\S]*FORCE_DISEMBARK_RECHECK[\s\S]*ABANDONED_EXIT_AUDIT[\s\S]*state_age_ms=[\s\S]*logical_occupants=[\s\S]*restore_pending=[\s\S]*meaningful_task=[\s\S]*pending_age_ms=[\s\S]*force_attempts=[\s\S]*next_action=' 'Terminal handoff must emit truthful state-change/rate-limited evidence while protected occupants or order restoration remain pending'
Assert-MethodContains $vehicleCoordinator 'ProcessTerminal' 'EmitAbandonedExitAudit[\s\S]*IsInfantryFallbackRestorePending[\s\S]*BeginForceDismountAttempt[\s\S]*ForceAliveGroupMembersOut[\s\S]*RestoreInfantryOrder[\s\S]*if \(restored\)[\s\S]*ClearInfantryFallbackRestorePending[\s\S]*if \(!groupOut\)[\s\S]*return;[\s\S]*CanDeleteVehicleSafely' 'Terminal polling must restore the current living group before clearance completes while retaining the protected cleanup gate'
Assert-MethodContains $vehicleCoordinator 'RestoreInfantryOrder' 'ORDER_RESTORE_REQUESTED[\s\S]*GetWaypoints[\s\S]*existingQueue\.Contains[\s\S]*RebuildCurrentOrder[\s\S]*AssignOrder[\s\S]*waypointQueue\.Contains[\s\S]*ORDER_RESTORE_RESULT[\s\S]*bound_to_group=%8[\s\S]*WAYPOINT_BIND_MISMATCH' 'Vehicle handoff must verify a real group-queue bind instead of treating waypoint allocation as restoration success'
Assert-MethodContains $vehicleCoordinator 'DeleteRuntimeWaypoint' 'WAYPOINT_REMOVED[\s\S]*waypoint_kind=VEHICLE[\s\S]*owner=VEHICLE_RUNTIME[\s\S]*remove_trigger=STATE_TRANSITION[\s\S]*DeleteOwnedWaypoint' 'Vehicle waypoint removal must be observable before the owned entity is deleted'

# A0/A1/A2 and D0 labels must use a role-local ordinal, not the raw global slot.
Assert-FileContains $groupMarkers 'RoleLocal|ROLE_LOCAL' 'Group markers must define a role-local slot key'
Assert-MethodContains $groupMarkers 'BuildMarkerText' 'RoleLocal|ROLE_LOCAL' 'Marker identity text must use the role-local slot key'
Assert-FileContains $groupMarkers 'ATTACK_SLOTS_PER_FACTION' 'Role-local marker numbering must account for the ATTACK slot boundary'

# Enforce has a fixed string.Format arity and does not support C-style ternary expressions.
$productionRoots = @('AIConflictCore', 'AIConflictArland')
Assert-StringFormatContracts $productionRoots
Assert-NoCStyleTernary $productionRoots

if ($failures.Count -gt 0) {
    Write-Host "Stage 3.5 static audit: FAIL ($($failures.Count) issue(s))" -ForegroundColor Red
    $failures | ForEach-Object { Write-Host " - $_" -ForegroundColor Red }
    exit 1
}

Write-Host 'Stage 3.5 static audit: PASS' -ForegroundColor Green
Write-Host 'Checked: five-member initial/replacement roster, 3 ATTACK + 1 forward DEFEND/QRF + 0 reserve, 48-agent floor, active-force CLI gate, exact readiness, four-slot motorization and cap, truck/light faction catalog, role-compatible capacity fallback, minimum new-request roster, multi-position wheeled-surface rejection before spawn, atomic exact-Cargo passenger tokens/reservations/transition fences/cleanup, exact per-member boarding evidence, bounded fragmented-cohesion waiting, terminal owner-directed dismount and infantry-order handoff, global task-loss and waypoint lifecycle telemetry, deterministic attack distribution, QRF hysteresis, role-local marker identity, string.Format arity/placeholders, and unsupported ternary tokens.'
