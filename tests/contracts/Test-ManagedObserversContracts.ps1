param([string]$RepositoryRoot = (Split-Path -Parent (Split-Path -Parent $PSScriptRoot)))
$ErrorActionPreference = 'Stop'
$core = Join-Path $RepositoryRoot 'AIConflictCore/Scripts/Game/AIConflict'
$source = [IO.File]::ReadAllText((Join-Path $core 'Forces/AICF_ManagedObservers.c'))
$root = [IO.File]::ReadAllText((Join-Path $core 'Bootstrap/AICF_MatchController.c'))
function Rules([string]$s, [string]$r) {
    [ordered]@{
        AUTHORITY = $s.Contains('!Replication.IsServer() || !campaign || !campaign.IsMaster()') -and $s.Contains('Replication.IsServer() && m_Campaign && m_Campaign.IsMaster() && m_Campaign.IsRunning()')
        READY = $s.Contains('!slot || !slot.IsCombatReady()')
        LIVE_POSITION = $s.Contains('AICF_GroupRuntime.ResolveAliveLeader(group)') -and $s.Contains('InsertObserverSP(key, 0, 0, member)') -and $s -notmatch 'group.GetOrigin\('
        GROUP_AUTHORITY = $s.Contains('rpl && !rpl.IsMaster()')
        STABLE_FACTION = $s.Contains('(expected != "US" && expected != "USSR")') -and $s.Contains('GetStableFactionKey(faction.GetFactionKey()) != expected')
        STATE_PROFILE = $s.Contains('GetStableFactionKey(state.GetFactionKey())') -and $s.Contains('GetStableFactionKey(us.GetFactionKey()) == "US"') -and $s.Contains('GetStableFactionKey(ussr.GetFactionKey()) == "USSR"')
        EMPTY_REJECTED = $s.Contains('if (!member) return null;') -and $s.Contains('if (!member) continue;')
        SLOT_KEY = $s.Contains('OBSERVER_KEY_BASE + 2 * slot.GetSlotId() + side')
        REMOVE_STALE = $s.Contains('if (!current.Contains(key)) m_Observers.RemoveObserverSP(key);') -and $s.Contains('m_aKeys.Copy(current);')
        STOP = $s.Contains('if (m_bStopped) return;') -and $s.Contains('foreach (int key : m_aKeys) m_Observers.RemoveObserverSP(key);') -and $s.Contains('m_aKeys.Clear();') -and $s.Contains('m_Observers = null;') -and $s.Contains('m_Campaign = null;')
        STOP_NO_REINSERT = $s.Contains('if (m_bStopped || !m_Observers) return;')
        NATIVE_OWNERS = $s -notmatch '\.(SpawnPatrol|RequestSpawn|SpawnMembers|SetLifecyclePolicy|SetLimitOfActiveAIs|SetIsEliminated|DeleteRplEntity|PreventMaxLOD)\('
        SINGLE_SCHEDULER = $s -notmatch 'CallLater\(|\.Insert\(.*Tick' -and $r.Contains('m_ManagedObservers.Update(m_USState, m_USSRState);')
        ROOT_LIFECYCLE = $r.Contains('m_ManagedObservers.Start(m_Campaign);') -and $r.Contains('m_ManagedObservers.Stop();') -and $r.Contains('m_ManagedObservers = null;')
    }
}
$rules = Rules $source $root
$failures = @($rules.Keys | Where-Object { -not $rules[$_] })
if ($failures.Count) { throw ('Managed observers: ' + ($failures -join ', ')) }
$mutations = @(
    @('!Replication.IsServer() || !campaign || !campaign.IsMaster()', '!campaign', 'AUTHORITY'),
    @('!slot || !slot.IsCombatReady()', '!slot', 'READY'),
    @('AICF_GroupRuntime.ResolveAliveLeader(group)', 'group.GetLeaderEntity()', 'LIVE_POSITION'),
    @('InsertObserverSP(key, 0, 0, member)', 'InsertObserverSP(key, 0, 0, group)', 'LIVE_POSITION'),
    @('rpl && !rpl.IsMaster()', 'false', 'GROUP_AUTHORITY'),
    @('GetStableFactionKey(faction.GetFactionKey()) != expected', 'GetStableFactionKey(faction.GetFactionKey()) == "FIA"', 'STABLE_FACTION'),
    @('AICF_ContentProfile.GetActive().GetStableFactionKey(state.GetFactionKey())', 'state.GetFactionKey()', 'STATE_PROFILE'),
    @('if (!member) return null;', '', 'EMPTY_REJECTED'),
    @('2 * slot.GetSlotId() + side', 'index', 'SLOT_KEY'),
    @('if (!current.Contains(key)) m_Observers.RemoveObserverSP(key);', '', 'REMOVE_STALE'),
    @('foreach (int key : m_aKeys) m_Observers.RemoveObserverSP(key);', '', 'STOP'),
    @('if (m_bStopped || !m_Observers) return;', 'if (!m_Observers) return;', 'STOP_NO_REINSERT')
)
foreach ($mutation in $mutations) {
    if (-not $source.Contains($mutation[0])) { throw "Missing mutation $($mutation[2])" }
    if ((Rules ($source.Replace($mutation[0], $mutation[1])) $root)[$mutation[2]]) { throw "Mutation survived $($mutation[2])" }
}
if ((Rules ($source + "`n group.RequestSpawn();") $root).NATIVE_OWNERS) { throw 'Native bypass mutation survived' }
if ((Rules $source ($root.Replace('m_ManagedObservers.Stop();', ''))).ROOT_LIFECYCLE) { throw 'Root cleanup mutation survived' }
"PASS Managed observers: $($rules.Count) rules, $($mutations.Count + 2) negative mutations; native runtime is a separate gate"
