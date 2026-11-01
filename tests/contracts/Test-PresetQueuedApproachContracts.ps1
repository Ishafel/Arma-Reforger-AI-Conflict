param([string]$RepositoryRoot=(Split-Path -Parent (Split-Path -Parent $PSScriptRoot)))
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot '../lib/Stage3StaticAudit.Common.ps1')
$path=Join-Path $RepositoryRoot 'AIConflictCore/Scripts/Game/AIConflict/Vehicles/AICF_PresetQueuedApproach.c'
$source=[IO.File]::ReadAllText($path)
function Body([string]$text,[string]$name){Get-AICFMethodBody ([pscustomobject]@{Source=$text;Code=(ConvertTo-AICFCodeText $text)}) $name}
$issue=Body $source 'ShouldIssue'
$guard=Body $source 'IsCurrent'
$guard=$guard.Replace('!m_Trip ||','m_Trip == null ||').Replace('!m_State ||','m_State == null ||').Replace('!m_State.GetTokens()','m_State.GetTokens() == null').Replace('ChimeraCharacter.Cast(m_Actor)','m_Actor').Replace('ChimeraCharacter actor','Actor actor').Replace('CompartmentAccessComponent access','Access access').Replace('!actor ||','actor == null ||').Replace('return access &&','return access != null &&').Replace('!CompartmentAccessComponent.GetVehicleIn(actor)','CompartmentAccessComponent.GetVehicleIn(actor) == null').Replace('System.GetTickCount()','Clock.Now')
$shim=@"
using System;
public static class Clock { public static int Now=100; }
public enum AICF_ETransportTripPhase { BOARDING, OTHER }
public static class Replication { public static bool Server=true; public static bool IsServer(){return Server;} }
public class Access { public bool Inside,In,Out,Switch; public bool IsInCompartment(){return Inside;} public bool IsGettingIn(){return In;} public bool IsGettingOut(){return Out;} public bool IsSwitchingSeatsAnim(){return Switch;} }
public class Actor { public bool Conscious=true,Inside; public object Linked; public Access Access=new Access(); public bool IsInVehicle(){return Inside;} public Access GetCompartmentAccessComponent(){return Access;} }
public static class SCR_AIDamageHandling { public static bool IsConscious(Actor a){return a.Conscious;} }
public static class CompartmentAccessComponent { public static object GetVehicleIn(Actor a){return a.Linked;} }
public class Token { public bool Queued,MatchesResult=true,Safe=true; public object Agent=new object(); public bool WasPresetQueued(){return Queued;} public bool Matches(Trip t,object l,object g){return MatchesResult;} public object GetAgent(){return Agent;} public bool IsExactCompartmentMutationSafe(){return Safe;} }
public class Tokens { public Token Found; public Token FindByAgent(object a){return Found;} }
public class State { public bool Exited; public int Deadline=1000; public Tokens Tokens=new Tokens(); public bool AreExitEffectsApplied(){return Exited;} public int GetAbsoluteDeadlineMs(){return Deadline;} public Tokens GetTokens(){return Tokens;} }
public class Trip { public bool Valid=true,Terminal,Current=true,Expired; public object Lease; public State State; public AICF_ETransportTripPhase Phase=AICF_ETransportTripPhase.BOARDING; public bool IsValid(){return Valid;} public bool IsTerminal(){return Terminal;} public bool IsVehiclePresetCurrent(){return Current;} public AICF_ETransportTripPhase GetPhase(){return Phase;} public object GetLease(){return Lease;} public State GetBoardingState(){return State;} public bool IsDeadlineReached(int n){return Expired;} }
public class Scope {
 const float RADIUS_METERS=8;
 public bool m_bStopped,OwnerSafe=true;
 public Trip m_Trip=new Trip(); public object m_Lease=new object(),m_Group=new object(); public State m_State=new State(); public Token m_Token=new Token(); public Actor m_Actor=new Actor();
 public bool IsOwnerSafe(){return OwnerSafe;}
 public bool IsCurrent() $guard
 public static bool ShouldIssue(bool current,bool issued,float distanceMeters,int nowMs,int deadlineMs) $issue
 public Scope(){m_Trip.Lease=m_Lease;m_Trip.State=m_State;m_State.Tokens.Found=m_Token;}
 public static bool Case(int c){var s=new Scope();Replication.Server=true;Clock.Now=100;
  switch(c){
   case 0:return s.IsCurrent();
   case 1:s.m_bStopped=true;break; case 2:Replication.Server=false;break;
   case 3:s.m_Trip=null;break; case 4:s.m_Trip.Valid=false;break; case 5:s.m_Trip.Terminal=true;break;
   case 6:s.m_Trip.Phase=AICF_ETransportTripPhase.OTHER;break; case 7:s.m_Trip.Current=false;break;
   case 8:s.m_Trip.Lease=new object();break; case 9:s.m_Trip.State=new State();break;
   case 10:s.m_State=null;s.m_Trip.State=null;break; case 11:s.m_State.Exited=true;break;
   case 12:s.m_Trip.Expired=true;break; case 13:s.m_State.Deadline=100;break;
   case 14:s.OwnerSafe=false;break; case 15:s.m_Token.Queued=true;break; case 16:s.m_Token.MatchesResult=false;break;
   case 17:s.m_State.Tokens=null;break; case 18:s.m_State.Tokens.Found=new Token();break;
   case 19:s.m_Token.Safe=false;break; case 20:s.m_Actor=null;break; case 21:s.m_Actor.Conscious=false;break;
   case 22:s.m_Actor.Access=null;break; case 23:s.m_Actor.Inside=true;break; case 24:s.m_Actor.Linked=new object();break;
   case 25:s.m_Actor.Access.Inside=true;break; case 26:s.m_Actor.Access.In=true;break;
   case 27:s.m_Actor.Access.Out=true;break; case 28:s.m_Actor.Access.Switch=true;break;
   default:throw new Exception("Unknown case");
  } return !s.IsCurrent();
 }
}
"@
Add-Type -TypeDefinition $shim
$checks=0
function Check([bool]$ok,[string]$rule){if(!$ok){throw "Preset queued approach: $rule"};$script:checks++}
for($case=0;$case -le 28;$case++){Check ([Scope]::Case($case)) "ACTUAL_SCOPE_$case"}
Check ([Scope]::ShouldIssue($true,$false,70,100,1000)) 'DISTANT_UNQUEUED_NATIVE_WALK'
Check (![Scope]::ShouldIssue($false,$false,70,100,1000)) 'STALE_REJECTED'
Check (![Scope]::ShouldIssue($true,$true,70,100,1000)) 'ONE_NATIVE_ACTION'
foreach($distance in @(-1,0,7.99,8,[float]::NaN)){Check (![Scope]::ShouldIssue($true,$false,$distance,100,1000)) 'NEAR_OR_UNKNOWN_NO_ACTION'}
Check (![Scope]::ShouldIssue($true,$false,70,-1,1000)) 'INVALID_CLOCK'
Check (![Scope]::ShouldIssue($true,$false,70,100,100)) 'EXACT_DEADLINE'
Check (![Scope]::ShouldIssue($true,$false,70,101,100)) 'PAST_DEADLINE'
Check ([Scope]::ShouldIssue($true,$false,8.01,100,101)) 'OPEN_BOUNDARY_NO_EXTENSION'
$owner=Body $source 'IsOwnerSafe'
foreach($required in @('m_Actor.GetID() != m_ActorId','ComponentRpl(m_Actor) != m_sActorRpl','m_Token.GetReservedEntity() != m_Actor','GetControlAIAgent() == m_Token.GetAgent()','m_Token.GetAgent().GetParentGroup() == m_Group','m_Token.IsPhysicalMutationOwnerSafe()')){Check ($owner.Contains($required)) "OWNER_$required"}
Check ($source.Contains('new SCR_AIMoveIndividuallyBehavior(') -and !$source.Contains('new SCR_AIGetInVehicle(')) 'FOOT_ACTION_ONLY'
Check ($source -notmatch '\.(Teleport|SetOrigin|SetWorldTransform|SetHealth|SetReserved)\(') 'NO_PHYSICAL_OR_SEAT_MUTATION'
Check ((Body $source 'Stop').Contains('m_Token.IsActionOwnedByUtility(m_Action)') -and (Body $source 'Stop').Contains('m_Action.Fail()')) 'OWNER_ONLY_RETIREMENT'
$flow=[IO.File]::ReadAllText((Join-Path $RepositoryRoot 'AIConflictCore/Scripts/Game/AIConflict/Vehicles/AICF_VehicleBoardingFlow.c'))
$tick=Body $flow 'TickPreset'
Check ($tick -match '(?s)if \(!token.WasPresetQueued\(\)\).*?MaintainPresetQueueApproach\(trip, lease, token\)') 'PREQUEUE_APPROACH'
$queue=Body $flow 'QueuePresetSeat'
Check ($queue.IndexOf('RetirePresetQueueApproach(token)') -ge 0 -and $queue.IndexOf('RetirePresetQueueApproach(token)') -lt $queue.IndexOf('token.RecordPresetQueued(')) 'RETIRE_BEFORE_ENTRY'
Check (@([regex]::Matches($queue,'IsPresetQueueContextCurrent\(trip, state, token\)')).Count -eq 2 -and $queue -match '(?s)RetirePresetQueueApproach\(token\).*?IsPresetQueueContextCurrent\(trip, state, token\).*?utility.AddAction') 'RECHECK_AFTER_NATIVE_CANCEL'
Check ($queue -match '(?s)RetirePresetQueueApproach\(token\).*?ResolveOwnedUtility\(token.GetAgent\(\), member\).*?currentUtility != utility') 'RE_RESOLVE_SAME_UTILITY_AFTER_CALLBACK'
$eligibility=Body $flow 'IsPresetQueueContextCurrent'
foreach($required in @('trip.IsVehiclePresetCurrent()','trip.IsTerminal()','AICF_ETransportTripPhase.BOARDING','trip.GetBoardingState() != state','state.AreExitEffectsApplied()','state.ArePresetQueueActionsChanging()','tokens.FindByAgent(token.GetAgent()) == token','GetControlAIAgent() == token.GetAgent()')){Check ($eligibility.Contains($required)) "REPEATED_CONTEXT_$required"}
$states=[IO.File]::ReadAllText((Join-Path $RepositoryRoot 'AIConflictCore/Scripts/Game/AIConflict/State/Vehicles/AICF_VehiclePhaseStates.c'))
$start=$states.IndexOf('class AICF_VehicleBoardingState')
$end=$states.IndexOf('class AICF_VehicleMovementState',$start)
$boarding=$states.Substring($start,$end-$start)
foreach($method in @('Reset','ApplyExitEffectsOwnerSafe')){Check ((Body $boarding $method).Contains('ClearPresetQueueApproaches();')) "LIFECYCLE_$method"}
$retire=Body $boarding 'RetirePresetQueueApproach'
Check ($retire.IndexOf('m_aPresetQueueApproaches.Remove(index)') -lt $retire.IndexOf('pending.Stop()')) 'DETACH_MATCHES_BEFORE_NATIVE_CALLBACK'
$clear=Body $boarding 'ClearPresetQueueApproaches'
Check ($clear.IndexOf('m_aPresetQueueApproaches.Clear()') -lt $clear.IndexOf('approach.Stop()')) 'DETACH_ALL_BEFORE_NATIVE_CALLBACK'
foreach($method in @('RetirePresetQueueApproach','ClearPresetQueueApproaches')){Check ((Body $boarding $method).Contains('m_bPresetQueueActionsChanging = wasChanging;')) "RESTORE_NESTED_MUTATION_GUARD_$method"}
Write-Output "PASS Preset queued approach: $checks actual scope/boundary/lifecycle controls; native walking still requires runtime proof"
