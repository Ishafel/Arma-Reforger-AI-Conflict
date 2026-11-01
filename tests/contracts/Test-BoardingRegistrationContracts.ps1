param([string]$RepositoryRoot=(Split-Path -Parent (Split-Path -Parent $PSScriptRoot)))
$ErrorActionPreference='Stop'
$path='AIConflictCore/Scripts/Game/AIConflict/Vehicles/'
$handoff=Get-Content (Join-Path $RepositoryRoot ($path+'AICF_VehicleTaskHandoff.c')) -Raw
$controller=Get-Content (Join-Path $RepositoryRoot ($path+'AICF_TransportTripController.c')) -Raw
$method=[regex]::Match($handoff,'\tbool RegisterBoardingVehicle\([\s\S]*?\n\t\}').Value
if(!$method){throw 'Missing registration boundary'}
$helper=[regex]::Match($handoff,'\tprotected bool RegisterVehicleRecord\([\s\S]*?\n\t\}').Value
$tactical=[regex]::Match($handoff,'\tbool RegisterTacticalVehicle\([\s\S]*?\n\t\}').Value
if(!$helper -or !$tactical){throw 'Missing retained registration boundary'}
$method += $helper + $tactical
if($method -match 'AddUsableVehicle|AddAction|CallLater|SetReserved|SetPhase'){throw 'Registration boundary performs unrelated side effects'}
if($controller -notmatch 'if \(!m_Handoff.RegisterBoardingVehicle\(trip, slot\)\)[\s\S]*BOARDING_VEHICLE_REGISTRATION_REJECTED[\s\S]*AICF_TripOutcome beginOutcome = m_BoardingFlow.Begin'){throw 'Native actions may precede registration'}
if($controller -notmatch 'if \(!m_Handoff.RegisterTacticalVehicle\(trip, slot\)\)[\s\S]*TACTICAL_VEHICLE_REGISTRATION_REJECTED[\s\S]*!m_BehaviorFlow.Begin'){throw 'Retained flow may precede registration'}
$method=$method.Replace('bool RegisterBoardingVehicle','public bool RegisterBoardingVehicle').Replace('bool RegisterTacticalVehicle','public bool RegisterTacticalVehicle')
$stubs=@'
public class Obj { public static implicit operator bool(Obj o){return o!=null;} }
public static class Replication {public static bool Server=true;public static bool IsServer(){return Server;}}
public enum AICF_ETransportTripPhase {BOARDING,TRANSIT,TACTICAL_DEPLOYMENT}
public class RplComponent:Obj {public bool Master=true;public bool IsMaster(){return Master;}public static RplComponent Cast(object o){return o as RplComponent;}}
public class SCR_AIVehicleUsageComponent:Obj {public static SCR_AIVehicleUsageComponent Cast(object o){return o as SCR_AIVehicleUsageComponent;}}
public class Vehicle:Obj {public SCR_AIVehicleUsageComponent Usage=new SCR_AIVehicleUsageComponent();public object FindComponent(System.Type t){return Usage;}}
public class SCR_AIGroupVehicle:Obj {public Vehicle Vehicle;public SCR_AIVehicleUsageComponent Usage;public Vehicle GetEntity(){return Vehicle;}public SCR_AIVehicleUsageComponent GetVehicleUsageComponent(){return Usage;}}
public class Manager:Obj {public Vehicle Vehicle;public int Adds;public SCR_AIGroupVehicle Record;public bool Reject;public SCR_AIGroupVehicle TryAddVehicle(SCR_AIVehicleUsageComponent u){Adds++;if(Reject)return null;if(Record==null)Record=new SCR_AIGroupVehicle{Vehicle=Vehicle,Usage=u};return Record;}public SCR_AIGroupVehicle FindVehicle(Vehicle v){return Record!=null&&Record.Vehicle==v?Record:null;}}
public class SCR_AIGroupUtilityComponent:Obj {public Manager m_VehicleMgr=new Manager();}
public class SCR_AIGroup:Obj {public RplComponent Rpl=new RplComponent();public SCR_AIGroupUtilityComponent Utility=new SCR_AIGroupUtilityComponent();public object FindComponent(System.Type t){return Rpl;}public SCR_AIGroupUtilityComponent GetGroupUtilityComponent(){return Utility;}}
public class Assignment {public SCR_AIGroup Group=new SCR_AIGroup();public SCR_AIGroup GetGroup(){return Group;}}
public class Lease {public Vehicle Vehicle=new Vehicle();public Vehicle GetVehicle(){return Vehicle;}}
public class AICF_GroupSlot:Obj {}
public class AICF_TransportTrip:Obj {public Assignment Assignment=new Assignment();public Lease Lease=new Lease();public AICF_ETransportTripPhase Phase;public object Preset=new object();public object GetVehiclePreset(){return Preset;}public Assignment GetAssignment(){return Assignment;}public Lease GetLease(){return Lease;}public AICF_ETransportTripPhase GetPhase(){return Phase;}}
public class Subject {public bool Current=true,LeaseCurrent=true;bool IsCurrentAssignment(AICF_TransportTrip t,AICF_GroupSlot s){return Current;}bool IsTripLeaseCurrent(AICF_TransportTrip t){return LeaseCurrent;}
'@
$method=$method.Replace('FindComponent(RplComponent)','FindComponent(typeof(RplComponent))').Replace('FindComponent(SCR_AIVehicleUsageComponent)','FindComponent(typeof(SCR_AIVehicleUsageComponent))').Replace('!trip.GetVehiclePreset()','trip.GetVehiclePreset()==null')
Add-Type -TypeDefinition ($stubs+$method+'}')
$checks=0
function Need($v,$why){if(!$v){throw $why};$script:checks++}
for($case=0;$case -lt 8;$case++){
 $s=[Subject]::new();$t=[AICF_TransportTrip]::new();$slot=[AICF_GroupSlot]::new();$mgr=$t.Assignment.Group.Utility.m_VehicleMgr;$mgr.Vehicle=$t.Lease.Vehicle
 [Replication]::Server=$true
 switch($case){1{[Replication]::Server=$false}2{$s.Current=$false}3{$s.LeaseCurrent=$false}4{$t.Phase=[AICF_ETransportTripPhase]::TRANSIT}5{$t.Assignment.Group.Rpl.Master=$false}6{$t.Lease.Vehicle.Usage=$null}7{$mgr.Reject=$true}}
 $ok=$s.RegisterBoardingVehicle($t,$slot)
 Need ($ok -eq ($case -eq 0)) "Unexpected admission case $case"
 if($case -eq 0){$record=$mgr.Record;Need ($s.RegisterBoardingVehicle($t,$slot)) 'Idempotent registration failed';Need ([object]::ReferenceEquals($record,$mgr.Record)) 'Replaced current record'}
 if($case -ge 1 -and $case -le 6){Need ($mgr.Adds -eq 0) 'Mutated manager on stale/non-authority context'}
}
[Replication]::Server=$true
$s=[Subject]::new();$t=[AICF_TransportTrip]::new();$slot=[AICF_GroupSlot]::new();$t.Assignment.Group.Utility.m_VehicleMgr.Vehicle=$t.Lease.Vehicle
Need (!$s.RegisterTacticalVehicle($t,$slot)) 'Tactical admitted in BOARDING'
$t.Phase=[AICF_ETransportTripPhase]::TACTICAL_DEPLOYMENT
Need (!$s.RegisterBoardingVehicle($t,$slot)) 'Boarding admitted in tactical phase'
Need ($s.RegisterTacticalVehicle($t,$slot)) 'Retained phase lacks native record'
$record=$t.Assignment.Group.Utility.m_VehicleMgr.Record
Need ($s.RegisterTacticalVehicle($t,$slot)) 'Retained idempotence failed'
Need ([object]::ReferenceEquals($record,$t.Assignment.Group.Utility.m_VehicleMgr.Record)) 'Retained record replaced'
$t.Preset=$null
Need (!$s.RegisterTacticalVehicle($t,$slot)) 'Automatic trip admitted to retained phase'
"PASS actual registration $checks cases; controller order and no-combat boundary"
