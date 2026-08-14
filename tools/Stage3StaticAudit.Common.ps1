Set-StrictMode -Version 2.0

function Add-AICFAuditFailure {
    param(
        [System.Collections.Generic.List[string]]$Failures,
        [string]$RuleId,
        [string]$Message
    )

    $Failures.Add("[$RuleId] $Message")
}

function Test-AICFRuleEnabled {
    param([string[]]$OnlyRules, [string]$RuleId)

    if (-not $OnlyRules -or $OnlyRules.Count -eq 0) {
        return $true
    }

    return $OnlyRules -contains $RuleId
}

function ConvertTo-AICFCodeText {
    param([string]$Source)

    if (-not $Source) {
        return ''
    }

    $builder = [System.Text.StringBuilder]::new($Source.Length)
    $inString = $false
    $inLineComment = $false
    $inBlockComment = $false

    for ($offset = 0; $offset -lt $Source.Length; $offset++) {
        $character = $Source[$offset]
        $nextCharacter = [char]0
        if ($offset + 1 -lt $Source.Length) {
            $nextCharacter = $Source[$offset + 1]
        }

        if ($inLineComment) {
            if ($character -eq "`n") {
                $inLineComment = $false
                [void]$builder.Append($character)
            }
            else {
                [void]$builder.Append(' ')
            }
            continue
        }

        if ($inBlockComment) {
            if ($character -eq '*' -and $nextCharacter -eq '/') {
                [void]$builder.Append(' ')
                [void]$builder.Append(' ')
                $offset++
                $inBlockComment = $false
            }
            elseif ($character -eq "`n") {
                [void]$builder.Append($character)
            }
            else {
                [void]$builder.Append(' ')
            }
            continue
        }

        if ($inString) {
            if ($character -eq '\') {
                [void]$builder.Append(' ')
                if ($offset + 1 -lt $Source.Length) {
                    $offset++
                    [void]$builder.Append(' ')
                }
                continue
            }
            if ($character -eq '"') {
                $inString = $false
            }
            [void]$builder.Append(' ')
            continue
        }

        if ($character -eq '/' -and $nextCharacter -eq '/') {
            [void]$builder.Append(' ')
            [void]$builder.Append(' ')
            $offset++
            $inLineComment = $true
            continue
        }
        if ($character -eq '/' -and $nextCharacter -eq '*') {
            [void]$builder.Append(' ')
            [void]$builder.Append(' ')
            $offset++
            $inBlockComment = $true
            continue
        }
        if ($character -eq '"') {
            [void]$builder.Append(' ')
            $inString = $true
            continue
        }

        [void]$builder.Append($character)
    }

    return $builder.ToString()
}

function Get-AICFStringLiteralText {
    param([string]$Source)

    if (-not $Source) {
        return ''
    }

    $builder = [System.Text.StringBuilder]::new()
    $literal = [System.Text.StringBuilder]::new()
    $inString = $false
    $inLineComment = $false
    $inBlockComment = $false

    for ($offset = 0; $offset -lt $Source.Length; $offset++) {
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
                [void]$literal.Append($character)
                if ($offset + 1 -lt $Source.Length) {
                    $offset++
                    [void]$literal.Append($Source[$offset])
                }
                continue
            }
            if ($character -eq '"') {
                [void]$builder.AppendLine($literal.ToString())
                [void]$literal.Clear()
                $inString = $false
                continue
            }
            [void]$literal.Append($character)
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
        }
    }

    return $builder.ToString()
}

function Get-AICFSourceRecords {
    param(
        [string]$RepositoryRoot,
        [string[]]$ProductionRoots = @('AIConflictCore/Scripts/Game/AIConflict')
    )

    $records = [System.Collections.Generic.List[object]]::new()
    $resolvedRoot = [System.IO.Path]::GetFullPath($RepositoryRoot).TrimEnd([char[]]"\/")

    foreach ($relativeRoot in $ProductionRoots) {
        $sourceRoot = Join-Path $resolvedRoot $relativeRoot
        if (-not (Test-Path -LiteralPath $sourceRoot -PathType Container)) {
            continue
        }

        foreach ($file in (Get-ChildItem -LiteralPath $sourceRoot -Recurse -Filter '*.c' -File)) {
            $source = Get-Content -LiteralPath $file.FullName -Raw
            $relativePath = $file.FullName.Substring($resolvedRoot.Length).TrimStart([char[]]"\/").Replace('\', '/')
            $records.Add([pscustomobject]@{
                Name = $file.Name
                FullName = $file.FullName
                RelativePath = $relativePath
                Source = $source
                Code = (ConvertTo-AICFCodeText $source)
                Strings = (Get-AICFStringLiteralText $source)
            })
        }
    }

    return $records.ToArray()
}

function Find-AICFClassRecord {
    param([object[]]$Records, [string]$ClassName)

    $escapedName = [regex]::Escape($ClassName)
    $matches = @($Records | Where-Object { $_.Code -match ("\bclass\s+" + $escapedName + "\b") })
    if ($matches.Count -eq 1) {
        return $matches[0]
    }
    return $null
}

function Get-AICFBracedBody {
    param([string]$Source, [string]$StartPattern)

    if (-not $Source) {
        return ''
    }

    $match = [regex]::Match($Source, $StartPattern, [System.Text.RegularExpressions.RegexOptions]::Multiline)
    if (-not $match.Success) {
        return ''
    }

    $openBrace = $Source.IndexOf('{', $match.Index + $match.Length)
    if ($openBrace -lt 0) {
        return ''
    }

    $depth = 0
    $inString = $false
    $inLineComment = $false
    $inBlockComment = $false
    for ($offset = $openBrace; $offset -lt $Source.Length; $offset++) {
        $character = $Source[$offset]
        $nextCharacter = [char]0
        if ($offset + 1 -lt $Source.Length) {
            $nextCharacter = $Source[$offset + 1]
        }

        if ($inLineComment) {
            if ($character -eq "`n") { $inLineComment = $false }
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
            if ($character -eq '\') { $offset++; continue }
            if ($character -eq '"') { $inString = $false }
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
        if ($character -eq '{') {
            $depth++
            continue
        }
        if ($character -eq '}') {
            $depth--
            if ($depth -eq 0) {
                return $Source.Substring($openBrace, $offset - $openBrace + 1)
            }
        }
    }

    return ''
}

function Get-AICFMethodBody {
    param([object]$Record, [string]$MethodName)

    if (-not $Record) {
        return ''
    }

    $escapedName = [regex]::Escape($MethodName)
    $signature = "(?m)^\s*(?!return\b)(?:(?:override|protected|private|static)\s+)*[A-Za-z_][A-Za-z0-9_<>.,\[\]]*\s+$escapedName\s*\("
    return Get-AICFBracedBody $Record.Source $signature
}

function Get-AICFFirstMethodBody {
    param([object]$Record, [string[]]$MethodNames)

    foreach ($methodName in $MethodNames) {
        $body = Get-AICFMethodBody $Record $methodName
        if ($body) {
            return $body
        }
    }
    return ''
}

function Assert-AICFContains {
    param(
        [System.Collections.Generic.List[string]]$Failures,
        [string]$RuleId,
        [string]$Text,
        [string]$Pattern,
        [string]$Message
    )

    if (-not $Text -or $Text -notmatch $Pattern) {
        Add-AICFAuditFailure $Failures $RuleId $Message
    }
}

function Assert-AICFNotContains {
    param(
        [System.Collections.Generic.List[string]]$Failures,
        [string]$RuleId,
        [string]$Text,
        [string]$Pattern,
        [string]$Message
    )

    if ($Text -and $Text -match $Pattern) {
        Add-AICFAuditFailure $Failures $RuleId $Message
    }
}

function Assert-AICFComponentPresent {
    param(
        [System.Collections.Generic.List[string]]$Failures,
        [string]$RuleId,
        [object]$Record,
        [string]$ClassName
    )

    if (-not $Record) {
        Add-AICFAuditFailure $Failures $RuleId "Missing unique component class $ClassName"
        return $false
    }
    return $true
}

function Invoke-AICFVehicleArchitectureAudit {
    param(
        [string]$RepositoryRoot,
        [string[]]$OnlyRules = @(),
        [switch]$FixtureMode
    )

    $failures = [System.Collections.Generic.List[string]]::new()
    $records = @(Get-AICFSourceRecords $RepositoryRoot)
    if ($records.Count -eq 0) {
        Add-AICFAuditFailure $failures 'SOURCE_TREE_MISSING' 'No production Enforce sources were found'
        return $failures.ToArray()
    }

    if (Test-AICFRuleEnabled $OnlyRules 'COORDINATOR_SIDE_EFFECT') {
        $coordinator = Find-AICFClassRecord $records 'AICF_VehicleCoordinator'
        if (Assert-AICFComponentPresent $failures 'COORDINATOR_SIDE_EFFECT' $coordinator 'AICF_VehicleCoordinator') {
            $bannedCoordinatorCapabilities = @(
                '\bSpawnEntityPrefabEx\s*\(',
                '\bDeleteRplEntity\s*\(',
                '\bSCR_AIGetInVehicle\b',
                '\bGetOutVehicle_NoDoor\s*\(',
                '\bEjectOccupant\s*\(',
                '\.Teleport\s*\(',
                '\bSetWorldTransform\s*\(',
                '\bSetVelocity\s*\(',
                '\bSetAngularVelocity\s*\('
            )
            foreach ($pattern in $bannedCoordinatorCapabilities) {
                if ($coordinator.Code -match $pattern) {
                    Add-AICFAuditFailure $failures 'COORDINATOR_SIDE_EFFECT' "Facade owns forbidden engine capability '$pattern' ($($coordinator.RelativePath))"
                }
            }
            Assert-AICFNotContains $failures 'COORDINATOR_SIDE_EFFECT' $coordinator.Code '\b(?:ProcessBoarding|ProcessMoving|ProcessDismount|ProcessFallback|ProcessTerminal|TrySpawn|ForceAliveGroupMembersOut|RequestVehicleDelete)\s*\(' 'Facade still contains an old phase/cleanup implementation method'
            if (-not $FixtureMode) {
                Assert-AICFContains $failures 'COORDINATOR_SIDE_EFFECT' $coordinator.Code '\bAICF_TransportTripController\b' 'Facade must dispatch trips through TransportTripController'
                Assert-AICFContains $failures 'COORDINATOR_SIDE_EFFECT' $coordinator.Code '\bAICF_FactionFleet\b' 'Facade must compose or access the faction fleet aggregate'
            }
        }
    }

    if (Test-AICFRuleEnabled $OnlyRules 'FLOW_CROSS_CALL') {
        $flowClasses = @(
            'AICF_VehicleAcquisitionFlow',
            'AICF_VehicleBoardingFlow',
            'AICF_VehicleTransitFlow',
            'AICF_VehicleDismountFlow'
        )
        foreach ($flowClass in $flowClasses) {
            $flow = Find-AICFClassRecord $records $flowClass
            if (-not (Assert-AICFComponentPresent $failures 'FLOW_CROSS_CALL' $flow $flowClass)) {
                continue
            }

            foreach ($otherFlowClass in $flowClasses) {
                if ($otherFlowClass -eq $flowClass) {
                    continue
                }
                if ($flow.Code -match ('\b' + [regex]::Escape($otherFlowClass) + '\b')) {
                    Add-AICFAuditFailure $failures 'FLOW_CROSS_CALL' "$flowClass depends directly on $otherFlowClass"
                }
            }

            Assert-AICFNotContains $failures 'FLOW_CROSS_CALL' $flow.Code '\b(?:AICF_VehicleCleanupManager|AICF_VehicleTaskHandoff|AICF_TransportTripController)\b' "$flowClass depends on an orchestration/cleanup component"
            Assert-AICFNotContains $failures 'FLOW_CROSS_CALL' $flow.Code '\.(?:CommitTransition|TransitionTo|BeginFallback|ProcessFallback|RequestCleanup)\s*\(|(?<!AICF_TripOutcome)\.ReleaseLease\s*\(' "$flowClass invokes transition, fallback, cleanup, or lease release directly"
            if (-not $FixtureMode) {
                Assert-AICFContains $failures 'FLOW_CROSS_CALL' $flow.Code '\bAICF_TripOutcome\b' "$flowClass must return a typed trip outcome"
            }
        }
    }

    if (Test-AICFRuleEnabled $OnlyRules 'WAYPOINT_SIDE_EFFECT_OWNER') {
        $handoff = Find-AICFClassRecord $records 'AICF_VehicleTaskHandoff'
        $waypointFactory = Find-AICFClassRecord $records 'AICF_VehicleWaypointFactory'
        $phaseFlowNames = @(
            'AICF_VehicleAcquisitionFlow',
            'AICF_VehicleBoardingFlow',
            'AICF_VehicleTransitFlow',
            'AICF_VehicleDismountFlow'
        )

        foreach ($flowName in $phaseFlowNames) {
            $flow = Find-AICFClassRecord $records $flowName
            if (-not $flow) {
                continue
            }
            foreach ($capability in @('\.AddWaypointAt\s*\(', '\.RemoveWaypoint\s*\(', '\bDeleteRplEntity\s*\(')) {
                if ($flow.Code -match $capability) {
                    Add-AICFAuditFailure $failures 'WAYPOINT_SIDE_EFFECT_OWNER' "$flowName owns forbidden group/waypoint side effect '$capability' ($($flow.RelativePath))"
                }
            }
        }

        foreach ($record in $records) {
            if ($record.RelativePath -notmatch '/Vehicles/' -or
                ($handoff -and $record.FullName -eq $handoff.FullName)) {
                continue
            }
            foreach ($groupQueueCapability in @('\.AddWaypointAt\s*\(', '\.RemoveWaypoint\s*\(')) {
                if ($record.Code -match $groupQueueCapability) {
                    Add-AICFAuditFailure $failures 'WAYPOINT_SIDE_EFFECT_OWNER' "Group vehicle-waypoint queue mutation '$groupQueueCapability' exists outside VehicleTaskHandoff ($($record.RelativePath))"
                }
            }
        }

        if ($waypointFactory) {
            foreach ($factorySideEffect in @('\.AddWaypointAt\s*\(', '\.RemoveWaypoint\s*\(', '\bDeleteRplEntity\s*\(')) {
                if ($waypointFactory.Code -match $factorySideEffect) {
                    Add-AICFAuditFailure $failures 'WAYPOINT_SIDE_EFFECT_OWNER' "VehicleWaypointFactory performs side effect '$factorySideEffect'; factory ownership is construction-only"
                }
            }
        }

        if (-not $FixtureMode -and
            (Assert-AICFComponentPresent $failures 'WAYPOINT_SIDE_EFFECT_OWNER' $handoff 'AICF_VehicleTaskHandoff')) {
            Assert-AICFContains $failures 'WAYPOINT_SIDE_EFFECT_OWNER' $handoff.Code '\.AddWaypointAt\s*\(' 'VehicleTaskHandoff must own group vehicle-waypoint binding'
            Assert-AICFContains $failures 'WAYPOINT_SIDE_EFFECT_OWNER' $handoff.Code '\.RemoveWaypoint\s*\(' 'VehicleTaskHandoff must own group vehicle-waypoint removal'
            Assert-AICFContains $failures 'WAYPOINT_SIDE_EFFECT_OWNER' $handoff.Code '\bDeleteRplEntity\s*\(' 'VehicleTaskHandoff must own destructive waypoint entity removal'
            [void](Assert-AICFComponentPresent $failures 'WAYPOINT_SIDE_EFFECT_OWNER' $waypointFactory 'AICF_VehicleWaypointFactory')
        }
    }

    if (Test-AICFRuleEnabled $OnlyRules 'TRANSITION_OUTSIDE_CONTROLLER') {
        $trip = Find-AICFClassRecord $records 'AICF_TransportTrip'
        $controller = Find-AICFClassRecord $records 'AICF_TransportTripController'
        if (Assert-AICFComponentPresent $failures 'TRANSITION_OUTSIDE_CONTROLLER' $trip 'AICF_TransportTrip') {
            Assert-AICFContains $failures 'TRANSITION_OUTSIDE_CONTROLLER' $trip.Code '\bCommitTransition\s*\(' 'TransportTrip must expose the single committed transition mutation boundary'
        }
        if (Assert-AICFComponentPresent $failures 'TRANSITION_OUTSIDE_CONTROLLER' $controller 'AICF_TransportTripController') {
            Assert-AICFContains $failures 'TRANSITION_OUTSIDE_CONTROLLER' $controller.Code '\.CommitTransition\s*\(' 'TransportTripController must apply typed outcomes through CommitTransition'
        }

        foreach ($record in $records) {
            if (($trip -and $record.FullName -eq $trip.FullName) -or ($controller -and $record.FullName -eq $controller.FullName)) {
                continue
            }
            foreach ($tripMutation in @('TryAttachLease', 'DetachLease', 'CommitRetarget', 'CommitTransition')) {
                if ($record.Code -match ('\.' + $tripMutation + '\s*\(')) {
                    Add-AICFAuditFailure $failures 'TRANSITION_OUTSIDE_CONTROLLER' "Trip mutation $tripMutation invoked outside TransportTripController ($($record.RelativePath))"
                }
            }
            if ($record.Code -match '\.TransitionTo\s*\(') {
                Add-AICFAuditFailure $failures 'TRANSITION_OUTSIDE_CONTROLLER' "Trip transition helper invoked outside TransportTripController ($($record.RelativePath))"
            }
            if ($record.Code -match '\bSetTripPhase\s*\(') {
                Add-AICFAuditFailure $failures 'TRANSITION_OUTSIDE_CONTROLLER' "Direct trip phase setter exists outside TransportTrip ($($record.RelativePath))"
            }
        }
    }

    if (Test-AICFRuleEnabled $OnlyRules 'TRANSITION_EFFECT_ORDER') {
        $trip = Find-AICFClassRecord $records 'AICF_TransportTrip'
        $controller = Find-AICFClassRecord $records 'AICF_TransportTripController'
        if (Assert-AICFComponentPresent $failures 'TRANSITION_EFFECT_ORDER' $trip 'AICF_TransportTrip') {
            $preflightBody = ConvertTo-AICFCodeText (Get-AICFMethodBody $trip 'CanTransitionTo')
            Assert-AICFContains $failures 'TRANSITION_EFFECT_ORDER' $preflightBody '\b(?:IsTransitionAllowedTo|IsAllowedTransition)\s*\(' 'Trip transition preflight must evaluate the authoritative allowed-transition matrix'
            Assert-AICFNotContains $failures 'TRANSITION_EFFECT_ORDER' $preflightBody '\b(?:Reset|Commit|Detach|Attach|Release)[A-Za-z0-9_]*\s*\(' 'Trip transition preflight must be side-effect free'
        }
        if (Assert-AICFComponentPresent $failures 'TRANSITION_EFFECT_ORDER' $controller 'AICF_TransportTripController') {
            $transitionBody = ConvertTo-AICFCodeText (Get-AICFMethodBody $controller 'TransitionTo')
            if (-not $transitionBody) {
                Add-AICFAuditFailure $failures 'TRANSITION_EFFECT_ORDER' 'TransportTripController has no single TransitionTo commit boundary'
            }
            else {
                $preflightOffset = $transitionBody.IndexOf('CanTransitionTo')
                $exitOffset = $transitionBody.IndexOf('ExitPhaseEffects')
                $commitOffset = $transitionBody.IndexOf('CommitTransition')
                if ($preflightOffset -lt 0 -or $exitOffset -lt 0 -or $commitOffset -lt 0 -or
                    $preflightOffset -ge $exitOffset -or $exitOffset -ge $commitOffset) {
                    Add-AICFAuditFailure $failures 'TRANSITION_EFFECT_ORDER' 'Allowed-transition preflight must succeed before exit effects, and commit must follow those effects'
                }
                Assert-AICFContains $failures 'TRANSITION_EFFECT_ORDER' $transitionBody 'if\s*\([\s\S]{0,260}!\s*trip\.CanTransitionTo\s*\([^\)]*\)[\s\S]{0,120}\)\s*return\s+false\s*;' 'CanTransitionTo must be a rejecting guard, not an informational call before exit effects'
            }
        }
    }

    if (Test-AICFRuleEnabled $OnlyRules 'WAITING_WITH_LEASE') {
        $trip = Find-AICFClassRecord $records 'AICF_TransportTrip'
        if (Assert-AICFComponentPresent $failures 'WAITING_WITH_LEASE' $trip 'AICF_TransportTrip') {
            $waitingLeaseInvariantA = 'WAITING_FOR_SITE[\s\S]{0,600}(?:HasLease\s*\(\)|m_Lease)[\s\S]{0,300}(?:return\s+false|TERMINAL_FAIL_CLOSED|FAILED_CLOSED)'
            $waitingLeaseInvariantB = '(?:HasLease\s*\(\)|m_Lease)[\s\S]{0,300}WAITING_FOR_SITE[\s\S]{0,300}(?:return\s+false|TERMINAL_FAIL_CLOSED|FAILED_CLOSED)'
            if ($trip.Code -notmatch $waitingLeaseInvariantA -and $trip.Code -notmatch $waitingLeaseInvariantB) {
                Add-AICFAuditFailure $failures 'WAITING_WITH_LEASE' 'TransportTrip does not fail closed when WAITING_FOR_SITE retains a lease'
            }
            Assert-AICFNotContains $failures 'WAITING_WITH_LEASE' $trip.Code 'WAITING_FOR_SITE[\s\S]{0,220}\.(?:TryReserveLease|ReserveLease|AcquireLease)\s*\(' 'WAITING_FOR_SITE directly acquires a lease'
        }
    }

    if (Test-AICFRuleEnabled $OnlyRules 'HANDOFF_CLEARANCE_GATE') {
        $handoff = Find-AICFClassRecord $records 'AICF_VehicleTaskHandoff'
        $handoffState = Find-AICFClassRecord $records 'AICF_VehicleHandoffState'
        if (Assert-AICFComponentPresent $failures 'HANDOFF_CLEARANCE_GATE' $handoff 'AICF_VehicleTaskHandoff') {
            $restoreBody = Get-AICFFirstMethodBody $handoff @('RestoreInfantryOrder', 'EnsureMeaningfulInfantryOrder', 'RequestOrderRestore')
            if (-not $restoreBody) {
                Add-AICFAuditFailure $failures 'HANDOFF_CLEARANCE_GATE' 'VehicleTaskHandoff has no dedicated infantry-order restore method'
            }
            else {
                $restoreCode = ConvertTo-AICFCodeText $restoreBody
                Assert-AICFNotContains $failures 'HANDOFF_CLEARANCE_GATE' $restoreCode '\b(?:Is)?ClearanceSafe\b|\bclearanceSafe\b|\bclearance_safe\b' 'Order restoration reads clearance state and can therefore be delayed by occupant cleanup'
                if (-not $FixtureMode) {
                    Assert-AICFContains $failures 'HANDOFF_CLEARANCE_GATE' $restoreCode 'GetWaypoints|IsWaypointInQueue|WaypointQueue' 'Restore success must inspect the authoritative waypoint queue'
                    Assert-AICFContains $failures 'HANDOFF_CLEARANCE_GATE' $restoreCode 'Contains\s*\(|IsWaypointInQueue' 'Restore success must prove the exact waypoint remains queued'
                    Assert-AICFContains $failures 'HANDOFF_CLEARANCE_GATE' $restoreCode 'boundToGroup|bound_to_group|IsBoundToGroup' 'Restore success must prove bound_to_group'
                    Assert-AICFContains $failures 'HANDOFF_CLEARANCE_GATE' $restoreCode 'isCurrent|is_current|GetCurrentWaypoint' 'Restore success must prove is_current'
                    Assert-AICFContains $failures 'HANDOFF_CLEARANCE_GATE' $restoreCode 'meaningful|Meaningful' 'Restore success must prove the meaningful-task postcondition'
                }
            }
        }
        if (-not $FixtureMode -and (Assert-AICFComponentPresent $failures 'HANDOFF_CLEARANCE_GATE' $handoffState 'AICF_VehicleHandoffState')) {
            $orderRestoredBody = Get-AICFMethodBody $handoffState 'IsOrderRestored'
            Assert-AICFContains $failures 'HANDOFF_CLEARANCE_GATE' (ConvertTo-AICFCodeText $orderRestoredBody) 'bound|Bound' 'Handoff state must include bound proof in IsOrderRestored'
            Assert-AICFContains $failures 'HANDOFF_CLEARANCE_GATE' (ConvertTo-AICFCodeText $orderRestoredBody) 'current|Current' 'Handoff state must include current proof in IsOrderRestored'
            Assert-AICFContains $failures 'HANDOFF_CLEARANCE_GATE' (ConvertTo-AICFCodeText $orderRestoredBody) 'queue|Queue' 'Handoff state must include queue proof in IsOrderRestored'
            Assert-AICFContains $failures 'HANDOFF_CLEARANCE_GATE' (ConvertTo-AICFCodeText $orderRestoredBody) 'meaningful|Meaningful' 'Handoff state must include meaningful-task proof in IsOrderRestored'
        }
    }

    if (Test-AICFRuleEnabled $OnlyRules 'CLEANUP_CLEARANCE_OWNER') {
        $controller = Find-AICFClassRecord $records 'AICF_TransportTripController'
        $handoff = Find-AICFClassRecord $records 'AICF_VehicleTaskHandoff'
        $dismount = Find-AICFClassRecord $records 'AICF_VehicleDismountFlow'
        $cleanup = Find-AICFClassRecord $records 'AICF_VehicleCleanupManager'
        $handoffState = Find-AICFClassRecord $records 'AICF_VehicleHandoffState'

        if ($handoff) {
            Assert-AICFNotContains $failures 'CLEANUP_CLEARANCE_OWNER' $handoff.Code '\.RecordClearanceResult\s*\(\s*true\s*\)|\bm_bClearanceSafe\s*=\s*true\b' 'VehicleTaskHandoff must not manufacture full physical-clearance proof'
            Assert-AICFNotContains $failures 'CLEANUP_CLEARANCE_OWNER' $handoff.Code '(?<!AICF_TripOutcome)\.(?:ReleaseLease|ReleaseLeaseAt|RetireLeaseAt)\s*\(' 'VehicleTaskHandoff must not release a physical lease directly'
        }

        if ($dismount) {
            Assert-AICFNotContains $failures 'CLEANUP_CLEARANCE_OWNER' $dismount.Code '\.RecordClearanceResult\s*\(|\.RequestLeaseRelease\s*\(' 'Managed dismount may report a typed release request but must not assert full clearance or request cleanup directly'
            Assert-AICFNotContains $failures 'CLEANUP_CLEARANCE_OWNER' $dismount.Code '(?<!AICF_TripOutcome)\.(?:ReleaseLease|ReleaseLeaseAt|RetireLeaseAt)\s*\(' 'Managed dismount must not mutate Fleet lease ownership directly'
            if (-not $FixtureMode) {
                Assert-AICFContains $failures 'CLEANUP_CLEARANCE_OWNER' $dismount.Code '\bAICF_TripOutcome\.ReleaseLease\s*\(' 'Managed dismount release evidence must remain a typed scan request for the controller'
            }
        }

        if (Assert-AICFComponentPresent $failures 'CLEANUP_CLEARANCE_OWNER' $controller 'AICF_TransportTripController') {
            Assert-AICFNotContains $failures 'CLEANUP_CLEARANCE_OWNER' $controller.Code '(?<!AICF_TripOutcome)\.(?:ReleaseLease|ReleaseLeaseAt|RetireLeaseAt)\s*\(' 'TransportTripController must not bypass CleanupManager to release a physical lease'

            $forwardReleaseBody = ConvertTo-AICFCodeText (Get-AICFMethodBody $controller 'ForwardLeaseRelease')
            if ($forwardReleaseBody) {
                Assert-AICFContains $failures 'CLEANUP_CLEARANCE_OWNER' $forwardReleaseBody '\.RequestLeaseRelease\s*\(' 'Controller must translate managed dismount release evidence into a cleanup scan request'
                Assert-AICFNotContains $failures 'CLEANUP_CLEARANCE_OWNER' $forwardReleaseBody '\.RecordClearanceResult\s*\(|(?<!AICF_TripOutcome)\.(?:ReleaseLease|ReleaseLeaseAt|RetireLeaseAt)\s*\(' 'Controller release forwarding must not assert clearance or mutate Fleet ownership'
            }
            elseif (-not $FixtureMode) {
                Add-AICFAuditFailure $failures 'CLEANUP_CLEARANCE_OWNER' 'TransportTripController has no bounded ForwardLeaseRelease scan-request boundary'
            }

            $clearanceCalls = [regex]::Matches($controller.Code, '\.RecordClearanceResult\s*\(\s*([^\)]+)\)')
            $forwardedClearanceCount = 0
            foreach ($call in $clearanceCalls) {
                $argument = $call.Groups[1].Value.Trim()
                if ($argument -eq 'false') {
                    continue
                }
                if ($argument -eq 'clearanceSafe') {
                    $forwardedClearanceCount++
                    continue
                }
                if ($argument -ne 'true') {
                    Add-AICFAuditFailure $failures 'CLEANUP_CLEARANCE_OWNER' "Controller clearance assertion uses an unaudited expression '$argument'"
                    continue
                }

                $prefixStart = [Math]::Max(0, $call.Index - 900)
                $prefix = $controller.Code.Substring($prefixStart, $call.Index - $prefixStart)
                $guardedNoLease = $prefix -match 'if\s*\(\s*!\s*trip\.GetLease\s*\(\s*\)\s*\)\s*\{[^\{\}]*$'
                $guardedEmptyReservation = $prefix -match 'if\s*\(\s*!\s*trip\.GetLease\s*\(\s*\)\.HasPhysicalAsset\s*\(\s*\)\s*\)\s*\{[^\{\}]*ReleaseEmptyReservationIfPresent\s*\([^\)]*\)[^\{\}]*$'
                if (-not $guardedNoLease -and -not $guardedEmptyReservation) {
                    Add-AICFAuditFailure $failures 'CLEANUP_CLEARANCE_OWNER' 'Controller asserts clearance=true without proving that no physical lease remains'
                }
            }

            $beginEvidenceBody = ConvertTo-AICFCodeText (Get-AICFMethodBody $controller 'BeginHandoffEvidence')
            $bodyForwardedCount = [regex]::Matches($beginEvidenceBody, '\.RecordClearanceResult\s*\(\s*clearanceSafe\s*\)').Count
            if ($forwardedClearanceCount -ne $bodyForwardedCount) {
                Add-AICFAuditFailure $failures 'CLEANUP_CLEARANCE_OWNER' 'Controller forwards a clearance boolean outside its audited BeginHandoffEvidence boundary'
            }
            foreach ($beginCall in [regex]::Matches($controller.Code, '(?m)^\s*BeginHandoffEvidence\s*\(([\s\S]*?)\)\s*;')) {
                $arguments = $beginCall.Groups[1].Value
                if ($arguments -notmatch ',\s*(?:false|!\s*trip\.GetLease\s*\(\s*\))\s*$') {
                    Add-AICFAuditFailure $failures 'CLEANUP_CLEARANCE_OWNER' 'BeginHandoffEvidence may assert clearance only from the explicit no-lease predicate'
                }
            }
        }

        foreach ($record in $records) {
            if (($controller -and $record.FullName -eq $controller.FullName) -or
                ($cleanup -and $record.FullName -eq $cleanup.FullName) -or
                ($handoffState -and $record.FullName -eq $handoffState.FullName)) {
                continue
            }
            if ($record.Code -match '\.RecordClearanceResult\s*\(\s*true\s*\)|\bm_bClearanceSafe\s*=\s*true\b') {
                Add-AICFAuditFailure $failures 'CLEANUP_CLEARANCE_OWNER' "Full clearance is asserted outside CleanupManager/no-physical-lease controller proof ($($record.RelativePath))"
            }
        }
    }

    if (Test-AICFRuleEnabled $OnlyRules 'CLEANUP_IDENTITY_SAFETY') {
        $cleanup = Find-AICFClassRecord $records 'AICF_VehicleCleanupManager'
        $cleanupState = Find-AICFClassRecord $records 'AICF_VehicleCleanupState'
        $lease = Find-AICFClassRecord $records 'AICF_VehicleLease'
        if (Assert-AICFComponentPresent $failures 'CLEANUP_IDENTITY_SAFETY' $cleanup 'AICF_VehicleCleanupManager') {
            $identitySource = $cleanup.Source
            if ($cleanupState) { $identitySource += "`n" + $cleanupState.Source }
            if ($lease) { $identitySource += "`n" + $lease.Source }
            $identityCode = ConvertTo-AICFCodeText $identitySource

            foreach ($identityPattern in @(
                'last_?entity_?id|LastEntityId',
                'last_?rpl_?id|LastRplId',
                'last_?origin|LastOrigin',
                'prefab|Prefab',
                'release(?:d)?_?(?:at|time)|Release(?:d)?At',
                'cleanup_?trigger|CleanupTrigger'
            )) {
                Assert-AICFContains $failures 'CLEANUP_IDENTITY_SAFETY' $identityCode $identityPattern "Immutable cleanup snapshot is missing '$identityPattern'"
            }

            Assert-AICFContains $failures 'CLEANUP_IDENTITY_SAFETY' $cleanup.Code 'STABLE_CLEAR|StableClear|5000' 'Cleanup must enforce a continuous stable-clear window'
            Assert-AICFContains $failures 'CLEANUP_IDENTITY_SAFETY' $cleanup.Code 'EntityID|GetID\s*\(' 'Cleanup must validate EntityID'
            Assert-AICFContains $failures 'CLEANUP_IDENTITY_SAFETY' $cleanup.Code 'RplId|RplComponent|\.Id\s*\(' 'Cleanup must validate replicated identity'
            Assert-AICFContains $failures 'CLEANUP_IDENTITY_SAFETY' $cleanup.Code 'FindEntityByID' 'Delete confirmation must resolve the immutable EntityID'
            Assert-AICFContains $failures 'CLEANUP_IDENTITY_SAFETY' $cleanup.Code 'DeleteRplEntity\s*\(' 'Cleanup manager must own replicated vehicle deletion'

            $deleteOffset = $cleanup.Code.IndexOf('DeleteRplEntity')
            if ($deleteOffset -ge 0) {
                $beforeDelete = $cleanup.Code.Substring(0, $deleteOffset)
                $scanCount = [regex]::Matches($beforeDelete, '(?:InspectProtectedCleanupUse|ScanProtectedCleanup|InspectCleanupSafety|CanDeleteVehicleSafely)\s*\(').Count
                if ($scanCount -lt 2) {
                    Add-AICFAuditFailure $failures 'CLEANUP_IDENTITY_SAFETY' 'Delete path lacks initial safety scan plus immediate pre-delete re-scan'
                }
            }
            Assert-AICFContains $failures 'CLEANUP_IDENTITY_SAFETY' $cleanup.Code 'CONFIRM|Confirm|confirmation|Confirmation' 'Cleanup must retain delete confirmation state'
            Assert-AICFContains $failures 'CLEANUP_IDENTITY_SAFETY' $cleanup.Code 'FAIL_CLOSED|FailClosed|RETAIN|Retain|STOP_CLEANUP_RETAINED' 'Cleanup timeout/identity mismatch must retain fail-closed state'
        }
        if (-not $FixtureMode) {
            [void](Assert-AICFComponentPresent $failures 'CLEANUP_IDENTITY_SAFETY' $cleanupState 'AICF_VehicleCleanupState')
            [void](Assert-AICFComponentPresent $failures 'CLEANUP_IDENTITY_SAFETY' $lease 'AICF_VehicleLease')
        }
    }

    if (Test-AICFRuleEnabled $OnlyRules 'ACQUISITION_SPAWN_OWNER') {
        $acquisition = Find-AICFClassRecord $records 'AICF_VehicleAcquisitionFlow'
        $spawner = Find-AICFClassRecord $records 'AICF_VehicleSpawner'
        if (Assert-AICFComponentPresent $failures 'ACQUISITION_SPAWN_OWNER' $acquisition 'AICF_VehicleAcquisitionFlow') {
            Assert-AICFContains $failures 'ACQUISITION_SPAWN_OWNER' $acquisition.Code '\bAICF_VehicleSpawner\b|\.TrySpawn\s*\(' 'Acquisition must be the sole high-level vehicle spawn caller'
            Assert-AICFContains $failures 'ACQUISITION_SPAWN_OWNER' $acquisition.Code '\bAICF_TripOutcome\b' 'Acquisition must return typed outcomes'
        }
        if (Assert-AICFComponentPresent $failures 'ACQUISITION_SPAWN_OWNER' $spawner 'AICF_VehicleSpawner') {
            Assert-AICFContains $failures 'ACQUISITION_SPAWN_OWNER' $spawner.Code 'Replication\.IsServer\s*\(' 'Vehicle spawn helper must be server authoritative'
            Assert-AICFContains $failures 'ACQUISITION_SPAWN_OWNER' $spawner.Code 'FindAllEmptyTerrainPositions' 'Acquisition must evaluate multiple safe-site positions'
            Assert-AICFContains $failures 'ACQUISITION_SPAWN_OWNER' $spawner.Code 'IsWheeledSpawnSurfaceSuitable' 'Acquisition must reject water/undrivable surfaces before spawn'
            Assert-AICFContains $failures 'ACQUISITION_SPAWN_OWNER' $spawner.Code 'preflightOnly' 'Acquisition must support entity-free WAITING_FOR_SITE preflight'
            $spawnCount = [regex]::Matches($spawner.Code, '\bSpawnEntityPrefabEx\s*\(').Count
            if ($spawnCount -ne 1) {
                Add-AICFAuditFailure $failures 'ACQUISITION_SPAWN_OWNER' "VehicleSpawner must have one authoritative SpawnEntityPrefabEx boundary; found $spawnCount"
            }
        }

        foreach ($record in $records) {
            if (($acquisition -and $record.FullName -eq $acquisition.FullName) -or ($spawner -and $record.FullName -eq $spawner.FullName)) {
                continue
            }
            if ($record.Code -match '\bSpawnEntityPrefabEx\s*\(' -and
                $record.RelativePath -match '/Vehicles/' -and
                $record.Name -ne 'AICF_VehicleWaypointFactory.c') {
                Add-AICFAuditFailure $failures 'ACQUISITION_SPAWN_OWNER' "Vehicle-domain entity spawn exists outside acquisition/spawner ($($record.RelativePath))"
            }
            if ($record.Code -match '\bAICF_VehicleSpawner\b[\s\S]{0,500}\.TrySpawn\s*\(') {
                Add-AICFAuditFailure $failures 'ACQUISITION_SPAWN_OWNER' "VehicleSpawner is invoked outside VehicleAcquisitionFlow ($($record.RelativePath))"
            }
        }
    }

    if (Test-AICFRuleEnabled $OnlyRules 'FLEET_OWNS_LEASE_CAP_GENERATION') {
        $fleet = Find-AICFClassRecord $records 'AICF_FactionFleet'
        $lease = Find-AICFClassRecord $records 'AICF_VehicleLease'
        if (Assert-AICFComponentPresent $failures 'FLEET_OWNS_LEASE_CAP_GENERATION' $fleet 'AICF_FactionFleet') {
            foreach ($methodName in @('TryReserveLease', 'BindReservedLeaseVehicle', 'ReleaseLease', 'AddWorldPoolAsset')) {
                Assert-AICFContains $failures 'FLEET_OWNS_LEASE_CAP_GENERATION' $fleet.Code ('\b' + $methodName + '\s*\(') "FactionFleet is missing $methodName"
            }
            Assert-AICFContains $failures 'FLEET_OWNS_LEASE_CAP_GENERATION' $fleet.Code 'GetMaxVehiclesPerFaction|maximumLeases|maxVehicles|MAX_VEHICLES|MaximumActiveOrReserved|MAX_ACTIVE_OR_RESERVED' 'FactionFleet must enforce the per-faction lease cap'
            Assert-AICFContains $failures 'FLEET_OWNS_LEASE_CAP_GENERATION' $fleet.Code 'slotId|SlotId' 'FactionFleet must enforce one lease per stable slot'
        }
        [void](Assert-AICFComponentPresent $failures 'FLEET_OWNS_LEASE_CAP_GENERATION' $lease 'AICF_VehicleLease')

        $allCode = ($records | ForEach-Object { $_.Code }) -join "`n"
        Assert-AICFNotContains $failures 'FLEET_OWNS_LEASE_CAP_GENERATION' $allCode '\bm_a(?:US|USSR)(?:Runtime|Vehicle|WorldPool|Cooldown|Generation|Lease)[A-Za-z0-9_]*\b' 'Parallel US/USSR vehicle runtime/cap/pool arrays are forbidden'
        Assert-AICFNotContains $failures 'FLEET_OWNS_LEASE_CAP_GENERATION' $allCode 'if\s*\([^\)]*vehicle\s*!=\s*null[^\)]*\)[\s\S]{0,120}(?:count\+\+|active\+\+|reserved\+\+)' 'Vehicle pointer presence must not be used as lease cap accounting'

        foreach ($record in $records) {
            if (($fleet -and $record.FullName -eq $fleet.FullName) -or ($lease -and $record.FullName -eq $lease.FullName)) {
                continue
            }
            if ($record.Code -match '\bm_i(?:AcceptedVehicle|NextLease)Generation\s*(?:\+\+|--|=)') {
                Add-AICFAuditFailure $failures 'FLEET_OWNS_LEASE_CAP_GENERATION' "Accepted/lease generation counter is mutated outside Fleet ($($record.RelativePath))"
            }
            if ($record.Code -match '\bm_i(?:Lease|Vehicle)Generation\s*(?:\+\+|--)') {
                Add-AICFAuditFailure $failures 'FLEET_OWNS_LEASE_CAP_GENERATION' "Lease/vehicle generation is incremented outside Fleet/Lease ($($record.RelativePath))"
            }
            foreach ($generationAssignment in [regex]::Matches($record.Code, '(?m)\bm_i(Lease|Vehicle)Generation\s*=\s*([^;]+);')) {
                $generationKind = $generationAssignment.Groups[1].Value
                $rightHandSide = $generationAssignment.Groups[2].Value
                if ($rightHandSide -notmatch ('\.Get' + $generationKind + 'Generation\s*\(')) {
                    Add-AICFAuditFailure $failures 'FLEET_OWNS_LEASE_CAP_GENERATION' "$generationKind generation is authored rather than immutably copied outside Fleet/Lease ($($record.RelativePath))"
                }
            }
        }
    }

    if (Test-AICFRuleEnabled $OnlyRules 'PHASE_LOCAL_STATE_RESET') {
        foreach ($stateClass in @(
            'AICF_VehicleRequestState',
            'AICF_VehicleBoardingState',
            'AICF_VehicleMovementState',
            'AICF_VehicleDismountState',
            'AICF_VehicleHandoffState',
            'AICF_VehicleCleanupState'
        )) {
            $stateRecord = Find-AICFClassRecord $records $stateClass
            if (-not (Assert-AICFComponentPresent $failures 'PHASE_LOCAL_STATE_RESET' $stateRecord $stateClass)) {
                continue
            }
            $classBody = Get-AICFBracedBody $stateRecord.Source ('(?m)^\s*class\s+' + [regex]::Escape($stateClass) + '\b')
            Assert-AICFContains $failures 'PHASE_LOCAL_STATE_RESET' (ConvertTo-AICFCodeText $classBody) '\bReset\s*\(' "$stateClass must expose an explicit phase-exit Reset"
        }
        foreach ($record in $records) {
            if ($record.Code -match '\bclass\s+AICF_VehicleRuntime\b') {
                Add-AICFAuditFailure $failures 'PHASE_LOCAL_STATE_RESET' "Obsolete shared AICF_VehicleRuntime still exists ($($record.RelativePath))"
            }
        }
    }

    if (Test-AICFRuleEnabled $OnlyRules 'DISMOUNT_NORMAL_NONDESTRUCTIVE') {
        $dismount = Find-AICFClassRecord $records 'AICF_VehicleDismountFlow'
        if (Assert-AICFComponentPresent $failures 'DISMOUNT_NORMAL_NONDESTRUCTIVE' $dismount 'AICF_VehicleDismountFlow') {
            $normalBody = Get-AICFFirstMethodBody $dismount @('ProcessNormalDismount', 'UpdateNormalDismount', 'ProcessNormal')
            if (-not $normalBody) {
                Add-AICFAuditFailure $failures 'DISMOUNT_NORMAL_NONDESTRUCTIVE' 'Dismount flow must isolate its normal path in a dedicated method'
            }
            else {
                $normalCode = ConvertTo-AICFCodeText $normalBody
                Assert-AICFNotContains $failures 'DISMOUNT_NORMAL_NONDESTRUCTIVE' $normalCode '\.Teleport\s*\(|\bEjectOccupant\s*\(|\bSetWorldTransform\s*\(|\bGetOutVehicle_NoDoor\s*\(' 'Normal dismount contains terminal relocation/ejection capability'
                if (-not $FixtureMode) {
                    Assert-AICFContains $failures 'DISMOUNT_NORMAL_NONDESTRUCTIVE' $normalCode 'Guide|Movement|MoveIndividually|FindEmptyTerrainPosition' 'Normal dismount must provide bounded physical guidance for trapped members'
                }
            }
        }
    }

    if (Test-AICFRuleEnabled $OnlyRules 'DOMAIN_IDENTITIES') {
        $trip = Find-AICFClassRecord $records 'AICF_TransportTrip'
        $lease = Find-AICFClassRecord $records 'AICF_VehicleLease'
        if (Assert-AICFComponentPresent $failures 'DOMAIN_IDENTITIES' $trip 'AICF_TransportTrip') {
            foreach ($pattern in @('FactionKey', 'SlotId', 'GroupGeneration', 'TripGeneration', 'operation|Operation', 'causation|Causation', 'deadline|Deadline', 'terminal|Terminal')) {
                Assert-AICFContains $failures 'DOMAIN_IDENTITIES' $trip.Source $pattern "TransportTrip identity/state is missing '$pattern'"
            }
        }
        if (Assert-AICFComponentPresent $failures 'DOMAIN_IDENTITIES' $lease 'AICF_VehicleLease') {
            foreach ($pattern in @('FactionKey', 'SlotId', 'GroupGeneration', 'LeaseGeneration', 'VehicleGeneration', 'lifecycle|Lifecycle', 'EntityID|EntityId', 'RplId|RplID', 'prefab|Prefab')) {
                Assert-AICFContains $failures 'DOMAIN_IDENTITIES' $lease.Source $pattern "VehicleLease identity is missing '$pattern'"
            }
        }
    }

    if (Test-AICFRuleEnabled $OnlyRules 'VEHICLE_LIVENESS_OWNERSHIP') {
        $coordinator = Find-AICFClassRecord $records 'AICF_VehicleCoordinator'
        $controller = Find-AICFClassRecord $records 'AICF_TransportTripController'
        $handoff = Find-AICFClassRecord $records 'AICF_VehicleTaskHandoff'
        $matchController = Find-AICFClassRecord $records 'AICF_MatchController'

        if (Assert-AICFComponentPresent $failures 'VEHICLE_LIVENESS_OWNERSHIP' $controller 'AICF_TransportTripController') {
            $abortBody = ConvertTo-AICFCodeText (Get-AICFMethodBody $controller 'AbortForStop')
            Assert-AICFContains $failures 'VEHICLE_LIVENESS_OWNERSHIP' $abortBody 'TransitionTo\s*\(' 'Controller stop path must commit live Trips through the normal exit-effect boundary'
            Assert-AICFContains $failures 'VEHICLE_LIVENESS_OWNERSHIP' $abortBody 'CancelResidualOwnedEffectsForStop\s*\(' 'Controller stop path must cancel terminal-clearance residual actions and waypoints'
            Assert-AICFContains $failures 'VEHICLE_LIVENESS_OWNERSHIP' $abortBody 'RestoreInfantryOrder\s*\(' 'Non-destructive stop must restore the current infantry order'

            $retargetBody = ConvertTo-AICFCodeText (Get-AICFMethodBody $controller 'ApplyCurrentRetarget')
            if ($retargetBody -notmatch 'AICF_ETransportTripPhase\.DISMOUNT[\s\S]{0,300}SuspendInfantryOrder\s*\(') {
                Add-AICFAuditFailure $failures 'VEHICLE_LIVENESS_OWNERSHIP' 'DISMOUNT retarget does not suspend the concurrently planned infantry waypoint'
            }
        }

        if (Assert-AICFComponentPresent $failures 'VEHICLE_LIVENESS_OWNERSHIP' $coordinator 'AICF_VehicleCoordinator') {
            $stopBody = ConvertTo-AICFCodeText (Get-AICFMethodBody $coordinator 'StopWithFactionContexts')
            $abortOffset = $stopBody.IndexOf('AbortForStop')
            $cleanupOffset = $stopBody.IndexOf('BeginStopLease')
            if ($abortOffset -lt 0 -or ($cleanupOffset -ge 0 -and $abortOffset -ge $cleanupOffset)) {
                Add-AICFAuditFailure $failures 'VEHICLE_LIVENESS_OWNERSHIP' 'Facade must abort Trip-owned phase side effects before queuing physical stop cleanup'
            }
        }

        if (Assert-AICFComponentPresent $failures 'VEHICLE_LIVENESS_OWNERSHIP' $handoff 'AICF_VehicleTaskHandoff') {
            $restoreBody = ConvertTo-AICFCodeText (Get-AICFMethodBody $handoff 'RestoreInfantryOrder')
            $proofOffset = $restoreBody.IndexOf('ObserveExistingInfantryOrder')
            $attemptOffset = $restoreBody.IndexOf('BeginOrderRestoreRequest')
            if ($proofOffset -lt 0 -or $attemptOffset -lt 0 -or $proofOffset -ge $attemptOffset) {
                Add-AICFAuditFailure $failures 'VEHICLE_LIVENESS_OWNERSHIP' 'Proof-only current-order observation must run before the bounded mutation-attempt gate'
            }
        }

        if (Assert-AICFComponentPresent $failures 'VEHICLE_LIVENESS_OWNERSHIP' $matchController 'AICF_MatchController') {
            $reliabilityBody = ConvertTo-AICFCodeText (Get-AICFMethodBody $matchController 'ProcessFactionReliability')
            Assert-AICFContains $failures 'VEHICLE_LIVENESS_OWNERSHIP' $reliabilityBody 'IsRestorePending\s*\(' 'Reliability must not compete with the authoritative vehicle handoff restore owner'
        }
    }

    return $failures.ToArray()
}

function New-AICFFixtureFile {
    param([string]$FixtureRoot, [string]$RelativePath, [string]$Content)

    $path = Join-Path $FixtureRoot $RelativePath
    $directory = Split-Path -Parent $path
    if (-not (Test-Path -LiteralPath $directory -PathType Container)) {
        [void](New-Item -ItemType Directory -Path $directory -Force)
    }
    Set-Content -LiteralPath $path -Value $Content -Encoding UTF8
}

function Invoke-AICFArchitectureNegativeSelfCheck {
    $selfCheckFailures = [System.Collections.Generic.List[string]]::new()
    $tempBase = [System.IO.Path]::GetFullPath([System.IO.Path]::GetTempPath()).TrimEnd([char[]]"\/")
    $fixtureRoot = Join-Path $tempBase ("aicf-static-selfcheck-" + [guid]::NewGuid().ToString('N'))

    try {
        [void](New-Item -ItemType Directory -Path $fixtureRoot)

        $fixtures = @(
            [pscustomobject]@{
                Rule = 'COORDINATOR_SIDE_EFFECT'
                Files = @{
                    'AIConflictCore/Scripts/Game/AIConflict/Vehicles/AICF_VehicleCoordinator.c' = @'
class AICF_VehicleCoordinator
{
    void Update() { GetGame().SpawnEntityPrefabEx(null, null); }
}
'@
                }
            },
            [pscustomobject]@{
                Rule = 'FLOW_CROSS_CALL'
                Files = @{
                    'AIConflictCore/Scripts/Game/AIConflict/Vehicles/AICF_VehicleAcquisitionFlow.c' = 'class AICF_VehicleAcquisitionFlow { AICF_TripOutcome Poll() { return null; } }'
                    'AIConflictCore/Scripts/Game/AIConflict/Vehicles/AICF_VehicleBoardingFlow.c' = 'class AICF_VehicleBoardingFlow { AICF_VehicleDismountFlow m_Dismount; AICF_TripOutcome Poll() { m_Dismount.Poll(); return null; } }'
                    'AIConflictCore/Scripts/Game/AIConflict/Vehicles/AICF_VehicleTransitFlow.c' = 'class AICF_VehicleTransitFlow { AICF_TripOutcome Poll() { return null; } }'
                    'AIConflictCore/Scripts/Game/AIConflict/Vehicles/AICF_VehicleDismountFlow.c' = 'class AICF_VehicleDismountFlow { AICF_TripOutcome Poll() { return null; } }'
                }
            },
            [pscustomobject]@{
                Rule = 'TRANSITION_OUTSIDE_CONTROLLER'
                ExpectedPatterns = @('TryAttachLease', 'DetachLease', 'CommitRetarget', 'CommitTransition')
                Files = @{
                    'AIConflictCore/Scripts/Game/AIConflict/State/Vehicles/AICF_TransportTrip.c' = 'class AICF_TransportTrip { bool TryAttachLease(int lease) { return true; } bool DetachLease(int lease) { return true; } bool CommitRetarget(int assignment) { return true; } bool CommitTransition(int phase) { return true; } }'
                    'AIConflictCore/Scripts/Game/AIConflict/Vehicles/AICF_TransportTripController.c' = 'class AICF_TransportTripController { void Apply(AICF_TransportTrip trip) { trip.CommitTransition(1); } }'
                    'AIConflictCore/Scripts/Game/AIConflict/Vehicles/AICF_VehicleBoardingFlow.c' = 'class AICF_VehicleBoardingFlow { void Poll(AICF_TransportTrip trip) { trip.TryAttachLease(1); trip.DetachLease(1); trip.CommitRetarget(1); trip.CommitTransition(2); } }'
                }
            },
            [pscustomobject]@{
                Rule = 'TRANSITION_EFFECT_ORDER'
                ExpectedPatterns = @('preflight must succeed before exit effects')
                Files = @{
                    'AIConflictCore/Scripts/Game/AIConflict/State/Vehicles/AICF_TransportTrip.c' = @'
class AICF_TransportTrip
{
    bool CanTransitionTo(int phase)
    {
        return IsAllowedTransition(phase);
    }
    bool IsAllowedTransition(int phase) { return true; }
}
'@
                    'AIConflictCore/Scripts/Game/AIConflict/Vehicles/AICF_TransportTripController.c' = @'
class AICF_TransportTripController
{
    bool TransitionTo(AICF_TransportTrip trip, int phase)
    {
        ExitPhaseEffects();
        if (!trip.CanTransitionTo(phase)) return false;
        return trip.CommitTransition(phase);
    }
    void ExitPhaseEffects() {}
}
'@
                }
            },
            [pscustomobject]@{
                Rule = 'WAYPOINT_SIDE_EFFECT_OWNER'
                ExpectedPatterns = @('AICF_VehicleBoardingFlow owns forbidden', 'factory ownership is construction-only')
                Files = @{
                    'AIConflictCore/Scripts/Game/AIConflict/Vehicles/AICF_VehicleBoardingFlow.c' = 'class AICF_VehicleBoardingFlow { void Poll(SCR_AIGroup group, AIWaypoint waypoint) { group.AddWaypointAt(waypoint, 0); } }'
                    'AIConflictCore/Scripts/Game/AIConflict/Vehicles/AICF_VehicleWaypointFactory.c' = 'class AICF_VehicleWaypointFactory { void Destroy(SCR_AIGroup group, AIWaypoint waypoint) { group.RemoveWaypoint(waypoint); RplComponent.DeleteRplEntity(waypoint, false); } }'
                }
            },
            [pscustomobject]@{
                Rule = 'WAITING_WITH_LEASE'
                Files = @{
                    'AIConflictCore/Scripts/Game/AIConflict/State/Vehicles/AICF_TransportTrip.c' = 'class AICF_TransportTrip { int m_Phase; ref AICF_VehicleLease m_Lease; bool HasLease() { return m_Lease != null; } bool Validate() { if (m_Phase == AICF_ETransportTripPhase.WAITING_FOR_SITE) return HasLease(); return true; } }'
                }
            },
            [pscustomobject]@{
                Rule = 'HANDOFF_CLEARANCE_GATE'
                Files = @{
                    'AIConflictCore/Scripts/Game/AIConflict/Vehicles/AICF_VehicleTaskHandoff.c' = @'
class AICF_VehicleTaskHandoff
{
    bool RestoreInfantryOrder(AICF_VehicleHandoffState state)
    {
        if (!state.IsClearanceSafe()) return false;
        return true;
    }
}
'@
                }
            },
            [pscustomobject]@{
                Rule = 'CLEANUP_CLEARANCE_OWNER'
                ExpectedPatterns = @('must not bypass CleanupManager', 'asserts clearance=true without proving')
                Files = @{
                    'AIConflictCore/Scripts/Game/AIConflict/Vehicles/AICF_TransportTripController.c' = @'
class AICF_TransportTripController
{
    void Unsafe(AICF_TransportTrip trip, AICF_FactionFleet fleet, AICF_VehicleLease lease)
    {
        trip.GetHandoffState().RecordClearanceResult(true);
        fleet.ReleaseLeaseAt(lease, true, "fixture", vector.Zero, 1, null);
    }
}
'@
                }
            },
            [pscustomobject]@{
                Rule = 'CLEANUP_IDENTITY_SAFETY'
                Files = @{
                    'AIConflictCore/Scripts/Game/AIConflict/Vehicles/AICF_VehicleCleanupManager.c' = 'class AICF_VehicleCleanupManager { void Delete(Vehicle vehicle) { RplComponent.DeleteRplEntity(vehicle, false); } }'
                }
            },
            [pscustomobject]@{
                Rule = 'VEHICLE_LIVENESS_OWNERSHIP'
                ExpectedPatterns = @(
                    'stop path must commit live Trips',
                    'DISMOUNT retarget does not suspend',
                    'Facade must abort Trip-owned phase side effects',
                    'Proof-only current-order observation must run before',
                    'Reliability must not compete'
                )
                Files = @{
                    'AIConflictCore/Scripts/Game/AIConflict/Vehicles/AICF_TransportTripController.c' = @'
class AICF_TransportTripController
{
    void AbortForStop() {}
    void ApplyCurrentRetarget()
    {
        if (phase == AICF_ETransportTripPhase.TRANSIT)
            SuspendInfantryOrder();
    }
}
'@
                    'AIConflictCore/Scripts/Game/AIConflict/Vehicles/AICF_VehicleCoordinator.c' = @'
class AICF_VehicleCoordinator
{
    void StopWithFactionContexts()
    {
        BeginStopLease();
    }
}
'@
                    'AIConflictCore/Scripts/Game/AIConflict/Vehicles/AICF_VehicleTaskHandoff.c' = @'
class AICF_VehicleTaskHandoff
{
    void RestoreInfantryOrder()
    {
        BeginOrderRestoreRequest();
        ObserveExistingInfantryOrder();
    }
}
'@
                    'AIConflictCore/Scripts/Game/AIConflict/Bootstrap/AICF_MatchController.c' = @'
class AICF_MatchController
{
    void ProcessFactionReliability()
    {
        IsControllingMovement();
    }
}
'@
                }
            }
        )

        foreach ($fixture in $fixtures) {
            $caseName = $fixture.Rule
            if ($fixture.PSObject.Properties['Case'] -and $fixture.Case) {
                $caseName += '-' + $fixture.Case
            }
            $caseRoot = Join-Path $fixtureRoot $caseName
            foreach ($relativePath in $fixture.Files.Keys) {
                New-AICFFixtureFile $caseRoot $relativePath $fixture.Files[$relativePath]
            }

            $fixtureFailures = @(Invoke-AICFVehicleArchitectureAudit -RepositoryRoot $caseRoot -OnlyRules @($fixture.Rule) -FixtureMode)
            $expectedPrefix = '[' + $fixture.Rule + ']'
            $matched = @($fixtureFailures | Where-Object { $_.StartsWith($expectedPrefix) }).Count -gt 0
            if (-not $matched) {
                Add-AICFAuditFailure $selfCheckFailures 'NEGATIVE_SELF_CHECK' "Broken fixture did not trigger expected rule $($fixture.Rule)"
                continue
            }
            if ($fixture.PSObject.Properties['ExpectedPatterns']) {
                foreach ($expectedPattern in $fixture.ExpectedPatterns) {
                    $specificMatch = @($fixtureFailures | Where-Object {
                        $_.StartsWith($expectedPrefix) -and $_ -match $expectedPattern
                    }).Count -gt 0
                    if (-not $specificMatch) {
                        Add-AICFAuditFailure $selfCheckFailures 'NEGATIVE_SELF_CHECK' "Broken fixture $caseName did not trigger expected semantic evidence '$expectedPattern'"
                    }
                }
            }
        }
    }
    finally {
        $fullFixtureRoot = [System.IO.Path]::GetFullPath($fixtureRoot)
        if ($fullFixtureRoot.StartsWith($tempBase, [System.StringComparison]::OrdinalIgnoreCase) -and
            (Split-Path -Leaf $fullFixtureRoot).StartsWith('aicf-static-selfcheck-', [System.StringComparison]::OrdinalIgnoreCase) -and
            (Test-Path -LiteralPath $fullFixtureRoot -PathType Container)) {
            Remove-Item -LiteralPath $fullFixtureRoot -Recurse -Force
        }
    }

    return $selfCheckFailures.ToArray()
}

function Get-AICFSourceLineNumber {
    param([string]$Source, [int]$Offset)
    if ($Offset -le 0) { return 1 }
    return 1 + [regex]::Matches($Source.Substring(0, $Offset), "`n").Count
}

function Get-AICFStringFormatCallInfo {
    param([string]$Source, [int]$OpenParenOffset)

    $depth = 1
    $argumentCount = 1
    $hasContent = $false
    $inString = $false
    $inLineComment = $false
    $inBlockComment = $false
    $literal = ''

    for ($offset = $OpenParenOffset + 1; $offset -lt $Source.Length; $offset++) {
        $character = $Source[$offset]
        $nextCharacter = [char]0
        if ($offset + 1 -lt $Source.Length) { $nextCharacter = $Source[$offset + 1] }

        if ($inLineComment) {
            if ($character -eq "`n") { $inLineComment = $false }
            continue
        }
        if ($inBlockComment) {
            if ($character -eq '*' -and $nextCharacter -eq '/') { $inBlockComment = $false; $offset++ }
            continue
        }
        if ($inString) {
            if ($character -eq '\') { $offset++; continue }
            if ($character -eq '"') { $inString = $false }
            continue
        }
        if ($character -eq '/' -and $nextCharacter -eq '/') { $inLineComment = $true; $offset++; continue }
        if ($character -eq '/' -and $nextCharacter -eq '*') { $inBlockComment = $true; $offset++; continue }
        if ($character -eq '"') { $inString = $true; $hasContent = $true; continue }
        if ($character -eq '(' -or $character -eq '[' -or $character -eq '{') { $depth++; $hasContent = $true; continue }
        if ($character -eq ')' -or $character -eq ']' -or $character -eq '}') {
            $depth--
            if ($depth -eq 0) {
                if (-not $hasContent) { $argumentCount = 0 }
                return [pscustomobject]@{ Closed = $true; ArgumentCount = $argumentCount; Literal = $literal }
            }
            continue
        }
        if ($character -eq ',' -and $depth -eq 1) { $argumentCount++; continue }
        if (-not [char]::IsWhiteSpace($character)) { $hasContent = $true }
    }

    return [pscustomobject]@{ Closed = $false; ArgumentCount = $argumentCount; Literal = $literal }
}

function Invoke-AICFLanguageAudit {
    param([string]$RepositoryRoot)

    $failures = [System.Collections.Generic.List[string]]::new()
    $records = @(Get-AICFSourceRecords $RepositoryRoot @('AIConflictCore', 'AIConflictArland'))
    foreach ($record in $records) {
        if ($record.Code -match '\?') {
            Add-AICFAuditFailure $failures 'ENFORCE_TERNARY' "C-style ternary token is forbidden ($($record.RelativePath))"
        }

        $source = $record.Source
        $code = $record.Code
        foreach ($formatMatch in [regex]::Matches($code, '\bstring\.Format\s*\(')) {
            $openParenOffset = $code.IndexOf('(', $formatMatch.Index)
            $callInfo = Get-AICFStringFormatCallInfo $source $openParenOffset
            $line = Get-AICFSourceLineNumber $source $formatMatch.Index
            if (-not $callInfo.Closed) {
                Add-AICFAuditFailure $failures 'STRING_FORMAT_ARITY' "Unclosed string.Format call ($($record.RelativePath):$line)"
                continue
            }
            $substitutions = [Math]::Max(0, $callInfo.ArgumentCount - 1)
            if ($substitutions -gt 9) {
                Add-AICFAuditFailure $failures 'STRING_FORMAT_ARITY' "string.Format has $substitutions substitutions; maximum is 9 ($($record.RelativePath):$line)"
            }
        }

        foreach ($placeholderMatch in [regex]::Matches($record.Strings, '%([0-9]{2,})')) {
            $placeholder = 0
            if ([int]::TryParse($placeholderMatch.Groups[1].Value, [ref]$placeholder) -and $placeholder -gt 9) {
                Add-AICFAuditFailure $failures 'STRING_FORMAT_PLACEHOLDER' "String literal uses unsupported placeholder %$placeholder ($($record.RelativePath))"
            }
        }
    }

    return $failures.ToArray()
}
