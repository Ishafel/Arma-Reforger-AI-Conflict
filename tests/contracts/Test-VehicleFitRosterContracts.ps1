param([string]$RepositoryRoot=(Split-Path -Parent (Split-Path -Parent $PSScriptRoot)))
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot '../lib/Stage3StaticAudit.Common.ps1')
$base=Join-Path $RepositoryRoot 'AIConflictCore/Scripts/Game/AIConflict'
$trip=[IO.File]::ReadAllText((Join-Path $base 'State/Vehicles/AICF_TransportTrip.c'))
$flow=[IO.File]::ReadAllText((Join-Path $base 'Vehicles/AICF_VehicleBehaviorFlow.c'))
function Body([string]$source,[string]$name){Get-AICFMethodBody ([pscustomobject]@{Source=$source;Code=(ConvertTo-AICFCodeText $source)}) $name}
function Rules([string]$t,[string]$f){
 $current=Body $t 'IsVehiclePresetCurrent';$fill=Body $f 'FillSurvivorCargo';$check=Body $f 'CheckFitGroupAssignments';$poll=Body $f 'PollReboarding';$restore=Body $f 'RestoreCrewAssignments'
 foreach($guard in @('!m_Assignment','m_aVehiclePresetMembers.Count() != AICF_Stage1Config.MAX_GROUP_SIZE','m_aVehiclePresetMemberIds.Count() != AICF_Stage1Config.MAX_GROUP_SIZE','for (int index; index < AICF_Stage1Config.MAX_GROUP_SIZE; index++)','VehiclePresetMember(index)','m_VehiclePresetSlot.GetRosterMember(index)','if (AICF_GroupRuntime.IsAliveCharacter(currentMember)) return false;','currentMember != member')){if(!$current.Contains($guard)){'FULL_CAPTURED_IDENTITY';break}}
 if($current.Contains('m_VehiclePreset.m_aMembers')){'EFFECTIVE_ASSIGNMENT_CANNOT_HIDE_IDENTITY'}
 foreach($guard in @('if (!IsCurrent(trip)) return false;','for (int position; position < AICF_Stage1Config.MAX_GROUP_SIZE; position++)','if (!IsFit(actor)) continue;','FindCapturedPosition(trip, actor) != position','if (preset.m_aMembers.Contains(position)) continue;','for (int index; index < seats.Count(); index++)','seat.GetType() != ECompartmentType.CARGO || IsCrewSeat(seat) || seat.GetOccupant()','IsFit(trip.VehiclePresetMember(preset.m_aMembers[index]))','cargo = index;','if (cargo < 0)','!trip.ReassignVehiclePresetSeat(cargo, position)','m_iSettledPolls = 0','CheckFitGroupAssignments(trip, seats, false, reason)')){if(!$fill.Contains($guard)){'DETERMINISTIC_ORIGINAL_SURVIVOR_CARGO_OR_FALLBACK';break}}
 if(!$restore.Contains('return FillSurvivorCargo(trip, seats, reason);')){'CREW_THEN_SURVIVORS'}
 foreach($guard in @('if (!IsCurrent(trip)) return false;','group.GetAgents(agents);','if (!agent) return false;','!AICF_GroupRuntime.IsAliveCharacter(actor) || !SCR_AIDamageHandling.IsConscious(actor)','agent.GetParentGroup() != group || !IsFit(actor)','int position = FindCapturedPosition(trip, actor);','position < 0 || !HasUniqueSeatAssignment','seat.GetOccupant() != actor || !access || !access.IsInCompartment() || access.GetCompartment() != seat','access.IsGettingIn() || access.IsGettingOut() || access.IsSwitchingSeatsAnim()','CompartmentAccessComponent.GetVehicleIn(actor) != trip.GetLease().GetVehicle()','if (fit == 0) return false;')){if(!$check.Contains($guard)){'ACTUAL_FIT_GROUP_EXACT_UNIQUE_SETTLED';break}}
 if(!$poll.Contains('if (!FillSurvivorCargo(trip, seats, fitReason)) return AICF_TripOutcome.FallbackToFoot(fitReason, causationId);') -or $poll.IndexOf('FillSurvivorCargo') -gt $poll.IndexOf('bool settled') -or !$poll.Contains('CheckFitGroupAssignments(trip, seats, true, fitReason)') -or $poll.IndexOf('CheckFitGroupAssignments') -lt $poll.IndexOf('CancelEntries(trip);',$poll.IndexOf('state.m_iSettledPolls++'))){'FILL_DURING_REBOARD_AND_FINAL_ROSTER_ORACLE'}
 if((ConvertTo-AICFCodeText ($fill+$check)) -match 'SetHealth|SetDamage|Heal|Resurrect|Spawn|Teleport|SetOrigin|m_iReboardDeadline\s*=|m_iRetries\s*=|m_iShortAttempts\s*=|GetVehiclePresetSlot\(\)\.'){'NO_HEAL_SPAWN_RESTART_OR_LIBRARY_MUTATION'}
 if($poll -match 'm_iReboardDeadline\s*='){'REBOARD_DEADLINE_UNCHANGED'}
}
$issues=@(Rules $trip $flow);if($issues.Count){throw "Fit roster contracts: $issues"}
$checks=1
foreach($mutation in @(
 @('t','for (int index; index < AICF_Stage1Config.MAX_GROUP_SIZE; index++)','for (int index; index < 1; index++)','FULL_CAPTURED_IDENTITY'),
 @('t','m_aVehiclePresetMembers.Count() != AICF_Stage1Config.MAX_GROUP_SIZE','false','FULL_CAPTURED_IDENTITY'),
 @('t','if (AICF_GroupRuntime.IsAliveCharacter(currentMember)) return false;','if (false) return false;','FULL_CAPTURED_IDENTITY'),
 @('f','FindCapturedPosition(trip, actor) != position','false','DETERMINISTIC_ORIGINAL_SURVIVOR_CARGO_OR_FALLBACK'),
 @('f','seat.GetType() != ECompartmentType.CARGO || IsCrewSeat(seat) || seat.GetOccupant()','false','DETERMINISTIC_ORIGINAL_SURVIVOR_CARGO_OR_FALLBACK'),
 @('f','if (cargo < 0)','if (false)','DETERMINISTIC_ORIGINAL_SURVIVOR_CARGO_OR_FALLBACK'),
 @('f','return FillSurvivorCargo(trip, seats, reason);','return true;','CREW_THEN_SURVIVORS'),
 @('f','position < 0 || !HasUniqueSeatAssignment','false && !HasUniqueSeatAssignment','ACTUAL_FIT_GROUP_EXACT_UNIQUE_SETTLED'),
 @('f','access.IsGettingIn() || access.IsGettingOut() || access.IsSwitchingSeatsAnim()','false','ACTUAL_FIT_GROUP_EXACT_UNIQUE_SETTLED'),
 @('f','if (!FillSurvivorCargo(trip, seats, fitReason)) return AICF_TripOutcome.FallbackToFoot(fitReason, causationId);','','FILL_DURING_REBOARD_AND_FINAL_ROSTER_ORACLE'),
 @('f','CheckFitGroupAssignments(trip, seats, true, fitReason)','CheckFitGroupAssignments(trip, seats, false, fitReason)','FILL_DURING_REBOARD_AND_FINAL_ROSTER_ORACLE')
)){
 $t=$trip;$f=$flow
 if($mutation[0] -eq 't'){$t=$t.Replace($mutation[1],$mutation[2]);if($t -ceq $trip){throw 'Trip mutation absent'}}else{$f=$f.Replace($mutation[1],$mutation[2]);if($f -ceq $flow){throw 'Flow mutation absent'}}
 if($mutation[3] -notin @(Rules $t $f)){throw "Fit roster mutation survived: $($mutation[3])"};$checks++
}
# Actual production identity body и unique-seat predicate; адаптеры заменяют native types.
$identity=(Body $trip 'IsVehiclePresetCurrent').Replace('!m_Assignment','m_Assignment == null').Replace('!m_VehiclePresetSlot ||','m_VehiclePresetSlot == null ||').Replace('!m_VehiclePreset)','m_VehiclePreset == null)').Replace('int index;','int index = 0;')
$unique=(Body $flow 'HasUniqueSeatAssignment').Replace('int count;','int count = 0;').Replace('int assigned : members','int assigned in members')
$fill=(Body $flow 'FillSurvivorCargo').Replace('int position;','int position = 0;').Replace('int index;','int index = 0;').Replace('AICF_VehiclePreset preset','AICF_TestPreset preset').Replace('seat.GetOccupant() ||','seat.GetOccupant() != null ||').Replace('CheckFitGroupAssignments(trip, seats, false, reason)','CheckFitGroupAssignments(trip, seats, false, out reason)')
$code=@'
using System.Collections.Generic;
public class IEntity { public bool Alive=true; public bool Authority=true; public int Id; public int GetID(){return Id;} }
public class AICF_Stage1Config { public const int MAX_GROUP_SIZE=4; }
public class AICF_GroupRuntime { public static bool IsAliveCharacter(IEntity e){return e!=null && e.Alive;} }
public class AICF_VehicleBoardingMutationFence { public static bool IsAuthoritativeAIEntity(IEntity e){return e!=null && e.Alive && e.Authority;} }
public class NativeArray<T> : List<T> { public new int Count(){return base.Count;} }
public enum ECompartmentType { PILOT, TURRET, CARGO }
public class BaseCompartmentSlot { public ECompartmentType Type; public bool Crew; public IEntity Occupant; public new ECompartmentType GetType(){return Type;} public IEntity GetOccupant(){return Occupant;} }
public class AICF_VehiclePreset { public static string SeatKey(BaseCompartmentSlot s){return "SEAT";} }
public class AICF_TestPreset { public NativeArray<int> m_aMembers=new NativeArray<int>(); }
public class AICF_TestBehaviorState { public int m_iSettledPolls=1; }
public class AICF_TestSlot { public IEntity[] Roster=new IEntity[4]; public object Group=new object(); public int Revision=7; public int Generation=1; public int GetVehiclePresetRevision(){return Revision;} public object GetGroup(){return Group;} public int GetSpawnGeneration(){return Generation;} public bool HasVehiclePreset(){return true;} public IEntity GetRosterMember(int i){return Roster[i];} }
public class AICF_TestAssignment { public object Group; public object GetGroup(){return Group;} }
public class AICF_IdentityTrip {
 public AICF_TestSlot m_VehiclePresetSlot=new AICF_TestSlot(); public AICF_TestAssignment m_Assignment=new AICF_TestAssignment(); public AICF_TestPreset m_VehiclePreset=new AICF_TestPreset(); public int m_iVehiclePresetRevision=7; public AICF_TestBehaviorState State=new AICF_TestBehaviorState();
 public NativeArray<IEntity> m_aVehiclePresetMembers=new NativeArray<IEntity>(); public NativeArray<int> m_aVehiclePresetMemberIds=new NativeArray<int>();
 public AICF_IdentityTrip(){m_Assignment.Group=m_VehiclePresetSlot.Group; for(int i=0;i<4;i++){var e=new IEntity{Id=i+10};m_VehiclePresetSlot.Roster[i]=e;m_aVehiclePresetMembers.Add(e);m_aVehiclePresetMemberIds.Add(e.Id);}}
 public int GetGroupGeneration(){return 1;}
 public IEntity VehiclePresetMember(int i){if(i<0 || i>=m_aVehiclePresetMembers.Count())return null;var e=m_aVehiclePresetMembers[i];return e!=null && e.Id==m_aVehiclePresetMemberIds[i]?e:null;}
 public bool IsVehiclePresetCurrent() IDENTITY
 public static bool HasUniqueSeatAssignment(List<int> members,int position) UNIQUE
 public AICF_TestPreset GetVehiclePreset(){return m_VehiclePreset;}
 public AICF_TestBehaviorState GetBehaviorState(){return State;}
 public bool ReassignVehiclePresetSeat(int i,int p){for(int j=0;j<m_VehiclePreset.m_aMembers.Count();j++)if(m_VehiclePreset.m_aMembers[j]==p)m_VehiclePreset.m_aMembers[j]=-1;m_VehiclePreset.m_aMembers[i]=p;return true;}
 public static bool IsCurrent(AICF_IdentityTrip trip){return trip.IsVehiclePresetCurrent();}
 public static bool IsFit(IEntity actor){return AICF_VehicleBoardingMutationFence.IsAuthoritativeAIEntity(actor);}
 public static int FindCapturedPosition(AICF_IdentityTrip trip,IEntity actor){return trip.m_aVehiclePresetMembers.IndexOf(actor);}
 public static bool IsCrewSeat(BaseCompartmentSlot seat){return seat.Crew || seat.Type!=ECompartmentType.CARGO;}
 public static void Report(AICF_IdentityTrip trip,string name,string details){}
 public static bool CheckFitGroupAssignments(AICF_IdentityTrip trip,NativeArray<BaseCompartmentSlot> seats,bool settled,out string reason){reason="READY";return true;}
 public static bool FillSurvivorCargo(AICF_IdentityTrip trip,NativeArray<BaseCompartmentSlot> seats,out string reason) FILL
}
'@
Add-Type -TypeDefinition ($code.Replace('IDENTITY',$identity).Replace('UNIQUE',$unique).Replace('FILL',$fill))
function IdentityCase([AICF_IdentityTrip]$value,[bool]$expected,[string]$name){if($value.IsVehiclePresetCurrent() -ne $expected){throw "Production identity failed: $name"};$script:checks++}
$t=New-Object AICF_IdentityTrip;IdentityCase $t $true 'original full roster'
$t.m_aVehiclePresetMembers[0].Alive=$false;$t.m_VehiclePresetSlot.Roster[0]=$null;IdentityCase $t $true 'dead original removed after role remap'
$t.m_VehiclePresetSlot.Roster[0]=New-Object IEntity;IdentityCase $t $false 'paid replacement on former pilot position'
$t.m_VehiclePresetSlot.Roster[0]=$t.m_aVehiclePresetMembers[0];$t.m_aVehiclePresetMembers[0].Alive=$true;IdentityCase $t $true 'original wakes after role reassignment'
$t.m_VehiclePresetSlot.Roster[3]=New-Object IEntity;IdentityCase $t $false 'new actor on any captured position'
$t=New-Object AICF_IdentityTrip;$t.m_aVehiclePresetMembers[3].Id=12345;IdentityCase $t $false 'captured entity identity changed'
$t=New-Object AICF_IdentityTrip;$t.m_aVehiclePresetMembers[0].Authority=$false;IdentityCase $t $false 'foreign possession'
$t=New-Object AICF_IdentityTrip;$t.m_aVehiclePresetMemberIds.RemoveAt(3);IdentityCase $t $false 'capture shape changed'
$t=New-Object AICF_IdentityTrip;$t.m_Assignment=$null;IdentityCase $t $false 'assignment gone'
foreach($case in @(@(@(1,2,-1),0,$false),@(@(1,0,2),0,$true),@(@(0,1,0),0,$false),@(@(-1,1,2),-1,$false),@(@(1,2,3),3,$true))){
 $members=New-Object 'System.Collections.Generic.List[int]';foreach($n in $case[0]){$members.Add($n)}
 if([AICF_IdentityTrip]::HasUniqueSeatAssignment($members,$case[1]) -ne $case[2]){throw 'Production unique-seat predicate failed'};$checks++
}
function CargoCase([int[]]$assignment,[int[]]$expected,[bool]$want,[int[]]$occupied=@(),[int[]]$crew=@()){
 $t=New-Object AICF_IdentityTrip;$seats=New-Object 'NativeArray[BaseCompartmentSlot]'
 for($i=0;$i -lt 4;$i++){
  $t.m_VehiclePreset.m_aMembers.Add($assignment[$i]);$s=New-Object BaseCompartmentSlot;$s.Type=[ECompartmentType]::CARGO
  if($i -eq 0){$s.Type=[ECompartmentType]::PILOT};if($crew -contains $i){$s.Crew=$true};if($occupied -contains $i){$s.Occupant=New-Object IEntity};$seats.Add($s)
 }
 $reason='';$got=[AICF_IdentityTrip]::FillSurvivorCargo($t,$seats,[ref]$reason)
 if($got -ne $want -or ($t.m_VehiclePreset.m_aMembers -join ',') -ne ($expected -join ',')){throw "Production cargo fill failed: got=$got assignment=$($t.m_VehiclePreset.m_aMembers) reason=$reason"}
 if($got -and $assignment -contains -1 -and $t.State.m_iSettledPolls -ne 0){throw 'Cargo assignment reused settled receipt'};$script:checks++
}
CargoCase @(1,2,3,-1) @(1,2,3,0) $true
CargoCase @(2,3,-1,-1) @(2,3,0,1) $true
CargoCase @(1,2,3,-1) @(1,2,3,-1) $false @(3)
CargoCase @(2,3,-1,-1) @(2,3,-1,0) $false @(2)
CargoCase @(1,2,3,-1) @(1,2,3,-1) $false @() @(3)
CargoCase @(0,1,2,3) @(0,1,2,3) $true
"PASS Fit roster contracts checks=$checks identity_unique_and_cargo_fill=EXECUTED physical_wake_native=NOT_RUN"
