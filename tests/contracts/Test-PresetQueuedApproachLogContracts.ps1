param([string]$RepositoryRoot=(Split-Path -Parent (Split-Path -Parent $PSScriptRoot)))
$ErrorActionPreference='Stop'
. (Join-Path $RepositoryRoot 'tests/log-audits/Test-PresetQueuedApproachLog.ps1')
$argv=@('-gproj','project','-profile','profile','-addonsDir','addons','-addons','ids','-aicfVehiclePresetProbe','1','-aicfVehiclePresetCrew','unarmed','-aicfPresetQueuedApproachProbe','1')
$launcher='AICF_RUNTIME_MANIFEST_JSON='+(@{arguments=$argv}|ConvertTo-Json -Compress)
$start='[PREQUEUE_PROBE_START] operation_id=A member=42 start=<0,0,0> vehicle=6 distance_m=22 deadline_ms=1000 pose_writes=0 health_writes=0'
$walk='[PREQUEUE_PROBE_WALK] operation_id=A member=42 displacement_m=12 native_polls=3 entry_queued=0 deadline_unchanged=1 original_member=1 health_unchanged=1 pos=<12,0,0> pose_writes=0'
$entry='[PREQUEUE_PROBE_ENTRY] operation_id=A member=42 accepted=1 deadline_unchanged=1 native_walk_before_entry=1'
$lines=@(('CLI Params: '+($argv -join ' ')),'[ROSTER_READY]',$start,$walk,$entry,'[VEHICLE_PRESET_OCCUPANT] operation=A revision=1 position=1 seat=0:3:2 member=42 occupant=42')
0..3|ForEach-Object{$lines+='[VEHICLE_PRESET_CHECK] rule=EXACT_OCCUPANT_'+$_+' passed=1'}
$lines+='[VEHICLE_PRESET_DONE] checks=82 failures=0';$lines+='Game destroyed.'
$baseline=$lines -join "`n"
$checks=0
function Check([bool]$ok,[string]$rule){if(!$ok){throw "Queued approach log: $rule"};$script:checks++}
function Verdict([string]$log,[string]$all=$log,[string]$manifest=$launcher,[string]$native='0'){Get-AICFPresetQueuedApproachVerdict $log $all $manifest $native $true}
$positive=Verdict $baseline
Check ($positive.verdict -eq 'PASS' -and $positive.functional_pass -and $positive.fatal_pass) 'POSITIVE'
foreach($marker in @($start,$walk,$entry))
{
	Check (!(Verdict ($baseline.Replace($marker,''))).functional_pass) 'MISSING_CAUSAL_RECEIPT'
	Check (!(Verdict ($baseline+"`n"+$marker)).functional_pass) 'DUPLICATE_CAUSAL_RECEIPT'
}
$mutants=@(
 @($walk,$walk.Replace('member=42','member=43')),
 @($walk,$walk.Replace('operation_id=A','operation_id=B')),
 @($walk,$walk.Replace('displacement_m=12','displacement_m=9.99')),
 @($walk,$walk.Replace('displacement_m=12','displacement_m=NaN')),
 @($walk,$walk.Replace('displacement_m=12','displacement_m=Infinity')),
 @($walk,$walk.Replace('displacement_m=12','displacement_m=invalid')),
 @($walk,$walk.Replace('native_polls=3','native_polls=2')),
 @($walk,$walk.Replace('entry_queued=0','entry_queued=1')),
 @($walk,$walk.Replace('deadline_unchanged=1','deadline_unchanged=0')),
 @($walk,$walk.Replace('original_member=1','original_member=0')),
 @($walk,$walk.Replace('health_unchanged=1','health_unchanged=0')),
 @($walk,$walk.Replace('pose_writes=0','pose_writes=1')),
 @($entry,$entry.Replace('accepted=1','accepted=0')),
 @($entry,$entry.Replace('deadline_unchanged=1','deadline_unchanged=0')),
 @($entry,$entry.Replace('native_walk_before_entry=1','native_walk_before_entry=0')),
 @('member=42 occupant=42','member=42 occupant=43'),
 @('[VEHICLE_PRESET_CHECK] rule=EXACT_OCCUPANT_3 passed=1',''),
 @('EXACT_OCCUPANT_3 passed=1','EXACT_OCCUPANT_3 passed=0'),
 @('checks=82 failures=0','checks=82 failures=1'),
 @('Game destroyed.',''),@('[ROSTER_READY]','')
)
foreach($pair in $mutants){Check (!(Verdict ($baseline.Replace($pair[0],$pair[1]))).functional_pass) ('REJECT_'+$pair[1])}
Check (!(Verdict ($baseline.Replace($walk+"`n"+$entry,$entry+"`n"+$walk))).functional_pass) 'CAUSAL_ORDER'
Check (!(Verdict $baseline $baseline $launcher '-1').functional_pass) 'NATIVE_ABORT'
Check (!(Verdict $baseline $baseline ($launcher+"`n"+$launcher)).functional_pass) 'MANIFEST_DUPLICATE'
Check (!(Verdict ($baseline.Replace('CLI Params: ','CLI Params: extra '))).functional_pass) 'NATIVE_CLI_MISMATCH'
$fatal=Verdict $baseline ($baseline+"`nSCRIPT (E): sibling failure")
Check ($fatal.functional_pass -and !$fatal.fatal_pass -and $fatal.verdict -eq 'FAIL') 'ALL_SIBLING_FATAL_GATE'
$resource=Verdict $baseline ($baseline+"`nRESOURCES (E): native baseline")
Check ($resource.functional_pass -and $resource.fatal_pass -and !$resource.strict_resources_pass -and $resource.verdict -eq 'FAIL') 'NO_RESOURCE_WHITELIST'
$fixture=[IO.File]::ReadAllText((Join-Path $RepositoryRoot 'tests/fixtures/AICF_PresetQueuedApproachProbe.c'))
Check ($fixture.Contains('m_QueueProbeActorId') -and $fixture.Contains('m_sQueueProbeActorRpl') -and $fixture.Contains('m_fQueueProbeHealth') -and $fixture.Contains('nowMs - lastMs >= 500')) 'MEASURED_FIXTURE_FENCES'
Check ($fixture -notmatch '\.(Teleport|SetOrigin|SetWorldTransform|SetHealth|SetHealthScaled)\(') 'FIXTURE_NO_PHYSICAL_WRITES'
Write-Output "PASS Preset queued approach logs: $checks positive/negative controls"
