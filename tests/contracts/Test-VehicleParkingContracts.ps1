param([string]$RepositoryRoot=(Split-Path -Parent (Split-Path -Parent $PSScriptRoot)))
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot '../lib/Stage3StaticAudit.Common.ps1')
$core=Join-Path $RepositoryRoot 'AIConflictCore/Scripts/Game/AIConflict'
$parking=[IO.File]::ReadAllText((Join-Path $core 'Vehicles/AICF_VehicleParking.c'))
$flow=[IO.File]::ReadAllText((Join-Path $core 'Vehicles/AICF_VehicleBehaviorFlow.c'))
function Body([string]$s,[string]$n){Get-AICFMethodBody ([pscustomobject]@{Source=$s;Code=(ConvertTo-AICFCodeText $s)}) $n}
function Rules([string]$p,[string]$f){
 $exact=Body $p 'IsExactVehicle';$human=Body $p 'HasNoForeignOrHumanUse';$apply=Body $p 'EnsureApplied';$release=Body $p 'ReleaseForResume';$stop=Body $p 'Stop'
 foreach($guard in @('Replication.IsServer()','trip == m_Trip','trip.GetLease() == m_Lease','m_Lease.HasPhysicalAsset()',
  'trip.GetGroupGeneration() == m_iGroupGeneration','trip.GetTripGeneration() == m_iTripGeneration',
  'm_Lease.GetLeaseGeneration() == m_iLeaseGeneration','m_Lease.GetVehicleGeneration() == m_iVehicleGeneration',
  'm_Vehicle.GetID() == m_VehicleId','m_Lease.MatchesEntityIdentity(m_Vehicle, m_VehicleId, m_sVehicleRpl)',
  'AICF_VehicleBehaviorBinding.ComponentRpl(m_Vehicle) == m_sVehicleRpl')){if(!$exact.Contains($guard)){'EXACT_CAR_OWNER';break}}
 foreach($guard in @('if (!controlled && !main) return false;','entity == m_Vehicle','CompartmentAccessComponent.GetVehicleIn(entity) == m_Vehicle',
  'access.GetVehicleCompartmentManagerOwner() == m_Vehicle','!HasCapturedMember(occupant)',
  'players.GetPlayerIdFromControlledEntity(occupant) > 0','SCR_PossessingManagerComponent.GetPlayerIdFromMainEntity(occupant) > 0',
  'agent.GetParentGroup() != m_Trip.GetAssignment().GetGroup()')){if(!$human.Contains($guard)){'NO_HUMAN_FOREIGN_MUTATION';break}}
 if(!$apply.Contains('if (!m_Controller.GetPersistentHandBrake())') -or !$apply.Contains('m_Controller.SetPersistentHandBrake(true);') -or
  !$apply.Contains('m_bOwnsBrake = m_Controller.GetPersistentHandBrake();') -or
  !$apply.Contains('if (!IsExactVehicle(trip) || !IsUsableVehicle() || !HasNoForeignOrHumanUse()) return false;')){'CLAIM_ONLY_OWN_REPEATED_SAFE_WRITE'}
 if(!$stop.Contains('if (m_bOwnsBrake && IsExactVehicle(trip) && HasNoForeignOrHumanUse())') -or
  !$stop.Contains('if (m_Controller.GetPersistentHandBrake()) m_Controller.SetPersistentHandBrake(false);') -or
  !$release.Contains('AICF_VehicleNativeSubgroup.Settled(pilot, m_Vehicle)') -or
  !$release.Contains('car.GetAIAgent().GetControlledEntity() != m_Vehicle') -or !$release.Contains('return m_bStopped && !m_bOwnsBrake;')){'OWN_RELEASE_NATIVE_PILOT_ONLY'}
 $begin=Body $f 'Begin';$deploy=Body $f 'DeployActors';$exit=Body $f 'EnsureExit';$resume=Body $f 'PollReboarding'
 if(!$begin.Contains('if (!state.m_Parking.EnsureApplied(trip)) return false;') -or !$deploy.Contains('m_Parking.IsReadyForExit(trip)') -or
  $deploy.IndexOf('m_Parking.IsReadyForExit(') -gt $deploy.IndexOf('EnsureExit(') -or !$exit.Contains('m_Parking.IsReadyForExit(trip)') -or
  $exit.IndexOf('m_Parking.IsReadyForExit(') -gt $exit.IndexOf('new SCR_AIGetOutVehicle(') -or !$resume.Contains('state.m_Parking.ReleaseForResume(trip)') -or
  !(Body $f 'Stop').Contains('trip.GetBehaviorState().m_Parking.Stop(trip);')){'PARK_BEFORE_DRIVER_EXIT_RELEASE_ON_RESUME_STOP'}
 if((ConvertTo-AICFCodeText $p) -match '\b(?:SetVelocity|SetAngularVelocity|SetWorldTransform|SetOrigin|Teleport|SetHealth|Heal|GetInVehicle|GetOutVehicle|CallLater)\s*\('){'NO_PHYSICAL_HEALTH_OR_ASYNC_EFFECTS'}
}
$issues=@(Rules $parking $flow);if($issues.Count){throw "Parking source: $issues"};$checks=1
foreach($m in @(
 @('trip.GetLease() == m_Lease','true','EXACT_CAR_OWNER'),@('m_Vehicle.GetID() == m_VehicleId','true','EXACT_CAR_OWNER'),
 @('m_Lease.MatchesEntityIdentity(m_Vehicle, m_VehicleId, m_sVehicleRpl)','true','EXACT_CAR_OWNER'),
 @('entity == m_Vehicle','false','NO_HUMAN_FOREIGN_MUTATION'),@('!HasCapturedMember(occupant)','false','NO_HUMAN_FOREIGN_MUTATION'),
 @('access.GetVehicleCompartmentManagerOwner() == m_Vehicle','false','NO_HUMAN_FOREIGN_MUTATION'),
 @('if (!m_Controller.GetPersistentHandBrake())','if (true)','CLAIM_ONLY_OWN_REPEATED_SAFE_WRITE'),
 @('m_bOwnsBrake && IsExactVehicle(trip) && HasNoForeignOrHumanUse()','IsExactVehicle(trip)','OWN_RELEASE_NATIVE_PILOT_ONLY'),
 @('return m_bStopped && !m_bOwnsBrake;','return m_bStopped;','OWN_RELEASE_NATIVE_PILOT_ONLY')
)){
 if($m[2] -notin @(Rules ($parking.Replace($m[0],$m[1])) $flow)){throw "Parking mutation survived: $($m[0])"};$checks++
}
# Actual production writer/retirement bodies. Native scope adapters expose each
# identity/safety veto and permit readback refusal; no replacement implementation.
$apply=(Body $parking 'EnsureApplied').Replace('!trip.GetVehiclePreset()','trip.GetVehiclePreset() == null')
$stop=Body $parking 'Stop'
$ready=Body $parking 'IsReadyForExit'
$code=@'
using System;
public enum AICF_ETransportTripPhase{TRANSIT,TACTICAL_DEPLOYMENT}
public enum AICF_EVehicleBehavior{TRANSPORT,COMBAT}
public class Preset{public AICF_EVehicleBehavior m_iBehavior=AICF_EVehicleBehavior.TRANSPORT;}
public class AICF_TransportTrip{public bool Valid=true,Terminal,CurrentPreset=true;public int Revision=1;public AICF_ETransportTripPhase Phase=AICF_ETransportTripPhase.TACTICAL_DEPLOYMENT;public Preset Preset=new Preset();public bool IsValid(){return Valid;}public bool IsTerminal(){return Terminal;}public bool IsVehiclePresetCurrent(){return CurrentPreset;}public int GetVehiclePresetRevision(){return Revision;}public Preset GetVehiclePreset(){return Preset;}public AICF_ETransportTripPhase GetPhase(){return Phase;}}
public class Car{public bool Brake,Refuse;public int Writes,Enabled,Disabled;public bool GetPersistentHandBrake(){return Brake;}public void SetPersistentHandBrake(bool value){Writes++;if(value)Enabled++;else Disabled++;if(!Refuse)Brake=value;}}
public class Vehicle{public bool Slow=true;}
public static class AICF_VehicleShortRecovery{public static bool IsSlowVehicle(Vehicle vehicle){return vehicle.Slow;}}
public class Parking{public bool Current=true,Safe=true,Healthy=true,m_bOwnsBrake,m_bStopped;public int CurrentReads,CurrentRejectAt=int.MaxValue,SafeReads,SafeRejectAt=int.MaxValue,HealthReads,HealthRejectAt=int.MaxValue,m_iPresetRevision=1;public Car m_Controller=new Car();public Vehicle m_Vehicle=new Vehicle();public bool IsExactVehicle(AICF_TransportTrip trip){CurrentReads++;return trip!=null && Current && CurrentReads<CurrentRejectAt && !m_bStopped;}public bool HasNoForeignOrHumanUse(){SafeReads++;return Safe && SafeReads<SafeRejectAt;}public bool IsUsableVehicle(){HealthReads++;return Healthy && HealthReads<HealthRejectAt;}public void Log(string value){}public bool EnsureApplied(AICF_TransportTrip trip) APPLY public bool IsReadyForExit(AICF_TransportTrip trip) READY public void Stop(AICF_TransportTrip trip) STOP}
'@
Add-Type -TypeDefinition ($code.Replace(' APPLY',' '+$apply).Replace(' READY',' '+$ready).Replace(' STOP',' '+$stop))
function Good{$t=New-Object AICF_TransportTrip;$p=New-Object Parking;return @($t,$p)}
foreach($case in @(
 @{bad={param($t,$p)$p.Current=$false}},@{bad={param($t,$p)$p.Safe=$false}},@{bad={param($t,$p)$p.Healthy=$false}},
 @{bad={param($t,$p)$t.Valid=$false}},@{bad={param($t,$p)$t.Terminal=$true}},@{bad={param($t,$p)$t.CurrentPreset=$false}},
 @{bad={param($t,$p)$t.Revision=2}},@{bad={param($t,$p)$t.Preset.m_iBehavior=[AICF_EVehicleBehavior]::COMBAT}},
 @{bad={param($t,$p)$t.Phase=[AICF_ETransportTripPhase]::TRANSIT}},@{bad={param($t,$p)$p.CurrentRejectAt=2}},
 @{bad={param($t,$p)$p.SafeRejectAt=2}},@{bad={param($t,$p)$p.HealthRejectAt=2}}
)){$t,$p=Good;& $case.bad $t $p;if($p.EnsureApplied($t) -or $p.m_Controller.Writes -ne 0 -or $p.m_bOwnsBrake){throw 'Parking wrote through invalid native adapter scope'};$checks++}
$t,$p=Good;if(!$p.EnsureApplied($t) -or !$p.EnsureApplied($t) -or $p.m_Controller.Writes -ne 1 -or !$p.m_bOwnsBrake){throw 'Own parking write not idempotent'};$p.Stop($t);$p.Stop($t);if($p.m_Controller.Brake -or $p.m_Controller.Disabled -ne 1 -or $p.m_bOwnsBrake){throw 'Own parking release not exactly once'};$checks++
$t,$p=Good;$p.m_Controller.Brake=$true;if(!$p.EnsureApplied($t) -or $p.m_bOwnsBrake){throw 'Claimed preexisting native handbrake'};$p.Stop($t);if(!$p.m_Controller.Brake -or $p.m_Controller.Writes -ne 0){throw 'Cleared foreign/native preexisting brake'};$checks++
$t,$p=Good;$p.m_Vehicle.Slow=$false;if($p.IsReadyForExit($t) -or !$p.m_Controller.Brake){throw 'Driver may exit moving car or parking not requested'};$p.m_Vehicle.Slow=$true;if(!$p.IsReadyForExit($t)){throw 'Stopped parked car could not deploy'};$checks++
$t,$p=Good;$p.m_Controller.Refuse=$true;if($p.EnsureApplied($t) -or $p.m_bOwnsBrake){throw 'Claimed rejected native write'};$checks++
foreach($field in @('Safe','Current')){$t,$p=Good;$null=$p.EnsureApplied($t);$p.$field=$false;$p.Stop($t);if(!$p.m_Controller.Brake -or $p.m_Controller.Disabled -ne 0){throw 'Released into foreign/human/replaced context'};$checks++}
"PASS Vehicle parking controls=$checks actual_writer_retirement=EXECUTED native_slope=SEPARATE"
