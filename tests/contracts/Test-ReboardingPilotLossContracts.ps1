param([string]$RepositoryRoot=(Split-Path -Parent (Split-Path -Parent $PSScriptRoot)))
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot '../lib/Stage3StaticAudit.Common.ps1')
$source=[IO.File]::ReadAllText((Join-Path $RepositoryRoot 'AIConflictCore/Scripts/Game/AIConflict/Vehicles/AICF_VehicleBehaviorFlow.c'))
$fixture=[IO.File]::ReadAllText((Join-Path $RepositoryRoot 'tests/fixtures/AICF_VehicleBehaviorProbe.c'))
function Body([string]$s,[string]$n){Get-AICFMethodBody ([pscustomobject]@{Source=$s;Code=(ConvertTo-AICFCodeText $s)}) $n}
function Rules([string]$s){
 $poll=Body $s 'PollReboarding';$pilot=Body $s 'HasMobilePilot';$rebind=Body $s 'ReconcileReboardingActions';$exit=Body $s 'EnsureExit'
 foreach($guard in @('seat.GetType() != ECompartmentType.PILOT','trip.VehiclePresetMember(preset.m_aMembers[index])','!IsFit(actor) || !AICF_VehicleShortRecovery.IsCurrentMember(trip, actor)','if (!requireSettled) return true;','seat.GetOccupant() == actor && access && access.IsInCompartment() && access.GetCompartment() == seat','!access.IsGettingIn() && !access.IsGettingOut() && !access.IsSwitchingSeatsAnim()','CompartmentAccessComponent.GetVehicleIn(actor) == trip.GetLease().GetVehicle()')){if(!$pilot.Contains($guard)){'EXACT_FIT_PILOT_REQUIRED';break}}
 if(!$poll.Contains('!HasMobilePilot(trip, seats, false) && !RestoreCrewAssignments(trip, fitReason)') -or !$poll.Contains('if (fitReason == "CREW_RESTORE_PENDING") return AICF_TripOutcome.Wait(fitReason, causationId);') -or $poll.IndexOf('RestoreCrewAssignments') -lt $poll.IndexOf('System.GetTickCount() >= state.m_iReboardDeadline') -or $poll.IndexOf('RestoreCrewAssignments') -gt $poll.IndexOf('bool settled')){'REPAIR_OR_WAIT_INSIDE_ORIGINAL_DEADLINE'}
 if(!$poll.Contains('if (!HasMobilePilot(trip, seats, true)) return AICF_TripOutcome.FallbackToFoot("REBOARD_PILOT_UNAVAILABLE", causationId);') -or $poll.IndexOf('HasMobilePilot(trip, seats, true)') -lt $poll.IndexOf('CancelEntries(trip);',$poll.IndexOf('state.m_iSettledPolls++')) -or $poll.IndexOf('HasMobilePilot(trip, seats, true)') -gt $poll.IndexOf('REBOARD_COMPLETE')){'FINAL_PILOT_READBACK_AFTER_CALLBACKS'}
 foreach($guard in @('if (!token || !IsFit(token.m_Actor)) continue;','FindCapturedPosition(trip, token.m_Actor)','preset.m_aMembers.Find(position)','token.m_Seat == seats[seatIndex]','token.Retire();','!IsCurrent(trip) || !AICF_VehicleShortRecovery.IsCurrentMember(trip, token.m_Actor)','new AICF_VehicleBehaviorAction(trip, token.m_Actor, seats[seatIndex])','rebound.m_iRetries = token.m_iRetries;','rebound.m_iShortAttempts = token.m_iShortAttempts;','rebound.m_iLastProgressAt = token.m_iLastProgressAt;','rebound.m_vLastProgressPosition = token.m_vLastProgressPosition;','state.m_aActions[index] = rebound;','state.m_iSettledPolls = 0;')){if(!$rebind.Contains($guard)){'EXACT_ROLE_ACTION_REBIND_RETAINS_BUDGETS';break}}
 if(!$exit.Contains('AICF_VehicleBehaviorAction token = FindAction(trip, actor);') -or !$exit.Contains('if (token && token.m_Exit) return;') -or !$exit.Contains('if (token) token.Retire();') -or !$exit.Contains('!trip.GetBehaviorState().m_aActions.Contains(token)')){'OWNED_GETIN_TO_GETOUT_WITHOUT_TOKEN_RESET'}
 if(!$poll.Contains('!ReconcileReboardingActions(trip, seats)') -or $poll.IndexOf('ReconcileReboardingActions') -gt $poll.IndexOf('bool settled')){'REMAP_BEFORE_ENTRY'}
 if((ConvertTo-AICFCodeText ($pilot+$rebind+$poll+$exit)) -match 'm_iReboardDeadline\s*=|m_iCrewRestoreDeadline\s*=|m_iRetries\s*=\s*0|m_iShortAttempts\s*=\s*0|SetHealth|SetDamage|Resurrect|SpawnEntity|CommitTransition'){'NO_NEW_DEADLINE_BUDGET_OR_HEALTH'}
}
function FixtureRules([string]$s){
 $init=Body $s 'AICF_VBInitialize';$inject=Body $s 'AICF_VBTryReboardPilotLoss';$death=Body $s 'AICF_VBObserveReboardPilotDeath';$verify=Body $s 'AICF_VBReboardPilotRecovered';$update=Body $s 'Update'
 if(!$init.Contains('System.GetCLIParam("aicfReboardPilotLossProbe", pilotLossFlag) && pilotLossFlag == "1"') -or !$init.Contains('m_sVBScenario != "combat"') -or !$init.Contains('m_iVBSize = 3;') -or !$init.Contains('System.GetCLIParam("aicfCleanupIncapProbe", cleanupFlag)')){'ISOLATED_OPT_IN_THREE_ROSTER'}
 foreach($g in @('!m_bVBReboardPilotLossProbe || m_iVBStep != 4','if (m_bVBReboardPilotLossInjected)','trip != m_VBTrip','!trip.IsVehiclePresetCurrent()','trip.GetPhase() != AICF_ETransportTripPhase.TACTICAL_DEPLOYMENT || !trip.GetBehaviorState().m_bReboarding','state.m_iReboardDeadline <= System.GetTickCount()','trip.GetLease().GetLeaseGeneration() != m_iVBLeaseGeneration','!AICF_VehicleShortRecovery.IsCurrentMember(trip, replacement)','!AICF_VehicleShortRecovery.IsCurrentMember(trip, gunner)','AICF_VehicleBehaviorFlow.HasMobilePilot(trip, seats, true)','AICF_GroupRuntime.CountAliveAgents(m_VBGroup) != 2','m_iVBOriginalReboardDeadline = state.m_iReboardDeadline;','damage.Kill(Instigator.CreateInstigatorGM());','REBOARD_PILOT_LOSS_PHASE')){if(!$inject.Contains($g)){'CURRENT_UNSETTLED_NATIVE_PILOT_KILL';break}}
 if(!$inject.Contains('AICF_VBObserveReboardPilotDeath();') -or $inject.IndexOf('AICF_VBObserveReboardPilotDeath();') -gt $inject.IndexOf('damage.Kill(') -or $inject.Contains('REBOARD_PILOT_LOSS_NATIVE') -or !$death.Contains('nowMs > m_iVBReboardKillSubmittedAt') -or !$death.Contains('nowMs < m_iVBOriginalReboardDeadline') -or !$death.Contains('m_VBReboardVictim.GetID() == m_VBReboardVictimId') -or !$death.Contains('REBOARD_PILOT_LOSS_NATIVE') -or $death.Contains('.Kill(')){'ASYNC_DEATH_EXACT_LATER_UPDATE_ONE_KILL'}
 foreach($g in @('trip.GetBehaviorState().m_iReboardDeadline != m_iVBOriginalReboardDeadline','m_VBReboardGunner.GetID() != m_VBReboardGunnerId','m_VBReboardVictim.GetID() != m_VBReboardVictimId','AICF_VehicleBehaviorFlow.HasMobilePilot(trip, seats, true)','seats[index].GetOccupant() != m_VBReboardGunner','preset.m_aMembers[index] >= 0 || seats[index].GetOccupant()','damage.GetHealthScaled() <= m_fVBReboardGunnerHealth + 0.001','AICF_GroupRuntime.CountAliveAgents(m_VBGroup) == 1 && AICF_VBIdentities()')){if(!$verify.Contains($g)){'NATIVE_SINGLE_GUNNER_MOBILE_DEGRADED_SURVIVOR';break}}
 if(!$update.Contains('AICF_VBTryReboardPilotLoss();') -or $update.IndexOf('AICF_VBTryReboardPilotLoss();') -gt $update.IndexOf('m_VBCoordinator.Update(') -or !$update.Contains('REBOARD_PILOT_LOSS_RECOVERED') -or !$update.Contains('REBOARD_PILOT_LOSS_MOVEMENT') -or !$update.Contains('vector.DistanceXZ(m_vVBMovementStart, m_VBVehicle.GetOrigin()) >= 10')){'HOOK_BEFORE_TICK_AND_REAL_RESUME_ORACLE'}
 if((ConvertTo-AICFCodeText ($inject+$death+$verify)) -match 'SetHealth|SetDamage|ForceUnconsciousness|Teleport|GetInVehicle|SetOrigin|m_iReboardDeadline\s*=(?!=)|m_iRetries\s*=(?!=)|m_iShortAttempts\s*=(?!=)|CallLater'){'NO_FIXTURE_SEATING_HEAL_BUDGET_OR_DEADLINE_PRIMING'}
}
$issues=@(Rules $source);if($issues.Count){throw "Reboarding pilot loss: $issues"};$checks=1
$issues=@(FixtureRules $fixture);if($issues.Count){throw "Reboarding pilot loss fixture: $issues"};$checks++
foreach($m in @(
 @('m_iVBSize = 3;','m_iVBSize = 5;','ISOLATED_OPT_IN_THREE_ROSTER'),
 @('!trip.IsVehiclePresetCurrent()','false','CURRENT_UNSETTLED_NATIVE_PILOT_KILL'),
 @('AICF_VehicleBehaviorFlow.HasMobilePilot(trip, seats, true)','false','CURRENT_UNSETTLED_NATIVE_PILOT_KILL'),
 @('damage.Kill(Instigator.CreateInstigatorGM());','','CURRENT_UNSETTLED_NATIVE_PILOT_KILL'),
 @('trip.GetBehaviorState().m_iReboardDeadline != m_iVBOriginalReboardDeadline','false','NATIVE_SINGLE_GUNNER_MOBILE_DEGRADED_SURVIVOR'),
 @('preset.m_aMembers[index] >= 0 || seats[index].GetOccupant()','false','NATIVE_SINGLE_GUNNER_MOBILE_DEGRADED_SURVIVOR'),
 @('damage.GetHealthScaled() <= m_fVBReboardGunnerHealth + 0.001','true','NATIVE_SINGLE_GUNNER_MOBILE_DEGRADED_SURVIVOR'),
 @('AICF_VBTryReboardPilotLoss();','','HOOK_BEFORE_TICK_AND_REAL_RESUME_ORACLE')
 ,@('nowMs > m_iVBReboardKillSubmittedAt','true','ASYNC_DEATH_EXACT_LATER_UPDATE_ONE_KILL')
)){$changed=$fixture.Replace($m[0],$m[1]);if($changed -ceq $fixture -or $m[2] -notin @(FixtureRules $changed)){throw "Pilot fixture mutation survived: $($m[2])"};$checks++}
foreach($m in @(
 @('!IsFit(actor) || !AICF_VehicleShortRecovery.IsCurrentMember(trip, actor)','false','EXACT_FIT_PILOT_REQUIRED'),
 @('seat.GetOccupant() == actor && access && access.IsInCompartment() && access.GetCompartment() == seat','true','EXACT_FIT_PILOT_REQUIRED'),
 @('!HasMobilePilot(trip, seats, false) && !RestoreCrewAssignments(trip, fitReason)','false','REPAIR_OR_WAIT_INSIDE_ORIGINAL_DEADLINE'),
 @('HasMobilePilot(trip, seats, true)','HasMobilePilot(trip, seats, false)','FINAL_PILOT_READBACK_AFTER_CALLBACKS'),
 @('rebound.m_iRetries = token.m_iRetries;','rebound.m_iRetries = 0;','EXACT_ROLE_ACTION_REBIND_RETAINS_BUDGETS'),
 @('rebound.m_iShortAttempts = token.m_iShortAttempts;','rebound.m_iShortAttempts = 0;','EXACT_ROLE_ACTION_REBIND_RETAINS_BUDGETS'),
 @('if (token) token.Retire();','if (false) token.Retire();','OWNED_GETIN_TO_GETOUT_WITHOUT_TOKEN_RESET'),
 @('!ReconcileReboardingActions(trip, seats)','false','REMAP_BEFORE_ENTRY')
)){
 $changed=$source.Replace($m[0],$m[1]);if($changed -ceq $source -or $m[2] -notin @(Rules $changed)){throw "Pilot mutation survived: $($m[2])"};$checks++
}
# Реальные production bodies; заменяются только native типы/синтаксис Enforce.
$pilot=(Body $source 'HasMobilePilot').Replace('int index;','int index = 0;').Replace('&& access &&','&& access != null &&')
$rebind=(Body $source 'ReconcileReboardingActions').Replace('int index;','int index = 0;').Replace('!token ||','token == null ||')
$code=@'
using System.Collections.Generic;
public class NativeArray<T>:List<T>{public new int Count(){return base.Count;}public int Find(T value){return IndexOf(value);}}
public enum ECompartmentType{PILOT,CARGO,TURRET}
public class IEntity{public bool Fit=true;public bool Current=true;public int Position;public CompartmentAccessComponent Access=new CompartmentAccessComponent();}
public class ChimeraCharacter{public static IEntity Cast(IEntity a){return a;}}
public class BaseCompartmentSlot{public ECompartmentType Type;public IEntity Occupant;public new ECompartmentType GetType(){return Type;}public IEntity GetOccupant(){return Occupant;}}
public class CompartmentAccessComponent{public BaseCompartmentSlot Seat;public bool Inside=true,Entering,Exiting,Switching;public object Vehicle;public bool IsInCompartment(){return Inside;}public BaseCompartmentSlot GetCompartment(){return Seat;}public bool IsGettingIn(){return Entering;}public bool IsGettingOut(){return Exiting;}public bool IsSwitchingSeatsAnim(){return Switching;}public static object GetVehicleIn(IEntity a){return a.Access.Vehicle;}}
public class AICF_VehiclePreset{public NativeArray<int> m_aMembers=new NativeArray<int>();}
public class AICF_TestLease{public object Vehicle=new object();public object GetVehicle(){return Vehicle;}}
public class AICF_VehicleBehaviorState{public NativeArray<AICF_VehicleBehaviorAction> m_aActions=new NativeArray<AICF_VehicleBehaviorAction>();public int m_iSettledPolls=1;}
public class AICF_TransportTrip{public bool Current=true;public AICF_VehiclePreset Preset=new AICF_VehiclePreset();public AICF_TestLease Lease=new AICF_TestLease();public AICF_VehicleBehaviorState State=new AICF_VehicleBehaviorState();public IEntity[] Roster=new IEntity[3];public AICF_VehiclePreset GetVehiclePreset(){return Preset;}public AICF_TestLease GetLease(){return Lease;}public AICF_VehicleBehaviorState GetBehaviorState(){return State;}public IEntity VehiclePresetMember(int p){return p>=0 && p<3?Roster[p]:null;}}
public class AICF_VehicleShortRecovery{public static bool IsCurrentMember(AICF_TransportTrip trip,IEntity actor){return trip.Current && actor!=null && actor.Current;}}
public class AICF_VehicleBehaviorAction{public IEntity m_Actor;public BaseCompartmentSlot m_Seat;public int m_iRetries,m_iShortAttempts,m_iLastProgressAt,m_vLastProgressPosition,Retired;public AICF_VehicleBehaviorAction(AICF_TransportTrip t,IEntity a,BaseCompartmentSlot s){m_Actor=a;m_Seat=s;}public void Retire(){Retired++;}}
public class AICF_PilotProduction{
 public static bool IsFit(IEntity a){return a!=null && a.Fit;}
 public static bool IsCurrent(AICF_TransportTrip t){return t.Current;}
 public static int FindCapturedPosition(AICF_TransportTrip t,IEntity a){return a.Current?a.Position:-1;}
 public static bool HasMobilePilot(AICF_TransportTrip trip,NativeArray<BaseCompartmentSlot> seats,bool requireSettled) PILOT
 public static bool ReconcileReboardingActions(AICF_TransportTrip trip,NativeArray<BaseCompartmentSlot> seats) REBIND
}
'@
$code=$code.Replace('public CompartmentAccessComponent Access=new CompartmentAccessComponent();','public CompartmentAccessComponent Access=new CompartmentAccessComponent();public CompartmentAccessComponent GetCompartmentAccessComponent(){return Access;}')
Add-Type -TypeDefinition ($code.Replace(' PILOT',' '+$pilot).Replace(' REBIND',' '+$rebind))
$t=New-Object AICF_TransportTrip;$seats=New-Object 'NativeArray[BaseCompartmentSlot]'
for($i=0;$i -lt 3;$i++){$a=New-Object IEntity;$a.Position=$i;$a.Access.Vehicle=$t.Lease.Vehicle;$t.Roster[$i]=$a;$s=New-Object BaseCompartmentSlot;$s.Type=[ECompartmentType]::CARGO;if($i -eq 0){$s.Type=[ECompartmentType]::PILOT};$s.Occupant=$a;$a.Access.Seat=$s;$seats.Add($s);$t.Preset.m_aMembers.Add($i)}
function Pilot([bool]$settled,[bool]$expected,[string]$name){if([AICF_PilotProduction]::HasMobilePilot($t,$seats,$settled) -ne $expected){throw "Production Pilot predicate failed: $name"};$script:checks++}
Pilot $true $true 'initial exact pilot'
$t.Roster[0].Fit=$false;Pilot $false $false 'pilot lost during boarding with other fit cargo'
Pilot $true $false 'all other fit seats cannot fake resume'
$token=New-Object AICF_VehicleBehaviorAction($t,$t.Roster[1],$seats[1]);$token.m_iRetries=2;$token.m_iShortAttempts=3;$token.m_iLastProgressAt=51;$token.m_vLastProgressPosition=71;$t.State.m_aActions.Add($token)
$t.Preset.m_aMembers[0]=1;$t.Preset.m_aMembers[1]=-1
if(![AICF_PilotProduction]::ReconcileReboardingActions($t,$seats)){throw 'Production remap rejected current survivor'}
$new=$t.State.m_aActions[0]
if($token.Retired -ne 1 -or $new.m_Seat -ne $seats[0] -or $new.m_iRetries -ne 2 -or $new.m_iShortAttempts -ne 3 -or $new.m_iLastProgressAt -ne 51 -or $new.m_vLastProgressPosition -ne 71 -or $t.State.m_iSettledPolls -ne 0){throw 'Production remap reset budget/owner/history'};$checks++
Pilot $false $true 'current fit replacement assigned'
Pilot $true $false 'assignment does not prove actual pilot'
$seats[0].Occupant=$t.Roster[1];$t.Roster[1].Access.Seat=$seats[0];Pilot $true $true 'exact physical replacement permits resume'
$t.Roster[1].Access.Entering=$true;Pilot $true $false 'still entering cannot resume';$t.Roster[1].Access.Entering=$false
$t.Roster[1].Access.Vehicle=New-Object object;Pilot $true $false 'foreign vehicle cannot resume';$t.Roster[1].Access.Vehicle=$t.Lease.Vehicle
$t.Roster[1].Current=$false;Pilot $false $false 'stale replacement identity'
$t=New-Object AICF_TransportTrip;$t.Preset.m_aMembers.Add(-1);$empty=New-Object 'NativeArray[BaseCompartmentSlot]';$s=New-Object BaseCompartmentSlot;$s.Type=[ECompartmentType]::PILOT;$empty.Add($s)
if([AICF_PilotProduction]::HasMobilePilot($t,$empty,$false)){throw 'Empty pilot accepted'};$checks++
"PASS Reboarding pilot loss controls=$checks production_pilot_and_action_remap=EXECUTED native_midboarding_loss=NOT_RUN"
