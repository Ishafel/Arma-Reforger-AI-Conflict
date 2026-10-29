param([string]$RepositoryRoot=(Split-Path -Parent (Split-Path -Parent $PSScriptRoot)))
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot '../lib/Stage3StaticAudit.Common.ps1')
$root=Join-Path $RepositoryRoot 'AIConflictCore/Scripts/Game/AIConflict/Vehicles'
$handoff=[IO.File]::ReadAllText((Join-Path $root 'AICF_VehicleTaskHandoff.c'))
$policy=[IO.File]::ReadAllText((Join-Path $root 'AICF_VehicleBehaviorPolicy.c'))
$flow=[IO.File]::ReadAllText((Join-Path $root 'AICF_VehicleBehaviorFlow.c'))
function Body([string]$s,[string]$n){Get-AICFMethodBody ([pscustomobject]@{Source=$s;Code=(ConvertTo-AICFCodeText $s)}) $n}
function Rules([string]$h,[string]$p,[string]$f){
 $attach=Body $h 'AttachVehicleUtility';$register=Body $p 'Register';$current=Body $p 'IsCurrent';$phase=Body $p 'IsBehaviorPhase'
 foreach($guard in @('!IsTripLeaseCurrent(trip)','if (trip.GetVehiclePreset())','!AICF_VehicleBehaviorPolicy.IsBehaviorPhase(trip.GetPhase())','AICF_VehicleBehaviorPolicy.Register(trip);','binding = AICF_VehicleBehaviorPolicy.Find(vehicle);','!binding || binding.m_Trip != trip','AICF_VehicleBehaviorPolicy.Unregister(trip);','groupUtility.AddUsableVehicle(usage);','if (binding && !binding.IsCurrent()) attached = false;','if (!attached && trip.GetVehiclePreset()) AICF_VehicleBehaviorPolicy.Unregister(trip);')){if(!$attach.Contains($guard)){'EXACT_POLICY_BEFORE_NATIVE_ATTACH_AND_ROLLBACK';break}}
 if($attach.IndexOf('AICF_VehicleBehaviorPolicy.Register(trip);') -gt $attach.IndexOf('groupUtility.AddUsableVehicle(usage);') -or $attach.IndexOf('binding.m_Trip != trip') -gt $attach.IndexOf('groupUtility.AddUsableVehicle(usage);')){'REGISTER_AND_PROOF_PRECEDE_NATIVE_ACTION_CREATION'}
 if(!$attach.Contains('if (attached && trip.GetVehiclePreset())') -or !$attach.Contains('attached = groupUtility.AICF_ReconcileExactVehicleSubgroup(trip);') -or
  $attach.IndexOf('AICF_ReconcileExactVehicleSubgroup(') -gt $attach.IndexOf('if (!attached && trip.GetVehiclePreset())')){'NATIVE_RECONCILE_RESULT_AND_ROLLBACK'}
 foreach($guard in @('!trip.IsValid()', 'trip.IsTerminal()', '!trip.GetAssignment()', '!IsBehaviorPhase(trip.GetPhase())','!trip.IsVehiclePresetCurrent()','!trip.GetLease() || !trip.GetLease().HasPhysicalAsset()')){if(!$register.Contains($guard)){'REGISTER_CURRENT_LEASE_AND_PHASE_ONLY';break}}
 if(!$current.Contains('!AICF_VehicleBehaviorPolicy.IsBehaviorPhase(m_Trip.GetPhase())') -or !$phase.Contains('return phase == AICF_ETransportTripPhase.TRANSIT || phase == AICF_ETransportTripPhase.TACTICAL_DEPLOYMENT;')){'LOOKUP_PHASE_FENCE'}
 if(!(Body $f 'Stop').Contains('AICF_VehicleBehaviorPolicy.Unregister(trip);')){'EXISTING_STOP_CLEANUP_RETAINED'}
 if((ConvertTo-AICFCodeText $attach) -match 'GetInVehicle|GetOutVehicle|SetHealth|SetOrigin|m_i.*Deadline\s*=|CommitTransition'){'NO_PHYSICAL_HEALTH_OR_CLOCK_MUTATION'}
}
$issues=@(Rules $handoff $policy $flow);if($issues.Count){throw "Policy attach: $issues"};$checks=1
foreach($m in @(
 @('h','AICF_VehicleBehaviorPolicy.Register(trip);','','EXACT_POLICY_BEFORE_NATIVE_ATTACH_AND_ROLLBACK'),
 @('h','!binding || binding.m_Trip != trip','false','EXACT_POLICY_BEFORE_NATIVE_ATTACH_AND_ROLLBACK'),
 @('h','if (!attached && trip.GetVehiclePreset()) AICF_VehicleBehaviorPolicy.Unregister(trip);','','EXACT_POLICY_BEFORE_NATIVE_ATTACH_AND_ROLLBACK'),
 @('h','attached = groupUtility.AICF_ReconcileExactVehicleSubgroup(trip);','attached = true;','NATIVE_RECONCILE_RESULT_AND_ROLLBACK'),
 @('p','!trip.IsVehiclePresetCurrent()','false','REGISTER_CURRENT_LEASE_AND_PHASE_ONLY'),
 @('p','!AICF_VehicleBehaviorPolicy.IsBehaviorPhase(m_Trip.GetPhase())','false','LOOKUP_PHASE_FENCE')
)){
 $h=$handoff;$p=$policy;if($m[0] -eq 'h'){$h=$h.Replace($m[1],$m[2])}else{$p=$p.Replace($m[1],$m[2])}
 if(($h -ceq $handoff -and $p -ceq $policy) -or $m[3] -notin @(Rules $h $p $flow)){throw "Attach mutation survived: $($m[3])"};$checks++
}
$reordered=$handoff.Replace('AICF_VehicleBehaviorPolicy.Register(trip);','').Replace('groupUtility.AddUsableVehicle(usage);','groupUtility.AddUsableVehicle(usage); AICF_VehicleBehaviorPolicy.Register(trip);')
if('REGISTER_AND_PROOF_PRECEDE_NATIVE_ACTION_CREATION' -notin @(Rules $reordered $policy $flow)){throw 'Late registration mutation survived'};$checks++
# Реальный AttachVehicleUtility и phase predicate; native adapters фиксируют
# наличие binding непосредственно в момент создания native activity.
$attach=(Body $handoff 'AttachVehicleUtility').Replace('!usage || !groupUtility','usage == null || groupUtility == null').Replace('if (trip.GetVehiclePreset())','if (trip.GetVehiclePreset() != null)').Replace('AICF_VehicleBehaviorBinding binding;','AICF_VehicleBehaviorBinding binding = null;').Replace('!binding ||','binding == null ||').Replace('if (binding &&','if (binding != null &&').Replace('&& trip.GetVehiclePreset())','&& trip.GetVehiclePreset() != null)').Replace('vehicle.FindComponent(SCR_AIVehicleUsageComponent)','vehicle.Usage')
$phase=Body $policy 'IsBehaviorPhase'
$code=@'
public enum AICF_ETransportTripPhase { BOARDING,TRANSIT,TACTICAL_DEPLOYMENT,DISMOUNT,HANDOFF,FALLBACK }
public class SCR_AIVehicleUsageComponent { public static SCR_AIVehicleUsageComponent Cast(SCR_AIVehicleUsageComponent value){return value;} }
public class Vehicle{public SCR_AIVehicleUsageComponent Usage=new SCR_AIVehicleUsageComponent();}
public class SCR_AIGroupUtilityComponent{public bool NativeAccept=true,NativeReconcileResult=true,InvalidateOnAdd,InvalidateOnReconcile,Called,BoundAtAdd,ReconcileCalled,BoundAtReconcile;public void AddUsableVehicle(SCR_AIVehicleUsageComponent u){Called=true;BoundAtAdd=AICF_VehicleBehaviorPolicy.Binding!=null;if(InvalidateOnAdd && AICF_VehicleBehaviorPolicy.Binding!=null)AICF_VehicleBehaviorPolicy.Binding.Current=false;}public bool IsUsableVehicle(SCR_AIVehicleUsageComponent u){return NativeAccept;}public bool AICF_ReconcileExactVehicleSubgroup(AICF_TransportTrip trip){ReconcileCalled=true;BoundAtReconcile=AICF_VehicleBehaviorPolicy.Binding!=null;if(InvalidateOnReconcile && AICF_VehicleBehaviorPolicy.Binding!=null)AICF_VehicleBehaviorPolicy.Binding.Current=false;return NativeReconcileResult;}}
public class SCR_AIGroup{public SCR_AIGroupUtilityComponent Utility=new SCR_AIGroupUtilityComponent();public SCR_AIGroupUtilityComponent GetGroupUtilityComponent(){return Utility;}}
public class AICF_TestAssignment{public SCR_AIGroup Group=new SCR_AIGroup();public SCR_AIGroup GetGroup(){return Group;}}
public class AICF_TestLease{public Vehicle Vehicle=new Vehicle();public Vehicle GetVehicle(){return Vehicle;}}
public class AICF_TransportTrip{public bool LeaseCurrent=true;public object Preset=new object();public AICF_ETransportTripPhase Phase=AICF_ETransportTripPhase.TRANSIT;public AICF_TestAssignment Assignment=new AICF_TestAssignment();public AICF_TestLease Lease=new AICF_TestLease();public AICF_TestAssignment GetAssignment(){return Assignment;}public AICF_TestLease GetLease(){return Lease;}public object GetVehiclePreset(){return Preset;}public AICF_ETransportTripPhase GetPhase(){return Phase;}}
public class AICF_VehicleBehaviorBinding{public AICF_TransportTrip m_Trip;public bool Current=true;public bool IsCurrent(){return Current;}}
public class AICF_VehicleBehaviorPolicy{public static AICF_VehicleBehaviorBinding Binding;public static bool Accept=true;public static int Registrations,Removals;public static bool IsBehaviorPhase(AICF_ETransportTripPhase phase) PHASE public static void Register(AICF_TransportTrip t){Registrations++;if(Accept)Binding=new AICF_VehicleBehaviorBinding{m_Trip=t};}public static AICF_VehicleBehaviorBinding Find(Vehicle v){return Binding;}public static void Unregister(AICF_TransportTrip t){Binding=null;Removals++;}}
public class AICF_AttachProduction{public static bool IsTripLeaseCurrent(AICF_TransportTrip t){return t.LeaseCurrent;}public static bool AttachVehicleUtility(AICF_TransportTrip trip) ATTACH}
'@
Add-Type -TypeDefinition ($code.Replace(' PHASE',' '+$phase).Replace(' ATTACH',' '+$attach))
function AttachCase([AICF_TransportTrip]$t,[bool]$expected,[bool]$called,[bool]$bound,[bool]$reconciled=$false){
 [AICF_VehicleBehaviorPolicy]::Binding=$null
 if([AICF_AttachProduction]::AttachVehicleUtility($t) -ne $expected -or $t.Assignment.Group.Utility.Called -ne $called -or $t.Assignment.Group.Utility.BoundAtAdd -ne $bound -or
  $t.Assignment.Group.Utility.ReconcileCalled -ne $reconciled -or $t.Assignment.Group.Utility.BoundAtReconcile -ne ($reconciled -and $bound)){throw 'Production attach order/postcondition failed'};$script:checks++
}
foreach($p in [Enum]::GetValues([AICF_ETransportTripPhase])){if([AICF_VehicleBehaviorPolicy]::IsBehaviorPhase($p) -ne ($p -eq [AICF_ETransportTripPhase]::TRANSIT -or $p -eq [AICF_ETransportTripPhase]::TACTICAL_DEPLOYMENT)){throw 'Production phase predicate failed'};$checks++}
$t=New-Object AICF_TransportTrip;AttachCase $t $true $true $true $true
$t=New-Object AICF_TransportTrip;$t.Phase=[AICF_ETransportTripPhase]::TACTICAL_DEPLOYMENT;AttachCase $t $true $true $true $true
$t=New-Object AICF_TransportTrip;$t.Phase=[AICF_ETransportTripPhase]::BOARDING;AttachCase $t $false $false $false
$t=New-Object AICF_TransportTrip;$t.LeaseCurrent=$false;AttachCase $t $false $false $false
[AICF_VehicleBehaviorPolicy]::Accept=$false;$t=New-Object AICF_TransportTrip;AttachCase $t $false $false $false;[AICF_VehicleBehaviorPolicy]::Accept=$true
$t=New-Object AICF_TransportTrip;$t.Assignment.Group.Utility.NativeAccept=$false;AttachCase $t $false $true $true;if([AICF_VehicleBehaviorPolicy]::Binding){throw 'Failed native attach leaked binding'};$checks++
$t=New-Object AICF_TransportTrip;$t.Assignment.Group.Utility.InvalidateOnAdd=$true;AttachCase $t $false $true $true $true;if([AICF_VehicleBehaviorPolicy]::Binding){throw 'Changed context leaked binding'};$checks++
$t=New-Object AICF_TransportTrip;$t.Preset=$null;AttachCase $t $true $true $false
$removals=[AICF_VehicleBehaviorPolicy]::Removals
$t=New-Object AICF_TransportTrip;$t.Assignment.Group.Utility.NativeReconcileResult=$false;AttachCase $t $false $true $true $true
if([AICF_VehicleBehaviorPolicy]::Binding -or [AICF_VehicleBehaviorPolicy]::Removals -ne ($removals+1)){throw 'Failed native reconcile did not roll back exact binding'};$checks++
$t=New-Object AICF_TransportTrip;$t.Assignment.Group.Utility.InvalidateOnReconcile=$true;AttachCase $t $false $true $true $true
if([AICF_VehicleBehaviorPolicy]::Binding){throw 'Context invalidated by native reconcile leaked binding'};$checks++
"PASS Behavior policy attach controls=$checks production_attach_and_phase=EXECUTED native_cluster_resume=NOT_RUN"
