param([string]$RepositoryRoot=(Split-Path -Parent (Split-Path -Parent $PSScriptRoot)))
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot '../lib/Stage3StaticAudit.Common.ps1')
$root=Join-Path $RepositoryRoot 'AIConflictCore/Scripts/Game/AIConflict'
$policy=[IO.File]::ReadAllText((Join-Path $root 'Vehicles/AICF_VehicleCrewRoleOwnership.c'))
$flow=[IO.File]::ReadAllText((Join-Path $root 'Vehicles/AICF_VehicleTransitFlow.c'))
function Body([string]$text,[string]$name){Get-AICFMethodBody ([pscustomobject]@{Source=$text;Code=(ConvertTo-AICFCodeText $text)}) $name}
function Rules([string]$proof,[string]$source){
 $own=Body $proof 'IsCurrentExactPresetOccupant'
 $observe=Body $source 'ObserveExactOwnedCrewRole'
 $start=Body $source 'StartCrewRecovery'
 $token=Body $source 'ProcessCrewRecoveryToken'
 if(!$own -or !$observe -or !$start -or !$token){'METHODS';return}
 foreach($guard in @('!Replication.IsServer()','!trip.IsValid()','trip.IsTerminal()',
  'trip.GetPhase() != AICF_ETransportTripPhase.TRANSIT','trip.IsDeadlineReached(nowMs)',
  '!trip.GetVehiclePreset()','!trip.IsVehiclePresetCurrent()',
  'slot.GetGroup() != assignment.GetGroup()','slot.GetSpawnGeneration() != trip.GetGroupGeneration()',
  'slot.GetStrategicAssignmentRevision() != assignment.GetAssignmentRevision()',
  'slot.GetStrategicIntentRevision() != assignment.GetStrategicIntentRevision()')){
  if(!$own.Contains($guard)){'CURRENT_PRESET_CONTEXT'}
 }
 if($own -notmatch '!lease.MatchesTripIdentity\(' -or
  $own -notmatch '!lease.MatchesEntityIdentity\(vehicle, vehicle.GetID\(\), rpl.Id\(\).ToString\(\)\)' -or
  $own -notmatch '!AICF_VehicleBoardingMutationFence.IsAuthoritativeReplicatedEntity\(group\)' -or
  $own -notmatch '!AICF_VehicleBoardingMutationFence.IsAuthoritativeReplicatedEntity\(vehicle\)'){'CURRENT_AUTHORITY_LEASE'}
 foreach($guard in @('seat.GetType() != ECompartmentType.PILOT','seat.GetType() != ECompartmentType.TURRET',
  '!preset.MatchesSeats(seats)','int index = seats.Find(seat)','int position = preset.m_aMembers[index]',
  'trip.VehiclePresetMember(position) != member','slot.GetRosterMember(position) != member')){
  if(!$own.Contains($guard)){'EXACT_ROLE_AND_IMMUTABLE_MEMBER'}
 }
 if($own -notmatch '!AICF_VehicleBoardingMutationFence.IsAuthoritativeAIEntity\(member\)' -or
  $own -notmatch '!AICF_GroupRuntime.IsAliveCharacter\(member\)' -or
  $own -notmatch '!SCR_AIDamageHandling.IsConscious\(member\)' -or
  $own -notmatch 'agent.GetParentGroup\(\) == group && agent.GetControlledEntity\(\) == member' -or
  $own -notmatch 'utility.m_OwnerEntity == member'){'CURRENT_AI_NO_HUMAN_OR_OTHER_GROUP'}
 foreach($guard in @('member.IsInVehicle()','access.IsInCompartment()','!access.IsSwitchingSeatsAnim()',
  'access.GetCompartment() == seat','CompartmentAccessComponent.GetVehicleIn(member) == vehicle')){
  if(!$own.Contains($guard)){'EXACT_PHYSICAL_ROLE'}
 }
 if($observe -notmatch 'AICF_VehicleCrewRoleOwnership.IsCurrentExactPresetOccupant\(trip, seat, role, nowMs\)' -or
  $observe -notmatch 'ObserveCrewRoleSettledLoss\(trip, snapshot, causationId, nowMs\)' -or
  $observe -notmatch 'Wait\("EXACT_OWN_CREW_ROLE_SETTLED_RECHECK"' -or
  $observe -match 'BeginCrewRecovery|ClearCrewRecovery|TrackCrewRecovery|SetReserved|m_i\w+\s*='){'EXISTING_BOUNDED_PASSIVE_OBSERVER'}
 $ownAt=$start.IndexOf('ObserveExactOwnedCrewRole(')
 $foreignAt=$start.IndexOf('"CREW_ROLE_OCCUPIED_BY_FOREIGN_ENTITY"')
 $budgetAt=$start.IndexOf('state.BeginCrewRecovery()')
 if($ownAt -lt 0 -or $foreignAt -le $ownAt -or $budgetAt -le $foreignAt -or
  $start -notmatch 'if \(ownTransition\)\s*return ownTransition;' -or
  $start -notmatch 'if \(occupant && SCR_AIDamageHandling.IsConscious\(occupant\)\)' -or
  $start -notmatch 'VEHICLE_CREW_OCCUPANT_REJECTED'){'OWN_WAIT_BEFORE_ATTEMPT_FOREIGN_FAIL_CLOSED'}
 $expiryAt=$token.IndexOf('!token.IsOwnerValid() || actionState == EAIActionState.FAILED || token.IsDeadlineReached(nowMs)')
 $preemptAt=$token.IndexOf('if (token.GetRole() == EAICompartmentType.Turret')
 $settledAt=$token.IndexOf('if (token.IsExactCompartmentSettled(')
 if($expiryAt -lt 0 -or $preemptAt -le $expiryAt -or $settledAt -le $expiryAt){'TOKEN_INVALIDITY_BEFORE_PASSIVE_WAIT'}
 if($token -notmatch '(?s)AICF_TripOutcome ownPilotTransition = ObserveExactOwnedCrewRole\(.*?if \(ownPilotTransition\)\s*return ownPilotTransition;\s*token.CancelOwnerSafe\(\);\s*state.ClearCrewRecoveryToken\(token\);\s*return StartCrewRecovery' -or
  $token -match 'new AICF_VehicleCrewRecoveryToken|TrackCrewRecoveryToken|BeginCrewRecovery|SetLastPhysicalMotionAtMs|Set.*Deadline'){'PRESERVE_EXISTING_TOKEN_WHILE_WAITING'}
 if($proof -match 'SetOrigin|Teleport|CallLater|SetReserved|AddAction|\bFail\(|ClearCrew|BeginCrew|\bReset\w*\(|DeadlineMs\s*='){'READ_ONLY_OWNERSHIP_PROOF'}
}
$errors=@(Rules $policy $flow)
if($errors.Count){throw "Own crew transition: $errors"}
# Выполняем извлечённый production predicate непрерывной потери settled-state.
$observer=Body $flow 'ObserveCrewRoleSettledLoss'
$terminal=[regex]::Match($observer,'polls >= CREW_SETTLED_LOSS_MIN_POLLS &&\s*ageMs >= CREW_SETTLED_LOSS_GRACE_MS').Value
$pollLimit=[int]([regex]::Match($flow,'CREW_SETTLED_LOSS_MIN_POLLS = (\d+)').Groups[1].Value)
$grace=[int]([regex]::Match($flow,'CREW_SETTLED_LOSS_GRACE_MS = (\d+)').Groups[1].Value)
$terminal=$terminal.Replace('CREW_SETTLED_LOSS_MIN_POLLS','$pollLimit').Replace('CREW_SETTLED_LOSS_GRACE_MS','$grace').Replace('polls','$polls').Replace('ageMs','$ageMs').Replace('>=','-ge').Replace('&&','-and')
$check=[scriptblock]::Create('param($polls,$ageMs,$pollLimit,$grace) '+$terminal)
$cases=@(@(1,0,$false),@(2,3000,$false),@(3,2999,$false),@(3,3000,$true),@(30,30000,$true))
foreach($case in $cases){if([bool](& $check $case[0] $case[1] $pollLimit $grace) -ne $case[2]){throw 'Continuous transition deadline failed'}}
if($pollLimit -ne 3 -or $grace -ne 3000 -or $observer -notmatch 'FallbackToFoot\(reason, causationId\)'){throw 'Existing 3s transition bound changed'}
# Выполняем guard существующего token: expiry/owner/failure приоритетнее own wait.
$token=Body $flow 'ProcessCrewRecoveryToken'
$invalid=[regex]::Match($token,'!token.IsOwnerValid\(\) \|\| actionState == EAIActionState.FAILED \|\| token.IsDeadlineReached\(nowMs\)').Value
$invalid=$invalid.Replace('!token.IsOwnerValid()','!$valid').Replace('actionState == EAIActionState.FAILED','$failed').Replace('token.IsDeadlineReached(nowMs)','$expired').Replace('||','-or').Replace('!','-not ')
$checkToken=[scriptblock]::Create('param($valid,$failed,$expired) '+$invalid)
$tokenCases=@(@($true,$false,$false,$false),@($false,$false,$false,$true),@($true,$true,$false,$true),@($true,$false,$true,$true))
foreach($case in $tokenCases){if([bool](& $checkToken $case[0] $case[1] $case[2]) -ne $case[3]){throw 'Token must reject expiry/invalid owner/FAILED before own transition'}}
$controls=1
foreach($mutation in @(
 @('!trip.IsVehiclePresetCurrent()','false','CURRENT_PRESET_CONTEXT'),
 @('trip.IsDeadlineReached(nowMs)','false','CURRENT_PRESET_CONTEXT'),
 @('slot.GetSpawnGeneration() != trip.GetGroupGeneration()','false','CURRENT_PRESET_CONTEXT'),
 @('!lease.MatchesTripIdentity(','UnsafeLease(','CURRENT_AUTHORITY_LEASE'),
 @('!AICF_VehicleBoardingMutationFence.IsAuthoritativeReplicatedEntity(vehicle)','false','CURRENT_AUTHORITY_LEASE'),
 @('seat.GetType() != ECompartmentType.PILOT','false','EXACT_ROLE_AND_IMMUTABLE_MEMBER'),
 @('seat.GetType() != ECompartmentType.TURRET','false','EXACT_ROLE_AND_IMMUTABLE_MEMBER'),
 @('trip.VehiclePresetMember(position) != member','false','EXACT_ROLE_AND_IMMUTABLE_MEMBER'),
 @('slot.GetRosterMember(position) != member','false','EXACT_ROLE_AND_IMMUTABLE_MEMBER'),
 @('!AICF_VehicleBoardingMutationFence.IsAuthoritativeAIEntity(member)','false','CURRENT_AI_NO_HUMAN_OR_OTHER_GROUP'),
 @('!SCR_AIDamageHandling.IsConscious(member)','false','CURRENT_AI_NO_HUMAN_OR_OTHER_GROUP'),
 @('agent.GetParentGroup() == group','true','CURRENT_AI_NO_HUMAN_OR_OTHER_GROUP'),
 @('access.GetCompartment() == seat','true','EXACT_PHYSICAL_ROLE'),
 @('!access.IsSwitchingSeatsAnim()','true','EXACT_PHYSICAL_ROLE')
)){
 if(!$policy.Contains($mutation[0])){throw "Missing mutation target: $($mutation[0])"}
 if($mutation[2] -notin @(Rules ($policy.Replace($mutation[0],$mutation[1])) $flow)){throw "Mutation survived: $($mutation[2])"}
 $controls++
}
foreach($mutation in @(
 @('if (ownTransition)','if (false)','OWN_WAIT_BEFORE_ATTEMPT_FOREIGN_FAIL_CLOSED'),
 @('if (ownPilotTransition)','if (false)','PRESERVE_EXISTING_TOKEN_WHILE_WAITING'),
 @('!token.IsOwnerValid() || actionState == EAIActionState.FAILED || token.IsDeadlineReached(nowMs)','false','TOKEN_INVALIDITY_BEFORE_PASSIVE_WAIT'),
 @('ObserveCrewRoleSettledLoss(trip, snapshot, causationId, nowMs)','AICF_TripOutcome.Wait("UNBOUNDED", causationId)','EXISTING_BOUNDED_PASSIVE_OBSERVER')
)){
 if(!$flow.Contains($mutation[0])){throw "Missing flow mutation target: $($mutation[0])"}
 if($mutation[2] -notin @(Rules $policy ($flow.Replace($mutation[0],$mutation[1])))){throw "Flow mutation survived: $($mutation[2])"}
 $controls++
}
"PASS Own crew transitions: current exact preset AI/role/lease only, foreign/human fail closed, existing token expiry precedes preemption, passive wait retains token/attempts and 3s bound; controls=$controls continuous_cases=$($cases.Count) token_cases=$($tokenCases.Count)"
exit 0
