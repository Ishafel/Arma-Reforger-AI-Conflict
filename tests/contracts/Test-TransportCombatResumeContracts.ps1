param([string]$RepositoryRoot=(Split-Path -Parent (Split-Path -Parent $PSScriptRoot)))
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot '../lib/Stage3StaticAudit.Common.ps1')
$fixture=[IO.File]::ReadAllText((Join-Path $RepositoryRoot 'tests/fixtures/AICF_TransportCombatResumeProbe.c'))
$runner=[IO.File]::ReadAllText((Join-Path $RepositoryRoot 'tools/Invoke-AICFTransportCombatResume.ps1'))
function Body([string]$source,[string]$name){Get-AICFMethodBody ([pscustomobject]@{Source=$source;Code=(ConvertTo-AICFCodeText $source)}) $name}
function Rules([string]$source){
 $evaluate=Body $source 'CustomEvaluate';$identityBody=Body $source 'RejectionReason';$arm=Body $source 'AICF_TCRArm';$update=Body $source 'Update';$finish=Body $source 'AICF_TCRFinish';$stop=Body $source 'Stop'
 if(!$evaluate -or !$identityBody -or !$arm -or !$update -or !$finish -or !$stop){'MISSING_METHOD';return}
 if($source -notmatch 'modded class SCR_AIVehicleCombatActivity' -or $evaluate -notmatch 'AICF_TCRGate.Scores\(this, m_Utility\)' -or $evaluate -notmatch 'return super.CustomEvaluate\(\)'){'EXACT_NATIVE_EVALUATION_ONLY'}
 if((ConvertTo-AICFCodeText $source) -match 'SetCurrentAction|SetExecutedAction|\.AddAction\(|\.Fail\(|\.Complete\(|SetActionState|SetActionIsSuspended|new SCR_AIVehicleCombatActivity|ObservePhysicalMotion|ObserveRouteProgress|SetOrigin|Teleport|CommitTransition|SetPresetDestinationWaiting|m_i(?:LastPhysicalMotionAt|LastRouteProgressAt|AbsoluteDeadline)Ms\s*[+]?='){'NO_ACTION_OR_PROGRESS_FORGERY'}
 foreach($fence in @('m_Trip.GetOperationId() != m_sOperation','m_Trip.GetTripGeneration() != m_iTripGeneration','m_Trip.GetGroupGeneration() != m_iGroupGeneration','m_Trip.GetAbsoluteDeadlineMs() != m_iDeadline','!m_Trip.IsVehiclePresetCurrent()','m_Slot.GetVehiclePresetRevision() != m_iPresetRevision','m_Trip.GetAssignment().GetAssignmentRevision() != m_iAssignment','m_Trip.GetAssignment().GetTargetPosition() != m_vTarget','m_Group.GetID() != m_GroupId','ComponentRpl(m_Group) != m_sGroupRpl','!actions.Contains(m_Activity)','m_Trip.GetLease() != m_Lease','m_Lease.GetLeaseGeneration() != m_iLeaseGeneration','m_Lease.GetVehicleGeneration() != m_iVehicleGeneration','m_Lease.MatchesEntityIdentity(m_Vehicle, m_VehicleId, m_sVehicleRpl)','m_Slot.GetRosterMember(index) != actor','actor.GetID() != m_aActorIds[index]','ComponentRpl(actor) != m_aActorRpls[index]','agent.GetParentGroup() == m_Group')){if(!$identityBody.Contains($fence)){'FULL_IDENTITY_FENCES';break}}
 if($arm -notmatch 'FindActionOfType\(SCR_AIVehicleCombatActivity\)' -or $arm -notmatch 'movement.GetCrewRecoveryToken\(\) \|\| movement.GetNativeGate\(\)' -or $arm -notmatch 'if \(!gate.Matches\(\)\)' -or $update -notmatch 'distance < 650|AICF_TCRTarget' -or $update -notmatch 'vector.DistanceXZ\(m_vTCRStart, m_TCRVehicle.GetOrigin\(\)\) < 25 \|\| m_iTCRMoving < 5'){'ARM_NATIVE_AFTER_REAL_DRIVE'}
 if($update -notmatch 'now - m_iTCRHoldAt < 90000' -or $update -notmatch 'now - m_iTCRStationaryAt < 60000' -or $update -notmatch 'movement.GetLastPhysicalMotionAtMs\(\) != m_iTCRMotionClock' -or $update -notmatch 'movement.GetLastRouteProgressAtMs\(\) != m_iTCRProgressClock' -or $update -notmatch 'AICF_TransportCombatHold.IsCurrentCombat\(trip\)'){'LONG_CURRENT_HOLD_WITH_REAL_CLOCKS'}
 if($update -notmatch 'resumed >= 100 && m_iTCRMoving >= 5 && AICF_TCRSeats\(slot\) && nativeRoute' -or $update -notmatch 'nativeMove.m_RelatedWaypoint == movement.GetRouteWaypoint\(\)' -or $update -notmatch 'm_TCRGroup.GetCurrentWaypoint\(\) == movement.GetRouteWaypoint\(\)' -or $update -notmatch 'farthest > 35' -or $update -notmatch 'trip.GetPhase\(\) != AICF_ETransportTripPhase.COMPLETE'){'PHYSICAL_NATIVE_RESUME_AND_NORMAL_FOOT'}
 if($finish -notmatch 'AICF_TCRGate.Clear\(\)' -or $stop -notmatch 'AICF_TCRGate.Clear\(\)' -or $update -notmatch 'now - m_iTCRStarted > 600000' -or $update -notmatch 'AICF_TCRGate.Clear\(\)'){'BOUNDED_GATE_CLEANUP'}
 if($update.IndexOf('if (!m_iTCRStarted)') -gt $update.IndexOf('if (!m_bRosterReady)') -or $update -notmatch 'm_aTCRActors.Count\(\) != 5 && now - m_iTCRStarted >= 180000' -or $update.IndexOf('SETUP_TIMEOUT') -gt $update.IndexOf('if (!m_bRosterReady)')){'BOUNDED_SETUP_BEFORE_READINESS'}
 if($update -notmatch 'm_bStopped \|\| !m_Campaign' -or $update -notmatch 'if \(!m_Campaign.IsRunning\(\)\)' -or $update -notmatch 'm_bTCRWasRunning \|\| m_bCampaignWasRunning \|\| m_Campaign.GetState\(\) != SCR_EGameModeState.PREGAME' -or $update.IndexOf('if (!m_Campaign.IsRunning())') -gt $update.IndexOf('AICF_TCRSetup()') -or $stop -notmatch 'AICF_TCRFail\("MATCH_STOPPED"\)'){'MATCH_LIFECYCLE_EARLY_ABORT'}
 if($update -notmatch 'movement.GetNativeCombatActivity\(\) != m_TCRWitness.m_Activity' -or $source -notmatch 'm_iTCRObservedAt = now' -or $source -notmatch 'eventName, m_iTCRObservedAt, m_iTCRStep'){'PRODUCTION_SELECTION_AND_DECISION_TIMESTAMP'}
 $component=Body $source 'ComponentRpl';$rejectLog=Body $source 'LogReject';$match=Body $source 'Matches'
 if($component -notmatch 'RplComponent.Cast\(entity.FindComponent\(RplComponent\)\)' -or $component -notmatch 'return rpl.Id\(\).ToString\(\)' -or $arm -notmatch 'gate.m_sVehicleRpl = AICF_TCRGate.ComponentRpl\(m_TCRVehicle\)' -or $identityBody -notmatch 'ComponentRpl\(m_Vehicle\) != m_sVehicleRpl' -or (ConvertTo-AICFCodeText ($source.Replace($rejectLog,''))) -match 'Replication.FindItemId'){'CANONICAL_COMPONENT_RPL_NOT_ENTITY_ITEM'}
 if($match -notmatch 'string reason = RejectionReason\(\)' -or $match -notmatch 'return reason.IsEmpty\(\)' -or $arm -notmatch 'gate.LogReject\("ARM", System.GetTickCount\(\)\)' -or $update -notmatch 'm_TCRWitness.LogReject\("ACTIVE", now\)' -or $rejectLog -notmatch 'expected_component_rpl=.*current_component_rpl=.*vehicle_item_rpl=.*lease_component_rpl=' -or $identityBody -notmatch 'LEASE_ENTITY_COMPONENT_RPL'){'NAMED_IDENTITY_REJECTION_DIAGNOSTICS'}
 $health=Body $source 'AICF_TCRHealth';$category=Body $source 'RejectionCategory'
 if($source -notmatch 'damage_protection_scope=ROOT_ONLY' -or $health -notmatch 'm_iTCRHealthSample = now \+ 5000;' -or $health -notmatch 'now < m_iTCRHealthSample' -or $health -notmatch 'AICF_VehicleHealthTrace.Capture\(vehicle\)' -or $rejectLog -notmatch 'AICF_VehicleHealthTrace.Capture\(m_Vehicle\)' -or $rejectLog.IndexOf('AICF_VehicleHealthTrace.Capture') -gt $rejectLog.IndexOf('Print(details)') -or $update.IndexOf('AICF_TCRHealth(trip, now)') -gt $update.IndexOf('!m_TCRWitness.Matches()') -or $category -notmatch 'reason == "VEHICLE_DAMAGE"\) return "HEALTH"' -or $update -notmatch 'AICF_TCRFail\("WITNESS_" \+ AICF_TCRGate.RejectionCategory'){'LEAF_HEALTH_BEFORE_VETO_AND_ROOT_SCOPE'}
 if($finish -notmatch 'AICF_TCRLog\("CLEANUP_BEGIN"' -or $finish.IndexOf('AICF_TCRLog("CLEANUP_BEGIN"') -gt $finish.IndexOf('StopWithFactionContexts')){'EXPLICIT_CLEANUP_BOUNDARY'}
}
$issues=@(Rules $fixture)
if($issues.Count){throw "Fixture contracts: $issues"}
$controls=1
foreach($mutation in @(
 @('return super.CustomEvaluate();','return GetPriority();','EXACT_NATIVE_EVALUATION_ONLY'),
 @('m_Trip.GetAbsoluteDeadlineMs() != m_iDeadline','false','FULL_IDENTITY_FENCES'),
 @('!actions.Contains(m_Activity)','false','FULL_IDENTITY_FENCES'),
 @('actor.GetID() != m_aActorIds[index]','false','FULL_IDENTITY_FENCES'),
 @('movement.GetCrewRecoveryToken() || movement.GetNativeGate()','false','ARM_NATIVE_AFTER_REAL_DRIVE'),
 @('now - m_iTCRHoldAt < 90000','now - m_iTCRHoldAt < 1000','LONG_CURRENT_HOLD_WITH_REAL_CLOCKS'),
 @('movement.GetLastPhysicalMotionAtMs() != m_iTCRMotionClock','false','LONG_CURRENT_HOLD_WITH_REAL_CLOCKS'),
 @('resumed >= 100 && m_iTCRMoving >= 5 && AICF_TCRSeats(slot) && nativeRoute','resumed >= 25','PHYSICAL_NATIVE_RESUME_AND_NORMAL_FOOT'),
 @('AICF_TCRGate.Clear();','Print("leaked");','BOUNDED_GATE_CLEANUP')
 ,@('m_aTCRActors.Count() != 5 && now - m_iTCRStarted >= 180000','false','BOUNDED_SETUP_BEFORE_READINESS')
 ,@('m_bTCRWasRunning || m_bCampaignWasRunning || m_Campaign.GetState() != SCR_EGameModeState.PREGAME','false','MATCH_LIFECYCLE_EARLY_ABORT')
 ,@('AICF_TCRFail("MATCH_STOPPED");','Print("ignored stop");','MATCH_LIFECYCLE_EARLY_ABORT')
 ,@('movement.GetNativeCombatActivity() != m_TCRWitness.m_Activity','false','PRODUCTION_SELECTION_AND_DECISION_TIMESTAMP')
 ,@('gate.m_sVehicleRpl = AICF_TCRGate.ComponentRpl(m_TCRVehicle);','gate.m_sVehicleRpl = Replication.FindItemId(m_TCRVehicle).ToString();','CANONICAL_COMPONENT_RPL_NOT_ENTITY_ITEM')
 ,@('return rpl.Id().ToString();','return Replication.FindItemId(entity).ToString();','CANONICAL_COMPONENT_RPL_NOT_ENTITY_ITEM')
 ,@('m_Lease.MatchesEntityIdentity(m_Vehicle, m_VehicleId, m_sVehicleRpl)','true','FULL_IDENTITY_FENCES')
 ,@('gate.LogReject("ARM", System.GetTickCount());','Print("rejected");','NAMED_IDENTITY_REJECTION_DIAGNOSTICS')
 ,@('damage_protection_scope=ROOT_ONLY','damage_protection_scope=FULL','LEAF_HEALTH_BEFORE_VETO_AND_ROOT_SCOPE')
 ,@('m_iTCRHealthSample = now + 5000','m_iTCRHealthSample = now + 50000','LEAF_HEALTH_BEFORE_VETO_AND_ROOT_SCOPE')
 ,@('AICF_VehicleHealthTrace.Capture(m_Vehicle)','"health_status=READ_ONLY"','LEAF_HEALTH_BEFORE_VETO_AND_ROOT_SCOPE')
 ,@('if (reason == "VEHICLE_DAMAGE") return "HEALTH";','if (reason == "VEHICLE_DAMAGE") return "IDENTITY";','LEAF_HEALTH_BEFORE_VETO_AND_ROOT_SCOPE')
 ,@('AICF_TCRLog("CLEANUP_BEGIN"','AICF_TCRLog("HIDDEN_CLEANUP"','EXPLICIT_CLEANUP_BOUNDARY')
)){
 if(!$fixture.Contains($mutation[0])){throw "Mutation target absent: $($mutation[2])"}
 if($mutation[2] -notin @(Rules ($fixture.Replace($mutation[0],$mutation[1])))){throw "Mutation survived: $($mutation[2]) target=$($mutation[0])"}
 $controls++
}
$evaluate=Body $fixture 'CustomEvaluate'
if('NO_ACTION_OR_PROGRESS_FORGERY' -notin @(Rules ($fixture.Replace($evaluate,$evaluate.Insert(1,'m_Utility.SetCurrentAction(this);'))))){throw 'Action forgery survived'}
$controls++
foreach($needle in @('Start-AICFRuntime.ps1','-Role Server','-aicfTransportCombatResume','Script validation successful.','source-hashes-before.json','source-hashes-after.json','Get-TaskSourceHashes','Compare-Object','FAULT_INJECTED','normal_war=$false')){if(!$runner.Contains($needle)){throw "Runner missing: $needle"}}
if(Test-Path -LiteralPath (Join-Path $RepositoryRoot 'AIConflictCore/Scripts/Game/AIConflict/AICF_TransportCombatResumeProbe.c')){throw 'Test fixture leaked into production'}
$work=Join-Path ([IO.Path]::GetTempPath()) ('aicf-tcr-contract-'+[guid]::NewGuid().ToString('N'))
[void][IO.Directory]::CreateDirectory($work)
$argv=@('-gproj','source','-profile','profile','-addonsDir','addons','-addons','CORE,MAP','-aicfTransportCombatResume','1')
[IO.File]::WriteAllText("$work/launcher.txt",'AICF_RUNTIME_MANIFEST_JSON='+(@{arguments=$argv}|ConvertTo-Json -Compress))
$good='CLI Params: '+($argv -join ' ')+"`n[ROSTER_READY]`n"
$good+="[TCR_ENV] t_ms=1000 step=0 mode=FAULT_INJECTED truce=1 hold_fire=1 damage_protection=1 damage_protection_scope=ROOT_ONLY normal_war=0 teleport=0`n"
for($index=0;$index -lt 5;$index++){$good+="[TCR_MEMBER] t_ms=1000 step=0 position=$index actor=M$index rpl=R$index`n"}
foreach($rule in @('TRUCE','GROUP_CREATED','RECIPE','INITIAL_ORDER','EXACT_BOARDING','ARRIVAL','NORMAL_COMPLETE','FOOT_ARRIVAL')){$good+="[TCR_CHECK] t_ms=1000 step=0 rule=$rule passed=1`n"}
$good+="[TCR_GOAL] t_ms=2000 step=1 target=<700,0,0> initial_distance_m=700 intent_revision=2 dismount_radius_m=150`n"
$good+="[TCR_ARM] t_ms=5000 step=2 operation_id=O1 group=G1 vehicle=V1 vehicle_rpl=RV1 lease_generation=1 vehicle_generation=1 deadline_ms=1200000 assignment_revision=2 intent_revision=2 target=<700,0,0> warm_displacement_m=30 warm_moving_polls=5`n"
$good+="[TRANSPORT_NATIVE_COMBAT_STARTED] operation_id=O1 reason=EXACT_CURRENT_ACTIVITY`n[TCR_SELECTED] t_ms=6000 step=3 operation_id=O1 motion_clock=5000 progress_clock=5000 deadline_ms=1200000`n"
for($index=0;$index -lt 18;$index++){
 $hold=$index*5000
 $good+="[TCR_HEALTH] t_ms=$(6000+$hold) step=4 operation_id=O1 health_status=READ_ONLY health_vehicle=V1 health_component_rpl=RV1 health_can_move=1 health_on_fire=0 health_movement_damage=0 health_root_damage_enabled=0 health_cause_proven=0`n"
 $good+="[TCR_HOLD] t_ms=$(6000+$hold) step=4 operation_id=O1 hold_ms=$hold stationary_ms=$hold motion_clock=5000 progress_clock=5000 deadline_ms=1200000 exact_current=1`n"
}
$good+="[TCR_RELEASE] t_ms=96000 step=4 operation_id=O1 hold_ms=90000 stationary_ms=85000 motion_clock=5000 progress_clock=5000 deadline_ms=1200000`n"
$good+="[TRANSPORT_NATIVE_COMBAT_ENDED] operation_id=O1 reason=ACTIVITY_OR_CONTEXT_CHANGED`n[TCR_DESELECTED] t_ms=97000 step=5 operation_id=O1 elapsed_ms=1000 policy_current=0`n"
$good+="[TCR_RESUMED] t_ms=120000 step=6 operation_id=O1 vehicle=V1 displacement_m=110 moving_polls=8 exact_seats=5 deadline_ms=1200000 intent_revision=2 native_route=1`n"
$good+="[TCR_ARRIVAL] t_ms=150000 step=6 operation_id=O1 vehicle=V1 distance_m=145 target=<700,0,0> intent_revision=2`n"
$good+="[DISEMBARK_COMPLETE] operation_id=O1 logical_occupants=0 transitions=0 inside_bounds=0`n[VEHICLE_STATE_CHANGED] operation_id=O1 previous_state=HANDOFF state=COMPLETE`n"
$good+="[TCR_FOOT] t_ms=180000 step=8 operation_id=O1 farthest_m=20 alive=5 target=<700,0,0> intent_revision=2`n[TCR_CLEANUP_BEGIN] t_ms=180500 step=8 operation_id=O1 failures=0 fixture_cleanup=1`n[TCR_DONE] t_ms=181000 step=8 checks=8 failures=0 gate_cleared=1 mode=FAULT_INJECTED`nGame destroyed."
function Run([string]$name,[string]$body,[int]$expected,[string]$exitCode='0',[string]$sibling=''){
 [IO.File]::WriteAllText("$work/console.log",$body);[IO.File]::WriteAllText("$work/script.log",$sibling);[IO.File]::WriteAllText("$work/exit.txt",$exitCode)
 $result=& (Join-Path $RepositoryRoot 'tests/log-audits/Test-TransportCombatResumeLog.ps1') -LogPath "$work/console.log" -LauncherPath "$work/launcher.txt" -NativeExitPath "$work/exit.txt"
 if($LASTEXITCODE -ne $expected){throw "$name unexpected verdict: $result"}
 $script:lastAudit=@($result)
 $script:controls++
}
try{
 Run valid $good 0
 foreach($mutation in @(
  @('undeclared-fault','normal_war=0','normal_war=1'),@('short-hold','hold_ms=90000','hold_ms=59000'),@('moving-hold','stationary_ms=85000','stationary_ms=5000'),
  @('clock-forgery','motion_clock=5000 progress_clock=5000 deadline_ms=1200000 exact_current=1','motion_clock=9000 progress_clock=5000 deadline_ms=1200000 exact_current=1'),
  @('deadline-rearm','deadline_ms=1200000 intent_revision=2 native_route=1','deadline_ms=1300000 intent_revision=2 native_route=1'),
  @('wrong-vehicle','operation_id=O1 vehicle=V1 displacement_m=110','operation_id=O1 vehicle=V2 displacement_m=110'),@('short-resume','displacement_m=110','displacement_m=25'),
  @('no-velocity','moving_polls=8','moving_polls=0'),@('no-native-route','native_route=1','native_route=0'),@('wrong-operation','step=6 operation_id=O1 vehicle=V1 displacement','step=6 operation_id=O2 vehicle=V1 displacement'),
  @('far-foot','farthest_m=20','farthest_m=200'),@('dead-member','alive=5','alive=4'),@('leaked-gate','gate_cleared=1','gate_cleared=0'),@('not-stopped','Game destroyed.',''),
  @('no-release','[TCR_RELEASE]','[MISSING_RELEASE]'),@('not-natural-end','reason=ACTIVITY_OR_CONTEXT_CHANGED','reason=OWNED_ACTIONS_ABORTED'),@('no-physical-exit','inside_bounds=0','inside_bounds=2')
 )){Run $mutation[0] ($good.Replace($mutation[1],$mutation[2])) 1}
 Run native-failure $good 1 '1'
 Run sibling-fatal $good 1 '0' 'SCRIPT (E): Virtual Machine Exception'
 Run fallback ($good+"`n[VEHICLE_STATE_CHANGED] operation_id=O1 previous_state=TRANSIT state=FALLBACK") 1
 Run sparse-hold ([regex]::Replace($good,'(?m)^\[TCR_HOLD\][^\r\n]*\r?\n','')) 1
 $compressed=[regex]::Replace($good,'t_ms=\d+', 't_ms=1000')
 Run compressed-ticks $compressed 1
 Run non-monotonic-ticks ($good.Replace('[TCR_RELEASE] t_ms=96000','[TCR_RELEASE] t_ms=2000')) 1
 Run forged-hold-ticks ($good.Replace('[TCR_HOLD] t_ms=11000','[TCR_HOLD] t_ms=12000')) 1
 Run late-deselection-ticks ($good.Replace('[TCR_DESELECTED] t_ms=97000','[TCR_DESELECTED] t_ms=110000')) 1
 Run late-resume-ticks ($good.Replace('[TCR_RESUMED] t_ms=120000','[TCR_RESUMED] t_ms=300000')) 1
 Run unknown-native-getout-class $good 1 '0' "WORLD (E): Unknown class 'SCR_AIGoalReaction_GetOutVehicle'"
 Run unknown-native-getin-class $good 1 '0' "WORLD (E): Unknown class 'SCR_AIGoalReaction_GetInVehicle'"
 $startLine="[TRANSPORT_NATIVE_COMBAT_STARTED] operation_id=O1 reason=EXACT_CURRENT_ACTIVITY`n"
 Run start-before-arm ($good.Replace($startLine,'').Replace('[TCR_ARM]',$startLine+'[TCR_ARM]')) 1
 $endLine="[TRANSPORT_NATIVE_COMBAT_ENDED] operation_id=O1 reason=ACTIVITY_OR_CONTEXT_CHANGED`n"
 Run end-before-release ($good.Replace($endLine,'').Replace('[TCR_RELEASE]',$endLine+'[TCR_RELEASE]')) 1
 $dismountLine="[DISEMBARK_COMPLETE] operation_id=O1 logical_occupants=0 transitions=0 inside_bounds=0`n"
 Run dismount-before-arrival ($good.Replace($dismountLine,'').Replace('[TCR_ARRIVAL]',$dismountLine+'[TCR_ARRIVAL]')) 1
 $completeLine="[VEHICLE_STATE_CHANGED] operation_id=O1 previous_state=HANDOFF state=COMPLETE`n"
 Run complete-before-dismount ($good.Replace($completeLine,'').Replace('[DISEMBARK_COMPLETE]',$completeLine+'[DISEMBARK_COMPLETE]')) 1
 Run identity-rejection ($good+"`n[TCR_REJECT] t_ms=181000 step=-1 reason=LEASE_ENTITY_COMPONENT_RPL") 1
 if('IDENTITY_REJECTED' -notin $script:lastAudit){throw 'Identity rejection lost its category'}
 foreach($mutation in @(
  @('missing-root-scope','damage_protection_scope=ROOT_ONLY','damage_protection_scope=FULL'),
  @('no-leaf-health','[TCR_HEALTH]','[MISSING_HEALTH]'),
  @('wrong-health-vehicle','health_vehicle=V1','health_vehicle=V2'),
  @('wrong-health-rpl','health_component_rpl=RV1','health_component_rpl=RV2'),
  @('leaf-native-veto','health_can_move=1','health_can_move=0'),
  @('leaf-movement-veto','health_movement_damage=0','health_movement_damage=1'),
  @('leaf-root-protection','health_root_damage_enabled=0','health_root_damage_enabled=1'),
  @('leaf-cause-overclaim','health_cause_proven=0','health_cause_proven=1'),
  @('missing-cleanup','[TCR_CLEANUP_BEGIN]','[MISSING_CLEANUP]'),
  @('undeclared-cleanup','fixture_cleanup=1','fixture_cleanup=0')
 )){Run $mutation[0] ($good.Replace($mutation[1],$mutation[2])) 1}
 $healthFailure=$good.Replace('rule=ARRIVAL passed=1','rule=ARRIVAL passed=0').Replace('failures=0','failures=1')
 $healthFailure=$healthFailure.Replace('[TCR_CLEANUP_BEGIN]',"[TCR_REJECT] t_ms=180400 step=-1 reason=VEHICLE_DAMAGE category=HEALTH`n[TCR_CLEANUP_BEGIN]")
 $secondary="[VEHICLE_STATE_CHANGED] operation_id=O1 previous_state=TRANSIT state=FAILED_CLOSED`n[ACCEPTANCE_FAILURE_LATCHED] reason=COORDINATOR_STOP`n"
 Run health-with-secondary-cleanup ($healthFailure.Replace('[TCR_DONE]',$secondary+'[TCR_DONE]')) 1
 if('HEALTH_REJECTED' -notin $script:lastAudit -or 'IDENTITY_REJECTED' -in $script:lastAudit -or 'NO_FALLBACK_OR_MOBILITY_RECOVERY' -in $script:lastAudit){throw "Secondary cleanup/category attribution incorrect: $script:lastAudit"}
 Run health-with-prior-fallback ($healthFailure.Replace('[TCR_CLEANUP_BEGIN]',$secondary+'[TCR_CLEANUP_BEGIN]')) 1
 if('NO_FALLBACK_OR_MOBILITY_RECOVERY' -notin $script:lastAudit){throw 'Pre-cleanup fallback was hidden'}
 $forgedCleanup=$good.Replace('failures=0','failures=1').Replace('[TCR_DONE]',$secondary+'[TCR_DONE]')
 Run unproven-primary-cannot-excuse-cleanup $forgedCleanup 1
 if('NO_FALLBACK_OR_MOBILITY_RECOVERY' -notin $script:lastAudit){throw 'Cleanup without a proven earlier failure excused fallback'}
 Run legacy-health-rejection ($good+"`n[TCR_REJECT] t_ms=181000 step=-1 reason=VEHICLE_DAMAGE") 1
 if('HEALTH_REJECTED' -notin $script:lastAudit -or 'IDENTITY_REJECTED' -in $script:lastAudit){throw 'Legacy health reason mislabeled identity'}
}
finally{
 $resolved=[IO.Path]::GetFullPath($work)
 if(!$resolved.StartsWith([IO.Path]::GetTempPath(),[StringComparison]::OrdinalIgnoreCase) -or (Split-Path -Leaf $resolved) -notlike 'aicf-tcr-contract-*'){throw 'Unsafe contract cleanup'}
 Remove-Item -LiteralPath $resolved -Recurse -Force
}
"PASS Transport combat resume fixture/runner/log contracts controls=$controls; FAULT_INJECTED only"
