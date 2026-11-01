param([string]$RepositoryRoot=(Split-Path -Parent (Split-Path -Parent $PSScriptRoot)))
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot '../lib/Stage3StaticAudit.Common.ps1')
$core=Join-Path $RepositoryRoot 'AIConflictCore/Scripts/Game/AIConflict'
function Body($Relative,$Method){
 $source=[IO.File]::ReadAllText((Join-Path $RepositoryRoot $Relative))
 Get-AICFMethodBody ([pscustomobject]@{Source=$source;Code=(ConvertTo-AICFCodeText $source)}) $Method
}
$stock=Body 'AIConflictCore/Scripts/Game/AIConflict/Content/AICF_ContentProfile.c' 'IsVehiclePresetCrewSeat'
$wcs='AIConflictArlandWCSRHS/Scripts/Game/AIConflictArlandWCSRHS/AICF_WCSRHSContentProfile.c'
$tank=Body $wcs 'AllowsTrackedVehiclePreset'
$tankCrew=(Body $wcs 'IsVehiclePresetCrewSeat').Replace('super.','base.')
$flow=Body 'AIConflictCore/Scripts/Game/AIConflict/Vehicles/AICF_VehicleBehaviorFlow.c' 'IsCrewSeat'
$adapter=@"
public static class ECompartmentType { public const int PILOT=0,TURRET=1,CARGO=2; }
public class BaseCompartmentSlot {
 int type; bool attached; public BaseCompartmentSlot(int t,bool a){type=t;attached=a;}
 public new int GetType(){return type;} public bool GetAttachedTurret(){return attached;}
 public static implicit operator bool(BaseCompartmentSlot seat){return seat!=null;}
}
public class AICF_VehiclePreset {
 public string m_sPrefab,m_sFaction;
 public AICF_VehiclePreset(string prefab,string faction){m_sPrefab=prefab;m_sFaction=faction;}
 public static implicit operator bool(AICF_VehiclePreset preset){return preset!=null;}
}
public class AICF_ContentProfile {
 public static AICF_ContentProfile Active=new AICF_ContentProfile();
 public static AICF_ContentProfile GetActive(){return Active;}
 public virtual bool IsVehiclePresetCrewSeat(string prefab,string stableKey,BaseCompartmentSlot seat) $stock
}
public class AICF_WCSRHSContentProfile:AICF_ContentProfile {
 public bool AllowsTrackedVehiclePreset(string prefab,string stableKey) $tank
 public override bool IsVehiclePresetCrewSeat(string prefab,string stableKey,BaseCompartmentSlot seat) $tankCrew
}
public static class AICF_VehicleBehaviorFlow {
 public static bool IsCrewSeat(BaseCompartmentSlot seat,AICF_VehiclePreset preset) $flow
}
"@
Add-Type -TypeDefinition $adapter
$checks=0
function Require([bool]$Value,[string]$Name){if(!$Value){throw "FAIL $Name"};$script:checks++}
foreach($wcsEnabled in @($false,$true)){
 $profile=New-Object AICF_ContentProfile
 if($wcsEnabled){$profile=New-Object AICF_WCSRHSContentProfile}
 [AICF_ContentProfile]::Active=$profile
 foreach($case in @(
  @('Prefabs/Vehicles/Tracked/M1A1/M1A1_Olive.et','US',$true),
  @('Prefabs/Vehicles/Tracked/M1A1/M1A2/M1A2.et','US',$true),
  @('Prefabs/Vehicles/Tracked/M1A1/M1A1_Olive.et','USSR',$false),
  @('Prefabs/Vehicles/Tracked/T72A/T72A.et','USSR',$true),
  @('Prefabs/Vehicles/Tracked/T72B/T72B.et','USSR',$true),
  @('Prefabs/Vehicles/Tracked/T72B/T72B.et','US',$false),
  @('Prefabs/Vehicles/Wheeled/LAV25/LAV25.et','US',$false),
  @('Prefabs/Vehicles/Tracked/BMP3/BMP3.et','USSR',$false),
  @('Prefabs/Vehicles/Tracked/M1A1/M1A1_Olive.et','FIA',$false)
 )){
  $preset=New-Object AICF_VehiclePreset($case[0],$case[1])
  foreach($kind in 0..2){foreach($attached in @($false,$true)){
   $seat=New-Object BaseCompartmentSlot($kind,$attached)
   $expected=($kind -ne 2 -or $attached -or ($wcsEnabled -and $case[2]))
   Require ([AICF_VehicleBehaviorFlow]::IsCrewSeat($seat,$preset) -eq $expected) "CREW_${wcsEnabled}_$($case[0])_$($case[1])_${kind}_$attached"
  }}
  Require (![AICF_VehicleBehaviorFlow]::IsCrewSeat($null,$preset)) 'NULL_SEAT_DENIED'
 }
 Require (![AICF_VehicleBehaviorFlow]::IsCrewSeat((New-Object BaseCompartmentSlot(2,$false)),$null)) 'NO_RECIPE_NO_LOADER_PERMISSION'
}
$policy=[IO.File]::ReadAllText((Join-Path $core 'Vehicles/AICF_VehicleBehaviorPolicy.c'))
Require ($policy -match 'AICF_VehicleBehaviorFlow.IsCrewSeat\(seat, binding.m_Trip.GetVehiclePreset\(\)\)') 'NATIVE_KEEP_USES_CURRENT_BINDING_RECIPE'
Require ($policy -match 'message.m_CompartmentSlot.GetVehicle\(\) == binding.m_Vehicle' -and $policy -match 'AICF_VehicleBehaviorFlow.IsCrewSeat\(message.m_CompartmentSlot, binding.m_Trip.GetVehiclePreset\(\)\)') 'EXACT_LOADER_REPLACEMENT_DEFERRED_DURING_COMBAT'
Require ($policy -match '!message.m_CompartmentSlot && role == EAICompartmentType.Cargo' -and $policy -match 'AICF_VehiclePreset.Collect\(binding.m_Vehicle, seats\)' -and $policy -match 'seat.GetType\(\) == ECompartmentType.CARGO && AICF_VehicleBehaviorFlow.IsCrewSeat') 'ROLE_ONLY_LOADER_REPLACEMENT_DEFERRED_DURING_COMBAT'
$behavior=[IO.File]::ReadAllText((Join-Path $core 'Vehicles/AICF_VehicleBehaviorFlow.c'))
Require ($behavior -notmatch 'IsCrewSeat\((?:seat|seats\[[^\]]+\])\)') 'ALL_BEHAVIOR_CLASSIFICATION_USES_RECIPE'
"Vehicle preset crew policy: PASS checks=$checks; actual production profile/behavior predicates; tank CARGO loader retained"
