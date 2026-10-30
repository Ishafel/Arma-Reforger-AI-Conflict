param([string]$RepositoryRoot=(Split-Path -Parent (Split-Path -Parent $PSScriptRoot)))
$ErrorActionPreference='Stop'
$fixturePath=Join-Path $RepositoryRoot 'tests/fixtures/AICF_NativeGateCrewExitProbe.c'
$fixture=[IO.File]::ReadAllText($fixturePath)
function Rules($s){
 foreach($guard in @('!managedDeployment','group.GetAgentsCount() != 0','group.GetIntendedActiveCount() != 0','group.SetOrigin(AICF_GateCrewProbe.Pad()',
  'return super.SpawnInitialRoster(factionState, faction);','super.ProcessFaction(factionState, faction);','HasExactFactionRoster(slot.GetGroup(), "USSR", 1',
  'm_VehicleCoordinator.GetTrip(slot)','EXISTING_BEFORE_INTENT','!m_bRosterReady','super.TrySelectSiteForAcquisition(','selection.m_vPosition = AICF_GateCrewProbe.Pad();',
  'GetNormalizedDoorState()) > 0.01','GetControlValue()) > 0.01','rpl && (!rpl.IsMaster() || rpl.IsProxy())','m_Campaign.IsMaster()',
  'a.IsFactionFriendly(b)','gate.m_Seat.GetType() != ECompartmentType.PILOT','gate.m_SmartOwner.GetID() != s_DoorId','gate.m_bReturnCompleted',
  'trip.GetMovementState().GetNativeGate() == gate','gate.Matches(trip)','!gate.IsExpired(System.GetTickCount())','ACTUAL_EMPTY_BEFORE_TICK',
  'trip.GetAbsoluteDeadlineMs() != m_iGCPDeadline','GetMobilityRecoveryAttempts() != m_iGCPAttempts','System.GetTickCount() + 500',
  's_OriginalSeat = gate.m_Seat;','s_OriginalSeat.GetVehicle() != car','if (s_Started)',
  'vector.DistanceXZ(AICF_GateCrewProbe.s_ReturnPosition, car.GetOrigin()) < 10','m_iGCPMoving < 3','s_ReturnedAt < AICF_GateCrewProbe.s_CompletedAt',
  'ORIGINAL_ENTITY_RPL_CHANGED','System.GetTickCount() - m_iGCPStarted > 300000','native_compartment_exact=%1','s_MainWorldSpawns++','EntityID actorId = actor.GetID();','EntityID memberId = m_GCPActor.GetID();','EntityID carId = m_GCPCar.GetID();')){if(!$s.Contains($guard)){'MISSING_NATIVE_PROOF_GUARD';break}}
 if(!$s.Contains('trip.GetLastTransitionReason() == "CREW_UNAVAILABLE"') -or !$s.Contains('FALSE_CREW_UNAVAILABLE_TACTICAL')){'ACTUAL_OLD_FALSE_CREW_TRANSITION_WITHOUT_PARKING_FAULT'}
 if($s -match '\b(?:MarkReady|CommitTransition|SetHealth|SetHealthScaled|SetWorldTransform|CreditNativeRouteProgress|AddAction|ActivateAI|DeactivateAI|SetLOD)\s*\('){'NO_FAKE_READINESS_ACTIONS_PATH_HEALTH_OR_LIVE_TRANSFORMS'}
 if(([regex]::Matches($s,'\.SetOrigin\(')).Count -ne 1){'ONLY_EMPTY_CONTROLLER_SITE_BIRTH'}
 if($s -match 'override\s+(?:protected\s+)?(?:bool|void)\s+(?:RevalidateSpawnCommit|TrySpawnSelectedSiteForAcquisition|IsExpired)\('){'COMMIT_AND_NATIVE_GATE_CLOCK_UNCHANGED'}
 $post=$s.Substring($s.IndexOf('if (!car || !AICF_GateCrewProbe.s_ReturnedAt) return;'))
 $post=$post.Substring(0,$post.IndexOf('override protected void Stop('))
 if($post -match '\bs_Gate\b'){'POST_RETURN_PROOF_SURVIVES_NATIVE_OWNER_RELEASE'}
}
if(@(Rules $fixture).Count){throw "Gate crew fixture: $(Rules $fixture)"}
$mutants=0
foreach($guard in @('!managedDeployment','group.GetAgentsCount() != 0','super.ProcessFaction(factionState, faction);','HasExactFactionRoster(slot.GetGroup(), "USSR", 1',
 'GetNormalizedDoorState()) > 0.01','m_Campaign.IsMaster()','gate.m_bReturnCompleted','gate.Matches(trip)','System.GetTickCount() + 500','GetMobilityRecoveryAttempts() != m_iGCPAttempts','m_iGCPMoving < 3','ORIGINAL_ENTITY_RPL_CHANGED','s_OriginalSeat = gate.m_Seat;','s_OriginalSeat.GetVehicle() != car','!m_bRosterReady','EntityID memberId = m_GCPActor.GetID();')){
 if(!@(Rules ($fixture.Replace($guard,'MUTANT'))).Count){throw "Fixture mutant escaped $guard"};$mutants++
}
if(!@(Rules ($fixture.Replace('if (!car || !AICF_GateCrewProbe.s_ReturnedAt) return;','if (!car || !AICF_GateCrewProbe.s_ReturnedAt) return; if (!AICF_GateCrewProbe.s_Gate) return;'))).Count){throw 'Post-return weak gate owner mutant escaped'};$mutants++
$audit=Join-Path $RepositoryRoot 'tests/log-audits/Test-NativeGateCrewExitLog.ps1'
. $audit -LogPath 'UNUSED' -LauncherPath 'UNUSED' -NativeExitPath 'UNUSED' -EvidenceRoot 'UNUSED'
$raw=@'
[ROSTER_READY]
[GATE_CREW_PROBE] event=EMPTY_BIRTH mode=AFTER classification=CONTROLLED_GATE_SITE_ONLY group=G agents=0 intended=0 actor_teleports=0
[GATE_CREW_PROBE] event=ACTUAL_READY_CLOSED mode=AFTER classification=CONTROLLED_GATE_SITE_ONLY group=G actors=1 actor=A actor_rpl=AR gate=D state=0 control=0 fake_ready=0
[VEHICLE_PRESET_PURCHASE] operation=OP supplies=50
[GATE_CREW_PROBE] event=NATIVE_CAR_SPAWN mode=AFTER classification=CONTROLLED_GATE_SITE_ONLY vehicle=V vehicle_rpl=VR native_spawns=1
[VEHICLE_STATE_CHANGED] group_generation=1 trip_generation=1 operation_id=OP reason=NEW_VEHICLE
[GATE_CREW_PROBE] event=NATIVE_PILOT_CHAIN mode=AFTER classification=CONTROLLED_GATE_SITE_ONLY operation=OP member=A seat=0:2:0 gate=D window_start=1000 window_deadline=61000 actual_original_pilot=1 closed_setup=1 vehicle=V vehicle_rpl=VR
[GATE_CREW_PROBE] event=ACTUAL_EMPTY_BEFORE_TICK mode=AFTER classification=CONTROLLED_GATE_SITE_ONLY operation=OP member=A current_native=SCR_AIGetOutVehicle:RUNNING window_start=1000 window_deadline=61000 actor_health=1 vehicle_health=1 conscious=1
[TRANSPORT_NATIVE_GATE_CREW_EXIT_OWNED] operation_id=OP
[GATE_CREW_PROBE] event=NATIVE_COMPLETED_RETURN mode=AFTER classification=CONTROLLED_GATE_SITE_ONLY completion=10000 returned=11000 observed_exit=1 actual_callback=1 owned_polls=5 window_start=1000 window_deadline=61000
[GATE_CREW_PROBE] event=DONE mode=AFTER classification=CONTROLLED_GATE_SITE_ONLY passed=1 reason=ACTUAL_PILOT_NATIVE_RETURN_AND_10M actual_empty=2000 owned_polls=5 completion=10000 returned=11000 moving_polls=3 deadline=100000 current_deadline=100000 attempts=0 captured_attempts=0 measured_motion_m=10 member=A member_rpl=AR actor_health=1 captured_actor_health=1 vehicle=V vehicle_rpl=VR vehicle_health=1 captured_vehicle_health=1 native_compartment_exact=1 native_spawns=1
Game destroyed.
'@
$cases=@();$cases+=,@('positive',$raw,$true)
foreach($pair in @(@('missing chain','event=NATIVE_PILOT_CHAIN','event=UNKNOWN'),@('fake birth','agents=0','agents=1'),@('unclosed door','state=0 control=0','state=1 control=1'),
 @('wrong pilot','actual_original_pilot=1','actual_original_pilot=0'),@('foreign stage','SCR_AIGetOutVehicle:RUNNING','SCR_AIIdleBehavior:RUNNING'),@('unconscious original','conscious=1','conscious=0'),
 @('injured original','actor_health=1','actor_health=0.5'),@('no actual callback','actual_callback=1','actual_callback=0'),@('renewed window','window_deadline=61000','window_deadline=62000'),
 @('wrong actor rpl','member_rpl=AR','member_rpl=OTHER'),@('wrong vehicle rpl','vehicle_rpl=VR vehicle_health','vehicle_rpl=OTHER vehicle_health'),@('wrong seat','native_compartment_exact=1','native_compartment_exact=0'),
 @('short motion','measured_motion_m=10','measured_motion_m=9.99'),@('few motion polls','moving_polls=3','moving_polls=2'),@('changed deadline','current_deadline=100000','current_deadline=100001'),@('spent budget','attempts=0 captured_attempts=0','attempts=1 captured_attempts=0'),
 @('wrong birth group','group=G agents=0','group=OTHER agents=0'),@('empty vehicle rpl','vehicle_rpl=VR','vehicle_rpl=NONE'),@('equal dead health','vehicle_health=1 captured_vehicle_health=1','vehicle_health=0 captured_vehicle_health=0'),
 @('completion before window','completion=10000','completion=999'),@('return after window','returned=11000','returned=61001'),@('return exactly deadline','returned=11000','returned=61000'),@('DONE receipt differs','completion=10000 returned=11000 moving_polls','completion=10001 returned=11001 moving_polls'),
 @('extended window everywhere','window_deadline=61000','window_deadline=61001'),@('missing production proof','[TRANSPORT_NATIVE_GATE_CREW_EXIT_OWNED]','[UNKNOWN]'),@('empty exactly expired','actual_empty=2000','actual_empty=61000'),@('extra native car','native_spawns=1','native_spawns=2'),@('missing actual native spawn','event=NATIVE_CAR_SPAWN','event=UNKNOWN'))){
 $changed=$raw.Replace($pair[1],$pair[2]);if($pair[0] -eq 'renewed window'){$changed=$raw.Replace('actual_callback=1 owned_polls=5 window_start=1000 window_deadline=61000','actual_callback=1 owned_polls=5 window_start=1000 window_deadline=62000')};$cases+=,@($pair[0],$changed,$false)
}
$actual=0
foreach($case in $cases){$issues=@(Test-AICFGateCrewProof $case[1] 'AFTER');if(($issues.Count -eq 0) -ne $case[2]){throw "Gate crew log case: $($case[0]) issues=$issues"};$actual++}
$stop='[ACCEPTANCE_FAILURE_LATCHED] operation_id=OP group_gen=1 trip_gen=1 reason=COORDINATOR_STOP'
$stopCases=@();$stopCases+=,@('exact owned stop after DONE',($raw+"`n"+$stop),$true)
$stopCases+=,@('before DONE stop',($stop+"`n"+$raw),$false)
$stopCases+=,@('foreign post DONE stop',($raw+"`n"+$stop.Replace('operation_id=OP','operation_id=FOREIGN')),$false)
$stopCases+=,@('wrong generation stop',($raw+"`n"+$stop.Replace('group_gen=1','group_gen=2')),$false)
$stopCases+=,@('different post DONE error',($raw+"`n"+$stop.Replace('COORDINATOR_STOP','OTHER')),$false)
foreach($case in $stopCases){$issues=@(Test-AICFGateCrewProof $case[1] 'AFTER');if(($issues.Count -eq 0) -ne $case[2]){throw "Gate crew stop case $($case[0]) issues=$issues"};$actual++}
$before=$raw.Replace('mode=AFTER','mode=BEFORE').Replace('reason=ACTUAL_PILOT_NATIVE_RETURN_AND_10M','reason=TERMINAL_TACTICAL_BEGIN_REJECTED').Replace('actual_empty=2000 owned_polls=5 completion=10000 returned=11000','actual_empty=2000 owned_polls=0 completion=0 returned=0')
$before=[regex]::Replace($before,'(?m)^.*event=NATIVE_COMPLETED_RETURN[^\r\n]*\r?\n','')
$before=$before.Replace('[GATE_CREW_PROBE] event=DONE',"[VEHICLE_STATE_CHANGED] operation_id=OP reason=CREW_UNAVAILABLE`n[GATE_CREW_PROBE] event=DONE")
if(@(Test-AICFGateCrewProof $before 'BEFORE').Count){throw 'Actual old crew failure positive rejected'};$actual++
if(@(Test-AICFGateCrewProof ($before.Replace('TERMINAL_TACTICAL_BEGIN_REJECTED','FALSE_CREW_UNAVAILABLE_TACTICAL')) 'BEFORE').Count){throw 'Actual old tactical false crew transition rejected'};$actual++
if(!@(Test-AICFGateCrewProof ($before.Replace('reason=CREW_UNAVAILABLE','reason=OTHER')) 'BEFORE').Count){throw 'Unknown old failure accepted'};$actual++
if(!@(Test-AICFGateCrewProof ($before.Replace('actual_empty=2000','actual_empty=61000')) 'BEFORE').Count){throw 'Expired old empty observation accepted'};$actual++
Write-Output "PASS gate crew native probe: source_mutants=$mutants actual_log_cases=$actual native=NOT_RUN"
