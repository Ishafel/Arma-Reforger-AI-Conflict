param([string]$RepositoryRoot=(Split-Path -Parent (Split-Path -Parent $PSScriptRoot)))
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot '../lib/Stage3StaticAudit.Common.ps1')
$vehicles=Join-Path $RepositoryRoot 'AIConflictCore/Scripts/Game/AIConflict/Vehicles'
$policy=[IO.File]::ReadAllText((Join-Path $vehicles 'AICF_VehicleBehaviorPolicy.c'))
$native=[IO.File]::ReadAllText((Join-Path $vehicles 'AICF_VehicleBehaviorNativeCombat.c'))
$reaction=[IO.File]::ReadAllText((Join-Path $vehicles 'AICF_VehiclePresetNativeSeatReaction.c'))
function Body([string]$text,[string]$name){Get-AICFMethodBody ([pscustomobject]@{Source=$text;Code=(ConvertTo-AICFCodeText $text)}) $name}
function Rules([string]$p,[string]$n,[string]$r){
 $current=Body $p 'IsCurrent';$owns=Body $p 'OwnsActor';$deployed=Body $p 'IsDeployed'
 $register=Body $p 'Register';$unregister=Body $p 'Unregister';$update=Body $p 'Update';$find=Body $p 'Find'
 $combat=Body $p 'HasLocalCombat';$local=Body $p 'IsLocalThreat';$defer=Body $p 'ShouldDeferCrewReplacement'
 $suppress=Body $p 'SuppressGetIn';$keep=Body $p 'KeepCombatCrew';$activity=Body $p 'OwnsCombatActivity'
 $usage=Body $n 'VehicleUsageLogic';$life=Body $n 'OnAgentLifeStateChanged';$turret=Body $n 'DismountTurretCondition'
 $override=Body $p 'HasTransportCombatOverride';$yield=Body $p 'ShouldYieldNativeCombat';$evaluate=Body $n 'CustomEvaluate';$getOut=Body $n 'PerformReaction'
 $perform=Body $r 'PerformReaction'
 if(!$current -or !$combat -or !$suppress -or !$usage -or !$perform){'METHODS';return}
 $binding=$p.Substring(0,$p.IndexOf('class AICF_VehicleBehaviorPolicy'))
 if((ConvertTo-AICFCodeText $binding) -match '\bref\s+(?:AICF_TransportTrip|SCR_AIGroup|AICF_VehicleLease|Vehicle|IEntity)' -or
  $register -notmatch 'if \(existing.m_Trip == trip\) return' -or $unregister -notmatch 's_aBindings\[index\].m_Trip == trip'){'WEAK_SCOPED_LIFETIME'}
 foreach($guard in @('!Replication.IsServer()','!m_Trip.IsValid()','m_Trip.IsTerminal()',
  '!m_Trip.IsVehiclePresetCurrent()','m_Trip.GetOperationId() != m_sOperation',
  'm_Trip.GetGroupGeneration() != m_iGroupGeneration','m_Trip.GetTripGeneration() != m_iTripGeneration',
  'm_Trip.GetVehiclePresetRevision() != m_iPresetRevision',
  'slot.GetSpawnGeneration() != m_iGroupGeneration','slot.GetStrategicIntentRevision() != assignment.GetStrategicIntentRevision()',
  'm_Group.GetID() != m_GroupId','ComponentRpl(m_Group) != m_sGroupRpl',
  'm_Trip.GetLease() != m_Lease','m_Lease.GetLeaseGeneration() != m_iLeaseGeneration',
  'm_Lease.GetVehicleGeneration() != m_iVehicleGeneration','m_Vehicle.GetID() != m_VehicleId',
  'ComponentRpl(m_Vehicle) != m_sVehicleRpl','m_Lease.MatchesEntityIdentity(m_Vehicle, m_VehicleId, m_sVehicleRpl)')){
  if(!$current.Contains($guard)){'EXACT_CURRENT_TRIP_LEASE';break}
 }
 if($owns -notmatch '!AICF_VehicleBoardingMutationFence.IsAuthoritativeAIEntity\(member\)' -or
  $owns -notmatch 'agent.GetControlledEntity\(\) != member \|\| agent.GetParentGroup\(\) != m_Group' -or
  $owns -notmatch 'm_Trip.VehiclePresetMember\(index\) == member && slot.GetRosterMember\(index\) == member' -or
  $deployed -notmatch 'member.GetID\(\) == m_aDeployedIds\[index\]' -or $deployed -notmatch 'ComponentRpl\(member\) == m_aDeployedRpls\[index\]'){'LIVE_EXACT_DEPLOYED_ACTOR'}
 if($find -notmatch '!Replication.IsServer\(\)' -or $find -notmatch 'binding.m_Vehicle != vehicle \|\| !binding.IsCurrent\(\)' -or
  $find -notmatch 'if \(found\) return null' -or $update -notmatch '!binding.OwnsActor\(actor\)' -or
  $update -notmatch 'binding.m_aDeployed.Clear\(\)' -or $update -notmatch 'binding.m_aDeployedIds.Insert\(actor.GetID\(\)\)'){'CURRENT_UNAMBIGUOUS_DEPLOYMENT_SET'}
 if($defer -notmatch 'binding.m_Trip.GetVehiclePreset\(\).m_iBehavior == AICF_EVehicleBehavior.COMBAT && HasLocalCombat\(binding.m_Trip\)' -or
  $combat -notmatch 'cluster.m_State.GetTimeSinceLastNewInformation\(\)' -or $combat -notmatch 'target.GetTimeSinceSeen\(\)' -or
  $combat -notmatch 'utility.m_ThreatSystem.GetState\(\) >= EAIThreatState.ALERTED' -or
  $combat -notmatch 'utility.m_ThreatSystem.GetThreatMeasureWithoutInjuryFactor\(\) > 0.01' -or
  $combat -notmatch 'SCR_AIAttackBehavior.Cast\(behavior\) \|\| SCR_AISuppressBehavior.Cast\(behavior\)' -or
  $combat -notmatch '!utility.m_ThreatSystem \|\| !utility.m_CombatComponent\) return true' -or
  $local -notmatch 'distanceMeters >= 0 && distanceMeters <= COMBAT_RADIUS_METERS && ageSeconds >= 0 && ageSeconds <= TARGET_MEMORY_SECONDS'){'FRESH_LOCAL_COMBAT_NOT_CACHED_HINT'}
 if($suppress -notmatch 'if \(binding.IsDeployed\(utility.m_OwnerEntity\)\) return true' -or
  $suppress -notmatch '!binding.OwnsActor\(utility.m_OwnerEntity\)' -or $suppress -notmatch 'ShouldDeferCrewReplacement\(binding.m_Vehicle\)' -or
  $suppress -notmatch 'message.m_CompartmentSlot == currentSeat' -or $suppress -notmatch 'CompartmentClassToType\(currentSeat.Type\(\)\) == role' -or
  $suppress -notmatch 'role == EAICompartmentType.Pilot \|\| role == EAICompartmentType.Turret \|\| role == EAICompartmentType.None'){'NO_DEPLOYED_REBOARD_OR_COMBAT_ROLE_SWAP'}
 if($perform.IndexOf('AICF_VehicleBehaviorPolicy.SuppressGetIn(utility, getIn)') -lt 0 -or
  $perform.IndexOf('AICF_VehicleBehaviorPolicy.SuppressGetIn(utility, getIn)') -gt $perform.IndexOf('AICF_VehiclePresetNativeSeatPolicy.Prepare')){'GUARD_BEFORE_NATIVE_RESERVATION_OR_ACTION'}
 if($usage -notmatch 'if \(!AICF_VehicleBehaviorPolicy.OwnsCombatActivity\(this, vehicle\)\)' -or
  $usage -notmatch 'super.VehicleUsageLogic\(vehicle\);\s*return' -or
  $usage -notmatch 'FindFireteamsOfVehicle\(cargo, usage, SCR_AIGroupFireteamVehicleCargo\)' -or
  $usage -notmatch 'SendDismountMessage\(agent, entity, ordinal, this, comms, ACTIVITY_NAME\)' -or
  $usage -notmatch 'fireteam.RemoveMember\(agent\)' -or $usage -match 'GetAvailableAgent|SendGetInMessage|VehicleCrew'){'SCOPED_NATIVE_CARGO_ONLY'}
 if($activity -notmatch 'activity.m_Utility == utility' -or $activity -notmatch 'utility.GetCurrentAction\(\) == activity' -or
  $activity -notmatch '!activity.GetRemoveAction\(\) && !activity.GetActionIsSuspended\(\)' -or
  $life -notmatch 'if \(AICF_VehicleBehaviorPolicy.ShouldDeferCrewReplacement\(Vehicle.Cast\(vehicle\)\)\) return' -or
  $life -notmatch 'super.OnAgentLifeStateChanged\(incapacitatedAgent, infoIncap, vehicle, lifeState\)'){'SCOPED_NATIVE_LIFE_STATE_POLICY'}
 if($turret -notmatch 'AICF_VehicleBehaviorPolicy.KeepCombatTurret\(m_Utility, m_CurrentVehicle\)' -or
  $turret -notmatch 'super.DismountTurretCondition\(targetPos, targetPosProvided, threatPriority\)' -or
  $keep -notmatch 'binding.m_Trip.GetVehiclePreset\(\).m_iBehavior != AICF_EVehicleBehavior.COMBAT' -or
  $keep -notmatch '!binding.OwnsActor\(utility.m_OwnerEntity\)' -or $keep -notmatch '!SCR_AIDamageHandling.IsConscious\(utility.m_OwnerEntity\)' -or
  $keep -notmatch 'usage.GetDamageState\(\) == EDamageState.DESTROYED' -or $keep -notmatch 'VehicleIsOnFire\(vehicle\)' -or
  $keep -notmatch '!SCR_AIVehicleUsability.VehicleCanMove\(vehicle\)' -or $keep -notmatch 'transform\[1\]\[1\] < 0.25' -or
  $keep -notmatch 'seat.GetOccupant\(\) == utility.m_OwnerEntity'){'KEEP_COMBAT_TURRET_WITH_EMERGENCY_EXITS'}
 if($getOut -notmatch 'AICF_VehicleBehaviorPolicy.SuppressGetOut\(utility, SCR_AIMessage_GetOut.Cast\(message\)\)' -or
  $getOut -notmatch 'super.PerformReaction\(utility, message\)' -or (Body $p 'SuppressGetOut') -notmatch 'KeepCombatCrew\(utility, message.m_Vehicle\)'){'KEEP_CREW_FROM_UNRELATED_NATIVE_GETOUT'}
 if($keep -notmatch '\(binding.m_Trip.GetVehiclePreset\(\).m_iBehavior != AICF_EVehicleBehavior.COMBAT &&\s*!HasTransportCombatOverride\(binding.m_Trip\)\)' -or
  (Body $p 'KeepCombatTurret') -notmatch 'return KeepCombatCrew\(utility, vehicle\)'){'KEEP_CREW_ONLY_COMBAT_OR_CURRENT_ESCAPE'}
 if($override -notmatch 'trip.GetVehiclePreset\(\).m_iBehavior != AICF_EVehicleBehavior.TRANSPORT' -or
  $override -notmatch 'binding && binding.m_Trip == trip' -or
  $override -notmatch 'm_iCombatOverrideIntent >= 0' -or
  $override -notmatch 'm_iCombatOverrideIntent == trip.GetAssignment\(\).GetStrategicIntentRevision\(\)' -or
  $yield -notmatch '!binding.IsCurrent\(\) \|\| !HasTransportCombatOverride\(binding.m_Trip\)' -or
  $yield -notmatch 'activity.m_Utility == utility' -or $yield -notmatch 'FindVehicle\(binding.m_Vehicle\)' -or
  $evaluate -notmatch 'float nativePriority = super.CustomEvaluate\(\)' -or
  $evaluate -notmatch 'if \(AICF_VehicleBehaviorPolicy.ShouldYieldNativeCombat\(this\)\) return 0' -or
  $evaluate -notmatch 'return nativePriority'){'ONLY_EXPLICIT_CURRENT_TRANSPORT_INTENT_YIELDS_COMBAT'}
 if((ConvertTo-AICFCodeText ($p+$n)) -match 'SetOrigin|SetWorldTransform|\.GetInVehicle\(|\.GetOutVehicle\(|\.Fail\(|\.Complete\(|SetHealth|SetDamage|Resurrect|CommitTransition|CallLater|m_i(?:AbsoluteDeadline|LastPhysicalMotionAt|LastRouteProgressAt)Ms\s*[+]?='){'NO_LIFECYCLE_HEALTH_TELEPORT_OR_CLOCK_MUTATION'}
}
$failures=@(Rules $policy $native $reaction)
if($failures.Count){throw "Vehicle behavior native: $failures"}
$controls=1
foreach($mutation in @(
 @('p','AICF_TransportTrip m_Trip;','ref AICF_TransportTrip m_Trip;','WEAK_SCOPED_LIFETIME'),
 @('p','m_Trip.GetOperationId() != m_sOperation','false','EXACT_CURRENT_TRIP_LEASE'),
 @('p','m_Lease.GetVehicleGeneration() != m_iVehicleGeneration','false','EXACT_CURRENT_TRIP_LEASE'),
 @('p','ComponentRpl(m_Vehicle) != m_sVehicleRpl','false','EXACT_CURRENT_TRIP_LEASE'),
 @('p','!AICF_VehicleBoardingMutationFence.IsAuthoritativeAIEntity(member)','false','LIVE_EXACT_DEPLOYED_ACTOR'),
 @('p','agent.GetParentGroup() != m_Group','false','LIVE_EXACT_DEPLOYED_ACTOR'),
 @('p','ComponentRpl(member) == m_aDeployedRpls[index]','true','LIVE_EXACT_DEPLOYED_ACTOR'),
 @('p','if (found) return null;','if (false) return null;','CURRENT_UNAMBIGUOUS_DEPLOYMENT_SET'),
 @('p','&& HasLocalCombat(binding.m_Trip)','&& true','FRESH_LOCAL_COMBAT_NOT_CACHED_HINT'),
 @('p','ageSeconds <= TARGET_MEMORY_SECONDS','true','FRESH_LOCAL_COMBAT_NOT_CACHED_HINT'),
 @('p','utility.m_ThreatSystem.GetThreatMeasureWithoutInjuryFactor() > 0.01','true','FRESH_LOCAL_COMBAT_NOT_CACHED_HINT'),
 @('p','if (binding.IsDeployed(utility.m_OwnerEntity)) return true;','','NO_DEPLOYED_REBOARD_OR_COMBAT_ROLE_SWAP'),
 @('p','message.m_CompartmentSlot == currentSeat','true','NO_DEPLOYED_REBOARD_OR_COMBAT_ROLE_SWAP'),
 @('r','if (AICF_VehicleBehaviorPolicy.SuppressGetIn(utility, getIn)) return;','','GUARD_BEFORE_NATIVE_RESERVATION_OR_ACTION'),
 @('n','SCR_AIGroupFireteamVehicleCargo','SCR_AIGroupFireteamVehicleCrew','SCOPED_NATIVE_CARGO_ONLY'),
 @('n','super.VehicleUsageLogic(vehicle);','return;','SCOPED_NATIVE_CARGO_ONLY'),
 @('p','utility.GetCurrentAction() == activity','true','SCOPED_NATIVE_LIFE_STATE_POLICY'),
 @('n','if (AICF_VehicleBehaviorPolicy.ShouldDeferCrewReplacement(Vehicle.Cast(vehicle))) return;','','SCOPED_NATIVE_LIFE_STATE_POLICY'),
 @('p','!SCR_AIDamageHandling.IsConscious(utility.m_OwnerEntity)','false','KEEP_COMBAT_TURRET_WITH_EMERGENCY_EXITS'),
 @('p','SCR_AIVehicleUsability.VehicleIsOnFire(vehicle)','false','KEEP_COMBAT_TURRET_WITH_EMERGENCY_EXITS'),
 @('p','!SCR_AIVehicleUsability.VehicleCanMove(vehicle)','false','KEEP_COMBAT_TURRET_WITH_EMERGENCY_EXITS'),
 @('p','transform[1][1] < 0.25','false','KEEP_COMBAT_TURRET_WITH_EMERGENCY_EXITS'),
 @('p','!HasTransportCombatOverride(binding.m_Trip)','false','KEEP_CREW_ONLY_COMBAT_OR_CURRENT_ESCAPE'),
 @('p','binding.m_Trip.GetVehiclePreset().m_iBehavior != AICF_EVehicleBehavior.COMBAT &&','false &&','KEEP_CREW_ONLY_COMBAT_OR_CURRENT_ESCAPE'),
 @('n','if (AICF_VehicleBehaviorPolicy.SuppressGetOut(utility, SCR_AIMessage_GetOut.Cast(message))) return;','','KEEP_CREW_FROM_UNRELATED_NATIVE_GETOUT'),
 @('p','binding && binding.m_Trip == trip','binding','ONLY_EXPLICIT_CURRENT_TRANSPORT_INTENT_YIELDS_COMBAT'),
 @('p','m_iCombatOverrideIntent == trip.GetAssignment().GetStrategicIntentRevision()','m_iCombatOverrideIntent >= 0','ONLY_EXPLICIT_CURRENT_TRANSPORT_INTENT_YIELDS_COMBAT'),
 @('n','return nativePriority;','return 0;','ONLY_EXPLICIT_CURRENT_TRANSPORT_INTENT_YIELDS_COMBAT')
)){
 $p=$policy;$n=$native;$r=$reaction
 $original=@{p=$p;n=$n;r=$r}[$mutation[0]]
 if(!$original.Contains($mutation[1])){throw "Mutation target absent: $($mutation[3])"}
 $changed=$original.Replace($mutation[1],$mutation[2])
 switch($mutation[0]){'p'{$p=$changed}'n'{$n=$changed}'r'{$r=$changed}}
 if($mutation[3] -notin @(Rules $p $n $r)){throw "Mutation survived: $($mutation[3])"}
 $controls++
}
# Исполняем фактический production predicate радиуса/давности на обеих границах.
$body=Body $policy 'IsLocalThreat'
$expression=[regex]::Match($body,'(?s)return\s+(.*?);').Groups[1].Value
$radius=[regex]::Match($policy,'COMBAT_RADIUS_METERS\s*=\s*([0-9.]+)').Groups[1].Value
$memory=[regex]::Match($policy,'TARGET_MEMORY_SECONDS\s*=\s*([0-9.]+)').Groups[1].Value
$expression=$expression.Replace('distanceMeters','$distance').Replace('ageSeconds','$age').Replace('COMBAT_RADIUS_METERS',$radius).Replace('TARGET_MEMORY_SECONDS',$memory).
 Replace('>=','-ge').Replace('<=','-le').Replace('&&','-and')
$predicate=[scriptblock]::Create('param($distance,$age) '+$expression)
foreach($case in @(@(0,0,$true),@(300,20,$true),@(299.999,19.999,$true),@(300.001,0,$false),@(0,20.001,$false),@(-0.001,0,$false),@(0,-0.001,$false))){
 if([bool](& $predicate $case[0] $case[1]) -ne $case[2]){throw "Local threat boundary failed: $case"}
 $controls++
}
$body=Body $policy 'HasTransportCombatOverride'
$expression=[regex]::Match($body,'(?s)return (binding &&.*?);').Groups[1].Value.
 Replace('binding.m_Trip == trip','$sameTrip').Replace('trip.GetBehaviorState().m_iCombatOverrideIntent','$receipt').
 Replace('trip.GetAssignment().GetStrategicIntentRevision()','$intent').Replace('binding','$binding').Replace('>=','-ge').Replace('==','-eq').Replace('&&','-and')
$predicate=[scriptblock]::Create('param($binding,$sameTrip,$receipt,$intent) '+$expression)
foreach($case in @(@($true,$true,3,3,$true),@($true,$true,3,4,$false),@($true,$true,-1,-1,$false),@($true,$false,3,3,$false),@($false,$true,3,3,$false))){
 if([bool](& $predicate $case[0] $case[1] $case[2] $case[3]) -ne $case[4]){throw "Transport intent receipt failed: $case"}
 $controls++
}
# Фактический reject-mode predicate: обычный TRANSPORT сохраняет native exit,
# COMBAT либо exact current withdrawal receipt допускают только общий crew fence.
$body=Body $policy 'KeepCombatCrew'
$expression=[regex]::Match($body,'\((binding.m_Trip.GetVehiclePreset\(\).m_iBehavior != AICF_EVehicleBehavior.COMBAT &&\s*!HasTransportCombatOverride\(binding.m_Trip\))\)').Groups[1].Value.
 Replace('binding.m_Trip.GetVehiclePreset().m_iBehavior != AICF_EVehicleBehavior.COMBAT','(-not $combat)').
 Replace('!HasTransportCombatOverride(binding.m_Trip)','(-not $receipt)').Replace('&&','-and')
if(!$expression){throw 'Crew mode predicate absent'}
$modeReject=[scriptblock]::Create('param($combat,$receipt) '+$expression)
foreach($case in @(@($true,$false,$false),@($false,$true,$false),@($false,$false,$true),@($true,$true,$false))){
 if([bool](& $modeReject $case[0] $case[1]) -ne $case[2]){throw "Crew combat/receipt combination rejected incorrectly: $case"}
 $controls++
}
"PASS Vehicle behavior native: current weak lease+actors, fresh local threat boundaries, deployed reboard veto, scoped cargo-only combat and emergency exits; controls=$controls"
