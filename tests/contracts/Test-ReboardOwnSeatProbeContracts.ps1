param([string]$RepositoryRoot=(Split-Path -Parent (Split-Path -Parent $PSScriptRoot)))
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot '../lib/Stage3StaticAudit.Common.ps1')
$fixture=[IO.File]::ReadAllText((Join-Path $RepositoryRoot 'tests/fixtures/AICF_ReboardOwnSeatProbe.c'))
function Body([string]$s,[string]$name){Get-AICFMethodBody ([pscustomobject]@{Source=$s;Code=(ConvertTo-AICFCodeText $s)}) $name}
function Rules([string]$s){
 foreach($g in @('!m_bRosterReady','super.SpawnInitialRoster(factionState, faction)','super.ProcessFaction(factionState, faction);','HasExactFactionRoster(slot.GetGroup(), "USSR", 5',
 'recipe.ValidateRoster(slot, catalog.Seats(), reason) && PreflightVehiclePreset(slot, faction, recipe, reason)','AICF_OwnSeatProbe.Current(trip)',
 'AICF_OwnSeatProbe.s_FaultCalls++;','pilotSeat.GetOccupant() || pilotSeat.IsReserved() || combat','!AICF_VehicleShortRecovery.CanSeat(trip, cargo, pilotSeat)',
 'AICF_VehicleShortRecovery.TrySeat(trip, cargo, pilotSeat, "CONTROLLED_OWN_WRONG_SEAT_FAULT")','AICF_OwnSeatProbe.CaptureConflict(trip);','super.PollReboarding(trip, causationId, combat)',
 'state.m_iReboardDeadline != AICF_OwnSeatProbe.s_Deadline','System.GetTickCount() >= AICF_OwnSeatProbe.s_Deadline',
 'AICF_OwnSeatProbe.s_MotionPolls >= 3','System.GetTickCount() + 500','AICF_OwnSeatProbe.s_LastShort <= 3','AICF_OwnSeatProbe.s_Payments == 1','AICF_OwnSeatProbe.s_Spawns == 1',
 'trip.GetMovementState().GetMobilityRecoveryAttempts() == AICF_OwnSeatProbe.s_OriginalBudget','s_Lease.GetLeaseGeneration() != s_LeaseGeneration','s_Lease.GetVehicleGeneration() != s_VehicleGeneration',
 'trip.GetTripGeneration() != s_TripGeneration','actor.GetID() != s_Ids[index]','Health(actor) != s_Health[index]','AICF_VehicleBehaviorBinding.ComponentRpl(actor) != s_Rpls[index]',
 'm_iAbsoluteDeadlineMs == before + naturalPause','super.CommitBehaviorClockPause(paused, nowMs)','s_Car.GetID() != s_CarId','AICF_OwnSeatProbe.s_Ids.Clear()')){if(!$s.Contains($g)){'EXACT_NATIVE_BOUNDARY_GUARDS';break}}
 if(!$s.Contains('!s_Owner || AICF_MatchController.GetActiveController() != s_Owner || !s_Slot') -or !$s.Contains('if (!AICF_OwnSeatProbe.Current(trip)) return outcome;') -or !$s.Contains('if (m_bStopped || AICF_MatchController.GetActiveController() != this || AICF_OwnSeatProbe.s_Owner != this) return;')){'OWNER_AND_POST_SUPER_REENTRY_FENCES'}
 foreach($g in @('aicfReboardOwnSeatInterrupt','bool pendingBefore = state.m_ReboardSeatConflict != null;','bool distantBefore = HasDistantOrder(trip);','outcome.GetReason() == "COMBAT_REBOARD_CANCELLED"','FIRST_THREAT_CLEARED_NO_FAR_INTENT','CORE_SECOND_REBOARD_WINDOW','if (AICF_OwnSeatProbe.InterruptCase() && !AICF_OwnSeatProbe.s_InterruptObserved)','AICF_OwnSeatProbe.s_FirstShort = AICF_OwnSeatProbe.s_LastShort;')){if(!$s.Contains($g)){'ACTUAL_THREAT_CANCEL_BETWEEN_NATIVE_EXIT_AND_OBSERVER';break}}
 if((ConvertTo-AICFCodeText $s) -match '\b(?:MarkReady|SetHealth|SetDamage|SetHealthScaled|Kill|SetOrigin|SetWorldTransform|Complete|Fail|ActivateAI|DeactivateAI|SetLOD|CommitTransition|SetReserved|CancelEntries|AbortOwnedActions)\s*\('){'NO_FAKE_ACTION_READINESS_HEALTH_POSE_OR_RESERVATION_CLEAR'}
 $poll=Body $s 'PollReboarding'
 if(([regex]::Matches($poll,'AICF_VehicleShortRecovery.TrySeat\(')).Count -ne 1 -or $poll.IndexOf('AICF_OwnSeatProbe.s_FaultCalls++;') -gt $poll.IndexOf('AICF_VehicleShortRecovery.TrySeat(') -or $poll.IndexOf('AICF_OwnSeatProbe.CaptureConflict(trip);') -gt $poll.LastIndexOf('super.PollReboarding(trip, causationId, combat)')){'ONE_FAULT_ACTUAL_READBACK_BEFORE_DECISION'}
 if((ConvertTo-AICFCodeText $poll) -match 'm_iReboardDeadline\s*=(?!=)|m_iShortAttempts\s*=(?!=)|m_iRetries\s*=(?!=)'){'NO_DEADLINE_OR_BUDGET_WRITES'}
}
if(@(Rules $fixture).Count){throw "Own-seat fixture: $(Rules $fixture)"};$source=1
foreach($guard in @('!m_bRosterReady','pilotSeat.IsReserved()','AICF_OwnSeatProbe.s_FaultCalls++;','state.m_iReboardDeadline != AICF_OwnSeatProbe.s_Deadline','AICF_OwnSeatProbe.s_MotionPolls >= 3','AICF_OwnSeatProbe.s_Payments == 1','actor.GetID() != s_Ids[index]','Health(actor) != s_Health[index]','m_iAbsoluteDeadlineMs == before + naturalPause')){if(!@(Rules ($fixture.Replace($guard,'MUTANT'))).Count){throw "Fixture negative escaped: $guard"};$source++}
foreach($guard in @('!s_Owner || AICF_MatchController.GetActiveController() != s_Owner || !s_Slot','if (!AICF_OwnSeatProbe.Current(trip)) return outcome;','if (m_bStopped || AICF_MatchController.GetActiveController() != this || AICF_OwnSeatProbe.s_Owner != this) return;')){if(!@(Rules ($fixture.Replace($guard,'MUTANT'))).Count){throw "Fixture reentry negative escaped: $guard"};$source++}
foreach($guard in @('bool pendingBefore = state.m_ReboardSeatConflict != null;','outcome.GetReason() == "COMBAT_REBOARD_CANCELLED"','FIRST_THREAT_CLEARED_NO_FAR_INTENT','AICF_OwnSeatProbe.s_FirstShort = AICF_OwnSeatProbe.s_LastShort;')){if(!@(Rules ($fixture.Replace($guard,'MUTANT'))).Count){throw "Fixture interrupt negative escaped: $guard"};$source++}
$audit=Join-Path $RepositoryRoot 'tests/log-audits/Test-ReboardOwnSeatLog.ps1'
. $audit -LogPath UNUSED -LauncherPath UNUSED -NativeExitPath UNUSED -EvidenceRoot UNUSED
$lines=New-Object 'System.Collections.Generic.List[string]'
$lines.Add('[ROSTER_READY]')
$prefix='[OWN_SEAT_PROBE] mode=AFTER scope=CONTROLLED_OWN_WRONG_SEAT '
$lines.Add($prefix+'event=READY actual=5 aggregate_ready=1 fake_ready=0 group_rpl=GR')
$lines.Add($prefix+'event=NATIVE_SPAWN vehicle=V rpl=VR count=1')
$lines.Add($prefix+'event=NATIVE_PURCHASE operation=OP purchased=1 paid=475 payments=1')
$lines.Add($prefix+'event=INITIAL_EXACT5 operation=OP vehicle=V vehicle_rpl=VR')
for($p=0;$p -lt 5;$p++){$lines.Add($prefix+"event=ACTOR phase=INITIAL position=$p actor=A$p rpl=R$p health=1 captured_health=1 captured_id=A$p captured_rpl=R$p expected_seat=S$p actual_seat=S$p")}
$lines.Add($prefix+'event=NATIVE_CLOCK_PAUSE operation=OP paused=1 accepted=1 exact_native_clock=1 clock_write=0')
$lines.Add($prefix+'event=NATIVE_FAULT_SUBMITTED operation=OP accepted=1 calls=1 actor_displacement_bound=25 original_deadline=130000 submitted_at=10000 mapping_write=0 health_write=0 vehicle_pose_write=0')
$lines.Add($prefix+'event=ACTUAL_CONFLICT_BEFORE_SUPER operation=OP expected=A0 occupant=A1 seat=S0 occupant_assigned=S1 deadline=130000 remaining=120000 actual_wrongseat=1 mapping_unchanged=1 health_unchanged=1')
$lines.Add('[VEHICLE_REBOARD_SEAT_CONFLICT] operation_id=OP trigger=BEFORE_DECISION')
$lines.Add('[VEHICLE_REBOARD_OWN_RESERVATION] operation_id=OP occupant=A1 seat=S0 captured_own_reservation=1 before_clear_own=1 occupied_same=1 native_wrong_entry=0 phase=BEFORE_EXIT_CLEAR')
$lines.Add('[VEHICLE_REBOARD_OWN_RESERVATION] operation_id=OP occupant=A1 seat=S0 captured_own_reservation=1 after_clear_unreserved=1 occupied_same=1 phase=AFTER_EXIT_CLEAR')
$lines.Add('[VEHICLE_REBOARD_OWN_SEAT_CLEARANCE] operation_id=OP occupant=A1 wrong_seat=S0 assigned_seat=S1 short_attempt=1 maximum_shared_short_attempts=3 accepted=1 reboard_deadline_ms=130000 mapping_unchanged=1')
$lines.Add($prefix+'event=NATIVE_CLOCK_PAUSE operation=OP paused=0 accepted=1 exact_native_clock=1 clock_write=0')
for($p=0;$p -lt 5;$p++){$lines.Add($prefix+"event=ACTOR phase=FINAL position=$p actor=A$p rpl=R$p health=1 captured_health=1 captured_id=A$p captured_rpl=R$p expected_seat=S$p actual_seat=S$p")}
$lines.Add($prefix+'event=DONE passed=1 reason=OWN_ORIGINAL_RECOVERED_EXACT5_NATIVE_10M fault_calls=1 conflict=1 exit_readback=1 shared_short=1 deadline=130000 current_deadline=130000 payments=1 paid=475 moving_polls=3 displacement_m=10 current_identity=1 exact5=1 old_mobility_attempts=0 current_mobility_attempts=0 physical_spawns=1 operation=OP group_gen=1 trip_gen=1 car=V car_rpl=VR car_health=1 captured_car_health=1 interrupt_case=0 actual_interrupt=0 first_deadline=0 second_deadline=0 first_shared_short=0')
$lines.Add('Game destroyed.')
$raw=$lines -join "`n"
$cases=@();$cases+=,@('positive',$raw,$true)
foreach($m in @(
 @('missing readiness','[ROSTER_READY]','UNKNOWN'),@('missing stop','Game destroyed.','UNKNOWN'),@('fake aggregate','aggregate_ready=1','aggregate_ready=0'),
 @('extra fault','fault_calls=1','fault_calls=2'),@('unsafe fault bound','actor_displacement_bound=25','actor_displacement_bound=26'),
 @('expired fault','submitted_at=10000','submitted_at=130000'),@('renewed deadline','current_deadline=130000','current_deadline=130001'),
 @('wrong actor','actor=A1 rpl=R1 health=1 captured_health=1 captured_id=A1','actor=FOREIGN rpl=R1 health=1 captured_health=1 captured_id=A1'),
 @('dead original','health=1 captured_health=1','health=0 captured_health=0'),@('missing Rpl','rpl=R1','rpl=NONE'),
 @('wrong conflict original','expected=A0 occupant=A1','expected=A0 occupant=FOREIGN'),@('swapped mapping','occupant_assigned=S1','occupant_assigned=S2'),
 @('few motion polls','moving_polls=3','moving_polls=2'),@('short motion','displacement_m=10','displacement_m=9.99'),@('infinite motion','displacement_m=10','displacement_m=Infinity'),@('NaN motion','displacement_m=10','displacement_m=NaN'),
 @('new car','car=V car_rpl=VR','car=NEW car_rpl=VR'),@('extra spawn','physical_spawns=1','physical_spawns=2'),
 @('extra purchase','payments=1 paid=475','payments=2 paid=475'),@('extra short budget','shared_short=1','shared_short=4'),
 @('foreign exited actor','operation_id=OP occupant=A1 wrong_seat','operation_id=OP occupant=FOREIGN wrong_seat'),
 @('reset exit token','short_attempt=1 maximum_shared','short_attempt=0 maximum_shared'),
 @('unknown reservation at capture','captured_own_reservation=1','captured_own_reservation=0'),
 @('reservation changed before clear','before_clear_own=1','before_clear_own=0'),
 @('still reserved after clear','after_clear_unreserved=1','after_clear_unreserved=0'),
 @('live native wrong entry','native_wrong_entry=0','native_wrong_entry=1'),
 @('missing actual exit','event=ACTUAL_CONFLICT_BEFORE_SUPER','event=UNKNOWN'),@('missing production exit','[VEHICLE_REBOARD_OWN_SEAT_CLEARANCE]','[UNKNOWN]'),
 @('damaged car','car_health=1 captured_car_health=1','car_health=0 captured_car_health=0'),@('clock extended','exact_native_clock=1','exact_native_clock=0'),
 @('wrong final seat','phase=FINAL position=1 actor=A1 rpl=R1 health=1 captured_health=1 captured_id=A1 captured_rpl=R1 expected_seat=S1 actual_seat=S1','phase=FINAL position=1 actor=A1 rpl=R1 health=1 captured_health=1 captured_id=A1 captured_rpl=R1 expected_seat=S1 actual_seat=S0')
)){$cases+=,@($m[0],$raw.Replace($m[1],$m[2]),$false)}
$stop='[ACCEPTANCE_FAILURE_LATCHED] operation_id=OP group_gen=1 trip_gen=1 reason=COORDINATOR_STOP'
$cases+=,@('post DONE own stop',($raw+"`n"+$stop),$true)
$cases+=,@('pre DONE stop',($stop+"`n"+$raw),$false)
$cases+=,@('foreign post DONE stop',($raw+"`n"+$stop.Replace('operation_id=OP','operation_id=FOREIGN')),$false)
$actual=0
$reservationLines=@([regex]::Matches($raw,'\[VEHICLE_REBOARD_OWN_RESERVATION\][^\r\n]*')|ForEach-Object{$_.Value})
$reordered=$raw.Replace($reservationLines[0],'__RESERVATION_SWAP__').Replace($reservationLines[1],$reservationLines[0]).Replace('__RESERVATION_SWAP__',$reservationLines[1])
$cases+=,@('reversed exact reservation receipts',$reordered,$false)
foreach($case in $cases){$issues=@(Test-AICFOwnSeatProof $case[1] 'AFTER');if(($issues.Count -eq 0) -ne $case[2]){throw "Own-seat log $($case[0]): $issues"};$actual++}
$before=$raw.Replace('mode=AFTER','mode=BEFORE').Replace('reason=OWN_ORIGINAL_RECOVERED_EXACT5_NATIVE_10M','reason=TERMINAL_TACTICAL_SEAT_OCCUPIED')+"`n[VEHICLE_STATE_CHANGED] operation_id=OP reason=TACTICAL_SEAT_OCCUPIED`n[ACCEPTANCE_FAILURE_LATCHED] operation_id=OP group_gen=1 trip_gen=1 reason=TACTICAL_SEAT_OCCUPIED"
if(@(Test-AICFOwnSeatProof $before BEFORE).Count){throw 'Actual old-flow negative proof rejected'};$actual++
$interruptLines=@(
 ($prefix+'event=INTERRUPT_NATIVE_THREAT_SUBMITTED operation=OP first_deadline=130000 shared_short=1 pending_receipt=1 pilot_unreserved=1 actual_native_threat_next_tick=1'),
 ($prefix+'event=ACTUAL_LOCAL_THREAT_CANCELLED operation=OP first_deadline=130000 pending_before=1 pending_after=0 combat_argument=1 distant_before=0 pilot_unreserved=1 first_shared_short=1 outcome=COMBAT_REBOARD_CANCELLED'),
 ($prefix+'event=CORE_SECOND_REBOARD_WINDOW operation=OP first_deadline=130000 second_deadline=140000 observed_at=15000 fixture_clock_write=0')
) -join "`n"
$unpause=$prefix+'event=NATIVE_CLOCK_PAUSE operation=OP paused=0'
$interrupted=$raw.Replace($unpause,($interruptLines+"`n"+$unpause)).Replace('deadline=130000 current_deadline=130000 payments','deadline=140000 current_deadline=140000 payments').Replace('interrupt_case=0 actual_interrupt=0 first_deadline=0 second_deadline=0 first_shared_short=0','interrupt_case=1 actual_interrupt=1 first_deadline=130000 second_deadline=140000 first_shared_short=1')
if(@(Test-AICFOwnSeatProof $interrupted AFTER $true).Count){throw "Actual native interrupt positive rejected: $(Test-AICFOwnSeatProof $interrupted AFTER $true)"};$actual++
foreach($m in @(@('cancel before pending','pending_before=1','pending_before=0'),@('not real threat','combat_argument=1','combat_argument=0'),@('distant override prevents cancel','distant_before=0','distant_before=1'),@('residual reservation','pilot_unreserved=1','pilot_unreserved=0'),@('wrong cancelled budget','first_shared_short=1','first_shared_short=0'),@('fixture rewrites clock','fixture_clock_write=0','fixture_clock_write=1'),@('no real cancel','outcome=COMBAT_REBOARD_CANCELLED','outcome=TACTICAL_REBOARD_PENDING'),@('missing new window','event=CORE_SECOND_REBOARD_WINDOW','event=UNKNOWN'),@('new trip','event=CORE_SECOND_REBOARD_WINDOW operation=OP','event=CORE_SECOND_REBOARD_WINDOW operation=NEW'))){if(!@(Test-AICFOwnSeatProof $interrupted.Replace($m[1],$m[2]) AFTER $true).Count){throw "Interrupt unsafe log accepted: $($m[0])"};$actual++}
if(!@(Test-AICFOwnSeatProof $interrupted.Replace('second_deadline=140000','second_deadline=130000').Replace('deadline=140000 current_deadline=140000','deadline=130000 current_deadline=130000') AFTER $true).Count){throw 'Old deadline falsely accepted as new episode'};$actual++
"PASS Own-seat probe source=$source full_log_cases=$actual native=NOT_RUN"
