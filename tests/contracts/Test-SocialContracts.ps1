param([string]$RepositoryRoot = (Split-Path -Parent (Split-Path -Parent $PSScriptRoot)))
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot '../lib/Stage3StaticAudit.Common.ps1')
$core = Join-Path $RepositoryRoot 'AIConflictCore/Scripts/Game/AIConflict'
$files = @('Config/AICF_SocialConfig.c','State/AICF_GreetingGate.c','Forces/AICF_SocialActor.c','Forces/AICF_SocialService.c','Integration/AICF_GreetingAudio.c')
$source = ($files | ForEach-Object { [IO.File]::ReadAllText((Join-Path $core $_)) }) -join "`n"

function Test-Social([string]$Source) {
    $failures = [Collections.Generic.List[string]]::new()
    $code = ConvertTo-AICFCodeText $Source
    $rules = [ordered]@{
        COOLDOWN = 'COOLDOWN_MS = 30000;'
        HYSTERESIS = '(?s)EXIT_METERS = 7;.*m_bInside = false;.*if \(m_bInside \|\| distance > AICF_SocialConfig.ENTER_METERS\)'
        RESERVATION = 'token <= 0 \|\| m_iPendingToken != 0 \|\| now < m_iNextAt'
        GENERATION = 'm_Slot.GetSpawnGeneration\(\) != m_iGeneration'
        READY = '!m_Slot.IsCombatReady\(\)'
        INITIAL_ONLY = 'slot.GetSpawnGeneration\(\) != 1'
        PLAYER = 'GetPlayerIdFromControlledEntity\(m_Member\) > 0'
        THREAT = 'utility.m_ThreatSystem.GetState\(\) != EAIThreatState.SAFE'
        PARTIAL = '(?s)candidate.BeginSalute\(\).*?player.Commit\(token, now\).*?AICF_PlayGreeting'
        SALUTE_FREE = 'm_iPhase != 0 \|\| controller.IsLoitering\(\) \|\| controller.IsPlayingGesture\(\)'
        OUT_TRANSITION = 'System.GetTickCount\(\) < m_iSaluteBlockedUntil'
        STALE_PLAYER = 'GetPlayerControlledEntity\(actor.m_iPlayerId\) != actor.m_PlayerEntity'
        ONE_BOT = 'actor.m_iToken > 0 \|\| !actor.Available\(\)'
        AUTHORITY = '!Replication.IsServer\(\) \|\| !m_Campaign \|\| !m_Campaign.IsMaster\(\)'
        CLEANUP = '(?s)Remove\(Tick\).*?Remove\(OnDisconnected\).*?SubscribeSlots\(m_US, false\).*?SubscribeSlots\(m_USSR, false\)'
        DEDUP = 'token <= m_iAICFLastGreetingToken'
        ENGLISH = 'GREETING_EVENT ='
        PROFILE = 'GetStableFactionKey\(playerFaction.GetFactionKey\(\)\)'
    }
    foreach ($rule in $rules.GetEnumerator()) {
        Assert-AICFContains $failures $rule.Key $code $rule.Value $rule.Key
    }
    if (-not $Source.Contains('SOUND_TUTORIAL_AM_GREETING_C_SOLDIER_E_01')) { $failures.Add('[ENGLISH] Fixed English event required') }
    if ($code -match 'RplProp|\.AddWaypoint\(|\.RequestSpawn\(|\.SetSupplies\(') { $failures.Add('[BOUNDARY] Social service must not own gameplay or JIP audio state') }
    return $failures.ToArray()
}
$failures = @(Test-Social $source)
if ($failures.Count) { $failures; exit 1 }
$mutations = @(
    @('COOLDOWN','COOLDOWN_MS = 30000;','COOLDOWN_MS = 29999;'),
    @('RESERVATION','m_iPendingToken != 0','false'),
    @('GENERATION','m_Slot.GetSpawnGeneration() != m_iGeneration','false'),
    @('READY','!m_Slot.IsCombatReady()','false'),
    @('INITIAL_ONLY','slot.GetSpawnGeneration() != 1','false'),
    @('PLAYER','GetPlayerIdFromControlledEntity(m_Member) > 0','GetPlayerIdFromControlledEntity(m_Member) < 0'),
    @('THREAT','utility.m_ThreatSystem.GetState() != EAIThreatState.SAFE','false'),
    @('PARTIAL','player.Commit(token, now)','player.Release(token)'),
    @('STALE_PLAYER','GetPlayerControlledEntity(actor.m_iPlayerId) != actor.m_PlayerEntity','GetPlayerControlledEntity(actor.m_iPlayerId) == actor.m_PlayerEntity'),
    @('SALUTE_FREE','m_iPhase != 0 || controller.IsLoitering() || controller.IsPlayingGesture()', 'false'),
    @('OUT_TRANSITION','System.GetTickCount() < m_iSaluteBlockedUntil','false'),
    @('ONE_BOT','actor.m_iToken > 0 || !actor.Available()','!actor.Available()'),
    @('CLEANUP','Remove(Tick)','Remove(Advance)'),
    @('DEDUP','token <= m_iAICFLastGreetingToken','token < 0'),
    @('ENGLISH','SOUND_TUTORIAL_AM_GREETING_C_SOLDIER_E_01','SOUND_OTHER')
)
foreach ($mutation in $mutations) {
    $rejected = @(Test-Social ($source.Replace($mutation[1], $mutation[2])))
    if (-not ($rejected -match ('\[' + $mutation[0] + '\]'))) { throw "Mutation escaped: $($mutation[0])" }
}
Write-Output "Social contracts: PASS; rules=19; negative mutations=$($mutations.Count); runtime=NOT_RUN"
