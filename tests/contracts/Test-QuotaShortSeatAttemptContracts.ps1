param([string]$RepositoryRoot=(Split-Path -Parent (Split-Path -Parent $PSScriptRoot)))
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'Test-ReboardOwnSeatConflictContracts.ps1') -RepositoryRoot $RepositoryRoot
$path=Join-Path $RepositoryRoot 'AIConflictCore/Scripts/Game/AIConflict/State/Vehicles/AICF_VehicleShortSeatAttempt.c'
$attemptText=[IO.File]::ReadAllText($path)
$plain=ConvertTo-AICFCodeText $attemptText
if($plain-match'GetWorld|GetOrigin|GetID\(|FindComponent|GetControlAIAgent|GetCharacterController|CallLater|SetReserved|GetInVehicle|SetHealth|SetLifeState|CommitTransition|Invoke\('){throw 'Counter commit must stay script-state-only'}
$seatBody=Body $short 'TrySeat'
$mark=$seatBody.IndexOf('quota.MarkDispatched();');$commit=$seatBody.IndexOf('attempt.Commit(');$native=$seatBody.IndexOf('bool accepted = access.GetInVehicle(')
if($mark-lt0-or$commit-lt$mark-or$native-lt$commit){throw 'Mark/guard/commit/native order broken'}
$tail=$seatBody.Substring($commit,$native-$commit)
if($tail-match'GetOrigin|GetID\(|FindComponent|Diagnostics|Invoke\(|SetReserved'){throw 'Native getter/logging/sideeffect between commit and GetIn'}
$attemptCode=$attemptText.Replace('ref ','').Replace('protected static','public static').Replace('static AICF_VehicleShortSeatAttempt For','public static AICF_VehicleShortSeatAttempt For')
$attemptCode=$attemptCode.Replace('class AICF_VehicleShortSeatAttempt','public class AICF_VehicleShortSeatAttempt:NativeObject').Replace('bool IsCurrent(','public bool IsCurrent(').Replace('bool Commit(','public bool Commit(')
$attemptCode=[regex]::Replace($attemptCode,'(?m)^(\s*)(AICF_\w+|IEntity|BaseCompartmentSlot|int|bool) (m_\w+);','$1public $2 $3;')
$attemptCode=$attemptCode.Replace('bool found;','bool found = false;').Replace('for (int index;','for (int index = 0;')
$coreReplay=[regex]::Replace($compiled,'(?m)^using [^\r\n]+;\s*','')
$stub='public class AICF_VehicleShortSeatAttempt:NativeObject{public bool Commit(AICF_TransportTrip t,IEntity a,BaseCompartmentSlot s,int now){return true;}}'
if(!$coreReplay.Contains($stub)){throw 'Known old caller shim missing'}
$coreReplay=$coreReplay.Replace($stub,$attemptCode).Replace('TRANSIT,TACTICAL_DEPLOYMENT,FALLBACK','TRANSIT,TACTICAL_DEPLOYMENT,FALLBACK,BOARDING').Replace('MAX_GROUP_SIZE=5','MAX_GROUP_SIZE=10')
$coreReplay=$coreReplay.Replace('public class AICF_TransportTrip:NativeObject{','public class AICF_TransportTrip:NativeObject{public AICF_VehicleBoardingState Board=new AICF_VehicleBoardingState();public AICF_VehicleBoardingState GetBoardingState(){return Board;}public bool IsTerminal(){return !Current||Phase==AICF_ETransportTripPhase.FALLBACK;}')
$coreReplay=$coreReplay.Replace('public class AICF_VehicleBehaviorAction:NativeObject{','public class AICF_VehicleBehaviorAction:NativeObject{public AICF_TransportTrip m_Trip{get{return Trip;}set{Trip=value;}}')
$prefix=@'
using System;using System.Collections.Generic;namespace QuotaShortSeatReplay {
public class AICF_VehicleBoardingActionToken:NativeObject{public IEntity Actor;public BaseCompartmentSlot Seat;public bool Short;public IEntity GetReservedEntity(){return Actor;}public BaseCompartmentSlot GetCompartment(){return Seat;}public bool WasShortRecoveryAttempted(){return Short;}public void MarkShortRecoveryAttempted(){Short=true;}}
public class AICF_VehicleBoardingTokenSet:NativeObject{public NativeArray<AICF_VehicleBoardingActionToken> Items=new NativeArray<AICF_VehicleBoardingActionToken>();public int Count(){return Items.Count();}public AICF_VehicleBoardingActionToken Get(int i){return Items[i];}}
public class AICF_VehicleBoardingState:NativeObject{public int Deadline=9000;public AICF_VehicleBoardingTokenSet Tokens=new AICF_VehicleBoardingTokenSet();public int GetAbsoluteDeadlineMs(){return Deadline;}public AICF_VehicleBoardingTokenSet GetTokens(){return Tokens;}}
'@
$caseCode=@'
public static class AttemptCases
{
 static int Cases;static AICF_TransportTrip T;static IEntity A;static BaseCompartmentSlot Seat;static AICF_VehicleBehaviorAction Token;static AICF_VehicleShortSeatAttempt Attempt;
 static void Need(bool yes,string why){Cases++;if(!yes)throw new Exception(why);}
 static void Setup(int spent=0)
 {
  AICF_AIQuotaVehicleDispatch.Reset();Clock.Now=1000;T=AICF_ConflictFixture.Create();A=T.Roster[1];Seat=AICF_ConflictFixture.Seats[0];Seat.Occupant=null;Seat.Reserved=null;A.Access.Inside=false;A.Access.Vehicle=null;A.Access.Seat=null;Token=new AICF_VehicleBehaviorAction(T,A,Seat){m_iShortAttempts=spent};T.State.m_aActions.Add(Token);Attempt=AICF_VehicleShortSeatAttempt.ForBehavior(T,Token);if(Attempt==null)throw new Exception("valid factory failed");
 }
 static bool Run(){return AICF_ExitProduction.TrySeat(T,A,Seat,"quota-counter",null,Attempt);}
 public static int RunAll()
 {
  Cases=0;Setup();Need(Run()&&Token.m_iShortAttempts==1&&A.Access.PhysicalEntries==1,"one actual native entry commits one old attempt");Need(!Attempt.Commit(T,A,Seat,Clock.Now)&&Token.m_iShortAttempts==1,"postcall cannot commit twice");
  Setup(2);A.Access.EntryAccepted=false;Need(!Run()&&Token.m_iShortAttempts==3&&A.Access.PhysicalEntries==1,"native false still consumes one real physical attempt");Need(AICF_VehicleShortSeatAttempt.ForBehavior(T,Token)==null,"spent3 does not get another allowance");
  Setup();AICF_AIQuotaVehicleDispatch.Allowed=false;Need(!Run()&&Token.m_iShortAttempts==0&&A.Access.PhysicalEntries==0&&Seat.ReservationWrites==0,"quota veto zero counter/native/reservation writes");
  Setup();AICF_AIQuotaVehicleDispatch.OnBegin=()=>T.Transitions+=2;Need(!Run()&&Token.m_iShortAttempts==0&&A.Access.PhysicalEntries==0,"quota callback phaseABA spends no attempt");
  Setup();Seat.OnReservationWrite=()=>AICF_AIQuotaVehicleDispatch.Current=false;Need(!Run()&&Token.m_iShortAttempts==0&&A.Access.PhysicalEntries==0,"reservation callback quota UNKNOWN zero native/counter");
  Setup();Seat.OnReservationWrite=()=>A.Current=false;Need(!Run()&&Token.m_iShortAttempts==0&&A.Access.PhysicalEntries==0,"human/stale ownership callback zero native/counter");
  Setup();Seat.OnReservationWrite=()=>Seat.Deleted=true;Need(!Run()&&Token.m_iShortAttempts==0&&A.Access.PhysicalEntries==0,"weak seat callback zero native/counter");
  Setup();Seat.OnReservationWrite=()=>Seat.Reserved=T.Roster[0];Need(!Run()&&Token.m_iShortAttempts==0&&A.Access.PhysicalEntries==0&&Seat.Reserved==T.Roster[0],"foreign reservation callback preserved without counter/native");
  Setup();AICF_AIQuotaVehicleDispatch.OnMark=()=>AICF_AIQuotaVehicleDispatch.Current=false;Need(!Run()&&Token.m_iShortAttempts==0&&A.Access.PhysicalEntries==0,"last dispatch metadata callback cannot spend before native");
  Setup();Seat.OnReservationWrite=()=>Clock.Now=9000;Need(!Run()&&Token.m_iShortAttempts==0&&A.Access.PhysicalEntries==0,"deadline unchanged and enforced immediately before physical commit");
  Setup();Seat.OnReservationWrite=()=>T.State.m_iReboardDeadline++;Need(!Run()&&Token.m_iShortAttempts==0&&A.Access.PhysicalEntries==0,"changed deadline is a new context, no borrowed old allowance");
  Setup();Seat.OnReservationWrite=()=>T.State.m_aActions.Clear();Need(!Run()&&Token.m_iShortAttempts==0&&A.Access.PhysicalEntries==0,"removed owner token cannot commit");
  Setup();Seat.OnReservationWrite=()=>T.Assignment=new AICF_StrategicAssignmentSnapshot();Need(!Run()&&Token.m_iShortAttempts==0&&A.Access.PhysicalEntries==0,"assignment replacement during reservation cannot commit");
  Setup();Seat.OnReservationWrite=()=>T.Transitions+=2;Need(!Run()&&Token.m_iShortAttempts==0&&A.Access.PhysicalEntries==0,"reservation phaseABA cannot commit");
  Setup();T.Transitions+=2;Need(!Attempt.Commit(T,A,Seat,1000)&&Token.m_iShortAttempts==0,"pure commit independently rejects phaseABA");
  Setup();A.Deleted=true;Need(!Attempt.Commit(T,A,Seat,1000)&&Token.m_iShortAttempts==0,"pure commit independently rejects weak actor");
  Setup();Token.m_iShortAttempts=1;Need(!Attempt.Commit(T,A,Seat,1000)&&Token.m_iShortAttempts==1,"other operation counter is preserved without reset/reuse");
  Setup();bool second=true;A.Access.OnGetIn=()=>second=Attempt.Commit(T,A,Seat,1000);Need(Run()&&!second&&Token.m_iShortAttempts==1&&A.Access.PhysicalEntries==1,"counter already committed before synchronous native callback");
  Setup();T.Phase=AICF_ETransportTripPhase.BOARDING;var board=new AICF_VehicleBoardingActionToken{Actor=A,Seat=Seat};T.Board.Tokens.Items.Add(board);Attempt=AICF_VehicleShortSeatAttempt.ForBoarding(T,T.Board,board);Need(Attempt!=null&&Run()&&board.Short&&A.Access.PhysicalEntries==1,"boarding old one-shot commits only at actual GetIn");Need(!Attempt.Commit(T,A,Seat,1000),"boarding one-shot cannot commit again");
  Setup();T.Phase=AICF_ETransportTripPhase.BOARDING;board=new AICF_VehicleBoardingActionToken{Actor=A,Seat=Seat};T.Board.Tokens.Items.Add(board);Attempt=AICF_VehicleShortSeatAttempt.ForBoarding(T,T.Board,board);Seat.OnReservationWrite=()=>AICF_AIQuotaVehicleDispatch.Current=false;Need(!Run()&&!board.Short&&A.Access.PhysicalEntries==0,"boarding quota UNKNOWN does not consume one-shot");
  Setup();T.Phase=AICF_ETransportTripPhase.BOARDING;board=new AICF_VehicleBoardingActionToken{Actor=A,Seat=Seat};T.Board.Tokens.Items.Add(board);Attempt=AICF_VehicleShortSeatAttempt.ForBoarding(T,T.Board,board);T.Board.Tokens.Items.Clear();Need(!Attempt.Commit(T,A,Seat,1000)&&!board.Short,"boarding removed token remains unspent");
  return Cases;
 }
}
}
'@
$replay=$prefix+$coreReplay+$caseCode
Add-Type -TypeDefinition $replay -Language CSharp
$actual=[QuotaShortSeatReplay.AttemptCases]::RunAll()
$unsafe=@(
 @{name='UncheckedCommit';from='if (!IsCurrent(trip, actor, seat, nowMs)) return false;';to='if (m_bCommitted) return false;'},
 @{name='PhaseABA';from='trip.GetTransitionCount() != m_iTransitions';to='trip.GetTransitionCount() < 0'},
 @{name='LateDeadline';from='nowMs >= m_iDeadline';to='nowMs > m_iDeadline + 1000'},
 @{name='TokenRemoved';from='!m_BehaviorState.m_aActions.Contains(m_Behavior)';to='m_BehaviorState.m_aActions.Contains(null)'},
 @{name='ResetBudget';from='m_Behavior.m_iShortAttempts++;';to='m_Behavior.m_iShortAttempts = 0;'},
 @{name='WeakActor';from='|| !actor || !seat || trip != m_Trip';to='|| !seat || trip != m_Trip'}
)
$caught=0
foreach($m in $unsafe){
 if(!$replay.Contains($m.from)){throw ('Mutation source absent '+$m.name)}
 $ns='QuotaShortSeatUnsafe'+$m.name
 Add-Type -TypeDefinition ($replay.Replace('QuotaShortSeatReplay',$ns).Replace($m.from,$m.to)) -Language CSharp
 $type=($ns+'.AttemptCases')-as[type];$bad=$false
 try{[void]$type::RunAll()}catch{$bad=$true}
 if(!$bad){throw ('Unsafe commit mutation survived '+$m.name)};$caught++
}
"PASS Quota short seat delegated commit actual_physical_cases=$actual executed_unsafe_mutants=$caught native=NOT_RUN"
