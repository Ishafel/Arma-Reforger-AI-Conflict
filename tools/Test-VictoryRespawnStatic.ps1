param([string]$RepositoryRoot = (Split-Path -Parent $PSScriptRoot))
$ErrorActionPreference = 'Stop'
$core = Join-Path $RepositoryRoot 'AIConflictCore/Scripts/Game/AIConflict'
$sources = @{}
foreach ($entry in @{
    victory='Victory/AICF_VictorySystem.c'; slot='State/AICF_GroupSlot.c'
    selector='Economy/AICF_ReinforcementBaseSelector.c'; adapter='Integration/AICF_ConflictAdapter.c'
    economy='Economy/AICF_EconomySystem.c'; controller='Bootstrap/AICF_MatchController.c'
}.GetEnumerator()) { $sources[$entry.Key] = Get-Content (Join-Path $core $entry.Value) -Raw -Encoding UTF8 }
function Test-Contracts($s) {
    if ($s.victory -match 'base.AreEnemiesPresent\(\)') { 'PRESENCE_ONLY_ALLOWED' }
    $rules = @(
        @('victory', 'bool usWins = ussrState.GetTickets\(\) <= 0 \|\| ControlsAllObjectives', 'US_OR'),
        @('victory', 'bool ussrWins = usState.GetTickets\(\) <= 0 \|\| ControlsAllObjectives', 'USSR_OR'),
        @('victory', 'if \(usWins == ussrWins\)\s*return false;', 'UNIQUE_WINNER'),
        @('victory', '!Replication.IsServer\(\).*!campaign.IsMaster\(\)', 'AUTHORITY'),
        @('victory', 'if \(!node.IsObjective\(\)\)\s*continue;', 'RELAY_EXCLUDED'),
        @('victory', 'if \(base.IsHQ\(\)\)\s*continue;', 'HQ_EXCLUDED'),
        @('victory', 'base.GetFaction\(\).GetFactionKey\(\) != factionKey', 'OWNER'),
        @('victory', 'base.GetCaptureState\(\) != SCR_EBaseCaptureState.NONE', 'CAPTURE_STATE'),
        @('victory', 'base.IsBeingCaptured\(\)', 'CONTESTED'),
        @('victory', 'return objectives > 0;', 'NONEMPTY'),
        @('selector', 'if \(!slot.TryGetReplacementOrigin\(deathPosition\)\)\s*return false;', 'ORIGIN_REQUIRED'),
        @('selector', 'candidate.DeathDistanceSq < best.DeathDistanceSq', 'NEAREST'),
        @('selector', 'candidate.NodeId < best.NodeId', 'TIE_BREAK'),
        @('adapter', 'service.GetType\(\) == SCR_EServicePointType.BARRACKS &&\s*service.GetServiceState\(\) == SCR_EServicePointStatus.ONLINE', 'BARRACKS'),
        @('economy', 'GetReplacementSpawnRejectionReason\(reservation.GetBase\(\), faction\)', 'REVALIDATION'),
        @('slot', 'm_iSpawnGeneration != m_iReplacementObservedGeneration', 'GENERATION'),
        @('slot', 'GetLifeState\(\) != ECharacterLifeState.DEAD', 'DEATH_ONLY'),
        @('slot', 'GetOnAgentRemoved\(\).Remove\(OnReplacementMemberRemoved\)', 'UNSUBSCRIBE'),
        @('controller', 'slot.StopReplacementOriginObserver\(\);', 'STOP_OBSERVER')
    )
    foreach ($rule in $rules) { if ($s[$rule[0]] -notmatch $rule[1]) { $rule[2] } }
}
$failures = @(Test-Contracts $sources)
if (Test-Path (Join-Path $core 'Victory/AICF_VictoryRespawnProbe.c')) { $failures += 'FIXTURE_IN_PRODUCTION' }
$mutations = @(
    @('victory','base.IsBeingCaptured()','base.IsBeingCaptured() || base.AreEnemiesPresent()','PRESENCE_ONLY_ALLOWED'),
    @('victory','<= 0 || ControlsAllObjectives','<= 0 && ControlsAllObjectives','US_OR'),
    @('victory','return objectives > 0;','return true;','NONEMPTY'),
    @('victory','if (base.IsHQ())','if (false)','HQ_EXCLUDED'),
    @('victory','base.IsBeingCaptured()','false','CONTESTED'),
    @('victory','base.GetCaptureState() != SCR_EBaseCaptureState.NONE','false','CAPTURE_STATE'),
    @('selector','candidate.DeathDistanceSq < best.DeathDistanceSq','candidate.DeathDistanceSq > best.DeathDistanceSq','NEAREST'),
    @('selector','if (!slot.TryGetReplacementOrigin(deathPosition))','if (false)','ORIGIN_REQUIRED'),
    @('adapter','service.GetServiceState() == SCR_EServicePointStatus.ONLINE','true','BARRACKS'),
    @('economy','GetReplacementSpawnRejectionReason(reservation.GetBase(), faction)','GetSpawnRejectionReason(reservation.GetBase(), faction)','REVALIDATION'),
    @('slot','GetLifeState() != ECharacterLifeState.DEAD','GetLifeState() == ECharacterLifeState.DEAD','DEATH_ONLY'),
    @('slot','m_iSpawnGeneration != m_iReplacementObservedGeneration','false','GENERATION'),
    @('controller','slot.StopReplacementOriginObserver();','','STOP_OBSERVER')
)
foreach ($mutation in $mutations) {
    $copy = $sources.Clone()
    if (!$copy[$mutation[0]].Contains($mutation[1])) { throw "Missing mutation anchor: $($mutation[3])" }
    $copy[$mutation[0]] = $copy[$mutation[0]].Replace($mutation[1], $mutation[2])
    if ($mutation[3] -notin @(Test-Contracts $copy)) { $failures += "MUTATION_ESCAPED $($mutation[3])" }
}
if ($failures.Count) { $failures; exit 1 }
Write-Output "Victory/respawn static: PASS; mutations=$($mutations.Count)"
