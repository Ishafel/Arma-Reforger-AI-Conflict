param(
    [string]$RepositoryRoot = (Split-Path -Parent $PSScriptRoot)
)

$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'Stage3StaticAudit.Common.ps1')

$failures = [System.Collections.Generic.List[string]]::new()

function Get-AICommanderRecordByName {
    param([object[]]$Records, [string]$Name)

    $matches = @($Records | Where-Object { $_.Name -eq $Name })
    if ($matches.Count -eq 1) {
        return $matches[0]
    }
    return $null
}

function Require-AICommanderRecord {
    param([object]$Record, [string]$Name)

    if ($Record) {
        return $true
    }
    Add-AICFAuditFailure $failures 'AI_COMMANDER_COMPONENT' "Missing unique production source $Name"
    return $false
}

function Require-AICommanderClass {
    param([object]$Record, [string]$ClassName)

    if ($Record) {
        return $true
    }
    Add-AICFAuditFailure $failures 'AI_COMMANDER_COMPONENT' "Missing unique production class $ClassName"
    return $false
}

function Assert-AICommanderOrdered {
    param(
        [string]$RuleId,
        [string]$Text,
        [string]$First,
        [string]$Second,
        [string]$Message
    )

    $firstOffset = $Text.IndexOf($First, [System.StringComparison]::Ordinal)
    $secondOffset = $Text.IndexOf($Second, [System.StringComparison]::Ordinal)
    if ($firstOffset -lt 0 -or $secondOffset -lt 0 -or $firstOffset -ge $secondOffset) {
        Add-AICFAuditFailure $failures $RuleId $Message
    }
}

function Assert-AICommanderMethodPresent {
    param(
        [object]$Record,
        [string]$MethodName,
        [string]$RuleId
    )

    $body = Get-AICFMethodBody $Record $MethodName
    if (-not $body) {
        Add-AICFAuditFailure $failures $RuleId "Missing method $MethodName"
    }
    return $body
}

$records = @(Get-AICFSourceRecords $RepositoryRoot)
$arlandRecords = @(Get-AICFSourceRecords $RepositoryRoot @(
    'AIConflictArland/Scripts/Game/AIConflictArland'
))
if ($records.Count -eq 0) {
    Add-AICFAuditFailure $failures 'AI_COMMANDER_SOURCE_TREE' 'No AIConflictCore Enforce sources were found'
}

$stage1Config = Get-AICommanderRecordByName $records 'AICF_Stage1Config.c'
$matchController = Find-AICFClassRecord $records 'AICF_MatchController'
$authorityPolicy = Find-AICFClassRecord $records 'AICF_CommandAuthorityPolicy'
$aiCommander = Find-AICFClassRecord $records 'AICF_AICommander'
$orderPlanner = Find-AICFClassRecord $records 'AICF_OrderPlanner'
$groupSlot = Find-AICFClassRecord $records 'AICF_GroupSlot'
$campaignState = Find-AICFClassRecord $records 'SCR_GameModeCampaign'
$strategicUI = Find-AICFClassRecord $records 'AICF_StrategicUIController'
$groupMapMarkers = Find-AICFClassRecord $records 'AICF_GroupMapMarkerSystem'
$vehicleHandoff = Find-AICFClassRecord $records 'AICF_VehicleTaskHandoff'
$vehicleHandoffState = Find-AICFClassRecord $records 'AICF_VehicleHandoffState'
$assignmentSnapshot = Find-AICFClassRecord $records 'AICF_StrategicAssignmentSnapshot'
$vehicleCoordinator = Find-AICFClassRecord $records 'AICF_VehicleCoordinator'
$transportTripController = Find-AICFClassRecord $records 'AICF_TransportTripController'
$enums = Get-AICommanderRecordByName $records 'AICF_Stage1Enums.c'
$arlandBootstrap = Get-AICommanderRecordByName $arlandRecords 'AICF_ArlandCampaignBootstrap.c'

if (Require-AICommanderRecord $stage1Config 'AICF_Stage1Config.c') {
    foreach ($entry in @(
        @('AI_COMMANDER_MODE_BOTH\s*=\s*"BOTH"\s*;', 'BOTH must be an exact command-mode literal'),
        @('AI_COMMANDER_MODE_US\s*=\s*"US"\s*;', 'US must be an exact command-mode literal'),
        @('AI_COMMANDER_MODE_USSR\s*=\s*"USSR"\s*;', 'USSR must be an exact command-mode literal')
    )) {
        Assert-AICFContains $failures 'AI_COMMANDER_CLI_WHITELIST' $stage1Config.Source $entry[0] $entry[1]
    }
    Assert-AICFNotContains $failures 'AI_COMMANDER_CLI_WHITELIST' $stage1Config.Code '\bAI_COMMANDER_MODE_(?:NONE|OFF|PLAYER)\b' 'The command-mode whitelist must contain only BOTH, US, and USSR'

    $resetDefaults = Assert-AICommanderMethodPresent $stage1Config 'ResetToDefaults' 'AI_COMMANDER_CLI_DEFAULT'
    $setMode = Assert-AICommanderMethodPresent $stage1Config 'SetAICommanderMode' 'AI_COMMANDER_CLI_WHITELIST'
    $applyCLI = Assert-AICommanderMethodPresent $stage1Config 'ApplyCLIOverrides' 'AI_COMMANDER_CLI_WHITELIST'
    $resetCode = ConvertTo-AICFCodeText $resetDefaults
    $setModeCode = ConvertTo-AICFCodeText $setMode
    $applyCLICode = ConvertTo-AICFCodeText $applyCLI

    Assert-AICFContains $failures 'AI_COMMANDER_CLI_DEFAULT' $resetCode 'm_sAICommanderMode\s*=\s*AI_COMMANDER_MODE_BOTH\s*;' 'The immutable match mode must default to BOTH'
    Assert-AICFContains $failures 'AI_COMMANDER_CLI_DEFAULT' $resetCode 'm_bAICommanderModeValid\s*=\s*true\s*;' 'The BOTH default must start valid'
    Assert-AICFContains $failures 'AI_COMMANDER_CLI_WHITELIST' $setModeCode 'm_bAICommanderModeValid\s*=\s*mode\s*==\s*AI_COMMANDER_MODE_BOTH\s*\|\|\s*mode\s*==\s*AI_COMMANDER_MODE_US\s*\|\|\s*mode\s*==\s*AI_COMMANDER_MODE_USSR\s*;' 'The complete validity expression must accept exactly BOTH, US, or USSR and no fourth branch'
    Assert-AICFContains $failures 'AI_COMMANDER_CLI_WHITELIST' $setModeCode 'if\s*\(\s*!m_bAICommanderModeValid\s*\)[\s\S]*m_sInvalidAICommanderMode\s*=\s*mode\s*;[\s\S]*return\s*;' 'An invalid raw CLI value must be retained and rejected'
    Assert-AICFContains $failures 'AI_COMMANDER_CLI_WHITELIST' $setModeCode 'm_sAICommanderMode\s*=\s*mode\s*;' 'A valid exact mode must be retained without normalization'
    Assert-AICFNotContains $failures 'AI_COMMANDER_CLI_WHITELIST' $setModeCode '\.(?:ToUpper|ToLower|Trim)\s*\(' 'Command mode must not silently normalize an invalid value'
    Assert-AICFNotContains $failures 'AI_COMMANDER_CLI_WHITELIST' $setModeCode 'm_sAICommanderMode\s*=\s*AI_COMMANDER_MODE_BOTH' 'Invalid SetAICommanderMode input must not fall back to BOTH'
    Assert-AICFContains $failures 'AI_COMMANDER_CLI_WHITELIST' $applyCLI 'System\.GetCLIParam\("aicfAICommanderMode"\s*,\s*value\)[\s\S]*SetAICommanderMode\(value\)' 'The exact aicfAICommanderMode CLI parameter must feed SetAICommanderMode'
}

if (Require-AICommanderClass $matchController 'AICF_MatchController') {
    $start = Assert-AICommanderMethodPresent $matchController 'Start' 'AI_COMMANDER_EARLY_REJECT'
    $startCode = ConvertTo-AICFCodeText $start
    $invalidModeBlock = Get-AICFBracedBody $start 'if\s*\(\s*!m_Config\.IsAICommanderModeValid\s*\(\s*\)\s*\)'
    Assert-AICFContains $failures 'AI_COMMANDER_EARLY_REJECT' $invalidModeBlock '"CONFIG_INVALID"[\s\S]*GetInvalidAICommanderMode\s*\(\s*\)[\s\S]*return\s*;' 'Invalid command mode must fail with its raw value and return immediately'
    foreach ($laterMarker in @(
        'new AICF_Stage2Config',
        'new AICF_ObjectiveGraph',
        'new AICF_FactionState',
        'SpawnInitialRoster',
        'CallLater'
    )) {
        Assert-AICommanderOrdered 'AI_COMMANDER_EARLY_REJECT' $startCode 'if (!m_Config.IsAICommanderModeValid())' $laterMarker "Invalid-mode rejection must precede $laterMarker"
    }
    Assert-AICFContains $failures 'AI_COMMANDER_EARLY_REJECT' $start 'allowed=BOTH,US,USSR' 'CONFIG_INVALID must publish the exact accepted values'
    Assert-AICFContains $failures 'AI_COMMANDER_POLICY_IMMUTABLE' $startCode 'm_Config\s*=\s*immutableConfig\s*;[\s\S]*if\s*\(\s*!m_Config\s*\)[\s\S]*m_Config\s*=\s*new\s+AICF_Stage1Config\s*\(' 'MatchController must retain a prevalidated immutable config while preserving a direct-start fallback'
}

if (Require-AICommanderRecord $arlandBootstrap 'AICF_ArlandCampaignBootstrap.c') {
    $onGameStart = Assert-AICommanderMethodPresent $arlandBootstrap 'OnGameStart' 'AI_COMMANDER_EARLY_REJECT'
    $scheduleStage1 = Assert-AICommanderMethodPresent $arlandBootstrap 'AICF_ScheduleStage1' 'AI_COMMANDER_EARLY_REJECT'
    $validateConfig = Assert-AICommanderMethodPresent $arlandBootstrap 'AICF_ValidateStage1Config' 'AI_COMMANDER_EARLY_REJECT'
    $runStage1 = Assert-AICommanderMethodPresent $arlandBootstrap 'AICF_RunStage1' 'AI_COMMANDER_EARLY_REJECT'
    $onGameStartCode = ConvertTo-AICFCodeText $onGameStart
    $scheduleCode = ConvertTo-AICFCodeText $scheduleStage1
    $preflightGuard = Get-AICFBracedBody $onGameStart 'if\s*\(\s*GetGame\(\)\.InPlayMode\(\)[\s\S]*?!AICF_ValidateStage1Config\(\)\s*\)'

    Assert-AICFContains $failures 'AI_COMMANDER_EARLY_REJECT' $validateConfig 'm_AICFStage1Config\s*=\s*new\s+AICF_Stage1Config\s*\([\s\S]*m_AICFStage1Config\.IsAICommanderModeValid\s*\([\s\S]*allowed=BOTH,US,USSR[\s\S]*"CONFIG_INVALID"[\s\S]*return\s+false\s*;' 'Arland config preflight must retain the raw invalid value and fail closed'
    Assert-AICFContains $failures 'AI_COMMANDER_EARLY_REJECT' $preflightGuard '\breturn\s*;' 'OnGameStart invalid-mode guard must return before any AICF startup side effect'
    Assert-AICFContains $failures 'AI_COMMANDER_EARLY_REJECT' $scheduleCode 'if\s*\(\s*!AICF_ValidateStage1Config\(\)\s*\)\s*return\s*;' 'Schedule invalid-mode guard must return before radio normalization and controller scheduling'
    Assert-AICommanderOrdered 'AI_COMMANDER_EARLY_REJECT' $onGameStartCode '!AICF_ValidateStage1Config())' 'new AICF_StrategicUIController' 'Invalid-mode rejection must precede the strategic UI repeating callqueue'
    Assert-AICommanderOrdered 'AI_COMMANDER_EARLY_REJECT' $scheduleCode 'if (!AICF_ValidateStage1Config())' 'new AICF_ArlandRadioBridgeNormalizer' 'Invalid-mode rejection must precede Arland radio normalization and subscription'
    Assert-AICommanderOrdered 'AI_COMMANDER_EARLY_REJECT' $scheduleCode 'if (!AICF_ValidateStage1Config())' 'CallLater(AICF_RunStage1' 'Invalid-mode rejection must precede the deferred match-controller loop'
    Assert-AICFContains $failures 'AI_COMMANDER_POLICY_IMMUTABLE' (ConvertTo-AICFCodeText $runStage1) 'Start\s*\(\s*this\s*,\s*m_AICFStage1Config\s*,\s*m_AICFContentProfile\s*\)' 'Arland bootstrap must hand the exact prevalidated config and selected content profile to MatchController'
}

if (Require-AICommanderClass $authorityPolicy 'AICF_CommandAuthorityPolicy') {
    $policyConstructor = Get-AICFBracedBody $authorityPolicy.Source '(?m)^\s*void\s+AICF_CommandAuthorityPolicy\s*\('
    $policyConstructorCode = ConvertTo-AICFCodeText $policyConstructor
    $isEnabled = Assert-AICommanderMethodPresent $authorityPolicy 'IsAICommanderEnabled' 'AI_COMMANDER_POLICY'
    $getAuthority = Assert-AICommanderMethodPresent $authorityPolicy 'GetFactionAuthority' 'AI_COMMANDER_POLICY'
    $isEnabledCode = ConvertTo-AICFCodeText $isEnabled
    $getAuthorityCode = ConvertTo-AICFCodeText $getAuthority

    Assert-AICFContains $failures 'AI_COMMANDER_POLICY' $policyConstructorCode 'm_bValid\s*=\s*config\s*&&\s*config\.IsAICommanderModeValid\s*\(\s*\)' 'Policy construction must snapshot validated config state'
    Assert-AICFContains $failures 'AI_COMMANDER_POLICY' $policyConstructorCode 'm_sMode\s*=\s*config\.GetAICommanderMode\s*\(\s*\)' 'Policy construction must snapshot the validated mode'
    Assert-AICFNotContains $failures 'AI_COMMANDER_POLICY_IMMUTABLE' $authorityPolicy.Code '\bSystem\.GetCLIParam\s*\(' 'The match-scoped policy must not reread CLI state'
    Assert-AICFNotContains $failures 'AI_COMMANDER_POLICY_IMMUTABLE' $authorityPolicy.Code '\b(?:SetMode|SetValid|ResetToDefaults|ApplyCLIOverrides)\s*\(' 'The match-scoped policy must not expose a mutator'

    $outsideConstructor = $authorityPolicy.Code
    if ($policyConstructor) {
        $outsideConstructor = ConvertTo-AICFCodeText (
            $authorityPolicy.Source.Replace($policyConstructor, ''))
    }
    Assert-AICFNotContains $failures 'AI_COMMANDER_POLICY_IMMUTABLE' $outsideConstructor '\bm_sMode\s*=(?!=)' 'Policy mode may be assigned only during construction'
    Assert-AICFNotContains $failures 'AI_COMMANDER_POLICY_IMMUTABLE' $outsideConstructor '\bm_bValid\s*=(?!=)' 'Policy validity may be assigned only during construction'

    Assert-AICFContains $failures 'AI_COMMANDER_POLICY' $isEnabled 'factionKey\s*==\s*"US"[\s\S]*AI_COMMANDER_MODE_BOTH[\s\S]*AI_COMMANDER_MODE_US' 'US authority must map only from BOTH or US'
    Assert-AICFContains $failures 'AI_COMMANDER_POLICY' $isEnabled 'factionKey\s*==\s*"USSR"[\s\S]*AI_COMMANDER_MODE_BOTH[\s\S]*AI_COMMANDER_MODE_USSR' 'USSR authority must map only from BOTH or USSR'
    Assert-AICFContains $failures 'AI_COMMANDER_POLICY' $isEnabledCode 'return\s+false\s*;\s*\}' 'Unknown factions must fail closed'
    Assert-AICFContains $failures 'AI_COMMANDER_POLICY' $getAuthorityCode 'AI_COMMANDER[\s\S]*PLAYER_COMMAND' 'Valid factions must resolve to AI_COMMANDER or PLAYER_COMMAND'
    Assert-AICFContains $failures 'AI_COMMANDER_POLICY' $getAuthority 'factionKey\s*!=\s*"US"\s*&&\s*factionKey\s*!=\s*"USSR"[\s\S]*NONE' 'Unknown factions must resolve to NONE'
}

if (Require-AICommanderClass $aiCommander 'AICF_AICommander') {
    $ownsSlot = Assert-AICommanderMethodPresent $aiCommander 'OwnsSlot' 'AI_COMMANDER_FACTION_SCOPE'
    $isCommanderEnabled = Assert-AICommanderMethodPresent $aiCommander 'IsEnabled' 'AI_COMMANDER_FACTION_SCOPE'
    $tick = Assert-AICommanderMethodPresent $aiCommander 'Tick' 'AI_COMMANDER_FACTION_SCOPE'
    $assign = Assert-AICommanderMethodPresent $aiCommander 'AssignOrder' 'AI_COMMANDER_SELECTOR_BOUNDARY'
    $reconcile = Assert-AICommanderMethodPresent $aiCommander 'ReconcileStrategicOrder' 'AI_COMMANDER_SELECTOR_BOUNDARY'
    $loss = Assert-AICommanderMethodPresent $aiCommander 'AssignLossResponseOrder' 'AI_COMMANDER_SELECTOR_BOUNDARY'

    Assert-AICFContains $failures 'AI_COMMANDER_FACTION_SCOPE' (ConvertTo-AICFCodeText $ownsSlot) 'm_FactionState\.GetSlot\s*\(\s*slot\.GetSlotId\s*\(\s*\)\s*\)\s*==\s*slot' 'Commander ownership must use stable faction+numeric-slot identity'
    Assert-AICFContains $failures 'AI_COMMANDER_FACTION_SCOPE' (ConvertTo-AICFCodeText $isCommanderEnabled) 'm_AuthorityPolicy\.IsAICommanderEnabled\s*\(\s*m_sFactionKey\s*\)' 'Commander enablement must remain faction-bound'
    Assert-AICFContains $failures 'AI_COMMANDER_FACTION_SCOPE' (ConvertTo-AICFCodeText $tick) 'matchController\.RunAICommanderTick\s*\(\s*this\s*,\s*reason\s*\)' 'Commander tick must re-enter the identity-checking MatchController boundary'
    Assert-AICFContains $failures 'AI_COMMANDER_SELECTOR_BOUNDARY' (ConvertTo-AICFCodeText $assign) 'OwnsSlot\s*\([\s\S]*AssignAICommanderOrder\s*\(' 'Autonomous assignment must require owned slot identity'
    Assert-AICFContains $failures 'AI_COMMANDER_SELECTOR_BOUNDARY' (ConvertTo-AICFCodeText $reconcile) 'OwnsSlot\s*\([\s\S]*ReconcileAICommanderOrder\s*\(' 'Autonomous reconciliation must require owned slot identity'
    Assert-AICFContains $failures 'AI_COMMANDER_SELECTOR_BOUNDARY' (ConvertTo-AICFCodeText $loss) 'OwnsSlot\s*\([\s\S]*AssignAICommanderLossResponseOrder\s*\(' 'Autonomous loss response must require owned slot identity'
    Assert-AICFNotContains $failures 'AI_COMMANDER_FACTION_SCOPE' $aiCommander.Code '\b(?:AICF_GroupSpawner|AICF_ReinforcementSystem|AICF_EconomySystem|AICF_VehicleCoordinator|AICF_VehicleSpawner|AICF_TransportTripController)\b' 'Faction commander must not own spawn, economy, or vehicle side effects'
    Assert-AICFNotContains $failures 'AI_COMMANDER_FACTION_SCOPE' $aiCommander.Code '\b(?:SpawnEntityPrefabEx|DeleteRplEntity|AddWaypointAt|RemoveWaypoint|SetTickets)\s*\(' 'Faction commander must not mutate runtime entities or tickets'
}

if (Require-AICommanderClass $orderPlanner 'AICF_OrderPlanner') {
    foreach ($entrypoint in @(
        'AssignAICommanderOrder',
        'ReconcileAICommanderOrder',
        'AssignAICommanderLossResponseOrder'
    )) {
        [void](Assert-AICommanderMethodPresent $orderPlanner $entrypoint 'AI_COMMANDER_SELECTOR_BOUNDARY')
    }

    foreach ($record in $records) {
        $calls = [regex]::Matches(
            $record.Code,
            '\.\s*(?:AssignAICommanderOrder|ReconcileAICommanderOrder|AssignAICommanderLossResponseOrder)\s*\(')
        if ($calls.Count -gt 0 -and
            (!$aiCommander -or $record.FullName -ne $aiCommander.FullName)) {
            Add-AICFAuditFailure $failures 'AI_COMMANDER_SELECTOR_BOUNDARY' "Autonomous planner entrypoint is called outside AICF_AICommander ($($record.RelativePath))"
        }
    }

    foreach ($genericMethodName in @(
        'AssignOrder',
        'ReconcileStrategicOrder',
        'AssignLossResponseOrder',
        'RecoverOrder',
        'RebuildCurrentOrder'
    )) {
        $genericBody = Assert-AICommanderMethodPresent $orderPlanner $genericMethodName 'AI_COMMANDER_SELECTOR_BOUNDARY'
        $genericCode = ConvertTo-AICFCodeText $genericBody
        Assert-AICFNotContains $failures 'AI_COMMANDER_SELECTOR_BOUNDARY' $genericCode '\b(?:SelectOperationalTarget|SelectAttackTarget|SelectDefendTarget|SelectLossResponseTarget)\s*\(' "$genericMethodName must restore/hold committed intent without autonomous target selection"
        Assert-AICFNotContains $failures 'AI_COMMANDER_SELECTOR_BOUNDARY' $genericCode '\b(?:AssignAICommanderOrder|ReconcileAICommanderOrder|AssignAICommanderLossResponseOrder)\s*\(' "$genericMethodName must not bypass AICF_AICommander"
    }
}

if (Require-AICommanderRecord $enums 'AICF_Stage1Enums.c') {
    $slotState = Get-AICFBracedBody $enums.Source '(?m)^\s*enum\s+AICF_EGroupSlotState\b'
    $decisionAuthority = Get-AICFBracedBody $enums.Source '(?m)^\s*enum\s+AICF_EStrategicDecisionAuthority\b'
    foreach ($authority in @('NONE', 'AI_COMMANDER', 'PLAYER_COMMAND', 'SYSTEM_HOLD')) {
        Assert-AICFContains $failures 'AI_COMMANDER_DECISION_AUTHORITY' $decisionAuthority ("\b" + $authority + "\b") "Strategic authority enum omits $authority"
    }
    Assert-AICFNotContains $failures 'AI_COMMANDER_AWAITING_STATE' $slotState '\bAWAITING_PLAYER_COMMAND\b' 'AWAITING_PLAYER_COMMAND must remain orthogonal to group lifecycle state'
    foreach ($label in @('AI_COMMANDER', 'PLAYER_COMMAND', 'SYSTEM_HOLD', 'NONE')) {
        Assert-AICFContains $failures 'AI_COMMANDER_DECISION_AUTHORITY' $enums.Source ('"' + $label + '"') "Decision-authority diagnostics omit $label"
    }
}

if (Require-AICommanderClass $groupSlot 'AICF_GroupSlot') {
    foreach ($field in @(
        'm_StrategicIntentTargetBase',
        'm_StrategicIntentRole',
        'm_StrategicIntentAuthority',
        'm_sStrategicIntentPosture',
        'm_iStrategicIntentRevision'
    )) {
        Assert-AICFContains $failures 'AI_COMMANDER_DURABLE_INTENT' $groupSlot.Code ("\b" + $field + "\b") "Stable slot omits durable intent field $field"
    }

    $commitIntent = Assert-AICommanderMethodPresent $groupSlot 'CommitStrategicIntent' 'AI_COMMANDER_DURABLE_INTENT'
    $commitCode = ConvertTo-AICFCodeText $commitIntent
    Assert-AICFContains $failures 'AI_COMMANDER_DURABLE_INTENT' $commitCode 'm_StrategicIntentTargetBase\s*==\s*targetBase[\s\S]*m_sStrategicIntentPosture\s*==\s*posture[\s\S]*m_StrategicIntentAuthority\s*==\s*authority[\s\S]*m_StrategicIntentRole\s*==\s*m_Role[\s\S]*return\s*;' 'Equivalent intent must not create a new revision'
    Assert-AICFContains $failures 'AI_COMMANDER_DURABLE_INTENT' $commitCode 'm_StrategicIntentTargetBase\s*=\s*targetBase[\s\S]*m_sStrategicIntentPosture\s*=\s*posture[\s\S]*m_StrategicIntentAuthority\s*=\s*authority[\s\S]*m_StrategicIntentRole\s*=\s*m_Role[\s\S]*m_iStrategicIntentRevision\+\+' 'Intent commit must atomically record target, posture, authority, role, and revision'

    foreach ($methodName in @(
        'ClearStrategicIntent',
        'HasStrategicIntent',
        'GetStrategicIntentTargetBase',
        'GetStrategicIntentPosture',
        'GetStrategicIntentAuthority',
        'IsStrategicIntentRoleCurrent',
        'GetStrategicIntentRevision',
        'GetDecisionAuthority',
        'SetDecisionAuthority',
        'IsAwaitingPlayerCommand',
        'IsSystemHoldOrder'
    )) {
        [void](Assert-AICommanderMethodPresent $groupSlot $methodName 'AI_COMMANDER_DURABLE_INTENT')
    }

    $runtimeClear = Assert-AICommanderMethodPresent $groupSlot 'ClearRuntimeReferences' 'AI_COMMANDER_REPLACEMENT_INTENT'
    $runtimeClearCode = ConvertTo-AICFCodeText $runtimeClear
    Assert-AICFNotContains $failures 'AI_COMMANDER_REPLACEMENT_INTENT' $runtimeClearCode '\b(?:m_StrategicIntentTargetBase|m_StrategicIntentRole|m_StrategicIntentAuthority|m_sStrategicIntentPosture|m_iStrategicIntentRevision|ClearStrategicIntent|ClearPlayerStrategicIntent)\b' 'Replacement runtime cleanup must preserve durable strategic intent'
    $replacementSpawn = Assert-AICommanderMethodPresent $groupSlot 'BeginReplacementSpawn' 'AI_COMMANDER_REPLACEMENT_INTENT'
    Assert-AICFContains $failures 'AI_COMMANDER_REPLACEMENT_INTENT' (ConvertTo-AICFCodeText $replacementSpawn) 'ClearRuntimeReferences\s*\(\s*\)' 'Replacement spawn must clear runtime identity without clearing strategic intent'
    $fullReset = Assert-AICommanderMethodPresent $groupSlot 'Reset' 'AI_COMMANDER_REPLACEMENT_INTENT'
    Assert-AICFContains $failures 'AI_COMMANDER_REPLACEMENT_INTENT' (ConvertTo-AICFCodeText $fullReset) 'ClearStrategicIntent\s*\(\s*\)' 'Only full stable-slot reset must clear durable intent'

    $assignSuspended = Assert-AICommanderMethodPresent $groupSlot 'AssignSuspendedObjective' 'AI_COMMANDER_VEHICLE_SUSPENSION'
    $runtimeWaypointReplacement = Assert-AICommanderMethodPresent $groupSlot 'RecordRuntimeWaypointReplacement' 'AI_COMMANDER_VEHICLE_SUSPENSION'
    Assert-AICFContains $failures 'AI_COMMANDER_VEHICLE_SUSPENSION' (ConvertTo-AICFCodeText $assignSuspended) 'm_Waypoint[\s\S]*return\s+false[\s\S]*m_TargetBase\s*=\s*targetBase' 'Suspended objective assignment must require the vehicle-owned waypoint gap and update only slot target state'
    Assert-AICFContains $failures 'AI_COMMANDER_VEHICLE_SUSPENSION' (ConvertTo-AICFCodeText $runtimeWaypointReplacement) 'm_iStrategicAssignmentRevision\+\+' 'A same-target runtime waypoint replacement must advance asynchronous assignment identity'
}

if ($orderPlanner) {
    $assignOrder = ConvertTo-AICFCodeText (Get-AICFMethodBody $orderPlanner 'AssignOrder')
    $assignAIOrder = ConvertTo-AICFCodeText (Get-AICFMethodBody $orderPlanner 'AssignAICommanderOrder')
    $assignPlayer = ConvertTo-AICFCodeText (Get-AICFMethodBody $orderPlanner 'AssignPlayerOrder')
    $restorePlayer = ConvertTo-AICFCodeText (Get-AICFMethodBody $orderPlanner 'RestorePlayerStrategicIntent')
    $invalidatePlayer = ConvertTo-AICFCodeText (Get-AICFMethodBody $orderPlanner 'InvalidatePlayerStrategicIntent')
    $assignHold = ConvertTo-AICFCodeText (Get-AICFMethodBody $orderPlanner 'AssignSystemHold')
    $replaceOrder = ConvertTo-AICFCodeText (Get-AICFMethodBody $orderPlanner 'ReplaceOrder')
    $isCurrentTargetValid = ConvertTo-AICFCodeText (Get-AICFMethodBody $orderPlanner 'IsCurrentTargetValid')
    $getFailure = ConvertTo-AICFCodeText (Get-AICFMethodBody $orderPlanner 'GetOrderFailureReason')
    $snapshot = ConvertTo-AICFCodeText (Get-AICFMethodBody $orderPlanner 'TryCreateAssignmentSnapshot')
    $loneSurvivor = ConvertTo-AICFCodeText (Get-AICFMethodBody $orderPlanner 'AssignLoneSurvivorRetreat')
    $applySuspendedBody = Get-AICFMethodBody $orderPlanner 'ApplySuspendedStrategicAssignment'
    $applySuspended = ConvertTo-AICFCodeText $applySuspendedBody

    Assert-AICFContains $failures 'AI_COMMANDER_PLAYER_INTENT' $assignPlayer 'IsTargetValidForRole\s*\([\s\S]*PLAYER_COMMAND[\s\S]*RecordPlayerStrategicIntent\s*\(' 'Player assignment must revalidate and durably record the target'
    Assert-AICFContains $failures 'AI_COMMANDER_PLAYER_INTENT' $assignOrder 'HasPlayerStrategicIntent\s*\([\s\S]*IsPlayerStrategicIntentRoleCurrent\s*\([\s\S]*IsTargetValidForRole\s*\([\s\S]*InvalidatePlayerStrategicIntent\s*\([\s\S]*RestorePlayerStrategicIntent\s*\(' 'Generic restoration must revalidate player role and target before reuse'
    Assert-AICFContains $failures 'AI_COMMANDER_PLAYER_INTENT' $restorePlayer 'IsPlayerStrategicIntentRoleCurrent\s*\([\s\S]*IsTargetValidForRole\s*\([\s\S]*PLAYER_COMMAND' 'Player intent restoration must fail closed on stale role or target'
    Assert-AICFContains $failures 'AI_COMMANDER_PLAYER_INTENT' (Get-AICFMethodBody $orderPlanner 'InvalidatePlayerStrategicIntent') 'PLAYER_INTENT_INVALIDATED[\s\S]*ClearPlayerStrategicIntent\s*\(' 'Invalid player intent must be diagnosed and cleared'
    Assert-AICFContains $failures 'AI_COMMANDER_EXCLUDED_TARGET' $assignAIOrder 'intentTarget\s*!=\s*excludedTarget[\s\S]*RestoreStrategicIntent\s*\(' 'AI full replan must not restore the explicitly excluded failed target'
    Assert-AICFContains $failures 'AI_COMMANDER_EXCLUDED_TARGET' $assignAIOrder 'intentTarget\s*!=\s*excludedTarget[\s\S]*RestoreStrategicIntent\s*\([\s\S]*\}\s*else\s*\{\s*InvalidateStrategicIntent\s*\(' 'An excluded but otherwise valid AI intent must survive until a replacement is committed'
    Assert-AICFContains $failures 'AI_COMMANDER_EXCLUDED_TARGET' $assignOrder 'intentTarget\s*!=\s*excludedTarget[\s\S]*RestoreStrategicIntent\s*\([\s\S]*\}\s*else\s*\{\s*InvalidateStrategicIntent\s*\(' 'Generic restore-only callers must not erase an excluded but otherwise valid AI intent'

    Assert-AICFContains $failures 'AI_COMMANDER_SYSTEM_HOLD' $assignHold '!m_AuthorityPolicy\.IsValid\s*\(\s*\)[\s\S]*m_AuthorityPolicy\.IsAICommanderEnabled\s*\(\s*faction\.GetFactionKey\s*\(\s*\)\s*\)' 'SYSTEM_HOLD must be available only under valid player command authority'
    Assert-AICFContains $failures 'AI_COMMANDER_SYSTEM_HOLD' $assignHold 'faction\.GetMainBase\s*\(\s*\)[\s\S]*mainBase\.IsInitialized\s*\(\s*\)[\s\S]*mainBase\.GetFaction\s*\(\s*\)\s*!=\s*faction' 'SYSTEM_HOLD must bind to the live faction HQ'
    Assert-AICFContains $failures 'AI_COMMANDER_SYSTEM_HOLD' $assignHold 'SYSTEM_HOLD[\s\S]*BeginAwaitingPlayerCommand\s*\([\s\S]*LogCommandWaiting\s*\(' 'SYSTEM_HOLD must publish awaiting-player command state'
    Assert-AICFContains $failures 'AI_COMMANDER_SYSTEM_HOLD' $replaceOrder 'if\s*\(\s*systemHold\s*\)[\s\S]*waypointRole\s*=\s*AICF_EGroupRole\.DEFEND' 'SYSTEM_HOLD must always create an HQ defend waypoint regardless of slot role'
    Assert-AICFContains $failures 'AI_COMMANDER_SYSTEM_HOLD' $isCurrentTargetValid 'IsSystemHoldOrder\s*\([\s\S]*faction\.GetMainBase\s*\([\s\S]*slot\.GetTargetBase\s*\(\s*\)\s*==\s*mainBase' 'SYSTEM_HOLD target validity must be HQ-specific rather than role-specific'
    Assert-AICFContains $failures 'AI_COMMANDER_SYSTEM_HOLD' $getFailure 'IsSystemHoldOrder\s*\([\s\S]*SCR_DefendWaypoint\.Cast\s*\(' 'SYSTEM_HOLD reliability must require a defend waypoint'
    Assert-AICFContains $failures 'AI_COMMANDER_SYSTEM_HOLD' $snapshot 'IsSystemHoldOrder\s*\(\s*\)\s*\|\|\s*slot\.IsAwaitingPlayerCommand\s*\(\s*\)' 'Awaiting/SYSTEM_HOLD orders must not enter vehicle assignment snapshots'
    Assert-AICFContains $failures 'AI_COMMANDER_VEHICLE_RESTORE' $snapshot 'slot\.GetStrategicAssignmentRevision\s*\(\s*\)[\s\S]*slot\.GetStrategicIntentRevision\s*\(\s*\)' 'Vehicle snapshots must carry runtime assignment identity and the independent strategic-intent revision'
    Assert-AICFContains $failures 'AI_COMMANDER_SYSTEM_HOLD' $loneSurvivor 'slot\.GetDecisionAuthority\s*\(\s*\)\s*,\s*false\s*,\s*false' 'Safety retreat must preserve authority without masquerading as SYSTEM_HOLD'

    Assert-AICFContains $failures 'AI_COMMANDER_VEHICLE_SUSPENSION' $assignPlayer 'waypointSuspendedByVehicle[\s\S]*ApplySuspendedStrategicAssignment\s*\(' 'Explicit player orders must update a vehicle-suspended strategic assignment without creating an infantry waypoint'
    Assert-AICFContains $failures 'AI_COMMANDER_VEHICLE_SUSPENSION' $applySuspended 'slot\.GetWaypoint\s*\(\s*\)[\s\S]*AssignSuspendedObjective\s*\([\s\S]*RecordStrategicAssignment\s*\(' 'Suspended planning must be waypoint-free and revisioned'
    Assert-AICFContains $failures 'AI_COMMANDER_VEHICLE_SUSPENSION' $applySuspendedBody 'vehicle_control=1' 'Suspended strategic assignment must be explicitly diagnosed as vehicle-controlled'
    Assert-AICFNotContains $failures 'AI_COMMANDER_VEHICLE_SUSPENSION' $applySuspended '\b(?:ReplaceOrder|CreateWaypoint|AddWaypointAt|AssignObjective)\s*\(' 'Suspended planning must not mutate the infantry waypoint queue'
    Assert-AICFContains $failures 'AI_COMMANDER_VEHICLE_SUSPENSION' $replaceOrder 'RecordRuntimeWaypointReplacement\s*\(\s*\)' 'Every same-target waypoint replacement must invalidate stale vehicle/recovery snapshots'
}

if ($matchController) {
    $commanderTick = ConvertTo-AICFCodeText (Get-AICFMethodBody $matchController 'CommanderTick')
    $runCommanderTick = ConvertTo-AICFCodeText (Get-AICFMethodBody $matchController 'RunAICommanderTick')
    $activeTaskAudit = ConvertTo-AICFCodeText (Get-AICFMethodBody $matchController 'AuditActiveFactionTasking')
    $meaningfulTask = ConvertTo-AICFCodeText (Get-AICFMethodBody $matchController 'HasMeaningfulTask')
    $reliability = ConvertTo-AICFCodeText (Get-AICFMethodBody $matchController 'ProcessFactionReliability')
    $buildSummary = ConvertTo-AICFCodeText (Get-AICFMethodBody $matchController 'BuildGroupSummary')
    $requestPlayerOrder = ConvertTo-AICFCodeText (Get-AICFMethodBody $matchController 'RequestPlayerOrder')
    $requestGroupConfig = ConvertTo-AICFCodeText (Get-AICFMethodBody $matchController 'RequestPlayerGroupConfiguration')
    $revalidateOrders = ConvertTo-AICFCodeText (Get-AICFMethodBody $matchController 'RevalidateFactionOrders')
    $replanAfterBaseChange = ConvertTo-AICFCodeText (Get-AICFMethodBody $matchController 'ReplanFactionAfterBaseChange')
    $completeLoneRetreat = ConvertTo-AICFCodeText (Get-AICFMethodBody $matchController 'TryCompleteLoneSurvivorRetreat')

    Assert-AICommanderOrdered 'AI_COMMANDER_TICK_ORDER' $commanderTick 'm_USAICommander' 'm_USSRAICommander' 'CommanderTick must visit US before USSR'
    Assert-AICommanderOrdered 'AI_COMMANDER_TICK_ORDER' $commanderTick 'm_USState' 'm_USSRState' 'Player-command maintenance must visit US before USSR'
    Assert-AICFContains $failures 'AI_COMMANDER_TICK_ORDER' $commanderTick 'm_USAICommander\.Tick\s*\(\s*this' 'US must use its faction commander when enabled'
    Assert-AICFContains $failures 'AI_COMMANDER_TICK_ORDER' $commanderTick 'm_USSRAICommander\.Tick\s*\(\s*this' 'USSR must use its faction commander when enabled'
    Assert-AICFContains $failures 'AI_COMMANDER_FACTION_SCOPE' $runCommanderTick 'commander\s*==\s*m_USAICommander[\s\S]*commander\.GetFactionState\s*\(\s*\)\s*==\s*m_USState[\s\S]*commander\.GetFaction\s*\(\s*\)\s*==\s*m_USFaction' 'US commander tick must verify commander, state, and faction identity'
    Assert-AICFContains $failures 'AI_COMMANDER_FACTION_SCOPE' $runCommanderTick 'commander\s*==\s*m_USSRAICommander[\s\S]*commander\.GetFactionState\s*\(\s*\)\s*==\s*m_USSRState[\s\S]*commander\.GetFaction\s*\(\s*\)\s*==\s*m_USSRFaction' 'USSR commander tick must verify commander, state, and faction identity'
    Assert-AICFContains $failures 'AI_COMMANDER_AWAITING_STATE' $activeTaskAudit 'IsAwaitingPlayerCommand\s*\([\s\S]*ObserveMeaningfulTaskLoss\s*\(\s*false\s*\)[\s\S]*continue\s*;' 'Awaiting groups must not consume meaningful-task deadlines'
    Assert-AICFContains $failures 'AI_COMMANDER_AWAITING_STATE' $meaningfulTask 'IsAwaitingPlayerCommand\s*\(\s*\)[\s\S]*return\s+true\s*;' 'The executable-task audit must treat awaiting command as an explicit safe state while reliability repairs SYSTEM_HOLD'
    Assert-AICFContains $failures 'AI_COMMANDER_AWAITING_STATE' $reliability 'IsAwaitingPlayerCommand\s*\([\s\S]*AssignSystemHold\s*\([\s\S]*continue\s*;' 'Reliability may repair SYSTEM_HOLD but must not autonomously replan an awaiting group'
    Assert-AICommanderOrdered 'AI_COMMANDER_VEHICLE_SUSPENSION' $reliability 'if (slot.HasPendingOrderRecovery())' 'if (slot.IsAwaitingPlayerCommand())' 'Vehicle-restored SYSTEM_HOLD durability must complete before the awaiting branch'
    Assert-AICFContains $failures 'AI_COMMANDER_UI_STATE' $buildSummary 'IsAwaitingPlayerCommand\s*\([\s\S]*state\s*=\s*' 'Replicated group summaries must expose AWAITING_PLAYER_COMMAND separately from lifecycle READY'
    Assert-AICFContains $failures 'AI_COMMANDER_UI_STATE' (Get-AICFMethodBody $matchController 'BuildGroupSummary') '"AWAITING_PLAYER_COMMAND"' 'Replicated group summaries must publish the exact awaiting state'

    Assert-AICFContains $failures 'AI_COMMANDER_VEHICLE_SUSPENSION' $requestPlayerOrder 'IsInfantryOrderSuspended\s*\([\s\S]*AssignPlayerOrder\s*\([\s\S]*waypointSuspendedByVehicle' 'Player target RPC must preserve vehicle waypoint ownership and then adopt the current strategic revision'
    Assert-AICFContains $failures 'AI_COMMANDER_VEHICLE_SUSPENSION' $requestGroupConfig 'IsRestorePending\s*\([\s\S]*IsInfantryOrderSuspended\s*\([\s\S]*AssignFactionStrategicOrder\s*\([\s\S]*waypointSuspendedByVehicle' 'Role changes must preserve both active movement and restore-pending waypoint ownership'
    Assert-AICFContains $failures 'AI_COMMANDER_VEHICLE_SUSPENSION' $requestGroupConfig 'orderAssigned\s*&&\s*vehicleControlled\s*&&\s*!vehicleRestorePending\s*&&\s*!unitTypeChanged' 'A simultaneous role and unit-profile change must not retarget-commit the old physical vehicle trip'
    Assert-AICFContains $failures 'AI_COMMANDER_VEHICLE_SUSPENSION' $revalidateOrders 'IsControllingMovement\s*\([\s\S]*IsRestorePending\s*\([\s\S]*IsInfantryOrderSuspended\s*\([\s\S]*ReconcileStrategicOrder' 'Commander maintenance must use suspended planning while vehicle handoff owns the queue'
    Assert-AICFContains $failures 'AI_COMMANDER_VEHICLE_SUSPENSION' $replanAfterBaseChange 'vehicleOwnsMovementOrRestore[\s\S]*waypointSuspendedByVehicle[\s\S]*AssignFactionLossResponseOrder[\s\S]*ReconcileFactionStrategicOrder' 'Base-change and loss-response planning must stay behind the vehicle suspension boundary'
    Assert-AICommanderOrdered 'AI_COMMANDER_LONE_SURVIVOR_INTENT' $revalidateOrders 'if (slot.IsLoneSurvivorRetreat())' 'bool vehicleControlled' 'Commander reconciliation must not reinterpret the temporary HQ retreat as the durable intent target'
    Assert-AICommanderOrdered 'AI_COMMANDER_LONE_SURVIVOR_INTENT' $replanAfterBaseChange 'if (slot.IsLoneSurvivorRetreat())' 'if (slot.HasPendingOrderRecovery())' 'Base-change revalidation must defer a survivor intent until retreat completion'
    Assert-AICommanderOrdered 'AI_COMMANDER_LONE_SURVIVOR_INTENT' $completeLoneRetreat 'slot.ClearLoneSurvivorRetreat();' 'AssignFactionStrategicOrder(' 'Lone-survivor completion must clear temporary runtime posture before revalidating durable intent'

    $start = Get-AICFMethodBody $matchController 'Start'
    $startCode = ConvertTo-AICFCodeText $start
    $rosterReadyCode = ConvertTo-AICFCodeText (Get-AICFMethodBody $matchController 'TryLogRosterReady')
    $stopCode = ConvertTo-AICFCodeText (Get-AICFMethodBody $matchController 'Stop')
    Assert-AICFNotContains $failures 'AI_COMMANDER_REPLICATION' $startCode 'm_Campaign\.AICF_SetAICommanderState\s*\(' 'Start must leave command flags at false/false while asynchronous roster slots are not READY'
    Assert-AICFContains $failures 'AI_COMMANDER_REPLICATION' $rosterReadyCode 'm_USState\.CountSlotsByState\s*\(\s*AICF_EGroupSlotState\.READY\s*\)\s*!=\s*AICF_Stage1Config\.GROUP_SLOTS_PER_FACTION[\s\S]*m_USSRState\.CountSlotsByState\s*\(\s*AICF_EGroupSlotState\.READY\s*\)\s*!=\s*AICF_Stage1Config\.GROUP_SLOTS_PER_FACTION\s*\)\s*return\s*;[\s\S]*m_bRosterReady\s*=\s*true[\s\S]*m_Campaign\.AICF_SetAICommanderState\s*\(' 'Authoritative command flags may publish only after an all-twenty-READY guard that returns early'
    Assert-AICFContains $failures 'AI_COMMANDER_REPLICATION' $stopCode 'm_Campaign\.AICF_SetAICommanderState\s*\(\s*false\s*,\s*false\s*\)' 'Fatal or completed controller stop must clear replicated command availability'
    Assert-AICommanderOrdered 'AI_COMMANDER_REPLICATION' $stopCode 'm_Campaign.AICF_SetAICommanderState(false, false)' 'if (m_bSubscribed)' 'Stop must clear authority availability before removing subscriptions'
    Assert-AICommanderOrdered 'AI_COMMANDER_REPLICATION' $stopCode 'm_Campaign.AICF_SetAICommanderState(false, false)' 'if (m_GroupMapMarkers)' 'Stop must clear authority availability before subsystem cleanup'
}

if (Require-AICommanderClass $campaignState 'SCR_GameModeCampaign') {
    foreach ($field in @(
        'm_bAICFUSAICommanderEnabled',
        'm_bAICFUSSRAICommanderEnabled'
    )) {
        $fieldPattern = '\[RplProp\(onRplName:\s*"AICF_OnAICommanderStateReplicated"\)\]\s*protected\s+bool\s+' + $field + '\s*;'
        Assert-AICFContains $failures 'AI_COMMANDER_REPLICATION' $campaignState.Source $fieldPattern "Campaign flag $field must be a callback-backed RplProp"
    }

    $setState = Assert-AICommanderMethodPresent $campaignState 'AICF_SetAICommanderState' 'AI_COMMANDER_REPLICATION'
    $getEnabled = Assert-AICommanderMethodPresent $campaignState 'AICF_GetAICommanderEnabled' 'AI_COMMANDER_REPLICATION'
    $hasState = Assert-AICommanderMethodPresent $campaignState 'AICF_HasAICommanderState' 'AI_COMMANDER_REPLICATION'
    [void](Assert-AICommanderMethodPresent $campaignState 'AICF_OnAICommanderStateReplicated' 'AI_COMMANDER_REPLICATION')
    $setStateCode = ConvertTo-AICFCodeText $setState
    $getEnabledCode = ConvertTo-AICFCodeText $getEnabled
    $hasStateCode = ConvertTo-AICFCodeText $hasState

    Assert-AICFContains $failures 'AI_COMMANDER_REPLICATION' $setStateCode '!Replication\.IsServer\s*\(\s*\)\s*\|\|\s*!IsMaster\s*\(\s*\)' 'Campaign command flags may change only on authoritative server/master'
    Assert-AICFContains $failures 'AI_COMMANDER_REPLICATION' $setStateCode 'm_bAICFUSAICommanderEnabled\s*==\s*usEnabled\s*&&\s*m_bAICFUSSRAICommanderEnabled\s*==\s*ussrEnabled[\s\S]*return\s*;' 'Campaign setter must avoid redundant replication bumps'
    Assert-AICFContains $failures 'AI_COMMANDER_REPLICATION' $setStateCode 'm_bAICFUSAICommanderEnabled\s*=\s*usEnabled[\s\S]*m_bAICFUSSRAICommanderEnabled\s*=\s*ussrEnabled[\s\S]*Replication\.BumpMe\s*\(\s*\)' 'Campaign setter must assign both flags before one replication bump'
    Assert-AICFContains $failures 'AI_COMMANDER_REPLICATION' $getEnabledCode 'if\s*\(\s*isUSSR\s*\)[\s\S]*m_bAICFUSSRAICommanderEnabled[\s\S]*m_bAICFUSAICommanderEnabled' 'Campaign getter must select the requested faction flag'
    Assert-AICFContains $failures 'AI_COMMANDER_REPLICATION' $hasStateCode 'm_bAICFUSAICommanderEnabled\s*\|\|\s*m_bAICFUSSRAICommanderEnabled' 'false/false must remain the pre-snapshot COMMAND SYNC sentinel'
}

if (Require-AICommanderClass $strategicUI 'AICF_StrategicUIController') {
    foreach ($label in @('AI COMMANDER', 'PLAYER COMMAND', 'COMMAND SYNC')) {
        Assert-AICFContains $failures 'AI_COMMANDER_UI_STATE' $strategicUI.Source ('"' + $label + '"') "Strategic UI omits exact label $label"
    }
    $refreshPanel = Assert-AICommanderMethodPresent $strategicUI 'RefreshCommandPanel' 'AI_COMMANDER_UI_STATE'
    $targetMode = Assert-AICommanderMethodPresent $strategicUI 'GetTargetMode' 'AI_COMMANDER_UI_STATE'
    $authorityLabel = Assert-AICommanderMethodPresent $strategicUI 'GetCommandAuthorityLabel' 'AI_COMMANDER_UI_STATE'
    $refreshCode = ConvertTo-AICFCodeText $refreshPanel
    $authorityLabelCode = ConvertTo-AICFCodeText $authorityLabel
    Assert-AICFContains $failures 'AI_COMMANDER_UI_STATE' $refreshCode 'GetCommandAuthorityLabel\s*\(\s*\)' 'Command panel must render the replicated authority label'
    Assert-AICFContains $failures 'AI_COMMANDER_UI_STATE' $authorityLabelCode 'AICF_HasAICommanderState\s*\(\s*\)[\s\S]*AICF_GetAICommanderEnabled\s*\(\s*m_bLocalUSSR\s*\)' 'UI authority label must use the replicated readiness and faction flag'
    Assert-AICFContains $failures 'AI_COMMANDER_UI_STATE' $targetMode 'fields\[2\]\s*!=\s*"READY"\s*&&\s*fields\[2\]\s*!=\s*"AWAITING_PLAYER_COMMAND"' 'Target parser must accept both READY and AWAITING_PLAYER_COMMAND'
}

if (Require-AICommanderClass $groupMapMarkers 'AICF_GroupMapMarkerSystem') {
    $describeTask = Assert-AICommanderMethodPresent $groupMapMarkers 'DescribeTask' 'AI_COMMANDER_UI_STATE'
    $syncObjectives = Assert-AICommanderMethodPresent $groupMapMarkers 'SyncFactionObjectiveMarkers' 'AI_COMMANDER_UI_STATE'
    Assert-AICFContains $failures 'AI_COMMANDER_UI_STATE' (ConvertTo-AICFCodeText $describeTask) 'IsAwaitingPlayerCommand\s*\(\s*\)\s*\|\|\s*slot\.IsSystemHoldOrder\s*\(\s*\)' 'Map task text must recognize player-command waiting before role-specific labels'
    Assert-AICFContains $failures 'AI_COMMANDER_UI_STATE' $describeTask '"AWAITING PLAYER COMMAND"' 'Map task text must expose the exact player-command waiting label'
    Assert-AICFContains $failures 'AI_COMMANDER_UI_STATE' (ConvertTo-AICFCodeText $syncObjectives) 'IsAwaitingPlayerCommand\s*\(\s*\)\s*\|\|\s*slot\.IsSystemHoldOrder\s*\(\s*\)' 'SYSTEM_HOLD ATTACK slots must not create false objective markers on their own HQ'
}

if (Require-AICommanderClass $vehicleHandoff 'AICF_VehicleTaskHandoff') {
    $restoreInfantry = Assert-AICommanderMethodPresent $vehicleHandoff 'RestoreInfantryOrder' 'AI_COMMANDER_VEHICLE_RESTORE'
    $reportRestored = Assert-AICommanderMethodPresent $vehicleHandoff 'ReportRestoredInfantryOrder' 'AI_COMMANDER_VEHICLE_RESTORE'
    Assert-AICFContains $failures 'AI_COMMANDER_VEHICLE_RESTORE' (ConvertTo-AICFCodeText $restoreInfantry) 'loneSurvivorRetreat\s*&&\s*!slot\.IsSystemHoldOrder\s*\(\s*\)[\s\S]*AssignLoneSurvivorRetreat\s*\(' 'Vehicle fallback must rebuild a SYSTEM_HOLD Defend waypoint instead of replacing it with lone-survivor Move'
    Assert-AICFContains $failures 'AI_COMMANDER_VEHICLE_RESTORE' (ConvertTo-AICFCodeText $reportRestored) 'if\s*\(\s*!slot\.IsSystemHoldOrder\s*\(\s*\)\s*\)[\s\S]*BeginOrderRecoveryVerification\s*\(' 'Vehicle-restored SYSTEM_HOLD must stay outside generic stuck/task-loss recovery accounting'
}

if (Require-AICommanderClass $vehicleHandoffState 'AICF_VehicleHandoffState') {
    $rearmRestore = Assert-AICommanderMethodPresent $vehicleHandoffState 'RearmOrderRestoreForIntent' 'AI_COMMANDER_VEHICLE_RESTORE'
    $rearmRestoreCode = ConvertTo-AICFCodeText $rearmRestore
    Assert-AICFContains $failures 'AI_COMMANDER_VEHICLE_RESTORE' $rearmRestoreCode 'strategicIntentRevision\s*<=\s*m_iOrderRestoreIntentRevision[\s\S]*return\s+false\s*;' 'Only a strictly newer strategic-intent revision may rearm HANDOFF order restore'
    foreach ($orderField in @(
        'm_iRestoreAttempts',
        'm_bRestoreRequested',
        'm_bBoundToGroup',
        'm_bIsCurrent',
        'm_bWaypointInQueue',
        'm_bMeaningfulTask',
        'm_bOrderRestored',
        'm_iDurablePollCount',
        'm_RestoredWaypoint'
    )) {
        Assert-AICFContains $failures 'AI_COMMANDER_VEHICLE_RESTORE' $rearmRestoreCode ([regex]::Escape($orderField) + '\s*=') "HANDOFF rearm must reset order-only field $orderField"
    }
    Assert-AICFNotContains $failures 'AI_COMMANDER_VEHICLE_RESTORE' $rearmRestoreCode 'm_b(?:LeaseReleaseRequested|CleanupQueueAttempted|CleanupQueueAccepted|CleanupReleaseComplete|CleanupRetainedFailClosed|CleanupOwnershipAcceptedTerminal|ClearanceSafe)\s*=' 'HANDOFF order rearm must preserve lease, cleanup, and clearance evidence'
}

if (Require-AICommanderClass $assignmentSnapshot 'AICF_StrategicAssignmentSnapshot') {
    $snapshotConstructor = Get-AICFBracedBody $assignmentSnapshot.Source '(?m)^\s*void\s+AICF_StrategicAssignmentSnapshot\s*\('
    $snapshotGetter = Assert-AICommanderMethodPresent $assignmentSnapshot 'GetStrategicIntentRevision' 'AI_COMMANDER_VEHICLE_RESTORE'
    Assert-AICFContains $failures 'AI_COMMANDER_VEHICLE_RESTORE' $snapshotConstructor 'm_iStrategicIntentRevision\s*=\s*strategicIntentRevision\s*;' 'Snapshot constructor must retain the strategic-intent revision independently'
    Assert-AICFContains $failures 'AI_COMMANDER_VEHICLE_RESTORE' $snapshotGetter 'm_iStrategicIntentRevision' 'Snapshot getter must expose the retained strategic-intent revision'
}

if (Require-AICommanderClass $vehicleCoordinator 'AICF_VehicleCoordinator') {
    $isSuspended = ConvertTo-AICFCodeText (Assert-AICommanderMethodPresent $vehicleCoordinator 'IsInfantryOrderSuspended' 'AI_COMMANDER_VEHICLE_SUSPENSION')
    Assert-AICFContains $failures 'AI_COMMANDER_VEHICLE_SUSPENSION' $isSuspended 'BOARDING[\s\S]*TRANSIT[\s\S]*DISMOUNT[\s\S]*HANDOFF[\s\S]*IsRestoreRequested' 'Vehicle suspension must cover every phase that owns or is restoring the infantry waypoint queue'
}

if (Require-AICommanderClass $transportTripController 'AICF_TransportTripController') {
    $currentIdentity = ConvertTo-AICFCodeText (Assert-AICommanderMethodPresent $transportTripController 'HasCurrentIdentity' 'AI_COMMANDER_VEHICLE_SUSPENSION')
    $tripTick = ConvertTo-AICFCodeText (Assert-AICommanderMethodPresent $transportTripController 'Tick' 'AI_COMMANDER_VEHICLE_RESTORE')
    $controllerRearmBody = Assert-AICommanderMethodPresent $transportTripController 'RearmHandoffOrderRestoreForIntent' 'AI_COMMANDER_VEHICLE_RESTORE'
    $controllerRearm = ConvertTo-AICFCodeText $controllerRearmBody
    Assert-AICFContains $failures 'AI_COMMANDER_VEHICLE_SUSPENSION' $currentIdentity 'slot\.GetSlotId\s*\([\s\S]*slot\.GetSpawnGeneration\s*\([\s\S]*slot\.GetGroup\s*\([\s\S]*slot\.GetStrategicAssignmentRevision\s*\(' 'Trip callbacks must validate stable faction+numeric-slot, generation/entity, and assignment revision'
    Assert-AICFNotContains $failures 'AI_COMMANDER_VEHICLE_SUSPENSION' $currentIdentity 'GetSlotKey\s*\(' 'Role-local slot names must not participate in durable Trip identity'
    Assert-AICommanderOrdered 'AI_COMMANDER_VEHICLE_RESTORE' $tripTick 'HasStaleRevision(trip, currentAssignment)' 'RearmHandoffOrderRestoreForIntent(trip, currentAssignment)' 'Stale revisions must be rejected before intent-scoped HANDOFF rearm'
    Assert-AICommanderOrdered 'AI_COMMANDER_VEHICLE_RESTORE' $tripTick 'RearmHandoffOrderRestoreForIntent(trip, currentAssignment)' 'if (trip.IsTerminal())' 'Intent-scoped HANDOFF rearm must run before terminal restore processing'
    Assert-AICFContains $failures 'AI_COMMANDER_VEHICLE_RESTORE' $controllerRearm 'GetStartedAtMs\s*\(\s*\)\s*<=\s*0[\s\S]*return\s*;' 'HANDOFF rearm must not initialize restore state before actual handoff start'
    Assert-AICFContains $failures 'AI_COMMANDER_VEHICLE_RESTORE' $controllerRearm 'GetStrategicIntentRevision\s*\(\s*\)' 'HANDOFF restore budget must bind to strategic intent rather than runtime waypoint revision'
    Assert-AICFNotContains $failures 'AI_COMMANDER_VEHICLE_RESTORE' $controllerRearm 'currentAssignment\.GetAssignmentRevision\s*\(\s*\)[\s\S]*RearmOrderRestoreForIntent' 'Runtime waypoint revision must not drive HANDOFF restore rearm'
    Assert-AICFContains $failures 'AI_COMMANDER_VEHICLE_RESTORE' $controllerRearmBody '"ORDER_RESTORE_REARMED"' 'Intent-scoped HANDOFF rearm must be diagnosable'
}

$allSources = ($records | ForEach-Object { $_.Source }) -join [Environment]::NewLine
$allStrings = ($records | ForEach-Object { $_.Strings }) -join [Environment]::NewLine
foreach ($eventName in @(
    'COMMAND_AUTHORITY_SET',
    'COMMAND_AUTHORITY_REPLICATED',
    'COMMAND_WAITING',
    'STRATEGIC_ASSIGNMENT'
)) {
    if ($allStrings -notmatch ('(?m)^' + [regex]::Escape($eventName) + '\r?$')) {
        Add-AICFAuditFailure $failures 'AI_COMMANDER_DIAGNOSTICS' "Missing exact event literal $eventName"
    }
}
foreach ($field in @(
    'ai_commander_mode=',
    'ai_commander_us=',
    'ai_commander_ussr=',
    'decision_authority=',
    'intent_revision='
)) {
    Assert-AICFContains $failures 'AI_COMMANDER_DIAGNOSTICS' $allSources ([regex]::Escape($field)) "Diagnostics omit $field"
}

if ($failures.Count -gt 0) {
    Write-Host "AI commander mode static audit: FAIL ($($failures.Count) issue(s))" -ForegroundColor Red
    $failures | ForEach-Object { Write-Host " - $_" -ForegroundColor Red }
    exit 1
}

Write-Host 'AI commander mode static audit: PASS' -ForegroundColor Green
Write-Host 'Checked: exact CLI mode, pre-loop rejection, immutable faction policy, commander-only selectors, stable-slot durable intent, player priority/revalidation, SYSTEM_HOLD/AWAITING behavior, all-READY replication, vehicle-suspended waypoint ownership/identity, US-to-USSR tick order, UI parser/labels, and decision-authority diagnostics.'
