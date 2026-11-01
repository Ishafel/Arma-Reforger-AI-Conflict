param([string]$RepositoryRoot=(Split-Path -Parent (Split-Path -Parent $PSScriptRoot)))
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot '../lib/Stage3StaticAudit.Common.ps1')
$text=[IO.File]::ReadAllText((Join-Path $RepositoryRoot 'AIConflictCore/Scripts/Game/AIConflict/Vehicles/AICF_VehicleTransitFlow.c'))
function Body([string]$source,[string]$method){Get-AICFMethodBody ([pscustomobject]@{Source=$source;Code=(ConvertTo-AICFCodeText $source)}) $method}
function Rules([string]$source){
 $mandatory=Body $source 'ProcessMandatoryCrew';$token=Body $source 'ProcessCrewRecoveryToken';$required=Body $source 'RequiresGunner'
 if($mandatory -notmatch 'if \(!RequiresGunner\(trip\)\)\s*return null;' -or $token -notmatch 'settledRole == EAICompartmentType.Pilot &&\s*RequiresGunner\(trip\) &&'){'ALL_CREW_PATHS_USE_EFFECTIVE_REQUIREMENT'}
 foreach($guard in @('trip.GetLease().GetKind() != AICF_EVehicleKind.ARMED_LIGHT','if (!preset) return true;','!AICF_VehiclePreset.CollectCaptured(trip.GetLease().GetVehicle(), preset, seats)','preset.m_aMembers[index] >= 0')){if(!$required.Contains($guard)){'AUTO_PRESERVED_MANUAL_MAPPING_AND_TOPOLOGY_FAIL_CLOSED';break}}
}
$issues=@(Rules $text);if($issues.Count){throw "Degraded crew: $issues"}
$checks=1
foreach($mutation in @(
 @('if (!RequiresGunner(trip))','if (false)','ALL_CREW_PATHS_USE_EFFECTIVE_REQUIREMENT'),
 @('RequiresGunner(trip) &&','true &&','ALL_CREW_PATHS_USE_EFFECTIVE_REQUIREMENT'),
 @('if (!preset) return true;','if (!preset) return false;','AUTO_PRESERVED_MANUAL_MAPPING_AND_TOPOLOGY_FAIL_CLOSED'),
 @('preset.m_aMembers[index] >= 0','true','AUTO_PRESERVED_MANUAL_MAPPING_AND_TOPOLOGY_FAIL_CLOSED')
)){
 $changed=$text.Replace($mutation[0],$mutation[1]);if($changed -ceq $text -or $mutation[2] -notin @(Rules $changed)){throw 'Degraded crew mutation survived'};$checks++
}
# Выполняется production RequiresGunner; адаптер заменяет только native types.
$body=(Body $text 'RequiresGunner').Replace('if (!preset)','if (preset == null)').Replace('array<BaseCompartmentSlot> seats = {};','List<BaseCompartmentSlot> seats = new List<BaseCompartmentSlot>();').Replace('seats.Count()','seats.Count').Replace('int index;','int index = 0;')
$code=@'
using System.Collections.Generic;
public enum AICF_EVehicleKind { LIGHT_TRANSPORT, ARMED_LIGHT }
public enum ECompartmentType { PILOT, TURRET, CARGO }
public class BaseCompartmentSlot { public ECompartmentType Type; public bool Attached; public new ECompartmentType GetType(){return Type;} public bool GetAttachedTurret(){return Attached;} }
public class AICF_VehiclePreset { public bool Valid=true; public List<BaseCompartmentSlot> Seats=new List<BaseCompartmentSlot>(); public List<int> m_aMembers=new List<int>(); public static bool CollectCaptured(object vehicle,AICF_VehiclePreset preset,List<BaseCompartmentSlot> seats){seats.AddRange(preset.Seats);return preset.Valid;} }
public class AICF_DegradedLease { public AICF_EVehicleKind Kind; public AICF_EVehicleKind GetKind(){return Kind;} public object GetVehicle(){return this;} }
public class AICF_TransportTrip { public AICF_DegradedLease Lease=new AICF_DegradedLease(); public AICF_VehiclePreset Preset; public AICF_DegradedLease GetLease(){return Lease;} public AICF_VehiclePreset GetVehiclePreset(){return Preset;} }
public class AICF_DegradedPolicy { public static bool RequiresGunner(AICF_TransportTrip trip) BODY }
'@
Add-Type -TypeDefinition ($code.Replace('BODY',$body))
$trip=New-Object AICF_TransportTrip
function Check([bool]$expected){if([AICF_DegradedPolicy]::RequiresGunner($trip) -ne $expected){throw 'Production gunner predicate disagrees with physical/effective crew case'};$script:checks++}
$trip.Lease.Kind=[AICF_EVehicleKind]::LIGHT_TRANSPORT;Check $false
$trip.Lease.Kind=[AICF_EVehicleKind]::ARMED_LIGHT;Check $true
$trip.Preset=New-Object AICF_VehiclePreset
$seat=New-Object BaseCompartmentSlot;$seat.Type=[ECompartmentType]::TURRET
$trip.Preset.Seats.Add($seat);$trip.Preset.m_aMembers.Add(1);Check $true
$trip.Preset.m_aMembers[0]=-1;Check $false
$trip.Preset.Valid=$false;Check $true
$trip.Preset.Valid=$true;$seat.Type=[ECompartmentType]::CARGO;$seat.Attached=$true;$trip.Preset.m_aMembers[0]=1;Check $true
$trip.Preset.m_aMembers[0]=-1;Check $false
"PASS Degraded crew contracts checks=$checks production_predicate=EXECUTED native_physical_loss=NOT_RUN"
