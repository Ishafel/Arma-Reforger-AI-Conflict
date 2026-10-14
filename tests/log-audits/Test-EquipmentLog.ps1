[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)][string]$LogPath,
    [Parameter(Mandatory = $true)][ValidateSet('Full','Inventory','Safety','Lifecycle','Player','Search')][string]$Mode,
    [Parameter(Mandatory = $true)][string]$OutputDirectory,
    [int]$ExpectedFamilies = 0
)
$ErrorActionPreference = 'Stop'
$lines = @(Get-Content -LiteralPath $LogPath)
$events = @{ Full='EQUIPMENT_PROBE'; Inventory='EQUIPMENT_PROBE'; Safety='EQUIPMENT_SAFETY'; Lifecycle='EQUIPMENT_LIFECYCLE'; Player='EQUIPMENT_PLAYER'; Search='EQUIPMENT_SEARCH' }
$required = @{
    Full=@('THRESHOLD_ABOVE','THRESHOLD_BELOW','ONE_SHOT','HYSTERESIS','SPAWN_ASSIGNED_INVENTORY','SPAWN_DEFAULT_WEAPON','PHYSICAL_APPROACH','SAME_ITEM_IDENTITY','PARTIAL_LOOT_NEED_RETAINED','SUFFICIENT_LOOT_CANCELS_VISIT','STALE_BINDING_REJECTED','NO_DISTANT_ISSUE','LIVE_ISSUE_ROLLBACK','LATEST_REVISION_APPLIED','TARGET_ROUNDS','ISSUE_UNLOCKED_AND_LEDGER_UNCHANGED','NO_RESUPPLY_LOOP','ORIGINAL_INTENT_RESTORED','RECRUITMENT_FENCED','TRANSPORT_FENCED','SECOND_SERVICE_FENCED')
    Inventory=@('SPAWN_ASSIGNED_INVENTORY','SPAWN_DEFAULT_WEAPON','LIVE_ISSUE_ROLLBACK','LATEST_REVISION_APPLIED','TARGET_ROUNDS','ISSUE_UNLOCKED_AND_LEDGER_UNCHANGED','NO_RESUPPLY_LOOP','ORIGINAL_INTENT_RESTORED')
    Safety=@('ONE_SHOT_REAL_INVENTORY','PARTIAL_MAGAZINES_ROUNDS','TWO_PHYSICAL_RECIPIENTS','ONE_ITEM_ONE_OWNER','WITHDRAWN_ITEM_REJECTED','COMBAT_BEFORE_START','COMBAT_CANCELS_SERVICE','NON_AT_ROLE_REFUSED','NATIVE_TRANSFER_REFUSED','FULL_STORAGE_REFUSED','OCCUPIED_WEAPON_SLOT_REFUSED','LAUNCHER_TRANSFER','ROUND_TRANSFER','LOADED_READBACK','DISPOSABLE_USED_EMPTY','DISPOSABLE_EMPTY_DEFICIT','DISPOSABLE_SPENT_SOURCE_REFUSED','ENEMY_CORPSE_AMMO','FIA_CORPSE_AMMO','CORPSE_DELETE_ORDER_CURRENT','REMOVED_CORPSE_REFUSED','NEW_MANUAL_ORDER_PRESERVED')
    Lifecycle=@('ACTIVE_RECRUITMENT_FENCES_SERVICE','ACTIVE_TRANSPORT_FENCES_SERVICE','SOURCE_NEAREST_ARMORY','OWNER_LOSS_REJECTED','OWNER_LOSS_ALTERNATIVE','REMOVED_SERVICE_REJECTED','REMOVED_SERVICE_ALTERNATIVE','NO_ALTERNATIVE_NEED_RETAINED','BOUNDED_RETRY_CLOCK','DEAD_MEMBER_REJECTED','REPLACEMENT_STALE_REJECTED','STOP_CLEARS_AND_NO_MUTATIONS')
    Player=@('CONNECTED_PLAYER_CONTROL','LIVE_PLAYER_SOURCE_REFUSED','DEAD_PLAYER_MAIN_SOURCE_REFUSED','PLAYER_TAKEOVER_CANCELS','PLAYER_INVENTORY_NO_STALE_MUTATION')
    Search=@('INSIDE_SEARCH_RADIUS','STABLE_ID_TIE_BREAK','OUTSIDE_RADIUS_NOT_SELECTED','REAL_CORPSE_TIMEOUT','NO_DISTANT_LOOT_OR_STUCK_SLOT','SEARCH_RETRY_BOUNDED')
}
$failures = New-Object 'System.Collections.Generic.List[string]'
$rows = @(foreach ($line in $lines) {
    if ($line -match ('\[' + $events[$Mode] + '\] case=(\S+) passed=([01]) phase=(\d+)')) {
        [pscustomobject]@{ case=$Matches[1]; passed=($Matches[2] -eq '1'); phase=[int]$Matches[3]; line=$line }
    }
})
foreach ($row in $rows) { if (-not $row.passed) { $failures.Add("Failed case: $($row.case)") } }
foreach ($case in $required[$Mode]) { if ($case -notin $rows.case) { $failures.Add("Missing case: $case") } }
$finish = @($lines | Select-String -Pattern ('\[' + $events[$Mode] + '_FINISHED\] checks=(\d+) failures=(\d+)(?: families=(\d+))?'))
if ($finish.Count -ne 1) { $failures.Add('Expected exactly one completion marker') }
elseif ([int]$finish[0].Matches[0].Groups[1].Value -ne $rows.Count -or [int]$finish[0].Matches[0].Groups[2].Value -ne 0) { $failures.Add('Completion count/failures disagree with cases') }
if ($ExpectedFamilies -gt 0 -and ($finish.Count -ne 1 -or [int]$finish[0].Matches[0].Groups[3].Value -ne $ExpectedFamilies)) { $failures.Add('Unexpected family matrix') }
if (-not ($lines -match 'ENGINE\s+: Game destroyed\.')) { $failures.Add('Log is not a full stopped server log') }
if (-not ($lines -match '\[ROSTER_READY\]')) { $failures.Add('Roster never ready') }
if ($lines -match 'Virtual Machine Exception|NULL pointer to instance|SCRIPT\s+\(F\)|ENGINE\s+\(F\)|EQUIPMENT_RECOVERY_TIMEOUT|READBACK_NOT_OWNED|ORIGINAL_RETAINED_AT_FEET') { $failures.Add('Runtime exception or incomplete inventory recovery') }
[void](New-Item -ItemType Directory -Path $OutputDirectory -Force)
$matrix = @($lines | Where-Object { $_ -match '\[EQUIPMENT_SAFETY\] case=(LAUNCHER_TRANSFER|ROUND_TRANSFER|DISPOSABLE_USED_EMPTY)' })
$matrix | Set-Content -LiteralPath (Join-Path $OutputDirectory 'families.txt') -Encoding UTF8
$report = [ordered]@{ verdict='PASS'; mode=$Mode; log=(Resolve-Path -LiteralPath $LogPath).Path; sha256=(Get-FileHash -LiteralPath $LogPath).Hash; checks=$rows.Count; failures=@($failures); cases=$rows; engine_diagnostics=@($lines | Where-Object { $_ -match '\([EF]\)' }) }
if ($failures.Count) { $report.verdict='FAIL' }
$report | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path $OutputDirectory 'report.json') -Encoding UTF8
if ($failures.Count) { throw ($failures -join '; ') }
"PASS Equipment $Mode log: $($rows.Count) cases; full stopped log; diagnostics saved separately"
