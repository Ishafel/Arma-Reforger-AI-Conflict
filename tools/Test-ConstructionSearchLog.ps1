[CmdletBinding()]
param(
    [Parameter(Mandatory=$true)][string]$LogPath,
    [ValidateSet('functional','dynamic','owner','provider','cancel','context','no-site')][string]$Scenario = 'functional',
    [switch]$RequireBothSmall,
    [int]$MaxSmallPlacementMs = 0
)
$ErrorActionPreference = 'Stop'
$lines = @(Get-Content -LiteralPath $LogPath)
$raw = $lines -join "`n"
$failures = [System.Collections.Generic.List[string]]::new()
function Check([string]$Name, [bool]$Passed) {
    if ($Passed) { Write-Output "PASS $Name" }
    else { Write-Output "FAIL $Name"; $failures.Add($Name) }
}
Check 'STOPPED_LOG' ($raw -match 'Game destroyed\.')
Check 'ROSTER_READY' ($raw -match '\[ROSTER_READY\]')
Check 'NO_ENGINE_SCRIPT_ERROR' ($raw -notmatch 'SCRIPT\s+\([EF]\)|ENGINE\s+\(F\)|Virtual Machine Exception|NULL pointer')
Check 'NO_SEARCH_COOLDOWN' ($raw -notmatch '\[CONSTRUCTION_SEARCH_PAUSED\]')
if ($RequireBothSmall) {
    foreach ($faction in @('US','USSR')) {
        $placed = @($lines | Where-Object { $_ -match "\[CONSTRUCTION_PLACED\].*?faction=$faction\s.*?type=SMALL_BARRACKS\s" })
        $completed = @($lines | Where-Object { $_ -match "\[CONSTRUCTION_COMPLETED\].*?faction=$faction\s.*?type=SMALL_BARRACKS\s.*?service_online=1" })
        Check "$faction/SMALL_PLACED" ($placed.Count -gt 0)
        Check "$faction/SMALL_SERVICE_ONLINE" ($completed.Count -gt 0)
        foreach ($line in $completed) {
            if ($line -notmatch 'layout=(\S+)') { throw 'Completed layout missing' }
            $layout = [regex]::Escape($Matches[1])
            Check "$faction/PHYSICAL_WORK" ($raw -match "\[BUILDER_WORK_STARTED\].*?faction=$faction\s.*?target=$layout\s.*?tool_active=1 item_using=1")
            Check "$faction/WORKER_COMPLETED" ($raw -match "\[BUILDER_COMPLETED\].*?faction=$faction\s.*?target=$layout\s.*?tool_active=1 item_using=1")
        }
        if ($MaxSmallPlacementMs -gt 0 -and $placed.Count) {
            $placed[0] -match 'token=(\S+)' | Out-Null
            $placedToken = [regex]::Escape($Matches[1])
            $placed[0] -match 't_ms=(\d+)' | Out-Null
            $placedAt = [int]$Matches[1]
            $decision = @($lines | Where-Object { $_ -match "\[CONSTRUCTION_DECISION\].*?token=$placedToken\s" })
            Check "$faction/DECISION_OBSERVED" ($decision.Count -eq 1)
            if ($decision.Count -eq 1) {
                $decision[0] -match 't_ms=(\d+)' | Out-Null
                Check "$faction/SMALL_WITHIN_${MaxSmallPlacementMs}MS" ($placedAt - [int]$Matches[1] -le $MaxSmallPlacementMs)
            }
        }
    }
}
if ($Scenario -eq 'no-site') {
    Check 'ANALYTIC_AREA_EXHAUSTED' ($raw -match '\[CONSTRUCTION_CANCELLED\].*?reason=SEARCH_AREA_EXHAUSTED.*?cause=FOOTPRINT_EXCEEDS_PROVIDER_DIAMETER.*?search_status=AREA_EXHAUSTED')
    Check 'NO_PLACEMENT_OR_PAYMENT' ($raw -notmatch '\[CONSTRUCTION_(PLACED|PAYMENT)\]')
} elseif ($Scenario -ne 'functional') {
    $injected = @($lines | Where-Object { $_ -match "\[CONSTRUCTION_FAULT_PROBE\].*?fault=$Scenario injected=1" })
    Check 'FAULT_INJECTED_ONCE' ($injected.Count -eq 1)
    if ($injected.Count -eq 1) {
        $injected[0] -match 'token=(\S+)' | Out-Null
        $token = [regex]::Escape($Matches[1])
        if ($Scenario -eq 'dynamic') {
            Check 'LIVE_BLOCKER_REJECTED_UNPAID' ($raw -match "\[CONSTRUCTION_FAULT_PROBE\].*?token=$token\s.*?fault=dynamic clear=0 expected_clear=0 paid=0 blocker_match=1")
            Check 'SELECTED_REJECTION_RECORDED' ($raw -match "\[CONSTRUCTION_SELECTED_REJECTED\].*?token=$token\s.*?phase=COMMIT_GEOMETRY")
            Check 'SAME_ORDER_CONTINUED_TO_PLACEMENT' ($raw -match "\[CONSTRUCTION_PLACED\].*?token=$token\s")
            Check 'SAME_ORDER_COMPLETED' ($raw -match "\[CONSTRUCTION_COMPLETED\].*?token=$token\s.*?service_online=1")
        } else {
            Check 'PENDING_ORDER_CLEANED_UNPAID' ($raw -match "\[CONSTRUCTION_FAULT_PROBE\].*?token=$token\s.*?cancelled=1 accepted=0 paid=0 reserved=0 checkpoints=0 path_present=0")
            Check 'INVALIDATED_ORDER_NEVER_PLACED' ($raw -notmatch "\[CONSTRUCTION_PLACED\].*?token=$token\s")
        }
    }
}
if ($failures.Count) { Write-Output "Construction search runtime: FAIL ($($failures -join ','))"; exit 1 }
Write-Output 'Construction search runtime: PASS'
