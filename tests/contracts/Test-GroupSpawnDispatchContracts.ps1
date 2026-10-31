param([string]$RepositoryRoot = (Split-Path -Parent (Split-Path -Parent $PSScriptRoot)))
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot '../lib/Stage3StaticAudit.Common.ps1')
$taskPlacement=Get-Content -LiteralPath (Join-Path $RepositoryRoot 'AIConflictCore/Scripts/Game/AIConflict/Forces/AICF_InfantrySpawnPlacement.c') -Raw
$taskRuntime=Get-Content -LiteralPath (Join-Path $RepositoryRoot 'AIConflictCore/Scripts/Game/AIConflict/State/AICF_GroupRuntime.c') -Raw
$taskFixture=Get-Content -LiteralPath (Join-Path $RepositoryRoot 'tests/fixtures/AICF_GroupReplacementStressProbe.c') -Raw
function Test-Dispatch([string]$Placement,[string]$Runtime,[string]$Fixture){
 $issues=[Collections.Generic.List[string]]::new()
 $code=ConvertTo-AICFCodeText $Placement
 Assert-AICFContains $issues 'BOUNDARY_AUTHORITY' $code 'if \(!m_bAICFSafeInfantrySpawn \|\| !Replication.IsServer\(\)\) return super.ExpandOneMember\(\);' 'Diagnostics must only instrument explicitly managed authoritative groups'
 Assert-AICFContains $issues 'BOUNDARY_DISPATCH' $code '(?s)m_iAICFExpansionCalls\+\+;.*?bool expanded = super.ExpandOneMember\(\);.*?if \(expanded\) m_iAICFExpansionSucceeded\+\+;.*?return expanded;' 'Observe one real expansion dispatch and preserve its result'
 Assert-AICFContains $issues 'BOUNDARY_NULL_SPAWN' $code '(?s)int agentsBefore = GetAgentsCount\(\);.*?bool spawned = super.SpawnGroupMember.*?if \(GetAgentsCount\(\) <= agentsBefore\).*?m_iAICFSpawnWithoutAgent\+\+;' 'Native success without an added agent must be distinguishable from actual materialisation'
 foreach($field in @('expansion_calls=','expansion_successes=','expansion_age_ms=','member_spawn_calls=','member_spawn_age_ms=','placement_deferred=','spawn_without_agent=')){
  Assert-AICFContains $issues 'BOUNDARY_FIELDS' $Placement ([regex]::Escape($field)) "Missing real dispatch diagnostic $field"
 }
 foreach($api in @('GetCurrentNumOfActiveAIs','GetLimitOfActiveAIs','GetActiveGroupIntendedCount','CanActivateGroup','CanAIBeActivated','GetImportance','GetDormantAliveCount','GetDormantDeadCount','IsExpandComplete','AICF_DescribeSpawnDispatch')){
  Assert-AICFContains $issues 'NATIVE_READBACK' (ConvertTo-AICFCodeText $Runtime) ($api+'\(') "Missing verified native readback $api"
 }
 $probe=ConvertTo-AICFCodeText $Fixture
 Assert-AICFContains $issues 'STRESS_EXACT_ROSTER' $probe '(?s)HasExactFactionRoster\(slot.GetGroup\(\), state.GetFactionKey\(\), 5, actual, wrongFaction, nonAlive\).*?GetSpawnGeneration\(\) == m_aAICFReplacementStressGenerations\[record\] \+ 1' 'Stress must require exact actual five and first replacement generation'
 Assert-AICFContains $issues 'STRESS_CLEANUP' $probe '(?s)override protected void Stop\(bool cleanupEntities\).*?Remove\(AICF_ReplacementStressTick\)' 'Stop must cancel the recurring observation'
 Assert-AICFContains $issues 'STRESS_CAPACITY_THRESHOLD' $probe '(?s)int free = limit - Math.Max\(active, intended\);\s*if \(limit <= 0 \|\| free >= 5\) return false;' 'Capacity mode must wait on actual native budget with fewer free slots than the requested five'
 Assert-AICFNotContains $issues 'STRESS_NO_BYPASS' $probe '\b(SpawnUnits|ExpandOneMember|RequestSpawn|MarkReady|SetDormantCounts|SetImportance|SetHealth)\s*\(' 'Stress must use ordinary replacement lifecycle rather than bypass readiness or queue limits'
 $record=[pscustomobject]@{Source=$Fixture;Code=$probe}
 $inject=Get-AICFMethodBody $record 'AICF_ReplacementStressInjectCapacity'
 $restore=Get-AICFMethodBody $record 'AICF_ReplacementStressRestoreCapacity'
 if(([regex]::Matches($probe,'SetLimitOfActiveAIs\(')).Count -ne 2 -or
    $inject -notmatch '(?s)if \(!m_bAICFReplacementStressInjected.*?return;.*?m_iAICFReplacementStressSavedLimit = world.GetLimitOfActiveAIs\(\).*?world.SetLimitOfActiveAIs\(injectedLimit\)' -or
    $restore -notmatch '(?s)if \(!m_bAICFReplacementStressInjected.*?return;.*?world.SetLimitOfActiveAIs\(originalLimit\)' -or
    (Get-AICFMethodBody $record 'Stop') -notmatch 'AICF_ReplacementStressRestoreCapacity\(\)'){$issues.Add('[STRESS_INJECTION_SCOPE] Only explicit fixture injection may change native limit and Stop must restore it')}
 return $issues.ToArray()
}
$taskIssues=@(Test-Dispatch $taskPlacement $taskRuntime $taskFixture)
if($taskIssues.Count){$taskIssues;exit 1}
$taskMutants=@(
 @{name='unmanaged-diagnostics';file='placement';before='!m_bAICFSafeInfantrySpawn || !Replication.IsServer()';after='!Replication.IsServer()';rule='BOUNDARY_AUTHORITY'},
 @{name='missing-expansion-boundary';file='placement';before='m_iAICFExpansionCalls++;';after='';rule='BOUNDARY_DISPATCH'},
 @{name='fictional-materialisation';file='placement';before='GetAgentsCount() <= agentsBefore';after='false';rule='BOUNDARY_NULL_SPAWN'},
 @{name='missing-native-budget-proof';file='runtime';before='aiWorld.CanActivateGroup(group)';after='true';rule='NATIVE_READBACK'},
 @{name='retry-generations-accepted';file='fixture';before='slot.GetSpawnGeneration() == m_aAICFReplacementStressGenerations[record] + 1';after='true';rule='STRESS_EXACT_ROSTER'},
 @{name='timer-left-running';file='fixture';before='if (GetGame()) GetGame().GetCallqueue().Remove(AICF_ReplacementStressTick);';after='';rule='STRESS_CLEANUP'},
 @{name='pressure-with-free-capacity';file='fixture';before='free >= 5';after='free < 5';rule='STRESS_CAPACITY_THRESHOLD'},
 @{name='direct-spawn-bypass';file='fixture';before='m_bAICFReplacementStressDone = true;';after='m_bAICFReplacementStressDone = true; slot.MarkReady();';rule='STRESS_NO_BYPASS'},
 @{name='unscoped-native-limit';file='fixture';before='m_bAICFReplacementStressDone = true;';after='m_bAICFReplacementStressDone = true; world.SetLimitOfActiveAIs(1000);';rule='STRESS_INJECTION_SCOPE'},
 @{name='injection-guard-removed';file='fixture';before='!m_bAICFReplacementStressInjected ||';after='';rule='STRESS_INJECTION_SCOPE'},
 @{name='restore-on-stop-missing';file='fixture';before="if (GetGame()) GetGame().GetCallqueue().Remove(AICF_ReplacementStressTick);`n`t`tAICF_ReplacementStressRestoreCapacity();";after="if (GetGame()) GetGame().GetCallqueue().Remove(AICF_ReplacementStressTick);";rule='STRESS_INJECTION_SCOPE'}
)
foreach($taskMutation in $taskMutants){
 $placement=$taskPlacement;$runtime=$taskRuntime;$fixture=$taskFixture
 switch($taskMutation.file){
  placement {$placement=$placement.Replace($taskMutation.before,$taskMutation.after)}
  runtime {$runtime=$runtime.Replace($taskMutation.before,$taskMutation.after)}
  fixture {$fixture=$fixture.Replace("`r`n","`n").Replace($taskMutation.before,$taskMutation.after)}
 }
 if($placement -eq $taskPlacement -and $runtime -eq $taskRuntime -and $fixture -eq $taskFixture){throw "No mutation: $($taskMutation.name)"}
 if(!(@(Test-Dispatch $placement $runtime $fixture) -match ('\['+$taskMutation.rule+'\]'))){throw "Escaped mutation: $($taskMutation.name)"}
 Write-Output "PASS negative mutation=$($taskMutation.name)"
}
Write-Output "Group spawn dispatch contracts: PASS; negative mutants=$($taskMutants.Count)"
. (Join-Path $PSScriptRoot '../log-audits/Test-GroupReplacementStressLog.ps1') -LogPath 'unused' -NativeExitPath 'unused'
$taskSynthetic="[REPLACEMENT_STRESS_START] warmup_ms=90000`n"
foreach($wave in 1..2){foreach($faction in @('US','USSR')){foreach($slot in 0..2){
 $taskSynthetic+="[REPLACEMENT_STRESS_KILL] wave=$wave faction=$faction numeric_slot=$slot generation=$wave group=0x123 actual=5`n"
 $generation=$wave+1
 $taskSynthetic+="[REPLACEMENT_STRESS_READY] wave=$wave faction=$faction numeric_slot=$slot generation=$generation exact=1 first_generation=1 actual=5`n"
}}}
$taskSynthetic+="[REPLACEMENT_STRESS_DONE] waves=2 failures=0 elapsed_ms=160000`nGame destroyed."
if(@(Get-AICFReplacementStressFailures $taskSynthetic 0).Count){throw 'Positive native stress log rejected'}
foreach($mutation in @(
 @{name='skip-generation';before='generation=2 exact=1';after='generation=8 exact=1';rule='GENERATION_SKIP'},
 @{name='four-members';before='actual=5';after='actual=4';rule='EXACT_FIVE'},
 @{name='partial-roster';before='exact=1';after='exact=0';rule='EXACT_FIVE'},
 @{name='retry-reported';before='first_generation=1';after='first_generation=0';rule='EXACT_FIVE'},
 @{name='wrong-side';before='faction=USSR';after='faction=FIA';rule='MISSING_CASE'},
 @{name='missing-stop';before='Game destroyed.';after='';rule='STOPPED_LOG'},
 @{name='incomplete-waves';before='waves=2 failures=0';after='waves=1 failures=0';rule='EXACT_DONE'},
 @{name='native-fatal';before='Game destroyed.';after="SCRIPT (E): failure`nGame destroyed.";rule='FULL_FATAL'},
 @{name='spawn-timeout';before='Game destroyed.';after="[GROUP_SPAWN_TIMEOUT]`nGame destroyed.";rule='FULL_FATAL'}
)){
 $changed=$taskSynthetic.Replace($mutation.before,$mutation.after)
 if(!(@(Get-AICFReplacementStressFailures $changed 0) -contains $mutation.rule)){throw "Log mutation escaped: $($mutation.name)"}
 Write-Output "PASS log negative=$($mutation.name)"
}
if(!(@(Get-AICFReplacementStressFailures $taskSynthetic 1) -contains 'NATIVE_EXIT')){throw 'Native exit bypassed'}
Write-Output 'Group replacement log controls: PASS; positive=1 negatives=10'
$taskCapacity="[REPLACEMENT_STRESS_START] classification=CONTROLLED_NATIVE_CAPACITY_STRESS`n[REPLACEMENT_STRESS_PRESSURE] active=228 intended=228 limit=228 free=0 desired=5 group_can_activate=0 limit_changed=0 importance_changed=0`n"
foreach($faction in @('US','USSR')){foreach($slot in 0..2){
 $taskCapacity+="[REPLACEMENT_STRESS_KILL] wave=1 faction=$faction numeric_slot=$slot generation=1 group=0x123 actual=5`n"
}}
$taskCapacity+="[REPLACEMENT_STRESS_PRESSURE_HOLD] faction=US numeric_slot=0 generation=2 active=228 intended=228 limit=228 free=0 request_age_ms=6000 actual=0 global_can_activate=0 phase=SPAWNING expansion_calls=0`n"
$taskCapacity+="[REPLACEMENT_STRESS_CAPACITY_RELEASE] elapsed_since_kill_ms=65001 active_before=228 intended_before=228 limit=228 mechanism=KILL_OTHER_OWN_MANAGED_AI limit_changed=0 importance_changed=0`n"
foreach($faction in @('US','USSR')){foreach($slot in 0..2){
 $taskCapacity+="[REPLACEMENT_STRESS_READY] wave=1 faction=$faction numeric_slot=$slot generation=2 exact=1 first_generation=1 actual=5`n"
}}
$taskCapacity+="[REPLACEMENT_STRESS_DONE] waves=1 failures=0 elapsed_ms=700000`nGame destroyed."
if(@(Get-AICFReplacementStressFailures $taskCapacity 0 Capacity).Count){throw 'Positive native capacity log rejected'}
foreach($mutation in @(
 @{name='wrong-classification';before='CONTROLLED_NATIVE_CAPACITY_STRESS';after='CONTROLLED_REPLACEMENT_STRESS';rule='CAPACITY_CLASSIFICATION'},
 @{name='pressure-not-full';before='free=0 desired=5';after='free=10 desired=5';rule='CAPACITY_ACTUAL_PRESSURE'},
 @{name='pressure-math-fabricated';before='limit=228 free=0 desired=5';after='limit=230 free=0 desired=5';rule='CAPACITY_ACTUAL_PRESSURE'},
 @{name='no-observed-hold';before='request_age_ms=6000';after='request_age_ms=100';rule='CAPACITY_NATIVE_HOLD'},
 @{name='native-admission-not-denied';before='actual=0 global_can_activate=0';after='actual=0 global_can_activate=1';rule='CAPACITY_NATIVE_HOLD'},
 @{name='release-before-old-deadline';before='elapsed_since_kill_ms=65001';after='elapsed_since_kill_ms=50000';rule='CAPACITY_REAL_RELEASE'},
 @{name='changed-hard-limit';before='mechanism=KILL_OTHER_OWN_MANAGED_AI limit_changed=0';after='mechanism=KILL_OTHER_OWN_MANAGED_AI limit_changed=1';rule='CAPACITY_REAL_RELEASE'}
)){
 $changed=$taskCapacity.Replace($mutation.before,$mutation.after)
 if(!(@(Get-AICFReplacementStressFailures $changed 0 Capacity) -contains $mutation.rule)){throw "Capacity mutation escaped: $($mutation.name)"}
 Write-Output "PASS capacity negative=$($mutation.name)"
}
Write-Output 'Native capacity log controls: PASS; positive=1 negatives=7'
$taskInjected=$taskCapacity.Replace('CONTROLLED_NATIVE_CAPACITY_STRESS','CONTROLLED_NATIVE_CAPACITY_INJECTION').Replace('group_can_activate=0 limit_changed=0 importance_changed=0','group_can_activate=-1 limit_changed=1 importance_changed=0 original_limit=228 classification=CONTROLLED_NATIVE_CAPACITY_INJECTION').Replace('mechanism=KILL_OTHER_OWN_MANAGED_AI limit_changed=0','mechanism=RESTORE_ORIGINAL_NATIVE_LIMIT limit_changed=1').Replace('[REPLACEMENT_STRESS_CAPACITY_RELEASE]',"[REPLACEMENT_STRESS_NATIVE_LIMIT_RESTORED] original=228 actual=228`n[REPLACEMENT_STRESS_CAPACITY_RELEASE]")
if(@(Get-AICFReplacementStressFailures $taskInjected 0 Injected).Count){throw 'Positive injected native capacity log rejected'}
if(!(@(Get-AICFReplacementStressFailures ($taskInjected.Replace('original=228 actual=228','original=228 actual=227')) 0 Injected) -contains 'INJECTED_LIMIT_RESTORED')){throw 'Wrong restored limit accepted'}
if(!(@(Get-AICFReplacementStressFailures $taskInjected 0 Capacity) -contains 'CAPACITY_CLASSIFICATION')){throw 'Injected pressure accepted as natural'}
Write-Output 'Injected capacity log controls: PASS; positive=1 negatives=2'
$taskWaiting=$taskInjected.Replace('generation=2 active=228 intended=228 limit=228 free=0 request_age_ms=6000 actual=0 global_can_activate=0 phase=SPAWNING expansion_calls=0','generation=1 active=227 intended=227 limit=228 free=1 request_age_ms=0 actual=0 global_can_activate=1 phase=WAITING_NO_NEW_GENERATION no_new_generation=1 elapsed_since_kill_ms=35001')
$taskWaitEvent='[GROUP_SPAWN_NATIVE_CAPACITY_WAIT] faction=US slot=0 stable_slot=S0 generation=1 active=227 intended=227 hard_limit=228 pending=0 requested=5 projected=232 global_can_activate=1 generation_unchanged=1'
$taskReleaseEvent='[GROUP_SPAWN_NATIVE_CAPACITY_RELEASED] faction=US slot=0 stable_slot=S0 generation=1 active=180 intended=180 hard_limit=228 pending=0 requested=5 projected=185 global_can_activate=1 generation_unchanged=1'
$taskWaiting=$taskWaiting.Replace('[REPLACEMENT_STRESS_PRESSURE_HOLD]',"$taskWaitEvent`n[REPLACEMENT_STRESS_PRESSURE_HOLD]").Replace('[REPLACEMENT_STRESS_READY] wave=1 faction=US numeric_slot=0',"$taskReleaseEvent`n[REPLACEMENT_STRESS_READY] wave=1 faction=US numeric_slot=0")
$taskWaitingFailures=@(Get-AICFReplacementStressFailures $taskWaiting 0 Injected)
if($taskWaitingFailures.Count){throw "Positive batch admission WAIT rejected: $taskWaitingFailures"}
foreach($mutation in @(
 @{name='waiting-no-production-receipt';before=$taskWaitEvent;after='';rule='CAPACITY_PRODUCTION_ADMISSION_WAIT'},
 @{name='fabricated-projected-budget';before='requested=5 projected=232';after='requested=5 projected=227';rule='CAPACITY_PRODUCTION_ADMISSION_WAIT'},
 @{name='original-generation-consumed';before='generation=1 active=227';after='generation=2 active=227';rule='CAPACITY_ORIGINAL_GENERATION'},
 @{name='short-wait';before='elapsed_since_kill_ms=35001';after='elapsed_since_kill_ms=3000';rule='CAPACITY_NATIVE_HOLD'},
 @{name='early-ticket-debit';before=$taskWaitEvent;after=($taskWaitEvent+"`n[DEPLOYMENT_RESERVED] faction=US slot=0");rule='CAPACITY_NO_EARLY_SPAWN_OR_DEBIT'},
 @{name='missing-release';before=$taskReleaseEvent;after='';rule='CAPACITY_PRODUCTION_ADMISSION_RELEASE'},
 @{name='release-still-over-budget';before='requested=5 projected=185';after='requested=5 projected=235';rule='CAPACITY_PRODUCTION_ADMISSION_RELEASE'}
)){
 if(!(@(Get-AICFReplacementStressFailures ($taskWaiting.Replace($mutation.before,$mutation.after)) 0 Injected) -contains $mutation.rule)){throw "Batch WAIT mutation escaped: $($mutation.name)"}
 Write-Output "PASS batch WAIT negative=$($mutation.name)"
}
Write-Output 'Batch admission WAIT controls: PASS; positive=1 negatives=7'
$taskWork=Join-Path ([IO.Path]::GetTempPath()) ('aicf-spawn-siblings-'+[guid]::NewGuid().ToString('N'))
[void][IO.Directory]::CreateDirectory($taskWork)
try{
 [IO.File]::WriteAllText((Join-Path $taskWork 'console.log'),$taskSynthetic)
 [IO.File]::WriteAllText((Join-Path $taskWork 'native-exit.txt'),'0')
 foreach($case in @(
  @{name='siblings-clean';text='';expected=0;receipt='gate=functional verdict=PASS'},
  @{name='sibling-assertion';text='Assertion failed enf_entity.cpp280';expected=1;receipt='FULL_SIBLING_LOG_FATAL'},
  @{name='sibling-resource';text='RESOURCES (E): Missing mesh';expected=1;receipt='gate=strict_native_resources verdict=FAIL'}
 )){
  [IO.File]::WriteAllText((Join-Path $taskWork 'crash.log'),$case.text)
  $taskResult=@(& pwsh -NoProfile -File (Join-Path $PSScriptRoot '../log-audits/Test-GroupReplacementStressLog.ps1') -LogPath (Join-Path $taskWork 'console.log') -NativeExitPath (Join-Path $taskWork 'native-exit.txt'))
  if($LASTEXITCODE -ne $case.expected -or ($taskResult -join "`n") -notmatch [regex]::Escape($case.receipt)){throw "Sibling control escaped: $($case.name) $taskResult"}
  Write-Output "PASS full sibling=$($case.name)"
 }
}finally{Remove-Item -LiteralPath $taskWork -Recurse -Force}
