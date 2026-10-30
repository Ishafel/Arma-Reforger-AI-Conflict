param([string]$RepositoryRoot=(Split-Path -Parent (Split-Path -Parent $PSScriptRoot)))
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot '../lib/Stage3StaticAudit.Common.ps1')
$core=Join-Path $RepositoryRoot 'AIConflictCore/Scripts/Game/AIConflict'
$helper=[IO.File]::ReadAllText((Join-Path $core 'Vehicles/AICF_VehicleShortRecovery.c'))
$boarding=[IO.File]::ReadAllText((Join-Path $core 'Vehicles/AICF_VehicleBoardingFlow.c'))
$dismount=[IO.File]::ReadAllText((Join-Path $core 'Vehicles/AICF_VehicleDismountFlow.c'))
$tokens=[IO.File]::ReadAllText((Join-Path $core 'State/Vehicles/AICF_VehicleBoardingTokens.c'))
function Body([string]$text,[string]$name){Get-AICFMethodBody ([pscustomobject]@{Source=$text;Code=(ConvertTo-AICFCodeText $text)}) $name}
function Rules([string]$h,[string]$b,[string]$d,[string]$t){
 $identity=Body $h 'IsCurrentMember';$can=Body $h 'CanSeat';$seat=Body $h 'TrySeat';$exit=Body $h 'TryExit'
 $recover=Body $b 'TryShortPresetSeatRecovery';$normal=Body $d 'ProcessNormalDismount';$permit=Body $d 'CanApplyTerminalHiddenRecovery'
 if(!$identity -or !$can -or !$seat -or !$exit -or !$recover -or !$normal -or !$permit){'METHODS';return}
 foreach($guard in @('!Replication.IsServer()', '!trip.IsValid()', '!lease.HasPhysicalAsset()',
  '!AICF_VehicleBoardingMutationFence.IsAuthoritativeReplicatedEntity(group)',
  '!AICF_VehicleBoardingMutationFence.IsAuthoritativeReplicatedEntity(vehicle)',
  '!AICF_VehicleBoardingMutationFence.IsAuthoritativeAIEntity(member)',
  '!lease.MatchesTripIdentity(trip.GetFactionKey(), trip.GetSlotId(), trip.GetGroupGeneration(), trip.GetTripGeneration())',
  '!lease.MatchesEntityIdentity(vehicle, vehicle.GetID(), rpl.Id().ToString())',
  '!SCR_AIDamageHandling.IsConscious(character)', 'agent.GetControlledEntity() != member || agent.GetParentGroup() != group',
  'slot.GetSpawnGeneration() != trip.GetGroupGeneration()', 'slot.GetVehiclePresetRevision() != trip.GetVehiclePresetRevision()',
  'trip.VehiclePresetMember(position) == member && slot.GetRosterMember(position) == member')){
  if(!$identity.Contains($guard)){'EXACT_AI_IDENTITY';break}
 }
 if(!$h.Contains('MAX_ACTOR_DISPLACEMENT_METERS = 25.0;') -or !$h.Contains('MAX_SEAT_HEIGHT_METERS = 5.0;') -or
  !$can.Contains('vector.Distance(origin, destination) > MAX_ACTOR_DISPLACEMENT_METERS') -or
  !$can.Contains('Math.AbsFloat(origin[1] - destination[1]) > MAX_SEAT_HEIGHT_METERS') -or
  @([regex]::Matches($exit,'vector.Distance\(member.GetOrigin\(\), destination\[3\]\) > MAX_ACTOR_DISPLACEMENT_METERS')).Count -ne 3){'SHORT_ACTOR_DISTANCE'}
 $release=$exit.IndexOf('conflict.ReleaseOwnedReservationBeforeExit()')
 $repeat=$exit.LastIndexOf('vector.Distance(member.GetOrigin(), destination[3]) > MAX_ACTOR_DISPLACEMENT_METERS')
 if($release-lt0-or$repeat-lt$release-or$repeat-gt$exit.IndexOf('access.GetOutVehicle_NoDoor(')){'REPEAT_SHORT_DISTANCE_AFTER_RESERVATION_CALLBACK'}
 foreach($guard in @('seat.GetVehicle() != vehicle','!IsSlowVehicle(vehicle)','!seat.IsCompartmentAccessible()',
  'seat.GetOccupant()', 'seat.IsGetInLockedFor(member)', 'seat.IsReserved() && !seat.IsReservedBy(member)',
  '!access.IsGettingIn() && !access.IsGettingOut() && !access.IsSwitchingSeatsAnim()', '!CompartmentAccessComponent.GetVehicleIn(member)')){
  if(!$can.Contains($guard)){'EMPTY_EXACT_SEAT_NO_FOREIGN_MUTATION';break}
 }
 if(@([regex]::Matches($seat,'CanSeat\(trip, member, seat\)')).Count -ne 2 -or
  !$seat.Contains('!seat.IsReservedBy(member)') -or !$seat.Contains('access.GetInVehicle(vehicle, seat, true, -1, ECloseDoorAfterActions.INVALID, false)')){'REPEAT_SEAT_FENCE_BEFORE_NATIVE'}
 if(@([regex]::Matches($exit,'IsCurrentMember\(trip, member\)')).Count -ne 2 -or
  !$exit.Contains('seat.GetOccupant() != member') -or !$exit.Contains('access.GetCompartment() != seat') -or
  !$exit.Contains('CompartmentAccessComponent.GetVehicleIn(member) != vehicle') -or
  !$exit.Contains('ChimeraWorldUtils.TryGetWaterSurfaceSimple(vehicle.GetWorld(), destination[3])') -or
  !$exit.Contains('access.GetOutVehicle_NoDoor(destination, false, false, true)')){'REPEAT_EXACT_EXIT_FENCE'}
 if((ConvertTo-AICFCodeText $h) -match 'CanApplyHiddenRecovery|GetAllPlayers|IsHiddenRecoveryCombatSafe'){'VISIBLE_RECOVERY_ALLOWED'}
 $incap=Body $h 'IsIncapacitatedSeatCurrent';$clear=Body $h 'TryClearIncapacitatedSeat'
 foreach($guard in @('players.GetPlayerIdFromControlledEntity(member) > 0',
  'SCR_PossessingManagerComponent.GetPlayerIdFromMainEntity(member) > 0',
  'controller.GetLifeState() != ECharacterLifeState.INCAPACITATED',
  'controller.GetLifeState() != ECharacterLifeState.DEAD', 'trip.VehiclePresetMember(position) == member',
  '!AICF_VehicleBoardingMutationFence.IsAuthoritativeReplicatedEntity(member)',
  'slot.GetSpawnGeneration() != trip.GetGroupGeneration()', 'slot.GetVehiclePresetRevision() != trip.GetVehiclePresetRevision()')){
  if(!$incap.Contains($guard)){'ONLY_CAPTURED_NONPLAYER_INCAPACITATED';break}
 }
 if(@([regex]::Matches($clear,'IsIncapacitatedSeatCurrent\(trip, seat\)')).Count -ne 2 -or
  !$clear.Contains('seat.GetOccupant() != member') -or !$clear.Contains('access.GetOutVehicle_NoDoor(destination, true, false, true)') -or
  !$clear.Contains('vector.Distance(member.GetOrigin(), destination[3]) > MAX_ACTOR_DISPLACEMENT_METERS')){'BOUNDED_INCAPACITATED_EGRESS'}
 if((ConvertTo-AICFCodeText $h) -match 'SetHealth|SetLifeState|Resurrect|SetDamage|ForceUnconscious|\.Teleport\(|\.SetOrigin\(|SetWorldTransform|CommitTransition|CallLater|SpawnEntity'){'NO_HEAL_RESURRECTION_VEHICLE_RELOCATION_OR_LIFECYCLE'}
 if(!$recover.Contains('token.WasShortRecoveryAttempted()') -or !$recover.Contains('token.WasPresetQueued()') -or
  !$recover.Contains('token.Matches(trip, lease, group)') -or !$recover.Contains('System.GetTickCount() >= state.GetAbsoluteDeadlineMs()') -or
  !$recover.Contains('token.MarkShortRecoveryAttempted();') -or !$t.Contains('m_bShortRecoveryAttempted = true;')){'FINITE_CURRENT_TOKEN_ATTEMPT'}
 if(!$recover.Contains('actionState == EAIActionState.FAILED || actionState == EAIActionState.COMPLETED') -or
  !$recover.Contains('!failed && !disappeared && !stalled') -or !$recover.Contains('token.GetProgressAgeMs() >= 5000') -or
  $recover -match 'GetProgressAgeMs\(\) <|DeadlineMs\s*=|\.Begin\('){'IMMEDIATE_FAILURE_NO_DEADLINE_EXTENSION'}
 if($recover.IndexOf('token.CancelOwnerSafe();') -lt 0 -or $recover.IndexOf('token.CancelOwnerSafe();') -gt $recover.IndexOf('token.MarkShortRecoveryAttempted();') -or
  $recover.IndexOf('token.MarkShortRecoveryAttempted();') -gt $recover.IndexOf('AICF_VehicleShortRecovery.TrySeat(') -or
  @([regex]::Matches($recover,'AICF_VehicleShortRecovery.CanSeat\(trip, member, seat\)')).Count -ne 2){'CANCEL_BEFORE_SEAT_AND_REVALIDATE'}
 if(!$normal.Contains('sample.m_iLogicalOccupants > 0 && HasFailedExactNativeExit(trip)') -or
  !$normal.Contains('sample.m_iLogicalOccupants == 0 && sample.m_iTransitions == 0 &&') -or
  (Body $d 'HasFailedExactNativeExit') -notmatch 'exit.m_Vehicle.m_Value == vehicle && exit.GetActionState\(\) == EAIActionState.FAILED'){'IMMEDIATE_PROVEN_EXIT_BLOCKER'}
 if($permit -match 'CanApplyHiddenRecovery|IsHiddenRecoveryCombatSafe|GetHiddenRecoveryEnabled' -or
  !$permit.Contains('vector.Distance(source, destination) > AICF_VehicleShortRecovery.MAX_ACTOR_DISPLACEMENT_METERS') -or
  !$permit.Contains('!AICF_VehicleBoardingMutationFence.IsAuthoritativeReplicatedEntity(group)')){'VISIBLE_DISMOUNT_BOUNDED'}
}
$issues=@(Rules $helper $boarding $dismount $tokens);if($issues.Count){throw "Short recovery: $issues"}
$controls=1
foreach($mutation in @(
 @('h','!Replication.IsServer()','false','EXACT_AI_IDENTITY'),
 @('h','!SCR_AIDamageHandling.IsConscious(character)','false','EXACT_AI_IDENTITY'),
 @('h','!lease.MatchesEntityIdentity(vehicle, vehicle.GetID(), rpl.Id().ToString())','false','EXACT_AI_IDENTITY'),
 @('h','MAX_ACTOR_DISPLACEMENT_METERS = 25.0;','MAX_ACTOR_DISPLACEMENT_METERS = 300.0;','SHORT_ACTOR_DISTANCE'),
 @('h','Math.AbsFloat(origin[1] - destination[1]) > MAX_SEAT_HEIGHT_METERS','false','SHORT_ACTOR_DISTANCE'),
 @('h','conflict.ReleaseOwnedReservationBeforeExit()','conflict.IsCurrent()','REPEAT_SHORT_DISTANCE_AFTER_RESERVATION_CALLBACK'),
 @('h','seat.IsReserved() && !seat.IsReservedBy(member)','false','EMPTY_EXACT_SEAT_NO_FOREIGN_MUTATION'),
 @('h','seat.GetOccupant() != member','false','REPEAT_EXACT_EXIT_FENCE'),
 @('h','players.GetPlayerIdFromControlledEntity(member) > 0','false','ONLY_CAPTURED_NONPLAYER_INCAPACITATED'),
 @('h','controller.GetLifeState() != ECharacterLifeState.INCAPACITATED','false','ONLY_CAPTURED_NONPLAYER_INCAPACITATED'),
 @('h','access.GetOutVehicle_NoDoor(destination, true, false, true)','access.GetOutVehicle_NoDoor(destination, false, false, true)','BOUNDED_INCAPACITATED_EGRESS'),
 @('h','access.GetInVehicle(vehicle, seat, true, -1, ECloseDoorAfterActions.INVALID, false)','access.GetInVehicle(vehicle, seat, false, -1, ECloseDoorAfterActions.INVALID, false)','REPEAT_SEAT_FENCE_BEFORE_NATIVE'),
 @('b','token.WasShortRecoveryAttempted()','false','FINITE_CURRENT_TOKEN_ATTEMPT'),
 @('b','!failed && !disappeared && !stalled','!stalled','IMMEDIATE_FAILURE_NO_DEADLINE_EXTENSION'),
 @('b','token.CancelOwnerSafe();','Print("skip");','CANCEL_BEFORE_SEAT_AND_REVALIDATE'),
 @('d','vector.Distance(source, destination) > AICF_VehicleShortRecovery.MAX_ACTOR_DISPLACEMENT_METERS','false','VISIBLE_DISMOUNT_BOUNDED')
)){
 $h=$helper;$b=$boarding;$d=$dismount;$t=$tokens
 switch($mutation[0]){'h'{$h=$h.Replace($mutation[1],$mutation[2])}'b'{$b=$b.Replace($mutation[1],$mutation[2])}'d'{$d=$d.Replace($mutation[1],$mutation[2])}}
 if($h -ceq $helper -and $b -ceq $boarding -and $d -ceq $dismount){throw "Missing mutation: $($mutation[3])"}
 if($mutation[3] -notin @(Rules $h $b $d $t)){throw "Mutation survived: $($mutation[3])"};$controls++
}
$body=Body $helper 'TrySeat'
$unsafe=$body.Insert(1,"`n vehicle.SetOrigin(member.GetOrigin());")
if('NO_HEAL_RESURRECTION_VEHICLE_RELOCATION_OR_LIFECYCLE' -notin @(Rules ($helper.Replace($body,$unsafe)) $boarding $dismount $tokens)){throw 'Vehicle relocation mutation survived'};$controls++
$unsafe=$body.Insert(1,"`n member.SetHealth(1);")
if('NO_HEAL_RESURRECTION_VEHICLE_RELOCATION_OR_LIFECYCLE' -notin @(Rules ($helper.Replace($body,$unsafe)) $boarding $dismount $tokens)){throw 'Healing mutation survived'};$controls++
"PASS Vehicle short recovery: current authoritative conscious AI only, exact seat/lease guards, native failure immediately or bounded progress observation, at most25m actor movement, no player visibility veto/healing/vehicle relocation; controls=$controls"
