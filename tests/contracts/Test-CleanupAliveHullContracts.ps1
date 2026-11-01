param([string]$RepositoryRoot=(Split-Path -Parent (Split-Path -Parent $PSScriptRoot)))
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot '../lib/Stage3StaticAudit.Common.ps1')
$root=Join-Path $RepositoryRoot 'AIConflictCore/Scripts/Game/AIConflict'
$owner=[IO.File]::ReadAllText((Join-Path $root 'Vehicles/AICF_VehicleCleanupManager.c'))
$roster=[IO.File]::ReadAllText((Join-Path $root 'State/Vehicles/AICF_VehicleCleanupRecoveryRoster.c'))
$fixture=[IO.File]::ReadAllText((Join-Path $RepositoryRoot 'tests/fixtures/AICF_VehicleBehaviorProbe.c'))
function Body($s,$m){Get-AICFMethodBody ([pscustomobject]@{Source=$s;Code=(ConvertTo-AICFCodeText $s)}) $m}
function Rules($o,$r,$f){
 $eligible=Body $o 'IsCurrentDetachedHullMember';$legacy=Body $o 'IsCurrentIncapacitatedHullMember';$move=Body $o 'TryRelocateIncapacitatedHullMembers';$project=Body $o 'ProjectDetachedHullDestination';$actions=Body $o 'HasOwnedNativeVehicleTransition';$native=Body $f 'AICF_VBCleanupAliveUpdate'
 foreach($part in @('!job.m_RecoveryRoster.IsCurrent(job, vehicle, agent, character)','!m_Watchdog.IsAuthoritativeNonPlayerCharacter(character)','controller.IsPlayerControlled()','physics.GetVelocity().Length() > 0.25','controller.GetLifeState() == ECharacterLifeState.ALIVE && HasOwnedNativeVehicleTransition(agent, character)','IsDetachedHullPhysicalState(controller.GetLifeState()')){if(!$eligible.Contains($part)){'EXACT_CURRENT_ALIVE_OR_INCAP_BODY';break}}
 foreach($part in @('utility.m_OwnerEntity != character','utility.GetActions(actions)','utility.GetCurrentAction()','utility.GetExecutedAction()','SCR_AIGetInVehicle.Cast(action)','SCR_AIGetOutVehicle.Cast(action)','vehicleAction.m_Utility != utility','!vehicleAction.GetRemoveAction()','EAIActionState.FAILED','EAIActionState.COMPLETED')){if(!$actions.Contains($part)){'NO_OWNED_LIVE_NATIVE_VEHICLE_TRANSITION';break}}
 foreach($part in @('AICF_InfantrySpawnPlacement.IsUsable(job.m_Group, query, position)','vector.DistanceXZ(query, position) <= 0.5','Math.AbsFloat(origin[1] - position[1]) <= AICF_VehicleShortRecovery.MAX_SEAT_HEIGHT_METERS','vector.Distance(origin, position) <= AICF_VehicleShortRecovery.MAX_ACTOR_DISPLACEMENT_METERS')){if(!$project.Contains($part)){'PROJECTED_STANDING_SHORT_HEIGHT_BOUNDS';break}}
 if(([regex]::Matches($move,'IsCurrentDetachedHullMember\(job, vehicle, agent, character\)')).Count -ne 2 -or ([regex]::Matches($move,'ProjectDetachedHullDestination\(')).Count -ne 2 -or !$move.Contains('vector.Distance(repeated, safePosition) > 0.1')){'REPEATED_IMMEDIATE_COMMIT_FENCES'}
 if(!$move.Contains('VEHICLE_CLEANUP_INCAPACITATED_HULL_RELOCATED') -or !$move.Contains('VEHICLE_CLEANUP_ALIVE_HULL_RELOCATED') -or !$move.Contains('life_state=%9') -or !$move.Contains('lifeState = "ALIVE"') -or !$legacy.Contains('GetLifeState() == ECharacterLifeState.INCAPACITATED')){'PRESERVE_OLD_INCAP_EVENT_AND_ACCURATE_ALIVE_EVENT'}
 if(!$move.Contains('job.m_iManagedExactRecoveryAttempts >= MANAGED_EXACT_RECOVERY_MAX_ATTEMPTS') -or $move.IndexOf('job.m_iManagedExactRecoveryAttempts++;') -lt 0 -or $move.IndexOf('job.m_iManagedExactRecoveryAttempts++;') -gt $move.IndexOf('character.Teleport(transform)') -or $move -match 'm_i\w*(Attempts|Deadline)\w*\s*=\s*0|Reset\('){'SHARED_OLD_THREE_BUDGET_CONSUMED_BEFORE_TELEPORT'}
 foreach($part in @('CLEANUP_ALIVE_RELEASE_QUEUED','CLEANUP_ALIVE_HULL_GHOST_ESTABLISHED','CLEANUP_ALIVE_HULL_PHYSICAL_CLEAR','CLEANUP_ALIVE_LIFE_HEALTH_PRESERVED','CLEANUP_ALIVE_RECOVERY_BUDGET','CLEANUP_ALIVE_FULL_RELEASE','victim.Teleport(transform)','SetDisableMovementControls(true)','SetDisableMovementControls(m_bVBCleanupOldMovementDisabled)','ECharacterLifeState.ALIVE','attempts > 0 && attempts <= 3','AICF_VBCleanupDeadline(m_VBCleanupLease) == m_iVBCleanupDeadline','later_native_readback=1','query.IsClearanceSafe() && !query.IsRetainedFailClosed()')){if(!$native.Contains($part)){'ACTUAL_ALIVE_GHOST_BODY_CLEAR_LIFE_BUDGET_RELEASE';break}}
 if(!$f.Contains('aicfCleanupAliveProbe') -or !$f.Contains('if (m_bVBCleanupAliveMode) AICF_VBCleanupAliveUpdate(now)')){'ISOLATED_NATIVE_ALIVE_OPT_IN'}
 if((ConvertTo-AICFCodeText $native) -match 'ForceUnconsciousness|\.Kill\(|SetHealth|SetLifeState|SetDamageHandlingEnabled|SetVelocity'){ 'NO_FIXTURE_HEALTH_OR_INVULNERABILITY_OR_CAR_FREEZE' }
 if((ConvertTo-AICFCodeText ($move+$eligible+$project+$actions)) -match 'SetHealth|SetLifeState|(?:GetOutVehicle|EjectOccupant)\s*\(|\.Fail\(|\.Complete\(|SetRemoveAction|SetDisableMovementControls|m_ActorHistory'){'PHYSICAL_ONLY_PRODUCTION_NO_ACTION_OR_HEALTH_MUTATION'}
}
$bad=@(Rules $owner $roster $fixture);if($bad.Count){throw ($bad -join ',')}
$mutants=@(
 @('o','controller.IsPlayerControlled()','false'),@('o','!m_Watchdog.IsAuthoritativeNonPlayerCharacter(character)','false'),
 @('o','physics.GetVelocity().Length() > 0.25','false'),@('o','HasOwnedNativeVehicleTransition(agent, character)','false'),
 @('o','utility.m_OwnerEntity != character','false'),@('o','utility.GetExecutedAction()','null'),@('o','SCR_AIGetOutVehicle.Cast(action)','false'),
 @('o','vector.DistanceXZ(query, position) <= 0.5','true'),@('o','Math.AbsFloat(origin[1] - position[1]) <= AICF_VehicleShortRecovery.MAX_SEAT_HEIGHT_METERS','true'),
 @('o','vector.Distance(origin, position) <= AICF_VehicleShortRecovery.MAX_ACTOR_DISPLACEMENT_METERS','true'),
 @('o','vector.Distance(repeated, safePosition) > 0.1','false'),@('o','job.m_iManagedExactRecoveryAttempts++;',''),
 @('o','VEHICLE_CLEANUP_ALIVE_HULL_RELOCATED','VEHICLE_CLEANUP_INCAPACITATED_HULL_RELOCATED'),
 @('f','SetDisableMovementControls(true)','SetDisableMovementControls(false)'),@('f','attempts > 0 && attempts <= 3','attempts >= 0'),
 @('f','later_native_readback=1','later_native_readback=0')
)
foreach($m in $mutants){$o=$owner;$r=$roster;$f=$fixture;switch($m[0]){'o'{$o=$o.Replace($m[1],$m[2])}'r'{$r=$r.Replace($m[1],$m[2])}'f'{$f=$f.Replace($m[1],$m[2])}};if($o -ceq $owner -and $r -ceq $roster -and $f -ceq $fixture){throw "Stale mutant $($m[1])"};if(!@(Rules $o $r $f).Count){throw "Escaped mutant $($m[1])"}}
$body=Body $roster 'IsDetachedHullPhysicalState'
Add-Type -TypeDefinition ('public enum AICFAliveLife {ALIVE,INCAPACITATED,DEAD} public static class AICFAliveHull { public static bool Check(AICFAliveLife life,bool characterInVehicle,bool linked,bool inCompartment,bool gettingIn,bool gettingOut,bool switching,bool inside)'+$body.Replace('ECharacterLifeState.','AICFAliveLife.')+'}')
$count=0
foreach($life in 0..2){foreach($mask in 0..127){$a=@(for($bit=0;$bit -lt 7;$bit++){[bool]($mask -band (1 -shl $bit))});$actual=[AICFAliveHull]::Check($life,$a[0],$a[1],$a[2],$a[3],$a[4],$a[5],$a[6]);$expected=($life -eq 0 -or $life -eq 1) -and $mask -eq 64;if($actual -ne $expected){throw "Actual detached hull predicate mismatch life=$life mask=$mask"};$count++}}
"PASS Alive hull cleanup: source16 negative mutants, actual production predicate $count combinations, legacyINCAP unchanged, native proof REQUIRED separately"
